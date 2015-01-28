dnl Copyright (c) 1999-2009, 2013 Paul Mattes.
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
XX_TH(X3270IF,1,XX_DATE)
XX_SH(Name)
x3270if XX_DASHED() command interface to
ifelse(XX_PLATFORM,unix,`x3270, ')dnl
XX_C3270 and XX_S3270
XX_SH(Synopsis)
XX_FB(x3270if) [option]... [ XX_FI(action) ]
XX_BR
XX_FB(x3270if XX_DASHED(i))
XX_SH(Description)
XX_FB(x3270if) provides an interface between scripts and
the 3270 emulators
ifelse(XX_PLATFORM,unix,`XX_FI(x3270), ')dnl
XX_FI(XX_C3270) and XX_FI(XX_S3270).
XX_LP()
XX_FB(x3270if) operates in one of two modes.
In XX_FB(action mode), it passes a single action and parameters to the
emulator for execution.
The result of the action is written to standard output, along with the
(optional) status of the emulator.
(The action is optional as well, so that XX_FB(x3270if) can just reports the
emulator status.)
In XX_FB(iterative mode), it forms a continuous conduit between a script and
the emulator.
XX_LP()
The XX_FI(action) takes the form:
XX_IP()
XX_FI(action-name)(XX_FI(param)[,XX_FI(param)]...)
XX_LP()
The parentheses are manatory, and usually must be quoted when XX_FB(x3270if) is
called from a shell script.
XX_LP()
If any XX_FI(param) contains a space or comma, it must be surrounded by
double quotes.
XX_SH(Options)
XX_TPS()dnl
XX_TP(XX_FB(XX_DASHED(s)) XX_FI(field))
Causes XX_FB(x3270if) to write to stdout the value of one of the
emulator status fields.
XX_FI(Field) is an integer in the range 0 through 11.
The value 0 is a no-op and is used only to return exit status indicating the
state of the emulator.
The indices 1-11 and meanings of each field are documented on the
XX_LINK(XX_X3270-script.html,XX_FI(XX_X3270-script)(1)) manual page.
If an XX_FI(action) is specified as well, the status field is written after the
output of the action, separated by a newline.
The XX_FB(XX_DASHED(s)) option is mutually exclusive with the
XX_FB(XX_DASHED(S)) and XX_FB(XX_DASHED(i)) options.
XX_TP(XX_FB(XX_DASHED(S)))
Causes XX_FB(x3270if) to write to stdout the value of all of the
emulator status fields.
If an XX_FI(action) is specified as well, the status fields are written after the
output of the action, separated by a newline.
The XX_FB(XX_DASHED(S)) option is mutually exclusive with the
XX_FB(XX_DASHED(s)) and XX_FB(XX_DASHED(i)) options.
XX_TP(XX_FB(XX_DASHED(i)))
Puts XX_FB(x3270if) in iterative mode.
Data from XX_POSESSIVE(XX_FB(x3270if)) standard input is copied to the
XX_POSESSIVE(emulator) script input; data from the
XX_POSESSIVE(emulator) script output is copied to
XX_POSESSIVE(XX_FB(x3270if)) standard output.
The XX_FB(XX_DASHED(i)) option is mutually exclusive with the
XX_FB(XX_DASHED(s)) and XX_FB(XX_DASHED(S)) options.
XX_FB(x3270if)
runs until it detects end-of-file on its standard input or on the
output from the emulator.
(This mode exists primarily to give XX_FI(expect)(1)
a process to run, on systems which do not support bidirectional pipes.)
ifelse(XX_PLATFORM,unix,`XX_TP(XX_FB(XX_DASHED(p)) XX_FI(process-id))
Causes XX_FI(x3270if) to use a Unix-domain socket to connect to the emulator,
rather than pipe file descriptors given in environment variables.
The emulator must have been started with the XX_FB(XX_DASHED(socket)) option.
')dnl
XX_TP(XX_FB(XX_DASHED(t)) XX_FI(port))
Causes XX_FI(x3270if) to use a TCP socket to connect to the emulator,
rather than pipe file descriptors given in environment variables.
The emulator must have been started with the XX_FB(XX_DASHED(scriptport))
option.
XX_TP(XX_FB(XX_DASHED(v)))
Turns on verbose debug messages, showing on stderr the literal data that is
passed between the emulator and XX_FB(x3270if).
XX_TPE()dnl
XX_SH(Exit Status)
In action mode, if the requested XX_FI(action) succeeds,
XX_FB(x3270if) exits with status 0.
If the action fails, XX_FB(x3270if) exits with status 1.
In iterative mode, XX_FB(x3270if)
exits with status 0 when it encounters end-of-file.
If there is an operational error within XX_FB(x3270if)
itself, such as a command-line syntax error, missing environment
variable, or an unexpectedly closed pipe,
XX_FB(x3270if) exits with status 2.
XX_SH(Environment)
When a script is run as a child process of one of the emulators via the
XX_FB(Script) action, the emulator passes information about how to control it
in environment variables.
ifelse(XX_PLATFORM,unix,`XX_LP()
On Unix, the emulator process creates a pair of pipes for communication with
the child script process.
The values of the file descriptors for these pipes are encoded as text
in two environment variables:
XX_TPS()dnl
XX_TP(XX_FB(X3270OUTPUT))
Output from the emulator, input to the child process.
XX_TP(XX_FB(X3270INPUT))
Input to the emulator, output from the child process.
XX_TPE()dnl
')dnl
XX_LP()
ifelse(XX_PLATFORM,unix,`When an emulator is started with the
XX_FB(XX_DASHED(scriptport)) option, the ',`The')
emulator will pass the scriptport port number
encoded as text in the XX_FB(X3270PORT) environment variable.
XX_FI(x3270if) will use that value as if it had been passed to it via the
XX_FB(XX_DASHED(t)) option.
ifelse(XX_PLATFORM,unix,`XX_FB(X3270PORT) takes precedence over
XX_FB(X3270OUTPUT) and XX_FB(X3270INPUT).
')dnl
XX_SH(See Also)
ifelse(XX_PLATFORM,unix,`XX_LINK(x3270-man.html,x3270(1)), ')dnl
XX_LINK(XX_C3270-man.html,XX_C3270`'(1)),
XX_LINK(XX_S3270-man.html,XX_S3270`'(1)),
XX_LINK(XX_X3270-script.html,XX_X3270-script(1))
XX_SH(Copyright)
Copyright`'XX_COPY()1999-2009, XX_CYEAR Paul Mattes.
XX_BR
All rights reserved.
XX_LP()
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
XX_TPS()
XX_TP(*)
Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
XX_TP(*)
Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
XX_TP(*)
Neither the names of Paul Mattes nor the names of his contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.
XX_TPE()
XX_LP()
THIS SOFTWARE IS PROVIDED BY PAUL MATTES XX_DQUOTED(AS IS) AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
