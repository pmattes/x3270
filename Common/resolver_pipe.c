/*
 * Copyright (c) 2019-2026 Paul Mattes.
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
 *	resolver_pipe.c
 *		Integration logic for the asynchronous DNS resolver.
 */

#include "globals.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#if !defined(_WIN32) /*[*/
# include <netinet/in.h>
#endif /*]*/

#include "names.h"
#include "popups.h"
#include "query.h"
#include "resolver.h"
#include "sockaddr_46.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"

#include "resolver_pipe.h"

#define NUM_HA	4

/* Resolver context. */
struct _rp {
    struct _rp *next;		/* queue linkage */
    void *context;		/* if NULL, rp is orphaned */
    rp_complete_t *complete;	/* completion callback */
    int pipe[2];		/* resolver writes completed slot numbers here */
    int slot;			/* slot number for active resolution, or -1 if none */
    iosrc_t event;		/* resolver signals this (Windows) */
    ioid_t id;			/* AddInput id */
    unsigned pending_cancels;	/* number of pending canceled resolution operations */
    struct timeval start;	/* starting timestamp */

    int num_ha;			/* number of addresses resolved */
    sockaddr_46_t haddr[NUM_HA];/* resolved addresses */
    socklen_t ha_len[NUM_HA];	/* resolved address lengths */
    unsigned short port;	/* resolved port */
};
#define ACTIVE(rp)	((rp)->complete != NULL)

/* Active sessions */
static struct _rp *active_rps = NULL;
/* Sessions that were freed while there were canceled resolution operations pending */
static struct _rp *orphaned_rps = NULL;

static unsigned n_complete, n_canceled;
static unsigned long min_us, avg_us, max_us;
static unsigned long total_us;

static void resolve_done(iosrc_t fd, ioid_t id);

/**
 * Allocate an rp.
 * @param[in] context	Opaque caller context
 * @param[in] complete	Completion callback
 * @returns rp session handle
 */
rp_t
rp_alloc(void *context, rp_complete_t *complete)
{
    rp_t rp = Calloc(1, sizeof(struct _rp));
    int i;
    int rv;

    assert(complete != NULL);
    rp->context = context;
    rp->complete = complete;

    rp->pipe[0] = rp->pipe[1] = -1;
    rp->slot = -1;
    rp->event = INVALID_IOSRC;
    rp->id = NULL_IOID;
    for (i = 0; i < NUM_HA; i++) {
	rp->ha_len[i] = sizeof(sockaddr_46_t);
    }
    rp->port = 0;
    rp->pending_cancels = 0;

#if !defined(_WIN32) /*[*/
    rv = pipe(rp->pipe);
#else /*][*/
    rv = _pipe(rp->pipe, 512, _O_BINARY);
#endif /*]*/
    if (rv < 0) {
	popup_an_error("resolver pipe: %s", strerror(errno));
	return NULL;
    }
#if !defined(_WIN32) /*[*/
    rp->id = AddInput(rp->pipe[0], resolve_done);
#else /*][*/
    rp->event = CreateEvent(NULL, FALSE, FALSE, NULL);
    rp->id = AddInput(rp->event, resolve_done);
#endif /*]*/

    rp->next = active_rps;
    active_rps = rp;
    return rp;
}

/* Release resources for a session and free the rp. */
static void
rp_clean_and_free(rp_t rp)
{
    close(rp->pipe[0]);
    close(rp->pipe[1]);
    RemoveInput(rp->id);
#if defined(_WIN32) /*[*/
    CloseHandle(rp->event);
#endif /*]*/
    memset(rp, 0, sizeof(struct _rp));
    Free(rp);
}

/* Remove an rp from a queue. */
static void
rp_dequeue(rp_t rp, rp_t *head)
{
    rp_t r;
    rp_t prev = NULL;

    for (r = *head; r != NULL; r = r->next) {
	if (r == rp) {
	    break;
	}
	prev = rp;
    }
    assert(r != NULL);
    if (prev != NULL) {
	prev->next = rp->next;
    } else {
	*head = rp->next;
    }
}

/* Discard an orphaned rp. */
static void
rp_orphan_free(rp_t rp)
{
    rp_dequeue(rp, &orphaned_rps);
    rp_clean_and_free(rp);
}

/**
 * Free an rp.
 * @param[in,out] rpp	pointer to rp session handle
 */
void
rp_free(rp_t *rpp)
{
    rp_t rp = *rpp;

    if (rp == NULL) {
	/* Redundant. */
	return;
    }

    assert(ACTIVE(rp));
    rp_dequeue(rp, &active_rps);
    rp->complete = NULL;
    rp_cancel(rp);
    if (rp->pending_cancels > 0) {
	/* Canceled requests are pending. Keep it on the orphaned list so we can clean up when it completes. */
	rp->next = orphaned_rps;
	orphaned_rps = rp;
    } else {
	rp_clean_and_free(rp);
    }
    *rpp = NULL;
}

/**
 * Cancel a pending lookup.
 * @param[in] rp	rp session handle
 */
void
rp_cancel(rp_t rp)
{
    if (rp->slot >= 0) {
	vctrace(TC_DNS, "Canceling resolver slot %d\n", rp->slot);
	rp->slot = -1;
	rp->pending_cancels++;
    }
}

