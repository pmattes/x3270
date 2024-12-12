/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	status.c
 *		This module handles the 3270 status line.
 */

#include "globals.h"
#include "xglobals.h"

#include <assert.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include "3270ds.h"
#include "appres.h"
#include "cg.h"

#include "actions.h"
#include "kybd.h"
#include "host.h"
#include "screen.h"
#include "status.h"
#include "tables.h"
#include "trace.h" /* temp */
#include "unicodec.h"
#include "utils.h"
#include "xappres.h"
#include "xscreen.h"
#include "xstatus.h"
#include "xtables.h"

static XChar2b *status_2b;
static unsigned char *status_1b;
static XChar2b *display_2b;
static bool *sxcursor_want;
static bool *sxcursor_have;
static bool  status_changed = false;

static struct status_line {
    bool         changed;
    int             start, len, color;
    XChar2b        *s2b;
    unsigned char  *s1b;
    XChar2b        *d2b;
} *status_line;

static int offsets[] = {
    0,	/* connection status */
    8,	/* wait, locked */
    39,	/* shift, insert, timing, cursor position */
    -1
};
#define SSZ ((sizeof(offsets)/sizeof(offsets[0])) - 1)

#define CTLR_REGION	0
#define WAIT_REGION	1
#define MISC_REGION	2

static int colors[SSZ] =  {
    FA_INT_NORM_NSEL,
    FA_INT_HIGH_SEL,
    FA_INT_NORM_NSEL
};

static int colors3279[SSZ] =  {
    HOST_COLOR_BLUE,
    HOST_COLOR_WHITE,
    HOST_COLOR_BLUE
};

#define CM	(60 * 10)	/* csec per minute */

/*
 * The status line is laid out thusly (M is maxCOLS):
 *
 *   0		"4" in a square
 *   1		"A" underlined
 *   2		solid box if connected, "?" in a box if not
 *   3..7	empty
 *   8...	message area
 *   M-41	Meta indication ("M" or blank)
 *   M-40	Alt indication ("A" or blank)
 *   M-39	Shift indication (Special symbol/"^" or blank)
 *   M-38	APL indication (Special symbol/"a" or blank)
 *   M-37	empty
 *   M-36	Compose indication ("C" or blank)
 *   M-35	Compose first character
 *   M-34	empty
 *   M-33	Typeahead indication ("T" or blank)
 *   M-32	Screentrace count
 *   M-31	Alternate keymap indication ("K" or blank)
 *   M-30	Reverse input mode indication ("R" or blank)
 *   M-29	Insert mode indication (Special symbol/"I" or blank)
 *   M-28	Printer indication ("P" or blank)
 *   M-27	Script indication ("s" or blank)
 *   M-26..M-16	empty
 *   M-15..M-9	command timing (Clock symbol and m:ss, or blank)
 *   M-7..M	cursor position (rrr/ccc or blank)
 */

/* Positions */

#define LBOX	0		/* left-hand box */
#define CNCT	1		/* connection between */
#define RBOX	2		/* right-hand box */

#define M0	8		/* message area */

#define SHIFT	(maxCOLS-39)	/* shift indication */

#define COMPOSE	(maxCOLS-36)	/* compose characters */

#define TYPEAHD	(maxCOLS-33)	/* typeahead */

#define SCRNTRC	(maxCOLS-32)	/* screentrace */

#define KMAP	(maxCOLS-31)	/* alt keymap in effect */

#define REVERSE (maxCOLS-30)	/* reverse input mode in effect */

#define INSERT	(maxCOLS-29)	/* insert mode */

#define PSESS	(maxCOLS-28)	/* printer session */

#define SCRIPT	(maxCOLS-27)	/* script in progress */

#define LU	(maxCOLS-25)	/* LU name */
#define LUCNT	8

#define T0	(maxCOLS-15)	/* timings */
#define	TCNT	7

#define C0	(maxCOLS-7)	/* cursor position */
#define CCNT	7

#define STATUS_Y	(*screen_height - *descent)

static unsigned char	nullblank;
static Position		status_y;

/* Status line contents (high-level) */

static void do_disconnected(void);
static void do_reconnecting(void);
static void do_resolving(void);
static void do_connecting(void);
static void do_tls(void);
static void do_proxy(void);
static void do_telnet(void);
static void do_tn3270e(void);
static void do_awaiting_first(void);
static void do_unlock_delay(void);
static void do_inhibit(void);
static void do_blank(void);
static void do_twait(void);
static void do_syswait(void);
static void do_protected(void);
static void do_numeric(void);
static void do_overflow(void);
static void do_dbcs(void);
static void do_scrolled(void);
static void do_minus(void);
static void do_disabled(void);
static void do_file_transfer(void);

