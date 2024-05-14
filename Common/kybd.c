/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	kybd.c
 *		This module handles the keyboard for the 3270 emulator.
 */

#include "globals.h"

#include <fcntl.h>
#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"
#include "resources.h"
#include "screen.h"

#include "actions.h"
#include "apl.h"
#include "boolstr.h"
#include "codepage.h"
#include "ctlrc.h"
#include "unicodec.h"
#include "ft.h"
#include "host.h"
#include "idle.h"
#include "kybd.h"
#include "latin1.h"
#include "linemode.h"
#include "names.h"
#include "nvt.h"
#include "popups.h"
#include "print_screen.h"
#include "product.h"
#include "query.h"
#include "screen.h"
#include "scroll.h"
#include "split_host.h"
#include "stringscript.h"
#include "task.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"
#include "vstatus.h"

/*#define KYBDLOCK_TRACE	1*/

#define MarginedPaste()	(IN_3270 && !IN_SSCP && (toggled(MARGINED_PASTE) || toggled(OVERLAY_PASTE)))

/* Statics */
static enum	{ NONE, COMPOSE, FIRST } composing = NONE;
static unsigned char pf_xlate[] = { 
    AID_PF1,  AID_PF2,  AID_PF3,  AID_PF4,  AID_PF5,  AID_PF6,
    AID_PF7,  AID_PF8,  AID_PF9,  AID_PF10, AID_PF11, AID_PF12,
    AID_PF13, AID_PF14, AID_PF15, AID_PF16, AID_PF17, AID_PF18,
    AID_PF19, AID_PF20, AID_PF21, AID_PF22, AID_PF23, AID_PF24
};
static unsigned char pa_xlate[] = { 
    AID_PA1, AID_PA2, AID_PA3
};
#define PF_SZ	(sizeof(pf_xlate)/sizeof(pf_xlate[0]))
#define PA_SZ	(sizeof(pa_xlate)/sizeof(pa_xlate[0]))
static ioid_t unlock_id = NULL_IOID;
static time_t unlock_delay_time;
static bool key_Character(unsigned ebc, bool with_ge, bool pasting,
	bool oerr_fail, bool *consumed);
static bool flush_ta(void);
static void key_AID(unsigned char aid_code);
static void kybdlock_set(unsigned int bits, const char *cause);
static ks_t my_string_to_key(const char *s, enum keytype *keytypep,
    ucs4_t *ucs4);

static bool key_WCharacter(unsigned char code[], bool oerr_fail);

/* Globals */
unsigned int	kybdlock = KL_NOT_CONNECTED;
unsigned char	aid = AID_NO;		/* current attention ID */

/* Composite key mappings. */

struct akey {
    ucs4_t ucs4;
    enum keytype keytype;
};
static struct akey cc_first;
static struct composite {
    struct akey k1, k2;
    struct akey translation;
} *composites = NULL;
static int n_composites = 0;
static char *default_compose_map_name = NULL;
static char *temporary_compose_map_name = NULL;

#define ak_eq(k1, k2)	(((k1).ucs4  == (k2).ucs4) && \
			 ((k1).keytype == (k2).keytype))

typedef struct ta {
    struct ta *next;
    const char *efn_name;
    action_t *fn;
    const char *parm1;
    const char *parm2;
} ta_t;
ta_t *ta_head = (struct ta *) NULL;
ta_t *ta_tail = (struct ta *) NULL;

static char dxl[] = "0123456789abcdef";
#define FROM_HEX(c)	(int)(strchr(dxl, tolower((unsigned char)c)) - dxl)

#define KYBDLOCK_IS_OERR	(kybdlock && !(kybdlock & ~KL_OERR_MASK))

/* Common kybdlock logic for actions that clear overflows */
#define OERR_CLEAR_OR_ENQ(action) do { \
    if (kybdlock) { \
	if (KYBDLOCK_IS_OERR) { \
	    kybdlock_clr(KL_OERR_MASK, action); \
	    vstatus_reset(); \
	} else { \
	    enq_ta(action, NULL, NULL); \
	    return true; \
	} \
    } \
} while(false)

/* Keyboard lock test for AIDs used by CUT-mode file transfers. */
#define IS_LOCKED(ia) \
    ((kybdlock & ~KL_FT) || \
     ((kybdlock & KL_FT) && (ia != IA_FT)))

static action_t Attn_action;
static action_t BackSpace_action;
static action_t BackTab_action;
static action_t Attn_action;
static action_t BackSpace_action;
static action_t BackTab_action;
static action_t CircumNot_action;
static action_t Clear_action;
static action_t Compose_action;
static action_t CursorSelect_action;
static action_t Delete_action;
static action_t DeleteField_action;
static action_t DeleteWord_action;
static action_t Dup_action;
static action_t Enter_action;
static action_t Erase_action;
static action_t EraseEOF_action;
static action_t EraseInput_action;
static action_t FieldEnd_action;
static action_t FieldMark_action;
static action_t Flip_action;
static action_t HexString_action;
static action_t Home_action;
static action_t Insert_action;
static action_t Interrupt_action;
static action_t Key_action;
static action_t Left2_action;
static action_t MonoCase_action;
static action_t MoveCursor_action;
static action_t MoveCursor1_action;
static action_t Newline_action;
static action_t NextWord_action;
static action_t PA_action;
static action_t PasteString_action;
static action_t PF_action;
static action_t PreviousWord_action;
static action_t Reset_action;
static action_t Right2_action;
static action_t String_action;
static action_t SysReq_action;
static action_t Tab_action;
static action_t TemporaryComposeMap_action;
static action_t ToggleInsert_action;
static action_t ToggleReverse_action;

static action_table_t kybd_actions[] = {
    { AnAttn,		Attn_action,		ACTION_KE },
    { AnBackSpace,	BackSpace_action,	ACTION_KE },
    { AnBackTab,	BackTab_action,		ACTION_KE },
    { AnCircumNot,	CircumNot_action,	ACTION_KE },
    { AnClear,		Clear_action,		ACTION_KE },
    { AnCursorSelect,	CursorSelect_action,	ACTION_KE },
    { AnDelete,		Delete_action,		ACTION_KE },
    { AnDeleteField,	DeleteField_action,	ACTION_KE },
    { AnDeleteWord,	DeleteWord_action,	ACTION_KE },
    { AnDown,		Down_action,		ACTION_KE },
    { AnDup,		Dup_action,		ACTION_KE },
    { AnEnter,		Enter_action,		ACTION_KE },
    { AnErase,		Erase_action,		ACTION_KE },
    { AnEraseEOF,	EraseEOF_action,	ACTION_KE },
    { AnEraseInput,	EraseInput_action,	ACTION_KE },
    { AnFieldEnd,	FieldEnd_action,	ACTION_KE },
    { AnFieldMark,	FieldMark_action,	ACTION_KE },
    { AnFlip,		Flip_action,		ACTION_KE },
    { AnHexString,	HexString_action,	ACTION_KE },
    { AnHome,		Home_action,		ACTION_KE },
    { AnInsert,		Insert_action,		ACTION_KE },
    { AnInterrupt,	Interrupt_action,	ACTION_KE },
    { AnKey,		Key_action,		ACTION_KE },
    { AnLeft,		Left_action,		ACTION_KE },
    { AnLeft2,		Left2_action,		ACTION_KE },
    { AnMonoCase,	MonoCase_action,	ACTION_KE | ACTION_HIDDEN },
    { AnMoveCursor,	MoveCursor_action,	ACTION_KE },
    { AnMoveCursor1,	MoveCursor1_action,	ACTION_KE },
    { AnNewline,	Newline_action,		ACTION_KE },
    { AnNextWord,	NextWord_action,	ACTION_KE },
    { AnPA,		PA_action,		ACTION_KE },
    { AnPasteString,	PasteString_action,	ACTION_KE },
    { AnPF,		PF_action,		ACTION_KE },
    { AnPreviousWord,	PreviousWord_action,	ACTION_KE },
    { AnReset,		Reset_action,		ACTION_KE },
    { AnRight,		Right_action,		ACTION_KE },
    { AnRight2,		Right2_action,		ACTION_KE },
    { AnString,		String_action,		ACTION_KE },
    { AnSysReq,		SysReq_action,		ACTION_KE },
    { AnTab,		Tab_action,		ACTION_KE },
    { AnTemporaryComposeMap,TemporaryComposeMap_action,ACTION_KE },
    { AnToggleInsert,	ToggleInsert_action,	ACTION_KE },
    { AnToggleReverse,	ToggleReverse_action,	ACTION_KE },
    { AnUp,		Up_action,		ACTION_KE }
};

static action_table_t kybd_dactions[] = {
    { AnCompose,	Compose_action,		ACTION_KE }
};

/*
 * Put a function or action on the typeahead queue.
 */
static void
enq_xta(const char *name, action_t *fn, const char *parm1, const char *parm2)
{
    ta_t *ta;

    /* If no connection, forget it. */
    if (!IN_3270 && !IN_NVT && !IN_SSCP) {
	vtrace("  dropped (not connected)\n");
	return;
    }

    /* If operator error, complain and drop it. */
    if (kybdlock & KL_OERR_MASK) {
	ring_bell();
	vtrace("  dropped (operator error)\n");
	return;
    }

    /* If scroll lock, complain and drop it. */
    if (kybdlock & KL_SCROLLED) {
	ring_bell();
	vtrace("  dropped (scrolled)\n");
	return;
    }

    /* If file transfer in progress, complain and drop it. */
    if (kybdlock & KL_FT) {
	ring_bell();
	vtrace("  dropped (file transfer in progress)\n");
	return;
    }

    /* If typeahead disabled, complain and drop it. */
    if (!toggled(TYPEAHEAD)) {
	vtrace("  dropped (no typeahead)\n");
	return;
    }

    ta = (ta_t *)Malloc(sizeof(*ta));
    ta->next = NULL;
    ta->efn_name = name;
    ta->fn = fn;
    ta->parm1 = ta->parm2 = NULL;
    if (parm1) {
	ta->parm1 = NewString(parm1);
	if (parm2) {
	    ta->parm2 = NewString(parm2);
	}
    }
    if (ta_head) {
	ta_tail->next = ta;
    } else {
	ta_head = ta;
	vstatus_typeahead(true);
    }
    ta_tail = ta;

    vtrace("  action queued (kybdlock 0x%x)\n", kybdlock);
}

/*
 * Put an action on the typeahead queue.
 */
static void
enq_ta(const char *efn_name, const char *parm1, const char *parm2)
{
    enq_xta(efn_name, NULL, parm1, parm2);
}

/*
 * Put a function on the typeahead queue.
 */
static void
enq_fta(action_t *fn, const char *parm1, const char *parm2)
{
    enq_xta(NULL, fn, parm1, parm2);
}

/*
 * Execute an action from the typeahead queue.
 */
bool
run_ta(void)
{
    ta_t *ta;

    if (kybdlock || (ta = ta_head) == NULL) {
	return false;
    }

    if ((ta_head = ta->next) == NULL) {
	ta_tail = NULL;
	vstatus_typeahead(false);
    }

    if (ta->efn_name) {
	run_action(ta->efn_name, IA_TYPEAHEAD, ta->parm1, ta->parm2);
    } else {
	unsigned argc = 0;
	const char *argv[2];

	if (ta->parm1) {
	    argv[argc++] = ta->parm1;
	    if (ta->parm2) {
		argv[argc++] = ta->parm2;
	    }
	}
	(*ta->fn)(IA_TYPEAHEAD, argc, argv);
    }
    Free((char *)ta->parm1);
    Free((char *)ta->parm2);
    Free(ta);

    return true;
}

/*
 * Flush the typeahead queue.
 * Returns whether or not anything was flushed.
 */
static bool
flush_ta(void)
{
    ta_t *ta, *next;
    bool any = false;

    for (ta = ta_head; ta != NULL; ta = next) {
	Free((char *)ta->parm1);
	Free((char *)ta->parm2);
	next = ta->next;
	Free(ta);
	any = true;
    }
    ta_head = ta_tail = NULL;
    vstatus_typeahead(false);
    return any;
}

/* Decode keyboard lock bits. */
static char *
kybdlock_decode(char *how, unsigned int bits)
{
    varbuf_t r;
    char *space = "";

    if (bits == (unsigned int)-1) {
	return txAsprintf("%sall", how);
    }

    if (bits == 0) {
	return txAsprintf("%snone", how);
    }

    vb_init(&r);
    if (bits & KL_OERR_MASK) {
	vb_appendf(&r, "%sOERR(", how);
	switch(bits & KL_OERR_MASK) {
	case KL_OERR_PROTECTED:
	    vb_appends(&r, "PROTECTED");
	    break;
	case KL_OERR_NUMERIC:
	    vb_appends(&r, "NUMERIC");
	    break;
	case KL_OERR_OVERFLOW:
	    vb_appends(&r, "OVERFLOW");
	    break;
	case KL_OERR_DBCS:
	    vb_appends(&r, "DBCS");
	    break;
	default:
	    vb_appendf(&r, "?%d", bits & KL_OERR_MASK);
	    break;
	}
	vb_appendf(&r, ")");
	space = " ";
    }
    if (bits & KL_NOT_CONNECTED) {
	vb_appendf(&r, "%s%sNOT_CONNECTED", space, how);
	space = " ";
    }
    if (bits & KL_AWAITING_FIRST) {
	vb_appendf(&r, "%s%sAWAITING_FIRST", space, how);
	space = " ";
    }
    if (bits & KL_OIA_TWAIT) {
	vb_appendf(&r, "%s%sOIA_TWAIT", space, how);
	space = " ";
    }
    if (bits & KL_OIA_LOCKED) {
	vb_appendf(&r, "%s%sOIA_LOCKED", space, how);
	space = " ";
    }
    if (bits & KL_DEFERRED_UNLOCK) {
	vb_appendf(&r, "%s%sDEFERRED_UNLOCK", space, how);
	space = " ";
    }
    if (bits & KL_ENTER_INHIBIT) {
	vb_appendf(&r, "%s%sENTER_INHIBIT", space, how);
	space = " ";
    }
    if (bits & KL_SCROLLED) {
	vb_appendf(&r, "%s%sSCROLLED", space, how);
	space = " ";
    }
    if (bits & KL_OIA_MINUS) {
	vb_appendf(&r, "%s%sOIA_MINUS", space, how);
	space = " ";
    }
    if (bits & KL_FT) {
	vb_appendf(&r, "%s%sFT", space, how);
	space = " ";
    }
    if (bits & KL_BID) {
	vb_appendf(&r, "%s%sBID", space, how);
	space = " ";
    }

    return txdFree(vb_consume(&r));
}

/* Set bits in the keyboard lock. */
static void
kybdlock_set(unsigned int bits, const char *cause _is_unused)
{
    unsigned int n;

    if (!(kybdlock & bits)) {
	vtrace("Keyboard lock(%s) %s %s -> %s\n", cause,
		kybdlock_decode("", kybdlock),
		kybdlock_decode("+", bits),
		kybdlock_decode("", kybdlock | bits));
    }
    n = kybdlock | bits;
    if (n != kybdlock) {
	if ((kybdlock ^ bits) & KL_DEFERRED_UNLOCK) {
	    /* Turned on deferred unlock. */
	    unlock_delay_time = time(NULL);
	}
	kybdlock = n;
    }
}

