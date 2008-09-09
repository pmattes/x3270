/*
 * Copyright 1993-2008 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	popups.c
 *		This module handles pop-up dialogs: errors, host names,
 *		font names, information.
 */

#include "globals.h"
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Text.h>
#include <stdarg.h>
#include "objects.h"
#include "appres.h"

#include "actionsc.h"
#include "hostc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "screenc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"

static char vmsgbuf[4096];

static enum form_type forms[] = { FORM_NO_WHITE, FORM_NO_CC, FORM_AS_IS };

static Dimension wm_width, wm_height;


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
		if (parent == root_window)
			break;
	}

	/*
	 * Figure out if they're running a virtual desktop, by seeing if
	 * the 1st child of root is bigger than it is.  If so, pretend that
	 * the virtual desktop is the root.
	 */
	if (nw > 1 &&
	    (ancestor[nw-2].wa.width > ancestor[nw-1].wa.width ||
	     ancestor[nw-2].wa.height > ancestor[nw-1].wa.height))
		--nw;
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

typedef struct {
    Widget w;
    Position x, y;
    enum placement p;
} want_t;

static void
popup_move_again(XtPointer closure, XtIntervalId *id unused)
{
	want_t *wx = (want_t *)closure;
    	Position x, y;

	XtVaGetValues(wx->w, XtNx, &x, XtNy, &y, NULL);
#if defined(POPUP_DEBUG) /*[*/
	printf("want x %d got x %d, want y %d, got y %d\n",
		wx->x, x,
		wx->y, y);
#endif /*]*/

	if (x != wx->x || y != wx->y) {
	    	Position main_x, main_y;
		Dimension main_width, main_height;
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
		printf("wm width %u height %u\n", wm_width, wm_height);
#endif /*]*/

		XtVaGetValues(toplevel, XtNx, &main_x, XtNy, &main_y,
			XtNwidth, &main_width, XtNheight, &main_height, NULL);

		switch (wx->p) {
		    case Bottom:
			x = main_x - wm_width;
			y = main_y + main_height + wm_width;
			break;
		    case Left:
			XtVaGetValues(wx->w, XtNwidth, &popup_width, NULL);
			x = main_x - (3 * wm_width) - popup_width;
			y = main_y - wm_height;
			break;
		    case Right:
			x = main_x + wm_width + main_width;
			y = main_y - wm_height;
			break;
		    case InsideRight:
			XtVaGetValues(wx->w, XtNwidth, &popup_width, NULL);
			x = main_x - (2 * wm_width) + main_width - popup_width;
			y = main_y + menubar_qheight(main_width);
			break;
		    default:
			return;
		}

#if defined(POPUP_DEBUG) /*[*/
		printf(" re-setting x %d y %d\n", x, y);
#endif /*]*/
	    	XtVaSetValues(wx->w, XtNx, x, XtNy, y, NULL);
	}
	XtFree((XtPointer)wx);
}

