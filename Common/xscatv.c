/*
 * Copyright (c) 2025-2026 Paul Mattes.
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

#include "unicodec.h"
#if defined(TEST) /*[*/
# include "utf8.h"
#endif /*]*/
#include "varbuf.h"

#include "xscatv.h"

#if defined(TEST) /*[*/
/* When testing, we assume Unicode, and we use our own conversions. */
ucs4_t
multibyte_to_unicode(const char *mb, size_t mb_len, int *consumedp, enum me_fail *errorp)
{
    int nw;
    ucs4_t ucs4;

    /* Translate from UTF-8 to UCS-4. */
    nw = utf8_to_unicode(mb, (int)mb_len, &ucs4);
    if (nw < 0) {
	*errorp = ME_INVALID;
	return 0;
    }
    if (nw == 0) {
	*errorp = ME_SHORT;
	return 0;
    }
    *consumedp = nw;
    return ucs4;
}
#endif /*]*/

/**
 * Make a string safe for display, as 'cat -v' does.
 *
 * @param[in] s		String to expand.
 * @param[in] len	Length of string.
 * @param[in] ulimit	Maximum number of Unicode characters to process, or -1.
 * @param[in] qtype	Quoting type.
 * @param[in] opts	Options.
 *
 * @returns Expanded string.
 */
char *
xscatv(const char *s, size_t len, ssize_t ulimit, enum xscatv_quote qtype, unsigned opts)
{
    varbuf_t r;
    bool quoting = qtype == XSCQ_QUOTE;
    ssize_t nu = 0;

    /* For automatic quoting, scan first to see if quotes are needed. */
    if (qtype == XSCQ_ARGQUOTE || qtype == XSCQ_SHELLQUOTE) {
	const char *t = s;
	size_t tlen = len;
	const char *triggers = (qtype == XSCQ_ARGQUOTE)? " (),\"": " \"";

	while (tlen--) {
	    if (strchr(triggers, *t++) != NULL) {
		quoting = true;
		break;
	    }
	}
    }

    vb_init(&r);
    if (quoting) {
	vb_appends(&r, "\"");
    }
    while (len && (ulimit < 0 || nu < ulimit)) {
	ucs4_t uc;
	int consumed = 0;
	enum me_fail error = ME_NONE;

	uc = multibyte_to_unicode(s, len, &consumed, &error);
	if (error != ME_NONE) {
	    if (error == ME_SHORT) {
		vb_appends(&r, "<incomplete multi-byte>");
	    }
	    if (error == ME_INVALID) {
		vb_appends(&r, "<invalid multi-byte>");
	    }
	    break;
	}

	/* Expand this character. */
	if (uc < ' ' && (!(opts & XSCF_NLTHRU) || uc != '\n')) {
	    /* C0 control character. */
	    vb_appendf(&r, "^%c", '@' + uc);
	} else if (uc == 0x7f) {
	    /* DEL */
	    vb_appends(&r, "^?");
	} else if ((uc & 0x80) && (uc & 0x7f) < ' ') {
	    /* C1 control character. */
	    vb_appendf(&r, "M-^%c", '@' + (uc & 0x7f));
	} else if (uc == 0xa0) {
	    /* No-break space. */
	    vb_appends(&r, "M- ");
	} else if (quoting && uc == '"') {
	    vb_appends(&r, "\\\"");
	} else {
	    /* Legal printable character. */
	    vb_append(&r, s, consumed);
	}
	s += consumed;
	len -= consumed;
	nu++;
    }
    if (quoting) {
	vb_appends(&r, "\"");
    }
    return vb_consume(&r);
}

/**
 * Check a string for display safety.
 *
 * @param[in] s		String to inspect.
 * @param[in] len	Length of string.
 * @param[in] opts	Options.
 *
 * @returns true if safe, false if not
 */
bool
xscatv_safe(const char *s, size_t len, unsigned opts)
{
    while (len) {
	ucs4_t uc;
	int consumed = 0;
	enum me_fail error = ME_NONE;

	uc = multibyte_to_unicode(s, len, &consumed, &error);
	if (error != ME_NONE) {
	    return false;
	}

	/* Check this character. */
	if (((opts & XSCC_WHITESPACE) && isspace(uc)) ||
	    ((opts & XSCC_CONTROLS) && (uc < ' ' || uc == 0x7f || ((uc & 0x80) && (uc & 0x7f) < ' '))) ||
	    ((opts & XSCC_NBSP) && uc == 0xa0) ||
	    ((opts & XSCC_DBSPACE) && uc == 0x3000)) {
	    return false;
	}

	s += consumed;
	len -= consumed;
    }
    return true;
}
