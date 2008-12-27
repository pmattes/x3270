/*
 * Modifications Copyright 1993-2008 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	kybd.c
 *		This module handles the keyboard for the 3270 emulator.
 */

#include "globals.h"

#if defined(X3270_DISPLAY) /*[*/
#include <X11/Xatom.h>
#endif
#define XK_3270
#if defined(X3270_APL) /*[*/
#define XK_APL
#endif /*]*/
#include <X11/keysym.h>

#include <fcntl.h>
#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"
#if defined(X3270_DISPLAY) /*[*/
#include "keysym2ucs.h"
#endif /*]*/
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "aplc.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "ftc.h"
#include "hostc.h"
#include "idlec.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "printc.h"
#include "screenc.h"
#if defined(X3270_DISPLAY) /*[*/
#include "selectc.h"
#endif /*]*/
#include "statusc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"

#if defined(_WIN32) /*[*/
#include <windows.h>
#endif /*]*/

/*#define KYBDLOCK_TRACE	1*/

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
static unsigned long unlock_id;
static time_t unlock_delay_time;
Boolean key_Character(int code, Boolean with_ge, Boolean pasting,
			     Boolean *skipped);
static Boolean flush_ta(void);
static void key_AID(unsigned char aid_code);
static void kybdlock_set(unsigned int bits, const char *cause);
static KeySym MyStringToKeysym(char *s, enum keytype *keytypep,
	ucs4_t *ucs4);

#if defined(X3270_DBCS) /*[*/
Boolean key_WCharacter(unsigned char code[], Boolean *skipped);
#endif /*]*/

static Boolean		insert = False;		/* insert mode */
static Boolean		reverse = False;	/* reverse-input mode */

/* Globals */
unsigned int	kybdlock = KL_NOT_CONNECTED;
unsigned char	aid = AID_NO;		/* current attention ID */

/* Composite key mappings. */

struct akeysym {
	KeySym keysym;
	enum keytype keytype;
};
static struct akeysym cc_first;
static struct composite {
	struct akeysym k1, k2;
	struct akeysym translation;
} *composites = NULL;
static int n_composites = 0;

#define ak_eq(k1, k2)	(((k1).keysym  == (k2).keysym) && \
			 ((k1).keytype == (k2).keytype))

static struct ta {
	struct ta *next;
	XtActionProc fn;
	char *parm1;
	char *parm2;
} *ta_head = (struct ta *) NULL,
  *ta_tail = (struct ta *) NULL;

static char dxl[] = "0123456789abcdef";
#define FROM_HEX(c)	(strchr(dxl, tolower(c)) - dxl)

extern Widget *screen;

#define KYBDLOCK_IS_OERR	(kybdlock && !(kybdlock & ~KL_OERR_MASK))


/*
 * Put an action on the typeahead queue.
 */
static void
enq_ta(XtActionProc fn, char *parm1, char *parm2)
{
	struct ta *ta;

	/* If no connection, forget it. */
	if (!CONNECTED) {
		trace_event("  dropped (not connected)\n");
		return;
	}

	/* If operator error, complain and drop it. */
	if (kybdlock & KL_OERR_MASK) {
		ring_bell();
		trace_event("  dropped (operator error)\n");
		return;
	}

	/* If scroll lock, complain and drop it. */
	if (kybdlock & KL_SCROLLED) {
		ring_bell();
		trace_event("  dropped (scrolled)\n");
		return;
	}

	/* If typeahead disabled, complain and drop it. */
	if (!appres.typeahead) {
		trace_event("  dropped (no typeahead)\n");
		return;
	}

	ta = (struct ta *) Malloc(sizeof(*ta));
	ta->next = (struct ta *) NULL;
	ta->fn = fn;
	ta->parm1 = ta->parm2 = CN;
	if (parm1) {
		ta->parm1 = NewString(parm1);
		if (parm2)
			ta->parm2 = NewString(parm2);
	}
	if (ta_head)
		ta_tail->next = ta;
	else {
		ta_head = ta;
		status_typeahead(True);
	}
	ta_tail = ta;

	trace_event("  action queued (kybdlock 0x%x)\n", kybdlock);
}

/*
 * Execute an action from the typeahead queue.
 */
Boolean
run_ta(void)
{
	struct ta *ta;

	if (kybdlock || (ta = ta_head) == (struct ta *)NULL)
		return False;

	if ((ta_head = ta->next) == (struct ta *)NULL) {
		ta_tail = (struct ta *)NULL;
		status_typeahead(False);
	}

	action_internal(ta->fn, IA_TYPEAHEAD, ta->parm1, ta->parm2);
	Free(ta->parm1);
	Free(ta->parm2);
	Free(ta);

	return True;
}

/*
 * Flush the typeahead queue.
 * Returns whether or not anything was flushed.
 */
static Boolean
flush_ta(void)
{
	struct ta *ta, *next;
	Boolean any = False;

	for (ta = ta_head; ta != (struct ta *) NULL; ta = next) {
		Free(ta->parm1);
		Free(ta->parm2);
		next = ta->next;
		Free(ta);
		any = True;
	}
	ta_head = ta_tail = (struct ta *) NULL;
	status_typeahead(False);
	return any;
}

/* Decode keyboard lock bits. */
static char *
kybdlock_decode(char *how, unsigned int bits)
{
    	static char buf[1024];
	char *s = buf;
	char *space = "";

	if (bits == (unsigned int)-1)
	    	return "all";
	if (bits & KL_OERR_MASK) {
	    	s += sprintf(s, "%sOERR(", how);
	    	switch(bits & KL_OERR_MASK) {
		    case KL_OERR_PROTECTED:
			s += sprintf(s, "PROTECTED");
			break;
		    case KL_OERR_NUMERIC:
			s += sprintf(s, "NUMERIC");
			break;
		    case KL_OERR_OVERFLOW:
			s += sprintf(s, "OVERFLOW");
			break;
		    case KL_OERR_DBCS:
			s += sprintf(s, "DBCS");
			break;
		    default:
			s += sprintf(s, "?%d", bits & KL_OERR_MASK);
			break;
		}
		s += sprintf(s, ")");
		space = " ";
	}
	if (bits & KL_NOT_CONNECTED) {
	    s += sprintf(s, "%s%sNOT_CONNECTED", space, how);
	    space = " ";
	}
	if (bits & KL_AWAITING_FIRST) {
	    s += sprintf(s, "%s%sAWAITING_FIRST", space, how);
	    space = " ";
	}
	if (bits & KL_OIA_TWAIT) {
	    s += sprintf(s, "%s%sOIA_TWAIT", space, how);
	    space = " ";
	}
	if (bits & KL_OIA_LOCKED) {
	    s += sprintf(s, "%s%sOIA_LOCKED", space, how);
	    space = " ";
	}
	if (bits & KL_DEFERRED_UNLOCK) {
	    s += sprintf(s, "%s%sDEFERRED_UNLOCK", space, how);
	    space = " ";
	}
	if (bits & KL_ENTER_INHIBIT) {
	    s += sprintf(s, "%s%sENTER_INHIBIT", space, how);
	    space = " ";
	}
	if (bits & KL_SCROLLED) {
	    s += sprintf(s, "%s%sSCROLLED", space, how);
	    space = " ";
	}
	if (bits & KL_OIA_MINUS) {
	    s += sprintf(s, "%s%sOIA_MINUS", space, how);
	    space = " ";
	}

	return buf;
}

/* Set bits in the keyboard lock. */
static void
kybdlock_set(unsigned int bits, const char *cause _is_unused)
{
	unsigned int n;

	trace_event("Keyboard lock(%s) %s\n", cause,
		kybdlock_decode("+", bits));
	n = kybdlock | bits;
	if (n != kybdlock) {
#if defined(KYBDLOCK_TRACE) /*[*/
	       trace_event("  %s: kybdlock |= 0x%04x, 0x%04x -> 0x%04x\n",
		    cause, bits, kybdlock, n);
#endif /*]*/
		if ((kybdlock ^ bits) & KL_DEFERRED_UNLOCK) {
			/* Turned on deferred unlock. */
			unlock_delay_time = time(NULL);
		}
		kybdlock = n;
		status_kybdlock();
	}
}

/* Clear bits in the keyboard lock. */
void
kybdlock_clr(unsigned int bits, const char *cause _is_unused)
{
	unsigned int n;

	if (kybdlock & bits)
		trace_event("Keyboard unlock(%s) %s\n", cause,
			kybdlock_decode("-", kybdlock & bits));
	n = kybdlock & ~bits;
	if (n != kybdlock) {
#if defined(KYBDLOCK_TRACE) /*[*/
		trace_event("  %s: kybdlock &= ~0x%04x, 0x%04x -> 0x%04x\n",
		    cause, bits, kybdlock, n);
#endif /*]*/
		if ((kybdlock ^ n) & KL_DEFERRED_UNLOCK) {
			/* Turned off deferred unlock. */
			unlock_delay_time = 0;
		}
		kybdlock = n;
		status_kybdlock();
	}
}

/*
 * Set or clear enter-inhibit mode.
 */
void
kybd_inhibit(Boolean inhibit)
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
kybd_connect(Boolean connected)
{
	if (kybdlock & KL_DEFERRED_UNLOCK)
		RemoveTimeOut(unlock_id);
	kybdlock_clr(-1, "kybd_connect");

	if (connected) {
		/* Wait for any output or a WCC(restore) from the host */
		kybdlock_set(KL_AWAITING_FIRST, "kybd_connect");
	} else {
		kybdlock_set(KL_NOT_CONNECTED, "kybd_connect");
		(void) flush_ta();
	}
}

/*
 * Called when we switch between 3270 and ANSI modes.
 */
static void
kybd_in3270(Boolean in3270 _is_unused)
{
	if (kybdlock & KL_DEFERRED_UNLOCK)
		RemoveTimeOut(unlock_id);
	kybdlock_clr(~KL_AWAITING_FIRST, "kybd_in3270");

	/* There might be a macro pending. */
	if (CONNECTED)
		ps_process();
}

/*
 * Called to initialize the keyboard logic.
 */
void
kybd_init(void)
{
	/* Register interest in connect and disconnect events. */
	register_schange(ST_CONNECT, kybd_connect);
	register_schange(ST_3270_MODE, kybd_in3270);
}

/*
 * Toggle insert mode.
 */
