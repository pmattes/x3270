/*
 * Copyright (c) 1996-2015 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	ft_gui.c
 *		IND$FILE file transfer dialogs.
 */

#include "globals.h"
#include "xglobals.h"

#include <assert.h>

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
#include "ft.h"
#include "ft_dft.h"
#include "ft_private.h"
#include "kybd.h"
#include "lazya.h"
#include "objects.h"
#include "popups.h"
#include "utils.h"
#include "varbuf.h"
#include "xmenubar.h"
#include "xpopups.h"

#include "ft_gui.h"

/* Macros. */
#define FILE_WIDTH	300	/* width of file name widgets */
#define MARGIN		3	/* distance from margins to widgets */
#define CLOSE_VGAP	0	/* distance between paired toggles */
#define FAR_VGAP	10	/* distance between single toggles and groups */
#define BUTTON_GAP	5	/* horizontal distance between buttons */
#define COLUMN_GAP	40	/* distance between columns */

/* Globals. */

/* Statics. */
static Widget ft_dialog, ft_shell, local_file, host_file;
static Widget lrecl_widget, blksize_widget;
static Widget primspace_widget, secspace_widget;
static Widget avblock_size_widget;
static Widget send_toggle, receive_toggle;
static Widget vm_toggle, tso_toggle, cics_toggle;
static Widget ascii_toggle, binary_toggle;
static Widget cr_widget;
static Widget remap_widget;
static Widget buffersize_widget;

static bool host_is_tso = true;	/* bools used by dialog */
static bool host_is_tso_or_vm = true;/*  sensitivity logic */
static host_type_t s_tso = HT_TSO;	/* Values used by toggle callbacks. */
static host_type_t s_vm = HT_VM;
static host_type_t s_cics = HT_CICS;
static Widget recfm_options[5];
static Widget units_options[5];
static struct toggle_list recfm_toggles = { recfm_options };
static struct toggle_list units_toggles = { units_options };

static bool recfm_default = true;
static recfm_t r_default_recfm = DEFAULT_RECFM;
static recfm_t r_fixed = RECFM_FIXED;
static recfm_t r_variable = RECFM_VARIABLE;
static recfm_t r_undefined = RECFM_UNDEFINED;

static bool units_default = true;
static bool units_avblock = false;
static units_t u_default_units = DEFAULT_UNITS;
static units_t u_tracks = TRACKS;
static units_t u_cylinders = CYLINDERS;
static units_t u_avblock = AVBLOCK;

static sr_t *ft_sr = NULL;

static Widget progress_shell, from_file, to_file;
static Widget ft_status, waiting, aborting;
static String status_string;

static Widget overwrite_shell;

static bool interactive_overwrite = false;

static ft_private_t xftp;
static bool xftp_initted = false;

static void ft_cancel(Widget w, XtPointer client_data, XtPointer call_data);
static void ft_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void ft_popup_init(void);
static bool ft_start(void);
static void ft_start_callback(Widget w, XtPointer call_parms,
    XtPointer call_data);
static void overwrite_cancel_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void overwrite_okay_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void overwrite_popdown(Widget w, XtPointer client_data,
    XtPointer call_data);
