/*
 * Copyright (c) 2007-2009, 2014-2016, 2019 Paul Mattes.
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

#include <assert.h>
#if !defined(_WIN32) /*[*/
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
#endif /*]*/

#include <stdio.h>
#include "lazya.h"
#include "resolver.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "winvers.h"
#endif /*]*/

#if defined(X3270_IPV6) && (defined(_WIN32) || defined(HAVE_GETADDRINFO_A)) /*[*/
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
    struct addrinfo result;	/* result */
# else /*][*/
    int rc;			/* return code */
    struct addrinfo *result;	/* result */
    HANDLE event;		/* event to signal */
# endif /*]*/
} gai[GAI_SLOTS];
#endif /*]*/

#if defined(X3270_IPV6) /*[*/
/*
 * Resolve a hostname and port using getaddrinfo, allowing IPv4 or IPv6.
 * Synchronous version.
 */
static rhp_t
resolve_host_and_port_v46(const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
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
		*errmsg = lazyaf("%s/%s:\n%s", host, portname, "Invalid port");
	    }
	    return RHP_CANNOT_RESOLVE;
	}
    }

    memset(&hints, '\0', sizeof(struct addrinfo));
    hints.ai_flags = 0;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    rc = getaddrinfo(host, portname, &hints, &res0);
    if (rc != 0) {
	if (errmsg) {
	    *errmsg = lazyaf("%s/%s:\n%s", host, portname? portname: "(none)",
		    gai_strerror(rc));
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
		    *errmsg = lazyaf("%s:\nunknown family %d", host,
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
#endif /*]*/

#if defined(ASYNC_RESOLVER) /*[*/

# if !defined(_WIN32) /*[*/
/* Notification function for lookup completion. */
static void
gai_notify(union sigval sigval)
{
    char slot = (char)sigval.sival_int;
    struct gai *gaip = &gai[(int)slot];

    assert(gaip->busy == true);
    assert(gaip->done == false);
    gaip->done = true;

    /*
     * Write our slot number into the pipe, so the main thread can poll us for
     * the completion status.
     */
    write(gaip->pipe, &slot, 1);
}

# else /*][*/

/* Asynchronous resolution thread. */
static DWORD WINAPI
async_resolve(LPVOID parameter)
{
    struct gai *gaip = (struct gai *)parameter;
    char slot = (char)(gaip - gai);
    struct addrinfo hints;

    assert(gaip->busy == true);
    assert(gaip->done == false);
    gaip->done = true;
    memset(&hints, '\0', sizeof(struct addrinfo));
    hints.ai_flags = 0;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    gaip->rc = getaddrinfo(gaip->host, gaip->port, &hints, &gaip->result);

    /*
     * Write our slot number into the pipe, so the main thread can poll us for
     * the completion status.
     */
    write(gaip->pipe, &slot, 1);

    /* Tell the main thread we are done. */
    SetEvent(gaip->event);

    /* Exit the thread. */
    return 0;
}

# endif /*]*/

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
# if !defined(_WIN32) /*[*/
    int rc;
# else /*][*/
    HANDLE thread;
# endif /*]*/

    *nr = 0;

    /* getaddrinfo() does not appear to range-check the port. Do that here. */
    if (portname != NULL) {
	unsigned long l;

	if ((l = strtoul(portname, NULL, 0)) && (l & ~0xffffL)) {
	    if (errmsg) {
		*errmsg = lazyaf("%s/%s:\n%s", host, portname, "Invalid port");
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
	    *errmsg = lazya(NewString("Too many resolver reqests pending"));
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
    gai[*slot].result.ai_flags = 0;
    gai[*slot].result.ai_family = PF_UNSPEC;
    gai[*slot].result.ai_socktype = SOCK_STREAM;
    gai[*slot].result.ai_protocol = IPPROTO_TCP;

    gai[*slot].gaicbs = &gai[*slot].gaicb;
    gai[*slot].gaicb.ar_name = host;
    gai[*slot].gaicb.ar_service = portname;
    gai[*slot].gaicb.ar_result = &gai[*slot].result;

    gai[*slot].sigevent.sigev_notify = SIGEV_THREAD;
    gai[*slot].sigevent.sigev_value.sival_int = *slot;
    gai[*slot].sigevent.sigev_notify_function = gai_notify;

    rc = getaddrinfo_a(GAI_NOWAIT, &gai[*slot].gaicbs, 1, &gai[*slot].sigevent);
    if (rc != 0) {
	if (errmsg) {
	    *errmsg = lazyaf("%s/%s:\n%s", host, portname? portname: "(none)",
		    gai_strerror(rc));
	}
	return RHP_CANNOT_RESOLVE;
    }
# else /*][*/
    thread = CreateThread(NULL, 0, async_resolve, &gai[*slot], 0, NULL);
    if (thread == INVALID_HANDLE_VALUE) {
	if (errmsg) {
	    *errmsg = lazyaf("%s/%s:\n%s", host, portname? portname: "(none)",
		    win32_strerror(GetLastError()));
	}
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
	for (i = 0; i < max && res != NULL; i++, res = res->ai_next) {
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
		default:
		    if (errmsg) {
			*errmsg = lazyaf("unknown family %d", res->ai_family);
		    }
		    freeaddrinfo(gaip->gaicb.ar_result);
		    return RHP_FATAL;
		}
	    }
	    rsa = (char *)rsa + sa_len;
	    (*nr)++;
	}
	freeaddrinfo(gaip->gaicb.ar_result);
	return RHP_SUCCESS;
    case EAI_INPROGRESS:	/* still pending, should not happen */
    case EAI_CANCELED:		/* canceled, should not happen */
	assert(rc != EAI_INPROGRESS && rc != EAI_CANCELED);
	return RHP_FATAL;
    default:			/* failure */
	if (errmsg) {
	    *errmsg = lazyaf("%s/%s:\n%s", gaip->host,
		    gaip->port? gaip->port: "*(none)",
		    gai_strerror(rc));
	}
	return RHP_CANNOT_RESOLVE;
    }

# else /*][*/

    if (gaip->rc != 0) {
	if (errmsg) {
	    *errmsg = lazyaf("%s/%s:\n%s", gaip->host,
		    gaip->port? gaip->port: "*(none)",
		    gai_strerror(gaip->rc));
	}
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
		    *errmsg = lazyaf("%s:\nunknown family %d", gaip->host,
			    res->ai_family);
		}
		freeaddrinfo(gaip->result);
		return RHP_FATAL;
	    }
	}

	rsa = (char *)rsa + sa_len;
	(*nr)++;
    }

    freeaddrinfo(gaip->result);
    return RHP_SUCCESS;
# endif /*]*/
#else /*][*/
    if (errmsg != NULL) {
	*errmsg =
	    lazya(NewString("Asynchronous name resolution not supported"));
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
    }
# endif /*]*/

    Replace(gaip->host, NULL);
    Replace(gaip->port, NULL);
#endif /*]*/
}

