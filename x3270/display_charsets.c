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
 *	display_charsets.c
 *		Display character set lookup.
 */
#include "globals.h"

#include "display_charsets.h"

typedef struct {
    const char *name;
    const char *display_charset;
} dcs_t;

static dcs_t dcs[] = {
    { "cp037", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp273", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp275", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp277", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp278", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp280", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp284", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp285", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp297", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp424", "3270cg-8,iso10646-1,iso8859-8" },
    { "cp500", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp803", "3270cg-8,iso10646-1,iso-8859-8" },
    { "cp870", "iso10646-1,iso8859-2" },
    { "cp871", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp875", "3270cg-7,iso10646-1,iso8859-7" },
    { "cp880", "iso10646-1,koi8-r" },
    { "cp930", "iso10646-1,jisx0201.1976-0" },
    { "cp935", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp937", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp939", "iso10646-1,jisx0201.1976-0" },
    { "cp1026", "iso10646-1,iso8859-9" },
    { "cp1047", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp1140", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1141", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1142", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1143", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1144", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1145", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1146", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1147", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1148", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1149", "3270cg-1a,3270cg-1,iso10646-1,iso8859-15" },
    { "cp1160", "iso10646-1,iso8859-11" },
    { "cp1388", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { "cp1390", "iso10646-1,jisx0201.1976-0" },
    { "cp1399", "iso10646-1,jisx0201.1976-0" },
    { "apl", "3270cg-1a,iso10646-1" },
    { "bracket", "3270cg-1a,3270cg-1,iso10646-1,iso8859-1" },
    { NULL, NULL }
};

/**
 * Return the X11 SBCS display character sets for a given host character set
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
lookup_display_charset(const char *charset_name)
{
    int i;

    for (i = 0; dcs[i].name != NULL; i++) {
	if (!strcasecmp(charset_name, dcs[i].name)) {
	    return dcs[i].display_charset;
	}
    }
    return NULL;
}