static bool oia_undera = true;
static bool oia_boxsolid = false;
static int oia_shift = 0;
static bool oia_typeahead = false;
static int oia_screentrace = -1;
static bool oia_compose = false;
static ucs4_t oia_compose_char = 0;
static enum keytype oia_compose_keytype = KT_STD;
static enum msg {
    DISCONNECTED,	/* X Not Connected */
    XRECONNECTING,	/* X Reconnecting */
    XRESOLVING,		/* X [DNS] */
    CONNECTING,		/* X [TCP] */
    TLS,		/* X [TLS] */
    PROXY,		/* X [PROXY] */
    TELNET,		/* X [TELNET] */
    TN3270E,		/* X [TN3270E] */
    AWAITING_FIRST,	/* X [Field] */
    UNLOCK_DELAY,	/* X */
    INHIBIT,		/* X Inhibit */
    BLANK,		/* (blank) */
    TWAIT,		/* X Wait */
    SYSWAIT,		/* X SYSTEM */
    PROTECTED,		/* X Protected */
    NUMERIC,		/* X Numeric */
    OVERFLOW,		/* X Overflow */
    DBCS,		/* X DBCS */
    SCROLLED,		/* X Scrolled */
    MINUS,		/* X -f */
    KBD_DISABLED,	/* X Disabled */
    FILE_TRANSFER,	/* X File Transfer */
    N_MSGS
} oia_msg = DISCONNECTED, scroll_saved_msg, disabled_saved_msg = BLANK;
static char oia_lu[LUCNT+1];
static bool msg_is_saved = false;
static int n_scrolled = 0;
static void (*msg_proc[N_MSGS])(void) = {
    do_disconnected,
    do_reconnecting,
    do_resolving,
    do_connecting,
    do_tls,
    do_proxy,
    do_telnet,
    do_tn3270e,
    do_awaiting_first,
    do_unlock_delay,
    do_inhibit,
    do_blank,
    do_twait,
    do_syswait,
    do_protected,
    do_numeric,
    do_overflow,
    do_dbcs,
    do_scrolled,
    do_minus,
    do_disabled,
    do_file_transfer,
};
static int msg_color[N_MSGS] = {
    FA_INT_NORM_NSEL,	/* disconnected */
    FA_INT_NORM_NSEL,	/* reconnecting */
    FA_INT_NORM_NSEL,	/* resolving */
    FA_INT_NORM_NSEL,	/* connecting */
    FA_INT_NORM_NSEL,	/* tls */
    FA_INT_NORM_NSEL,	/* proxy */
    FA_INT_NORM_NSEL,	/* telnet */
    FA_INT_NORM_NSEL,	/* tn3270e */
    FA_INT_NORM_NSEL,	/* awaiting_first */
    FA_INT_NORM_NSEL,	/* unlock_delay */
    FA_INT_NORM_NSEL,	/* inhibit */
    FA_INT_NORM_NSEL,	/* blank */
    FA_INT_NORM_NSEL,	/* twait */
    FA_INT_NORM_NSEL,	/* syswait */
    FA_INT_HIGH_SEL,	/* protected */
    FA_INT_HIGH_SEL,	/* numeric */
    FA_INT_HIGH_SEL,	/* overflow */
    FA_INT_HIGH_SEL,	/* dbcs */
    FA_INT_NORM_NSEL,	/* scrolled */
    FA_INT_HIGH_SEL,	/* minus */
    FA_INT_HIGH_SEL,	/* disabled */
    FA_INT_NORM_NSEL,	/* file transfer */
};
static int msg_color3279[N_MSGS] = {
    HOST_COLOR_WHITE,	/* disconnected */
    HOST_COLOR_WHITE,	/* reconnecting */
    HOST_COLOR_WHITE,	/* resolving */
    HOST_COLOR_WHITE,	/* connecting */
    HOST_COLOR_WHITE,	/* tls */
    HOST_COLOR_WHITE,	/* proxy */
    HOST_COLOR_WHITE,	/* telnet */
    HOST_COLOR_WHITE,	/* tn3270e */
    HOST_COLOR_WHITE,	/* awaiting_first */
    HOST_COLOR_WHITE,	/* unlock_delay */
    HOST_COLOR_WHITE,	/* inhibit */
    HOST_COLOR_BLUE,	/* blank */
    HOST_COLOR_WHITE,	/* twait */
    HOST_COLOR_WHITE,	/* syswait */
    HOST_COLOR_RED,	/* protected */
    HOST_COLOR_RED,	/* numeric */
    HOST_COLOR_RED,	/* overflow */
    HOST_COLOR_RED,	/* dbcs */
    HOST_COLOR_WHITE,	/* scrolled */
    HOST_COLOR_RED,	/* minus */
    HOST_COLOR_RED,	/* disabled */
    HOST_COLOR_WHITE,	/* file transfer */
};
static bool oia_insert = false;
static bool oia_reverse = false;
static bool oia_kmap = false;
static bool oia_script = false;
static bool oia_printer = false;
static char *oia_cursor = NULL;
static char *oia_timing = NULL;

static unsigned char disc_msg[] = {
    CG_lock, CG_space, CG_commhi, CG_badcommhi, CG_commhi, CG_commjag,
    CG_commlo, CG_space
};
static int disc_len = sizeof(disc_msg);

static unsigned char recon_msg[] = {
    CG_lock, CG_space, CG_commhi, CG_badcommhi, CG_commhi, CG_commjag,
    CG_commlo, CG_space, CG_clockleft, CG_clockright
};
static int recon_len = sizeof(recon_msg);

static unsigned char rslv_msg[] = {
    CG_lock, CG_space, CG_commhi, CG_badcommhi, CG_commhi, CG_commjag,
    CG_commlo, CG_space, CG_bracketleft, CG_D, CG_N, CG_S, CG_bracketright
};
static int rslv_len = sizeof(rslv_msg);

static unsigned char cnct_msg[] = {
    CG_lock, CG_space, CG_commhi, CG_badcommhi, CG_commhi, CG_commjag,
    CG_commlo, CG_space, CG_bracketleft, CG_T, CG_C, CG_P, CG_bracketright
};
static int cnct_len = sizeof(cnct_msg);

static unsigned char tls_msg[] = {
    CG_lock, CG_space, CG_commhi, CG_badcommhi, CG_commhi, CG_commjag,
    CG_commlo, CG_space, CG_bracketleft, CG_T, CG_L, CG_S, CG_bracketright
};
static int tls_len = sizeof(tls_msg);

static unsigned char proxy_msg[] = {
    CG_lock, CG_space, CG_commhi, CG_badcommhi, CG_commhi, CG_commjag,
    CG_commlo, CG_space, CG_bracketleft, CG_P, CG_r, CG_o, CG_x, CG_y,
    CG_bracketright
};
static int proxy_len = sizeof(proxy_msg);

static unsigned char telnet_msg[] = {
    CG_lock, CG_space, CG_bracketleft, CG_T, CG_E, CG_L, CG_N, CG_E, CG_T,
    CG_bracketright
};
static int telnet_len = sizeof(telnet_msg);

static unsigned char tn3270e_msg[] = {
    CG_lock, CG_space, CG_bracketleft, CG_T, CG_N, CG_3, CG_2, CG_7, CG_0,
    CG_E, CG_bracketright
};
static int tn3270e_len = sizeof(tn3270e_msg);

static unsigned char awaiting_first_msg[] = {
    CG_lock, CG_space, CG_bracketleft, CG_F, CG_i, CG_e, CG_l, CG_d,
    CG_bracketright
};
static int awaiting_first_len = sizeof(awaiting_first_msg);