/* Place a newly popped-up shell */
void
place_popup(Widget w, XtPointer client_data, XtPointer call_data unused)
{
	Dimension width, height;
	Position x = 0, y = 0;
	Position xnew, ynew;
	Dimension win_width, win_height;
	Dimension popup_width, popup_height;
	enum placement p = *(enum placement *)client_data;
	XWindowAttributes wa;
	Boolean parent_is_root = False;
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
	if (x < 0 || y < 0)
		return;

#if defined(POPUP_DEBUG) /*[*/
	printf("place_popup: toplevel x %d y %d width %u height %u\n",
		x, y, win_width, win_height);
#endif /*]*/
	if (parent_of(XtWindow(toplevel)) == root_of(XtWindow(toplevel))) {
#if defined(POPUP_DEBUG) /*[*/
		printf("parent is root!\n");
#endif /*]*/
		parent_is_root = True;
	} else {
	    XGetWindowAttributes(display, parent_of(XtWindow(toplevel)), &wa);
#if defined(POPUP_DEBUG) /*[*/
	    printf("parent x %d y %d width %u height %u\n",
		    wa.x, wa.y, wa.width, wa.height);
#endif /*]*/
	}


	switch (p) {
	    case Center:
		XtVaGetValues(w,
		    XtNwidth, &popup_width,
		    XtNheight, &popup_height,
		    NULL);
		xnew = x + (win_width-popup_width) / (unsigned) 2;
		if (xnew < 0)
			xnew = 0;
		ynew = y + (win_height-popup_height) / (unsigned) 2;
		if (ynew < 0)
			ynew = 0;
		XtVaSetValues(w, XtNx, xnew, XtNy, ynew, NULL);
		break;
	    case Bottom:
		if (parent_is_root) {
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
			/* Measure what the window manager does. */
			wx = (want_t *)XtMalloc(sizeof(want_t));
			wx->w = w;
			wx->x = x;
			wx->y = y;
			wx->p = p;
			XtAppAddTimeOut(appcontext, 250, popup_move_again,
				(XtPointer)wx);
		} else {
			/* Do it precisely. */
			x = wa.x;
			y = wa.y + wa.height;
#if defined(POPUP_DEBUG) /*[*/
			printf("setting x %d y %d\n", x, y);
#endif /*]*/
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		}
		break;
	    case Left:
		if (parent_is_root) {
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
			wx = (want_t *)XtMalloc(sizeof(want_t));
			wx->w = w;
			wx->x = x;
			wx->y = y;
			wx->p = p;
			XtAppAddTimeOut(appcontext, 250, popup_move_again,
				(XtPointer)wx);
		} else {
			XtVaGetValues(w, XtNwidth, &popup_width, NULL);
			x = wa.x - popup_width - (wa.width - main_width);
			y = wa.y;
#if defined(POPUP_DEBUG) /*[*/
			printf("setting x %d y %d\n", x, y);
#endif /*]*/
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		}
		break;
	    case Right:
		if (parent_is_root) {
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
			wx = (want_t *)XtMalloc(sizeof(want_t));
			wx->w = w;
			wx->x = x;
			wx->y = y;
			wx->p = p;
			XtAppAddTimeOut(appcontext, 250, popup_move_again,
				(XtPointer)wx);
		} else {
			x = wa.x + wa.width;
			y = wa.y;
#if defined(POPUP_DEBUG) /*[*/
			printf("setting x %d y %d\n", x, y);
#endif /*]*/
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		}
		break;
	    case InsideRight:
		if (parent_is_root) {
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
			wx = (want_t *)XtMalloc(sizeof(want_t));
			wx->w = w;
			wx->x = x;
			wx->y = y;
			wx->p = p;
			XtAppAddTimeOut(appcontext, 250, popup_move_again,
				(XtPointer)wx);
		} else {
			XtVaGetValues(w, XtNwidth, &popup_width, NULL);
			x = wa.x + win_width - popup_width;
			y = wa.y + menubar_qheight(win_width) + (y - wa.y);
#if defined(POPUP_DEBUG) /*[*/
			printf("setting x %d y %d\n", x, y);
#endif /*]*/
			XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		}
		break;
	}
}

/* Move an existing popped-up shell */
void
move_popup(Widget w, XtPointer client_data, XtPointer call_data unused)
{
	Position x = 0, y = 0;
	Position xnew, ynew;
	Dimension win_width, win_height;
	Dimension popup_width, popup_height;
	enum placement p = *(enum placement *)client_data;
	XWindowAttributes wa;

	XtVaGetValues(toplevel, XtNx, &x, XtNy, &y, XtNwidth, &win_width,
		XtNheight, &win_height, NULL);
	if (x < 0 || y < 0)
		return;

#if defined(POPUP_DEBUG) /*[*/
	printf("move_popup: toplevel x %d y %d width %u height %u\n",
		x, y, win_width, win_height);
#endif /*]*/
	if (parent_of(XtWindow(toplevel)) == root_of(XtWindow(toplevel))) {
		/* Fake the parent window attributes. */
#if defined(POPUP_DEBUG) /*[*/
		printf("move_popup: parent is root\n");
#endif /*]*/
		wa.x = x - wm_width;
		wa.y = y - wm_height;
		wa.width = win_width + (2 * wm_width);
		wa.height = win_height + wm_height + wm_width;
	} else {
		XGetWindowAttributes(display, parent_of(XtWindow(toplevel)),
			&wa);
#if defined(POPUP_DEBUG) /*[*/
		printf("parent x %d y %d width %u height %u\n",
			wa.x, wa.y, wa.width, wa.height);
#endif /*]*/
	}

	switch (p) {
	    case Center:
		XtVaGetValues(w,
		    XtNwidth, &popup_width,
		    XtNheight, &popup_height,
		    NULL);
		xnew = x + (win_width-popup_width) / (unsigned) 2;
		if (xnew < 0)
			xnew = 0;
		ynew = y + (win_height-popup_height) / (unsigned) 2;
		if (ynew < 0)
			ynew = 0;
		XtVaSetValues(w, XtNx, xnew, XtNy, ynew, NULL);
		break;
	    case Bottom:
		x = wa.x;
		y = wa.y + wa.height;
#if defined(POPUP_DEBUG) /*[*/
		printf("setting x %d y %d\n", x, y);
#endif /*]*/
		XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		break;
	    case Left:
		XtVaGetValues(w, XtNwidth, &popup_width, NULL);
		x = wa.x - popup_width - (wa.width - main_width);
		y = wa.y;
#if defined(POPUP_DEBUG) /*[*/
		printf("setting x %d y %d\n", x, y);
#endif /*]*/
		XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		break;
	    case Right:
		x = wa.x + wa.width;
		y = wa.y;
#if defined(POPUP_DEBUG) /*[*/
		printf("setting x %d y %d\n", x, y);
#endif /*]*/
		XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		break;
	    case InsideRight:
		XtVaGetValues(w, XtNwidth, &popup_width, NULL);
		x = wa.x + win_width - popup_width;
		y = wa.y + menubar_qheight(win_width) + (y - wa.y);
#if defined(POPUP_DEBUG) /*[*/
		printf("setting x %d y %d\n", x, y);
#endif /*]*/
		XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
		break;
	}
}

