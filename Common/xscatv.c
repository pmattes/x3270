/*
 * Copyright (c) 2025 Paul Mattes.
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
 *      xscatv.c
 *              Basic scatv functionality.
 */

#include "globals.h"

#include "varbuf.h"

#include "xscatv.h"

/**
 * Make a string safe for display, as 'cat -v' does.
 * @param[in] s		String to expand.
 * @param[in] len	Length of string.
 * @param[in] quote	If true, add double quotes.
 *
 * @returns Expanded string.
 */
char *
xscatv(const char *s, size_t len, bool quote)
{
    varbuf_t r;

    vb_init(&r);
    if (quote) {
	vb_appends(&r, "\"");
    }
    while (len--) {
	unsigned char uc = (unsigned char)*s++;

	/* Expand this character. */
	switch (uc) {
	case '\b':
	    vb_appends(&r, "\\b");
	    break;
	case '\f':
	    vb_appends(&r, "\\f");
	    break;
	case '\n':
	    vb_appends(&r, "\\n");
	    break;
	case '\r':
	    vb_appends(&r, "\\r");
	    break;
	case '\t':
	    vb_appends(&r, "\\t");
	    break;
	case '\\':
	    vb_appends(&r, "\\\\");
	    break;
	case '"':
	    if (quote) {
		vb_appends(&r, "\\\"");
		break;
	    }
	    /* else fall through */
	default:
	    if (uc < ' ' || uc == 0x7f) {
		vb_appendf(&r, "\\%03o", uc);
	    } else {
		vb_append(&r, (char *)&uc, 1);
	    }
	    break;
	}
    }
    if (quote) {
	vb_appends(&r, "\"");
    }
    return vb_consume(&r);
}