/* Clear bits in the keyboard lock. */
void
kybdlock_clr(unsigned int bits, const char *cause)
{
    unsigned int n;

    if (kybdlock & bits) {
	vtrace("Keyboard unlock(%s) %s %s -> %s\n", cause,
		kybdlock_decode("", kybdlock),
		kybdlock_decode("-", kybdlock & bits),
		kybdlock_decode("", kybdlock & ~bits));
    }
    n = kybdlock & ~bits;
    if (n != kybdlock) {
	if ((kybdlock ^ n) & KL_DEFERRED_UNLOCK) {
	    /* Turned off deferred unlock. */
	    unlock_delay_time = 0;
	}
	kybdlock = n;
    }
}

/*
 * Set or clear enter-inhibit mode.
 */
void
kybd_inhibit(bool inhibit)
{
    if (inhibit) {
	kybdlock_set(KL_ENTER_INHIBIT, "kybd_inhibit");
	if (kybdlock == KL_ENTER_INHIBIT) {
	    vstatus_reset();
	}
    } else {
	kybdlock_clr(KL_ENTER_INHIBIT, "kybd_inhibit");
	if (!kybdlock) {
	    vstatus_reset();
	}
    }
}

/*
 * Set or clear file transfer mode.
 */
void
kybd_ft(bool ft)
{
    if (ft) {
	kybdlock_set(KL_FT, "kybd_ft");
	if (kybdlock == KL_FT) {
	    vstatus_reset();
	}
    } else {
	kybdlock_clr(KL_FT, "kybd_ft");
	if (!kybdlock) {
	    vstatus_reset();
	}
    }
}

/*
 * Toggle insert mode.
 */
static void
insert_mode(bool on)
{
    if (on != toggled(INSERT_MODE)) {
	do_toggle(INSERT_MODE);
    }
}


/*
 * Called when a host connects or disconnects.
 */
static void
kybd_connect(bool connected _is_unused)
{
    if ((kybdlock & KL_DEFERRED_UNLOCK) && unlock_id) {
	RemoveTimeOut(unlock_id);
	unlock_id = NULL_IOID;
    }
    kybdlock_clr(-1, "kybd_connect");

    if (CONNECTED) {
	if (!appres.nvt_mode && !HOST_FLAG(ANSI_HOST) &&
		    !HOST_FLAG(NO_LOGIN_HOST)) {
	    /* Wait for any output or a WCC(restore) from the host */
	    kybdlock_set(KL_AWAITING_FIRST, "kybd_connect");
	}
    } else {
	kybdlock_set(KL_NOT_CONNECTED, "kybd_connect");
	flush_ta();
	insert_mode(false);
    }
}

/*
 * Called when we switch between 3270 and NVT modes.
 */
static void
kybd_in3270(bool in3270 _is_unused)
{
    if ((kybdlock & KL_DEFERRED_UNLOCK) && unlock_id != NULL_IOID) {
	RemoveTimeOut(unlock_id);
	unlock_id = NULL_IOID;
    }
    insert_mode(IN_3270 && toggled(ALWAYS_INSERT));

    switch ((int)cstate) {
    case CONNECTED_UNBOUND:
	/*
	 * We just processed and UNBIND from the host. We are waiting
	 * for a BIND, or data to switch us to 3270, NVT or SSCP-LU
	 * mode.
	 */
	if (!HOST_FLAG(NO_LOGIN_HOST)) {
	    kybdlock_set(KL_AWAITING_FIRST, "kybd_in3270");
	}
	break;
    case CONNECTED_NVT:
    case CONNECTED_NVT_CHAR:
    case CONNECTED_E_NVT:
    case CONNECTED_SSCP:
	/*
	 * We just transitioned to NVT, TN3270E NVT or TN3270E SSCP-LU
	 * mode.  Remove all lock bits.
	 */
	kybdlock_clr(-1, "kybd_in3270");
	break;
    case CONNECTED_TN3270E:
	/*
	 * We are in TN3270E 3270 mode. If so configured and we were
	 * explicitly bound, then the keyboard must be unlocked now.
	 * If not, we are implicitly in 3270 mode because the host did
	 * not negotiate BIND notifications, and we should continue to
	 * wait for a Write command before unlocking the keyboard.
	 */
	if (appres.bind_unlock && net_bound()) {
	    kybdlock_clr(-1, "kybd_in3270");
	} else {
	    /*
	     * Clear everything but AWAITING_FIRST and LOCKED.
	     * The former was set by this function when we were
	     * unbound. The latter may be a leftover from the user
	     * initiating a host switch by sending a command with an
	     * AID. If this is a non-bind-unlock host (bind_unlock is
	     * clear, the default), we want to preserve that until the
	     * host sends a Write with a Keyboard Restore in it.
	     */
	    kybdlock_clr(~(KL_AWAITING_FIRST | KL_OIA_LOCKED), "kybd_in3270");
	}
	break;
    default:
	/*
	 * We just transitioned into or out of 3270 mode.
	 * Remove all lock bits except AWAITING_FIRST.
	 */
	kybdlock_clr(~KL_AWAITING_FIRST, "kybd_in3270");
	break;
    }

    /* There might be a macro pending. */
    if (CONNECTED) {
	ps_process();
    }
}

/*
 * Toggle the operator error lock setting.
 */
static toggle_upcall_ret_t
toggle_oerr_lock(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    const char *errmsg = boolstr(value, &appres.oerr_lock);

    if (errmsg != NULL) {
	popup_an_error("%s %s", ResOerrLock, errmsg);
	return TU_FAILURE;
    }
    return TU_SUCCESS;
}

/*
 * Toggle the unlock delay setting.
 */
static toggle_upcall_ret_t
toggle_unlock_delay(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    const char *errmsg = boolstr(value, &appres.unlock_delay);

    if (errmsg != NULL) {
	popup_an_error("%s %s", ResUnlockDelay, errmsg);
	return TU_FAILURE;
    }
    return TU_SUCCESS;
}

/*
 * Toggle the unlock delay milliseconds setting.
 */
static toggle_upcall_ret_t
toggle_unlock_delay_ms(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    unsigned long l;
    char *end;
    int ms;

    if (!*value) {
	appres.unlock_delay_ms = 0;
	return TU_SUCCESS;
    }

    l = strtoul(value, &end, 10);
    ms = (int)l;
    if (*end != '\0' || (unsigned long)ms != l || ms < 0) {
	popup_an_error("Invalid %s value", ResUnlockDelay);
	return TU_FAILURE;
    }
    appres.unlock_delay_ms = ms;
    return TU_SUCCESS;
}

/* The always-insert toggle changed. */
static void
toggle_always_insert(toggle_index_t ix, enum toggle_type type)
{
    insert_mode(IN_3270 && toggled(ALWAYS_INSERT));
}

/* The right-to-left display toggle changed. */
static void
toggle_right_to_left(toggle_index_t ix, enum toggle_type type)
{
    if (screen_flipped() != toggled(RIGHT_TO_LEFT)) {
	screen_flip();
    }
}

/*
 * Lock the keyboard because of an operator error.
 *
 * Returns false (command failed) if error message was generated, true
 * otherwise.
 */
static bool
operator_error(int error_type, bool oerr_fail)
{
    if (oerr_fail) {
	popup_an_error("Keyboard locked");
    }
    if (appres.oerr_lock || oerr_fail) {
	vstatus_oerr(error_type);
	mcursor_locked();
	kybdlock_set((unsigned int)error_type, "operator_error");
	flush_ta();
    }
    return !oerr_fail;
}

/* The reverse input toggle changed. */
static void
toggle_reverse_input(toggle_index_t ix, enum toggle_type type)
{
    vstatus_reverse_mode(toggled(REVERSE_INPUT));
}

/* The insert mode toggle changed. */
static void
toggle_insert_mode(toggle_index_t ix, enum toggle_type type)
{
    vstatus_insert_mode(toggled(INSERT_MODE));
}

/* Dump the keyboard lock state. */
static const char *
kybdlock_dump(void)
{
    return TrueFalse(task_kbwait_state());
}

/*
 * Keyboard module registration.
 */
void
kybd_register(void)
{
    static toggle_register_t toggles[] = {
	{ BLANK_FILL,	NULL,	0 },
	{ SHOW_TIMING,	NULL,	0 },
	{ ALWAYS_INSERT,toggle_always_insert,	0 },
	{ RIGHT_TO_LEFT,toggle_right_to_left,	0 },
	{ REVERSE_INPUT,toggle_reverse_input,	0 },
	{ INSERT_MODE,	toggle_insert_mode,	0 },
	{ UNDERSCORE_BLANK_FILL, NULL,	0 },
    };
    static query_t queries[] = {
	{ KwKeyboardLock, kybdlock_dump, NULL, false, false },
    };

    /* Register interest in connect and disconnect events. */
    register_schange_ordered(ST_CONNECT, kybd_connect, 1000);
    register_schange_ordered(ST_3270_MODE, kybd_in3270, 1000);

    /* Register the actions. */
    register_actions(kybd_actions, array_count(kybd_actions));

    /* Register the interactive actions. */
    if (product_has_display()) {
	register_actions(kybd_dactions, array_count(kybd_dactions));
    }

    /* Register the toggles. */
    register_toggles(toggles, array_count(toggles));
    register_extended_toggle(ResOerrLock, toggle_oerr_lock, NULL, NULL,
	    (void **)&appres.oerr_lock, XRM_BOOLEAN);
    register_extended_toggle(ResUnlockDelay, toggle_unlock_delay, NULL, NULL,
	    (void **)&appres.unlock_delay, XRM_BOOLEAN);
    register_extended_toggle(ResUnlockDelayMs, toggle_unlock_delay_ms, NULL,
	    NULL, (void **)&appres.unlock_delay_ms, XRM_INT);

    /* Register queries. */
    register_queries(queries, array_count(queries));
}

/*
 * Handle an AID (Attention IDentifier) key.  This is the common stuff that
 * gets executed for all AID keys (PFs, PAs, Clear and etc).
 */
static void
key_AID(unsigned char aid_code)
{
    if (IN_NVT) {
	register unsigned i;

	if (aid_code == AID_ENTER) {
	    net_sendc('\r');
	    return;
	}
	for (i = 0; i < PF_SZ; i++)
	    if (aid_code == pf_xlate[i]) {
		nvt_send_pf(i+1);
		return;
	    }
	for (i = 0; i < PA_SZ; i++) {
	    if (aid_code == pa_xlate[i]) {
		nvt_send_pa(i+1);
		return;
	    }
	}
	return;
    }

    if (IN_SSCP) {
	bool need_scroll = false;

	if (kybdlock & KL_OIA_MINUS) {
	    return;
	}
	switch (aid_code) {
	case AID_CLEAR:
	    /* Handled locally. */
	    break;
	case AID_ENTER:
	    /* Add a newline. */
	    if (cursor_addr / COLS == ROWS - 1) {
		need_scroll = true;
	    } else {
		/* Move the cursor to the beginning of the next row. */
		cursor_move(((cursor_addr + COLS) / COLS) * COLS);
	    }

	    /*
	     * Act as if the host had written our input, and
	     * send it as a Read Modified.
	     */
	    buffer_addr = cursor_addr;
	    aid = aid_code;
	    ctlr_read_modified(aid, false);
	    vstatus_ctlr_done();
	    if (need_scroll) {
		ctlr_scroll(0, 0);
		cursor_move((ROWS - 1) * COLS);
		buffer_addr = (ROWS - 1) * COLS;
	    }
	    break;
	default:
	    /* Everything else is invalid in SSCP-LU mode. */
	    vstatus_minus();
	    kybdlock_set(KL_OIA_MINUS, "key_AID");
	    return;
	}
	return;
    }

    vstatus_twait();
    mcursor_waiting();
    insert_mode(toggled(ALWAYS_INSERT));
    kybdlock_set(KL_OIA_TWAIT | KL_OIA_LOCKED, "key_AID");
    aid = aid_code;
    ctlr_read_modified(aid, false);
    ticking_start(false);
    vstatus_ctlr_done();
}

static bool
PF_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned k;

    action_debug(AnPF, ia, argc, argv);
    if (check_argc(AnPF, argc, 1, 1) < 0) {
	return false;
    }
    k = atoi(argv[0]);
    if (k < 1 || k > PF_SZ) {
	popup_an_error(AnPF "(): Invalid argument '%s'", argv[0]);
	return false;
    }
    if (kybdlock & KL_OIA_MINUS) {
	return true;
    }
    if (IS_LOCKED(ia)) {
	enq_ta(AnPF, argv[0], NULL);
    } else {
	key_AID(pf_xlate[k-1]);
    }
    return true;
}

static bool
PA_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned k;

    action_debug(AnPA, ia, argc, argv);
    if (check_argc(AnPA, argc, 1, 1) < 0) {
	return false;
    }
    k = atoi(argv[0]);
    if (k < 1 || k > PA_SZ) {
	popup_an_error(AnPA "(): Invalid argument '%s'", argv[0]);
	return false;
    }
    if (kybdlock & KL_OIA_MINUS) {
	return true;
    }
    if (kybdlock) {
	enq_ta(AnPA, argv[0], NULL);
    } else {
	key_AID(pa_xlate[k-1]);
    }
    return true;
}

/*
 * ATTN key, per RFC 2355.  Sends IP, regardless.
 */
static bool
Attn_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnAttn, ia, argc, argv);
    if (check_argc(AnAttn, argc, 0, 0) < 0) {
	return false;
    }
    if (IN_E) {
	if (net_bound()) {
	    net_interrupt(0);
	} else {
	    vstatus_minus();
	    kybdlock_set(KL_OIA_MINUS, AnAttn);
	}
	return true;
    }
    if (IN_3270) {
	net_break(0);
	return true;
    }
    return false;
}

/*
 * IAC IP, which works for 5250 System Request and interrupts the program
 * on an AS/400, even when the keyboard is locked.
 *
 * This is now the same as the Attn action.
 */
static bool
Interrupt_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnInterrupt, ia, argc, argv);
    if (check_argc(AnInterrupt, argc, 0, 0) < 0) {
	return false;
    }
    if (!IN_3270) {
	return false;
    }
    net_interrupt(0);
    return true;
}


/*
 * Prepare for an insert of 'count' bytes.
 * Returns true if the insert is legal, false otherwise.
 */
static bool
ins_prep(int faddr, int baddr, int count, bool *no_room, bool oerr_fail)
{
    int next_faddr;
    int xaddr;
    int need;
    int ntb;
    int copy_len;

    *no_room = false;

    /* Find the end of the field. */
    if (faddr == -1) {
	/* Unformatted.  Use the end of the screen. */
	next_faddr = (ROWS * COLS) - 1;
    } else {
	next_faddr = faddr;
	INC_BA(next_faddr);
	while (next_faddr != faddr && !ea_buf[next_faddr].fa) {
	    INC_BA(next_faddr);
	}
    }

    /* Are there enough NULs or trailing blanks available? */
    xaddr = baddr;
    need = count;
    ntb = 0;
    while (need && (xaddr != next_faddr)) {
	if (ea_buf[xaddr].ec == EBC_null) {
	    need--; 
	} else if (toggled(BLANK_FILL) &&
		((ea_buf[xaddr].ec == EBC_space) ||
		 (toggled(UNDERSCORE_BLANK_FILL) &&
		  (ea_buf[xaddr].ec == EBC_underscore)))) {
		ntb++;
	} else {
	    ntb = 0;
	}
	INC_BA(xaddr);
    }
    if (need - ntb > 0) {
	if (!toggled(REVERSE_INPUT)) {
	    (void) operator_error(KL_OERR_OVERFLOW, oerr_fail);
	    return false;
	} else {
	    *no_room = true;
	    return true;
	}
    }

    /* Shift the buffer to the right until we've consumed enough NULs. */
    need = count;
    xaddr = baddr;
    while (need && (xaddr != next_faddr)) {
	int n_nulls = 0;
	int first_null = -1;

	while (need && (ea_buf[xaddr].ec == EBC_null)) {
	    need--;
	    n_nulls++;
	    if (first_null == -1) {
		first_null = xaddr;
	    }
	    INC_BA(xaddr);
	}
	if (n_nulls) {
	    int to;

	    /* Shift right n_nulls worth. */
	    copy_len = first_null - baddr;
	    if (copy_len < 0) {
		copy_len += ROWS*COLS;
	    }
	    to = (baddr + n_nulls) % (ROWS*COLS);
	    if (copy_len) {
		ctlr_wrapping_memmove(to, baddr, copy_len);
	    }
	}
	INC_BA(xaddr);
    }

    if (!need) {
	return true;
    }

    /*
     * Shift the buffer to the right over trailing spaces and underscores
     * (which we know we have enough of).
     */
    xaddr = next_faddr;
    copy_len = xaddr - baddr; /* field length */
    if (copy_len < 0) {
	copy_len += ROWS * COLS;
    }
    copy_len -= need;
    ctlr_wrapping_memmove((baddr + need) % (ROWS * COLS), baddr, copy_len);

    return true;
}

