/*
 * Copyright (c) 2013-2015, 2019-2020 Paul Mattes.
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
 *     * Neither the names of Paul Mattes nor his contributors may be used to
 *       endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	stmenu.c
 *		Pop-up window to initiate screen tracing.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/TextSrc.h>
#include <X11/Xaw/TextSink.h>
#include <X11/Xaw/AsciiSrc.h>
#include <X11/Xaw/AsciiSink.h>
#include "appres.h"
#include "objects.h"
#include "resources.h"

#include "dialog.h"
#include "popups.h"
#include "print_screen.h"
#include "stmenu.h"
#include "toggles.h"
#include "trace.h"
#include "screentrace.h"
#include "xmenubar.h"
#include "xpopups.h"

#define CLOSE_VGAP	0
#define FAR_VGAP	10
#define FAR_HGAP	65
#define MARGIN		3
#define FILE_WIDTH	300
#define BUTTON_GAP	5

static Widget stmenu_shell = NULL;
static Widget stmenu_form;

static bool continuously_flag = true;	/* save continuously */
static Widget continuously_toggle = NULL;
static Widget once_toggle = NULL;

static bool file_flag = true;		/* save in file */
static Widget file_toggle = NULL;
static Widget printer_toggle = NULL;

static ptype_t stm_ptype = P_TEXT;		/* save as text/html/rtf */
static Widget text_toggle = NULL;
static Widget html_toggle = NULL;
static Widget rtf_toggle = NULL;

static Widget filename_label = NULL;
static Widget filename = NULL;
static Widget print_command_label = NULL;
static Widget print_command = NULL;

static ptype_t s_text = P_TEXT;
static ptype_t s_html = P_HTML;
static ptype_t s_rtf = P_RTF;

/* Called when OK is pressed in the screentrace popup. */
static void
screentrace_ok(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    String name;

    if (file_flag) {
	XtVaGetValues(filename, XtNstring, &name, NULL);
    } else {
	XtVaGetValues(print_command, XtNstring, &name, NULL);
    }
    trace_set_screentrace_file(file_flag? TSS_FILE: TSS_PRINTER,
	    file_flag? stm_ptype: P_TEXT, 0, name);
    do_toggle(SCREEN_TRACE);
    if (!continuously_flag && toggled(SCREEN_TRACE)) {
	do_toggle(SCREEN_TRACE);
    }

    XtPopdown(stmenu_shell);
}

/* Called when Cancel is pressed in the screentrace popup. */
static void
screentrace_cancel(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(stmenu_shell);
}

/* Screentrace pop-up popping up. */
static void
stmenu_popup_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Set the focus to the file or printer text widget. */
    XawTextDisplayCaret(filename, file_flag);
    XawTextDisplayCaret(print_command, !file_flag);
    XtSetKeyboardFocus(stmenu_form, file_flag? filename: print_command);
}

/* Continuously/Once toggle callback. */
static void
toggle_continuously(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag. */
    continuously_flag = *(bool *)client_data;

    /* Change the widget states. */
    dialog_mark_toggle(continuously_toggle,
	    continuously_flag? diamond: no_diamond);
    dialog_mark_toggle(once_toggle,
	    continuously_flag? no_diamond: diamond);
}

/* File/Printer toggle callback. */
static void
toggle_file(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag. */
    file_flag = *(bool *)client_data;

    /* Change the widget states. */
    dialog_mark_toggle(file_toggle, file_flag? diamond: no_diamond);
    dialog_mark_toggle(printer_toggle, file_flag? no_diamond: diamond);
    XtVaSetValues(filename_label, XtNsensitive, file_flag, NULL);
    XtVaSetValues(filename, XtNsensitive, file_flag, NULL);
    XtVaSetValues(text_toggle, XtNsensitive, file_flag, NULL);
    XtVaSetValues(html_toggle, XtNsensitive, file_flag, NULL);
    XtVaSetValues(rtf_toggle, XtNsensitive, file_flag, NULL);
    XtVaSetValues(print_command_label, XtNsensitive, !file_flag, NULL);
    XtVaSetValues(print_command, XtNsensitive, !file_flag, NULL);
    XawTextDisplayCaret(filename, file_flag);
    XawTextDisplayCaret(print_command, !file_flag);
    XtSetKeyboardFocus(stmenu_form, file_flag? filename: print_command);
}

