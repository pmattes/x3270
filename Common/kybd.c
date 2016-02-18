/*
 * Copyright (c) 1993-2009, 2013-2016 Paul Mattes.
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
#include "charset.h"
#include "ctlrc.h"
#include "unicodec.h"
#include "ft.h"
#include "host.h"
#include "idle.h"
#include "kybd.h"
#include "latin1.h"
#include "lazya.h"
#include "linemode.h"
#include "macros.h"
#include "nvt.h"
#include "popups.h"
#include "print_screen.h"
#include "product.h"
#include "screen.h"
#include "scroll.h"
#include "status.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"

/*#define KYBDLOCK_TRACE	1*/

#define MarginedPaste()	(toggled(MARGINED_PASTE) || toggled(OVERLAY_PASTE))

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
static bool key_Character(unsigned ebc, bool with_ge, bool pasting);
static bool flush_ta(void);
static void key_AID(unsigned char aid_code);
static void kybdlock_set(unsigned int bits, const char *cause);
static ks_t my_string_to_key(const char *s, enum keytype *keytypep,
	ucs4_t *ucs4);

static bool key_WCharacter(unsigned char code[]);

static bool		insert = false;		/* insert mode */
static bool		reverse = false;	/* reverse-input mode */

/* Globals */
unsigned int	kybdlock = KL_NOT_CONNECTED;
unsigned char	aid = AID_NO;		/* current attention ID */

/* Composite key mappings. */

struct akey {
	ks_t key;
	enum keytype keytype;
};
static struct akey cc_first;
static struct composite {
	struct akey k1, k2;
	struct akey translation;
} *composites = NULL;
static int n_composites = 0;

#define ak_eq(k1, k2)	(((k1).key  == (k2).key) && \
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
	    status_reset(); \
	} else { \
	    enq_ta(action, NULL, NULL); \
	    return true; \
	} \
    } \
} while(false)

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
static action_t Newline_action;
static action_t NextWord_action;
static action_t PA_action;
static action_t PF_action;
static action_t PreviousWord_action;
static action_t Reset_action;
static action_t Right2_action;
static action_t String_action;
static action_t SysReq_action;
static action_t Tab_action;
static action_t ToggleInsert_action;
static action_t ToggleReverse_action;

static action_table_t kybd_actions[] = {
    { "Attn",		Attn_action,		ACTION_KE },
    { "BackSpace",	BackSpace_action,	ACTION_KE },
    { "BackTab",	BackTab_action,		ACTION_KE },
    { "CircumNot",	CircumNot_action,	ACTION_KE },
    { "Clear",		Clear_action,		ACTION_KE },
    { "CursorSelect",	CursorSelect_action,	ACTION_KE },
    { "Delete",		Delete_action,		ACTION_KE },
    { "DeleteField",	DeleteField_action,	ACTION_KE },
    { "DeleteWord",	DeleteWord_action,	ACTION_KE },
    { "Down",		Down_action,		ACTION_KE },
    { "Dup",		Dup_action,		ACTION_KE },
    { "Enter",		Enter_action,		ACTION_KE },
    { "Erase",		Erase_action,		ACTION_KE },
    { "EraseEOF",	EraseEOF_action,	ACTION_KE },
    { "EraseInput",	EraseInput_action,	ACTION_KE },
    { "FieldEnd",	FieldEnd_action,	ACTION_KE },
    { "FieldMark",	FieldMark_action,	ACTION_KE },
    { "Flip",		Flip_action,		ACTION_KE },
    { "HexString",	HexString_action,	ACTION_KE },
    { "Home",		Home_action,		ACTION_KE },
    { "Insert",		Insert_action,		ACTION_KE },
    { "Interrupt",	Interrupt_action,	ACTION_KE },
    { "Key",		Key_action,		ACTION_KE },
    { "Left2",		Left2_action,		ACTION_KE },
    { "Left",		Left_action,		ACTION_KE },
    { "MonoCase",	MonoCase_action,	ACTION_KE },
    { "MoveCursor",	MoveCursor_action,	ACTION_KE },
    { "Newline",	Newline_action,		ACTION_KE },
    { "NextWord",	NextWord_action,	ACTION_KE },
    { "PA",		PA_action,		ACTION_KE },
    { "PF",		PF_action,		ACTION_KE },
    { "PreviousWord",	PreviousWord_action,	ACTION_KE },
    { "Reset",		Reset_action,		ACTION_KE },
    { "Right2",		Right2_action,		ACTION_KE },
    { "Right",		Right_action,		ACTION_KE },
    { "String",		String_action,		ACTION_KE },
    { "SysReq",		SysReq_action,		ACTION_KE },
    { "Tab",		Tab_action,		ACTION_KE },
    { "ToggleInsert",	ToggleInsert_action,	ACTION_KE },
    { "ToggleReverse",	ToggleReverse_action,	ACTION_KE },
    { "Up",		Up_action,		ACTION_KE }
};

static action_table_t kybd_dactions[] = {
    { "Compose",	Compose_action,		ACTION_KE }
};


/*
 * Put a function or action on the typeahead queue.
 */
