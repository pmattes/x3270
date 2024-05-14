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
 * Portions of this code were taken from xterm/button.c:
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 *	select.c
 *		This module handles selections.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>
#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"
#include "resources.h"
#include "toggles.h"

#include "codepage.h"
#include "ctlrc.h"
#include "kybd.h"
#include "popups.h"
#include "screen.h"
#include "selectc.h"
#include "tables.h"
#include "trace.h"
#include "unicodec.h"
#include "utf8.h"
#include "utils.h"
#include "xactions.h"
#include "xselect.h"
#include "xscreen.h"

#define Max(x, y)	(((x) > (y))? (x): (y))
#define Min(x, y)	(((x) < (y))? (x): (y))

#define HTTP_PREFIX	"http://"
#define HTTPS_PREFIX	"https://"

/*
 * Mouse side.
 */

/* A button click establishes the boundaries of the 'fixed' area. */
static int      f_start = 0;	/* 'fixed' area */
static int      f_end = 0;

/* Mouse motion moves the boundaries of the 'varying' area. */
static int      v_start = 0;	/* 'varying' area */
static int      v_end = 0;

static unsigned long down_time = 0;
static unsigned long down1_time = 0;
static Dimension down1_x, down1_y;
static unsigned long up_time = 0;
static bool      saw_motion = false;
static int      num_clicks = 0;
static int	last_move_baddr = 0;
static bool	click_unselected = false;
static bool grab_sel(int start, int end, bool really, Time t, bool as_url);
#define NS		5
static Atom     want_sel[NS];
static struct {		/* owned selections */
    Atom atom;		/* atom */
    char *buffer;	/* buffer contents (UTF-8) */
    bool initial;	/* true if from initial operation */
    Time time;		/* timestamp */
} own_sel[NS] = {
    { None, NULL, false },
    { None, NULL, false },
    { None, NULL, false },
    { None, NULL, false },
    { None, NULL, false }
};
static bool  cursor_moved = false;
static int      saved_cursor_addr;
static void own_sels(Time t, bool initial);
static int	n_owned_initial;
static bool	any_selected = false;

#define CLICK_INTERVAL	300

#define event_x(event)		event->xbutton.x
#define event_y(event)		event->xbutton.y
#define event_time(event)	event->xbutton.time

#define xyBOUNDED_COL_ROW(_x, _y, _col, _row) {	\
    _col = X_TO_COL(_x);		\
    if (_col < 0) {			\
	_col = 0;			\
    }					\
    if (_col >= COLS) {			\
	_col = COLS - 1;		\
    }					\
    if (flipped) {			\
	_col = (COLS - _col) - 1;	\
    }					\
    _row = Y_TO_ROW(_y);		\
    if (_row <= 0) {			\
	_row = 0;			\
    }					\
    if (_row >= ROWS) {			\
	_row = ROWS - 1;		\
    }					\
}

#define BOUNDED_COL_ROW(event, _col, _row) \
    xyBOUNDED_COL_ROW(event_x(event), event_y(event), _col, _row)

#define XOFFSET(x)	((x) - COL_TO_X(X_TO_COL(x)))
#define LEFT_QUARTER(x)	(XOFFSET(x) <= (*char_width) / 4)
#define RIGHT_QUARTER(x) (XOFFSET(x) >= (*char_width) * 3 / 4)
#define LEFT_THIRD(x)	(XOFFSET(x) <= (*char_width) / 3)
#define RIGHT_THIRD(x)	(XOFFSET(x) >= (*char_width) * 2 / 3)
#define LEFT_HALF(x)	(XOFFSET(x) <= (*char_width) / 2)
#define RIGHT_HALF(x)	(XOFFSET(x) >= (*char_width) / 2)

#define YOFFSET(y)	((y) - ROW_TO_Y(Y_TO_ROW(y) - 1))
#define TOP_HALF(y)	(YOFFSET(y) <= (*char_height) / 2)
#define BOTTOM_HALF(y)	(YOFFSET(y) >= (*char_height) / 2)

/* Default character class. */
static int char_class[256] = {
/* nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si */
    32,  1,  1,  1,  1,  1,  1,  1,  1, 32,  1,  1,  1,  1,  1,  1,
/* dle dc1 dc2 dc3 dc4 nak syn etb can  em sub esc  fs  gs  rs  us */
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/*  sp   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
/*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 58, 59, 60, 61, 62, 63,
/*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    64, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 91, 92, 93, 94, 48,
/*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    96, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~  del */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,123,124,125,126,   1,
/* ---,---,---,---,---,---,---,---,---,---,---,---,---,---,---,--- */
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* ---,---,---,---,---,---,---,---,---,---,---,---,---,---,---,--- */
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* nob exc cen ste cur yen bro sec dia cop ord gui not hyp reg mac */
    32,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
/* deg plu two thr acu mu  par per ce  one mas gui one one thr que */
   176,177,178,179,180,181,182,183,184,185,186,178,188,189,190,191,
/* Agr Aac Aci Ati Adi Ari AE  Cce Egr Eac Eci Edi Igr Iac Ici Idi */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/* ETH Nti Ogr Oac Oci Oti Odi mul Oob Ugr Uac Uci Udi Yac THO ssh */
    48, 48, 48, 48, 48, 48, 48,215, 48, 48, 48, 48, 48, 48, 48, 48,
/* agr aac aci ati adi ari ae  cce egr eac eci edi igr iac ici idi */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/* eth nti ogr oac oci oti odi div osl ugr uac uci udi yac tho ydi */
    48, 48, 48, 48, 48, 48, 48,247, 48, 48, 48, 48, 48, 48, 48, 48
};

/* Character class for isolating URLs. */
static int url_char_class[256] = {
/* nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si */
    32,  1,  1,  1,  1,  1,  1,  1,  1, 32,  1,  1,  1,  1,  1,  1,
/* dle dc1 dc2 dc3 dc4 nak syn etb can  em sub esc  fs  gs  rs  us */
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/*  sp   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    32, 33, 34, 35, 36, 48, 48, 39, 40, 41, 42, 43, 44, 45, 48, 48,
/*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 59, 60, 61, 62, 48,
/*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 92, 48, 94, 48,
/*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    96, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~  del */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,123,124,125,126,   1,
/* ---,---,---,---,---,---,---,---,---,---,---,---,---,---,---,--- */
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* ---,---,---,---,---,---,---,---,---,---,---,---,---,---,---,--- */
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* nob exc cen ste cur yen bro sec dia cop ord gui not hyp reg mac */
    32,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
/* deg plu two thr acu mu  par per ce  one mas gui one one thr que */
   176,177,178,179,180,181,182,183,184,185,186,178,188,189,190,191,
/* Agr Aac Aci Ati Adi Ari AE  Cce Egr Eac Eci Edi Igr Iac Ici Idi */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/* ETH Nti Ogr Oac Oci Oti Odi mul Oob Ugr Uac Uci Udi Yac THO ssh */
    48, 48, 48, 48, 48, 48, 48,215, 48, 48, 48, 48, 48, 48, 48, 48,
/* agr aac aci ati adi ari ae  cce egr eac eci edi igr iac ici idi */
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
/* eth nti ogr oac oci oti odi div osl ugr uac uci udi yac tho ydi */
    48, 48, 48, 48, 48, 48, 48,247, 48, 48, 48, 48, 48, 48, 48, 48
};

