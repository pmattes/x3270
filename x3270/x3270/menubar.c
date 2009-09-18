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
 *	menubar.c
 *		This module handles the menu bar.
 */

#include "globals.h"

#if defined(X3270_MENUS) /*[*/

#include <stdarg.h>

#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/Dialog.h>
#include "Husk.h"
#include "CmplxMenu.h"
#include "CmeBSB.h"
#include "CmeLine.h"

#include "appres.h"
#include "objects.h"
#include "resources.h"
#include "screen.h"

#include "actionsc.h"
#include "aboutc.h"
#include "charsetc.h"
#include "ftc.h"
#include "hostc.h"
#include "idlec.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "printerc.h"
#include "printc.h"
#include "savec.h"
#include "screenc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "utilc.h"
#include "xioc.h"

#define MACROS_MENU	"macrosMenu"

extern Widget		keypad_shell;
extern int		linemode;
extern Boolean		keypad_popped;

static struct scheme {
	char *label;
	char **parents;
	char *scheme;
	struct scheme *next;
} *schemes, *last_scheme;
static int scheme_count;
static Widget  *scheme_widgets;
static Widget file_menu;
static Widget options_menu;
static Widget fonts_option;
static Pixel fm_background = 0;
static Dimension fm_borderWidth;
static Pixel fm_borderColor;
static Dimension fm_leftMargin;
static Dimension fm_rightMargin;

static struct charset {
	char **parents;
	char *label;
	char *charset;
	struct charset *next;
} *charsets, *last_charset;
static int charset_count;
static Widget  *charset_widgets;

static void scheme_init(void);
static void charsets_init(void);
static void options_menu_init(Boolean regen, Position x, Position y);
#if defined(X3270_KEYPAD) /*[*/
static void keypad_button_init(Position x, Position y);
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
static void ssl_icon_init(Position x, Position y);
#endif /*]*/
static void connect_menu_init(Boolean regen, Position x, Position y);
static void macros_menu_init(Boolean regen, Position x, Position y);
static void file_menu_init(Boolean regen, Dimension x, Dimension y);
static void Bye(Widget w, XtPointer client_data, XtPointer call_data);
static void menubar_in3270(Boolean in3270);
static void menubar_linemode(Boolean in_linemode);
static void menubar_connect(Boolean ignored);
#if defined(X3270_PRINTER) /*[*/
static void menubar_printer(Boolean printer_on);
#endif /*]*/
static void menubar_remodel(Boolean ignored _is_unused);
static void menubar_charset(Boolean ignored _is_unused);

#define NO_BANG(s)	(((s)[0] == '!')? (s) + 1: (s))

#include "dot.bm"
#include "no_dot.bm"
#include "arrow.bm"
#include "diamond.bm"
#include "no_diamond.bm"
#if defined(X3270_KEYPAD) /*[*/
#include "ky.bm"
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
#include "locked.bm"
#include "unlocked.bm"
#endif /*]*/
#include "null.bm"


/*
 * Menu Bar
 */

static Widget	menu_parent;
static Boolean  menubar_buttons;
static Widget   disconnect_button;
static Widget   exit_button;
static Widget   exit_menu;
static Widget   macros_button;
static Widget	ft_button;
#if defined(X3270_PRINTER) /*[*/
static Widget	printer_button;
static Widget	assoc_button;
static Widget	lu_button;
static Widget	printer_off_button;
#endif /*]*/
static Widget   connect_button;
#if defined(HAVE_LIBSSL) /*[*/
static Widget   locked_icon;
static Widget   unlocked_icon;
#endif /*]*/
#if defined(X3270_KEYPAD) /*[*/
static Widget   keypad_button;
#endif /*]*/
static Widget   linemode_button;
static Widget   charmode_button;
static Widget   models_option;
static Widget   model_2_button;
static Widget   model_3_button;
static Widget   model_4_button;
static Widget   model_5_button;
static Widget	oversize_button;
static Widget	extended_button;
static Widget	m3278_button;
static Widget	m3279_button;
static Widget   keypad_option_button;
static Widget	scheme_button;
static Widget   connect_menu;
#if defined(X3270_SCRIPT) /*[*/
static Widget	script_abort_button;
static Widget	idle_button;
#endif /*]*/

static Pixmap   arrow;
Pixmap	dot;
Pixmap	no_dot;
Pixmap	diamond;
Pixmap	no_diamond;
Pixmap	null;

static int	n_bye;

static Boolean	toggle_init(Widget, int, const char *, const char *, Boolean *);

#define TOP_MARGIN	3
#define BOTTOM_MARGIN	3
#define LEFT_MARGIN	3
#define KEY_HEIGHT	18
#define KEY_WIDTH	70
#define BORDER		1
#define SPACING		3

#define BUTTON_X(n)	(LEFT_MARGIN + (n)*(KEY_WIDTH+2*BORDER+SPACING))

#define MENU_BORDER	2

#if defined(X3270_KEYPAD) /*[*/
#define KY_WIDTH	(ky_width + 8)
#else /*][*/
#define KY_WIDTH	0
#endif /*]*/

#define	MENU_MIN_WIDTH	(LEFT_MARGIN + 3*(KEY_WIDTH+2*BORDER+SPACING) + \
			 LEFT_MARGIN + KY_WIDTH + 2*BORDER + SPACING + \
			 2*MENU_BORDER)

/* Menu hierarchy structure. */
struct menu_hier {
	Widget menu_shell;		/* complexMenu widget */
	char *name;			/* my name (root name is NULL) */
	struct menu_hier *parent;	/* parent menu */
	struct menu_hier *child;	/* child menu */
	struct menu_hier *sibling;	/* sibling menu */
};

/*
 * Add an entry to a menu hierarchy.
 * Adds intermediate nodes as need, and returns the menu shell widget to
 * add the leaf entry to.
 */
static Widget
add_menu_hier(struct menu_hier *root, char **parents, ArgList args,
		Cardinal num_args)
{
	struct menu_hier *h = root;
	static int menu_num = 0;

	/* Search for a parent match, creating levels as needed. */
	while (parents && *parents) {
		struct menu_hier *child, *last_child = NULL;

		if (h->name != CN && !strcmp(h->name, *parents))
			break;
		last_child = h->child;
		for (child = h->child; child != NULL; child = child->sibling) {
			if (!strcmp(child->name, *parents))
				break;
			last_child = child;
		}
		if (child != NULL)
			h = child;
		else {
			struct menu_hier *new_child;
			char namebuf[64];
			char *menu_name;
			char *m;
			int i;
			Arg my_arglist[2];
			ArgList merged_args;

			new_child = (struct menu_hier *)XtCalloc(1,
					sizeof(struct menu_hier));
			new_child->name = *parents;
			new_child->parent = h;
			if (last_child != NULL)
				last_child->sibling = new_child;
			else
				h->child = new_child;
			h = new_child;

			/*
			 * Create a menu for the children of this new
			 * intermediate node.
			 */
			sprintf(namebuf, "csMenu%d", menu_num++);
			for (i = 0, m = namebuf + strlen(namebuf);
			     (*parents)[i] && ((size_t)(m - namebuf) < sizeof(namebuf));
			     i++) {
				if (isalnum((*parents)[i])) {
					*m++ = (*parents)[i];
				}
			}
			*m = '\0';
			menu_name = XtNewString(namebuf);
			h->menu_shell = XtVaCreatePopupShell(
				menu_name, complexMenuWidgetClass,
				h->parent->menu_shell,
				NULL);

			/*
			 * Add this item to its parent's menu, as a pullright.
			 */
			XtSetArg(my_arglist[0], XtNrightBitmap, arrow);
			XtSetArg(my_arglist[1], XtNmenuName, menu_name);
			merged_args = XtMergeArgLists(my_arglist, 2, args,
					num_args);
			(void) XtCreateManagedWidget(
			    h->name, cmeBSBObjectClass, h->parent->menu_shell,
			    merged_args, 2 + num_args);
			XtFree((XtPointer)merged_args);
		}

		/* Go on to the next level. */
		parents++;
	}

	/* Add here. */
	return h->menu_shell;
}

static void
free_menu_hier(struct menu_hier *root)
{
    	if (root->sibling)
	    	free_menu_hier(root->sibling);
	if (root->child)
	    	free_menu_hier(root->child);
	XtFree((char *)root);
}

/*
 * Compute the potential height of the menu bar.
 */
Dimension
menubar_qheight(Dimension container_width)
{
	if (!appres.menubar ||
	    (!fixed_width && (container_width < (unsigned) MENU_MIN_WIDTH)))
		return 0;
	else
		return TOP_MARGIN + KEY_HEIGHT+2*BORDER + BOTTOM_MARGIN +
			2*MENU_BORDER;
}

