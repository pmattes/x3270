XX_TH(PR3287,1,XX_DATE)
XX_SH(Name)
XX_PRODUCT XX_DASHED()
XX_SM(IBM)
host printing tool
XX_SH(Synopsis)
XX_FB(XX_PRODUCT)
[ XX_FI(options) ]       
[ L: ] [[ XX_FI(LUname) [, XX_FI(LUname) ...]@] XX_FI(hostname) [: XX_FI(port) ]] 
XX_SH(Description)
XX_FB(XX_PRODUCT)
opens a telnet connection to an
XX_SM(IBM)
host, and emulates an XX_SM(IBM) 3287 printer.
It implements RFCs 2355 (TN3270E), 1576 (TN3270) and 1646 (LU name selection).
XX_LP
If the XX_FI(hostname) is prefixed with XX_FB(L:), the connection will be made
through an SSL tunnel.
XX_FB(XX_PRODUCT) also supports TELNET START-TLS option negotiation without any
need for command-line options.
XX_LP
A specific LU name to use may be specified by prepending it to the
XX_FI(hostname)
with an
XX_DQUOTED(XX_FB(@)).
Multiple LU names to try can be separated by commas.
An empty LU can be placed in the list with an extra comma.
XX_LP
The port to connect to defaults to
XX_FB(telnet).
This can be overridden by appending a
XX_FI(port)
to the
XX_FI(hostname)
with a colon
XX_DQUOTED(XX_FB(:)).
XX_SH(Options)
XX_FB(XX_PRODUCT)
understands the following options:
XX_TPS()dnl
XX_TP(XX_FB(XX_DASHED(accepthostname)) XX_FI(spec))
Specifies a particular hostname to accept when validating the name presented
in the host's SSL certificate, instead of comparing to the name or address
used to make the connection.
XX_FI(spec) can either be XX_FB(any), which
disables name validation, XX_FB(DNS:)`'XX_FI(hostname), which matches a
particular DNS hostname, or XX_FB(IP:)`'XX_FI(address), which matches a
particular numeric IPv4 or IPv6 address.
XX_TP(XX_FB(XX_DASHED(assoc)) XX_FI(LUname))
Causes the session to be associated with the specified
XX_FI(LUname).
XX_TP(XX_FB(XX_DASHED(blanklines)))
In LU3 formatted mode, print blank lines even if they are all NULLs or control
characters.
(This is a violation of the 3270 printer protocol, but some hosts require it.)
XX_TP(XX_FB(XX_DASHED(cadir)) XX_FI(directory))
Specifies a directory containing CA (root) certificates to use when verifying a
certificate provided by the host.
XX_TP(XX_FB(XX_DASHED(cafile)) XX_FI(filename))
Specifies a XX_SM(PEM)-format file containing CA (root) certificates to use
when verifying a certificate provided by the host.
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
Specifies an alternate host code page (input XX_SM(EBCDIC) mapping).
The default maps the U.S. English (037) code page to the
ifelse(XX_PRODUCT,pr3287,`current locale character encoding.',
`the system ANSI code page (unless overridden by the XX_DASHED(printercp) option).')
XX_PRODUCT generally supports the same host character sets as
ifelse(XX_PRODUCT,pr3287,x3270,wc3270).
ifelse(XX_PRODUCT,pr3287,`XX_TP(XX_FB(XX_DASHED(command)) XX_FI(command))
Specifies the command to run for each print job.
The default is XX_FB(lpr).')
ifelse(XX_PRODUCT,pr3287,`XX_TP(XX_FB(XX_DASHED(crlf)))
Causes newline characters in the output to be expanded to
carriage-return/linefeed sequences.',
`XX_TP(XX_FB(XX_DASHED(nocrlf)))
Causes newline characters in the output to be left as-is, and not expanded to
carriage-return/linefeed sequences.')
XX_TP(XX_FB(XX_DASHED(crthru)))
In unformatted 3270 mode, causes XX_SM(EBCDIC) CR orders to be passed to
directly to the printer as XX_SM(ASCII) CR characters, and the output buffer to
be flushed, instead of being specially interpreted by XX_FI(XX_PRODUCT).
XX_IP
By default, XX_SM(EBCDIC) CRs cause the (virtual) print head to return to
column 0, so that subsequent text overwrites what is already in the buffer,
and the buffer is flushed only when an XX_SM(EBCDIC) NL or EM order is
received.
ifelse(XX_PRODUCT,pr3287,`XX_TP(XX_FB(XX_DASHED(daemon)))
Causes
XX_FI(XX_PRODUCT)
to become a daemon (background) process.')
XX_TP(XX_FB(XX_DASHED(eojtimeout)) XX_FI(seconds))
Causes XX_FI(XX_PRODUCT) to complete the print job after XX_FI(seconds) seconds
of inactivity.
XX_TP(XX_FB(XX_DASHED(emflush)))
Causes XX_FI(XX_PRODUCT) to flush any pending printer output whenever an EM
(End of Medium) order arrives in unformatted 3270 mode.
This can help preserve multi-page output with hosts that do not clear the 3270
buffer between pages.
(Note: This option is defined for historical purposes only; XX_FB(XX_DASHED(emflush)) is now
the default.)
XX_TP(XX_FB(XX_DASHED(noemflush)))
Causes XX_FI(XX_PRODUCT) not to flush any pending printer output when an EM
(End of Medium) order arrives in unformatted 3270 mode.
XX_TP(XX_FB(XX_DASHED(ignoreeoj)))
Ignore TN3270E PRINT-EOJ commands, relying on UNBIND commands to indicate
the ends of print jobs.
XX_TP(XX_FB(XX_DASHED(ffeoj)))
Causes XX_FI(XX_PRODUCT) to add a FF (formfeed) at the end of each print job.
XX_TP(XX_FB(XX_DASHED(ffskip)))
Causes XX_FI(XX_PRODUCT) to ignore a FF (formfeed) order if it occurs
at the top of a page.
XX_TP(XX_FB(XX_DASHED(ffthru)))
In SCS mode, causes XX_FI(XX_PRODUCT) to pass FF (formfeed) orders through to the
printer as ASCII formfeed characters, rather than simulating them based on the
values of the MPL (maximum presentation line) and TM (top margin) parameters.
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
XX_TP(XX_FB(XX_DASHED(mpp) XX_FI(n)))
Specifies a non-default value for the Maximum Presentation Position (the
line length for unformatted Write commands).
The default is 132.
The minimum is 40 and the maximum is 256.
ifelse(XX_PRODUCT,wpr3287,`XX_TP(XX_FB(XX_DASHED(printer)) XX_FI(printer))
Specifies the Windows printer to use for each print job.
The default is to use the printer specified by the XX_FB($PRINTER) environment
variable, if defined, and otherwise to use the default Windows printer.
XX_LP
The printer can be the name of a local printer, or a UNC path to a remote
printer, e.g., <b>\\server\printer1</b>.
XX_TP(XX_FB(XX_DASHED(printercp)) XX_FI(codepage))
Specifies the code page to use when generating printer output.
The default is to use the system ANSI code page.')
XX_TP(XX_FB(XX_DASHED(proxy) XX_FI(type):XX_FI(host)[:XX_FI(port)]))
Causes XX_FB(XX_PRODUCT) to connect via the specified proxy, instead of
using a direct connection.
The XX_FI(host) can be an IP address or hostname.
The optional XX_FI(port) can be a number or a service name.
For a list of supported proxy XX_FI(types), see XX_LINK(#Proxy,XX_SM(PROXY))
below.
XX_TP(XX_FB(XX_DASHED(reconnect)))
Causes XX_FI(XX_PRODUCT) to reconnect to the host, whenever the connection is
broken.
There is a 5-second delay between reconnect attempts, to reduce network
thrashing for down or misconfigured hosts.
XX_TP(XX_FB(XX_DASHED(selfsignedok)))
Allow self-signed host certificates.
XX_TP(XX_FB(XX_DASHED(skipcc)))
For unformatted writes, skip ASA carriage control characters (e.g., blank for
single-space, `0' for double-space, `1' for formfeed, etc.) in the first
position of each line of host output.
XX_TP(XX_FB(XX_DASHED(trace)))
Turns on data stream tracing.
Trace information is usually saved in the file
ifelse(XX_PRODUCT,pr3287,`XX_FB(/tmp/x3trc.)`'XX_FI(pid).',
`XX_FB(x3trc.)`'XX_FI(pid)`'XX_FB(.txt).')
XX_TP(XX_FB(XX_DASHED(tracedir)) XX_FI(dir))
Specifies the directory to save trace files in, instead of
ifelse(XX_PRODUCT,pr3287,XX_FB(/tmp), the current directory).
XX_TP(XX_FB(XX_DASHED(trnpre) XX_FI(file)))
Specifies a file containing data that will be sent to the printer before each
print job.
The file contents are treated as transparent data, i.e., they are not
translated in any way.
XX_TP(XX_FB(XX_DASHED(trnpost) XX_FI(file)))
Specifies a file containing data that will be sent to the printer after each
print job.
The file contents are treated as transparent data, i.e., they are not
translated in any way.
XX_TP(XX_FB(XX_DASHED(v)))
Display build and version information and exit.
XX_TP(XX_FB(XX_DASHED(verifycert)))
Verify the host certificate for tunneled SSL and negotiated SSL/TLS
connections.
XX_TP(XX_FB(XX_DASHED(xtable) XX_FI(file)))
Specifies a file containing transparent data translations.
The file specifies EBCDIC characters that will be translated into transparent
ASCII data (data that will not be further translated and will not count as
taking up column(s) on the print line).
Any printable ECBDIC code can be translated to zero or more ASCII characters.
XX_IP
The table consists of lines that look like:
XX_RS(XX_FB(ebcdic) XX_FI(ebcdic-code) XX_FB(ascii) XX_FI(ascii-code)...
)
XX_IP
The XX_FI(ebcdic-code) can be specified in hexadecimal X'XX_FI(nn)' notation or
as numbers in decimal, octal (with a leading 0) or hexadecimal (with a leading
0x).
The XX_FI(ascii-code)s can be specified as numbers in decimal, octal or
hexadecimal, control codes such as XX_FB(^X), symbolic control codes such
as XX_FB(CR) or XX_FB(Escape), or as double-quoted strings, following the
full C-language conventions, such as XX_FB(XX_BACKSLASH(r)) for a carriage return.
Comments begin with XX_FB(#), XX_FB(!) or XX_FB(//).
XX_IP
Here are some examples of translations.
XX_BR
# Expand EBCDIC D to an escape sequence.
XX_BR
ebcdic X'C4' ascii Esc "]1,3" 0x6d
XX_BR
# Delete EBCDIC XX_POSESSIVE(B)
XX_BR
ebcdic X'C2' ascii
XX_IP
The full list of symbolic control codes is: XX_FB(BS CR BEL ESC ESCAPE FF HT LF NL NUL SPACE TAB VT).
XX_TPE()dnl
ifelse(XX_PRODUCT,pr3287,`XX_SH(Signals)
SIGINT, SIGHUP and SIGTERM cause the current print job to be flushed (any
pending data to be printed) and XX_FI(XX_PRODUCT) to exit.
XX_LP()
SIGUSR1 causes the current print job to be flushed without otherwise
affecting the XX_FI(XX_PRODUCT) process.')
ifelse(XX_PRODUCT,wpr3287,`XX_SH(Environment)
XX_TPS()dnl
XX_TP(XX_FB(PRINTER))
Specifies the Windows printer to use for print jobs.
The XX_FB(XX_DASHED(printer)) command-line option overrides XX_FB($PRINTER).
XX_TPE()dnl')
XX_SH(Proxy)
The XX_FB(XX_DASHED(proxy)) option
causes XX_PRODUCT to use a proxy server to connect to the host.
The syntax of the option is:
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
XX_SH(See Also)
ifelse(XX_PRODUCT,pr3287,`x3270(1), c3270(1), telnet(1), tn3270(1)',
`wc3270(1)')
XX_BR
Data Stream Programmer's Reference, IBM GA23`'XX_DASHED(0059)
XX_BR
Character Set Reference, IBM GA27`'XX_DASHED(3831)
XX_BR
3174 Establishment Controller Functional Description, IBM GA23`'XX_DASHED(0218)
XX_BR
RFC 1576, TN3270 Current Practices
XX_BR
RFC 1646, TN3270 Extensions for LUname and Printer Selection
XX_BR
RFC 2355, TN3270 Enhancements
XX_SH(Copyrights)
Copyright`'XX_COPY()1993-2014, Paul Mattes.
XX_BR
Copyright`'XX_COPY()1990, Jeff Sparkes.
XX_BR
Copyright`'XX_COPY()1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 30332.
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
Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
their contributors may be used to endorse or promote products derived
from this software without specific prior written permission.
XX_LP()
THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC XX_DQUOTED(AS IS) AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
XX_SH(Version)
XX_PRODUCT XX_VERSION_NUMBER
