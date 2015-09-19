dnl Copyright (c) 2000-2012, Paul Mattes.
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
dnl
dnl Man page macros in m4, produces output for nroff -man
changecom()dnl
changequote(<,>)dnl
define(XX_POSESSIVE,$1's)dnl
define(XX_DQUOTED,``$1'')dnl
changequote(`,')dnl
define(XX_SH,.SH `"translit($1,abcdefghijklmnopqrstuvwxyz,ABCDEFGHIJKLMNOPQRSTUVWXYZ)"')dnl
define(XX_TH,.TH $1 $2 "$3")dnl
define(XX_SM,\s-1$1\s+1)dnl
define(XX_LP,.LP)dnl
define(XX_IP,.IP)dnl
define(XX_BR,.br)dnl
define(XX_RS,.RS
$1
.RE)dnl
define(XX_TS,.TS
$2)dnl
define(XX_TR,$1)dnl
define(XX_TD,T{
.na
.nh
$1
T})dnl
define(XX_TDH,T{
.na
.nh
.in +2
.ti -2
$1
T})dnl
define(XX_T_,_)dnl
define(XX_TC,$1)dnl
define(XX_TE,.TE)dnl
define(XX_TPS)dnl
define(XX_TP,.TP
$1)dnl
define(XX_TPE)dnl
define(XX_PP,.PP)dnl
define(XX_RI2,.RI $1 $2)dnl
define(XX_DASH,\-)dnl
define(XX_DASHED,\-$1)dnl
define(XX_FI,\fI$1\fP)dnl
define(XX_FB,\fB$1\fP)dnl
define(XX_NBSP,`\ ')dnl
define(XX_LT,<)dnl
define(XX_BS,\e)dnl
define(XX_TARGET)dnl
define(XX_LINK,$2)dnl
define(XX_COPY,` ')dnl
define(XX_NOT,notsign)dnl
define(XX_BACKSLASH,\\$1)dnl
define(XX_HO)dnl
define(XX_HY,$1\%$2)dnl
dnl Make sure it gets run through tbl first.
'\" t