static void
insert_mode(Boolean on)
{
	insert = on;
	status_insert_mode(on);
}

/*
 * Toggle reverse mode.
 */
static void
reverse_mode(Boolean on)
{
	reverse = on;
	status_reverse_mode(on);
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
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		register unsigned i;

		if (aid_code == AID_ENTER) {
			net_sendc('\r');
			return;
		}
		for (i = 0; i < PF_SZ; i++)
			if (aid_code == pf_xlate[i]) {
				ansi_send_pf(i+1);
				return;
			}
		for (i = 0; i < PA_SZ; i++)
			if (aid_code == pa_xlate[i]) {
				ansi_send_pa(i+1);
				return;
			}
		return;
	}
#endif /*]*/
#if defined(X3270_PLUGIN) /*[*/
	plugin_aid(aid_code);
#endif /*]*/
	if (IN_SSCP) {
		if (kybdlock & KL_OIA_MINUS)
			return;
		if (aid_code != AID_ENTER && aid_code != AID_CLEAR) {
			status_minus();
			kybdlock_set(KL_OIA_MINUS, "key_AID");
			return;
		}
	}
	if (IN_SSCP && aid_code == AID_ENTER) {
		/* Act as if the host had written our input. */
		buffer_addr = cursor_addr;
	}
	if (!IN_SSCP || aid_code != AID_CLEAR) {
		status_twait();
		mcursor_waiting();
		insert_mode(False);
		kybdlock_set(KL_OIA_TWAIT | KL_OIA_LOCKED, "key_AID");
	}
	aid = aid_code;
	ctlr_read_modified(aid, False);
	ticking_start(False);
	status_ctlr_done();
}

void
PF_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	unsigned k;

	action_debug(PF_action, event, params, num_params);
	if (check_usage(PF_action, *num_params, 1, 1) < 0)
		return;
	k = atoi(params[0]);
	if (k < 1 || k > PF_SZ) {
		popup_an_error("%s: Invalid argument '%s'",
		    action_name(PF_action), params[0]);
		cancel_if_idle_command();
		return;
	}
	reset_idle_timer();
	if (kybdlock & KL_OIA_MINUS)
		return;
	else if (kybdlock)
		enq_ta(PF_action, params[0], CN);
	else
		key_AID(pf_xlate[k-1]);
}

void
PA_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	unsigned k;

	action_debug(PA_action, event, params, num_params);
	if (check_usage(PA_action, *num_params, 1, 1) < 0)
		return;
	k = atoi(params[0]);
	if (k < 1 || k > PA_SZ) {
		popup_an_error("%s: Invalid argument '%s'",
		    action_name(PA_action), params[0]);
		cancel_if_idle_command();
		return;
	}
	reset_idle_timer();
	if (kybdlock & KL_OIA_MINUS)
		return;
	else if (kybdlock)
		enq_ta(PA_action, params[0], CN);
	else
		key_AID(pa_xlate[k-1]);
}


/*
 * ATTN key, per RFC 2355.  Sends IP, regardless.
 */
void
Attn_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Attn_action, event, params, num_params);
	if (!IN_3270)
		return;
	reset_idle_timer();

	if (IN_E) {
	    if (net_bound()) {
		net_interrupt();
	    } else {
		status_minus();
		kybdlock_set(KL_OIA_MINUS, "Attn_action");
	    }
	} else {
	    net_break();
	}
}

/*
 * IAC IP, which works for 5250 System Request and interrupts the program
 * on an AS/400, even when the keyboard is locked.
 *
 * This is now the same as the Attn action.
 */
void
Interrupt_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Interrupt_action, event, params, num_params);
	if (!IN_3270)
		return;
	reset_idle_timer();
	net_interrupt();
}



/*
 * Prepare for an insert of 'count' bytes.
 * Returns True if the insert is legal, False otherwise.
 */
static Boolean
ins_prep(int faddr, int baddr, int count)
{
	int next_faddr;
	int xaddr;
	int need;
	int ntb;
	int tb_start = -1;
	int copy_len;

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
		operator_error(KL_OERR_OVERFLOW);
		return False;
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

	return True;

}

#define GE_WFLAG	0x100
#define PASTE_WFLAG	0x200

static void
key_Character_wrapper(Widget w _is_unused, XEvent *event _is_unused, String *params,
    Cardinal *num_params _is_unused)
{
	int code;
	Boolean with_ge = False;
	Boolean pasting = False;
	char mb[16];
	ucs4_t uc;

	code = atoi(params[0]);
	if (code & GE_WFLAG) {
		with_ge = True;
		code &= ~GE_WFLAG;
	}
	if (code & PASTE_WFLAG) {
		pasting = True;
		code &= ~PASTE_WFLAG;
	}
	ebcdic_to_multibyte_x(code, with_ge? CS_GE: CS_BASE,
		mb, sizeof(mb), True, &uc);
	trace_event(" %s -> Key(%s\"%s\")\n",
	    ia_name[(int) ia_cause],
	    with_ge ? "GE " : "", mb);
	(void) key_Character(code, with_ge, pasting, NULL);
}

/*
 * Handle an ordinary displayable character key.  Lots of stuff to handle
 * insert-mode, protected fields and etc.
 */
