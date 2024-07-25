/*
 * Copyright (c) 2007-2024 Paul Mattes.
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
 *	resolver.c
 *		Hostname resolution.
 */

#include "globals.h"

#include <errno.h>
#include <assert.h>
#if !defined(_WIN32) /*[*/
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <signal.h>
#endif /*]*/

#include <stdio.h>
#include "resolver.h"
#include "txa.h"
#include "utils.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "winvers.h"
#endif /*]*/

#if defined(_WIN32) || defined(HAVE_GETADDRINFO_A) /*[*/
# define ASYNC_RESOLVER 1
#endif /*]*/

#if defined(ASYNC_RESOLVER) /*[*/
# define GAI_SLOTS	10
static struct gai {
    bool busy;			/* true if busy */
    bool done;			/* true if done */
    int pipe;			/* pipe to write status into */
    char *host;			/* host name */
    char *port;			/* port name */
# if !defined(_WIN32) /*[*/
    struct gaicb gaicb;		/* control block */
    struct gaicb *gaicbs;	/* control blocks (just one) */
    struct sigevent sigevent;	/* sigevent block */
    struct addrinfo hints;	/* hints */
# else /*][*/
    int rc;			/* return code */
    struct addrinfo *result;	/* result */
    HANDLE event;		/* event to signal */
# endif /*]*/
} gai[GAI_SLOTS];
#endif /*]*/

bool prefer_ipv4;
bool prefer_ipv6;

/* Set the IPv4/IPv6 lookup preferences. */
void
set_46(bool prefer4, bool prefer6)
{
    prefer_ipv4 = prefer4;
    prefer_ipv6 = prefer6;
}

/* Map the -4 and -6 options onto the right getaddrinfo setting. */
static int
want_pf(void)
{
    if (prefer_ipv4 && !prefer_ipv6) {
	return PF_INET;
    } else if (!prefer_ipv4 && prefer_ipv6) {
	return PF_INET6;
    } else {
	return PF_UNSPEC;
    }
}

# if defined(_WIN32) /*[*/
/* Wrap gai_strerror() in a function that translates the code page. */
static const char *
my_gai_strerror(int rc)
{
    return to_localcp(gai_strerror(rc));
}
# else /*][*/
# define my_gai_strerror(x)	gai_strerror(x)
# endif /*]*/
/*
 * Resolve a hostname and port using getaddrinfo, allowing IPv4 or IPv6.
 * Synchronous version.
 */
static rhp_t
resolve_host_and_port_v46(const char *host, char *portname,
	bool abs, unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr)
{
    struct addrinfo hints, *res0, *res;
    int rc;
    int i;
    void *rsa = sa;

    *nr = 0;

    /* getaddrinfo() does not appear to range-check the port. Do that here. */
    if (portname != NULL) {
	unsigned long l;

	if ((l = strtoul(portname, NULL, 0)) && (l & ~0xffffL)) {
	    if (errmsg) {
		*errmsg = txAsprintf("%s/%s:\n%s", host, portname, "Invalid port");
	    }
	    return RHP_CANNOT_RESOLVE;
	}
    }

    memset(&hints, '\0', sizeof(struct addrinfo));
    hints.ai_flags = 0;
    hints.ai_family = abs? PF_UNSPEC: want_pf();
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    rc = getaddrinfo(host, portname, &hints, &res0);
    if (rc != 0) {
	if (errmsg) {
	    *errmsg = txAsprintf("%s/%s:\n%s", host, portname? portname: "(none)",
		    my_gai_strerror(rc));
	}
	return RHP_CANNOT_RESOLVE;
    }
    res = res0;

    /* Return the addresses. */
    for (i = 0; i < max && res != NULL; i++, res = res->ai_next) {
	memcpy(rsa, res->ai_addr, res->ai_addrlen);
	sa_rlen[*nr] = (socklen_t)res->ai_addrlen;
	if (i == 0) {
	    /* Return the port. */
	    switch (res->ai_family) {
	    case AF_INET:
		*pport =
		    ntohs(((struct sockaddr_in *) res->ai_addr)->sin_port);
		break;
	    case AF_INET6:
		*pport =
		    ntohs(((struct sockaddr_in6 *) res->ai_addr)->sin6_port);
		break;
	    default:
		if (errmsg) {
		    *errmsg = txAsprintf("%s:\nunknown family %d", host,
			    res->ai_family);
		}
		freeaddrinfo(res0);
		return RHP_FATAL;
	    }
	}

	rsa = (char *)rsa + sa_len;
	(*nr)++;
    }

    freeaddrinfo(res0);
    return RHP_SUCCESS;
}

