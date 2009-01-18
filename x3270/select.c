/*
 * Copyright (c) 1993-2009, Paul Mattes.
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
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>
#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"
#include "screen.h"
#include "resources.h"

#include "actionsc.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "kybdc.h"
#include "popupsc.h"
#include "screenc.h"
#include "selectc.h"
#include "tablesc.h"
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"

#define Max(x, y)	(((x) > (y))? (x): (y))
#define Min(x, y)	(((x) < (y))? (x): (y))

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
static int      saw_motion = 0;
static int      num_clicks = 0;
static void grab_sel(int start, int end, Boolean really, Time t);
#define NS		5
static Atom     want_sel[NS];
static struct {			/* owned selections */
	Atom            atom;	/* atom */
	char           *buffer;	/* buffer contents (UTF-8) */
}               own_sel[NS];
static Boolean  cursor_moved = False;
static int      saved_cursor_addr;
static void own_sels(Time t);
static int	n_owned = -1;
static Boolean	any_selected = False;

extern Widget  *screen;

#define CLICK_INTERVAL	300

#define event_x(event)		event->xbutton.x
#define event_y(event)		event->xbutton.y
#define event_time(event)	event->xbutton.time

#define BOUNDED_XY(event, x, y) {	\
	x = X_TO_COL(event_x(event));	\
	if (x < 0)			\
		x = 0;			\
	if (x >= COLS)			\
		x = COLS - 1;		\
	if (flipped)			\
		x = (COLS - x) - 1;	\
	y = Y_TO_ROW(event_y(event) - *descent);	\
	if (y <= 0)			\
		y = 0;			\
	if (y >= ROWS)			\
		y = ROWS - 1;		\
}


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
		if (isdigit(c)) {
			if (n == -1)
				n = 0;
			n = (n * 10) + (c - '0');
			if (n > 255)
				goto fail;
		} else if (c == '-') {
			if (n == -1 || low != -1)
				goto fail;
			low = n;
			n = -1;
		} else if (c == ':') {
			if (n == -1)
				goto fail;
			high = n;
			n = -1;
		} else if (c == ',' || c == '\0') {
			if (n == -1)
				goto fail;
			value = n;
			n = -1;
			if (high == -1)
				goto fail;
			if (low == -1)
				low = high;
			if (high < low)
				goto fail;
			for (i = low; i <= high; i++)
				char_class[i] = value;
			low = -1;
			high = -1;
			if (c == '\0')
				return;
		} else
			goto fail;
	}

    fail:
	popup_an_error("Error in %s string", ResCharClass);
}

static void
select_word(int baddr, Time t)
{
	unsigned char fa = get_field_attribute(baddr);
	unsigned char ch;
	int class;

	/* Find the initial character class */
	if (FA_IS_ZERO(fa))
		ch = EBC_space;
	else
		ch = ea_buf[baddr].cc;
	class = char_class[ebc2asc0[ch]];

	/* Find the beginning */
	for (f_start = baddr; f_start % COLS; f_start--) {
		fa = get_field_attribute(f_start);
		if (FA_IS_ZERO(fa))
			ch = EBC_space;
		else
			ch = ea_buf[f_start].cc;
		if (char_class[ebc2asc0[ch]] != class) {
			f_start++;
			break;
		}
	}

	/* Find the end */
	for (f_end = baddr; (f_end+1) % COLS; f_end++) {
		fa = get_field_attribute(f_end);
		if (FA_IS_ZERO(fa))
			ch = EBC_space;
		else
			ch = ea_buf[f_end].cc;
		if (char_class[ebc2asc0[ch]] != class) {
			f_end--;
			break;
		}
	}

	v_start = f_start;
	v_end = f_end;
	grab_sel(f_start, f_end, True, t);
}

static void
select_line(int baddr, Time t)
{
	f_start = baddr - (baddr % COLS);
	f_end = f_start + COLS - 1;
	v_start = f_start;
	v_end = f_end;
	grab_sel(f_start, f_end, True, t);
}