static unsigned char *a_not_connected;
static unsigned char *a_reconnecting;
static unsigned char *a_resolving;
static unsigned char *a_connecting;
static unsigned char *a_tls;
static unsigned char *a_proxy;
static unsigned char *a_telnet;
static unsigned char *a_tn3270e;
static unsigned char *a_awaiting_first;
static unsigned char *a_inhibit;
static unsigned char *a_twait;
static unsigned char *a_syswait;
static unsigned char *a_protected;
static unsigned char *a_numeric;
static unsigned char *a_overflow;
static unsigned char *a_dbcs;
static unsigned char *a_scrolled;
static unsigned char *a_minus;
static unsigned char *a_disabled;
static unsigned char *a_file_transfer;

static ioid_t revert_timer_id = NULL_IOID;

static unsigned char *make_amsg(const char *key);

static void cancel_disabled_revert(void);
static void status_render(int region);
static void do_ctlr(void);
static void do_msg(enum msg t);
static void paint_msg(enum msg t);
static void do_insert(bool on);
static void do_reverse(bool on);
static void do_kmap(bool on);
static void do_script(bool on);
static void do_printer(bool on);
static void do_shift(int state);
static void do_typeahead(int state);
static void do_screentrace(int state);
static void do_compose(bool on, ucs4_t ucs4, enum keytype keytype);
static void do_lu(const char *lu);
static void do_timing(char *buf);
static void do_cursor(char *buf);

static void status_connect(bool connected);
static void status_3270_mode(bool connected);
static void status_printer(bool on);

/**
 * Status line module registration.
 */
void
status_register(void)
{
    register_schange(ST_NEGOTIATING, status_connect);
    register_schange(ST_CONNECT, status_connect);
    register_schange(ST_3270_MODE, status_3270_mode);
    register_schange(ST_PRINTER, status_printer);
}

/* Initialize the status line */
void
status_init(void)
{
    a_not_connected = make_amsg("statusNotConnected");
    a_reconnecting = make_amsg("statusReconnecting");
    a_resolving = make_amsg("statusResolving");
    a_connecting = make_amsg("statusConnecting");
    a_tls = make_amsg("statusTlsPending");
    a_proxy = make_amsg("statusProxyPending");
    a_telnet = make_amsg("statusTelnetPending");
    a_tn3270e = make_amsg("statusTn3270ePending");
    a_awaiting_first = make_amsg("statusAwaitingFirst");
    a_inhibit = make_amsg("statusInhibit");
    a_twait = make_amsg("statusTwait");
    a_syswait = make_amsg("statusSyswait");
    a_protected = make_amsg("statusProtected");
    a_numeric = make_amsg("statusNumeric");
    a_overflow = make_amsg("statusOverflow");
    a_dbcs = make_amsg("statusDbcs");
    a_scrolled = make_amsg("statusScrolled");
    a_minus = make_amsg("statusMinus");
    a_disabled = make_amsg("statusDisabled");
    a_file_transfer = make_amsg("statusFileTransfer");

    oia_shift = toggled(APL_MODE)? AplMode: 0;
}

/* Reinitialize the status line */
void
status_reinit(unsigned cmask)
{
    unsigned i;

    if (cmask & FONT_CHANGE) {
	nullblank = *standard_font ? ' ' : CG_space;
    }
    if (cmask & (FONT_CHANGE | MODEL_CHANGE | SCROLL_CHANGE)) {
	status_y = STATUS_Y;
	if (!*descent) {
	    ++status_y;
	}
    }
    if (cmask & MODEL_CHANGE) {
	Replace(status_line,
		(struct status_line *)XtCalloc(sizeof(struct status_line),
		    SSZ));
	Replace(status_2b,
		(XChar2b *)XtCalloc(sizeof(XChar2b), maxCOLS));
	Replace(status_1b,
		(unsigned char *)XtCalloc(sizeof(unsigned char), maxCOLS));
	Replace(display_2b,
		(XChar2b *)XtCalloc(sizeof(XChar2b), maxCOLS));
	Replace(sxcursor_want,
		(bool *)XtCalloc(sizeof(bool), maxCOLS));
	Replace(sxcursor_have,
		(bool *)XtCalloc(sizeof(bool), maxCOLS));
	offsets[SSZ] = maxCOLS;
	if (appres.interactive.mono) {
	    colors[1] = FA_INT_NORM_NSEL;
	}
	for (i = 0; i < SSZ; i++) {
	    status_line[i].start = offsets[i];
	    status_line[i].len = offsets[i+1] - offsets[i];
	    status_line[i].s2b = status_2b + offsets[i];
	    status_line[i].s1b = status_1b + offsets[i];
	    status_line[i].d2b = display_2b + offsets[i];
	}
    } else {
	memset(display_2b, 0, maxCOLS * sizeof(XChar2b));
    }
    if (cmask & (COLOR_CHANGE | MODEL_CHANGE)) {
	for (i = 0; i < SSZ; i++) {
	    status_line[i].color = mode3279 ? colors3279[i] : colors[i];
	}
    }

    for (i = 0; i < SSZ; i++) {
	status_line[i].changed = true;
    }
    status_changed = true;

    /*
     * Always redraw all the fields; it's easier than keeping track of
     * what may have changed and why.
     */
    do_ctlr();
    paint_msg(oia_msg);
    do_insert(oia_insert);
    do_reverse(oia_reverse);
    do_kmap(oia_kmap);
    do_script(oia_script);
    do_printer(oia_printer);
    do_shift(oia_shift);
    do_typeahead(oia_typeahead);
    do_screentrace(oia_screentrace);
    do_compose(oia_compose, oia_compose_char, oia_compose_keytype);
    do_lu(oia_lu);
    do_cursor(oia_cursor);
    do_timing(oia_timing);
}

/* Check for a space. */
static bool
status_space(int col)
{
    return (*standard_font &&
	    (status_1b[col] == ' ' || status_1b[col] == 0)) ||
	   (!*standard_font &&
	    (status_1b[col] == CG_space || status_1b[col] == CG_null));
}

