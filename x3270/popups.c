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
 *	popups.c
 *		This module handles pop-up dialogs: errors, host names,
 *		font names, information.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Text.h>
#include "objects.h"
#include "appres.h"

#include "actions.h"
#include "host.h"
#include "names.h"
#include "popups.h" /* must come before child_popups.h */
#include "child_popups.h"
#include "resources.h"
#include "screen.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "xio.h"
#include "xmenubar.h"
#include "xpopups.h"
#include "xscreen.h"

typedef enum {
    WMT_ROOT,	/* No windows added by wmgr */
    WMT_SIMPLE,	/* One window added by wmgr */
    WMT_TRANS,	/* Two windows added by wmgr */
    WMT_UNKNOWN	/* Three or more window added -- mystery */
} wm_type_t;

const char *popup_separator = "\n";

static enum form_type forms[] = { FORM_NO_WHITE, FORM_NO_CC, FORM_AS_IS };

static Dimension wm_width, wm_height;

static ioid_t info_id = NULL_IOID;

static struct {		/* Error pop-up delay: */
    bool active;	/*  Is it active? */
    char *text;		/*  Saved text */
    pae_t type;		/*  Error type */
} epd = { true, NULL, ET_OTHER };

/*
 * General popup support
 */

/* Find the parent of a window */
static Window
parent_of(Window w)
{
    Window root, parent, *wchildren;
    unsigned int nchildren;

    XQueryTree(display, w, &root, &parent, &wchildren, &nchildren);
    XFree((char *)wchildren);
    return parent;
}

static Window
root_of(Window w)
{
    Window root, parent, *wchildren;
    unsigned int nchildren;

    XQueryTree(display, w, &root, &parent, &wchildren, &nchildren);
    XFree((char *)wchildren);
    return root;
}

/*
 * Find the base window (the one with the wmgr decorations) and the virtual
 * root, so we can pop up a window relative to them.
 */
void
toplevel_geometry(Position *x, Position *y, Dimension *width,
	Dimension *height)
{
    Window tlw = XtWindow(toplevel);
    Window win;
    Window parent;
    int nw;
    struct {
	Window w;
	XWindowAttributes wa;
    } ancestor[10];
    XWindowAttributes wa, *base_wa, *root_wa;

    /*
     * Trace the family tree of toplevel.
     */
    for (win = tlw, nw = 0; ; win = parent) {
	parent = parent_of(win);
	ancestor[nw].w = parent;
	XGetWindowAttributes(display, parent, &ancestor[nw].wa);
	++nw;
	if (parent == root_window) {
	    break;
	}
    }

    /*
     * Figure out if they're running a virtual desktop, by seeing if
     * the 1st child of root is bigger than it is.  If so, pretend that
     * the virtual desktop is the root.
     */
    if (nw > 1 &&
	(ancestor[nw-2].wa.width > ancestor[nw-1].wa.width ||
	 ancestor[nw-2].wa.height > ancestor[nw-1].wa.height)) {
	--nw;
    }
    root_wa = &ancestor[nw-1].wa;

    /*
     * Now identify the base window as the window below the root
     * window.
     */
    if (nw >= 2) {
	base_wa = &ancestor[nw-2].wa;
    } else {
	XGetWindowAttributes(display, tlw, &wa);
	base_wa = &wa;
    }

    *x = base_wa->x + root_wa->x;
    *y = base_wa->y + root_wa->y;
    *width = base_wa->width + 2*base_wa->border_width;
    *height = base_wa->height + 2*base_wa->border_width;
}

/* Figure out the window manager type. */
wm_type_t
get_wm_type(Window w)
{
    Window root = root_of(w);

    if (parent_of(w) == root) {
	return WMT_ROOT;
    }

    if (parent_of(parent_of(w)) == root) {
	return WMT_SIMPLE;
    }

    if (parent_of(parent_of(parent_of(w))) == root) {
	return WMT_TRANS;
    }

#if defined(POPUP_DEBUG) /*[*/
    printf("Unknown window manager type -- three or more windows added\n");
#endif /*]*/
	
    return WMT_UNKNOWN;
}

/* Pop up a popup shell */
void
popup_popup(Widget shell, XtGrabKind grab)
{
    XtPopup(shell, grab);
    XSetWMProtocols(display, XtWindow(shell), &a_delete_me, 1);
}

static enum placement CenterD = Center;
enum placement *CenterP = &CenterD;
static enum placement BottomD = Bottom;
enum placement *BottomP = &BottomD;
static enum placement LeftD = Left;
enum placement *LeftP = &LeftD;
static enum placement RightD = Right;
enum placement *RightP = &RightD;
static enum placement InsideRightD = InsideRight;
enum placement *InsideRightP = &InsideRightD;

typedef struct want {
    struct want *next;
    Widget w;
    Position x, y;
    enum placement p;
    XtIntervalId timeout_id;
} want_t;
static want_t *wants;