/* Action called when "Return" is pressed in data entry popup */
void
PA_confirm_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	Widget w2;

	/* Find the Confirm or Okay button */
	w2 = XtNameToWidget(XtParent(w), ObjConfirmButton);
	if (w2 == NULL)
		w2 = XtNameToWidget(XtParent(w), ObjConfirmButton);
	if (w2 == NULL)
		w2 = XtNameToWidget(w, ObjConfirmButton);
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
cancel_button_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	XtPopdown((Widget) client_data);
}

/*
 * Callback for text source changes.  Ensures that the dialog text does not
 * contain white space -- especially newlines.
 */
static void
popup_dialog_callback(Widget w, XtPointer client_data,
    XtPointer call_data unused)
{
	static Boolean called_back = False;
	XawTextBlock b, nullb;	/* firstPos, length, ptr, format */
	XawTextPosition pos = 0;
	int front_len = 0;
	int end_len = 0;
	int end_pos = 0;
	int i;
	enum { FRONT, MIDDLE, END } place = FRONT;
	enum form_type *ftp = (enum form_type *)client_data;

	if (*ftp == FORM_AS_IS)
		return;

	if (called_back)
		return;
	else
		called_back = True;

	nullb.firstPos = 0;
	nullb.length = 0;
	nullb.ptr = NULL;

	/*
	 * Scan the text for whitespace.  Leading whitespace is deleted;
	 * embedded whitespace causes the rest of the text to be deleted.
	 */
	while (1) {
		XawTextSourceRead(w, pos, &b, 1024);
		if (b.length <= 0)
			break;
		nullb.format = b.format;
		if (place == END) {
			end_len += b.length;
			continue;
		}
		for (i = 0; i < b.length; i++) {
			char c;

			c = *(b.ptr + i);
			if (isspace(c) && (*ftp != FORM_NO_CC || c != ' ')) {
				if (place == FRONT) {
					front_len++;
					continue;
				} else {
					end_pos = b.firstPos + i;
					end_len = b.length - i;
					place = END;
					break;
				}
			} else
				place = MIDDLE;
		}
		pos += b.length;
		if (b.length < 1024)
			break;
	}
	if (front_len)
		XawTextSourceReplace(w, 0, front_len, &nullb);
	if (end_len)
		XawTextSourceReplace(w, end_pos - front_len,
		    end_pos - front_len + end_len, &nullb);
	called_back = False;
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

	/* Create the popup shell */

	widgetname = xs_buffer("%sPopup", name);
	if (isupper(widgetname[0]))
		widgetname[0] = tolower(widgetname[0]);
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
	XtVaSetValues(XtNameToWidget(dialog, XtNlabel),
	    NULL);

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
	XtAddCallback(w, XtNcallback, cancel_button_callback, (XtPointer) shell);

	if (form_type == FORM_AS_IS)
		return shell;

	/* Modify the translations for the objects in the dialog */

	w = XtNameToWidget(dialog, XtNvalue);
	if (w == NULL)
		xs_warning("Cannot find \"%s\" in dialog", XtNvalue);

	/* Set a callback for text modifications */
	w = XawTextGetSource(w);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, popup_dialog_callback,
		    &forms[(int)form_type]);

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
	Boolean is_error;			/* is it? */
	Boolean overwrites;			/* does it? */
	const char *itext;			/* initial text */
	Widget shell;				/* pop-up shell */
	Widget form;				/* dialog form */
	Widget cancel_button;			/* cancel button */
	abort_callback_t *cancel_callback;	/* callback for cancel button */
	Boolean visible;			/* visibility flag */
	Boolean moving;				/* move in progress */
	struct rsm *rsms;			/* stored messages */
};

