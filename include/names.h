/*
 * Copyright (c) 2020 Paul Mattes.
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
 *	names.h
 *		Common names.
 */

/* Action names. */
#define AnAbort		"Abort"
#define AnAttn		"Attn"
#define AnBackSpace	"BackSpace"
#define AnBackTab	"BackTab"
#define AnBell		"Bell"
#define AnAnsiText	"AnsiText"
#define AnAscii		"Ascii"
#define AnAscii1	"Ascii1"
#define AnAsciiField	"AsciiField"
#define AnCapabilities	"Capabilities"
#define AnCircumNot	"CircumNot"
#define AnClear		"Clear"
#define AnClose		"Close"
#define AnCompose	"Compose"
#define AnConnect	"Connect"
#define AnCursorSelect	"CursorSelect"
#define AnCloseScript	"CloseScript"
#define AnDelete	"Delete"
#define AnDeleteField	"DeleteField"
#define AnDeleteWord	"DeleteWord"
#define AnDisconnect	"Disconnect"
#define AnDown		"Down"
#define AnDup		"Dup"
#define AnEnter		"Enter"
#define AnErase		"Erase"
#define AnEraseEOF	"EraseEOF"
#define AnEraseInput	"EraseInput"
#define AnFieldEnd	"FieldEnd"
#define AnFieldMark	"FieldMark"
#define AnFlip		"Flip"
#define AnEbcdic	"Ebcdic"
#define AnEbcdic1	"Ebcdic1"
#define AnEbcdicField	"EbcdicField"
#define AnEscape	"Escape"
#define AnExecute	"Execute"
#define AnExpect	"Expect"
#define AnHexString	"HexString"
#define AnHome		"Home"
#define	AnHelp		"Help"
#define Anignore	"ignore"
#define AnInsert	"Insert"
#define AnInterrupt	"Interrupt"
#define AnKey		"Key"
#define AnKeymap	"Keymap"
#define AnKeypad	"Keypad"
#define AnLeft		"Left"
#define AnLeft2		"Left2"
#define AnKeyboardDisable "KeyboardDisable"
#define AnMacro		"Macro"
#define AnMonoCase	"MonoCase"
#define AnMoveCursor	"MoveCursor"
#define AnMoveCursor1	"MoveCursor1"
#define AnNewline	"Newline"
#define AnNextWord	"NextWord"
#define AnOpen		"Open"
#define AnPA		"PA"
#define AnPasteString	"PasteString"
#define AnPF		"PF"
#define AnPreviousWord	"PreviousWord"
#define AnPrinter	"Printer"
#define AnPrintText	"PrintText"
#define AnPrompt	"Prompt"
#define AnQuery		"Query"
#define AnQuit		"Quit"
#define AnReadBuffer	"ReadBuffer"
#define AnReconnect	"Reconnect"
#define AnRedraw	"Redraw"
#define AnRequestInput	"RequestInput"
#define AnReset		"Reset"
#define AnRight		"Right"
#define AnRight2	"Right2"
#define AnScreenTrace	"ScreenTrace"
#define AnScript	"Script"
#define AnScroll	"Scroll"
#define AnSet		"Set"
#define AnShow		"Show"
#define AnString	"String"
#define AnSnap		"Snap"
#define AnSource	"Source"
#define AnSysReq	"SysReq"
#define AnTab		"Tab"
#define AnTemporaryComposeMap "TemporaryComposeMap"
#define AnTemporaryKeymap "TemporaryKeymap"
#define AnTitle		"Title"
#define AnToggle	"Toggle"
#define AnToggleInsert	"ToggleInsert"
#define AnToggleReverse	"ToggleReverse"
#define AnTrace		"Trace"
#define AnTransfer	"Transfer"
#define AnUp		"Up"
#define AnWait		"Wait"

