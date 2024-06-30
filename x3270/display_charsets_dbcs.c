/*
 * Copyright (c) 2008-2024 Paul Mattes.
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
 *	display_charsets_dbcs.c
 *		DBCS display chararacter set lookup.
 */
#include "globals.h"

#include "display_charsets_dbcs.h"

/*
 * Note: #undef'ing X3270_DBCS disables the ability to configure a DBCS host
 *  codepage, but it does not disable the internal logic that supports DBCS.
 *  Its purpose is to save space in the executable by removing the translation
 *  tables, not by turning the code into #ifdef spaghetti.
 */

/*
 * DBCS EBCDIC-to-Unicode translation tables.
 */

typedef struct {
    const char *name;
    const char *display_charset;
} dcd_t;

static dcd_t dcd[] = {
    { "cp930",  "jisx0208.1983-0,iso10646-1" },
    { "cp935",  "gb2312.1980-0,iso10646-1"   },
    { "cp937",  "big5-0,iso10646-1"          },
    { "cp939",  "jisx0208.1983-0,iso10646-1" },
    { "cp1388", "gb18030.2000-1,iso10646-1"  },
    { "cp1390", "jisx0208.1983-0,iso10646-1" },
    { "cp1399", "jisx0208.1983-0,iso10646-1" },
    { NULL, NULL }
};

/**
 * Return the X11 DBCS display character sets for a given host character set
 * (code page).
 *
 * Does not support aliases. If the user-supplied name is an alias, then the
 * canonical name must be used instead.
 *
 * @param[in] charset_name	Canonical chararcter set name
 *
 * @return Comma-separated list of display character sets, or NULL if no match
 * is found.
 */
const char *
lookup_display_charset_dbcs(const char *charset_name)
{
    int i;

    for (i = 0; dcd[i].name != NULL; i++) {
	if (!strcasecmp(charset_name, dcd[i].name)) {
	    return dcd[i].display_charset;
	}
    }
    return NULL;
}