#if defined(ASYNC_RESOLVER) /*[*/

# if !defined(_WIN32) /*[*/
/* Notification function for lookup completion. */
static void
gai_notify(union sigval sigval)
{
    char slot = (char)sigval.sival_int;
    struct gai *gaip = &gai[(int)slot];
    ssize_t nw;

    assert(gaip->busy == true);
    assert(gaip->done == false);
    gaip->done = true;

    /*
     * Write our slot number into the pipe, so the main thread can poll us for
     * the completion status.
     */
    nw = write(gaip->pipe, &slot, 1);
    assert(nw == 1);
}

# else /*][*/

/* Asynchronous resolution thread. */
static DWORD WINAPI
async_resolve(LPVOID parameter)
{
    struct gai *gaip = (struct gai *)parameter;
    char slot = (char)(gaip - gai);
    struct addrinfo hints;
    ssize_t nw;

    assert(gaip->busy == true);
    assert(gaip->done == false);
    gaip->done = true;
    memset(&hints, '\0', sizeof(struct addrinfo));
    hints.ai_flags = 0;
    hints.ai_family = want_pf();
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    gaip->rc = getaddrinfo(gaip->host, gaip->port, &hints, &gaip->result);

    /*
     * Write our slot number into the pipe, so the main thread can poll us for
     * the completion status.
     */
    nw = write(gaip->pipe, &slot, 1);
    assert(nw == 1);

    /* Tell the main thread we are done. */
    SetEvent(gaip->event);

    /* Exit the thread. */
    return 0;
}

# endif /*]*/

/*
 * Clean up a partially-complete slot.
 */
static void
cleanup_partial_slot(int slot)
{
    struct gai *gaip = &gai[slot];

    assert(gaip->busy == true);
    assert(gaip->done == false);

    gaip->busy = false;
    gaip->pipe = -1;
    Replace(gaip->host, NULL);
    Replace(gaip->port, NULL);
# if defined(_WIN32) /*[*/
    gaip->event = INVALID_HANDLE_VALUE;
# endif /*]*/
}

/*
 * Resolve a hostname and port using getaddrinfo_a, allowing IPv4 or IPv6.
 * Asynchronous version.
 */
static rhp_t
resolve_host_and_port_v46_a(const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr, int *slot,
	int pipe, iosrc_t event)
{
    static bool initted = false;
# if !defined(_WIN32) /*[*/
    int rc;
# else /*][*/
    HANDLE thread;
# endif /*]*/

    if (!initted) {
	int i;
	for (i = 0; i < GAI_SLOTS; i++) {
	    gai[i].pipe = -1;
# if defined(_WIN32) /*[*/
	    gai[i].event = INVALID_HANDLE_VALUE;
# endif /*]*/
	}
	initted = true;
    }

    *nr = 0;

    /* getaddrinfo() does not appear to range-check the port. Do that here. */
    if (portname != NULL) {
	unsigned long l;

	if ((l = strtoul(portname, NULL, 0)) && (l & ~0xffffL)) {
	    if (errmsg) {
		*errmsg = txAsprintf("%s/%s:\n%s", host, portname, "Invalid port");
	    }
	    return RHP_CANNOT_RESOLVE;
	}
    }

    /* Find an empty slot. */
    for (*slot = 0; *slot < GAI_SLOTS; (*slot)++) {
	if (!gai[*slot].busy) {
	    break;
	}
    }
    if (*slot >= GAI_SLOTS) {
	*slot = -1;
	if (errmsg) {
	    *errmsg = txdFree(NewString("Too many resolver reqests pending"));
	}
	return RHP_FATAL;
    }

    gai[*slot].pipe = pipe;
    gai[*slot].busy = true;
    gai[*slot].done = false;
# if defined(_WIN32) /*[*/
    gai[*slot].event = event;
# endif /*]*/

    gai[*slot].host = NewString(host);
    gai[*slot].port = portname? NewString(portname) : NULL;

# if !defined(_WIN32) /*[*/
    gai[*slot].hints.ai_flags = AI_ADDRCONFIG;
    gai[*slot].hints.ai_family = want_pf();
    gai[*slot].hints.ai_socktype = SOCK_STREAM;
    gai[*slot].hints.ai_protocol = IPPROTO_TCP;

    gai[*slot].gaicbs = &gai[*slot].gaicb;
    gai[*slot].gaicb.ar_name = host;
    gai[*slot].gaicb.ar_service = gai[*slot].port;
    gai[*slot].gaicb.ar_request = &gai[*slot].hints;
    gai[*slot].gaicb.ar_result = NULL;

    gai[*slot].sigevent.sigev_notify = SIGEV_THREAD;
    gai[*slot].sigevent.sigev_value.sival_int = *slot;
    gai[*slot].sigevent.sigev_notify_function = gai_notify;

    rc = getaddrinfo_a(GAI_NOWAIT, &gai[*slot].gaicbs, 1, &gai[*slot].sigevent);
    if (rc != 0) {
	if (errmsg) {
	    *errmsg = txAsprintf("%s/%s:\n%s", host, portname? portname: "(none)",
		    my_gai_strerror(rc));
	}
	cleanup_partial_slot(*slot);
	return RHP_CANNOT_RESOLVE;
    }
# else /*][*/
    thread = CreateThread(NULL, 0, async_resolve, &gai[*slot], 0, NULL);
    if (thread == INVALID_HANDLE_VALUE) {
	if (errmsg) {
	    *errmsg = txAsprintf("%s/%s:\n%s", host, portname? portname: "(none)",
		    win32_strerror(GetLastError()));
	}
	cleanup_partial_slot(*slot);
	return RHP_CANNOT_RESOLVE;
    }
    CloseHandle(thread);
# endif /*]*/

    return RHP_PENDING;
}