static struct rop error_popup = {
	"errorPopup", XtGrabExclusive, True, False,
	"first line\nsecond line\nthird line",
	NULL, NULL, NULL, NULL,
	False, False, NULL
};
static struct rop info_popup = {
	"infoPopup", XtGrabNonexclusive, False, False,
	"first line\nsecond line\nthird line",
	NULL, NULL, NULL, NULL,
	False, False, NULL
};

static struct rop printer_error_popup = {
	"printerErrorPopup", XtGrabExclusive, True, True,
	"first line\nsecond line\nthird line\nfourth line",
	NULL, NULL, NULL, NULL, False, False, NULL
};
static struct rop printer_info_popup = {
	"printerInfoPopup", XtGrabNonexclusive, False, True,
	"first line\nsecond line\nthird line\nfourth line",
	NULL,
	NULL, NULL, NULL, False, False, NULL
};

static struct rop child_error_popup = {
	"childErrorPopup", XtGrabNonexclusive, True, True,
	"first line\nsecond line\nthird line\nfourth line",
	NULL, NULL, NULL, NULL, False, False, NULL
};
static struct rop child_info_popup = {
	"childInfoPopup", XtGrabNonexclusive, False, True,
	"first line\nsecond line\nthird line\nfourth line",
	NULL,
	NULL, NULL, NULL, False, False, NULL
};

/* Called when OK is pressed in a read-only popup */
static void
rop_ok(Widget w unused, XtPointer client_data, XtPointer call_data unused)
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
rop_cancel(Widget w unused, XtPointer client_data, XtPointer call_data unused)
{
	struct rop *rop = (struct rop *)client_data;

	XtPopdown(rop->shell);
	if (rop->cancel_callback != NULL)
		(*rop->cancel_callback)();
}

/* Called when a read-only popup is closed */
static void
rop_popdown(Widget w unused, XtPointer client_data, XtPointer call_data unused)
{
	struct rop *rop = (struct rop *)client_data;

	if (rop->moving) {
		rop->moving = False;
		XtPopup(rop->shell, rop->grab);
		return;
	}
	rop->visible = False;
	if (exiting && rop->is_error)
		x3270_exit(1);
}