/* Render the status line onto the screen */
void
status_disp(void)
{
    unsigned i;
    int col;

    if (!status_changed) {
	return;
    }
    for (i = 0; i < SSZ; i++) {
	if (status_line[i].changed) {
	    status_render(i);
	    memmove(status_line[i].d2b, status_line[i].s2b,
		    status_line[i].len * sizeof(XChar2b));
	    status_line[i].changed = false;
	}
    }

    /* Draw or undraw the crosshair. */
    for (col = 0; col < maxCOLS; col++) {
	if (sxcursor_want[col]) {
	    if (status_space(col)) {
		XTextItem16 text1;
		XChar2b text = screen_vcrosshair();

		text1.chars = &text;
		text1.nchars = 1;
		text1.delta = 0;
		text1.font = *fid;
		XDrawText16(display, *screen_window,
			screen_crosshair_gc(),
			COL_TO_X(col), status_y, &text1, 1);
		sxcursor_have[col] = true;
	    }
	} else if (sxcursor_have[col]) {
	    XFillRectangle(display, *screen_window, screen_invgc(0),
		    COL_TO_X(col),
		    status_y - *ascent,
		    *char_width, *char_height);
	    sxcursor_have[col] = false;
	}
    }

    status_changed = false;
}

/* Mark the entire status line as changed */
void
status_touch(void)
{
    unsigned i;

    for (i = 0; i < SSZ; i++) {
	status_line[i].changed = true;
	memset(status_line[i].d2b, 0, status_line[i].len * sizeof(XChar2b));
    }
    status_changed = true;
}

/* Connected or disconnected */
static void
status_connect(bool connected)
{
    if (connected) {
	oia_boxsolid = IN_3270 && !IN_SSCP;
	do_ctlr();
	if (cstate == RECONNECTING) {
	    cancel_disabled_revert();
	    do_msg(XRECONNECTING);
	} else if (cstate == RESOLVING) {
	    oia_boxsolid = false;
	    do_ctlr();
	    cancel_disabled_revert();
	    do_msg(XRESOLVING);
	    status_untiming();
	    status_uncursor_pos();
	} else if (cstate == TCP_PENDING) {
	    oia_boxsolid = false;
	    do_ctlr();
	    cancel_disabled_revert();
	    do_msg(CONNECTING);
	    status_untiming();
	    status_uncursor_pos();
	} else if (cstate == TLS_PENDING) {
	    oia_boxsolid = false;
	    do_ctlr();
	    cancel_disabled_revert();
	    do_msg(TLS);
	    status_untiming();
	    status_uncursor_pos();
	} else if (cstate == PROXY_PENDING) {
	    oia_boxsolid = false;
	    do_ctlr();
	    cancel_disabled_revert();
	    do_msg(PROXY);
	    status_untiming();
	    status_uncursor_pos();
	} else if (cstate == TELNET_PENDING) {
	    oia_boxsolid = false;
	    do_ctlr();
	    cancel_disabled_revert();
	    do_msg(TELNET);
	    status_untiming();
	    status_uncursor_pos();
	} else if (cstate == CONNECTED_UNBOUND) {
	    oia_boxsolid = false;
	    do_ctlr();
	    cancel_disabled_revert();
	    do_msg(TN3270E);
	    status_untiming();
	    status_uncursor_pos();
	} else if (kybdlock & KL_AWAITING_FIRST) {
	    cancel_disabled_revert();
	    do_msg(AWAITING_FIRST);
	} else if (kybdlock & KL_ENTER_INHIBIT) {
	    cancel_disabled_revert();
	    do_msg(INHIBIT);
        } else if (kybdlock & KL_BID) {
	    cancel_disabled_revert();
	    do_msg(TWAIT);
        } else if (kybdlock & KL_FT) {
	    cancel_disabled_revert();
	    do_msg(FILE_TRANSFER);
        } else if (kybdlock & KL_DEFERRED_UNLOCK) {
	    cancel_disabled_revert();
	    do_msg(UNLOCK_DELAY);
	} else {
	    cancel_disabled_revert();
	    do_msg(BLANK);
	}
    } else {
	oia_boxsolid = false;
	do_ctlr();
	cancel_disabled_revert();
	do_msg(DISCONNECTED);
	status_uncursor_pos();
    }
    status_untiming();
}

/* Changed 3270 mode */
static void
status_3270_mode(bool connected)
{
    oia_boxsolid = IN_3270 && !IN_SSCP;
    do_ctlr();
    status_untiming();
    status_connect(CONNECTED);
}

/* Toggle printer session mode */
static void
status_printer(bool on)
{
    do_printer(oia_printer = on);
}

/* Revert the disabled message. */
static void
revert_disabled(ioid_t id _is_unused)
{
    assert(disabled_saved_msg != KBD_DISABLED);
    paint_msg(disabled_saved_msg);
    revert_timer_id = NULL_IOID;
}

/* Cancel the revert timer. */
static void
cancel_disabled_revert(void)
{
    if (revert_timer_id != NULL_IOID) {
	RemoveTimeOut(revert_timer_id);
	revert_timer_id = NULL_IOID;
    }
}

/* Revert early. */
static void
revert_early(void)
{
    if (revert_timer_id != NULL_IOID) {
	RemoveTimeOut(revert_timer_id);
	revert_disabled(NULL_IOID);
    }
}

/* Keyboard disable flash. */
void
status_keyboard_disable_flash(void)
{
    if (keyboard_disabled()) {
	if (oia_msg == KBD_DISABLED) {
	    /* Push out the revert timer. */
	    if (revert_timer_id != NULL_IOID) {
		RemoveTimeOut(revert_timer_id);
		revert_timer_id = AddTimeOut(1000, revert_disabled);
	    }
	} else {
	    disabled_saved_msg = oia_msg;
	    paint_msg(KBD_DISABLED);

	    /* Revert the message in 1s. */
	    assert(revert_timer_id == NULL_IOID);
	    revert_timer_id = AddTimeOut(1000, revert_disabled);
	}
    } else {
	if (oia_msg == KBD_DISABLED) {
	    cancel_disabled_revert();
	    paint_msg(disabled_saved_msg);
	}
    }
}

/* Lock the keyboard (twait) */
void
status_twait(void)
{
    oia_undera = false;
    do_ctlr();
    cancel_disabled_revert();
    do_msg(TWAIT);
}

/* Done with controller confirmation */
void
status_ctlr_done(void)
{
    oia_undera = true;
    do_ctlr();
}

/* Lock the keyboard (X SYSTEM) */
void
status_syswait(void)
{
    cancel_disabled_revert();
    do_msg(SYSWAIT);
}

