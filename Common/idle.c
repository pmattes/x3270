/*
 * Copyright 1993, 1994, 1995, 2002, 2003, 2005 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	idle.c
 *		This module handles the idle command.
 */

#include "globals.h"

#if defined(X3270_SCRIPT) /*[*/

#if defined(X3270_DISPLAY) /*[*/
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
#endif /*]*/
#include <errno.h>

#include "appres.h"
#include "dialogc.h"
#include "hostc.h"
#include "idlec.h"
#include "macrosc.h"
#include "objects.h"
#include "popupsc.h"
#include "resources.h"
#include "trace_dsc.h"
#include "utilc.h"

/* Macros. */
#define MSEC_PER_SEC	1000L
#define IDLE_SEC	1L
#define IDLE_MIN	60L
#define IDLE_HR		(60L * 60L)
#define IDLE_MS		(7L * IDLE_MIN * MSEC_PER_SEC)

#if defined(X3270_DISPLAY) /*[*/
#define FILE_WIDTH	300	/* width of file name widgets */
#define MARGIN		3	/* distance from margins to widgets */
#define CLOSE_VGAP	0	/* distance between paired toggles */
#define FAR_VGAP	10	/* distance between single toggles and groups */
#define BUTTON_GAP	5	/* horizontal distance between buttons */
#endif /*]*/

#define BN	(Boolean *)NULL

/* Externals. */
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
extern Pixmap diamond;
extern Pixmap no_diamond;
extern Pixmap dot;
extern Pixmap no_dot;
#endif /*]*/

/* Globals. */
Boolean idle_changed = False;
char *idle_command = CN;
char *idle_timeout_string = CN;
enum idle_enum idle_user_enabled = IDLE_DISABLED;

/* Statics. */
static Boolean idle_enabled = False;	/* validated and user-enabled */
static unsigned long idle_n = 0L;
static unsigned long idle_multiplier = IDLE_SEC;
static unsigned long idle_id;
static unsigned long idle_ms;
static Boolean idle_randomize = False;
static Boolean idle_ticking = False;

static void idle_in3270(Boolean in3270);
static int process_timeout_value(char *t);

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static enum idle_enum s_disabled = IDLE_DISABLED;
static enum idle_enum s_session = IDLE_SESSION;
static enum idle_enum s_perm = IDLE_PERM;
static char hms = 'm';
static Boolean fuzz = False;
static char s_hours = 'h';
static char s_minutes = 'm';
static char s_seconds = 's';
static Widget idle_dialog, idle_shell, command_value, timeout_value;
static Widget enable_toggle, enable_perm_toggle, disable_toggle;
static Widget hours_toggle, minutes_toggle, seconds_toggle, fuzz_toggle;
static sr_t *idle_sr = (sr_t *)NULL;

static void idle_cancel(Widget w, XtPointer client_data, XtPointer call_data);
static void idle_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void idle_popup_init(void);
static int idle_start(void);
static void okay_callback(Widget w, XtPointer call_parms,
    XtPointer call_data);
static void toggle_enable(Widget w, XtPointer client_data,
    XtPointer call_data);
static void mark_toggle(Widget w, Pixmap p);
static void toggle_hms(Widget w, XtPointer client_data,
    XtPointer call_data);
static void toggle_fuzz(Widget w, XtPointer client_data,
    XtPointer call_data);
#endif /*]*/

/* Initialization. */
void
idle_init(void)
{
	char *cmd, *tmo;

	/* Register for state changes. */
	register_schange(ST_3270_MODE, idle_in3270);
	register_schange(ST_CONNECT, idle_in3270);

	/* Get values from resources. */
	cmd = get_resource(ResIdleCommand);
	idle_command = cmd? NewString(cmd): CN;
	tmo = get_resource(ResIdleTimeout);
	idle_timeout_string = tmo? NewString(tmo): CN;
	if (appres.idle_command_enabled)
		idle_user_enabled = IDLE_PERM;
	else
		idle_user_enabled = IDLE_DISABLED;
	if (idle_user_enabled &&
	    idle_command != CN &&
	    process_timeout_value(idle_timeout_string) == 0)
		idle_enabled = True;

	/* Seed the random number generator (we seem to be the only user). */
	srandom(time(NULL));
}

