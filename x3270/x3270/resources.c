/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001, 2002,
 *   2003, 2004, 2005, 2007 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *   All Rights Reserved.  GTRC hereby grants public use of this software.
 *   Derivative works based on this software must incorporate this copyright
 *   notice.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	resources.c
 *		Resource definitions for x3270.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include <X11/StringDefs.h>

#include "resourcesc.h"

#define offset(field)		XtOffset(AppResptr, field)
#define toggle_offset(index)	offset(toggle[index].value)
XtResource resources[] = {
	{ XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	  offset(foreground), XtRString, "XtDefaultForeground" },
	{ XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	  offset(background), XtRString, "XtDefaultBackground" },
	{ ResColorBackground, ClsColorBackground, XtRString, sizeof(String),
	  offset(colorbg_name), XtRString, "black" },
	{ ResSelectBackground, ClsSelectBackground, XtRString, sizeof(String),
	  offset(selbg_name), XtRString, "dim gray" },
	{ ResNormalColor, ClsNormalColor, XtRString, sizeof(String),
	  offset(normal_name), XtRString, "green" },
	{ ResInputColor, ClsInputColor, XtRString, sizeof(String),
	  offset(select_name), XtRString, "green" },
	{ ResBoldColor, ClsBoldColor, XtRString, sizeof(String),
	  offset(bold_name), XtRString, "green" },
	{ ResCursorColor, ClsCursorColor, XtRString, sizeof(String),
	  offset(cursor_color_name), XtRString, "red" },
	{ ResMono, ClsMono, XtRBoolean, sizeof(Boolean),
	  offset(mono), XtRString, ResFalse },
	{ ResExtended, ClsExtended, XtRBoolean, sizeof(Boolean),
	  offset(extended), XtRString, ResTrue },
	{ ResM3279, ClsM3279, XtRBoolean, sizeof(Boolean),
	  offset(m3279), XtRString, ResTrue },
#if defined(X3270_KEYPAD) /*[*/
	{ ResKeypad, ClsKeypad, XtRString, sizeof(String),
	  offset(keypad), XtRString, KpRight },
	{ ResKeypadOn, ClsKeypadOn, XtRBoolean, sizeof(Boolean),
	  offset(keypad_on), XtRString, ResFalse },
	{ ResInvertKeypadShift, ClsInvertKeypadShift, XtRBoolean, sizeof(Boolean),
	  offset(invert_kpshift), XtRString, ResFalse },
#endif /*]*/
	{ ResSaveLines, ClsSaveLines, XtRInt, sizeof(int),
	  offset(save_lines), XtRString, "64" },
	{ ResMenuBar, ClsMenuBar, XtRBoolean, sizeof(Boolean),
	  offset(menubar), XtRString, ResTrue },
	{ ResActiveIcon, ClsActiveIcon, XtRBoolean, sizeof(Boolean),
	  offset(active_icon), XtRString, ResFalse },
	{ ResLabelIcon, ClsLabelIcon, XtRBoolean, sizeof(Boolean),
	  offset(label_icon), XtRString, ResFalse },
	{ ResKeypadBackground, ClsKeypadBackground, XtRString, sizeof(String),
	  offset(keypadbg_name), XtRString, "grey70" },
	{ ResEmulatorFont, ClsEmulatorFont, XtRString, sizeof(char *),
	  offset(efontname), XtRString, 0 },
	{ ResVisualBell, ClsVisualBell, XtRBoolean, sizeof(Boolean),
	  offset(visual_bell), XtRString, ResFalse },
	{ ResAplMode, ClsAplMode, XtRBoolean, sizeof(Boolean),
	  offset(apl_mode), XtRString, ResFalse },
	{ ResOnce, ClsOnce, XtRBoolean, sizeof(Boolean),
	  offset(once), XtRString,
#if defined(X3270_MENUS) /*[*/
	  ResFalse
#else /*][*/
	  ResTrue
#endif /*]*/
	},
	{ ResScripted, ClsScripted, XtRBoolean, sizeof(Boolean),
	  offset(scripted), XtRString, ResFalse },
	{ ResModifiedSel, ClsModifiedSel, XtRBoolean, sizeof(Boolean),
	  offset(modified_sel), XtRString, ResFalse },
	{ ResUnlockDelay, ClsUnlockDelay, XtRBoolean, sizeof(Boolean),
	  offset(unlock_delay), XtRString, ResTrue },
#if defined(X3270_SCRIPT) /*[*/
	{ ResSocket, ClsSocket, XtRBoolean, sizeof(Boolean),
	  offset(socket), XtRString, ResFalse },
	{ ResPluginCommand, ClsPluginCommand, XtRString, sizeof(String),
	  offset(plugin_command), XtRString, "x3270hist.pl" },
#endif /*]*/
	{ ResUseCursorColor, ClsUseCursorColor, XtRBoolean, sizeof(Boolean),
	  offset(use_cursor_color), XtRString, ResFalse },
	{ ResReconnect, ClsReconnect, XtRBoolean, sizeof(Boolean),
	  offset(reconnect), XtRString, ResFalse },
	{ ResVisualSelect, ClsVisualSelect, XtRBoolean, sizeof(Boolean),
	  offset(visual_select), XtRString, ResFalse },
	{ ResSuppressHost, ClsSuppressHost, XtRBoolean, sizeof(Boolean),
	  offset(suppress_host), XtRString, ResFalse },
	{ ResSuppressFontMenu, ClsSuppressFontMenu, XtRBoolean, sizeof(Boolean),
	  offset(suppress_font_menu), XtRString, ResFalse },
	{ ResDoConfirms, ClsDoConfirms, XtRBoolean, sizeof(Boolean),
	  offset(do_confirms), XtRString, ResTrue },
	{ ResNumericLock, ClsNumericLock, XtRBoolean, sizeof(Boolean),
	  offset(numeric_lock), XtRString, ResFalse },
	{ ResAllowResize, ClsAllowResize, XtRBoolean, sizeof(Boolean),
	  offset(allow_resize), XtRString, ResTrue },
	{ ResSecure, ClsSecure, XtRBoolean, sizeof(Boolean),
	  offset(secure), XtRString, ResFalse },
	{ ResNoOther, ClsNoOther, XtRBoolean, sizeof(Boolean),
	  offset(no_other), XtRString, ResFalse },
	{ ResOerrLock, ClsOerrLock, XtRBoolean, sizeof(Boolean),
	  offset(oerr_lock), XtRString, ResTrue },
	{ ResTypeahead, ClsTypeahead, XtRBoolean, sizeof(Boolean),
	  offset(typeahead), XtRString, ResTrue },
	{ ResDebugTracing, ClsDebugTracing, XtRBoolean, sizeof(Boolean),
	  offset(debug_tracing), XtRString, ResTrue },
	{ ResDisconnectClear, ClsDisconnectClear, XtRBoolean, sizeof(Boolean),
	  offset(disconnect_clear), XtRString, ResFalse },
	{ ResHighlightBold, ClsHighlightBold, XtRBoolean, sizeof(Boolean),
	  offset(highlight_bold), XtRString, ResFalse },
	{ ResColor8, ClsColor8, XtRBoolean, sizeof(Boolean),
	  offset(color8), XtRString, ResFalse },
	{ ResBsdTm, ClsBsdTm, XtRBoolean, sizeof(Boolean),
	  offset(bsd_tm), XtRString, ResFalse },
	{ ResBellVolume, ClsBellVolume, XtRInt, sizeof(int),
	  offset(bell_volume), XtRString, "0" },
	{ ResOversize, ClsOversize, XtRString, sizeof(char *),
	  offset(oversize), XtRString, 0 },
	{ ResCharClass, ClsCharClass, XtRString, sizeof(char *),
	  offset(char_class), XtRString, 0 },
	{ ResModifiedSelColor, ClsModifiedSelColor, XtRInt, sizeof(int),
	  offset(modified_sel_color), XtRString, "10" },
	{ ResVisualSelectColor, ClsVisualSelectColor, XtRInt, sizeof(int),
	  offset(visual_select_color), XtRString, "6" },
	{ ResConfDir, ClsConfDir, XtRString, sizeof(char *),
	  offset(conf_dir), XtRString, LIBX3270DIR },
	{ ResModel, ClsModel, XtRString, sizeof(char *),
	  offset(model), XtRString,
#if defined(RESTRICT_3279) /*[*/
	  "3279-3-E"
#else /*][*/
	  "3279-4-E"
#endif /*]*/
	  },
	{ ResKeymap, ClsKeymap, XtRString, sizeof(char *),
	  offset(key_map), XtRString, 0 },
	{ ResComposeMap, ClsComposeMap, XtRString, sizeof(char *),
	  offset(compose_map), XtRString, "latin1" },
	{ ResHostsFile, ClsHostsFile, XtRString, sizeof(char *),
	  offset(hostsfile), XtRString, 0 },
	{ ResPort, ClsPort, XtRString, sizeof(char *),
	  offset(port), XtRString, "telnet" },
	{ ResCharset, ClsCharset, XtRString, sizeof(char *),
	  offset(charset), XtRString, "bracket" },
	{ ResTermName, ClsTermName, XtRString, sizeof(char *),
	  offset(termname), XtRString, 0 },
	{ ResDebugFont, ClsDebugFont, XtRString, sizeof(char *),
	  offset(debug_font), XtRString, "3270d" },
	{ ResIconFont, ClsIconFont, XtRString, sizeof(char *),
	  offset(icon_font), XtRString, "nil2" },
	{ ResIconLabelFont, ClsIconLabelFont, XtRString, sizeof(char *),
	  offset(icon_label_font), XtRString, "8x13" },
	{ ResNormalCursor, ClsNormalCursor, XtRCursor, sizeof(Cursor),
	  offset(normal_mcursor), XtRString, "top_left_arrow" },
	{ ResWaitCursor, ClsWaitCursor, XtRCursor, sizeof(Cursor),
	  offset(wait_mcursor), XtRString, "watch" },
	{ ResLockedCursor, ClsLockedCursor, XtRCursor, sizeof(Cursor),
	  offset(locked_mcursor), XtRString, "X_cursor" },
	{ ResMacros, ClsMacros, XtRString, sizeof(char *),
	  offset(macros), XtRString, 0 },
	{ ResFixedSize, ClsFixedSize, XtRString, sizeof(char *),
	  offset(fixed_size), XtRString, 0 },
#if defined(X3270_TRACE) /*[*/
	{ ResTraceDir, ClsTraceDir, XtRString, sizeof(char *),
	  offset(trace_dir), XtRString, "/tmp" },
	{ ResTraceFile, ClsTraceFile, XtRString, sizeof(char *),
	  offset(trace_file), XtRString, 0 },
	{ ResTraceFileSize, ClsTraceFileSize, XtRString, sizeof(char *),
	  offset(trace_file_size), XtRString, 0 },
	{ ResTraceMonitor, ClsTraceMonitor, XtRBoolean, sizeof(Boolean),
	  offset(trace_monitor), XtRString, ResTrue },
	{ ResScreenTraceFile, ClsScreenTraceFile, XtRString, sizeof(char *),
	  offset(screentrace_file), XtRString, 0 },
#endif /*]*/
	{ ResColorScheme, ClsColorScheme, XtRString, sizeof(String),
	  offset(color_scheme), XtRString, "default" },
#if defined(X3270_FT) /*[*/
	{ ResFtCommand, ClsFtCommand, XtRString, sizeof(String),
	  offset(ft_command), XtRString, 0 },
	{ ResDftBufferSize, ClsDftBufferSize, XtRInt, sizeof(int),
	  offset(dft_buffer_size), XtRString, "4096" },
#endif /*]*/
	{ ResConnectFileName, ClsConnectFileName, XtRString, sizeof(String),
	  offset(connectfile_name), XtRString, "~/.x3270connect" },
#if defined(X3270_SCRIPT) /*[*/
	{ ResIdleCommand, ClsIdleCommand, XtRString, sizeof(String),
	  offset(idle_command), XtRString, 0 },
	{ ResIdleCommandEnabled, ClsIdleCommandEnabled, XtRBoolean, sizeof(Boolean),
	  offset(idle_command_enabled), XtRString, ResFalse },
	{ ResIdleTimeout, ClsIdleTimeout, XtRString, sizeof(String),
	  offset(idle_timeout), XtRString, 0 },
#endif /*]*/
	{ ResProxy, ClsProxy, XtRString, sizeof(String),
	  offset(proxy), XtRString, 0 },

	{ ResMonoCase, ClsMonoCase, XtRBoolean, sizeof(Boolean),
	  toggle_offset(MONOCASE), XtRString, ResFalse },
	{ ResAltCursor, ClsAltCursor, XtRBoolean, sizeof(Boolean),
	  toggle_offset(ALT_CURSOR), XtRString, ResFalse },
	{ ResCursorBlink, ClsCursorBlink, XtRBoolean, sizeof(Boolean),
	  toggle_offset(CURSOR_BLINK), XtRString, ResFalse },
	{ ResShowTiming, ClsShowTiming, XtRBoolean, sizeof(Boolean),
	  toggle_offset(SHOW_TIMING), XtRString, ResFalse },
	{ ResCursorPos, ClsCursorPos, XtRBoolean, sizeof(Boolean),
	  toggle_offset(CURSOR_POS), XtRString, ResTrue },
#if defined(X3270_TRACE) /*[*/
	{ ResDsTrace, ClsDsTrace, XtRBoolean, sizeof(Boolean),
	  toggle_offset(DS_TRACE), XtRString, ResFalse },
#endif /*]*/
	{ ResScrollBar, ClsScrollBar, XtRBoolean, sizeof(Boolean),
	  toggle_offset(SCROLL_BAR), XtRString, ResFalse },
#if defined(X3270_ANSI) /*[*/
	{ ResLineWrap, ClsLineWrap, XtRBoolean, sizeof(Boolean),
	  toggle_offset(LINE_WRAP), XtRString, ResTrue },
#endif /*]*/
	{ ResBlankFill, ClsBlankFill, XtRBoolean, sizeof(Boolean),
	  toggle_offset(BLANK_FILL), XtRString, ResFalse },
#if defined(X3270_TRACE) /*[*/
	{ ResScreenTrace, ClsScreenTrace, XtRBoolean, sizeof(Boolean),
	  toggle_offset(SCREEN_TRACE), XtRString, ResFalse },
	{ ResEventTrace, ClsEventTrace, XtRBoolean, sizeof(Boolean),
	  toggle_offset(EVENT_TRACE), XtRString, ResFalse },
#endif /*]*/
	{ ResMarginedPaste, ClsMarginedPaste, XtRBoolean, sizeof(Boolean),
	  toggle_offset(MARGINED_PASTE), XtRString, ResFalse },
	{ ResRectangleSelect, ClsRectangleSelect, XtRBoolean, sizeof(Boolean),
	  toggle_offset(RECTANGLE_SELECT), XtRString, ResFalse },
	{ ResCrosshair, ClsCrosshair, XtRBoolean, sizeof(Boolean),
	  toggle_offset(CROSSHAIR), XtRString, ResFalse },
	{ ResVisibleControl, ClsVisibleControl, XtRBoolean, sizeof(Boolean),
	  toggle_offset(VISIBLE_CONTROL), XtRString, ResFalse },
#if defined(X3270_SCRIPT) /*[*/
	{ ResAidWait, ClsAidWait, XtRBoolean, sizeof(Boolean),
	  toggle_offset(AID_WAIT), XtRString, ResTrue },
#endif /*]*/

#if defined(X3270_ANSI) /*[*/
	{ ResIcrnl, ClsIcrnl, XtRBoolean, sizeof(Boolean),
	  offset(icrnl), XtRString, ResTrue },
	{ ResInlcr, ClsInlcr, XtRBoolean, sizeof(Boolean),
	  offset(inlcr), XtRString, ResFalse },
	{ ResOnlcr, ClsOnlcr, XtRBoolean, sizeof(Boolean),
	  offset(onlcr), XtRString, ResTrue },
	{ ResErase, ClsErase, XtRString, sizeof(char *),
	  offset(erase), XtRString, "^?" },
	{ ResKill, ClsKill, XtRString, sizeof(char *),
	  offset(kill), XtRString, "^U" },
	{ ResWerase, ClsWerase, XtRString, sizeof(char *),
	  offset(werase), XtRString, "^W" },
	{ ResRprnt, ClsRprnt, XtRString, sizeof(char *),
	  offset(rprnt), XtRString, "^R" },
	{ ResLnext, ClsLnext, XtRString, sizeof(char *),
	  offset(lnext), XtRString, "^V" },
	{ ResIntr, ClsIntr, XtRString, sizeof(char *),
	  offset(intr), XtRString, "^C" },
	{ ResQuit, ClsQuit, XtRString, sizeof(char *),
	  offset(quit), XtRString, "^\\" },
	{ ResEof, ClsEof, XtRString, sizeof(char *),
	  offset(eof), XtRString, "^D" },
#endif /*]*/

#if defined(X3270_PRINTER) /*[*/
	{ ResPrinterLu, ClsPrinterLu, XtRString, sizeof(char *),
	  offset(printer_lu), XtRString, 0 },
#endif /*]*/

#if defined(X3270_DBCS) /*[*/
	{ ResInputMethod, ClsInputMethod, XtRString, sizeof(char *),
	  offset(input_method), XtRString, 0 },
	{ ResPreeditType, ClsPreeditType, XtRString, sizeof(char *),
	  offset(preedit_type), XtRString, PT_OVER_THE_SPOT "+1" },
	{ ResLocalEncoding, ClsLocalEncoding, XtRString, sizeof(char *),
	  offset(local_encoding), XtRString, 0 },
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
	{ ResCertFile, ClsCertFile, XtRString, sizeof(char *),
	  offset(cert_file), XtRString, 0 },
#endif /*]*/

#if defined(USE_APP_DEFAULTS) /*[*/
	{ ResAdVersion, ClsAdVersion, XtRString, sizeof(char *),
	  offset(ad_version), XtRString, 0 },
#endif /*]*/
};
#undef offset
#undef toggle_offset

Cardinal num_resources = XtNumber(resources);