/* Dequeue a pending move_again operation and free its context. */
static void
dequeue_and_free_want(want_t *wx)
{
    want_t *wp, *wp_prev = NULL;

    for (wp = wants; wp != NULL; wp = wp->next) {
	if (wp == wx) {
	    if (wp_prev != NULL) {
		wp_prev->next = wp->next;
	    } else {
		wants = wp->next;
	    }
	    break;
	}
	wp_prev = wp;
    }
    XtFree((XtPointer)wx);
}

static void
popup_move_again(XtPointer closure, XtIntervalId *id _is_unused)
{
    want_t *wx = (want_t *)closure;
    Position x, y;

    XtVaGetValues(wx->w, XtNx, &x, XtNy, &y, NULL);
#if defined(POPUP_DEBUG) /*[*/
    printf("popup_move_again: want x=%d got x=%d, want y=%d, got y=%d\n",
	    wx->x, x,
	    wx->y, y);
#endif /*]*/

    if (x != wx->x || y != wx->y) {
	Position tl_x, tl_y;
	Dimension tl_width, tl_height;
	Dimension popup_width;

	/*
	 * The position has been shifted down and to the right by
	 * the Window Manager.  The amound of the shift is the width
	 * of the Window Manager decorations.  We can use these to
	 * figure out the correct location of the pop-up.
	 */
	wm_width = x - wx->x;
	wm_height = y - wx->y;
#if defined(POPUP_DEBUG) /*[*/
	printf("popup_move_again: wm width=%u height=%u\n", wm_width, wm_height);
#endif /*]*/

	XtVaGetValues(toplevel, XtNx, &tl_x, XtNy, &tl_y,
		XtNwidth, &tl_width, XtNheight, &tl_height, NULL);

	switch (wx->p) {
	case Bottom:
	    x = tl_x - wm_width;
	    y = tl_y + tl_height + wm_width;
	    break;
	case Left:
	    XtVaGetValues(wx->w, XtNwidth, &popup_width, NULL);
	    x = tl_x - (3 * wm_width) - popup_width;
	    y = tl_y - wm_height;
	    break;
	case Right:
	    x = tl_x + wm_width + tl_width;
	    y = tl_y - wm_height;
	    break;
	case InsideRight:
	    XtVaGetValues(wx->w, XtNwidth, &popup_width, NULL);
	    x = tl_x - (2 * wm_width) + tl_width - popup_width;
	    y = tl_y + menubar_qheight(tl_width);
	    break;
	default:
	    return;
	}

#if defined(POPUP_DEBUG) /*[*/
	printf("popup_move_again:  re-setting x=%d y=%d\n", x, y);
#endif /*]*/
	XtVaSetValues(wx->w, XtNx, x, XtNy, y, NULL);
    }

    /* Dequeue and free. */
    dequeue_and_free_want(wx);
}