/*
 * Process a timeout value: <empty> or ~?[0-9]+[HhMmSs]
 * Returns 0 for success, -1 for failure.
 * Sets idle_ms and idle_randomize as side-effects.
 */
static int
process_timeout_value(char *t)
{
	char *s = t;
	char *ptr;

	if (s == CN || *s == '\0') {
		idle_ms = IDLE_MS;
		idle_randomize = True;
		return 0;
	}

	if (*s == '~') {
		idle_randomize = True;
		s++;
	}
	idle_n = strtoul(s, &ptr, 0);
	if (idle_n <= 0)
		goto bad_idle;
	switch (*ptr) {
	    case 'H':
	    case 'h':
		idle_multiplier = IDLE_HR;
		break;
	    case 'M':
	    case 'm':
		idle_multiplier = IDLE_MIN;
		break;
	    case 'S':
	    case 's':
	    case '\0':
		idle_multiplier = IDLE_SEC;
		break;
	    default:
		goto bad_idle;
	}
	idle_ms = idle_n * idle_multiplier * MSEC_PER_SEC;
	return 0;

    bad_idle:
	popup_an_error("Invalid idle timeout value '%s'", t);
	idle_ms = 0L;
	idle_randomize = False;
	return -1;
}

/* Called when a host connects or disconnects. */
static void
idle_in3270(Boolean in3270 unused)
{
	if (IN_3270) {
		reset_idle_timer();
	} else {
		/* Not in 3270 mode any more, turn off the timeout. */
		if (idle_ticking) {
			RemoveTimeOut(idle_id);
			idle_ticking = False;
		}

		/* If the user didn't want it to be permanent, disable it. */
		if (idle_user_enabled != IDLE_PERM)
			idle_user_enabled = IDLE_DISABLED;
	}
}

/*
 * Idle timeout.
 */
static void
idle_timeout(void)
{
	trace_event("Idle timeout\n");
	push_idle(idle_command);
	reset_idle_timer();
}

/*
 * Reset (and re-enable) the idle timer.  Called when the user presses a key or
 * clicks with the mouse.
 */
void
reset_idle_timer(void)
{
	if (idle_enabled) {
		unsigned long idle_ms_now;

		if (idle_ticking) {
			RemoveTimeOut(idle_id);
			idle_ticking = False;
		}
		idle_ms_now = idle_ms;
		if (idle_randomize) {
			idle_ms_now = idle_ms;
			idle_ms_now -= random() % (idle_ms / 10L);
		}
#if defined(DEBUG_IDLE_TIMEOUT) /*[*/
		trace_event("Setting idle timeout to %lu\n", idle_ms_now);
#endif /*]*/
		idle_id = AddTimeOut(idle_ms_now, idle_timeout);
		idle_ticking = True;
	}
}

/*
 * Cancel the idle timer.  This is called when there is an error in
 * processing the idle command.
 */
void
cancel_idle_timer(void)
{
	if (idle_ticking) {
		RemoveTimeOut(idle_id);
		idle_ticking = False;
	}
	idle_enabled = False;
}

char *
get_idle_command(void)
{
	return idle_command;
}

