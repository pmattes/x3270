/*
 * Copyright (c) 1993-2025 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	nvt_gui.c
 *		X11-specific functions for NVT mode.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/Xlib.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include "appres.h"
#include "ctlrc.h"
#include "model.h"
#include "nvt_gui.h"
#include "screen.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "xactions.h"
#include "xappres.h"
#include "xscreen.h"
#include "xtwinops.h"

/**
 * xterm text escape
 *
 * @param[in] code	Operation to perform
 * @param[in] text	Associated text
 */
void
xterm_text_gui(unsigned short code, const char *text)
{
    switch (code) {
    case 0:	/* icon name and window title */
	XtVaSetValues(toplevel, XtNiconName, text, NULL);
	screen_set_title(text);
	break;
    case 1:	/* icon name */
	XtVaSetValues(toplevel, XtNiconName, text, NULL);
	break;
    case 2:	/* window_title */
	screen_set_title(text);
	break;
    case 50:	/* font */
	screen_newfont(text, False, False);
	break;
    default:
	break;
    }
}

/**
 * xterm window operations
 *
 * @param[in] p1	Parameter 1
 * @param[in] p2	Parameter 2
 * @param[in] p3	Parameter 3
 * @param[out] rp1	Return parameter 1
 * @param[out] rp2	Return parameter 2
 * @param[out] rtext	Return text parameter
 */