/*
 * Initialize the menu bar.
 */
void
menubar_init(Widget container, Dimension overall_width, Dimension current_width)
{
	static Widget menu_bar;
	static Boolean ever = False;
	Boolean mb_old;
	Dimension height;

	if (!ever) {

		scheme_init();
		charsets_init();
		XtRegisterGrabAction(HandleMenu_action, True,
		    (ButtonPressMask|ButtonReleaseMask),
		    GrabModeAsync, GrabModeAsync);

		/* Create bitmaps. */
		dot = XCreateBitmapFromData(display, root_window,
		    (char *) dot_bits, dot_width, dot_height);
		no_dot = XCreateBitmapFromData(display, root_window,
		    (char *) no_dot_bits, no_dot_width, no_dot_height);
		arrow = XCreateBitmapFromData(display, root_window,
		    (char *) arrow_bits, arrow_width, arrow_height);
		diamond = XCreateBitmapFromData(display, root_window,
		    (char *) diamond_bits, diamond_width, diamond_height);
		no_diamond = XCreateBitmapFromData(display, root_window,
		    (char *) no_diamond_bits, no_diamond_width,
			no_diamond_height);
		null = XCreateBitmapFromData(display, root_window,
		    (char *) null_bits, null_width, null_height);

		/* Register interest in state transtions. */
		register_schange(ST_3270_MODE, menubar_in3270);
		register_schange(ST_LINE_MODE, menubar_linemode);
		register_schange(ST_HALF_CONNECT, menubar_connect);
		register_schange(ST_CONNECT, menubar_connect);
#if defined(X3270_PRINTER) /*[*/
		register_schange(ST_PRINTER, menubar_printer);
#endif /*]*/
		register_schange(ST_REMODEL, menubar_remodel);
		register_schange(ST_CHARSET, menubar_charset);

		ever = True;
	}

	height = menubar_qheight(current_width);
	mb_old = menubar_buttons;
	menubar_buttons = (height != 0);
	if (menubar_buttons) {
		if (menu_bar == (Widget)NULL) {
			/* Create the menu bar. */
			menu_bar = XtVaCreateManagedWidget(
			    "menuBarContainer", huskWidgetClass, container,
			    XtNborderWidth, MENU_BORDER,
			    XtNwidth, overall_width - 2*MENU_BORDER,
			    XtNheight, height - 2*MENU_BORDER,
			    NULL);
		} else {
			/* Resize and map the menu bar. */
			XtVaSetValues(menu_bar,
			    XtNborderWidth, MENU_BORDER,
			    XtNwidth, overall_width - 2*MENU_BORDER,
			    NULL);
			XtMapWidget(menu_bar);
		}
		menu_parent = menu_bar;
	} else if (menu_bar != (Widget)NULL) {
		/* Hide the menu bar. */
		XtUnmapWidget(menu_bar);
		menu_parent = container;
	} else
		menu_parent = container;

	/* "File..." menu */

	file_menu_init(mb_old != menubar_buttons, LEFT_MARGIN, TOP_MARGIN);

	/* "Options..." menu */

	options_menu_init(mb_old != menubar_buttons,
	    BUTTON_X(file_menu != NULL),
	    TOP_MARGIN);

	/* "Connect..." menu */

	if (!appres.reconnect)
		connect_menu_init(mb_old != menubar_buttons,
		    BUTTON_X((file_menu != NULL) + (options_menu != NULL)),
		    TOP_MARGIN);

	/* "Macros..." menu */

	macros_menu_init(mb_old != menubar_buttons,
	    BUTTON_X((file_menu != NULL) + (options_menu != NULL)),
	    TOP_MARGIN);

#if defined(HAVE_LIBSSL) /*[*/
	/* SSL icon */

	ssl_icon_init(
	    (Position) (current_width - LEFT_MARGIN -
#if defined(X3270_KEYPAD) /*[*/
			    (ky_width+8) -
#endif /*]*/
			    4*BORDER - 2*MENU_BORDER - (locked_width+8)),
	    TOP_MARGIN);
#endif /*]*/

#if defined(X3270_KEYPAD) /*[*/
	/* Keypad button */

	keypad_button_init(
	    (Position) (current_width - LEFT_MARGIN - (ky_width+8) -
			    2*BORDER - 2*MENU_BORDER),
	    TOP_MARGIN);
#endif /*]*/
}

/*
 * External entry points
 */

/*
 * Called when connected to or disconnected from a host.
 */
static void
menubar_connect(Boolean ignored _is_unused)
{
	/* Set the disconnect button sensitivity. */
	if (disconnect_button != (Widget)NULL)
		XtVaSetValues(disconnect_button,
		    XtNsensitive, PCONNECTED,
		    NULL);

	/* Set up the exit button, either with a pullright or a callback. */
	if (exit_button != (Widget)NULL) {
		if (PCONNECTED) {
			/* Remove the immediate callback. */
			if (n_bye) {
				XtRemoveCallback(exit_button, XtNcallback,
				    Bye, NULL);
				n_bye--;
			}

			/* Set pullright for extra confirmation. */
			XtVaSetValues(exit_button,
			    XtNrightBitmap, arrow,
			    XtNmenuName, "exitMenu",
			    NULL);
		} else {
			/* Install the immediate callback. */
			if (!n_bye) {
				XtAddCallback(exit_button, XtNcallback,
				    Bye, NULL);
				n_bye++;
			}

			/* Remove the pullright. */
			XtVaSetValues(exit_button,
			    XtNrightBitmap, NULL,
			    XtNmenuName, NULL,
			    NULL);
		}
	}

	/* Set up the connect menu. */
	if (!appres.reconnect && connect_menu != (Widget)NULL) {
		if (PCONNECTED && connect_button != (Widget)NULL)
			XtUnmapWidget(connect_button);
		else {
			connect_menu_init(True,
			    BUTTON_X((file_menu != NULL) +
				     (options_menu != NULL)),
			    TOP_MARGIN);
			if (menubar_buttons)
				XtMapWidget(connect_button);
		}
	}

	/* Set up the macros menu. */
	macros_menu_init(True,
	    BUTTON_X((file_menu != NULL) + (options_menu != NULL)),
	    TOP_MARGIN);

	/* Set up the various option buttons. */
	if (ft_button != (Widget)NULL)
		XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
#if defined(X3270_PRINTER) /*[*/
	if (printer_button != (Widget)NULL)
		XtVaSetValues(printer_button, XtNsensitive, IN_3270,
		    NULL);
	if (assoc_button != (Widget)NULL)
		XtVaSetValues(assoc_button, XtNsensitive,
		    !printer_running() && IN_3270 && IN_TN3270E,
		    NULL);
	if (lu_button != (Widget)NULL)
		XtVaSetValues(lu_button, XtNsensitive,
		    !printer_running() && IN_3270,
		    NULL);
#endif /*]*/
	if (linemode_button != (Widget)NULL)
		XtVaSetValues(linemode_button, XtNsensitive, IN_ANSI, NULL);
	if (charmode_button != (Widget)NULL)
		XtVaSetValues(charmode_button, XtNsensitive, IN_ANSI, NULL);
#if defined(X3270_ANSI) /*[*/
	if (appres.toggle[LINE_WRAP].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[LINE_WRAP].w[0],
		    XtNsensitive, IN_ANSI,
		    NULL);
#endif /*]*/
	if (appres.toggle[RECTANGLE_SELECT].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[RECTANGLE_SELECT].w[0],
		    XtNsensitive, IN_ANSI,
		    NULL);
	if (models_option != (Widget)NULL)
		XtVaSetValues(models_option, XtNsensitive, !PCONNECTED, NULL);
	if (extended_button != (Widget)NULL)
		XtVaSetValues(extended_button, XtNsensitive, !PCONNECTED,
		    NULL);
	if (m3278_button != (Widget)NULL)
		XtVaSetValues(m3278_button, XtNsensitive, !PCONNECTED,
		    NULL);
	if (m3279_button != (Widget)NULL)
		XtVaSetValues(m3279_button, XtNsensitive, !PCONNECTED,
		    NULL);

#if defined(HAVE_LIBSSL) /*[*/
	if (locked_icon != NULL) {
		if (CONNECTED) {
			if (secure_connection) {
				XtMapWidget(locked_icon);
				XtUnmapWidget(unlocked_icon);
			} else {
				XtMapWidget(unlocked_icon);
				XtUnmapWidget(locked_icon);
			}
		} else {
			XtUnmapWidget(locked_icon);
			XtUnmapWidget(unlocked_icon);
		}
	}
#endif /*]*/
}