#endif /*]*/

/* Collect the status for a slot. */
rhp_t
collect_host_and_port(int slot, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, unsigned short *pport, char **errmsg, int max,
	int *nr)
{
#if defined(ASYNC_RESOLVER) /*[*/
# if !defined(_WIN32) /*[*/
    int rc;
# endif /*]*/
    struct addrinfo *res;
    int i;
    void *rsa = sa;
    struct gai *gaip = &gai[slot];

    assert(gaip->busy == true);
    assert(gaip->done == true);
    gaip->busy = false;
    gaip->done = false;

    *nr = 0;
# if !defined(_WIN32) /*[*/
    switch ((rc = gai_error(&gaip->gaicb))) {
    case 0:			/* success */
	/* Return the addresses. */
	res = gaip->gaicb.ar_result;
	for (i = 0; *nr < max && res != NULL; i++, res = res->ai_next) {
	    memcpy(rsa, res->ai_addr, res->ai_addrlen);
	    sa_rlen[*nr] = (socklen_t)res->ai_addrlen;
	    if (i == 0) {
		/* Return the port. */
		switch (res->ai_family) {
		case AF_INET:
		    *pport =
			ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port);
		    break;
		case AF_INET6:
		    *pport =
			ntohs(((struct sockaddr_in6 *)res->ai_addr)->sin6_port);
		    break;
		}
	    }
	    rsa = (char *)rsa + sa_len;
	    (*nr)++;
	}
	if (gaip->gaicb.ar_result != NULL) {
	    freeaddrinfo(gaip->gaicb.ar_result);
	    gaip->gaicb.ar_result = NULL;
	}
	if (*nr) {
	    Replace(gai->host, NULL);
	    Replace(gai->port, NULL);
	    return RHP_SUCCESS;
	} else {
	    if (errmsg) {
		*errmsg = txAsprintf("%s/%s:\n%s", gaip->host,
			gaip->port? gaip->port: "(none)",
			"no suitable resolution");
	    }
	    Replace(gai->host, NULL);
	    Replace(gai->port, NULL);
	    return RHP_CANNOT_RESOLVE;
	}
    case EAI_INPROGRESS:	/* still pending, should not happen */
    case EAI_CANCELED:		/* canceled, should not happen */
	assert(rc != EAI_INPROGRESS && rc != EAI_CANCELED);
	if (gaip->gaicb.ar_result != NULL) {
	    freeaddrinfo(gaip->gaicb.ar_result);
	    gaip->gaicb.ar_result = NULL;
	}
	Replace(gai->host, NULL);
	Replace(gai->port, NULL);
	return RHP_FATAL;
    default:			/* failure */
	if (gaip->gaicb.ar_result != NULL) {
	    freeaddrinfo(gaip->gaicb.ar_result);
	    gaip->gaicb.ar_result = NULL;
	}
	if (errmsg) {
	    *errmsg = txAsprintf("%s/%s:\n%s", gaip->host,
		    gaip->port? gaip->port: "(none)",
		    my_gai_strerror(rc));
	}
	Replace(gai->host, NULL);
	Replace(gai->port, NULL);
	return RHP_CANNOT_RESOLVE;
    }