#if !defined(X3270_IPV6) /*[*/
/*
 * Resolve a hostname and port using gethostbyname and getservbyname, and
 * allowing only IPv4.
 * Synchronous only.
 */
static rhp_t
resolve_host_and_port_v4(const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr)
{
    struct hostent *hp;
    struct servent *sp;
    unsigned short port;
    unsigned long lport;
    char *ptr;
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    int i;
    void *rsa = sa;

    *nr = 0;

    /* Get the port number. */
    lport = strtoul(portname, &ptr, 0);
    if (ptr == portname || *ptr != '\0' || lport == 0L || lport & ~0xffff) {
	if (!(sp = getservbyname(portname, "tcp"))) {
	    if (errmsg) {
		*errmsg = lazyaf("Unknown port number or service: %s",
			portname);
	    }
	    return RHP_FATAL;
	}
	port = sp->s_port;
    } else {
	port = htons((unsigned short)lport);
    }
    *pport = ntohs(port);

    /* Use gethostbyname() to resolve the hostname. */
    hp = gethostbyname(host);
    if (hp == (struct hostent *)0) {
	/* Try inet_addr(). */
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == INADDR_NONE) {
	    if (errmsg) {
		*errmsg = lazyaf("Unknown host:\n%s", host);
	    }
	    return RHP_CANNOT_RESOLVE;
	}
	*sa_rlen = sizeof(struct sockaddr_in);
	*nr = 1;
	return RHP_SUCCESS;
    }

    /* Return the addresses. */
    for (i = 0; i < max && hp->h_addr_list[i] != NULL; i++) {
	struct sockaddr_in *rsin = rsa;

	rsin->sin_family = hp->h_addrtype;
	memcpy(&rsin->sin_addr, hp->h_addr_list[i], hp->h_length);
	rsin->sin_port = port;
	sa_rlen[*nr] = sizeof(struct sockaddr_in);

	rsa = (char *)rsa + sa_len;
	(*nr)++;
    }

    return RHP_SUCCESS;
}
#endif /*]*/

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
#if defined(X3270_IPV6) /*[*/
    return resolve_host_and_port_v46(host, portname, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr);
#else /*][*/
    return resolve_host_and_port_v4(host, portname, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr);
#endif
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
    return resolve_host_and_port_v46_a(host, portname, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr, slot, pipe, event);
#else /*][*/
    *slot = -1;
# if defined(X3270_IPV6)  /*][*/
    return resolve_host_and_port_v46(host, portname, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr);
# else /*][*/
    return resolve_host_and_port_v4(host, portname, pport, sa, sa_len,
	    sa_rlen, errmsg, max, nr);
# endif /*]*/
#endif /*]*/
}

#if defined(_WIN32) /*[*/
# define LEN DWORD
#else /*][*/
# define LEN size_t
#endif /*]*/

/*
 * Resolve a sockaddr into a numeric hostname and port.
 * Returns Trur for success, false for failure.
 */
bool
numeric_host_and_port(const struct sockaddr *sa, socklen_t salen, char *host,
	size_t hostlen, char *serv, size_t servlen, char **errmsg)
{
#if defined(X3270_IPV6) /*[*/
    int rc;

    /* Use getnameinfo(). */
    rc = getnameinfo(sa, salen, host, (LEN)hostlen, serv, (LEN)servlen,
	    NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0) {
	if (errmsg) {
	    *errmsg = lazyaf("%s", gai_strerror(rc));
	}
	return false;
    }
    return true;
#else /*][*/
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

    /* Use inet_ntoa() and snprintf(). */
    snprintf(host, hostlen, "%s", inet_ntoa(sin->sin_addr));
    snprintf(serv, servlen, "%u", ntohs(sin->sin_port));
    return true;
#endif /*]*/
}
