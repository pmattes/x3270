/*
 * Copyright (c) 2014-2024 Paul Mattes.
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
 *      percent_decode.c
 *              URI percent decoder.
 */

#include "globals.h"

#include "varbuf.h"

#include "percent_decode.h"

/**
 * Translate a hex digit to a number.
 *
 * @param[in] c		Digit character
 *
 * @return Value, or -1 if not a valid digit
 */
static int
hex_digit(char c)
{
    static const char *xlc = "0123456789abcdef";
    static const char *xuc = "0123456789ABCDEF";
    char *x;

    x = strchr(xlc, c);
    if (x != NULL) {
	return (int)(x - xlc);
    }
    x = strchr(xuc, c);
    if (x != NULL) {
	return (int)(x - xuc);
    }
    return -1;
}

/**
 * Do percent substitution decoding on a URI element.
 *
 * @param[in] uri	URI to parse
 * @param[in] len	Length of URI
 * @param[in] plus	Translate '+' to ' ' as well
 *
 * @return Translated, newly-allocated and NULL-terminated URI, or NULL if
 *  there is a syntax error
 */
char *
percent_decode(const char *uri, size_t len, bool plus)
{
    enum {
	PS_BASE,	/* base state */
	PS_PCT,		/* saw % */
	PS_HEX1		/* saw % and one hex digit */
    } state = PS_BASE;
    int hex1 = 0, hex2;
    const char *s;
    char c;
    varbuf_t r;
    char xc;

    vb_init(&r);

    /* Walk and translate. */
    s = uri;
    while (s < uri + len) {
	c = *s++;

	switch (state) {
	case PS_BASE:
	    if (c == '%') {
		state = PS_PCT;
	    } else {
		if (plus && c == '+') {
		    vb_appends(&r, " ");
		} else {
		    vb_append(&r, &c, 1);
		}
	    }
	    break;
	case PS_PCT:
	    hex1 = hex_digit(c);
	    if (hex1 < 0) {
		vb_free(&r);
		return NULL;
	    }
	    state = PS_HEX1;
	    break;
	case PS_HEX1:
	    hex2 = hex_digit(c);
	    if (hex2 < 0) {
		vb_free(&r);
		return NULL;
	    }
	    xc = (hex1 << 4) | hex2;
	    vb_append(&r, &xc, 1);
	    state = PS_BASE;
	    break;
	}
    }

    /* If we end with a partially-digested sequence, fail. */
    if (state != PS_BASE) {
	vb_free(&r);
	return NULL;
    }

    /* Done. */
    return vb_consume(&r);
}