/* Flags OR'ed into an EBCDIC code when pushed into the typeahead queue. */
#define GE_WFLAG	0x10000
#define PASTE_WFLAG	0x20000

/*
 * Callback for enqueued typeahead. The single parameter is an EBCDIC code,
 * OR'd with the flags above.
 */
static bool
key_Character_wrapper(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    unsigned ebc;
    bool with_ge = false;
    bool pasting = false;
    char mb[16];
    ucs4_t uc;
    bool oerr_fail = false;

    if (argc > 1 && !strcasecmp(argv[1], KwFailOnError)) {
	oerr_fail = true;
    }
    ebc = atoi(argv[0]);
    if (ebc & GE_WFLAG) {
	with_ge = true;
	ebc &= ~GE_WFLAG;
    }
    if (ebc & PASTE_WFLAG) {
	pasting = true;
	ebc &= ~PASTE_WFLAG;
    }
    ebcdic_to_multibyte_x(ebc, with_ge? CS_GE: CS_BASE,
	    mb, sizeof(mb), EUO_BLANK_UNDEF, &uc);
    vtrace(" %s -> Key(%s\"%s\")\n",
	ia_name[(int) ia_cause],
	with_ge ? "GE " : "", mb);
    key_Character(ebc, with_ge, pasting, oerr_fail, NULL);
    return true;
}

/*
 * Handle an ordinary displayable character key.  Lots of stuff to handle
 * insert-mode, protected fields and etc.
 *
 * Returns true if action was successfully processed, including typeahead and
 * silent operator errors.
 * If true is returned, consumed is returned as true if the character was
 * actually accepted.
 */
static bool
key_Character(unsigned ebc, bool with_ge, bool pasting, bool oerr_fail,
	bool *consumed)
{
    register int baddr, faddr, xaddr;
    register unsigned char fa;
    enum dbcs_why why = DBCS_FIELD;
    bool no_room = false;
    bool auto_skip = true;

    if (consumed != NULL) {
	*consumed = false;
    }

    if (kybdlock) {
	char *codename;

	codename = txAsprintf("%d", ebc |
		(with_ge ? GE_WFLAG : 0) |
		(pasting ? PASTE_WFLAG : 0));
	enq_fta(key_Character_wrapper, codename,
		oerr_fail ? KwFailOnError : KwNoFailOnError);
	return true;
    }
    baddr = cursor_addr;
    faddr = find_field_attribute(baddr);
    fa = get_field_attribute(baddr);

    if (pasting && toggled(OVERLAY_PASTE)) {
	auto_skip = false;
    }

    if (ea_buf[baddr].fa || FA_IS_PROTECTED(fa)) {
	if (!auto_skip) {
	    /*
	     * In overlay-paste mode, protected fields cause paste buffer
	     * data to be dropped while moving the cursor right.
	     */
	    INC_BA(baddr);
	    cursor_move(baddr);
	    return true;
	} else {
	    return operator_error(KL_OERR_PROTECTED, oerr_fail);
	}
    }
    if (FA_IS_NUMERIC(fa) && appres.numeric_lock &&
	    !((ebc >= EBC_0 && ebc <= EBC_9) ||
	      ebc == EBC_plus ||
	      ebc == EBC_minus ||
	      ebc == EBC_period ||
	      ebc == EBC_comma)) {
	return operator_error(KL_OERR_NUMERIC, oerr_fail);
    }

    /* Can't put an SBCS in a DBCS field. */
    if (ea_buf[faddr].cs == CS_DBCS) {
	return operator_error(KL_OERR_DBCS, oerr_fail);
    }

    /* If it's an SI (end of DBCS subfield), move over one position. */
    if (ea_buf[baddr].ec == EBC_si) {
	INC_BA(baddr);
	if (baddr == faddr) {
	    return operator_error(KL_OERR_OVERFLOW, oerr_fail);
	}
    }

    /* Add the character. */
    if (ea_buf[baddr].ec == EBC_so) {

	if (toggled(INSERT_MODE)) {
	    if (!ins_prep(faddr, baddr, 1, &no_room, oerr_fail)) {
		return oerr_fail;
	    }
	} else {
	    bool was_si = false;

	    /*
	     * Overwriting an SO (start of DBCS subfield).
	     * If it's followed by an SI, replace the SO/SI
	     * pair with x/space.  If not, replace it and
	     * the following DBCS character with
	     * x/space/SO.
	     */
	    xaddr = baddr;
	    INC_BA(xaddr);
	    was_si = (ea_buf[xaddr].ec == EBC_si);
	    ctlr_add(xaddr, EBC_space, CS_BASE);
	    ctlr_add_fg(xaddr, 0);
	    ctlr_add_bg(xaddr, 0);
	    if (!was_si) {
		INC_BA(xaddr);
		ctlr_add(xaddr, EBC_so, CS_BASE);
		ctlr_add_fg(xaddr, 0);
		ctlr_add_bg(xaddr, 0);
	    }
	}
    } else switch (ctlr_lookleft_state(baddr, &why)) {
    case DBCS_RIGHT:
	DEC_BA(baddr);
	/* fall through... */
    case DBCS_LEFT:
	if (why == DBCS_ATTRIBUTE) {
	    if (toggled(INSERT_MODE)) {
		if (!ins_prep(faddr, baddr, 1, &no_room, oerr_fail)) {
		    return oerr_fail;
		}
	    } else {
		/* Replace single DBCS char with x/space. */
		xaddr = baddr;
		INC_BA(xaddr);
		ctlr_add(xaddr, EBC_space, CS_BASE);
		ctlr_add_fg(xaddr, 0);
		ctlr_add_gr(xaddr, 0);
	    }
	} else {
	    bool was_si;

	    if (toggled(INSERT_MODE)) {
		/*
		 * Inserting SBCS into a DBCS subfield.  If this is the first
		 * position, we can just insert one character in front of the
		 * SO.  Otherwise, we'll need room for SI (to end subfield),
		 * the character, and SO (to begin the subfield again).
		 */
		xaddr = baddr;
		DEC_BA(xaddr);
		if (ea_buf[xaddr].ec == EBC_so) {
		    DEC_BA(baddr);
		    if (!ins_prep(faddr, baddr, 1, &no_room, oerr_fail)) {
			return oerr_fail;
		    }
		} else {
		    if (!ins_prep(faddr, baddr, 3, &no_room, oerr_fail)) {
			return oerr_fail;
		    }
		    xaddr = baddr;
		    ctlr_add(xaddr, EBC_si, CS_BASE);
		    ctlr_add_fg(xaddr, 0);
		    ctlr_add_gr(xaddr, 0);
		    INC_BA(xaddr);
		    INC_BA(baddr);
		    INC_BA(xaddr);
		    ctlr_add(xaddr, EBC_so, CS_BASE);
		    ctlr_add_fg(xaddr, 0);
		    ctlr_add_gr(xaddr, 0);
		}
	    } else {
		/* Overwriting part of a subfield. */
		xaddr = baddr;
		ctlr_add(xaddr, EBC_si, CS_BASE);
		ctlr_add_fg(xaddr, 0);
		ctlr_add_gr(xaddr, 0);
		INC_BA(xaddr);
		INC_BA(baddr);
		INC_BA(xaddr);
		was_si = (ea_buf[xaddr].ec == EBC_si);
		ctlr_add(xaddr, EBC_space, CS_BASE);
		ctlr_add_fg(xaddr, 0);
		ctlr_add_gr(xaddr, 0);
		if (!was_si) {
		    INC_BA(xaddr);
		    ctlr_add(xaddr, EBC_so, CS_BASE);
		    ctlr_add_fg(xaddr, 0);
		    ctlr_add_gr(xaddr, 0);
		}
	    }
	}
	break;
    default:
    case DBCS_NONE:
	if ((toggled(REVERSE_INPUT) || toggled(INSERT_MODE)) &&
		!ins_prep(faddr, baddr, 1, &no_room, oerr_fail)) {
	    return oerr_fail;
	}
	break;
    }
    if (no_room) {
	do {
	    INC_BA(baddr);
	} while (ea_buf[baddr].fa);
    } else {
	ctlr_add(baddr, (unsigned char)ebc,
		(unsigned char)(with_ge ? CS_GE : 0));
	ctlr_add_fg(baddr, 0);
	ctlr_add_gr(baddr, 0);
	if (!toggled(REVERSE_INPUT)) {
	    INC_BA(baddr);
	    if (IN_SSCP && baddr == 0) {
		/* Scroll. */
		ctlr_scroll(0, 0);
		ctlr_sscp_up();
		cursor_move((ROWS - 1) * COLS);
		buffer_addr = (ROWS - 1) * COLS;
		baddr = (ROWS - 1) * COLS;
	    }
	}
    }

    /* Replace leading nulls with blanks, if desired. */
    if (formatted && toggled(BLANK_FILL)) {
	register int baddr_fill = baddr;

	DEC_BA(baddr_fill);
	while (baddr_fill != faddr) {

	    /* Check for backward line wrap. */
	    if ((baddr_fill % COLS) == COLS - 1) {
		bool aborted = true;
		register int baddr_scan = baddr_fill;

		/* Check the field within the preceeding line for NULs. */
		while (baddr_scan != faddr) {
		    if (ea_buf[baddr_scan].ec != EBC_null) {
			aborted = false;
			break;
		    }
		    if (!(baddr_scan % COLS)) {
			break;
		    }
		    DEC_BA(baddr_scan);
		}
		if (aborted) {
		    break;
		}
	    }

	    if (ea_buf[baddr_fill].ec == EBC_null) {
		ctlr_add(baddr_fill, EBC_space, 0);
	    }
	    DEC_BA(baddr_fill);
	}
    }

    mdt_set(cursor_addr);

    /*
     * Implement auto-skip, and don't land on attribute bytes.
     * This happens for all pasted data (even DUP), and for all
     * keyboard-generated data except DUP.
     */
    if (auto_skip && (pasting || (ebc != EBC_dup))) {
	while (ea_buf[baddr].fa) {
	    if (FA_IS_SKIP(ea_buf[baddr].fa)) {
		baddr = next_unprotected(baddr);
	    } else {
		INC_BA(baddr);
	    }
	}
	cursor_move(baddr);
    } else {
	cursor_move(baddr);
    }

    ctlr_dbcs_postprocess();
    if (consumed != NULL) {
	*consumed = true;
    }
    return true;
}

static bool
key_WCharacter_wrapper(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    unsigned ebc_wide;
    unsigned char ebc_pair[2];
    bool oerr_fail = false;

    if (argc > 1 && !strcasecmp(argv[1], KwFailOnError)) {
	oerr_fail = true;
    }
    ebc_wide = atoi(argv[0]);
    vtrace(" %s -> Key(X'%04x')\n", ia_name[(int) ia_cause], ebc_wide);
    ebc_pair[0] = (ebc_wide >> 8) & 0xff;
    ebc_pair[1] = ebc_wide & 0xff;
    key_WCharacter(ebc_pair, oerr_fail);
    return true;
}

/*
 * Input a DBCS character.
 * Returns true if a character was stored in the buffer, false otherwise.
 */
