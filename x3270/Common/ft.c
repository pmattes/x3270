/*
 * Copyright (c) 1996-2009, Paul Mattes.
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
 *	ft.c
 *		This module handles the file transfer dialogs.
 */

#include "globals.h"

#if defined(X3270_FT) /*[*/

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
#include "actionsc.h"
#include "charsetc.h"
#include "ft_cutc.h"
#include "ft_dftc.h"
#include "ftc.h"
#include "dialogc.h"
#include "hostc.h"
#if defined(C3270) || defined(WC3270) /*[*/
#include "icmdc.h"
#endif /*]*/
#include "kybdc.h"
#include "macrosc.h"
#include "objects.h"
#include "popupsc.h"
#include "screenc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "utilc.h"
#if defined(_MSC_VER) /*[*/
#include "Msc/deprecated.h"
#endif /*]*/

/* Macros. */
#define eos(s)	strchr((s), '\0')

#if defined(X3270_DISPLAY) /*[*/
#define FILE_WIDTH	300	/* width of file name widgets */
#define MARGIN		3	/* distance from margins to widgets */
#define CLOSE_VGAP	0	/* distance between paired toggles */
#define FAR_VGAP	10	/* distance between single toggles and groups */
#define BUTTON_GAP	5	/* horizontal distance between buttons */
#define COLUMN_GAP	40	/* distance between columns */
#endif /*]*/

#define BN	(Boolean *)NULL

/* Externals. */
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
extern Pixmap diamond;
extern Pixmap no_diamond;
extern Pixmap null;
extern Pixmap dot;
extern Pixmap no_dot;
#endif /*]*/

/* Globals. */
enum ft_state ft_state = FT_NONE;	/* File transfer state */
char *ft_local_filename;		/* Local file to transfer to/from */
FILE *ft_local_file = (FILE *)NULL;	/* File descriptor for local file */
Boolean ft_last_cr = False;		/* CR was last char in local file */
Boolean ascii_flag = True;		/* Convert to ascii */
Boolean cr_flag = True;			/* Add crlf to each line */
Boolean remap_flag = True;		/* Remap ASCII<->EBCDIC */
unsigned long ft_length = 0;		/* Length of transfer */

/* Statics. */
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static Widget ft_dialog, ft_shell, local_file, host_file;
static Widget lrecl_widget, blksize_widget;
static Widget primspace_widget, secspace_widget;
static Widget send_toggle, receive_toggle;
static Widget vm_toggle, tso_toggle;
static Widget ascii_toggle, binary_toggle;
static Widget cr_widget;
static Widget remap_widget;
static Widget buffersize_widget;
#endif /*]*/

static char *ft_host_filename;		/* Host file to transfer to/from */
static Boolean receive_flag = True;	/* Current transfer is receive */
static Boolean append_flag = False;	/* Append transfer */
static Boolean vm_flag = False;		/* VM Transfer flag */

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static Widget recfm_options[5];
static Widget units_options[5];
static struct toggle_list recfm_toggles = { recfm_options };
static struct toggle_list units_toggles = { units_options };
#endif /*]*/

static enum recfm {
	DEFAULT_RECFM, RECFM_FIXED, RECFM_VARIABLE, RECFM_UNDEFINED
} recfm = DEFAULT_RECFM;
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static Boolean recfm_default = True;
static enum recfm r_default_recfm = DEFAULT_RECFM;
static enum recfm r_fixed = RECFM_FIXED;
static enum recfm r_variable = RECFM_VARIABLE;
static enum recfm r_undefined = RECFM_UNDEFINED;
#endif /*]*/

static enum units {
	DEFAULT_UNITS, TRACKS, CYLINDERS, AVBLOCK
} units = DEFAULT_UNITS;
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static Boolean units_default = True;
static enum units u_default_units = DEFAULT_UNITS;
static enum units u_tracks = TRACKS;
static enum units u_cylinders = CYLINDERS;
static enum units u_avblock = AVBLOCK;
#endif /*]*/

static Boolean allow_overwrite = False;
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static sr_t *ft_sr = (sr_t *)NULL;

static Widget progress_shell, from_file, to_file;
static Widget ft_status, waiting, aborting;
static String status_string;
#endif /*]*/
static struct timeval t0;		/* Starting time */
static Boolean ft_is_cut;		/* File transfer is CUT-style */

/* Translation table: "ASCII" to EBCDIC, as seen by IND$FILE. */
unsigned char i_asc2ft[256] = {
0x00,0x01,0x02,0x03,0x37,0x2d,0x2e,0x2f,0x16,0x05,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x3c,0x3d,0x32,0x26,0x18,0x19,0x3f,0x27,0x1c,0x1d,0x1e,0x1f,
0x40,0x5a,0x7f,0x7b,0x5b,0x6c,0x50,0x7d,0x4d,0x5d,0x5c,0x4e,0x6b,0x60,0x4b,0x61,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0x7a,0x5e,0x4c,0x7e,0x6e,0x6f,
0x7c,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,
0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0x4a,0xe0,0x4f,0x5f,0x6d,
0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
0x97,0x98,0x99,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xc0,0x6a,0xd0,0xa1,0x07,
0x20,0x21,0x22,0x23,0x24,0x15,0x06,0x17,0x28,0x29,0x2a,0x2b,0x2c,0x09,0x0a,0x1b,
0x30,0x31,0x1a,0x33,0x34,0x35,0x36,0x08,0x38,0x39,0x3a,0x3b,0x04,0x14,0x3e,0xe1,
0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
0x58,0x59,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x70,0x71,0x72,0x73,0x74,0x75,
0x76,0x77,0x78,0x80,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,0x90,0x9a,0x9b,0x9c,0x9d,0x9e,
0x9f,0xa0,0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xca,0xcb,0xcc,0xcd,0xce,0xcf,0xda,0xdb,
0xdc,0xdd,0xde,0xdf,0xea,0xeb,0xec,0xed,0xee,0xef,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};

