dnl Copyright (c) 2000-2013, Paul Mattes.
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
define(XX_HY,$1$2)dnl
dnl Stream 1 has the table of contents, stream 2 the body, stream 3 the tail
divert(1)dnl
<html>
<head>
<title>XX_PAGENAME Manual Page</title>
<link HREF="http://www.w3.org/StyleSheets/Core/Steely" TYPE="text/css" REL="stylesheet">
</head>
<body>
<h1>XX_PAGENAME Manual Page</h1>
<hr>
<p><b>Note:</b> This page is no longer being maintained for XX_PAGENAME 4.0 and later.
Please refer to the <a href="https://x3270.miraheze.org/wiki/Main_Page">the x3270 Wiki</a> for up-to-date documentation.</p>
<hr>
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
