/*
 * Copyright (c) 1999-2026 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
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
 * 	defer.c
 * 		Deferred event support.
 */

#include "globals.h"
#include "glue.h"
#include "task.h"
#include "timeouts.h"
#include "trace.h"
#include "utils.h"

typedef struct {
    llist_t llist;	/* linkage */
    tofn_t fn;		/* callback */
    bool added_while_running;
} defer_t;
llist_t deferred = LLIST_INIT(deferred);

/*
 * Set while we are executing the list, so we can tell which ones were added
 * while excuting others.
 */
bool run_deferred_running = false;

/*
 * Deferred events are a way to give up the CPU and let other things run.
 *
 * For example, when a 3270 data stream command from the host is read in and
 * executed, or an action is read from s3270's stdin or by httpd and executed,
 * if there is more input in the buffer beyond that, processing the remaining
 * input is set up as a deferred event. Deferred events are not run until the
 * task queues have been run to react to what has come in, and a non-blocking
 * trip through the scheduler has completed, to process pending network events
 * and timeouts.
 *
 * This prevents one data stream from hogging the system, preventing it from
 * responding to input from other streams.
 */

/**
 * Add a deferred event. It will be run once.
 *
 * @param[in] fn	Function to execute.
 *
 * @returns I/O ID.
 */
ioid_t
AddDefer(tofn_t fn)
{
    defer_t *d = Malloc(sizeof(defer_t));

    vcdtrace(TC_SCHED, "AddDefer 0x%lx\n", (unsigned long)(size_t)d);
    memset(d, 0, sizeof(defer_t));
    llist_init(&d->llist);
    d->fn = fn;
    d->added_while_running = run_deferred_running;
    LLIST_APPEND(&d->llist, deferred);
    return (ioid_t)d;
}

/**
 * (Possibly) Remove a deferred event.
 *
 * @param[in] id	I/O identifier
 *
 * @returns true if the identifier matches an active event.
 */
bool
RemoveDefer(ioid_t id)
{
    defer_t *d;
    bool found = false;

    FOREACH_LLIST(&deferred, d, defer_t *) {
	if (d == (defer_t *)id) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&deferred, d, defer_t *);

    if (found) {
	vcdtrace(TC_SCHED, "RemoveDefer 0x%lx\n", (unsigned long)(size_t)d);
	llist_unlink(&d->llist);
	Free(d);
	return true;
    }
    return false;
}

/**
 * Run the deferred events, stopping when we hit one that was added while we were running.
 */
bool
run_deferred(void)
{
    defer_t *d;
    bool any = false;

    run_deferred_running = true;
    while (!llist_isempty(&deferred)) {
	d = (defer_t *)deferred.next;
	if (d->added_while_running) {
	    vcdtrace(TC_SCHED, "RunDeferred stop @ 0x%lx, added while running\n", (unsigned long)(size_t)d);
	    break;
	}
	vcdtrace(TC_SCHED, "RunDeferred 0x%lx\n", (unsigned long)(size_t)d);
	llist_unlink(&d->llist);
	(*d->fn)((ioid_t)d);
	Free(d);
	any = true;
    }
    run_deferred_running = false;

    /* Clean up the added_while_running flags. */
    FOREACH_LLIST(&deferred, d, defer_t *) {
	d->added_while_running = false;
    } FOREACH_LLIST_END(&deferred, d, defer_t *);

    return any;
}

/**
 * Check for any deferred events.
 *
 * @returns true if there are any deferred events queued.
 */
bool
any_deferred(void)
{
    return !llist_isempty(&deferred);
}
