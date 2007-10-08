dnl Copyright 1994, 1995, 1999, 2000, 2001 by Paul Mattes.
dnl  Permission to use, copy, modify, and distribute this software and its
dnl  documentation for any purpose and without fee is hereby granted,
dnl  provided that the above copyright notice appear in all copies and that
dnl  both that copyright notice and this permission notice appear in
dnl  supporting documentation.
XX_TH(IBM_HOSTS,5,XX_DATE)
XX_SH(Name)
ibm_hosts XX_DASHED() host database for x3270 and c3270
XX_SH(Synopsis)
/usr/lib/X11/x3270/ibm_hosts
XX_SH(Description)
The XX_FB(ibm_hosts)
file contains information regarding IBM hosts on the network.
An XX_FI(IBM host) is a host which can communicate with a 3270 terminal
emulator such as XX_FB(x3270) or XX_FB(c3270).
Each line defines a name in the following format
(optional fields are shown in brackets):
XX_LP
XX_FI(name)	XX_FI(type) [XX_FI(opt):]...[XX_FI(luname)@]XX_FI(hostname)[:XX_FI(port)] [XX_FI(actions)]
XX_LP
Items are separated by any number of blanks and/or TAB characters.
A line beginning with `#' is taken as a comment (note that `#' anywhere
else on a line does XX_FI(not) indicate a comment).
XX_LP
The XX_FI(name) field is a mnemonic used to identify the host.
XX_LP
The XX_FI(type) field is a keyword that indicates the type of entry.
The value XX_FB(primary) means that the XX_FI(name)
will be included in host-selection menus that may be displayed by a
3270 emulator.
The value XX_FB(alias) means that the XX_FI(name)
will not be included in menus, but will still be accepted as valid input
when a host name is required.
XX_LP
The XX_FI(hostname)
field is the Internet hostname or dot-notation Internet address of the host.
XX_LP
The XX_FI(hostname)
can `include' ``s:'' or ``p:'' prefixes, e.g., XX_FB(s:finicky)
(see the XX_LINK(x3270-man.html,XX_FI(x3270)(1)) or
XX_LINK(c3270-man.html,XX_FI(c3270)(1)) man page sfor details).
It can also include an LU name, separated by an ``@'' character, e.g.,
XX_FB(oddlu@bluehost).
Finally, it can include a non-default XX_FI(port) number, appended to the
XX_FI(hostname) with a colon ``:'' character, e.g.,
XX_FB(bluehost:97).
(For compatability with earlier versions of XX_FI(x3270),
the XX_FI(port) can also be separated by a slash ``/'' character.)
XX_LP
The optional XX_FI(actions)
field specifies actions to be taken once the connection is made and a
data-entry field is defined.
If the text looks like an action, e.g.,
XX_FB(PF(1)),
it is unmodified; otherwise it is taken as the parameter to the
XX_FB(String())
action.
The
XX_FI(actions)
are not intended for entering usernames and passwords; rather they provide an
automated way of specifying a front-end menu option.
XX_SH(Example)
Given the following
XX_FB(ibm_hosts)
file:
XX_LP
XX_RS(`mvs	primary	mvs-host
XX_BR
tso	alias	mvs-host
XX_BR
mvs2	primary	mvs-host:4012
XX_BR
vm	primary	vtam	Tab() String(3) Enter()
')
A 3270 emulator will display four names (XX_FB(mvs), XX_FB(mvs2),
XX_FB(afhost) and XX_FB(vm)) on its hosts menu.
The names XX_FB(mvs) and XX_FB(tso) will cause connections to the host
XX_FB(mvs-host).
The name XX_FB(mvs2) will also cause a connection to
XX_FB(mvs-host),
but to port 4012 rather than the emulator's default port (usually 23).
The name XX_FB(vm) will cause the 3270 emulator to connect to the host
XX_FB(vtam) (presumably some sort of host-selection front-end),
enter the string ``3'' on the second data-entry field on the screen, and
send the Enter XX_SM(AID) sequence.
XX_SH(Files)
/usr/lib/X11/x3270/ibm_hosts
XX_SH(See Also)
XX_LINK(x3270-man.html,x3270(1)),
XX_LINK(c3270-man.html,c3270(1))