#if defined(X3270_PRINTER) /*[*/
/* Called when the printer starts or stops. */
static void
menubar_printer(Boolean printer_on)
{
	if (assoc_button != (Widget)NULL)
		XtVaSetValues(assoc_button, XtNsensitive,
		    !printer_on && IN_3270 && IN_TN3270E,
		    NULL);
	if (lu_button != (Widget)NULL)
		XtVaSetValues(lu_button, XtNsensitive,
		    !printer_on && IN_3270,
		    NULL);
	if (printer_off_button != (Widget)NULL)
		XtVaSetValues(printer_off_button,
		    XtNsensitive, printer_on,
		    NULL);
}
#endif /*]*/

#if defined(X3270_KEYPAD) /*[*/
void
menubar_keypad_changed(void)
{
	if (keypad_option_button != (Widget)NULL)
		XtVaSetValues(keypad_option_button,
		    XtNleftBitmap,
			appres.keypad_on || keypad_popped ? dot : None,
		    NULL);
}
#endif /*]*/

/* Called when we switch between ANSI and 3270 modes. */
static void
menubar_in3270(Boolean in3270)
{
	if (ft_button != (Widget)NULL)
		XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
#if defined(X3270_PRINTER) /*[*/
	if (printer_button != (Widget)NULL)
		XtVaSetValues(printer_button, XtNsensitive, IN_3270,
		    NULL);
	if (assoc_button != (Widget)NULL)
		XtVaSetValues(assoc_button, XtNsensitive,
		    !printer_running() && IN_3270 && IN_TN3270E,
		    NULL);
	if (lu_button != (Widget)NULL)
		XtVaSetValues(lu_button, XtNsensitive,
		    !printer_running() && IN_3270,
		    NULL);
#endif /*]*/
	if (linemode_button != (Widget)NULL)
		XtVaSetValues(linemode_button,
		    XtNsensitive, !in3270,
		    XtNleftBitmap, in3270 ? no_diamond
					: (linemode ? diamond : no_diamond),
		    NULL);
	if (charmode_button != (Widget)NULL)
		XtVaSetValues(charmode_button,
		    XtNsensitive, !in3270,
		    XtNleftBitmap, in3270 ? no_diamond
					: (linemode ? no_diamond : diamond),
		    NULL);
#if defined(X3270_ANSI) /*[*/
	if (appres.toggle[LINE_WRAP].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[LINE_WRAP].w[0],
		    XtNsensitive, !in3270,
		    NULL);
#endif /*]*/
	if (appres.toggle[RECTANGLE_SELECT].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[RECTANGLE_SELECT].w[0],
		    XtNsensitive, !in3270,
		    NULL);
#if defined(X3270_SCRIPT) /*[*/
	if (idle_button != (Widget)NULL)
		XtVaSetValues(idle_button,
		    XtNsensitive, in3270,
		    NULL);
#endif /*]*/
}

/* Called when we switch between ANSI line and character. */
static void
menubar_linemode(Boolean in_linemode)
{
	if (linemode_button != (Widget)NULL)
		XtVaSetValues(linemode_button,
		    XtNleftBitmap, in_linemode ? diamond : no_diamond,
		    NULL);
	if (charmode_button != (Widget)NULL)
		XtVaSetValues(charmode_button,
		    XtNleftBitmap, in_linemode ? no_diamond : diamond,
		    NULL);
}

#if defined(X3270_SCRIPT) /*[*/
/* Called to change the sensitivity of the "Abort Script" button. */
void
menubar_as_set(Boolean sensitive)
{
	if (script_abort_button != (Widget)NULL)
		XtVaSetValues(script_abort_button,
		    XtNsensitive, sensitive,
		    NULL);
}
#endif /*]*/


/*
 * "File..." menu
 */
static Widget save_shell = (Widget) NULL;

/* Called from "Exit x3270" button on "File..." menu */
static void
Bye(Widget w _is_unused, XtPointer client_data _is_unused, XtPointer call_data _is_unused)
{
	x3270_exit(0);
}

/* Called from the "Disconnect" button on the "File..." menu */
static void
disconnect(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	host_disconnect(False);
}

#if defined(X3270_SCRIPT) /*[*/
/* Called from the "Abort Script" button on the "File..." menu */
static void
script_abort_callback(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	abort_script();
}
#endif /*]*/

/* "About x3270" popups */
static void
show_about_copyright(Widget w _is_unused, XtPointer userdata _is_unused,
    XtPointer calldata _is_unused)
{
	popup_about_copyright();
}

static void
show_about_config(Widget w _is_unused, XtPointer userdata _is_unused,
    XtPointer calldata _is_unused)
{
	popup_about_config();
}

static void
show_about_status(Widget w _is_unused, XtPointer userdata _is_unused,
    XtPointer calldata _is_unused)
{
	popup_about_status();
}

/* Called from the "Save" button on the save options dialog */
static void
save_button_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (!save_options(s))
		XtPopdown(save_shell);
}

/* Called from the "Save Options in File" button on the "File..." menu */
static void
do_save_options(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	if (save_shell == NULL)
		save_shell = create_form_popup("SaveOptions",
		    save_button_callback, (XtCallbackProc)NULL, FORM_NO_WHITE);
	XtVaSetValues(XtNameToWidget(save_shell, ObjDialog),
	    XtNvalue, profile_name,
	    NULL);
	popup_popup(save_shell, XtGrabExclusive);
}

#if defined(X3270_PRINTER) /*[*/
/* Callback for printer session options. */
static void
do_printer(Widget w _is_unused, XtPointer client_data, XtPointer call_data _is_unused)
{
	if (client_data == NULL)
		printer_start(CN);
	else if (!strcmp(client_data, "lu"))
		printer_lu_dialog();
	else
		printer_stop();
}
#endif /*]*/

/* Figure out if a Widget is suppressed. */
static Boolean
item_suppressed(Widget parent, const char *name)
{
	char *s = NULL;
	char *t = NULL;
	Widget p = parent;
	char *suppress;

	while (p != NULL) {
		char *n = XtName(p);

		if (n == CN)
			break;
		if (s != CN) {
			t = xs_buffer("%s.%s", n, s);
			Free(s);
			s = t;
		} else {
			s = NewString(n);
		}
		p = XtParent(p);
	}
	suppress = get_fresource("%s.%s.%s", s, name, ResSuppress);
	Free(s);
	return suppress != CN &&
	       !strncasecmp(suppress, "True", strlen(suppress));
}

/*
 * Create a dividing line, if *spaced isn't True.
 */
static void
cond_space(Widget menu, Boolean *spaced)
{
	if (spaced != NULL && !*spaced) {
		(void) XtVaCreateManagedWidget(
		    "space", cmeLineObjectClass, menu,
		    NULL);
		*spaced = True;
	}
}

/*
 * Add a menu item to a menu, but only if it is not suppressed.
 */
static Widget
add_menu_itemv(char *name, Widget menu, XtCallbackProc callback, XtPointer arg,
		Boolean *spaced, ...)
{

	if (!item_suppressed(menu, name)) {
		Widget w;
		static Arg *args = NULL;
		Cardinal num_args = 0;
		static Cardinal max_num_args = 0;
		va_list a;
		String argname;
		XtArgVal value;

		cond_space(menu, spaced);
		va_start(a, spaced);
		while ((argname = va_arg(a, String)) != NULL) {
			value = va_arg(a, XtArgVal);
			while (num_args >= max_num_args) {
				max_num_args++;
				args = (Arg *)Realloc(args,
						max_num_args * sizeof(Arg));
			}
			XtSetArg(args[num_args], argname, value);
			num_args++;
		}
		va_end(a);
		w = XtCreateManagedWidget(name,
				cmeBSBObjectClass, menu,
				args, num_args);
		XtAddCallback(w, XtNcallback, callback, arg);
		return w;
	} else
		return NULL;
}

