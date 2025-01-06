/*
 * Copyright (c) 1999-2025 Paul Mattes.
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
 * 	timeouts.c
 * 		Timeout handler.
 */

#include "globals.h"
#include "glue.h"
#include "task.h"
#include "timeouts.h"
#include "trace.h"
#include "utils.h"

#include <stdio.h>

#define MILLION		1000000L

#if defined(_WIN32) /*[*/
# define GET_TS(v)	ms_ts(v)
# define EXPIRED(t, now) (t->ts <= now)
#else /*][*/
# define GET_TS(v)	gettimeofday(v, NULL);
# define EXPIRED(t, now) (t->tv.tv_sec < now.tv_sec || \
			  (t->tv.tv_sec == now.tv_sec && t->tv.tv_usec < now.tv_usec))
#endif /*]*/

/* Timeouts. */

#if defined(_WIN32) /*[*/
static void
ms_ts(unsigned long long *u)
{
    FILETIME t;

    /* Get the system time, in 100ns units. */
    GetSystemTimeAsFileTime(&t);
    memcpy(u, &t, sizeof(unsigned long long));

    /* Divide by 10,000 to get ms. */
    *u /= 10000ULL;
}
#endif /*]*/

typedef struct timeout {
    struct timeout *next;
#if defined(_WIN32) /*[*/
    unsigned long long ts;
#else /*][*/
    struct timeval tv;
#endif /*]*/
    tofn_t proc;
    bool in_play;
} timeout_t;
static timeout_t *timeouts = NULL;

ioid_t
AddTimeOut(unsigned long interval_ms, tofn_t proc)
{
    timeout_t *t_new;
    timeout_t *t;
    timeout_t *prev = NULL;

    t_new = (timeout_t *)Malloc(sizeof(timeout_t));
    t_new->proc = proc;
    t_new->in_play = false;
#if defined(_WIN32) /*[*/
    ms_ts(&t_new->ts);
    t_new->ts += interval_ms;
#else /*][*/
    gettimeofday(&t_new->tv, NULL);
    t_new->tv.tv_sec += interval_ms / 1000L;
    t_new->tv.tv_usec += (interval_ms % 1000L) * 1000L;
    if (t_new->tv.tv_usec > MILLION) {
	t_new->tv.tv_sec += t_new->tv.tv_usec / MILLION;
	t_new->tv.tv_usec %= MILLION;
    }
#endif /*]*/

    /* Find where to insert this item. */
    for (t = timeouts; t != NULL; t = t->next) {
#if defined(_WIN32) /*[*/
	if (t->ts > t_new->ts)
#else /*][*/
	if (t->tv.tv_sec > t_new->tv.tv_sec ||
	    (t->tv.tv_sec == t_new->tv.tv_sec &&
	     t->tv.tv_usec > t_new->tv.tv_usec))
#endif /*]*/
	{
	    break;
	}
	prev = t;
    }

    /* Insert it. */
    if (prev == NULL) {		/* Front. */
	t_new->next = timeouts;
	timeouts = t_new;
    } else if (t == NULL) {	/* Rear. */
	t_new->next = NULL;
	prev->next = t_new;
    } else {			/* Middle. */
	t_new->next = t;
	prev->next = t_new;
    }

    return (ioid_t)t_new;
}

void
RemoveTimeOut(ioid_t timer)
{
    timeout_t *st = (timeout_t *)timer;
    timeout_t *t;
    timeout_t *prev = NULL;

    if (st->in_play) {
	return;
    }
    for (t = timeouts; t != NULL; t = t->next) {
	if (t == st) {
	    if (prev != NULL) {
		prev->next = t->next;
	    } else {
		timeouts = t->next;
	    }
	    Free(t);
	    return;
	}
	prev = t;
    }
}

/*
 * Computes the time offset to the next timeout.
 */
