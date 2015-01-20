/*
 * Copyright (c) 1993-2015 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR
 * GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	resources.c
 *		Resource definitions for x3270.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"
#include "xappres.h"

#include <X11/StringDefs.h>

#include "resourcesc.h"

#define offset(field)		XtOffset(AppResptr, field)
#define toggle_offset(index)	offset(toggle[index])
XtResource resources[] = {
	{ ResColorBackground, ClsColorBackground, XtRString, sizeof(String),
	  offset(x3270.colorbg_name), XtRString, "black" },
	{ ResSelectBackground, ClsSelectBackground, XtRString, sizeof(String),
	  offset(x3270.selbg_name), XtRString, "dim gray" },
	{ ResNormalColor, ClsNormalColor, XtRString, sizeof(String),
	  offset(x3270.normal_name), XtRString, "green" },
	{ ResInputColor, ClsInputColor, XtRString, sizeof(String),
	  offset(x3270.select_name), XtRString, "green" },
	{ ResBoldColor, ClsBoldColor, XtRString, sizeof(String),
	  offset(x3270.bold_name), XtRString, "green" },
	{ ResCursorColor, ClsCursorColor, XtRString, sizeof(String),
	  offset(x3270.cursor_color_name), XtRString, "red" },
	{ ResMono, ClsMono, XtRBoolean, sizeof(Boolean),
	  offset(interactive.mono), XtRString, ResFalse },
	{ ResExtended, ClsExtended, XtRBoolean, sizeof(Boolean),
	  offset(extended), XtRString, ResTrue },
	{ ResM3279, ClsM3279, XtRBoolean, sizeof(Boolean),
	  offset(m3279), XtRString, ResTrue },
	{ ResKeypad, ClsKeypad, XtRString, sizeof(String),
	  offset(x3270.keypad), XtRString, KpRight },
	{ ResKeypadOn, ClsKeypadOn, XtRBoolean, sizeof(Boolean),
	  offset(x3270.keypad_on), XtRString, ResFalse },
	{ ResInvertKeypadShift, ClsInvertKeypadShift, XtRBoolean,
	    sizeof(Boolean),
	  offset(x3270.invert_kpshift), XtRString, ResFalse },
	{ ResSaveLines, ClsSaveLines, XtRInt, sizeof(int),
	  offset(interactive.save_lines), XtRString, "4096" },
	{ ResMenuBar, ClsMenuBar, XtRBoolean, sizeof(Boolean),
	  offset(interactive.menubar), XtRString, ResTrue },
	{ ResActiveIcon, ClsActiveIcon, XtRBoolean, sizeof(Boolean),
	  offset(x3270.active_icon), XtRString, ResFalse },
	{ ResLabelIcon, ClsLabelIcon, XtRBoolean, sizeof(Boolean),
	  offset(x3270.label_icon), XtRString, ResFalse },
	{ ResKeypadBackground, ClsKeypadBackground, XtRString, sizeof(String),
	  offset(x3270.keypadbg_name), XtRString, "grey70" },
	{ ResEmulatorFont, ClsEmulatorFont, XtRString, sizeof(char *),
	  offset(x3270.efontname), XtRString, 0 },
	{ ResVisualBell, ClsVisualBell, XtRBoolean, sizeof(Boolean),
	  offset(interactive.visual_bell), XtRString, ResFalse },
	{ ResAplMode, ClsAplMode, XtRBoolean, sizeof(Boolean),
	  offset(apl_mode), XtRString, ResFalse },
	{ ResOnce, ClsOnce, XtRBoolean, sizeof(Boolean),
	  offset(once), XtRString, ResFalse },
	{ ResScripted, ClsScripted, XtRBoolean, sizeof(Boolean),
	  offset(scripted), XtRString, ResFalse },
	{ ResModifiedSel, ClsModifiedSel, XtRBoolean, sizeof(Boolean),
	  offset(modified_sel), XtRString, ResFalse },
	{ ResUnlockDelay, ClsUnlockDelay, XtRBoolean, sizeof(Boolean),
	  offset(unlock_delay), XtRString, ResTrue },
	{ ResUnlockDelayMs, ClsUnlockDelayMs, XtRInt, sizeof(int),
	  offset(unlock_delay_ms), XtRString, "350" },
	{ ResBindLimit, ClsBindLimit, XtRBoolean, sizeof(Boolean),
	  offset(bind_limit), XtRString, ResTrue },
	{ ResNewEnviron, ClsNewEnviron, XtRBoolean, sizeof(Boolean),
	  offset(new_environ), XtRString, ResTrue },
	{ ResSocket, ClsSocket, XtRBoolean, sizeof(Boolean),
	  offset(socket), XtRString, ResFalse },
	{ ResScriptPort, ClsScriptPort, XtRInt, sizeof(int),
	  offset(script_port), XtRString, "0" },
	{ ResHttpd, ClsHttpd, XtRString, sizeof(String),
	  offset(httpd_port), XtRString, 0 },
	{ ResLoginMacro, ClsLoginMacro, XtRString, sizeof(String),
	  offset(login_macro), XtRString, 0 },
	{ ResUseCursorColor, ClsUseCursorColor, XtRBoolean, sizeof(Boolean),
	  offset(x3270.use_cursor_color), XtRString, ResFalse },
	{ ResReconnect, ClsReconnect, XtRBoolean, sizeof(Boolean),
	  offset(interactive.reconnect), XtRString, ResFalse },
	{ ResVisualSelect, ClsVisualSelect, XtRBoolean, sizeof(Boolean),
	  offset(x3270.visual_select), XtRString, ResFalse },
	{ ResSuppressHost, ClsSuppressHost, XtRBoolean, sizeof(Boolean),
	  offset(x3270.suppress_host), XtRString, ResFalse },
	{ ResSuppressFontMenu, ClsSuppressFontMenu, XtRBoolean, sizeof(Boolean),
	  offset(x3270.suppress_font_menu), XtRString, ResFalse },
	{ ResDoConfirms, ClsDoConfirms, XtRBoolean, sizeof(Boolean),
	  offset(interactive.do_confirms), XtRString, ResTrue },
	{ ResNumericLock, ClsNumericLock, XtRBoolean, sizeof(Boolean),
	  offset(numeric_lock), XtRString, ResFalse },
	{ ResAllowResize, ClsAllowResize, XtRBoolean, sizeof(Boolean),
	  offset(x3270.allow_resize), XtRString, ResTrue },
	{ ResSecure, ClsSecure, XtRBoolean, sizeof(Boolean),
	  offset(secure), XtRString, ResFalse },
	{ ResNoOther, ClsNoOther, XtRBoolean, sizeof(Boolean),
	  offset(x3270.no_other), XtRString, ResFalse },
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
	  offset(x3270.bell_volume), XtRString, "0" },
	{ ResOversize, ClsOversize, XtRString, sizeof(char *),
	  offset(oversize), XtRString, 0 },
	{ ResCharClass, ClsCharClass, XtRString, sizeof(char *),
	  offset(x3270.char_class), XtRString, 0 },
	{ ResModifiedSelColor, ClsModifiedSelColor, XtRInt, sizeof(int),
	  offset(x3270.modified_sel_color), XtRString, "10" },
	{ ResVisualSelectColor, ClsVisualSelectColor, XtRInt, sizeof(int),
	  offset(x3270.visual_select_color), XtRString, "6" },
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
	  offset(interactive.key_map), XtRString, 0 },
	{ ResComposeMap, ClsComposeMap, XtRString, sizeof(char *),
	  offset(interactive.compose_map), XtRString, "latin1" },
	{ ResHostsFile, ClsHostsFile, XtRString, sizeof(char *),
	  offset(hostsfile), XtRString, 0 },
	{ ResPort, ClsPort, XtRString, sizeof(char *),
	  offset(port), XtRString, "telnet" },
	{ ResCharset, ClsCharset, XtRString, sizeof(char *),
	  offset(charset), XtRString, "bracket" },
	{ ResSbcsCgcsgid, ClsSbcsCgcsgid, XtRString, sizeof(char *),
	  offset(sbcs_cgcsgid), XtRString, 0 },
	{ ResTermName, ClsTermName, XtRString, sizeof(char *),
	  offset(termname), XtRString, 0 },
	{ ResDevName, ClsDevName, XtRString, sizeof(char *),
	  offset(devname), XtRString, "x3270" },
	{ ResUser, ClsUser, XtRString, sizeof(char *),
	  offset(user), XtRString, 0 },
	{ ResIconFont, ClsIconFont, XtRString, sizeof(char *),
	  offset(x3270.icon_font), XtRString, "nil2" },
	{ ResIconLabelFont, ClsIconLabelFont, XtRString, sizeof(char *),
	  offset(x3270.icon_label_font), XtRString, "8x13" },
	{ ResMacros, ClsMacros, XtRString, sizeof(char *),
	  offset(macros), XtRString, 0 },
	{ ResFixedSize, ClsFixedSize, XtRString, sizeof(char *),
	  offset(x3270.fixed_size), XtRString, 0 },
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
	{ ResColorScheme, ClsColorScheme, XtRString, sizeof(String),
	  offset(x3270.color_scheme), XtRString, "default" },
	{ ResDftBufferSize, ClsDftBufferSize, XtRInt, sizeof(int),
	  offset(dft_buffer_size), XtRString, "4096" },
	{ ResConnectFileName, ClsConnectFileName, XtRString, sizeof(String),
	  offset(connectfile_name), XtRString, "~/.x3270connect" },
	{ ResIdleCommand, ClsIdleCommand, XtRString, sizeof(String),
	  offset(idle_command), XtRString, 0 },
	{ ResIdleCommandEnabled, ClsIdleCommandEnabled, XtRBoolean, sizeof(Boolean),
	  offset(idle_command_enabled), XtRString, ResFalse },
	{ ResIdleTimeout, ClsIdleTimeout, XtRString, sizeof(String),
	  offset(idle_timeout), XtRString, 0 },
	{ ResProxy, ClsProxy, XtRString, sizeof(String),
	  offset(proxy), XtRString, 0 },
	{ ResHostname, ClsHostname, XtRString, sizeof(String),
	  offset(hostname), XtRString, 0 },

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
	{ ResTrace, ClsTrace, XtRBoolean, sizeof(Boolean),
	  toggle_offset(TRACING), XtRString, ResFalse },
	{ ResDsTrace, ClsDsTrace, XtRBoolean, sizeof(Boolean),
	  offset(dsTrace_bc), XtRString, ResFalse },
	{ ResEventTrace, ClsEventTrace, XtRBoolean, sizeof(Boolean),
	  offset(eventTrace_bc), XtRString, ResFalse },
	{ ResScrollBar, ClsScrollBar, XtRBoolean, sizeof(Boolean),
	  toggle_offset(SCROLL_BAR), XtRString, ResTrue },
	{ ResLineWrap, ClsLineWrap, XtRBoolean, sizeof(Boolean),
	  toggle_offset(LINE_WRAP), XtRString, ResTrue },
	{ ResBlankFill, ClsBlankFill, XtRBoolean, sizeof(Boolean),
	  toggle_offset(BLANK_FILL), XtRString, ResFalse },
	{ ResScreenTrace, ClsScreenTrace, XtRBoolean, sizeof(Boolean),
	  toggle_offset(SCREEN_TRACE), XtRString, ResFalse },
	{ ResMarginedPaste, ClsMarginedPaste, XtRBoolean, sizeof(Boolean),
	  toggle_offset(MARGINED_PASTE), XtRString, ResFalse },
	{ ResRectangleSelect, ClsRectangleSelect, XtRBoolean, sizeof(Boolean),
	  toggle_offset(RECTANGLE_SELECT), XtRString, ResFalse },
	{ ResCrosshair, ClsCrosshair, XtRBoolean, sizeof(Boolean),
	  toggle_offset(CROSSHAIR), XtRString, ResFalse },
	{ ResVisibleControl, ClsVisibleControl, XtRBoolean, sizeof(Boolean),
	  toggle_offset(VISIBLE_CONTROL), XtRString, ResFalse },
	{ ResAidWait, ClsAidWait, XtRBoolean, sizeof(Boolean),
	  toggle_offset(AID_WAIT), XtRString, ResTrue },
	{ ResOverlayPaste, ClsOverlayPaste, XtRBoolean, sizeof(Boolean),
	  toggle_offset(OVERLAY_PASTE), XtRString, ResFalse },

	{ ResIcrnl, ClsIcrnl, XtRBoolean, sizeof(Boolean),
	  offset(linemode.icrnl), XtRString, ResTrue },
	{ ResInlcr, ClsInlcr, XtRBoolean, sizeof(Boolean),
	  offset(linemode.inlcr), XtRString, ResFalse },
	{ ResOnlcr, ClsOnlcr, XtRBoolean, sizeof(Boolean),
	  offset(linemode.onlcr), XtRString, ResTrue },
	{ ResErase, ClsErase, XtRString, sizeof(char *),
	  offset(linemode.erase), XtRString, "^?" },
	{ ResKill, ClsKill, XtRString, sizeof(char *),
	  offset(linemode.kill), XtRString, "^U" },
	{ ResWerase, ClsWerase, XtRString, sizeof(char *),
	  offset(linemode.werase), XtRString, "^W" },
	{ ResRprnt, ClsRprnt, XtRString, sizeof(char *),
	  offset(linemode.rprnt), XtRString, "^R" },
	{ ResLnext, ClsLnext, XtRString, sizeof(char *),
	  offset(linemode.lnext), XtRString, "^V" },
	{ ResIntr, ClsIntr, XtRString, sizeof(char *),
	  offset(linemode.intr), XtRString, "^C" },
	{ ResQuit, ClsQuit, XtRString, sizeof(char *),
	  offset(linemode.quit), XtRString, "^\\" },
	{ ResEof, ClsEof, XtRString, sizeof(char *),
	  offset(linemode.eof), XtRString, "^D" },

	{ ResPrinterLu, ClsPrinterLu, XtRString, sizeof(char *),
	  offset(interactive.printer_lu), XtRString, 0 },
	{ ResInputMethod, ClsInputMethod, XtRString, sizeof(char *),
	  offset(x3270.input_method), XtRString, 0 },
	{ ResPreeditType, ClsPreeditType, XtRString, sizeof(char *),
	  offset(x3270.preedit_type), XtRString, PT_OVER_THE_SPOT "+1" },
	{ ResDbcsCgcsgid, ClsDbcsCgcsgid, XtRString, sizeof(char *),
	  offset(dbcs_cgcsgid), XtRString, 0 },
#if defined(HAVE_LIBSSL) /*[*/
	{ ResAcceptHostname, ClsAcceptHostname, XtRString, sizeof(char *),
	  offset(ssl.accept_hostname), XtRString, 0 },
	{ ResCaDir, ClsCaDir, XtRString, sizeof(char *),
	  offset(ssl.ca_dir), XtRString, 0 },
	{ ResCaFile, ClsCaFile, XtRString, sizeof(char *),
	  offset(ssl.ca_file), XtRString, 0 },
	{ ResCertFile, ClsCertFile, XtRString, sizeof(char *),
	  offset(ssl.cert_file), XtRString, 0 },
	{ ResCertFileType, ClsCertFileType, XtRString, sizeof(char *),
	  offset(ssl.cert_file_type), XtRString, 0 },
	{ ResChainFile, ClsChainFile, XtRString, sizeof(char *),
	  offset(ssl.chain_file), XtRString, 0 },
	{ ResKeyFile, ClsKeyFile, XtRString, sizeof(char *),
	  offset(ssl.key_file), XtRString, 0 },
	{ ResKeyFileType, ClsKeyFileType, XtRString, sizeof(char *),
	  offset(ssl.key_file_type), XtRString, 0 },
	{ ResKeyPasswd, ClsKeyPasswd, XtRString, sizeof(char *),
	  offset(ssl.key_passwd), XtRString, 0 },
	{ ResSelfSignedOk, ClsSelfSignedOk, XtRBoolean, sizeof(Boolean),
	  offset(ssl.self_signed_ok), XtRString, ResFalse },
	{ ResTls, ClsTls, XtRBoolean, sizeof(Boolean),
	  offset(ssl.tls), XtRString, ResTrue },
	{ ResVerifyHostCert, ClsVerifyHostCert, XtRBoolean, sizeof(Boolean),
	  offset(ssl.verify_host_cert), XtRString, ResFalse },
#endif /*]*/

#if defined(USE_APP_DEFAULTS) /*[*/
	{ ResAdVersion, ClsAdVersion, XtRString, sizeof(char *),
	  offset(x3270.ad_version), XtRString, 0 },
#endif /*]*/
};
#undef offset
#undef toggle_offset

Cardinal num_resources = XtNumber(resources);

#define offset(field)		XtOffset(xappresptr_t, field)
XtResource xresources[] = {
	{ XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	  offset(foreground), XtRString, "XtDefaultForeground" },
	{ XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	  offset(background), XtRString, "XtDefaultBackground" },
	{ ResNormalCursor, ClsNormalCursor, XtRCursor, sizeof(Cursor),
	  offset(normal_mcursor), XtRString, "top_left_arrow" },
	{ ResWaitCursor, ClsWaitCursor, XtRCursor, sizeof(Cursor),
	  offset(wait_mcursor), XtRString, "watch" },
	{ ResLockedCursor, ClsLockedCursor, XtRCursor, sizeof(Cursor),
	  offset(locked_mcursor), XtRString, "X_cursor" },
};

Cardinal num_xresources = XtNumber(xresources);
