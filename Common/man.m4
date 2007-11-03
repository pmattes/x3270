dnl Copyright 2000, 2001, 2002, 2003, 2005 by Paul Mattes.
dnl  Permission to use, copy, modify, and distribute this software and its
dnl  documentation for any purpose and without fee is hereby granted,
dnl  provided that the above copyright notice appear in all copies and that
dnl  both that copyright notice and this permission notice appear in
dnl  supporting documentation.
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
dnl Make sure it gets run through tbl first.
'\" t