static void overwrite_popup_init(void);
static void popup_overwrite(void);
static void popup_progress(void);
static void progress_cancel_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void progress_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void progress_popup_init(void);
static void recfm_callback(Widget w, XtPointer user_data, XtPointer call_data);
static void toggle_append(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_ascii(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_cr(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_remap(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_receive(Widget w, XtPointer client_data,
    XtPointer call_data);
static void toggle_host_type(Widget w, XtPointer client_data,
    XtPointer call_data);
static void units_callback(Widget w, XtPointer user_data, XtPointer call_data);

/* "File Transfer" dialog. */

/*
 * Pop up the "Transfer" menu.
 * Called back from the "File Transfer" option on the File menu.
 */
void
ft_gui_popup_ft(void)
{
    /* Initialize it. */
    if (ft_shell == NULL) {
	ft_popup_init();
    }

    /* Pop it up. */
    dialog_set(&ft_sr, ft_dialog);
    popup_popup(ft_shell, XtGrabNone);
}

/* Initialize the transfer pop-up. */
static void
ft_popup_init(void)
{
    Widget w;
    Widget cancel_button;
    Widget local_label, host_label;
    Widget append_widget;
    Widget lrecl_label, blksize_label, primspace_label, secspace_label;
    Widget avblock_size_label;
    Widget h_ref = NULL;
#if 0
    Dimension d1;
    Dimension maxw = 0;
#endif
    Widget recfm_label, units_label;
    Widget buffersize_label;
    Widget start_button;
    Widget spacer_toggle;
    char *s;

    /* Init the file transfer state structure from defaults. */
    if (!xftp_initted) {
	ft_init_private(&xftp);
	xftp.is_action = false;
	xftp_initted = true;
    }

    recfm_default = (xftp.recfm == DEFAULT_RECFM);
    units_default = (xftp.units == DEFAULT_UNITS);
    units_avblock = (xftp.units == AVBLOCK);

    /* Prep the dialog functions. */
    dialog_set(&ft_sr, ft_dialog);

    /* Create the menu shell. */
    ft_shell = XtVaCreatePopupShell(
	    "ftPopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(ft_shell, XtNpopupCallback, place_popup, (XtPointer)CenterP);
    XtAddCallback(ft_shell, XtNpopupCallback, ft_popup_callback, NULL);

    /* Create the form within the shell. */
    ft_dialog = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, ft_shell,
	    NULL);

    /* Create the file name widgets. */
    local_label = XtVaCreateManagedWidget(
	    "local", labelWidgetClass, ft_dialog,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    local_file = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, local_label,
	    XtNhorizDistance, 0,
	    NULL);
    if (xftp.local_filename) {
	XtVaSetValues(local_file, XtNstring, xftp.local_filename, NULL);
	XawTextSetInsertionPoint(local_file, strlen(xftp.local_filename));
    }
    dialog_match_dimension(local_label, local_file, XtNheight);
    w = XawTextGetSource(local_file);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_unixfile);
    }
    dialog_register_sensitivity(local_file,
	    NULL, false,
	    NULL, false,
	    NULL, false);

    host_label = XtVaCreateManagedWidget(
	    "host", labelWidgetClass, ft_dialog,
	    XtNfromVert, local_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    host_file = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNdisplayCaret, False,
	    XtNfromVert, local_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, host_label,
	    XtNhorizDistance, 0,
	    NULL);
    if (xftp.host_filename) {
	XtVaSetValues(host_file, XtNstring, xftp.host_filename, NULL);
	XawTextSetInsertionPoint(host_file, strlen(xftp.host_filename));
    }
    dialog_match_dimension(host_label, host_file, XtNheight);
    dialog_match_dimension(local_label, host_label, XtNwidth);
    w = XawTextGetSource(host_file);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_hostfile);
    }
    dialog_register_sensitivity(host_file,
	    NULL, false,
	    NULL, false,
	    NULL, false);

    /* Create the left column. */

    /* Create send/receive toggles. */
    send_toggle = XtVaCreateManagedWidget(
	    "send", commandWidgetClass, ft_dialog,
	    XtNfromVert, host_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(send_toggle, xftp.receive_flag? no_diamond:
							      diamond);
    XtAddCallback(send_toggle, XtNcallback, toggle_receive,
	    (XtPointer)&s_false);
    receive_toggle = XtVaCreateManagedWidget(
	    "receive", commandWidgetClass, ft_dialog,
	    XtNfromVert, send_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(receive_toggle, xftp.receive_flag? diamond:
								 no_diamond);
    XtAddCallback(receive_toggle, XtNcallback, toggle_receive,
	    (XtPointer)&s_true);
    spacer_toggle = XtVaCreateManagedWidget(
	    "empty", labelWidgetClass, ft_dialog,
	    XtNfromVert, receive_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNlabel, "",
	    NULL);

    /* Create ASCII/binary toggles. */
    ascii_toggle = XtVaCreateManagedWidget(
	    "ascii", commandWidgetClass, ft_dialog,
	    XtNfromVert, spacer_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(ascii_toggle,
	    xftp.ascii_flag? diamond: no_diamond);
    XtAddCallback(ascii_toggle, XtNcallback, toggle_ascii, (XtPointer)&s_true);
    binary_toggle = XtVaCreateManagedWidget(
	    "binary", commandWidgetClass, ft_dialog,
	    XtNfromVert, ascii_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(binary_toggle,
	    xftp.ascii_flag? no_diamond: diamond);
    XtAddCallback(binary_toggle, XtNcallback, toggle_ascii,
	    (XtPointer)&s_false);

    /* Create append toggle. */
    append_widget = XtVaCreateManagedWidget(
	    "append", commandWidgetClass, ft_dialog,
	    XtNfromVert, binary_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(append_widget, xftp.append_flag? dot: no_dot);
    XtAddCallback(append_widget, XtNcallback, toggle_append, NULL);

    /* Set up the recfm group. */
    recfm_label = XtVaCreateManagedWidget(
	    "file", labelWidgetClass, ft_dialog,
	    XtNfromVert, append_widget,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(recfm_label,
	    &xftp.receive_flag, false,
	    &host_is_tso_or_vm, true,
	    NULL, false);

    recfm_options[0] = XtVaCreateManagedWidget(
	    "recfmDefault", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[0],
	    (xftp.recfm == DEFAULT_RECFM)? diamond: no_diamond);
    XtAddCallback(recfm_options[0], XtNcallback, recfm_callback,
	    (XtPointer)&r_default_recfm);
    dialog_register_sensitivity(recfm_options[0],
	    &xftp.receive_flag, false,
	    &host_is_tso_or_vm, true,
	    NULL, false);

    recfm_options[1] = XtVaCreateManagedWidget(
	    "fixed", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[0],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[1],
	    (xftp.recfm == RECFM_FIXED)? diamond: no_diamond);
    XtAddCallback(recfm_options[1], XtNcallback, recfm_callback,
	    (XtPointer)&r_fixed);
    dialog_register_sensitivity(recfm_options[1],
	    &xftp.receive_flag, false,
	    &host_is_tso_or_vm, true,
	    NULL, false);

    recfm_options[2] = XtVaCreateManagedWidget(
	    "variable", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[1],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[2],
	    (xftp.recfm == RECFM_VARIABLE)? diamond: no_diamond);
    XtAddCallback(recfm_options[2], XtNcallback, recfm_callback,
	    (XtPointer)&r_variable);
    dialog_register_sensitivity(recfm_options[2],
	    &xftp.receive_flag, false,
	    &host_is_tso_or_vm, true,
	    NULL, false);

    recfm_options[3] = XtVaCreateManagedWidget(
	    "undefined", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[2],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[3],
	    (xftp.recfm == RECFM_UNDEFINED)? diamond: no_diamond);
    XtAddCallback(recfm_options[3], XtNcallback, recfm_callback,
	    (XtPointer)&r_undefined);
    dialog_register_sensitivity(recfm_options[3],
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    NULL, false);

    lrecl_label = XtVaCreateManagedWidget(
	    "lrecl", labelWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[3],
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(lrecl_label,
	    &xftp.receive_flag, false,
	    &recfm_default, false,
	    &host_is_tso_or_vm, true);
    lrecl_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[3],
	    XtNvertDistance, 3,
	    XtNfromHoriz, lrecl_label,
	    XtNhorizDistance, MARGIN,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    if (xftp.lrecl && xftp.host_type != HT_CICS) {
	char *lr = lazyaf("%d", xftp.lrecl);

	XtVaSetValues(lrecl_widget, XtNstring, lr, NULL);
	XawTextSetInsertionPoint(lrecl_widget, strlen(lr));
    }
    dialog_match_dimension(lrecl_label, lrecl_widget, XtNheight);
    w = XawTextGetSource(lrecl_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(lrecl_widget,
	    &xftp.receive_flag, false,
	    &recfm_default, false,
	    &host_is_tso_or_vm, true);

    blksize_label = XtVaCreateManagedWidget(
	    "blksize", labelWidgetClass, ft_dialog,
	    XtNfromVert, lrecl_widget,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_match_dimension(blksize_label, lrecl_label, XtNwidth);
    dialog_register_sensitivity(blksize_label,
	    &xftp.receive_flag, false,
	    &recfm_default, false,
	    &host_is_tso, true);
    blksize_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, lrecl_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, blksize_label,
	    XtNhorizDistance, MARGIN,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    if (xftp.blksize && xftp.host_type != HT_CICS) {
	char *bs = lazyaf("%d", xftp.blksize);

	XtVaSetValues(blksize_widget, XtNstring, bs, NULL);
	XawTextSetInsertionPoint(blksize_widget, strlen(bs));
    }
    dialog_match_dimension(blksize_label, blksize_widget, XtNheight);
    w = XawTextGetSource(blksize_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(blksize_widget,
	    &xftp.receive_flag, false,
	    &recfm_default, false,
	    &host_is_tso, true);

    /* Find the widest widget in the left column. */
#if 0
    XtVaGetValues(send_toggle, XtNwidth, &maxw, NULL);
    h_ref = send_toggle;
#define REMAX(w) { \
	XtVaGetValues((w), XtNwidth, &d1, NULL); \
	if (d1 > maxw) { \
	    maxw = d1; \
	    h_ref = (w); \
	} \
    }
    REMAX(receive_toggle);
    REMAX(ascii_toggle);
    REMAX(binary_toggle);
    REMAX(append_widget);
#undef REMAX
#endif
    h_ref = blksize_widget;

    /* Create the right column buttons. */

    /* Create VM/TSO/CICS toggles. */
    vm_toggle = XtVaCreateManagedWidget(
	    "vm", commandWidgetClass, ft_dialog,
	    XtNfromVert, host_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(vm_toggle,
	    (xftp.host_type == HT_VM)? diamond: no_diamond);
    XtAddCallback(vm_toggle, XtNcallback, toggle_host_type,
	    (XtPointer)&s_vm);
    tso_toggle =  XtVaCreateManagedWidget(
	    "tso", commandWidgetClass, ft_dialog,
	    XtNfromVert, vm_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(tso_toggle,
	    (xftp.host_type == HT_TSO)? diamond : no_diamond);
    XtAddCallback(tso_toggle, XtNcallback, toggle_host_type,
	    (XtPointer)&s_tso);
    cics_toggle =  XtVaCreateManagedWidget(
	    "cics", commandWidgetClass, ft_dialog,
	    XtNfromVert, tso_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(cics_toggle,
	    (xftp.host_type == HT_CICS)? diamond : no_diamond);
    XtAddCallback(cics_toggle, XtNcallback, toggle_host_type,
	    (XtPointer)&s_cics);

    /* Create CR toggle. */
    cr_widget = XtVaCreateManagedWidget(
	    "cr", commandWidgetClass, ft_dialog,
	    XtNfromVert, cics_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(cr_widget,
	    xftp.ascii_flag && xftp.cr_flag? dot: no_dot);
    XtAddCallback(cr_widget, XtNcallback, toggle_cr, 0);
    dialog_register_sensitivity(cr_widget,
	    &xftp.ascii_flag, true,
	    NULL, false,
	    NULL, false);

    /* Create remap toggle. */
    remap_widget = XtVaCreateManagedWidget(
	    "remap", commandWidgetClass, ft_dialog,
	    XtNfromVert, cr_widget,
	    XtNfromHoriz, h_ref,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(remap_widget,
	    xftp.ascii_flag && xftp.remap_flag? dot: no_dot);
    XtAddCallback(remap_widget, XtNcallback, toggle_remap, NULL);
    dialog_register_sensitivity(remap_widget,
	    &xftp.ascii_flag, true,
	    NULL, false,
	    NULL, false);

    /* Set up the Units group. */
    units_label = XtVaCreateManagedWidget(
	    "units", labelWidgetClass, ft_dialog,
	    XtNfromVert, append_widget,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(units_label,
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    NULL, false);

    units_options[0] = XtVaCreateManagedWidget(
	    "spaceDefault", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[0],
	    (xftp.units == DEFAULT_UNITS)? diamond: no_diamond);
    XtAddCallback(units_options[0], XtNcallback,
	    units_callback, (XtPointer)&u_default_units);
    dialog_register_sensitivity(units_options[0],
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    NULL, false);

    units_options[1] = XtVaCreateManagedWidget(
	    "tracks", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[0],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[1],
	    (xftp.units == TRACKS)? diamond: no_diamond);
    XtAddCallback(units_options[1], XtNcallback,
	    units_callback, (XtPointer)&u_tracks);
    dialog_register_sensitivity(units_options[1],
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    NULL, false);

    units_options[2] = XtVaCreateManagedWidget(
	    "cylinders", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[1],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[2],
	    (xftp.units == CYLINDERS)? diamond: no_diamond);
    XtAddCallback(units_options[2], XtNcallback,
	    units_callback, (XtPointer)&u_cylinders);
    dialog_register_sensitivity(units_options[2],
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    NULL, false);

    units_options[3] = XtVaCreateManagedWidget(
	    "avblock", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[2],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[3],
	    (xftp.units == AVBLOCK)? diamond: no_diamond);
    XtAddCallback(units_options[3], XtNcallback,
	    units_callback, (XtPointer)&u_avblock);
    dialog_register_sensitivity(units_options[3],
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    NULL, false);

    primspace_label = XtVaCreateManagedWidget(
	    "primspace", labelWidgetClass, ft_dialog,
	    XtNfromVert, units_options[3],
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(primspace_label,
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    &units_default, false);
    primspace_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, units_options[3],
	    XtNvertDistance, 3,
	    XtNfromHoriz, primspace_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    if (xftp.primary_space) {
	s = xs_buffer("%d", xftp.primary_space);
	XtVaSetValues(primspace_widget, XtNstring, s, NULL);
	XawTextSetInsertionPoint(primspace_widget, strlen(s));
	XtFree(s);
    }
    dialog_match_dimension(primspace_label, primspace_widget, XtNheight);
    w = XawTextGetSource(primspace_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(primspace_widget,
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    &units_default, false);

    secspace_label = XtVaCreateManagedWidget(
	    "secspace", labelWidgetClass, ft_dialog,
	    XtNfromVert, primspace_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_match_dimension(primspace_label, secspace_label, XtNwidth);
    dialog_register_sensitivity(secspace_label,
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    &units_default, false);
    secspace_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, primspace_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, secspace_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    if (xftp.secondary_space) {
	s = xs_buffer("%d", xftp.secondary_space);
	XtVaSetValues(secspace_widget, XtNstring, s, NULL);
	XawTextSetInsertionPoint(secspace_widget, strlen(s));
	XtFree(s);
    }
    dialog_match_dimension(secspace_label, secspace_widget, XtNheight);
    w = XawTextGetSource(secspace_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(secspace_widget,
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    &units_default, false);

    avblock_size_label = XtVaCreateManagedWidget(
	    "avblockSize", labelWidgetClass, ft_dialog,
	    XtNfromVert, secspace_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_match_dimension(secspace_label, avblock_size_label, XtNwidth);
    dialog_register_sensitivity(avblock_size_label,
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    &units_avblock, true);
    avblock_size_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, secspace_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, avblock_size_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    if (xftp.avblock) {
	s = xs_buffer("%d", xftp.avblock);
	XtVaSetValues(avblock_size_widget, XtNstring, s, NULL);
	XawTextSetInsertionPoint(avblock_size_widget, strlen(s));
	XtFree(s);
    }
    dialog_match_dimension(avblock_size_label, avblock_size_widget, XtNheight);
    w = XawTextGetSource(avblock_size_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(avblock_size_widget,
	    &xftp.receive_flag, false,
	    &host_is_tso, true,
	    &units_avblock, true);

    /* Set up the DFT buffer size. */
    buffersize_label = XtVaCreateManagedWidget(
	    "buffersize", labelWidgetClass, ft_dialog,
	    XtNfromVert, blksize_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    buffersize_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, blksize_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, buffersize_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    dialog_match_dimension(buffersize_label, buffersize_widget, XtNheight);
    w = XawTextGetSource(buffersize_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(buffersize_widget,
	    NULL, false,
	    NULL, false,
	    NULL, false);
    set_dft_buffersize();
    s = xs_buffer("%d", dft_buffersize);
    XtVaSetValues(buffersize_widget, XtNstring, s, NULL);
    XawTextSetInsertionPoint(buffersize_widget, strlen(s));
    XtFree(s);

    /* Set up the buttons at the bottom. */
    start_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, ft_dialog,
	    XtNfromVert, buffersize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(start_button, XtNcallback, ft_start_callback, NULL);

    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, ft_dialog,
	    XtNfromVert, buffersize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, start_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, ft_cancel, 0);
}

/* Callbacks for all the transfer widgets. */

/* Transfer pop-up popping up. */
static void
ft_popup_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Set the focus to the local file widget. */
    PA_dialog_focus_xaction(local_file, NULL, NULL, NULL);

    /* Disallow overwrites. */
    interactive_overwrite = false;
}

/* Cancel button pushed. */
static void
ft_cancel(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(ft_shell);
}

/* recfm options. */
static void
recfm_callback(Widget w, XtPointer user_data, XtPointer call_data _is_unused)
{
    xftp.recfm = *(recfm_t *)user_data;
    recfm_default = (xftp.recfm == DEFAULT_RECFM);
    dialog_check_sensitivity(&recfm_default);
    dialog_flip_toggles(&recfm_toggles, w);
}

/* Units options. */
static void
units_callback(Widget w, XtPointer user_data, XtPointer call_data _is_unused)
{
    xftp.units = *(units_t *)user_data;
    units_default = (xftp.units == DEFAULT_UNITS);
    units_avblock = (xftp.units == AVBLOCK);
    dialog_check_sensitivity(&units_default);
    dialog_check_sensitivity(&units_avblock);
    dialog_flip_toggles(&units_toggles, w);
}

/* OK button pushed. */
static void
ft_start_callback(Widget w _is_unused, XtPointer call_parms _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(ft_shell);

    if (ft_start()) {
	popup_progress();
    }
}

/* Send/receive options. */
static void
toggle_receive(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag */
    xftp.receive_flag = *(bool *)client_data;

    /* Change the widget states. */
    dialog_mark_toggle(receive_toggle, xftp.receive_flag? diamond:
								no_diamond);
    dialog_mark_toggle(send_toggle, xftp.receive_flag? no_diamond:
							     diamond);
    dialog_check_sensitivity(&xftp.receive_flag);
}

/* Ascii/binary options. */
static void
toggle_ascii(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag. */
    xftp.ascii_flag = *(bool *)client_data;

    /* Change the widget states. */
    dialog_mark_toggle(ascii_toggle,
	    xftp.ascii_flag? diamond: no_diamond);
    dialog_mark_toggle(binary_toggle,
	    xftp.ascii_flag? no_diamond: diamond);
    xftp.cr_flag = xftp.ascii_flag;
    xftp.remap_flag = xftp.ascii_flag;
    dialog_mark_toggle(cr_widget, xftp.cr_flag? dot: no_dot);
    dialog_mark_toggle(remap_widget, xftp.remap_flag? dot: no_dot);
    dialog_check_sensitivity(&xftp.ascii_flag);
}

/* CR option. */
static void
toggle_cr(Widget w, XtPointer client_data _is_unused, XtPointer call_data _is_unused)
{
    /* Toggle the cr flag */
    xftp.cr_flag = !xftp.cr_flag;

    dialog_mark_toggle(w, xftp.cr_flag? dot: no_dot);
}

/* Append option. */
static void
toggle_append(Widget w, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Toggle Append Flag */
    xftp.append_flag = !xftp.append_flag;

    dialog_mark_toggle(w, xftp.append_flag? dot: no_dot);
}

/* Remap option. */
static void
toggle_remap(Widget w, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Toggle Remap Flag */
    xftp.remap_flag = !xftp.remap_flag;

    dialog_mark_toggle(w, xftp.remap_flag? dot: no_dot);
}

/*
 * Set the individual bool variables used by the dialog sensitivity
 * functions, and call dialog_check_sensitivity().
 */
static void
set_host_type_booleans(void)
{
    switch (xftp.host_type) {
    case HT_TSO:
	host_is_tso = true;
	host_is_tso_or_vm = true;
	break;
    case HT_VM:
	host_is_tso = false;
	host_is_tso_or_vm = true;
	break;
    case HT_CICS:
	host_is_tso = false;
	host_is_tso_or_vm = false;
    }

    dialog_check_sensitivity(&host_is_tso);
    dialog_check_sensitivity(&host_is_tso_or_vm);
}

/* TSO/VM/CICS option. */
static void
toggle_host_type(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    host_type_t old_host_type;

    /* Toggle the flag. */
    old_host_type = xftp.host_type;
    xftp.host_type = *(host_type_t *)client_data;
    if (xftp.host_type == old_host_type) {
	return;
    }

    /* Change the widget states. */
    dialog_mark_toggle(vm_toggle,
	    (xftp.host_type == HT_VM)? diamond: no_diamond);
    dialog_mark_toggle(tso_toggle,
	    (xftp.host_type == HT_TSO)? diamond: no_diamond);
    dialog_mark_toggle(cics_toggle,
	    (xftp.host_type == HT_CICS)? diamond: no_diamond);

    if (xftp.host_type != HT_TSO) {
	/* Reset record format. */
	if ((xftp.host_type == HT_VM &&
	     xftp.recfm == RECFM_UNDEFINED) ||
	    (xftp.host_type == HT_CICS &&
	     xftp.recfm != DEFAULT_RECFM)) {
	    xftp.recfm = DEFAULT_RECFM;
	    recfm_default = true;
	    dialog_flip_toggles(&recfm_toggles, recfm_toggles.widgets[0]);
	}
	/* Reset units. */
	if (xftp.units != DEFAULT_UNITS) {
	    xftp.units = DEFAULT_UNITS;
	    units_default = true;
	    units_avblock = false;
	    dialog_flip_toggles(&units_toggles, units_toggles.widgets[0]);
	}
	if (xftp.host_type == HT_CICS) {
	    /* Reset logical record size. */
	    XtVaSetValues(lrecl_widget, XtNstring, "", NULL);
	}
	/* Reset block size, primary space and secondary space. */
	XtVaSetValues(blksize_widget, XtNstring, "", NULL);
	XtVaSetValues(primspace_widget, XtNstring, "", NULL);
	XtVaSetValues(secspace_widget, XtNstring, "", NULL);
    }

    set_host_type_booleans();
}

/*
 * Get a numerical value from a string widget.
 *
 * @param[in] w		Widget to interrogate
 *
 * @return Numerical value of widget contents.
 */
static int
get_widget_n(Widget w)
{
    String s;

    XtVaGetValues(w, XtNstring, &s, NULL);
    if (strlen(s) > 0) {
	return atoi(s);
    } else {
	return 0;
    }
}


/**
 * Begin the transfer.
 *
 * @return true if the transfer has started, false otherwise
 */
static bool
ft_start(void)
{
    ft_dbcs_state = FT_DBCS_NONE;

    /*
     * Get the DFT buffer size, and update the widget with the default if they
     * entered nothing (or an explicit 0).
     */
    dft_buffersize = get_widget_n(buffersize_widget);
    set_dft_buffersize();
    XtVaSetValues(buffersize_widget, XtNstring, lazyaf("%d", dft_buffersize),
	    NULL);

    /* Get the host file from its widget */
    XtVaGetValues(host_file, XtNstring, &xftp.host_filename, NULL);
    if (!*xftp.host_filename) {
	return false;
    }

    /* Get the local file from its widget */
    XtVaGetValues(local_file, XtNstring,  &xftp.local_filename, NULL);
    if (!*xftp.local_filename) {
	return false;
    }

    /* Fetch the rest of the numeric parameters. */
    xftp.lrecl = get_widget_n(lrecl_widget);
    xftp.blksize = get_widget_n(blksize_widget);
    xftp.primary_space = get_widget_n(primspace_widget);
    xftp.secondary_space = get_widget_n(secspace_widget);
    xftp.avblock = get_widget_n(avblock_size_widget);

    /* Check for primary space. */
    if (xftp.host_type == HT_TSO &&
	xftp.units != DEFAULT_UNITS &&
	xftp.primary_space <= 0) {

	popup_an_error("Missing or invalid Primary Space");
	return false;
    }

    /* Prompt for local file overwrite. */
    if (xftp.receive_flag && !xftp.append_flag &&
	    !(xftp.allow_overwrite || interactive_overwrite)) {
	ft_local_file = fopen(xftp.local_filename,
		xftp.ascii_flag? "r": "rb");
	if (ft_local_file != NULL) {
	    (void) fclose(ft_local_file);
	    ft_local_file = NULL;
	    popup_overwrite();
	    return false;
	}
    }

    /* Start the transfer. */
    ft_local_file = ft_go(&xftp);
    if (ft_local_file == NULL) {
	interactive_overwrite = false;
	return false;
    }

    /* Get this thing started. */
    interactive_overwrite = false;
    xftp.is_cut = false;
    ft_last_cr = false;
    ft_last_dbcs = false;

    ft_state = FT_AWAIT_ACK;

    return true;
}

/* "Transfer in Progress" pop-up. */

/* Pop up the "in progress" pop-up. */
static void
popup_progress(void)
{
    /* Initialize it. */
    if (progress_shell == NULL) {
	progress_popup_init();
    }

    /* Pop it up. */
    popup_popup(progress_shell, XtGrabNone);
}

/* Initialize the "in progress" pop-up. */
static void
progress_popup_init(void)
{
    Widget progress_pop, from_label, to_label, cancel_button;

    /* Create the shell. */
    progress_shell = XtVaCreatePopupShell(
	    "ftProgressPopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(progress_shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
    XtAddCallback(progress_shell, XtNpopupCallback,
	    progress_popup_callback, NULL);

    /* Create a form structure to contain the other stuff */
    progress_pop = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, progress_shell,
	    NULL);

    /* Create the widgets. */
    from_label = XtVaCreateManagedWidget(
	    "fromLabel", labelWidgetClass, progress_pop,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    from_file = XtVaCreateManagedWidget(
	    "filename", labelWidgetClass, progress_pop,
	    XtNwidth, FILE_WIDTH,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, from_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(from_label, from_file, XtNheight);

    to_label = XtVaCreateManagedWidget(
	    "toLabel", labelWidgetClass, progress_pop,
	    XtNfromVert, from_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    to_file = XtVaCreateManagedWidget(
	    "filename", labelWidgetClass, progress_pop,
	    XtNwidth, FILE_WIDTH,
	    XtNfromVert, from_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, to_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(to_label, to_file, XtNheight);

    dialog_match_dimension(from_label, to_label, XtNwidth);

    waiting = XtVaCreateManagedWidget(
	    "waiting", labelWidgetClass, progress_pop,
	    XtNfromVert, to_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNmappedWhenManaged, False,
	    NULL);

    ft_status = XtVaCreateManagedWidget(
	    "status", labelWidgetClass, progress_pop,
	    XtNfromVert, to_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNresizable, True,
	    XtNmappedWhenManaged, False,
	    NULL);
    XtVaGetValues(ft_status, XtNlabel, &status_string, NULL);
    status_string = XtNewString(status_string);

    aborting = XtVaCreateManagedWidget(
	    "aborting", labelWidgetClass, progress_pop,
	    XtNfromVert, to_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNmappedWhenManaged, False,
	    NULL);

    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, progress_pop,
	    XtNfromVert, ft_status,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, progress_cancel_callback, NULL);
}

/* Callbacks for the "in progress" pop-up. */

/* In-progress pop-up popped up. */
static void
progress_popup_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtVaSetValues(from_file, XtNlabel,
	    xftp.receive_flag? xftp.host_filename:
				     xftp.local_filename,
	    NULL);
    XtVaSetValues(to_file, XtNlabel,
	    xftp.receive_flag? xftp.local_filename:
				     xftp.host_filename,
	    NULL);

    switch (ft_state) {
    case FT_AWAIT_ACK:
	XtUnmapWidget(ft_status);
	XtUnmapWidget(aborting);
	XtMapWidget(waiting);
	break;
    case FT_RUNNING:
	XtUnmapWidget(waiting);
	XtUnmapWidget(aborting);
	XtMapWidget(ft_status);
	break;
    case FT_ABORT_WAIT:
    case FT_ABORT_SENT:
	XtUnmapWidget(waiting);
	XtUnmapWidget(ft_status);
	XtMapWidget(aborting);
	break;
    default:
	break;
    }
}

/* In-progress "cancel" button. */
static void
progress_cancel_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (ft_state == FT_RUNNING) {
	ft_state = FT_ABORT_WAIT;
	XtUnmapWidget(waiting);
	XtUnmapWidget(ft_status);
	XtMapWidget(aborting);
    } else {
	/* Impatient user or hung host -- just clean up. */
	ft_complete(get_message("ftUserCancel"));
    }
}

/* "Overwrite existing?" pop-up. */

/* Pop up the "overwrite" pop-up. */
static void
popup_overwrite(void)
{
    /* Initialize it. */
    if (overwrite_shell == NULL) {
	overwrite_popup_init();
    }

    /* Pop it up. */
    popup_popup(overwrite_shell, XtGrabExclusive);
}

/* Initialize the "overwrite" pop-up. */
static void
overwrite_popup_init(void)
{
    Widget overwrite_pop, overwrite_name, okay_button, cancel_button;
    String overwrite_string, label, lf;
    Dimension d;

    /* Create the shell. */
    overwrite_shell = XtVaCreatePopupShell(
	    "ftOverwritePopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(overwrite_shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
    XtAddCallback(overwrite_shell, XtNpopdownCallback, overwrite_popdown,
	    NULL);

    /* Create a form structure to contain the other stuff */
    overwrite_pop = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, overwrite_shell,
	    NULL);

    /* Create the widgets. */
    overwrite_name = XtVaCreateManagedWidget(
	    "overwriteName", labelWidgetClass, overwrite_pop,
	    XtNvertDistance, MARGIN,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNresizable, True,
	    NULL);
    XtVaGetValues(overwrite_name, XtNlabel, &overwrite_string, NULL);
    XtVaGetValues(local_file, XtNstring, &lf, NULL);
    label = xs_buffer(overwrite_string, lf);
    XtVaSetValues(overwrite_name, XtNlabel, label, NULL);
    XtFree(label);
    XtVaGetValues(overwrite_name, XtNwidth, &d, NULL);
    if ((Dimension)(d + 20) < 400) {
	d = 400;
    } else {
	d += 20;
    }
    XtVaSetValues(overwrite_name, XtNwidth, d, NULL);
    XtVaGetValues(overwrite_name, XtNheight, &d, NULL);
    XtVaSetValues(overwrite_name, XtNheight, d + 20, NULL);

    okay_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, overwrite_pop,
	    XtNfromVert, overwrite_name,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(okay_button, XtNcallback, overwrite_okay_callback, NULL);

    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, overwrite_pop,
	    XtNfromVert, overwrite_name,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, okay_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, overwrite_cancel_callback, NULL);
}

/* Overwrite "okay" button. */
static void
overwrite_okay_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(overwrite_shell);

    interactive_overwrite = true;
    if (ft_start()) {
	XtPopdown(ft_shell);
	popup_progress();
    }
}

/* Overwrite "cancel" button. */
static void
overwrite_cancel_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(overwrite_shell);
}

/* Overwrite pop-up popped down. */
static void
overwrite_popdown(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtDestroyWidget(overwrite_shell);
    overwrite_shell = NULL;
}

/* Entry points called from the common FT logic. */

/* Pop down the transfer-in-progress pop-up. */
void
ft_gui_progress_popdown(void)
{
    if (!ft_private->is_action) {
	XtPopdown(progress_shell);
    }
}

/* Massage a file transfer error message so it will fit in the pop-up. */
#define MAX_MSGLEN 50
void
ft_gui_errmsg_prepare(char *msg)
{
    if (strlen(msg) > MAX_MSGLEN && strchr(msg, '\n') == NULL) {
	char *s = msg + MAX_MSGLEN;
	while (s > msg && *s != ' ') {
	    s--;
	}
	if (s > msg) {
	    *s = '\n';      /* yikes! */
	}
    }
}

/* Clear out the progress display. */
void
ft_gui_clear_progress(void)
{
}

/* Pop up a successful completion message. */
void
ft_gui_complete_popup(const char *msg)
{
    popup_an_info("%s", msg);
}

/* Update the bytes-transferred count on the progress pop-up. */
void
ft_gui_update_length(unsigned long length)
{
    if (!ft_private->is_action) {
	char *s = xs_buffer(status_string, length);

	XtVaSetValues(ft_status, XtNlabel, s, NULL);
	XtFree(s);
    }
}

/* Replace the 'waiting' pop-up with the 'in-progress' pop-up. */
void
ft_gui_running(unsigned long length)
{
    if (!ft_private->is_action) {
	XtUnmapWidget(waiting);
	ft_gui_update_length(length);
	XtMapWidget(ft_status);
    }
}

/* Process a protocol-generated abort. */
void
ft_gui_aborting(void)
{
    if (!ft_private->is_action) {
	XtUnmapWidget(waiting);
	XtUnmapWidget(ft_status);
	XtMapWidget(aborting);
    }
}

/* Check for interactive mode. */
ft_gui_interact_t
ft_gui_interact(ft_private_t *p)
{
    return FGI_NOP;
}

/* Display an "Awaiting start of transfer" message. */
void
ft_gui_awaiting(void)
{
}
