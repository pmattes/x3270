dnl Copyright 2000, 2001, 2002, 2003, 2005 by Paul Mattes.
dnl  Permission to use, copy, modify, and distribute this software and its
dnl  documentation for any purpose and without fee is hereby granted,
dnl  provided that the above copyright notice appear in all copies and that
dnl  both that copyright notice and this permission notice appear in
dnl  supporting documentation.
dnl
dnl x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
dnl be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
dnl for more details.
dnl
dnl Man page macros in m4, produces html output
changequote(<,>)dnl
define(XX_POSESSIVE,$1's)dnl
define(XX_DQUOTED,``$1'')dnl
changequote(`,')dnl
changecom()dnl
define(XX_TH)dnl
define(XX_XL,`translit($1,` ()/',-)')dnl
define(XX_XR,`<a HREF="`#'XX_XL($1)">$1</a><br>')dnl
define(XX_SH,`<a NAME="XX_XL($1)"></a>dnl
divert(1)dnl
XX_XR($1)
divert(2)dnl
<h2>$1</h2>')dnl
define(XX_SM,<font size=-1>$1</font>)dnl
define(XX_LP,<p>)dnl
define(XX_IP,<p>)dnl
define(XX_BR,<br>)dnl
define(XX_RS,<blockquote>$1</blockquote>)dnl
define(XX_TS,<table BORDER cols=$1 width="75%">)dnl
define(XX_TR,<tr>$1</tr>)dnl
define(XX_TD,<td $2>$1</td>)dnl
define(XX_TDH,<td $2>$1</td>)dnl
define(XX_T_)dnl
define(XX_TC,<center>$1</center>)dnl
define(XX_TE,</table>)dnl
define(XX_TPS,<dl>)dnl
define(XX_TPE,</dl>
)dnl
define(XX_TP,<dt>$1</dt><dd>)dnl
define(XX_PP,<p>)dnl
define(XX_DASH,`-')dnl
define(XX_DASHED,`-'$1)dnl
define(XX_FI,<i>$1</i>)dnl
define(XX_FB,<b>$1</b>)dnl
define(XX_NBSP,&nbsp;)dnl
define(XX_LT,&lt;)dnl
define(XX_BS,\)dnl
define(XX_TARGET,<a NAME="$1"></a>)dnl
define(XX_LINK,<a HREF="$1">$2</a>)dnl
define(XX_COPY,` &copy; ')dnl
define(XX_BACKSLASH,``&#92;''$1)dnl
define(XX_NOT,&not;)dnl
define(XX_HO,$1)dnl
dnl Stream 1 has the table of contents, stream 2 the body, stream 3 the tail
divert(1)dnl
<html>
<head>
<title>XX_PRODUCT Manual Page</title>
<link HREF="http://www.w3.org/StyleSheets/Core/Steely" TYPE="text/css" REL="stylesheet">
</head>
<body>
<h1>XX_PRODUCT Manual Page</h1>
<h2>Contents</h2>
<blockquote>
divert(3)dnl
<hr>
<i>
This HTML document and the accompanying troff document were generated with
a set of write-only <b>m4</b> macros and the powerful <b>vi</b> editor.
<br>Last modified XX_DATE.
</i>
</body>
</html>
divert(2)dnl
</blockquote>dnl