/* Parse a charClass string: [low-]high:value[,...] */
void
reclass(char *s)
{
    int n;
    int low, high, value;
    int i;
    char c;

    n = -1;
    low = -1;
    high = -1;
    for (;;) {
	c = *s++;
	if (isdigit((unsigned char)c)) {
	    if (n == -1) {
		n = 0;
	    }
	    n = (n * 10) + (c - '0');
	    if (n > 255) {
		goto fail;
	    }
	} else if (c == '-') {
	    if (n == -1 || low != -1) {
		goto fail;
	    }
	    low = n;
	    n = -1;
	} else if (c == ':') {
	    if (n == -1) {
		goto fail;
	    }
	    high = n;
	    n = -1;
	} else if (c == ',' || c == '\0') {
	    if (n == -1) {
		goto fail;
	    }
	    value = n;
	    n = -1;
	    if (high == -1) {
		goto fail;
	    }
	    if (low == -1) {
		low = high;
	    }
	    if (high < low) {
		goto fail;
	    }
	    for (i = low; i <= high; i++) {
		char_class[i] = value;
	    }
	    low = -1;
	    high = -1;
	    if (c == '\0') {
		return;
	    }
	} else {
	    goto fail;
	}
    }

fail:
    popup_an_error("Error in %s string", ResCharClass);
}

static int
ucs4_class(ucs4_t u)
{
    return (u < 0x100)? char_class[u]: (int)u;
}

static int
ucs4_url_class(ucs4_t u)
{
    return (u < 0x100)? url_char_class[u]: (int)u;
}

static int
xchar_class(ucs4_t u, bool as_url)
{
    return as_url? ucs4_url_class(u): ucs4_class(u);
}

static bool
select_word_x(int baddr, Time t, bool as_url)
{
    unsigned char fa = get_field_attribute(baddr);
    unsigned char ch;
    int class;

    /* Find the initial character class */
    if (ea_buf[baddr].ucs4) {
	class = xchar_class(ea_buf[baddr].ucs4, as_url);
    } else {
	if (FA_IS_ZERO(fa)) {
	    ch = EBC_space;
	} else {
	    ch = ea_buf[baddr].ec;
	}
	class = xchar_class(ebc2asc0[ch], as_url);
    }

    /* Find the beginning */
    for (f_start = baddr; ; f_start--) {
	int xclass;

	if (ea_buf[f_start].ucs4) {
	    xclass = xchar_class(ea_buf[f_start].ucs4, as_url);
	} else {
	    fa = get_field_attribute(f_start);
	    if (FA_IS_ZERO(fa)) {
		ch = EBC_space;
	    } else {
		ch = ea_buf[f_start].ec;
	    }
	    xclass = xchar_class(ebc2asc0[ch], as_url);
	}
	if (xclass != class) {
	    f_start++;
	    break;
	}

	/*
	 * If there was a line wrap, the last postion on the previous row will
	 * have GR_WRAP set.
	 */
	if (!(f_start % COLS) && f_start && (ea_buf[f_start - 1].gr & GR_WRAP)) {
	    continue;
	}

	if (!(f_start % COLS)) {
	    break;
	}
    }

    /* Find the end */
    for (f_end = baddr; ; f_end++) {
	int xclass;

	if (ea_buf[f_start].ucs4) {
	    xclass = xchar_class(ea_buf[f_end].ucs4, as_url);
	} else {
	    fa = get_field_attribute(f_end);
	    if (FA_IS_ZERO(fa)) {
		ch = EBC_space;
	    } else {
		ch = ea_buf[f_end].ec;
	    }
	    xclass = xchar_class(ebc2asc0[ch], as_url);
	}
	if (xclass != class) {
	    f_end--;
	    break;
	}

	/*
	 * If there was a line wrap, the last postion in the row will have GR_WRAP
	 * set.
	 */
	if (f_end != ((ROWS * COLS) - 1) && (ea_buf[f_end].gr & GR_WRAP)) {
	    continue;
	}

	if (!((f_end + 1) % COLS)) {
	    break;
	}
    }

    v_start = f_start;
    v_end = f_end;
    return grab_sel(f_start, f_end, true, t, as_url);
}

/* Select a word. Incorporates URL selection. */
static void
select_word(int baddr, Time t)
{
#if defined(HAVE_START) /*[*/
    if (select_word_x(baddr, t, true)) {
	return;
    }
#endif /*]*/
    select_word_x(baddr, t, false);
}

static void
select_line(int baddr, Time t)
{
    f_start = baddr - (baddr % COLS);
    f_end = f_start + COLS - 1;
    v_start = f_start;
    v_end = f_end;
    grab_sel(f_start, f_end, true, t, false);
}


/*
 * Start a new selection.
 * Usually bound to <Btn1Down>.
 */
void
select_start_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{
    int col, row;
    int baddr;

    xaction_debug(select_start_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }
    BOUNDED_COL_ROW(event, col, row);
    baddr = ROWCOL_TO_BA(row, col);
    f_start = f_end = v_start = v_end = baddr;
    down1_time = down_time = event_time(event);
    down1_x = event_x(event);
    down1_y = event_y(event);
    if (down_time - up_time > CLICK_INTERVAL) {
	num_clicks = 0;
	/* Commit any previous cursor move. */
	cursor_moved = false;
    }
    if (num_clicks == 0) {
	unselect(0, ROWS*COLS);
    }
}

/*
 * Alternate form of select_start, which combines cursor motion with selection.
 * Usually bound to <Btn1Down> in a user-specified keymap.
 */
void
move_select_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{
    int col, row;
    int baddr;

    xaction_debug(move_select_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }
    BOUNDED_COL_ROW(event, col, row);
    baddr = ROWCOL_TO_BA(row, col);

    f_start = f_end = v_start = v_end = baddr;
    down1_time = down_time = event_time(event);
    down1_x = event_x(event);
    down1_y = event_y(event);

    if (down_time - up_time > CLICK_INTERVAL) {
	num_clicks = 0;
	/* Commit any previous cursor move. */
	cursor_moved = false;
    }
    if (num_clicks == 0) {
	if (any_selected) {
	    unselect(0, ROWS*COLS);
	} else {
	    cursor_moved = true;
	    saved_cursor_addr = cursor_addr;
	    cursor_move(baddr);
	}
    }
}