/*
 * Start a new selection.
 * Usually bound to <Btn1Down>.
 */
void
select_start_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	int x, y;
	register int baddr;

	action_debug(select_start_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(select_start_action));
		return;
	}
	if (w != *screen)
		return;
	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);
	f_start = f_end = v_start = v_end = baddr;
	down1_time = down_time = event_time(event);
	down1_x = event_x(event);
	down1_y = event_y(event);
	if (down_time - up_time > CLICK_INTERVAL) {
		num_clicks = 0;
		/* Commit any previous cursor move. */
		cursor_moved = False;
	}
	if (num_clicks == 0)
		unselect(0, ROWS*COLS);
}

/*
 * Alternate form of select_start, which combines cursor motion with selection.
 * Usually bound to <Btn1Down> in a user-specified keymap.
 */
void
move_select_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	int x, y;
	register int baddr;

	action_debug(move_select_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(move_select_action));
		return;
	}
	if (w != *screen)
		return;
	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

	f_start = f_end = v_start = v_end = baddr;
	down1_time = down_time = event_time(event);
	down1_x = event_x(event);
	down1_y = event_y(event);

	if (down_time - up_time > CLICK_INTERVAL) {
		num_clicks = 0;
		/* Commit any previous cursor move. */
		cursor_moved = False;
	}
	if (num_clicks == 0) {
		if (any_selected) {
			unselect(0, ROWS*COLS);
		} else {
			cursor_moved = True;
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
start_extend_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	int x, y;
	int baddr;
	Boolean continuous = (!ever_3270 && !toggled(RECTANGLE_SELECT));

	action_debug(start_extend_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(start_extend_action));
		return;
	}
	if (w != *screen)
		return;

	down1_time = 0L;

	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

	if (continuous) {
		/* Think linearly. */
		if (baddr < f_start)
			v_start = baddr;
		else if (baddr > f_end)
			v_end = baddr;
		else if (baddr - f_start > f_end - baddr)
			v_end = baddr;
		else
			v_start = baddr;
	} else {
		/* Think rectangularly. */
		int nrow = baddr / COLS;
		int ncol = baddr % COLS;
		int vrow_ul = v_start / COLS;
		int vrow_lr = v_end / COLS;
		int vcol_ul = Min(v_start % COLS, v_end % COLS);
		int vcol_lr = Max(v_start % COLS, v_end % COLS);

		/* Set up the row. */
		if (nrow <= vrow_ul)
			vrow_ul = nrow;
		else if (nrow >= vrow_lr)
			vrow_lr = nrow;
		else if (nrow - vrow_ul > vrow_lr - nrow)
			vrow_lr = nrow;
		else
			vrow_ul = nrow;

		/* Set up the column. */
		if (ncol <= vcol_ul)
			vcol_ul = ncol;
		else if (ncol >= vcol_lr)
			vcol_lr = ncol;
		else if (ncol - vcol_ul > vcol_lr - ncol)
			vcol_lr = ncol;
		else
			vcol_ul = ncol;

		v_start = (vrow_ul * COLS) + vcol_ul;
		v_end = (vrow_lr * COLS) + vcol_lr;
	}

	grab_sel(v_start, v_end, True, event_time(event));
	saw_motion = 1;
	num_clicks = 0;
}

/*
 * Continuously extend the current selection.
 * Usually bound to <Btn1Motion> and <Btn3Motion>.
 */
void
select_extend_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	int x, y;
	int baddr;

	action_debug(select_extend_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(select_extend_action));
		return;
	}
	if (w != *screen)
		return;

	/* Ignore initial drag events if are too near. */
	if (down1_time != 0L &&
	    abs((int) event_x(event) - (int) down1_x) < *char_width &&
	    abs((int) event_y(event) - (int) down1_y) < *char_height)
		return;
	else
		down1_time = 0L;

	/* If we moved the 3270 cursor on the first click, put it back. */
	if (cursor_moved) {
		cursor_move(saved_cursor_addr);
		cursor_moved = False;
	}

	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

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
		if (baddr - v_start < v_end - baddr)
			v_start = baddr;
		else
			v_end = baddr;
	}

	num_clicks = 0;
	saw_motion = 1;
	grab_sel(v_start, v_end, False, event_time(event));
}