static void
file_menu_init(Boolean regen, Dimension x, Dimension y)
{
	Widget about_option;
	Widget w;
	Boolean spaced = False;
	Boolean any = False;

	if (regen && (file_menu != (Widget)NULL)) {
		XtDestroyWidget(file_menu);
		file_menu = (Widget)NULL;
	}
	if (file_menu != (Widget)NULL)
		return;

	file_menu = XtVaCreatePopupShell(
	    "fileMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    file_menu, NULL);

	/* About x3270... */
	if (!item_suppressed(file_menu, "aboutOption")) {
		Boolean any_about = False;

		w = XtVaCreatePopupShell(
		    "aboutMenu", complexMenuWidgetClass, file_menu,
		    NULL);
		any_about |= add_menu_itemv("aboutCopyright", w,
			show_about_copyright, NULL, NULL, NULL) != (Widget)NULL;
		any_about |= add_menu_itemv("aboutConfig", w,
			show_about_config, NULL, NULL, NULL) != (Widget)NULL;
		any_about |= add_menu_itemv("aboutStatus", w,
			show_about_status, NULL, NULL, NULL) != (Widget)NULL;
		if (any_about) {
			about_option = XtVaCreateManagedWidget(
			    "aboutOption", cmeBSBObjectClass, file_menu,
			    XtNrightBitmap, arrow,
			    XtNmenuName, "aboutMenu",
			    NULL);
			any = True;
		} else {
			XtDestroyWidget(w);
		}
	}

#if defined(X3270_FT) /*[*/
	/* File Transfer */
	if (!appres.secure) {
		spaced = False;
		ft_button = add_menu_itemv("ftOption", file_menu,
				popup_ft, NULL, &spaced,
				XtNsensitive, IN_3270,
				NULL);
		any |= (ft_button != NULL);
	}
#endif /*]*/

#if defined(X3270_PRINTER) /*[*/
	/* Printer start/stop */
	if (!item_suppressed(file_menu, "printerOption")) {
		w = XtVaCreatePopupShell(
		    "printerMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		assoc_button = add_menu_itemv("assocButton", w,
				do_printer, NULL, NULL,
				XtNsensitive, IN_3270 && IN_TN3270E,
				NULL);
		lu_button = add_menu_itemv("luButton", w,
				do_printer, "lu", NULL,
				NULL);
		printer_off_button = add_menu_itemv("printerOffButton", w,
				do_printer, "off", NULL,
				XtNsensitive, printer_running(),
				NULL);

		if (assoc_button != NULL ||
		    lu_button != NULL ||
		    printer_off_button != NULL) {
			(void) XtCreateManagedWidget(
			    "space", cmeLineObjectClass, file_menu,
			    NULL, 0);
			printer_button = XtVaCreateManagedWidget(
			    "printerOption", cmeBSBObjectClass, file_menu,
			    XtNsensitive, IN_3270,
			    XtNrightBitmap, arrow,
			    XtNmenuName, "printerMenu",
			    NULL);
			any = True;
		} else
			XtDestroyWidget(w);
	}
#endif /*]*/

#if defined(X3270_TRACE) /*[*/
	/* Trace Data Stream
	   Trace X Events
	   Save Screen(s) in File */
	spaced = False;
	if (appres.debug_tracing) {
		any |= toggle_init(file_menu, DS_TRACE, "dsTraceOption", CN,
				&spaced);
		any |= toggle_init(file_menu, EVENT_TRACE, "eventTraceOption",
				CN, &spaced);
	}
	if (!appres.secure)
		any |= toggle_init(file_menu, SCREEN_TRACE,
				"screenTraceOption", CN, &spaced);
#endif /*]*/

	/* Print Screen Text, Save Screen Text */
	spaced = False;
	w = add_menu_itemv("printTextOption", file_menu,
			      print_text_option, NULL, &spaced,
			      NULL);
	any |= (w != NULL);
	if (!appres.secure) {
		w = add_menu_itemv("saveTextOption", file_menu,
				      save_text_option, NULL, &spaced,
				      NULL);
		any |= (w != NULL);
	}

	/* Print Window Bitmap */
	w =  add_menu_itemv("printWindowOption", file_menu,
			      print_window_option, NULL, &spaced,
			      NULL);
	any |= (w != NULL);

	if (!appres.secure) {

		/* Save Options */
		spaced = False;
		w = add_menu_itemv("saveOption", file_menu,
				      do_save_options, NULL, &spaced,
				      NULL);
		any |= (w != NULL);

		/* Execute an action */
		spaced = False;
		w = add_menu_itemv("executeActionOption", file_menu,
				      execute_action_option, NULL, &spaced,
				      NULL);
		any |= (w != NULL);
	}

#if defined(X3270_SCRIPT) /*[*/
	/* Abort script */
	spaced = False;
	script_abort_button = add_menu_itemv("abortScriptOption", file_menu,
			script_abort_callback, NULL, &spaced,
			XtNsensitive, sms_active(),
			NULL);
	any |= (script_abort_button != NULL);
#endif /*]*/

	/* Disconnect */
	spaced = False;
	disconnect_button = add_menu_itemv("disconnectOption", file_menu,
			disconnect, NULL, &spaced,
			XtNsensitive, PCONNECTED,
			NULL);
	any |= (disconnect_button != NULL);

	/* Exit x3270 */
	if (exit_menu != (Widget)NULL)
		XtDestroyWidget(exit_menu);
	exit_menu = XtVaCreatePopupShell(
	    "exitMenu", complexMenuWidgetClass, menu_parent,
	    NULL);
	/* exitReallyOption cannot be disabled */
	w = XtVaCreateManagedWidget(
	    "exitReallyOption", cmeBSBObjectClass, exit_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, Bye, NULL);
	exit_button = add_menu_itemv("exitOption", file_menu,
			Bye, NULL, &spaced,
			NULL);
	if (exit_button != NULL) {
		n_bye = 1;
		any = True;
	}

	/* File... */
	if (any) {
		if (menubar_buttons) {
			w = XtVaCreateManagedWidget(
			    "fileMenuButton", menuButtonWidgetClass, menu_parent,
			    XtNx, x,
			    XtNy, y,
			    XtNwidth, KEY_WIDTH,
			    XtNheight, KEY_HEIGHT,
			    XtNmenuName, "fileMenu",
			    NULL);
		}
	} else {
		XtDestroyWidget(file_menu);
		file_menu = NULL;
	}
}


/*
 * "Connect..." menu
 */

static Widget connect_shell = NULL;

/* Called from each button on the "Connect..." menu */
static void
host_connect_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	(void) host_connect(client_data);
}

/* Called from the lone "Connect" button on the connect dialog */
static void
connect_button_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (!host_connect(s))
		XtPopdown(connect_shell);
}

/* Called from the "Other..." button on the "Connect..." menu */
static void
do_connect_popup(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	if (connect_shell == NULL)
		connect_shell = create_form_popup("Connect",
		    connect_button_callback, (XtCallbackProc)NULL, FORM_NO_CC);
	popup_popup(connect_shell, XtGrabExclusive);
}

/*
 * Initialize the "Connect..." menu
 */