/*
 * Begin extending the current selection.
 * Usually bound to <Btn3Down>.
 */
void
start_extend_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{
    int col, row;
    int baddr;
    bool continuous = (!ever_3270 && !toggled(RECTANGLE_SELECT));

    xaction_debug(start_extend_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    down1_time = 0L;

    BOUNDED_COL_ROW(event, col, row);
    baddr = ROWCOL_TO_BA(row, col);

    if (continuous) {
	/* Think linearly. */
	if (baddr < f_start) {
	    v_start = baddr;
	} else if (baddr > f_end) {
	    v_end = baddr;
	} else if (baddr - f_start > f_end - baddr) {
	    v_end = baddr;
	} else {
	    v_start = baddr;
	}
    } else {
	/* Think rectangularly. */
	int nrow = baddr / COLS;
	int ncol = baddr % COLS;
	int vrow_ul = v_start / COLS;
	int vrow_lr = v_end / COLS;
	int vcol_ul = Min(v_start % COLS, v_end % COLS);
	int vcol_lr = Max(v_start % COLS, v_end % COLS);

	/* Set up the row. */
	if (nrow <= vrow_ul) {
	    vrow_ul = nrow;
	} else if (nrow >= vrow_lr) {
	    vrow_lr = nrow;
	} else if (nrow - vrow_ul > vrow_lr - nrow) {
	    vrow_lr = nrow;
	} else {
	    vrow_ul = nrow;
	}

	/* Set up the column. */
	if (ncol <= vcol_ul) {
	    vcol_ul = ncol;
	} else if (ncol >= vcol_lr) {
	    vcol_lr = ncol;
	} else if (ncol - vcol_ul > vcol_lr - ncol) {
	    vcol_lr = ncol;
	} else {
	    vcol_ul = ncol;
	}

	v_start = (vrow_ul * COLS) + vcol_ul;
	v_end = (vrow_lr * COLS) + vcol_lr;
    }

    grab_sel(v_start, v_end, true, event_time(event), false);
    saw_motion = true;
    num_clicks = 0;
}

/*
 * Continuously extend the current selection.
 * Usually bound to <Btn1Motion> and <Btn3Motion>.
 */
void
select_extend_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{
    int col, row;
    int baddr;

    xaction_debug(select_extend_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    /* Ignore initial drag events if are too near. */
    if (down1_time != 0L &&
	abs((int)event_x(event) - (int)down1_x) < *char_width &&
	abs((int)event_y(event) - (int)down1_y) < *char_height) {
	return;
    } else {
	down1_time = 0L;
    }

    /* If we moved the 3270 cursor on the first click, put it back. */
    if (cursor_moved) {
	cursor_move(saved_cursor_addr);
	cursor_moved = false;
    }

    BOUNDED_COL_ROW(event, col, row);
    baddr = ROWCOL_TO_BA(row, col);

    /*
     * If baddr falls outside if the v range, open up the v range.  In
     * addition, if we are extending one end of the v range, make sure the
     * other end at least covers the f range.
     */
    if (baddr <= v_start) {
	v_start = baddr;
	v_end = f_end;
    }
    if (baddr >= v_end) {
	v_end = baddr;
	v_start = f_start;
    }

    /*
     * If baddr falls within the v range, narrow up the nearer end of the
     * v range.
     */
    if (baddr > v_start && baddr < v_end) {
	if (baddr - v_start < v_end - baddr) {
	    v_start = baddr;
	} else {
	    v_end = baddr;
	}
    }

    num_clicks = 0;
    saw_motion = true;
    grab_sel(v_start, v_end, false, event_time(event), false);
}

/*
 * Convert a sequence of strings to a list of selection atoms.
 */
static void
set_want_sel(String *params, Cardinal *num_params, Cardinal offset)
{
    Cardinal i;
    int num_ret = 0;

    for (i = offset; i < *num_params; i++) {
	Atom sel = XInternAtom(display, params[i], false);

	if (sel != None) {
	    int j;
	    bool dup = false;

	    for (j = 0; j < num_ret; j++) {
		if (want_sel[j] == sel) {
		    dup = true;
		    break;
		}
	    }
	    if (!dup && num_ret < NS) {
		want_sel[num_ret++] = sel;
	    }
	}
    }
    if (num_ret == 0) {
	want_sel[0] = XA_PRIMARY;
    }
    for (i = num_ret; i < NS; i++) {
	want_sel[i] = None;
    }
}

/*
 * End the selection.
 * Usually bound to <BtnUp>.
 */
void
select_end_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    int col, row;

    xaction_debug(select_end_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    set_want_sel(params, num_params, 0);

    BOUNDED_COL_ROW(event, col, row);
    up_time = event_time(event);

    if (up_time - down_time > CLICK_INTERVAL) {
	num_clicks = 0;
    }

    if (++num_clicks > 3) {
	num_clicks = 1;
    }

    switch (num_clicks) {
    case 1:
	if (saw_motion) {
	    f_start = v_start;
	    f_end = v_end;
	    grab_sel(f_start, f_end, true, event_time(event), false);
	}
	break;
    case 2:
	/*
	 * If we moved the 3270 cursor on the first click, put it back.
	 */
	if (cursor_moved) {
	    cursor_move(saved_cursor_addr);
	    cursor_moved = false;
	}
	select_word(f_start, event_time(event));
	break;
    case 3:
	select_line(f_start, event_time(event));
	break;
    }
    saw_motion = false;
}

/*
 * New-style selection actions.
 *
 * These actions work a bit more intuitively in 3270 mode than the historic
 * ones.
 *  SelectDown is usually bound to Btn1Down.
 *  SelectMotion is usually bound to Btn1Motion.
 *  SelectUp is usually bound to Btn1Up.
 *
 * SelectDown deselects on the first click, and remembers the screen position.
 *
 * SelectMotion extends the selection from the location remembered by
 *  SelectDown to the current location.
 *
 * SelectUp moves the cursor on the first click.  On clicks two and three, it
 *  selects a word or line.  On click four, it deselects and resets the click
 *  counter to one.
 */

void
SelectDown_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    xaction_debug(SelectDown_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    /* Just remember the start point. We haven't selected anything yet. */
    if (event_time(event) - down_time > CLICK_INTERVAL) {
	num_clicks = 0;
    }
    down_time = event_time(event);
    if (num_clicks == 0) {
	down1_time = down_time;
	down1_x = event_x(event);
	down1_y = event_y(event);
	if (any_selected) {
	    vtrace("SelectDown: unselected\n");
	    unselect(0, ROWS*COLS);
	    click_unselected = true;
	}
    }
}

void
SelectMotion_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    int x, y;
    int col, row;
    int baddr;
    int start_col, start_row;
    int start_baddr;
    int i;

    xaction_debug(SelectMotion_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    x = event_x(event);
    y = event_y(event);
    vtrace("SelectMotion: x %+d, y %+d\n",
	(int)x - (int)down1_x, (int)y - (int)down1_y);

    BOUNDED_COL_ROW(event, col, row);
    baddr = ROWCOL_TO_BA(row, col);
    xyBOUNDED_COL_ROW(down1_x, down1_y, start_col, start_row);
    start_baddr = ROWCOL_TO_BA(start_row, start_col);

    if (!saw_motion) {
	/*
	 * We haven't selected any cells yet.
	 *
	 * If the initial click was in the right two-thirds of a cell, we're
	 * goint to include that cell in the selection. If we're now in the
	 * right quarter of that same cell, or in the right half of another
	 * cell, we can start the selection. (Plus the equivalent if moving
	 * right-to-left: started in the left two-thirds, etc, etc.)
	 *
	 * There is probably a simpler way to do this.
	 */
	if ((x < down1_x && !LEFT_THIRD(down1_x) &&
	     ((col == start_col && LEFT_QUARTER(x)) ||
	      (col != start_col && RIGHT_HALF(x)))) ||
	    (x > down1_x && !RIGHT_THIRD(down1_x) &&
	     ((col == start_col && RIGHT_QUARTER(x)) ||
	      (col != start_col && RIGHT_HALF(x))))) {
	    /* Moved left or right far enough to select the initial cell. */
	    f_start = f_end = v_start = v_end = start_baddr;
	    saw_motion = true;
	    down1_time = 0L;
	} else if (col != start_col) {
	    if ((x < down1_x && LEFT_HALF(x)) ||
		(x > down1_x && RIGHT_HALF(x))) {
		/* Moved far enough to select this cell. */
		f_start = f_end = v_start = v_end = baddr;
		saw_motion = true;
		down1_time = 0L;
	    }
	}
	if (!saw_motion && row != start_row) {
	    if (!((row < start_row && TOP_HALF(y)) ||
		 (row > start_row && BOTTOM_HALF(y)))) {
		return;
	    }
	    /* Moved up or down by a row. */
	    if (LEFT_THIRD(down1_x)) {
		f_start = f_end = v_start = v_end = start_baddr;
	    } else {
		f_start = f_end = v_start = v_end = start_baddr + 1;
	    }
	    saw_motion = true;
	    down1_time = 0L;
	}
	if (!saw_motion) {
	    return;
	}
    } else {
	/* Extend the selection only if we've crossed the middle of a cell. */
	int last_move_row = last_move_baddr / COLS;
	int last_move_col = last_move_baddr % COLS;

	if (baddr <= v_start || baddr >= v_end) {
	    if (col < last_move_col && !LEFT_HALF(x)) {
		col++;
	    }
	    if (col > last_move_col && !RIGHT_HALF(x)) {
		col--;
	    }
	    if (row < last_move_row && !TOP_HALF(y)) {
		row++;
	    }
	    if (row > last_move_row && !BOTTOM_HALF(y)) {
		row--;
	    }
	}
	baddr = ROWCOL_TO_BA(row, col);

	if (baddr == last_move_baddr) {
	    num_clicks = 0;
	    return;
	}
    }

    /*
     * The following logic is subtly wrong.
     * When we move from two rows selected to just one, it gets it wrong the
     * first time, then the next time through, with no change in baddr, it gets
     * it right.
     *
     * For now, we do it twice every time to fix this.
     */

    for (i = 0; i < 2; i++) {
	/*
	 * If baddr falls outside if the v range, open up the v range.  In
	 * addition, if we are extending one end of the v range, make sure the
	 * other end at least covers the f range.
	 */
	if (baddr <= v_start) {
	    v_start = baddr;
	    v_end = f_end;
	}
	if (baddr >= v_end) {
	    v_end = baddr;
	    v_start = f_start;
	}

	/*
	 * If baddr falls within the v range, narrow up the nearer end of the
	 * v range.
	 */
	if (baddr > v_start && baddr < v_end) {
	    if (baddr - v_start < v_end - baddr) {
		v_start = baddr;
	    } else {
		v_end = baddr;
	    }
	}
    }

    num_clicks = 0;
    grab_sel(v_start, v_end, false, event_time(event), false);
}

void
SelectUp_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    int col, row;
    int baddr;

    xaction_debug(SelectUp_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    set_want_sel(params, num_params, 0);

    BOUNDED_COL_ROW(event, col, row);
    baddr = ROWCOL_TO_BA(row, col);

    if (event_time(event) - up_time > CLICK_INTERVAL) {
#if defined(DEBUG_CLICKS) /*[*/
	printf("too long, reset\n");
#endif /*]*/
	num_clicks = 0;
    }
    up_time = event_time(event);

    if (++num_clicks > 3) {
#if defined(DEBUG_CLICKS) /*[*/
	printf("wrap\n");
#endif /*]*/
	num_clicks = 1;
    }

#if defined(DEBUG_CLICKS) /*[*/
    printf("%d clicks\n", num_clicks);
#endif /*]*/
    switch (num_clicks) {
    case 1:
	/*
	 * If we saw motion, then take the selection.
	 * Otherwise, if we're in 3270 mode, move the cursor.
	 */
	if (saw_motion) {
	    f_start = v_start;
	    f_end = v_end;
	    grab_sel(f_start, f_end, true, event_time(event), false);
	} else if (IN_3270) {
	    if (!click_unselected) {
		cursor_moved = true;
		saved_cursor_addr = cursor_addr;
		cursor_move(baddr);
	    }
	}
	break;
    case 2:
	if (cursor_moved) {
	    cursor_move(saved_cursor_addr);
	    cursor_moved = false;
	}
	select_word(baddr, event_time(event));
	break;
    case 3:
	if (cursor_moved) {
	    cursor_move(saved_cursor_addr);
	    cursor_moved = false;
	}
	select_line(baddr, event_time(event));
	break;
    }
    saw_motion = false;
    click_unselected = false;
}

static void
set_select(XEvent *event, String *params, Cardinal *num_params)
{
    if (!any_selected) {
	return;
    }
    set_want_sel(params, num_params, 0);
    own_sels(event_time(event), false);
}

/*
 * Set the selection.
 * Usually bound to the Copy key.
 */
void
set_select_xaction(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
    xaction_debug(set_select_xaction, event, params, num_params);

    set_select(event, params, num_params);
}

/*
 * Translate the mouse position to a buffer address.
 */
int
mouse_baddr(Widget w, XEvent *event)
{
    int col, row;

    if (w != *screen) {
	return 0;
    }
    BOUNDED_COL_ROW(event, col, row);
    return ROWCOL_TO_BA(row, col);
}

/*
 * Cut action.
 */
#define ULS	sizeof(unsigned long)
#define ULBS	(ULS * 8)

void
Cut_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    int baddr;
    int ba2;
    unsigned char fa = get_field_attribute(0);
    unsigned long *target;

    xaction_debug(Cut_xaction, event, params, num_params);

    if (!any_selected) {
	return;
    }

    set_select(event, params, num_params);

    target = (unsigned long *)XtCalloc(ULS, ((ROWS*COLS)+(ULBS-1)) / ULBS);

    /* Identify the positions to empty. */
    for (baddr = 0; baddr < ROWS*COLS; baddr++) {
	if (ea_buf[baddr].fa) {
	    fa = ea_buf[baddr].fa;
	} else if ((IN_NVT || !FA_IS_PROTECTED(fa)) && screen_selected(baddr)) {
	    target[baddr/ULBS] |= 1L << (baddr%ULBS);
	}
    }

    /* Erase them. */
    for (baddr = 0; baddr < ROWS*COLS; baddr++) {
	if ((target[baddr/ULBS] & (1L << (baddr%ULBS)))
		    && ea_buf[baddr].ec != EBC_so
		    && ea_buf[baddr].ec != EBC_si) {
	    switch (ctlr_dbcs_state(baddr)) {
	    case DBCS_NONE:
	    case DBCS_SB:
		ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		break;
	    case DBCS_LEFT:
		ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		ba2 = baddr;
		INC_BA(ba2);
		ctlr_add(ba2, EBC_space, ea_buf[baddr].cs);
		break;
	    case DBCS_RIGHT:
		ba2 = baddr;
		DEC_BA(ba2);
		ctlr_add(ba2, EBC_space, ea_buf[baddr].cs);
		ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		break;
	    default:
		break;
	    }
	    mdt_set(baddr);
	}
    }

    Free(target);
}

/*
 * KybdSelect action.  Extends the selection area in the indicated direction.
 */
void
KybdSelect_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    enum { UP, DOWN, LEFT, RIGHT } direction;
    int x_start, x_end;

    xaction_debug(KybdSelect_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    if (*num_params < 1) {
	popup_an_error("%s(): Requires at least one argument",
		action_name(KybdSelect_xaction));
	return;
    }
    if (!strcasecmp(params[0], "Up")) {
	direction = UP;
    } else if (!strcasecmp(params[0], "Down")) {
	direction = DOWN;
    } else if (!strcasecmp(params[0], "Left")) {
	direction = LEFT;
    } else if (!strcasecmp(params[0], "Right")) {
	direction = RIGHT;
    } else {
	popup_an_error("%s(): First argument must be Up, Down, Left, or "
		"Right", action_name(KybdSelect_xaction));
	return;
    }

    if (!any_selected) {
	x_start = x_end = cursor_addr;
    } else {
	if (f_start < f_end) {
	    x_start = f_start;
	    x_end = f_end;
	} else {
	    x_start = f_end;
	    x_end = f_start;
	}
    }

    switch (direction) {
    case UP:
	if (!(x_start / COLS)) {
	    return;
	}
	x_start -= COLS;
	break;
    case DOWN:
	if ((x_end / COLS) == ROWS - 1) {
	    return;
	}
	x_end += COLS;
	break;
    case LEFT:
	if (!(x_start % COLS)) {
	    return;
	}
	x_start--;
	break;
    case RIGHT:
	if ((x_end % COLS) == COLS - 1) {
	    return;
	}
	x_end++;
	break;
    }

    /* Figure out the atoms they want. */
    set_want_sel(params, num_params, 1);

    /* Grab the selection. */
    f_start = v_start = x_start;
    f_end = v_end = x_end;
    grab_sel(f_start, f_end, true, event_time(event), false);
}

/*
 * unselect action.  Removes a selection.
 */
void
Unselect_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    xaction_debug(Unselect_xaction, event, params, num_params);

    /* It's just cosmetic. */
    unselect(0, ROWS*COLS);
}

