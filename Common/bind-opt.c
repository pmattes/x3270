/*
 * Copyright (c) 2014-2024 Paul Mattes.
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
 *      bind-opt.c
 *              Option parsing for -scriptport and -httpd.
 */

#include "globals.h"

#include <limits.h>
#include <errno.h>
#if !defined(_WIN32) /*[*/
# include <arpa/inet.h>
# include <netinet/in.h>
#endif /*]*/

#include "wincmn.h"
#include "resolver.h"
#include "txa.h"
#include "utils.h"

#include "bind-opt.h"

typedef union {
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;
} sau_t;

/**
 * Parse a bind option for -httpd or -scriptport.
 * @verbatim
 * Syntax:
 *  <port> or :<port>
 *   implies 127.0.0.1
 *  <ip4addr>:<port>
 *  *:<port>
 *   implies 0.0.0.0
 *  [<ip6addr>]:<port>
 *
 * So, to bind to INADDR_ANY, port 4080, specify:
 *  *:4080
 * or
 *  0.0.0.0:4080
 * To bind to the same thing in IPv6, specify:
 *  [::]:4080
 * To bind to the IPv6 loopback address, specify:
 *  [::1]:4080
 * @endverbatim
 * It does not understand symbolic port names like 'telnet', and it does not
 * understand symbolic host names.
 *
 * @param[in] spec	string to parse
 * @param[out] addr	returned address
 * @param[out] addrlen	returned length of address
 *
 * @return true if address parsed successfully, false otherwise
 */
bool
parse_bind_opt(const char *spec, struct sockaddr **addr, socklen_t *addrlen)
{
    size_t hlen;
    char *host_str;
    char *port_str;
    unsigned short port;
    rhp_t rv;
    int nr;

    /* Start with nothing. */
    *addr = NULL;
    *addrlen = 0;

    if (spec == NULL || *spec == '\0') {
	return false;
    }

    /* Tease apart the syntax. */
    if (spec[0] == '[') {
	char *rbrack = strchr(spec, ']');

	/* We appear to have a hostname in square brackets. */
	if (rbrack == NULL ||
	    rbrack == spec + 1 ||
	    *(rbrack + 1) != ':' ||
	    !*(rbrack + 2)) {

	    return false;
	}

	hlen = rbrack - spec - 1;
	host_str = Malloc(hlen + 1);
	strncpy(host_str, spec + 1, hlen);
	host_str[hlen] = '\0';

	port_str = Malloc(strlen(rbrack + 2) + 1);
	strcpy(port_str, rbrack + 2);
    } else {
	char *colon;

	/* No square brackets. Use the colon to split the address and port. */
	colon = strchr(spec, ':');
	if (colon == NULL) {
	    /* Just a port. */
	    host_str = NewString("127.0.0.1");
	    port_str = NewString(spec);
	} else if (colon == spec) {
	    /* Just a colon and a port. */
	    if (!*(colon + 1)) {
		return false;
	    }
	    host_str = NewString("127.0.0.1");
	    port_str = NewString(spec + 1);
	} else {
	    /* <address>:<port>. */
	    if (colon == NULL || colon == spec || !*(colon + 1)) {
		return false;
	    }

	    hlen = colon - spec;
	    host_str = Malloc(hlen + 1);
	    strncpy(host_str, spec, hlen);
	    host_str[hlen] = '\0';

	    port_str = Malloc(strlen(colon + 1) + 1);
	    strcpy(port_str, colon + 1);
	}
    }

    /* Translate '*'. */
    if (!strcmp(host_str, "*")) {
	Free(host_str);
	host_str = NewString("0.0.0.0");
    }

    /* Use the resolver to resolve the components we've split apart. */
    *addr = Malloc(sizeof(sau_t));
    rv = resolve_host_and_port_abs(host_str, port_str, &port, *addr,
	    sizeof(sau_t), addrlen, NULL, 1, &nr);
    Free(host_str);
    Free(port_str);
    if (RHP_IS_ERROR(rv)) {
	Free(*addr);
	*addr = NULL;
	return false;
    }

    return true;
}

/**
 * Return the canonical form of a bind option.
 *
 * @param[in] sa	Sockaddr to encode
 *
 * @returns encoded address and port
 */
const char *
canonical_bind_opt(struct sockaddr *sa)
{
#   define RET_LEN 128
    char addrbuf[RET_LEN];
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;

    switch (sa->sa_family) {
    case AF_INET:
	sin = (struct sockaddr_in *)sa;
	return txAsprintf("[%s]:%u",
	    inet_ntop(sa->sa_family, &sin->sin_addr, addrbuf, RET_LEN),
	    ntohs(sin->sin_port));
    case AF_INET6:
	sin6 = (struct sockaddr_in6 *)sa;
	return txAsprintf("[%s]:%u",
	    inet_ntop(sa->sa_family, &sin6->sin6_addr, addrbuf, RET_LEN),
	    ntohs(sin6->sin6_port));
    default:
	return "unknown";
    }
}

/**
 * Return the canonical form of a bind option, given a resource value.
 *
 * @param[in] res	Resource value (might be NULL)
 *
 * @returns Canonical representation
 */
const char *
canonical_bind_opt_res(const char *res)
{
    struct sockaddr *sa;
    socklen_t len;
    const char *ret;

    if (res == NULL || !parse_bind_opt(res, &sa, &len)) {
	return NULL;
    }
    ret = canonical_bind_opt(sa);
    Free(sa);
    return ret;
}
