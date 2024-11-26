/*
 * Copyright (c) 2022-2024 Paul Mattes.
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
 *      devname_test.c
 *              DEVNAME unit tests
 */

#include "globals.h"

#include <assert.h>

#include "devname.h"
#include "utils.h"

static void basic_test(void);
static void const_test(void);
static void three_digit_test(void);
static void contiguous_test(void);

static struct {
    const char *name;
    void (*function)(void);
} test[] = {
    { "Basic", basic_test },
    { "Const", const_test },
    { "Three digits", three_digit_test },
    { "Contiguous", contiguous_test },
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

/* Basic test, one digit. */
static void
basic_test(void)
{
    devname_t *d = devname_init("xyz=");
    int i;

    for (i = 1; i <= 10; i++) {
	char *expect = Asprintf("xyz%d", (i < 10)? i: 9);
	const char *got = devname_next(d);

	assert(!strcmp(expect, got));
	Free(expect);
    }
    devname_free(d);
}

/* Const test (no substitutions). */
static void
const_test(void)
{
    devname_t *d = devname_init("xyz");
    int i;

    for (i = 1; i < 2; i++) {
	const char *got = devname_next(d);

	assert(!strcmp("xyz", got));
    }
    devname_free(d);
}

/* 3-digit test. */
static void
three_digit_test(void)
{
    devname_t *d = devname_init("xyz===");
    int i;

    for (i = 1; i <= 1000; i++) {
	char *expect = Asprintf("xyz%03d", (i < 1000)? i: 999);
	const char *got = devname_next(d);

	assert(!strcmp(expect, got));
	Free(expect);
    }
    devname_free(d);
}

/* Contiguous field test, one digit. */
static void
contiguous_test(void)
{
    devname_t *d = devname_init("x=z=");
    int i;

    for (i = 1; i <= 10; i++) {
	char *expect = Asprintf("x=z%d", (i < 10)? i: 9);
	const char *got = devname_next(d);

	assert(!strcmp(expect, got));
	Free(expect);
    }
    devname_free(d);
}