/*
 * End the selection.
 * Usually bound to <BtnUp>.
 */
void
select_end_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	Cardinal i;
	int x, y;

	action_debug(select_end_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(select_end_action));
		return;
	}
	if (w != *screen)
		return;

	if (n_owned == -1) {
		for (i = 0; i < NS; i++)
			own_sel[i].atom = None;
		n_owned = 0;
	}
	for (i = 0; i < NS; i++)
		if (i < *num_params)
			want_sel[i] = XInternAtom(display, params[i], False);
		else
			want_sel[i] = None;
	if (*num_params == 0)
		want_sel[0] = XA_PRIMARY;

	BOUNDED_XY(event, x, y);
	up_time = event_time(event);

	if (up_time - down_time > CLICK_INTERVAL)
		num_clicks = 0;

	if (++num_clicks > 3)
		num_clicks = 1;

	switch (num_clicks) {
	    case 1:
		if (saw_motion) {
			f_start = v_start;
			f_end = v_end;
			grab_sel(f_start, f_end, True, event_time(event));
		}
		break;
	    case 2:
		/*
		 * If we moved the 3270 cursor on the first click, put it back.
		 */
		if (cursor_moved) {
			cursor_move(saved_cursor_addr);
			cursor_moved = False;
		}
		select_word(f_start, event_time(event));
		break;
	    case 3:
		select_line(f_start, event_time(event));
		break;
	}
	saw_motion = 0;
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
SelectDown_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	int x, y;
	register int baddr;

	action_debug(SelectDown_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(SelectDown_action));
		return;
	}
	if (w != *screen)
		return;
	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

	if (event_time(event) - down_time > CLICK_INTERVAL)
		num_clicks = 0;

	down_time = event_time(event);
	if (num_clicks == 0) {
		f_start = f_end = v_start = v_end = baddr;
		down1_time = down_time;
		if (any_selected) {
			unselect(0, ROWS*COLS);
		}
	}
}

void
SelectMotion_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	int x, y;
	register int baddr;

	action_debug(SelectMotion_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(SelectMotion_action));
		return;
	}
	if (w != *screen)
		return;

	/* Ignore initial drag events if are too near. */
	if (down1_time != 0L &&
	    abs((int) event_x(event) - (int) down1_x) < *char_width &&
	    abs((int) event_y(event) - (int) down1_y) < *char_height)
		return;
	else
		down1_time = 0L;

	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

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
		if (baddr - v_start < v_end - baddr)
			v_start = baddr;
		else
			v_end = baddr;
	}

	num_clicks = 0;
	saw_motion = 1;
	grab_sel(v_start, v_end, False, event_time(event));
}

void
SelectUp_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	int x, y;
	register int baddr;
	Cardinal i;

	action_debug(SelectUp_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(SelectUp_action));
		return;
	}
	if (w != *screen)
		return;

	if (n_owned == -1) {
		for (i = 0; i < NS; i++)
			own_sel[i].atom = None;
		n_owned = 0;
	}
	for (i = 0; i < NS; i++) {
		if (i < *num_params)
			want_sel[i] = XInternAtom(display, params[i], False);
		else
			want_sel[i] = None;
	}
	if (*num_params == 0)
		want_sel[0] = XA_PRIMARY;

	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

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
			grab_sel(f_start, f_end, True, event_time(event));
		} else if (IN_3270) {
			cursor_move(baddr);
		}
		break;
	    case 2:
		/*
		 * If we moved the 3270 cursor on the first click, put it back.
		 */
		select_word(f_start, event_time(event));
		break;
	    case 3:
		select_line(f_start, event_time(event));
		break;
	}
	saw_motion = 0;
}

/*
 * Set the selection.
 * Usually bound to the Copy key.
 */