/*static*/ Boolean
key_Character(int code, Boolean with_ge, Boolean pasting, Boolean *skipped)
{
	register int	baddr, faddr, xaddr;
	register unsigned char	fa;
	enum dbcs_why why = DBCS_FIELD;

	reset_idle_timer();

	if (skipped != NULL)
		*skipped = False;

	if (kybdlock) {
		char codename[64];

		(void) sprintf(codename, "%d", code |
			(with_ge ? GE_WFLAG : 0) |
			(pasting ? PASTE_WFLAG : 0));
		enq_ta(key_Character_wrapper, codename, CN);
		return False;
	}
	baddr = cursor_addr;
	faddr = find_field_attribute(baddr);
	fa = get_field_attribute(baddr);
	if (ea_buf[baddr].fa || FA_IS_PROTECTED(fa)) {
		operator_error(KL_OERR_PROTECTED);
		return False;
	}
	if (appres.numeric_lock && FA_IS_NUMERIC(fa) &&
	    !((code >= EBC_0 && code <= EBC_9) ||
	      code == EBC_minus || code == EBC_period)) {
		operator_error(KL_OERR_NUMERIC);
		return False;
	}

	/* Can't put an SBCS in a DBCS field. */
	if (ea_buf[faddr].cs == CS_DBCS) {
		operator_error(KL_OERR_DBCS);
		return False;
	}

	/* If it's an SI (end of DBCS subfield), move over one position. */
	if (ea_buf[baddr].cc == EBC_si) {
		INC_BA(baddr);
		if (baddr == faddr) {
			operator_error(KL_OERR_OVERFLOW);
			return False;
		}
	}

	/* Add the character. */
	if (ea_buf[baddr].cc == EBC_so) {

		if (insert) {
			if (!ins_prep(faddr, baddr, 1))
				return False;
		} else {
			Boolean was_si = False;

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
#if defined(X3270_ANSI) /*[*/
			ctlr_add_bg(xaddr, 0);
#endif /*]*/
			if (!was_si) {
				INC_BA(xaddr);
				ctlr_add(xaddr, EBC_so, CS_BASE);
				ctlr_add_fg(xaddr, 0);
#if defined(X3270_ANSI) /*[*/
				ctlr_add_bg(xaddr, 0);
#endif /*]*/
			}
		}

	} else switch (ctlr_lookleft_state(baddr, &why)) {
	case DBCS_RIGHT:
		DEC_BA(baddr);
		/* fall through... */
	case DBCS_LEFT:
		if (why == DBCS_ATTRIBUTE) {
			if (insert) {
				if (!ins_prep(faddr, baddr, 1))
					return False;
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
			Boolean was_si;

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
					if (!ins_prep(faddr, baddr, 1))
						return False;
				} else {
					if (!ins_prep(faddr, baddr, 3))
						return False;
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
		if (insert && !ins_prep(faddr, baddr, 1))
			return False;
		break;
	}
	ctlr_add(baddr, (unsigned char)code,
	    (unsigned char)(with_ge ? CS_GE : 0));
	ctlr_add_fg(baddr, 0);
	ctlr_add_gr(baddr, 0);
	INC_BA(baddr);

	/* Replace leading nulls with blanks, if desired. */
	if (formatted && toggled(BLANK_FILL)) {
		register int	baddr_fill = baddr;

		DEC_BA(baddr_fill);
		while (baddr_fill != faddr) {

			/* Check for backward line wrap. */
			if ((baddr_fill % COLS) == COLS - 1) {
				Boolean aborted = True;
				register int baddr_scan = baddr_fill;

				/*
				 * Check the field within the preceeding line
				 * for NULLs.
				 */
				while (baddr_scan != faddr) {
					if (ea_buf[baddr_scan].cc != EBC_null) {
						aborted = False;
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
	if (pasting || (code != EBC_dup)) {
		while (ea_buf[baddr].fa) {
			if (skipped != NULL)
				*skipped = True;
			if (FA_IS_SKIP(ea_buf[baddr].fa))
				baddr = next_unprotected(baddr);
			else
				INC_BA(baddr);
		}
		cursor_move(baddr);
	}

	(void) ctlr_dbcs_postprocess();
	return True;
}

#if defined(X3270_DBCS) /*[*/
static void
key_WCharacter_wrapper(Widget w _is_unused, XEvent *event _is_unused, String *params,
    Cardinal *num_params _is_unused)
{
	int code;
	unsigned char codebuf[2];

	code = atoi(params[0]);
	trace_event(" %s -> Key(0x%04x)\n",
	    ia_name[(int) ia_cause], code);
	codebuf[0] = (code >> 8) & 0xff;
	codebuf[1] = code & 0xff;
	(void) key_WCharacter(codebuf, NULL);
}

/*
 * Input a DBCS character.
 * Returns True if a character was stored in the buffer, False otherwise.
 */
Boolean
key_WCharacter(unsigned char code[], Boolean *skipped)
{
	int baddr;
	register unsigned char fa;
	int faddr;
	enum dbcs_state d;
	int xaddr;
	Boolean done = False;
	Boolean no_si = False;
	extern unsigned char reply_mode; /* XXX */

	reset_idle_timer();

	if (kybdlock) {
		char codename[64];

		(void) sprintf(codename, "%d", (code[0] << 8) | code[1]);
		enq_ta(key_WCharacter_wrapper, codename, CN);
		return False;
	}

	if (skipped != NULL)
		*skipped = False;

	/* In DBCS mode? */
	if (!dbcs) {
		trace_event("DBCS character received when not in DBCS mode, "
		    "ignoring.\n");
		return True;
	}

#if defined(X3270_ANSI) /*[*/
	/* In ANSI mode? */
	if (IN_ANSI) {
	    char mb[16];

	    (void) ebcdic_to_multibyte((code[0] << 8) | code[1], mb,
				       sizeof(mb));
	    net_sends(mb);
	    return True;
	}
#endif /*]*/

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	faddr = find_field_attribute(baddr);

	/* Protected? */
	if (ea_buf[baddr].fa || FA_IS_PROTECTED(fa)) {
		operator_error(KL_OERR_PROTECTED);
		return False;
	}

	/* Numeric? */
	if (appres.numeric_lock && FA_IS_NUMERIC(fa)) {
		operator_error(KL_OERR_NUMERIC);
		return False;
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
			if (!ins_prep(faddr, baddr, 2)) {
				return False;
			}
		}
		ctlr_add(baddr, code[0], ea_buf[baddr].cs);
		INC_BA(baddr);
		ctlr_add(baddr, code[1], ea_buf[baddr].cs);
		INC_BA(baddr);
		done = True;
		break;
	case DBCS_SB:
		/* Back up one position and process it as an SI. */
		DEC_BA(baddr);
		/* fall through... */
	case DBCS_SI:
		/* Extend the subfield to the right. */
		if (insert) {
			if (!ins_prep(faddr, baddr, 2)) {
				return False;
			}
		} else {
			/* Don't overwrite a field attribute or an SO. */
			xaddr = baddr;
			INC_BA(xaddr);	/* C1 */
			if (ea_buf[xaddr].fa)
				break;
			if (ea_buf[xaddr].cc == EBC_so)
				no_si = True;
			INC_BA(xaddr);	/* SI */
			if (ea_buf[xaddr].fa || ea_buf[xaddr].cc == EBC_so)
				break;
		}
		ctlr_add(baddr, code[0], ea_buf[baddr].cs);
		INC_BA(baddr);
		ctlr_add(baddr, code[1], ea_buf[baddr].cs);
		if (!no_si) {
			INC_BA(baddr);
			ctlr_add(baddr, EBC_si, ea_buf[baddr].cs);
		}
		done = True;
		break;
	case DBCS_DEAD:
		break;
	case DBCS_NONE:
		if (ea_buf[faddr].ic) {
			Boolean extend_left = False;

			/* Is there room? */
			if (insert) {
				if (!ins_prep(faddr, baddr, 4)) {
					return False;
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
						extend_left = True;
						no_si = True;
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
					no_si = True;
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
			ctlr_add(baddr, code[0], ea_buf[baddr].cs);
			INC_BA(baddr);
			ctlr_add(baddr, code[1], ea_buf[baddr].cs);
			if (!no_si) {
				INC_BA(baddr);
				ctlr_add(baddr, EBC_si, ea_buf[baddr].cs);
			}
			done = True;
		} else if (reply_mode == SF_SRM_CHAR) {
			/* Use the character attribute. */
			if (insert) {
				if (!ins_prep(faddr, baddr, 2)) {
					return False;
				}
			} else {
				xaddr = baddr;
				INC_BA(xaddr);
				if (ea_buf[xaddr].fa)
					break;
			}
			ctlr_add(baddr, code[0], CS_DBCS);
			INC_BA(baddr);
			ctlr_add(baddr, code[1], CS_DBCS);
			INC_BA(baddr);
			done = True;
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
			if (skipped != NULL)
				*skipped = True;
			if (FA_IS_SKIP(ea_buf[baddr].fa))
				baddr = next_unprotected(baddr);
			else
				INC_BA(baddr);
		}
		cursor_move(baddr);
		(void) ctlr_dbcs_postprocess();
		return True;
	} else {
		operator_error(KL_OERR_DBCS);
		return False;
	}
}
#endif /*]*/

/*
 * Handle an ordinary character key, given its Unicode value.
 */
static void
key_UCharacter(ucs4_t ucs4, enum keytype keytype, enum iaction cause,
	       Boolean *skipped)
{
	register int i;
	struct akeysym ak;

	reset_idle_timer();

	if (skipped != NULL)
		*skipped = False;

	ak.keysym = ucs4;
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
			cc_first.keysym = ucs4;
			cc_first.keytype = keytype;
			composing = FIRST;
			status_compose(True, ucs4, keytype);
		} else {
			ring_bell();
			composing = NONE;
			status_compose(False, 0, KT_STD);
		}
		return;
	    case FIRST:
		composing = NONE;
		status_compose(False, 0, KT_STD);
		for (i = 0; i < n_composites; i++)
			if ((ak_eq(composites[i].k1, cc_first) &&
			     ak_eq(composites[i].k2, ak)) ||
			    (ak_eq(composites[i].k1, ak) &&
			     ak_eq(composites[i].k2, cc_first)))
				break;
		if (i < n_composites) {
			ucs4 = composites[i].translation.keysym;
			keytype = composites[i].translation.keytype;
		} else {
			ring_bell();
			return;
		}
		break;
	}

	trace_event(" %s -> Key(U+%04x)\n", ia_name[(int) cause], ucs4);
	if (IN_3270) {
	    	ebc_t ebc;
		Boolean ge;

		if (ucs4 < ' ') {
			trace_event("  dropped (control char)\n");
			return;
		}
		ebc = unicode_to_ebcdic_ge(ucs4, &ge);
		if (ebc == 0) {
			trace_event("  dropped (no EBCDIC translation)\n");
			return;
		}
#if defined(X3270_DBCS) /*[*/
		if (ebc & 0xff00) {
		    	unsigned char code[2];

			code[0] = (ebc & 0xff00)>> 8;
			code[1] = ebc & 0xff;
			(void) key_WCharacter(code, skipped);
		} else
#endif /*]*/
			(void) key_Character(ebc, (keytype == KT_GE) || ge,
					     False, skipped);
	}
#if defined(X3270_ANSI) /*[*/
	else if (IN_ANSI) {
	    	char mb[16];

		unicode_to_multibyte(ucs4, mb, sizeof(mb));
		net_sends(mb);
	}
#endif /*]*/
	else {
		trace_event("  dropped (not connected)\n");
	}
}

#if defined(X3270_DISPLAY) /*[*/
/*
 * Handle an ordinary character key, given its NULL-terminated multibyte
 * representation.
 */
static void
key_ACharacter(char *mb, enum keytype keytype, enum iaction cause,
	       Boolean *skipped)
{
	ucs4_t ucs4;
	int consumed;
	enum me_fail error;

	reset_idle_timer();

	if (skipped != NULL)
		*skipped = False;

	/* Convert the multibyte string to UCS4. */
	ucs4 = multibyte_to_unicode(mb, strlen(mb), &consumed, &error);
	if (ucs4 == 0) {
		trace_event(" %s -> Key(?)\n", ia_name[(int) cause]);
		trace_event("  dropped (invalid multibyte sequence)\n");
		return;
	}

	key_UCharacter(ucs4, keytype, cause, skipped);
}
#endif /*]*/


/*
 * Simple toggles.
 */
#if defined(X3270_DISPLAY) /*[*/
void
AltCursor_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(AltCursor_action, event, params, num_params);
	reset_idle_timer();
	do_toggle(ALT_CURSOR);
}
#endif /*]*/

void
MonoCase_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(MonoCase_action, event, params, num_params);
	reset_idle_timer();
	do_toggle(MONOCASE);
}

/*
 * Flip the display left-to-right
 */
void
Flip_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Flip_action, event, params, num_params);
	reset_idle_timer();
	screen_flip();
}



/*
 * Tab forward to next field.
 */
void
Tab_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Tab_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "Tab");
			status_reset();
		} else {
			enq_ta(Tab_action, CN, CN);
			return;
		}
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_sendc('\t');
		return;
	}
#endif /*]*/
	cursor_move(next_unprotected(cursor_addr));
}


/*
 * Tab backward to previous field.
 */
void
BackTab_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	register int	baddr, nbaddr;
	int		sbaddr;

	action_debug(BackTab_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "BackTab");
			status_reset();
		} else {
			enq_ta(BackTab_action, CN, CN);
			return;
		}
	}
	if (!IN_3270)
		return;
	baddr = cursor_addr;
	DEC_BA(baddr);
	if (ea_buf[baddr].fa)	/* at bof */
		DEC_BA(baddr);
	sbaddr = baddr;
	while (True) {
		nbaddr = baddr;
		INC_BA(nbaddr);
		if (ea_buf[baddr].fa &&
		    !FA_IS_PROTECTED(ea_buf[baddr].fa) &&
		    !ea_buf[nbaddr].fa)
			break;
		DEC_BA(baddr);
		if (baddr == sbaddr) {
			cursor_move(0);
			return;
		}
	}
	INC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Deferred keyboard unlock.
 */

static void
defer_unlock(void)
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
do_reset(Boolean explicit)
{
	/*
	 * If explicit (from the keyboard) and there is typeahead or
	 * a half-composed key, simply flush it.
	 */
	if (explicit
#if defined(X3270_FT) /*[*/
	    || ft_state != FT_NONE
#endif /*]*/
	    ) {
		Boolean half_reset = False;

		if (flush_ta())
			half_reset = True;
		if (composing != NONE) {
			composing = NONE;
			status_compose(False, 0, KT_STD);
			half_reset = True;
		}
		if (half_reset)
			return;
	}

	/* Always clear insert mode. */
	insert_mode(False);

	/* Otherwise, if not connect, reset is a no-op. */
	if (!CONNECTED)
		return;

	/*
	 * Remove any deferred keyboard unlock.  We will either unlock the
	 * keyboard now, or want to defer further into the future.
	 */
	if (kybdlock & KL_DEFERRED_UNLOCK)
		RemoveTimeOut(unlock_id);

	/*
	 * If explicit (from the keyboard), unlock the keyboard now.
	 * Otherwise (from the host), schedule a deferred keyboard unlock.
	 */
	if (explicit
#if defined(X3270_FT) /*[*/
	    || ft_state != FT_NONE
#endif /*]*/
	    || (!appres.unlock_delay && !sms_in_macro())
	    || (unlock_delay_time != 0 && (time(NULL) - unlock_delay_time) > 1)
	    || !appres.unlock_delay_ms) {
		kybdlock_clr(-1, "do_reset");
	} else if (kybdlock &
  (KL_DEFERRED_UNLOCK | KL_OIA_TWAIT | KL_OIA_LOCKED | KL_AWAITING_FIRST)) {
		kybdlock_clr(~KL_DEFERRED_UNLOCK, "do_reset");
		kybdlock_set(KL_DEFERRED_UNLOCK, "do_reset");
		unlock_id = AddTimeOut(appres.unlock_delay_ms, defer_unlock);
		trace_event("Deferring keyboard unlock %dms\n",
			appres.unlock_delay_ms);
	}

	/* Clean up other modes. */
	status_reset();
	mcursor_normal();
	composing = NONE;
	status_compose(False, 0, KT_STD);
}

