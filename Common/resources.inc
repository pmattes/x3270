dnl Copyright (c) 2000-2009, 2014-2016 Paul Mattes.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are met:
dnl     * Redistributions of source code must retain the above copyright
dnl       notice, this list of conditions and the following disclaimer.
dnl     * Redistributions in binary form must reproduce the above copyright
dnl       notice, this list of conditions and the following disclaimer in the
dnl       documentation and/or other materials provided with the distribution.
dnl     * Neither the names of Paul Mattes nor the names of his contributors
dnl       may be used to endorse or promote products derived from this software
dnl       without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
dnl WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
dnl EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
dnl TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
dnl PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
dnl LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
dnl NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
dnl SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
ifelse(XX_PRODUCT,c3270,`XX_SH(Screen Size Switching)
When running as a 3270 Model 5, XX_FB(c3270) can take advantage of terminals
that can switch between 80 and 132 column modes.
XX_LP
Because the curses library does not support mode switching, the escape
sequences and resulting screen dimensions must be specified explicitly to
XX_FB(c3270).
These are specified with the XX_FB(XX_DASHED(altscreen)) and
XX_FB(XX_DASHED(defscreen)) command-line options, or the
XX_FB(altScreen) and XX_FB(defScreen) resources.
XX_FB(XX_DASHED(altscreen)) or XX_FB(altScreen) defines the alternate
(132-column) mode; XX_FB(XX_DASHED(defscreen)) or XX_FB(defScreen) defines the
default (80-column) mode.
XX_LP
The syntax for the options and resources is
XX_FI(rows)`'XX_FB(x)`'XX_FI(cols)`'XX_FB(=)`'XX_FI(init_string),
where XX_FI(rows) and XX_FI(cols) give the screen dimensions, and
XX_FI(init_string) is the escape sequence to transmit to the terminal to
enter that mode.
For XX_FB(defscreen), the minimum dimensions are 24 rows and 80 columns.
For XX_FB(altscreen), the minimum dimensions are 27 rows and 132 columns.
Within XX_FI(init_string), the usual escape sequences are supported
(XX_BACKSLASH(E) for escape, XX_BACKSLASH(r), XX_BACKSLASH(b), etc.).
For example, the init string for a 132-column xterm is:
XX_IP
XX_BACKSLASH(E)[?40h`'XX_BACKSLASH(E)[?3h
XX_LP
Note: When XX_FB(defscreen) and XX_FB(altscreen) are specified, the model
number is always set to 5.
')dnl
XX_SH(Resources)
Certain XX_FB(XX_PRODUCT)
options can be configured via resources.
Resources are defined
ifelse(XX_PRODUCT,c3270,`in the file XX_FB(.c3270pro) in the XX_POSESSIVE(user)
home directory, and
')dnl
ifelse(XX_PRODUCT,wc3270,`in the session file, or
')dnl
by XX_FB(XX_DASHED(xrm)) options.
The definitions are similar to X11 resources, and use a similar syntax.
The resources available in XX_FB(XX_PRODUCT) are:
XX_LP
XX_TS(4, l l l l.)
XX_TR(XX_TD(Resource)	XX_TD(Default)	XX_TD(Option)	XX_TD(Purpose))
XX_T_()
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(allBold)	XX_TD(Auto)	XX_TD(XX_DASHED(allbold))	XX_TD(Display all characters bold))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(altScreen)	XX_TD(XX_NBSP)	XX_TD(XX_DASHED(altscreen))	XX_TD(132-col screen definition))
')dnl
XX_TR(XX_TD(blankFill)	XX_TD(False)	XX_TD(XX_DASHED(set) blankFill)	XX_TD(Blank Fill mode))
XX_TR(XX_TD(charset)	XX_TD(bracket)	XX_TD(XX_DASHED(charset))	XX_TD(XX_SM(EBCDIC) character set))
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(composeMap)	XX_TD(latin1)	XX_TD(XX_NBSP)	XX_TD(Name of composed-character map))
')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(consoleColorForHostColor`'XX_FI(n))	XX_TD(XX_LINK(#rn4,`(note 6)'))	XX_TD(XX_NBSP)	XX_TD(Color mapping))')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(XX_HY(cursesColor,ForHostColor)`'XX_FI(n))	XX_TD(XX_LINK(#rn4,`(note 6)'))	XX_TD(XX_NBSP)	XX_TD(Color mapping))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(XX_HY(cursesColor,ForDefault))	XX_TD(green)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(XX_HY(cursesColor,ForIntensified))	XX_TD(red)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(XX_HY(cursesColor,ForProtected))	XX_TD(blue)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(XX_HY(cursesColor,ForProtectedIntensified))	XX_TD(white)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(cursesKeymap)	XX_TD(True)	XX_TD(XX_NBSP)	XX_TD(Set curses keymap option))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(defScreen)	XX_TD(XX_NBSP)	XX_TD(XX_DASHED(defscreen))	XX_TD(80-col screen definition))
')dnl
XX_TR(XX_TD(dbcsCgcsgid)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP)	XX_TD(Override DBCS CGCSGID))
XX_TR(XX_TD(dsTrace)	XX_TD(False)	XX_TD(XX_DASHED(trace))	XX_TD(Data stream tracing))
XX_TR(XX_TD(eof)	XX_TD(^D)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode XX_SM(EOF) character))
XX_TR(XX_TD(erase)	XX_TD(^H)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode erase character))
XX_TR(XX_TD(extended)	XX_TD(True)	XX_TD(XX_NBSP)	XX_TD(Use 3270 extended data stream))
XX_TR(XX_TD(eventTrace)	XX_TD(False)	XX_TD(XX_DASHED(trace))	XX_TD(Event tracing))
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(hostColorForDefault)	XX_TD(green)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(hostColorForIntensified)	XX_TD(red)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(hostColorForProtected)	XX_TD(blue)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(hostColorForProtectedIntensified)	XX_TD(neutralWhite)	XX_TD(XX_NBSP)	XX_TD(Default color mapping))')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(hostsFile)	XX_TD(XX_NBSP)	XX_TD(XX_DASHED(hostsfile))	XX_TD(Host alias/macro file))
')dnl
XX_TR(XX_TD(icrnl)	XX_TD(False)	XX_TD(XX_NBSP)	XX_TD(Map XX_SM(CR) to XX_SM(NL) on XX_SM(NVT)-mode input))
XX_TR(XX_TD(inlcr)	XX_TD(False)	XX_TD(XX_NBSP)	XX_TD(Map XX_SM(NL) to XX_SM(CR) in XX_SM(NVT)-mode input))
XX_TR(XX_TD(intr)	XX_TD(^C)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode interrupt character))
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(keymap)	XX_TD(XX_NBSP)	XX_TD(XX_DASHED(keymap))	XX_TD(Keyboard map name))
XX_TR(XX_TD(keymap.XX_FI(foo))	XX_TD(XX_NBSP)	XX_TD(XX_NBSP)	XX_TD(Definition of keymap XX_FI(foo)))
')dnl
XX_TR(XX_TD(kill)	XX_TD(^U)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode kill character))
XX_TR(XX_TD(lineWrap)	XX_TD(False)	XX_TD(XX_DASHED(set) lineWrap)	XX_TD(XX_SM(NVT) line wrap mode))
XX_TR(XX_TD(lnext)	XX_TD(^V)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode lnext character))
ifelse(XX_PRODUCT,ws3270,`XX_TR(XX_TD(localCp)	XX_TD((system ANSI code page))	XX_TD(XX_DASHED(localcp))	XX_TD(Windows code page for local I/O))
')dnl
XX_TR(XX_TD(m3279)	XX_TD(XX_LINK(#rn1,`(note 1)'))	XX_TD(XX_DASHED(model))	XX_TD(3279 (color) emulation))
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(metaEscape)	XX_TD(Auto)	XX_TD(XX_NBSP)	XX_TD(Interpret ESC-x as Meta-x))
')dnl
ifelse(XX_PRODUCT,c3270,,XX_PRODUCT,s3270,,XX_PRODUCT,tcl3270,,`XX_TR(XX_TD(marginedPaste)	XX_TD(False)	XX_TD(XX_DASHED(set) marginedPaste)	XX_TD(Keep left margin when pasting))
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(mono)	XX_TD(XX_LINK(#rn5, `(note 5)'))	XX_TD(XX_DASHED(mono))	XX_TD(Ignore terminal color capabilities))
')dnl
XX_TR(XX_TD(monoCase)	XX_TD(False)	XX_TD(XX_DASHED(set) monoCase)	XX_TD(Mono-case mode))
ifelse(XX_MODE,console,`XX_TR(XX_TD(noPrompt)	XX_TD(False)	XX_TD(XX_DASHED(noprompt))	XX_TD(Disable command-prompt mode))
')dnl
XX_TR(XX_TD(numericLock)	XX_TD(False)	XX_TD(XX_NBSP)	XX_TD(Lock keyboard for numeric field error))
XX_TR(XX_TD(oerrLock)	XX_TD(ifelse(XX_PRODUCT,c3270,True,False))	XX_TD(XX_NBSP)	XX_TD(Lock keyboard for input error))
ifelse(XX_PRODUCT,c3270,,XX_PRODUCT,s3270,,XX_PRODUCT,tcl3270,,`XX_TR(XX_TD(overlayPaste)	XX_TD(False)	XX_TD(XX_DASHED(set) overlayPaste)	XX_TD(Overlay protected fields when pasting))
')dnl
XX_TR(XX_TD(oversize)	XX_TD(XX_NBSP)	XX_TD(XX_DASHED(oversize))	XX_TD(Oversize screen dimensions))
XX_TR(XX_TD(port)	XX_TD(telnet)	XX_TD(XX_DASHED(port))	XX_TD(Non-default TCP port))
ifelse(XX_MODE,console,`XX_TR(XX_TD(printer.*)	XX_TD(XX_LINK(#rn4,`(note 4)'))	XX_TD(XX_NBSP)	XX_TD(Printer session config))
XX_TR(XX_TD(printerLu)	XX_TD(XX_LINK(#rn4,`(note 4)'))	XX_TD(XX_NBSP)	XX_TD(Printer session config))
')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(printTextFont)	XX_TD(Courier New)	XX_TD(XX_NBSP)	XX_TD(PrintText font name))
XX_TR(XX_TD(printTextSize)	XX_TD(8)	XX_TD(XX_NBSP)	XX_TD(PrintText font size))
')dnl
XX_TR(XX_TD(quit)	XX_TD(^XX_BS)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode quit character))
ifelse(XX_MODE,console,`XX_TR(XX_TD(reconnect)	XX_TD(False)	XX_TD(XX_DASHED(reconnect))	XX_TD(Automatically reconnect to host))
')dnl
XX_TR(XX_TD(rprnt)	XX_TD(^R)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode reprint character))
XX_TR(XX_TD(sbcsCgcsgid)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP)	XX_TD(Override SBCS CGCSGID))
XX_TR(XX_TD(secure)	XX_TD(False)	XX_TD(XX_NBSP)	XX_TD(Disable "dangerous" options))
XX_TR(XX_TD(termName)	XX_TD(XX_LINK(#rn2,`(note 2)'))	XX_TD(XX_DASHED(tn))	XX_TD(XX_SM(TELNET) terminal type string))
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(title)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP)	XX_TD(Console window title))
')dnl
ifelse(XX_PLATFORM,windows,,`XX_TR(XX_TD(traceDir)	XX_TD(/tmp)	XX_TD(XX_NBSP)	XX_TD(Directory for trace files))
')dnl
XX_TR(XX_TD(traceFile)	XX_TD(XX_LINK(#rn3,`(note 3)'))	XX_TD(XX_DASHED(tracefile))	XX_TD(File for trace output))
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TD(typeahead)	XX_TD(True)	XX_TD(XX_NBSP)	XX_TD(Allow typeahead))
')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TD(visualBell)	XX_TD(False)	XX_TD(XX_NBSP)	XX_TD(Disable bell sound))
')dnl
XX_TR(XX_TD(werase)	XX_TD(^W)	XX_TD(XX_NBSP)	XX_TD(XX_SM(NVT)-mode word-erase character))
XX_TE()
XX_LP
XX_RS(`XX_TARGET(rn1)XX_FI(Note 1): XX_FB(m3279) defaults to
ifelse(XX_PRODUCT,c3270,`XX_FB(True) if the terminal supports color,
XX_FB(False) otherwise.
It can be forced to XX_FB(False) with the proper XX_FB(XX_DASHED(model))
option.',
XX_PRODUCT,wc3270,`XX_FB(True).
It can be forced to XX_FB(False) with the proper XX_FB(XX_DASHED(model))
option.',
XX_PRODUCT,x3270,`XX_FB(True) if the display supports color,
XX_FB(False) otherwise.
It can be forced to XX_FB(False) with the proper XX_FB(XX_DASHED(model))
option.',
`XX_FB(False).
It can be forced to XX_FB(True) with the proper XX_FB(XX_DASHED(model))
option.')
XX_LP
XX_TARGET(rn2)XX_FI(Note 2):
The default terminal type string is constructed from the model number, color
emulation, and extended data stream modes.
E.g., a model 2 with color emulation and the extended data stream option
would be sent as XX_FB(IBM-3279-2-E).
Note also that when XX_SM(TN3270E)
mode is used, the terminal type is always sent as 3278, but this does not
affect color capabilities.
XX_LP
XX_TARGET(rn3)XX_FI(Note 3): The default trace file is
ifelse(XX_PRODUCT,wc3270,`XX_FB(x3trc.)`'XX_FI(pid)`'XX_FB(.txt)
on the current XX_POSESSIVE(user) desktop',XX_PRODUCT,ws3270,`XX_FB(x3trc.)`'XX_FI(pid)`'XX_FB(.txt)
in the current directory',`XX_FB(x3trc.)`'XX_FI(pid) in the directory specified by
the XX_FB(traceDir) resource').
ifelse(XX_PRODUCT,c3270,`XX_LP
XX_TARGET(rn4)XX_FI(Note 4): See XX_LINK(#Printer-Support,XX_SM(PRINTER SUPPORT)) for details.')
ifelse(XX_PRODUCT,wc3270,`XX_LP
XX_TARGET(rn4)XX_FI(Note 4): See XX_LINK(#Printer-Support,XX_SM(PRINTER SUPPORT)) for details.')'
ifelse(XX_PRODUCT,c3270,`XX_LP
XX_TARGET(rn5)XX_FI(Note 5): XX_FB(mono) defaults to XX_FB(false) if the
terminal supports at least 8 colors and to XX_FB(true) otherwise.')
ifelse(XX_PRODUCT,wc3270,`XX_LP
XX_TARGET(rn6)`XX_FI(Note 6): The default console color mappings for host colors
0 through 15 are: 0, 9, 12, 13, 10, 11, 14, 15, 0, 1, 12, 5, 2, 3, 7, and 15.'
')dnl
ifelse(XX_PRODUCT,c3270,`XX_LP
XX_TARGET(rn6)`XX_FI(Note 6): The default curses color mappings for host colors
0 through 15 are: black, blue, red, magenta, green, cyan, yellow, white, black,
blue, yellow, blue, green, cyan, black and white.'
'))
XX_LP
ifelse(XX_PRODUCT,c3270,`In XX_FB(.c3270pro), lines are continued with a
backslash character.
XX_LP
XX_FB(XX_DASHED(xrm)) options override definitions found in XX_FB(.c3270pro).
')dnl
If more than one XX_FB(XX_DASHED(xrm)) option is given for the same resource,
the last one on the command line is used.