/* Place a newly popped-up shell */
void
place_popup(Widget w, XtPointer client_data, XtPointer call_data _is_unused)
{
    wm_type_t wm_type;
    Dimension width, height;
    Position x = 0, y = 0;
    Position xnew, ynew;
    Dimension win_width, win_height;
    Dimension popup_width, popup_height;
    enum placement p = *(enum placement *)client_data;
    XWindowAttributes twa, pwa;
    want_t *wx = NULL;

    /* Get and fix the popup's dimensions */
    XtRealizeWidget(w);
    XtVaGetValues(w,
	    XtNwidth, &width,
	    XtNheight, &height,
	    NULL);
    XtVaSetValues(w,
	    XtNheight, height,
	    XtNwidth, width,
	    XtNbaseHeight, height,
	    XtNbaseWidth, width,
	    XtNminHeight, height,
	    XtNminWidth, width,
	    XtNmaxHeight, height,
	    XtNmaxWidth, width,
	    NULL);

    XtVaGetValues(toplevel, XtNx, &x, XtNy, &y, XtNwidth, &win_width,
	    XtNheight, &win_height, NULL);
    if (x < 0 || y < 0) {
	return;
    }

    wm_type = get_wm_type(XtWindow(w));

#if defined(POPUP_DEBUG) /*[*/
    printf("place_popup: toplevel x=%d y=%d width=%u height=%u\n",
	    x, y, win_width, win_height);
#endif /*]*/

    switch (wm_type) {
    case WMT_ROOT:
#if defined(POPUP_DEBUG) /*[*/
	printf("place_popup: parent is root\n");
#endif /*]*/
	break;
    default:
    case WMT_SIMPLE:
	XGetWindowAttributes(display, parent_of(XtWindow(toplevel)), &twa);
	break;
    case WMT_TRANS:
	XGetWindowAttributes(display, parent_of(XtWindow(toplevel)), &pwa);
	break;
    }

    switch (p) {
    case Center:
	XtVaGetValues(w,
		XtNwidth, &popup_width,
		XtNheight, &popup_height,
		NULL);
#if defined(POPUP_DEBUG) /*[*/
	printf("place_popup: Center: popup width=%u height=%u\n",
		popup_width, popup_height);
#endif /*]*/
	xnew = x + (win_width-popup_width) / (unsigned) 2;
	if (xnew < 0) {
	    xnew = 0;
	}
	ynew = y + (win_height-popup_height) / (unsigned) 2;
	if (ynew < 0) {
	    ynew = 0;
	}
#if defined(POPUP_DEBUG) /*[*/
	printf("place_popup: Center: setting x=%d y=%d\n", xnew, ynew);
#endif /*]*/
	XtVaSetValues(w, XtNx, xnew, XtNy, ynew, NULL);
	break;
    case Bottom:
	switch (wm_type) {
	case WMT_ROOT:
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    /* Measure what the window manager does. */
	    wx = (want_t *)XtMalloc(sizeof(want_t));
	    wx->w = w;
	    wx->x = x;
	    wx->y = y;
	    wx->p = p;
	    wx->timeout_id = XtAppAddTimeOut(appcontext, 250, popup_move_again,
		    (XtPointer)wx);
	    wx->next = wants;
	    wants = wx;
	    break;
	default:
	case WMT_SIMPLE:
	    /* Do it precisely. */
	    x = twa.x;
	    y = twa.y + twa.height;
#if defined(POPUP_DEBUG) /*[*/
	    printf("setting x %d y %d\n", x, y);
#endif /*]*/
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    break;
	}
	break;
    case Left:
	switch (wm_type) {
	case WMT_ROOT:
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    wx = (want_t *)XtMalloc(sizeof(want_t));
	    wx->w = w;
	    wx->x = x;
	    wx->y = y;
	    wx->p = p;
	    wx->timeout_id = XtAppAddTimeOut(appcontext, 250, popup_move_again,
		    (XtPointer)wx);
	    wx->next = wants;
	    wants = wx;
	    break;
	default:
	case WMT_SIMPLE:
	    XtVaGetValues(w, XtNwidth, &popup_width, NULL);
	    x = twa.x - popup_width - (twa.width - main_width);
	    y = twa.y;
#if defined(POPUP_DEBUG) /*[*/
	    printf("place_popup: setting x=%d y=%d\n", x, y);
#endif /*]*/
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    break;
	case WMT_TRANS:
	    XtVaGetValues(w, XtNwidth, &popup_width, NULL);
	    x = x - popup_width - (2 * pwa.x);
	    y = y - pwa.y;
#if defined(POPUP_DEBUG) /*[*/
	    printf("place_popup: setting x=%d y=%d\n", x, y);
#endif /*]*/
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    break;
	}
	break;
    case Right:
	switch (wm_type) {
	case WMT_ROOT:
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    wx = (want_t *)XtMalloc(sizeof(want_t));
	    wx->w = w;
	    wx->x = x;
	    wx->y = y;
	    wx->p = p;
	    wx->timeout_id = XtAppAddTimeOut(appcontext, 250, popup_move_again,
		    (XtPointer)wx);
	    wx->next = wants;
	    wants = wx;
	    break;
	default:
	case WMT_SIMPLE:
	    x = twa.x + twa.width;
	    y = twa.y;
#if defined(POPUP_DEBUG) /*[*/
	    printf("place_popup: setting x=%d y=%d\n", x, y);
#endif /*]*/
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    break;
	case WMT_TRANS:
	    x = x + win_width + (2 * pwa.x);
	    y = y - pwa.y;
#if defined(POPUP_DEBUG) /*[*/
	    printf("place_popup: setting x=%d y=%d\n", x, y);
#endif /*]*/
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    break;
	}
	break;
    case InsideRight:
	switch (wm_type) {
	case WMT_ROOT:
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	    wx = (want_t *)XtMalloc(sizeof(want_t));
	    wx->w = w;
	    wx->x = x;
	    wx->y = y;
	    wx->p = p;
	    wx->timeout_id = XtAppAddTimeOut(appcontext, 250, popup_move_again,
		    (XtPointer)wx);
	    wx->next = wants;
	    wants = wx;
	    break;
	default:
	case WMT_SIMPLE:
	    XtVaGetValues(w, XtNwidth, &popup_width, NULL);
	    x = twa.x + win_width - popup_width;
	    y = twa.y + menubar_qheight(win_width) + (y - twa.y);
#if defined(POPUP_DEBUG) /*[*/
	    printf("place_popup: setting x=%d y=%d\n", x, y);
#endif /*]*/
	    XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	}
	break;
    }
}

