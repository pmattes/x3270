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
 *	resolver_abs.c
 *		Hostname resolution.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <signal.h>
#else /*][*/
# include "w3misc.h"
#endif /*]*/

#include "gai_strerror.h"
#include "resolver.h"
#include "txa.h"
#include "utils.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "winvers.h"
#endif /*]*/

/**
 * Resolve a hostname and port, using PF_UNSPEC regardless of other settings, and not allowing unit tests to
 * insert a delay.
 * Blocking version (there is no async version).
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
resolve_host_and_port_abs(const char *host, const char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, const char **errmsg, int max, int *nr)
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
		*errmsg = NewString("Invalid port");
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
	    *errmsg = my_gai_strerror(rc);
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