static void
connect_menu_init(Boolean regen, Position x, Position y)
{
	Widget w;
	int n_hosts = 0;
	Boolean any_hosts = False;
	struct host *h;
	Boolean need_line = False;
	int n_primary = 0;
	int n_recent = 0;
	static struct menu_hier *root = NULL;

	if (regen && (connect_menu != (Widget)NULL)) {
		XtDestroyWidget(connect_menu);
		connect_menu = (Widget)NULL;
		if (connect_button != (Widget)NULL) {
			XtDestroyWidget(connect_button);
			connect_button = (Widget)NULL;
		}
		free_menu_hier(root);
		root = NULL;
	}
	if (connect_menu != (Widget)NULL)
		return;

	/* Create the menu */
	root = (struct menu_hier *)XtCalloc(1,
			sizeof(struct menu_hier));
	root->menu_shell = connect_menu = XtVaCreatePopupShell(
	    "hostMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		need_line = True;

	/* Walk the host list from the file to produce the host menu */

	for (h = hosts; h; h = h->next) {
		switch (h->entry_type) {
		case ALIAS:
			continue;
		case PRIMARY:
			/*
			 * If there's already a 'recent' entry with the same
			 * name, skip this one.
			 */
			if (h->parents == NULL) {
				struct host *j;

				for (j = hosts;
				     j != (struct host *)NULL;
				     j = j->next) {
					if (j->entry_type != RECENT) {
						j = (struct host *)NULL;
						break;
					}
					if (!strcmp(j->name, h->name))
						break;
				}
				if (j != (struct host *)NULL)
					continue;
			}
			n_primary++;
			break;
		case RECENT:
			n_recent++;
			break;
		}
		if ((need_line && !any_hosts) ||
		    (n_recent > 0 && n_primary == 1)) {
			(void) XtVaCreateManagedWidget("space",
			    cmeLineObjectClass, connect_menu, NULL);
		}
		any_hosts = True;
		w = XtVaCreateManagedWidget(
		    h->name, cmeBSBObjectClass,
		    add_menu_hier(root, h->parents, NULL, 0), 
		    NULL);
		XtAddCallback(w, XtNcallback, host_connect_callback,
		    XtNewString(h->name));
		n_hosts++;
	}
	if (any_hosts)
		need_line = True;

	/* Add an "Other..." button at the bottom */

	if (!any_hosts || !appres.no_other) {
		if (need_line)
			(void) XtVaCreateManagedWidget("space",
			    cmeLineObjectClass,
			    connect_menu, NULL);
		w = XtVaCreateManagedWidget(
		    "otherHostOption", cmeBSBObjectClass, connect_menu,
		    NULL);
		XtAddCallback(w, XtNcallback, do_connect_popup, NULL);
	}

	/* Add the "Connect..." button itself to the menu_parent. */

	if (menubar_buttons) {
		if (n_hosts) {
			/* Connect button pops up a menu. */
			connect_button = XtVaCreateManagedWidget(
			    "connectMenuButton", menuButtonWidgetClass,
			    menu_parent,
			    XtNx, x,
			    XtNy, y,
			    XtNwidth, KEY_WIDTH,
			    XtNheight, KEY_HEIGHT,
			    XtNmenuName, "hostMenu",
			    XtNmappedWhenManaged, !PCONNECTED,
			    NULL);
		} else {
			/* Connect button pops up a dialog. */
			connect_button = XtVaCreateManagedWidget(
			    "connectMenuButton", commandWidgetClass,
			    menu_parent,
			    XtNx, x,
			    XtNy, y,
			    XtNwidth, KEY_WIDTH,
			    XtNheight, KEY_HEIGHT,
			    XtNmappedWhenManaged, !PCONNECTED,
			    NULL);
			XtAddCallback(connect_button, XtNcallback,
			    do_connect_popup, NULL);
		}
	}
}

/*
 * Callback for macros
 */
static void
do_macro(Widget w _is_unused, XtPointer client_data, XtPointer call_data _is_unused)
{
	macro_command((struct macro_def *)client_data);
}

/*
 * Initialize the "Macros..." menu
 */
static void
macros_menu_init(Boolean regen, Position x, Position y)
{
	static Widget macros_menu;
	Widget w;
	struct macro_def *m;
	Boolean any = False;
	static struct menu_hier *root = NULL;

	if (regen && (macros_menu != (Widget)NULL)) {
		XtDestroyWidget(macros_menu);
		macros_menu = (Widget)NULL;
		if (macros_button != (Widget)NULL) {
			XtDestroyWidget(macros_button);
			macros_button = (Widget)NULL;
		}
	}
	if (regen && root != NULL) {
		free_menu_hier(root);
		root = NULL;
	}
	if (macros_menu != (Widget)NULL || !PCONNECTED)
		return;

	/* Walk the list */

	macros_init();	/* possibly different for each host */
	root = (struct menu_hier *)XtCalloc(1, sizeof(struct menu_hier));
	for (m = macro_defs; m; m = m->next) {
		if (!any) {
			/* Create the menu */
			root->menu_shell = macros_menu = XtVaCreatePopupShell(
			    MACROS_MENU, complexMenuWidgetClass, menu_parent,
			    menubar_buttons ? XtNlabel : NULL, NULL,
			    NULL);
			if (!menubar_buttons)
				(void) XtVaCreateManagedWidget("space",
				    cmeLineObjectClass, macros_menu, NULL);
		}
		w = XtVaCreateManagedWidget(
		    m->name, cmeBSBObjectClass,
		    add_menu_hier(root, m->parents, NULL, 0), 
		    NULL);
		XtAddCallback(w, XtNcallback, do_macro, (XtPointer)m);
		any = True;
	}

	/* Add the "Macros..." button itself to the menu_parent */

	if (any && menubar_buttons)
		macros_button = XtVaCreateManagedWidget(
		    "macrosMenuButton", menuButtonWidgetClass,
		    menu_parent,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, KEY_WIDTH,
		    XtNheight, KEY_HEIGHT,
		    XtNmenuName, MACROS_MENU,
		    NULL);
}

#if defined(X3270_KEYPAD) /*[*/
/* Called toggle the keypad */
static void
toggle_keypad(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	switch (kp_placement) {
	    case kp_integral:
		screen_showikeypad(appres.keypad_on = !appres.keypad_on);
		break;
	    case kp_left:
	    case kp_right:
	    case kp_bottom:
	    case kp_inside_right:
		keypad_popup_init();
		if (keypad_popped) 
			XtPopdown(keypad_shell);
		else
			popup_popup(keypad_shell, XtGrabNone);
		break;
	}
	menubar_keypad_changed();
	keypad_changed = True;
}

static void
keypad_button_init(Position x, Position y)
{
	if (!menubar_buttons)
		return;
	if (keypad_button == (Widget)NULL) {
		Pixmap pixmap;

		pixmap = XCreateBitmapFromData(display, root_window,
		    (char *) ky_bits, ky_width, ky_height);
		keypad_button = XtVaCreateManagedWidget(
		    "keypadButton", commandWidgetClass, menu_parent,
		    XtNbitmap, pixmap,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, ky_width+8,
		    XtNheight, KEY_HEIGHT,
		    NULL);
		XtAddCallback(keypad_button, XtNcallback,
		    toggle_keypad, NULL);
	} else {
		XtVaSetValues(keypad_button, XtNx, x, NULL);
	}
}
#endif /*]*/

#if defined(HAVE_LIBSSL) /*[*/
static void
ssl_icon_init(Position x, Position y)
{
	if (!menubar_buttons)
		return;
	if (locked_icon == (Widget)NULL) {
		Pixmap pixmap;

		pixmap = XCreateBitmapFromData(display, root_window,
		    (char *) locked_bits, locked_width, locked_height);
		locked_icon = XtVaCreateManagedWidget(
		    "lockedIcon", labelWidgetClass, menu_parent,
		    XtNbitmap, pixmap,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, locked_width+8,
		    XtNheight, KEY_HEIGHT,
		    XtNmappedWhenManaged, CONNECTED && secure_connection,
		    NULL);
		pixmap = XCreateBitmapFromData(display, root_window,
		    (char *) unlocked_bits, unlocked_width, unlocked_height);
		unlocked_icon = XtVaCreateManagedWidget(
		    "unlockedIcon", labelWidgetClass, menu_parent,
		    XtNbitmap, pixmap,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, unlocked_width+8,
		    XtNheight, KEY_HEIGHT,
		    XtNmappedWhenManaged, CONNECTED && !secure_connection,
		    NULL);
	} else {
		XtVaSetValues(locked_icon, XtNx, x, NULL);
		XtVaSetValues(unlocked_icon, XtNx, x, NULL);
	}
}
#endif /*]*/

void
menubar_resize(Dimension width)
{
#if defined(HAVE_LIBSSL) /*[*/
	ssl_icon_init(
	    (Position) (width - LEFT_MARGIN -
#if defined(X3270_KEYPAD) /*[*/
			    (ky_width+8) -
#endif /*]*/
			    4*BORDER - 2*MENU_BORDER - (locked_width+8)),
	    TOP_MARGIN);
#endif /*]*/
#if defined(X3270_KEYPAD) /*[*/
	keypad_button_init(
	    (Position) (width - LEFT_MARGIN - (ky_width+8) - 2*BORDER),
	    TOP_MARGIN);
#endif /*]*/
}


/*
 * "Options..." menu
 */

static void
toggle_callback(Widget w, XtPointer userdata, XtPointer calldata _is_unused)
{
	struct toggle *t = (struct toggle *) userdata;

	/*
	 * If this is a two-button radio group, rather than a simple toggle,
	 * there is nothing to do if they are clicking on the current value.
	 *
	 * t->w[0] is the "toggle true" button; t->w[1] is "toggle false".
	 */
	if (t->w[1] != 0 && w == t->w[!t->value])
		return;

	do_toggle(t - appres.toggle);
}

static Widget oversize_shell = NULL;

/* Called from the "Change" button on the oversize dialog */
static void
oversize_button_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *s;
	int ovc, ovr;
	char junk;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (sscanf(s, "%dx%d%c", &ovc, &ovr, &junk) == 2) {
		XtPopdown(oversize_shell);
		screen_change_model(model_num, ovc, ovr);
	} else
		popup_an_error("Illegal size: %s", s);
}

/* Called from the "Oversize..." button on the "Models..." menu */
static void
do_oversize_popup(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	if (oversize_shell == NULL)
		oversize_shell = create_form_popup("Oversize",
		    oversize_button_callback, (XtCallbackProc)NULL,
		    FORM_NO_WHITE);
	popup_popup(oversize_shell, XtGrabExclusive);
}