/* Lock the keyboard (operator error) */
void
status_oerr(int error_type)
{
    switch (error_type) {
    case KL_OERR_PROTECTED:
	cancel_disabled_revert();
	do_msg(PROTECTED);
	break;
    case KL_OERR_NUMERIC:
	cancel_disabled_revert();
	do_msg(NUMERIC);
	break;
    case KL_OERR_OVERFLOW:
	cancel_disabled_revert();
	do_msg(OVERFLOW);
	break;
    case KL_OERR_DBCS:
	cancel_disabled_revert();
	do_msg(DBCS);
	break;
    }
}

/*
 * The interaction of SCROLLED and KBD_DISABLED is somewhat complex.
 *
 * KBD_DISABLED overlays SCROLLED, and KBD_DISABLED overlays SCROLLED.  When
 * the disable revert timer expires, SCROLLED will be restored (if we are still
 * scrolled). But when SCROLLED reverts, KBD_DISABLED will *not* be restored.
 *
 * Meanwhile, any other OIA state that is set while showing SCROLLED (or
 * showing KBD_DISABLED which is overlaying SCROLLED) will be saved, to be
 * restored when scrolling reverts.
 */

/* Lock the keyboard (X Scrolled) */
void
status_scrolled(int n)
{
    /* Fire the 'X Disabled' revert timer early. */
    revert_early();

    n_scrolled = n;
    if (n != 0) {
	if (!msg_is_saved) {
	    scroll_saved_msg = oia_msg;
	    assert(scroll_saved_msg != SCROLLED);
	    assert(scroll_saved_msg != KBD_DISABLED);
	    msg_is_saved = true;
	}
	paint_msg(SCROLLED);
    } else {
	if (msg_is_saved) {
	    msg_is_saved = false;
	    paint_msg(scroll_saved_msg);
	}
    }
}

/* Lock the keyboard (X -f) */
void
status_minus(void)
{
    cancel_disabled_revert();
    do_msg(MINUS);
}

/* Unlock the keyboard */
void
status_reset(void)
{
    cancel_disabled_revert();
    status_connect(PCONNECTED);
}

/* Toggle insert mode */
void
status_insert_mode(bool on)
{
    do_insert(oia_insert = on);
}

/* Toggle reverse mode */
void
status_reverse_mode(bool on)
{
    do_reverse(oia_reverse = on);
}

/* Toggle kmap mode */
void
status_kmap(bool on)
{
    do_kmap(oia_kmap = on);
}

/* Toggle script mode */
void
status_script(bool on)
{
    do_script(oia_script = on);
}

/* Toggle shift mode */
void
status_shift_mode(int state)
{
    do_shift(oia_shift = (oia_shift & AplMode) | state);
}

/* Toggle APL mode. */
void
status_apl_mode(bool on)
{
    do_shift(oia_shift = (oia_shift & ~AplMode) | (on? AplMode: 0));
}

/* Toggle typeahead */
void
status_typeahead(bool on)
{
    do_typeahead(oia_typeahead = on);
}

/* Change screentrace */
void
status_screentrace(int n)
{
    do_screentrace(oia_screentrace = n);
}

/* Set compose character */
void
status_compose(bool on, ucs4_t ucs4, enum keytype keytype)
{
    oia_compose = on;
    oia_compose_char = ucs4;
    oia_compose_keytype = keytype;
    do_compose(on, ucs4, keytype);
}

/* Set LU name */
void
status_lu(const char *lu)
{
    if (lu != NULL) {
	strncpy(oia_lu, lu, LUCNT);
	oia_lu[LUCNT] = '\0';
    } else {
	memset(oia_lu, '\0', sizeof(oia_lu));
    }
    do_lu(oia_lu);
}

/* Display timing */
void
status_timing(struct timeval *t0, struct timeval *t1)
{
    static char no_time[] = ":??.?";
    static char buf[32];

    if (t1->tv_sec - t0->tv_sec > (99*60)) {
	do_timing(oia_timing = no_time);
    } else {
	unsigned long cs;	/* centiseconds */

	cs = (t1->tv_sec - t0->tv_sec) * 10 +
	     (t1->tv_usec - t0->tv_usec + 50000) / 100000;
	if (cs < CM) {
	    snprintf(buf, sizeof(buf), ":%02ld.%ld", cs / 10, cs % 10);
	} else {
	    snprintf(buf, sizeof(buf), "%02ld:%02ld", cs / CM, (cs % CM) / 10);
	}
	do_timing(oia_timing = buf);
    }
}

/* Erase timing indication */
void
status_untiming(void)
{
    do_timing(oia_timing = NULL);
}

/* Update cursor position */
void
status_cursor_pos(int ca)
{
    char *buf;

    if (xappres.xquartz_hack) {
	buf = Asprintf("%02d/%02d", ((ca / COLS) + 1) % 100,
		((ca % COLS) + 1) % 100);
    } else {
	buf = Asprintf("%03d/%03d", ((ca / COLS) + 1) % 1000,
		((ca % COLS) + 1) % 1000);
    }
    Replace(oia_cursor, buf);
    do_cursor(oia_cursor);
}

/* Erase cursor position */
void
status_uncursor_pos(void)
{
    Replace(oia_cursor, NULL);
    do_cursor(oia_cursor);
}

/* Internal routines */

/* Set the changed status for a particular status-line column. */
static void
set_status_changed(int col)
{
    unsigned i;

    status_changed = true;
    for (i = 0; i < SSZ; i++) {
	if (col >= status_line[i].start &&
	    col <  status_line[i].start + status_line[i].len) {
	    status_line[i].changed = true;
	    break;
	}
    }
}

/* Update the status line by displaying "symbol" at column "col".  */
static void
status_add(int col, unsigned char symbol, enum keytype keytype)
{
    XChar2b n2b;

    /* Store the text. */
    n2b.byte1 = (keytype == KT_STD) ? 0 : 1;
    n2b.byte2 = symbol;
    if (status_2b[col].byte1 == n2b.byte1 &&
	    status_2b[col].byte2 == n2b.byte2) {
	return;
    }
    status_2b[col] = n2b;
    status_1b[col] = symbol;

    /* Update change status. */
    set_status_changed(col);
}

/**
 * Draw the crosshair cursor.
 *
 * @param[in] column	Column where the cursor should be
 */