/* Cancel a pending pop-up placement. */
void
unplace_popup(Widget w)
{
    want_t *wx;

    /* Cancel any pending move_again activity. */
    for (wx = wants; wx != NULL; wx = wx->next) {
	if (wx->w == w) {
	    XtRemoveTimeOut(wx->timeout_id);
	    dequeue_and_free_want(wx);
	    return;
	}
    }
}

#if defined(POPUP_DEBUG) /*[*/
static void
dump_windows(char *what, Widget w)
{
    Position x = 0, y = 0;
    Dimension win_width, win_height;

    XtVaGetValues(w, XtNx, &x, XtNy, &y, XtNwidth, &win_width,
	    XtNheight, &win_height, NULL);
    printf("%s [abs] x=%d y=%d width=%u height=%u\n",
	    what, x, y, win_width, win_height);
    {
	Window win = XtWindow(w);
	int i = 0;

	while (win != root_of(XtWindow(w))) {
	    XWindowAttributes wx;

	    XGetWindowAttributes(display, win, &wx);
	    printf("%s [rel] #%d x=%d y=%d width=%u height=%u\n",
		    what, i, wx.x, wx.y, wx.width, wx.height);
	    win = parent_of(win);
	    i++;
	}
    }
}
#endif /*]*/

/*
 * Most window managers put one window behind each window they control:
 *  An inserted window is the size of the app window plus the decorations. Its
 *   coordinates are absolute (it is on the root window).
 *  The app window is offset by the dimensions of the decorations.
 *
 * Unity puts two windows behind each window it controls:
 *  A transparent resize window is the size of the app window, plus the
 *   decorations, plus (if the window is resizable) a 10-pixel resize area. Its
 *   coordinates are absolute (it is on the root window).
 *  A second window is offset by the size of the decorations and optional
 *   resize area. It is the same size as the app window.
 *  The app window has no offset. (This is a signature of Unity, as is
 *   $XDG_CURRENT_DESKTOP == Unity.)
 *
 * On non-Unity, the correct y coordinate for a Right-side pop-up window is
 * the absolute y coordinate of the toplevel window. On Unity, the toplevel
 * window is resizable, while the pop-up is not, so the window manager shifts
 * them over different amounts. So the correct y coordinate for a Right-side
 * pop-up window is the absolute y coordinate of the toplevel window, plus the
 * 10-pixel resize thickness, which can be inferred from the x offset of the
 * parent of the toplevel window.
 */

/* Move an existing popped-up shell */
void
move_popup(Widget w, XtPointer client_data, XtPointer call_data _is_unused)
{
    wm_type_t wm_type = get_wm_type(XtWindow(w));
    Position x = 0, y = 0;
    Position xnew, ynew;
    Dimension win_width, win_height;
    Dimension popup_width, popup_height;
    enum placement p = *(enum placement *)client_data;
    XWindowAttributes twa; /* toplevel parent window attributes */
    XWindowAttributes pwa; /* popup parent */

#if defined(POPUP_DEBUG) /*[*/
    printf("\n");
    dump_windows("popup", w);
    dump_windows("toplevel", toplevel);
#endif /*]*/

    XtVaGetValues(toplevel, XtNx, &x, XtNy, &y, XtNwidth, &win_width,
	    XtNheight, &win_height, NULL);

    switch (wm_type) {
    case WMT_ROOT:
	/* Fake the parent window attributes. */
#if defined(POPUP_DEBUG) /*[*/
	printf("move_popup: parent is root\n");
#endif /*]*/
	twa.x = x - wm_width;
	twa.y = y - wm_height;
	twa.width = win_width + (2 * wm_width);
	twa.height = win_height + wm_height + wm_width;
	break;
    default:
    case WMT_SIMPLE:
	XGetWindowAttributes(display, parent_of(XtWindow(toplevel)), &twa);
	break;
    case WMT_TRANS:
	XGetWindowAttributes(display, parent_of(XtWindow(w)), &pwa);
	break;
    }

    switch (p) {
    case Center:
	XtVaGetValues(w,
		XtNwidth, &popup_width,
		XtNheight, &popup_height,
		NULL);
	xnew = x + (win_width-popup_width) / (unsigned) 2;
	if (xnew < 0) {
	    xnew = 0;
	}
	ynew = y + (win_height-popup_height) / (unsigned) 2;
	if (ynew < 0) {
	    ynew = 0;
	}
#if defined(POPUP_DEBUG) /*[*/
	printf("move_popup: Center: setting x=%d y=%d\n", xnew, ynew);
#endif /*]*/
	XtVaSetValues(w, XtNx, xnew, XtNy, ynew, NULL);
	break;
    case Bottom:
	if (wm_type == WMT_TRANS) {
	    /* x is unchanged */
	    y = y + win_height;
	} else {
	    x = twa.x;
	    y = twa.y + twa.height;
	}
#if defined(POPUP_DEBUG) /*[*/
	printf("move_popup: Bottom: setting x=%d y=%d\n", x, y);
#endif /*]*/
	XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	break;
    case Left:
	XtVaGetValues(w, XtNwidth, &popup_width, NULL);
	if (wm_type == WMT_TRANS) {
	    x = x - popup_width - (2 * pwa.x);
	    y = y - pwa.y;
	} else {
	    x = twa.x - popup_width - (twa.width - main_width);
	    y = twa.y;
	}
#if defined(POPUP_DEBUG) /*[*/
	printf("move_popup: Left: setting x=%d y=%d\n", x, y);
#endif /*]*/
	XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	break;
    case Right:
	if (wm_type == WMT_TRANS) {
	    x = x + win_width + (2 * pwa.x);
	    y = y - pwa.y;
	} else {
	    x = twa.x + twa.width;
	    y = twa.y;
	}
#if defined(POPUP_DEBUG) /*[*/
	printf("move_popup: Right: setting x=%d y=%d\n", x, y);
#endif /*]*/
	XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	break;
    case InsideRight:
	XtVaGetValues(w, XtNwidth, &popup_width, NULL);
	if (wm_type == WMT_TRANS) {
	    x = x + win_width - popup_width;
	    y = y - pwa.y + menubar_qheight(win_width) + (pwa.y);
	} else {
	    x = twa.x + win_width - popup_width;
	    y = twa.y + menubar_qheight(win_width) + (y - twa.y);
	}
#if defined(POPUP_DEBUG) /*[*/
	printf("move_popup: InsideRight: setting x=%d y=%d\n", x, y);
#endif /*]*/
	XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
	break;
    }
}