/* Init a toggle, menu-wise */
static Boolean
toggle_init(Widget menu, int ix, const char *name1, const char *name2,
		Boolean *spaced)
{
	struct toggle *t = &appres.toggle[ix];

	if (!item_suppressed(menu, name1) &&
	    (name2 == NULL || !item_suppressed(menu, name2))) {
		if (spaced != NULL)
			cond_space(menu, spaced);
		t->label[0] = name1;
		t->label[1] = name2;
		t->w[0] = XtVaCreateManagedWidget(
		    name1, cmeBSBObjectClass, menu,
		    XtNleftBitmap,
		     t->value? (name2? diamond: dot):
		               (name2? no_diamond: None),
		    NULL);
		XtAddCallback(t->w[0], XtNcallback, toggle_callback,
				(XtPointer) t);
		if (name2 != NULL) {
			t->w[1] = XtVaCreateManagedWidget(
			    name2, cmeBSBObjectClass, menu,
			    XtNleftBitmap, t->value? no_diamond: diamond,
			    NULL);
			XtAddCallback(t->w[1], XtNcallback, toggle_callback,
			    (XtPointer) t);
		} else
			t->w[1] = NULL;
		return True;
	} else
		return False;
}

static Widget *font_widgets = NULL;
static Widget other_font;
static Widget font_shell = NULL;

static void
do_newfont(Widget w _is_unused, XtPointer userdata, XtPointer calldata _is_unused)
{
	screen_newfont((char *)userdata, True, False);
}

/* Called from the "Select Font" button on the font dialog */
static void
font_button_callback(Widget w, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	XtPopdown(font_shell);
	do_newfont(w, s, PN);
}

static void
do_otherfont(Widget w _is_unused, XtPointer userdata _is_unused,
    XtPointer calldata _is_unused)
{
	if (font_shell == NULL)
		font_shell = create_form_popup("Font", font_button_callback,
						(XtCallbackProc)NULL,
						FORM_NO_CC);
	popup_popup(font_shell, XtGrabExclusive);
}

/* Initialze the color scheme list. */
static void
scheme_init(void)
{
	char *cm;
	char *label;
	char *scheme;
	struct scheme *s;

	cm = get_resource(ResSchemeList);
	if (cm == CN)
		return;
	cm = XtNewString(cm);

	scheme_count = 0;
	while (split_dresource(&cm, &label, &scheme) == 1) {
		s = (struct scheme *)XtMalloc(sizeof(struct scheme));
		if (!split_hier(label, &s->label, &s->parents)) {
			XtFree((XtPointer)s);
			continue;
		}
		s->label = label;
		s->scheme = scheme;
		s->next = (struct scheme *)NULL;
		if (last_scheme != (struct scheme *)NULL)
			last_scheme->next = s;
		else
			schemes = s;
		last_scheme = s;
		scheme_count++;
	}
}

static void
do_newscheme(Widget w _is_unused, XtPointer userdata, XtPointer calldata _is_unused)
{
	screen_newscheme((char *)userdata);
}

/* Initialze the character set list. */
static void
charsets_init(void)
{
	char *cm;
	char *label;
	char *charset;
	struct charset *s;
	static char *vgcm;

	cm = get_resource(ResCharsetList);
	if (cm == CN)
		return;
	vgcm = cm = XtNewString(cm);

	charset_count = 0;
	while (split_dresource(&cm, &label, &charset) == 1) {
		s = (struct charset *)XtMalloc(sizeof(struct charset));
		if (!split_hier(label, &s->label, &s->parents)) {
			XtFree((XtPointer)s);
			continue;
		}
		s->charset = charset;
		s->next = (struct charset *)NULL;
		if (last_charset != (struct charset *)NULL)
			last_charset->next = s;
		else
			charsets = s;
		last_charset = s;
		charset_count++;
	}
}

static void
do_newcharset(Widget w _is_unused, XtPointer userdata, XtPointer calldata _is_unused)
{
	struct charset *s;
	int i;

	/* Change the character set. */
	screen_newcharset((char *)userdata);

	/* Update the menu. */
	for (i = 0, s = charsets; i < charset_count; i++, s = s->next)
		XtVaSetValues(charset_widgets[i],
		    XtNleftBitmap,
			(strcmp(get_charset_name(), s->charset)) ?
			    no_diamond : diamond,
		    NULL);
}

static Widget keymap_shell = NULL;

/* Called from the "Set Keymap" button on the keymap dialog */
static void
keymap_button_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (s != CN && !*s)
		s = CN;
	XtPopdown(keymap_shell);
	keymap_init(s, True);
}

/* Callback from the "Keymap" menu option */
static void
do_keymap(Widget w _is_unused, XtPointer userdata _is_unused, XtPointer calldata _is_unused)
{
	if (keymap_shell == NULL)
		keymap_shell = create_form_popup("Keymap",
		    keymap_button_callback, (XtCallbackProc)NULL,
		    FORM_NO_WHITE);
	popup_popup(keymap_shell, XtGrabExclusive);
}

#if defined(X3270_SCRIPT) /*[*/
/* Callback from the "Idle Command" menu option */
static void
do_idle_command(Widget w _is_unused, XtPointer userdata _is_unused,
    XtPointer calldata _is_unused)
{
	popup_idle();
}
#endif /*]*/

/* Called to change telnet modes */
static void
linemode_callback(Widget w _is_unused, XtPointer client_data _is_unused, XtPointer call_data _is_unused)
{
	net_linemode();
}

static void
charmode_callback(Widget w _is_unused, XtPointer client_data _is_unused, 
    XtPointer call_data _is_unused)
{
	net_charmode();
}

/* Called to change models */
static void
change_model_callback(Widget w, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	int m;

	m = atoi(client_data);
	switch (model_num) {
	case 2:
		if (model_2_button != NULL)
			XtVaSetValues(model_2_button, XtNleftBitmap, no_diamond,
					NULL);
		break;
	case 3:
		if (model_3_button != NULL)
			XtVaSetValues(model_3_button, XtNleftBitmap, no_diamond,
					NULL);
		break;
	case 4:
		if (model_4_button != NULL)
			XtVaSetValues(model_4_button, XtNleftBitmap, no_diamond,
					NULL);
		break;
	case 5:
		if (model_5_button != NULL)
			XtVaSetValues(model_5_button, XtNleftBitmap, no_diamond,
					NULL);
		break;
	}
	XtVaSetValues(w, XtNleftBitmap, diamond, NULL);
	screen_change_model(m, 0, 0);
}

/* Called to when model changes outside our control */
static void
menubar_remodel(Boolean ignored _is_unused)
{
	if (model_2_button != NULL)
		XtVaSetValues(model_2_button, XtNleftBitmap,
				(model_num == 2)? diamond: no_diamond, NULL);
	if (model_2_button != NULL)
		XtVaSetValues(model_3_button, XtNleftBitmap,
				(model_num == 3)? diamond: no_diamond, NULL);
	if (model_2_button != NULL)
		XtVaSetValues(model_4_button, XtNleftBitmap,
				(model_num == 4)? diamond: no_diamond, NULL);
	if (model_2_button != NULL)
		XtVaSetValues(model_5_button, XtNleftBitmap,
				(model_num == 5)? diamond: no_diamond, NULL);
}

/* Compare a font name to the current emulator font name. */
static Boolean
is_efont(const char *font_name)
{
	return !strcmp(NO_BANG(font_name), NO_BANG(efontname)) ||
	       !strcmp(NO_BANG(font_name), NO_BANG(full_efontname));
}

/* Create, or re-create the font menu. */
static void
create_font_menu(Boolean regen, Boolean even_if_unknown)
{
	Widget t;
	struct font_list *f;
	int ix;
	int count;
	static struct menu_hier *root = NULL;

	if (root != NULL) {
		XtDestroyWidget(root->menu_shell);
		free_menu_hier(root);
		root = NULL;
	}
	Free(font_widgets);

	root = (struct menu_hier *)XtCalloc(1, sizeof(struct menu_hier));
	root->menu_shell = t = XtVaCreatePopupShell(
	    "fontsMenu", complexMenuWidgetClass, menu_parent,
	    XtNborderWidth, fm_borderWidth,
	    XtNborderColor, fm_borderColor,
	    XtNbackground, fm_background,
	    NULL);
	count = font_count;
	if (font_count)
		font_widgets = (Widget *)XtCalloc(count, sizeof(Widget));
	else
		font_widgets = NULL;
	for (f = font_list, ix = 0; f; f = f->next, ix++) {
		Arg args[3];

		XtSetArg(args[0], XtNleftMargin, fm_leftMargin);
		XtSetArg(args[1], XtNrightMargin, fm_rightMargin);
		XtSetArg(args[2], XtNbackground, fm_background);
		font_widgets[ix] = XtVaCreateManagedWidget(
		    f->label, cmeBSBObjectClass,
		    add_menu_hier(root, f->parents, args, 3),
		    XtNleftBitmap,
			is_efont(f->font)? diamond: no_diamond,
		    XtNleftMargin, fm_leftMargin,
		    XtNrightMargin, fm_rightMargin,
		    XtNbackground, fm_background,
		    NULL);
		XtAddCallback(font_widgets[ix], XtNcallback, do_newfont,
		    XtNewString(f->font));
	}
	if (!appres.no_other) {
		other_font = XtVaCreateManagedWidget(
		    "otherFontOption", cmeBSBObjectClass, t,
		    NULL);
		XtAddCallback(other_font, XtNcallback, do_otherfont, NULL);
	}

	XtVaSetValues(fonts_option, XtNmenuName, "fontsMenu", NULL);
}