void
xtwinops(unsigned short p1, unsigned short *p2, unsigned short *p3, unsigned short *rp1, unsigned short *rp2, const char **rtext)
{
    *rp1 = 0;
    *rp2 = 0;
    *rtext = NULL;
    Position x, y;
    Dimension width, height;
    long op;

    switch (p1) {
    case XTW_1DEICONIFY: /* de-iconify */
	if (iconic) {
	    send_wmgr("NVT restore window",
		    XInternAtom(display, "_NET_ACTIVE_WINDOW", False),
		    2L,
		    CurrentTime,
		    0L);
	}
	break;
    case XTW_2ICONIFY: /* iconify */
	if (!iconic) {
	    XIconifyWindow(display, XtWindow(toplevel), DefaultScreen(display));
	}
	break;
    case XTW_3MOVE: /* move to x,y */
	if (!(maximized | fullscreen) && !iconic) {
	    XMoveWindow(display, XtWindow(toplevel), *p2, *p3);
	}
	break;
    case XTW_4RESIZE_PIXELS: /* resize to h,w pixels */
	if ((p2 && *p2 == 0) || (p3 && *p3 == 0)) {
	    /* We don't do the 'use the screen' thing, at least not yet. */
	    return;
	}
	if (!xappres.fixed_size && !(maximized | fullscreen) && !iconic) {
	    XtVaGetValues(toplevel, XtNx, &x, XtNy, &y, NULL);
	    XResizeWindow(display, XtWindow(toplevel), p2? *p2: height, p3? *p3: width);
	}
	break;
    case XTW_5RAISE: /* raise */
	XRaiseWindow(display, XtWindow(toplevel));
	break;
    case XTW_6LOWER: /* lower */
	XLowerWindow(display, XtWindow(toplevel));
	break;
    case XTW_7REFRESH: /* refresh */
	xaction_internal(PA_Expose_xaction, IA_REDRAW, NULL, NULL);
	break;
    case XTW_8RESIZE_CHARACTERS: /* set screen size to h,w characters */
	if ((p2 && *p2 == 0) || (p3 && *p3 == 0)) {
	    /* We don't do the 'use the screen' thing, at least not yet. */
	    return;
	}
	if (!xappres.fixed_size && !(maximized | fullscreen) && !iconic) {
	    live_change_oversize(p3? *p3: COLS, p2? *p2: ROWS);
	}
	break;
    case XTW_9MAXIMIZE: /* restore (0) or maximize (1) */
	if (!iconic && (((!p2 || *p2 == XTW_9MAXIMIZE_0RESET) && maximized) || (p2 && *p2 == XTW_9MAXIMIZE_1SET && !maximized))) {
	    send_wmgr("NVT maximize/unmaximize", a_net_wm_state, (p2 && *p2 == XTW_9MAXIMIZE_1SET)? NWS_ADD: NWS_REMOVE,
		    a_net_wm_state_maximized_horz, a_net_wm_state_maximized_vert);
	}
	break;
    case XTW_10FULLSCREEN: /* undo full-screen (0), full-screen (1), toggle full-screen (2): */
	if (iconic) {
	    return;
	}
	op = NWS_REMOVE;
	switch (p2? *p2: 0) {
	case XTW_10FULLSCREEN_0RESET: /* undo full-screen */
	    op = NWS_REMOVE;
	    break;
	case XTW_10FULLSCREEN_1SET: /* full-screen */
	    op = NWS_ADD;
	    break;
	case XTW_10FULLSCREEN_2TOGGLE: /* toggle full-screen */
	    op = NWS_TOGGLE;
	    break;
	default:
	    return;
	}
	send_wmgr("NVT fullscreen/unfullscreen", a_net_wm_state, op, a_net_wm_state_fullscreen, 0L);
	break;
    case XTWR_11WINDOWSTATE: /* report window state */
	*rp1 = iconic? XTW_2ICONIFY: XTW_1DEICONIFY;
	break;
    case XTWR_13WINDOWPOSITION: /* report window position */
	XtVaGetValues(toplevel, XtNx, &x, XtNy, &y, NULL);
	*rp1 = x;
	*rp2 = y;
	break;
    case XTWR_14WINDOWSIZE_PIXELS:
	if (!p2 || *p2 == XTWR_14WINDOWSIZE_PIXELS_0TEXTAREA) {
	    /* report text area size in pixels */
	    *rp1 = maxROWS * *char_height;
	    *rp1 = maxCOLS * *char_width;
	} else if (p2 && *p2 == XTWR_14WINDOWSIZE_PIXELS_2WINDOW) {
	    /* report window size in pixels */
	    XtVaGetValues(toplevel, XtNheight, &height, XtNwidth, &width, NULL);
	    *rp1 = height;
	    *rp2 = width;
	}
	break;
    case XTWR_15SCREENSIZE_PIXELS: /* report screen size in pixels */
	height = DisplayHeight(display, DefaultScreen(display));
	width = DisplayWidth(display, DefaultScreen(display));
	*rp1 = height;
	*rp2 = width;
	break;
    case XTWR_16CHARACTERSIZE_PIXELS: /* report character cell size in pixels */
	*rp1 = *char_height;
	*rp2 = *char_width;
	break;
    case XTWR_19SCREENSIZE_PIXELS: /* report screen size in characters */
	height = DisplayHeight(display, DefaultScreen(display)) / *char_height;
	width = DisplayWidth(display, DefaultScreen(display)) / *char_width;
	*rp1 = height;
	*rp2 = width;
	break;
    case XTWR_20ICONLABEL: /* report window label */
	XtVaGetValues(toplevel, XtNtitle, rtext, NULL);
	break;
    case XTWR_21WINDOWLABEL: /* report icon label */
	XtVaGetValues(toplevel, XtNiconName, rtext, NULL);
	break;
    default:
	if (!xappres.fixed_size && !(maximized | fullscreen) && !iconic) {
	    live_change_oversize(COLS, p1);
	}
	break;
    }
}

/* Query() callbacks. */

void
get_screen_pixels(unsigned *height, unsigned *width)
{
    *height = DisplayHeight(display, DefaultScreen(display));
    *width = DisplayWidth(display, DefaultScreen(display));
}

void
get_window_pixels(unsigned *height, unsigned *width)
{
    Dimension xwidth, xheight;

    XtVaGetValues(toplevel, XtNheight, &xheight, XtNwidth, &xwidth, NULL);
    *height = xheight;
    *width = xwidth;
}

void
get_character_pixels(unsigned *height, unsigned *width)
{
    *height = *char_height;
    *width = *char_width;
}

void
get_window_location(int *x, int *y)
{
    Position winx, winy;

    XtVaGetValues(toplevel, XtNx, &winx, XtNy, &winy, NULL);
    *x = winx;
    *y = winy;
}

window_state_t
get_window_state(void)
{
    return iconic? WS_ICONIFIED: (fullscreen? WS_FULLSCREEN: (maximized? WS_MAXIMIZED: WS_NORMAL));
}