static bool
key_WCharacter(unsigned char ebc_pair[], bool oerr_fail)
{
    int baddr;
    register unsigned char fa;
    int faddr;
    enum dbcs_state d;
    int xaddr;
    bool done = false;
    bool no_si = false;
    bool no_room = false;

    if (kybdlock) {
	char *codename;

	codename = txAsprintf("%d", (ebc_pair[0] << 8) | ebc_pair[1]);
	enq_fta(key_WCharacter_wrapper, codename,
		oerr_fail ? KwFailOnError : KwNoFailOnError);
	return false;
    }

    if (!dbcs) {
	vtrace("DBCS character received when not in DBCS mode, ignoring.\n");
	return true;
    }

    /* In NVT mode? */
    if (IN_NVT) {
	char mb[16];

	ebcdic_to_multibyte((ebc_pair[0] << 8) | ebc_pair[1], mb, sizeof(mb));
	net_sends(mb);
	return true;
    }

    baddr = cursor_addr;
    fa = get_field_attribute(baddr);
    faddr = find_field_attribute(baddr);

    /* Protected? */
    if (ea_buf[baddr].fa || FA_IS_PROTECTED(fa)) {
	return operator_error(KL_OERR_PROTECTED, oerr_fail);
    }

    /* Numeric? */
    if (FA_IS_NUMERIC(fa)) {
	if (appres.numeric_lock) {
	    return operator_error(KL_OERR_NUMERIC, oerr_fail);
	} else {
#if 0
	    /* Ignore it, successfully. */
	    vtrace("Ignoring non-numeric character in numeric field\n");
	    return true;
#endif
	}
    }

    /*
     * Figure our what to do based on the DBCS state of the buffer.
     * Leaves baddr pointing to the next unmodified position.
     */
retry:
    switch (d = ctlr_dbcs_state(baddr)) {
    case DBCS_RIGHT:
    case DBCS_RIGHT_WRAP:
	/* Back up one position and process it as a LEFT. */
	DEC_BA(baddr);
	/* fall through... */
    case DBCS_LEFT:
    case DBCS_LEFT_WRAP:
	/* Overwrite the existing character. */
	if (toggled(INSERT_MODE)) {
	    if (!ins_prep(faddr, baddr, 2, &no_room, oerr_fail)) {
		return oerr_fail;
	    }
	}
	ctlr_add(baddr, ebc_pair[0], ea_buf[baddr].cs);
	INC_BA(baddr);
	ctlr_add(baddr, ebc_pair[1], ea_buf[baddr].cs);
	INC_BA(baddr);
	done = true;
	break;
    case DBCS_SB:
	/* Back up one position and process it as an SI. */
	DEC_BA(baddr);
	/* fall through... */
    case DBCS_SI:
	/* Extend the subfield to the right. */
	if (toggled(INSERT_MODE)) {
	    if (!ins_prep(faddr, baddr, 2, &no_room, oerr_fail)) {
		return oerr_fail;
	    }
	} else {
	    /* Don't overwrite a field attribute or an SO. */
	    xaddr = baddr;
	    INC_BA(xaddr);	/* C1 */
	    if (ea_buf[xaddr].fa) {
		break;
	    }
	    if (ea_buf[xaddr].ec == EBC_so) {
		no_si = true;
	    }
	    INC_BA(xaddr);	/* SI */
	    if (ea_buf[xaddr].fa || ea_buf[xaddr].ec == EBC_so) {
		break;
	    }
	}
	ctlr_add(baddr, ebc_pair[0], ea_buf[baddr].cs);
	INC_BA(baddr);
	ctlr_add(baddr, ebc_pair[1], ea_buf[baddr].cs);
	if (!no_si) {
	    INC_BA(baddr);
	    ctlr_add(baddr, EBC_si, ea_buf[baddr].cs);
	}
	done = true;
	break;
    case DBCS_DEAD:
	break;
    case DBCS_NONE:
	if (ea_buf[faddr].ic) {
	    bool extend_left = false;

	    /* Is there room? */
	    if (toggled(INSERT_MODE)) {
		if (!ins_prep(faddr, baddr, 4, &no_room, oerr_fail)) {
		    return oerr_fail;
		}
	    } else {
		xaddr = baddr;	/* baddr, SO */
		if (ea_buf[xaddr].ec == EBC_so) {
		    /*
		     * (baddr), where we would have put the SO, is already an
		     * SO. Move to (baddr+1) and try again.
		     */
#if defined(DBCS_RIGHT_DEBUG) /*[*/
		    printf("SO in position 0\n");
#endif /*]*/
		    INC_BA(baddr);
		    goto retry;
		}

		INC_BA(xaddr);	/* baddr+1, C0 */
		if (ea_buf[xaddr].fa) {
		    break;
		}
		if (ea_buf[xaddr].ec == EBC_so) {
		    enum dbcs_state e;

		    /*
		     * (baddr+1), where we would have put the left side of the
		     * DBCS, is a SO.  If there's room, we can extend the
		     * * subfield to the left.  If not, we're stuck.
		     */
		    DEC_BA(xaddr);
		    DEC_BA(xaddr);
		    e = ctlr_dbcs_state(xaddr);
		    if (e == DBCS_NONE || e == DBCS_SB) {
			extend_left = true;
			no_si = true;
#if defined(DBCS_RIGHT_DEBUG) /*[*/
			printf("SO in position 1, extend left\n");
#endif /*]*/
		    } else {
			/*
			 * Won't actually happen, because this implies that the
			 * buffer addr at baddr is an SB.
			 */
#if defined(DBCS_RIGHT_DEBUG) /*[*/
			printf("SO in position 1, no room on left, fail\n");
#endif /*]*/
			break;
		    }
		}

		INC_BA(xaddr); /* baddr+2, C1 */
		if (ea_buf[xaddr].fa) {
		    break;
		}
		if (ea_buf[xaddr].ec == EBC_so) {
		    /*
		     * (baddr+2), where we want to put the right half of the
		     * DBCS character, is a SO. This is a natural extension to
		     * the left -- just make sure we don't write an SI.
		     */
		    no_si = true;
#if defined(DBCS_RIGHT_DEBUG) /*[*/
		    printf("SO in position 2, no SI\n");
#endif /*]*/
		}

		/*
		 * Check the fourth position only if we're
		 * not doing an extend-left.
		 */
		if (!no_si) {
		    INC_BA(xaddr); /* baddr+3, SI */
		    if (ea_buf[xaddr].fa) {
			break;
		    }
		    if (ea_buf[xaddr].ec == EBC_so) {
			/*
			 * (baddr+3), where we want to put an SI, is an SO.
			 * Forget it.
			 */
#if defined(DBCS_RIGHT_DEBUG) /*[*/
			printf("SO in position 3, retry right\n");
			INC_BA(baddr);
			goto retry;
#endif /*]*/
			break;
		    }
		}
	    }
	    /* Yes, add it. */
	    if (extend_left) {
		DEC_BA(baddr);
	    }
	    ctlr_add(baddr, EBC_so, ea_buf[baddr].cs);
	    INC_BA(baddr);
	    ctlr_add(baddr, ebc_pair[0], ea_buf[baddr].cs);
	    INC_BA(baddr);
	    ctlr_add(baddr, ebc_pair[1], ea_buf[baddr].cs);
	    if (!no_si) {
		INC_BA(baddr);
		ctlr_add(baddr, EBC_si, ea_buf[baddr].cs);
	    }
	    done = true;
	} else if (reply_mode == SF_SRM_CHAR) {
	    /* Use the character attribute. */
	    if (toggled(INSERT_MODE)) {
		if (!ins_prep(faddr, baddr, 2, &no_room, oerr_fail)) {
		    return oerr_fail;
		}
	    } else {
		xaddr = baddr;
		INC_BA(xaddr);
		if (ea_buf[xaddr].fa) {
		    break;
		}
	    }
	    ctlr_add(baddr, ebc_pair[0], CS_DBCS);
	    INC_BA(baddr);
	    ctlr_add(baddr, ebc_pair[1], CS_DBCS);
	    INC_BA(baddr);
	    done = true;
	}
	break;
    }

    if (done) {
	/* Implement blank fill mode. */
	if (toggled(BLANK_FILL)) {
	    xaddr = faddr;
	    INC_BA(xaddr);
	    while (xaddr != baddr) {
		if (ea_buf[xaddr].ec == EBC_null) {
		    ctlr_add(xaddr, EBC_space, CS_BASE);
		} else {
		    break;
		}
		INC_BA(xaddr);
	    }
	}

	mdt_set(cursor_addr);

	/* Implement auto-skip. */
	while (ea_buf[baddr].fa) {
	    if (FA_IS_SKIP(ea_buf[baddr].fa)) {
		baddr = next_unprotected(baddr);
	    } else {
		INC_BA(baddr);
	    }
	}
	cursor_move(baddr);
	ctlr_dbcs_postprocess();
	return true;
    } else {
	return operator_error(KL_OERR_DBCS, oerr_fail);
    }
}

/*
 * Handle an ordinary character key, given its Unicode value.
 */
void
key_UCharacter(ucs4_t ucs4, enum keytype keytype, enum iaction cause,
	bool oerr_fail)
{
    register int i;
    struct akey ak;

    if (keyboard_disabled() && IA_IS_KEY(cause)) {
	vtrace("  [suppressed, keyboard disabled]\n");
	vstatus_keyboard_disable_flash();
	return;
    }

    if (kybdlock) {
	const char *apl_name;

	if (keytype == KT_STD) {
	    enq_ta(AnKey, txAsprintf("U+%04x", ucs4),
		    oerr_fail ? KwFailOnError : KwNoFailOnError);
	} else {
	    /* APL character */
	    apl_name = ucs4_to_apl_key(ucs4);
	    if (apl_name != NULL) {
		enq_ta(AnKey, txAsprintf("apl_%s", apl_name),
			oerr_fail ? KwFailOnError : KwNoFailOnError);
	    } else {
		vtrace("  dropped (invalid key type or name)\n");
	    }
	}
	return;
    }

    ak.ucs4 = ucs4;
    ak.keytype = keytype;

    switch (composing) {
    case NONE:
	break;
    case COMPOSE:
	for (i = 0; i < n_composites; i++) {
	    if (ak_eq(composites[i].k1, ak) || ak_eq(composites[i].k2, ak)) {
		break;
	    }
	}
	if (i < n_composites) {
	    cc_first.ucs4 = ucs4;
	    cc_first.keytype = keytype;
	    composing = FIRST;
	    vstatus_compose(true, ucs4, keytype);
	} else {
	    ring_bell();
	    composing = NONE;
	    vstatus_compose(false, 0, KT_STD);
	}
	return;
    case FIRST:
	composing = NONE;
	vstatus_compose(false, 0, KT_STD);
	for (i = 0; i < n_composites; i++) {
	    if ((ak_eq(composites[i].k1, cc_first) &&
		 ak_eq(composites[i].k2, ak)) ||
		(ak_eq(composites[i].k1, ak) &&
		 ak_eq(composites[i].k2, cc_first))) {
		break;
	    }
	}
	if (i < n_composites) {
	    ucs4 = composites[i].translation.ucs4;
	    keytype = composites[i].translation.keytype;
	} else {
	    ring_bell();
	    return;
	}
	break;
    }

    vtrace(" %s -> Key(U+%04x)\n", ia_name[(int) cause], ucs4);
    if (IN_3270) {
	ebc_t ebc;
	bool ge;

	if (ucs4 < 0x20) {
	    vtrace("  dropped (control char)\n");
	    return;
	}
	ebc = unicode_to_ebcdic_ge(ucs4, &ge,
		keytype == KT_GE || toggled(APL_MODE));
	if (ebc == 0) {
	    vtrace("  dropped (no EBCDIC translation)\n");
	    return;
	}
	if (ebc & 0xff00) {
	    unsigned char ebc_pair[2];

	    ebc_pair[0] = (ebc & 0xff00)>> 8;
	    ebc_pair[1] = ebc & 0xff;
	    key_WCharacter(ebc_pair, oerr_fail);
	} else {
	    key_Character(ebc, (keytype == KT_GE) || ge, (cause == IA_PASTE),
		    oerr_fail, NULL);
	}
    } else if (IN_NVT) {
	char mb[16];

	unicode_to_multibyte(ucs4, mb, sizeof(mb));
	net_sends(mb);
    } else {
	const char *why;

	switch (cstate) {
	case NOT_CONNECTED:
	case RECONNECTING:
	    why = "connected";
	    break;
	default:
	    why = "negotiated";
	    break;
	case CONNECTED_UNBOUND:
	    why = "bound";
	    break;
	}

	vtrace("  dropped (not %s)\n", why);
    }
}

static bool
MonoCase_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnMonoCase, ia, argc, argv);
    if (check_argc(AnMonoCase, argc, 0, 0) < 0) {
	return false;
    }
    do_toggle(MONOCASE);
    return true;
}

/*
 * Flip the display left-to-right
 */
static bool
Flip_action(ia_t ia, unsigned argc, const char **argv)
{
    const char *toggle_argv[2] = { ResRightToLeftMode, NULL };

    action_debug(AnFlip, ia, argc, argv);
    if (check_argc(AnFlip, argc, 0, 0) < 0) {
	return false;
    }
    return Toggle_action(ia, 1, toggle_argv);
}

/*
 * Tab forward to next field.
 */
static bool
Tab_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnTab, ia, argc, argv);
    if (check_argc(AnTab, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnTab);
    if (IN_NVT) {
	net_sendc('\t');
	return true;
    }
    cursor_move(next_unprotected(cursor_addr));
    return true;
}

/*
 * Tab backward to previous field.
 */
static bool
BackTab_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr, nbaddr;
    int	sbaddr;

    action_debug(AnBackTab, ia, argc, argv);
    if (check_argc(AnBackTab, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnBackTab);
    if (IN_NVT) {
	popup_an_error("%s() is not valid in NVT mode", AnBackTab);
	return false;
    }
    if (!IN_3270) {
	return true;
    }
    baddr = cursor_addr;
    DEC_BA(baddr);
    if (ea_buf[baddr].fa) {	/* at bof */
	DEC_BA(baddr);
    }
    sbaddr = baddr;
    while (true) {
	nbaddr = baddr;
	INC_BA(nbaddr);
	if (ea_buf[baddr].fa &&
	    !FA_IS_PROTECTED(ea_buf[baddr].fa) &&
	    !ea_buf[nbaddr].fa) {
	    break;
	}
	DEC_BA(baddr);
	if (baddr == sbaddr) {
	    cursor_move(0);
	    return true;
	}
    }
    INC_BA(baddr);
    cursor_move(baddr);
    return true;
}

/*
 * Deferred keyboard unlock.
 */
static void
defer_unlock(ioid_t id _is_unused)
{
    kybdlock_clr(KL_DEFERRED_UNLOCK, "defer_unlock");
    vstatus_reset();
    if (CONNECTED) {
	ps_process();
    }
}

/*
 * Reset keyboard lock.
 */
void
do_reset(bool explicit)
{
    /*
     * If explicit (from the keyboard) and there is typeahead or
     * a half-composed key, simply flush it.
     */
    if (explicit || ft_state != FT_NONE) {
	bool half_reset = false;

	if (flush_ta()) {
	    half_reset = true;
	}
	if (composing != NONE) {
	    composing = NONE;
	    vstatus_compose(false, 0, KT_STD);
	    half_reset = true;
	}
	if (half_reset) {
	    return;
	}
    }

    /* Always reset scrolling. */
    scroll_to_bottom();

    /* Otherwise, if not connected, reset is a no-op. */
    if (!CONNECTED) {
	insert_mode(false);
	return;
    }

    /* Set insert mode according to the default. */
    insert_mode(IN_3270 && toggled(ALWAYS_INSERT));

    /*
     * Remove any deferred keyboard unlock.  We will either unlock the
     * keyboard now, or want to defer further into the future.
     */
    if ((kybdlock & KL_DEFERRED_UNLOCK) && unlock_id != NULL_IOID) {
	RemoveTimeOut(unlock_id);
	unlock_id = NULL_IOID;
    }

    /*
     * If explicit (from the keyboard), unlock the keyboard now.
     * Otherwise (from the host), schedule a deferred keyboard unlock.
     */
    if (explicit
	|| ft_state != FT_NONE
	|| !appres.unlock_delay
	|| (unlock_delay_time != 0 && (time(NULL) - unlock_delay_time) > 1)
	|| !appres.unlock_delay_ms) {
	kybdlock_clr(explicit? ~KL_BID: ~(KL_FT | KL_BID), "do_reset");
    } else if (kybdlock &
(KL_DEFERRED_UNLOCK | KL_OIA_TWAIT | KL_OIA_LOCKED | KL_AWAITING_FIRST)) {
	kybdlock_clr(~KL_DEFERRED_UNLOCK, "do_reset");
	kybdlock_set(KL_DEFERRED_UNLOCK, "do_reset");
	unlock_id = AddTimeOut(appres.unlock_delay_ms, defer_unlock);
	vtrace("Deferring keyboard unlock %dms\n", appres.unlock_delay_ms);
    }

    /* Clean up other modes. */
    vstatus_reset();
    mcursor_normal();
    composing = NONE;
    vstatus_compose(false, 0, KT_STD);
    if (explicit) {
	ctlr_reset();
    }
}

static bool
Reset_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnReset, ia, argc, argv);
    if (check_argc(AnReset, argc, 0, 0) < 0) {
	return false;
    }
    do_reset(true);
    return true;
}

/*
 * Move to first unprotected field on screen.
 */
static bool
Home_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnHome, ia, argc, argv);
    if (check_argc(AnHome, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnHome);
    if (IN_NVT) {
	nvt_send_home();
	return true;
    }
    if (!formatted) {
	cursor_move(0);
	return true;
    }
    cursor_move(next_unprotected(ROWS*COLS-1));
    return true;
}

/*
 * Cursor left 1 position.
 */
static void
do_left(void)
{
    register int baddr;
    enum dbcs_state d;

    baddr = cursor_addr;
    DEC_BA(baddr);
    d = ctlr_dbcs_state(baddr);
    if (IS_RIGHT(d)) {
	DEC_BA(baddr);
    } else if (IS_LEFT(d)) {
	DEC_BA(baddr);
	d = ctlr_dbcs_state(baddr);
	if (IS_RIGHT(d)) {
	    DEC_BA(baddr);
	}
    }
    cursor_move(baddr);
}

bool
Left_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnLeft, ia, argc, argv);
    if (check_argc(AnLeft, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnLeft);
    if (IN_NVT) {
	nvt_send_left();
	return true;
    }
    if (!flipped) {
	do_left();
    } else {
	int baddr;

	baddr = cursor_addr;
	INC_BA(baddr);
	cursor_move(baddr);
    }
    return true;
}

/*
 * Delete char key.
 * Returns "true" if succeeds, "false" otherwise.
 */