bool
compute_timeout(TIMEOUT_T *tmop, bool block)
{
#if defined(_WIN32) /*[*/
    unsigned long long now;
#else /*][*/
    struct timeval now;
    static struct timeval twait;
#endif /*]*/

    if (block) {

	if (timeouts != NULL) {
	    /* Compute how long to wait for the first timeout. */
	    GET_TS(&now);
#if defined(_WIN32) /*[*/
	    if (now > timeouts->ts) {
		vtrace("sched: Timeout(s) already expired\n");
		*tmop = 0;
	    } else {
		*tmop = (DWORD)(timeouts->ts - now);
	    }
#else /*][*/
	    twait.tv_sec = timeouts->tv.tv_sec - now.tv_sec;
	    twait.tv_usec = timeouts->tv.tv_usec - now.tv_usec;
	    if (twait.tv_usec < 0L) {
		twait.tv_sec--;
		twait.tv_usec += MILLION;
	    }
	    if (twait.tv_sec < 0L) {
		vtrace("sched: Timeout(s) already expired\n");
		twait.tv_sec = twait.tv_usec = 0L;
	    }
# if defined(HAVE_POLL) /*[*/
	    /* Convert from microseconds to milliseconds. */
	    *tmop = (twait.tv_sec * 1000) + (twait.tv_usec / 1000);
	    if (*tmop == 0 && (twait.tv_sec != 0 || twait.tv_usec != 0)) {
		/*
		 * The time offset is non-zero, but less than the granularity of poll(),
		 * which is 1ms. Set the timeout to 1ms. This is to prevent the situation where
		 * there is no I/O pending and we end up spinning until the clock time
		 * reaches the time of the first timeout.
		 */
		vtrace("sched: Timeout(s) less than 1ms\n");
		*tmop = 1;
	    }
# else /*][*/
	    *tmop = &twait;
# endif /*]*/
#endif /*]*/
	    return true; /* yes, there is something pending */
	}

	/* Block indefinitely. */
#if defined(_WIN32) /*[*/
	*tmop = INFINITE;
#elif defined(HAVE_POLL) /*][*/
	*tmop = -1;
#else /*][*/
	*tmop = NULL;
#endif /*]*/
	return false; /* no, there isn't anyting pending */
    }

    /* Do not block. */
#if defined(_WIN32) /*[*/
    *tmop = 1;
#elif defined(HAVE_POLL) /*[*/
    *tmop = 0;
#else /*][*/
    twait.tv_sec = twait.tv_usec = 0L;
    *tmop = &twait;
#endif /*]*/
    return false;
}

/*
 * Processes timeouts. Returns true if any were processed.
 */
bool
process_timeouts(void)
{
    bool processed_any = false;

    if (timeouts != NULL) {
#if defined(_WIN32) /*[*/
	unsigned long long now;
#else /*][*/
	struct timeval now;
#endif /*]*/
	timeout_t *t;

	GET_TS(&now);
	while ((t = timeouts) != NULL) {
	    if (EXPIRED(t, now)) {
		timeouts = t->next;
		t->in_play = true;
		(*t->proc)((ioid_t)t);
		processed_any = true;
		Free(t);
	    } else {
		break;
	    }
	}
    }
    return processed_any;
}

/*
 * Formats a string for the timeout value.
 */
const char *
trace_tmo(TIMEOUT_T tmo, char *buf, size_t bufsize)
{
#if defined(_WIN32) /*[*/
    if (tmo != INFINITE) {
	snprintf(buf, bufsize, "%d ms", (int)tmo);
	buf[bufsize - 1] = '\0';
	return buf;
    }
#elif defined(HAVE_POLL) /*[*/
    if (tmo >= 0) {
	snprintf(buf, bufsize, "%u.%03u sec", tmo / 1000, tmo % 1000);
	buf[bufsize - 1] = '\0';
	return buf;
    }
#else /*][*/
    if (tmo != NULL) {
	unsigned msec = (tmo->tv_usec + 500) / 1000;
	unsigned sec = tmo->tv_sec;

	if (msec >= 1000) {
	    sec++;
	    msec -= 1000;
	}
	snprintf(buf, bufsize, "%u.%03u sec", sec, msec);
	buf[bufsize - 1] = '\0';
	return buf;
    }
#endif /*]*/
    return NULL;
}