void
status_crosshair(int column)
{
    sxcursor_want[column] = true;
    set_status_changed(column);
}

/**
 * Turn off the crosshair cursor, wherever it is.
 */
void
status_crosshair_off(void)
{
    int i;

    for (i = 0; i < maxCOLS; i++) {
	if (sxcursor_want[i]) {
	    sxcursor_want[i] = false;
	    set_status_changed(i);
	}
    }
}

/*
 * Render a region of the status line onto the display, the idea being to
 * minimize the number of redundant X drawing operations performed.
 *
 * What isn't optimized is what happens when "ABC" becomes "XBZ" -- should we
 * redundantly draw over B or not?  Right now we don't.
 */
static void
status_render(int region)
{
    int i;
    struct status_line *sl = &status_line[region];
    int nd = 0;
    int i0 = -1;
    XTextItem16 text1;

    /* The status region may change colors; don't be so clever */
    if (region == WAIT_REGION) {
	XFillRectangle(display, *screen_window,
		screen_invgc(sl->color),
		COL_TO_X(sl->start), status_y - *ascent,
		*char_width * sl->len, *char_height);
	text1.chars = sl->s2b;
	text1.nchars = sl->len;
	text1.delta = 0;
	text1.font = *fid;
	XDrawText16(display, *screen_window, screen_gc(sl->color),
		COL_TO_X(sl->start), status_y, &text1, 1);
    } else {
	for (i = 0; i < sl->len; i++) {
	    if (*funky_font || *xtra_width) {
		if (!sl->s1b[i]) {
		    continue;
		}
		XFillRectangle(display, *screen_window,
			screen_invgc(sl->color),
			COL_TO_X(sl->start + i), status_y - *ascent,
			*char_width, *char_height);
		text1.chars = sl->s2b + i;
		text1.nchars = 1;
		text1.delta = 0;
		text1.font = *fid;
		XDrawText16(display,
			*screen_window,
			screen_gc(sl->color),
			COL_TO_X(sl->start + i),
			status_y,
			&text1, 1);
		continue;
	    }
	    if (sl->s2b[i].byte1 == sl->d2b[i].byte1 &&
		sl->s2b[i].byte2 == sl->d2b[i].byte2) {
		if (nd) {
		    XFillRectangle(display, *screen_window,
			    screen_invgc(sl->color), COL_TO_X(sl->start + i0),
			    status_y - *ascent, *char_width * nd,
			    *char_height);
		    text1.chars = sl->s2b + i0;
		    text1.nchars = nd;
		    text1.delta = 0;
		    text1.font = *fid;
		    XDrawText16(display, *screen_window, screen_gc(sl->color),
			    COL_TO_X(sl->start + i0), status_y, &text1, 1);
		    nd = 0;
		    i0 = -1;
		}
	    } else {
		if (!nd++) {
		    i0 = i;
		}
	    }
	}
	if (nd) {
	    XFillRectangle(display, *screen_window, screen_invgc(sl->color),
		    COL_TO_X(sl->start + i0), status_y - *ascent,
		    *char_width * nd, *char_height);
	    text1.chars = sl->s2b + i0;
	    text1.nchars = nd;
	    text1.delta = 0;
	    text1.font = *fid;
	    XDrawText16(display, *screen_window,
		screen_gc(sl->color),
		COL_TO_X(sl->start + i0), status_y,
		&text1, 1);
	}
    }

    /* Leftmost region has unusual attributes */
    if (*standard_font && region == CTLR_REGION) {
	XFillRectangle(display, *screen_window,
		screen_invgc(sl->color),
		COL_TO_X(sl->start), status_y - *ascent,
		*char_width * 3, *char_height);
	XFillRectangle(display, *screen_window,
		screen_gc(sl->color),
		COL_TO_X(sl->start + LBOX), status_y - *ascent,
		*char_width, *char_height);
	XFillRectangle(display, *screen_window,
		screen_gc(sl->color),
		COL_TO_X(sl->start + RBOX), status_y - *ascent,
		*char_width, *char_height);
	text1.chars = sl->s2b + LBOX;
	text1.nchars = 1;
	text1.delta = 0;
	text1.font = *fid;
	XDrawText16(display, *screen_window,
		screen_invgc(sl->color),
		COL_TO_X(sl->start + LBOX), status_y,
		&text1, 1);
	XDrawRectangle(display, *screen_window, screen_gc(sl->color),
		COL_TO_X(sl->start + CNCT),
		status_y - *ascent + *char_height - 1,
		*char_width - 1, 0);
	text1.chars = sl->s2b + CNCT;
	XDrawText16(display, *screen_window,
		screen_gc(sl->color),
		COL_TO_X(sl->start + CNCT), status_y,
		&text1, 1);
	text1.chars = sl->s2b + RBOX;
	XDrawText16(display, *screen_window,
		screen_invgc(sl->color),
		COL_TO_X(sl->start + RBOX), status_y,
		&text1, 1);
    }
}

/* Write into the message area of the status line */
static void
status_msg_set(unsigned const char *msg, int len)
{
    int i;

    for (i = 0; i < status_line[WAIT_REGION].len; i++) {
	status_add(M0 + i, len ? msg[i] : nullblank, KT_STD);
	if (len) {
	    len--;
	}
    }
}

/* Controller status */
static void
do_ctlr(void)
{
    if (*standard_font) {
	status_add(LBOX, '4', KT_STD);
	if (oia_undera) {
	    status_add(CNCT, (IN_E ? 'B' : 'A'), KT_STD);
	} else {
	    status_add(CNCT, ' ', KT_STD);
	}
	if (IN_NVT) {
	    status_add(RBOX, 'N', KT_STD);
	} else if (oia_boxsolid) {
	    status_add(RBOX, ' ', KT_STD);
	} else if (IN_SSCP) {
	    status_add(RBOX, 'S', KT_STD);
	} else {
	    status_add(RBOX, '?', KT_STD);
	}
    } else {
	status_add(LBOX, CG_box4, KT_STD);
	if (oia_undera) {
	    status_add(CNCT, (IN_E ? CG_underB : CG_underA), KT_STD);
	} else {
	    status_add(CNCT, CG_space, KT_STD);
	}
	if (IN_NVT) {
	    status_add(RBOX, CG_N, KT_STD);
	} else if (oia_boxsolid) {
	    status_add(RBOX, CG_boxsolid, KT_STD);
	} else if (IN_SSCP) {
	    status_add(RBOX, CG_boxhuman, KT_STD);
	} else {
	    status_add(RBOX, CG_boxquestion, KT_STD);
	}
    }
}