/* Action called when "Return" is pressed in data entry popup */
void
PA_confirm_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{
    Widget w2;

    /* Find the Confirm or Okay button */
    w2 = XtNameToWidget(XtParent(w), ObjConfirmButton);
    if (w2 == NULL) {
	w2 = XtNameToWidget(XtParent(w), ObjConfirmButton);
    }
    if (w2 == NULL) {
	w2 = XtNameToWidget(w, ObjConfirmButton);
    }
    if (w2 == NULL) {
	xs_warning("confirm: cannot find %s", ObjConfirmButton);
	return;
    }

    /* Call its "notify" event */
    XtCallActionProc(w2, "set", event, params, *num_params);
    XtCallActionProc(w2, "notify", event, params, *num_params);
    XtCallActionProc(w2, "unset", event, params, *num_params);
}

/* Callback for "Cancel" button in data entry popup */
static void
cancel_button_callback(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    XtPopdown((Widget) client_data);
}

/*
 * Callback for text source changes.  Ensures that the dialog text does not
 * contain white space -- especially newlines.
 */
static void
popup_dialog_callback(Widget w, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    static bool called_back = false;
    XawTextBlock b, nullb;	/* firstPos, length, ptr, format */
    XawTextPosition pos = 0;
    int front_len = 0;
    int end_len = 0;
    int end_pos = 0;
    int i;
    enum { FRONT, MIDDLE, END } place = FRONT;
    enum form_type *ftp = (enum form_type *)client_data;

    if (*ftp == FORM_AS_IS) {
	return;
    }

    if (called_back) {
	return;
    } else {
	called_back = true;
    }

    nullb.firstPos = 0;
    nullb.length = 0;
    nullb.ptr = NULL;

    /*
     * Scan the text for whitespace.  Leading whitespace is deleted;
     * embedded whitespace causes the rest of the text to be deleted.
     */
    while (1) {
	XawTextSourceRead(w, pos, &b, 1024);
	if (b.length <= 0) {
	    break;
	}
	nullb.format = b.format;
	if (place == END) {
	    end_len += b.length;
	    continue;
	}
	for (i = 0; i < b.length; i++) {
	    char c;

	    c = *(b.ptr + i);
	    if (isspace((unsigned char)c) && (*ftp != FORM_NO_CC || c != ' ')) {
		if (place == FRONT) {
		    front_len++;
		    continue;
		} else {
		    end_pos = b.firstPos + i;
		    end_len = b.length - i;
		    place = END;
		    break;
		}
	    } else {
		place = MIDDLE;
	    }
	}
	pos += b.length;
	if (b.length < 1024) {
	    break;
	}
    }
    if (front_len) {
	XawTextSourceReplace(w, 0, front_len, &nullb);
    }
    if (end_len) {
	XawTextSourceReplace(w, end_pos - front_len,
		end_pos - front_len + end_len, &nullb);
    }
    called_back = false;
}

