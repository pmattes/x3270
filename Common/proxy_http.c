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
 *	proxy_http.c
 *		RFC 2817 HTTP CONNECT tunnel proxy.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <errno.h>
# include <sys/ioctl.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
#endif /*]*/

#include "base64.h"
#include "popups.h"
#include "proxy.h"
#include "proxy_private.h"
#include "proxy_http.h"
#include "resolver.h"
#include "telnet_core.h"
#include "txa.h"
#include "trace.h"
#include "utils.h"
#include "w3misc.h"

#define RBUF	1024

/* Proxy state. */
static struct {
    socket_t fd;
    unsigned char *rbuf;
    size_t nread;
} ps = { INVALID_SOCKET, NULL, 0 };

/* HTTP (RFC 2817 CONNECT tunnel) proxy. */
proxy_negotiate_ret_t
proxy_http(socket_t fd, const char *user, const char *host, unsigned short port)
{
    char *sbuf;
    char *colon;

    ps.fd = fd;
    ps.rbuf = Malloc(RBUF);
    ps.nread = 0;

    /* Send the CONNECT request. */
    colon = strchr(host, ':');
    sbuf = Asprintf("CONNECT %s%s%s:%u HTTP/1.1\r\n",
	    (colon? "[": ""),
	    host,
	    (colon? "]": ""),
	    port);

    vtrace("HTTP Proxy: xmit '%.*s'\n", (int)(strlen(sbuf) - 2), sbuf);
    trace_netdata('>', (unsigned char *)sbuf, strlen(sbuf));

    if (send(fd, sbuf, (int)strlen(sbuf), 0) < 0) {
	popup_a_sockerr("HTTP Proxy: send error");
	Free(sbuf);
	return PX_FAILURE;
    }

    Free(sbuf);
    sbuf = Asprintf("Host: %s%s%s:%u\r\n",
	    (colon? "[": ""),
	    host,
	    (colon? "]": ""),
	    port);

    vtrace("HTTP Proxy: xmit '%.*s'\n", (int)(strlen(sbuf) - 2), sbuf);
    trace_netdata('>', (unsigned char *)sbuf, strlen(sbuf));

    if (send(fd, sbuf, (int)strlen(sbuf), 0) < 0) {
	popup_a_sockerr("HTTP Proxy: send error");
	Free(sbuf);
	return PX_FAILURE;
    }

    if (user != NULL) {
	Free(sbuf);
	sbuf = Asprintf("Proxy-Authorization: Basic %s\r\n",
		txdFree(base64_encode(user)));

	vtrace("HTTP Proxy: xmit '%.*s'\n", (int)(strlen(sbuf) - 2), sbuf);
	trace_netdata('>', (unsigned char *)sbuf, strlen(sbuf));

	if (send(fd, sbuf, (int)strlen(sbuf), 0) < 0) {
	    popup_a_sockerr("HTTP Proxy: send error");
	    Free(sbuf);
	    return PX_FAILURE;
	}
    }

    Free(sbuf);
    sbuf = "\r\n";
    vtrace("HTTP Proxy: xmit ''\n");
    trace_netdata('>', (unsigned char *)sbuf, strlen(sbuf));

    if (send(fd, sbuf, (int)strlen(sbuf), 0) < 0) {
	popup_a_sockerr("HTTP Proxy: send error");
	return PX_FAILURE;
    }

    return PX_WANTMORE;
}

/* HTTP proxy continuation. */
proxy_negotiate_ret_t
proxy_http_continue(void)
{
    char *space;
    bool nl = false;

    /*
     * Process the reply.
     * Read a byte at a time until two \n or EOF.
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
	    popup_a_sockerr("HTTP Proxy: receive error");
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    return PX_FAILURE;
	}
	if (nr == 0) {
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    popup_an_error("HTTP Proxy: unexpected EOF");
	    return PX_FAILURE;
	}
	if (++ps.nread >= RBUF) {
	    ps.nread = RBUF - 1;
	    break;
	}
	if (ps.nread && ps.rbuf[ps.nread - 1] == '\n') {
	    if (nl) {
		break;
	    }
	    nl = true;
	}
    }

    trace_netdata('<', ps.rbuf, ps.nread);
    if (ps.rbuf[ps.nread - 1] == '\n') {
	--ps.nread;
    }
    if (ps.rbuf[ps.nread - 1] == '\r') {
	--ps.nread;
    }
    if (ps.rbuf[ps.nread - 1] == '\n') {
	--ps.nread;
    }
    if (ps.rbuf[ps.nread - 1] == '\r') {
	--ps.nread;
    }
    ps.rbuf[ps.nread] = '\0';
    vtrace("HTTP Proxy: recv '%s'\n", (char *)ps.rbuf);

    if (strncmp((char *)ps.rbuf, "HTTP/", 5) ||
	    (space = strchr((char *)ps.rbuf, ' ')) == NULL) {
	popup_an_error("HTTP Proxy: unrecognized reply");
	return PX_FAILURE;
    }
    if (*(space + 1) != '2') {
	popup_an_error("HTTP Proxy: CONNECT failed:\n%s",
		(char *)ps.rbuf);
	return PX_FAILURE;
    }

    return PX_SUCCESS;
}

/*
 * Close the HTTP proxy.
 */
void
proxy_http_close(void)
{
    ps.fd = INVALID_SOCKET;
    Replace(ps.rbuf, NULL);
    ps.nread = 0;
}