/* Message area */

/* Change the state of the message area, or if scrolled, the saved message */
static void
do_msg(enum msg t)
{
    if (msg_is_saved) {
	scroll_saved_msg = t;
	return;
    }
    paint_msg(t);
}

/* Paint the message area. */
static void
paint_msg(enum msg t)
{
    oia_msg = t;
    (*msg_proc[(int)t])();
    if (!appres.interactive.mono) {
	status_line[WAIT_REGION].color = mode3279 ?
	    msg_color3279[(int)t] : msg_color[(int)t];
    }
}

static void
do_blank(void)
{
    status_msg_set((unsigned char *) 0, 0);
}

static void
do_disconnected(void)
{
    if (*standard_font) {
	status_msg_set(a_not_connected, strlen((char *)a_not_connected));
    } else {
	status_msg_set(disc_msg, disc_len);
    }
}

static void
do_reconnecting(void)
{
    if (*standard_font) {
	status_msg_set(a_reconnecting, strlen((char *)a_reconnecting));
    } else {
	status_msg_set(recon_msg, recon_len);
    }
}

static void
do_resolving(void)
{
    if (*standard_font) {
	status_msg_set(a_resolving, strlen((char *)a_resolving));
    } else {
	status_msg_set(rslv_msg, rslv_len);
    }
}

static void
do_connecting(void)
{
    if (*standard_font) {
	status_msg_set(a_connecting, strlen((char *)a_connecting));
    } else {
	status_msg_set(cnct_msg, cnct_len);
    }
}

static void
do_tls(void)
{
    if (*standard_font) {
	status_msg_set(a_tls, strlen((char *)a_tls));
    } else {
	status_msg_set(tls_msg, tls_len);
    }
}

static void
do_proxy(void)
{
    if (*standard_font) {
	status_msg_set(a_proxy, strlen((char *)a_proxy));
    } else {
	status_msg_set(proxy_msg, proxy_len);
    }
}

static void
do_telnet(void)
{
    if (*standard_font) {
	status_msg_set(a_telnet, strlen((char *)a_telnet));
    } else {
	status_msg_set(telnet_msg, telnet_len);
    }
}

static void
do_tn3270e(void)
{
    if (*standard_font) {
	status_msg_set(a_tn3270e, strlen((char *)a_tn3270e));
    } else {
	status_msg_set(tn3270e_msg, tn3270e_len);
    }
}

static void
do_awaiting_first(void)
{
    if (*standard_font) {
	status_msg_set(a_awaiting_first, strlen((char *)a_awaiting_first));
    } else {
	status_msg_set(awaiting_first_msg, awaiting_first_len);
    }
}

static void
do_unlock_delay(void)
{
    static unsigned char unlock_delay[] = {
	CG_lock
    };

    if (*standard_font) {
	status_msg_set((unsigned const char *)"X", 1);
    } else {
	status_msg_set(unlock_delay, sizeof(unlock_delay));
    }
}

static void
do_inhibit(void)
{
    static unsigned char inhibit[] = {
	CG_lock, CG_space, CG_I, CG_n, CG_h, CG_i, CG_b, CG_i, CG_t
    };

    if (*standard_font) {
	status_msg_set(a_inhibit, strlen((char *)a_inhibit));
    } else {
	status_msg_set(inhibit, sizeof(inhibit));
    }
}

static void
do_twait(void)
{
    static unsigned char twait[] = {
	CG_lock, CG_space, CG_clockleft, CG_clockright
    };

    if (*standard_font) {
	status_msg_set(a_twait, strlen((char *)a_twait));
    } else {
	status_msg_set(twait, sizeof(twait));
    }
}

static void
do_syswait(void)
{
    static unsigned char syswait[] = {
	CG_lock, CG_space, CG_S, CG_Y, CG_S, CG_T, CG_E, CG_M
    };

    if (*standard_font) {
	status_msg_set(a_syswait, strlen((char *)a_syswait));
    } else {
	status_msg_set(syswait, sizeof(syswait));
    }
}

static void
do_protected(void)
{
    static unsigned char protected[] = {
	CG_lock, CG_space, CG_leftarrow, CG_human, CG_rightarrow
    };

    if (*standard_font) {
	status_msg_set(a_protected, strlen((char *)a_protected));
    } else {
	status_msg_set(protected, sizeof(protected));
    }
}

static void
do_numeric(void)
{
    static unsigned char numeric[] = {
	CG_lock, CG_space, CG_human, CG_N, CG_U, CG_M
    };

    if (*standard_font) {
	status_msg_set(a_numeric, strlen((char *)a_numeric));
    } else {
	status_msg_set(numeric, sizeof(numeric));
    }
}

static void
do_overflow(void)
{
    static unsigned char overflow[] = {
	CG_lock, CG_space, CG_human, CG_greater
    };

    if (*standard_font) {
	status_msg_set(a_overflow, strlen((char *)a_overflow));
    } else {
	status_msg_set(overflow, sizeof(overflow));
    }
}

static void
do_dbcs(void)
{
    static unsigned char dbcs[] = {
	CG_lock, CG_space, CG_less, CG_S, CG_greater
    };

    if (*standard_font) {
	status_msg_set(a_dbcs, strlen((char *)a_dbcs));
    } else {
	status_msg_set(dbcs, sizeof(dbcs));
    }
}

static void
do_scrolled(void)
{
    static unsigned char scrolled[] = {
	CG_lock, CG_space, CG_S, CG_c, CG_r, CG_o, CG_l, CG_l, CG_e,
	CG_d, CG_space, CG_space, CG_space, CG_space, CG_space
    };
    static unsigned char spaces[] = {
	CG_space, CG_space, CG_space, CG_space
    };

    if (*standard_font) {
	char *t;

	t = XtMalloc(strlen((char *)a_scrolled) + 4);
	sprintf(t, "%s %d", (char *)a_scrolled, n_scrolled);
	status_msg_set((unsigned char *)t, strlen(t));
	XtFree(t);
    } else {
	char nnn[5];
	int i;

	sprintf(nnn, "%d", n_scrolled);
	memcpy((char *)&scrolled[11], (char *)spaces, sizeof(spaces));
	for (i = 0; nnn[i]; i++) {
	    scrolled[11 + i] = asc2cg0[(int)nnn[i]];
	}
	status_msg_set(scrolled, sizeof(scrolled));
    }
}

