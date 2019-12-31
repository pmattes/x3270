dnl Copyright (c) 2018, Paul Mattes.
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
dnl THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND ANY EXPRESS OR IMPLIED
dnl WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
dnl EVENT SHALL PAUL MATTES OR JEFF SPARKES BE LIABLE FOR ANY DIRECT, INDIRECT,
dnl INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
dnl NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
dnl DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
dnl THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
dnl (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
dnl THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
XX_TH(XX_PRODUCT,1,XX_DATE)
XX_SH(Name)
XX_PRODUCT XX_DASH network stream trace facility
XX_SH(Synopsis)
XX_FB(XX_PRODUCT) [XX_DASHED(p) XX_FI(listenport)] [XX_DASHED(f) XX_FI(outfile)]
XX_SH(Description)
XX_FB(XX_PRODUCT) is a proxy server that traces the data passing through it.
It supports the Sun XX_FI(passthru) protocol, where the client writes the
desired host name and port, separated by a space and terminated by a carriage
return and line feed, at the beginning of the session.
XX_LP
Network data is written in hexadecimal to the specified file.
XX_LP
The name is derived from its position in the network stream: the man in the
middle.
XX_SH(Options)
XX_TP(XX_FB(XX_DASHED(p)) XX_FI(listenport))
Specifies the port to listen on.
The default port is 4200.
XX_TP(XX_FB(XX_DASHED(f)) XX_FI(outfile))
Specifies the trace file to create.
The default is
ifelse(XX_PLATFORM,unix,/tmp/mitm.XX_FI(pid),mitm.XX_FI(pid).txt on the
Desktop).
XX_SH(Example)
The emulator command-line option to route connection through XX_FB(mitm)
is:
XX_IP
XX_RS(XX_DASHED(proxy) passthru:127.0.0.1:4200)
XX_SH(See Also)
XX_LINK(XX_S3270-man.html,XX_S3270`'(1))`, '
ifelse(XX_PLATFORM,unix,XX_LINK(XX_X3270-man.html,XX_X3270`'(1))`, ')dnl
XX_LINK(XX_C3270-man.html,XX_C3270`'(1))
XX_SH(Copyrights)
Copyright`'XX_COPY()2018-XX_CYEAR, Paul Mattes.
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
Neither the name of Paul Mattes nor the names of his contributors may be used
to endorse or promote products derived from this software without specific
prior written permission.
XX_TPE()
XX_LP
THIS SOFTWARE IS PROVIDED BY PAUL MATTES
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
XX_SH(Version)
XX_PRODUCT XX_VERSION_NUMBER