/* Called when the character set changes. */
static void
menubar_charset(Boolean ignored _is_unused)
{
	if (!appres.suppress_font_menu)
		create_font_menu(False, False);
}

/* Called to change emulation modes */
static void
toggle_extended(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	appres.extended = !appres.extended;
	if (extended_button != NULL)
		XtVaSetValues(extended_button,
				XtNleftBitmap, appres.extended ? dot : (Pixmap)NULL,
				NULL);
	if (oversize_button != NULL)
		XtVaSetValues(oversize_button,
				XtNsensitive, appres.extended,
				NULL);
	if (!appres.extended)
		screen_change_model(model_num, 0, 0);
	screen_extended(appres.extended);
}

static void
toggle_m3279(Widget w, XtPointer client_data _is_unused, XtPointer call_data _is_unused)
{
	if (w == m3278_button)
		appres.m3279 = False;
	else if (w == m3279_button)
		appres.m3279 = True;
	else
		return;
	XtVaSetValues(m3278_button, XtNleftBitmap,
		appres.m3279 ? no_diamond : diamond,
		NULL);
	XtVaSetValues(m3279_button, XtNleftBitmap,
		appres.m3279 ? diamond : no_diamond,
		NULL);
#if defined(RESTRICT_3279) /*[*/
	if (model_4_button != NULL)
		XtVaSetValues(model_4_button, XtNsensitive, !appres.m3279,
				NULL);
	if (model_5_button != NULL)
		XtVaSetValues(model_5_button, XtNsensitive, !appres.m3279,
				NULL);
	if (model_num == 4 || model_num == 5)
		screen_change_model(3, 0, 0);
#endif /*]*/
	if (scheme_button != (Widget)NULL)
		XtVaSetValues(scheme_button, XtNsensitive, appres.m3279, NULL);
	screen_m3279(appres.m3279);
}

