/*
 * Copyright (c) 2018-2019 Paul Mattes.
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
 *      b3270proto.h
 *              Constants for the XML b3270 protocol.
 */

/* Start elements. */
#define DocIn		"b3270-in"
#define DocOut		"b3270-out"

/* Indications. */
#define IndAttr		"attr"
#define IndBell		"bell"
#define IndChar		"char"
#define IndCharset	"charset"
#define IndConnectAttempt "connect-attempt"
#define IndConnection	"connection"
#define IndCursor	"cursor"
#define IndErase	"erase"
#define IndFlipped	"flipped"
#define IndFont		"font"
#define IndFormatted	"formatted"
#define IndFt		"ft"
#define IndHello	"hello"
#define IndIconName	"icon-name"
#define IndOia		"oia"
#define IndPassthru	"passthru"
#define IndPopup	"popup"
#define IndReady	"ready"
#define IndRow		"row"
#define IndRunResult	"run-result"
#define IndScreenMode	"screen-mode"
#define IndScroll	"scroll"
#define IndSetting	"setting"
#define IndStats	"stats"
#define IndTerminalName	"terminal-name"
#define IndThumb	"thumb"
#define IndTls		"tls"
#define IndTlsHello	"tls-hello"
#define IndTraceFile	"trace-file"
#define IndUiError	"ui-error"
#define IndWindowTitle	"window-title"

/* Operations. */
#define OperFail	"fail"
#define OperRegister	"register"
#define OperRun		"run"
#define OperSucceed	"succeed"

/* Attributes. */
#define AttrAbort	"abort"
#define AttrAction	"action"
#define AttrActions	"actions"
#define AttrArg		"arg"
#define AttrAttribute	"attribute"
#define AttrBack	"back"
#define AttrBg		"bg"
#define AttrBuild	"build"
#define AttrBytes	"bytes"
#define AttrBytesReceived "bytes-received"
#define AttrBytesSent	"bytes-sent"
#define AttrCause	"cause"
#define AttrChar	"char"
#define AttrColor	"color"
#define AttrColumn	"column"
#define AttrColumns	"columns"
#define AttrCount	"count"
#define AttrCopyright	"copyright"
#define AttrElement	"element"
#define AttrEnabled	"enabled"
#define AttrError	"error"
#define AttrExtended	"extended"
#define AttrFatal	"fatal"
#define AttrField	"field"
#define AttrHelpText	"help-text"
#define AttrHelpParms	"help-parms"
#define AttrHost	"host"
#define AttrHostCert	"host-cert"
#define AttrHostIp	"host-ip"
#define AttrFg		"fg"
#define AttrField	"field"
#define AttrLine	"line"
#define AttrLogicalColumns "logical-columns"
#define AttrLogicalRows	"logical-rows"
#define AttrLu		"lu"
#define AttrModel	"model"
#define AttrName	"name"
#define AttrOptions	"options"
#define AttrOverride	"override"
#define AttrOversize	"oversize"
#define AttrPTag	"p-tag"
#define AttrParentRTag	"parent-r-tag"
#define AttrPort	"port"
#define AttrProvider	"provider"
#define AttrRecordsReceived "records-received"
#define AttrRecordsSent	"records-sent"
#define AttrRTag	"r-tag"
#define AttrRow		"row"
#define AttrRows	"rows"
#define AttrSaved	"saved"
#define AttrScreen	"screen"
#define AttrSecure	"secure"
#define AttrSession	"session"
#define AttrShown	"shown"
#define AttrState	"state"
#define AttrSuccess	"success"
#define AttrSupported	"supported"
#define AttrText	"text"
#define AttrTime	"time"
#define AttrTop		"top"
#define AttrType	"type"
#define AttrValue	"value"
#define AttrVerified	"verified"
#define AttrVersion	"version"

/* OIA fields. */
#define OiaNotUndera	"not-undera"
#define OiaCompose	"compose"
#define OiaInsert	"insert"
#define OiaLock		"lock"
#define OiaLu		"lu"
#define OiaReverseInput	"reverse-input"
#define OiaScreentrace	"screentrace"
#define OiaScript	"script"
#define OiaTiming	"timing"
#define OiaTypeahead	"typeahead"

/* OIA lock reasons. */
#define OiaLockNotConnected "not-connected"
#define OiaLockDeferred "deferred"
#define OiaLockInhibit "inhibit"
#define OiaLockMinus	"minus"
#define OiaLockOerr	"oerr"
#define OiaLockScrolled	"scrolled"
#define OiaLockSyswait	"syswait"
#define OiaLockTwait	"twait"
#define OiaLockDisabled	"disabled"

/* OIA operator errors */
#define OiaOerrProtected "protected"
#define OiaOerrNumeric	"numeric"
#define OiaOerrOverflow	"overflow"
#define OiaOerrDbcs	"dbcs"

/* Toggles. */
#define ToggleExtended	"extended"

/* Values. */
#define ValTrue		"true"
#define ValFalse	"false"
#define ValTrueFalse(b)	((b)? ValTrue: ValFalse)
