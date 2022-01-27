/*
 * Copyright (c) 2022 Paul Mattes.
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
 *      utf8.c
 *              JSON parser/formatter unit tests
 */

#include "globals.h"

#include <assert.h>

#include "utf8.h"

static void positive_encode_tests(void);
static void negative_encode_tests(void);
static void positive_decode_tests(void);
static void negative_decode_tests(void);

static struct {
    const char *name;
    void (*function)(void);
} test[] = {
    { "Positive encode", positive_encode_tests },
    { "Negative encode", negative_encode_tests },
    { "Positive decode", positive_decode_tests },
    { "Negative decode", negative_decode_tests },
    { NULL, NULL }
};

int
main(int argc, char *argv[])
{
    int i;
    bool verbose = false;

    if (argc > 1 && !strcmp(argv[1], "-v")) {
	verbose = true;
    }

    /* Loop through the tests. */
    for (i = 0; test[i].name != NULL; i++) {
	(*test[i].function)();
	if (verbose) {
	    printf("%s test - PASS\n", test[i].name);
	} else {
	    printf(".");
	    fflush(stdout);
	}
    }

    /* Success. */
    printf("\nPASS\n");
    return 0;
}

#if false
static void
dump_unicode(ucs4_t u, int len, char buf[])
{
    int i;

    printf("U+%04x ", u);
    for (i = 0; i < len; i++) {
	printf("\\x%02x", buf[i] & 0xff);
    }
    printf("\n");
}
#endif

/* Positive encode tests. */
static void
positive_encode_tests(void)
{
    char buf[6];

    /* 1-byte range. */
    assert(unicode_to_utf8(0, buf) == 1);
    assert(!memcmp("\x00", buf, 1));
    assert(unicode_to_utf8(0x7f, buf) == 1);
    assert(!memcmp("\x7f", buf, 1));

    /* 2-byte range. */
    assert(unicode_to_utf8(0x80, buf) == 2);
    assert(!memcmp("\xc2\x80", buf, 2));
    assert(unicode_to_utf8(0x7ff, buf) == 2);
    assert(!memcmp("\xdf\xbf", buf, 2));

    /* 3-byte range. */
    assert(unicode_to_utf8(0x800, buf) == 3);
    assert(!memcmp("\xe0\xa0\x80", buf, 3));
    assert(unicode_to_utf8(0xffff, buf) == 3);
    assert(!memcmp("\xef\xbf\xbf", buf, 3));

    /* 4-byte range. */
    assert(unicode_to_utf8(0x10000, buf) == 4);
    assert(!memcmp("\xf0\x90\x80\x80", buf, 4));
    assert(unicode_to_utf8(0x1fffff, buf) == 4);
    assert(!memcmp("\xf7\xbf\xbf\xbf", buf, 4));

    /* 5-byte range. */
    assert(unicode_to_utf8(0x200000, buf) == 5);
    assert(!memcmp("\xf8\x88\x80\x80\x80", buf, 5));
    assert(unicode_to_utf8(0x3ffffff, buf) == 5);
    assert(!memcmp("\xfb\xbf\xbf\xbf\xbf", buf, 5));

    /* 6-byte range. */
    assert(unicode_to_utf8(0x4000000, buf) == 6);
    assert(!memcmp("\xfc\x84\x80\x80\x80\x80", buf, 6));
    assert(unicode_to_utf8(0x7fffffff, buf) == 6);
    assert(!memcmp("\xfd\xbf\xbf\xbf\xbf\xbf", buf, 6));
}

/* Negative encode tests. */
static void
negative_encode_tests(void)
{
    char buf[6];

    assert(unicode_to_utf8(0x80000000, buf) < 0);
    assert(unicode_to_utf8(0xffffffff, buf) < 0);
}