void
Reset_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Reset_action, event, params, num_params);
	reset_idle_timer();
	do_reset(True);
}


/*
 * Move to first unprotected field on screen.
 */
void
Home_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Home_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(Home_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		ansi_send_home();
		return;
	}
#endif /*]*/
	if (!formatted) {
		cursor_move(0);
		return;
	}
	cursor_move(next_unprotected(ROWS*COLS-1));
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

void
Left_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Left_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "Left");
			status_reset();
		} else {
			enq_ta(Left_action, CN, CN);
			return;
		}
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		ansi_send_left();
		return;
	}
#endif /*]*/
	if (!flipped)
		do_left();
	else {
		register int	baddr;

		baddr = cursor_addr;
		INC_BA(baddr);
		cursor_move(baddr);
	}
}


/*
 * Delete char key.
 * Returns "True" if succeeds, "False" otherwise.
 */
static Boolean
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
		return False;
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
			return False;
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
			return True;
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
	return True;
}

void
Delete_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Delete_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(Delete_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_sendc('\177');
		return;
	}
#endif /*]*/
	if (!do_delete())
		return;
	if (reverse) {
		int baddr = cursor_addr;

		DEC_BA(baddr);
		if (!ea_buf[baddr].fa)
			cursor_move(baddr);
	}
}


/*
 * 3270-style backspace.
 */
void
BackSpace_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(BackSpace_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(BackSpace_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_send_erase();
		return;
	}
#endif /*]*/
	if (reverse)
		(void) do_delete();
	else if (!flipped)
		do_left();
	else {
		register int	baddr;

		baddr = cursor_addr;
		DEC_BA(baddr);
		cursor_move(baddr);
	}
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

void
Erase_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Erase_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(Erase_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_send_erase();
		return;
	}
#endif /*]*/
	do_erase();
}


/*
 * Cursor right 1 position.
 */
void
Right_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	register int	baddr;
	enum dbcs_state d;

	action_debug(Right_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "Right");
			status_reset();
		} else {
			enq_ta(Right_action, CN, CN);
			return;
		}
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		ansi_send_right();
		return;
	}
#endif /*]*/
	if (!flipped) {
		baddr = cursor_addr;
		INC_BA(baddr);
		d = ctlr_dbcs_state(baddr);
		if (IS_RIGHT(d))
			INC_BA(baddr);
		cursor_move(baddr);
	} else
		do_left();
}


/*
 * Cursor left 2 positions.
 */
void
Left2_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	register int	baddr;
	enum dbcs_state d;

	action_debug(Left2_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "Left2");
			status_reset();
		} else {
			enq_ta(Left2_action, CN, CN);
			return;
		}
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	baddr = cursor_addr;
	DEC_BA(baddr);
	d = ctlr_dbcs_state(baddr);
	if (IS_LEFT(d))
		DEC_BA(baddr);
	DEC_BA(baddr);
	d = ctlr_dbcs_state(baddr);
	if (IS_LEFT(d))
		DEC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Cursor to previous word.
 */
void
PreviousWord_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	register int baddr;
	int baddr0;
	unsigned char  c;
	Boolean prot;

	action_debug(PreviousWord_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(PreviousWord_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	if (!formatted)
		return;

	baddr = cursor_addr;
	prot = FA_IS_PROTECTED(get_field_attribute(baddr));

	/* Skip to before this word, if in one now. */
	if (!prot) {
		c = ea_buf[baddr].cc;
		while (!ea_buf[baddr].fa && c != EBC_space && c != EBC_null) {
			DEC_BA(baddr);
			if (baddr == cursor_addr)
				return;
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
		if (!prot && c != EBC_space && c != EBC_null)
			break;
		DEC_BA(baddr);
	} while (baddr != baddr0);

	if (baddr == baddr0)
		return;

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
}


/*
 * Cursor right 2 positions.
 */
void
Right2_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	register int	baddr;
	enum dbcs_state d;

	action_debug(Right2_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "Right2");
			status_reset();
		} else {
			enq_ta(Right2_action, CN, CN);
			return;
		}
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	baddr = cursor_addr;
	INC_BA(baddr);
	d = ctlr_dbcs_state(baddr);
	if (IS_RIGHT(d))
		INC_BA(baddr);
	INC_BA(baddr);
	d = ctlr_dbcs_state(baddr);
	if (IS_RIGHT(d))
		INC_BA(baddr);
	cursor_move(baddr);
}


/* Find the next unprotected word, or -1 */
static int
nu_word(int baddr)
{
	int baddr0 = baddr;
	unsigned char c;
	Boolean prot;

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
	Boolean in_word = True;

	do {
		c = ea_buf[baddr].cc;
		if (ea_buf[baddr].fa)
			return -1;
		if (in_word) {
			if (c == EBC_space || c == EBC_null)
				in_word = False;
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
void
NextWord_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int	baddr;
	unsigned char c;

	action_debug(NextWord_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(NextWord_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	if (!formatted)
		return;

	/* If not in an unprotected field, go to the next unprotected word. */
	if (ea_buf[cursor_addr].fa ||
	    FA_IS_PROTECTED(get_field_attribute(cursor_addr))) {
		baddr = nu_word(cursor_addr);
		if (baddr != -1)
			cursor_move(baddr);
		return;
	}

	/* If there's another word in this field, go to it. */
	baddr = nt_word(cursor_addr);
	if (baddr != -1) {
		cursor_move(baddr);
		return;
	}

	/* If in a word, go to just after its end. */
	c = ea_buf[cursor_addr].cc;
	if (c != EBC_space && c != EBC_null) {
		baddr = cursor_addr;
		do {
			c = ea_buf[baddr].cc;
			if (c == EBC_space || c == EBC_null) {
				cursor_move(baddr);
				return;
			} else if (ea_buf[baddr].fa) {
				baddr = nu_word(baddr);
				if (baddr != -1)
					cursor_move(baddr);
				return;
			}
			INC_BA(baddr);
		} while (baddr != cursor_addr);
	}
	/* Otherwise, go to the next unprotected word. */
	else {
		baddr = nu_word(cursor_addr);
		if (baddr != -1)
			cursor_move(baddr);
	}
}


/*
 * Cursor up 1 position.
 */
void
Up_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int	baddr;

	action_debug(Up_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "Up");
			status_reset();
		} else {
			enq_ta(Up_action, CN, CN);
			return;
		}
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		ansi_send_up();
		return;
	}
#endif /*]*/
	baddr = cursor_addr - COLS;
	if (baddr < 0)
		baddr = (cursor_addr + (ROWS * COLS)) - COLS;
	cursor_move(baddr);
}


/*
 * Cursor down 1 position.
 */
void
Down_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int	baddr;

	action_debug(Down_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		if (KYBDLOCK_IS_OERR) {
			kybdlock_clr(KL_OERR_MASK, "Down");
			status_reset();
		} else {
			enq_ta(Down_action, CN, CN);
			return;
		}
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		ansi_send_down();
		return;
	}
#endif /*]*/
	baddr = (cursor_addr + COLS) % (COLS * ROWS);
	cursor_move(baddr);
}


/*
 * Cursor to first field on next line or any lines after that.
 */
void
Newline_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int	baddr, faddr;
	register unsigned char	fa;

	action_debug(Newline_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(Newline_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_sendc('\n');
		return;
	}
#endif /*]*/
	baddr = (cursor_addr + COLS) % (COLS * ROWS);	/* down */
	baddr = (baddr / COLS) * COLS;			/* 1st col */
	faddr = find_field_attribute(baddr);
	fa = ea_buf[faddr].fa;
	if (faddr != baddr && !FA_IS_PROTECTED(fa))
		cursor_move(baddr);
	else
		cursor_move(next_unprotected(baddr));
}


/*
 * DUP key
 */
void
Dup_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Dup_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(Dup_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	if (key_Character(EBC_dup, False, False, NULL))
		cursor_move(next_unprotected(cursor_addr));
}


/*
 * FM key
 */
void
FieldMark_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(FieldMark_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(FieldMark_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	(void) key_Character(EBC_fm, False, False, NULL);
}


/*
 * Vanilla AID keys.
 */
void
Enter_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Enter_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock & KL_OIA_MINUS)
		return;
	else if (kybdlock)
		enq_ta(Enter_action, CN, CN);
	else
		key_AID(AID_ENTER);
}


void
SysReq_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(SysReq_action, event, params, num_params);
	reset_idle_timer();
	if (IN_ANSI)
		return;
#if defined(X3270_TN3270E) /*[*/
	if (IN_E) {
		net_abort();
	} else
#endif /*]*/
	{
		if (kybdlock & KL_OIA_MINUS)
			return;
		else if (kybdlock)
			enq_ta(SysReq_action, CN, CN);
		else
			key_AID(AID_SYSREQ);
	}
}


/*
 * Clear AID key
 */
void
Clear_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Clear_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock & KL_OIA_MINUS)
		return;
	if (kybdlock && CONNECTED) {
		enq_ta(Clear_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		ansi_send_clear();
		return;
	}
#endif /*]*/
	buffer_addr = 0;
	ctlr_clear(True);
	cursor_move(0);
	if (CONNECTED)
		key_AID(AID_CLEAR);
}


/*
 * Cursor Select key (light pen simulator).
 */
