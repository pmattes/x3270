/*
 * Copyright (c) 2007-2009, 2014-2016 Paul Mattes.
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

#if !defined(_WIN32) /*[*/
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif /*]*/

#include <stdio.h>
#include "lazya.h"
#include "resolver.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "winvers.h"
#endif /*]*/

#if defined(X3270_IPV6) /*[*/
/*
 * Resolve a hostname and port using getaddrinfo, allowing IPv4 or IPv6.
 * Returns RHP_SUCCESS for success, RHP_FATAL for fatal error (name resolution
 * impossible), RHP_CANNOT_RESOLVE for simple error (cannot resolve the name).
 *
 * XXX: Apparently getaddrinfo does not range-check a numeric service.
 */
static rhp_t
resolve_host_and_port_v46(const char *host, char *portname, int ix,
	unsigned short *pport, struct sockaddr *sa, socklen_t *sa_len,
	char **errmsg, int *lastp)
{
    struct addrinfo hints, *res0, *res;
    int rc;

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

    (void) memset(&hints, '\0', sizeof(struct addrinfo));
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

    /*
     * Return the reqested element.
     * Hopefully the list will not change between calls.
     */
    while (ix && res->ai_next != NULL) {
	res = res->ai_next;
	ix--;
    }
    if (res == NULL) {
	/* Ran off the end?  The list must have changed. */
	if (errmsg) {
	    *errmsg = lazyaf("%s/%s:\n%s", host, portname? portname: "(none)",
		    gai_strerror(EAI_AGAIN));
	}
	freeaddrinfo(res);
	return RHP_CANNOT_RESOLVE;
    }

    switch (res->ai_family) {
    case AF_INET:
	*pport = ntohs(((struct sockaddr_in *) res->ai_addr)->sin_port);
	break;
    case AF_INET6:
	*pport = ntohs(((struct sockaddr_in6 *) res->ai_addr)->sin6_port);
	break;
    default:
	if (errmsg) {
	    *errmsg = lazyaf("%s:\nunknown family %d", host, res->ai_family);
	}
	freeaddrinfo(res);
	return RHP_FATAL;
    }
    (void) memcpy(sa, res->ai_addr, res->ai_addrlen);
    *sa_len = (socklen_t)res->ai_addrlen;
    if (lastp != NULL) {
	*lastp = (res->ai_next == NULL);
    }
    freeaddrinfo(res0);

    return RHP_SUCCESS;
}
#endif /*]*/

#if !defined(X3270_IPV6) /*[*/
/*
 * Resolve a hostname and port using gethostbyname and getservbyname, and
 * allowing only IPv4.
 * Returns RHP_SUCCESS for success, RHP_FATAL for fatal error (name resolution
 * impossible), RHP_CANNOT_RESOLVE for simple error (cannot resolve the name).
 */
static rhp_t
resolve_host_and_port_v4(const char *host, char *portname, int ix,
	unsigned short *pport, struct sockaddr *sa, socklen_t *sa_len,
	char **errmsg, int *lastp)
{
    struct hostent *hp;
    struct servent *sp;
    unsigned short port;
    unsigned long lport;
    char *ptr;
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

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
    if (hp == (struct hostent *) 0) {
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == INADDR_NONE) {
	    if (errmsg) {
		*errmsg = lazyaf("Unknown host:\n%s", host);
	    }
	    return RHP_CANNOT_RESOLVE;
	}
	if (lastp != NULL) {
	    *lastp = true;
	}
    } else {
	int i;

	for (i = 0; i < ix; i++) {
	    if (hp->h_addr_list[i] == NULL) {
		if (errmsg) {
		    *errmsg = lazyaf("Unknown host:\n%s", host);
		}
		return RHP_CANNOT_RESOLVE;
	    }
	}
	sin->sin_family = hp->h_addrtype;
	(void) memmove(&sin->sin_addr, hp->h_addr_list[i], hp->h_length);
	if (lastp != NULL) {
	    *lastp = (hp->h_addr_list[i + 1] == NULL);
	}
    }
    sin->sin_port = port;
    *sa_len = sizeof(struct sockaddr_in);

    return RHP_SUCCESS;
}
#endif /*]*/

/*
 * Resolve a hostname and port.
 * Returns RHP_SUCCESS for success, RHP_FATAL for fatal error (name resolution
 * impossible), RHP_CANNOT_RESOLVE for simple error (cannot resolve the name).
 */
rhp_t
resolve_host_and_port(const char *host, char *portname, int ix,
	unsigned short *pport, struct sockaddr *sa, socklen_t *sa_len,
	char **errmsg, int *lastp)
{
#if !defined(X3270_IPV6) /*[*/
    return resolve_host_and_port_v4(host, portname, ix, pport, sa, sa_len,
	    errmsg, lastp);
#else /*][*/
    return resolve_host_and_port_v46(host, portname, ix, pport, sa, sa_len,
	    errmsg, lastp);
#endif
}

#if defined(X3270_IPV6) /*[*/
/*
 * Resolve a sockaddr into a numeric hostname and port, IPv4 or IPv6.
 * Returns true for success, false for failure.
 */
# if defined(_WIN32) /*[*/
#  define LEN DWORD
# else /*][*/
#  define LEN size_t
# endif /*]*/
static bool
numeric_host_and_port_v46(const struct sockaddr *sa, socklen_t salen,
	char *host, size_t hostlen, char *serv, size_t servlen, char **errmsg)
{
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
}
#endif /*]*/

#if !defined(X3270_IPV6) /*[*/
/*
 * Resolve a sockaddr into a numeric hostname and port, IPv4 only.
 * Returns true for success, false for failure.
 */
static bool
numeric_host_and_port_v4(const struct sockaddr *sa, socklen_t salen,
	char *host, size_t hostlen, char *serv, size_t servlen, char **errmsg)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

    /* Use inet_ntoa() and snprintf(). */
    snprintf(host, hostlen, "%s", inet_ntoa(sin->sin_addr));
    snprintf(serv, servlen, "%u", ntohs(sin->sin_port));
    return true;
}
#endif /*]*/

/*
 * Resolve a sockaddr into a numeric hostname and port.
 * Returns Trur for success, false for failure.
 */
bool
numeric_host_and_port(const struct sockaddr *sa, socklen_t salen, char *host,
	size_t hostlen, char *serv, size_t servlen, char **errmsg)
{
#if !defined(X3270_IPV6) /*[*/
    return numeric_host_and_port_v4(sa, salen, host, hostlen, serv, servlen,
	    errmsg);
#else /*][*/
    return numeric_host_and_port_v46(sa, salen, host, hostlen, serv, servlen,
	    errmsg);
#endif /*]*/
}
