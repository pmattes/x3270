/*
 * Copyright (c) 2001-2009, 2013-2015, 2019 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	codepage.c
 *		Limited code page support.
 */

#include "globals.h"

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#if !defined(_WIN32) /*[*/
# include <locale.h>
# include <langinfo.h>
#endif /*]*/

#if defined(__CYGWIN__) /*[*/
# include <w32api/windows.h>
#undef _WIN32
#endif /*]*/

#include "3270ds.h"
#include "codepage.h"
#include "unicodec.h"
#include "unicode_dbcs.h"
#include "utf8.h"

#if defined(_WIN32) /*[*/
# define LOCAL_CODEPAGE	CP_ACP
#else /*][*/
# define LOCAL_CODEPAGE	0
#endif /*]*/

unsigned long cgcsgid = 0x02b90025;
unsigned long cgcsgid_dbcs = 0x02b90025;
int dbcs = 0;

char *encoding = NULL;
char *converters = NULL;

/*
 * Change host code pages.
 */
enum cs_result
codepage_init(const char *cpname)
{
#if !defined(_WIN32) /*[*/
    char *codeset_name;
#endif /*]*/
    const char *host_codepage;
    const char *cgcsgid_str;

#if !defined(_WIN32) /*[*/
    setlocale(LC_ALL, "");
    codeset_name = nl_langinfo(CODESET);
# if defined(__CYGWIN__) /*[*/
    /*
     * Cygwin's locale support is quite limited.  If the locale
     * indicates "US-ASCII", which appears to be the only supported
     * encoding, ignore it and use the Windows ANSI code page, which
     * observation indicates is what is actually supported.
     *
     * Hopefully at some point Cygwin will start returning something
     * meaningful here and this logic will stop triggering.
     *
     * If this (lack of) functionality persists, then it will probably
     * become necessary for pr3287 to support the wpr3287 '-printercp'
     * option, so that the printer code page can be configured.
     */
    if (!strcmp(codeset_name, "US-ASCII")) {
	codeset_name = Malloc(64);
	sprintf(codeset_name, "CP%d", GetACP());
    }
# endif /*]*/
    set_codeset(codeset_name, false);
#endif /*]*/

    if (!set_uni(cpname, LOCAL_CODEPAGE, &host_codepage, &cgcsgid_str, NULL,
		NULL)) {
	return CS_NOTFOUND;
    }
    cgcsgid = strtoul(cgcsgid_str, NULL, 0);
    if (!(cgcsgid & ~0xffff)) {
	cgcsgid |= 0x02b90000;
    }

    if (set_uni_dbcs(cpname, &cgcsgid_str) == 0) {
	dbcs = 1;
	cgcsgid_dbcs = strtoul(cgcsgid_str, NULL, 0);
    }

    return CS_OKAY;
}