/* Initialize a read-only pop-up. */
static void
rop_init(struct rop *rop)
{
	Widget w;
	struct rsm *r;

	if (rop->shell != NULL)
		return;

	rop->shell = XtVaCreatePopupShell(
	    rop->name, transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(rop->shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
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

	/* If there's a pending message, pop it up now. */
	if ((r = rop->rsms) != NULL) {
		if (rop->is_error)
			popup_an_error("%s", r->text);
		else
			popup_an_info("%s", r->text);
		rop->rsms = r->next;
		Free(r->text);
		Free(r);
	}
}

/* Pop up a dialog.  Common logic for all forms. */
static void
popup_rop(struct rop *rop, abort_callback_t *a, const char *fmt, va_list args)
{
	(void) vsprintf(vmsgbuf, fmt, args);
	if (!rop->shell || (rop->visible && !rop->overwrites)) {
#if 0
		char *nl;

		while ((nl = strchr(vmsgbuf, '\n')) != CN) {
			*nl = ' ';
		}
		(void) fprintf(stderr, "%s: %s\n", programname, vmsgbuf);
		if (rop->is_error)
			exit(1);
		else
			return;
#endif
		struct rsm *r, **s;

		r = (struct rsm *)Malloc(sizeof(struct rsm));
		r->text = NewString(vmsgbuf);
		r->next = NULL;
		for (s = &rop->rsms; *s != NULL; s = &(*s)->next)
			;
		*s = r;
		return;
	}

#if defined(X3270_TRACE) /*[*/
	/* Put the error in the trace file. */
	if (rop->is_error) {
		if (toggled(DS_TRACE))
			trace_dsn("\nError: %s\n", vmsgbuf);
		else if (toggled(EVENT_TRACE)) {
			trace_event("\nError: %s\n", vmsgbuf);
		}
	}
#endif /*]*/

	if (rop->is_error && sms_redirect()) {
		sms_error(vmsgbuf);
		return;
	}

	XtVaSetValues(rop->form, XtNlabel, vmsgbuf, NULL);
	if (a != NULL)
		XtMapWidget(rop->cancel_button);
	else
		XtUnmapWidget(rop->cancel_button);
	rop->cancel_callback = a;
	if (!rop->visible) {
		if (rop->is_error)
			ring_bell();
		rop->visible = True;
		popup_popup(rop->shell, rop->grab);
	}
}

static void
error_exit(void)
{
	x3270_exit(0);
}

/* Pop up an error dialog. */
void
popup_an_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	popup_rop(&error_popup, appres.reconnect? error_exit: NULL, fmt, args);
	va_end(args);
}

/* Pop down an error dialog. */
void
popdown_an_error(void)
{
	if (error_popup.visible) {
		XtPopdown(error_popup.shell);
	}
}

/* Pop up an error dialog, based on an error number. */
void
popup_an_errno(int errn, const char *fmt, ...)
{
	va_list args;
	char *s;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);
	s = XtNewString(vmsgbuf);

	if (errn > 0)
		popup_an_error("%s:\n%s", s, strerror(errn));
	else
		popup_an_error(s);
	XtFree(s);
}

/* Pop up an info dialog. */
void
popup_an_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	popup_rop(&info_popup, NULL, fmt, args);
	va_end(args);
}

/*
 * Produce a result of some sort.  If there is a script running, return it
 * as the value; otherwise, pop it up as an info.
 */
void
action_output(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (sms_redirect()) {
		char buf[4096];

		(void) vsprintf(buf, fmt, args);
		sms_info("%s", buf);
	} else
		popup_rop(&info_popup, NULL, fmt, args);
	va_end(args);
}

/* Callback for x3270 exit.  Dumps any undisplayed error messages to stderr. */
static void
dump_errmsgs(Boolean b unused)
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
	register_schange(ST_EXITING, dump_errmsgs);
}

void
error_popup_init(void)
{
	rop_init(&error_popup);
}

void
info_popup_init(void)
{
	rop_init(&info_popup);
}

void
printer_popup_init(void)
{
	if (printer_error_popup.shell != NULL)
		return;
	rop_init(&printer_error_popup);
	rop_init(&printer_info_popup);
}

void
child_popup_init(void)
{
	if (child_error_popup.shell != NULL)
		return;
	rop_init(&child_error_popup);
	rop_init(&child_info_popup);
}

/* Query. */
Boolean
error_popup_visible(void)
{
	return error_popup.visible;
}

/*
 * Printer pop-up.
 * Allows both error and info popups, and a cancel button.
 *   is_err	If True, this is an error pop-up.  If false, this is an info
 *		pop-up.
 *   a		If non-NULL, the Cancel button is enabled, and this is the
 *		callback function for it.  If NULL, there will be no Cancel
 *		button.
 *   fmt...	printf()-like format and arguments.
 */
void
popup_printer_output(Boolean is_err, abort_callback_t *a, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	popup_rop(is_err? &printer_error_popup: &printer_info_popup, a, fmt,
	    args);
	va_end(args);
}

/*
 * Child output pop-up.
 * Allows both error and info popups, and a cancel button.
 *   is_err	If True, this is an error pop-up.  If false, this is an info
 *		pop-up.
 *   a		If non-NULL, the Cancel button is enabled, and this is the
 *		callback function for it.  If NULL, there will be no Cancel
 *		button.
 *   fmt...	printf()-like format and arguments.
 */
void
popup_child_output(Boolean is_err, abort_callback_t *a, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	popup_rop(is_err? &child_error_popup: &child_info_popup, a, fmt, args);
	va_end(args);
}

/*
 * Script actions
 */
void
Info_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Info_action, event, params, num_params);
	if (check_usage(Info_action, *num_params, 1, 1) < 0)
		return;
	popup_an_info(params[0]);
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
			rops[i]->moving = True;
			XtPopdown(rops[i]->shell);
		}
	}
}