void
set_select_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	Cardinal i;

	action_debug(set_select_action, event, params, num_params);

	if (!any_selected)
		return;
	if (n_owned == -1) {
		for (i = 0; i < NS; i++)
			own_sel[i].atom = None;
		n_owned = 0;
	}
	for (i = 0; i < NS; i++)
		if (i < *num_params)
			want_sel[i] = XInternAtom(display, params[i], False);
		else
			want_sel[i] = None;
	if (*num_params == 0)
		want_sel[0] = XA_PRIMARY;
	own_sels(event_time(event));
}

/*
 * Translate the mouse position to a buffer address.
 */
int
mouse_baddr(Widget w, XEvent *event)
{
	int x, y;

	if (w != *screen)
		return 0;
	BOUNDED_XY(event, x, y);
	return ROWCOL_TO_BA(y, x);
}

/*
 * Cut action.
 * For now, merely erases all unprotected characters currently selected.
 * In future, it may interact more with selections.
 */
#define ULS	sizeof(unsigned long)
#define ULBS	(ULS * 8)

void
Cut_action(Widget w _is_unused, XEvent *event, String *params, Cardinal *num_params)
{
	register int baddr;
	unsigned char fa = get_field_attribute(0);
	unsigned long *target;
	register unsigned char repl;

	action_debug(Cut_action, event, params, num_params);

	target = (unsigned long *)XtCalloc(ULS, ((ROWS*COLS)+(ULBS-1))/ULBS);

	/* Identify the positions to empty. */
	for (baddr = 0; baddr < ROWS*COLS; baddr++) {
		if (ea_buf[baddr].fa)
			fa = ea_buf[baddr].fa;
		else if ((IN_ANSI || !FA_IS_PROTECTED(fa)) && SELECTED(baddr))
			target[baddr/ULBS] |= 1 << (baddr%ULBS);
	}

	/* Erase them. */
	if (IN_3270)
		repl = EBC_null;
	else
		repl = EBC_space;
	for (baddr = 0; baddr < ROWS*COLS; baddr++)
		if (target[baddr/ULBS] & (1 << (baddr%ULBS)))
			ctlr_add(baddr, repl, 0);

	Free(target);
}

/*
 * KybdSelect action.  Extends the selection area in the indicated direction.
 */
void
KybdSelect_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	enum { UP, DOWN, LEFT, RIGHT } direction;
	int x_start, x_end;
	Cardinal i;

	action_debug(KybdSelect_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(select_start_action));
		return;
	}
	if (w != *screen)
		return;

	if (*num_params < 1) {
		popup_an_error("%s requires one argument",
		    action_name(KybdSelect_action));
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
		popup_an_error("%s first argument must be Up, Down, Left, or "
		    "Right", action_name(KybdSelect_action));
		return;
	}

	if (!any_selected)
		x_start = x_end = cursor_addr;
	else {
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
		if (!(x_start / COLS))
			return;
		x_start -= COLS;
		break;
	    case DOWN:
		if ((x_end / COLS) == ROWS - 1)
			return;
		x_end += COLS;
		break;
	    case LEFT:
		if (!(x_start % COLS))
			return;
		x_start--;
		break;
	    case RIGHT:
		if ((x_end % COLS) == COLS - 1)
			return;
		x_end++;
		break;
	}

	/* Figure out the atoms they want. */
	if (n_owned == -1) {
		for (i = 0; i < NS; i++)
			own_sel[i].atom = None;
		n_owned = 0;
	}
	for (i = 1; i < NS; i++)
		if (i < *num_params)
			want_sel[i] = XInternAtom(display, params[i], False);
		else
			want_sel[i] = None;
	if (*num_params == 1)
		want_sel[0] = XA_PRIMARY;

	/* Grab the selection. */
	f_start = v_start = x_start;
	f_end = v_end = x_end;
	grab_sel(f_start, f_end, True, event_time(event));
}

/*
 * unselect action.  Removes a selection.
 */
