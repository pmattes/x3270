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
 *	menubar.c
 *		This module handles the menu bar.
 */

#include "globals.h"
#include "xglobals.h"

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

#include "actions.h"
#include "about.h"
#include "codepage.h"
#include "ft_private.h"
#include "ft_gui.h"
#include "host.h"
#include "idle_gui.h"
#include "keymap.h"
#include "kybd.h"
#include "menubar.h"
#include "model.h"
#include "names.h"
#include "popups.h"
#include "print_screen.h"
#include "print_window.h"
#include "pr3287_session.h"
#include "printer_gui.h"
#include "stmenu.h"
#include "task.h"
#include "telnet.h"
#include "toggles.h"
#include "unicodec.h"
#include "utils.h"
#include "xaa.h"
#include "xappres.h"
#include "xactions.h"
#include "xkeypad.h"
#include "xft_gui.h"
#include "xio.h"
#include "xmenubar.h"
#include "xpopups.h"
#include "xsave.h"
#include "xscreen.h"

#define MACROS_MENU	"macrosMenu"

/* Menu widgets associated with toggles. */
toggle_widget_t toggle_widget[N_TOGGLES];

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
static bool snap_enabled = true;
static bool keypad_sensitive = true;
static struct codepage {
    char **parents;
    char *label;
    char *codepage;
    struct codepage *next;
} *codepages, *last_codepage;
static int codepage_count;
static Widget  *codepage_widgets;
static struct host_list {
    char *name;
    struct host_list *next;
} *host_list;

static void scheme_init(void);
static void codepages_init(void);
static void options_menu_init(bool regen, Position x, Position y);
static void keypad_button_init(Position x, Position y);
static void tls_icon_init(Position x, Position y);
static void connect_menu_init(bool regen, Position x, Position y);
static void macros_menu_init(bool regen, Position x, Position y);
static void file_menu_init(bool regen, Dimension x, Dimension y);
static void Bye(Widget w, XtPointer client_data, XtPointer call_data);
static void menubar_in3270(bool in3270);
static void menubar_linemode(bool in_linemode);
static void menubar_connect(bool ignored);
static void menubar_printer(bool printer_on);
static void menubar_remodel(bool ignored _is_unused);
static void menubar_codepage(bool ignored _is_unused);
static void menubar_keyboard_disable(bool ignored _is_unused);
static void screensave_option(Widget w, XtPointer client_data,
	XtPointer call_data);

#define NO_BANG(s)	(((s)[0] == '!')? (s) + 1: (s))

#include "dot.bm"
#include "dot15.bm"
#include "dot20.bm"

#include "no_dot.bm"
#include "no_dot15.bm"
#include "no_dot20.bm"

#include "arrow.bm"
#include "arrow15.bm"
#include "arrow20.bm"

#include "diamond.bm"
#include "diamond15.bm"
#include "diamond20.bm"

#include "no_diamond.bm"
#include "no_diamond15.bm"
#include "no_diamond20.bm"

#include "ky.bm"
#include "ky15.bm"
#include "ky20.bm"

#include "locked.bm"
#include "locked15.bm"
#include "locked20.bm"

#include "unlocked.bm"
#include "unlocked15.bm"
#include "unlocked20.bm"

#include "null.bm"

/*
 * Menu Bar
 */

static Widget	menu_parent;
static bool  	menubar_buttons;
static Widget   disconnect_button;
static Widget   exit_button;
static Widget   exit_menu;
static Widget   macros_button;
static Widget	ft_button;
static Widget	printer_button;
static Widget	assoc_button;
static Widget	lu_button;
static Widget	printer_off_button;
static Widget   connect_button;
static Widget   locked_icon;
static Widget   unlocked_icon;
static Widget   unverified_icon;
static Widget   keypad_button;
static Widget	retry_button;
static Widget	reconnect_button;
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
static Widget	script_abort_button;
static Widget	idle_button;
static Widget	snap_button;
static Widget	reenable_button;
static Widget	save_input_button;
static Widget	restore_input_button;

static int scaled_locked_width;
static int scaled_locked_height;
static int scaled_unlocked_width;
static int scaled_unlocked_height;
static unsigned char *scaled_locked_bits;
static unsigned char *scaled_unlocked_bits;

static int scaled_dot_width;
static int scaled_dot_height;
static int scaled_no_dot_width;
static int scaled_no_dot_height;
static unsigned char *scaled_dot_bits;
static unsigned char *scaled_no_dot_bits;

static int scaled_diamond_width;
static int scaled_diamond_height;
static int scaled_no_diamond_width;
static int scaled_no_diamond_height;
static unsigned char *scaled_diamond_bits;
static unsigned char *scaled_no_diamond_bits;

static int scaled_ky_width;
static int scaled_ky_height;
static unsigned char *scaled_ky_bits;

static int scaled_arrow_width;
static int scaled_arrow_height;
static unsigned char *scaled_arrow_bits;

static Pixmap   arrow;
Pixmap	dot;
Pixmap	no_dot;
Pixmap	diamond;
Pixmap	no_diamond;
Pixmap	null;

static int	n_bye;

static bool	toggle_init(Widget, int, const char *, const char *, bool *);

#define TOP_MARGIN	rescale(3)
#define BOTTOM_MARGIN	rescale(3)
#define LEFT_MARGIN	rescale(3)
#define KEY_HEIGHT	rescale(18)
#define KEY_WIDTH	rescale(70)
#define BORDER		rescale(1)
#define SPACING		rescale(3)

#define BUTTON_X(n)	(LEFT_MARGIN + (n)*(KEY_WIDTH + 2 * BORDER + SPACING))

#define MENU_BORDER	rescale(2)

#define KY_WIDTH	(scaled_ky_width + rescale(8))

#define	MENU_MIN_WIDTH	(LEFT_MARGIN + 3 * (KEY_WIDTH + 2 * BORDER + SPACING) + \
			 LEFT_MARGIN + KY_WIDTH + 2 * BORDER + SPACING + \
			 2 * MENU_BORDER)

/* Menu hierarchy structure. */
struct menu_hier {
    Widget menu_shell;		/* complexMenu widget */
    char *name;			/* my name (root name is NULL) */
    char *menu_name;		/* menu name */
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

