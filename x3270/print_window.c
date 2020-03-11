/*
 * Copyright (c) 1994-2015, 2019-2020 Paul Mattes.
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
 *	print_window.c
 *		"Print Window Bitmap" support.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>

#include "appres.h"
#include "objects.h"
#include "resources.h"

#include "actions.h"
#include "names.h"
#include "popups.h"
#include "print_window.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"
#include "xpopups.h"

/* Typedefs */

/* Globals */

/* Statics */
static Widget print_window_shell = NULL;
static char *print_window_command = NULL;

/*
 * Printing the window bitmap is a rather convoluted process:
 *    The PrintWindow action calls PrintWindow_action(), or a menu option calls
 *	print_window_option().
 *    print_window_option() pops up the dialog.
 *    The OK button on the dialog triggers print_window_callback.
 *    print_window_callback pops down the dialog, then schedules a timeout
 *     1 second away.
 *    When the timeout expires, it triggers snap_it(), which finally calls
 *     xwd.
 * The timeout indirection is necessary because xwd prints the actual contents
 * of the window, including any pop-up dialog in front of it.  We pop down the
 * dialog, but then it is up to the server and Xt to send us the appropriate
 * expose events to repaint our window.  Hopefully, one second is enough to do
 * that.
 */

/* Termination procedure for window print. */
static void
print_window_done(int status)
{
    if (status) {
	popup_an_error("Print program exited with status %d.",
		(status & 0xff00) >> 8);
    } else if (appres.interactive.do_confirms) {
	popup_an_info("Bitmap printed.");
    }
}

/* Timeout callback for window print. */
static void
snap_it(XtPointer closure _is_unused, XtIntervalId *id _is_unused)
{
    if (!print_window_command) {
	return;
    }
    vtrace("PrintWindow: Running '%s'\n", print_window_command);
    XSync(display, 0);
    print_window_done(system(print_window_command));
}

/*
 * Expand the window print command.
 *
 * The token used to substitute the window ID is "%d", which is a historical
 * artifact of the original, amazingly insecure implementation.
 */
static char *
expand_print_window_command(const char *command)
{
    const char *s;
    varbuf_t r;
#   define WINDOW	"%d"
#   define WINDOW_SIZE	(sizeof(WINDOW) - 1)

    vb_init(&r);
    s = command;
    while (*s) {
	if (!strncasecmp(s, WINDOW, WINDOW_SIZE)) {
	    vb_appendf(&r, "%ld", (unsigned long)XtWindow(toplevel));
	    s += WINDOW_SIZE;
	} else {
	    vb_append(&r, s, 1);
	    s++;
	}
    }
    return vb_consume(&r);
}

/* Callback for "OK" button on print window popup. */
static void
print_window_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
    char *cmd;

    cmd = XawDialogGetValueString((Widget)client_data);

    XtPopdown(print_window_shell);

    if (cmd) {
	/* Expand the command. */
	Replace(print_window_command, expand_print_window_command(cmd));

	/* In 1 second, snap the window. */
	XtAppAddTimeOut(appcontext, 1000, snap_it, 0);
    }
}

/* Print the contents of the screen as a bitmap. */
static bool
PrintWindow_action(ia_t ia, unsigned argc, const char **argv)
{
    const char *command;
    bool secure = appres.secure;

    action_debug(AnPrintWindow, ia, argc, argv);

    /* Figure out what the command is. */
    command = get_resource(ResPrintWindowCommand);
    if (argc > 0) {
	command = argv[0];
    }
    if (argc > 1) {
	popup_an_error(AnPrintWindow "(): Extra arguments ignored");
    }
    if (command == NULL || !*command) {
	popup_an_error(AnPrintWindow "(): No %s defined", ResPrintWindowCommand);
	return false;
    }

    /* Check for secure mode. */
    if (command[0] == '@') {
	secure = true;
	if (!*++command) {
	    popup_an_error(AnPrintWindow "(): Invalid %s", ResPrintWindowCommand);
	    return false;
	}
    }
    if (secure) {
	char *xcommand = expand_print_window_command(command);

	vtrace("PrintWindow: Running '%s'\n", xcommand);
	print_window_done(system(xcommand));
	XtFree(xcommand);
	return true;
    }

    /* Pop up the dialog. */
    if (print_window_shell == NULL) {
	print_window_shell = create_form_popup("printWindow",
		print_window_callback, NULL, FORM_AS_IS);
    }
    XtVaSetValues(XtNameToWidget(print_window_shell, ObjDialog),
	XtNvalue, command,
	NULL);
    popup_popup(print_window_shell, XtGrabExclusive);
    return true;
}

/* Callback for menu Print Window option. */
void
print_window_option(Widget w, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
    PrintWindow_action(IA_KEYMAP, 0, NULL);
}

/**
 * Window printing module registration.
 */
void
print_window_register(void)
{
    static action_table_t print_window_actions[] = {
	{ AnPrintWindow,	PrintWindow_action,	ACTION_KE }
    };

    /* Register the actions. */
    register_actions(print_window_actions, array_count(print_window_actions));
}
