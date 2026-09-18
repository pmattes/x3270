/*
 * Copyright (c) 2026 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	crash.c
 *		Crash action support.
 */

#include "globals.h"
#include <assert.h>

#include "actions.h"
#include "names.h"
#include "popups.h"
#include "utils.h"

typedef enum {
    CRASH_ASSERT,
    CRASH_EXIT,
    CRASH_NULL
} crash_operation_t;

typedef struct crash_deferred {
    struct crash_deferred *next;
    crash_operation_t operation;
    int exit_code;
} crash_deferred_t;

static crash_deferred_t *crash_deferred_head;
static crash_deferred_t *crash_deferred_tail;

static void crash_deferred_fn(ioid_t id);
static void crash_do(crash_operation_t operation, int exit_code);

/**
 * Execute a crash operation.
 *
 * @param[in] operation	Crash operation to execute
 * @param[in] exit_code	Exit status for CRASH_EXIT
 */
static void
crash_do(crash_operation_t operation, int exit_code)
{
    static char *crashptr = NULL;

    switch (operation) {
    case CRASH_ASSERT:
	assert(false);
	popup_an_error(AnCrash "(): Assert did not work");
	break;
    case CRASH_EXIT:
	exit(exit_code);
	popup_an_error(AnCrash "(): Exit did not work");
	break;
    case CRASH_NULL:
	printf("%c\n", *crashptr);
	popup_an_error(AnCrash "(): Null did not work");
	break;
    }
}

/**
 * Execute a deferred crash operation.
 *
 * @param[in] id	Deferred operation identifier
 */
static void
crash_deferred_fn(ioid_t id _is_unused)
{
    crash_deferred_t *d = crash_deferred_head;

    crash_deferred_head = d->next;
    if (crash_deferred_head == NULL) {
	crash_deferred_tail = NULL;
    }
    crash_do(d->operation, d->exit_code);
    Free(d);
}

/**
 * Crash() action, used in unit tests to exercise error recovery.
 *  Crash([Async,]Assert) causes an assertion failure.
 *  Crash([Async,]Exit[,code]) causes the program to exit with a status code.
 *  Crash([Async,]Null) causes the program to crash with a SEGV.
 * Enabled by the unit test environment variable CRASH.
 *
 * @param[in] ia	Cause for action invocation
 * @param[in] argc	Argument count
 * @param[in] argv	Arguments
 * @returns True for success
 */
static bool
Crash_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned offset = 0;
    crash_operation_t operation;
    int exit_code = 27;
    char *end;
    long l;

    action_debug(AnCrash, ia, argc, argv);
    if (check_argc(AnCrash, argc, 1, 3) < 0) {
	return false;
    }

    if (!strcasecmp(argv[0], KwAsync) || !strcasecmp(argv[0], KwSync)) {
	offset++;
	if (argc == offset) {
	    popup_an_error(AnCrash "(): Missing operation");
	    return false;
	}
    }

    if (!strcasecmp(argv[offset], KwAssert)) {
	operation = CRASH_ASSERT;
    } else if (!strcasecmp(argv[offset], KwExit)) {
	operation = CRASH_EXIT;
    } else if (!strcasecmp(argv[offset], KwNull)) {
	operation = CRASH_NULL;
    } else {
	popup_an_error(AnCrash "(): Must specify " KwAssert ", " KwExit " or " KwNull);
	return false;
    }

    offset++;
    if (operation != CRASH_EXIT && argc != offset) {
	popup_an_error(AnCrash "(): Extra arguments");
	return false;
    }
    if (argc > offset) {
	if (argc != offset + 1) {
	    popup_an_error(AnCrash "(): Too many arguments");
	    return false;
	}
	l = strtol(argv[offset], &end, 0);
	if (*argv[offset] == '\0' || *end != '\0' || l < 0 || l > 255) {
	    popup_an_error(AnCrash "(): Invalid exit code '%s'", argv[offset]);
	    return false;
	}
	exit_code = (int)l;
    }

    if (!strcasecmp(argv[0], KwAsync)) {
	crash_deferred_t *d = Malloc(sizeof(*d));

	d->operation = operation;
	d->exit_code = exit_code;
	if (crash_deferred_tail != NULL) {
	    crash_deferred_tail->next = d;
	} else {
	    crash_deferred_head = d;
	}
	crash_deferred_tail = d;
	AddDefer(crash_deferred_fn);
    } else {
	crash_do(operation, exit_code);
    }

    return false;
}

/**
 * Crash module registration.
 */
void
crash_register(void)
{
    static action_table_t actions[] = {
	{ AnCrash, Crash_action, ACTION_HIDDEN },
    };

    if (ut_getenv("CRASH") != NULL) {
	register_actions(actions, array_count(actions));
    }
}