/* Translation table: EBCDIC to "ASCII", as seen by IND$FILE. */
unsigned char i_ft2asc[256] = {
0x00,0x01,0x02,0x03,0x9c,0x09,0x86,0x7f,0x97,0x8d,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x9d,0x85,0x08,0x87,0x18,0x19,0x92,0x8f,0x1c,0x1d,0x1e,0x1f,
0x80,0x81,0x82,0x83,0x84,0x00,0x17,0x1b,0x88,0x89,0x8a,0x8b,0x8c,0x05,0x06,0x07,
0x90,0x91,0x16,0x93,0x94,0x95,0x96,0x04,0x98,0x99,0x9a,0x9b,0x14,0x15,0x9e,0x1a,
0x20,0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0x5b,0x2e,0x3c,0x28,0x2b,0x5d,
0x26,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0,0xb1,0x21,0x24,0x2a,0x29,0x3b,0x5e,
0x2d,0x2f,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0x7c,0x2c,0x25,0x5f,0x3e,0x3f,
0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xc0,0xc1,0xc2,0x60,0x3a,0x23,0x40,0x27,0x3d,0x22,
0xc3,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,
0xca,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,0x71,0x72,0xcb,0xcc,0xcd,0xce,0xcf,0xd0,
0xd1,0x7e,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
0x7b,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0xe8,0xe9,0xea,0xeb,0xec,0xed,
0x7d,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0xee,0xef,0xf0,0xf1,0xf2,0xf3,
0x5c,0x9f,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};

#if defined(X3270_DBCS) /*[*/
enum ftd ft_dbcs_state = FT_DBCS_NONE;
unsigned char ft_dbcs_byte1;
Boolean ft_last_dbcs = False;
#endif /*]*/

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static Widget overwrite_shell;
#endif /*]*/
static Boolean ft_is_action;
static unsigned long ft_start_id = 0;

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static void ft_cancel(Widget w, XtPointer client_data, XtPointer call_data);
static void ft_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void ft_popup_init(void);
static int ft_start(void);
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
static void toggle_vm(Widget w, XtPointer client_data, XtPointer call_data);
static void units_callback(Widget w, XtPointer user_data, XtPointer call_data);
#endif /*]*/
static void ft_connected(Boolean ignored);
static void ft_in3270(Boolean ignored);

/* Main external entry point. */

#if !defined(X3270_DISPLAY) || !defined(X3270_MENUS) /*[*/
void
ft_init(void)
{
	/* Register for state changes. */
	register_schange(ST_CONNECT, ft_connected);
	register_schange(ST_3270_MODE, ft_in3270);
}
#endif /*]*/

/* Return the right value for fopen()ing the local file. */
static char *
local_fflag(void)
{
	static char ret[3];
	int nr = 0;

	ret[nr++] = receive_flag? (append_flag? 'a': 'w' ): 'r';
#if defined(_WIN32) /*[*/
	if (!ascii_flag)
		ret[nr++] = 'b';
#endif /*]*/
	ret[nr] = '\0';
	return ret;
}

/* Timeout function for stalled transfers. */
static void
ft_didnt_start(void)
{
	if (ft_local_file != NULL) {
		fclose(ft_local_file);
		ft_local_file = NULL;
		if (receive_flag && !append_flag)
		    unlink(ft_local_filename);
	}
	allow_overwrite = False;

	ft_complete(get_message("ftStartTimeout"));
	sms_continue();
}

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
/* "File Transfer" dialog. */

/*
 * Pop up the "Transfer" menu.
 * Called back from the "File Transfer" option on the File menu.
 */
