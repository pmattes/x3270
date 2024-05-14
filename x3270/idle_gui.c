/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *	idle_gui.c
 *		This module handles the idle command GUI.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Shell.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/TextSrc.h>
#include <X11/Xaw/TextSink.h>
#include <X11/Xaw/AsciiSrc.h>
#include <X11/Xaw/AsciiSink.h>

#include <errno.h>

#include "appres.h"
#include "dialog.h"
#include "idle.h"
#include "idle_gui.h"
#include "objects.h"
#include "popups.h"
#include "task.h"
#include "utils.h"
#include "xmenubar.h"
#include "xpopups.h"

/* Macros. */
#define FILE_WIDTH	300	/* width of file name widgets */
#define MARGIN		3	/* distance from margins to widgets */
#define CLOSE_VGAP	0	/* distance between paired toggles */
#define FAR_VGAP	10	/* distance between single toggles and groups */
#define BUTTON_GAP	5	/* horizontal distance between buttons */

/* Globals. */

/* Statics. */
static enum idle_enum s_disabled = IDLE_DISABLED;
static enum idle_enum s_session = IDLE_SESSION;
static enum idle_enum s_perm = IDLE_PERM;
static char hms = 'm';
static bool fuzz = false;
static char s_hours = 'h';
static char s_minutes = 'm';
static char s_seconds = 's';
static Widget idle_dialog, idle_shell, command_value, timeout_value;
static Widget enable_toggle, enable_perm_toggle, disable_toggle;
static Widget hours_toggle, minutes_toggle, seconds_toggle, fuzz_toggle;
static sr_t *idle_sr = NULL;

static void idle_cancel(Widget w, XtPointer client_data, XtPointer call_data);
static void idle_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void idle_popup_init(void);
static bool idle_start(void);
static void okay_callback(Widget w, XtPointer call_parms,
    XtPointer call_data);
static void toggle_enable(Widget w, XtPointer client_data,
    XtPointer call_data);
static void mark_toggle(Widget w, Pixmap p);
static void toggle_hms(Widget w, XtPointer client_data,
    XtPointer call_data);
static void toggle_fuzz(Widget w, XtPointer client_data,
    XtPointer call_data);

/* "Idle Command" dialog. */

/*
 * Pop up the "Idle" menu.
 * Called back from the "Configure Idle Command" option on the Options menu.
 */
void
popup_idle(void)
{
    char *its;
    char *s;

    /* Initialize it. */
    if (idle_shell == NULL) {
	idle_popup_init();
    }

    /*
     * Split the idle_timeout_string (the raw resource value) into fuzz,
     * a number, and h/m/s.
     */
    its = NewString(idle_timeout_string);
    if (its != NULL) {
	if (*its == '~') {
	    fuzz = true;
	    its++;
	} else {
	    fuzz = false;
	}
	s = its;
	while (isdigit((unsigned char)*s)) {
	    s++;
	}
	switch (*s) {
	case 'h':
	case 'H':
	    hms = 'h';
	    break;
	case 'm':
	case 'M':
	    hms = 'm';
	    break;
	case 's':
	case 'S':
	    hms = 's';
	    break;
	default:
	    break;
	}
	*s = '\0';
    }

    /* Set the resource values. */
    dialog_set(&idle_sr, idle_dialog);
    XtVaSetValues(command_value,
	    XtNstring, idle_command,
	    NULL);
    XtVaSetValues(timeout_value, XtNstring, its, NULL);
    mark_toggle(enable_toggle, (idle_user_enabled == IDLE_SESSION)?
	    diamond : no_diamond);
    mark_toggle(enable_perm_toggle, (idle_user_enabled == IDLE_PERM)?
	    diamond : no_diamond);
    mark_toggle(disable_toggle, (idle_user_enabled == IDLE_DISABLED)?
	    diamond : no_diamond);
    mark_toggle(hours_toggle, (hms == 'h') ? diamond : no_diamond);
    mark_toggle(minutes_toggle, (hms == 'm') ? diamond : no_diamond);
    mark_toggle(seconds_toggle, (hms == 's') ? diamond : no_diamond);
    mark_toggle(fuzz_toggle, fuzz ? dot : no_dot);

    /* Pop it up. */
    popup_popup(idle_shell, XtGrabNone);
}