char *
get_idle_timeout(void)
{
	return idle_timeout_string;
}

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
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
	if (idle_shell == (Widget)NULL)
		idle_popup_init();

	/*
	 * Split the idle_timeout_string (the raw resource value) into fuzz,
	 * a number, and h/m/s.
	 */
	its = NewString(idle_timeout_string);
	if (its != CN) {
		if (*its == '~') {
			fuzz = True;
			its++;
		} else {
			fuzz = False;
		}
		s = its;
		while (isdigit(*s))
			s++;
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
	XtAddCallback(idle_shell, XtNpopupCallback, idle_popup_callback,
	    (XtPointer)NULL);

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
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_command);
	dialog_register_sensitivity(command_value,
	    BN, False,
	    BN, False,
	    BN, False);

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
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_numeric);
	dialog_register_sensitivity(timeout_value,
	    BN, False,
	    BN, False,
	    BN, False);

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
	XtAddCallback(fuzz_toggle, XtNcallback, toggle_fuzz, (XtPointer)NULL);

	/* Create enable/disable toggles. */
	enable_toggle = XtVaCreateManagedWidget(
	    "enable", commandWidgetClass, idle_dialog,
	    XtNfromVert, fuzz_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(enable_toggle,
			(idle_user_enabled == IDLE_SESSION)?
			    diamond : no_diamond);
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
			(idle_user_enabled == IDLE_PERM)?
			    diamond : no_diamond);
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
			(idle_user_enabled == IDLE_DISABLED)?
			    diamond : no_diamond);
	XtAddCallback(disable_toggle, XtNcallback, toggle_enable,
	    (XtPointer)&s_disabled);

	/* Set up the buttons at the bottom. */
	okay_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, idle_dialog,
	    XtNfromVert, disable_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
	XtAddCallback(okay_button, XtNcallback, okay_callback,
	    (XtPointer)NULL);

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
idle_popup_callback(Widget w unused, XtPointer client_data unused,
	XtPointer call_data unused)
{
	/* Set the focus to the command widget. */
	PA_dialog_focus_action(command_value, (XEvent *)NULL, (String *)NULL,
	    (Cardinal *)NULL);
}

/* Cancel button pushed. */
static void
idle_cancel(Widget w unused, XtPointer client_data unused,
	XtPointer call_data unused)
{
	XtPopdown(idle_shell);
}

/* OK button pushed. */
static void
okay_callback(Widget w unused, XtPointer call_parms unused,
	XtPointer call_data unused)
{
	if (idle_start() == 0) {
		idle_changed = True;
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
toggle_hms(Widget w unused, XtPointer client_data,
	XtPointer call_data unused)
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
toggle_fuzz(Widget w unused, XtPointer client_data,
	XtPointer call_data unused)
{
	/* Toggle the flag */
	fuzz = !fuzz;

	/* Change the widget state. */
	mark_toggle(fuzz_toggle, fuzz ? dot : no_dot);
}

/* Enable/disable options. */
static void
toggle_enable(Widget w unused, XtPointer client_data,
	XtPointer call_data unused)
{
	/* Toggle the flag */
	idle_user_enabled = *(enum idle_enum *)client_data;

	/* Change the widget states. */
	mark_toggle(enable_toggle,
			(idle_user_enabled == IDLE_SESSION)?
			    diamond: no_diamond);
	mark_toggle(enable_perm_toggle,
			(idle_user_enabled == IDLE_PERM)?
			    diamond: no_diamond);
	mark_toggle(disable_toggle,
			(idle_user_enabled == IDLE_DISABLED)?
			    diamond: no_diamond);
}

/*
 * Called when the user presses the OK button on the idle command dialog.
 * Returns 0 for success, -1 otherwise.
 */
static int
idle_start(void)
{
	char *cmd, *tmo, *its;

	/* Update the globals, so the dialog has the same values next time. */
	XtVaGetValues(command_value, XtNstring, &cmd, NULL);
	Replace(idle_command, NewString(cmd));
	XtVaGetValues(timeout_value, XtNstring, &tmo, NULL);
	its = Malloc(strlen(tmo) + 3);
	(void) sprintf(its, "%s%s%c", fuzz? "~": "", tmo, hms);
	Replace(idle_timeout_string, its);

	/* See if they've turned it off. */
	if (!idle_user_enabled) {
		/* If they're turned it off, cancel the timer. */
		idle_enabled = False;
		if (idle_ticking) {
			RemoveTimeOut(idle_id);
			idle_ticking = False;
		}
		return 0;
	}

	/* They've turned it on, and possibly reconfigured it. */

	/* Validate the timeout.  It should work, yes? */
	if (process_timeout_value(its) < 0) {
		return -1;
	}

	/* Seems okay.  Reset to the new interval and command. */
	idle_enabled = True;
	if (IN_3270) {
		reset_idle_timer();
	}
	return 0;
}

#endif /*]*/

#endif /*]*/