static void
lightpen_select(int baddr)
{
	int faddr;
	register unsigned char	fa;
	int designator;
#if defined(X3270_DBCS) /*[*/
	int designator2;
#endif /*]*/

	faddr = find_field_attribute(baddr);
	fa = ea_buf[faddr].fa;
	if (!FA_IS_SELECTABLE(fa)) {
		ring_bell();
		return;
	}
	designator = faddr;
	INC_BA(designator);

#if defined(X3270_DBCS) /*[*/
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
#endif /*]*/

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
void
CursorSelect_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(CursorSelect_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(CursorSelect_action, CN, CN);
		return;
	}

#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	lightpen_select(cursor_addr);
}

#if defined(X3270_DISPLAY) /*[*/
/*
 * Cursor Select mouse action (light pen simulator).
 */
void
MouseSelect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(MouseSelect_action, event, params, num_params);
	if (w != *screen)
		return;
	reset_idle_timer();
	if (kybdlock)
		return;
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	lightpen_select(mouse_baddr(w, event));
}
#endif /*]*/


/*
 * Erase End Of Field Key.
 */
void
EraseEOF_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int	baddr;
	register unsigned char	fa;
	enum dbcs_state d;
	enum dbcs_why why = DBCS_FIELD;

	action_debug(EraseEOF_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(EraseEOF_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
		operator_error(KL_OERR_PROTECTED);
		return;
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
		} else
			ea_buf[cursor_addr].cc = EBC_si;
	}
	(void) ctlr_dbcs_postprocess();
}


/*
 * Erase all Input Key.
 */
void
EraseInput_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int	baddr, sbaddr;
	unsigned char	fa;
	Boolean		f;

	action_debug(EraseInput_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(EraseInput_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	if (formatted) {
		/* find first field attribute */
		baddr = 0;
		do {
			if (ea_buf[baddr].fa)
				break;
			INC_BA(baddr);
		} while (baddr != 0);
		sbaddr = baddr;
		f = False;
		do {
			fa = ea_buf[baddr].fa;
			if (!FA_IS_PROTECTED(fa)) {
				mdt_clear(baddr);
				do {
					INC_BA(baddr);
					if (!f) {
						cursor_move(baddr);
						f = True;
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
		if (!f)
			cursor_move(0);
	} else {
		ctlr_clear(True);
		cursor_move(0);
	}
}



/*
 * Delete word key.  Backspaces the cursor until it hits the front of a word,
 * deletes characters until it hits a blank or null, and deletes all of these
 * but the last.
 *
 * Which is to say, does a ^W.
 */
void
DeleteWord_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int baddr;
	register unsigned char	fa;

	action_debug(DeleteWord_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(DeleteWord_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_send_werase();
		return;
	}
#endif /*]*/
	if (!formatted)
		return;

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);

	/* Make sure we're on a modifiable field. */
	if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}

	/* Backspace over any spaces to the left of the cursor. */
	for (;;) {
		baddr = cursor_addr;
		DEC_BA(baddr);
		if (ea_buf[baddr].fa)
			return;
		if (ea_buf[baddr].cc == EBC_null ||
		    ea_buf[baddr].cc == EBC_space)
			do_erase();
		else
			break;
	}

	/* Backspace until the character to the left of the cursor is blank. */
	for (;;) {
		baddr = cursor_addr;
		DEC_BA(baddr);
		if (ea_buf[baddr].fa)
			return;
		if (ea_buf[baddr].cc == EBC_null ||
		    ea_buf[baddr].cc == EBC_space)
			break;
		else
			do_erase();
	}
}



/*
 * Delete field key.  Similar to EraseEOF, but it wipes out the entire field
 * rather than just to the right of the cursor, and it leaves the cursor at
 * the front of the field.
 *
 * Which is to say, does a ^U.
 */
void
DeleteField_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int	baddr;
	register unsigned char	fa;

	action_debug(DeleteField_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(DeleteField_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_send_kill();
		return;
	}
#endif /*]*/
	if (!formatted)
		return;

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(fa) || ea_buf[baddr].fa) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}
	while (!ea_buf[baddr].fa)
		DEC_BA(baddr);
	INC_BA(baddr);
	mdt_set(cursor_addr);
	cursor_move(baddr);
	while (!ea_buf[baddr].fa) {
		ctlr_add(baddr, EBC_null, 0);
		INC_BA(baddr);
	}
}



/*
 * Set insert mode key.
 */
void
Insert_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Insert_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(Insert_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	insert_mode(True);
}


/*
 * Toggle insert mode key.
 */
void
ToggleInsert_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(ToggleInsert_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(ToggleInsert_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	if (insert)
		insert_mode(False);
	else
		insert_mode(True);
}


/*
 * Toggle reverse mode key.
 */
void
ToggleReverse_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(ToggleReverse_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(ToggleReverse_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	reverse_mode(!reverse);
}


/*
 * Move the cursor to the first blank after the last nonblank in the
 * field, or if the field is full, to the last character in the field.
 */
void
FieldEnd_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	int	baddr, faddr;
	unsigned char	fa, c;
	int	last_nonblank = -1;

	action_debug(FieldEnd_action, event, params, num_params);
	reset_idle_timer();
	if (kybdlock) {
		enq_ta(FieldEnd_action, CN, CN);
		return;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return;
#endif /*]*/
	if (!formatted)
		return;
	baddr = cursor_addr;
	faddr = find_field_attribute(baddr);
	fa = ea_buf[faddr].fa;
	if (faddr == baddr || FA_IS_PROTECTED(fa))
		return;

	baddr = faddr;
	while (True) {
		INC_BA(baddr);
		c = ea_buf[baddr].cc;
		if (ea_buf[baddr].fa)
			break;
		if (c != EBC_null && c != EBC_space)
			last_nonblank = baddr;
	}

	if (last_nonblank == -1) {
		baddr = faddr;
		INC_BA(baddr);
	} else {
		baddr = last_nonblank;
		INC_BA(baddr);
		if (ea_buf[baddr].fa)
			baddr = last_nonblank;
	}
	cursor_move(baddr);
}

/*
 * MoveCursor action.  Depending on arguments, this is either a move to the
 * mouse cursor position, or to an absolute location.
 */
void
MoveCursor_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	register int baddr;
	int row, col;

	action_debug(MoveCursor_action, event, params, num_params);

	reset_idle_timer();
	if (kybdlock) {
		if (*num_params == 2)
			enq_ta(MoveCursor_action, params[0], params[1]);
		return;
	}

	switch (*num_params) {
#if defined(X3270_DISPLAY) /*[*/
	    case 0:		/* mouse click, presumably */
		if (w != *screen)
			return;
		cursor_move(mouse_baddr(w, event));
		break;
#endif /*]*/
	    case 2:		/* probably a macro call */
		row = atoi(params[0]);
		col = atoi(params[1]);
		if (!IN_3270) {
			row--;
			col--;
		}
		if (row < 0)
			row = 0;
		if (col < 0)
			col = 0;
		baddr = ((row * COLS) + col) % (ROWS * COLS);
		cursor_move(baddr);
		break;
	    default:		/* couln't say */
		popup_an_error("%s requires 0 or 2 arguments",
		    action_name(MoveCursor_action));
		cancel_if_idle_command();
		break;
	}
}


#if defined(X3270_DBCS) && defined(X3270_DISPLAY) /*[*/
/*
 * Run a KeyPress through XIM.
 * Returns True if there is further processing to do, False otherwise.
 */
static Boolean
xim_lookup(XKeyEvent *event)
{
	static char *buf = NULL;
	static int buf_len = 0, rlen;
	KeySym k;
	Status status;
	extern XIC ic;
	int i;
	Boolean rv = False;
#define BASE_BUFSIZE 50

	if (ic == NULL)
		return True;

	if (buf == NULL) {
		buf_len = BASE_BUFSIZE;
		buf = Malloc(buf_len);
	}

	for (;;) {
		memset(buf, '\0', buf_len);
		rlen = XmbLookupString(ic, event, buf, buf_len - 1, &k,
					&status);
		if (status != XBufferOverflow)
			break;
		buf_len += BASE_BUFSIZE;
		buf = Realloc(buf, buf_len);
	}

	switch (status) {
	case XLookupNone:
		rv = False;
		break;
	case XLookupKeySym:
		rv = True;
		break;
	case XLookupChars:
		trace_event("%d XIM char%s:", rlen, (rlen != 1)? "s": "");
		for (i = 0; i < rlen; i++) {
			trace_event(" %02x", buf[i] & 0xff);
		}
		trace_event("\n");
		buf[rlen] = '\0';
		key_ACharacter(buf, KT_STD, ia_cause, NULL);
		rv = False;
		break;
	case XLookupBoth:
		rv = True;
		break;
	}
	return rv;
}
#endif /*]*/


/*
 * Key action.
 */
void
Key_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	Cardinal i;
	KeySym k;
	enum keytype keytype;
	ucs4_t ucs4;

	action_debug(Key_action, event, params, num_params);
	reset_idle_timer();

	for (i = 0; i < *num_params; i++) {
		char *s = params[i];

		k = MyStringToKeysym(s, &keytype, &ucs4);
		if (k == NoSymbol && !ucs4) {
			popup_an_error("%s: Nonexistent or invalid KeySym: %s",
			    action_name(Key_action), s);
			cancel_if_idle_command();
			continue;
		}
		if (k & ~0xff) {
		    	/*
			 * Can't pass symbolic KeySyms that aren't in the
			 * range 0x01..0xff.
			 */
			popup_an_error("%s: Invalid KeySym: %s",
			    action_name(Key_action), s);
			cancel_if_idle_command();
			continue;
		}
		if (k != NoSymbol)
			key_UCharacter(k, keytype, IA_KEY, NULL);
		else
			key_UCharacter(ucs4, keytype, IA_KEY, NULL);
	}
}

/*
 * String action.
 */
void
String_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	Cardinal i;
	int len = 0;
	char *s;

	action_debug(String_action, event, params, num_params);
	reset_idle_timer();

	/* Determine the total length of the strings. */
	for (i = 0; i < *num_params; i++)
		len += strlen(params[i]);
	if (!len)
		return;

	/* Allocate a block of memory and copy them in. */
	s = Malloc(len + 1);
	s[0] = '\0';
	for (i = 0; i < *num_params; i++) {
	    	strcat(s, params[i]);
	}

	/* Set a pending string. */
	ps_set(s, False);
	Free(s);
}

/*
 * HexString action.
 */