static bool
do_delete(void)
{
    register int baddr, end_baddr;
    int xaddr;
    register unsigned char fa;
    int ndel;
    register int i;

    baddr = cursor_addr;

    /* Can't delete a field attribute. */
    fa = get_field_attribute(baddr);
    if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
	return operator_error(KL_OERR_PROTECTED, true);
    }
    if (ea_buf[baddr].ec == EBC_so || ea_buf[baddr].ec == EBC_si) {
	/*
	 * Can't delete SO or SI, unless it's adjacent to its
	 * opposite.
	 */
	xaddr = baddr;
	INC_BA(xaddr);
	if (ea_buf[xaddr].ec == SOSI(ea_buf[baddr].ec)) {
	    ndel = 2;
	} else {
	    return operator_error(KL_OERR_PROTECTED, true);
	}
    } else if (IS_DBCS(ea_buf[baddr].db)) {
	if (IS_RIGHT(ea_buf[baddr].db)) {
	    DEC_BA(baddr);
	}
	ndel = 2;
    } else {
	ndel = 1;
    }

    /* Find next fa */
    if (formatted) {
	end_baddr = baddr;
	do {
	    INC_BA(end_baddr);
	    if (ea_buf[end_baddr].fa) {
		break;
	    }
	} while (end_baddr != baddr);
	DEC_BA(end_baddr);
    } else {
	end_baddr = (ROWS * COLS) - 1;
    }

    /* Shift the remainder of the field left. */
    if (end_baddr > baddr) {
	ctlr_bcopy(baddr + ndel, baddr, end_baddr - (baddr + ndel) + 1, 0);
    } else if (end_baddr != baddr) {
	/* XXX: Need to verify this. */
	ctlr_bcopy(baddr + ndel, baddr,
		((ROWS * COLS) - 1) - (baddr + ndel) + 1, 0);
	ctlr_bcopy(0, (ROWS * COLS) - ndel, ndel, 0);
	ctlr_bcopy(ndel, 0, end_baddr - ndel + 1, 0);
    }

    /* NULL fill at the end. */
    for (i = 0; i < ndel; i++) {
	ctlr_add(end_baddr - i, EBC_null, 0);
    }

    /* Set the MDT for this field. */
    mdt_set(cursor_addr);

    /* Patch up the DBCS state for display. */
    ctlr_dbcs_postprocess();
    return true;
}

static bool
Delete_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnDelete, ia, argc, argv);
    if (check_argc(AnDelete, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock) {
	enq_ta(AnDelete, NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	net_sendc('\177');
	return true;
    }
    if (!do_delete()) {
	return true;
    }
    if (toggled(REVERSE_INPUT)) {
	int baddr = cursor_addr;

	DEC_BA(baddr);
	if (!ea_buf[baddr].fa) {
	    cursor_move(baddr);
	}
    }
    return true;
}

/*
 * 3270-style backspace.
 */
static bool
BackSpace_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnBackSpace, ia, argc, argv);
    if (check_argc(AnBackSpace, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock) {
	enq_ta(AnBackSpace, NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	linemode_send_erase();
	return true;
    }
    if (toggled(REVERSE_INPUT)) {
	do_delete();
    } else if (!flipped) {
	do_left();
    } else {
	int baddr;

	baddr = cursor_addr;
	DEC_BA(baddr);
	cursor_move(baddr);
    }
    return true;
}

/*
 * Destructive backspace, like Unix "erase".
 */
static void
do_erase(void)
{
    int baddr, faddr;
    enum dbcs_state d;

    baddr = cursor_addr;
    faddr = find_field_attribute(baddr);
    if (faddr == baddr || FA_IS_PROTECTED(ea_buf[baddr].fa)) {
	operator_error(KL_OERR_PROTECTED, true);
    }
    if (baddr && faddr == baddr - 1) {
	return;
    }
    do_left();

    /*
     * If we are now on an SI, move left again.
     */
    if (ea_buf[cursor_addr].ec == EBC_si) {
	baddr = cursor_addr;
	DEC_BA(baddr);
	cursor_move(baddr);
    }

    /*
     * If we landed on the right-hand side of a DBCS character, move to the
     * left-hand side.
     * This ensures that if this is the end of a DBCS subfield, we will
     * land on the SI, instead of on the character following.
     */
    d = ctlr_dbcs_state(cursor_addr);
    if (IS_RIGHT(d)) {
	baddr = cursor_addr;
	DEC_BA(baddr);
	cursor_move(baddr);
    }

    /*
     * Try to delete this character.
     */
    if (!do_delete()) {
	return;
    }

    /*
     * If we've just erased the last character of a DBCS subfield, erase
     * the SO/SI pair as well.
     */
    baddr = cursor_addr;
    DEC_BA(baddr);
    if (ea_buf[baddr].ec == EBC_so && ea_buf[cursor_addr].ec == EBC_si) {
	cursor_move(baddr);
	do_delete();
    }
}

static bool
Erase_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnErase, ia, argc, argv);
    if (check_argc(AnErase, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock) {
	enq_ta(AnErase, NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	linemode_send_erase();
	return true;
    }
    if (toggled(REVERSE_INPUT)) {
	do_delete();
    } else {
	do_erase();
    }
    return true;
}

/*
 * Cursor right 1 position.
 */
bool
Right_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    enum dbcs_state d;

    action_debug(AnRight, ia, argc, argv);
    if (check_argc(AnRight, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnRight);
    if (IN_NVT) {
	nvt_send_right();
	return true;
    }
    if (!flipped) {
	baddr = cursor_addr;
	INC_BA(baddr);
	d = ctlr_dbcs_state(baddr);
	if (IS_RIGHT(d)) {
	    INC_BA(baddr);
	}
	cursor_move(baddr);
    } else {
	do_left();
    }
    return true;
}

/*
 * Cursor left 2 positions.
 */
static bool
Left2_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    enum dbcs_state d;

    action_debug(AnLeft2, ia, argc, argv);
    if (check_argc(AnLeft2, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnLeft2);
    if (IN_NVT) {
	return false;
    }
    baddr = cursor_addr;
    DEC_BA(baddr);
    d = ctlr_dbcs_state(baddr);
    if (IS_LEFT(d)) {
	DEC_BA(baddr);
    }
    DEC_BA(baddr);
    d = ctlr_dbcs_state(baddr);
    if (IS_LEFT(d)) {
	DEC_BA(baddr);
    }
    cursor_move(baddr);
    return true;
}

/*
 * Cursor to previous word.
 */
static bool
PreviousWord_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    int baddr0;
    unsigned char c;
    bool prot;

    action_debug(AnPreviousWord, ia, argc, argv);
    if (check_argc(AnPreviousWord, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock) {
	enq_ta(AnPreviousWord, NULL, NULL);
	return true;
    }
    if (IN_NVT || !formatted) {
	return false;
    }

    baddr = cursor_addr;
    prot = FA_IS_PROTECTED(get_field_attribute(baddr));

    /* Skip to before this word, if in one now. */
    if (!prot) {
	c = ea_buf[baddr].ec;
	while (!ea_buf[baddr].fa && c != EBC_space && c != EBC_null) {
	    DEC_BA(baddr);
	    if (baddr == cursor_addr) {
		return true;
	    }
	    c = ea_buf[baddr].ec;
	}
    }
    baddr0 = baddr;

    /* Find the end of the preceding word. */
    do {
	c = ea_buf[baddr].ec;
	if (ea_buf[baddr].fa) {
	    DEC_BA(baddr);
	    prot = FA_IS_PROTECTED(get_field_attribute(baddr));
	    continue;
	}
	if (!prot && c != EBC_space && c != EBC_null) {
	    break;
	}
	DEC_BA(baddr);
    } while (baddr != baddr0);

    if (baddr == baddr0) {
	return true;
    }

    /* Go it its front. */
    for (;;) {
	DEC_BA(baddr);
	c = ea_buf[baddr].ec;
	if (ea_buf[baddr].fa || c == EBC_space || c == EBC_null) {
	    break;
	}
    }
    INC_BA(baddr);
    cursor_move(baddr);
    return true;
}

/*
 * Cursor right 2 positions.
 */
static bool
Right2_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    enum dbcs_state d;

    action_debug(AnRight2, ia, argc, argv);
    if (check_argc(AnRight2, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnRight2);
    if (IN_NVT) {
	return false;
    }
    baddr = cursor_addr;
    INC_BA(baddr);
    d = ctlr_dbcs_state(baddr);
    if (IS_RIGHT(d)) {
	INC_BA(baddr);
    }
    INC_BA(baddr);
    d = ctlr_dbcs_state(baddr);
    if (IS_RIGHT(d)) {
	INC_BA(baddr);
    }
    cursor_move(baddr);
    return true;
}

/* Find the next unprotected word, or -1 */
static int
nu_word(int baddr)
{
    int baddr0 = baddr;
    unsigned char c;
    bool prot;

    prot = FA_IS_PROTECTED(get_field_attribute(baddr));

    do {
	c = ea_buf[baddr].ec;
	if (ea_buf[baddr].fa) {
	    prot = FA_IS_PROTECTED(ea_buf[baddr].fa);
	} else if (!prot && c != EBC_space && c != EBC_null) {
	    return baddr;
	}
	INC_BA(baddr);
    } while (baddr != baddr0);

    return -1;
}

/* Find the next word in this field, or -1 */
static int
nt_word(int baddr)
{
    int baddr0 = baddr;
    unsigned char c;
    bool in_word = true;

    do {
	c = ea_buf[baddr].ec;
	if (ea_buf[baddr].fa)
	    return -1;
	if (in_word) {
	    if (c == EBC_space || c == EBC_null) {
		in_word = false;
	    }
	} else {
	    if (c != EBC_space && c != EBC_null) {
		return baddr;
	    }
	}
	INC_BA(baddr);
    } while (baddr != baddr0);

	return -1;
}


/*
 * Cursor to next unprotected word.
 */
static bool
NextWord_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    unsigned char c;

    action_debug(AnNextWord, ia, argc, argv);
    if (check_argc(AnNextWord, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock) {
	enq_ta(AnNextWord, NULL, NULL);
	return true;
    }
    if (IN_NVT || !formatted) {
	return false;
    }

    /* If not in an unprotected field, go to the next unprotected word. */
    if (ea_buf[cursor_addr].fa ||
	FA_IS_PROTECTED(get_field_attribute(cursor_addr))) {
	baddr = nu_word(cursor_addr);
	if (baddr != -1) {
	    cursor_move(baddr);
	}
	return true;
    }

    /* If there's another word in this field, go to it. */
    baddr = nt_word(cursor_addr);
    if (baddr != -1) {
	cursor_move(baddr);
	return true;
    }

    /* If in a word, go to just after its end. */
    c = ea_buf[cursor_addr].ec;
    if (c != EBC_space && c != EBC_null) {
	baddr = cursor_addr;
	do {
	    c = ea_buf[baddr].ec;
	    if (c == EBC_space || c == EBC_null) {
		cursor_move(baddr);
		return true;
	    } else if (ea_buf[baddr].fa) {
		baddr = nu_word(baddr);
		if (baddr != -1) {
		    cursor_move(baddr);
		}
		return true;
	    }
	    INC_BA(baddr);
	} while (baddr != cursor_addr);
    } else {
	/* Otherwise, go to the next unprotected word. */
	baddr = nu_word(cursor_addr);
	if (baddr != -1) {
	    cursor_move(baddr);
	}
    }
    return true;
}

/*
 * Cursor up 1 position.
 */
bool
Up_action(ia_t ia, unsigned argc, const char **argv)
{
    register int baddr;

    action_debug(AnUp, ia, argc, argv);
    if (check_argc(AnUp, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnUp);
    if (IN_NVT) {
	nvt_send_up();
	return true;
    }
    baddr = cursor_addr - COLS;
    if (baddr < 0) {
	baddr = (cursor_addr + (ROWS * COLS)) - COLS;
    }
    cursor_move(baddr);
    return true;
}

/*
 * Cursor down 1 position.
 */
bool
Down_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;

    action_debug(AnDown, ia, argc, argv);
    if (check_argc(AnDown, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnDown);
    if (IN_NVT) {
	nvt_send_down();
	return true;
    }
    baddr = (cursor_addr + COLS) % (COLS * ROWS);
    cursor_move(baddr);
    return false;
}

/*
 * Cursor to first field on next line or any lines after that.
 */
static bool
Newline_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr, faddr;
    unsigned char fa;

    action_debug(AnNewline, ia, argc, argv);
    if (check_argc(AnNewline, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock) {
	enq_ta(AnNewline, NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	net_sendc('\n');
	return true;
    }
    baddr = (cursor_addr + COLS) % (COLS * ROWS);	/* down */
    baddr = (baddr / COLS) * COLS;			/* 1st col */
    faddr = find_field_attribute(baddr);
    fa = ea_buf[faddr].fa;
    if (faddr != baddr && !FA_IS_PROTECTED(fa)) {
	cursor_move(baddr);
    } else {
	cursor_move(next_unprotected(baddr));
    }
    return true;
}

/*
 * DUP key
 */
static bool
Dup_action(ia_t ia, unsigned argc, const char **argv)
{
    bool oerr_fail = !IA_IS_KEY(ia);
    bool consumed = false;

    action_debug(AnDup, ia, argc, argv);
    if (check_argc(AnDup, argc, 0, 1) < 0) {
	return false;
    }
    if (argc > 0) {
	if (!strcasecmp(argv[0], KwFailOnError)) {
	    oerr_fail = true;
	} else if (strcasecmp(argv[0], KwNoFailOnError)) {
	    return action_args_are(AnDup, KwFailOnError, KwNoFailOnError,
		    NULL);
	    return false;
	}
    }
    if (kybdlock) {
	enq_ta(AnDup, oerr_fail ? KwFailOnError : KwNoFailOnError, NULL);
	return true;
    }
    if (IN_NVT) {
	return false;
    }
    if (key_Character(EBC_dup, false, false, oerr_fail, &consumed)) {
	if (consumed) {
	    cursor_move(next_unprotected(cursor_addr));
	}
	return true;
    } else {
	return false;
    }
}

/*
 * FM key
 */
static bool
FieldMark_action(ia_t ia, unsigned argc, const char **argv)
{
    bool oerr_fail = !IA_IS_KEY(ia);

    action_debug(AnFieldMark, ia, argc, argv);
    if (check_argc(AnFieldMark, argc, 0, 1) < 0) {
	return false;
    }
    if (argc > 0) {
	if (!strcasecmp(argv[0], KwFailOnError)) {
	    oerr_fail = true;
	} else if (strcasecmp(argv[0], KwNoFailOnError)) {
	    return action_args_are(AnFieldMark, KwFailOnError, KwNoFailOnError,
		    NULL);
	}
    }
    if (kybdlock) {
	enq_ta(AnFieldMark, oerr_fail ? KwFailOnError : KwNoFailOnError, NULL);
	return true;
    }
    if (IN_NVT) {
	return false;
    }
    return key_Character(EBC_fm, false, false, oerr_fail, NULL);
}

/*
 * Vanilla AID keys.
 */
static bool
Enter_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnEnter, ia, argc, argv);
    if (check_argc(AnEnter, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock & KL_OIA_MINUS) {
	return false;
    } else if (IS_LOCKED(ia)) {
	enq_ta(AnEnter, NULL, NULL);
    } else {
	key_AID(AID_ENTER);
    }
    return true;
}

static bool
SysReq_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnSysReq, ia, argc, argv);
    if (check_argc(AnSysReq, argc, 0, 0) < 0) {
	return false;
    }
    if (IN_NVT) {
	return false;
    }
    if (IN_E) {
	net_abort();
    } else {
	if (kybdlock & KL_OIA_MINUS) {
	    return false;
	} else if (kybdlock) {
	    enq_ta(AnSysReq, NULL, NULL);
	} else {
	    key_AID(AID_SYSREQ);
	}
    }
    return true;
}