static void
enq_xta(const char *name, action_t *fn, const char *parm1, const char *parm2)
{
	ta_t *ta;

	/* If no connection, forget it. */
	if (!CONNECTED) {
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

	/* If typeahead disabled, complain and drop it. */
	if (!appres.typeahead) {
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
		if (parm2)
			ta->parm2 = NewString(parm2);
	}
	if (ta_head)
		ta_tail->next = ta;
	else {
		ta_head = ta;
		status_typeahead(true);
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
	status_typeahead(false);
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
	(void) (*ta->fn)(IA_TYPEAHEAD, argc, argv);
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
    status_typeahead(false);
    return any;
}

/* Decode keyboard lock bits. */
static char *
kybdlock_decode(char *how, unsigned int bits)
{
    static char *rs = NULL;
    varbuf_t r;
    char *space = "";

    if (bits == (unsigned int)-1) {
	return "all";
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

    Replace(rs, vb_consume(&r));
    return rs;
}

/* Set bits in the keyboard lock. */
static void
kybdlock_set(unsigned int bits, const char *cause _is_unused)
{
	unsigned int n;

	vtrace("Keyboard lock(%s) %s\n", cause,
		kybdlock_decode("+", bits));
	n = kybdlock | bits;
	if (n != kybdlock) {
#if defined(KYBDLOCK_TRACE) /*[*/
	       vtrace("  %s: kybdlock |= 0x%04x, 0x%04x -> 0x%04x\n",
		    cause, bits, kybdlock, n);
#endif /*]*/
		if ((kybdlock ^ bits) & KL_DEFERRED_UNLOCK) {
			/* Turned on deferred unlock. */
			unlock_delay_time = time(NULL);
		}
		kybdlock = n;
	}
}

/* Clear bits in the keyboard lock. */
void
kybdlock_clr(unsigned int bits, const char *cause _is_unused)
{
	unsigned int n;

	if (kybdlock & bits)
		vtrace("Keyboard unlock(%s) %s\n", cause,
			kybdlock_decode("-", kybdlock & bits));
	n = kybdlock & ~bits;
	if (n != kybdlock) {
#if defined(KYBDLOCK_TRACE) /*[*/
		vtrace("  %s: kybdlock &= ~0x%04x, 0x%04x -> 0x%04x\n",
		    cause, bits, kybdlock, n);
#endif /*]*/
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
		if (kybdlock == KL_ENTER_INHIBIT)
			status_reset();
	} else {
		kybdlock_clr(KL_ENTER_INHIBIT, "kybd_inhibit");
		if (!kybdlock)
			status_reset();
	}
}

/*
 * Called when a host connects or disconnects.
 */
static void
kybd_connect(bool connected)
{
    if ((kybdlock & KL_DEFERRED_UNLOCK) && unlock_id) {
	RemoveTimeOut(unlock_id);
	unlock_id = NULL_IOID;
    }
    kybdlock_clr(-1, "kybd_connect");

    if (connected) {
	if (!appres.nvt_mode) {
	    /* Wait for any output or a WCC(restore) from the host */
	    kybdlock_set(KL_AWAITING_FIRST, "kybd_connect");
	}
    } else {
	kybdlock_set(KL_NOT_CONNECTED, "kybd_connect");
	(void) flush_ta();
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

	switch ((int)cstate) {
	case CONNECTED_UNBOUND:
		/*
		 * We just processed and UNBIND from the host. We are waiting
		 * for a BIND, or data to switch us to 3270, NVT or SSCP-LU
		 * mode.
		 */
		kybdlock_set(KL_AWAITING_FIRST, "kybd_in3270");
		break;
	case CONNECTED_NVT:
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
		    kybdlock_clr(~(KL_AWAITING_FIRST | KL_OIA_LOCKED),
				"kybd_in3270");
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
	if (CONNECTED)
		ps_process();
}

/*
 * Keyboard module registration.
 */
void
kybd_register(void)
{
    static toggle_register_t toggles[] = {
	{ BLANK_FILL,	NULL,	0 }
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
}

/*
 * Toggle insert mode.
 */
static void
insert_mode(bool on)
{
	insert = on;
	status_insert_mode(on);
}

/*
 * Toggle reverse mode.
 */
static void
reverse_mode(bool on)
{
	if (!dbcs) {
		reverse = on;
		status_reverse_mode(on);
	}
}

/*
 * Lock the keyboard because of an operator error.
 */
static void
operator_error(int error_type)
{
	if (sms_redirect())
		popup_an_error("Keyboard locked");
	if (appres.oerr_lock || sms_redirect()) {
		status_oerr(error_type);
		mcursor_locked();
		kybdlock_set((unsigned int)error_type, "operator_error");
		(void) flush_ta();
	} else {
		ring_bell();
	}
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
		for (i = 0; i < PA_SZ; i++)
			if (aid_code == pa_xlate[i]) {
				nvt_send_pa(i+1);
				return;
			}
		return;
	}

	if (IN_SSCP) {
		if (kybdlock & KL_OIA_MINUS)
			return;
		switch (aid_code) {
		case AID_CLEAR:
		    	/* Handled locally. */
			break;
		case AID_ENTER:
			/*
			 * Act as if the host had written our input, and
			 * send it as a Read Modified.
			 */
			buffer_addr = cursor_addr;
			aid = aid_code;
			ctlr_read_modified(aid, false);
			status_ctlr_done();
			break;
		default:
			/* Everything else is invalid in SSCP-LU mode. */
			status_minus();
			kybdlock_set(KL_OIA_MINUS, "key_AID");
			return;
		}
		return;
	}

	status_twait();
	mcursor_waiting();
	insert_mode(false);
	kybdlock_set(KL_OIA_TWAIT | KL_OIA_LOCKED, "key_AID");
	aid = aid_code;
	ctlr_read_modified(aid, false);
	ticking_start(false);
	status_ctlr_done();
}

static bool
PF_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned k;

    action_debug("PF", ia, argc, argv);
    if (check_argc("PF", argc, 1, 1) < 0) {
	return false;
    }
    k = atoi(argv[0]);
    if (k < 1 || k > PF_SZ) {
	popup_an_error("PF: Invalid argument '%s'", argv[0]);
	cancel_if_idle_command();
	return false;
    }
    reset_idle_timer();
    if (kybdlock & KL_OIA_MINUS) {
	return true;
    }
    if (kybdlock) {
	enq_ta("PF", argv[0], NULL);
    } else {
	key_AID(pf_xlate[k-1]);
    }
    return true;
}

static bool
PA_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned k;

    action_debug("PA", ia, argc, argv);
    if (check_argc("PA", argc, 1, 1) < 0) {
	return false;
    }
    k = atoi(argv[0]);
    if (k < 1 || k > PA_SZ) {
	popup_an_error("PA: Invalid argument '%s'", argv[0]);
	cancel_if_idle_command();
	return false;
    }
    reset_idle_timer();
    if (kybdlock & KL_OIA_MINUS) {
	return true;
    }
    if (kybdlock) {
	enq_ta("PA", argv[0], NULL);
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
    action_debug("Attn", ia, argc, argv);
    if (check_argc("Attn", argc, 0, 0) < 0) {
	return false;
    }
    if (!IN_3270) {
	return false;
    }
    reset_idle_timer();

    if (IN_E) {
	if (net_bound()) {
	    net_interrupt();
	} else {
	    status_minus();
	    kybdlock_set(KL_OIA_MINUS, "Attn");
	}
    } else {
	net_break();
    }
    return true;
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
    action_debug("Interrupt", ia, argc, argv);
    if (check_argc("Interrupt", argc, 0, 0) < 0) {
	return false;
    }
    if (!IN_3270) {
	return false;
    }
    reset_idle_timer();
    net_interrupt();
    return true;
}



/*
 * Prepare for an insert of 'count' bytes.
 * Returns true if the insert is legal, false otherwise.
 */
static bool
ins_prep(int faddr, int baddr, int count, bool *no_room)
{
	int next_faddr;
	int xaddr;
	int need;
	int ntb;
	int tb_start = -1;
	int copy_len;

	*no_room = false;

	/* Find the end of the field. */
	if (faddr == -1) {
		/* Unformatted.  Use the end of the line. */
		next_faddr = (((baddr / COLS) + 1) * COLS) % (ROWS*COLS);
	} else {
		next_faddr = faddr;
		INC_BA(next_faddr);
		while (next_faddr != faddr && !ea_buf[next_faddr].fa) {
			INC_BA(next_faddr);
		}
	}

	/* Are there enough NULLs or trailing blanks available? */
	xaddr = baddr;
	need = count;
	ntb = 0;
	while (need && (xaddr != next_faddr)) {
		if (ea_buf[xaddr].cc == EBC_null)
			need--;
		else if (toggled(BLANK_FILL) &&
			((ea_buf[xaddr].cc == EBC_space) ||
			 (ea_buf[xaddr].cc == EBC_underscore))) {
			if (tb_start == -1)
				tb_start = xaddr;
			ntb++;
		} else {
			tb_start = -1;
			ntb = 0;
		}
		INC_BA(xaddr);
	}
#if defined(_ST) /*[*/
	printf("need %d at %d, tb_start at %d\n", count, baddr, tb_start);
#endif /*]*/
	if (need - ntb > 0) {
	    	if (!reverse) {
			operator_error(KL_OERR_OVERFLOW);
			return false;
		} else {
		    	*no_room = true;
			return true;
		}
	}

	/*
	 * Shift the buffer to the right until we've consumed the available
	 * (and needed) NULLs.
	 */
	need = count;
	xaddr = baddr;
	while (need && (xaddr != next_faddr)) {
		int n_nulls = 0;
		int first_null = -1;

		while (need &&
		       ((ea_buf[xaddr].cc == EBC_null) ||
		        (tb_start >= 0 && xaddr >= tb_start))) {
			need--;
			n_nulls++;
			if (first_null == -1)
				first_null = xaddr;
			INC_BA(xaddr);
		}
		if (n_nulls) {
			int to;

			/* Shift right n_nulls worth. */
			copy_len = first_null - baddr;
			if (copy_len < 0)
				copy_len += ROWS*COLS;
			to = (baddr + n_nulls) % (ROWS*COLS);
#if defined(_ST) /*[*/
			printf("found %d NULLs at %d\n", n_nulls, first_null);
			printf("copying %d from %d to %d\n", copy_len, to,
			    first_null);
#endif /*]*/
			if (copy_len)
				ctlr_wrapping_memmove(to, baddr, copy_len);
		}
		INC_BA(xaddr);
	}

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
key_Character_wrapper(ia_t ia _is_unused, unsigned argc _is_unused,
	const char **argv)
{
    unsigned ebc;
    bool with_ge = false;
    bool pasting = false;
    char mb[16];
    ucs4_t uc;

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
    (void) key_Character(ebc, with_ge, pasting);
    return true;
}

/*
 * Handle an ordinary displayable character key.  Lots of stuff to handle
 * insert-mode, protected fields and etc.
 */
static bool
key_Character(unsigned ebc, bool with_ge, bool pasting)
{
	register int	baddr, faddr, xaddr;
	register unsigned char	fa;
	enum dbcs_why why = DBCS_FIELD;
	bool no_room = false;
	bool auto_skip = true;

	reset_idle_timer();

	if (kybdlock) {
		char *codename;

		codename = lazyaf("%d", ebc |
			(with_ge ? GE_WFLAG : 0) |
			(pasting ? PASTE_WFLAG : 0));
		enq_fta(key_Character_wrapper, codename, NULL);
		return false;
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
		operator_error(KL_OERR_PROTECTED);
		return false;
	    }
	}
	if (appres.numeric_lock && FA_IS_NUMERIC(fa) &&
	    !((ebc >= EBC_0 && ebc <= EBC_9) ||
	      ebc == EBC_minus || ebc == EBC_period)) {
		operator_error(KL_OERR_NUMERIC);
		return false;
	}

	/* Can't put an SBCS in a DBCS field. */
	if (ea_buf[faddr].cs == CS_DBCS) {
		operator_error(KL_OERR_DBCS);
		return false;
	}

	/* If it's an SI (end of DBCS subfield), move over one position. */
	if (ea_buf[baddr].cc == EBC_si) {
		INC_BA(baddr);
		if (baddr == faddr) {
			operator_error(KL_OERR_OVERFLOW);
			return false;
		}
	}

	/* Add the character. */
	if (ea_buf[baddr].cc == EBC_so) {

		if (insert) {
			if (!ins_prep(faddr, baddr, 1, &no_room))
				return false;
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
			was_si = (ea_buf[xaddr].cc == EBC_si);
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
			if (insert) {
				if (!ins_prep(faddr, baddr, 1, &no_room))
					return false;
			} else {
				/*
				 * Replace single DBCS char with
				 * x/space.
				 */
				xaddr = baddr;
				INC_BA(xaddr);
				ctlr_add(xaddr, EBC_space, CS_BASE);
				ctlr_add_fg(xaddr, 0);
				ctlr_add_gr(xaddr, 0);
			}
		} else {
			bool was_si;

			if (insert) {
				/*
				 * Inserting SBCS into a DBCS subfield.
				 * If this is the first position, we
				 * can just insert one character in
				 * front of the SO.  Otherwise, we'll
				 * need room for SI (to end subfield),
				 * the character, and SO (to begin the
				 * subfield again).
				 */
				xaddr = baddr;
				DEC_BA(xaddr);
				if (ea_buf[xaddr].cc == EBC_so) {
					DEC_BA(baddr);
					if (!ins_prep(faddr, baddr, 1,
						    &no_room))
						return false;
				} else {
					if (!ins_prep(faddr, baddr, 3,
						    &no_room))
						return false;
					xaddr = baddr;
					ctlr_add(xaddr, EBC_si,
					    CS_BASE);
					ctlr_add_fg(xaddr, 0);
					ctlr_add_gr(xaddr, 0);
					INC_BA(xaddr);
					INC_BA(baddr);
					INC_BA(xaddr);
					ctlr_add(xaddr, EBC_so,
					    CS_BASE);
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
				was_si = (ea_buf[xaddr].cc == EBC_si);
				ctlr_add(xaddr, EBC_space, CS_BASE);
				ctlr_add_fg(xaddr, 0);
				ctlr_add_gr(xaddr, 0);
				if (!was_si) {
					INC_BA(xaddr);
					ctlr_add(xaddr, EBC_so,
					    CS_BASE);
					ctlr_add_fg(xaddr, 0);
					ctlr_add_gr(xaddr, 0);
				}
			}
		}
		break;
	default:
	case DBCS_NONE:
		if ((reverse || insert) && !ins_prep(faddr, baddr, 1, &no_room))
			return false;
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
		if (!reverse)
			INC_BA(baddr);
	}

	/* Replace leading nulls with blanks, if desired. */
	if (formatted && toggled(BLANK_FILL)) {
		register int	baddr_fill = baddr;

		DEC_BA(baddr_fill);
		while (baddr_fill != faddr) {

			/* Check for backward line wrap. */
			if ((baddr_fill % COLS) == COLS - 1) {
				bool aborted = true;
				register int baddr_scan = baddr_fill;

				/*
				 * Check the field within the preceeding line
				 * for NULLs.
				 */
				while (baddr_scan != faddr) {
					if (ea_buf[baddr_scan].cc != EBC_null) {
						aborted = false;
						break;
					}
					if (!(baddr_scan % COLS))
						break;
					DEC_BA(baddr_scan);
				}
				if (aborted)
					break;
			}

			if (ea_buf[baddr_fill].cc == EBC_null)
				ctlr_add(baddr_fill, EBC_space, 0);
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
			if (FA_IS_SKIP(ea_buf[baddr].fa))
				baddr = next_unprotected(baddr);
			else
				INC_BA(baddr);
		}
		cursor_move(baddr);
	} else {
	    cursor_move(baddr);
	}

	(void) ctlr_dbcs_postprocess();
	return true;
}

static bool
key_WCharacter_wrapper(ia_t ia _is_unused, unsigned argc _is_unused,
	const char **argv)
{
    unsigned ebc_wide;
    unsigned char ebc_pair[2];

    ebc_wide = atoi(argv[0]);
    vtrace(" %s -> Key(X'%04x')\n", ia_name[(int) ia_cause], ebc_wide);
    ebc_pair[0] = (ebc_wide >> 8) & 0xff;
    ebc_pair[1] = ebc_wide & 0xff;
    (void) key_WCharacter(ebc_pair);
    return true;
}

/*
 * Input a DBCS character.
 * Returns true if a character was stored in the buffer, false otherwise.
 */
static bool
key_WCharacter(unsigned char ebc_pair[])
{
	int baddr;
	register unsigned char fa;
	int faddr;
	enum dbcs_state d;
	int xaddr;
	bool done = false;
	bool no_si = false;
	bool no_room = false;

	reset_idle_timer();

	if (kybdlock) {
		char *codename;

		codename = lazyaf("%d",
			(ebc_pair[0] << 8) | ebc_pair[1]);
		enq_fta(key_WCharacter_wrapper, codename, NULL);
		return false;
	}

	if (!dbcs) {
		vtrace("DBCS character received when not in DBCS mode, "
		    "ignoring.\n");
		return true;
	}

	/* In NVT mode? */
	if (IN_NVT) {
	    char mb[16];

	    (void) ebcdic_to_multibyte((ebc_pair[0] << 8) | ebc_pair[1], mb,
				       sizeof(mb));
	    net_sends(mb);
	    return true;
	}

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	faddr = find_field_attribute(baddr);

	/* Protected? */
	if (ea_buf[baddr].fa || FA_IS_PROTECTED(fa)) {
		operator_error(KL_OERR_PROTECTED);
		return false;
	}

	/* Numeric? */
	if (appres.numeric_lock && FA_IS_NUMERIC(fa)) {
		operator_error(KL_OERR_NUMERIC);
		return false;
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
		if (insert) {
			if (!ins_prep(faddr, baddr, 2, &no_room)) {
				return false;
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
		if (insert) {
			if (!ins_prep(faddr, baddr, 2, &no_room)) {
				return false;
			}
		} else {
			/* Don't overwrite a field attribute or an SO. */
			xaddr = baddr;
			INC_BA(xaddr);	/* C1 */
			if (ea_buf[xaddr].fa)
				break;
			if (ea_buf[xaddr].cc == EBC_so)
				no_si = true;
			INC_BA(xaddr);	/* SI */
			if (ea_buf[xaddr].fa || ea_buf[xaddr].cc == EBC_so)
				break;
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
			if (insert) {
				if (!ins_prep(faddr, baddr, 4, &no_room)) {
					return false;
				}
			} else {
				xaddr = baddr;	/* baddr, SO */
				if (ea_buf[xaddr].cc == EBC_so) {
					/*
					 * (baddr), where we would have put the
					 * SO, is already an SO.  Move to
					 * (baddr+1) and try again.
					 */
#if defined(DBCS_RIGHT_DEBUG) /*[*/
					printf("SO in position 0\n");
#endif /*]*/
					INC_BA(baddr);
					goto retry;
				}

				INC_BA(xaddr);	/* baddr+1, C0 */
				if (ea_buf[xaddr].fa)
					break;
				if (ea_buf[xaddr].cc == EBC_so) {
					enum dbcs_state e;

					/*
					 * (baddr+1), where we would have put
					 * the left side of the DBCS, is a SO.
					 * If there's room, we can extend the
					 * subfield to the left.  If not, we're
					 * stuck.
					 */
					DEC_BA(xaddr);
					DEC_BA(xaddr);
					e = ctlr_dbcs_state(xaddr);
					if (e == DBCS_NONE || e == DBCS_SB) {
						extend_left = true;
						no_si = true;
#if defined(DBCS_RIGHT_DEBUG) /*[*/
						printf("SO in position 1, "
							"extend left\n");
#endif /*]*/
					} else {
						/*
						 * Won't actually happen,
						 * because this implies that
						 * the buffer addr at baddr
						 * is an SB.
						 */
#if defined(DBCS_RIGHT_DEBUG) /*[*/
						printf("SO in position 1, "
							"no room on left, "
							"fail\n");
#endif /*]*/
						break;
					}
				}

				INC_BA(xaddr); /* baddr+2, C1 */
				if (ea_buf[xaddr].fa)
					break;
				if (ea_buf[xaddr].cc == EBC_so) {
					/*
					 * (baddr+2), where we want to put the
					 * right half of the DBCS character, is
					 * a SO.  This is a natural extension
					 * to the left -- just make sure we
					 * don't write an SI.
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
					if (ea_buf[xaddr].fa)
						break;
					if (ea_buf[xaddr].cc == EBC_so) {
						/*
						 * (baddr+3), where we want to
						 * put an
						 * SI, is an SO.  Forget it.
						 */
#if defined(DBCS_RIGHT_DEBUG) /*[*/
						printf("SO in position 3, "
							"retry right\n");
						INC_BA(baddr);
						goto retry;
#endif /*]*/
						break;
					}
				}
			}
			/* Yes, add it. */
			if (extend_left)
				DEC_BA(baddr);
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
			if (insert) {
				if (!ins_prep(faddr, baddr, 2, &no_room)) {
					return false;
				}
			} else {
				xaddr = baddr;
				INC_BA(xaddr);
				if (ea_buf[xaddr].fa)
					break;
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
				if (ea_buf[xaddr].cc == EBC_null)
					ctlr_add(xaddr, EBC_space, CS_BASE);
				else
					break;
				INC_BA(xaddr);
			}
		}

		mdt_set(cursor_addr);

		/* Implement auto-skip. */
		while (ea_buf[baddr].fa) {
			if (FA_IS_SKIP(ea_buf[baddr].fa))
				baddr = next_unprotected(baddr);
			else
				INC_BA(baddr);
		}
		cursor_move(baddr);
		(void) ctlr_dbcs_postprocess();
		return true;
	} else {
		operator_error(KL_OERR_DBCS);
		return false;
	}
}

/*
 * Handle an ordinary character key, given its Unicode value.
 */
void
key_UCharacter(ucs4_t ucs4, enum keytype keytype, enum iaction cause)
{
	register int i;
	struct akey ak;

	reset_idle_timer();

	if (kybdlock) {
	    const char *apl_name;

	    if (keytype == KT_STD) {
		enq_ta("Key", lazyaf("U+%04x", ucs4), NULL);
	    } else {
		/* APL character */
		apl_name = key_to_apl_string(ucs4);
		if (apl_name != NULL) {
		    enq_ta("Key", lazyaf("apl_%s", apl_name), NULL);
		} else {
		    vtrace("  dropped (invalid key type or name)\n");
		}
	    }
	    return;
	}

	ak.key = ucs4;
	ak.keytype = keytype;

	switch (composing) {
	    case NONE:
		break;
	    case COMPOSE:
		for (i = 0; i < n_composites; i++)
			if (ak_eq(composites[i].k1, ak) ||
			    ak_eq(composites[i].k2, ak))
				break;
		if (i < n_composites) {
			cc_first.key = ucs4;
			cc_first.keytype = keytype;
			composing = FIRST;
			status_compose(true, ucs4, keytype);
		} else {
			ring_bell();
			composing = NONE;
			status_compose(false, 0, KT_STD);
		}
		return;
	    case FIRST:
		composing = NONE;
		status_compose(false, 0, KT_STD);
		for (i = 0; i < n_composites; i++)
			if ((ak_eq(composites[i].k1, cc_first) &&
			     ak_eq(composites[i].k2, ak)) ||
			    (ak_eq(composites[i].k1, ak) &&
			     ak_eq(composites[i].k2, cc_first)))
				break;
		if (i < n_composites) {
			ucs4 = composites[i].translation.key;
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
		ebc = unicode_to_ebcdic_ge(ucs4, &ge);
		if (ebc == 0) {
			vtrace("  dropped (no EBCDIC translation)\n");
			return;
		}
		if (ebc & 0xff00) {
		    	unsigned char ebc_pair[2];

			ebc_pair[0] = (ebc & 0xff00)>> 8;
			ebc_pair[1] = ebc & 0xff;
			(void) key_WCharacter(ebc_pair);
		} else {
			(void) key_Character(ebc, (keytype == KT_GE) || ge,
				(cause == IA_PASTE));
		}
	}
	else if (IN_NVT) {
	    	char mb[16];

		unicode_to_multibyte(ucs4, mb, sizeof(mb));
		net_sends(mb);
	} else {
		const char *why;

		switch (cstate) {
		case NOT_CONNECTED:
			why = "connected";
			break;
		case RESOLVING:
		case PENDING:
		case NEGOTIATING:
		case CONNECTED_INITIAL:
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
    action_debug("MonoCase", ia, argc, argv);
    if (check_argc("MonoCase", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    do_toggle(MONOCASE);
    return true;
}

/*
 * Flip the display left-to-right
 */
static bool
Flip_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Flip", ia, argc, argv);
    if (check_argc("Flip", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (dbcs) {
	return false;
    }
    screen_flip();
    return true;
}



/*
 * Tab forward to next field.
 */
static bool
Tab_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Tab", ia, argc, argv);
    if (check_argc("Tab", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Tab");
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

    action_debug("BackTab", ia, argc, argv);
    if (check_argc("BackTab", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("BackTab");
    if (!IN_3270) {
	return false;
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
	status_reset();
	if (CONNECTED)
		ps_process();
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

		if (flush_ta())
			half_reset = true;
		if (composing != NONE) {
			composing = NONE;
			status_compose(false, 0, KT_STD);
			half_reset = true;
		}
		if (half_reset)
			return;
	}

	/* Always clear insert mode. */
	insert_mode(false);

	/* Always reset scrolling. */
	scroll_to_bottom();

	/* Otherwise, if not connect, reset is a no-op. */
	if (!CONNECTED)
		return;

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
	    || (!appres.unlock_delay && !sms_in_macro())
	    || (unlock_delay_time != 0 && (time(NULL) - unlock_delay_time) > 1)
	    || !appres.unlock_delay_ms) {
		kybdlock_clr(-1, "do_reset");
	} else if (kybdlock &
  (KL_DEFERRED_UNLOCK | KL_OIA_TWAIT | KL_OIA_LOCKED | KL_AWAITING_FIRST)) {
		kybdlock_clr(~KL_DEFERRED_UNLOCK, "do_reset");
		kybdlock_set(KL_DEFERRED_UNLOCK, "do_reset");
		unlock_id = AddTimeOut(appres.unlock_delay_ms, defer_unlock);
		vtrace("Deferring keyboard unlock %dms\n",
			appres.unlock_delay_ms);
	}

	/* Clean up other modes. */
	status_reset();
	mcursor_normal();
	composing = NONE;
	status_compose(false, 0, KT_STD);
}

static bool
Reset_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Reset", ia, argc, argv);
    if (check_argc("Reset", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    do_reset(true);
    return true;
}


/*
 * Move to first unprotected field on screen.
 */
static bool
Home_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Home", ia, argc, argv);
    if (check_argc("Home", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Home");
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
	register int	baddr;
	enum dbcs_state d;

	baddr = cursor_addr;
	DEC_BA(baddr);
	d = ctlr_dbcs_state(baddr);
	if (IS_RIGHT(d)) {
		DEC_BA(baddr);
	} else if (IS_LEFT(d)) {
		DEC_BA(baddr);
		d = ctlr_dbcs_state(baddr);
		if (IS_RIGHT(d))
			DEC_BA(baddr);
	}
	cursor_move(baddr);
}

bool
Left_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Left", ia, argc, argv);
    if (check_argc("Left", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Left");
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
	register int	baddr, end_baddr;
	int xaddr;
	register unsigned char	fa;
	int ndel;
	register int i;

	baddr = cursor_addr;

	/* Can't delete a field attribute. */
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
		operator_error(KL_OERR_PROTECTED);
		return false;
	}
	if (ea_buf[baddr].cc == EBC_so || ea_buf[baddr].cc == EBC_si) {
		/*
		 * Can't delete SO or SI, unless it's adjacent to its
		 * opposite.
		 */
		xaddr = baddr;
		INC_BA(xaddr);
		if (ea_buf[xaddr].cc == SOSI(ea_buf[baddr].cc)) {
			ndel = 2;
		} else {
			operator_error(KL_OERR_PROTECTED);
			return false;
		}
	} else if (IS_DBCS(ea_buf[baddr].db)) {
		if (IS_RIGHT(ea_buf[baddr].db))
			DEC_BA(baddr);
		ndel = 2;
	} else
		ndel = 1;

	/* find next fa */
	if (formatted) {
		end_baddr = baddr;
		do {
			INC_BA(end_baddr);
			if (ea_buf[end_baddr].fa)
				break;
		} while (end_baddr != baddr);
		DEC_BA(end_baddr);
	} else {
		if ((baddr % COLS) == COLS - ndel)
			return true;
		end_baddr = baddr + (COLS - (baddr % COLS)) - 1;
	}

	/* Shift the remainder of the field left. */
	if (end_baddr > baddr) {
		ctlr_bcopy(baddr + ndel, baddr, end_baddr - (baddr + ndel) + 1,
		    0);
	} else if (end_baddr != baddr) {
		/* XXX: Need to verify this. */
		ctlr_bcopy(baddr + ndel, baddr,
		    ((ROWS * COLS) - 1) - (baddr + ndel) + 1, 0);
		ctlr_bcopy(0, (ROWS * COLS) - ndel, ndel, 0);
		ctlr_bcopy(ndel, 0, end_baddr - ndel + 1, 0);
	}

	/* NULL fill at the end. */
	for (i = 0; i < ndel; i++)
		ctlr_add(end_baddr - i, EBC_null, 0);

	/* Set the MDT for this field. */
	mdt_set(cursor_addr);

	/* Patch up the DBCS state for display. */
	(void) ctlr_dbcs_postprocess();
	return true;
}

static bool
Delete_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Delete", ia, argc, argv);
    if (check_argc("Delete", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("Delete", NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	net_sendc('\177');
	return true;
    }
    if (!do_delete()) {
	return true;
    }
    if (reverse) {
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
    action_debug("BackSpace", ia, argc, argv);
    if (check_argc("BackSpace", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("BackSpace", NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	linemode_send_erase();
	return true;
    }
    if (reverse) {
	(void) do_delete();
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
	int	baddr, faddr;
	enum dbcs_state d;

	baddr = cursor_addr;
	faddr = find_field_attribute(baddr);
	if (faddr == baddr || FA_IS_PROTECTED(ea_buf[baddr].fa)) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}
	if (baddr && faddr == baddr - 1)
		return;
	do_left();

	/*
	 * If we are now on an SI, move left again.
	 */
	if (ea_buf[cursor_addr].cc == EBC_si) {
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
	if (!do_delete())
		return;

	/*
	 * If we've just erased the last character of a DBCS subfield, erase
	 * the SO/SI pair as well.
	 */
	baddr = cursor_addr;
	DEC_BA(baddr);
	if (ea_buf[baddr].cc == EBC_so && ea_buf[cursor_addr].cc == EBC_si) {
		cursor_move(baddr);
		(void) do_delete();
	}
}

static bool
Erase_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Erase", ia, argc, argv);
    if (check_argc("Erase", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("Erase", NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	linemode_send_erase();
	return true;
    }
    if (reverse) {
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

    action_debug("Right", ia, argc, argv);
    if (check_argc("Right", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Right");
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

    action_debug("Left2", ia, argc, argv);
    if (check_argc("Left2", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Left2");
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

    action_debug("PreviousWord", ia, argc, argv);
    if (check_argc("PreviousWord", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("PreviousWord", NULL, NULL);
	return true;
    }
    if (IN_NVT || !formatted) {
	return false;
    }

    baddr = cursor_addr;
    prot = FA_IS_PROTECTED(get_field_attribute(baddr));

    /* Skip to before this word, if in one now. */
    if (!prot) {
	c = ea_buf[baddr].cc;
	while (!ea_buf[baddr].fa && c != EBC_space && c != EBC_null) {
	    DEC_BA(baddr);
	    if (baddr == cursor_addr) {
		return true;
	    }
	    c = ea_buf[baddr].cc;
	}
    }
    baddr0 = baddr;

    /* Find the end of the preceding word. */
    do {
	c = ea_buf[baddr].cc;
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
	c = ea_buf[baddr].cc;
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

    action_debug("Right2", ia, argc, argv);
    if (check_argc("Right2", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Right2");
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
		c = ea_buf[baddr].cc;
		if (ea_buf[baddr].fa)
			prot = FA_IS_PROTECTED(ea_buf[baddr].fa);
		else if (!prot && c != EBC_space && c != EBC_null)
			return baddr;
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
		c = ea_buf[baddr].cc;
		if (ea_buf[baddr].fa)
			return -1;
		if (in_word) {
			if (c == EBC_space || c == EBC_null)
				in_word = false;
		} else {
			if (c != EBC_space && c != EBC_null)
				return baddr;
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

    action_debug("NextWord", ia, argc, argv);
    if (check_argc("NextWord", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("NextWord", NULL, NULL);
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
    c = ea_buf[cursor_addr].cc;
    if (c != EBC_space && c != EBC_null) {
	baddr = cursor_addr;
	do {
	    c = ea_buf[baddr].cc;
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

    action_debug("Up", ia, argc, argv);
    if (check_argc("Up", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Up");
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

    action_debug("Down", ia, argc, argv);
    if (check_argc("Down", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Down");
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

    action_debug("Newline", ia, argc, argv);
    if (check_argc("Newline", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("Newline", NULL, NULL);
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
    action_debug("Dup", ia, argc, argv);
    if (check_argc("Dup", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("Dup", NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	return false;
    }
    if (key_Character(EBC_dup, false, false)) {
	cursor_move(next_unprotected(cursor_addr));
    }
    return true;
}


/*
 * FM key
 */
static bool
FieldMark_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("FieldMark", ia, argc, argv);
    if (check_argc("FieldMark", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("FieldMark", NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	return false;
    }
    (void) key_Character(EBC_fm, false, false);
    return true;
}


/*
 * Vanilla AID keys.
 */
static bool
Enter_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Enter", ia, argc, argv);
    if (check_argc("Enter", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock & KL_OIA_MINUS) {
	return false;
    } else if (kybdlock) {
	enq_ta("Enter", NULL, NULL);
    } else {
	key_AID(AID_ENTER);
    }
    return true;
}

static bool
SysReq_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("SysReq", ia, argc, argv);
    if (check_argc("SysReq", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (IN_NVT) {
	return false;
    }
    if (IN_E) {
	net_abort();
    } else {
	if (kybdlock & KL_OIA_MINUS) {
	    return false;
	} else if (kybdlock) {
	    enq_ta("SysReq", NULL, NULL);
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
    action_debug("Clear", ia, argc, argv);
    if (check_argc("Clear", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock & KL_OIA_MINUS) {
	return false;
    }
    if (kybdlock && CONNECTED) {
	enq_ta("Clear", NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	nvt_send_clear();
	return true;
    }
    buffer_addr = 0;
    ctlr_clear(true);
    cursor_move(0);
    if (CONNECTED) {
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
	register unsigned char	fa;
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
			if (ea_buf[designator].cc == 0x42 &&
			    ea_buf[designator2].cc == EBC_greater) {
				ctlr_add(designator2, EBC_question, CS_DBCS);
				mdt_clear(faddr);
			} else if (ea_buf[designator].cc == 0x42 &&
				   ea_buf[designator2].cc == EBC_question) {
				ctlr_add(designator2, EBC_greater, CS_DBCS);
				mdt_clear(faddr);
			} else if ((ea_buf[designator].cc == EBC_space &&
				    ea_buf[designator2].cc == EBC_space) ||
			           (ea_buf[designator].cc == EBC_null &&
				    ea_buf[designator2].cc == EBC_null)) {
				ctlr_add(designator2, EBC_greater, CS_DBCS);
				mdt_set(faddr);
				key_AID(AID_SELECT);
			} else if (ea_buf[designator].cc == 0x42 &&
				   ea_buf[designator2].cc == EBC_ampersand) {
				mdt_set(faddr);
				key_AID(AID_ENTER);
			} else {
				ring_bell();
			}
			return;
		}
	} 

	switch (ea_buf[designator].cc) {
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
    action_debug("CursorSelect", ia, argc, argv);
    if (check_argc("CursorSelect", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("CursorSelect", NULL, NULL);
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

    action_debug("EraseEOF", ia, argc, argv);
    if (check_argc("EraseEOF", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("EraseEOF");
    if (IN_NVT) {
	return false;
    }
    baddr = cursor_addr;
    fa = get_field_attribute(baddr);
    if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
	operator_error(KL_OERR_PROTECTED);
	return false;
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
	    ea_buf[baddr].cc = EBC_si;
	} else {
	    ea_buf[cursor_addr].cc = EBC_si;
	}
    }
    (void) ctlr_dbcs_postprocess();
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

    action_debug("EraseInput", ia, argc, argv);
    if (check_argc("EraseInput", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("EraseInput");
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
    (void) ctlr_dbcs_postprocess();

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

    action_debug("DeleteWord", ia, argc, argv);
    if (check_argc("DeleteWord", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("DeleteWord");
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
	operator_error(KL_OERR_PROTECTED);
	return false;
    }

    /* Backspace over any spaces to the left of the cursor. */
    for (;;) {
	baddr = cursor_addr;
	DEC_BA(baddr);
	if (ea_buf[baddr].fa) {
	    return true;
	}
	if (ea_buf[baddr].cc == EBC_null || ea_buf[baddr].cc == EBC_space) {
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
	if (ea_buf[baddr].cc == EBC_null || ea_buf[baddr].cc == EBC_space) {
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

    action_debug("DeleteField", ia, argc, argv);
    if (check_argc("DeleteField", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("DeleteField");
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
	operator_error(KL_OERR_PROTECTED);
	return false;
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
    action_debug("Insert", ia, argc, argv);
    if (check_argc("Insert", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("Insert");
    if (IN_NVT) {
	return false;
    }
    insert_mode(true);
    return true;
}


/*
 * Toggle insert mode key.
 */
static bool
ToggleInsert_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("ToggleInsert", ia, argc, argv);
    if (check_argc("ToggleInsert", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("ToggleInsert");
    if (IN_NVT) {
	return false;
    }
    if (insert) {
	insert_mode(false);
    } else {
	insert_mode(true);
    }
    return true;
}


/*
 * Toggle reverse mode key.
 */
static bool
ToggleReverse_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("ToggleReverse", ia, argc, argv);
    if (check_argc("ToggleReverse", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    if (kybdlock) {
	enq_ta("ToggleReverse", NULL, NULL);
	return true;
    }
    if (IN_NVT) {
	return false;
    }
    reverse_mode(!reverse);
    return true;
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

    action_debug("FieldEnd", ia, argc, argv);
    if (check_argc("FieldEnd", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    OERR_CLEAR_OR_ENQ("FieldEnd");
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
	c = ea_buf[baddr].cc;
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
 * MoveCursor action. Moves to a specific location.
 */
static bool
MoveCursor_action(ia_t ia, unsigned argc, const char **argv)
{
    int baddr;
    int row, col;

    action_debug("MoveCursor", ia, argc, argv);
    if (check_argc("MoveCursor", argc, 2, 2) < 0) {
	return false;
    }

    reset_idle_timer();
    if (kybdlock) {
	enq_ta("MoveCursor", argv[0], argv[1]);
	return true;
    }

    row = atoi(argv[0]);
    col = atoi(argv[1]);
    if (!IN_3270) {
	row--;
	col--;
    }
    if (row < 0) {
	row = 0;
    }
    if (col < 0) {
	col = 0;
    }
    baddr = ((row * COLS) + col) % (ROWS * COLS);
    cursor_move(baddr);

    return true;
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

    action_debug("Key", ia, argc, argv);
    reset_idle_timer();

    for (i = 0; i < argc; i++) {
	const char *s = argv[i];

	k = my_string_to_key(s, &keytype, &ucs4);
	if (k == KS_NONE && !ucs4) {
	    popup_an_error("Key: Nonexistent or invalid name: %s", s);
	    cancel_if_idle_command();
	    continue;
	}
	if (k & ~0xff) {
	    /*
	     * Can't pass symbolic names that aren't in the range 0x01..0xff.
	     */
	    popup_an_error("Key: Invalid name: %s", s);
	    cancel_if_idle_command();
	    continue;
	}
	if (k != KS_NONE) {
	    key_UCharacter(k, keytype, IA_KEY);
	} else {
	    key_UCharacter(ucs4, keytype, IA_KEY);
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
    unsigned i;
    size_t len = 0;
    char *s;

    action_debug("String", ia, argc, argv);
    reset_idle_timer();

    /* Determine the total length of the strings. */
    for (i = 0; i < argc; i++) {
	len += strlen(argv[i]);
    }
    if (!len) {
	return true;
    }

    /* Allocate a block of memory and copy them in. */
    s = Malloc(len + 1);
    s[0] = '\0';
    for (i = 0; i < argc; i++) {
	strcat(s, argv[i]);
    }

    /* Set a pending string. */
    ps_set(s, false);
    Free(s);
    return true;
}

/*
 * HexString action.
 */
static bool
HexString_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned i;
    size_t len = 0;
    char *s;
    const char *t;

    action_debug("HexString", ia, argc, argv);
    reset_idle_timer();

    /* Determine the total length of the strings. */
    for (i = 0; i < argc; i++) {
	t = argv[i];
	if (!strncmp(t, "0x", 2) || !strncmp(t, "0X", 2)) {
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
	if (!strncmp(t, "0x", 2) || !strncmp(t, "0X", 2))
	    t += 2;
	(void) strcat(s, t);
    }

    /* Set a pending string. */
    ps_set(s, true);
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
    action_debug("CircumNot", ia, argc, argv);
    if (check_argc("CircumNot", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();

    if (IN_3270 && composing == NONE) {
	key_UCharacter(0xac, KT_STD, IA_KEY);
    } else {
	key_UCharacter('^', KT_STD, IA_KEY);
    }
    return true;
}

/* PA key action for String actions */
static void
do_pa(unsigned n)
{
    if (n < 1 || n > PA_SZ) {
	popup_an_error("Unknown PA key %d", n);
	cancel_if_idle_command();
	return;
    }
    if (kybdlock) {
	enq_ta("PA", lazyaf("%d", n), NULL);
	return;
    }
    key_AID(pa_xlate[n-1]);
}

/* PF key action for String actions */
static void
do_pf(unsigned n)
{
    if (n < 1 || n > PF_SZ) {
	popup_an_error("Unknown PF key %d", n);
	cancel_if_idle_command();
	return;
    }
    if (kybdlock) {
	enq_ta("PF", lazyaf("%d", n), NULL);
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
	if (!IN_3270)
		return;
	if (lock)
		kybdlock_set(KL_SCROLLED, "kybd_scroll_lock");
	else
		kybdlock_clr(KL_SCROLLED, "kybd_scroll_lock");
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
			if (baddr <= b0)
				return false;
		}
	}

	cursor_move(baddr);
	return true;
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
	HEX, EBC, XGE
    } state = BASE;
    int literal = 0;
    int nc = 0;
    enum iaction ia = pasting ? IA_PASTE : IA_STRING;
    int orig_addr = cursor_addr;
    int orig_col = BA_TO_COL(cursor_addr);
    int last_addr = cursor_addr;
    int last_row = BA_TO_ROW(cursor_addr);
    bool just_wrapped = false;
    ucs4_t c;
    bool auto_skip = true;

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
		run_action("Left", ia, NULL, NULL);
		break;
	    case '\f':
		if (pasting) {
		    key_UCharacter(0x20, KT_STD, ia);
		} else {
		    run_action("Clear", ia, NULL, NULL);
		    if (IN_3270) {
			return xlen-1;
		    }
		}
		break;
	    case '\n':
		if (pasting) {
		    if (auto_skip) {
			if (!just_wrapped) {
			    run_action("Newline", ia, NULL, NULL);
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
		    run_action("Enter", ia, NULL, NULL);
		    if (IN_3270) {
			return xlen-1;
		    }
		}
		break;
	    case '\r':
		if (!pasting) {
		    run_action("Newline", ia, NULL, NULL);
		}
		break;
	    case '\t':
		run_action("Tab", ia, NULL, NULL);
		break;
	    case '\\':	/* backslashes are NOT special when pasting */
		if (!pasting) {
		    state = BACKSLASH;
		} else {
		    key_UCharacter((unsigned char)c, KT_STD, ia);
		}
		break;
	    case '\033': /* ESC is special only when pasting */
		if (pasting) {
		    state = XGE;
		}
		break;
	    case '[':	/* APL left bracket */
		if (pasting && appres.apl_mode) {
			key_UCharacter(latin1_Yacute, KT_GE, ia);
		} else {
			key_UCharacter((unsigned char)c, KT_STD, ia);
		}
		break;
	    case ']':	/* APL right bracket */
		if (pasting && appres.apl_mode) {
		    key_UCharacter(latin1_uml, KT_GE, ia);
		} else {
		    key_UCharacter((unsigned char)c, KT_STD, ia);
		}
		break;
	    case UPRIV_fm: /* private-use FM */
		if (pasting) {
		    key_Character(EBC_fm, false, true);
		}
		break;
	    case UPRIV_dup: /* private-use DUP */
		if (pasting) {
		    key_Character(EBC_dup, false, true);
		}
		break;
	    case UPRIV_eo: /* private-use EO */
		if (pasting) {
		    key_Character(EBC_eo, false, true);
		}
		break;
	    case UPRIV_sub: /* private-use SUB */
		if (pasting) {
		    key_Character(EBC_sub, false, true);
		}
		break;
	    default:
		if (pasting && (c >= UPRIV_GE_00 && c <= UPRIV_GE_ff)) {
		    key_Character(c - UPRIV_GE_00, KT_GE, ia);
		} else {
		    key_UCharacter(c, KT_STD, ia);
		}
		break;
	    }
	    break;

	case BACKSLASH:	/* last character was a backslash */
	    switch (c) {
	    case 'a':
		popup_an_error("String: Bell not supported");
		cancel_if_idle_command();
		state = BASE;
		break;
	    case 'b':
		run_action("Left", ia, NULL, NULL);
		state = BASE;
		break;
	    case 'f':
		run_action("Clear", ia, NULL, NULL);
		state = BASE;
		if (IN_3270) {
		    return xlen-1;
		}
		break;
	    case 'n':
		run_action("Enter", ia, NULL, NULL);
		state = BASE;
		if (IN_3270) {
		    return xlen-1;
		}
		break;
	    case 'p':
		state = BACKP;
		break;
	    case 'r':
		run_action("Newline", ia, NULL, NULL);
		state = BASE;
		break;
	    case 't':
		run_action("Tab", ia, NULL, NULL);
		state = BASE;
		break;
	    case 'T':
		run_action("BackTab", ia, NULL, NULL);
		state = BASE;
		break;
	    case 'v':
		popup_an_error("String: Vertical tab not supported");
		cancel_if_idle_command();
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
		key_UCharacter((unsigned char) c, KT_STD, ia);
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
		popup_an_error("String: Unknown character after \\p");
		cancel_if_idle_command();
		state = BASE;
		break;
	    }
	    break;

	case BACKPF: /* last three characters were "\pf" */
	    if (nc < 2 && isdigit((unsigned char)c)) {
		literal = (literal * 10) + (c - '0');
		nc++;
	    } else if (!nc) {
		popup_an_error("String: Unknown character after \\pf");
		cancel_if_idle_command();
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
		popup_an_error("String: Unknown character after \\pa");
		cancel_if_idle_command();
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
		popup_an_error("String: Missing hex digits after \\x");
		cancel_if_idle_command();
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
		popup_an_error("String: Missing hex digits after \\e");
		cancel_if_idle_command();
		state = BASE;
		continue;
	    }
	case OCTAL:	/* have seen \ and one or more octal digits */
	    if (nc < 3 && isdigit((unsigned char)c) && c < '8') {
		literal = (literal * 8) + FROM_HEX(c);
		nc++;
		break;
	    } else {
		key_UCharacter((unsigned char) literal, KT_STD, ia);
		state = BASE;
		continue;
	    }
	case HEX:	/* have seen \x and one or more hex digits */
	    if (nc < 4 && isxdigit((unsigned char)c)) {
		literal = (literal * 16) + FROM_HEX(c);
		nc++;
		break;
	    } else {
		key_UCharacter((unsigned char) literal, KT_STD, ia);
		state = BASE;
		continue;
	    }
	case EBC:	/* have seen \e and one or more hex digits */
	    if (nc < 4 && isxdigit((unsigned char)c)) {
		literal = (literal * 16) + FROM_HEX(c);
		nc++;
		break;
	    } else {
		vtrace(" %s -> Key(X'%02X')\n", ia_name[(int) ia], literal);
		if (!(literal & ~0xff)) {
		    key_Character((unsigned char) literal, false, true);
		} else {
		    unsigned char ebc_pair[2];

		    ebc_pair[0] = (literal >> 8) & 0xff;
		    ebc_pair[1] = literal & 0xff;
		    key_WCharacter(ebc_pair);
		}
		state = BASE;
		continue;
	    }
	case XGE:	/* have seen ESC */
	    switch (c) {
	    case ';':	/* FM */
		key_Character(EBC_fm, false, true);
		break;
	    case '*':	/* DUP */
		key_Character(EBC_dup, false, true);
		break;
	    default:
		key_UCharacter((unsigned char) c, KT_GE, ia);
		break;
	    }
	    state = BASE;
	    break;
	}
	ws++;
	xlen--;
    }

    switch (state) {
    case BASE:
	if (MarginedPaste() && BA_TO_COL(cursor_addr) < orig_col) {
	    (void) remargin(orig_col);
	}
	break;
    case OCTAL:
    case HEX:
	key_UCharacter((unsigned char) literal, KT_STD, ia);
	state = BASE;
	if (MarginedPaste() && BA_TO_COL(cursor_addr) < orig_col) {
	    (void) remargin(orig_col);
	}
	break;
    case EBC:
	vtrace(" %s -> Key(X'%02X')\n", ia_name[(int) ia], literal);
	key_Character((unsigned char) literal, false, true);
	state = BASE;
	if (MarginedPaste() && BA_TO_COL(cursor_addr) < orig_col) {
	    (void) remargin(orig_col);
	}
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
	popup_an_error("String: Missing data after \\");
	cancel_if_idle_command();
	break;
    }

    return xlen;
}

/* Multibyte version of emulate_uinput. */
size_t
emulate_input(const char *s, size_t len, bool pasting)
{
	static ucs4_t *w_ibuf = NULL;
	static size_t w_ibuf_len = 0;
	int xlen;

	/* Convert from a multi-byte string to a Unicode string. */
	if (len + 1 > w_ibuf_len) {
		w_ibuf_len = len + 1;
		w_ibuf = (ucs4_t *)Realloc(w_ibuf, w_ibuf_len * sizeof(ucs4_t));
	}
	xlen = multibyte_to_unicode_string(s, len, w_ibuf, w_ibuf_len);
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
	int nbytes = 0;

	/* Validate the string. */
	if (strlen(s) % 2) {
		popup_an_error("HexString: Odd number of characters in "
			"specification");
		cancel_if_idle_command();
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
				popup_an_error("HexString: Double \\E");
				cancel_if_idle_command();
				return;
			}
			if (!IN_3270) {
				popup_an_error("HexString: \\E in NVT mode");
				cancel_if_idle_command();
				return;
			}
			escaped = true;
		} else {
			popup_an_error("HexString: Illegal character in "
				"specification");
			cancel_if_idle_command();
			return;
		}
		t += 2;
	}
	if (escaped) {
		popup_an_error("HexString: Nothing follows \\E");
		cancel_if_idle_command();
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
			if (IN_3270)
				key_Character(c, escaped, true);
			else
				*tbuf++ = (unsigned char)c;
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
 * Returns the length of the input field, or 0 if there is no field
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
	if (kybdlock || !IN_3270) {
		return 0;
	}

	/*
	 * If unformatted, guess that we can use all the NULs from the cursor
	 * address forward, leaving one empty slot to delimit the end of the
	 * command.  It's up to the host to make sense of what we send.
	 */
	if (!formatted) {
		baddr = cursor_addr;

		while (ea_buf[baddr].cc == EBC_null ||
		       ea_buf[baddr].cc == EBC_space) {
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
			return 0;
		}

		/* Move the cursor there. */
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
 * Translate a key name to a key, including APL and extended characters.
 */
static ks_t
my_string_to_key(const char *s, enum keytype *keytypep, ucs4_t *ucs4)
{
    ks_t k;
    int consumed;
    enum me_fail error;

    /* No UCS-4 yet. */
    *ucs4 = 0L;

    /* Look for my contrived APL symbols. */
    if (!strncmp(s, "apl_", 4)) {
	int is_ge;

	k = apl_string_to_key(s, &is_ge);
	if (is_ge) {
	    *keytypep = KT_GE;
	} else {
	    *keytypep = KT_STD;
	}
	return k;
    } else {
	/* Look for a standard HTML entity or X11 keysym name. */
	k = string_to_key((char *)s);
	*keytypep = KT_STD;
	if (k != KS_NONE) {
	    return k;
	}
    }

    /* Look for "euro". */
    if (!strcasecmp(s, "euro")) {
	*ucs4 = 0x20ac;
	return KS_NONE;
    }

    /* Look for U+nnnn of 0xXXXX. */
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
build_composites(void)
{
    char *c, *c0, *c1;
    char *ln;
    char ksname[3][64];
    char junk[2];
    ks_t k[3];
    enum keytype a[3];
    int i;
    struct composite *cp;

if (appres.interactive.compose_map == NULL) {
    popup_an_error("Compose: No %s defined", ResComposeMap);
    return false;
}
c0 = get_fresource("%s.%s", ResComposeMap, appres.interactive.compose_map);
if (c0 == NULL) {
    popup_an_error("Compose: Cannot find %s \"%s\"", ResComposeMap,
	    appres.interactive.compose_map);
    return false;
}
c1 = c = NewString(c0);	/* will be modified by strtok */
while ((ln = strtok(c, "\n"))) {
    bool okay = true;

    c = NULL;
    if (sscanf(ln, " %63[^+ \t] + %63[^= \t] =%63s%1s",
		ksname[0], ksname[1], ksname[2], junk) != 3) {
	popup_an_error("Compose: Invalid syntax: %s", ln);
	continue;
    }
    for (i = 0; i < 3; i++) {
	ucs4_t ucs4;

	k[i] = my_string_to_key(ksname[i], &a[i], &ucs4);
	if (k[i] == KS_NONE) {
	    /* For now, ignore UCS4.  XXX: Fix this. */
	    popup_an_error("Compose: Invalid name: \"%s\"", ksname[i]);
	    okay = false;
	    break;
	}
    }
    if (!okay) {
	continue;
    }
    composites = (struct composite *) Realloc((char *)composites,
	    (n_composites + 1) * sizeof(struct composite));
    cp = composites + n_composites;
    cp->k1.key = k[0];
    cp->k1.keytype = a[0];
    cp->k2.key = k[1];
    cp->k2.keytype = a[1];
    cp->translation.key = k[2];
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
    action_debug("Compose", ia, argc, argv);
    if (check_argc("Compose", argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();

    if (!composites && !build_composites()) {
	return true;
    }

    if (composing == NONE) {
	composing = COMPOSE;
	status_compose(true, 0, KT_STD);
    }
    return true;
}
