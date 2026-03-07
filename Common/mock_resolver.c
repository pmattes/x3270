/*
 * Copyright (c) 2007-2026 Paul Mattes.
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
 *	mock_resolver.c
 *		Mock hostname resolver, for unit testing.
 */

#include "globals.h"

#include <assert.h>
#if !defined(_WIN32) /*[*/
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
#endif /*]*/

#include "resolver.h"
#include "sockaddr_46.h"
#include "trace.h"
#include "utils.h"
#include "xscatv.h"

#include "mock_resolver.h"

#define MGAI_SLOTS	10	/* number of mock resolver slots */

/* Mock resolver state. */
static struct mock_gai {
    bool busy;			/* true if slot is occupied */
    rhp_t ret;			/* return value */
    int pipe;			/* pipe to write slot number into */
#if defined(_WIN32) /*[*/
    HANDLE event;		/* event to signal */
#endif /*]*/
    sockaddr_46_t sa;		/* returned address */
    socklen_t sa_len;		/* returned address length */
    unsigned short port;	/* returned port */
    ioid_t id;			/* async event ID */
} mock_gai[MGAI_SLOTS];
static int mock_gai_wix = 0;	/* circular write index for async events */
static int mock_gai_rix = 0;	/* circular read index for async events */
static bool mas_searched = false; /* true if we have looked in the environment */
static char *mas = NULL;	/* specification */
static char *mas_saveptr = NULL; /* strtok_r save pointer */
static char *mas_next = NULL;	/* next token */

/* Check for the mock collector being active. */
bool
mock_resolver_ready(void)
{
    bool first = false;

    if (!mas_searched) {
	const char *mu;

	mas_searched = true;
	mu = ut_getenv("MOCK_ASYNC_RESOLVER");
	if (mu != NULL) {
	    mas = NewString(mu);
	    first = true;
	} else {
	    return false;
	}
    }

    if (mas == NULL) {
	return false;
    }

    mas_next = strtok_r(first? mas: NULL, ";", &mas_saveptr);
    if (mas_next != NULL) {
	return true;
    }
    vctrace(TC_DNS, "Mock async resolver spec exhausted\n");
    Replace(mas, NULL);
    return false;
}

/* Collect the status for a slot. */
rhp_t
mock_collect_host_and_port(int slot, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, unsigned short *pport, const char **errmsg, int max,
	int *nr)
{
    struct mock_gai *g;

    assert(slot >= gai_slots && slot <= gai_slots + MGAI_SLOTS);
    g = &mock_gai[slot - gai_slots];
    assert(g->busy);
    if (g->ret != RHP_SUCCESS) {
	*errmsg = "Mock async resolver fail-async";
	*nr = 0;
    } else {
	*errmsg = NULL;
	memcpy(sa, &g->sa, g->sa_len);
	*sa_rlen = g->sa_len;
	*pport = g->port;
	*nr = 1;
    }

    g->busy = false;
    return g->ret;
}

/* Clean up a canceled request. */
void
mock_cleanup_host_and_port(int slot)
{
    struct mock_gai *g;

    assert(slot >= gai_slots && slot <= gai_slots + MGAI_SLOTS);
    g = &mock_gai[slot - gai_slots];
    assert(g->busy);
    g->busy = false;
}

/* Mock async operation is complete (timeout callback). */
static void
mock_async_done(ioid_t id _is_unused)
{
    struct mock_gai *g = &mock_gai[mock_gai_rix];
    char slot = (char)(mock_gai_rix + gai_slots);
    ssize_t nw;

    assert(g->busy);
    mock_gai_rix = (mock_gai_rix + 1) % MGAI_SLOTS;
    nw = write(g->pipe, &slot, 1);
    assert(nw == 1);
#if defined(_WIN32) /*[*/
    SetEvent(g->event);
#endif /*]*/
}