static void
options_menu_init(Boolean regen, Position x, Position y)
{
	Widget t;
	struct font_list *f;
	struct scheme *s;
	int ix;
	static Widget options_menu_button = NULL;
	Widget dummy_font_menu, dummy_font_element;
	static struct menu_hier *scheme_root = NULL;
	static struct menu_hier *charset_root = NULL;
	Boolean spaced = False;
	Boolean any = False;
	Widget w;

	if (regen && (options_menu != (Widget)NULL)) {
		XtDestroyWidget(options_menu);
		options_menu = (Widget)NULL;
		if (options_menu_button != NULL) {
			XtDestroyWidget(options_menu_button);
			options_menu_button = NULL;
		}
	}
	if (options_menu != (Widget)NULL) {
		if (font_widgets != NULL) {
			/* Set the current font. */
			for (f = font_list, ix = 0; f; f = f->next, ix++) {
				XtVaSetValues(font_widgets[ix], XtNleftBitmap,
					is_efont(f->font)? diamond: no_diamond,
					NULL);
			}
		}
		/* Set the current color scheme. */
		s = schemes;
		for (ix = 0, s = schemes;
		     ix < scheme_count;
		     ix++, s = s->next) {
			XtVaSetValues(scheme_widgets[ix], XtNleftBitmap,
				!strcmp(appres.color_scheme, s->scheme) ?
				    diamond : no_diamond,
			    NULL);
		}
		return;
	}

	/* Create the shell */
	options_menu = XtVaCreatePopupShell(
	    "optionsMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu, NULL);

	/* Create the "toggles" pullright */
	if (!item_suppressed(options_menu, "togglesOption")) {
		t = XtVaCreatePopupShell(
		    "togglesMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
#if defined(X3270_KEYPAD) /*[*/
		if (!menubar_buttons) {
			keypad_option_button = add_menu_itemv("keypadOption", t,
					toggle_keypad, NULL,
					NULL,
					XtNleftBitmap,
						(appres.keypad_on || keypad_popped)?
						dot : None,
					NULL);
			if (keypad_option_button != NULL)
				spaced = False;
			else
				spaced = True;
		}
#endif /*]*/
		toggle_init(t, MONOCASE, "monocaseOption", CN, &spaced);
		toggle_init(t, CURSOR_BLINK, "cursorBlinkOption", CN, &spaced);
		toggle_init(t, BLANK_FILL, "blankFillOption", CN, &spaced);
		toggle_init(t, SHOW_TIMING, "showTimingOption", CN, &spaced);
		toggle_init(t, CURSOR_POS, "cursorPosOption", CN, &spaced);
		toggle_init(t, SCROLL_BAR, "scrollBarOption", CN, &spaced);
#if defined(X3270_ANSI) /*[*/
		toggle_init(t, LINE_WRAP, "lineWrapOption", CN, &spaced);
#endif /*]*/
		toggle_init(t, MARGINED_PASTE, "marginedPasteOption", CN, &spaced);
		toggle_init(t, RECTANGLE_SELECT, "rectangleSelectOption", CN, &spaced);
		toggle_init(t, CROSSHAIR, "crosshairOption", CN, &spaced);
		toggle_init(t, VISIBLE_CONTROL, "visibleControlOption", CN, &spaced);
		spaced = False;
		toggle_init(t, ALT_CURSOR, "underlineCursorOption",
		    "blockCursorOption", &spaced);
		spaced = False;
		linemode_button = add_menu_itemv("lineModeOption", t,
				linemode_callback, NULL,
				&spaced,
				XtNleftBitmap, linemode? diamond: no_diamond,
				XtNsensitive, IN_ANSI,
				NULL);
		charmode_button = add_menu_itemv("characterModeOption", t,
				charmode_callback, NULL,
				&spaced,
				XtNleftBitmap, linemode? no_diamond: diamond,
				XtNsensitive, IN_ANSI,
				NULL);
		if (!appres.mono) {
			spaced = False;
			m3278_button = add_menu_itemv( "m3278Option", t,
					toggle_m3279, NULL,
					&spaced,
					XtNleftBitmap,
						appres.m3279? no_diamond: diamond,
					XtNsensitive, !PCONNECTED,
					NULL);
			m3279_button = add_menu_itemv("m3279Option", t,
					toggle_m3279, NULL,
					&spaced,
					XtNleftBitmap,
						appres.m3279? diamond: no_diamond,
					XtNsensitive, !PCONNECTED,
					NULL);
		}
		spaced = False;
		extended_button = add_menu_itemv("extendedDsOption", t,
				toggle_extended, NULL,
				&spaced,
				XtNleftBitmap, appres.extended? dot: (Pixmap)NULL,
				XtNsensitive, !PCONNECTED,
				NULL);
		if (keypad_option_button != NULL ||
		    appres.toggle[MONOCASE].w[0] != NULL ||
		    appres.toggle[CURSOR_BLINK].w[0] != NULL ||
		    appres.toggle[BLANK_FILL].w[0] != NULL ||
		    appres.toggle[SHOW_TIMING].w[0] != NULL ||
		    appres.toggle[CURSOR_POS].w[0] != NULL ||
		    appres.toggle[SCROLL_BAR].w[0] != NULL ||
#if defined(X3270_ANSI) /*[*/
		    appres.toggle[LINE_WRAP].w[0] != NULL ||
#endif /*]*/
		    appres.toggle[MARGINED_PASTE].w[0] != NULL ||
		    appres.toggle[RECTANGLE_SELECT].w[0] != NULL ||
		    appres.toggle[CROSSHAIR].w[0] != NULL ||
		    appres.toggle[VISIBLE_CONTROL].w[0] != NULL ||
		    appres.toggle[ALT_CURSOR].w[0] != NULL ||
		    linemode_button != NULL ||
		    charmode_button != NULL ||
		    m3278_button != NULL ||
		    m3279_button != NULL) {
			(void) XtVaCreateManagedWidget(
			    "togglesOption", cmeBSBObjectClass, options_menu,
			    XtNrightBitmap, arrow,
			    XtNmenuName, "togglesMenu",
			    NULL);
			any = True;
		} else
			XtDestroyWidget(t);
	}

	if (!appres.suppress_font_menu &&
			!item_suppressed(options_menu, "fontsOption")) {
		/* Create the "fonts" pullright */

		/*
		 * Create a dummy menu with the well-known name, so we can get
		 * the values of background, borderWidth, borderColor and
		 * leftMargin from its resources.
		 */
		dummy_font_menu = XtVaCreatePopupShell(
		    "fontsMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		dummy_font_element =  XtVaCreateManagedWidget(
		    "entry", cmeBSBObjectClass, dummy_font_menu,
		    XtNleftBitmap, no_diamond,
		    NULL);
		XtRealizeWidget(dummy_font_menu);
		XtVaGetValues(dummy_font_menu,
		    XtNborderWidth, &fm_borderWidth,
		    XtNborderColor, &fm_borderColor,
		    XtNbackground, &fm_background,
		    NULL);
		XtVaGetValues(dummy_font_element,
		    XtNleftMargin, &fm_leftMargin,
		    XtNrightMargin, &fm_rightMargin,
		    NULL);
		XtDestroyWidget(dummy_font_menu);

		(void) XtVaCreateManagedWidget(
		    "space", cmeLineObjectClass, options_menu,
		    NULL);
		fonts_option = XtVaCreateManagedWidget(
		    "fontsOption", cmeBSBObjectClass, options_menu,
		    XtNrightBitmap, arrow,
		    NULL);
		create_font_menu(regen, True);
		any = True;
	}

	/* Create the "models" pullright */
	if (!item_suppressed(options_menu, "modelsOption")) {
		t = XtVaCreatePopupShell(
		    "modelsMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		model_2_button = add_menu_itemv("model2Option", t,
				change_model_callback, NewString("2"),
				NULL,
				XtNleftBitmap, (model_num == 2)?
					diamond: no_diamond,
				NULL);
		model_3_button = add_menu_itemv("model3Option", t,
				change_model_callback, NewString("3"),
				NULL,
				XtNleftBitmap, (model_num == 3)?
					diamond: no_diamond,
				NULL);
		model_4_button = add_menu_itemv("model4Option", t,
				change_model_callback, NewString("4"),
				NULL,
				XtNleftBitmap, (model_num == 4)?
					diamond: no_diamond,
#if defined(RESTRICT_3279) /*[*/
				XtNsensitive, !appres.m3279,
#endif /*]*/
				NULL);
		model_5_button = add_menu_itemv("model5Option", t,
				change_model_callback, NewString("5"),
				NULL,
				XtNleftBitmap, (model_num == 5)?
					diamond: no_diamond,
#if defined(RESTRICT_3279) /*[*/
				XtNsensitive, !appres.m3279,
#endif /*]*/
				NULL);
		oversize_button = add_menu_itemv("oversizeOption", t,
				do_oversize_popup, NULL,
				NULL,
				XtNsensitive, appres.extended,
				NULL);
		if (model_2_button != NULL ||
		    model_3_button != NULL ||
		    model_4_button != NULL ||
		    model_5_button != NULL ||
		    oversize_button != NULL) {
			(void) XtVaCreateManagedWidget(
			    "space", cmeLineObjectClass, options_menu,
			    NULL);
			models_option = XtVaCreateManagedWidget(
			    "modelsOption", cmeBSBObjectClass, options_menu,
			    XtNrightBitmap, arrow,
			    XtNmenuName, "modelsMenu",
			    XtNsensitive, !PCONNECTED,
			    NULL);
			any = True;
		} else
			XtDestroyWidget(t);
	}

	/* Create the "colors" pullright */
	if (scheme_count && !item_suppressed(options_menu, "colorsOption")) {

		scheme_widgets = (Widget *)XtCalloc(scheme_count,
		    sizeof(Widget));
		if (scheme_root != NULL)
		    	free_menu_hier(scheme_root);
		scheme_root = (struct menu_hier *)XtCalloc(1,
				sizeof(struct menu_hier));
		scheme_root->menu_shell = XtVaCreatePopupShell(
		    "colorsMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		s = schemes;
		for (ix = 0, s = schemes; ix < scheme_count; ix++, s = s->next) {
			scheme_widgets[ix] = XtVaCreateManagedWidget(
			    s->label, cmeBSBObjectClass,
			    add_menu_hier(scheme_root, s->parents, NULL, 0),
			    XtNleftBitmap,
				!strcmp(appres.color_scheme, s->scheme) ?
				    diamond : no_diamond,
			    NULL);
			XtAddCallback(scheme_widgets[ix], XtNcallback,
			    do_newscheme, s->scheme);
		}
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu,
		    NULL);
		scheme_button = XtVaCreateManagedWidget(
		    "colorsOption", cmeBSBObjectClass, options_menu,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "colorsMenu",
		    XtNsensitive, appres.m3279,
		    NULL);
		any = True;
	}

	/* Create the "character set" pullright */
	if (charset_count && !item_suppressed(options_menu, "charsetOption")) {
		struct charset *cs;

		if (charset_root != NULL)
		    	free_menu_hier(charset_root);
		charset_root = (struct menu_hier *)XtCalloc(1,
				sizeof(struct menu_hier));
		charset_root->menu_shell = XtVaCreatePopupShell(
		    "charsetMenu", complexMenuWidgetClass, menu_parent,
		    NULL);

		charset_widgets = (Widget *)XtCalloc(charset_count,
		    sizeof(Widget));
		for (ix = 0, cs = charsets;
                     ix < charset_count;
                     ix++, cs = cs->next) {
			t = add_menu_hier(charset_root, cs->parents, NULL, 0);
			charset_widgets[ix] = XtVaCreateManagedWidget(
			    cs->label, cmeBSBObjectClass, t,
			    XtNleftBitmap,
				(strcmp(get_charset_name(), cs->charset)) ?
				    no_diamond : diamond,
			    NULL);
			XtAddCallback(charset_widgets[ix], XtNcallback,
			    do_newcharset, cs->charset);
		}

		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu,
		    NULL);
		(void) XtVaCreateManagedWidget(
		    "charsetOption", cmeBSBObjectClass, options_menu,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "charsetMenu",
		    NULL);
		any = True;
	}

	/* Create the "keymap" option */
	if (!appres.no_other) {
		spaced = False;
		w = add_menu_itemv("keymapOption", options_menu,
				      do_keymap, NULL,
				      &spaced,
				      NULL);
		any |= (w != NULL);
	}

	/* Create the "display keymap" option */
	spaced = False;
	w = add_menu_itemv("keymapDisplayOption", options_menu,
			      do_keymap_display, NULL,
			      &spaced,
			      NULL);
	any |= (w != NULL);

#if defined(X3270_SCRIPT) /*[*/
	/* Create the "Idle Command" option */
	if (!appres.secure) {
		spaced = False;
		idle_button = add_menu_itemv("idleCommandOption", options_menu,
				do_idle_command, NULL,
				&spaced,
				XtNsensitive, IN_3270,
				NULL);
		any |= (idle_button != NULL);
	}
#endif /*]*/

	if (any) {
		if (menubar_buttons) {
			options_menu_button = XtVaCreateManagedWidget(
			    "optionsMenuButton", menuButtonWidgetClass, menu_parent,
			    XtNx, x,
			    XtNy, y,
			    XtNwidth, KEY_WIDTH,
			    XtNheight, KEY_HEIGHT,
			    XtNmenuName, "optionsMenu",
			    NULL);
			keypad_option_button = NULL;
		}
	} else {
		XtDestroyWidget(options_menu);
		options_menu = NULL;
	}
}

/*
 * Change a menu checkmark
 */
void
menubar_retoggle(struct toggle *t)
{
	if (t->w[0] != NULL)
		XtVaSetValues(t->w[0],
		    XtNleftBitmap, t->value ? (t->w[1] ? diamond : dot) : None,
		    NULL);
	if (t->w[1] != NULL)
		XtVaSetValues(t->w[1],
		    XtNleftBitmap, t->value ? no_diamond : diamond,
		    NULL);
}

void
HandleMenu_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	String p;

	action_debug(HandleMenu_action, event, params, num_params);
	if (check_usage(HandleMenu_action, *num_params, 1, 2) < 0)
		return;
	if (!CONNECTED || *num_params == 1)
		p = params[0];
	else
		p = params[1];
	if (!XtNameToWidget(menu_parent, p)) {
#if 0
		if (strcmp(p, MACROS_MENU))
			popup_an_error("%s: cannot find menu %s",
			    action_name(HandleMenu_action), p);
#endif
		return;
	}
	XtCallActionProc(menu_parent, "XawPositionComplexMenu", event, &p, 1);
	XtCallActionProc(menu_parent, "MenuPopup", event, &p, 1);
}

#endif /*]*/