void
SelectAll_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    xaction_debug(SelectUp_xaction, event, params, num_params);
    if (w != *screen) {
	return;
    }

    set_want_sel(params, num_params, 0);
    grab_sel(0, (ROWS * COLS) - 1, true, event_time(event), false);
}

/*
 * Screen side.
 */

static char    *select_buf = NULL;
static char    *sb_ptr = NULL;
static int      sb_size = 0;
#define SB_CHUNK	1024

static void
init_select_buf(void)
{
    if (select_buf == NULL) {
	select_buf = XtMalloc(sb_size = SB_CHUNK);
    }
    sb_ptr = select_buf;
}

static void
store_sel(char c)
{
    if (sb_ptr - select_buf >= sb_size) {
	sb_size += SB_CHUNK;
	select_buf = XtRealloc(select_buf, sb_size);
	sb_ptr = select_buf + sb_size - SB_CHUNK;
    }
    *(sb_ptr++) = c;
}

/*
 * Convert the UTF-8 string to an ICCCM-defined STRING (ISO 8859-1 printable
 * plus tab and newline).
 *
 * Returns the length of the stored string.
 */
static unsigned long
store_icccm_string(XtPointer value, const char *buf)
{
    char *dst = (char *)value;
    unsigned long len = 0;
    bool skip = false;

    while (*buf) {
	int nw;
	ucs4_t ucs;

	if (*buf == '\033') {
	    /* Funky GE sequence.  Skip it. */
	    *dst++ = ' ';
	    len++;
	    buf++;
	    skip = true;
	    continue;
	}
	nw = utf8_to_unicode(buf, strlen(buf), &ucs);
	if (nw <= 0) {
	    return len;
	}
	if (skip) {
	    skip = false;
	    continue;
	}
	if (ucs == '\n' ||
	    (ucs >= 0x20 && ucs <= 0x7f) ||
	    (ucs >= 0xa0 && ucs <= 0xff)) {
	    *dst++ = ucs & 0xff;
	    len++;
	}
	buf += nw;
    }
    return len;
}