/* Initialize the idle pop-up. */
static void
idle_popup_init(void)
{
    Widget w;
    Widget cancel_button;
    Widget command_label, timeout_label;
    Widget okay_button;

    /* Prime the dialog functions. */
    dialog_set(&idle_sr, idle_dialog);

    /* Create the menu shell. */
    idle_shell = XtVaCreatePopupShell(
	    "idlePopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(idle_shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
    XtAddCallback(idle_shell, XtNpopupCallback, idle_popup_callback, NULL);

    /* Create the form within the shell. */
    idle_dialog = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, idle_shell,
	    NULL);

    /* Create the file name widgets. */
    command_label = XtVaCreateManagedWidget(
	    "command", labelWidgetClass, idle_dialog,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    command_value = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, idle_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, command_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(command_label, command_value, XtNheight);
    w = XawTextGetSource(command_value);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_command);
    }
    dialog_register_sensitivity(command_value,
	    NULL, False,
	    NULL, False,
	    NULL, False);

    timeout_label = XtVaCreateManagedWidget(
	    "timeout", labelWidgetClass, idle_dialog,
	    XtNfromVert, command_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    timeout_value = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, idle_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNdisplayCaret, False,
	    XtNfromVert, command_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, timeout_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(timeout_label, timeout_value, XtNheight);
    dialog_match_dimension(command_label, timeout_label, XtNwidth);
    w = XawTextGetSource(timeout_value);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(timeout_value,
	    NULL, False,
	    NULL, False,
	    NULL, False);

    /* Create the hour/minute/seconds radio buttons. */
    hours_toggle = XtVaCreateManagedWidget(
	    "hours", commandWidgetClass, idle_dialog,
	    XtNfromVert, timeout_value,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNsensitive, True,
	    NULL);
    dialog_apply_bitmap(hours_toggle, no_diamond);
    XtAddCallback(hours_toggle, XtNcallback, toggle_hms,
	    (XtPointer)&s_hours);
    minutes_toggle = XtVaCreateManagedWidget(
	    "minutes", commandWidgetClass, idle_dialog,
	    XtNfromVert, timeout_value,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, hours_toggle,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNsensitive, True,
	    NULL);
    dialog_apply_bitmap(minutes_toggle, diamond);
    XtAddCallback(minutes_toggle, XtNcallback, toggle_hms,
	    (XtPointer)&s_minutes);
    seconds_toggle = XtVaCreateManagedWidget(
	    "seconds", commandWidgetClass, idle_dialog,
	    XtNfromVert, timeout_value,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, minutes_toggle,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNsensitive, True,
	    NULL);
    dialog_apply_bitmap(seconds_toggle, no_diamond);
    XtAddCallback(seconds_toggle, XtNcallback, toggle_hms,
	    (XtPointer)&s_seconds);

    /* Create the fuzz toggle. */
    fuzz_toggle = XtVaCreateManagedWidget(
	    "fuzz", commandWidgetClass, idle_dialog,
	    XtNfromVert, hours_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNsensitive, True,
	    NULL);
    dialog_apply_bitmap(fuzz_toggle, no_dot);
    XtAddCallback(fuzz_toggle, XtNcallback, toggle_fuzz, NULL);

    /* Create enable/disable toggles. */
    enable_toggle = XtVaCreateManagedWidget(
	    "enable", commandWidgetClass, idle_dialog,
	    XtNfromVert, fuzz_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(enable_toggle,
	    (idle_user_enabled == IDLE_SESSION)? diamond: no_diamond);
    XtAddCallback(enable_toggle, XtNcallback, toggle_enable,
	    (XtPointer)&s_session);
    enable_perm_toggle = XtVaCreateManagedWidget(
	    "enablePerm", commandWidgetClass, idle_dialog,
	    XtNfromVert, enable_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(enable_perm_toggle,
	    (idle_user_enabled == IDLE_PERM)? diamond: no_diamond);
    XtAddCallback(enable_perm_toggle, XtNcallback, toggle_enable,
	    (XtPointer)&s_perm);
    disable_toggle = XtVaCreateManagedWidget(
	    "disable", commandWidgetClass, idle_dialog,
	    XtNfromVert, enable_perm_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(disable_toggle,
	    (idle_user_enabled == IDLE_DISABLED)? diamond: no_diamond);
    XtAddCallback(disable_toggle, XtNcallback, toggle_enable,
	    (XtPointer)&s_disabled);

    /* Set up the buttons at the bottom. */
    okay_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, idle_dialog,
	    XtNfromVert, disable_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(okay_button, XtNcallback, okay_callback, NULL);

    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, idle_dialog,
	    XtNfromVert, disable_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, okay_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, idle_cancel, 0);
}

/* Callbacks for all the idle widgets. */

/* Idle pop-up popping up. */
static void
idle_popup_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Set the focus to the command widget. */
    PA_dialog_focus_xaction(command_value, NULL, NULL, NULL);
}

/* Cancel button pushed. */
static void
idle_cancel(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(idle_shell);
}

/* OK button pushed. */
static void
okay_callback(Widget w _is_unused, XtPointer call_parms _is_unused,
	XtPointer call_data _is_unused)
{
    if (idle_start()) {
	idle_changed = true;
	XtPopdown(idle_shell);
    }
}

/* Mark a toggle. */
static void
mark_toggle(Widget w, Pixmap p)
{
    XtVaSetValues(w, XtNleftBitmap, p, NULL);
}

/* Hour/minute/second options. */
static void
toggle_hms(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag */
    hms = *(char *)client_data;

    /* Change the widget states. */
    mark_toggle(hours_toggle, (hms == 'h') ? diamond : no_diamond);
    mark_toggle(minutes_toggle, (hms == 'm') ? diamond : no_diamond);
    mark_toggle(seconds_toggle, (hms == 's') ? diamond : no_diamond);
}

/* Fuzz option. */
static void
toggle_fuzz(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag */
    fuzz = !fuzz;

    /* Change the widget state. */
    mark_toggle(fuzz_toggle, fuzz ? dot : no_dot);
}

/* Enable/disable options. */
static void
toggle_enable(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag */
    idle_user_enabled = *(enum idle_enum *)client_data;

    /* Change the widget states. */
    mark_toggle(enable_toggle, (idle_user_enabled == IDLE_SESSION)?
	    diamond: no_diamond);
    mark_toggle(enable_perm_toggle, (idle_user_enabled == IDLE_PERM)?
	    diamond: no_diamond);
    mark_toggle(disable_toggle, (idle_user_enabled == IDLE_DISABLED)?
	    diamond: no_diamond);
}

/*
 * Called when the user presses the OK button on the idle command dialog.
 * Returns true for success, false otherwise.
 */
static bool
idle_start(void)
{
    char *cmd, *tmo, *its;
    char *s;
    char *error;

    /* Update the globals, so the dialog has the same values next time. */
    XtVaGetValues(command_value, XtNstring, &cmd, NULL);
    XtVaGetValues(timeout_value, XtNstring, &tmo, NULL);
    Replace(idle_command, NewString(cmd));
    its = Asprintf("%s%s%c", fuzz? "~": "", tmo, hms);
    Replace(idle_timeout_string, its);

    /* See if they've turned it off. */
    if (!idle_user_enabled) {
	/* If they're turned it off, cancel the timer. */
	cancel_idle_timer();
	return true;
    }

    /* Validate the command. */
    s = cmd;
    while (isspace((int)*s)) {
	s++;
    }
    if (!*s) {
	popup_an_error("Missing idle command");
	return false;
    }
    if (!validate_command(cmd, 0, &error)) {
	popup_an_error("Invalid idle command:\n%s", error);
	Free(error);
	return false;
    }

    /* Validate the timeout. */
    if (!isdigit((int)its[0])) {
	popup_an_error("Missing timeout");
	return false;
    }
    if (!process_idle_timeout_value(its)) {
	return false;
    }

    /* Reset to the new interval and command. */
    if (IN_3270) {
	reset_idle_timer();
    }
    return true;
}