/* Text/HTML toggle callback. */
static void
toggle_ptype(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *d;
    XawTextBlock b;
    String name;

    XtVaGetValues(filename, XtNstring, &name, NULL);

    /* Toggle the flag. */
    stm_ptype = *(bool *)client_data;

    /* Change the widget states. */
    dialog_mark_toggle(text_toggle,
	    (stm_ptype == P_TEXT)? diamond: no_diamond);
    dialog_mark_toggle(html_toggle,
	    (stm_ptype == P_HTML)? diamond: no_diamond);
    dialog_mark_toggle(rtf_toggle,
	    (stm_ptype == P_RTF)? diamond: no_diamond);

    d = screentrace_default_file(stm_ptype);
    b.firstPos = 0;
    b.length = strlen(d);
    b.ptr = d;
    b.format = XawFmt8Bit;
    XawTextReplace(filename, 0, strlen(name), &b);
    XawTextSetInsertionPoint(filename, strlen(d));
    XtFree(d);
}

/*
 * Initialize the screentrace (Save Screens) pop-up.
 *
 * The pop-up consists of:
 *  A pair of radio buttons for Continuously/Once
 *  A pair of radio buttons for File/Printer
 *  A pair of radio buttons for Text/HTML
 *  A label for "File Name" or "Print Command"
 *  A text box to fill in the above
 *  An OK button
 *  An Abort button
 *
 *  The radio buttons work like radio buttons.
 *  When File/Printer is toggled, the label for the text box flips, and the
 *  text box contents switch to the last value used for that type (or an
 *  appropriate default).
 */