/*
 * Clear AID key
 */
static bool
Clear_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnClear, ia, argc, argv);
    if (check_argc(AnClear, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock & KL_OIA_MINUS) {
	return false;
    }
    if (kybdlock && FULL_SESSION) {
	enq_ta(AnClear, NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	nvt_send_clear();
	return true;
    }
    buffer_addr = 0;
    ctlr_clear(true);
    cursor_move(0);
    if (IN_3270 || IN_SSCP) {
	key_AID(AID_CLEAR);
    }
    return true;
}

/*
 * Cursor Select key (light pen simulator).
 */
void
lightpen_select(int baddr)
{
    int faddr;
    register unsigned char fa;
    int designator;
    int designator2;

    faddr = find_field_attribute(baddr);
    fa = ea_buf[faddr].fa;
    if (!FA_IS_SELECTABLE(fa)) {
	vtrace("  lightpen select on non-selectable field\n");
	ring_bell();
	    return;
    }
    designator = faddr;
    INC_BA(designator);

    if (dbcs) {
	if (ea_buf[baddr].cs == CS_DBCS) {
	    designator2 = designator;
	    INC_BA(designator2);
	    if ((ea_buf[designator].db != DBCS_LEFT &&
		 ea_buf[designator].db != DBCS_LEFT_WRAP) &&
		(ea_buf[designator2].db != DBCS_RIGHT &&
		 ea_buf[designator2].db != DBCS_RIGHT_WRAP)) {
		    ring_bell();
		    return;
	    }
	    if (ea_buf[designator].ec == 0x42 &&
		    ea_buf[designator2].ec == EBC_greater) {
		ctlr_add(designator2, EBC_question, CS_DBCS);
		mdt_clear(faddr);
	    } else if (ea_buf[designator].ec == 0x42 &&
		       ea_buf[designator2].ec == EBC_question) {
		ctlr_add(designator2, EBC_greater, CS_DBCS);
		mdt_clear(faddr);
	    } else if ((ea_buf[designator].ec == EBC_space &&
			ea_buf[designator2].ec == EBC_space) ||
		       (ea_buf[designator].ec == EBC_null &&
			ea_buf[designator2].ec == EBC_null)) {
		ctlr_add(designator2, EBC_greater, CS_DBCS);
		mdt_set(faddr);
		key_AID(AID_SELECT);
	    } else if (ea_buf[designator].ec == 0x42 &&
		       ea_buf[designator2].ec == EBC_ampersand) {
		mdt_set(faddr);
		key_AID(AID_ENTER);
	    } else {
		ring_bell();
	    }
	    return;
	}
    } 

    switch (ea_buf[designator].ec) {
    case EBC_greater:		/* > */
	ctlr_add(designator, EBC_question, 0); /* change to ? */
	mdt_clear(faddr);
	break;
    case EBC_question:		/* ? */
	ctlr_add(designator, EBC_greater, 0);	/* change to > */
	mdt_set(faddr);
	break;
    case EBC_space:		/* space */
    case EBC_null:		/* null */
	mdt_set(faddr);
	key_AID(AID_SELECT);
	break;
    case EBC_ampersand:		/* & */
	mdt_set(faddr);
	key_AID(AID_ENTER);
	break;
    default:
	ring_bell();
	break;
    }
}

/*
 * Cursor Select key (light pen simulator) -- at the current cursor location.
 */
static bool
CursorSelect_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnCursorSelect, ia, argc, argv);
    if (check_argc(AnCursorSelect, argc, 0, 0) < 0) {
	return false;
    }
    if (kybdlock) {
	enq_ta(AnCursorSelect, NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	return false;
    }
    lightpen_select(cursor_addr);
    return true;
}

/*
 * Erase End Of Field Key.
 */
static bool
EraseEOF_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    unsigned char fa;
    enum dbcs_state d;
    enum dbcs_why why = DBCS_FIELD;

    action_debug(AnEraseEOF, ia, argc, argv);
    if (check_argc(AnEraseEOF, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnEraseEOF);
    if (IN_NVT) {
	return false;
    }
    baddr = cursor_addr;
    fa = get_field_attribute(baddr);
    if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
	return operator_error(KL_OERR_PROTECTED, true);
    }
    if (formatted) {	/* erase to next field attribute */
	do {
	    ctlr_add(baddr, EBC_null, 0);
	    INC_BA(baddr);
	} while (!ea_buf[baddr].fa);
	mdt_set(cursor_addr);
    } else {	/* erase to end of screen */
	do {
	    ctlr_add(baddr, EBC_null, 0);
	    INC_BA(baddr);
	} while (baddr != 0);
    }

    /* If the cursor was in a DBCS subfield, re-create the SI. */
    d = ctlr_lookleft_state(cursor_addr, &why);
    if (IS_DBCS(d) && why == DBCS_SUBFIELD) {
	if (d == DBCS_RIGHT) {
	    baddr = cursor_addr;
	    DEC_BA(baddr);
	    ea_buf[baddr].ec = EBC_si;
	} else {
	    ea_buf[cursor_addr].ec = EBC_si;
	}
    }
    ctlr_dbcs_postprocess();
    return true;
}

/*
 * Erase all Input Key.
 */
static bool
EraseInput_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr, sbaddr;
    unsigned char fa;
    bool f;

    action_debug(AnEraseInput, ia, argc, argv);
    if (check_argc(AnEraseInput, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnEraseInput);
    if (IN_NVT) {
	return false;
    }
    if (formatted) {
	    /* find first field attribute */
	baddr = 0;
	do {
	    if (ea_buf[baddr].fa) {
		break;
	    }
	    INC_BA(baddr);
	} while (baddr != 0);
	sbaddr = baddr;
	f = false;
	do {
	    fa = ea_buf[baddr].fa;
	    if (!FA_IS_PROTECTED(fa)) {
		mdt_clear(baddr);
		do {
		    INC_BA(baddr);
		    if (!f) {
			cursor_move(baddr);
			f = true;
		    }
		    if (!ea_buf[baddr].fa) {
			ctlr_add(baddr, EBC_null, 0);
		    }
		} while (!ea_buf[baddr].fa);
	    } else {	/* skip protected */
		do {
		    INC_BA(baddr);
		} while (!ea_buf[baddr].fa);
	    }
	} while (baddr != sbaddr);
	if (!f) {
	    cursor_move(0);
	}
    } else {
	ctlr_clear(true);
	cursor_move(0);
    }

    /* Synchronize the DBCS state. */
    ctlr_dbcs_postprocess();

    return true;
}

/*
 * Delete word key.  Backspaces the cursor until it hits the front of a word,
 * deletes characters until it hits a blank or null, and deletes all of these
 * but the last.
 *
 * Which is to say, does a ^W.
 */
static bool
DeleteWord_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    unsigned char fa;

    action_debug(AnDeleteWord, ia, argc, argv);
    if (check_argc(AnDeleteWord, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnDeleteWord);
    if (IN_NVT) {
	linemode_send_werase();
	return true;
    }
    if (!formatted) {
	return false;
    }

    baddr = cursor_addr;
    fa = get_field_attribute(baddr);

    /* Make sure we're on a modifiable field. */
    if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
	return operator_error(KL_OERR_PROTECTED, true);
    }

    /* Backspace over any spaces to the left of the cursor. */
    for (;;) {
	baddr = cursor_addr;
	DEC_BA(baddr);
	if (ea_buf[baddr].fa) {
	    return true;
	}
	if (ea_buf[baddr].ec == EBC_null || ea_buf[baddr].ec == EBC_space) {
	    do_erase();
	} else {
	    break;
	}
    }

    /* Backspace until the character to the left of the cursor is blank. */
    for (;;) {
	baddr = cursor_addr;
	DEC_BA(baddr);
	if (ea_buf[baddr].fa) {
	    return true;
	}
	if (ea_buf[baddr].ec == EBC_null || ea_buf[baddr].ec == EBC_space) {
	    break;
	} else {
	    do_erase();
	}
    }
    return true;
}


/*
 * Delete field key.  Similar to EraseEOF, but it wipes out the entire field
 * rather than just to the right of the cursor, and it leaves the cursor at
 * the front of the field.
 *
 * Which is to say, does a ^U.
 */
static bool
DeleteField_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    unsigned char fa;

    action_debug(AnDeleteField, ia, argc, argv);
    if (check_argc(AnDeleteField, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnDeleteField);
    if (IN_NVT) {
	linemode_send_kill();
	return true;
    }
    if (!formatted) {
	return false;
    }

    baddr = cursor_addr;
    fa = get_field_attribute(baddr);
    if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
	return operator_error(KL_OERR_PROTECTED, true);
    }
    while (!ea_buf[baddr].fa) {
	DEC_BA(baddr);
    }
    INC_BA(baddr);
    mdt_set(cursor_addr);
    cursor_move(baddr);
    while (!ea_buf[baddr].fa) {
	ctlr_add(baddr, EBC_null, 0);
	INC_BA(baddr);
    }
    return true;
}

/*
 * Set insert mode key.
 */
static bool
Insert_action(ia_t ia, unsigned argc, const char **argv)
{
    const char *set_argv[3] = { ResInsertMode, ResTrue, NULL };

    action_debug(AnInsert, ia, argc, argv);
    if (check_argc(AnInsert, argc, 0, 0) < 0) {
	return false;
    }
    return Set_action(ia, 2, set_argv);
}

/*
 * Toggle insert mode key.
 */
static bool
ToggleInsert_action(ia_t ia, unsigned argc, const char **argv)
{
    const char *toggle_argv[2] = { ResInsertMode, NULL };

    action_debug(AnToggleInsert, ia, argc, argv);
    if (check_argc(AnToggleInsert, argc, 0, 0) < 0) {
	return false;
    }
    return Toggle_action(ia, 1, toggle_argv);
}

/*
 * Toggle reverse-input mode key.
 */
static bool
ToggleReverse_action(ia_t ia, unsigned argc, const char **argv)
{
    const char *toggle_argv[2] = { ResReverseInputMode, NULL };

    action_debug(AnToggleReverse, ia, argc, argv);
    if (check_argc(AnToggleReverse, argc, 0, 0) < 0) {
	return false;
    }
    return Toggle_action(ia, 1, toggle_argv);
}

/*
 * Move the cursor to the first blank after the last nonblank in the
 * field, or if the field is full, to the last character in the field.
 */
static bool
FieldEnd_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr, faddr;
    unsigned char fa, c;
    int	last_nonblank = -1;

    action_debug(AnFieldEnd, ia, argc, argv);
    if (check_argc(AnFieldEnd, argc, 0, 0) < 0) {
	return false;
    }
    OERR_CLEAR_OR_ENQ(AnFieldEnd);
    if (IN_NVT) {
	return false;
    }
    if (!formatted) {
	return false;
    }
    baddr = cursor_addr;
    faddr = find_field_attribute(baddr);
    fa = ea_buf[faddr].fa;
    if (faddr == baddr || FA_IS_PROTECTED(fa)) {
	return true;
    }

    baddr = faddr;
    while (true) {
	INC_BA(baddr);
	c = ea_buf[baddr].ec;
	if (ea_buf[baddr].fa) {
	    break;
	}
	if (c != EBC_null && c != EBC_space) {
	    last_nonblank = baddr;
	}
    }

    if (last_nonblank == -1) {
	baddr = faddr;
	INC_BA(baddr);
    } else {
	baddr = last_nonblank;
	INC_BA(baddr);
	if (ea_buf[baddr].fa) {
	    baddr = last_nonblank;
	}
    }
    cursor_move(baddr);
    return true;
}

/*
 * Common MoveCursor/MoveCursor1 logic. Moves to a specific location.
 */
static bool
MoveCursor_common(int origin, const char *name, ia_t ia, unsigned argc,
	const char **argv)
{
    int baddr;
    int row, col;

    action_debug(name, ia, argc, argv);
    if (check_argc(name, argc, 1, 2) < 0) {
	return false;
    }

    if (IN_NVT) {
	popup_an_error("%s() is not valid in NVT mode", name);
	return false;
    }

    if (kybdlock) {
	enq_ta(name, argv[0], argv[1]);
	return true;
    }

    if (argc == 1) {
	baddr = atoi(argv[0]);
	if (baddr < 0 || baddr >= ROWS * COLS) {
	    popup_an_error("%s(): Invalid offset", name);
	    return false;
	}
    } else {
	row = atoi(argv[0]);
	if (row < 0) {
	    if (-row > ROWS) {
		popup_an_error("%s(): Invalid row", name);
		return false;
	    }
	    row += ROWS + origin;
	} else if (row < origin) {
	    row = origin;
	} else if (row > ROWS - !origin) {
	    popup_an_error("%s(): Invalid row", name);
	    return false;
	}
	col = atoi(argv[1]);
	if (col < 0) {
	    if (-col > COLS) {
		popup_an_error("%s(): Invalid column", name);
		return false;
	    }
	    col += COLS + origin;
	} else if (col < origin) {
	    col = origin;
	} else if (col > COLS - !origin) {
	    popup_an_error("%s(): Invalid column", name);
	    return false;
	}
	baddr = (((row - origin) * COLS) + (col - origin)) % (ROWS * COLS);
    }
    if (baddr < 0) {
	baddr = 0;
    } else if (baddr >= ROWS * COLS) {
	baddr = (ROWS * COLS) - 1;
    }
    cursor_move(baddr);

    return true;
}

/*
 * 0-origin MoveCursor action. Moves to a specific location.
 * For backwards compatibility.
 */
static bool
MoveCursor_action(ia_t ia, unsigned argc, const char **argv)
{
    return MoveCursor_common(0, AnMoveCursor, ia, argc, argv);
}

/*
 * 1-origin MoveCursor action. Moves to a specific location.
 */
static bool
MoveCursor1_action(ia_t ia, unsigned argc, const char **argv)
{
    return MoveCursor_common(1, AnMoveCursor1, ia, argc, argv);
}

/*
 * Key action.
 */
static bool
Key_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned i;
    ks_t k;
    enum keytype keytype;
    ucs4_t ucs4;
    bool oerr_fail = !IA_IS_KEY(ia);

    action_debug(AnKey, ia, argc, argv);

    /*
     * Allow FailOnError or NoFailOnError anywhere, but only pay attention to
     * the last.
     */
    for (i = 0; i < argc; i++) {
	if (!strcasecmp(argv[i], KwFailOnError)) {
	    oerr_fail = true;
	} else if (!strcasecmp(argv[0], KwNoFailOnError)) {
	    oerr_fail = false;
	}
    }

    for (i = 0; i < argc; i++) {
	const char *s = argv[i];

	if (!strcasecmp(s, KwFailOnError) || !strcasecmp(s, KwNoFailOnError)) {
	    continue;
	}
	k = my_string_to_key(s, &keytype, &ucs4);
	if (k == KS_NONE && !ucs4) {
	    popup_an_error(AnKey "(): Nonexistent or invalid name: %s", s);
	    continue;
	}
	if (k & ~0xff) {
	    /*
	     * Can't pass symbolic names that aren't in the range 0x01..0xff.
	     */
	    popup_an_error(AnKey "(): Invalid name: %s", s);
	    continue;
	}
	if (k != KS_NONE) {
	    key_UCharacter(k, keytype, ia, oerr_fail);
	} else {
	    key_UCharacter(ucs4, keytype, ia, oerr_fail);
	}
    }
    return true;
}