void
HexString_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	Cardinal i;
	int len = 0;
	char *s;
	char *t;

	action_debug(HexString_action, event, params, num_params);
	reset_idle_timer();

	/* Determine the total length of the strings. */
	for (i = 0; i < *num_params; i++) {
		t = params[i];
		if (!strncmp(t, "0x", 2) || !strncmp(t, "0X", 2))
			t += 2;
		len += strlen(t);
	}
	if (!len)
		return;

	/* Allocate a block of memory and copy them in. */
	s = Malloc(len + 1);
	*s = '\0';
	for (i = 0; i < *num_params; i++) {
		t = params[i];
		if (!strncmp(t, "0x", 2) || !strncmp(t, "0X", 2))
			t += 2;
		(void) strcat(s, t);
	}

	/* Set a pending string. */
	ps_set(s, True);
}

/*
 * Dual-mode action for the "asciicircum" ("^") key:
 *  If in ANSI mode, pass through untranslated.
 *  If in 3270 mode, translate to "notsign".
 * This action is obsoleted by the use of 3270-mode and NVT-mode keymaps, but
 * is still defined here for backwards compatibility with old keymaps.
 */
void
CircumNot_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(CircumNot_action, event, params, num_params);
	reset_idle_timer();

	if (IN_3270 && composing == NONE)
		key_UCharacter(0xac, KT_STD, IA_KEY, NULL);
	else
		key_UCharacter('^', KT_STD, IA_KEY, NULL);
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
		char nn[3];

		(void) sprintf(nn, "%d", n);
		enq_ta(PA_action, nn, CN);
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
		char nn[3];

		(void) sprintf(nn, "%d", n);
		enq_ta(PF_action, nn, CN);
		return;
	}
	key_AID(pf_xlate[n-1]);
}

/*
 * Set or clear the keyboard scroll lock.
 */
void
kybd_scroll_lock(Boolean lock)
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
 * Returns a Boolean indicating success.
 */
static Boolean
remargin(int lmargin)
{
	Boolean ever = False;
	int baddr, b0 = 0;
	int faddr;
	unsigned char fa;

	baddr = cursor_addr;
	while (BA_TO_COL(baddr) < lmargin) {
		baddr = ROWCOL_TO_BA(BA_TO_ROW(baddr), lmargin);
		if (!ever) {
			b0 = baddr;
			ever = True;
		}
		faddr = find_field_attribute(baddr);
		fa = ea_buf[faddr].fa;
		if (faddr == baddr || FA_IS_PROTECTED(fa)) {
			baddr = next_unprotected(baddr);
			if (baddr <= b0)
				return False;
		}
	}

	cursor_move(baddr);
	return True;
}

/*
 * Pretend that a sequence of keys was entered at the keyboard.
 *
 * "Pasting" means that the sequence came from the X clipboard.  Returns are
 * ignored; newlines mean "move to beginning of next line"; tabs and formfeeds
 * become spaces.  Backslashes are not special, but ASCII ESC characters are
 * used to signify 3270 Graphic Escapes.
 *
 * "Not pasting" means that the sequence is a login string specified in the
 * hosts file, or a parameter to the String action.  Returns are "move to
 * beginning of next line"; newlines mean "Enter AID" and the termination of
 * processing the string.  Backslashes are processed as in C.
 *
 * Returns the number of unprocessed characters.
 */
