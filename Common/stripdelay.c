/*
 * Copyright (c) 2026 Paul Mattes.
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
 *	stripdelay.c
 *		A curses-based 3270 Terminal Emulator
 *		Terminfo utility function
 */

#include "globals.h"

#if defined(TEST) /*[*/
# include <assert.h>
#endif /*]*/

#include "stripdelay.h"

/**
 * Strip the delay indications from a termino string.
 *
 * @param[in]	String to strip
 * @returns Copy of string with delays removed.
 */
char *
stripdelay(const char *tstr)
{
    char *copy = Malloc(strlen(tstr) + 1);
    const char *from = tstr;	/* string to copy from */
    char *to = copy;		/* string to copy to */
    const char *dstart = NULL;	/* start of suspected delay string */
    char c;
    enum { XD_BASE, XD_DOLLAR, XD_LT, XD_DIGIT, XD_SUFFIX } state = XD_BASE;
#define ABANDON { \
	while (dstart <= from - 1) { \
	    *to++ = *dstart++; \
	} \
	dstart = NULL; \
        state = XD_BASE; \
    }

    /*
     * Strip delay strings from a terminfo value. With readline, we don't want to use tputs to display the
     * prompt.
     * The delay strings match the regex: $<[0-9]+[~/]*> (with '~' actually being a '*', which I can't put here
     * because it would form a comment bracket).
     */
    while ((c = *from++) != '\0') {
	switch (state) {
	case XD_BASE:
	    if (c == '$') {
		dstart = from - 1;
		state = XD_DOLLAR;
	    } else {
		*to++ = c;
	    }
	    break;
	case XD_DOLLAR:
	    if (c == '<') {
		state = XD_LT;
	    } else {
		ABANDON;
	    }
	    break;
	case XD_LT:
	    if (c >= '0' && c <= '9') {
		state = XD_DIGIT;
	    } else {
		ABANDON;
	    }
	    break;
	case XD_DIGIT:
	    if (c >= '0' && c <= '9') {
		/* keep going */
	    } else if (c == '*' || c == '/') {
		state = XD_SUFFIX;
	    } else if (c == '>') {
		state = XD_BASE;
		dstart = NULL;
	    } else {
		ABANDON;
	    }
	    break;
	case XD_SUFFIX:
	    if (c == '*' || c == '/') {
		/* keep going */
	    } else if (c == '>') {
		state = XD_BASE;
		dstart = NULL;
	    } else {
		ABANDON;
	    }
	    break;
	}
    }
    if (state != XD_BASE) {
	ABANDON;
    }
    *to = '\0';
    return copy;
}

#if defined(TEST) /*[*/
void *
Malloc(size_t size)
{
    return malloc(size);
}

int
main(int argc, char *argv)
{
    static struct {
	const char *source;
	const char *want;
    } testcase[] = {
	/* No-op. */
	{ "abc", "abc" },

	/* Good cases. */
	{ "foo$<10>", "foo" },
	{ "foo$<10>x", "foox" },
	{ "foo$<10/>x", "foox" },
	{ "foo$<10/*>x", "foox" },
	{ "foo$<10*/>x", "foox" },
	{ "foo$<10*>x", "foox" },

	/* Bad cases. */
	{ "foo$<x", "foo$<x" },
	{ "foo$<1x", "foo$<1x" },
	{ "foo$<1/", "foo$<1/" },
	{ "foo$<1*", "foo$<1*" },
	{ "foo$<1*/", "foo$<1*/" },
	{ "foo$<>", "foo$<>" },
	{ NULL, NULL }
    };
    int i;

    for (i = 0; testcase[i].source; i++) {
	char *r = stripdelay(testcase[i].source);

	if (strcmp(r, testcase[i].want)) {
	    fprintf(stderr, "case %d: %s: Wanted %s, got %s\n", i + 1, testcase[i].source, testcase[i].want, r);
	    exit(1);
	}
	free(r);
    }
    return 0;
}
#endif /*]*/
