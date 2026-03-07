/*
 * Copyright (c) 2022-2026 Paul Mattes.
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
 *      catv_test.c
 *              xscatv unit tests
 */

#include "globals.h"

#include <assert.h>

#include "xscatv.h"
#include "utils.h"

static void basic_test(void);
static void quoting_test(void);
static void newline_test(void);
static void broken_test(void);
static void safety_test(void);

static struct {
    const char *name;
    void (*function)(void);
} test[] = {
    { "Basic", basic_test },
    { "Quoting", quoting_test },
    { "Newline", newline_test },
    { "Broken", broken_test },
    { "Safety", safety_test },
    { NULL, NULL }
};

typedef struct {
    const char *input;
    ssize_t ilen;
    ssize_t ulimit;
    enum xscatv_quote quote;
    unsigned opts;
    const char *output;
} xct_t;

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
	if (verbose) {
	    printf("%s test - ", test[i].name);
	    fflush(stdout);
	}
	(*test[i].function)();
	if (verbose) {
	    printf("PASS\n");
	} else {
	    printf(".");
	    fflush(stdout);
	}
    }

    /* Success. */
    printf("\nPASS\n");
    return 0;
}

static void
run_tests(xct_t *cases)
{
    for (; cases->input != NULL; cases++) {
	char *result = xscatv(cases->input,
		(cases->ilen >= 0)? (size_t)cases->ilen: strlen(cases->input),
		cases->ulimit, cases->quote, cases->opts);

	assert(!strcmp(result, cases->output));
	Free(result);
    }
}

/* Basic test. */
static void
basic_test(void)
{
    xct_t cases[] = {
	{ "123 456", -1, -1, XSCQ_NONE, XSCF_DEFAULT, "123 456" },
	{ "123\001\002\177\n", 8, -1, XSCQ_NONE, XSCF_DEFAULT, "123^A^B^?^J^@" },
	{ "123\302\200\302\201\302\237\302\240", -1, -1, XSCQ_NONE, XSCF_DEFAULT, "123M-^@M-^AM-^_M- " },
	{ "123国456", -1, -1, XSCQ_NONE, XSCF_DEFAULT, "123国456" },

	{ "123国456", -1, 1, XSCQ_NONE, XSCF_DEFAULT, "1" },
	{ "123国456", -1, 4, XSCQ_NONE, XSCF_DEFAULT, "123国" },
	{ "123国456", -1, 0, XSCQ_NONE, XSCF_DEFAULT, "" },

	{ NULL, 0, 0, 0, 0, NULL }
    };

    run_tests(cases);
}

/* Quoting test. */
static void
quoting_test(void)
{
    xct_t cases[] = {
	{ "123456", -1, -1, XSCQ_QUOTE, XSCF_DEFAULT, "\"123456\"" },
	{ "123\"456", -1, -1, XSCQ_QUOTE, XSCF_DEFAULT, "\"123\\\"456\"" },
	{ "123\001456", -1, -1, XSCQ_QUOTE, XSCF_DEFAULT, "\"123^A456\"" },

	{ "123456", -1, -1, XSCQ_ARGQUOTE, XSCF_DEFAULT, "123456" },
	{ "123 456", -1, -1, XSCQ_ARGQUOTE, XSCF_DEFAULT, "\"123 456\"" },
	{ "123,456", -1, -1, XSCQ_ARGQUOTE, XSCF_DEFAULT, "\"123,456\"" },
	{ "123\"456", -1, -1, XSCQ_ARGQUOTE, XSCF_DEFAULT, "\"123\\\"456\"" },
	{ "123(456", -1, -1, XSCQ_ARGQUOTE, XSCF_DEFAULT, "\"123(456\"" },
	{ "123)456", -1, -1, XSCQ_ARGQUOTE, XSCF_DEFAULT, "\"123)456\"" },

	{ "123456", -1, -1, XSCQ_SHELLQUOTE, XSCF_DEFAULT, "123456" },
	{ "123 456", -1, -1, XSCQ_SHELLQUOTE, XSCF_DEFAULT, "\"123 456\"" },
	{ "123,456", -1, -1, XSCQ_SHELLQUOTE, XSCF_DEFAULT, "123,456" },
	{ "123\"456", -1, -1, XSCQ_SHELLQUOTE, XSCF_DEFAULT, "\"123\\\"456\"" },
	{ "123(456", -1, -1, XSCQ_SHELLQUOTE, XSCF_DEFAULT, "123(456" },
	{ "123)456", -1, -1, XSCQ_SHELLQUOTE, XSCF_DEFAULT, "123)456" },

	{ NULL, 0, 0, 0, 0, NULL }
    };

    run_tests(cases);
}

/* Newline test. */
static void
newline_test(void)
{
    xct_t cases[] = {
	{ "123\n456", -1, -1, XSCQ_NONE, XSCF_NLTHRU, "123\n456" },

	/* The rest of these aren't sensible, but if I every change the code, they will break appropriately. */
	{ "123\n456", -1, -1, XSCQ_QUOTE, XSCF_NLTHRU, "\"123\n456\"" },
	{ "123\n456", -1, -1, XSCQ_ARGQUOTE, XSCF_NLTHRU, "123\n456" },
	{ "123\n456", -1, -1, XSCQ_SHELLQUOTE, XSCF_NLTHRU, "123\n456" },

	{ NULL, 0, 0, 0, 0, NULL }
    };

    run_tests(cases);
}

/* Broken (invalid UTF-8) test. */
static void
broken_test(void)
{
    xct_t cases[] = {
	{ "123\343\200", -1, -1, XSCQ_NONE, XSCF_DEFAULT, "123<incomplete multi-byte>" }, /* incomplete */
	{ "123\343xyz", -1, -1, XSCQ_NONE, XSCF_DEFAULT, "123<invalid multi-byte>" }, /* invalid */

	{ NULL, 0, 0, 0, 0, NULL }
    };

    run_tests(cases);
}

/* Safety test. */
static void
safety_test(void)
{
    struct {
	const char *string;
	unsigned opts;
	bool success;
    } cases[] = {
	/* Success. */
	{ "abc", XSCC_ALL, true },		/* Simple success. */

	/* Failure. */
	{ "abc def", XSCC_ALL, false },		/* White space. */
	{ "abc\001def", XSCC_ALL, false },	/* C0 control. */
	{ "abc\302\200def", XSCC_ALL, false },	/* C1 control. */
	{ "abc\302\240", XSCC_ALL, false },	/* No-break space. */
	{ "abc\343\200\200", XSCC_ALL, false },	/* Ideographic space. */

	/* Success because individual options are turned off. */
	{ "abc def", ~XSCC_WHITESPACE, true },	/* White space. */
	{ "abc\001def", ~XSCC_CONTROLS, true },	/* C0 control. */
	{ "abc\302\200def", ~XSCC_CONTROLS, true }, /* C1 control. */
	{ "abc\302\240", ~XSCC_NBSP, true },	/* No-break space. */
	{ "abc\343\200\200", ~XSCC_DBSPACE, true }, /* Ideographic space. */

	{ NULL, 0, false }
    };
    int i;

    for (i = 0; cases[i].string; i++) {
	bool result = xscatv_safe(cases[i].string, strlen(cases[i].string), cases[i].opts);

	assert(result == cases[i].success);
    }
}