void
Unselect_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Unselect_action, event, params, num_params);

	/* It's just cosmetic. */
	unselect(0, ROWS*COLS);
}

void
SelectAll_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	Cardinal i;

	action_debug(SelectUp_action, event, params, num_params);
	if (event == NULL) {
		popup_an_error("%s can only be used as a keymap action",
		    action_name(SelectAll_action));
		return;
	}
	if (w != *screen)
		return;

	if (n_owned == -1) {
		for (i = 0; i < NS; i++)
			own_sel[i].atom = None;
		n_owned = 0;
	}
	for (i = 0; i < NS; i++) {
		if (i < *num_params)
			want_sel[i] = XInternAtom(display, params[i], False);
		else
			want_sel[i] = None;
	}
	if (*num_params == 0)
		want_sel[0] = XA_PRIMARY;

	grab_sel(0, (ROWS * COLS) - 1, True, event_time(event));
}


/*
 * Screen side.
 */

static char    *select_buf = CN;
static char    *sb_ptr = CN;
static int      sb_size = 0;
#define SB_CHUNK	1024

static Time     sel_time;

static void
init_select_buf(void)
{
	if (select_buf == CN)
		select_buf = XtMalloc(sb_size = SB_CHUNK);
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
	Boolean skip = False;

	while (*buf) {
	    	int nw;
		ucs4_t ucs;

		if (*buf == '\033') {
		    	/* Funky GE sequence.  Skip it. */
		    	*dst++ = ' ';
			len++;
			buf++;
		    	skip = True;
			continue;
		}
	    	nw = utf8_to_unicode(buf, strlen(buf), &ucs);
		if (nw <= 0)
		    	return len;
		if (skip) {
		    	skip = False;
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

static Boolean
convert_sel(Widget w, Atom *selection, Atom *target, Atom *type,
    XtPointer *value, unsigned long *length, int *format)
{
	int i;

	/* Find the right selection. */
	for (i = 0; i < NS; i++)
		if (own_sel[i].atom == *selection)
			break;
	if (i >= NS)	/* not my selection */
		return False;

	if (*target == XA_TARGETS(display)) {
		Atom* targetP;
		Atom* std_targets;
		unsigned long std_length;

		XmuConvertStandardSelection(w, sel_time, selection,
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
		(void) memmove(targetP,  std_targets,
				   (int) (sizeof(Atom) * std_length));
		XtFree((char *) std_targets);
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
			)
			*type = *target;
		else
			*type = XA_STRING;
		*length = strlen(own_sel[i].buffer);
		*value = XtMalloc(*length);
#if defined(XA_UTF8_STRING) /*[*/
		if (*target == XA_UTF8_STRING(display))
			(void) memmove(*value, own_sel[i].buffer,
				       (int) *length);
		else
#endif /*]*/
		    	/*
			 * N.B.: We return a STRING for COMPOUND_TEXT.
			 * Someday we may do real ISO 2022, but not today.
			 */
		    	*length = store_icccm_string(*value,
				own_sel[i].buffer);
		*format = 8;
		return True;
	}
	if (*target == XA_LIST_LENGTH(display)) {
		*value = XtMalloc(4);
		if (sizeof(long) == 4)
			*(long *)*value = 1;
		else {
			long temp = 1;
			(void) memmove(*value,
					((char*) &temp) + sizeof(long) - 4,
					4);
		}
		*type = XA_INTEGER;
		*length = 1;
		*format = 32;
		return True;
	}
	if (*target == XA_LENGTH(display)) {
		*value = XtMalloc(4);
		if (sizeof(long) == 4)
			*(long*)*value = strlen(own_sel[i].buffer);
		else {
			long temp = strlen(own_sel[i].buffer);
			(void) memmove(*value,
			                   ((char *) &temp) + sizeof(long) - 4,
					   4);
		}
		*type = XA_INTEGER;
		*length = 1;
		*format = 32;
		return True;
	}

	if (XmuConvertStandardSelection(w, sel_time, selection,
	    target, type, (caddr_t *)value, length, format))
		return True;

	/* else */
#if 0
	printf("Unknown conversion request: %s to %s\n",
	    XGetAtomName(display, *selection),
	    XGetAtomName(display, *target));
#endif
	return False;
}

static void
lose_sel(Widget w _is_unused, Atom *selection)
{
	int i;

	for (i = 0; i < NS; i++)
		if (own_sel[i].atom != None && own_sel[i].atom == *selection) {
			own_sel[i].atom = None;
			XtFree(own_sel[i].buffer);
			own_sel[i].buffer = CN;
			n_owned--;
			break;
		}
	if (!n_owned)
		unselect(0, ROWS*COLS);
}

/*
 * Somewhat convoluted logic to return an ASCII character for a given screen
 * position.
 *
 * The character has to be found indirectly from ea_buf and the field
 * attirbutes, so that zero-intensity fields become blanks.
 */
static Boolean osc_valid = False;

static void
osc_start(void)
{
	osc_valid = False;
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
#if defined(X3270_DBCS) /*[*/
	int baddr2;
#endif /*]*/

	*rlen = 1;

	/* If we aren't moving forward, all bets are off. */
	if (osc_valid && baddr < osc_baddr)
		osc_valid = False;

	if (osc_valid) {
		/*
		 * Search for a new field attribute between the address we
		 * want and the last address we searched.  If we found a new
		 * field attribute, save the address for next time.
		 */
		(void) get_bounded_field_attribute(baddr, osc_baddr, &fa);
		osc_baddr = baddr;
	} else {
		/*
		 * Find the attribute the old way.
		 */
		fa = get_field_attribute(baddr);
		osc_baddr = baddr;
		osc_valid = True;
	}

	/* If it isn't visible, then make it a blank. */
	if (FA_IS_ZERO(fa)) {
		*r = ' ';
		return;
	}

#if defined(X3270_DBCS) /*[*/
	/* Handle DBCS. */
	switch (ctlr_dbcs_state(baddr)) {
	case DBCS_LEFT:
	    baddr2 = baddr;
	    INC_BA(baddr2);
	    uc = ebcdic_to_unicode((ea_buf[baddr].cc << 8) | ea_buf[baddr2].cc,
		    CS_BASE, False);
	    *rlen = unicode_to_utf8(uc, (char *)r);
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
#endif /*]*/

	switch (ea_buf[baddr].cs) {
	    case CS_BASE:
	    default:
		switch (ea_buf[baddr].cc) {
#if defined(X3270_DBCS) /*[*/
		    case EBC_so:
			/*
			 * Suppress SO's altogether.  They'll expand back on
			 * paste.
			 */
			*rlen = 0;
			return;
#endif /*]*/
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
			uc = ebcdic_base_to_unicode(ea_buf[baddr].cc, True,
				True);
			*rlen = unicode_to_utf8(uc, (char *)r);
			if (*rlen < 0)
			    	*rlen = 0;
			return;
		}
	    case CS_GE:
		switch (ea_buf[baddr].cc) {
		    case EBC_null:
			*r = 0;
			return;
		    case EBC_Yacute:
			*r = '[';
			return;
		    case EBC_diaeresis:
			*r = ']';
			return;
		    default:
			/* Translate APL to Unicode. */
			uc = apl_to_unicode(ea_buf[baddr].cc);
			if (uc == (ucs4_t)-1 ||
			    (appres.apl_mode && (uc < 0x100))) {
			    	/*
				 * No translation, or we're in APL mode and the
				 * GE character maps back onto a non-GE
				 * character.  Use private-use characters.
				 */
				uc = UPRIV_GE_00 + ea_buf[baddr].cc;
			}
			*rlen = unicode_to_utf8(uc, (char *)r);
			if (*rlen < 0)
				*rlen = 0;
			return;
		}
	    case CS_LINEDRAW:
		/* vt100 line-drawing character */
		*r = ea_buf[baddr].cc + 0x5f;
		return;
	}
}

/*
 * Attempt to own the selections in want_sel[].
 */
static void
own_sels(Time t)
{
	register int i, j;

	/*
	 * Try to grab any new selections we may want.
	 */
	for (i = 0; i < NS; i++) {
		Boolean already_own = False;

		if (want_sel[i] == None)
			continue;

		/* Check if we already own it. */
		for (j = 0; j < NS; j++)
			if (own_sel[j].atom == want_sel[i]) {
				already_own = True;
				break;
			}

		/* Find the slot for it. */
		if (!already_own) {
			for (j = 0; j < NS; j++)
				if (own_sel[j].atom == None)
					break;
			if (j >= NS)
				continue;
		}

		if (XtOwnSelection(*screen, want_sel[i], t, convert_sel,
		    lose_sel, NULL)) {
			if (!already_own) {
				n_owned++;
				own_sel[j].atom = want_sel[i];
			}
			Replace(own_sel[j].buffer,
			    XtMalloc(strlen(select_buf) + 1));
			(void) memmove(own_sel[j].buffer, select_buf,
			    strlen(select_buf) + 1);
		} else {
			XtWarning("Could not get selection");
			if (own_sel[j].atom != None) {
				XtFree(own_sel[j].buffer);
				own_sel[j].buffer = CN;
				own_sel[j].atom = None;
				n_owned--;
			}
		}
	}
	if (!n_owned)
		unselect(0, ROWS*COLS);
	sel_time = t;
}

/*
 * Copy the selected area on the screen into a buffer and attempt to
 * own the selections in want_sel[].
 */
#define VISUAL_LEFT(d)	((IS_LEFT(d)) || ((d) == DBCS_SI))
static void
grab_sel(int start, int end, Boolean really, Time t)
{
	register int i, j;
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
	osc_start();		/* prime the onscreen_char() routine */

	if (!ever_3270 && !toggled(RECTANGLE_SELECT)) {
		/* Continuous selections */
		if (IS_RIGHT(ctlr_dbcs_state(start)))
			DEC_BA(start);
		if (VISUAL_LEFT(ctlr_dbcs_state(end)))
			INC_BA(end);
		for (i = start; i <= end; i++) {
			SET_SELECT(i);
			if (really) {
				if (i != start && !(i % COLS)) {
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
					} else
						nulls++;
				}
			}
		}
		/* Check for newline extension on the last line. */
		if ((end % COLS) != (COLS - 1)) {
			Boolean all_blank = True;

			for (i = end; i < end + (COLS - (end % COLS)); i++) {
				onscreen_char(i, osc, &len);
				for (j = 0; j < len; j++) {
					if (osc[j]) {
						all_blank = False;
						break;
					}
				}
			}
			if (all_blank) {
				for (i = end; i < end + (COLS - (end % COLS)); i++) {
					SET_SELECT(i);
				}
				if (really)
					store_sel('\n');
			}
		}
	} else {
		/* Rectangular selections */
		if (start_row == end_row) {
			if (IS_RIGHT(ctlr_dbcs_state(start)))
				DEC_BA(start);
			if (VISUAL_LEFT(ctlr_dbcs_state(end)))
				INC_BA(end);
			for (i = start; i <= end; i++) {
				SET_SELECT(i);
				if (really) {
					onscreen_char(i, osc, &len);
					for (j = 0; j < len; j++) {
						if (osc[j]) {
							while (nulls) {
								store_sel(' ');
								nulls--;
							}
							store_sel((char)osc[j]);
						} else
							nulls++;
					}
				}
			}
			if (really && (end % COLS == COLS - 1))
				store_sel('\n');
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

				if (sc &&
				    IS_RIGHT(ctlr_dbcs_state(row*COLS + sc)))
					sc = sc - 1;
				if (ec < COLS-1 &&
				    VISUAL_LEFT(ctlr_dbcs_state(row*COLS + ec)))
					ec = ec + 1;

				for (col = sc; col <= ec; col++) {
					SET_SELECT(row*COLS + col);
					if (really) {
						onscreen_char(row*COLS + col,
						    osc, &len);
						for (j = 0; j < len; j++) {
							if (osc[j]) {
								while (nulls) {
									store_sel(' ');
									nulls--;
								}
								store_sel((char)osc[j]);
							} else
								nulls++;
						    }
					}
				}
				nulls = 0;
				if (really)
					store_sel('\n');
			}
		}
	}

	/* Terminate the result. */
	if (really)
		store_sel('\0');

	any_selected = True;
	ctlr_changed(0, ROWS*COLS);
	if (really)
		own_sels(t);
}

/*
 * Check if any character in a given region is selected.
 */
Boolean
area_is_selected(int baddr, int len)
{
	register int i;

	for (i = 0; i < len; i++)
		if (SELECTED(baddr+i))
			return True;
	return False;
}

/*
 * Unhighlight the region of selected text -- but don't give up the selection.
 */
void
unselect(int baddr _is_unused, int len _is_unused)
{
	if (any_selected) {
		(void) memset((char *) selected, 0, (ROWS*COLS + 7) / 8);
		ctlr_changed(0, ROWS*COLS);
		any_selected = False;
	}
}

/* Selection insertion. */
#define NP	5
static Atom	paste_atom[NP];
static int	n_pasting = 0;
static int	pix = 0;
static Time	paste_time;
#if defined(XA_UTF8_STRING) /*[*/
static Boolean	paste_utf8;
#endif /*]*/

static void
paste_callback(Widget w, XtPointer client_data _is_unused, Atom *selection _is_unused,
    Atom *type _is_unused, XtPointer value, unsigned long *length,
    int *format _is_unused)
{
	char *s, *t;
	unsigned long s_len, t_len;
	char *ei_buf;
	int ei_len;

	if ((value == NULL) || (*length == 0)) {
		XtFree(value);

		/* Try the next one. */
#if defined(XA_UTF8_STRING) /*[*/
		if (paste_utf8) {
		    	paste_utf8 = False;
			XtGetSelectionValue(w, paste_atom[pix], XA_STRING,
				paste_callback, NULL, paste_time);
		} else
#endif /*]*/
		if (n_pasting > pix) {
#if defined(XA_UTF8_STRING) /*[*/
		    	paste_utf8 = True;
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

	/* Convert the selection to local multibyte. */
	t_len = 2 * *length;
	t = ei_buf = Malloc(t_len);
	s_len = *length;
	s = (char *)value;
	ei_len = 0;

	while (s_len) {
	    	ucs4_t uc;
		int nm;

#if defined(XA_UTF8_STRING) /*[*/
	    	if (paste_utf8) {
		    	int nu;

		    	nu = utf8_to_unicode(s, s_len, &uc);
			if (nu <= 0)
			    	break;
			s += nu;
			s_len -= nu;
		} else
#endif /*]*/
		{
		    	/* ISO 8859-1. */
		    	uc = *s & 0xff;
			s++;
			s_len--;
		}
		nm = unicode_to_multibyte(uc, t, t_len);
		if (nm > 0)
			nm--;
		t += nm;
		t_len -= nm;
		ei_len += nm;
	}
	(void) emulate_input(ei_buf, ei_len, True);

	XtFree(ei_buf);
	XtFree(value);

	n_pasting = 0;
}

void
insert_selection_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	Cardinal i;
	Atom	a;
	XButtonEvent *be = (XButtonEvent *)event;

	action_debug(insert_selection_action, event, params, num_params);

	if (event == NULL) {
	    	popup_an_error("%s must be called from a keymap\n",
			action_name(insert_selection_action));
		return;
	}

	n_pasting = 0;
	for (i = 0; i < *num_params; i++) {
		a = XInternAtom(display, params[i], True);
		if (a == None) {
			popup_an_error("%s: No atom for selection",
			    action_name(insert_selection_action));
			continue;
		}
		if (n_pasting < NP)
			paste_atom[n_pasting++] = a;
	}
	pix = 0;
#if defined(XA_UTF8_STRING) /*[*/
	paste_utf8 = True;
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