/*
 * Mock asynchronous resolver.
 * The spec from MOCK_ASYNC_RESOLVER in the environment looks like:
 *  <spec>[;<spec>...]
 * <spec> looks like:
 *  fail-sync
 *  fail-async
 *  succeed-sync=<numeric-address>
 *  succeed-asnyc=<numeric-address>
 * This code will incrementally scan the spec and do as it specifies. If it encounters an error, it will
 * stop scanning and return a synchronous failure.
 */
rhp_t
mock_resolve_host_and_port_async(const char *host, const char *portname, int pf, unsigned short *pport,
	struct sockaddr *sa, size_t sa_len, socklen_t *sa_rlen, const char **errmsg,
	int max, int *nr, int *slot, int pipe, iosrc_t event)
{
    char *token = mas_next;
    rhp_t ret = RHP_SUCCESS;
    struct mock_gai *m;
    bool is_sync;
    size_t tlen;

    *errmsg = NULL;

    assert(token != NULL);
    vctrace(TC_DNS, "Processing mock async resolver token '%s'\n", xscatv(token, strlen(token), (ssize_t)-1, XSCQ_NONE,
		XSCF_DEFAULT));
    if (!strcmp("fail-sync", token)) {
	*errmsg = "Mock async resolver fail-sync";
	ret = RHP_CANNOT_RESOLVE;
    } else if (!strcmp("fail-async", token)) {
	m = &mock_gai[mock_gai_wix];
	assert(!m->busy);
	m->busy = true;
	m->ret = RHP_CANNOT_RESOLVE;
	m->pipe = pipe;
#if defined(_WIN32) /*[*/
	m->event = event;
#endif /*]*/
	m->id = AddTimeOut(0, mock_async_done);
	*slot = mock_gai_wix + gai_slots;
	mock_gai_wix = (mock_gai_wix + 1) % MGAI_SLOTS;
	ret = RHP_PENDING;
    } else if ((is_sync = !strncmp("succeed-sync=", token, (tlen = 13))) || !strncmp("succeed-async=", token, (tlen = 14))) {
	sockaddr_46_t *p_sa = (sockaddr_46_t *)sa;
	bool is_v6 = strchr(token, ':') != NULL;
	socklen_t socklen = is_v6? sizeof(p_sa->sin6): sizeof(p_sa->sin);
	int p;

	memset(p_sa, 0, socklen);
	p = inet_pton(is_v6? AF_INET6: AF_INET, token + tlen, is_v6? (void *)&p_sa->sin6.sin6_addr: (void *)&p_sa->sin.sin_addr);
	if (p == 1) {
	    unsigned short port = (portname != NULL)? atoi(portname): 0;

	    p_sa->sa.sa_family = is_v6? AF_INET6: AF_INET;
	    if (is_v6) {
		p_sa->sin6.sin6_port = htons(port);
	    } else {
		p_sa->sin.sin_port = htons(port);
	    }
	    if (is_sync) {
		*pport = port;
		memcpy(sa, p_sa, sizeof(sockaddr_46_t));
		*sa_rlen = socklen;
		*nr = 1;
		ret = RHP_SUCCESS;
	    } else {
		m = &mock_gai[mock_gai_wix];
		assert(!m->busy);
		m->busy = true;
		m->ret = RHP_SUCCESS;
		m->pipe = pipe;
#if defined(_WIN32) /*[*/
		m->event = event;
#endif /*]*/
		memcpy(&m->sa, p_sa, sizeof(sockaddr_46_t));
		m->sa_len = socklen;
		m->port = port;
		m->id = AddTimeOut(0, mock_async_done);
		*slot = mock_gai_wix + gai_slots;
		mock_gai_wix = (mock_gai_wix + 1) % MGAI_SLOTS;
		ret = RHP_PENDING;
	    }
	} else {
	    /* inet_pton() failed */
	    ret = RHP_FATAL;
	}
    } else {
	/* unknown token */
	ret = RHP_FATAL;
    }

    if (ret == RHP_FATAL) {
	*errmsg = "Invalid async resolver spec";
	Replace(mas, NULL);
    }

    return ret;
}
