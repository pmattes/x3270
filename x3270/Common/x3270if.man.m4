dnl Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 by Paul Mattes.
dnl  Permission to use, copy, modify, and distribute this software and its
dnl  documentation for any purpose and without fee is hereby granted,
dnl  provided that the above copyright notice appear in all copies and that
dnl  both that copyright notice and this permission notice appear in
dnl  supporting documentation.
XX_TH(X3270IF,1,XX_DATE)
XX_SH(Name)
x3270if XX_DASHED() command interface to x3270, c3270 and s3270
XX_SH(Synopsis)
XX_FB(x3270if) [option]... [ XX_FI(action) ]
XX_BR
XX_FB(x3270if XX_DASHED(i))
XX_SH(Description)
XX_FB(x3270if) provides an interface between scripts and
the 3270 emulators XX_FI(x3270), XX_FI(c3270), and XX_FI(s3270).
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
XX_LINK(x3270-script.html,XX_FI(x3270-script)(1)) manual page.
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
XX_TP(XX_FB(XX_DASHED(p)) XX_FI(process-id))
Causes XX_FI(x3270if) to use a Unix-domain socket to connect to the emulator,
rather than pipe file descriptors given in environment variables.
The emulator must have been started with the XX_FB(XX_DASHED(socket)) option.
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
Unless the XX_FB(XX_DASHED(socket)) option is given when they are started,
XX_FI(x3270), XX_FI(c3270), and XX_FI(s3270)
use a pair of pipes for communication with each child script process.
The values of the file descriptors for these pipes are encoded as text
in two environment variables, which are required by
XX_FB(x3270if):
XX_TPS()dnl
XX_TP(XX_FB(X3270OUTPUT))
Output from the emulator, input to the child process.
XX_TP(XX_FB(X3270INPUT))
Input to the emulator, output from the child process.
XX_TPE()dnl
XX_SH(See Also)
XX_LINK(x3270-man.html,x3270(1)),
XX_LINK(c3270-man.html,c3270(1)),
XX_LINK(s3270-man.html,s3270(1)),
XX_LINK(x3270-script.html,x3270-script(1))
XX_SH(Copyright)
Copyright`'XX_COPY()1999, 2000, 2001, 2004, 2005 by Paul Mattes.
XX_RS(`Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.')
