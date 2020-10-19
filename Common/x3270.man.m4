dnl Copyright (c) 1993-2020, Paul Mattes.
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
XX_TH(XX_PRODUCT,1,XX_DATE)
XX_SH(Name)
XX_PRODUCT XX_DASH
ifelse(XX_PRODUCT,c3270,`curses-based 
')dnl
XX_SM(IBM) host access tool
ifelse(XX_PRODUCT,b3270,`back end
')dnl
XX_SH(Synopsis)
XX_FB(XX_PRODUCT)
[XX_FI(options)]
ifelse(XX_PRODUCT,b3270,,`[XX_FI(host)]')
XX_BR
XX_FB(XX_PRODUCT) [XX_FI(options)] XX_FI(session-file).XX_PRODUCT
XX_SH(Description)
XX_FB(XX_PRODUCT) opens a telnet connection to an XX_SM(IBM)
ifelse(XX_PRODUCT,x3270,`host in an X window.',
XX_PRODUCT,s3270,`host, then allows a script to control the host login session.
It is derived from
XX_LINK(x3270-man.html,XX_FI(x3270)(1)),
an X-windows IBM 3270 emulator.',
XX_PRODUCT,ws3270,`host, then allows a script to control the host login session.',
XX_MODE,console,`host in a console window.',
XX_PRODUCT,b3270,`host, handling the 3270, TELNET and TLS protocols,
allowing a front-end application handle user interactions.
It uses XML on its standard input and standard output to communicate with the
front end.
')
It implements RFCs 2355 (TN3270E), 1576 (TN3270) and 1646 (LU name selection),
and supports IND$FILE file transfer.
ifelse(XX_PRODUCT,x3270,
`The window created by XX_FB(XX_PRODUCT)
can use its own font for displaying characters, so it is a fairly accurate
representation of an XX_SM(IBM) 3278 or 3279.
It is similar to XX_FI(tn3270)(1) except that it is X11-based, not curses-based.
')dnl
ifelse(XX_PRODUCT,c3270,
`If the console is capable of displaying colors, then XX_FB(c3270) emulates an
XX_SM(IBM) 3279.  Otherwise, it emulates a 3278.
')dnl
XX_SH(Wiki)
Primary documentation for XX_PRODUCT is on the XX_FB(x3270 Wiki), XX_LINK(https://x3270.miraheze.org/wiki/Main_Page,https://x3270.miraheze.org/wiki/Main_Page).
XX_SH(Version)
XX_PRODUCT XX_VERSION_NUMBER