# else /*][*/

    if (gaip->rc != 0) {
	if (errmsg) {
	    *errmsg = txAsprintf("%s/%s:\n%s", gaip->host,
		    gaip->port? gaip->port: "(none)",
		    my_gai_strerror(gaip->rc));
	}
	Replace(gai->host, NULL);
	Replace(gai->port, NULL);
	return RHP_CANNOT_RESOLVE;
    }

    /* Return the addresses. */
    res = gaip->result;
    for (i = 0; i < max && res != NULL; i++, res = res->ai_next) {
	memcpy(rsa, res->ai_addr, res->ai_addrlen);
	sa_rlen[*nr] = (socklen_t)res->ai_addrlen;
	if (i == 0) {
	    /* Return the port. */
	    switch (res->ai_family) {
	    case AF_INET:
		*pport =
		    ntohs(((struct sockaddr_in *) res->ai_addr)->sin_port);
		break;
	    case AF_INET6:
		*pport =
		    ntohs(((struct sockaddr_in6 *) res->ai_addr)->sin6_port);
		break;
	    default:
		if (errmsg) {
		    *errmsg = txAsprintf("%s:\nunknown family %d", gaip->host,
			    res->ai_family);
		}
		freeaddrinfo(gaip->result);
		Replace(gai->host, NULL);
		Replace(gai->port, NULL);
		return RHP_FATAL;
	    }
	}

	rsa = (char *)rsa + sa_len;
	(*nr)++;
    }

    freeaddrinfo(gaip->result);
    Replace(gai->host, NULL);
    Replace(gai->port, NULL);
    return RHP_SUCCESS;
# endif /*]*/
#else /*][*/
    if (errmsg != NULL) {
	*errmsg =
	    txdFree(NewString("Asynchronous name resolution not supported"));
    }
    return RHP_FATAL;
#endif /*]*/
}

/* Clean up a canceled request. */
void
cleanup_host_and_port(int slot)
{
#if defined(ASYNC_RESOLVER) /*[*/
    struct gai *gaip = &gai[slot];
# if !defined(_WIN32) /*[*/
    int rc;
# endif /*]*/

    assert(gaip->busy == true);
    assert(gaip->done == true);
    gaip->busy = false;
    gaip->done = false;

# if !defined(_WIN32) /*[*/
    switch ((rc = gai_error(&gaip->gaicb))) {
    case 0:			/* success */
	freeaddrinfo(gaip->gaicb.ar_result);
	gaip->gaicb.ar_result = NULL;
	break;
    case EAI_INPROGRESS:	/* still pending, should not happen */
    case EAI_CANCELED:		/* canceled, should not happen */
	assert(rc != EAI_INPROGRESS && rc != EAI_CANCELED);
	break;
    default:			/* failure */
	break;
    }
# else /*][*/
    if (gaip->rc == 0) {
	freeaddrinfo(gaip->result);
	gaip->result = NULL;
    }
# endif /*]*/

    Replace(gaip->host, NULL);
    Replace(gaip->port, NULL);
#endif /*]*/
}

/**
 * Mock the behavior of the synchronous resolver.
 *
 * @param[in] m		Mock definition
 * @param[in] host	Host name
 * @param[in] portname	Port name
 * @param[out] pport	Returned numeric port
 * @param[out] sa	Returned array of addresses
 * @param[in] sa_len	Number of elements in sa
 * @param[out] sa_rlen	Returned size of elements in sa
 * @param[out] errmsg	Returned error message
 * @param[in] max	Maximum number of elements to return
 * @param[out] nr	Number of elements returned
 *
 * @returns @ref rhp_t status
 */
static rhp_t
mock_sync_resolver(const char *m, const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr)
{
    /*
     * m is a string that looks like:
     *  address/port[;address/port...]
     * address is a numeric IPv4 or IPv6 address
     * port is a port name or number
     */
    char *mdup = NewString(m);
    char *outer_saveptr = NULL, *inner_saveptr = NULL;
    char *outer_chunk, *inner_chunk;
    char *outer_str, *inner_str;
    struct addrinfo hints;

    *nr = 0;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    outer_str = mdup;
    while (*nr < max &&
	    (outer_chunk = strtok_r(outer_str, ";", &outer_saveptr)) != NULL) {
	int np = 0;
	char *inner[2];
	struct addrinfo *res = NULL;

	outer_str = NULL;
	inner_str = outer_chunk;
	while ((inner_chunk = strtok_r(inner_str, "/", &inner_saveptr))
		!= NULL) {
	    inner_str = NULL;
	    assert(np < 2);
	    inner[np++] = inner_chunk;
	}
	assert(np == 2);

	assert(getaddrinfo(inner[0], inner[1], &hints, &res) == 0);
	memcpy(sa, res->ai_addr, res->ai_addrlen);
	sa_rlen[*nr] = (socklen_t)res->ai_addrlen;
	freeaddrinfo(res);
	sa = (struct sockaddr *)((char *)sa + sa_len);
	++(*nr);
    }

    Free(mdup);
    return RHP_SUCCESS;
}