Boolean
common_convert_sel(Widget w, Atom *selection, Atom *target, Atom *type,
	XtPointer *value, unsigned long *length, int *format, char *buffer,
	Time time)
{
    if (*target == XA_TARGETS(display)) {
	Atom* targetP;
	Atom* std_targets;
	unsigned long std_length;

	XmuConvertStandardSelection(w, time, selection,
		target, type, (caddr_t*) &std_targets, &std_length, format);
#if defined(XA_UTF8_STRING) /*[*/
	*length = std_length + 6;
#else /*][*/
	*length = std_length + 5;
#endif /*]*/
	*value = (XtPointer) XtMalloc(sizeof(Atom) * (*length));
	targetP = *(Atom**)value;
	*targetP++ = XA_STRING;
	*targetP++ = XA_TEXT(display);
	*targetP++ = XA_COMPOUND_TEXT(display);
#if defined(XA_UTF8_STRING) /*[*/
	*targetP++ = XA_UTF8_STRING(display);
#endif /*]*/
	*targetP++ = XA_LENGTH(display);
	*targetP++ = XA_LIST_LENGTH(display);
	memmove(targetP,  std_targets,
		(int) (sizeof(Atom) * std_length));
	XtFree((char *)std_targets);
	*type = XA_ATOM;
	*format = 32;
	return True;
    }

    if (*target == XA_STRING ||
	*target == XA_TEXT(display) ||
	*target == XA_COMPOUND_TEXT(display)
#if defined(XA_UTF8_STRING) /*[*/
	|| *target == XA_UTF8_STRING(display)
#endif /*]*/
	) {
	if (*target == XA_COMPOUND_TEXT(display)
#if defined(XA_UTF8_STRING) /*[*/
	    || *target == XA_UTF8_STRING(display)
#endif /*]*/
		) {
		*type = *target;
	} else {
		*type = XA_STRING;
	}
	*length = strlen(buffer);
	*value = XtMalloc(*length);
#if defined(XA_UTF8_STRING) /*[*/
	if (*target == XA_UTF8_STRING(display)) {
	    memmove(*value, buffer, (int) *length);
	} else {
#endif /*]*/
	    /*
	     * N.B.: We return a STRING for COMPOUND_TEXT.
	     * Someday we may do real ISO 2022, but not today.
	     */
	    *length = store_icccm_string(*value, buffer);
#if defined(XA_UTF8_STRING) /*[*/
	}
#endif /*]*/
	*format = 8;
	return True;
    }
    if (*target == XA_LIST_LENGTH(display)) {
	*value = XtMalloc(4);
	if (sizeof(long) == 4) {
	    *(long *)*value = 1;
	} else {
	    long temp = 1;
	    memmove(*value, ((char*) &temp) + sizeof(long) - 4, 4);
	}
	*type = XA_INTEGER;
	*length = 1;
	*format = 32;
	return True;
    }
    if (*target == XA_LENGTH(display)) {
	*value = XtMalloc(4);
	if (sizeof(long) == 4) {
	    *(long*)*value = strlen(buffer);
	} else {
	    long temp = strlen(buffer);
	    memmove(*value, ((char *) &temp) + sizeof(long) - 4, 4);
	}
	*type = XA_INTEGER;
	*length = 1;
	*format = 32;
	return True;
    }

    if (XmuConvertStandardSelection(w, time, selection,
	    target, type, (caddr_t *)value, length, format)) {
	return True;
    }

    return False;
}