/* Create a simple data entry popup */
Widget
create_form_popup(const char *name, XtCallbackProc callback,
	XtCallbackProc callback2, enum form_type form_type)
{
    char *widgetname;
    Widget shell;
    Widget dialog;
    Widget w;
    Dimension width;

    /* Create the popup shell */

    widgetname = Asprintf("%sPopup", name);
    if (isupper((unsigned char)widgetname[0])) {
	widgetname[0] = tolower((unsigned char)widgetname[0]);
    }
    shell = XtVaCreatePopupShell(
	    widgetname, transientShellWidgetClass, toplevel,
	    NULL);
    XtFree(widgetname);
    XtAddCallback(shell, XtNpopupCallback, place_popup, (XtPointer)CenterP);

    /* Create a dialog in the popup */

    dialog = XtVaCreateManagedWidget(
	    ObjDialog, dialogWidgetClass, shell,
	    XtNvalue, "",
	    NULL);
    XtVaSetValues(XtNameToWidget(dialog, XtNlabel), NULL);

    /* Add "Confirm" and "Cancel" buttons to the dialog */
    w = XtVaCreateManagedWidget(
	ObjConfirmButton, commandWidgetClass, dialog,
	NULL);
    XtAddCallback(w, XtNcallback, callback, (XtPointer)dialog);
    if (callback2) {
	w = XtVaCreateManagedWidget(
		ObjConfirm2Button, commandWidgetClass, dialog,
		NULL);
	XtAddCallback(w, XtNcallback, callback2, (XtPointer)dialog);
    }
    w = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, dialog,
	    NULL);
    XtAddCallback(w, XtNcallback, cancel_button_callback, (XtPointer)shell);

    if (form_type == FORM_AS_IS) {
	return shell;
    }

    /* Modify the translations for the objects in the dialog */

    w = XtNameToWidget(dialog, XtNvalue);
    if (w == NULL) {
	xs_warning("Cannot find \"%s\" in dialog", XtNvalue);
    }

    /* Modify the width of the value. */
    XtVaGetValues(w, XtNwidth, &width, NULL);
    XtVaSetValues(w, XtNwidth, rescale(width), NULL);

    /* Set a callback for text modifications */
    w = XawTextGetSource(w);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, popup_dialog_callback,
		&forms[(int)form_type]);
    }

    return shell;
}

/*
 * Read-only popups.
 */
struct rsm {
    struct rsm *next;
    char *text;
};
struct rop {
    const char *name;			/* resource name */
    XtGrabKind grab;			/* grab kind */
    bool is_error;			/* is it? */
    bool overwrites;			/* does it? */
    const char *itext;			/* initial text */
    Widget shell;			/* pop-up shell */
    Widget form;			/* dialog form */
    Widget cancel_button;		/* cancel button */
    abort_callback_t *cancel_callback;	/* callback for cancel button */
    bool visible;			/* visibility flag */
    bool moving;			/* move in progress */
    struct rsm *rsms;			/* stored messages */
    void (*popdown_callback)(void);	/* popdown_callback */
};

static struct rop error_popup = {
    "errorPopup", XtGrabExclusive, true, true,
    "first line\nsecond line\nthird line\nfourth line",
    NULL, NULL, NULL, NULL,
    false, false, NULL
};
static struct rop info_popup = {
    "infoPopup", XtGrabNonexclusive, false, false,
    "first line\nsecond line\nthird line",
    NULL, NULL, NULL, NULL,
    false, false, NULL
};

static struct rop printer_error_popup = {
    "printerErrorPopup", XtGrabExclusive, true, true,
    "first line\nsecond line\nthird line\nfourth line",
    NULL, NULL, NULL, NULL, false, false, NULL
};
static struct rop printer_info_popup = {
    "printerInfoPopup", XtGrabNonexclusive, false, true,
    "first line\nsecond line\nthird line\nfourth line",
    NULL,
    NULL, NULL, NULL, false, false, NULL
};

static struct rop child_error_popup = {
    "childErrorPopup", XtGrabNonexclusive, true, true,
    "first line\nsecond line\nthird line\nfourth line",
    NULL, NULL, NULL, NULL, false, false, NULL
};
static struct rop child_info_popup = {
    "childInfoPopup", XtGrabNonexclusive, false, true,
    "first line\nsecond line\nthird line\nfourth line",
    NULL,
    NULL, NULL, NULL, false, false, NULL
};

/* Called when OK is pressed in a read-only popup */
static void
rop_ok(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    struct rop *rop = (struct rop *)client_data;
    struct rsm *r;

    if ((r = rop->rsms) != NULL) {
	XtVaSetValues(rop->form, XtNlabel, r->text, NULL);
	rop->rsms = r->next;
	Free(r->text);
	Free(r);
    } else {
	XtPopdown(rop->shell);
    }
}

/* Called when Cancel is pressed in a read-only popup */
static void
rop_cancel(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    struct rop *rop = (struct rop *)client_data;

    XtPopdown(rop->shell);
    if (rop->cancel_callback != NULL) {
	(*rop->cancel_callback)();
    }
}
static void
delayed_repop(XtPointer closure, XtIntervalId *id _is_unused)
{
    struct rop *rop = (struct rop *)closure;

    rop->moving = false;
    XtPopup(rop->shell, rop->grab);
}