/**
 * Resolve a hostname and port.
 * Synchronous version.
 *
 * @param[in] host	Host name
 * @param[in] portname	Port name
 * @param[out] pport	Returned numeric port
 * @param[out] sa	Returned array of addresses
 * @param[in] sa_len	Number of elements in sa
 * @param[out] sa_rlen	Returned size of elements in sa
 * @param[out] errmsg	Returned error message
 * @param[in] max	Maximum number of elements to return
 * @param[out] nr	Number of elements returned
 *
 * @returns RHP_XXX status
 */
rhp_t
resolve_host_and_port(const char *host, char *portname, unsigned short *pport,
	struct sockaddr *sa, size_t sa_len, socklen_t *sa_rlen, char **errmsg,
	int max, int *nr)
{
    const char *m = ut_getenv("MOCK_SYNC_RESOLVER");

    if (m != NULL && *m != '\0') {
	return mock_sync_resolver(m, host, portname, pport, sa, sa_len,
		sa_rlen, errmsg, max, nr);
    }
    return resolve_host_and_port_v46(host, portname, false, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr);
}

/**
 * Resolve a hostname and port.
 * Synchronous version, without -4/-6 preferences.
 *
 * @param[in] host	Host name
 * @param[in] portname	Port name
 * @param[out] pport	Returned numeric port
 * @param[out] sa	Returned array of addresses
 * @param[in] sa_len	Number of elements in sa
 * @param[out] sa_rlen	Returned size of elements in sa
 * @param[out] errmsg	Returned error message
 * @param[in] max	Maximum number of elements to return
 * @param[out] nr	Number of elements returned
 *
 * @returns RHP_XXX status
 */
rhp_t
resolve_host_and_port_abs(const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr)
{
    return resolve_host_and_port_v46(host, portname, true, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr);
}

/*
 * Resolve a hostname and port.
 * Asynchronous version.
 *
 * @param[in] host	Host name
 * @param[in] portname	Port name
 * @param[out] pport	Returned numeric port
 * @param[out] sa	Returned array of addresses
 * @param[in] sa_len	Number of elements in sa
 * @param[out] sa_rlen	Returned size of elements in sa
 * @param[out] errmsg	Returned error message
 * @param[in] max	Maximum number of elements to return
 * @param[out] nr	Number of elements returned
 *
 * @returns RHP_XXX status
 */
rhp_t
resolve_host_and_port_a(const char *host, char *portname, unsigned short *pport,
	struct sockaddr *sa, size_t sa_len, socklen_t *sa_rlen, char **errmsg,
	int max, int *nr, int *slot, int pipe, iosrc_t event)
{
#if defined(ASYNC_RESOLVER) /*[*/
    if (ut_getenv("SYNC_RESOLVER") == NULL) {
	return resolve_host_and_port_v46_a(host, portname, pport, sa, sa_len,
		sa_rlen, errmsg, max, nr, slot, pipe, event);
    }
#endif /*]*/
    *slot = -1;
    return resolve_host_and_port_v46(host, portname, false, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr);
}

#if defined(_WIN32) /*[*/
# define LEN DWORD
#else /*][*/
# define LEN size_t
#endif /*]*/

/*
 * Resolve a sockaddr into a numeric hostname and port.
 * Returns True for success, false for failure.
 */
bool
numeric_host_and_port(const struct sockaddr *sa, socklen_t salen, char *host,
	size_t hostlen, char *serv, size_t servlen, char **errmsg)
{
    int rc;

    /* Use getnameinfo(). */
    rc = getnameinfo(sa, salen, host, (LEN)hostlen, serv, (LEN)servlen,
	    NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0) {
	if (errmsg) {
	    *errmsg = txAsprintf("%s", my_gai_strerror(rc));
	}
	return false;
    }
    return true;
}