static Boolean
convert_sel(Widget w, Atom *selection, Atom *target, Atom *type,
	XtPointer *value, unsigned long *length, int *format)
{
    int i;

    /* Find the right selection. */
    for (i = 0; i < NS; i++) {
	if (own_sel[i].atom == *selection) {
	    break;
	}
    }
    if (i >= NS) {	/* not my selection */
	return False;
    }

    return common_convert_sel(w, selection, target, type, value, length,
	    format, own_sel[i].buffer, own_sel[i].time);
}

static void
lose_sel(Widget w _is_unused, Atom *selection)
{
    char *a;
    int i;
    int initial_before = n_owned_initial;

    a = XGetAtomName(display, *selection);
    vtrace("main lose_sel %s\n", a);
    XFree(a);
    for (i = 0; i < NS; i++) {
	if (own_sel[i].atom != None && own_sel[i].atom == *selection) {
	    own_sel[i].atom = None;
	    XtFree(own_sel[i].buffer);
	    own_sel[i].buffer = NULL;
	    if (own_sel[i].initial) {
		own_sel[i].initial = false;
		n_owned_initial--;
	    }
	    break;
	}
    }
    if (initial_before && !n_owned_initial) {
	vtrace("main: lost all initial selections\n");
	unselect(0, ROWS*COLS);
    }
}

/*
 * Somewhat convoluted logic to return an ASCII character for a given screen
 * position.
 *
 * The character has to be found indirectly from ea_buf and the field
 * attirbutes, so that zero-intensity fields become blanks.
 */
static bool osc_valid = false;

static void
osc_start(void)
{
    osc_valid = false;
}

/*
 * Return a 'selection' version of a given character on the screen.
 * Returns a printable ASCII character, or 0 if the character is a NULL and
 * shouldn't be included in the selection.
 */
static void
onscreen_char(int baddr, unsigned char *r, int *rlen)
{
    static int osc_baddr;
    static unsigned char fa;
    ucs4_t uc;
    int baddr2;

    *rlen = 1;

    /* If we aren't moving forward, all bets are off. */
    if (osc_valid && baddr < osc_baddr) {
	osc_valid = false;
    }

    if (osc_valid) {
	/*
	 * Search for a new field attribute between the address we
	 * want and the last address we searched.  If we found a new
	 * field attribute, save the address for next time.
	 */
	get_bounded_field_attribute(baddr, osc_baddr, &fa);
	osc_baddr = baddr;
    } else {
	/*
	 * Find the attribute the old way.
	 */
	fa = get_field_attribute(baddr);
	osc_baddr = baddr;
	osc_valid = true;
    }

    /* If it isn't visible, then make it a blank. */
    if (FA_IS_ZERO(fa)) {
	*r = ' ';
	return;
    }

    /* Handle DBCS. */
    switch (ctlr_dbcs_state(baddr)) {
    case DBCS_LEFT:
	if (ea_buf[baddr].ucs4) {
	    *rlen = unicode_to_utf8(ea_buf[baddr].ucs4, (char *)r);
	} else {
	    baddr2 = baddr;
	    INC_BA(baddr2);
	    uc = ebcdic_to_unicode((ea_buf[baddr].ec << 8) |
			ea_buf[baddr2].ec,
		    CS_BASE, EUO_NONE);
	    *rlen = unicode_to_utf8(uc, (char *)r);
	}
	return;
    case DBCS_RIGHT:
	/* Returned the entire character when the left half was read. */
	*rlen = 0;
	return;
    case DBCS_SI:
	/* Suppress SI's altogether.  They'll expand back on paste. */
	*rlen = 0;
	return;
    case DBCS_SB:
	/* Treat SB's as normal SBCS characters. */
	break;
    default:
	break;
    }

    switch (ea_buf[baddr].cs) {
    case CS_BASE:
    default:
	if (ea_buf[baddr].ucs4) {
	    *rlen = unicode_to_utf8(ea_buf[baddr].ucs4, (char *)r);
	} else {
	    switch (ea_buf[baddr].ec) {
	    case EBC_so:
		/*
		 * Suppress SO's altogether.  They'll expand back on
		 * paste.
		 */
		*rlen = 0;
		return;
	    case EBC_null:
		*r = 0;
		return;
	    default:
		/*
		 * Note that we use the 'for_display' flavor of
		 * ebcdic_base_to_unicode here, so DUP and FM are
		 * translated to special private-use Unicode values.
		 * These will (hopefully) be ignored by other
		 * applications, but translated back to DUP and FM if
		 * pasted back into x3270.
		 */
		uc = ebcdic_base_to_unicode(ea_buf[baddr].ec,
			EUO_BLANK_UNDEF | EUO_UPRIV);
		*rlen = unicode_to_utf8(uc, (char *)r);
		if (*rlen < 0) {
		    *rlen = 0;
		}
	    }
	}
	return;
    case CS_GE:
	/* Translate APL to Unicode. */
	uc = apl_to_unicode(ea_buf[baddr].ec, EUO_NONE);
	if (uc == (ucs4_t)-1) {
	    /* No translation. */
	    uc = UPRIV_GE_00 + ea_buf[baddr].ec;
	}
	*rlen = unicode_to_utf8(uc, (char *)r);
	if (*rlen < 0) {
	    *rlen = 0;
	}
	return;
    case CS_LINEDRAW:
	/* VT100 line-drawing character. */
	*rlen = unicode_to_utf8(
		linedraw_to_unicode(ea_buf[baddr].ucs4, false), (char *)r);
	return;
    }
}

