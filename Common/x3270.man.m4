dnl Copyright (c) 1993-2016, Paul Mattes.
dnl Copyright (c) 1990, Jeff Sparkes.
dnl All rights reserved.
dnl 
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are met:
dnl     * Redistributions of source code must retain the above copyright
dnl       notice, this list of conditions and the following disclaimer.
dnl     * Redistributions in binary form must reproduce the above copyright
dnl       notice, this list of conditions and the following disclaimer in the
dnl       documentation and/or other materials provided with the distribution.
dnl     * Neither the names of Paul Mattes, Jeff Sparkes nor the names of their
dnl       contributors may be used to endorse or promote products derived from
dnl       this software without specific prior written permission.
dnl 
dnl THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND JEFF SPARKES "AS IS" AND
dnl ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
dnl IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
dnl ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR JEFF SPARKES BE LIABLE FOR
dnl ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
dnl SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
dnl CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
dnl LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
dnl OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
dnl DAMAGE.
define(XX_KEY,$1`'XX_LT()Key>$2)dnl
define(XX_BTN,$1`'XX_LT()Btn$2>)dnl
define(XX_action,`ifelse(XX_PRODUCT,tcl3270,command,action)')dnl
define(XX_Action,`ifelse(XX_PRODUCT,tcl3270,Command,Action)')dnl
XX_TH(XX_PRODUCT,1,XX_DATE)
XX_SH(Name)
XX_PRODUCT XX_DASH
ifelse(XX_PRODUCT,c3270,`curses-based 
')dnl
XX_SM(IBM) host access tool
XX_SH(Synopsis)
XX_FB(XX_PRODUCT)
ifelse(XX_PRODUCT,tcl3270,`[XX_FI(script)]
')dnl
[XX_FI(options)]
[XX_FI(host)]
ifelse(XX_PRODUCT,tcl3270,`[XX_DASHED(XX_DASH) XX_FI(script-arg)...]
')
XX_BR
XX_FB(XX_PRODUCT) [XX_FI(options)] ifelse(XX_PRODUCT,tcl3270,`[XX_FI(script)] ')XX_FI(session-file).XX_PRODUCT
ifelse(XX_PRODUCT,tcl3270,`[XX_DASHED(XX_DASH) XX_FI(script-arg)...]
')
XX_SH(Description)
XX_FB(XX_PRODUCT) opens a telnet connection to an XX_SM(IBM)
ifelse(XX_PRODUCT,x3270,`host in an X window.',
XX_PRODUCT,s3270,`host, then allows a script to control the host login session.
It is derived from
XX_LINK(x3270-man.html,XX_FI(x3270)(1)),
an X-windows IBM 3270 emulator.',
XX_PRODUCT,ws3270,`host, then allows a script to control the host login session.',
XX_PRODUCT,tcl3270,`host, then allows a tcl script to control the host login
session.
It is derived from
XX_LINK(x3270-man.html,XX_FI(x3270)(1)),
an X-windows IBM 3270 emulator.',
XX_MODE,console,`host in a console window.')
It implements RFCs 2355 (TN3270E), 1576 (TN3270) and 1646 (LU name selection),
and supports IND$FILE file transfer.
ifelse(XX_PRODUCT,x3270,
`The window created by XX_FB(XX_PRODUCT)
can use its own font for displaying characters, so it is a fairly accurate
representation of an XX_SM(IBM) 3278 or 3279.
It is similar to XX_FI(tn3270)(1) except that it is X-based, not curses-based.
')dnl
ifelse(XX_PRODUCT,c3270,
`If the console is capable of displaying colors, then XX_FB(c3270) emulates an
XX_SM(IBM) 3279.  Otherwise, it emulates a 3278.
')dnl
include(hostname.inc)
XX_SH(Options)
ifelse(XX_PRODUCT,x3270,`XX_FB(x3270) is a toolkit based program, so it understands standard Xt options and
resources.
It also understands',` XX_FB(XX_PRODUCT) understands')
the following options:XX_TPS()
XX_TP(XX_FB(XX_DASHED(accepthostname)) XX_FI(spec))
Specifies a particular hostname to accept when validating the name presented
in the host's SSL certificate, instead of comparing to the name or address
used to make the connection.
XX_FI(spec) can either be XX_FB(any), which
disables name validation, XX_FB(DNS:)`'XX_FI(hostname), which matches a
particular DNS hostname, or XX_FB(IP:)`'XX_FI(address), which matches a
particular numeric IPv4 or IPv6 address.
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(activeicon)))
Specifies that the icon should be a miniature version of the screen image.
See XX_LINK(#Icons,XX_SM(ICONS)) below.
')dnl
ifelse(XX_PRODUCT,wc3270,
`XX_TP(XX_FB(XX_DASHED(allbold)))
Forces all characters to be displayed using the XX_DQUOTED(bold) colors
(colors 8 through 15, rather than colors 0 through 7).
This helps with PC console windows in which colors 0 through 7 are
unreadably dim.
All-bold mode is the default for color (3279) emulation, but not for monochrome
(3278) emulation.
')dnl
ifelse(XX_PRODUCT,c3270,
`XX_TP(XX_FB(XX_DASHED(allbold)))
Forces all characters to be displayed in bold.
This helps with PC consoles which display non-bold characters in unreadably
dim colors.
All-bold mode is the default for color displays, but not for monochrome
displays.
XX_TP(XX_FB(XX_DASHED(altscreen) XX_FI(rows)`'XX_FB(x)`'XX_FI(cols)`'XX_FB(=)`'XX_FI(init_string)))
Defines the dimensions and escape sequence for the alternate (132-column)
screen mode.
See XX_LINK(#Screen-Size-Switching,XX_SM(SCREEN SIZE SWITCHING)), below.
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_TP(XX_FB(XX_DASHED(apl)))
Sets up XX_SM(APL) mode.
ifelse(XX_PRODUCT,x3270,`This is actually an abbreviation for several options.
')dnl
See XX_LINK(#APL-Support,XX_SM(APL SUPPORT)) below.
')dnl
XX_TP(XX_FB(XX_DASHED(cadir)) XX_FI(directory))
Specifies a directory containing CA (root) certificates to use when verifying a
certificate provided by the host.
XX_TP(XX_FB(XX_DASHED(cafile)) XX_FI(filename))
Specifies a XX_SM(PEM)-format file containing CA (root) certificates to use
when verifying a certificate provided by the host.
ifelse(XX_PRODUCT,c3270,
`XX_TP(XX_FB(XX_DASHED(cbreak)))
Causes XX_FB(c3270) to operate in XX_FI(cbreak) mode, instead of XX_FI(raw)
mode.
In XX_FI(cbreak) mode, the TTY driver will properly process XOFF and XON
characters, which are required by some terminals for proper operation.
However, those characters (usually ^S and ^Q), as well as the characters for
XX_FB(interrupt), XX_FB(quit), and XX_FB(lnext) (usually ^C, ^XX_BACKSLASH
and ^V respectively) will be seen by XX_FB(c3270) only if preceded by
the XX_FB(lnext) character.
The XX_FB(susp) character (usually ^Z) cannot be seen by XX_FB(c3270) at all.
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(cc)) XX_FI(range):XX_FI(value)[`,'...])
Sets character classes.
XX_HO(`See XX_LINK(#Character-Classes,XX_SM(CHARACTER CLASSES)), below.
')dnl
')dnl
XX_TP(XX_FB(XX_DASHED(certfile)) XX_FI(filename))
Specifies a file containing a certificate to provide to the host, if
requested.
The default file type is XX_SM(PEM).
XX_TP(XX_FB(XX_DASHED(certfiletype)) XX_FI(type))
Specifies the type of the certificate file specified
by XX_FB(XX_DASHED(certfile)).
XX_FI(Type) can be XX_FB(pem) or XX_FB(asn1).
XX_TP(XX_FB(XX_DASHED(chainfile) XX_FI(filename)))
Specifies a certificate chain file in XX_SM(PEM) format, containing a
certificate to provide to the host if requested, as well as one or more
intermediate certificates and the CA certificate used to sign that certificate.
If XX_FB(XX_DASHED(chainfile)) is specified, it
overrides XX_FB(XX_DASHED(certfile)).
XX_TP(XX_FB(XX_DASHED(charset)) XX_FI(name))
Specifies an XX_SM(EBCDIC) host character set.
XX_HO(`See XX_LINK(#Character-Sets,XX_SM(CHARACTER SETS)) below.
')dnl
XX_TP(XX_FB(XX_DASHED(clear)) XX_FI(toggle))
Sets the initial value of XX_FI(toggle) to XX_FB(false).
define(XX_TOGGLEREF,`ifelse(XX_PRODUCT,x3270,Menus,Toggles)')dnl
define(XX_TOGGLEREFNM,`ifelse(XX_PRODUCT,x3270,MENUS,TOGGLES)')dnl
XX_HO(`The list of toggle names is under XX_LINK(`#'XX_TOGGLEREF,XX_SM(XX_TOGGLEREFNM))
below.
')dnl
XX_TP(XX_FB(XX_DASHED(connecttimeout)) XX_FI(seconds))
Specifies the time that XX_PRODUCT will wait for a host connection to
complete.
ifelse(XX_PRODUCT,c3270,
`XX_TP(XX_FB(XX_DASHED(defaultfgbg)))
Causes XX_PRODUCT to use the XX_POSESSIVE(terminal) default foreground color
instead of the curses color XX_FB(black), and the XX_POSESSIVE(terminal)
default background color instead of the curses color XX_FB(white).
This is helpful for emulators such as XX_FI(gnome-terminal) whose
representation of a black background is a murky gray, and for emulators
configured to use black text on a white background.
It is set automatically if the environment variable XX_FB(COLORTERM) is
set to XX_FB(gnome-terminal). It is available only if XX_PRODUCT was
compiled with a version of XX_FI(ncurses) that supports default colors,
if the emulator supports default colors, and if the termcap/terminfo entry
indicates this capability.
')dnl
ifelse(XX_PRODUCT,c3270,
`XX_TP(XX_FB(XX_DASHED(defscreen) XX_FI(rows)`'XX_FB(x)`'XX_FI(cols)`'XX_FB(=)`'XX_FI(init_string)))
Defines the dimensions and escape sequence for the default (80-column)
screen mode.
See XX_LINK(#Screen-Size-Switching,XX_SM(SCREEN SIZE SWITCHING)), below.
')dnl
XX_TP(XX_FB(XX_DASHED(devname)) XX_FI(name))
Specifies a device name (workstation ID) for RFC 4777 support.
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(efont)) XX_FI(name))
Specifies a font for the emulator window.
XX_HO(`See XX_LINK(#Fonts,XX_SM(FONTS)) below.
')dnl
')dnl
ifelse(XX_INTERACTIVE,yes,`XX_TP(XX_FB(XX_DASHED(hostsfile)) XX_FI(file))
Uses XX_FI(file) as the hosts file, which allows aliases for host names and
scripts to be executed at login.
See XX_LINK(ibm_hosts.html,XX_FI(ibm_hosts)(1)) for details.
')dnl
ifelse(XX_PRODUCT,tcl3270,,`XX_TP(XX_FB(XX_DASHED(httpd)) XX_FB(`[')`'XX_FI(addr)`'XX_FB(`:]')`'XX_FI(port))
Specifies a port and optional address to listen on for HTTP connections.
XX_FI(Addr) can be specified as XX_DQUOTED(*) to indicate 0.0.0.0; the
default is 127.0.0.1. IPv6 numeric addresses must be specified inside of
square brackets, e.g., [::1]:4080 to specify the IPv6 loopback address and
TCP port 4080.
XX_IP
Note that this option is mutually-exclusive with the XX_DASHED(scriptport)
option
ifelse(XX_MODE,script,`and disables reading commands from standard input.',.)
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(iconname)) XX_FI(name))
Specifies an alternate title for the program icon.
XX_TP(XX_FB(XX_DASHED(iconx)) XX_FI(x))
Specifies the initial x coordinate for the program icon.
XX_TP(XX_FB(XX_DASHED(icony)) XX_FI(y))
Specifies the initial y coordinate for the program icon.
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(im)) XX_FI(method))
Specifies the name of the input method to use for multi-byte input.
(Supported only when XX_PRODUCT is compiled with DBCS support.)
')dnl
XX_TP(XX_FB(XX_DASHED(keyfile)) XX_FI(filename))
Specifies a file containing the private key for the certificate file
(specified via XX_FB(XX_DASHED(certfile)) or XX_FB(XX_DASHED(chainfile))).
The default file type is XX_SM(PEM).
XX_TP(XX_FB(XX_DASHED(keyfiletype)) XX_FI(type))
Specifies the type of the private key file specified
by XX_FB(XX_DASHED(keyfile)).
XX_FI(Type) can be XX_FB(pem) or XX_FB(asn1).
XX_TP(XX_FB(XX_DASHED(keypasswd)) XX_FI(type):XX_FI(value))
Specifies the password for the private key file, if it is encrypted.
The argument can be XX_FB(file):XX_FI(filename), specifying that the
password is in a file, or XX_FB(string):XX_FI(string), specifying the
password on the command-line directly.
If the private key file is encrypted and no XX_FB(XX_DASHED(keypasswd))
option is given,
ifelse(XX_INTERACTIVE,yes,`the password will be prompted for interactively.',
`secure connections will not be allowed.')
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(keymap)) XX_FI(name))
Specifies a keymap name and optional modifiers.
See XX_LINK(#Keymaps,XX_SM(KEYMAPS)) below.
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TP(XX_FB(XX_DASHED(keymap)) XX_FI(name))
Specifies a keyboard map to be found in the resource
XX_FB(c3270.keymap.)`'XX_FI(name) or the file XX_FI(name).
See XX_LINK(#Keymaps,XX_SM(KEYMAPS)) below for details.
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(keypad)))
Turns on the keypad as soon as XX_FB(XX_PRODUCT) starts.
')dnl
ifelse(XX_PLATFORM,unix,`XX_TP(XX_FB(XX_DASHED(km)) XX_FI(name))
Specifies the local encoding method for multi-byte text.
XX_FI(name) is an encoding name recognized by the ICU library.
(Supported only when XX_PRODUCT is compiled with DBCS support, and necessary
only when XX_PRODUCT cannot figure it out from the locale.)
')dnl
XX_TP(XX_FB(XX_DASHED(loginmacro)) XX_FI(Action(arg...) ...))
Specifies a macro to run at login time.
ifelse(XX_PRODUCT,ws3270,`XX_TP(XX_FB(XX_DASHED(localcp) XX_FI(codepage)))
Specifies the Windows code page to use for local I/O.
The default is to use the XX_POSESSIVE(system) ANSI code page.
')dnl
ifelse(XX_MODE,script,`XX_TP(XX_FB(XX_DASHED(minversion)) XX_FI(version))
The minimum required version of XX_FB(XX_PRODUCT), e.g., XX_VERSION_NUMBER.
If the running version is less than the specified version, XX_FB(XX_PRODUCT)
will abort.
The format of a version is
XX_FI(major)XX_FB(.)XX_FI(minor)XX_FB(type)XX_FI(iteration). XX_FB(type) is
ignored, and XX_FI(minor) and XX_FI(iteration) can be omitted.
')dnl
XX_TP(XX_FB(XX_DASHED(model)) XX_FI(name))
The model of 3270 display to be emulated.
The model name is in two parts, either of which may be omitted:
XX_IP
The first part is the
XX_FB(base model),
which is either XX_FB(3278) or XX_FB(3279).
XX_FB(3278) specifies a monochrome (green on black) 3270 display;
XX_FB(3279) specifies a color 3270 display.
XX_IP
The second part is the
XX_FB(model number),
which specifies the number of rows and columns.
Model 4 is the default.
XX_PP
XX_TS(3,`center;
c c c .')
XX_TR(XX_TD(XX_TC(Model Number))	XX_TD(XX_TC(Columns))	XX_TD(XX_TC(Rows)))
XX_T_
XX_TR(XX_TD(XX_TC(2))	XX_TD(XX_TC(80))	XX_TD(XX_TC(24)))
XX_TR(XX_TD(XX_TC(3))	XX_TD(XX_TC(80))	XX_TD(XX_TC(32)))
XX_TR(XX_TD(XX_TC(4))	XX_TD(XX_TC(80))	XX_TD(XX_TC(43)))
XX_TR(XX_TD(XX_TC(5))	XX_TD(XX_TC(132))	XX_TD(XX_TC(27)))
XX_TE()
XX_IP
Note: Technically, there is no such 3270 display as a 3279-4 or 3279-5, but
most hosts seem to work with them anyway.
XX_IP
The default model
ifelse(XX_PRODUCT,x3270,`for a color X display
is XX_FB(`3279'XX_DASHED(4)).
For a monochrome X display, it is
XX_FB(`3278'XX_DASHED(4)).',
XX_PRODUCT,c3270,`for a color display is XX_FB(`3279'XX_DASHED(4)).
For a monochrome display, it is XX_FB(`3278'XX_DASHED(4)).',
`is XX_FB(`3279'XX_DASHED(4)).')
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(mono)))
Forces XX_FB(XX_PRODUCT) to believe it is running on a monochrome X display.
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TP(XX_FB(XX_DASHED(mono)))
Prevents XX_FB(XX_PRODUCT) from using color, ignoring any color capabilities
reported by the terminal.
XX_TP(XX_FB(XX_DASHED(noprompt)))
An alias for XX_DASHED(secure).
')dnl
XX_TP(XX_FB(XX_DASHED(nvt)))
Start in NVT mode instead of waiting for the host to send data, and make the
default terminal type XX_FB(xterm).
ifelse(XX_PRODUCT,x3270,
XX_TP(XX_FB(XX_DASHED(once)))
Causes XX_FB(XX_PRODUCT) to exit after a host disconnects.
This option has effect only if a hostname is specified on the command line.
)dnl
XX_TP(XX_FB(XX_DASHED(oversize)) XX_FI(cols)`'XX_FB(x)`'XX_FI(rows))
Makes the screen larger than the default for the chosen model number.
This option has effect only in combination with extended data stream support
(controlled by the "XX_PRODUCT.extended" resource), and only if the host
supports the Query Reply structured field.
The number of columns multiplied by the number of rows must not exceed
16383 (3fff hex), the limit of 14-bit 3270 buffer addressing.
ifelse(XX_MODE,console,`XX_IP
It can also be specified as XX_FB(auto), which causes XX_FB(XX_PRODUCT) to fill
the entire terminal or console window.
')dnl
XX_TP(XX_FB(XX_DASHED(port)) XX_FI(n))
Specifies a different XX_SM(TCP) port to connect to.
XX_FI(n) can be a name from XX_FB(/etc/services) like XX_FB(telnet), or a
number.
This option changes the default port number used for all connections.
(The positional parameter affects only the initial connection.)
ifelse(XX_PRODUCT,x3270,
`XX_TP(XX_FB(XX_DASHED(printerlu) XX_FI(luname)))
Causes XX_FB(XX_PRODUCT) to automatically start a XX_FI(pr3287) printer
session.
If XX_FI(luname) is ".", then the printer session will be associated with the
interactive terminal session (this requires that the host support TN3270E).
Otherwise, the value is used as the explicit LU name to associate with the
printer session.
')dnl
XX_TP(XX_FB(XX_DASHED(proxy) XX_FI(type):XX_FI(host)[:XX_FI(port)]))
Causes XX_FB(XX_PRODUCT) to connect via the specified proxy, instead of
using a direct connection.
The XX_FI(host) can be an IP address or hostname.
The optional XX_FI(port) can be a number or a service name.
For a list of supported proxy XX_FI(types), see XX_LINK(#Proxy,XX_SM(PROXY))
below.
ifelse(XX_PRODUCT,c3270,
`XX_TP(XX_FB(XX_DASHED(printerlu) XX_FI(luname)))
Causes XX_FB(XX_PRODUCT) to automatically start a XX_FI(pr3287) printer
session.
If XX_FI(luname) is ".", then the printer session will be associated with the
interactive terminal session (this requires that the host support TN3270E).
Otherwise, the value is used as the explicit LU name to associate with the
printer session.
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_TP(XX_FB(XX_DASHED(pt)) XX_FI(type))
Specifies the preedit type for the multi-byte input method.
Valid values are XX_FB(OverTheSpot), XX_FB(OffTheSpot), XX_FB(Root) and
XX_FB(OnTheSpot).
The value for XX_FB(OverTheSpot) can `include' an optional suffix, a signed
number indicating the vertical distance in rows of the preedit window from the
cursor position, e.g. XX_FB(OverTheSpot+1) or XX_FB(OverTheSpot-2).
The default value is XX_FB(OverTheSpot+1).
(Supported only when XX_PRODUCT is compiled with DBCS support.)
')dnl
ifelse(XX_INTERACTIVE,yes,`XX_TP(XX_FB(XX_DASHED(reconnect)))
Causes XX_FB(XX_PRODUCT)
to automatically reconnect to the host if it ever disconnects.
This option has effect only if a hostname is specified on the command line.
')dnl
ifelse(XX_PRODUCT,c3270,`XX_TP(XX_FB(XX_DASHED(rv)))
Switches XX_PRODUCT from a white-on-black display to a black-on-white
display.
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(XX_DASHED(sb)))
Turns on the scrollbar.
XX_TP(XX_FB(+sb))
Turns the scrollbar off.
XX_TP(XX_FB(XX_DASHED(scheme)) XX_FI(name))
Specifes a color scheme to use in 3279 mode.
This option has effect only in combination with 3279 emulation.
XX_HO(`See XX_LINK(#Color-Schemes,XX_SM(COLOR SCHEMES)) below.
')dnl
XX_TP(XX_FB(XX_DASHED(script)))
Causes
XX_FB(XX_PRODUCT)
to read commands from standard input, with the results written to standard
output.
The protocol for these commands is documented in
XX_LINK(XX_X3270-script.html,XX_FI(XX_X3270-script)(1)).
')dnl 
ifelse(XX_INTERACTIVE,yes,`XX_TP(XX_FB(XX_DASHED(sl)) XX_FI(n))
Specifies that XX_FI(n) lines should be saved for scrolling back.
The default is 4096.
')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TP(XX_FB(XX_DASHED(S)))
Runs XX_PRODUCT in auto-shortcut mode.
XX_PRODUCT will create a temporary shorcut (.LNK file) that matches the
parameters in the session file (model number, characterset, etc.) and re-run
itself from the shortcut.
XX_TP(XX_FB(+S))
Disables auto-shortcut mode.
It is generally a good idea to put this option on the command lines of all
shortcuts, to avoid infinite looping.
')dnl
ifelse(XX_PRODUCT,tcl3270,,
`XX_TP(XX_FB(XX_DASHED(scriptport)) XX_FB(`[')`'XX_FI(addr)`'XX_FB(`:]')`'XX_FI(port))
Specifies a port and optional address to listen on for scripting connections.
XX_FI(Addr) can be specified as XX_DQUOTED(*) to indicate 0.0.0.0; the
default is 127.0.0.1. IPv6 numeric addresses must be specified inside of
square brackets, e.g., [::1]:4081 to specify the IPv6 loopback address and
TCP port 4081.
XX_IP
Note that this option is mutually-exclusive with the XX_DASHED(httpd)
option
ifelse(XX_MODE,script,`and disables reading commands from standard input.',.)
XX_TP(XX_FB(XX_DASHED(scriptportonce)))
Allows XX_PRODUCT to accept only one script connection. When that connection is
broken, XX_PRODUCT will exit.
')dnl
ifelse(XX_PRODUCT,c3270,
`XX_TP(XX_FB(XX_DASHED(secure)))
Disables the interactive XX_FB(c3270>) prompt.
When used, a hostname must be provided on the command line.
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_TP(XX_FB(XX_DASHED(secure)))
Disables run-time features that could compromise system security
(user-specified file names and commands, etc.).
')dnl
XX_TP(XX_FB(XX_DASHED(selfsignedok)))
When verifying a host XX_SM(SSL) certificate, allow it to be self-signed.
XX_TP(XX_FB(XX_DASHED(set)) XX_FI(toggle))
Sets the initial value of XX_FI(toggle) to XX_FB(true).
XX_HO(`The list of toggle names is under XX_LINK(`#'XX_TOGGLEREF,XX_SM(XX_TOGGLEREFNM))
below.
')dnl
ifelse(XX_PRODUCT,tcl3270,,XX_PLATFORM,windows,,
`XX_TP(XX_FB(XX_DASHED(socket)))
Causes the emulator to create a Unix-domain socket when it starts, for use
by script processes to send commands to the emulator.
The socket is named XX_FB(/tmp/x3sck.)`'XX_FI(pid).
')dnl
The XX_FB(XX_DASHED(p)) option of XX_FI(x3270if) causes it to use this socket,
instead of pipes specified by environment variables.
ifelse(XX_PRODUCT,wc3270,`XX_TP(XX_FB(XX_DASHED(title)) XX_FI(text))
Sets the console window title to XX_FI(text), overriding the automatic
setting of the hostname and the string XX_FB(wc3270).
')dnl
XX_TP(XX_TARGET(tn)XX_FB(XX_DASHED(tn)) XX_FI(name))
Specifies the terminal name to be transmitted over the telnet connection.
The default name is
XX_FB(`IBM'XX_DASH)`'XX_FI(model_name)`'XX_FB(XX_DASHED(E)),
for example,
ifelse(XX_PRODUCT,x3270,`XX_FB(`IBM'XX_DASHED(3279)XX_DASHED(4)XX_DASHED(E))
for a color ifelse(XX_PRODUCT,x3270,`X ')display, or
XX_FB(`IBM'XX_DASHED(3278)XX_DASHED(4)XX_DASHED(E))
for a monochrome ifelse(XX_PRODUCT,x3270,`X ')display.',
XX_PRODUCT,c3270,`XX_FB(`IBM'XX_DASHED(3279)XX_DASHED(4)XX_DASHED(E)) 
for a color ifelse(XX_PRODUCT,x3270,`X ')display, or
XX_FB(`IBM'XX_DASHED(3278)XX_DASHED(4)XX_DASHED(E))
for a monochrome ifelse(XX_PRODUCT,x3270,`X ')display.',
`XX_FB(`IBM'XX_DASHED(3278)XX_DASHED(4)XX_DASHED(E)).')
XX_IP
Some hosts are confused by the XX_FB(XX_DASHED(E))
suffix on the terminal name, and will ignore the extra screen area on
models 3, 4 and 5.
Prepending an XX_FB(s:) on the hostname, or setting the "XX_PRODUCT.extended"
resource to "false", removes the XX_FB(XX_DASHED(E))
from the terminal name when connecting to such hosts.
XX_IP
The name can also be specified with the "XX_PRODUCT.termName" resource.
XX_TP(XX_FB(XX_DASHED(trace)))
Turns on data stream ifelse(XX_PRODUCT,x3270,,`and event ')tracing at startup.
ifelse(XX_PRODUCT,x3270,`Unlike turning it on from a menu option,
there is no pop-up to confirm the file name, which defaults to',
`The default trace file name is')
ifelse(XX_PRODUCT,wc3270,`XX_FB(x3trc.)`'XX_FI(pid)XX_FB(.txt) on the
current XX_POSESSIVE(user) Desktop',XX_PRODUCT,ws3270,`XX_FB(x3trc.)`'XX_FI(pid)XX_FB(.txt) in the current directory',`XX_FB(/tmp/x3trc)').
XX_TP(XX_FB(XX_DASHED(tracefile)) XX_FI(file))
Specifies a file to save data stream and event traces into.
If the name starts with XX_DQUOTED(>>), data will be appended to the file.
ifelse(XX_PRODUCT,x3270,`If the value XX_FB(stdout)
is given, then traces will be written to standard output.
If the value XX_FB(none)
is given, then traces will be piped directly to the monitor window, and no
file will be created.
')dnl
XX_TP(XX_FB(XX_DASHED(tracefilesize)) XX_FI(size))
Places a limit on the size of a trace file.
If this option is not specified, or is specified as XX_FB(0) or XX_FB(none),
the trace file size will be unlimited.
The minimum size is 64 Kbytes.
The value of XX_FI(size) can have a XX_FB(K) or XX_FB(M) suffix, indicating
kilobytes or megabytes respectively.
When the trace file reaches the size limit, it will be renamed with a 
XX_DQUOTED(-) appended and a new file started.
XX_TP(XX_FB(XX_DASHED(user)) XX_FI(name))
Specifies the user name for RFC 4777 support.
ifelse(XX_MODE,script,`XX_TP(XX_FB(XX_DASHED(utf8)))
Forces the local codeset to be UTF-8, ignoring the locale or Windows codepage.
')dnl
XX_TP(XX_FB(XX_DASHED(v)))
Display the version and build options for XX_FB(XX_PRODUCT) and exit.
XX_TP(XX_FB(XX_DASHED(verifycert)))
For SSL or SSL/TLS connections, verify the host certificate, and do not allow
the connection to complete unless it can be validated.
ifelse(XX_PRODUCT,x3270,,
`XX_TP(XX_FB(XX_DASHED(xrm)) "XX_PRODUCT.XX_FI(resource): XX_FI(value)")
Sets the value of the named XX_FI(resource) to XX_FI(value).
Resources control less common XX_FB(XX_PRODUCT)
options, and are defined under XX_LINK(#Resources,XX_SM(RESOURCES)) below.
')dnl
ifelse(XX_PRODUCT,tcl3270,
`XX_TP(XX_FB(XX_DASHED()XX_DASHED()))
Terminates the list of XX_FB(tcl3270) options.
Whatever follows will be available to the script in the XX_FB($argv)
tcl variable.
')dnl
XX_TPE()dnl
ifelse(XX_PRODUCT,x3270,`XX_LP
After reading resource definitions from the X server
and any standandard X11 resource definition files
(XX_FB($HOME/.Xdefaults), etc.), XX_FB(XX_PRODUCT) will read definitions
from the file XX_FB($HOME/.x3270pro).
This file contains local customizations and is also used to save changed
options by the XX_FB(Save Changed Options in File) menu option.
XX_LP
Note that XX_FB(XX_DASHED(xrm)) options override any definitions in
the XX_FB(.x3270pro) file.
')dnl
XX_HO(`ifelse(XX_PRODUCT,x3270,`XX_SH(Fonts)
XX_FB(XX_PRODUCT) does not use the "*font" resource for its main
window.
Instead, it uses a custom 14-point font called
XX_FB(3270),
which is a close
approximation of a real 3270 display and allows XX_FB(XX_PRODUCT)
to display the XX_SM(ISO) `8859'XX_DASHED(1) (`Latin'XX_DASH()1)
character set and special status-line symbols.
A more compact font, XX_FB(`3270'XX_DASHED(12)), is also supported, as are the
various sized fonts XX_FB(3270gt8), XX_FB(3270gt12), XX_FB(3270gt16),
XX_FB(3270-20), XX_FB(3270gt24), and XX_FB(3270gt32).
The fonts XX_FB(3270h) and XX_FB(3270gr) are also included to allow display of
Hebrew and Greek text, respectively.
XX_LP
The font may be specified with the XX_FB(XX_DASHED(efont))
option or the "XX_PRODUCT.emulatorFont" resource.
XX_LP
XX_FB(XX_PRODUCT) can also use any X11 font that implements the
display character set required by the host XX_SM(EBCDIC) character set.
XX_PP
An additional font,
XX_FB(3270d),
is supplied.
This font is identical to the default XX_FB(3270)
font, except that it has bitmaps defined for field attribute characters.
This means that field attributes, which are normally displayed as blanks,
are now visible on the screen.
The characters displayed are hexadecimal codes, which can be translated
using a document provided with the XX_FB(XX_PRODUCT) sources.
XX_LP
The font can be changed at any time through a menu option.
It can also be implicitly changed by changing the size of the XX_FB(XX_PRODUCT)
window with the mouse: if the window is made larger, XX_PRODUCT will try to
change to a larger font, and vice-versa.
')dnl
ifelse(XX_MODE,console,`XX_SH(Modes)
XX_FB(XX_PRODUCT) has two basic modes: XX_FI(command-prompt) and XX_FI(session).
XX_PP
XX_FI(Command-prompt) mode is where the XX_FB(XX_PRODUCT`'>) prompt is
displayed.
Interactive commands can be entered at this prompt, to connect to a host,
disconnect from a host, transfer files, display statistics, exit
XX_FB(XX_PRODUCT), etc.
The complete list of interactive commands is listed under
XX_LINK(#Actions,XX_SM(ACTIONS)).
XX_PP
XX_FI(Session) mode is where the emulated 3270 screen is displayed;
keyboard commands cause the display buffer to be modified or data to be
sent to the host.
XX_PP
To switch from display mode to command-prompt mode, press
ifelse(XX_PRODUCT,c3270,`Ctrl-]',`the Escape key').
To switch from command-prompt mode to display mode, press XX_FB(Enter)
(without entering a command) at the XX_FB(XX_PRODUCT`'>) prompt.
')dnl
XX_SH(Character Sets)
The XX_FB(XX_DASHED(charset))
option or the "XX_PRODUCT.charset" resource controls the XX_SM(EBCDIC)
host character set used by XX_FB(XX_PRODUCT).
Available sets `include':
XX_PP
define(XX_CG1A,ifelse(XX_PRODUCT,x3270,3270cg-1a))dnl
define(XX_CG1,ifelse(XX_PRODUCT,x3270,3270cg-1))dnl
define(XX_CG7,ifelse(XX_PRODUCT,x3270,3270cg-7))dnl
define(XX_CG8,ifelse(XX_PRODUCT,x3270,3270cg-8))dnl
define(XX_CG9,ifelse(XX_PRODUCT,x3270,3270cg-9))dnl
define(XX_CG11,ifelse(XX_PRODUCT,x3270,3270cg-11))dnl
define(XX_CG15A,ifelse(XX_PRODUCT,x3270,3270cg-15a))dnl
define(XX_CG15,ifelse(XX_PRODUCT,x3270,3270cg-15))dnl
define(XX_88591,ifelse(XX_PLATFORM,windows,`',iso8859-1))dnl
define(XX_88592,ifelse(XX_PLATFORM,windows,`',iso8859-2))dnl
define(XX_88597,ifelse(XX_PLATFORM,windows,`',iso8859-7))dnl
define(XX_88598,ifelse(XX_PLATFORM,windows,`',iso8859-8))dnl
define(XX_88599,ifelse(XX_PLATFORM,windows,`',iso8859-9))dnl
define(XX_885911,ifelse(XX_PLATFORM,windows,`',iso8859-11))dnl
define(XX_885915,ifelse(XX_PLATFORM,windows,`',iso8859-15))dnl
define(XX_KOI8R,ifelse(XX_PLATFORM,windows,`',koi8-r))dnl
define(XX_TIS,ifelse(XX_PLATFORM,windows,`',tis620.2529-0))dnl
define(XX_GB,ifelse(XX_PLATFORM,windows,`',XX_CG1A XX_88591 + gb2312.1980-0))dnl
define(XX_GBX,ifelse(XX_PLATFORM,windows,`',XX_CG1A XX_88591 + iso10646-1))dnl
define(XX_BIG5,ifelse(XX_PLATFORM,windows,`',XX_CG1A XX_88591 + Big5-0))dnl
define(XX_JIS,ifelse(XX_PLATFORM,windows,`',jisx0201.1976-0 + jisx0208.1983-0))dnl
XX_TS(3,`center;
l l l
lfB l l.')
XX_TR(XX_TD(XX_TC(Charset Name))	XX_TD(XX_TC(Host Code Page))	XX_TD(XX_TC(ifelse(XX_PRODUCT,x3270,Display Character Sets,XX_PRODUCT,c3270,Display Character Set,XX_PLATFORM,windows,`',Character Set))))
XX_T_()
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TD(XX_TC(apl))	XX_TD(XX_TC(037))	XX_TD(XX_TC(XX_CG1A)))
')dnl
XX_TR(XX_TD(XX_TC(belgian))	XX_TD(XX_TC(500))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(belgian-euro))	XX_TD(XX_TC(1148))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(bracket))	XX_TD(XX_TC(037))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(brazilian))	XX_TD(XX_TC(275))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(chinese-gb18030))	XX_TD(XX_TC(1388))	XX_TD(XX_TC(XX_GBX)))
XX_TR(XX_TD(XX_TC(cp1047))	XX_TD(XX_TC(1047))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(cp870))	XX_TD(XX_TC(870))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88592)))
XX_TR(XX_TD(XX_TC(finnish))	XX_TD(XX_TC(278))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(finnish-euro))	XX_TD(XX_TC(1143))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(french))	XX_TD(XX_TC(297))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(french-euro))	XX_TD(XX_TC(1147))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(german))	XX_TD(XX_TC(273))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(german-euro))	XX_TD(XX_TC(1141))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(greek))	XX_TD(XX_TC(423))	XX_TD(XX_TC(XX_CG7 XX_88597)))
XX_TR(XX_TD(XX_TC(hebrew))	XX_TD(XX_TC(424))	XX_TD(XX_TC(XX_CG8 XX_88598)))
XX_TR(XX_TD(XX_TC(icelandic))	XX_TD(XX_TC(871))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(icelandic-euro))	XX_TD(XX_TC(1149))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(italian))	XX_TD(XX_TC(280))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(italian-euro))	XX_TD(XX_TC(1144))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(japanese-kana))	XX_TD(XX_TC(930))	XX_TD(XX_TC(XX_JIS)))
XX_TR(XX_TD(XX_TC(japanese-latin))	XX_TD(XX_TC(939))	XX_TD(XX_TC(XX_JIS)))
XX_TR(XX_TD(XX_TC(norwegian))	XX_TD(XX_TC(277))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(norwegian-euro))	XX_TD(XX_TC(1142))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(russian))	XX_TD(XX_TC(880))	XX_TD(XX_TC(XX_KOI8R)))
XX_TR(XX_TD(XX_TC(simplified-chinese))	XX_TD(XX_TC(935))	XX_TD(XX_TC(XX_GB)))
XX_TR(XX_TD(XX_TC(slovenian))	XX_TD(XX_TC(870))	XX_TD(XX_TC(XX_88592)))
XX_TR(XX_TD(XX_TC(spanish))	XX_TD(XX_TC(284))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(spanish-euro))	XX_TD(XX_TC(1145))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(swedish))	XX_TD(XX_TC(278))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(swedish-euro))	XX_TD(XX_TC(1143))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(thai))	XX_TD(XX_TC(1160))	XX_TD(XX_TC(XX_885911 XX_TIS)))
XX_TR(XX_TD(XX_TC(traditional-chinese))	XX_TD(XX_TC(937))	XX_TD(XX_TC(XX_BIG5)))
XX_TR(XX_TD(XX_TC(turkish))	XX_TD(XX_TC(1026))	XX_TD(XX_TC(XX_88599)))
XX_TR(XX_TD(XX_TC(uk))	XX_TD(XX_TC(285))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TR(XX_TD(XX_TC(uk-euro))	XX_TD(XX_TC(1146))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(us-euro))	XX_TD(XX_TC(1140))	XX_TD(XX_TC(XX_CG15A XX_CG15 XX_885915)))
XX_TR(XX_TD(XX_TC(us-intl))	XX_TD(XX_TC(037))	XX_TD(XX_TC(XX_CG1A XX_CG1 XX_88591)))
XX_TE()
XX_PP
The default character set is
XX_FB(bracket),
which is useful for common U.S. XX_SM(IBM) hosts which use XX_SM(EBCDIC)
codes AD and BD for the XX_DQUOTED([) and XX_DQUOTED(]) characters,
respectively.
XX_PP
Note that any of the host code pages listed above can be specified by adding
XX_FB(cp) to the host code page, e.g., XX_FB(cp037) for host code page 037.
Also note that the code pages available for a given version of XX_FB(XX_PRODUCT)
are displayed by the XX_FB(XX_DASHED(v)) command-line option.
ifelse(XX_PRODUCT,wc3270,`XX_PP
Note that DBCS character sets (Chinese, Japanese) display properly only on
32-bit Windows XP.  Work is proceeding on other platforms.
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_PP
Most 3270 fonts implement the 3270cg-1 display
character set, which is a reordered version of
the XX_SM(ISO) `8859'XX_DASHED(1) character set.
Some implement the 3270cg-1a display character set, which is a superset
of 3270cg-1 that includes APL2 characters.
3270h and 3270gr implement special character sets for Hebrew
and Greek, respectively.
XX_PP
You can also specify national-language translations for your keyboard;
see XX_LINK(#Keymaps,XX_SM(KEYMAPS)) below.
')dnl
ifelse(XX_PRODUCT,x3270,`XX_SH(Character Classes)
XX_FB(XX_PRODUCT) supports character classes (groupings of characters chosen
with a double mouse click) in the same manner as XX_FI(xterm)(1).
The "XX_PRODUCT.charClass" resource or the
XX_FB(XX_DASHED(cc)) option can be used to alter the character class table.
The default table is the same as
XX_POSESSIVE(XX_FI(xterm));
It groups letters together, and puts most punctuation characters in individual
classes.
To put all non-whitespace characters together in the same class (and
duplicate the behavior of some early versions of
XX_FB(XX_PRODUCT),
use the following value:
XX_PP
XX_RS(`33-127:48,161-255:48')
XX_PP
See XX_FI(xterm)(1) for further syntax details.
XX_SH(Keypad)
A keypad may optionally be displayed, with a mouse-clickable button for each
3270 function key (these functions are also available from the keyboard).
The keypad can be turned on and off by clicking on the "keypad" button in the
upper-right-hand corner of the window.
The "XX_PRODUCT.keypad" resource controls where it is displayed.
Options are:
XX_PP
XX_TS(2,center;
l l.)
XX_TR(`XX_TD(left)	XX_TD(`in a separate window, to the left of the screen')')
XX_TR(`XX_TD(right)	XX_TD(`in a separate window, to the right of the screen')')
XX_TR(`XX_TD(bottom)	XX_TD(`in a separate window, below the screen')')
XX_TR(`XX_TD(integral)	XX_TD(`in the same window as the screen, below it')')
XX_TE()
XX_PP
The default is XX_FB(right).
XX_PP
If the "XX_PRODUCT.keypadOn" resource is set to
XX_FB(true),
the keypad will be displayed at startup.
')dnl
ifelse(XX_MODE,console,`XX_SH(Menu Bar and Keypad)
XX_FB(XX_PRODUCT) supports a menu bar and pop-up keypad.
The menu bar allows common functions to be executed without needing to switch
to the XX_FB(XX_PRODUCT>) prompt.
It is available by pressing Alt-N, or if the console supports a mouse, by
clicking on the menu titles displayed at the top of the screen.
XX_LP
The on-screen menu title bar can be turned off via the "XX_PRODUCT.menuBar"
resource.
XX_LP
The pop-up keypad allows the 3270-specific keys (XX_SM(PF) keys, XX_SM(PA)
keys, Clear,
Reset, etc.) to be invoked without memorizing their key mappings or switching
to the XX_FB(XX_PRODUCT>) prompt.
The keypad can be popped up by pressing Alt-K, or can be invoked via a menu
option.
')dnl
ifelse(XX_MODE,script,,XX_PRODUCT,tcl3270,,XX_PLATFORM,windows,,
`XX_SH(Hosts Database)
XX_FB(XX_PRODUCT) uses the XX_FI(ibm_hosts) database to
ifelse(XX_PRODUCT,x3270,`construct a pull-down menu of hosts to connect to.
It also allows host name aliases to be defined, as well as specifying
',XX_PRODUCT,c3270,``define' aliases for host names, and to specify
')dnl
macros to be executed when a connection is first made.
See XX_LINK(ibm_hosts.html,XX_FI(ibm_hosts)(5)) for details.
XX_LP
You may specify a different XX_FI(ibm_hosts)
database with the "XX_PRODUCT.hostsFile" resource.
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_SH(Color Schemes)
When emulating a 3279 display, the X colors used to draw the display
are selected by two resources: the "XX_PRODUCT.colorScheme" resource, which gives
the name of the color scheme to use, and the
individual "XX_PRODUCT.colorScheme.XX_FI(xxx)" resources, which
give the actual definitions.
The color scheme resources are documented in the XX_FB(Resources)
file with the XX_FB(XX_PRODUCT) source.
XX_LP
The color scheme may also be changed while XX_FB(XX_PRODUCT)
is running with a selection from the XX_FB(Options) menu.
')dnl
XX_SH(NVT Mode)
Some hosts use an XX_SM(ASCII) front-end to do initial login negotiation,
then later switch to 3270 mode.
XX_FB(XX_PRODUCT) will emulate an XX_SM(ANSI) X3.64 terminal until the host
places it in 3270 mode (telnet XX_SM(BINARY) and XX_SM(SEND EOR) modes, or
XX_SM(TN3270E) mode negotiation).
ifelse(XX_PRODUCT,x3270,`The emulation is fairly complete; however, it is
not intended to make XX_FB(XX_PRODUCT) a replacement for XX_FI(xterm)(1).
')dnl
XX_PP
If the host later negotiates to stop functioning in 3270 mode,
XX_FB(XX_PRODUCT) will return to XX_SM(NVT) emulation.
XX_PP
In XX_SM(NVT) mode, XX_FB(XX_PRODUCT)
supports both character-at-a-time mode and line mode operation.
You may select the mode with a menu option.
When in line mode, the special characters and operational characteristics are
defined by resources:
XX_PP
XX_TS(3,center;
l c c.)
XX_TR(XX_TD(Mode/Character)	XX_TD(Resource)	XX_TD(Default))
XX_T_()
XX_TR(XX_TD(Translate CR to NL)	XX_TD(XX_PRODUCT.icrnl)	XX_TD(true))
XX_TR(XX_TD(Translate NL to CR)	XX_TD(XX_PRODUCT.inlcr)	XX_TD(false))
XX_TR(XX_TD(Erase previous character)	XX_TD(XX_PRODUCT.erase)	XX_TD(^?))
XX_TR(XX_TD(Erase entire line)	XX_TD(XX_PRODUCT.kill)	XX_TD(^U))
XX_TR(XX_TD(Erase previous word)	XX_TD(XX_PRODUCT.werase)	XX_TD(^W))
XX_TR(XX_TD(Redisplay line)	XX_TD(XX_PRODUCT.rprnt)	XX_TD(^R))
XX_TR(XX_TD(Ignore special meaning of next character)	XX_TD(XX_PRODUCT.lnext)	XX_TD(^V))
XX_TR(XX_TD(Interrupt)	XX_TD(XX_PRODUCT.intr)	XX_TD(^C))
XX_TR(XX_TD(Quit)	XX_TD(XX_PRODUCT.quit)	XX_TD(^XX_BS()))
XX_TR(XX_TD(End of file)	XX_TD(XX_PRODUCT.eof)	XX_TD(^D))
XX_TE()
ifelse(XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,tcl3270,,
`XX_LP
Separate keymaps can be defined for use only when XX_FB(XX_PRODUCT) is in
3270 mode or XX_SM(NVT) mode.
See XX_LINK(#Keymaps,XX_SM(KEYMAPS)) for details.
')dnl
ifelse(XX_PRODUCT,x3270,`include(menus.inc)',`XX_SH(Toggles)
XX_FB(XX_PRODUCT) has a number of configurable modes which may be selected by
the XX_FB(XX_DASHED(set)) and XX_FB(XX_DASHED(clear)) options.
These names can also be used as the first parameter to the XX_FB(Toggle)
action, and are the names of resources that can be used to set or clear
the value of each toggle at start-up.
XX_TPS()dnl
XX_TP(XX_FB(aidWait))
Changes the behavior of actions that send an XX_SM(AID) to the
host (XX_FB(Enter),
XX_FB(Clear), XX_FB(PA) and XX_FB(PF)).
When set, these actions no longer block until the host unlocks the keyboard.
It is up to the script to poll the prompt for the unlocked state, or to use
the XX_FB(Wait(Unlock)) action to wait for the unlock.
ifelse(XX_INTERACTIVE,yes,`XX_TP(XX_FB(altCursor))
If set, the cursor will be an underline. If clear, it will be a solid block.
')dnl
XX_TP(XX_FB(blankFill))
If set, XX_FB(XX_PRODUCT) modifies interactive 3270 behavior in two ways.
First, when a character is typed into a field, all nulls in the field to the
left of that character are changed to blanks.
This eliminates a common 3270 data-entry surprise.
Second, in insert mode, trailing blanks in a field are treated like nulls,
eliminating the annoying XX_DQUOTED(lock-up) that often occurs when inserting
into an field with (apparent) space at the end.
ifelse(XX_MODE,console,`XX_TP(XX_FB(crosshair))
When set, XX_PRODUCT will display a crosshair to help locate the cursor on the
screen.
')dnl
XX_TP(XX_FB(lineWrap))
If set, the XX_SM(NVT) terminal emulator automatically assumes
a XX_SM(NEWLINE) character when it reaches the end of a line.
ifelse(XX_PRODUCT,wc3270,`XX_TP(XX_FB(marginedPaste))
If set, pasting multi-line input via the XX_FB(Paste) action will maintain a
left margin (it will not move the cursor further left than where the paste
begins).
')dnl
ifelse(XX_INTERACTIVE,yes,`XX_TP(XX_FB(monoCase))
If set, XX_FB(XX_PRODUCT) operates in uppercase-only mode.
')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TP(XX_FB(overlayPaste))
If set, pasting over a protected field will simply increment the cursor
position instead of locking the keyboard.
This allows forms to be copied and pasted with the protected fields
included.
Setting this toggle also implicitly sets the XX_FB(marginedPaste) toggle.
')dnl
XX_TP(XX_FB(screenTrace))
Turns on screen tracing at start-up.
Each time the screen changes, its contents are appended to the file
ifelse(XX_PRODUCT,wc3270,`XX_FB(x3scr.)`'XX_FI(pid)`'XX_FB(.txt)
on the current XX_POSESSIVE(user) desktop',ws3270,`XX_FB(x3scr.)`'XX_FI(pid)`'XX_FB(.txt) in the current directory',`XX_FB(/tmp/x3scr.)`'XX_FI(pid)').
ifelse(XX_MODE,console,`XX_TP(XX_FB(showTiming))
If set, the time taken by the host to process an XX_SM(AID) is displayed on
the status line.
')dnl
XX_TP(XX_FB(trace))
Turns on data stream and event tracing at start-up.
Network traffic (both a hexadecimal representation and its
interpretation) is logged to the file
ifelse(XX_PRODUCT,wc3270,`XX_FB(x3trc.)`'XX_FI(pid)`'XX_FB(.txt)
on the current XX_POSESSIVE(user) desktop',ws3270,`XX_FB(x3trc.)`'XX_FI(pid)`'XX_FB(.txt) in the wc3270 AppData
directory',`XX_FB(/tmp/x3trc.)`'XX_FI(pid)').
The directory for the trace file can be changed with
the "XX_PRODUCT.traceDir" resource.
Script commands are also traced.
ifelse(XX_MODE,console,`XX_TP(XX_FB(underscore))
If set, XX_PRODUCT will display underlined fields by substituting
underscore XX_DQUOTED(_) characters for blanks or nulls in the field.
Otherwise, these fields will be displayed
ifelse(XX_PRODUCT,c3270,`using the XX_POSESSIVE(terminal) native
underlining mode, if one is defined.
',`with a highlighted background.
Note that setting XX_FB(underscore) also disables the highlighted background
for blinking fields.
XX_FB(underscore) is set by default.
')dnl
')dnl
ifelse(XX_MODE,console,`XX_TP(XX_FB(visibleControl))
If set, control characters (NULLs, SI/SO and field attributes), which are
usually displayed as blanks, are visible on the display.
NULs become periods, SO becomes XX_DQUOTED(XX_LT()),
SI becomes XX_DQUOTED(>).
Field attributes are mapped onto the characters 0 through 9 and A through V
and are displayed in
ifelse(c3270,`underlined',wc3270,`reverse-video')
yellow.
Field attribute mappings are part of the XX_FB(XX_PRODUCT) Resources
documentation for the XX_FB(visibleControl) resource.
')dnl
XX_TPE()dnl
XX_LP
')dnl
')dnl
ifelse(XX_INTERACTIVE,yes,`XX_SH(Status Line)
ifelse(XX_PRODUCT,c3270,`If the terminal that XX_FB(XX_PRODUCT) is running on
has at least one more row that the 3270 model requires (e.g., 25 rows for a
model 2), XX_FB(XX_PRODUCT) will display a status line.
')dnl
The XX_FB(XX_PRODUCT) status line contains a variety of information.
From left to right, the fields are:
XX_TPS()dnl
XX_TP(XX_FB(comm status))
The first symbol is always a XX_FB(4).
If XX_FB(XX_PRODUCT) is in TN3270E mode, the second symbol is a XX_FB(B);
otherwise it is an XX_FB(A).
ifelse(XX_PRODUCT,x3270,`If XX_FB(x3270) is disconnected, the third symbol
is a question mark.  Otherwise, if ',`If ')
XX_FB(XX_PRODUCT) is in SSCP-LU mode, the third symbol is an XX_FB(S).
Otherwise it is blank.
XX_TP(XX_FB(keyboard lock))
If the keyboard is locked, an "X" symbol and a message field indicate the
reason for the keyboard lock.
ifelse(XX_PRODUCT,x3270,`XX_TP(`XX_FB(`shift')')
Three characters indicate the keyboard modifier status.
"M" indicates the Meta key, "A" the Alt key, and an up-arrow or "^"
indicates the Shift key.
XX_TP(XX_FB(compose))
The letter "C" indicates that a composite character is in progress.
If another symbol follows the "C", it is the first character of the
composite.
')dnl
XX_TP(XX_FB(typeahead))
The letter "T" indicates that one or more keystrokes are in the typeahead
buffer.
XX_TP(XX_FB(temporary keymap))
The letter "K" indicates that a temporary keymap is in effect.
XX_TP(XX_FB(reverse))
The letter "R" indicates that the keyboard is in reverse field entry mode.
XX_TP(XX_FB(insert mode))
ifelse(XX_PRODUCT,x3270,`A thick caret "^" or the ',
`The ')
letter "I" indicates that the keyboard is in insert mode.
XX_TP(XX_FB(printer session))
The letter "P" indicates that a XX_FI(pr3287) session is active.
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(script))
The letter "S" indicates that a script is active.
')dnl
ifelse(XX_PRODUCT,x3270,,`XX_TP(XX_FB(secure connection))
A green letter "S" indicates that the connection is secured via SSL/TLS.
')dnl
XX_TP(XX_FB(LU name))
The LU name associated with the session, if there is one.
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(timing))
A clock symbol and a time in seconds indicate the time it took to process
the last XX_SM(AID) or the time to connect to a host.
This display is optional.
')dnl
XX_TP(XX_FB(cursor position))
The cursor row and column are optionally displayed, zero padded and separated
by a "/".
Location 001/001 is at the upper left, which is different from the row and
columns parameters used with various actions, where the upper left corner is
row 0, column 0.
XX_TPE()dnl
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_SH(Icons)
If the XX_FB(XX_DASHED(activeicon))
option is given (or the "XX_PRODUCT.activeIcon" resource is set to
XX_FB(true)`)',
XX_FB(XX_PRODUCT) will attempt to make its icon a miniature version of the
current screen image.
This function is highly dependent on your window manager:
XX_TPS()dnl
XX_TP(XX_FB(mwm))
The size of the icon is limited by the "Mwm.iconImageMaximum" resource, which
defaults to XX_FB(50x50).
The image will be clipped at the bottom and right.
The icon cannot accept keyboard input.
XX_TP(XX_FB(olwm))
The full screen image of all 3270 models can be displayed on the icon.
However, the icon cannot be resized, so if the model is later changed with an
XX_FB(XX_PRODUCT) menu option, the icon image will be corrupted.
The icon cannot accept keyboard input.
XX_TP(XX_FB(twm) and XX_FB(tvtwm))
The full screen image of all 3270 models can be displayed on the icon, and the
icon can be resized.
The icon can accept keyboard input.
XX_IP
However, XX_FB(twm) does not put labels on application-supplied icon windows.
You can have XX_FB(XX_PRODUCT)
add its own label to the icon by setting the "XX_PRODUCT.labelIcon" resource to
XX_FB(true).
The default font for icon labels is
XX_FB(8x13);
you may change it with the "XX_PRODUCT.iconLabelFont" resource.
XX_TPE()dnl
include(xkeymaps.inc)')
define(XX_LPAREN,`ifelse(XX_PRODUCT,tcl3270,` ',`(')')dnl
define(XX_RPAREN,`ifelse(XX_PRODUCT,tcl3270,,`)')')dnl
define(XX_COMMA,`ifelse(XX_PRODUCT,tcl3270,` ',`, ')')dnl
define(XX_SPACE,`ifelse(XX_PRODUCT,tcl3270,` ',`')')dnl
define(XX_WAIT,`ifelse(XX_PRODUCT,tcl3270,`Wait [XX_FI(timeout)] $1',
`Wait$1(XX_FI(timeout))')')dnl
XX_TARGET(actions)dnl
ifelse(XX_PRODUCT,x3270,,
XX_PRODUCT,tcl3270,
`XX_SH(Commands)
XX_FB(XX_PRODUCT) supports the following additional tcl commands:
',
`XX_SH(Actions)
Here is a complete list of basic XX_PRODUCT actions.
Script-specific actions are described on the
XX_LINK(XX_X3270-script.html,XX_FI(XX_X3270-script)(1)) manual page.
')dnl
define(XX_BLOCK,*))dnl
XX_PP
Actions marked with an asterisk (*) may block, sending data to the host and
possibly waiting for a response.
XX_PP
XX_TS(2,center; lw(3i) lw(3i).)
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,c3270,,XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,wc3270,,`XX_TR(XX_TDH(`Ascii'`')	XX_TD(return entire screen contents as text))
XX_TR(XX_TDH(`Ascii'`'XX_LPAREN`'XX_FI(length)`'XX_RPAREN`')	XX_TD(return screen contents at cursor as text))
XX_TR(XX_TDH(`Ascii'`'XX_LPAREN`'XX_FI(row)`'XX_COMMA`'XX_FI(col)`'XX_COMMA`'XX_FI(length)`'XX_RPAREN)	XX_TD(return screen contents as text))
XX_TR(XX_TDH(`Ascii'`'XX_LPAREN`'XX_FI(row)`'XX_COMMA`'XX_FI(col)`'XX_COMMA`'XX_FI(rows)`'XX_COMMA`'XX_FI(cols)`'XX_RPAREN`')	XX_TD(return screen region as text))
XX_TR(XX_TDH(`AsciiField')	XX_TD(return current field as text))
')dnl
XX_TR(XX_TDH(XX_BLOCK()`Attn')	XX_TD(attention key))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(AltCursor)	XX_TD(switch between block and underscore cursor))
')dnl
XX_TR(XX_TDH(`BackSpace')	XX_TD(move cursor left (or send XX_SM(ASCII BS))))
XX_TR(XX_TDH(`BackTab')	XX_TD(tab to start of previous input field))
XX_TR(`XX_TDH(CircumNot)	XX_TD(`input "^" in XX_SM(NVT) mode, or "XX_NOT" in 3270 mode''))
XX_TR(XX_TDH(XX_BLOCK()`Clear')	XX_TD(clear screen))
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,c3270,,`XX_TR(XX_TDH(`Cols')	XX_TD(report screen size))
')dnl
ifelse(XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,tcl3270,,`XX_TR(XX_TDH(Compose)	XX_TD(next two keys form a special symbol))
')dnl
XX_TR(XX_TDH(XX_BLOCK()Connect`'XX_LPAREN`'XX_FI(host)`'XX_RPAREN)	XX_TD(connect to XX_FI(host)))
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TDH(`Copy')	XX_TD(copy highlighted area to clipboard))
')dnl
XX_TR(XX_TDH(XX_BLOCK()`CursorSelect')	XX_TD(Cursor Select XX_SM(AID)))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(Cut)	XX_TD(copy highlighted area to clipboard and erase))
XX_TR(XX_TDH(Default)	XX_TD(enter key literally))
')dnl
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TDH(`Cut')	XX_TD(copy highlighted area to clipboard and erase))
')dnl
XX_TR(XX_TDH(`Delete')	XX_TD(delete character under cursor (or send XX_SM(ASCII DEL))))
XX_TR(XX_TDH(`DeleteField')	XX_TD(delete the entire field))
XX_TR(XX_TDH(`DeleteWord')	XX_TD(delete the current or previous word))
XX_TR(XX_TDH(XX_BLOCK()`Disconnect')	XX_TD(disconnect from host))
XX_TR(XX_TDH(`Down')	XX_TD(move cursor down))
XX_TR(XX_TDH(`Dup')	XX_TD(duplicate field))
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,c3270,,XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,wc3270,,`XX_TR(XX_TDH(`Ebcdic'`')	XX_TD(return entire screen contents in XX_SM(EBCDIC)))
XX_TR(XX_TDH(`Ebcdic'`'XX_LPAREN`'XX_FI(length)`'XX_RPAREN`')	XX_TD(return screen contents at cursor in XX_SM(EBCDIC)))
XX_TR(XX_TDH(`Ebcdic'`'XX_LPAREN`'XX_FI(row)`'XX_COMMA`'XX_FI(col)`'XX_COMMA`'XX_FI(length)`'XX_RPAREN)	XX_TD(return screen contents in XX_SM(EBCDIC)))
XX_TR(XX_TDH(`Ebcdic'`'XX_LPAREN`'XX_FI(row)`'XX_COMMA`'XX_FI(col)`'XX_COMMA`'XX_FI(rows)`'XX_COMMA`'XX_FI(cols)`'XX_RPAREN`')	XX_TD(return screen region in XX_SM(EBCDIC)))
XX_TR(XX_TDH(`EbcdicField')	XX_TD(return current field in XX_SM(EBCDIC)))
')dnl
XX_TR(XX_TDH(XX_BLOCK()`Enter')	XX_TD(Enter XX_SM(AID) (or send XX_SM(ASCII CR))))
XX_TR(XX_TDH(`Erase')	XX_TD(erase previous character (or send XX_SM(ASCII BS))))
XX_TR(XX_TDH(`EraseEOF')	XX_TD(erase to end of current field))
XX_TR(XX_TDH(`EraseInput')	XX_TD(erase all input fields))
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TDH(Escape)	XX_TD(escape to XX_FB(c3270>) prompt))
')dnl
ifelse(XX_PRODUCT,tcl3270,,`XX_TR(XX_TDH(Execute(XX_FI(cmd)))	XX_TD(execute a command in a shell))
')dnl
XX_TR(XX_TDH(`FieldEnd')	XX_TD(move cursor to end of field))
XX_TR(XX_TDH(`FieldMark')	XX_TD(mark field))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(HandleMenu(XX_FI(name)))	XX_TD(pop up a menu))
')dnl
XX_TR(XX_TDH(`HexString'`'XX_LPAREN`'XX_FI(hex_digits)`'XX_RPAREN)	XX_TD(insert control-character string))
XX_TR(XX_TDH(`Home')	XX_TD(move cursor to first input field))
XX_TR(XX_TDH(`Insert')	XX_TD(set insert mode))
XX_TR(XX_TDH(XX_BLOCK()`Interrupt')	XX_TD(send XX_SM(TELNET IP) to host))
ifelse(XX_MODE,console,`XX_TR(XX_TDH(`Keypad')	XX_TD(Display pop-up keypad))
')dnl
XX_TR(XX_TDH(Key`'XX_LPAREN`'XX_FI(keysym)`'XX_RPAREN)	XX_TD(insert key XX_FI(keysym)))
XX_TR(XX_TDH(Key`'XX_LPAREN`'0x`'XX_FI(xx)`'XX_RPAREN)	XX_TD(insert key with character code XX_FI(xx)))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(Keymap(XX_FI(keymap)))	XX_TD(toggle alternate XX_FI(keymap) (or remove with XX_FB(None))))
XX_TR(XX_TDH(KybdSelect(XX_FI(direction) [,XX_FI(atom)...]))	XX_TD(Extend selection by one row or column))
')dnl
XX_TR(XX_TDH(`Left')	XX_TD(move cursor left))
XX_TR(XX_TDH(`Left2')	XX_TD(move cursor left 2 positions))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(XX_BLOCK()Macro(XX_FI(macro)))	XX_TD(run a macro))
')dnl
ifelse(XX_MODE,console,`XX_TR(XX_TDH(`Menu')	XX_TD(Display menu bar))
')dnl
XX_TR(XX_TDH(`MonoCase')	XX_TD(toggle uppercase-only mode))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(MoveCursor)	XX_TD(move cursor to mouse position))
')dnl
XX_TR(XX_TDH(MoveCursor`'XX_LPAREN`'XX_FI(row)`'XX_COMMA`'XX_FI(col)`'XX_RPAREN)	XX_TD(move cursor to zero-origin (XX_FI(row),XX_FI(col))))
ifelse(XX_PRODUCT,x3270,`XX_TR(`XX_TDH(XX_BLOCK()MoveCursorSelect)	XX_TD(`move cursor to mouse position, light pen selection''))
')dnl
XX_TR(XX_TDH(`Newline')	XX_TD(move cursor to first field on next line (or send XX_SM(ASCII LF))))
XX_TR(XX_TDH(`NextWord')	XX_TD(move cursor to next word))
XX_TR(XX_TDH(XX_BLOCK()PA`'XX_LPAREN`'XX_FI(n)`'XX_RPAREN)	XX_TD(Program Attention XX_SM(AID) (XX_FI(n) from 1 to 3)))
XX_TR(XX_TDH(XX_BLOCK()PF`'XX_LPAREN`'XX_FI(n)`'XX_RPAREN)	XX_TD(Program Function XX_SM(AID) (XX_FI(n) from 1 to 24)))
XX_TR(XX_TDH(`PreviousWord')	XX_TD(move cursor to previous word))
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TDH(`Paste')	XX_TD(insert clipboard contents))')dnl
ifelse(XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,tcl3270,,
`XX_TR(XX_TDH(Printer(Start[,XX_FI(lu)]|Stop))	XX_TD(start or stop printer session))
')dnl
ifelse(XX_PLATFORM,windows,`XX_TR(XX_TDH(PrintText([gdi|wordpad,][dialog|nodialog,]XX_FI([printer-name])))	XX_TD(print screen text on printer))
')dnl
ifelse(XX_PRODUCT,ws3270,,XX_PRODUCT,wc3270,,XX_PRODUCT,tcl3270,,
`XX_TR(XX_TDH(PrintText(XX_FI(command)))	XX_TD(print screen text on printer))
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(PrintWindow(XX_FI(command)))	XX_TD(print screen image (bitmap) on printer))
')dnl
XX_TR(XX_TDH(Quit)	XX_TD(exit XX_FB(XX_PRODUCT)))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(XX_BLOCK()Reconnect)	XX_TD(reconnect to previous host))
')dnl
XX_TR(XX_TDH(`Redraw')	XX_TD(redraw window))
XX_TR(XX_TDH(`Reset')	XX_TD(reset locked keyboard))
XX_TR(XX_TDH(`Right')	XX_TD(move cursor right))
XX_TR(XX_TDH(`Right2')	XX_TD(move cursor right 2 positions))
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,c3270,,XX_PRODUCT,wc3270,,`XX_TR(XX_TDH(ReadBuffer`'XX_SPACE`'Ascii`')	XX_TD(dump screen buffer as text))
XX_TR(XX_TDH(ReadBuffer`'XX_SPACE`'Ebcdic`')	XX_TD(dump screen buffer in EBCDIC))
')dnl
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,c3270,,`XX_TR(XX_TDH(`Rows')	XX_TD(report screen size))
')dnl
ifelse(XX_PRODUCT,tcl3270,,`XX_TR(XX_TDH(XX_BLOCK()Script(XX_FI(command)[,XX_FI(arg)...]))	XX_TD(run a script))
')dnl
ifelse(XX_INTERACTIVE,yes,`XX_TR(XX_TDH(Scroll(Forward|Backward))	XX_TD(scroll screen))
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(SelectAll(XX_FI(atom)))	XX_TD(select entire screen))
')dnl
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(SetFont(XX_FI(font)))	XX_TD(change emulator font))
')dnl
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,c3270,,XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,wc3270,,
`ifelse(XX_PRODUCT,tcl3270,`XX_TR(XX_TDH(Snap)	XX_TD(same as XX_FB(Snap Save)))
')dnl
XX_TR(XX_TDH(Snap`'XX_SPACE`'Ascii`')	XX_TD(report saved screen data (see XX_FB(Ascii))))
XX_TR(XX_TDH(Snap`'XX_SPACE`'Cols`')	XX_TD(report saved screen size))
XX_TR(XX_TDH(Snap`'XX_SPACE`'Ebcdic`')	XX_TD(report saved screen data (see XX_FB(Ebcdic))))
XX_TR(XX_TDH(Snap`'XX_SPACE`'ReadBuffer`')	XX_TD(report saved screen data (see XX_FB(ReadBuffer))))
XX_TR(XX_TDH(Snap`'XX_SPACE`'Rows`')	XX_TD(report saved screen size))
XX_TR(XX_TDH(Snap`'XX_SPACE`'Save`')	XX_TD(save screen image))
XX_TR(XX_TDH(Snap`'XX_SPACE`'Status`')	XX_TD(report saved connection status))
XX_TR(XX_TDH(`ifelse(XX_PRODUCT,tcl3270,`XX_BLOCK()Snap Wait [XX_FI(timeout)] Output',
`XX_BLOCK()SnapWaitOuput(XX_FI(timeout))')')	XX_TD(wait for host output and save screen image))
ifelse(XX_PRODUCT,tcl3270,,`XX_TR(XX_TDH(XX_BLOCK()Source(XX_FI(file)))	XX_TD(read commands from XX_FI(file)))
')dnl
XX_TR(XX_TDH(Status`')	XX_TD(report connection status))
')dnl
XX_TR(XX_TDH(XX_BLOCK()String`'XX_LPAREN`'XX_FI(string)`'XX_RPAREN)	XX_TD(insert string (simple macro facility)))
ifelse(XX_PRODUCT,wc3270,`XX_TR(XX_TDH(XX_BLOCK()`SelectDown')	XX_TD(Extend selection down))
XX_TR(XX_TDH(SelectLeft)	XX_TD(Extend selection left))
XX_TR(XX_TDH(SelectUp)	XX_TD(Extend selection up))
XX_TR(XX_TDH(SelectDown)	XX_TD(Extend selection down))
XX_TR(XX_TDH(SysReq)	XX_TD(System Request XX_SM(AID)))
')dnl
XX_TR(XX_TDH(`Tab')	XX_TD(move cursor to next input field))
XX_TR(XX_TDH(`Toggle'XX_LPAREN`'XX_FI(option)[,XX_FI(set|clear)]XX_RPAREN)	XX_TD(toggle an option))
XX_TR(XX_TDH(`ToggleInsert')	XX_TD(toggle insert mode))
XX_TR(XX_TDH(`ToggleReverse')	XX_TD(toggle reverse-input mode))
XX_TR(XX_TDH(XX_BLOCK()Transfer`'XX_LPAREN`'XX_FI(option)=XX_FI(value)...'`'XX_RPAREN)	XX_TD(file transfer))
ifelse(XX_PRODUCT,x3270,`XX_TR(XX_TDH(Unselect)	XX_TD(release selection))
')dnl
XX_TR(XX_TDH(`Up')	XX_TD(move cursor up))
ifelse(XX_PRODUCT,c3270,`XX_TR(XX_TDH(ignore)	XX_TD(do nothing))
')dnl
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,c3270,,XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,wc3270,,
`XX_TR(XX_TDH(XX_BLOCK()XX_WAIT(3270mode))	XX_TD(wait for 3270 mode))
XX_TR(XX_TDH(XX_BLOCK()XX_WAIT(Disconnect))	XX_TD(wait for host to disconnect))
XX_TR(XX_TDH(XX_BLOCK()XX_WAIT(InputField))	XX_TD(wait for valid input field))
XX_TR(XX_TDH(XX_BLOCK()XX_WAIT(NVTMode))	XX_TD(wait for NVT mode))
XX_TR(XX_TDH(XX_BLOCK()XX_WAIT(Output))	XX_TD(wait for more host output))
')dnl
ifelse(XX_PRODUCT,x3270,`XX_T_()
XX_TR(XX_TDH((the following are similar to xterm),COLSPAN="2"))
XX_T_()
XX_TR(XX_TDH(ignore)	XX_TD(do nothing))
XX_TR(`XX_TDH(insert-selection([XX_FI(atom)[,XX_FI(atom)...]]))	XX_TD(``paste' selection')')
XX_TR(XX_TDH(move-select)	XX_TD(a combination of XX_FB(MoveCursor) and XX_FB(select-start)))
XX_TR(XX_TDH(select-end(XX_FI(atom)[,XX_FI(atom)...]]))	XX_TD(complete selection and assign to atom(s)))
XX_TR(XX_TDH(select-extend)	XX_TD(move the end of a selection))
XX_TR(XX_TDH(select-start)	XX_TD(mark the beginning of a selection))
XX_TR(XX_TDH(set-select(XX_FI(atom)[,XX_FI(atom)...]]))	XX_TD(assign existing selection to atom(s)))
XX_TR(XX_TDH(start-extend)	XX_TD(begin marking the end of a selection))
')dnl
XX_TE()
ifelse(XX_MODE,console,`XX_LP
Any of the above actions may be entered at the XX_FB(XX_PRODUCT>) prompt;
these commands are also available for use in keymaps
(see XX_LINK(#Keymaps,XX_SM(KEYMAPS))).
Command names are case-insensitive.
Parameters can be specified with parentheses and commas, e.g.:
XX_RS(PF(1))
or with spaces, e.g.:
XX_RS(PF 1)
Parameters can be quoted with double-quote characters, to allow spaces,
commas, and parentheses to be used.
XX_LP
XX_FB(XX_PRODUCT) also supports the following interactive commands:
XX_TPS()dnl
XX_TP(XX_FB(Help))
Displays a list of available commands.
XX_TP(XX_FB(ScreenTrace))
Turns screen tracing (saving screen images to a file) on or off.
The command XX_FB(screentrace on) enables screen tracing;
the command XX_FB(screentrace off) disables it.
After XX_FB(on), a filename may be specified to override the default
trace file name of
ifelse(XX_PLATFORM,windows,`XX_FB(x3scr.)`'XX_FI(pid)`'XX_FB(.txt)',`XX_FB(/tmp/x3scr.)`'XX_FI(pid)').
The keyaord XX_FB(on) can also be followed by the keyword XX_FB(printer) and an optional
ifelse(XX_PRODUCT,wc3270,printer name,print command)
to direct screen traces directly to the printer.
XX_TP(XX_FB(Show))
Displays statistics and settings.
XX_TP(XX_FB(Trace))
Turns tracing on or off.
The command XX_FB(trace on) enables data stream and keyboard event tracing;
the command XX_FB(trace off) disables it.
The qualifier XX_FB(data) or XX_FB(keyboard) can be specified
before XX_FB(on) or XX_FB(off) to enable or disable a particular trace.
After XX_FB(on), a filename may be specified to override the default
trace file name of
ifelse(XX_PLATFORM,windows,`XX_FB(x3trc.)`'XX_FI(pid)`'XX_FB(.txt)',`XX_FB(/tmp/x3trc.)`'XX_FI(pid)').
XX_TPE()dnl
')dnl
XX_LP
Note that certain parameters to XX_PRODUCT actions (such as the names of files
and keymaps) are subject to XX_FI(substitutions):
XX_LP
The character XX_FB(~) at the beginning of a string is replaced with the user's
home directory.
ifelse(XX_PLATFORM,unix,`A XX_FB(~) character followed by a username is
replaced with that XX_POSESSIVE(user) home directory.
')dnl
XX_LP
Environment variables are substituted using the Unix shell convention of
$XX_FI(name) or ${XX_FI(name)}.
XX_LP
Two special pseudo-environment variables are supported. ${TIMESTAMP} is
replaced with a microsecond-resolution timestamp; ${UNIQUE} is replaced with a
string guaranteed to make a unique filename (the process ID optionally
followed by a dash and a string of digits). ${UNIQUE} is used to form trace
file names.
ifelse(XX_PRODUCT,c3270,`include(keymaps.inc)
')dnl
ifelse(XX_PRODUCT,wc3270,`include(keymaps.inc)
')dnl
ifelse(XX_PRODUCT,x3270,,`include(ft.inc)
')dnl
XX_SH(The PrintText Action)
The XX_FB(PrintText) produces screen snapshots in a number of different
forms.
The default form wth no arguments sends a copy of the screen to the default
printer.
A single argument is
ifelse(XX_PLATFORM,windows,`the name of the printer to use',
`the command to use to print, e.g., XX_FB(lpr)').
ifelse(XX_PLATFORM,windows,`The font defaults to XX_FB(Courier New) and the
point size defaults to XX_FI(auto) (by default -- pick the widest font that
will fit across the page) or 8 (if using WordPad).
These can be overridden by the XX_FB(printTextFont) and XX_FB(printTextSize)
resources, respectively.
Unless the XX_FB(wordpad) keyword is used to force the output to be run through
the Windows WordPad utility, additional resources can control the
output. XX_FB(printTextHorizontalMargin) defines the left- and right-hand
margins. XX_FB(printTextVerticalMargin) defines the top and bottom margins.
Both default to 0.5 inches; the values are in inches by default but can be
suffixed with XX_FB(mm) or XX_FB(cm). XX_FB(printTextOrientation) defines the
page orientation as XX_FB(portrait) or XX_FB(landscape).
')dnl
XX_LP
Multiple arguments can include keywords to control the output of
XX_FB(PrintText):
XX_TPS()dnl
ifelse(XX_PLATFORM,windows,`XX_TP(XX_FB(gdi))
Print directly to the printer using GDI, instead of creating an RTF file and
running WordPad to print it. (This is the default).
XX_TP(XX_FB(wordpad))
Create an RTF file and run WordPad to print it. (This was the former default).
XX_TP(XX_FB(dialog))
In GDI mode, pop up the Windows print dialog.
ifelse(XX_PRODUCT,wc3270,`(This is the default.)
')dnl
XX_TP(XX_FB(nodialog))
In GDI mode, skip the usual Windows print dialog.
ifelse(XX_PRODUCT,ws3270,`(This is the default.)
')dnl
')dnl
XX_TP(XX_FB(file) XX_FI(filename))
Save the output in a file.
XX_TP(XX_FB(html))
Save the output as HTML.  This option implies XX_FB(file).
XX_TP(XX_FB(rtf))
Save the output as RichText.  This option implies XX_FB(file).
The font defaults to XX_FB(Courier New) and the
point size defaults to 8.
These can be overridden by the XX_FB(printTextFont) and XX_FB(printTextSize)
resources, respectively.
ifelse(XX_PLATFORM,unix,`XX_TP(XX_FB(string))
Return the output as a string.  This can only be used from scripts.
')dnl
XX_TP(XX_FB(modi))
Render modified fields in italics.
XX_TP(XX_FB(caption) XX_FI(text))
Add the specified XX_FI(text) as a caption above the output.
Within XX_FI(text), the special sequence XX_FB(%T%) will be replaced with
a timestamp.
ifelse(XX_PRODUCT,x3270,`XX_TP(XX_FB(secure))
Disables the pop-up dialog.
')dnl
ifelse(XX_PLATFORM,unix,`XX_TP(XX_FB(command) XX_FI(command))
Directs the output to a command.
This allows one or more of the other keywords to be specified, while still
sending the output to the printer.
')dnl
XX_TPE()
define(XX_SCRIPTS,`ifelse(XX_PRODUCT,x3270,Macros and Scripts,
XX_PRODUCT,c3270,Scripts,
Nested Scripts)')dnl
XX_SH(XX_SCRIPTS)
ifelse(XX_PRODUCT,tcl3270,,XX_PRODUCT,wc3270,,`There are several types of
ifelse(XX_PRODUCT,x3270,`macros and ',
XX_PRODUCT,x3270,,XX_PRODUCT,c3270,,
`nested ')dnl
script functions available.
')dnl
XX_TPS()dnl
XX_TP(XX_FB(The String XX_Action))
The simplest method for
ifelse(XX_PRODUCT,x3270,`macros ',
XX_PRODUCT,c3270,`scripting ',
`nested scripts ')dnl
is provided via the XX_FB(String)
XX_action`'ifelse(XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,tcl3270,,`, which
can be bound to any key in a keymap').
The arguments to XX_FB(String) are one or more double-quoted strings which are
inserted directly as if typed.
The C backslash conventions are honored as follows.
(Entries marked * mean that after sending the XX_SM(AID) code to the host,
XX_FB(XX_PRODUCT) will wait for the host to unlock the keyboard before further
processing the string.)
XX_TS(2,l l.)
XX_TR(XX_TD(XX_BS()b)	XX_TD(Left))
XX_TR(XX_TD(XX_BS()`e'XX_FI(xxxx))	XX_TD(EBCDIC character in hex))
XX_TR(XX_TD(XX_BS()f)	XX_TD(Clear*))
XX_TR(XX_TD(XX_BS()n)	XX_TD(Enter*))
XX_TR(XX_TD(XX_BS()`pa'XX_FI(n))	XX_TD(PA(XX_FI(n))*))
XX_TR(XX_TD(XX_BS()`pf'XX_FI(nn))	XX_TD(PF(XX_FI(nn))*))
XX_TR(XX_TD(XX_BS()r)	XX_TD(Newline))
XX_TR(XX_TD(XX_BS()t)	XX_TD(Tab))
XX_TR(XX_TD(XX_BS()T)	XX_TD(BackTab))
XX_TR(XX_TD(XX_BS()`u'XX_FI(xxxx))	XX_TD(Unicode character in hex))
XX_TR(XX_TD(XX_BS()`x'XX_FI(xxxx))	XX_TD(Unicode character in hex))
XX_TE()
XX_IP
Note that the numeric values for the XX_BS()e, XX_BS()u and XX_BS()x sequences
can be abbreviated to 2 digits.
Note also that EBCDIC codes greater than 255 and some Unicode character codes
represent DBCS characters, which will work only if XX_PRODUCT is built with
DBCS support and the host allows DBCS input in the current field.
ifelse(XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,tcl3270,,XX_PRODUCT,wc3270,
`XX_IP
An example keymap entry would be:
XX_RS(XX_KEY(Alt,p): String("probs clearrdr`'XX_BS()n"))
',
`XX_IP
An example keymap entry would be:
XX_RS(XX_KEY(Meta,p): String("probs clearrdr`'XX_BS()n"))
')dnl
XX_IP
XX_FB(Note:)
The strings are in XX_SM(ASCII) and converted to XX_SM(EBCDIC),
so beware of inserting
control codes.
ifelse(XX_PRODUCT,x3270,`Also, a backslash before a XX_FB(p) may need to be
doubled so it will not be removed when a resource file is read.
')dnl
XX_IP
There is also an alternate form of the XX_FB(String) XX_action, XX_FB(HexString),
which is used to enter non-printing data.
The argument to XX_FB(HexString) is a string of hexadecimal digits, two per
character.  A leading 0x or 0X is optional.
In 3270 mode, the hexadecimal data represent XX_SM(EBCDIC) characters, which
are entered into the current field.
In XX_SM(NVT) mode, the hexadecimal data represent XX_SM(ASCII) characters,
which are sent directly to the host.
ifelse(XX_PRODUCT,tcl3270,,`XX_TP(XX_FB(The Script Action))
This action causes XX_FB(XX_PRODUCT) to start a child process which can
execute XX_FB(XX_PRODUCT) actions.
ifelse(XX_PLATFORM,windows,
`XX_FB(XX_PRODUCT) listens for connections from the child process on a
dynamically-generated TCP port.
',
`Standard input and output from the child process are piped back to
XX_FB(XX_PRODUCT).
')dnl
The XX_FB(Script) action is fully documented in
XX_LINK(XX_X3270-script.html,XX_FI(XX_X3270-script)(1)).
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_TP(XX_FB(The macros Resource))
An alternate method of defining macros is the "XX_PRODUCT.macros" resource.
This resource is similar to a keymap, but instead of defining keyboard
mappings, it associates a list of X actions with a name.
These names are displayed on a Macros menu that appears when XX_FB(XX_PRODUCT)
is connected to a host.
Selecting one of the names on the menu executes the X actions associated with
it.
Typically the actions are XX_FB(String) calls, but any action may be specified.
Here is a sample macros resource definition, which would result in a four-entry
Macros menu:
XX_RS(XX_PRODUCT.macros: XX_BS()
XX_BR
	log off: String("logout`'XX_BS()n")XX_BS()n`'XX_BS()
XX_BR
	vtam: String("dial vtam`'XX_BS()n")XX_BS()n`'XX_BS()
XX_BR
	pa1: PA(1)XX_BS()n`'XX_BS()
XX_BR
	alt printer: PrintText("lpr -Plw2"))
XX_IP
You can also define a different set of macros for each host.
If there is a resource named
XX_DQUOTED(XX_PRODUCT.XX_FI(macros).XX_FI(somehost)),
it defines the macros menu for when XX_FB(XX_PRODUCT)
is connected to XX_FI(somehost).
XX_TP(XX_FB(The XX_DASHED(script) Option))
This facility allows XX_FB(XX_PRODUCT)
to operate under the complete control of a script.
XX_FB(XX_PRODUCT)
accepts actions from standard input, and prints results on standard output.
The XX_FB(XX_DASHED(script)) option is fully documented in
XX_LINK(XX_X3270-script.html,XX_FI(XX_X3270-script)(1)).
')dnl
XX_TPE()dnl
ifelse(XX_PRODUCT,s3270,,XX_PRODUCT,ws3270,,XX_PRODUCT,tcl3270,,XX_PRODUCT,wc3270,,`XX_SH(Composite Characters)
XX_FB(XX_PRODUCT)
allows the direct entry of accented letters and special symbols.
Pressing and releasing the "Compose" key, followed by two other keys, causes
entry of the symbol combining those two keys.
For example, "Compose" followed by the "C" key and the "," (comma) key, enters
the "C-cedilla" symbol.
A `C' on the status line indicates a pending composite character.
XX_PP
The mappings between these pairs of ordinary keys and the symbols they
represent is controlled by the "XX_PRODUCT.composeMap" resource; it gives the
name of the map to use.
The maps themselves are named "XX_PRODUCT.composeMap.XX_FI(name)".
The default is "latin1", which gives mappings for most of the symbols in
the XX_SM(ISO) 8859-1 Latin-1 character set that are not in the
7-bit XX_SM(ASCII)
character set.
XX_PP
XX_FB(Note:)
The default keymap defines
ifelse(XX_PRODUCT,x3270,`the "Multi_key" keysym',`XX_KEY(Meta,m)')
as the "Compose" key.
ifelse(XX_PRODUCT,x3270,`If your keyboard lacks such a key, you',`You')
may set up your own "Compose" key with
a keymap that maps some other keysym onto the XX_FB(Compose) action.
')dnl
ifelse(XX_PRODUCT,x3270,`include(apl.inc)')dnl
ifelse(XX_PRODUCT,c3270,
`XX_SH(Printer Session Support)
XX_PRODUCT supports associated printer sessions via the XX_FI(pr3287)(1)
program.
The XX_FB(Printer) action is used to start or stop a XX_FI(pr3287) session.
XX_LP
The action XX_FB(Printer Start) starts a printer session, associated with the
current LU.  (This works only if the host supports TN3270E.)
XX_LP
The action XX_FB(Printer Start) XX_FI(lu) starts a printer session, associated
with a specific XX_FI(lu).
XX_LP
The action XX_FB(Printer Stop) stops a printer session.
XX_LP
The resource XX_FB(c3270.printer.options) specifies extra options, such as
XX_FB(-trace) to pass to XX_FI(pr3287).
XX_LP
See XX_FI(pr3287)(1) for further details.
XX_LP
The resource XX_FB(c3270.printerLu) controls automatic printer session
start-up.  If it is set to XX_DQUOTED(XX_FB(.)), then whenever a login session is started,
a printer session will automatically be started, associated with the login
session.  If it is set an LU name, then the automatic printer session will be
associated with the specified LU.
')dnl
ifelse(XX_PRODUCT,wc3270,
`XX_SH(Printer Session Support)
XX_PRODUCT supports associated printer sessions via the XX_FI(wpr3287)(1)
program.
The XX_FB(Printer) action is used to start or stop a XX_FI(wpr3287) session.
XX_LP
The action XX_FB(Printer Start) starts a printer session, associated with the
current LU.  (This works only if the host supports TN3270E.)
XX_LP
The action XX_FB(Printer Start) XX_FI(lu) starts a printer session, associated
with a specific XX_FI(lu).
XX_LP
The action XX_FB(Printer Stop) stops a printer session.
XX_LP
The resource XX_FB(wc3270.printer.name) specifies the Windows printer used to
print each job.
It defaults to the value of the XX_FB($PRINTER) environment
variable, if set.
Otherwise the default system printer is used.
This resource also controls the printer used by the XX_FB(PrintText) action.
XX_LP
The resource XX_FB(wc3270.printer.options) specifies extra options, such as
XX_FB(-trace) to pass to XX_FI(wpr3287).
XX_LP
See XX_FI(wpr3287)(1) for further details.
XX_LP
The resource XX_FB(wc3270.printerLu) controls automatic printer session
start-up.  If it is set to XX_DQUOTED(XX_FB(.)), then whenever a login session is started,
a printer session will automatically be started, associated with the login
session.  If it is set an LU name, then the automatic printer session will be
associated with the specified LU.
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_SH(Screen Printing)
Screen printing is handled through options on the XX_FB(File) menu or by the
XX_FB(PrintText) and XX_FB(PrintWindow) actions.
Each results in a pop-up to confirm the print command.
XX_PP
The XX_FB(PrintText) action (usually assigned to the key XX_LT()Meta>p) sends
the current
screen image to the printer as XX_SM(ASCII) characters.
The default command used to print the data is controlled by
the "XX_PRODUCT.printTextCommand" resource; the default is
XX_FB(lpr).
You may also use a keymap definition to pass a print command the
XX_FB(PrintText) action itself.
The command receives the screen text as its standard input.
For example, the following keymap will save the screen text in a file:
XX_IP
XX_RS(XX_KEY(Meta,f): PrintText("cat >screen.image"))
XX_PP
Note: XX_FB(HardPrint) is an alias for XX_FB(PrintText).
XX_PP
The XX_FB(PrintWindow) action (usually assigned to the key XX_LT()Meta>b) sends the current
screen image to the printer as a bitmap.
The default command used to print the data is controlled by
the "XX_PRODUCT.printWindowCommand" resource; the default is
XX_IP
XX_RS(XX_FB(xwd XX_DASHED(id) %d | xpr | lpr).)
XX_PP
You may also use a keymap definition to pass a print command to the
XX_FB(PrintWindow) action itself.
If the command contains the text "%d", the window ID of
XX_FB(XX_PRODUCT) will be substituted before it is run.
For example, the following keymap will pop up a duplicate of the current
screen image:
XX_IP
XX_RS(XX_KEY(Meta,g): PrintWindow("xwd XX_DASHED(id) %d | xwud &"))
XX_LP
If the command for PrintWindow or PrintText begins with an "@" character,
the initial pop-up menu to confirm the print command is not displayed and
the command cannot be edited.
')dnl
ifelse(XX_PRODUCT,x3270,
`XX_SH(Bugs)
Cursor highlighting will not work with if you use the XX_FB(NoTitleFocus)
option in your .twmrc file.
')dnl
ifelse(XX_PRODUCT,wc3270,,`
XX_SH(Passthru)
XX_FB(XX_PRODUCT) supports the Sun XX_FI(telnet-passthru)
service provided by the XX_FI(in.telnet-gw) server.
This allows outbound telnet connections through a firewall machine.
When a XX_FB(p:) is prepended to a hostname, XX_FB(XX_PRODUCT)
acts much like the XX_FI(itelnet)(1) command.
It contacts the machine named XX_FB(internet-gateway) at the port defined in
XX_FB(/etc/services) as XX_FB(telnet-passthru)
(which defaults to 3514).
It then passes the requested hostname and port to the
XX_FB(in.telnet-gw) server.
')dnl
XX_SH(Proxy)
The XX_FB(XX_DASHED(proxy)) option or the XX_FB(XX_PRODUCT.proxy) resource
causes XX_PRODUCT to use a proxy server to connect to the host.
The syntax of the option or resource is:
XX_RS(XX_FI(type):XX_FI(host)[:XX_FI(port)]
)
The supported values for XX_FI(type) are:
XX_TS(3,`center;
c l c .')
XX_TR(XX_TD(XX_TC(Proxy Type))	XX_TD(XX_TC(Protocol))	XX_TD(XX_TC(Default Port)))
XX_T_
XX_TR(XX_TD(XX_TC(http))	XX_TD(XX_TC(RFC 2817 HTTP tunnel (squid)))	XX_TD(XX_TC(3128)))
XX_TR(XX_TD(XX_TC(passthru))	XX_TD(XX_TC(Sun in.telnet-gw))	XX_TD(XX_TC(none)))
XX_TR(XX_TD(XX_TC(socks4))	XX_TD(XX_TC(SOCKS version 4))	XX_TD(XX_TC(1080)))
XX_TR(XX_TD(XX_TC(socks5))	XX_TD(XX_TC(SOCKS version 5 (RFC 1928)))	XX_TD(XX_TC(1080)))
XX_TR(XX_TD(XX_TC(telnet))	XX_TD(XX_TC(No protocol (just send XX_FB(connect) XX_FI(host port))))	XX_TD(XX_TC(none)))
XX_TE()
XX_LP()
The special types XX_FB(socks4a) and XX_FB(socks5d) can also be used to force
the proxy server to do the hostname resolution for the SOCKS protocol.
ifelse(XX_PRODUCT,x3270,,`include(resources.inc)')dnl
ifelse(XX_PRODUCT,tcl3270,,XX_PRODUCT,wc3270,,XX_PRODUCT,ws3270,,`XX_SH(Files)
ifelse(XX_PRODUCT,x3270,/usr/lib/X11,/usr/local/lib)/x3270/ibm_hosts
XX_BR
ifelse(XX_PRODUCT,x3270,`$HOME/.x3270pro
',XX_PRODUCT,c3270,`$HOME/.c3270pro
')
')dnl
ifelse(XX_PRODUCT,x3270,`XX_SH(Environment Variables)
XX_FB(3270PRO) Path of profile file, containing resource definitions.
Merged after the system resource database, but before XX_FB(X3270RDB).
Defaults to $HOME/.x3270pro.
XX_BR
XX_FB(NOX3270PRO) If set, do not read the profile.
XX_BR
XX_FB(X3270RDB) Additional resource definitions, merged after the profile
file but before the command-line options.
XX_BR
XX_FB(KEYMAP) Keymap name.
XX_BR
XX_FB(KEYBD) Keymap name.
')dnl
XX_SH(See Also)
ifelse(XX_INTERACTIVE,yes,XX_LINK(XX_PR3287-man.html,XX_PR3287`'(1))`, ')dnl
ifelse(XX_PRODUCT,XX_S3270,,XX_LINK(XX_S3270-man.html,XX_S3270`'(1))`, ')dnl
ifelse(XX_PRODUCT,tcl3270,,XX_LINK(XX_X3270-script.html,`XX_X3270-script`'(1)), ')dnl
ifelse(XX_PLATFORM,unix,`ifelse(XX_PRODUCT,x3270,,XX_LINK(x3270-man.html,x3270(1))`, ')dnl
ifelse(XX_PRODUCT,c3270,,XX_LINK(c3270-man.html,c3270(1))`, ')dnl
ifelse(XX_PRODUCT,tcl3270,,XX_LINK(tcl3270-man.html,tcl3270(1))`, ')dnl
')dnl
telnet(1), tn3270(1)dnl
ifelse(XX_PRODUCT,x3270,`, XX_LINK(ibm_hosts.html,ibm_hosts(5))
XX_BR
X Toolkit Intrinsics
',`
')dnl
XX_BR
Data Stream XX_POSESSIVE(Programmer) Reference, IBM GA23-0059
XX_BR
Character Set Reference, IBM GA27-3831
XX_BR
RFC 1576, TN3270 Current Practices
XX_BR
RFC 1646, TN3270 Extensions for LUname and Printer Selection
XX_BR
RFC 2355, TN3270 Enhancements
XX_SH(Copyrights)
Copyright`'XX_COPY()1993-XX_CYEAR, Paul Mattes.
XX_BR
Copyright`'XX_COPY()2004-2005, Don Russell.
XX_BR
Copyright`'XX_COPY()2004, Dick Altenbern.
XX_BR
Copyright`'XX_COPY()1990, Jeff Sparkes.
XX_BR
Copyright`'XX_COPY()1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 30332.
XX_BR
All rights reserved.
XX_LP
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
XX_TPS()
XX_TP(*)
Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
XX_TP(*)
Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
XX_TP(*)
Neither the names of Paul Mattes, Don Russell, Dick Altenbern, Jeff Sparkes,
GTRC nor
the names of their contributors may be used to endorse or promote
products derived from this software without specific prior written
permission.
XX_TPE()
XX_LP
THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, DICK ALTENBERN, JEFF
SPARKES AND GTRC
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, DON RUSSELL, DICK
ALTENBERN, JEFF
SPARKES OR GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
XX_SH(Version)
XX_PRODUCT XX_VERSION_NUMBER
