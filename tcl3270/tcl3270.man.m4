dnl Copyright (c) 1993-2018, Paul Mattes.
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
XX_SM(IBM) host access tool
XX_SH(Synopsis)
XX_FB(XX_PRODUCT)
[XX_FI(script) [XX_FI(script-arg...)]] [-- [XX_FI(tcl3270-options)] [XX_FI(XX_S3270`'-options)] [XX_FI(host)]
XX_BR
XX_FB(XX_PRODUCT)
[XX_FI(script) [XX_FI(script-arg...)]] [-- [XX_FI(tcl3270-options)] [XX_FI(XX_S3270`'-options)] [XX_FI(session-file).XX_PRODUCT]
XX_BR
XX_FB(XX_PRODUCT) -v
XX_BR
XX_FB(XX_PRODUCT) --help
XX_SH(Description)
XX_FB(XX_PRODUCT) opens a telnet connection to an XX_SM(IBM)
host, then allows a Tcl script to control the host login session.
It is derived from
XX_LINK(XX_S3270-man.html,XX_FI(XX_S3270)(1)),
a script-based IBM 3270 emulator.
XX_LP
For each action supported by XX_S3270, XX_PRODUCT defines a
correcponding Tcl command.
The result of the Tcl command is the output of the XX_S3270 action.
If the output is one line, the result is a string.
If the output is multiple lines, the result is a list of strings.
XX_LP
On the command line, a session file can be named either
XX_FI(name).XX_FB(XX_PRODUCT) or XX_FI(name).XX_FB(XX_S3270).
Resource values, such as those used in XX_FB(XX_DASHED(xrm)) options or in a
session file, can be specified as XX_FB(XX_PRODUCT).XX_FI(resource) or
XX_FB(XX_S3270).XX_FI(resource).
XX_SH(XX_PRODUCT-Specific Options)
XX_TP(XX_FB(XX_DASHED(d)))
Turns on debugging information, tracing data going between XX_PRODUCT and
XX_S3270.
XX_SH(See Also)
XX_LINK(XX_S3270-man.html,XX_S3270`'(1))
XX_SH(Wiki)
Primary documentation for XX_PRODUCT is on the XX_FB(x3270 Wiki), XX_LINK(https://x3270.miraheze.org/wiki/Main_Page,https://x3270.miraheze.org/wiki/Main_Page).
XX_SH(Version)
XX_PRODUCT XX_VERSION_NUMBER