	if (h->name != NULL && !strcmp(h->name, *parents)) {
	    break;
	}
	last_child = h->child;
	for (child = h->child; child != NULL; child = child->sibling) {
	    if (!strcmp(child->name, *parents)) {
		break;
	    }
	    last_child = child;
	}
	if (child != NULL) {
	    h = child;
	} else {
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
	    if (last_child != NULL) {
		last_child->sibling = new_child;
	    } else {
		h->child = new_child;
	    }
	    h = new_child;

	    /*
	     * Create a menu for the children of this new
	     * intermediate node.
	     */
	    sprintf(namebuf, "csMenu%d", menu_num++);
	    for (i = 0, m = namebuf + strlen(namebuf);
		 (*parents)[i] && ((size_t)(m - namebuf) < sizeof(namebuf));
		 i++) {
		if (isalnum((unsigned char)(*parents)[i])) {
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
	    h->menu_name = menu_name;
	    merged_args = XtMergeArgLists(my_arglist, 2, args, num_args);
	    XtCreateManagedWidget(
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
    if (root->sibling) {
	free_menu_hier(root->sibling);
    }
    if (root->child) {
	free_menu_hier(root->child);
    }
    XtFree(root->menu_name);
    XtFree((char *)root);
}

/*
 * Compute the potential height of the menu bar.
 */
Dimension
menubar_qheight(Dimension container_width)
{
    if (!appres.interactive.menubar ||
	    (container_width < (unsigned) MENU_MIN_WIDTH)) {
	return 0;
    } else {
	return TOP_MARGIN + KEY_HEIGHT + 2 * BORDER + BOTTOM_MARGIN +
	    2 * MENU_BORDER;
    }
}

/*
 * Initialize the menu bar.
 */
void
menubar_init(Widget container, Dimension overall_width, Dimension current_width)
{
    static Widget menu_bar;
    static bool ever = false;
    bool mb_old;
    Dimension height;

    if (!ever) {

	scheme_init();
	codepages_init();
	XtRegisterGrabAction(HandleMenu_xaction, True,
		(ButtonPressMask|ButtonReleaseMask),
		GrabModeAsync, GrabModeAsync);

	/* Figure out which bitmaps to use. */
	if (rescale(locked_width) >= locked20_width) {
	    scaled_locked_width = locked20_width;
	    scaled_locked_height = locked20_height;
	    scaled_locked_bits = locked20_bits;
	    scaled_unlocked_width = unlocked20_width;
	    scaled_unlocked_height = unlocked20_height;
	    scaled_unlocked_bits = unlocked20_bits;
	} else if (rescale(locked_width) >= locked15_width) {
	    scaled_locked_width = locked15_width;
	    scaled_locked_height = locked15_height;
	    scaled_locked_bits = locked15_bits;
	    scaled_unlocked_width = unlocked15_width;
	    scaled_unlocked_height = unlocked15_height;
	    scaled_unlocked_bits = unlocked15_bits;
	} else {
	    scaled_locked_width = locked_width;
	    scaled_locked_height = locked_height;
	    scaled_locked_bits = locked_bits;
	    scaled_unlocked_width = unlocked_width;
	    scaled_unlocked_height = unlocked_height;
	    scaled_unlocked_bits = unlocked_bits;
	}

	if (rescale(dot_width) >= dot20_width) {
	    scaled_dot_width = dot20_width;
	    scaled_dot_height = dot20_height;
	    scaled_dot_bits = dot20_bits;
	    scaled_no_dot_width = no_dot20_width;
	    scaled_no_dot_height = no_dot20_height;
	    scaled_no_dot_bits = no_dot20_bits;
	} else if (rescale(dot_width) >= dot15_width) {
	    scaled_dot_width = dot15_width;
	    scaled_dot_height = dot15_height;
	    scaled_dot_bits = dot15_bits;
	    scaled_no_dot_width = no_dot15_width;
	    scaled_no_dot_height = no_dot15_height;
	    scaled_no_dot_bits = no_dot15_bits;
	} else {
	    scaled_dot_width = dot_width;
	    scaled_dot_height = dot_height;
	    scaled_dot_bits = dot_bits;
	    scaled_no_dot_width = no_dot_width;
	    scaled_no_dot_height = no_dot_height;
	    scaled_no_dot_bits = no_dot_bits;
	}

	if (rescale(diamond_width) >= diamond20_width) {
	    scaled_diamond_width = diamond20_width;
	    scaled_diamond_height = diamond20_height;
	    scaled_diamond_bits = diamond20_bits;
	    scaled_no_diamond_width = no_diamond20_width;
	    scaled_no_diamond_height = no_diamond20_height;
	    scaled_no_diamond_bits = no_diamond20_bits;
	} else if (rescale(diamond_width) >= diamond15_width) {
	    scaled_diamond_width = diamond15_width;
	    scaled_diamond_height = diamond15_height;
	    scaled_diamond_bits = diamond15_bits;
	    scaled_no_diamond_width = no_diamond15_width;
	    scaled_no_diamond_height = no_diamond15_height;
	    scaled_no_diamond_bits = no_diamond15_bits;
	} else {
	    scaled_diamond_width = diamond_width;
	    scaled_diamond_height = diamond_height;
	    scaled_diamond_bits = diamond_bits;
	    scaled_no_diamond_width = no_diamond_width;
	    scaled_no_diamond_height = no_diamond_height;
	    scaled_no_diamond_bits = no_diamond_bits;
	}

	if (rescale(ky_width) >= ky20_width) {
	    scaled_ky_width = ky20_width;
	    scaled_ky_height = ky20_height;
	    scaled_ky_bits = ky20_bits;
	} else if (rescale(ky_width) >= ky15_width) {
	    scaled_ky_width = ky15_width;
	    scaled_ky_height = ky15_height;
	    scaled_ky_bits = ky15_bits;
	} else {
	    scaled_ky_width = ky_width;
	    scaled_ky_height = ky_height;
	    scaled_ky_bits = ky_bits;
	}

	if (rescale(arrow_width) >= arrow20_width) {
	    scaled_arrow_width = arrow20_width;
	    scaled_arrow_height = arrow20_height;
	    scaled_arrow_bits = arrow20_bits;
	} else if (rescale(arrow_width) >= arrow15_width) {
	    scaled_arrow_width = arrow15_width;
	    scaled_arrow_height = arrow15_height;
	    scaled_arrow_bits = arrow15_bits;
	} else {
	    scaled_arrow_width = arrow_width;
	    scaled_arrow_height = arrow_height;
	    scaled_arrow_bits = arrow_bits;
	}

	/* Create bitmaps. */
	dot = XCreateBitmapFromData(display, root_window,
		(char *)scaled_dot_bits, scaled_dot_width, scaled_dot_height);
	no_dot = XCreateBitmapFromData(display, root_window,
		(char *)scaled_no_dot_bits, scaled_no_dot_width,
		scaled_no_dot_height);
	arrow = XCreateBitmapFromData(display, root_window,
		(char *)scaled_arrow_bits, scaled_arrow_width,
		scaled_arrow_height);
	diamond = XCreateBitmapFromData(display, root_window,
		(char *)scaled_diamond_bits, scaled_diamond_width,
		scaled_diamond_height);
	no_diamond = XCreateBitmapFromData(display, root_window,
		(char *)scaled_no_diamond_bits, scaled_no_diamond_width,
		scaled_no_diamond_height);
	null = XCreateBitmapFromData(display, root_window,
		(char *) null_bits, null_width, null_height);

	ever = true;
    }

    height = menubar_qheight(current_width);
    mb_old = menubar_buttons;
    menubar_buttons = (height != 0);
    if (menubar_buttons) {
	if (menu_bar == NULL) {
	    /* Create the menu bar. */
	    menu_bar = XtVaCreateManagedWidget(
		    "menuBarContainer", huskWidgetClass, container,
		    XtNborderWidth, MENU_BORDER,
		    XtNwidth, overall_width - 2 * MENU_BORDER,
		    XtNheight, height - 2 * MENU_BORDER,
		    NULL);
	} else {
	    /* Resize and map the menu bar. */
	    XtVaSetValues(menu_bar,
		    XtNborderWidth, MENU_BORDER,
		    XtNwidth, overall_width - 2 * MENU_BORDER,
		    NULL);
	    XtMapWidget(menu_bar);
	}
	menu_parent = menu_bar;
    } else if (menu_bar != NULL) {
	/* Hide the menu bar. */
	XtUnmapWidget(menu_bar);
	menu_parent = container;
    } else {
	menu_parent = container;
    }

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

    /* TLS icon */

    tls_icon_init(
	    (Position) (current_width - LEFT_MARGIN -
			(scaled_ky_width + rescale(8)) -
			4 * BORDER - 2 * MENU_BORDER - (scaled_locked_width + rescale(8))),
	    TOP_MARGIN);

    /* Keypad button */

    keypad_button_init(
	    (Position) (current_width - LEFT_MARGIN - (scaled_ky_width + rescale(8)) -
			    2 * BORDER - 2 * MENU_BORDER),
	    TOP_MARGIN);
}

/*
 * TLS status changed.
 */
static void
menubar_secure(bool ignored _is_unused)
{
    if (locked_icon != NULL) {
	if (CONNECTED) {
	    if (net_secure_connection()) {
		XtUnmapWidget(unlocked_icon);
		if (net_secure_unverified()) {
		    XtMapWidget(unverified_icon);
		} else {
		    XtMapWidget(locked_icon);
		}
	    } else {
		XtUnmapWidget(locked_icon);
		XtUnmapWidget(unverified_icon);
		XtMapWidget(unlocked_icon);
	    }
	} else {
	    XtUnmapWidget(locked_icon);
	    XtUnmapWidget(unverified_icon);
	    XtUnmapWidget(unlocked_icon);
	}
    }
}

/*
 * External entry points
 */

/*
 * Called when connected to or disconnected from a host.
 */
static void
menubar_connect(bool ignored _is_unused)
{
    /* Set the disconnect button sensitivity. */
    if (disconnect_button != NULL) {
	XtVaSetValues(disconnect_button, XtNsensitive, PCONNECTED, NULL);
    }

    /* Set up the exit button, either with a pullright or a callback. */
    if (exit_button != NULL) {
	if (PCONNECTED) {
	    /* Remove the immediate callback. */
	    if (n_bye) {
		XtRemoveCallback(exit_button, XtNcallback, Bye, NULL);
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
		XtAddCallback(exit_button, XtNcallback, Bye, NULL);
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
    if (!appres.reconnect && connect_menu != NULL) {
	if (PCONNECTED && connect_button != NULL) {
	    XtUnmapWidget(connect_button);
	} else {
	    connect_menu_init(true,
		    BUTTON_X((file_menu != NULL) + (options_menu != NULL)),
		    TOP_MARGIN);
	    if (menubar_buttons) {
		XtMapWidget(connect_button);
	    }
	}
    }

    /* Set up the macros menu. */
    macros_menu_init(true,
	    BUTTON_X((file_menu != NULL) + (options_menu != NULL)),
	    TOP_MARGIN);

    /* Set up the various option buttons. */
    if (ft_button != NULL) {
	XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
    }
    if (printer_button != NULL) {
	XtVaSetValues(printer_button, XtNsensitive, IN_3270, NULL);
    }
    if (assoc_button != NULL) {
	XtVaSetValues(assoc_button, XtNsensitive,
		!pr3287_session_running() && IN_3270 && IN_TN3270E,
		NULL);
    }
    if (lu_button != NULL) {
	XtVaSetValues(lu_button, XtNsensitive,
	    !pr3287_session_running() && IN_3270,
	    NULL);
    }
    if (linemode_button != NULL) {
	XtVaSetValues(linemode_button, XtNsensitive, IN_NVT, NULL);
    }
    if (charmode_button != NULL) {
	XtVaSetValues(charmode_button, XtNsensitive, IN_NVT, NULL);
    }
    if (toggle_widget[LINE_WRAP].w[0] != NULL) {
	XtVaSetValues(toggle_widget[LINE_WRAP].w[0],
		XtNsensitive, IN_NVT,
		NULL);
    }
    if (toggle_widget[RECTANGLE_SELECT].w[0] != NULL) {
	XtVaSetValues(toggle_widget[RECTANGLE_SELECT].w[0],
		XtNsensitive, IN_NVT,
		NULL);
    }
    if (models_option != NULL) {
	XtVaSetValues(models_option, XtNsensitive, !PCONNECTED, NULL);
    }
    if (extended_button != NULL) {
	XtVaSetValues(extended_button, XtNsensitive, !PCONNECTED, NULL);
    }
    if (m3278_button != NULL) {
	XtVaSetValues(m3278_button, XtNsensitive, !PCONNECTED, NULL);
    }
    if (m3279_button != NULL) {
	XtVaSetValues(m3279_button, XtNsensitive, !PCONNECTED, NULL);
    }

    menubar_secure(false);
}

/* Called when the printer starts or stops. */
static void
menubar_printer(bool printer_on)
{
    if (assoc_button != NULL) {
	XtVaSetValues(assoc_button, XtNsensitive,
		!printer_on && IN_3270 && IN_TN3270E,
		NULL);
    }
    if (lu_button != NULL) {
	XtVaSetValues(lu_button, XtNsensitive,
		!printer_on && IN_3270,
		NULL);
    }
    if (printer_off_button != NULL) {
	XtVaSetValues(printer_off_button,
		XtNsensitive, printer_on,
		NULL);
    }
}

void
menubar_keypad_changed(void)
{
    if (keypad_option_button != NULL) {
	XtVaSetValues(keypad_option_button,
		XtNleftBitmap,
		    xappres.keypad_on || keypad_popped ? dot : None,
		NULL);
    }
}

/* Called when we switch between NVT and 3270 modes. */
static void
menubar_in3270(bool in3270)
{
    if (ft_button != NULL) {
	XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
    }
    if (printer_button != NULL) {
	XtVaSetValues(printer_button, XtNsensitive, IN_3270, NULL);
    }
    if (assoc_button != NULL) {
	XtVaSetValues(assoc_button,
		XtNsensitive, !pr3287_session_running() && IN_3270 &&
		    IN_TN3270E,
		NULL);
    }
    if (lu_button != NULL) {
	XtVaSetValues(lu_button,
		XtNsensitive, !pr3287_session_running() && IN_3270,
		NULL);
    }
    if (linemode_button != NULL) {
	XtVaSetValues(linemode_button,
		XtNsensitive, !in3270,
		XtNleftBitmap, in3270? no_diamond:
				       (linemode? diamond: no_diamond),
		NULL);
    }
    if (charmode_button != NULL) {
	XtVaSetValues(charmode_button,
		XtNsensitive, !in3270,
		XtNleftBitmap, in3270? no_diamond:
				       (linemode? no_diamond: diamond),
		NULL);
    }
    if (toggle_widget[LINE_WRAP].w[0] != NULL) {
	XtVaSetValues(toggle_widget[LINE_WRAP].w[0],
		XtNsensitive, !in3270,
		NULL);
    }
    if (toggle_widget[RECTANGLE_SELECT].w[0] != NULL) {
	XtVaSetValues(toggle_widget[RECTANGLE_SELECT].w[0],
		XtNsensitive, !in3270,
		NULL);
    }
    if (idle_button != NULL) {
	XtVaSetValues(idle_button, XtNsensitive, in3270, NULL);
    }
    if (save_input_button != NULL) {
	XtVaSetValues(save_input_button, XtNsensitive, in3270, NULL);
    }
    if (restore_input_button != NULL) {
	XtVaSetValues(restore_input_button, XtNsensitive, in3270, NULL);
    }
}

/* Called when we switch between NVT line and character modes. */
static void
menubar_linemode(bool in_linemode)
{
    if (linemode_button != NULL) {
	XtVaSetValues(linemode_button,
		XtNleftBitmap, in_linemode ? diamond : no_diamond,
		NULL);
    }
    if (charmode_button != NULL) {
	XtVaSetValues(charmode_button,
		XtNleftBitmap, in_linemode ? no_diamond : diamond,
		NULL);
    }
}

/* Called to change the sensitivity of the "Abort Script" button. */
void
menubar_as_set(bool sensitive)
{
    if (script_abort_button != NULL) {
	XtVaSetValues(script_abort_button, XtNsensitive, sensitive, NULL);
    }
}

/*
 * "File..." menu
 */
static Widget save_shell = (Widget) NULL;

/* Called from "Exit x3270" button on "File..." menu */
static void
Bye(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    x3270_exit(0);
}

/* Called from the "Disconnect" button on the "File..." menu */
static void
disconnect(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    host_disconnect(false);
}

/* Called from the "Re-enble Keyboard" button on the "File..." menu */
static void
reenable_keyboard_option(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    push_macro(AnKeyboardDisable "(" KwForceEnable ")");
}

/* Called from the "Abort Script" button on the "File..." menu */
static void
script_abort_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    abort_script();
}

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
    if (!s || !*s) {
	return;
    }
    if (save_options(s)) {
	XtPopdown(save_shell);
    }
}

#if defined(HAVE_START) /*[*/
/* Called from the "Help" button on the "File..." menu. */
static void
do_help(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    start_help();
}
#endif /*]*/

/* Called from the "Save Options in File" button on the "File..." menu */
static void
do_save_options(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (save_shell == NULL) {
	save_shell = create_form_popup("SaveOptions", save_button_callback,
		NULL, FORM_NO_WHITE);
    }
    XtVaSetValues(XtNameToWidget(save_shell, ObjDialog),
	    XtNvalue, profile_name,
	    NULL);
    popup_popup(save_shell, XtGrabExclusive);
}

/* Called from the "Save Input" button on the "File..." menu */
static void
do_save_input(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    run_action(AnSaveInput, IA_UI, NULL, NULL);
}

/* Called from the "Restore Input" button on the "File..." menu */
static void
do_restore_input(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    run_action(AnRestoreInput, IA_UI, NULL, NULL);
}

/* Callback for printer session options. */
static void
do_printer(Widget w _is_unused, XtPointer client_data, XtPointer call_data _is_unused)
{
    if (client_data == NULL) {
	pr3287_session_start(NULL);
    } else if (!strcmp(client_data, "lu")) {
	printer_lu_dialog();
    } else {
	pr3287_session_stop();
    }
}

/* Figure out if a Widget is suppressed. */
static bool
item_suppressed(Widget parent, const char *name)
{
	char *suppress;

	suppress = get_fresource("%s.%s.%s", XtName(parent), name,
		ResSuppress);
#if defined(DEBUG_SUPPRESS) /*[*/
	printf("suppress: %s.%s.%s -> %s\n",
		XtName(parent), name, ResSuppress,
		suppress? suppress: "(null)");
#endif /*]*/
	return suppress != NULL &&
	       !strncasecmp(suppress, ResTrue, strlen(suppress));
}

/*
 * Create a dividing line, if *spaced isn't true.
 */
static void
cond_space(Widget menu, bool *spaced)
{
    if (spaced != NULL && !*spaced) {
	XtVaCreateManagedWidget(
		"space", cmeLineObjectClass, menu,
		NULL);
	*spaced = true;
    }
}

/*
 * Add a menu item to a menu, but only if it is not suppressed.
 */
static Widget
add_menu_itemv(char *name, Widget menu, XtCallbackProc callback, XtPointer arg,
		bool *spaced, ...)
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
		args = (Arg *)Realloc(args, max_num_args * sizeof(Arg));
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
    } else {
	return NULL;
    }
}

static void
popup_ft(Widget w _is_unused, XtPointer call_parms _is_unused,
	XtPointer call_data _is_unused)
{
    ft_gui_popup_ft();
}

static void
file_menu_init(bool regen, Dimension x, Dimension y)
{
    Widget w;
    bool spaced = false;
    bool any = false;

    if (regen && (file_menu != NULL)) {
	XtDestroyWidget(file_menu);
	file_menu = NULL;
    }
    if (file_menu != NULL) {
	return;
    }

    file_menu = XtVaCreatePopupShell(
	    "fileMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons? XtNlabel: NULL, NULL,
	    NULL);
    if (!menubar_buttons) {
	XtVaCreateManagedWidget("space", cmeLineObjectClass,
		file_menu, NULL);
    }

    /* Online help */
#if defined(HAVE_START) /*[*/
    if (!item_suppressed(file_menu, "helpOption")) {
	w = add_menu_itemv("helpOption", file_menu,
		do_help, NULL, &spaced,
		NULL);
	any |= (w != NULL);
    }
#endif /*]*/

    /* About x3270... */
    if (!item_suppressed(file_menu, "aboutOption")) {
	bool any_about = false;

	w = XtVaCreatePopupShell(
		"aboutMenu", complexMenuWidgetClass, file_menu,
		NULL);
	any_about |= add_menu_itemv("aboutCopyright", w,
		show_about_copyright, NULL, NULL, NULL) != NULL;
	any_about |= add_menu_itemv("aboutConfig", w,
		show_about_config, NULL, NULL, NULL) != NULL;
	any_about |= add_menu_itemv("aboutStatus", w,
		show_about_status, NULL, NULL, NULL) != NULL;
	if (any_about) {
	    XtVaCreateManagedWidget(
		    "aboutOption", cmeBSBObjectClass, file_menu,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "aboutMenu",
		    NULL);
	    any = true;
	} else {
	    XtDestroyWidget(w);
	}
    }

    /* File Transfer */
    if (!appres.secure) {
	spaced = false;
	ft_button = add_menu_itemv("ftOption", file_menu,
		popup_ft, NULL, &spaced,
		XtNsensitive, IN_3270,
		NULL);
	any |= (ft_button != NULL);
    }

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
		XtNsensitive, pr3287_session_running(),
		NULL);

	if (assoc_button != NULL || lu_button != NULL ||
		printer_off_button != NULL) {
	    XtCreateManagedWidget(
		    "space", cmeLineObjectClass, file_menu,
		    NULL, 0);
	    printer_button = XtVaCreateManagedWidget(
		    "printerOption", cmeBSBObjectClass, file_menu,
		    XtNsensitive, IN_3270,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "printerMenu",
		    NULL);
	    any = true;
	} else {
	    XtDestroyWidget(w);
	}
    }

    /* Trace Data Stream
       Trace X Events
       Save Screen(s) in File */
    spaced = false;
    if (!appres.secure && appres.debug_tracing) {
	any |= toggle_init(file_menu, TRACING, "traceOption", NULL, &spaced);
    }
    if (!appres.secure) {
	w = add_menu_itemv("screenTraceOption", file_menu,
		screensave_option, NULL, &spaced,
		NULL);
	if (w != NULL) {
	    any = true;
	    toggle_widget[SCREEN_TRACE].w[0] = w;
	    XtVaSetValues(w, XtNleftBitmap,
		    toggled(SCREEN_TRACE)? dot: None,
		    NULL);
	}
    }

    /* Print Window Bitmap */
    spaced = false;
    w =  add_menu_itemv("printWindowOption", file_menu,
	    print_window_option, NULL, &spaced,
	    NULL);
    any |= (w != NULL);

    if (!appres.secure) {

	/* Save Options */
	spaced = false;
	w = add_menu_itemv("saveOption", file_menu,
		do_save_options, NULL, &spaced,
		NULL);
	any |= (w != NULL);

	/* x3270> prompt. */
	spaced = false;
	w = add_menu_itemv("promptOption", file_menu,
		prompt_option, NULL, &spaced,
		NULL);
	any |= (w != NULL);
    }

    /* Save/restore input. */
    spaced = false;
    if (!appres.secure) {
	save_input_button = add_menu_itemv("saveInputOption", file_menu,
		do_save_input, NULL, &spaced,
		XtNsensitive, IN_3270,
		NULL);
	any |= (save_input_button != NULL);
	restore_input_button = add_menu_itemv("restoreInputOption", file_menu,
		do_restore_input, NULL, &spaced,
		XtNsensitive, IN_3270,
		NULL);
	any |= (restore_input_button != NULL);
    }

    /* Re-enable keyboard. */
    spaced = false;
    reenable_button = add_menu_itemv("reenableKeyboardOption", file_menu,
	    reenable_keyboard_option, NULL, &spaced,
	    XtNsensitive, keyboard_disabled(),
	    NULL);
    any |= (reenable_button != NULL);

    /* Abort script */
    spaced = false;
    script_abort_button = add_menu_itemv("abortScriptOption", file_menu,
	    script_abort_callback, NULL, &spaced,
	    XtNsensitive, task_active(),
	    NULL);
    any |= (script_abort_button != NULL);

    /* Disconnect */
    spaced = false;
    disconnect_button = add_menu_itemv("disconnectOption", file_menu,
	    disconnect, NULL, &spaced,
	    XtNsensitive, PCONNECTED,
	    NULL);
    any |= (disconnect_button != NULL);

    /* Exit x3270 */
    if (exit_menu != NULL) {
	XtDestroyWidget(exit_menu);
    }
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
	any = true;
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
	host_connect(client_data, IA_UI);
}

/* Called from the lone "Connect" button on the connect dialog */
static void
connect_button_callback(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *s;

    s = XawDialogGetValueString((Widget)client_data);
    if (!s || !*s) {
	return;
    }
    if (host_connect(s, IA_UI)) {
	XtPopdown(connect_shell);
    }
}

/* Called from the "Other..." button on the "Connect..." menu */
static void
do_connect_popup(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
    if (connect_shell == NULL) {
	connect_shell = create_form_popup("Connect",
		connect_button_callback, NULL, FORM_NO_CC);
    }
    popup_popup(connect_shell, XtGrabExclusive);
}

/*
 * Initialize the "Connect..." menu
 */
static void
connect_menu_init(bool regen, Position x, Position y)
{
    Widget w;
    int n_hosts = 0;
    bool any_hosts = false;
    struct host *h;
    struct host_list *hl;
    bool need_line = false;
    int n_primary = 0;
    int n_recent = 0;
    static struct menu_hier *root = NULL;
    Widget recent_menu = NULL;

    if (regen && (connect_menu != NULL)) {
	XtDestroyWidget(connect_menu);
	connect_menu = NULL;
	if (connect_button != NULL) {
	    XtDestroyWidget(connect_button);
	    connect_button = NULL;
	}
	free_menu_hier(root);
	root = NULL;
    }
    if (connect_menu != NULL) {
	return;
    }

    /* Create the menu */
    root = (struct menu_hier *)XtCalloc(1, sizeof(struct menu_hier));
    root->menu_shell = connect_menu = XtVaCreatePopupShell(
	    "hostMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
    if (!menubar_buttons) {
	need_line = true;
    }

    /* Walk the host list from the file to produce the host menu */

    while (host_list != NULL) {
	hl = host_list->next;
	free(host_list->name);
	free(host_list);
	host_list = hl;
    }
    for (h = hosts; h; h = h->next) {
	switch (h->entry_type) {
	case ALIAS:
	    continue;
	case PRIMARY:
	    n_primary++;
	    break;
	case RECENT:
	    n_recent++;
	    if (n_recent == 1 && n_primary) {
		recent_menu = XtVaCreatePopupShell(
			"recentMenu", complexMenuWidgetClass,
			connect_menu, NULL);
	    }
	    break;
	}
	if ((need_line && !any_hosts) ||
	    (n_primary > 0 && n_recent == 1)) {
	    XtVaCreateManagedWidget("space",
		    cmeLineObjectClass, connect_menu, NULL);
	}
	any_hosts = true;
	w = XtVaCreateManagedWidget(
		h->name, cmeBSBObjectClass,
		(h->entry_type == PRIMARY || recent_menu == NULL)?
		    add_menu_hier(root, h->parents, NULL, 0):
		recent_menu,
		NULL);
	hl = (struct host_list *)XtCalloc(1, sizeof(struct host_list));
	hl->name = XtNewString(h->name);
	hl->next = host_list;
	host_list = hl;
	XtAddCallback(w, XtNcallback, host_connect_callback, hl->name);
	n_hosts++;
    }
    if (recent_menu) {
	XtVaCreateManagedWidget(
		"recentOption", cmeBSBObjectClass, connect_menu,
		XtNrightBitmap, arrow,
		XtNmenuName, "recentMenu",
		NULL);
    }
    if (any_hosts) {
	need_line = true;
    }

    /* Add an "Other..." button at the bottom */

    if (!any_hosts || !xappres.no_other) {
	if (need_line) {
	    XtVaCreateManagedWidget("space",
		    cmeLineObjectClass,
		    connect_menu, NULL);
	}
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
macros_menu_init(bool regen, Position x, Position y)
{
    static Widget macros_menu;
    Widget w;
    struct macro_def *m;
    bool any = false;
    static struct menu_hier *root = NULL;

    if (regen && (macros_menu != NULL)) {
	XtDestroyWidget(macros_menu);
	macros_menu = NULL;
	if (macros_button != NULL) {
	    XtDestroyWidget(macros_button);
	    macros_button = NULL;
	}
    }
    if (regen && root != NULL) {
	free_menu_hier(root);
	root = NULL;
    }
    if (macros_menu != NULL || !PCONNECTED) {
	return;
    }

    /* Walk the list */

    root = (struct menu_hier *)XtCalloc(1, sizeof(struct menu_hier));
    for (m = macro_defs; m; m = m->next) {
	if (!any) {
	    /* Create the menu */
	    root->menu_shell = macros_menu = XtVaCreatePopupShell(
		    MACROS_MENU, complexMenuWidgetClass, menu_parent,
		    menubar_buttons ? XtNlabel : NULL, NULL,
		    NULL);
	    if (!menubar_buttons) {
		XtVaCreateManagedWidget("space",
			cmeLineObjectClass, macros_menu, NULL);
	    }
	}
	w = XtVaCreateManagedWidget(
		m->name, cmeBSBObjectClass,
		add_menu_hier(root, m->parents, NULL, 0), 
		NULL);
	XtAddCallback(w, XtNcallback, do_macro, (XtPointer)m);
	any = true;
    }

    /* Add the "Macros..." button itself to the menu_parent */

    if (any && menubar_buttons) {
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
}

/* Called to toggle the keypad */
static void
toggle_keypad(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
    switch (kp_placement) {
    case kp_integral:
	xappres.keypad_on = !xappres.keypad_on;
	screen_showikeypad(xappres.keypad_on);
	break;
    case kp_left:
    case kp_right:
    case kp_bottom:
    case kp_inside_right:
	keypad_popup_init();
	if (keypad_popped)  {
	    XtPopdown(keypad_shell);
	} else {
	    popup_popup(keypad_shell, XtGrabNone);
	}
	break;
    }
    menubar_keypad_changed();
    keypad_changed = true;
}

static void
keypad_button_init(Position x, Position y)
{
    if (!menubar_buttons) {
	return;
    }
    if (keypad_button == NULL) {
	Pixmap pixmap;

	pixmap = XCreateBitmapFromData(display, root_window,
		(char *)scaled_ky_bits, scaled_ky_width, scaled_ky_height);
	keypad_button = XtVaCreateManagedWidget(
		"keypadButton", commandWidgetClass, menu_parent,
		XtNbitmap, pixmap,
		XtNx, x,
		XtNy, y,
		XtNwidth, scaled_ky_width + rescale(8),
		XtNheight, KEY_HEIGHT,
		XtNsensitive, keypad_sensitive,
		NULL);
	XtAddCallback(keypad_button, XtNcallback,
		toggle_keypad, NULL);
    } else {
	XtVaSetValues(keypad_button, XtNx, x, NULL);
    }
}

static void
tls_icon_init(Position x, Position y)
{
    if (!menubar_buttons) {
	return;
    }

    if (locked_icon == NULL) {
	Pixmap pixmap;

	pixmap = XCreateBitmapFromData(display, root_window,
		(char *)scaled_locked_bits, scaled_locked_width,
		scaled_locked_height);
	locked_icon = XtVaCreateManagedWidget(
		"lockedIcon", commandWidgetClass, menu_parent,
		XtNbitmap, pixmap,
		XtNx, x,
		XtNy, y,
		XtNwidth, scaled_locked_width + rescale(8),
		XtNheight, KEY_HEIGHT,
		XtNmappedWhenManaged,
		    CONNECTED && net_secure_connection() &&
			!net_secure_unverified(),
		NULL);
	XtAddCallback(locked_icon, XtNcallback,
		show_about_status, NULL);
	unverified_icon = XtVaCreateManagedWidget(
		"unverifiedIcon", commandWidgetClass, menu_parent,
		XtNbitmap, pixmap,
		XtNx, x,
		XtNy, y,
		XtNwidth, scaled_locked_width + rescale(8),
		XtNheight, KEY_HEIGHT,
		XtNmappedWhenManaged,
		    CONNECTED && net_secure_connection() &&
			net_secure_unverified(),
		NULL);
	XtAddCallback(unverified_icon, XtNcallback,
		show_about_status, NULL);
	pixmap = XCreateBitmapFromData(display, root_window,
		(char *)scaled_unlocked_bits, scaled_unlocked_width,
		scaled_unlocked_height);
	unlocked_icon = XtVaCreateManagedWidget(
		"unlockedIcon", commandWidgetClass, menu_parent,
		XtNbitmap, pixmap,
		XtNx, x,
		XtNy, y,
		XtNwidth, scaled_unlocked_width + rescale(8),
		XtNheight, KEY_HEIGHT,
		XtNmappedWhenManaged, CONNECTED && !net_secure_connection(),
		NULL);
	XtAddCallback(unlocked_icon, XtNcallback,
		show_about_status, NULL);
    } else {
	XtVaSetValues(locked_icon, XtNx, x, NULL);
	XtVaSetValues(unverified_icon, XtNx, x, NULL);
	XtVaSetValues(unlocked_icon, XtNx, x, NULL);
    }
}

void
menubar_resize(Dimension width)
{
    tls_icon_init(
	    (Position) (width - LEFT_MARGIN -
			    (scaled_ky_width + rescale(8)) -
			    4 * BORDER - 2 * MENU_BORDER - (scaled_locked_width + rescale(8))),
	    TOP_MARGIN);
    keypad_button_init(
	    (Position) (width - LEFT_MARGIN - (scaled_ky_width + rescale(8)) - 2 * BORDER),
	    TOP_MARGIN);
}

/*
 * "Options..." menu
 */

static void
toggle_callback(Widget w, XtPointer userdata, XtPointer calldata _is_unused)
{
    toggle_widget_t *wx = (toggle_widget_t *)userdata;
    toggle_index_t ix = wx - toggle_widget;

    /*
     * If this is a two-button radio group, rather than a simple toggle,
     * there is nothing to do if they are clicking on the current value.
     *
     * toggle_widget[ix][0] is the "toggle true" button; toggle_widget[ix][1]
     * is "toggle false".
     */
    if (wx->w[1] != NULL && w == wx->w[!toggled(ix)]) {
	return;
    }

    do_menu_toggle(ix);
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
    if (!s || !*s) {
	return;
    }
    if (sscanf(s, "%dx%d%c", &ovc, &ovr, &junk) == 2) {
	XtPopdown(oversize_shell);
	screen_remodel(model_num, ovc, ovr);
    } else {
	popup_an_error("Illegal size: %s", s);
    }
}

/* Called from the "Oversize..." button on the "Models..." menu */
static void
do_oversize_popup(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (oversize_shell == NULL) {
	oversize_shell = create_form_popup("Oversize",
		oversize_button_callback, NULL,
		FORM_NO_WHITE);
    }
    XtVaSetValues(XtNameToWidget(oversize_shell, ObjDialog),
        XtNvalue, appres.oversize? appres.oversize: "",
        NULL);
    popup_popup(oversize_shell, XtGrabExclusive);
}

/* Init a toggle, menu-wise */
static bool
toggle_init(Widget menu, int ix, const char *name1, const char *name2,
	bool *spaced)
{
    toggle_widget_t *wx = &toggle_widget[ix];

    if (!item_suppressed(menu, name1) &&
	    (name2 == NULL || !item_suppressed(menu, name2))) {
	if (spaced != NULL) {
	    cond_space(menu, spaced);
	}
	wx->w[0] = XtVaCreateManagedWidget(
		name1, cmeBSBObjectClass, menu,
		XtNleftBitmap,
		 toggled(ix)? (name2? diamond: dot):
			      (name2? no_diamond: None),
		NULL);
	XtAddCallback(wx->w[0], XtNcallback, toggle_callback, (XtPointer)wx);
	if (name2 != NULL) {
	    wx->w[1] = XtVaCreateManagedWidget(
		    name2, cmeBSBObjectClass, menu,
		    XtNleftBitmap, toggled(ix)? no_diamond: diamond,
		    NULL);
	    XtAddCallback(wx->w[1], XtNcallback, toggle_callback,
		    (XtPointer)wx);
	} else {
	    wx->w[1] = NULL;
	}
	return true;
    } else {
	return false;
    }
}

static Widget *font_widgets = NULL;
static Widget other_font;
static Widget font_shell = NULL;

static void
do_newfont(Widget w _is_unused, XtPointer userdata, XtPointer
	calldata _is_unused)
{
    screen_newfont((char *)userdata, true, false);
}

/* Called from the "Select Font" button on the font dialog */
static void
font_button_callback(Widget w, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *s;

    s = XawDialogGetValueString((Widget)client_data);
    if (!s || !*s) {
	return;
    }
    XtPopdown(font_shell);
    do_newfont(w, s, NULL);
}

static void
do_otherfont(Widget w _is_unused, XtPointer userdata _is_unused,
	XtPointer calldata _is_unused)
{
    if (font_shell == NULL) {
	font_shell = create_form_popup("Font", font_button_callback, NULL,
		FORM_NO_CC);
    }
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
    size_t offset = 0;

    cm = get_resource(ResSchemeList);
    if (cm == NULL) {
	return;
    }

    scheme_count = 0;
    while (s_split_dresource(cm, &offset, &label, &scheme) == 1) {
	s = (struct scheme *)XtMalloc(sizeof(struct scheme));
	if (!split_hier(label, &s->label, &s->parents)) {
	    XtFree((XtPointer)s);
	    continue;
	}
	XtFree(label);
	s->scheme = scheme;
	s->next = NULL;
	if (last_scheme != NULL) {
	    last_scheme->next = s;
	} else {
	    schemes = s;
	}
	last_scheme = s;
	scheme_count++;
    }
}

static void
do_newscheme(Widget w _is_unused, XtPointer userdata,
	XtPointer calldata _is_unused)
{
    screen_newscheme((char *)userdata);
}

/* Initialze the code page list. */
static void
codepages_init(void)
{
    char *cm, *cm0;
    char *label;
    char *codepage;
    struct codepage *s;

    cm = get_resource(ResCharsetList);
    if (cm == NULL) {
	return;
    }
    cm = cm0 = XtNewString(cm);

    codepage_count = 0;
    while (split_dresource(&cm, &label, &codepage) == 1) {
	s = (struct codepage *)XtMalloc(sizeof(struct codepage));
	if (!split_hier(label, &s->label, &s->parents)) {
	    XtFree((XtPointer)s);
	    continue;
	}
	s->codepage = XtNewString(codepage);
	s->next = NULL;
	if (last_codepage != NULL) {
	    last_codepage->next = s;
	} else {
	    codepages = s;
	}
	last_codepage = s;
	codepage_count++;
    }
    XtFree(cm0);
}

static void
do_newcodepage(Widget w, XtPointer userdata, XtPointer calldata _is_unused)
{
    /* Change the code page. */
    screen_newcodepage((char *)userdata);
}

static Widget keymap_shell = NULL;

/* Called from the "Set Keymap" button on the keymap dialog */
static void
keymap_button_callback(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *s;

    s = XawDialogGetValueString((Widget)client_data);
    if (s != NULL && !*s) {
	s = NULL;
    }
    XtPopdown(keymap_shell);
    keymap_init(s, true);
}

/* Callback from the "Keymap" menu option */
static void
do_keymap(Widget w _is_unused, XtPointer userdata _is_unused,
	XtPointer calldata _is_unused)
{
    if (keymap_shell == NULL) {
	keymap_shell = create_form_popup("Keymap",
		keymap_button_callback, NULL,
		FORM_NO_WHITE);
    }
    popup_popup(keymap_shell, XtGrabExclusive);
}

/* Callback from the "Idle Command" menu option */
static void
do_idle_command(Widget w _is_unused, XtPointer userdata _is_unused,
	XtPointer calldata _is_unused)
{
    popup_idle();
}

/* Callback from the "Snap" menu option */
static void
do_snap(Widget w _is_unused, XtPointer userdata _is_unused,
	XtPointer calldata _is_unused)
{
    screen_snap_size();
}

/* Called to change telnet modes */
static void
linemode_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    net_linemode();
}

static void
charmode_callback(Widget w _is_unused, XtPointer client_data _is_unused, 
	XtPointer call_data _is_unused)
{
    net_charmode();
}

static void
toggle_retry(Widget w _is_unused, XtPointer client_data _is_unused, 
	XtPointer call_data _is_unused)
{
    push_macro(AnToggle "(" ResRetry ")");
}

static void
toggle_reconnect(Widget w _is_unused, XtPointer client_data _is_unused, 
	XtPointer call_data _is_unused)
{
    push_macro(AnToggle "(" ResReconnect ")");
}

/* Called to change models */
static void
change_model_callback(Widget w, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    int m = atoi(client_data);

    switch (model_num) {
    case 2:
	if (model_2_button != NULL) {
	    XtVaSetValues(model_2_button, XtNleftBitmap, no_diamond, NULL);
	}
	break;
    case 3:
	if (model_3_button != NULL) {
	    XtVaSetValues(model_3_button, XtNleftBitmap, no_diamond, NULL);
	}
	break;
    case 4:
	if (model_4_button != NULL) {
	    XtVaSetValues(model_4_button, XtNleftBitmap, no_diamond, NULL);
	}
	break;
    case 5:
	if (model_5_button != NULL) {
	    XtVaSetValues(model_5_button, XtNleftBitmap, no_diamond, NULL);
	}
	break;
    }
    XtVaSetValues(w, XtNleftBitmap, diamond, NULL);
    screen_remodel(m, 0, 0);
}

/* Called to when model changes outside our control. */
static void
menubar_remodel(bool ignored _is_unused)
{
    /* Set the model buttons. */
    if (model_2_button != NULL) {
	XtVaSetValues(model_2_button, XtNleftBitmap,
		(model_num == 2)? diamond: no_diamond, NULL);
    }
    if (model_2_button != NULL) {
	XtVaSetValues(model_3_button, XtNleftBitmap,
		(model_num == 3)? diamond: no_diamond, NULL);
    }
    if (model_2_button != NULL) {
	XtVaSetValues(model_4_button, XtNleftBitmap,
		(model_num == 4)? diamond: no_diamond, NULL);
    }
    if (model_2_button != NULL) {
	XtVaSetValues(model_5_button, XtNleftBitmap,
		(model_num == 5)? diamond: no_diamond, NULL);
    }

    /* Enable/disable the oversize option. */
    if (oversize_button != NULL) {
	XtVaSetValues(oversize_button,
		XtNsensitive, appres.extended_data_stream,
		NULL);
    }

    /* Set the toggle on the extended mode button. */
    if (extended_button != NULL) {
	XtVaSetValues(extended_button,
		XtNleftBitmap, appres.extended_data_stream? dot: (Pixmap)NULL,
		NULL);
    }

    /* Set the 3278/3279 toggles. */
    if (m3278_button != NULL) {
	XtVaSetValues(m3278_button, XtNleftBitmap,
		mode3279 ? no_diamond : diamond,
		NULL);
    }
    if (m3279_button != NULL) {
	XtVaSetValues(m3279_button, XtNleftBitmap,
		mode3279 ? diamond : no_diamond,
		NULL);
    }
}

/* Compare a font name to the current emulator font name. */
static bool
is_efont(const char *font_name)
{
    return !strcmp(NO_BANG(font_name), NO_BANG(efontname)) ||
	   !strcmp(NO_BANG(font_name), NO_BANG(full_efontname));
}

/* Create, or re-create the font menu. */
static void
create_font_menu(bool regen, bool even_if_unknown)
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
    if (font_count) {
	font_widgets = (Widget *)XtCalloc(count, sizeof(Widget));
    } else {
	font_widgets = NULL;
    }
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
	XtAddCallback(font_widgets[ix], XtNcallback, do_newfont, f->font);
    }
    if (!xappres.no_other) {
	other_font = XtVaCreateManagedWidget(
		"otherFontOption", cmeBSBObjectClass, t,
		NULL);
	XtAddCallback(other_font, XtNcallback, do_otherfont, NULL);
    }

    XtVaSetValues(fonts_option, XtNmenuName, "fontsMenu", NULL);
}

/* Called when the host code page changes. */
static void
menubar_codepage(bool ignored _is_unused)
{
    int i;
    struct codepage *s;
    const char *cpname;

    if (!xappres.suppress_font_menu) {
	create_font_menu(false, false);
    }

    /* Update the code page menu. */
    cpname = get_codepage_name();
    for (i = 0, s = codepages; i < codepage_count; i++, s = s->next) {
	XtVaSetValues(codepage_widgets[i],
		XtNleftBitmap,
		(!strcmp(cpname, s->codepage) ||
		 codepage_matches_alias(s->codepage, cpname))?
		    diamond: no_diamond,
		NULL);
    }
}

/* Called when keyboard enable/disable changes. */
static void
menubar_keyboard_disable(bool ignored _is_unused)
{
    XtVaSetValues(reenable_button, XtNsensitive, keyboard_disabled(), NULL);
}

/* Called to change emulation modes */
static void
toggle_extended(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    appres.extended_data_stream = !appres.extended_data_stream;
    if (extended_button != NULL) {
	XtVaSetValues(extended_button,
		XtNleftBitmap, appres.extended_data_stream? dot: (Pixmap)NULL,
		NULL);
    }
    if (oversize_button != NULL) {
	XtVaSetValues(oversize_button,
		XtNsensitive, appres.extended_data_stream,
		NULL);
    }
    if (!appres.extended_data_stream) {
	screen_remodel(model_num, 0, 0);
    }
    screen_extended(appres.extended_data_stream);
}

static void
toggle_m3279(Widget w, XtPointer client_data _is_unused, XtPointer
	call_data _is_unused)
{
    if (w == m3278_button) {
	mode3279 = false;
    } else if (w == m3279_button) {
	mode3279 = true;
    } else {
	return;
    }
    XtVaSetValues(m3278_button, XtNleftBitmap,
	    mode3279 ? no_diamond : diamond,
	    NULL);
    XtVaSetValues(m3279_button, XtNleftBitmap,
	    mode3279 ? diamond : no_diamond,
	    NULL);
    if (scheme_button != NULL) {
	XtVaSetValues(scheme_button, XtNsensitive, mode3279, NULL);
    }
    Replace(appres.model, create_model(model_num, mode3279));
    screen_m3279(mode3279);
}

static void
options_menu_init(bool regen, Position x, Position y)
{
    Widget t;
    struct font_list *f;
    struct scheme *s;
    int ix;
    static Widget options_menu_button = NULL;
    Widget dummy_font_menu, dummy_font_element;
    static struct menu_hier *scheme_root = NULL;
    static struct menu_hier *codepage_root = NULL;
    bool spaced = false;
    bool any = false;
    Widget w;

    if (regen && (options_menu != NULL)) {
	XtDestroyWidget(options_menu);
	options_menu = NULL;
	if (options_menu_button != NULL) {
	    XtDestroyWidget(options_menu_button);
	    options_menu_button = NULL;
	}
    }
    if (options_menu != NULL) {
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
	for (ix = 0, s = schemes; ix < scheme_count; ix++, s = s->next) {
	    XtVaSetValues(scheme_widgets[ix], XtNleftBitmap,
		    !strcmp(xappres.color_scheme, s->scheme)?
			diamond: no_diamond,
		    NULL);
	}
	return;
    }

    /* Create the shell */
    options_menu = XtVaCreatePopupShell(
	    "optionsMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
    if (!menubar_buttons) {
	XtVaCreateManagedWidget("space", cmeLineObjectClass,
		options_menu, NULL);
    }

    /* Create the "toggles" pullright */
    if (!item_suppressed(options_menu, "togglesOption")) {
	t = XtVaCreatePopupShell(
		"togglesMenu", complexMenuWidgetClass, menu_parent,
		NULL);
	if (!menubar_buttons) {
	    keypad_option_button = add_menu_itemv("keypadOption", t,
		    toggle_keypad, NULL,
		    NULL,
		    XtNleftBitmap, (xappres.keypad_on || keypad_popped)?
			dot: None,
		    NULL);
	    if (keypad_option_button != NULL) {
		spaced = false;
	    } else {
		spaced = true;
	    }
	}
	toggle_init(t, MONOCASE, "monocaseOption", NULL, &spaced);
	toggle_init(t, CURSOR_BLINK, "cursorBlinkOption", NULL, &spaced);
	toggle_init(t, BLANK_FILL, "blankFillOption", NULL, &spaced);
	toggle_init(t, UNDERSCORE_BLANK_FILL, "underscoreBlankFillOption",
		NULL, &spaced);
	toggle_init(t, SHOW_TIMING, "showTimingOption", NULL, &spaced);
	toggle_init(t, SCROLL_BAR, "scrollBarOption", NULL, &spaced);
	toggle_init(t, LINE_WRAP, "lineWrapOption", NULL, &spaced);
	toggle_init(t, MARGINED_PASTE, "marginedPasteOption", NULL, &spaced);
	toggle_init(t, OVERLAY_PASTE, "overlayPasteOption", NULL, &spaced);
	toggle_init(t, RECTANGLE_SELECT, "rectangleSelectOption", NULL,
		&spaced);
	toggle_init(t, CROSSHAIR, "crosshairOption", NULL, &spaced);
	toggle_init(t, VISIBLE_CONTROL, "visibleControlOption", NULL, &spaced);
	toggle_init(t, TYPEAHEAD, "typeaheadOption", NULL, &spaced);
	toggle_init(t, ALWAYS_INSERT, "alwaysInsertOption", NULL, &spaced);
	toggle_init(t, SELECT_URL, "selectUrlOption", NULL, &spaced);
	retry_button = add_menu_itemv("retryOption", t,
		    toggle_retry, NULL,
		    &spaced,
		    XtNleftBitmap, appres.retry? dot: (Pixmap)NULL,
		    XtNsensitive, True,
		    NULL);
	reconnect_button = add_menu_itemv("reconnectOption", t,
		    toggle_reconnect, NULL,
		    &spaced,
		    XtNleftBitmap, appres.reconnect? dot: (Pixmap)NULL,
		    XtNsensitive, True,
		    NULL);
	spaced = false;
	toggle_init(t, ALT_CURSOR, "underlineCursorOption",
		"blockCursorOption", &spaced);
	spaced = false;
	linemode_button = add_menu_itemv("lineModeOption", t,
		linemode_callback, NULL,
		&spaced,
		XtNleftBitmap, linemode? diamond: no_diamond,
		XtNsensitive, IN_NVT,
		NULL);
	charmode_button = add_menu_itemv("characterModeOption", t,
		charmode_callback, NULL,
		&spaced,
		XtNleftBitmap, linemode? no_diamond: diamond,
		XtNsensitive, IN_NVT,
		NULL);
	if (!appres.interactive.mono) {
	    spaced = false;
	    m3278_button = add_menu_itemv( "m3278Option", t,
		    toggle_m3279, NULL,
		    &spaced,
		    XtNleftBitmap, mode3279? no_diamond: diamond,
		    XtNsensitive, !PCONNECTED,
		    NULL);
	    m3279_button = add_menu_itemv("m3279Option", t,
		    toggle_m3279, NULL,
		    &spaced,
		    XtNleftBitmap, mode3279? diamond: no_diamond,
		    XtNsensitive, !PCONNECTED,
		    NULL);
	}
	spaced = false;
	extended_button = add_menu_itemv("extendedDsOption", t,
		    toggle_extended, NULL,
		    &spaced,
		    XtNleftBitmap, appres.extended_data_stream? dot: (Pixmap)NULL,
		    XtNsensitive, !PCONNECTED,
		    NULL);
	if (keypad_option_button != NULL ||
	    toggle_widget[MONOCASE].w[0] != NULL ||
	    toggle_widget[CURSOR_BLINK].w[0] != NULL ||
	    toggle_widget[BLANK_FILL].w[0] != NULL ||
	    toggle_widget[SHOW_TIMING].w[0] != NULL ||
	    toggle_widget[SCROLL_BAR].w[0] != NULL ||
	    toggle_widget[LINE_WRAP].w[0] != NULL ||
	    toggle_widget[MARGINED_PASTE].w[0] != NULL ||
	    toggle_widget[RECTANGLE_SELECT].w[0] != NULL ||
	    toggle_widget[CROSSHAIR].w[0] != NULL ||
	    toggle_widget[VISIBLE_CONTROL].w[0] != NULL ||
	    toggle_widget[ALT_CURSOR].w[0] != NULL ||
	    toggle_widget[ALWAYS_INSERT].w[0] != NULL ||
	    toggle_widget[UNDERSCORE_BLANK_FILL].w[0] != NULL ||
	    linemode_button != NULL ||
	    charmode_button != NULL ||
	    m3278_button != NULL ||
	    m3279_button != NULL) {

	    XtVaCreateManagedWidget(
		    "togglesOption", cmeBSBObjectClass, options_menu,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "togglesMenu",
		    NULL);
	    any = true;
	} else {
		XtDestroyWidget(t);
	}
    }

    if (!xappres.suppress_font_menu &&
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

	XtVaCreateManagedWidget(
		"space", cmeLineObjectClass, options_menu,
		NULL);
	fonts_option = XtVaCreateManagedWidget(
		"fontsOption", cmeBSBObjectClass, options_menu,
		XtNrightBitmap, arrow,
		NULL);
	create_font_menu(regen, true);
	any = true;
    }

    /* Create the "Snap" option. */
    if (!item_suppressed(options_menu, "snapOption")) {
	spaced = false;
	snap_button = add_menu_itemv("snapOption", options_menu,
		do_snap, NULL,
		&spaced,
		XtNsensitive, snap_enabled,
		NULL);
	any |= (snap_button != NULL);
    }

    /* Create the "models" pullright */
    if (!item_suppressed(options_menu, "modelsOption")) {
	t = XtVaCreatePopupShell(
		"modelsMenu", complexMenuWidgetClass, menu_parent,
		NULL);
	model_2_button = add_menu_itemv("model2Option", t,
		change_model_callback, NewString("2"),
		NULL,
		XtNleftBitmap, (model_num == 2)? diamond: no_diamond,
		NULL);
	model_3_button = add_menu_itemv("model3Option", t,
		change_model_callback, NewString("3"),
		NULL,
		XtNleftBitmap, (model_num == 3)? diamond: no_diamond,
		NULL);
	model_4_button = add_menu_itemv("model4Option", t,
		change_model_callback, NewString("4"),
		NULL,
		XtNleftBitmap, (model_num == 4)? diamond: no_diamond,
		NULL);
	model_5_button = add_menu_itemv("model5Option", t,
		change_model_callback, NewString("5"),
		NULL,
		XtNleftBitmap, (model_num == 5)? diamond: no_diamond,
		NULL);
	oversize_button = add_menu_itemv("oversizeOption", t,
		do_oversize_popup, NULL,
		NULL,
		XtNsensitive, appres.extended_data_stream,
		NULL);
	if (model_2_button != NULL ||
	    model_3_button != NULL ||
	    model_4_button != NULL ||
	    model_5_button != NULL ||
	    oversize_button != NULL) {

	    XtVaCreateManagedWidget(
		    "space", cmeLineObjectClass, options_menu,
		    NULL);
	    models_option = XtVaCreateManagedWidget(
		    "modelsOption", cmeBSBObjectClass, options_menu,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "modelsMenu",
		    XtNsensitive, !PCONNECTED,
		    NULL);
	    any = true;
	} else {
	    XtDestroyWidget(t);
	}
    }

    /* Create the "colors" pullright */
    if (scheme_count && !item_suppressed(options_menu, "colorsOption")) {

	scheme_widgets = (Widget *)XtCalloc(scheme_count, sizeof(Widget));
	if (scheme_root != NULL) {
	    free_menu_hier(scheme_root);
	}
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
			!strcmp(xappres.color_scheme, s->scheme)?
			    diamond: no_diamond,
		    NULL);
		XtAddCallback(scheme_widgets[ix], XtNcallback, do_newscheme,
			s->scheme);
	}
	XtVaCreateManagedWidget("space", cmeLineObjectClass,
		options_menu,
		NULL);
	scheme_button = XtVaCreateManagedWidget(
		"colorsOption", cmeBSBObjectClass, options_menu,
		XtNrightBitmap, arrow,
		XtNmenuName, "colorsMenu",
		XtNsensitive, mode3279,
		NULL);
	any = true;
    }

    /* Create the "code page" pullright */
    if (codepage_count && !item_suppressed(options_menu, "codepageOption")) {
	struct codepage *cs;
	const char *cpname;

	if (codepage_root != NULL) {
	    free_menu_hier(codepage_root);
	}
	codepage_root = (struct menu_hier *)XtCalloc(1,
		sizeof(struct menu_hier));
	codepage_root->menu_shell = XtVaCreatePopupShell(
		"codepageMenu", complexMenuWidgetClass, menu_parent,
		NULL);

	cpname = get_codepage_name();
	codepage_widgets = (Widget *)XtCalloc(codepage_count, sizeof(Widget));
	for (ix = 0, cs = codepages; ix < codepage_count; ix++, cs = cs->next) {
	    t = add_menu_hier(codepage_root, cs->parents, NULL, 0);
	    codepage_widgets[ix] = XtVaCreateManagedWidget(
		    cs->label, cmeBSBObjectClass, t,
		    XtNleftBitmap,
		    (!strcmp(cpname, cs->codepage) ||
		     codepage_matches_alias(cs->codepage, cpname))?
			diamond: no_diamond,
		    NULL);
	    XtAddCallback(codepage_widgets[ix], XtNcallback, do_newcodepage,
		    cs->codepage);
	}

	XtVaCreateManagedWidget("space", cmeLineObjectClass,
		options_menu,
		NULL);
	XtVaCreateManagedWidget(
		"codepageOption", cmeBSBObjectClass, options_menu,
		XtNrightBitmap, arrow,
		XtNmenuName, "codepageMenu",
		NULL);
	any = true;
    }

    /* Create the "keymap" option */
    if (!xappres.no_other) {
	spaced = false;
	w = add_menu_itemv("keymapOption", options_menu,
		do_keymap, NULL,
		&spaced,
		NULL);
	any |= (w != NULL);
    }

    /* Create the "display keymap" option */
    spaced = false;
    w = add_menu_itemv("keymapDisplayOption", options_menu,
	    do_keymap_display, NULL,
	    &spaced,
	    NULL);
    any |= (w != NULL);

    /* Create the "Idle Command" option */
    if (!appres.secure) {
	spaced = false;
	idle_button = add_menu_itemv("idleCommandOption", options_menu,
		do_idle_command, NULL,
		&spaced,
		XtNsensitive, IN_3270,
		NULL);
	any |= (idle_button != NULL);
    }

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
menubar_retoggle(toggle_index_t ix)
{
    if (toggle_widget[ix].w[0] != NULL) {
	XtVaSetValues(toggle_widget[ix].w[0],
		XtNleftBitmap, toggled(ix)? (toggle_widget[ix].w[1]? diamond:
		                                                     dot):
		                            None,
		NULL);
    }
    if (toggle_widget[ix].w[1] != NULL) {
	XtVaSetValues(toggle_widget[ix].w[1],
		XtNleftBitmap, toggled(ix)? no_diamond: diamond,
		NULL);
    }
}

/**
 * Enable or disable the Snap option.
 */
void
menubar_snap_enable(bool enable)
{
    snap_enabled = enable;
    if (snap_button != NULL) {
	XtVaSetValues(snap_button, XtNsensitive, enable, NULL);
    }
}

/**
 * Enable or disable the keypad button.
 *
 * @param[in] sensitive		true if enabled
 */
void
menubar_keypad_sensitive(bool sensitive)
{
    keypad_sensitive = sensitive;
    if (keypad_button) {
	XtVaSetValues(keypad_button, XtNsensitive, sensitive, NULL);
    }
}

void
HandleMenu_xaction(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
    String p;

    xaction_debug(HandleMenu_xaction, event, params, num_params);
    if (xcheck_usage(HandleMenu_xaction, *num_params, 1, 2) < 0) {
	return;
    }
    if (!CONNECTED || *num_params == 1) {
	p = params[0];
    } else {
	p = params[1];
    }
    if (!XtNameToWidget(menu_parent, p)) {
#if 0
	if (strcmp(p, MACROS_MENU)) {
	    popup_an_error("%s: cannot find menu %s",
		    action_name(HandleMenu_xaction), p);
	}
#endif
	return;
    }
    XtCallActionProc(menu_parent, "XawPositionComplexMenu", event, &p, 1);
    XtCallActionProc(menu_parent, "MenuPopup", event, &p, 1);
}

static void
screensave_option(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    stmenu_popup(STMP_AS_IS);
}

/* Extended toggle notification. */
static void
menubar_toggle_notify(const char *name, enum resource_type type, void **value, ia_t ia, unsigned flags)
{
    if (!strcasecmp(name, ResRetry)) {
	if (retry_button != NULL) {
	    XtVaSetValues(retry_button,
		    XtNleftBitmap,
		    *(bool *)value ? dot : None,
		    NULL);
	}
    } else if (!strcasecmp(name, ResReconnect)) {
	if (reconnect_button != NULL) {
	    XtVaSetValues(reconnect_button,
		    XtNleftBitmap,
		    *(bool *)value ? dot : None,
		    NULL);
	}
    }
}

/**
 * Menu module registration.
 */
void
menubar_register(void)
{
    /* Register interest in state transtions. */
    register_schange(ST_3270_MODE, menubar_in3270);
    register_schange(ST_LINE_MODE, menubar_linemode);
    register_schange_ordered(ST_CONNECT, menubar_connect, ORDER_LAST);
    register_schange(ST_PRINTER, menubar_printer);
    register_schange(ST_REMODEL, menubar_remodel);
    register_schange(ST_CODEPAGE, menubar_codepage);
    register_schange(ST_KBD_DISABLE, menubar_keyboard_disable);
    register_schange(ST_SECURE, menubar_secure);

    /* Register for extended toggle notifications. */
    register_extended_toggle_notify(menubar_toggle_notify);
}

