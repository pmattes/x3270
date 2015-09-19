The full syntax for XX_FI(host) is:
XX_RS([XX_FI(prefix):]...[XX_FI(LUname)@]XX_FI(hostname)[:XX_FI(port)])
ifelse(XX_PRODUCT,wc3270,,`XX_LP
Prepending a XX_FB(P:) onto XX_FI(hostname) causes the connection to go
through the XX_FI(telnet-passthru) service rather than directly to the host.
See XX_LINK(#Passthru,XX_SM(PASSTHRU)) below.
')dnl
XX_LP
Prepending an XX_FB(S:) onto XX_FI(hostname) removes the "extended data
stream" option reported to the host.
See XX_LINK(#tn,XX_FB(XX_DASHED(tn))) below for further information.
XX_LP
Prepending an XX_FB(N:) onto XX_FI(hostname)
turns off TN3270E support for the session.
XX_LP
Prepending an XX_FB(L:) onto XX_FI(hostname)
causes XX_FB(XX_PRODUCT) to first create an SSL tunnel to the host, and then
create a TN3270 session inside the tunnel.  (This function is supported only if
XX_FB(XX_PRODUCT) was built with SSL/TLS support).
Note that TLS-encrypted sessions using the TELNET START-TLS option are
negotiated with the host automatically; for these sessions the XX_FB(L:) prefix
should not be used.
XX_LP
Prepending a XX_FB(B:) onto XX_FI(hostname) changes the interaction of
scripts and the host BIND-IMAGE message.
Without XX_FB(B:), XX_PRODUCT will unlock the keyboard as soon as a
BIND-IMAGE is received. With XX_FB(B:), it will wait for a Write command that
explicitly unlocks the keyboard.
XX_LP
A specific Logical Unit (LU) name to use may be specified by prepending it to
the XX_FI(hostname) with an XX_DQUOTED(XX_FB(@)).
Multiple LU names to try can be separated by commas.
An empty LU can be placed in the list with an extra comma.
(Note that the LU name is used for different purposes by different kinds of
hosts.
For example, CICS uses the LU name as the Terminal ID.)
XX_LP
The XX_FI(hostname) may optionally be placed inside square-bracket
characters XX_DQUOTED(XX_FB([)) and XX_DQUOTED(XX_FB(])).
This will prevent any colon XX_DQUOTED(XX_FB(:)) characters in the hostname
from being interpreted as indicating option prefixes or port numbers.
This allows numeric IPv6 addresses to be used as hostnames.
ifelse(XX_PRODUCT,wc3270,,XX_PRODUCT,ws3270,,`XX_LP
On systems that support the XX_FI(forkpty) library call, the
XX_FI(hostname) may be replaced with XX_FB(XX_DASHED(e)) and a command string.
This will cause XX_FB(XX_PRODUCT) to connect to a local child process, such as
a shell.
')dnl
XX_LP
The port to connect to defaults to
XX_FB(telnet).
This can be overridden with the XX_FB(XX_DASHED(port)) option, or by appending
a XX_FI(port) to the XX_FI(hostname) with a colon
XX_DQUOTED(XX_FB(:)).
(For compatability with previous versions of XX_FB(XX_PRODUCT)
and with XX_FI(tn3270)(1), the XX_FI(port)
may also be specified as a second, separate argument.)