/* Called when a read-only popup is closed */
static void
rop_popdown(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    struct rop *rop = (struct rop *)client_data;
    void (*callback)(void);

    if (rop->moving) {
	XtAppAddTimeOut(appcontext, 250, delayed_repop, (XtPointer)rop);
	return;
    }
    rop->visible = false;
    if (exiting && rop->is_error) {
	x3270_exit(1);
    }

    callback = rop->popdown_callback;
    rop->popdown_callback = NULL;
    if (callback) {
	(*callback)();
    }
}

/* Initialize a read-only pop-up. */
static void
rop_init(struct rop *rop)
{
    Widget w;
    struct rsm *r;
    Dimension width;

    if (rop->shell != NULL) {
	return;
    }

    rop->shell = XtVaCreatePopupShell(
	    rop->name, transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(rop->shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
    XtAddCallback(rop->shell, XtNpopdownCallback, rop_popdown, rop);

    /* Create a dialog in the popup */
    rop->form = XtVaCreateManagedWidget(
	    ObjDialog, dialogWidgetClass, rop->shell,
	    NULL);
    XtVaSetValues(XtNameToWidget(rop->form, XtNlabel),
	    XtNlabel, rop->itext,
	    NULL);

    /* Add "OK" button to the dialog */
    w = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, rop->form,
	    NULL);
    XtAddCallback(w, XtNcallback, rop_ok, rop);

    /* Add an unmapped "Cancel" button to the dialog */
    rop->cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, rop->form,
	    XtNright, w,
	    XtNmappedWhenManaged, False,
	    NULL);
    XtAddCallback(rop->cancel_button, XtNcallback, rop_cancel, rop);

    /* Force it into existence so it sizes itself with 4-line text */
    XtRealizeWidget(rop->shell);

    /* Rescale the error dialogs, which have no initial value. */
    XtVaGetValues(rop->shell, XtNwidth, &width, NULL);
    XtVaSetValues(rop->shell, XtNwidth, rescale(width), NULL);

    /* If there's a pending message, pop it up now. */
    if ((r = rop->rsms) != NULL) {
	if (rop->is_error) {
	    popup_an_error("%s", r->text);
	} else {
	    popup_an_info("%s", r->text);
	}
	rop->rsms = r->next;
	Free(r->text);
	Free(r);
    }
}

/* Pop up a dialog. Common logic for all forms. */
static void
popup_rop(struct rop *rop, abort_callback_t *a, const char *buf)
{
    if (!rop->shell || (rop->visible && !rop->overwrites)) {
	struct rsm *r, **s;

	r = (struct rsm *)Malloc(sizeof(struct rsm));
	r->text = NewString(buf);
	r->next = NULL;
	for (s = &rop->rsms; *s != NULL; s = &(*s)->next) {
	}
	*s = r;
	return;
    }

    /* Put the error in the trace file. */
    if (rop->is_error) {
	vtrace("Error: %s\n", buf);
    }

    if (rop->is_error && task_redirect()) {
	task_error(buf);
	return;
    }

    XtVaSetValues(rop->form, XtNlabel, buf, NULL);
    if (a != NULL) {
	XtMapWidget(rop->cancel_button);
    } else {
	XtUnmapWidget(rop->cancel_button);
    }
    rop->cancel_callback = a;
    if (!rop->visible) {
	if (rop->is_error) {
	    ring_bell();
	}
	rop->visible = true;
	popup_popup(rop->shell, rop->grab);
    }
}

/* Pop up a dialog. Common logic for all forms. */
static void
popup_vrop(struct rop *rop, abort_callback_t *a, const char *fmt, va_list args)
{
    char *buf = Vasprintf(fmt, args);

    popup_rop(rop, a, buf);
    Free(buf);
}

static void
stop_trying(void)
{
    push_macro(AnSet "(" ResReconnect "=" ResFalse "," ResRetry "=" ResFalse ")");
    popdown_an_error();
}

/* Pop up an error dialog. */
bool
glue_gui_error(pae_t type, const char *s)
{
    char *t = NULL;

    /* Handle delayed error pop-ups. */
    if (epd.active) {
	epd.type = type;
	Replace(epd.text, NewString(s));
	return true;
    }

    /* Pop up a dialog with a possible retry button. */
    if (type == ET_CONNECT) {
	t = Asprintf("Connection failed%s:\n%s",
		host_retry_mode? ", retrying": "", s);
    }
    popup_rop(&error_popup,
	    (host_retry_mode && !appres.secure)? stop_trying: NULL,
	    (t != NULL)? t: s);
    if (t != NULL) {
	Free(t);
    }

    return true;
}

/* Pop down an error dialog. */
void
popdown_an_error(void)
{
    if (epd.active && epd.text != NULL) {
	Replace(epd.text, NULL);
	return;
    }
    if (error_popup.visible) {
	XtPopdown(error_popup.shell);
    }
}