int
emulate_uinput(ucs4_t *ws, int xlen, Boolean pasting)
{
	enum {
	    BASE, BACKSLASH, BACKX, BACKE, BACKP, BACKPA, BACKPF, OCTAL, HEX,
	    EBC, XGE
	} state = BASE;
	int literal = 0;
	int nc = 0;
	enum iaction ia = pasting ? IA_PASTE : IA_STRING;
	int orig_addr = cursor_addr;
	int orig_col = BA_TO_COL(cursor_addr);
	Boolean skipped = False;
	ucs4_t c;

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
			trace_event("  keyboard locked, string dropped\n");
			return 0;
		}

		if (pasting && IN_3270) {

			/* Check for cursor wrap to top of screen. */
			if (cursor_addr < orig_addr)
				return xlen-1;		/* wrapped */

			/* Jump cursor over left margin. */
			if (toggled(MARGINED_PASTE) &&
			    BA_TO_COL(cursor_addr) < orig_col) {
				if (!remargin(orig_col))
					return xlen-1;
				skipped = True;
			}
		}

		c = *ws;

		switch (state) {
		    case BASE:
			switch (c) {
			    case '\b':
				action_internal(Left_action, ia, CN, CN);
				skipped = False;
				break;
			    case '\f':
				if (pasting) {
					key_UCharacter(' ', KT_STD, ia,
						&skipped);
				} else {
					action_internal(Clear_action, ia, CN,
							CN);
					skipped = False;
					if (IN_3270)
						return xlen-1;
				}
				break;
			    case '\n':
				if (pasting) {
					if (!skipped)
						action_internal(Newline_action,
								ia, CN, CN);
					skipped = False;
				} else {
					action_internal(Enter_action, ia, CN,
							CN);
					skipped = False;
					if (IN_3270)
						return xlen-1;
				}
				break;
			    case '\r':	/* ignored */
				break;
			    case '\t':
				action_internal(Tab_action, ia, CN, CN);
				skipped = False;
				break;
			    case '\\':	/* backslashes are NOT special when
					   pasting */
				if (!pasting)
					state = BACKSLASH;
				else
					key_UCharacter((unsigned char)c,
						KT_STD, ia, &skipped);
				break;
			    case '\033': /* ESC is special only when pasting */
				if (pasting)
					state = XGE;
				break;
			    case '[':	/* APL left bracket */
				if (pasting && appres.apl_mode)
					key_UCharacter(XK_Yacute, KT_GE, ia,
						&skipped);
				else
					key_UCharacter((unsigned char)c,
						KT_STD, ia, &skipped);
				break;
			    case ']':	/* APL right bracket */
				if (pasting && appres.apl_mode)
					key_UCharacter(XK_diaeresis, KT_GE, ia,
						&skipped);
				else
					key_UCharacter((unsigned char)c,
						KT_STD, ia,
						&skipped);
				break;
			    case UPRIV_fm: /* private-use FM */
				if (pasting)
					key_Character(EBC_fm, False, True,
						&skipped);
				break;
			    case UPRIV_dup: /* private-use DUP */
				if (pasting)
					key_Character(EBC_dup, False, True,
						&skipped);
				break;
			    case UPRIV_eo: /* private-use EO */
				if (pasting)
					key_Character(EBC_eo, False, True,
						&skipped);
				break;
			    case UPRIV_sub: /* private-use SUB */
				if (pasting)
					key_Character(EBC_sub, False, True,
						&skipped);
				break;
			default:
				if (pasting &&
					(c >= UPRIV_GE_00 &&
					 c <= UPRIV_GE_ff))
					key_Character(c - UPRIV_GE_00, KT_GE,
						ia, &skipped);
				else
					key_UCharacter(c, KT_STD, ia,
						&skipped);
				break;
			}
			break;
		    case BACKSLASH:	/* last character was a backslash */
			switch (c) {
			    case 'a':
				popup_an_error("%s: Bell not supported",
				    action_name(String_action));
				cancel_if_idle_command();
				state = BASE;
				break;
			    case 'b':
				action_internal(Left_action, ia, CN, CN);
				skipped = False;
				state = BASE;
				break;
			    case 'f':
				action_internal(Clear_action, ia, CN, CN);
				skipped = False;
				state = BASE;
				if (IN_3270)
					return xlen-1;
				else
					break;
			    case 'n':
				action_internal(Enter_action, ia, CN, CN);
				skipped = False;
				state = BASE;
				if (IN_3270)
					return xlen-1;
				else
					break;
			    case 'p':
				state = BACKP;
				break;
			    case 'r':
				action_internal(Newline_action, ia, CN, CN);
				skipped = False;
				state = BASE;
				break;
			    case 't':
				action_internal(Tab_action, ia, CN, CN);
				skipped = False;
				state = BASE;
				break;
			    case 'T':
				action_internal(BackTab_action, ia, CN, CN);
				skipped = False;
				state = BASE;
				break;
			    case 'v':
				popup_an_error("%s: Vertical tab not supported",
				    action_name(String_action));
				cancel_if_idle_command();
				state = BASE;
				break;
			    case 'x':
				state = BACKX;
				break;
			    case 'e':
				state = BACKE;
				break;
			    case '\\':
				key_UCharacter((unsigned char) c, KT_STD, ia,
						&skipped);
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
				popup_an_error("%s: Unknown character "
						"after \\p",
				    action_name(String_action));
				cancel_if_idle_command();
				state = BASE;
				break;
			}
			break;
		    case BACKPF: /* last three characters were "\pf" */
			if (nc < 2 && isdigit(c)) {
				literal = (literal * 10) + (c - '0');
				nc++;
			} else if (!nc) {
				popup_an_error("%s: Unknown character "
						"after \\pf",
				    action_name(String_action));
				cancel_if_idle_command();
				state = BASE;
			} else {
				do_pf(literal);
				skipped = False;
				if (IN_3270)
					return xlen-1;
				state = BASE;
				continue;
			}
			break;
		    case BACKPA: /* last three characters were "\pa" */
			if (nc < 1 && isdigit(c)) {
				literal = (literal * 10) + (c - '0');
				nc++;
			} else if (!nc) {
				popup_an_error("%s: Unknown character "
						"after \\pa",
				    action_name(String_action));
				cancel_if_idle_command();
				state = BASE;
			} else {
				do_pa(literal);
				skipped = False;
				if (IN_3270)
					return xlen-1;
				state = BASE;
				continue;
			}
			break;
		    case BACKX:	/* last two characters were "\x" */
			if (isxdigit(c)) {
				state = HEX;
				literal = 0;
				nc = 0;
				continue;
			} else {
				popup_an_error("%s: Missing hex digits after \\x",
				    action_name(String_action));
				cancel_if_idle_command();
				state = BASE;
				continue;
			}
		    case BACKE:	/* last two characters were "\x" */
			if (isxdigit(c)) {
				state = EBC;
				literal = 0;
				nc = 0;
				continue;
			} else {
				popup_an_error("%s: Missing hex digits after \\e",
				    action_name(String_action));
				cancel_if_idle_command();
				state = BASE;
				continue;
			}
		    case OCTAL:	/* have seen \ and one or more octal digits */
			if (nc < 3 && isdigit(c) && c < '8') {
				literal = (literal * 8) + FROM_HEX(c);
				nc++;
				break;
			} else {
				key_UCharacter((unsigned char) literal, KT_STD,
				    ia, &skipped);
				state = BASE;
				continue;
			}
		    case HEX:	/* have seen \x and one or more hex digits */
			if (nc < 2 && isxdigit(c)) {
				literal = (literal * 16) + FROM_HEX(c);
				nc++;
				break;
			} else {
				key_UCharacter((unsigned char) literal, KT_STD,
				    ia, &skipped);
				state = BASE;
				continue;
			}
		    case EBC:	/* have seen \e and one or more hex digits */
			if (nc < 2 && isxdigit(c)) {
				literal = (literal * 16) + FROM_HEX(c);
				nc++;
				break;
			} else {
			    	trace_event(" %s -> Key(X'%02X')\n",
					ia_name[(int) ia], literal);
				key_Character((unsigned char) literal, False,
					True, &skipped);
				state = BASE;
				continue;
			}
		    case XGE:	/* have seen ESC */
			switch (c) {
			    case ';':	/* FM */
				key_Character(EBC_fm, False, True, &skipped);
				break;
			    case '*':	/* DUP */
				key_Character(EBC_dup, False, True, &skipped);
				break;
			    default:
				key_UCharacter((unsigned char) c, KT_GE, ia,
						&skipped);
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
		if (toggled(MARGINED_PASTE) &&
		    BA_TO_COL(cursor_addr) < orig_col) {
			(void) remargin(orig_col);
		}
		break;
	    case OCTAL:
	    case HEX:
		key_UCharacter((unsigned char) literal, KT_STD, ia, &skipped);
		state = BASE;
		if (toggled(MARGINED_PASTE) &&
		    BA_TO_COL(cursor_addr) < orig_col) {
			(void) remargin(orig_col);
		}
		break;
	    case EBC:
		/* XXX: line below added after 3.3.7p7 */
		trace_event(" %s -> Key(X'%02X')\n", ia_name[(int) ia],
			literal);
		key_Character((unsigned char) literal, False, True, &skipped);
		state = BASE;
		if (toggled(MARGINED_PASTE) &&
		    BA_TO_COL(cursor_addr) < orig_col) {
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
		popup_an_error("%s: Missing data after \\",
		    action_name(String_action));
		cancel_if_idle_command();
		break;
	}

	return xlen;
}

/* Multibyte version of emulate_uinput. */
int
emulate_input(char *s, int len, Boolean pasting)
{
	static ucs4_t *w_ibuf = NULL;
	static size_t w_ibuf_len = 0;
	int xlen;

	/* Convert from a multi-byte string to a Unicode string. */
	if ((size_t)(len + 1) > w_ibuf_len) {
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
 * per byte.  If connected in ANSI mode, these are treated as ASCII
 * characters; if in 3270 mode, they are considered EBCDIC.
 *
 * Graphic Escapes are handled as \E.
 */
void
hex_input(char *s)
{
	char *t;
	Boolean escaped;
#if defined(X3270_ANSI) /*[*/
	unsigned char *xbuf = (unsigned char *)NULL;
	unsigned char *tbuf = (unsigned char *)NULL;
	int nbytes = 0;
#endif /*]*/

	/* Validate the string. */
	if (strlen(s) % 2) {
		popup_an_error("%s: Odd number of characters in specification",
		    action_name(HexString_action));
		cancel_if_idle_command();
		return;
	}
	t = s;
	escaped = False;
	while (*t) {
		if (isxdigit(*t) && isxdigit(*(t + 1))) {
			escaped = False;
#if defined(X3270_ANSI) /*[*/
			nbytes++;
#endif /*]*/
		} else if (!strncmp(t, "\\E", 2) || !strncmp(t, "\\e", 2)) {
			if (escaped) {
				popup_an_error("%s: Double \\E",
				    action_name(HexString_action));
				cancel_if_idle_command();
				return;
			}
			if (!IN_3270) {
				popup_an_error("%s: \\E in ANSI mode",
				    action_name(HexString_action));
				cancel_if_idle_command();
				return;
			}
			escaped = True;
		} else {
			popup_an_error("%s: Illegal character in specification",
			    action_name(HexString_action));
			cancel_if_idle_command();
			return;
		}
		t += 2;
	}
	if (escaped) {
		popup_an_error("%s: Nothing follows \\E",
		    action_name(HexString_action));
		cancel_if_idle_command();
		return;
	}

#if defined(X3270_ANSI) /*[*/
	/* Allocate a temporary buffer. */
	if (!IN_3270 && nbytes)
		tbuf = xbuf = (unsigned char *)Malloc(nbytes);
#endif /*]*/

	/* Pump it in. */
	t = s;
	escaped = False;
	while (*t) {
		if (isxdigit(*t) && isxdigit(*(t + 1))) {
			unsigned c;

			c = (FROM_HEX(*t) * 16) + FROM_HEX(*(t + 1));
			if (IN_3270)
				key_Character(c, escaped, True, NULL);
#if defined(X3270_ANSI) /*[*/
			else
				*tbuf++ = (unsigned char)c;
#endif /*]*/
			escaped = False;
		} else if (!strncmp(t, "\\E", 2) || !strncmp(t, "\\e", 2)) {
			escaped = True;
		}
		t += 2;
	}
#if defined(X3270_ANSI) /*[*/
	if (!IN_3270 && nbytes) {
		net_hexansi_out(xbuf, nbytes);
		Free(xbuf);
	}
#endif /*]*/
}
 
void
ignore_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(ignore_action, event, params, num_params);
	reset_idle_timer();
}

#if defined(X3270_FT) /*[*/
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
	 * No point in trying if the screen isn't formatted, the keyboard
	 * is locked, or we aren't in 3270 mode.
	 */
	if (!formatted || kybdlock || !IN_3270)
		return 0;

	fa = get_field_attribute(cursor_addr);
	if (ea_buf[cursor_addr].fa || FA_IS_PROTECTED(fa)) {
		/*
		 * The cursor is not in an unprotected field.  Find the
		 * next one.
		 */
		baddr = next_unprotected(cursor_addr);

		/* If there isn't any, give up. */
		if (!baddr)
			return 0;

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
#endif /*]*/

/*
 * Translate a keysym name to a keysym, including APL and extended
 * characters.
 */
static KeySym
MyStringToKeysym(char *s, enum keytype *keytypep, ucs4_t *ucs4)
{
	KeySym k;
	int consumed;
	enum me_fail error;

	/* No UCS-4 yet. */
	*ucs4 = 0L;

#if defined(X3270_APL) /*[*/
	/* Look for my contrived APL symbols. */
	if (!strncmp(s, "apl_", 4)) {
		int is_ge;

		k = APLStringToKeysym(s, &is_ge);
		if (is_ge)
			*keytypep = KT_GE;
		else
			*keytypep = KT_STD;
		return k;
	} else
#endif /*]*/
	{
		/* Look for a standard X11 keysym. */
		k = StringToKeysym(s);
		*keytypep = KT_STD;
		if (k != NoSymbol)
		    	return k;
	}

	/* Look for "euro". */
	if (!strcasecmp(s, "euro")) {
	    	*ucs4 = 0x20ac;
		return NoSymbol;
	}

	/* Look for U+nnnn of 0xXXXX. */
	if (!strncasecmp(s, "U+", 2) || !strncasecmp(s, "0x", 2)) {
	    	*ucs4 = strtoul(s + 2, NULL, 16);
		return NoSymbol;
	}

	/* Look for a valid local multibyte character. */
	*ucs4 = multibyte_to_unicode(s, strlen(s), &consumed, &error);
	if ((size_t)consumed != strlen(s))
	    	*ucs4 = 0;
	return NoSymbol;
}

#if defined(X3270_DISPLAY) /*[*/
/*
 * X-dependent code starts here.
 */

/*
 * Translate a keymap (from an XQueryKeymap or a KeymapNotify event) into
 * a bitmap of Shift, Meta or Alt keys pressed.
 */
#define key_is_down(kc, bitmap) (kc && ((bitmap)[(kc)/8] & (1<<((kc)%8))))
int
state_from_keymap(char keymap[32])
{
	static Boolean	initted = False;
	static KeyCode	kc_Shift_L, kc_Shift_R;
	static KeyCode	kc_Meta_L, kc_Meta_R;
	static KeyCode	kc_Alt_L, kc_Alt_R;
	int	pseudo_state = 0;

	if (!initted) {
		kc_Shift_L = XKeysymToKeycode(display, XK_Shift_L);
		kc_Shift_R = XKeysymToKeycode(display, XK_Shift_R);
		kc_Meta_L  = XKeysymToKeycode(display, XK_Meta_L);
		kc_Meta_R  = XKeysymToKeycode(display, XK_Meta_R);
		kc_Alt_L   = XKeysymToKeycode(display, XK_Alt_L);
		kc_Alt_R   = XKeysymToKeycode(display, XK_Alt_R);
		initted = True;
	}
	if (key_is_down(kc_Shift_L, keymap) ||
	    key_is_down(kc_Shift_R, keymap))
		pseudo_state |= ShiftKeyDown;
	if (key_is_down(kc_Meta_L, keymap) ||
	    key_is_down(kc_Meta_R, keymap))
		pseudo_state |= MetaKeyDown;
	if (key_is_down(kc_Alt_L, keymap) ||
	    key_is_down(kc_Alt_R, keymap))
		pseudo_state |= AltKeyDown;
	return pseudo_state;
}
#undef key_is_down

/*
 * Process shift keyboard events.  The code has to look for the raw Shift keys,
 * rather than using the handy "state" field in the event structure.  This is
 * because the event state is the state _before_ the key was pressed or
 * released.  This isn't enough information to distinguish between "left
 * shift released" and "left shift released, right shift still held down"
 * events, for example.
 *
 * This function is also called as part of Focus event processing.
 */
void
PA_Shift_action(Widget w _is_unused, XEvent *event _is_unused, String *params _is_unused,
    Cardinal *num_params _is_unused)
{
	char	keys[32];

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_Shift_action, event, params, num_params);
#endif /*]*/
	XQueryKeymap(display, keys);
	shift_event(state_from_keymap(keys));
}
#endif /*]*/

#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
static Boolean
build_composites(void)
{
	char *c, *c0, *c1;
	char *ln;
	char ksname[3][64];
	char junk[2];
	KeySym k[3];
	enum keytype a[3];
	int i;
	struct composite *cp;

	if (appres.compose_map == CN) {
		popup_an_error("%s: No %s defined", action_name(Compose_action),
		    ResComposeMap);
		return False;
	}
	c0 = get_fresource("%s.%s", ResComposeMap, appres.compose_map);
	if (c0 == CN) {
		popup_an_error("%s: Cannot find %s \"%s\"",
		    action_name(Compose_action), ResComposeMap,
		    appres.compose_map);
		return False;
	}
	c1 = c = NewString(c0);	/* will be modified by strtok */
	while ((ln = strtok(c, "\n"))) {
		Boolean okay = True;

		c = NULL;
		if (sscanf(ln, " %63[^+ \t] + %63[^= \t] =%63s%1s",
		    ksname[0], ksname[1], ksname[2], junk) != 3) {
			popup_an_error("%s: Invalid syntax: %s",
			    action_name(Compose_action), ln);
			continue;
		}
		for (i = 0; i < 3; i++) {
		    	ucs4_t ucs4;

			k[i] = MyStringToKeysym(ksname[i], &a[i], &ucs4);
			if (k[i] == NoSymbol) {
				/* For now, ignore UCS4.  XXX: Fix this. */
				popup_an_error("%s: Invalid KeySym: \"%s\"",
				    action_name(Compose_action), ksname[i]);
				okay = False;
				break;
			}
		}
		if (!okay)
			continue;
		composites = (struct composite *) Realloc((char *)composites,
		    (n_composites + 1) * sizeof(struct composite));
		cp = composites + n_composites;
		cp->k1.keysym = k[0];
		cp->k1.keytype = a[0];
		cp->k2.keysym = k[1];
		cp->k2.keytype = a[1];
		cp->translation.keysym = k[2];
		cp->translation.keytype = a[2];
		n_composites++;
	}
	Free(c1);
	return True;
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
void
Compose_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Compose_action, event, params, num_params);
	reset_idle_timer();

	if (!composites && !build_composites())
		return;

	if (composing == NONE) {
		composing = COMPOSE;
		status_compose(True, 0, KT_STD);
	}
}
#endif /*]*/

#if defined(X3270_DISPLAY) /*[*/

/*
 * Called by the toolkit for any key without special actions.
 */
void
Default_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	XKeyEvent	*kevent = (XKeyEvent *)event;
	char		buf[32];
	KeySym		ks;
	int		ll;

	action_debug(Default_action, event, params, num_params);
	switch (event->type) {
	    case KeyPress:
#if defined(X3270_DBCS) /*[*/
		if (!xim_lookup((XKeyEvent *)event))
			return;
#endif /*]*/
		ll = XLookupString(kevent, buf, 32, &ks, (XComposeStatus *) 0);
		buf[ll] = '\0';
		if (ll > 1) {
			key_ACharacter(buf, KT_STD, IA_DEFAULT, NULL);
			return;
		}
		if (ll == 1) {
			/* Remap certain control characters. */
			if (!IN_ANSI) switch (buf[0]) {
			    case '\t':
				action_internal(Tab_action, IA_DEFAULT, CN, CN);
				break;
			   case '\177':
				action_internal(Delete_action, IA_DEFAULT, CN,
				    CN);
				break;
			    case '\b':
				action_internal(Erase_action, IA_DEFAULT,
				    CN, CN);
				break;
			    case '\r':
				action_internal(Enter_action, IA_DEFAULT, CN,
				    CN);
				break;
			    case '\n':
				action_internal(Newline_action, IA_DEFAULT, CN,
				    CN);
				break;
			    default:
				key_ACharacter(buf, KT_STD, IA_DEFAULT, NULL);
				break;
			} else {
				key_ACharacter(buf, KT_STD, IA_DEFAULT, NULL);
			}
			return;
		}

		/* Pick some other reasonable defaults. */
		switch (ks) {
		    case XK_Up:
			action_internal(Up_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Down:
			action_internal(Down_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Left:
			action_internal(Left_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Right:
			action_internal(Right_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Insert:
#if defined(XK_KP_Insert) /*[*/
		    case XK_KP_Insert:
#endif /*]*/
			action_internal(Insert_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Delete:
			action_internal(Delete_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Home:
			action_internal(Home_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Tab:
			action_internal(Tab_action, IA_DEFAULT, CN, CN);
			break;
#if defined(XK_ISO_Left_Tab) /*[*/
		    case XK_ISO_Left_Tab:
			action_internal(BackTab_action, IA_DEFAULT, CN, CN);
			break;
#endif /*]*/
		    case XK_Clear:
			action_internal(Clear_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_Sys_Req:
			action_internal(SysReq_action, IA_DEFAULT, CN, CN);
			break;
#if defined(XK_EuroSign) /*[*/
		    case XK_EuroSign:
			action_internal(Key_action, IA_DEFAULT, "currency",
				CN);
			break;
#endif /*]*/

#if defined(XK_3270_Duplicate) /*[*/
		    /* Funky 3270 keysyms. */
		    case XK_3270_Duplicate:
			action_internal(Dup_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_FieldMark:
			action_internal(FieldMark_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_Right2:
			action_internal(Right2_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_Left2:
			action_internal(Left2_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_BackTab:
			action_internal(BackTab_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_EraseEOF:
			action_internal(EraseEOF_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_EraseInput:
			action_internal(EraseInput_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_Reset:
			action_internal(Reset_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_PA1:
			action_internal(PA_action, IA_DEFAULT, "1", CN);
			break;
		    case XK_3270_PA2:
			action_internal(PA_action, IA_DEFAULT, "2", CN);
			break;
		    case XK_3270_PA3:
			action_internal(PA_action, IA_DEFAULT, "3", CN);
			break;
		    case XK_3270_Attn:
			action_internal(Attn_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_AltCursor:
			action_internal(AltCursor_action, IA_DEFAULT, CN, CN);
			break;
		    case XK_3270_CursorSelect:
			action_internal(CursorSelect_action, IA_DEFAULT, CN,
			    CN);
			break;
		    case XK_3270_Enter:
			action_internal(Enter_action, IA_DEFAULT, CN, CN);
			break;
#endif /*]*/

#if defined(X3270_APL) /*[*/
		    /* Funky APL keysyms. */
		    case XK_downcaret:
			action_internal(Key_action, IA_DEFAULT, "apl_downcaret",
			    CN);
			break;
		    case XK_upcaret:
			action_internal(Key_action, IA_DEFAULT, "apl_upcaret",
			    CN);
			break;
		    case XK_overbar:
			action_internal(Key_action, IA_DEFAULT, "apl_overbar",
			    CN);
			break;
		    case XK_downtack:
			action_internal(Key_action, IA_DEFAULT, "apl_downtack",
			    CN);
			break;
		    case XK_upshoe:
			action_internal(Key_action, IA_DEFAULT, "apl_upshoe",
			    CN);
			break;
		    case XK_downstile:
			action_internal(Key_action, IA_DEFAULT, "apl_downstile",
			    CN);
			break;
		    case XK_underbar:
			action_internal(Key_action, IA_DEFAULT, "apl_underbar",
			    CN);
			break;
		    case XK_jot:
			action_internal(Key_action, IA_DEFAULT, "apl_jot", CN);
			break;
		    case XK_quad:
			action_internal(Key_action, IA_DEFAULT, "apl_quad", CN);
			break;
		    case XK_uptack:
			action_internal(Key_action, IA_DEFAULT, "apl_uptack",
			    CN);
			break;
		    case XK_circle:
			action_internal(Key_action, IA_DEFAULT, "apl_circle",
			    CN);
			break;
		    case XK_upstile:
			action_internal(Key_action, IA_DEFAULT, "apl_upstile",
			    CN);
			break;
		    case XK_downshoe:
			action_internal(Key_action, IA_DEFAULT, "apl_downshoe",
			    CN);
			break;
		    case XK_rightshoe:
			action_internal(Key_action, IA_DEFAULT, "apl_rightshoe",
			    CN);
			break;
		    case XK_leftshoe:
			action_internal(Key_action, IA_DEFAULT, "apl_leftshoe",
			    CN);
			break;
		    case XK_lefttack:
			action_internal(Key_action, IA_DEFAULT, "apl_lefttack",
			    CN);
			break;
		    case XK_righttack:
			action_internal(Key_action, IA_DEFAULT, "apl_righttack",
			    CN);
			break;
#endif /*]*/

		    default:
			if (ks >= XK_F1 && ks <= XK_F24) {
				(void) sprintf(buf, "%ld", ks - XK_F1 + 1);
				action_internal(PF_action, IA_DEFAULT, buf, CN);
			} else {
				ucs4_t ucs4;

			    	ucs4 = keysym2ucs(ks);
				if (ucs4 != (ucs4_t)-1) {
				    	key_UCharacter(ucs4, KT_STD, IA_KEY,
						NULL);
				} else {
					trace_event(
					    " %s: dropped (unknown keysym)\n",
					    action_name(Default_action));
				}
			}
			break;
		}
		break;

	    case ButtonPress:
	    case ButtonRelease:
		trace_event(" %s: dropped (no action configured)\n",
		    action_name(Default_action));
		break;
	    default:
		trace_event(" %s: dropped (unknown event type)\n",
		    action_name(Default_action));
		break;
	}
}

/*
 * Set or clear a temporary keymap.
 *
 *   TemporaryKeymap(x)		toggle keymap "x" (add "x" to the keymap, or if
 *				"x" was already added, remove it)
 *   TemporaryKeymap()		removes the previous keymap, if any
 *   TemporaryKeymap(None)	removes the previous keymap, if any
 */
void
TemporaryKeymap_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(TemporaryKeymap_action, event, params, num_params);
	reset_idle_timer();

	if (check_usage(TemporaryKeymap_action, *num_params, 0, 1) < 0)
		return;

	if (*num_params == 0 || !strcmp(params[0], "None")) {
		(void) temporary_keymap(CN);
		return;
	}

	if (temporary_keymap(params[0]) < 0) {
		popup_an_error("%s: Can't find %s %s",
		    action_name(TemporaryKeymap_action), ResKeymap, params[0]);
		cancel_if_idle_command();
	}
}

#endif /*]*/
