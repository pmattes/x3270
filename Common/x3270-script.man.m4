dnl Copyright (c) 2000-2014, 2016 Paul Mattes.
dnl All rights reserved.
dnl 
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl     * Redistributions of source code must retain the above copyright
dnl       notice, this list of conditions and the following disclaimer.
dnl     * Redistributions in binary form must reproduce the above copyright
dnl       notice, this list of conditions and the following disclaimer in the
dnl       documentation and/or other materials provided with the distribution.
dnl     * Neither the name of Paul Mattes nor his contributors may be used
dnl       to endorse or promote products derived from this software without
dnl       specific prior written permission.
dnl 
dnl THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
dnl OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
dnl WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
dnl DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
dnl INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
dnl (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
dnl SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
dnl HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
dnl STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
dnl IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
dnl POSSIBILITY OF SUCH DAMAGE.
XX_TH(X3270-SCRIPT,1,XX_DATE)
XX_SH(Name)
Scripting Facilities for
ifelse(XX_PLATFORM,unix,`x3270,')
XX_C3270 and XX_S3270
XX_SH(Synopsis)
ifelse(XX_PLATFORM,unix,`XX_FB(x3270) XX_FB(XX_DASHED(script)) [ XX_FI(x3270-options) ]
XX_BR
XX_FB(x3270) XX_FB(XX_DASHED(socket)) [ XX_FI(x3270-options) ]
XX_BR
XX_FB(x3270) XX_FB(XX_DASHED(scriptport) XX_FI(port)) [ XX_FI(x3270-options) ]
XX_BR
')dnl
ifelse(XX_PLATFORM,unix,`XX_FB(XX_C3270) XX_FB(XX_DASHED(socket)) [ XX_FI(XX_C3270-options) ]
XX_BR
')dnl
XX_FB(XX_C3270) XX_FB(XX_DASHED(scriptport)) XX_FI(port) [ XX_FI(XX_S3270-options) ]
XX_BR
XX_FB(XX_S3270) [ XX_FI(XX_S3270-options) ]
XX_BR
XX_FB(`Script') ( XX_FI(command) [ `,'XX_FI(arg)... ] )
XX_SH(Description)
The XX_FB(x3270) scripting facilities allow the interactive 3270
ifelse(XX_PLATFORM,unix,`emulators XX_FB(x3270) and XX_FB(c3270)',
`emulator XX_FB(wc3270)')
to be operated under the control of another
program, and forms the basis for the script-only emulator XX_FB(XX_S3270).
XX_PP
There are
ifelse(XX_PLATFORM,unix,four,three)
basic scripting methods.
The first is the XX_FB(peer script) facility,
ifelse(XX_PLATFORM,unix,`invoked by the XX_FB(x3270)
XX_FB(XX_DASHED(script)) switch, and ')dnl
the default mode for XX_FB(XX_S3270).
This runs the emulator as a child of another process.
Typically this would be a script using
XX_FI(expect)(1), XX_FI(perl)(1),
or the co-process facility of the Korn Shell
XX_FI(ksh)(1).
In this mode, the emulator process looks for commands on its standard input,
and places the responses on standard output.
XX_PP
The second method is the XX_FB(child script)
facility, invoked by the emulator's XX_FB(Script) action.
This runs a script as a child process of the emulator.
ifelse(XX_PLATFORM,unix,`The child has access to pipes connected to the
emulator; the emulator looks for commands on one pipe, and places the responses on the other.
The file descriptor of the pipe for commands to the emulator
is passed in the environment variable X3270INPUT (e.g., the text string "7" if
the file descriptor is 7); the file descriptor
of the pipe for responses from the emulator is passed in the environment
variable X3270OUTPUT.
',`The emulator creates a TCP listening socket on the IPv4 loopback address,
127.0.0.1, and places the TCP port in the environment variable X3270PORT
(e.g., the text string "47196" if the TCP port is 47196).
')dnl
XX_PP
The third method uses a TCP socket.
The XX_FB(XX_DASHED(scrpiptport)) command-line option causes the emulator to
bind a socket to the specified port (on the IPv4 loopback address, 127.0.0.1).
The emulator accepts TCP connections on that port.
Multiple commands and responses can be sent over each connection.
ifelse(XX_PLATFORM,windows,`(Note that if the XX_FB(Script) action is used
with XX_FB(XX_DASHED(scriptport)), it will pass the scriptport port number to child
scripts, rather than creating a new socket.)
')dnl
ifelse(XX_PLATFORM,unix,`XX_PP
The fourth method uses a Unix-domain socket.
The XX_FB(XX_DASHED(socket)) command-line option causes the emulator to
create a Unix-domain stream socket named XX_FB(/tmp/x3sck.)`'XX_FI(pid).
The emulator accepts connections to that socket.
Multiple commands and responses can be sent over each connection.
')dnl
XX_PP
It is possible to nest the methods.
For example, a peer or TCP socket script can invoke the XX_FB(Script) action.
The calling script will be resumed when the nested script completes.
XX_PP
Commands are emulator XX_FI(actions); the syntax is the same as for the
right-hand side of
ifelse(XX_PLATFORM,unix,`an XX_FB(x3270) or',`a')
XX_FB(XX_C3270) keymap.
Unlike translation tables, action names are case-insensitive, can be
uniquely abbreviated, and the parentheses may be omitted if there are
no parameters.
Any input line that begins with XX_FB(#) or XX_FB(!) is treaded as a comment
and will be ignored.
XX_PP
Any emulator action may be specified.
Several specific actions have been defined for use by scripts, and the behavior
of certain other actions (and of the emulators in general) is different when
an action is initiated by a script.
XX_PP
Some actions generate output; some may delay completion until the certain
external events occur, such as the host unlocking the keyboard.
The completion of every command is marked by a two-line message.
The first line is the current status of the emulator, documented below.
If the command is successful, the second line is the string "ok"; otherwise it
is the string "error".
XX_SH(Status Format)
The status message consists of 12 blank-separated fields:
XX_TPS()dnl
XX_TP(1 Keyboard State)
If the keyboard is unlocked, the letter
XX_FB(U).
If the keyboard is locked waiting for a response from the host, or if not
connected to a host, the letter
XX_FB(L).
If the keyboard is locked because of an operator error (field overflow,
protected field, etc.), the letter
XX_FB(E).
XX_TP(2 Screen Formatting)
If the screen is formatted, the letter
XX_FB(F).
If unformatted or in XX_SM(NVT) mode, the letter XX_FB(U).
XX_TP(3 Field Protection)
If the field containing the cursor is protected, the letter
XX_FB(P).
If unprotected or unformatted, the letter
XX_FB(U).
XX_TP(4 Connection State)
If connected to a host, the string
XX_FB(`C(')`'XX_FI(hostname)`'XX_FB(`)').
Otherwise, the letter
XX_FB(N).
XX_TP(5 Emulator Mode)
If connected in 3270 mode, the letter
XX_FB(I).
If connected in XX_SM(NVT) line mode, the letter
XX_FB(L).
If connected in XX_SM(NVT) character mode, the letter
XX_FB(C).
If connected in unnegotiated mode (no BIND active from the host), the letter
XX_FB(P).
If not connected, the letter
XX_FB(N).
XX_TP(6 Model Number (2-5))
XX_TP(7 Number of Rows)
The current number of rows defined on the screen.
The host can request that the emulator
use a 24x80 screen, so this number may be smaller than the maximum number of
rows possible with the current model.
XX_TP(8 Number of Columns)
The current number of columns defined on the screen, subject to the same
difference for rows, above.
XX_TP(9 Cursor Row)
The current cursor row (zero-origin).
XX_TP(10 Cursor Column)
The current cursor column (zero-origin).
XX_TP(11 Window ID)
The X window identifier for the main
XX_FB(x3270)
window, in hexadecimal preceded by
XX_FB(0x).
For
XX_FB(XX_S3270) and XX_FB(XX_C3270),
this is zero.
XX_TP(12 Command Execution Time)
The time that it took for the host to respond to the previous commnd, in
seconds with milliseconds after the decimal.
If the previous command did not require a host response, this is a dash.
XX_TPE()dnl
XX_SH(Differences)
When an action is initiated by a script, the emulators
behave in several different ways:
XX_PP
If an error occurs in processing an action, the usual pop-up window does not
appear.
Instead, the text is written to standard output.
XX_PP
If end-of-file is detected on standard input, the emulator exits.
(A script can exit without killing the emulator
by using the XX_FB(CloseScript) action, below.)
Note that this applies to peer scripts only; end-of-file on the pipe
connected to a child script simply causes the pipes to be closed and
the
XX_FB(Script)
action to complete.
XX_PP
The XX_FB(Quit) action always causes the emulator to exit.
(When called from the keyboard, it will exit only if not connected to a host.)
XX_PP
Normally, the AID actions (XX_FB(Clear),
XX_FB(Enter),
XX_FB(PF),
and
XX_FB(PA))
will not complete until the host unlocks the keyboard.
If the parameter to a
XX_FB(String)
action includes a code for one these actions,
it will also wait for the keyboard to unlock before proceeding.
XX_PP
The XX_FB(AidWait) toggle controls with behavior.
When this toggle is set (the default), actions block as described above.
When the toggle is clear, AID actions complete immediately.
The XX_FB(Wait(Output)) action can then be used to delay a script until the
host changes something on the screen, and the
XX_FB(Wait(Unlock)) action can be used to delay a script until the host
unlocks the keyboard, regardless of the state of the XX_FB(AidWait) toggle.
XX_PP
Note that the
XX_FB(Script)
action does not complete until
ifelse(XX_PLATFORM,unix,`end-of-file is detected on the pipe or ')dnl
the XX_FB(CloseScript) action is called by the child process.
This behavior is not affected by the state of the XX_FB(AidWait) toggle.
XX_SH(Basic Programming Strategies)
3270 session scripting can be more difficult than other kinds of scripting,
because it can be hard to tell when the host is finished processing a
command.
There is a well-defined 3270 Data Stream facility for doing this: The emulator
locks the keyboard when it sends the host an AID, and the later host unlocks
the keyboard.
The emulator supports this facility directly by not allowing an AID action
to complete until the keyboard is unlocked.
Unfortunately, some hosts and some host applications unlock the keyboard as
soon as they begin processing the command, instead of after it is finished.
A human operator can see on the screen when the command is finished (e.g.,
when a READY prompt is displayed), but it can be difficult for a script to
do this. For such early-unlock hosts, the only option in a script is to poll the
screen until it can determine that the command is complete.
XX_LP
Another complication is that host I/O and script operation are asynchronous.
That is, the host can update the screen at any time, even between actions that
are reading the screen contents, so a script can get inconsistent results.
Assistance for this problem is provided by the XX_FB(Snap) action.
The XX_FB(Snap(Save)) action saves a snapshot of the screen in a special
buffer. Then the script can use XX_FB(Snap) variants of the XX_FB(Ascii) and
XX_FB(Ebcdic) actions (XX_FB(Snap(Ascii)) and XX_FB(Snap(Ebcdic))) to query
the saved buffer -- which the host cannot modify -- to get the data it wants.
Finally, XX_FB(Snap(Wait Output)) blocks the script until the host
modifies the screen, specifically since the last call to XX_FB(Snap(Save)).
Thus a script can poll the screen efficiently by writing a loop that begins
with XX_FB(Snap(Save)) and ends with XX_FB(Snap(Wait Output)).
XX_SH(Script-Specific Actions)
The following actions have been defined or modified for use with scripts.
(Note that unlike the display on the status line,
XX_FI(row)
and
XX_FI(col)
coordinates used in these actions use [0,0] as their origin at the upper left,
not [1,1]).
XX_TPS()dnl
XX_TP(XX_FB(AnsiText))
Outputs whatever data that has been output by the host in
XX_SM(NVT) mode
since the last time that
XX_FB(AnsiText)
was called.
The data is preceded by the string "data:XX_NBSP", and has had all control characters
expanded into C backslash sequences.
XX_IP
This is a convenient way to capture
XX_SM(NVT)
mode output in a synchronous manner without trying to decode the screen
contents.
XX_TP(XX_FB(Ascii)(XX_FI(row),XX_FI(col),XX_FI(rows),XX_FI(cols)))
XX_TP(XX_FB(Ascii)(XX_FI(row),XX_FI(col),XX_FI(length)))
XX_TP(XX_FB(Ascii)(XX_FI(length)))
XX_TP(XX_FB(Ascii))
Outputs an XX_SM(ASCII) text representation of the screen contents.
Each line is preceded by the string "data:XX_NBSP", and there are no control
characters.
XX_IP
If four parameters are given, a rectangular region of the screen is output.
(Note that the row and column are zero-origin.)
XX_IP
If three parameters are given,
XX_FI(length)
characters are output, starting at the specified zero-origin row and column.
XX_IP
If only the
XX_FI(length)
parameter is given, that many characters are output, starting at the cursor
position.
XX_IP
If no parameters are given, the entire screen is output.
XX_IP
The EBCDIC-to-ASCII translation and output character set depend on the both the
emulator character set (the XX_FB(XX_DASHED(charset)) option) and the locale.
UTF-8 and certain DBCS locales may result in multi-byte expansions of EBCDIC
characters that translate to ASCII codes greater than 0x7f.
XX_TP(XX_FB(AsciiField))
Outputs an XX_SM(ASCII) text representation of the field containing the cursor.
The text is preceded by the string "data:XX_NBSP".
XX_TP(XX_FB(Connect)(XX_FI(hostname)))
Connects to a host.
The command does not return until the emulator
is successfully connected in the proper mode, or the connection fails.
XX_TP(XX_FB(CloseScript)(XX_FI(status)))
Causes the emulator to stop reading commands from the script.
This is useful to allow a peer script to exit, with the emulator
proceeding interactively.
(Without this command, the emulator
would exit when it detected end-of-file on standard input.)
If the script was invoked by the
XX_FB(Script)
action, the optional
XX_FI(status)
is used as the return status of
XX_FB(Script);
if nonzero,
XX_FB(Script)
will complete with an error, and if this script was invoked as part of
login through the
XX_FB(ibm_hosts)
file, the connection will be broken.
XX_TP(XX_FB(ContinueScript)([XX_FI(param)]))
Allows a script that is waiting in a
XX_FB(PauseScript)
action, below, to continue.
The optional
XX_FI(param)
string is output by the
XX_FB(PauseScript)
action.
XX_TP(XX_FB(Disconnect))
Disconnects from the host.
XX_TP(XX_FB(Ebcdic)(XX_FI(row),XX_FI(col),XX_FI(rows),XX_FI(cols)))
XX_TP(XX_FB(Ebcdic)(XX_FI(row),XX_FI(col),XX_FI(length)))
XX_TP(XX_FB(Ebcdic)(XX_FI(length)))
XX_TP(XX_FB(Ebcdic))
The same function as
XX_FB(Ascii)
above, except that rather than generating
XX_SM(ASCII)
text, each character is output as a 2-digit or 4-digit hexadecimal
XX_SM(EBCDIC)
code.
XX_TP(XX_FB(EbcdicField))
The same function as
XX_FB(AsciiField)
above, except that it generates hexadecimal
XX_SM(EBCDIC)
codes.
XX_TP(XX_FB(Info)(XX_FI(message)))
In x3270, pops up an informational message.
In c3270 and wc3270, writes an informational message to the OIA (the line below
the display).
Not defined for s3270 or tcl3270.
XX_TP(XX_FB(Expect)(XX_FI(text)[,XX_FI(timeout)]))
Pauses the script until the specified
XX_FI(text)
appears in the data stream from the host, or the specified
XX_FI(timeout)
(in seconds) expires.
If no
XX_FI(timeout)
is specified, the default is 30 seconds.
XX_FI(Text)
can contain standard C-language escape (backslash) sequences.
No wild-card characters or pattern anchor characters are understood.
XX_FB(Expect)
is valid only in
XX_SM(NVT)
mode.
ifelse(XX_MODE,console,`XX_TP(XX_FB(Keymap([XX_FI(keymap)])))
Adds or removes a temporary keymap.
If the XX_FI(keymap) parameter is given, the named keymap is added.
If no parameter is given, the most recently added keymap is removed.
')dnl
XX_TP(XX_FB(MoveCursor)(XX_FI(row),XX_FI(col)))
Moves the cursor to the specified zero-origin coordinates.
XX_TP(XX_FB(PauseScript))
Stops a script until the
XX_FB(ContinueScript)
action, above, is executed.
This allows a script to wait for user input and continue.
Outputs the single parameter to
XX_FB(ContinueScript), if one is given.
XX_TP(XX_FB(PrintText)([XX_FB(command),]XX_FI(filter)))
Pipes an ASCII representation of the current screen image through the named
XX_FI(filter), e.g., XX_FB(lpr).
XX_TP(XX_FB(PrintText)([XX_FB(html),][XX_FB(append),][XX_FB(replace),]XX_FB(file),XX_FI(filename)))
Saves the current screen contents in a file.
With the XX_FB(html) option, saves it as HTML, otherwise saves it as plain
ASCII.
The XX_FB(append) option (the default) causes the data to be appended to the
file if it already exists. The XX_FB(replace) option causes the file to be
overwritten instead.
XX_TP(XX_FB(PrintText)(XX_FB(`html,string')))
Returns the current screen contents as HTML.
XX_TP(XX_FB(Query)(XX_FI(keyword)))
Returns state information.
Keywords are:
XX_PP
XX_TS(3,`center;
l l .')
XX_TR(XX_TD(Keyword)	XX_TD(Output))
XX_T_
XX_TR(XX_TD(BindPluName)	XX_TD(BIND PLU returned by the host))
XX_TR(XX_TD(ConnectionState)	XX_TD(TN3270/TN3270E mode and submode))
XX_TR(XX_TD(CodePage)	XX_TD(Host code page))
XX_TR(XX_TD(Cursor)	XX_TD(Cursor position (row col), zero-origin))
XX_TR(XX_TD(Formatted)	XX_TD(3270 format state (formatted or unformatted)))
XX_TR(XX_TD(Host)	XX_TD(Host name and port))
XX_TR(XX_TD(LocalEncoding)	XX_TD(Local character encoding))
XX_TR(XX_TD(LuName)	XX_TD(Host name LU name))
XX_TR(XX_TD(Model)	XX_TD(3270 model name (IBM-327x-n)))
XX_TR(XX_TD(ScreenCurSize)	XX_TD(Current screen size (rows cols)))
XX_TR(XX_TD(ScreenMaxSize)	XX_TD(Maximum screen size (rows cols)))
XX_TR(XX_TD(Ssl)	XX_TD(SSL state (secure or not-secure) and host validation state (host-verified or host-unverified)))
XX_TE
XX_IP
Without a XX_FI(keyword), XX_FB(Query) returns each of the defined attributes,
one per line, labeled by its name.
XX_TP(XX_FB(ReadBuffer)(XX_FB(Ascii)))
Dumps the contents of the screen buffer, one line at a time.
Positions inside data fields are generally output as 2-digit hexadecimal codes
in the current display character set.
If the current locale specifies UTF-8 (or certain DBCS character sets), some
positions may be output as multi-byte strings (4-, 6- or 8-digit codes).
DBCS characters take two positions in the screen buffer; the first location
is output as a multi-byte string in the current locale codeset, and the second
location is output as a dash.
Start-of-field characters (each of which takes up a display position) are
output as XX_FB(SF`(aa=nn[,...])'), where XX_FI(aa) is a field
attribute type and XX_FI(nn) is its value.
XX_PP
XX_TS(3,`center;
l l .')
XX_TR(XX_TD(XX_TC(Attribute))	XX_TD(XX_TC(Values)))
XX_T_
XX_TR(XX_TD(XX_TC(c0 basic 3270))	XX_TD(XX_TC(20 protected)))
XX_TR(XX_TD()	XX_TD(XX_TC(10 numeric)))
XX_TR(XX_TD()	XX_TD(XX_TC(04 detectable)))
XX_TR(XX_TD()	XX_TD(XX_TC(08 intensified)))
XX_TR(XX_TD()	XX_TD(XX_TC(0c non-display)))
XX_TR(XX_TD()	XX_TD(XX_TC(01 modified)))
XX_TR(XX_TD(XX_TC(41 highlighting))	XX_TD(XX_TC(f1 blink)))
XX_TR(XX_TD()	XX_TD(XX_TC(f2 reverse)))
XX_TR(XX_TD()	XX_TD(XX_TC(f4 underscore)))
XX_TR(XX_TD()	XX_TD(XX_TC(f8 intensify)))
XX_TR(XX_TD(XX_TC(42 foreground))	XX_TD(XX_TC(f0 neutral black)))
XX_TR(XX_TD()	XX_TD(XX_TC(f1 blue)))
XX_TR(XX_TD()	XX_TD(XX_TC(f2 red)))
XX_TR(XX_TD()	XX_TD(XX_TC(f3 pink)))
XX_TR(XX_TD()	XX_TD(XX_TC(f4 green)))
XX_TR(XX_TD()	XX_TD(XX_TC(f5 turquoise)))
XX_TR(XX_TD()	XX_TD(XX_TC(f6 yellow)))
XX_TR(XX_TD()	XX_TD(XX_TC(f7 neutral white)))
XX_TR(XX_TD()	XX_TD(XX_TC(f8 black)))
XX_TR(XX_TD()	XX_TD(XX_TC(f9 deep blue)))
XX_TR(XX_TD()	XX_TD(XX_TC(fa orange)))
XX_TR(XX_TD()	XX_TD(XX_TC(fb purple)))
XX_TR(XX_TD()	XX_TD(XX_TC(fc pale green)))
XX_TR(XX_TD()	XX_TD(XX_TC(fd pale turquoise)))
XX_TR(XX_TD()	XX_TD(XX_TC(fe grey)))
XX_TR(XX_TD()	XX_TD(XX_TC(ff white)))
XX_TR(XX_TD(XX_TC(43 character set))	XX_TD(XX_TC(f0 default)))
XX_TR(XX_TD()	XX_TD(XX_TC(f1 APL)))
XX_TR(XX_TD()	XX_TD(XX_TC(f8 DBCS)))
XX_TE()
XX_IP
Extended attributes (which do not take up display positions) are output as
XX_FB(SA`('aa=nn`)'), with XX_FI(aa) and XX_FI(nn) having
the same definitions as above (though the basic 3270 attribute will never
appear as an extended attribute).
XX_IP
In addition, NULL characters in the screen buffer are reported as ASCII
character 00 instead of 20, even though they should be displayed as blanks.
XX_TP(XX_FB(ReadBuffer)(XX_FB(Ebcdic)))
Equivalent to XX_FB(ReadBuffer)(XX_FB(Ascii)), but with the data fields output as
hexadecimal EBCDIC codes instead.
Additionally, if a buffer position has the Graphic Escape attribute, it is
displayed as XX_FB(GE`('XX_FI(xx)`)').
XX_TP(XX_FB(Script)(XX_FI(path)[,arg...]))
Runs a child script, passing it optional command-line arguments.
XX_FI(path) must specify an executable (binary) program: the emulator will
create a new process and execute it. If you simply want the emulator to read
commands from a file, use the XX_FB(Source) action.
XX_TP(XX_FB(Snap))
Equivalent to XX_FB(Snap)(XX_FB(Save)) (see XX_LINK(#save,below)).
XX_TP(XX_FB(Snap)(XX_FB(Ascii),...))
Performs the XX_FB(Ascii) action on the saved screen image.
XX_TP(XX_FB(Snap)(XX_FB(Cols)))
Returns the number of columns in the saved screen image.
XX_TP(XX_FB(Snap)(`XX_FB(Ebcdic),...'))
Performs the XX_FB(Ebcdic) action on the saved screen image.
XX_TP(XX_FB(Snap)(XX_FB(ReadBuffer)))
Performs the XX_FB(ReadBuffer) action on the saved screen image.
XX_TP(XX_FB(Snap(XX_FB(Rows))))
Returns the number of rows in the saved screen image.
XX_TARGET(save)dnl
XX_TP(XX_FB(Snap)(XX_FB(Save)))
Saves a copy of the screen image and status in a temporary buffer.
This copy can be queried with other
XX_FB(Snap)
actions to allow a script to examine a consistent screen image, even when the
host may be changing the image (or even the screen dimensions) dynamically.
XX_TP(XX_FB(Snap)(XX_FB(Status)))
Returns the status line from when the screen was last saved.
XX_TP(XX_FB(Snap)(XX_FB(Wait)[`,'XX_FI(timeout)]`,'XX_FB(Output)))
Pauses the script until the host sends further output, then updates the snap
buffer with the new screen contents.
Used when the host unlocks the keyboard (allowing the script to proceed after
an
XX_FB(Enter),
XX_FB(PF)
or
XX_FB(PA)
action), but has not finished updating the screen.
This action is usually invoked in a loop that uses the
XX_FB(Snap)(XX_FB(Ascii))
or
XX_FB(Snap)(XX_FB(Ebcdic))
action to scan the screen for some pattern that indicates that the host has
fully processed the last command.
XX_IP
The optional XX_FI(timeout) parameter specifies a number of seconds to wait
before failing the XX_FB(Snap) action.  The default is to wait indefinitely.
XX_TP(XX_FB(Source)(XX_FI(file)))
Read and execute commands from XX_FI(file).
Any output from those commands will become the output from XX_FB(Source).
If any of the commands fails, the XX_FB(Source) command will XX_FI(not) abort;
it will continue reading commands until EOF.
XX_TP(XX_FB(Title)(XX_FI(text)))
Changes the
ifelse(XX_PLATFORM,unix,x3270,wc3270)
window title to XX_FI(text).
XX_TP(XX_FB(Transfer)(XX_FI(keyword)=XX_FI(value),...))
Invokes IND$FILE file transfer.
See XX_LINK(#File-Transfer,XX_SM(FILE TRANSFER)) below.
XX_TP(XX_FB(Wait)([XX_FI(timeout)`,'] XX_FB(3270Mode)))
Used when communicating with a host that switches between
XX_SM(NVT) mode and 3270 mode.
Pauses the script or macro until the host negotiates 3270 mode, then waits for
a formatted screen as above.
XX_IP
The optional XX_FI(timeout) parameter specifies a number of seconds to wait
before failing the XX_FB(Wait) action.  The default is to wait indefinitely.
XX_IP
For backwards compatibility,
XX_FB(Wait(3270))
is equivalent to
XX_FB(Wait)(XX_FB(3270Mode))
XX_TP(XX_FB(Wait)([XX_FI(timeout)`,'] XX_FB(Disconnect)))
Pauses the script until the host disconnects.
Often used to after sending a
XX_FI(logoff)
command to a XX_SM(VM/CMS) host, to ensure that the session is not unintentionally
set to
XX_FB(disconnected)
state.
XX_IP
The optional XX_FI(timeout) parameter specifies a number of seconds to wait
before failing the XX_FB(Wait) action.  The default is to wait indefinitely.
XX_TP(XX_FB(Wait)([XX_FI(timeout)`,'] XX_FB(InputField)))
A useful utility for use at the beginning of scripts and after the
XX_FB(Connect) action.
In 3270 mode, waits until the screen is formatted, and the host has positioned
the cursor on a modifiable field.
In XX_SM(NVT) mode, waits until the host sends at least one byte of data.
XX_IP
The optional XX_FI(timeout) parameter specifies a number of seconds to wait
before failing the XX_FB(Wait) action.  The default is to wait indefinitely.
XX_IP
For backwards compatibility,
XX_FB(Wait)
is equivalent to
XX_FB(Wait)(XX_FB(InputField)).
XX_TP(XX_FB(Wait)([XX_FI(timeout)`,'] XX_FB(NVTMode)))
Used when communicating with a host that switches between 3270 mode and
XX_SM(NVT) mode.
Pauses the script or macro until the host negotiates XX_SM(NVT)
mode, then waits for
a byte from the host as above.
XX_IP
The optional XX_FI(timeout) parameter specifies a number of seconds to wait
before failing the XX_FB(Wait) action.  The default is to wait indefinitely.
XX_IP
For backwards compatibility,
XX_FB(Wait)(XX_FB(ansi))
is equivalent to
XX_FB(Wait)(XX_FB(NVTMode)).
XX_TP(XX_FB(Wait)([XX_FI(timeout)`,'] XX_FB(Output)))
Pauses the script until the host sends further output.
Often needed when the host unlocks the keyboard (allowing the script to
proceed after a
XX_FB(Clear),
XX_FB(Enter),
XX_FB(PF)
or
XX_FB(PA)
action), but has not finished updating the screen.
Also used in non-blocking AID mode (see XX_LINK(#Differences,XX_SM(DIFFERENCES))
for details).
This action is usually invoked in a loop that uses the
XX_FB(Ascii)
or
XX_FB(Ebcdic)
action to scan the screen for some pattern that indicates that the host has
fully processed the last command.
XX_IP
The optional XX_FI(timeout) parameter specifies a number of seconds to wait
before failing the XX_FB(Wait) action.  The default is to wait indefinitely.
XX_TP(XX_FB(Wait)([XX_FI(timeout)`,'] XX_FB(Unlock)))
Pauses the script until the host unlocks the keyboard.
This is useful when operating in non-blocking AID mode
(XX_FB(toggle AidWait clear)), to wait for a host command to complete.
See XX_LINK(#Differences,XX_SM(DIFFERENCES)) for details).
XX_IP
The optional XX_FI(timeout) parameter specifies a number of seconds to wait
before failing the XX_FB(Wait) action.  The default is to wait indefinitely.
XX_TP(XX_FB(Wait)(XX_FI(timeout)`,' XX_FB(Seconds)))
Delays the script XX_FI(timeout) seconds.
Unlike the other forms of XX_FB(Wait), the timeout is not optional.
ifelse(XX_PLATFORM,unix,`XX_TP(XX_FB(WindowState)(XX_FI(mode)))
If XX_FI(mode) is XX_FB(Iconic), changes the x3270 window into an icon.
If XX_FI(mode) is XX_FB(Normal), changes the x3270 window from an icon to a
normal window.
')dnl
XX_TPE()dnl
define(XX_action,action)dnl
include(ft.inc)dnl
XX_SH(See Also)
expect(1)
XX_BR
perl(1)
XX_BR
ksh(1)
ifelse(XX_PLATFORM,unix,`XX_BR
XX_LINK(x3270-man.html,x3270(1))
')dnl
XX_BR
ifelse(XX_PLATFORM,unix,`XX_LINK(Unix-x3270if.html,x3270if(1))',`XX_LINK(Windows-x3270if.html,x3270if(1))')
XX_BR
XX_LINK(XX_C3270-man.html,XX_C3270`(1)')
XX_BR
XX_LINK(XX_S3270-man.html,XX_S3270`(1)')
XX_SH(Version)
Version XX_VERSION_NUMBER