/* Error popup delay completion. */
void
error_popup_resume(void)
{
    epd.active = false;
    if (epd.text != NULL) {
	popup_an_xerror(epd.type, "%s", epd.text);
	Replace(epd.text, NULL);
    }
}

/* Pop up an info dialog. */
void
popup_an_info(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    popup_vrop(&info_popup, NULL, fmt, args);
    va_end(args);
}

/* Timeout for a timed info dialog. */
static void
timed_info_popdown(ioid_t id)
{
    info_id = NULL_IOID;
    XtPopdown(info_popup.shell);
}

/* Popup a timed info dialog. */
void
popup_a_timed_info(int timeout_ms, const char *fmt, ...)
{
    va_list args;

    if (info_id != NULL_IOID) {
	RemoveTimeOut(info_id);
	info_id = NULL_IOID;
    }
    info_id = AddTimeOut(timeout_ms, timed_info_popdown);

    va_start(args, fmt);
    popup_vrop(&info_popup, NULL, fmt, args);
    va_end(args);
}

/* Add a callback to the error popup. */
void
add_error_popdown_callback(void (*callback)(void))
{
    error_popup.popdown_callback = callback;
}

/*
 * Pop up some asynchronous action output.
 */
bool
glue_gui_output(const char *s)
{
    popup_rop(&info_popup, NULL, s);
    return true;
}

/* Callback for x3270 exit.  Dumps any undisplayed error messages to stderr. */
static void
dump_errmsgs(bool b _is_unused)
{
    while (error_popup.rsms != NULL) {
	fprintf(stderr, "Error: %s\n", error_popup.rsms->text);
	error_popup.rsms = error_popup.rsms->next;
    }
    while (info_popup.rsms != NULL) {
	fprintf(stderr, "%s\n", info_popup.rsms->text);
	info_popup.rsms = info_popup.rsms->next;
    }
}

/* Initialization. */

void
error_init(void)
{
}

void
error_popup_init(void)
{
    rop_init(&error_popup);
}

/* Callback for the info pop-up popping down. */
static void
info_popdown(Widget w, XtPointer client_data, XtPointer call_data _is_unused)
{
    if (info_id != NULL_IOID) {
	RemoveTimeOut(info_id);
	info_id = NULL_IOID;
    }
}

void
info_popup_init(void)
{
    rop_init(&info_popup);
    XtAddCallback(info_popup.shell, XtNpopdownCallback, info_popdown, NULL);
}

void
printer_popup_init(void)
{
    if (printer_error_popup.shell != NULL) {
	return;
    }
    rop_init(&printer_error_popup);
    rop_init(&printer_info_popup);
}

void
child_popup_init(void)
{
    if (child_error_popup.shell != NULL) {
	return;
    }
    rop_init(&child_error_popup);
    rop_init(&child_info_popup);
}

/* Query. */
bool
error_popup_visible(void)
{
    return (epd.active && epd.text != NULL) || error_popup.visible;
}

/*
 * Printer pop-up.
 * Allows both error and info popups, and a cancel button.
 *   is_err	If true, this is an error pop-up.  If false, this is an info
 *		pop-up.
 *   a		If non-NULL, the Cancel button is enabled, and this is the
 *		callback function for it.  If NULL, there will be no Cancel
 *		button.
 *   fmt...	printf()-like format and arguments.
 */
void
popup_printer_output(bool is_err, abort_callback_t *a, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    popup_vrop(is_err? &printer_error_popup: &printer_info_popup, a, fmt, args);
    va_end(args);
}

/*
 * Child output pop-up.
 * Allows both error and info popups, and a cancel button.
 *   is_err	If true, this is an error pop-up.  If false, this is an info
 *		pop-up.
 *   a		If non-NULL, the Cancel button is enabled, and this is the
 *		callback function for it.  If NULL, there will be no Cancel
 *		button.
 *   fmt...	printf()-like format and arguments.
 */
void
popup_child_output(bool is_err, abort_callback_t *a, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    popup_vrop(is_err? &child_error_popup: &child_info_popup, a, fmt, args);
    va_end(args);
}

/*
 * Move the popups that need moving.
 */
void
popups_move(void)
{
    static struct rop *rops[] = {
	&error_popup,
	&info_popup,
	&printer_error_popup,
	&printer_info_popup,
	&child_error_popup,
	&child_info_popup,
	NULL
    };
    int i;

    for (i = 0; rops[i] != NULL; i++) {
	if (rops[i]->visible) {
	    rops[i]->moving = true;
	    XtPopdown(rops[i]->shell);
	}
    }
}

/**
 * Pop-ups module registration.
 */
void
popups_register(void)
{
    /* Register for status change notifications. */
    register_schange(ST_EXITING, dump_errmsgs);
}