/*
 * String action.
 */
static bool
String_action(ia_t ia, unsigned argc, const char **argv)
{
    bool subst = false;
    unsigned i;
    size_t len = 0;
    char *s = NULL;

    action_debug(AnString, ia, argc, argv);

    /* Pick off the '-subst' option. */
    if (argc > 0 && !strcasecmp(argv[0], KwSubst)) {
	subst = true;
    }

    /* Concatenate and optionally substitute. */
    for (i = !!subst; i < argc; i++) {
	char *sb = subst? do_subst(argv[i], DS_VARS): NULL;
	const char *t = (sb != NULL)? sb: argv[i];

	if (strlen(t) > 0) {
	    if (s == NULL) {
		s = NewString(t);
		len = strlen(t) + 1;
	    } else {
		len += strlen(t);
		s = Realloc(s, len);
		strcat(s, t);
	    }
	}
	if (sb != NULL) {
	    Free(sb);
	}
    }

    if (!len) {
	return true;
    }

    /* Set a pending string. */
    ps_set(s, false, ia == IA_HTTPD);
    Free(s);
    return true;
}

/*
 * Return the value of a hexadecimal nybble.
 */
static int
nybble(unsigned char c)
{
    static char hex_digits[] = "0123456789abcdef";
    char *index = strchr(hex_digits, tolower((int)c));

    return (index == NULL)? -1: (int)(index - hex_digits);
}

/*
 * HexString action.
 */
static bool
HexString_action(ia_t ia, unsigned argc, const char **argv)
{
    bool is_ascii = false;
    unsigned i, j;
    size_t len = 0;
    char *s;
    size_t sl;
    const char *t;
    int out;

    action_debug(AnHexString, ia, argc, argv);

    /* Pick off the -Ascii option. */
    if (argc > 0 && !strcasecmp(argv[0], KwDashAscii)) {
	is_ascii = true;
	argc--;
	argv++;
    }

    /* Determine the total length of the strings. */
    for (i = 0; i < argc; i++) {
	t = argv[i];
	if (!strncmp(t, "0x", 2) || !strncmp(t, "0X", 2)) {
	    t += 2;
	}
	sl = strlen(t);
	for (j = 0; j < (unsigned)sl; j++) {
	    if (nybble((unsigned char)t[j]) < 0) {
		popup_an_error(AnHexString "(): Invalid hex character");
		return false;
	    }
	}
	len += sl;
    }
    if (!len) {
	return true;
    }
    if (len % 2) {
	popup_an_error(AnHexString "(): Odd number of nybbles");
	return false;
    }

    /* Allocate a block of memory and copy them in. */
    s = Malloc(len + 1);
    *s = '\0';
    out = 0;
    for (i = 0; i < argc; i++) {
	t = argv[i];
	if (!strncmp(t, "0x", 2) || !strncmp(t, "0X", 2)) {
	    t += 2;
	}
	if (is_ascii) {
	    sl = strlen(t);
	    for (j = 0; j < (unsigned)sl; j += 2) {
		int u = nybble((unsigned char)t[j]);
		int l = nybble((unsigned char)t[j + 1]);

		s[out++] = (char)((u * 16) + l);
	    }
	} else {
	    strcat(s, t);
	}
    }

    /* Set a pending string. */
    if (is_ascii) {
	s[out] = '\0';
	ps_set(s, false, ia == IA_HTTPD);
    } else {
	ps_set(s, true, ia == IA_HTTPD);
    }
    Free(s);
    return true;
}

/*
 * PasteString action.
 */
static bool
PasteString_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned i;
    size_t len = 0;
    char *s;
    const char *t;

    action_debug(AnPasteString, ia, argc, argv);
    if (check_argc(AnPasteString, argc, 1, 2) < 0) {
	return false;
    }

    /* Determine the total length of the strings. */
    for (i = 0; i < argc; i++) {
	t = argv[i];
	if (!strncasecmp(t, "0x", 2)) {
	    t += 2;
	}
	len += strlen(t);
    }
    if (!len) {
	return true;
    }

    /* Allocate a block of memory and copy them in. */
    s = Malloc(len + 1);
    *s = '\0';
    for (i = 0; i < argc; i++) {
	t = argv[i];
	if (!strncasecmp(t, "0x", 2)) {
	    t += 2;
	}
	strcat(s, t);
    }

    /* Set a pending string. */
    push_string(s, true, true, ia == IA_HTTPD);
    Free(s);
    return true;
}

/*
 * Dual-mode action for the "asciicircum" ("^") key:
 *  If in NVT mode, pass through untranslated.
 *  If in 3270 mode, translate to "notsign".
 * This action is obsoleted by the use of 3270-mode and NVT-mode keymaps, but
 * is still defined here for backwards compatibility with old keymaps.
 */
static bool
CircumNot_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnCircumNot, ia, argc, argv);
    if (check_argc(AnCircumNot, argc, 0, 0) < 0) {
	return false;
    }

    if (IN_3270 && composing == NONE) {
	key_UCharacter(0xac, KT_STD, ia, !IA_IS_KEY(ia));
    } else {
	key_UCharacter('^', KT_STD, ia, !IA_IS_KEY(ia));
    }
    return true;
}

/* PA key action for String actions */
static void
do_pa(unsigned n)
{
    if (n < 1 || n > PA_SZ) {
	popup_an_error(AnString "(): Unknown PA key %d", n);
	return;
    }
    if (kybdlock) {
	enq_ta(AnPA, txAsprintf("%d", n), NULL);
	return;
    }
    key_AID(pa_xlate[n-1]);
}

/* PF key action for String actions */
static void
do_pf(unsigned n)
{
    if (n < 1 || n > PF_SZ) {
	popup_an_error(AnString "(): Unknown PF key %d", n);
	return;
    }
    if (kybdlock) {
	enq_ta(AnPF, txAsprintf("%d", n), NULL);
	return;
    }
    key_AID(pf_xlate[n-1]);
}

/*
 * Set or clear the keyboard scroll lock.
 */
void
kybd_scroll_lock(bool lock)
{
    if (!IN_3270) {
	return;
    }
    if (lock) {
	kybdlock_set(KL_SCROLLED, "kybd_scroll_lock");
    } else {
	kybdlock_clr(KL_SCROLLED, "kybd_scroll_lock");
    }
}

/*
 * Move the cursor back within the legal paste area.
 * Returns a bool indicating success.
 */
static bool
remargin(int lmargin)
{
    bool ever = false;
    int baddr, b0 = 0;
    int faddr;
    unsigned char fa;

    if (toggled(OVERLAY_PASTE)) {
	/*
	 * If doing overlay paste as well, just drop down to the margin
	 * column on the next line, and don't worry about protected fields.
	 */
	baddr = ROWCOL_TO_BA(BA_TO_ROW(cursor_addr), lmargin);
	cursor_move(baddr);
	return true;
    }

    baddr = cursor_addr;
    while (BA_TO_COL(baddr) < lmargin) {
	baddr = ROWCOL_TO_BA(BA_TO_ROW(baddr), lmargin);
	if (!ever) {
	    b0 = baddr;
	    ever = true;
	}
	faddr = find_field_attribute(baddr);
	fa = ea_buf[faddr].fa;
	if (faddr == baddr || FA_IS_PROTECTED(fa)) {
	    baddr = next_unprotected(baddr);
	    if (baddr <= b0) {
		return false;
	    }
	}
    }

    cursor_move(baddr);
    return true;
}

/**
 * Run a nested action from String().
 *
 * @param[in] action	Action to run
 * @param[in] cause	Cause
 * @param[in] param	First parameter, on NULL
 */
static bool
ns_action(action_t action, enum iaction cause, const char *param)
{
    const char *args[2];

    args[0] = param;
    args[1] = NULL;
    return (*action)(cause, param != NULL, args);

}

/*
 * Pretend that a sequence of keys was entered at the keyboard.
 *
 * "Pasting" means that the sequence came from the X clipboard.  Returns are
 * ignored; newlines mean "move to beginning of next line"; tabs and formfeeds
 * become spaces.  Backslashes are not special, but ASCII ESC characters are
 * used to signify 3270 Graphic Escapes. If the NOSKIP_PASTE toggle is set,
 * then we don't do auto-skip, except at the end of the string; when the cursor
 * lands on a protected region of the screen, we treat printable characters as
 * cursor-right actions.
 *
 * "Not pasting" means that the sequence is a login string specified in the
 * hosts file, or a parameter to the String action.  Returns are "move to
 * beginning of next line"; newlines mean "Enter AID" and the termination of
 * processing the string.  Backslashes are processed as in C.
 *
 * Returns the number of unprocessed characters.
 */
size_t
emulate_uinput(const ucs4_t *ws, size_t xlen, bool pasting)
{
    enum {
	BASE, BACKSLASH, BACKX, BACKE, BACKP, BACKPA, BACKPF, OCTAL,
	HEX, EBC
    } state = BASE;
    ucs4_t literal = 0;
    int nc = 0;
    enum iaction ia = pasting ? IA_PASTE : IA_STRING;
    int orig_addr = cursor_addr;
    int orig_col = BA_TO_COL(cursor_addr);
    int last_addr = cursor_addr;
    int last_row = BA_TO_ROW(cursor_addr);
    bool just_wrapped = false;
    ucs4_t c;
    bool auto_skip = true;
    bool check_remargin = false;

    if (pasting && toggled(OVERLAY_PASTE)) {
	auto_skip = false;
    }

    /*
     * In the switch statements below, "break" generally means "consume
     * this character," while "continue" means "rescan this character."
     */
    while (xlen) {

	/*
	 * It isn't possible to unlock the keyboard from a string,
	 * so if the keyboard is locked, it's fatal
	 */
	if (kybdlock) {
	    vtrace("  keyboard locked, string dropped\n");
	    return 0;
	}

	if (pasting && IN_3270) {

	    /* Check for cursor wrap to top of screen. */
	    if (cursor_addr < orig_addr) {
		return xlen-1;		/* wrapped */
	    }

	    /* Jump cursor over left margin. */
	    if (MarginedPaste() && BA_TO_COL(cursor_addr) < orig_col) {
		if (!remargin(orig_col)) {
		    return xlen-1;
		}
	    }
	}

	if (last_addr != cursor_addr) {
	    last_addr = cursor_addr;
	    if (last_row == BA_TO_ROW(cursor_addr)) {
		just_wrapped = false;
	    } else {
		last_row = BA_TO_ROW(cursor_addr);
		just_wrapped = true;
	    }
	}

	c = *ws;

	switch (state) {
	case BASE:
	    switch (c) {
	    case '\b':
		ns_action(Left_action, ia, NULL);
		break;
	    case '\f':
		if (pasting) {
		    key_UCharacter(0x20, KT_STD, ia, true);
		} else {
		    ns_action(Clear_action, ia, NULL);
		    if (IN_3270) {
			return xlen-1;
		    }
		}
		break;
	    case '\n':
		if (pasting && !IN_NVT) {
		    if (auto_skip) {
			if (!just_wrapped) {
			    ns_action(Newline_action, ia, NULL);
			}
		    } else {
			int baddr;
			int row;

			/*
			 * Overlay paste mode: Move to the beginning of the
			 * next row, unless we just wrapped there.
			 *
			 * If this is the last pasted character, ignore it.
			 */
			if (xlen == 1) {
			    return 0;
			}
			if (!just_wrapped) {
			    row = BA_TO_ROW(cursor_addr);
			    if (row >= ROWS - 1) {
				return xlen - 1;
			    }
			    baddr = ROWCOL_TO_BA(row + 1, 0);
			    cursor_move(baddr);
			}
		    }
		    last_row = BA_TO_ROW(cursor_addr);
		    just_wrapped = false;
		} else {
		    ns_action(Enter_action, ia, NULL);
		    if (IN_3270) {
			return xlen-1;
		    }
		}
		break;
	    case '\r':
		if (!pasting) {
		    ns_action(Newline_action, ia, NULL);
		}
		break;
	    case '\t':
		ns_action(Tab_action, ia, NULL);
		break;
	    case '\\':	/* backslashes are NOT special when pasting */
		if (!pasting) {
		    state = BACKSLASH;
		} else {
		    key_UCharacter((unsigned char)c, KT_STD, ia, true);
		}
		break;
	    case UPRIV_fm: /* private-use FM */
	    case UPRIV2_fm:
		if (pasting) {
		    vtrace(" %s -> FM\n", ia_name[(int) ia]);
		    key_Character(EBC_fm, false, true, true, NULL);
		}
		break;
	    case UPRIV_dup: /* private-use DUP */
	    case UPRIV2_dup:
		if (pasting) {
		    vtrace(" %s -> DUP\n", ia_name[(int) ia]);
		    key_Character(EBC_dup, false, true, true, NULL);
		}
		break;
	    case UPRIV_eo: /* private-use EO */
		if (pasting) {
		    key_Character(EBC_eo, false, true, true, NULL);
		}
		break;
	    case UPRIV_sub: /* private-use SUB */
		if (pasting) {
		    key_Character(EBC_sub, false, true, true, NULL);
		}
		break;
	    default:
		if (pasting && (c >= UPRIV_GE_00 && c <= UPRIV_GE_ff)) {
		    /* Untranslatable CP 310 code point. */
		    key_Character(c - UPRIV_GE_00, true, ia, true, NULL);
		} else {
		    /* Ordinary text. */
		    key_UCharacter(c, KT_STD, ia, true);
		}
		break;
	    }
	    break;

	case BACKSLASH:	/* last character was a backslash */
	    switch (c) {
	    case 'a':
		popup_an_error(AnString "(): Bell not supported");
		state = BASE;
		break;
	    case 'b':
		ns_action(Left_action, ia, NULL);
		state = BASE;
		break;
	    case 'f':
		ns_action(Clear_action, ia, NULL);
		state = BASE;
		if (IN_3270) {
		    return xlen-1;
		}
		break;
	    case 'n':
		ns_action(Enter_action, ia, NULL);
		state = BASE;
		if (IN_3270) {
		    return xlen-1;
		}
		break;
	    case 'p':
		state = BACKP;
		break;
	    case 'r':
		ns_action(Newline_action, ia, NULL);
		state = BASE;
		break;
	    case 't':
		ns_action(Tab_action, ia, NULL);
		state = BASE;
		break;
	    case 'T':
		ns_action(BackTab_action, ia, NULL);
		state = BASE;
		break;
	    case 'v':
		popup_an_error(AnString "(): Vertical tab not supported");
		state = BASE;
		break;
	    case 'u':
	    case 'x':
		state = BACKX;
		break;
	    case 'e':
		state = BACKE;
		break;
	    case '\\':
		key_UCharacter((unsigned char) c, KT_STD, ia, true);
		state = BASE;
		break;
	    case '0': 
	    case '1': 
	    case '2': 
	    case '3':
	    case '4': 
	    case '5': 
	    case '6': 
	    case '7':
		state = OCTAL;
		literal = 0;
		nc = 0;
		continue;
	    default:
		state = BASE;
		continue;
	    }
	    break;

	case BACKP:	/* last two characters were "\p" */
	    switch (c) {
	    case 'a':
		literal = 0;
		nc = 0;
		state = BACKPA;
		break;
	    case 'f':
		literal = 0;
		nc = 0;
		state = BACKPF;
		break;
	    default:
		popup_an_error(AnString "(): Unknown character after \\p");
		state = BASE;
		break;
	    }
	    break;

	case BACKPF: /* last three characters were "\pf" */
	    if (nc < 2 && isdigit((unsigned char)c)) {
		literal = (literal * 10) + (c - '0');
		nc++;
	    } else if (!nc) {
		popup_an_error(AnString "(): Unknown character after \\pf");
		state = BASE;
	    } else {
		do_pf(literal);
		if (IN_3270) {
		    return xlen;
		}
		state = BASE;
		continue;
	    }
	    break;

	case BACKPA: /* last three characters were "\pa" */
	    if (nc < 1 && isdigit((unsigned char)c)) {
		literal = (literal * 10) + (c - '0');
		nc++;
	    } else if (!nc) {
		popup_an_error(AnString "(): Unknown character after \\pa");
		state = BASE;
	    } else {
		do_pa(literal);
		if (IN_3270) {
		    return xlen-1;
		}
		state = BASE;
		continue;
	    }
	    break;
	case BACKX:	/* last two characters were "\x" or "\u" */
	    if (isxdigit((unsigned char)c)) {
		state = HEX;
		literal = 0;
		nc = 0;
		continue;
	    } else {
		popup_an_error(AnString "(): Missing hex digits after \\x");
		state = BASE;
		continue;
	    }
	case BACKE:	/* last two characters were "\e" */
	    if (isxdigit((unsigned char)c)) {
		state = EBC;
		literal = 0;
		nc = 0;
		continue;
	    } else {
		popup_an_error(AnString "(): Missing hex digits after \\e");
		state = BASE;
		continue;
	    }
	case OCTAL:	/* have seen \ and one or more octal digits */
	    if (nc < 3 && isdigit((unsigned char)c) && c < '8') {
		literal = (literal * 8) + FROM_HEX(c);
		nc++;
		break;
	    } else {
		key_UCharacter((unsigned char) literal, KT_STD, ia, true);
		state = BASE;
		continue;
	    }
	case HEX:	/* have seen \x and one or more hex digits */
	    if (nc < 4 && isxdigit((unsigned char)c)) {
		literal = (literal * 16) + FROM_HEX(c);
		nc++;
		break;
	    } else {
		key_UCharacter(literal, KT_STD, ia, true);
		state = BASE;
		continue;
	    }
	case EBC:	/* have seen \e and one or more hex digits */
	    if (nc < 4 && isxdigit((unsigned char)c)) {
		literal = (literal * 16) + FROM_HEX(c);
		nc++;
		break;
	    } else {
		vtrace(" %s -> Key(X'%04X')\n", ia_name[(int) ia], literal);
		if (!(literal & ~0xff)) {
		    key_Character((unsigned char) literal, false, true, true,
			    NULL);
		} else {
		    unsigned char ebc_pair[2];

		    ebc_pair[0] = (literal >> 8) & 0xff;
		    ebc_pair[1] = literal & 0xff;
		    key_WCharacter(ebc_pair, true);
		}
		state = BASE;
		continue;
	    }
	}
	ws++;
	xlen--;
    }

    switch (state) {
    case BASE:
	check_remargin = true;
	break;
    case OCTAL:
	key_UCharacter((unsigned char) literal, KT_STD, ia, true);
	check_remargin = true;
	break;
    case HEX:
	key_UCharacter(literal, KT_STD, ia, true);
	check_remargin = true;
	break;
    case EBC:
	vtrace(" %s -> Key(X'%04X')\n", ia_name[(int) ia], literal);
	if (!(literal & ~0xff)) {
	    key_Character((unsigned char) literal, false, true, true,
		    NULL);
	} else {
	    unsigned char ebc_pair[2];

	    ebc_pair[0] = (literal >> 8) & 0xff;
	    ebc_pair[1] = literal & 0xff;
	    key_WCharacter(ebc_pair, true);
	}
	check_remargin = true;
	break;
    case BACKPF:
	if (nc > 0) {
	    do_pf(literal);
	    state = BASE;
	}
	break;
    case BACKPA:
	if (nc > 0) {
	    do_pa(literal);
	    state = BASE;
	}
	break;
    default:
	popup_an_error(AnString "(): Missing data after \\");
	break;
    }

    if (check_remargin && pasting && MarginedPaste() && BA_TO_COL(cursor_addr) < orig_col) {
	remargin(orig_col);
    }

    return xlen;
}

