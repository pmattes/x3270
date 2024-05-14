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
 *	proxy_socks4.c
 *		SOCKS version 4 proxy.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <errno.h>
# include <sys/ioctl.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
#endif /*]*/

#include "3270ds.h"
#include "popups.h"
#include "proxy.h"
#include "proxy_private.h"
#include "proxy_socks4.h"
#include "resolver.h"
#include "telnet_core.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "w3misc.h"

#define REPLY_LEN 8

static struct {
    socket_t fd;
    bool use_4a;
    size_t nread;
    unsigned char rbuf[REPLY_LEN];
} ps = { INVALID_SOCKET, false, 0 };

/* SOCKS version 4 proxy. */
proxy_negotiate_ret_t
proxy_socks4(socket_t fd, const char *user, const char *host,
	unsigned short port, bool force_a)
{
    struct hostent *hp;
    struct in_addr ipaddr;
    const char *ruser;
    char *sbuf;
    char *s;

    ps.fd = fd;
    ps.use_4a = false;
    ps.nread = 0;

    /* Resolve the hostname to an IPv4 address. */
    if (force_a) {
	ps.use_4a = true;
    } else {
	hp = gethostbyname(host);
	if (hp != NULL) {
	    memcpy(&ipaddr, hp->h_addr, hp->h_length);
	} else {
	    ipaddr.s_addr = inet_addr(host);
	    if (ipaddr.s_addr == (in_addr_t)-1) {
		ps.use_4a = true;
	    }
	}
    }

    /* Resolve the username. */
    if (user != NULL) {
	ruser = user;
    } else {
#if !defined(_WIN32) /*[*/
	ruser = getenv("USER");
#else /*][*/
	ruser = getenv("USERNAME");
#endif /*]*/
	if (ruser == NULL) {
	    ruser = "nobody";
	}
    }

    /* Send the request to the server. */
    if (ps.use_4a) {
	sbuf = Malloc(32 + strlen(ruser) + strlen(host));
	s = sbuf;
	*s++ = 0x04;
	*s++ = 0x01;
	SET16(s, port);
	SET32(s, 0x00000001);
	strcpy(s, ruser);
	s += strlen(ruser) + 1;
	strcpy(s, host);
	s += strlen(host) + 1;

	vtrace("SOCKS4 Proxy: version 4 connect port %u address 0.0.0.1 user "
		"'%s' host '%s'\n", port, ruser, host);
	trace_netdata('>', (unsigned char *)sbuf, s - sbuf);

	if (send(fd, sbuf, (int)(s - sbuf), 0) < 0) {
	    popup_a_sockerr("SOCKS4 Proxy: send error");
	    Free(sbuf);
	    return PX_FAILURE;
	}
	Free(sbuf);
    } else {
	unsigned long u;

	sbuf = Malloc(32 + strlen(ruser));
	s = sbuf;
	*s++ = 0x04;
	*s++ = 0x01;
	SET16(s, port);
	u = ntohl(ipaddr.s_addr);
	SET32(s, u);
	strcpy(s, ruser);
	s += strlen(ruser) + 1;

	vtrace("SOCKS4 Proxy: xmit version 4 connect port %u address %s user "
		"'%s'\n", port, inet_ntoa(ipaddr), ruser);
	trace_netdata('>', (unsigned char *)sbuf, s - sbuf);

	if (send(fd, sbuf, (int)(s - sbuf), 0) < 0) {
	    Free(sbuf);
	    popup_a_sockerr("SOCKS4 Proxy: send error");
	    return PX_FAILURE;
	}
	Free(sbuf);
    }

    return PX_WANTMORE;
}

/* SOCKS version 4 continuation. */
proxy_negotiate_ret_t
proxy_socks4_continue(void)
{
    /*
     * Process the reply.
     * Read the response.
     */
    for (;;) {
	ssize_t nr = recv(ps.fd, (char *)&ps.rbuf[ps.nread], 1, 0);

	if (nr < 0) {
	    if (socket_errno() == SE_EWOULDBLOCK) {
		if (ps.nread) {
		    trace_netdata('<', ps.rbuf, ps.nread);
		}
		return PX_WANTMORE;
	    }
	    popup_a_sockerr("SOCKS4 Proxy: receive error");
	    return PX_FAILURE;
	}
	if (nr == 0) {
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    popup_an_error("SOCKS4 Proxy: unexpected EOF");
	    return PX_FAILURE;
	    /* XXX: was break */
	}
	if (++ps.nread >= REPLY_LEN) {
	    break;
	}
    }

    trace_netdata('<', ps.rbuf, ps.nread);
    if (ps.use_4a) {
	struct in_addr a;
	unsigned short rport = (ps.rbuf[2] << 8) | ps.rbuf[3];

	memcpy(&a, &ps.rbuf[4], 4);
	vtrace("SOCKS4 Proxy: recv status 0x%02x port %u address %s\n",
		ps.rbuf[1], rport, inet_ntoa(a));
    } else {
	vtrace("SOCKS4 Proxy: recv status 0x%02x\n", ps.rbuf[1]);
    }

    switch (ps.rbuf[1]) {
    case 0x5a:
	break;
    case 0x5b:
	popup_an_error("SOCKS4 Proxy: request rejected or failed");
	return PX_FAILURE;
    case 0x5c:
	popup_an_error("SOCKS4 Proxy: client is not reachable");
	return PX_FAILURE;
    case 0x5d:
	popup_an_error("SOCKS4 Proxy: userid error");
	return PX_FAILURE;
    default:
	popup_an_error("SOCKS4 Proxy: unknown status 0x%02x",
		ps.rbuf[1]);
	return PX_FAILURE;
    }

    return PX_SUCCESS;
}

void
proxy_socks4_close(void)
{
    ps.fd = INVALID_SOCKET;
    ps.use_4a = false;
    ps.nread = 0;
}