/*
 * Attempt to own the selections in want_sel[].
 */
static void
own_sels(Time t, bool initial)
{
    int i, j;
    char *a;

    /*
     * Try to grab any new selections we may want.
     */
    for (i = 0; i < NS; i++) {
	bool already_own = false;

	if (want_sel[i] == None) {
	    continue;
	}

	/* Check if we already own it. */
	for (j = 0; j < NS; j++) {
	    if (own_sel[j].atom == want_sel[i]) {
		already_own = true;
		break;
	    }
	}

	/* Find the slot for it. */
	if (!already_own) {
	    for (j = 0; j < NS; j++) {
		if (own_sel[j].atom == None) {
		    break;
		}
	    }
	    if (j >= NS) {
		continue;
	    }
	}

	/*
	 * We call XtOwnSelection again, even if we already own the selection,
	 * to update the timestamp.
	 */
	if (XtOwnSelection(*screen, want_sel[i], t, convert_sel, lose_sel,
		    NULL)) {
	    if (!already_own) {
		if (initial) {
		    n_owned_initial++;
		}
		own_sel[j].atom = want_sel[i];
		own_sel[j].initial = initial;
	    }
	    Replace(own_sel[j].buffer, XtMalloc(strlen(select_buf) + 1));
	    memmove(own_sel[j].buffer, select_buf, strlen(select_buf) + 1);
	    own_sel[j].time = t;
	    a = XGetAtomName(display, want_sel[i]);
	    vtrace("main own_sel %s %s %lu\n", a,
		    initial? "initial": "subsequent",
		    (unsigned long)t);
	    XFree(a);
	} else {
	    a = XGetAtomName(display, want_sel[i]);
	    vtrace("Could not get selection %s\n", a);
	    XFree(a);
	    if (already_own) {
		XtFree(own_sel[j].buffer);
		own_sel[j].buffer = NULL;
		own_sel[j].atom = None;
		if (own_sel[j].initial && !--n_owned_initial) {
		    vtrace("main: lost all initial selections\n");
		    unselect(0, ROWS*COLS);
		}
	    }
	}
    }
}

/*
 * Copy the selected area on the screen into a buffer and attempt to
 * own the selections in want_sel[].
 */
#define VISUAL_LEFT(d)	((IS_LEFT(d)) || ((d) == DBCS_SI))
static bool
grab_sel(int start, int end, bool really, Time t, bool as_url)
{
    int i, j;
    int start_row, end_row;
    int nulls = 0;
    unsigned char osc[16];
    int len;

    unselect(0, ROWS*COLS);

    if (start > end) {
	int exch = end;

	end = start;
	start = exch;
    }

    start_row = start / COLS;
    end_row = end / COLS;

    init_select_buf();	/* prime the store_sel() routine */
    osc_start();	/* prime the onscreen_char() routine */

    if (!ever_3270 && !toggled(RECTANGLE_SELECT)) {
	/* Continuous selections */
	bool last_wrap = false;

	if (IS_RIGHT(ctlr_dbcs_state(start))) {
	    DEC_BA(start);
	}
	if (VISUAL_LEFT(ctlr_dbcs_state(end))) {
	    INC_BA(end);
	}
	for (i = start; i <= end; i++) {
	    screen_set_select(i);
	    if (really) {
		if (i != start && !(i % COLS) && !last_wrap) {
		    nulls = 0;
		    store_sel('\n');
		}
		onscreen_char(i, osc, &len);
		for (j = 0; j < len; j++) {
		    if (osc[j]) {
			while (nulls) {
			    store_sel(' ');
			    nulls--;
			}
			store_sel((char)osc[j]);
		    } else {
			nulls++;
		    }
		}
		last_wrap = (ea_buf[i].gr & GR_WRAP) != 0;
	    }
	}
	/* Check for newline extension on the last line. */
	if ((end % COLS) != (COLS - 1)) {
	    bool all_blank = true;

	    for (i = end; i < end + (COLS - (end % COLS)); i++) {
		onscreen_char(i, osc, &len);
		for (j = 0; j < len; j++) {
		    if (osc[j]) {
			all_blank = false;
			    break;
		    }
		}
	    }
	    if (all_blank) {
		for (i = end; i < end + (COLS - (end % COLS)); i++) {
		    screen_set_select(i);
		}
		if (really) {
		    store_sel('\n');
		}
	    }
	}
    } else {
	/* Rectangular selections */
	if (start_row == end_row) {
	    if (IS_RIGHT(ctlr_dbcs_state(start))) {
		DEC_BA(start);
	    }
	    if (VISUAL_LEFT(ctlr_dbcs_state(end))) {
		INC_BA(end);
	    }
	    for (i = start; i <= end; i++) {
		screen_set_select(i);
		if (really) {
		    onscreen_char(i, osc, &len);
		    for (j = 0; j < len; j++) {
			if (osc[j]) {
			    while (nulls) {
				store_sel(' ');
				nulls--;
			    }
			    store_sel((char)osc[j]);
			} else {
			    nulls++;
			}
		    }
		}
	    }
	} else {
	    int row, col;
	    int start_col = start % COLS;
	    int end_col = end % COLS;

	    if (start_col > end_col) {
		int exch = end_col;

		end_col = start_col;
		start_col = exch;
	    }

	    for (row = start_row; row <= end_row; row++) {
		int sc = start_col;
		int ec = end_col;

		if (sc && IS_RIGHT(ctlr_dbcs_state(row*COLS + sc))) {
		    sc = sc - 1;
		}
		if (ec < COLS-1 &&
			VISUAL_LEFT(ctlr_dbcs_state(row*COLS + ec))) {
		    ec = ec + 1;
		}

		for (col = sc; col <= ec; col++) {
		    screen_set_select(row*COLS + col);
		    if (really) {
			onscreen_char(row*COLS + col, osc, &len);
			for (j = 0; j < len; j++) {
			    if (osc[j]) {
				while (nulls) {
				    store_sel(' ');
				    nulls--;
				}
				store_sel((char)osc[j]);
			    } else {
				nulls++;
			    }
			}
		    }
		}
		nulls = 0;
		if (really && row < end_row) {
		    store_sel('\n');
		}
	    }
	}
    }

    /* Terminate the result. */
    if (really) {
	store_sel('\0');
    }

    any_selected = true;
    ctlr_changed(0, ROWS*COLS);

    if (really) {
	own_sels(t, true);
    }

#if defined(HAVE_START) /*[*/
    if (as_url &&
	toggled(SELECT_URL) &&
	(!strncmp(select_buf, HTTP_PREFIX, strlen(HTTP_PREFIX)) ||
	 !strncmp(select_buf, HTTPS_PREFIX, strlen(HTTPS_PREFIX)))) {
#if defined(_WIN32) /*[*/
	char *command = Asprintf("start %s", select_buf);
#elif defined(linux) || defined(__linux__) /*[*/
	char *command = Asprintf("xdg-open %s", select_buf);
#elif defined(__APPLE__) /*][*/
	char *command = Asprintf("open %s", select_buf);
#elif defined(__CYGWIN__) /*][*/
	char *command = Asprintf("cygstart -o %s", select_buf);
#endif /*]*/
	int rc;

	vtrace("Starting URL open command: %s\n", command);
	rc = system(command);
	if (rc != 0) {
	    popup_an_error("URL open failed, return code %d", rc);
	}
	Free(command);
	return true;
    }
#endif /*]*/
    return false;
}