/* Keywords. */
/*  Common keywords. */
#define KwFailOnError	"FailOnError"
#define KwNoFailOnError	"NoFailOnError"
/*  Parameters to HexString(). */
#define KwDashAscii	"-Ascii"
/*  Parameters to KeyboardDisable(). */
#define KwForceEnable	"ForceEnable"
/*  Parameters to Keymap(). */
#define KwNone		"None"
/*  Parameters to Printer(). */
#define KwStart		"Start"
#define KwStop		"Stop"
/*  Parameters to Query(). */
#define KwAbout		"About"
#define KwActions	"Actions"
#define KwBindPluName	"BindPluName"
#define KwBuildOptions	"BuildOptions"
#define KwConnectionState "ConnectionState"
#define KwConnectTime	"ConnectTime"
#define KwCodePage	"CodePage"
#define KwCodePages	"CodePages"
#define KwCopyright	"Copyright"
#define KwCursor	"Cursor"
#define KwCursor1	"Cursor1"
#define KwFormatted	"Formatted"
#define KwHost		"Host"
#define KwKeymap	"Keymap"
#define KwLocalEncoding	"LocalEncoding"
#define KwLuName	"LuName"
#define KwModel		"Model"
#define KwPrefixes	"Prefixes"
#define KwProxy		"Proxy"
#define KwScreenCurSize	"ScreenCurSize"
#define KwScreenMaxSize	"ScreenMaxSize"
#define KwScreenSizeCurrent "ScreenSizeCurrent"
#define KwScreenSizeMax	"ScreenSizeMax"
#define KwScreenTraceFile "ScreenTraceFile"
#define KwSsl		"Ssl"
#define KwStats		"Stats"
#define KwStatus	"Status"
#define KwStatsRx	"StatsRx"
#define KwStatsTx	"StatsTx"
#define KwTasks		"Tasks"
#define KwTelnetMyOptions "TelnetMyOptions"
#define KwTelnetHostOptions "TelnetHostOptions"
#define KwTerminalName	"TerminalName"
#define KwTn3270eOptions "Tn3270eOptions"
#define KwTraceFile	"TraceFile"
#define KwTls		"Tls"
#define KwTlsCertInfo	"TlsCertInfo"
#define KwTlsProvider	"TlsProvider"
#define KwTlsSessionInfo "TlsSessionInfo"
#define KwTn		"Tn"
#define KwVersion	"Version"
/*  Parameters to Quit(). */
#define KwDashForce	"-Force"
#define KwForce		"Force"
/*  Parameters to ReadBuffer(). */
#define KwAscii		"Ascii"
#define KwEbcdic	"Ebcdic"
#define KwUnicode	"Unicode"
#define KwField		"Field"
/*  Parameters to RequestInput(). */
#define KwDashNoEcho	"-NoEcho"
/*  Parameters to ScreenTrace(). */
#define KwOn		"On"
#define KwOff		"Off"
#define KwInfo		"Info"
#define KwFile		"File"
#define KwPrinter	"Printer"
#define KwText		"Text"
#define KwHtml		"Html"
#define KwRtf		"Rtf"
#define KwGdi		"Gdi"
#define KwDialog	"Dialog"
#define KwNoDialog	"NoDialog"
#define KwWordPad	"WordPad"
/*  Parameters to Script(). */
#define KwDashAsync	"-Async"
#define KwDashNoLock	"-NoLock"
#define KwDashSingle	"-Single"
#define KwDashNoStdoutRedirect "-NoStdoutRedirect"
/*  Parameters to Scroll(). */
#define KwForward	"Forward"
#define KwBackward	"Backward"
#define KwReset		"Reset"
#define KwSet		"Set"
/*  Parameters to Snap(). */
#define KwSave		"Save"
#define KwStatus	"Status"
#define KwRows		"Rows"
#define KwCols		"Cols"
/*  Parameters to Transfer(). */
#define KwCancel	"Cancel"
/*  Parameters to Wait(). */
#define Kw3270		"3270"
#define Kw3270Mode	"3270Mode"
#define KwAnsi		"Ansi"
#define KwDisconnect	"Disconnect"
#define KwInputField	"InputField"
#define KwNvtMode	"NvtMode"
#define KwOutput	"Output"
#define KwUnlock	"Unlock"
#define KwSeconds	"Seconds"