/* Initiate an asynchronous DNS lookup. */
rp_result_t
rp_resolve(rp_t rp, const char *host, const char *portname, int pf, const char **errmsg)
{
    rhp_t rv;

    rp_cancel(rp);

    gettimeofday(&rp->start, NULL);
    *errmsg = NULL;
    rv = resolve_host_and_port_async(host, portname, pf, &rp->port,
	    &rp->haddr[0].sa, sizeof(rp->haddr[0]), rp->ha_len, errmsg, NUM_HA,
	    &rp->num_ha, &rp->slot, rp->pipe[1], rp->event);
    if (RHP_IS_ERROR(rv)) {
	return RP_FAIL;
    }

    if (rv == RHP_PENDING) {
	vctrace(TC_DNS, "Resolving %s/%s, slot is %d\n",
		scatv(host), portname? scatv(portname): "(none)",
		rp->slot);
	return RP_PENDING;
    }

    /* Worked synchronously. */
    vctrace(TC_DNS, "%s/%s resolved synchronously, %d address%s\n",
	    scatv(host), portname? scatv(portname): "(none)",
	    rp->num_ha, (rp->num_ha == 1)? "": "es");
    ++n_complete; /* assume it took no time */
    return RP_SUCCESS_SYNC;
}

/**
 * Return the number of addresses resolved.
 * @param[in] rp	rp session handle
 * @returns number of addresses
 */
int
rp_num_ha(rp_t rp)
{
    return rp->num_ha;
}

/**
 * Return the resolve port number.
 * @param[in] rp	rp session handle
 * @returns port number
 */
unsigned short
rp_port(rp_t rp)
{
    return rp->port;
}

/**
 * Return a resolved address
 * @param[in] rp	rp session handle
 * @returns address
 */
sockaddr_46_t *
rp_haddr(rp_t rp, int ix)
{
    assert(ix < NUM_HA);
    return &rp->haddr[ix];
}

/**
 * Return the length of a resolved address
 * @param[in] rp	rp session handle
 * @returns address length
 */
socklen_t
rp_ha_len(rp_t rp, int ix)
{
    assert(ix < NUM_HA);
    return rp->ha_len[ix];
}

/* Display microseconds. */
static const char *
display_us(unsigned long us)
{
    return txAsprintf("%lu.%06lu", us / 1000000L, us % 1000000L);
}

/* There is data on the resolver pipe. */
static void
resolve_done(iosrc_t fd, ioid_t id)
{
    rp_t rp;
    int nr;
    char slot_byte;
    int slot;
    int rv;
    const char *errmsg;
    struct timeval stop;
    unsigned long cs;

    /* Find the rp. */
    for (rp = active_rps; rp != NULL; rp = rp->next) {
	if (rp->id == id) {
	    break;
	}
    }
    if (rp == NULL) {
	for (rp = orphaned_rps; rp != NULL; rp = rp->next) {
	    if (rp->id == id) {
		break;
	    }
	}
    }
    assert(rp != NULL);

    /* Read the data, which is the slot number. */
    nr = read(rp->pipe[0], &slot_byte, 1);
    if (nr < 0) {
	popup_an_errno(errno, "Resolver pipe read");
	return;
    }
    if (nr == 0) {
	popup_an_error("Resolver pipe EOF");
	return;
    }

    slot = (int)slot_byte;
    if (slot != rp->slot) {
	/* Canceled request. */
	vctrace(TC_DNS, "Cleaning up canceled resolver slot %d\n", slot);
	++n_canceled;
	cleanup_host_and_port(slot);
	assert(rp->pending_cancels > 0);
	if (!--rp->pending_cancels && !ACTIVE(rp)) {
	    vctrace(TC_DNS, "Cleaning up orphaned rp\n");
	    rp_orphan_free(rp);
	}
	return;
    }

    gettimeofday(&stop, NULL);
    cs = ((stop.tv_sec - rp->start.tv_sec) * 1000000L) + (stop.tv_usec - rp->start.tv_usec);

    rp->slot = -1;
    errmsg = NULL;
    rv = collect_host_and_port(slot, (struct sockaddr *)rp->haddr, sizeof(rp->haddr[0]), rp->ha_len,
	    &rp->port, &errmsg, NUM_HA, &rp->num_ha);

    if (RHP_IS_ERROR(rv)) {
	vctrace(TC_DNS, "Resolution failed in %ss: %s\n", display_us(cs), scatv(errmsg));
	(rp->complete)(rp, rp->context, false, errmsg);
    } else {
	vctrace(TC_DNS, "Resolution complete, %d address%s in %ss\n",
		rp->num_ha, (rp->num_ha == 1)? "": "es",
		display_us(cs));
	if (!min_us || cs < min_us) {
	    min_us = cs;
	}
	if (!max_us || cs > max_us) {
	    max_us = cs;
	}
	total_us += cs;
	avg_us = total_us / ++n_complete;
	(rp->complete)(rp, rp->context, true, NULL);
    }
}

/* Compute the number of pending resolution operations. */
static unsigned
n_pending(void)
{
    rp_t r;
    rp_t lists[] = { active_rps, orphaned_rps };
    unsigned i;
    unsigned sum = 0;

    for (i = 0; i < array_count(lists); i++) {
	for (r = lists[i]; r != NULL; r = r->next) {
	    sum += (r->slot >= 0) + r->pending_cancels;
	}
    }
    return sum;
}

/* Dump resolver stats. */
static const char *
resolver_dump(void)
{
    return txAsprintf("complete %u canceled %u pending %u min/avg/max %ss/%ss/%ss",
	    n_complete, n_canceled, n_pending(),
	    n_complete? display_us(min_us): "-",
	    n_complete? display_us(avg_us): "-",
	    n_complete? display_us(max_us): "-");
}

/* Module registration. */
void
resolver_pipe_register(void)
{
    static query_t queries[] = {
        { KwResolver, resolver_dump, NULL, QF_HIDDEN | QF_TRACEHDR },
    };

    /* Register our queries. */
    register_queries(queries, array_count(queries));
}