/*
 * Check if any character in a given region is selected.
 */
bool
area_is_selected(int baddr, int len)
{
    int i;

    for (i = 0; i < len; i++) {
	if (screen_selected(baddr+i)) {
	    return true;
	}
    }
    return false;
}

/*
 * Unhighlight the region of selected text -- but don't give up the selection.
 */
void
unselect(int baddr _is_unused, int len _is_unused)
{
    if (any_selected) {
	screen_unselect_all();
	ctlr_changed(0, ROWS*COLS);
	any_selected = false;
    }
}

/* Selection insertion. */
#define NP	5
static Atom	paste_atom[NP];
static int	n_pasting = 0;
static int	pix = 0;
static Time	paste_time;
#if defined(XA_UTF8_STRING) /*[*/
static bool	paste_utf8;
#endif /*]*/

static void
paste_callback(Widget w, XtPointer client_data _is_unused,
	Atom *selection _is_unused, Atom *type _is_unused, XtPointer value,
	unsigned long *length, int *format _is_unused)
{
    char *s;
    unsigned long s_len;
    ucs4_t *u_buf;
    size_t u_len;

    if ((value == NULL) || (*length == 0)) {
	XtFree(value);

	/* Try the next one. */
#if defined(XA_UTF8_STRING) /*[*/
	if (paste_utf8) {
	    paste_utf8 = false;
	    XtGetSelectionValue(w, paste_atom[(pix - 1)], XA_STRING,
		    paste_callback, NULL, paste_time);
	} else
#endif /*]*/
	if (n_pasting > pix) {
#if defined(XA_UTF8_STRING) /*[*/
	    paste_utf8 = true;
#endif /*]*/
	    XtGetSelectionValue(w, paste_atom[pix++],
#if defined(XA_UTF8_STRING) /*[*/
		    XA_UTF8_STRING(display),
#else /*][*/
		    XA_STRING,
#endif /*]*/
		    paste_callback, NULL,
		    paste_time);
	}
	return;
    }

    /* Convert the selection to Unicode. */
    s_len = *length;
    s = (char *)value;
    u_buf = (ucs4_t *)Malloc(*length * sizeof(ucs4_t));
    u_len = 0;

    while (s_len) {
	ucs4_t uc;

#if defined(XA_UTF8_STRING) /*[*/
	if (paste_utf8) {
	    int nu;

	    nu = utf8_to_unicode(s, s_len, &uc);
	    if (nu <= 0) {
		break;
	    }
	    s += nu;
	    s_len -= nu;
	} else
#endif /*]*/
	{
	    /* Assume it's ISO 8859-1. */
	    uc = *s & 0xff;
	    s++;
	    s_len--;
	}
	u_buf[u_len++] = uc;
    }
    emulate_uinput(u_buf, u_len, true);

    Free(u_buf);
    XtFree(value);

    n_pasting = 0;
}

void
insert_selection_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{
    Cardinal i;
    Atom a;
    XButtonEvent *be = (XButtonEvent *)event;

    xaction_debug(insert_selection_xaction, event, params, num_params);

    n_pasting = 0;
    for (i = 0; i < *num_params; i++) {
	a = XInternAtom(display, params[i], true);
	if (a == None) {
	    popup_an_error("%s(): No atom for selection",
		    action_name(insert_selection_xaction));
	    continue;
	}
	if (n_pasting < NP) {
	    paste_atom[n_pasting++] = a;
	}
    }
    pix = 0;
#if defined(XA_UTF8_STRING) /*[*/
    paste_utf8 = true;
#endif /*]*/
    if (n_pasting > pix) {
	paste_time = be->time;
	XtGetSelectionValue(w, paste_atom[pix++],
#if defined(XA_UTF8_STRING) /*[*/
		XA_UTF8_STRING(display),
#else /*][*/
		XA_STRING,
#endif /*]*/
		paste_callback, NULL,
		paste_time);
    }
}

/**
 * Select module registration.
 */
void
select_register(void)
{
    static toggle_register_t toggles[] = {
	{ RECTANGLE_SELECT,	NULL,	0 }
#if defined(HAVE_START) /*[*/
					   ,
	{ SELECT_URL,		NULL,	0 }
#endif /*]*/
    };

    register_toggles(toggles, array_count(toggles));
}