static void
do_minus(void)
{
    static unsigned char minus[] = {
	CG_lock, CG_space, CG_minus, CG_f
    };

    if (*standard_font) {
	status_msg_set(a_minus, strlen((char *)a_minus));
    } else {
	status_msg_set(minus, sizeof(minus));
    }
}

static void
do_disabled(void)
{
    static unsigned char disabled[] = {
	CG_lock, CG_space, CG_keyleft, CG_keyright
    };

    if (*standard_font) {
	status_msg_set(a_minus, strlen((char *)a_minus));
    } else {
	status_msg_set(disabled, sizeof(disabled));
    }
}

static void
do_file_transfer(void)
{
    static unsigned char file_transfer[] = {
	CG_lock, CG_space, CG_F, CG_i, CG_l, CG_e, CG_space,
	CG_T, CG_r, CG_a, CG_n, CG_s, CG_f, CG_e, CG_r
    };

    if (*standard_font) {
	status_msg_set(a_file_transfer, strlen((char *)a_file_transfer));
    } else {
	status_msg_set(file_transfer, sizeof(file_transfer));
    }
}

/* Insert, reverse, kmap, script, shift, compose */

static void
do_insert(bool on)
{
    status_add(INSERT,
	    on ? (*standard_font ? 'I' : CG_insert) : nullblank, KT_STD);
}

static void
do_reverse(bool on)
{
    status_add(REVERSE,
	    on ? (*standard_font ? 'R' : CG_R) : nullblank, KT_STD);
}

static void
do_kmap(bool on)
{
    status_add(KMAP,
	    on ? (*standard_font ? 'K' : CG_K) : nullblank, KT_STD);
}

static void
do_script(bool on)
{
    status_add(SCRIPT,
	    on ? (*standard_font ? 's' : CG_s) : nullblank, KT_STD);
}

static void
do_printer(bool on)
{
    status_add(PSESS,
	    on ? (*standard_font ? 'P' : CG_P) : nullblank, KT_STD);
}

static void
do_shift(int state)
{
    status_add(SHIFT-2, (state & MetaKeyDown) ?
	    (*standard_font ? 'M' : CG_M) : nullblank, KT_STD);
    status_add(SHIFT-1, (state & AltKeyDown) ?
	    (*standard_font ? 'A' : CG_A) : nullblank, KT_STD);
    status_add(SHIFT, (state & ShiftKeyDown) ?
	    (*standard_font ? '^' : CG_upshift) : nullblank, KT_STD);

    /* APL requires some somersaults. */
    if (state & AplMode) {
	status_add(SHIFT + 1,
		*full_apl_font? (CG_alpha & 0xff) :
		    (*standard_font? 'a': CG_a),
		*full_apl_font? KT_GE: KT_STD);
    } else {
	status_add(SHIFT + 1, nullblank, KT_STD);
    }
}

static void
do_typeahead(int state)
{
    status_add(TYPEAHD,
	    state ? (*standard_font ? 'T' : CG_T) : nullblank, KT_STD);
}

static void
do_screentrace(int n)
{
    unsigned char c;

    if (n < 0) {
	c = *standard_font? ' ': CG_space;
    } else if (n < 9) {
	c = *standard_font? ('1' + n): (CG_1 + n);
    } else {
	c = *standard_font? '+': CG_plus;
    }

    status_add(SCRNTRC, c, KT_STD);
}

static void
do_compose(bool on, ucs4_t ucs4, enum keytype keytype)
{
    if (on) {
	status_add(COMPOSE,
		(unsigned char)(*standard_font ? 'C' : CG_C), KT_STD);
	if (!ucs4) {
	    status_add(COMPOSE + 1, nullblank, KT_STD);
	} else {
	    if (*standard_font) {
		status_add(COMPOSE + 1, ucs4, KT_STD);
	    } else {
		ebc_t ebc;
		bool ge;

		ebc = unicode_to_ebcdic_ge(ucs4, &ge, false);
		status_add(COMPOSE + 1, ebc2cg0[ebc], ge? KT_GE: KT_STD);
	    }
	}
    } else {
	status_add(COMPOSE, nullblank, KT_STD);
	status_add(COMPOSE + 1, nullblank, KT_STD);
    }
}

static void
do_lu(const char *lu)
{
    int i;

    for (i = 0; i < LUCNT; i++) {
	status_add(LU + i,
	    lu[i]? (*standard_font? lu[i]: asc2cg0[(int)lu[i]]):
		nullblank,
	    KT_STD);
    }
}

/* Timing */
static void
do_timing(char *buf)
{
    int i;

    if (buf) {
	if (*standard_font) {
	    status_add(T0, nullblank, KT_STD);
	    status_add(T0 + 1, nullblank, KT_STD);
	} else {
	    status_add(T0, CG_clockleft, KT_STD);
	    status_add(T0 + 1, CG_clockright, KT_STD);
	}
	for (i = 0; i < (int) strlen(buf); i++) {
	    status_add(T0 + 2 + i,
		    *standard_font?
			buf[i]:
			asc2cg0[(unsigned char) buf[i]], KT_STD);
	}
    } else {
	for (i = 0; i < TCNT; i++) {
	    status_add(T0 + i, nullblank, KT_STD);
	}
    }
}

/* Cursor position */
static void
do_cursor(char *buf)
{
    int i;

    if (buf) {
	for (i = 0; i < (int) strlen(buf); i++) {
	    status_add(C0 + i,
		    *standard_font?
		    buf[i]: asc2cg0[(unsigned char) buf[i]], KT_STD);
	}
    } else {
	for (i = 0; i < CCNT; i++) {
	    status_add(C0 + i, nullblank, KT_STD);
	}
    }
}

/* Prepare status messages */

static unsigned char *
make_amsg(const char *key)
{
    return (unsigned char *)Asprintf("X %s", get_message(key));
}