void
init_screentrace_popup(void)
{
    Widget w = NULL;
    Widget confirm_button, cancel_button;
    char *d;
    XawTextBlock b;

    /* Get the defaults. */
    file_flag = trace_get_screentrace_target() == TSS_FILE;
    stm_ptype = trace_get_screentrace_type();

    /* Create the popup. */
    stmenu_shell = XtVaCreatePopupShell(
	    "screenTracePopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(stmenu_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
    XtAddCallback(stmenu_shell, XtNpopupCallback, stmenu_popup_callback,
	    NULL);

    /* Create a form in the popup. */
    stmenu_form = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, stmenu_shell,
	    NULL);

    /* Create the Continuously/Once radio buttons. */
    continuously_toggle = XtVaCreateManagedWidget(
	    "continuously", commandWidgetClass, stmenu_form,
	    XtNvertDistance, MARGIN,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNjustify, XtJustifyLeft,
	    NULL);
    dialog_apply_bitmap(continuously_toggle,
	    continuously_flag? diamond: no_diamond);
    XtAddCallback(continuously_toggle, XtNcallback, toggle_continuously,
	    (XtPointer)&s_true);
    once_toggle = XtVaCreateManagedWidget(
	    "once", commandWidgetClass, stmenu_form,
	    XtNfromVert, continuously_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNjustify, XtJustifyLeft,
	    NULL);
    dialog_apply_bitmap(once_toggle,
	    continuously_flag? no_diamond: diamond);
    XtAddCallback(once_toggle, XtNcallback, toggle_continuously,
	    (XtPointer)&s_false);
    dialog_match_dimension(continuously_toggle, once_toggle, XtNwidth);

    /* Create the File radio button. */
    file_toggle = XtVaCreateManagedWidget(
	    "file", commandWidgetClass, stmenu_form,
	    XtNfromVert, once_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNjustify, XtJustifyLeft,
	    NULL);
    dialog_apply_bitmap(file_toggle,
	    file_flag? diamond: no_diamond);
    XtAddCallback(file_toggle, XtNcallback, toggle_file,
	    (XtPointer)&s_true);

    /* Create the file name label and text widgets. */
    filename_label = XtVaCreateManagedWidget(
	    "fileName", labelWidgetClass, stmenu_form,
	    XtNfromVert, file_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, FAR_HGAP,
	    XtNborderWidth, 0,
	    XtNjustify, XtJustifyLeft,
	    XtNsensitive, file_flag,
	    NULL);
    filename = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, stmenu_form,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNfromVert, file_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, filename_label,
	    XtNhorizDistance, 0,
	    XtNsensitive, file_flag,
	    NULL);
    dialog_match_dimension(filename_label, filename, XtNheight);
    w = XawTextGetSource(filename);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_unixfile);
    }
    d = screentrace_default_file(stm_ptype);
    b.firstPos = 0;
    b.length = strlen(d);
    b.ptr = d;
    b.format = XawFmt8Bit;
    XawTextReplace(filename, 0, 0, &b);
    XawTextSetInsertionPoint(filename, strlen(d));
    XtFree(d);

    /* Create the Text/HTML/RTF radio buttons. */
    text_toggle = XtVaCreateManagedWidget(
	    "text", commandWidgetClass, stmenu_form,
	    XtNfromVert, filename_label,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, FAR_HGAP,
	    XtNborderWidth, 0,
	    XtNsensitive, file_flag,
	    NULL);
    dialog_apply_bitmap(text_toggle,
	    (stm_ptype == P_TEXT)? diamond: no_diamond);
    XtAddCallback(text_toggle, XtNcallback, toggle_ptype,
	    (XtPointer)&s_text);
    html_toggle = XtVaCreateManagedWidget(
	    "html", commandWidgetClass, stmenu_form,
	    XtNfromVert, text_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, FAR_HGAP,
	    XtNborderWidth, 0,
	    XtNsensitive, file_flag,
	    NULL);
    dialog_apply_bitmap(html_toggle,
	    (stm_ptype == P_HTML)? diamond: no_diamond);
    XtAddCallback(html_toggle, XtNcallback, toggle_ptype,
	    (XtPointer)&s_html);
    rtf_toggle = XtVaCreateManagedWidget(
	    "rtf", commandWidgetClass, stmenu_form,
	    XtNfromVert, html_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, FAR_HGAP,
	    XtNborderWidth, 0,
	    XtNsensitive, file_flag,
	    NULL);
    dialog_apply_bitmap(rtf_toggle,
	    (stm_ptype == P_RTF)? diamond: no_diamond);
    XtAddCallback(rtf_toggle, XtNcallback, toggle_ptype,
	    (XtPointer)&s_rtf);

    /* Create the printer toggle. */
    printer_toggle = XtVaCreateManagedWidget(
	    "printer", commandWidgetClass, stmenu_form,
	    XtNhorizDistance, MARGIN,
	    XtNfromVert, rtf_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNborderWidth, 0,
	    XtNjustify, XtJustifyLeft,
	    NULL);
    dialog_apply_bitmap(printer_toggle,
	    file_flag? no_diamond: diamond);
    XtAddCallback(printer_toggle, XtNcallback, toggle_file,
	    (XtPointer)&s_false);

    /* Create the print command label and text widgets. */
    print_command_label = XtVaCreateManagedWidget(
	    "printCommand", labelWidgetClass, stmenu_form,
	    XtNfromVert, printer_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, FAR_HGAP,
	    XtNborderWidth, 0,
	    XtNjustify, XtJustifyLeft,
	    XtNsensitive, !file_flag,
	    NULL);
    print_command = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, stmenu_form,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNfromVert, printer_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, print_command_label,
	    XtNhorizDistance, 0,
	    XtNsensitive, !file_flag,
	    NULL);
    dialog_match_dimension(print_command_label, print_command, XtNheight);
    dialog_match_dimension(filename_label, print_command_label, XtNwidth);
    w = XawTextGetSource(print_command);
    if (w == NULL) {
	    XtWarning("Cannot find text source in dialog");
    } else {
	    XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_command);
    }
    d = screentrace_default_printer();
    b.firstPos = 0;
    b.length = strlen(d);
    b.ptr = d;
    b.format = XawFmt8Bit;
    XawTextReplace(print_command, 0, 0, &b);
    XawTextSetInsertionPoint(print_command, strlen(d));
    XtFree(d);

    /* Create the buttons. */
    confirm_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, stmenu_form,
	    XtNfromVert, print_command_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(confirm_button, XtNcallback, screentrace_ok, NULL);
    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, stmenu_form,
	    XtNfromVert, print_command_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, confirm_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, screentrace_cancel, NULL);
}

/*
 * Pop up the Screen Trace menu.
 * Called from the "Save Screen Contents" option on the File menu.
 */
void
stmenu_popup(stmp_t stmp)
{
    /* If the toggle is set, clear it. */
    if (toggled(SCREEN_TRACE)) {
	do_toggle(SCREEN_TRACE);
	return;
    }

    /* Initialize it. */
    if (stmenu_shell == NULL) {
	init_screentrace_popup();
    }

    switch (stmp) {
    case STMP_AS_IS:
	break;
    case STMP_TEXT:
	/* Force a text file. */
	if (!file_flag) {
	    toggle_file(NULL, &s_true, NULL);
	}
	if (stm_ptype != P_TEXT) {
	    toggle_ptype(NULL, &s_text, NULL);
	}
	if (continuously_flag) {
	    toggle_continuously(NULL, &s_false, NULL);
	}
	break;
    case STMP_PRINTER:
	/* Force a printer. */
	if (file_flag) {
	    toggle_file(NULL, &s_false, NULL);
	}
	if (continuously_flag) {
	    toggle_continuously(NULL, &s_false, NULL);
	}
	break;
    }

    /* Pop it up. */
    popup_popup(stmenu_shell, XtGrabExclusive);
}
