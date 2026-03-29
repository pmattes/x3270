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
#include "sockaddr_46.h"	/* needed by resolver_pipe.h */
#include "resolver_pipe.h"
#include "task.h"
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
    char *host;
    char *user;
    unsigned short port;
    rp_t rp;
    proxy_disconnect_fn *async_disconnect;
    sockaddr_46_t ha;
    socklen_t ha_len;
} ps = { INVALID_SOCKET, false, 0 };

static proxy_negotiate_ret_t
send_request(socket_t fd)
{
    const char *ruser;
    char *sbuf;
    char *s;

    /* Resolve the username. */
    if (ps.user != NULL) {
	ruser = ps.user;
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
	sbuf = Malloc(32 + strlen(ruser) + strlen(ps.host));
	s = sbuf;
	*s++ = 0x04;
	*s++ = 0x01;
	SET16(s, ps.port);
	SET32(s, 0x00000001);
	strcpy(s, ruser);
	s += strlen(ruser) + 1;
	strcpy(s, ps.host);
	s += strlen(ps.host) + 1;

	vctrace(TC_PROXY, "SOCKS4: version 4 connect port %u address 0.0.0.1 user '%s' host '%s'\n",
		ps.port, ruser, ps.host);
	trace_netdata('>', (unsigned char *)sbuf, s - sbuf);

	if (send(fd, sbuf, (int)(s - sbuf), 0) < 0) {
	    popup_a_sockerr("SOCKS4 proxy: send error");
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
	SET16(s, ps.port);
	u = ntohl(ps.ha.sin.sin_addr.s_addr);
	SET32(s, u);
	strcpy(s, ruser);
	s += strlen(ruser) + 1;

	vctrace(TC_PROXY, "SOCKS4: xmit version 4 connect port %u address %s user '%s'\n",
		ps.port, inet_ntoa(ps.ha.sin.sin_addr), ruser);
	trace_netdata('>', (unsigned char *)sbuf, s - sbuf);

	if (send(fd, sbuf, (int)(s - sbuf), 0) < 0) {
	    Free(sbuf);
	    popup_a_sockerr("SOCKS4 proxy: send error");
	    return PX_FAILURE;
	}
	Free(sbuf);
    }

    return PX_WANTMORE;
}

/* Handle a resolution completion. */
static void
rp_done(rp_t rp, void *context, bool success, const char *errmsg)
{
    if (success) {
	ps.ha_len = rp_ha_len(rp, 0);
	memcpy(&ps.ha, rp_haddr(rp, 0), ps.ha_len);
	if (ps.ha.sa.sa_family != AF_INET) {
	    vctrace(TC_PROXY, "SOCKS4: %s resolves to IPv6, switching to SOCKS4A\n", ps.host);
	    ps.use_4a = true;
	}
    } else {
	/* Failed, switch to 4A. */
        vctrace(TC_PROXY, "SOCKS4: %s %s, switching to SOCKS4A\n", ps.host, errmsg);
	ps.use_4a = true;
    }

    /* Send the request. */
    if (send_request(ps.fd) == PX_FAILURE) {
        /* Error has already popped up. */
	ps.async_disconnect();
    }
}

/* SOCKS version 4 proxy. */
proxy_negotiate_ret_t
proxy_socks4(socket_t fd, const char *user, const char *host,
	unsigned short port, bool force_a, proxy_disconnect_fn async_disconnect)
{
    ps.fd = fd;
    ps.use_4a = false;
    ps.nread = 0;
    Replace(ps.user, NewString(user));
    Replace(ps.host, NewString(host));
    ps.port = port;

    /* Resolve the hostname to an IPv4 address. */
    if (force_a) {
	ps.use_4a = true;
    } else {
	const char *errmsg;

	ps.async_disconnect = async_disconnect;
        if (async_disconnect == NULL) {
            rhp_t rv;
            int nr;
	    unsigned short rport;

            rv = resolve_host_and_port_blocking(host, NULL, PF_UNSPEC, &rport, &ps.ha.sa, sizeof(ps.ha),
                    &ps.ha_len, &errmsg, 1, &nr);
            if (rv == RHP_CANNOT_RESOLVE) {
                ps.use_4a = true;
            } else if (RHP_IS_ERROR(rv)) {
                connect_error("SOCKS4 proxy: %s/%u: %s", host, port, errmsg);
                return PX_FAILURE;
            }
        } else {
	    rp_result_t result;

	    if (ps.rp == NULL && (ps.rp = rp_alloc(NULL, rp_done)) == NULL) {
		connect_error("SOCKS4 proxy: unable to allocate resolver context");
		return PX_FAILURE;
	    }

	    result = rp_resolve(ps.rp, host, NULL, PF_UNSPEC, &errmsg);
	    if (result == RP_FAIL) {
		vctrace(TC_PROXY, "SOCKS4 proxy: %s: %s, switching to SOCKS4A\n", host, errmsg);
		ps.use_4a = true;
		/* and continue */
	    } else if (result == RP_PENDING) {
		return PX_WANTMORE;
	    } else {
		/* Worked synchronously. */
		ps.ha_len = rp_ha_len(ps.rp, 0);
		memcpy(&ps.ha, rp_haddr(ps.rp, 0), ps.ha_len);
		if (ps.ha.sa.sa_family != AF_INET) {
		    vctrace(TC_PROXY, "SOCKS4 proxy: %s resolves to IPv6, switching to SOCKS4A\n", host);
		    ps.use_4a = true;
		}
	    }
	}
    }

    return send_request(fd);
}

/* SOCKS version 4 continuation. */
proxy_negotiate_ret_t
proxy_socks4_continue(socket_t fd)
{
    /*
     * Process the reply.
     * Read the response.
     */
    for (;;) {
	ssize_t nr = recv(fd, (char *)&ps.rbuf[ps.nread], 1, 0);

	if (nr < 0) {
	    if (IS_EWOULDBLOCK(socket_errno())) {
		if (ps.nread) {
		    trace_netdata('<', ps.rbuf, ps.nread);
		}
		return PX_WANTMORE;
	    }
	    popup_a_sockerr("SOCKS4 proxy: receive error");
	    return PX_FAILURE;
	}
	if (nr == 0) {
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    connect_error("SOCKS4 proxy: unexpected EOF");
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
	vctrace(TC_PROXY, "SOCKS4: recv status 0x%02x port %u address %s\n",
		ps.rbuf[1], rport, inet_ntoa(a));
    } else {
	vctrace(TC_PROXY, "SOCKS4: recv status 0x%02x\n", ps.rbuf[1]);
    }

    switch (ps.rbuf[1]) {
    case 0x5a:
	break;
    case 0x5b:
	connect_error("SOCKS4 proxy: request rejected or failed");
	return PX_FAILURE;
    case 0x5c:
	connect_error("SOCKS4 proxy: client is not reachable");
	return PX_FAILURE;
    case 0x5d:
	connect_error("SOCKS4 proxy: userid error");
	return PX_FAILURE;
    default:
	connect_error("SOCKS4 proxy: unknown status 0x%02x", ps.rbuf[1]);
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
    Replace(ps.host, NULL);
    Replace(ps.user, NULL);
    ps.port = 0;
    rp_free(&ps.rp);
    ps.async_disconnect = NULL;
    memset(&ps.ha, 0, sizeof(sockaddr_46_t));
    ps.ha_len = 0;
}