void
popup_ft(Widget w _is_unused, XtPointer call_parms _is_unused,
	XtPointer call_data _is_unused)
{
	/* Initialize it. */
	if (ft_shell == (Widget)NULL)
		ft_popup_init();

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
	Widget h_ref = (Widget)NULL;
	Dimension d1;
	Dimension maxw = 0;
	Widget recfm_label, units_label;
	Widget buffersize_label;
	Widget start_button;
	char buflen_buf[128];

	/* Register for state changes. */
	register_schange(ST_CONNECT, ft_connected);
	register_schange(ST_3270_MODE, ft_in3270);

	/* Prep the dialog functions. */
	dialog_set(&ft_sr, ft_dialog);

	/* Create the menu shell. */
	ft_shell = XtVaCreatePopupShell(
	    "ftPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(ft_shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
	XtAddCallback(ft_shell, XtNpopupCallback, ft_popup_callback,
	    (XtPointer)NULL);

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
	dialog_match_dimension(local_label, local_file, XtNheight);
	w = XawTextGetSource(local_file);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_unixfile);
	dialog_register_sensitivity(local_file,
	    BN, False,
	    BN, False,
	    BN, False);

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
	dialog_match_dimension(host_label, host_file, XtNheight);
	dialog_match_dimension(local_label, host_label, XtNwidth);
	w = XawTextGetSource(host_file);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_hostfile);
	dialog_register_sensitivity(host_file,
	    BN, False,
	    BN, False,
	    BN, False);

	/* Create the left column. */

	/* Create send/receive toggles. */
	send_toggle = XtVaCreateManagedWidget(
	    "send", commandWidgetClass, ft_dialog,
	    XtNfromVert, host_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(send_toggle, receive_flag ? no_diamond : diamond);
	XtAddCallback(send_toggle, XtNcallback, toggle_receive,
	    (XtPointer)&s_false);
	receive_toggle = XtVaCreateManagedWidget(
	    "receive", commandWidgetClass, ft_dialog,
	    XtNfromVert, send_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(receive_toggle, receive_flag ? diamond : no_diamond);
	XtAddCallback(receive_toggle, XtNcallback, toggle_receive,
	    (XtPointer)&s_true);

	/* Create ASCII/binary toggles. */
	ascii_toggle = XtVaCreateManagedWidget(
	    "ascii", commandWidgetClass, ft_dialog,
	    XtNfromVert, receive_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(ascii_toggle, ascii_flag ? diamond : no_diamond);
	XtAddCallback(ascii_toggle, XtNcallback, toggle_ascii,
	    (XtPointer)&s_true);
	binary_toggle = XtVaCreateManagedWidget(
	    "binary", commandWidgetClass, ft_dialog,
	    XtNfromVert, ascii_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(binary_toggle, ascii_flag ? no_diamond : diamond);
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
	dialog_apply_bitmap(append_widget, append_flag ? dot : no_dot);
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
	    &receive_flag, False,
	    BN, False,
	    BN, False);

	recfm_options[0] = XtVaCreateManagedWidget(
	    "recfmDefault", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(recfm_options[0],
	    (recfm == DEFAULT_RECFM) ? diamond : no_diamond);
	XtAddCallback(recfm_options[0], XtNcallback, recfm_callback,
	    (XtPointer)&r_default_recfm);
	dialog_register_sensitivity(recfm_options[0],
	    &receive_flag, False,
	    BN, False,
	    BN, False);

	recfm_options[1] = XtVaCreateManagedWidget(
	    "fixed", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[0],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(recfm_options[1],
	    (recfm == RECFM_FIXED) ? diamond : no_diamond);
	XtAddCallback(recfm_options[1], XtNcallback, recfm_callback,
	    (XtPointer)&r_fixed);
	dialog_register_sensitivity(recfm_options[1],
	    &receive_flag, False,
	    BN, False,
	    BN, False);

	recfm_options[2] = XtVaCreateManagedWidget(
	    "variable", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[1],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(recfm_options[2],
	    (recfm == RECFM_VARIABLE) ? diamond : no_diamond);
	XtAddCallback(recfm_options[2], XtNcallback, recfm_callback,
	    (XtPointer)&r_variable);
	dialog_register_sensitivity(recfm_options[2],
	    &receive_flag, False,
	    BN, False,
	    BN, False);

	recfm_options[3] = XtVaCreateManagedWidget(
	    "undefined", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[2],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(recfm_options[3],
	    (recfm == RECFM_UNDEFINED) ? diamond : no_diamond);
	XtAddCallback(recfm_options[3], XtNcallback, recfm_callback,
	    (XtPointer)&r_undefined);
	dialog_register_sensitivity(recfm_options[3],
	    &receive_flag, False,
	    &vm_flag, False,
	    BN, False);

	lrecl_label = XtVaCreateManagedWidget(
	    "lrecl", labelWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[3],
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_register_sensitivity(lrecl_label,
	    &receive_flag, False,
	    &recfm_default, False,
	    BN, False);
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
	dialog_match_dimension(lrecl_label, lrecl_widget, XtNheight);
	w = XawTextGetSource(lrecl_widget);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_numeric);
	dialog_register_sensitivity(lrecl_widget,
	    &receive_flag, False,
	    &recfm_default, False,
	    BN, False);

	blksize_label = XtVaCreateManagedWidget(
	    "blksize", labelWidgetClass, ft_dialog,
	    XtNfromVert, lrecl_widget,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_match_dimension(blksize_label, lrecl_label, XtNwidth);
	dialog_register_sensitivity(blksize_label,
	    &receive_flag, False,
	    &recfm_default, False,
	    BN, False);
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
	dialog_match_dimension(blksize_label, blksize_widget, XtNheight);
	w = XawTextGetSource(blksize_widget);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_numeric);
	dialog_register_sensitivity(blksize_widget,
	    &receive_flag, False,
	    &recfm_default, False,
	    BN, False);


	/* Find the widest widget in the left column. */
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

	/* Create the right column buttons. */

	/* Create VM/TSO toggle. */
	vm_toggle = XtVaCreateManagedWidget(
	    "vm", commandWidgetClass, ft_dialog,
	    XtNfromVert, host_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(vm_toggle, vm_flag ? diamond : no_diamond);
	XtAddCallback(vm_toggle, XtNcallback, toggle_vm, (XtPointer)&s_true);
	tso_toggle =  XtVaCreateManagedWidget(
	    "tso", commandWidgetClass, ft_dialog,
	    XtNfromVert, vm_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(tso_toggle, vm_flag ? no_diamond : diamond);
	XtAddCallback(tso_toggle, XtNcallback, toggle_vm, (XtPointer)&s_false);

	/* Create CR toggle. */
	cr_widget = XtVaCreateManagedWidget(
	    "cr", commandWidgetClass, ft_dialog,
	    XtNfromVert, tso_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(cr_widget, cr_flag ? dot : no_dot);
	XtAddCallback(cr_widget, XtNcallback, toggle_cr, 0);
	dialog_register_sensitivity(cr_widget,
	    BN, False,
	    BN, False,
	    BN, False);

	/* Create remap toggle. */
	remap_widget = XtVaCreateManagedWidget(
	    "remap", commandWidgetClass, ft_dialog,
	    XtNfromVert, cr_widget,
	    XtNfromHoriz, h_ref,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(remap_widget, remap_flag ? dot : no_dot);
	XtAddCallback(remap_widget, XtNcallback, toggle_remap, NULL);
	dialog_register_sensitivity(remap_widget,
	    &ascii_flag, True,
	    BN, False,
	    BN, False);

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
	    &receive_flag, False,
	    &vm_flag, False,
	    BN, False);

	units_options[0] = XtVaCreateManagedWidget(
	    "spaceDefault", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(units_options[0],
	    (units == DEFAULT_UNITS) ? diamond : no_diamond);
	XtAddCallback(units_options[0], XtNcallback,
	    units_callback, (XtPointer)&u_default_units);
	dialog_register_sensitivity(units_options[0],
	    &receive_flag, False,
	    &vm_flag, False,
	    BN, False);

	units_options[1] = XtVaCreateManagedWidget(
	    "tracks", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[0],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(units_options[1],
	    (units == TRACKS) ? diamond : no_diamond);
	XtAddCallback(units_options[1], XtNcallback,
	    units_callback, (XtPointer)&u_tracks);
	dialog_register_sensitivity(units_options[1],
	    &receive_flag, False,
	    &vm_flag, False,
	    BN, False);

	units_options[2] = XtVaCreateManagedWidget(
	    "cylinders", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[1],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(units_options[2],
	    (units == CYLINDERS) ? diamond : no_diamond);
	XtAddCallback(units_options[2], XtNcallback,
	    units_callback, (XtPointer)&u_cylinders);
	dialog_register_sensitivity(units_options[2],
	    &receive_flag, False,
	    &vm_flag, False,
	    BN, False);

	units_options[3] = XtVaCreateManagedWidget(
	    "avblock", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[2],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(units_options[3],
	    (units == AVBLOCK) ? diamond : no_diamond);
	XtAddCallback(units_options[3], XtNcallback,
	    units_callback, (XtPointer)&u_avblock);
	dialog_register_sensitivity(units_options[3],
	    &receive_flag, False,
	    &vm_flag, False,
	    BN, False);

	primspace_label = XtVaCreateManagedWidget(
	    "primspace", labelWidgetClass, ft_dialog,
	    XtNfromVert, units_options[3],
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
	dialog_register_sensitivity(primspace_label,
	    &receive_flag, False,
	    &vm_flag, False,
	    &units_default, False);
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
	dialog_match_dimension(primspace_label, primspace_widget, XtNheight);
	w = XawTextGetSource(primspace_widget);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_numeric);
	dialog_register_sensitivity(primspace_widget,
	    &receive_flag, False,
	    &vm_flag, False,
	    &units_default, False);

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
	    &receive_flag, False,
	    &vm_flag, False,
	    &units_default, False);
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
	dialog_match_dimension(secspace_label, secspace_widget, XtNheight);
	w = XawTextGetSource(secspace_widget);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_numeric);
	dialog_register_sensitivity(secspace_widget,
	    &receive_flag, False,
	    &vm_flag, False,
	    &units_default, False);

	/* Set up the DFT buffer size. */
	buffersize_label = XtVaCreateManagedWidget(
	    "buffersize", labelWidgetClass, ft_dialog,
	    XtNfromVert, blksize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	buffersize_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, blksize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, buffersize_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
	dialog_match_dimension(buffersize_label, buffersize_widget, XtNheight);
	w = XawTextGetSource(buffersize_widget);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_numeric);
	dialog_register_sensitivity(buffersize_widget,
	    BN, False,
	    BN, False,
	    BN, False);
	set_dft_buffersize();
	(void) sprintf(buflen_buf, "%d", dft_buffersize);
	XtVaSetValues(buffersize_widget, XtNstring, buflen_buf, NULL);

	/* Set up the buttons at the bottom. */
	start_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, ft_dialog,
	    XtNfromVert, buffersize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
	XtAddCallback(start_button, XtNcallback, ft_start_callback,
	    (XtPointer)NULL);

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
	PA_dialog_focus_action(local_file, (XEvent *)NULL, (String *)NULL,
	    (Cardinal *)NULL);

	/* Disallow overwrites. */
	allow_overwrite = False;
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
	recfm = *(enum recfm *)user_data;
	recfm_default = (recfm == DEFAULT_RECFM);
	dialog_check_sensitivity(&recfm_default);
	dialog_flip_toggles(&recfm_toggles, w);
}

/* Units options. */
static void
units_callback(Widget w, XtPointer user_data, XtPointer call_data _is_unused)
{
	units = *(enum units *)user_data;
	units_default = (units == DEFAULT_UNITS);
	dialog_check_sensitivity(&units_default);
	dialog_flip_toggles(&units_toggles, w);
}

/* OK button pushed. */
static void
ft_start_callback(Widget w _is_unused, XtPointer call_parms _is_unused,
	XtPointer call_data _is_unused)
{
	if (ft_start()) {
		XtPopdown(ft_shell);
		popup_progress();
	}
}

/* Send/receive options. */
static void
toggle_receive(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
	/* Toggle the flag */
	receive_flag = *(Boolean *)client_data;

	/* Change the widget states. */
	dialog_mark_toggle(receive_toggle, receive_flag ? diamond : no_diamond);
	dialog_mark_toggle(send_toggle, receive_flag ? no_diamond : diamond);
	dialog_check_sensitivity(&receive_flag);
}

/* Ascii/binary options. */
static void
toggle_ascii(Widget w _is_unused, XtPointer client_data, XtPointer call_data _is_unused)
{
	/* Toggle the flag. */
	ascii_flag = *(Boolean *)client_data;

	/* Change the widget states. */
	dialog_mark_toggle(ascii_toggle, ascii_flag ? diamond : no_diamond);
	dialog_mark_toggle(binary_toggle, ascii_flag ? no_diamond : diamond);
	cr_flag = ascii_flag;
	remap_flag = ascii_flag;
	dialog_mark_toggle(cr_widget, cr_flag ? dot : no_dot);
	dialog_mark_toggle(remap_widget, remap_flag ? dot : no_dot);
	dialog_check_sensitivity(&ascii_flag);
}

/* CR option. */
static void
toggle_cr(Widget w, XtPointer client_data _is_unused, XtPointer call_data _is_unused)
{
	/* Toggle the cr flag */
	cr_flag = !cr_flag;

	dialog_mark_toggle(w, cr_flag ? dot : no_dot);
}

/* Append option. */
static void
toggle_append(Widget w, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
	/* Toggle Append Flag */
	append_flag = !append_flag;

	dialog_mark_toggle(w, append_flag ? dot : no_dot);
}

/* Remap option. */
static void
toggle_remap(Widget w, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
	/* Toggle Remap Flag */
	remap_flag = !remap_flag;

	dialog_mark_toggle(w, remap_flag ? dot : no_dot);
}

/* TSO/VM option. */
static void
toggle_vm(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	/* Toggle the flag. */
	vm_flag = *(Boolean *)client_data;

	/* Change the widget states. */
	dialog_mark_toggle(vm_toggle, vm_flag ? diamond : no_diamond);
	dialog_mark_toggle(tso_toggle, vm_flag ? no_diamond : diamond);

	if (vm_flag) {
		if (recfm == RECFM_UNDEFINED) {
			recfm = DEFAULT_RECFM;
			recfm_default = True;
			dialog_flip_toggles(&recfm_toggles,
			    recfm_toggles.widgets[0]);
		}
	}
	dialog_check_sensitivity(&vm_flag);
}

/*
 * Begin the transfer.
 * Returns 1 if the transfer has started, 0 otherwise.
 */
static int
ft_start(void)
{
	char opts[80];
	char *op = opts + 1;
	char *cmd;
	String buffersize, lrecl, blksize, primspace, secspace;
	char updated_buffersize[128];
	unsigned flen;

	ft_is_action = False;

#if defined(X3270_DBCS) /*[*/
	ft_dbcs_state = FT_DBCS_NONE;
#endif /*]*/

	/* Get the DFT buffer size. */
	XtVaGetValues(buffersize_widget, XtNstring, &buffersize, NULL);
	if (*buffersize)
		dft_buffersize = atoi(buffersize);
	else
		dft_buffersize = 0;
	set_dft_buffersize();
	(void) sprintf(updated_buffersize, "%d", dft_buffersize);
	XtVaSetValues(buffersize_widget, XtNstring, updated_buffersize, NULL);

	/* Get the host file from its widget */
	XtVaGetValues(host_file, XtNstring, &ft_host_filename, NULL);
	if (!*ft_host_filename)
		return 0;
	/* XXX: probably more validation to do here */

	/* Get the local file from it widget */
	XtVaGetValues(local_file, XtNstring,  &ft_local_filename, NULL);
	if (!*ft_local_filename)
		return 0;

	/* See if the local file can be overwritten. */
	if (receive_flag && !append_flag && !allow_overwrite) {
		ft_local_file = fopen(ft_local_filename,
			ascii_flag? "r": "rb");
		if (ft_local_file != (FILE *)NULL) {
			(void) fclose(ft_local_file);
			ft_local_file = (FILE *)NULL;
			popup_overwrite();
			return 0;
		}
	}

	/* Open the local file. */
	ft_local_file = fopen(ft_local_filename, local_fflag());
	if (ft_local_file == (FILE *)NULL) {
		allow_overwrite = False;
		popup_an_errno(errno, "Open(%s)", ft_local_filename);
		return 0;
	}

	/* Build the ind$file command */
	op[0] = '\0';
	if (ascii_flag)
		strcat(op, " ascii");
	if (cr_flag)
		strcat(op, " crlf");
	if (append_flag && !receive_flag)
		strcat(op, " append");
	if (!receive_flag) {
		if (!vm_flag) {
			if (recfm != DEFAULT_RECFM) {
				/* RECFM Entered, process */
				strcat(op, " recfm(");
				switch (recfm) {
				    case RECFM_FIXED:
					strcat(op, "f");
					break;
				    case RECFM_VARIABLE:
					strcat(op, "v");
					break;
				    case RECFM_UNDEFINED:
					strcat(op, "u");
					break;
				    default:
					break;
				};
				strcat(op, ")");
				XtVaGetValues(lrecl_widget,
				    XtNstring, &lrecl,
				    NULL);
				if (strlen(lrecl) > 0)
					sprintf(eos(op), " lrecl(%s)", lrecl);
				XtVaGetValues(blksize_widget,
				    XtNstring, &blksize,
				    NULL);
				if (strlen(blksize) > 0)
					sprintf(eos(op), " blksize(%s)",
					    blksize);
			}
			if (units != DEFAULT_UNITS) {
				/* Space Entered, processs it */
				switch (units) {
				    case TRACKS:
					strcat(op, " tracks");
					break;
				    case CYLINDERS:
					strcat(op, " cylinders");
					break;
				    case AVBLOCK:
					strcat(op, " avblock");
					break;
				    default:
					break;
				};
				XtVaGetValues(primspace_widget, XtNstring,
				    &primspace, NULL);
				if (strlen(primspace) > 0) {
					sprintf(eos(op), " space(%s",
					    primspace);
					XtVaGetValues(secspace_widget,
					    XtNstring, &secspace,
					    NULL);
					if (strlen(secspace) > 0)
						sprintf(eos(op), ",%s",
						    secspace);
					strcat(op, ")");
				}
			}
		} else {
			if (recfm != DEFAULT_RECFM) {
				strcat(op, " recfm ");
				switch (recfm) {
				    case RECFM_FIXED:
					strcat(op, "f");
					break;
				    case RECFM_VARIABLE:
					strcat(op, "v");
					break;
				    default:
					break;
				};

				XtVaGetValues(lrecl_widget,
				    XtNstring, &lrecl,
				    NULL);
				if (strlen(lrecl) > 0)
					sprintf(eos(op), " lrecl %s", lrecl);
			}
		}
	}

	/* Insert the '(' for VM options. */
	if (strlen(op) > 0 && vm_flag) {
		opts[0] = ' ';
		opts[1] = '(';
		op = opts;
	}

	/* Build the whole command. */
	cmd = xs_buffer("ind\\e005Bfile %s %s%s\\n",
	    receive_flag ? "get" : "put", ft_host_filename, op);

	/* Erase the line and enter the command. */
	flen = kybd_prime();
	if (!flen || flen < strlen(cmd) - 1) {
		XtFree(cmd);
		if (ft_local_file != NULL) {
		    	fclose(ft_local_file);
			ft_local_file = NULL;
			if (receive_flag && !append_flag)
			    unlink(ft_local_filename);
		}
		popup_an_error(get_message("ftUnable"));
		allow_overwrite = False;
		return 0;
	}
	(void) emulate_input(cmd, strlen(cmd), False);
	XtFree(cmd);

	/* Get this thing started. */
	ft_state = FT_AWAIT_ACK;
	ft_is_cut = False;
	ft_last_cr = False;
#if defined(X3270_DBCS) /*[*/
	ft_last_dbcs = False;
#endif /*]*/

	return 1;
}

/* "Transfer in Progress" pop-up. */

/* Pop up the "in progress" pop-up. */
static void
popup_progress(void)
{
	/* Initialize it. */
	if (progress_shell == (Widget)NULL)
		progress_popup_init();

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
	    progress_popup_callback, (XtPointer)NULL);

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
	XtAddCallback(cancel_button, XtNcallback, progress_cancel_callback,
	    NULL);
}

/* Callbacks for the "in progress" pop-up. */

/* In-progress pop-up popped up. */
static void
progress_popup_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
	XtVaSetValues(from_file, XtNlabel,
	    receive_flag ? ft_host_filename : ft_local_filename, NULL);
	XtVaSetValues(to_file, XtNlabel,
	    receive_flag ? ft_local_filename : ft_host_filename, NULL);

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
	if (overwrite_shell == (Widget)NULL)
		overwrite_popup_init();

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
	    (XtPointer)NULL);

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
	if ((Dimension)(d + 20) < 400)
		d = 400;
	else
		d += 20;
	XtVaSetValues(overwrite_name, XtNwidth, d, NULL);
	XtVaGetValues(overwrite_name, XtNheight, &d, NULL);
	XtVaSetValues(overwrite_name, XtNheight, d + 20, NULL);

	okay_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, overwrite_pop,
	    XtNfromVert, overwrite_name,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
	XtAddCallback(okay_button, XtNcallback, overwrite_okay_callback,
	    NULL);

	cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, overwrite_pop,
	    XtNfromVert, overwrite_name,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, okay_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
	XtAddCallback(cancel_button, XtNcallback, overwrite_cancel_callback,
	    NULL);
}

/* Overwrite "okay" button. */
static void
overwrite_okay_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
	XtPopdown(overwrite_shell);

	allow_overwrite = True;
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
	overwrite_shell = (Widget)NULL;
}
#endif /*]*/

/* External entry points called by ft_dft and ft_cut. */

/* Pop up a message, end the transfer. */
void
ft_complete(const char *errmsg)
{
	/* Close the local file. */
	if (ft_local_file != (FILE *)NULL && fclose(ft_local_file) < 0)
		popup_an_errno(errno, "close(%s)", ft_local_filename);
	ft_local_file = (FILE *)NULL;

	/* Clean up the state. */
	ft_state = FT_NONE;

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
	/* Pop down the in-progress shell. */
	if (!ft_is_action)
		XtPopdown(progress_shell);
#endif /*]*/

	/* Pop up the text. */
	if (errmsg != CN) {
		char *msg_copy = NewString(errmsg);

		/* Make sure the error message will fit on the display. */
		if (strlen(msg_copy) > 50 && strchr(msg_copy, '\n') == CN) {
			char *s = msg_copy + 50;

			while (s > msg_copy && *s != ' ')
				s--;
			if (s > msg_copy)
				*s = '\n';	/* yikes! */
		}
#if defined(C3270) /*[*/
		printf("\r%79s\n", "");
		fflush(stdout);
#endif /*]*/
		popup_an_error(msg_copy);
		Free(msg_copy);
	} else {
		struct timeval t1;
		double kbytes_sec;
		char *buf;

		(void) gettimeofday(&t1, (struct timezone *)NULL);
		kbytes_sec = (double)ft_length / 1024.0 /
			((double)(t1.tv_sec - t0.tv_sec) + 
			 (double)(t1.tv_usec - t0.tv_usec) / 1.0e6);
		buf = Malloc(256);
		(void) sprintf(buf, get_message("ftComplete"), ft_length,
		    kbytes_sec, ft_is_cut ? "CUT" : "DFT");
		if (ft_is_action) {
#if defined(C3270) /*[*/
			printf("\r%79s\n", "");
			fflush(stdout);
#endif /*]*/
			sms_info("%s", buf);
			sms_continue();
		}
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
		else
			popup_an_info(buf);
#endif /*]*/
		Free(buf);
	}
}

/* Update the bytes-transferred count on the progress pop-up. */
void
ft_update_length(void)
{
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
	char text_string[80];

	/* Format the message */
	if (!ft_is_action) {
		sprintf(text_string, status_string, ft_length);

		XtVaSetValues(ft_status, XtNlabel, text_string, NULL);
	}
#endif /*]*/
#if defined(C3270) /*[*/
	printf("\r%79s\rTransferred %lu bytes. ", "", ft_length);
	fflush(stdout);
#endif /*]*/
}

/* Process a transfer acknowledgement. */
void
ft_running(Boolean is_cut)
{
	if (ft_state == FT_AWAIT_ACK) {
		ft_state = FT_RUNNING;
		if (ft_start_id) {
			RemoveTimeOut(ft_start_id);
			ft_start_id = 0;
		}
	}
	ft_is_cut = is_cut;
	(void) gettimeofday(&t0, (struct timezone *)NULL);
	ft_length = 0;

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
	if (!ft_is_action) {
		XtUnmapWidget(waiting);
		ft_update_length();
		XtMapWidget(ft_status);
	}
#endif /*]*/
#if defined(C3270) /*[*/
	ft_update_length();
#endif /*]*/
}

/* Process a protocol-generated abort. */
void
ft_aborting(void)
{
	if (ft_state == FT_RUNNING || ft_state == FT_ABORT_WAIT) {
		ft_state = FT_ABORT_SENT;
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
		if (!ft_is_action) {
			XtUnmapWidget(waiting);
			XtUnmapWidget(ft_status);
			XtMapWidget(aborting);
		}
#endif /*]*/
	}
}

/* Process a disconnect abort. */
static void
ft_connected(Boolean ignored _is_unused)
{
	if (!CONNECTED && ft_state != FT_NONE)
		ft_complete(get_message("ftDisconnected"));
}

/* Process an abort from no longer being in 3270 mode. */
static void
ft_in3270(Boolean ignored _is_unused)
{
	if (!IN_3270 && ft_state != FT_NONE)
		ft_complete(get_message("ftNot3270"));
}

/*
 * Script/macro action for file transfer.
 *  Transfer(option=value[,...])
 *  Options are:
 *   Direction=send|receive	default receive
 *   HostFile=name		required
 *   LocalFile=name			required
 *   Host=[tso|vm]		default tso
 *   Mode=[ascii|binary]	default ascii
 *   Cr=[add|remove|keep]	default add/remove
 *   Exist=[keep|replace|append]	default keep
 *   Recfm=[default|fixed|variable|undefined] default default
 *   Lrecl=n			no default
 *   Blksize=n			no default
 *   Allocation=[default|tracks|cylinders|avblock] default default
 *   PrimarySpace=n		no default
 *   SecondarySpace=n		no default
 */
static struct {
	const char *name;
	char *value;
	const char *keyword[4];
} tp[] = {
	{ "Direction",		CN, { "receive", "send" } },
	{ "HostFile" },
	{ "LocalFile" },
	{ "Host",		CN, { "tso", "vm" } },
	{ "Mode",		CN, { "ascii", "binary" } },
	{ "Cr",			CN, { "auto", "remove",	"add", "keep" } },
	{ "Exist",		CN, { "keep", "replace", "append" } },
	{ "Recfm",		CN, { "default", "fixed", "variable",
				      "undefined" } },
	{ "Lrecl" },
	{ "Blksize" },
	{ "Allocation",		CN, { "default", "tracks", "cylinders",
				      "avblock" } },
	{ "PrimarySpace" },
	{ "SecondarySpace" },
	{ "BufferSize" },
	{ CN }
};
enum ft_parm_name {
	PARM_DIRECTION,
	PARM_HOST_FILE,
	PARM_LOCAL_FILE,
	PARM_HOST,
	PARM_MODE,
	PARM_CR,
	PARM_EXIST,
	PARM_RECFM,
	PARM_LRECL,
	PARM_BLKSIZE,
	PARM_ALLOCATION,
	PARM_PRIMARY_SPACE,
	PARM_SECONDARY_SPACE,
	PARM_BUFFER_SIZE,
	N_PARMS
};

void  
Transfer_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	int i, k;
	Cardinal j;
	long l;
	char *ptr;

	char opts[80];
	char *op = opts + 1;
	char *cmd;
	unsigned flen;

	String *xparams = params;
	Cardinal xnparams = *num_params;

        action_debug(Transfer_action, event, params, num_params);

	ft_is_action = True;

	/* Make sure we're connected. */
	if (!IN_3270) {
		popup_an_error("Not connected");
		return;
	}

#if defined(C3270) || defined(WC3270) /*[*/
	/* Check for interactive mode. */
	if (xnparams == 0 && escaped) {
	    	if (interactive_transfer(&xparams, &xnparams) < 0) {
		    	printf("\n");
			fflush(stdout);
		    	action_output("Aborted");
		    	return;
		}
	}
#endif /*]*/

	/* Set everything to the default. */
	for (i = 0; i < N_PARMS; i++) {
		Free(tp[i].value);
		if (tp[i].keyword[0] != CN)
			tp[i].value =
				NewString(tp[i].keyword[0]);
		else
			tp[i].value = CN;
	}

	/* See what they specified. */
	for (j = 0; j < xnparams; j++) {
		for (i = 0; i < N_PARMS; i++) {
			char *eq;
			int kwlen;

			eq = strchr(xparams[j], '=');
			if (eq == CN || eq == xparams[j] || !*(eq + 1)) {
				popup_an_error("Invalid option syntax: '%s'",
					xparams[j]);
				return;
			}
			kwlen = eq - xparams[j];
			if (!strncasecmp(xparams[j], tp[i].name, kwlen)
					&& !tp[i].name[kwlen]) {
				if (tp[i].keyword[0]) {
					for (k = 0;
					     tp[i].keyword[k] != CN && k < 4;
					     k++) {
						if (!strcasecmp(eq + 1,
							tp[i].keyword[k])) {
							break;
						}
					}
					if (k >= 4 ||
					    tp[i].keyword[k] == CN) {
						popup_an_error("Invalid option "
							"value: '%s'", eq + 1);
						return;
					}
				} else switch (i) {
				    case PARM_LRECL:
				    case PARM_BLKSIZE:
				    case PARM_PRIMARY_SPACE:
				    case PARM_SECONDARY_SPACE:
				    case PARM_BUFFER_SIZE:
					l = strtol(eq + 1, &ptr, 10);
					if (ptr == eq + 1 || *ptr) {
						popup_an_error("Invalid option "
							"value: '%s'", eq + 1);
						return;
					}
					break;
				    default:
					break;
				}
				tp[i].value = NewString(eq + 1);
				break;
			}
		}
		if (i >= N_PARMS) {
			popup_an_error("Unknown option: %s", xparams[j]);
			return;
		}
	}

	/* Check for required values. */
	if (tp[PARM_HOST_FILE].value == CN) {
		popup_an_error("Missing 'HostFile' option");
		return;
	}
	if (tp[PARM_LOCAL_FILE].value == CN) {
		popup_an_error("Missing 'LocalFile' option");
		return;
	}

	/*
	 * Start the transfer.  Much of this is duplicated from ft_start()
	 * and should be made common.
	 */
	if (tp[PARM_BUFFER_SIZE].value != CN)
		dft_buffersize = atoi(tp[PARM_BUFFER_SIZE].value);
	else
		dft_buffersize = 0;
	set_dft_buffersize();

	receive_flag = !strcasecmp(tp[PARM_DIRECTION].value, "receive");
	append_flag = !strcasecmp(tp[PARM_EXIST].value, "append");
	allow_overwrite = !strcasecmp(tp[PARM_EXIST].value, "replace");
	ascii_flag = !strcasecmp(tp[PARM_MODE].value, "ascii");
	if (!strcasecmp(tp[PARM_CR].value, "auto")) {
		cr_flag = ascii_flag;
	} else {
		cr_flag = !strcasecmp(tp[PARM_CR].value, "remove") ||
			  !strcasecmp(tp[PARM_CR].value, "add");
	}
	vm_flag = !strcasecmp(tp[PARM_HOST].value, "vm");
	recfm = DEFAULT_RECFM;
	for (k = 0; tp[PARM_RECFM].keyword[k] != CN && k < 4; k++) {
		if (!strcasecmp(tp[PARM_RECFM].value,
			    tp[PARM_RECFM].keyword[k]))  {
			recfm = (enum recfm)k;
			break;
		}
	}
	units = DEFAULT_UNITS;
	for (k = 0; tp[PARM_ALLOCATION].keyword[k] != CN && k < 4; k++) {
		if (!strcasecmp(tp[PARM_ALLOCATION].value,
			    tp[PARM_ALLOCATION].keyword[k]))  {
			units = (enum units)k;
			break;
		}
	}

	ft_host_filename = tp[PARM_HOST_FILE].value;
	ft_local_filename = tp[PARM_LOCAL_FILE].value;

	/* See if the local file can be overwritten. */
	if (receive_flag && !append_flag && !allow_overwrite) {
		ft_local_file = fopen(ft_local_filename,
			ascii_flag? "r": "rb");
		if (ft_local_file != (FILE *)NULL) {
			(void) fclose(ft_local_file);
			popup_an_error("File exists");
			return;
		}
	}

	/* Open the local file. */
	ft_local_file = fopen(ft_local_filename, local_fflag());
	if (ft_local_file == (FILE *)NULL) {
		popup_an_errno(errno, "Open(%s)", ft_local_filename);
		return;
	}

	/* Build the ind$file command */
	op[0] = '\0';
	if (ascii_flag)
		strcat(op, " ascii");
	if (cr_flag)
		strcat(op, " crlf");
	if (append_flag && !receive_flag)
		strcat(op, " append");
	if (!receive_flag) {
		if (!vm_flag) {
			if (recfm != DEFAULT_RECFM) {
				/* RECFM Entered, process */
				strcat(op, " recfm(");
				switch (recfm) {
				    case RECFM_FIXED:
					strcat(op, "f");
					break;
				    case RECFM_VARIABLE:
					strcat(op, "v");
					break;
				    case RECFM_UNDEFINED:
					strcat(op, "u");
					break;
				    default:
					break;
				};
				strcat(op, ")");
				if (tp[PARM_LRECL].value != CN)
					sprintf(eos(op), " lrecl(%s)",
					    tp[PARM_LRECL].value);
				if (tp[PARM_BLKSIZE].value != CN)
					sprintf(eos(op), " blksize(%s)",
					    tp[PARM_BLKSIZE].value);
			}
			if (units != DEFAULT_UNITS) {
				/* Space Entered, processs it */
				switch (units) {
				    case TRACKS:
					strcat(op, " tracks");
					break;
				    case CYLINDERS:
					strcat(op, " cylinders");
					break;
				    case AVBLOCK:
					strcat(op, " avblock");
					break;
				    default:
					break;
				};
				if (tp[PARM_PRIMARY_SPACE].value != CN) {
					sprintf(eos(op), " space(%s",
					    tp[PARM_PRIMARY_SPACE].value);
					if (tp[PARM_SECONDARY_SPACE].value)
						sprintf(eos(op), ",%s",
						    tp[PARM_SECONDARY_SPACE].value);
					strcat(op, ")");
				}
			}
		} else {
			if (recfm != DEFAULT_RECFM) {
				strcat(op, " recfm ");
				switch (recfm) {
				    case RECFM_FIXED:
					strcat(op, "f");
					break;
				    case RECFM_VARIABLE:
					strcat(op, "v");
					break;
				    default:
					break;
				};

				if (tp[PARM_LRECL].value)
					sprintf(eos(op), " lrecl %s",
					    tp[PARM_LRECL].value);
			}
		}
	}

	/* Insert the '(' for VM options. */
	if (strlen(op) > 0 && vm_flag) {
		opts[0] = ' ';
		opts[1] = '(';
		op = opts;
	}

	/* Build the whole command. */
	cmd = xs_buffer("ind\\e005Bfile %s %s%s\\n",
	    receive_flag ? "get" : "put", ft_host_filename, op);

	/* Erase the line and enter the command. */
	flen = kybd_prime();
	if (!flen || flen < strlen(cmd) - 1) {
		Free(cmd);
		if (ft_local_file != NULL) {
		    	fclose(ft_local_file);
			ft_local_file = NULL;
			if (receive_flag && !append_flag)
			    unlink(ft_local_filename);
		}
		popup_an_error(get_message("ftUnable"));
		return;
	}
	(void) emulate_input(cmd, strlen(cmd), False);
	Free(cmd);
#if defined(C3270) /*[*/
	if (!escaped)
	    	screen_suspend();
	printf("Awaiting start of transfer... ");
	fflush(stdout);
#endif /*]*/

	/* Get this thing started. */
	ft_start_id = AddTimeOut(10 * 1000, ft_didnt_start);
	ft_state = FT_AWAIT_ACK;
	ft_is_cut = False;
}

#endif /*]*/