/* Positive decode tests. */
static void
positive_decode_tests(void)
{
    ucs4_t u;

    /* 1-byte range. */
    assert(utf8_to_unicode("\x00", 1, &u) == 1);
    assert(u == 0);
    assert(utf8_to_unicode("\x7f", 1, &u) == 1);
    assert(u == 0x7f);

    /* 2-byte range. */
    assert(utf8_to_unicode("\xc2\x80", 2, &u) == 2);
    assert(u == 0x80);
    assert(utf8_to_unicode("\xdf\xbf", 2, &u) == 2);
    assert(u == 0x7ff);

    /* 3-byte range. */
    assert(utf8_to_unicode("\xe0\xa0\x80", 3, &u) == 3);
    assert(u == 0x800);
    assert(utf8_to_unicode("\xef\xbf\xbf", 3, &u) == 3);
    assert(u == 0xffff);

    /* 4-byte range. */
    assert(utf8_to_unicode("\xf0\x90\x80\x80", 4, &u) == 4);
    assert(u == 0x10000);
    assert(utf8_to_unicode("\xf7\xbf\xbf\xbf", 4, &u) == 4);
    assert(u == 0x1fffff);

    /* 5-byte range. */
    assert(utf8_to_unicode("\xf8\x88\x80\x80\x80", 5, &u) == 5);
    assert(u == 0x200000);
    assert(utf8_to_unicode("\xfb\xbf\xbf\xbf\xbf", 5, &u) == 5);
    assert(u == 0x3ffffff);

    /* 6-byte range. */
    assert(utf8_to_unicode("\xfc\x84\x80\x80\x80\x80", 6, &u) == 6);
    assert(u == 0x4000000);
    assert(utf8_to_unicode("\xfd\xbf\xbf\xbf\xbf\xbf", 6, &u) == 6);
    assert(u == 0x7fffffff);
}

/* Negative decode tests. */
static void
negative_decode_tests(void)
{
    ucs4_t u;

    /* Incomplete. */
    assert(utf8_to_unicode("\xc2\x80", 0, &u) == 0);
    assert(utf8_to_unicode("\xc2\x80", 1, &u) == 0);
    assert(utf8_to_unicode("\xe0\xa0\x80", 2, &u) == 0);
    assert(utf8_to_unicode("\xf0\x90\x80\x80", 3, &u) == 0);
    assert(utf8_to_unicode("\xf8\x88\x80\x80\x80", 4, &u) == 0);
    assert(utf8_to_unicode("\xfc\x84\x80\x80\x80\x80", 5, &u) == 0);

    /* Invalid. */
    assert(utf8_to_unicode("\xc2\x01", 2, &u) == -1);
    assert(utf8_to_unicode("\xe0\xa0\x01", 3, &u) == -1);
    assert(utf8_to_unicode("\xe0\x01\x01", 3, &u) == -1);
    assert(utf8_to_unicode("\xf0\x90\x80\x01", 4, &u) == -1);
    assert(utf8_to_unicode("\xf0\x90\x01\x01", 4, &u) == -1);
    assert(utf8_to_unicode("\xf0\x01\x01\x01", 4, &u) == -1);
    assert(utf8_to_unicode("\xf8\x88\x80\x80\x01", 5, &u) == -1);
    assert(utf8_to_unicode("\xf8\x88\x80\x01\x01", 5, &u) == -1);
    assert(utf8_to_unicode("\xf8\x88\x01\x01\x01", 5, &u) == -1);
    assert(utf8_to_unicode("\xf8\x01\x01\x01\x01", 5, &u) == -1);
    assert(utf8_to_unicode("\xfc\x84\x80\x80\x80\x01", 6, &u) == -1);
    assert(utf8_to_unicode("\xfc\x84\x80\x80\x01\x01", 6, &u) == -1);
    assert(utf8_to_unicode("\xfc\x84\x80\x01\x01\x01", 6, &u) == -1);
    assert(utf8_to_unicode("\xfc\x84\x01\x01\x01\x01", 6, &u) == -1);
    assert(utf8_to_unicode("\xfc\x01\x01\x01\x01\x01", 6, &u) == -1);

    /* Illegal (too-long) encoding. */
    assert(utf8_to_unicode("\xe0\x80\x80", 3, &u) == -2);
    assert(utf8_to_unicode("\xf0\x80\x80\x80", 4, &u) == -2);
    assert(utf8_to_unicode("\xf8\x80\x80\x80\x80", 5, &u) == -2);
    assert(utf8_to_unicode("\xfc\x80\x80\x80\x80\x80", 6, &u) == -2);

    /* Invalid lead byte. */
    assert(utf8_to_unicode("\xff", 1, &u) == -3);
}