/* Multibyte version of emulate_uinput. */
size_t
emulate_input(const char *s, size_t len, bool pasting, bool force_utf8)
{
    static ucs4_t *w_ibuf = NULL;
    static size_t w_ibuf_len = 0;
    int xlen;

    /* Convert from a multi-byte string to a Unicode string. */
    if (len + 1 > w_ibuf_len) {
	w_ibuf_len = len + 1;
	w_ibuf = (ucs4_t *)Realloc(w_ibuf, w_ibuf_len * sizeof(ucs4_t));
    }
    xlen = multibyte_to_unicode_string(s, len, w_ibuf, w_ibuf_len,
	    force_utf8);
    if (xlen < 0) {
	return 0; /* failed */
    }

    /* Process it as Unicode. */
    return emulate_uinput(w_ibuf, xlen, pasting);
}

/*
 * Pretend that a sequence of hexadecimal characters was entered at the
 * keyboard.  The input is a sequence of hexadecimal bytes, 2 characters
 * per byte.  If connected in NVT mode, these are treated as ASCII
 * characters; if in 3270 mode, they are considered EBCDIC.
 *
 * Graphic Escapes are handled as \E.
 */
void
hex_input(const char *s)
{
    const char *t;
    bool escaped;
    unsigned char *xbuf = NULL;
    unsigned char *tbuf = NULL;
    size_t nbytes = 0;

    /* Validate the string. */
    if (strlen(s) % 2) {
	popup_an_error(AnHexString "(): Odd number of characters in "
		"specification");
	return;
    }
    t = s;
    escaped = false;
    while (*t) {
	if (isxdigit((unsigned char)*t) &&
	    isxdigit((unsigned char)*(t + 1))) {
	    escaped = false;
		nbytes++;
	} else if (!strncmp(t, "\\E", 2) || !strncmp(t, "\\e", 2)) {
	    if (escaped) {
		popup_an_error(AnHexString "(): Double \\E");
		return;
	    }
	    if (!IN_3270) {
		popup_an_error(AnHexString "(): \\E in NVT mode");
		return;
	    }
	    escaped = true;
	} else {
	    popup_an_error(AnHexString "(): Illegal character in "
		    "specification");
	    return;
	}
	t += 2;
    }
    if (escaped) {
	popup_an_error(AnHexString "(): Nothing follows \\E");
	return;
    }

    /* Allocate a temporary buffer. */
    if (!IN_3270 && nbytes) {
	tbuf = xbuf = (unsigned char *)Malloc(nbytes);
    }

    /* Pump it in. */
    t = s;
    escaped = false;
    while (*t) {
	if (isxdigit((unsigned char)*t) &&
	    isxdigit((unsigned char)*(t + 1))) {
	    unsigned c;

	    c = (FROM_HEX(*t) * 16) + FROM_HEX(*(t + 1));
	    if (IN_3270) {
		key_Character(c, escaped, true, true, NULL);
	    } else {
		*tbuf++ = (unsigned char)c;
	    }
	    escaped = false;
	} else if (!strncmp(t, "\\E", 2) || !strncmp(t, "\\e", 2)) {
	    escaped = true;
	}
	t += 2;
    }
    if (!IN_3270 && nbytes) {
	net_hexnvt_out(xbuf, nbytes);
	Free(xbuf);
    }
}

/*
* Set up the cursor and input field for command input.
* Returns the length of the input field, or <0 if there is no field
* to set up.
*/
int
kybd_prime(void)
{
    int baddr;
    register unsigned char fa;
    int len = 0;

    /*
     * No point in trying if the the keyboard is locked or we aren't in
     * 3270 mode.
     */
    if (kybdlock) {
	return KYP_LOCKED;
    }
    if (!IN_3270) {
	return KYP_NOT_3270;
    }

    /*
     * If unformatted, guess that we can use all the NULs from the cursor
     * address forward, leaving one empty slot to delimit the end of the
     * command.  It's up to the host to make sense of what we send.
     */
    if (!formatted) {
	baddr = cursor_addr;

	while (ea_buf[baddr].ec == EBC_null ||
	   ea_buf[baddr].ec == EBC_space) {
	    len++;
	    INC_BA(baddr);
	    if (baddr == cursor_addr) {
		break;
	    }
	}
	if (len) {
	    len--;
	}
	return len;
    }

    fa = get_field_attribute(cursor_addr);
    if (ea_buf[cursor_addr].fa || FA_IS_PROTECTED(fa)) {
	/*
	 * The cursor is not in an unprotected field.  Find the
	 * next one.
	 */
	baddr = next_unprotected(cursor_addr);

	/* If there isn't any, give up. */
	if (!baddr) {
	    return KYP_NO_FIELD;
	}
    } else {
	/* Already in an unprotected field.  Find its start. */
	baddr = cursor_addr;
	while (!ea_buf[baddr].fa) {
	    DEC_BA(baddr);
	}
	INC_BA(baddr);
    }

    /* Move the cursor to the beginning of the field. */
    cursor_move(baddr);

    /* Erase it. */
    while (!ea_buf[baddr].fa) {
	ctlr_add(baddr, 0, 0);
	len++;
	INC_BA(baddr);
    }

    /* Return the field length. */
    return len;
}

/*
 * Process a TN3270E BID request.
 * Returns true for success.
 */
bool
kybd_bid(bool signal _is_unused)
{
    kybdlock_set(KL_BID, "kybd_bid");
    vstatus_reset();
    return true;
}

/* Process a TN3270E SEND-DATA indication. */
void
kybd_send_data(void)
{
    kybdlock_clr(KL_BID, "kybd_send_data");
    vstatus_reset();
    ps_process();
}

/*
 * Translate a key name to a key, including APL and extended characters.
 */
static ks_t
my_string_to_key(const char *s, enum keytype *keytypep, ucs4_t *ucs4)
{
    ks_t k;
    int consumed;
    enum me_fail error;

    *keytypep = KT_STD;
    *ucs4 = 0L;

    /* Look for my contrived APL symbols. */
    if (!strncmp(s, "apl_", 4)) {
	bool is_ge;

	*ucs4 = apl_key_to_ucs4(s, &is_ge);
	if (*ucs4 != 0) {
	    *keytypep = is_ge? KT_GE: KT_STD;
	    return KS_NONE;
	}

    }

    /* Look for a standard HTML entity or X11 keysym name. */
    k = string_to_key((char *)s);
    if (k != KS_NONE) {
	return k;
    }

    /* Look for "euro". */
    if (!strcasecmp(s, "euro")) {
	*ucs4 = 0x20ac;
	return KS_NONE;
    }

    /* Look for U+nnnn or 0xXXXX. */
    if (!strncasecmp(s, "U+", 2) || !strncasecmp(s, "0x", 2)) {
	*ucs4 = strtoul(s + 2, NULL, 16);
	return KS_NONE;
    }

    /* Look for a valid local multibyte character. */
    *ucs4 = multibyte_to_unicode(s, strlen(s), &consumed, &error);
    if ((size_t)consumed != strlen(s)) {
	*ucs4 = 0;
    }
    return KS_NONE;
}

static bool
build_composites(const char *how)
{
    char *c, *c0, *c1;
    char *ln;
    char ksname[3][64];
    char junk[2];
    enum keytype a[3];
    ucs4_t ucs4[3];
    int i;
    struct composite *cp;

    if (appres.interactive.compose_map == NULL) {
	popup_an_error("%s: No %s defined", how, ResComposeMap);
	return false;
    }
    c0 = get_fresource("%s.%s", ResComposeMap, appres.interactive.compose_map);
    if (c0 == NULL) {
	popup_an_error("%s: Cannot find %s \"%s\"", how, ResComposeMap,
		appres.interactive.compose_map);
	return false;
    }
    c1 = c = NewString(c0);	/* will be modified by strtok */
    while ((ln = strtok(c, "\n"))) {
	bool okay = true;

	c = NULL;
	if (sscanf(ln, " %63[^+ \t] + %63[^= \t] =%63s%1s",
		    ksname[0], ksname[1], ksname[2], junk) != 3) {
	    popup_an_error("%s: Invalid syntax: %s", how, ln);
	    continue;
	}
	for (i = 0; i < 3; i++) {
	    ks_t k = my_string_to_key(ksname[i], &a[i], &ucs4[i]);

	    if ((k == KS_NONE && !ucs4[i]) || (k & ~0xff)) {
		popup_an_error("%s: Invalid name: \"%s\"", how, ksname[i]);
		okay = false;
		break;
	    }
	    if (k != KS_NONE) {
		ucs4[i] = k;
	    }
	}
	if (!okay) {
	    continue;
	}
	composites = (struct composite *) Realloc((char *)composites,
		(n_composites + 1) * sizeof(struct composite));
	cp = composites + n_composites;
	cp->k1.ucs4 = ucs4[0];
	cp->k1.keytype = a[0];
	cp->k2.ucs4 = ucs4[1];
	cp->k2.keytype = a[1];
	cp->translation.ucs4 = ucs4[2];
	cp->translation.keytype = a[2];
	n_composites++;
    }
    Free(c1);
    return true;
}

/*
 * Called by the toolkit when the "Compose" key is pressed.  "Compose" is
 * implemented by pressing and releasing three keys: "Compose" and two
 * data keys.  For example, "Compose" "s" "s" gives the German "ssharp"
 * character, and "Compose" "C", "," gives a capital "C" with a cedilla
 * (symbol Ccedilla).
 *
 * The mechanism breaks down a little when the user presses "Compose" and
 * then a non-data key.  Oh well.
 */
static bool
Compose_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnCompose, ia, argc, argv);
    if (check_argc(AnCompose, argc, 0, 0) < 0) {
	return false;
    }

    if (!composites && !build_composites(AnCompose)) {
	return true;
    }

    composing = COMPOSE;
    vstatus_compose(true, 0, KT_STD);
    return true;
}

/* Destroy the current compose map. */
static void
destroy_compose_map(void)
{
    composing = NONE;
    vstatus_compose(false, 0, KT_STD);

    Replace(composites, NULL);
    n_composites = 0;
}

/* Set or clear a temporary compose map. */
bool
temporary_compose_map(const char *name, const char *how)
{
    /* Make sure we track the default. */
    if (default_compose_map_name == NULL
	    && appres.interactive.compose_map != NULL) {
	default_compose_map_name = NewString(appres.interactive.compose_map);
    }

    /* Destroy the current map. */
    destroy_compose_map();
    Replace(appres.interactive.compose_map,
	    NewString(default_compose_map_name));

    if (name == NULL ||
	    (temporary_compose_map_name != NULL &&
	     !strcmp(temporary_compose_map_name, name))) {
	/* Clear out the temporary map. */
	Replace(temporary_compose_map_name, NULL);
	return true;
    }

    /* Set the temporary one. */
    temporary_compose_map_name = NewString(name);
    appres.interactive.compose_map = NewString(name);
    return !build_composites(how);
}

/*
 * TemporaryComposeMap() clears out any temporary compose map.
 * TemporaryComposeMap(x) makes the temporary compose map, or if x is already
 *  the temporary compose map, removes it.
 */
static bool
TemporaryComposeMap_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnTemporaryComposeMap, ia, argc, argv);
    if (check_argc(AnTemporaryComposeMap, argc, 0, 1) < 0) {
	return false;
    }

    return temporary_compose_map((argc > 0)? argv[0]: NULL,
	    AnTemporaryComposeMap);
}
