/*
 * Copyright 2001-2008 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * pr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	charset.c
 *		Limited character set support.
 */

#include "globals.h"

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#if !defined(_WIN32) /*[*/
#include <locale.h>
#include <langinfo.h>
#endif /*]*/

#if defined(__CYGWIN__) /*[*/
#include <w32api/windows.h>
#undef _WIN32
#endif /*]*/

#include "3270ds.h"
#include "charsetc.h"
#include "unicodec.h"
#include "unicode_dbcsc.h"
#include "utf8c.h"

unsigned long cgcsgid = 0x02b90025;
unsigned long cgcsgid_dbcs = 0x02b90025;
int dbcs = 0;

char *encoding = CN;
char *converters = CN;

/*
 * Change character sets.
 * Returns 0 if the new character set was found, -1 otherwise.
 */
enum cs_result
charset_init(char *csname)
{
#if !defined(_WIN32) /*[*/
	char *codeset_name;
#endif /*]*/
    	const char *codepage;
	const char *display_charsets;

#if !defined(_WIN32) /*[*/
	setlocale(LC_ALL, "");
	codeset_name = nl_langinfo(CODESET);
#if defined(__CYGWIN__) /*[*/
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
#endif /*]*/
	set_codeset(codeset_name);
#endif /*]*/

    	if (set_uni(csname, &codepage, &display_charsets) < 0)
		return CS_NOTFOUND;
	cgcsgid = strtoul(codepage, NULL, 0);
	if (!(cgcsgid & ~0xffff))
	    	cgcsgid |= 0x02b90000;

#if defined(X3270_DBCS) /*[*/
	if (set_uni_dbcs(csname, &codepage, &display_charsets) == 0) {
	    	dbcs = 1;
		cgcsgid_dbcs = strtoul(codepage, NULL, 0);
	}
#endif /*]*/

	return CS_OKAY;
}
