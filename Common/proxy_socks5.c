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
 *	proxy_socks5.c
 *		SOCKS version 5 proxy.
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
#include "proxy_socks5.h"
#include "resolver.h"
#include "telnet_core.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "w3misc.h"

/* Pending proxy state. */
#define REPLY_LEN	2
enum phase {
    PROCESS_AUTH_REPLY,
    PROCESS_CRED_REPLY,
    PROCESS_CONNECT_REPLY
};
struct {
    socket_t fd;
    bool use_name;
    unsigned short port;
    unsigned char rbuf[REPLY_LEN];
    size_t nread;
    char *host;
    char *user;
    int n2read;
    enum phase phase;
    unsigned char *vrbuf;
    union {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
    } ha;
} ps = { INVALID_SOCKET };

static continue_t proxy_socks5_process_auth_reply;
static continue_t proxy_socks5_process_cred_reply;
static continue_t proxy_socks5_process_connect_reply;

static continue_t *proxy_socks5_continues[] = {
    proxy_socks5_process_auth_reply,
    proxy_socks5_process_cred_reply,
    proxy_socks5_process_connect_reply
};

static proxy_negotiate_ret_t proxy_socks5_send_connect(void);

/* SOCKS version 5 (RFC 1928) proxy. */
proxy_negotiate_ret_t
proxy_socks5(socket_t fd, const char *user, const char *host,
	unsigned short port, bool force_d)
{
    socklen_t ha_len = 0;
    unsigned char sbuf[8];
    unsigned short rport;
    int nw0;

    ps.fd = fd;
    ps.port = port;

    if (force_d) {
	ps.use_name = true;
    } else {
	char *errmsg;
	rhp_t rv;
	int nr;

	/* Resolve the hostname. */
	/* XXX: This is blocking. Crud. */
	rv = resolve_host_and_port(host, NULL, &rport, &ps.ha.sa, sizeof(ps.ha),
		&ha_len, &errmsg, 1, &nr);
	if (rv == RHP_CANNOT_RESOLVE) {
	    ps.use_name = true;
	} else if (RHP_IS_ERROR(rv)) {
	    popup_an_error("SOCKS5 proxy: %s/%u: %s", host, port, errmsg);
	    return PX_FAILURE;
	}
    }
    if (user != NULL) {
	ps.user = NewString(user);
    }
    ps.host = NewString(host);

    /* Send the authentication request to the server. */
    if (user != NULL) {
	memcpy((char *)sbuf, "\005\002\000\002", 4);
	vtrace("SOCKS5 Proxy: xmit version 5 nmethods 2 (no auth, "
		"username/password)\n");
	nw0 = 4;
    } else {
	strcpy((char *)sbuf, "\005\001\000");
	vtrace("SOCKS5 Proxy: xmit version 5 nmethods 1 (no auth)\n");
	nw0 = 3;
    }
    trace_netdata('>', sbuf, nw0);
    if (send(fd, (char *)sbuf, nw0, 0) < 0) {
	popup_a_sockerr("SOCKS5 Proxy: send error");
	return PX_FAILURE;
    }

    ps.nread = 0;
    ps.phase = PROCESS_AUTH_REPLY;
    return PX_WANTMORE;
}

/* Process a SOCKS5 authentication reply. */
static proxy_negotiate_ret_t
proxy_socks5_process_auth_reply(void)
{
    /*
     * Wait for the server reply.
     * Read 2 bytes of response.
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
	    popup_a_sockerr("SOCKS5 Proxy: receive error");
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    return PX_FAILURE;
	}
	if (nr == 0) {
	    popup_an_error("SOCKS5 Proxy: unexpected EOF");
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    return PX_FAILURE;
	}
	if (++ps.nread >= REPLY_LEN) {
	    break;
	}
    }

    trace_netdata('<', ps.rbuf, ps.nread);

    if (ps.rbuf[0] != 0x05) {
	popup_an_error("SOCKS5 Proxy: bad authentication response");
	return PX_FAILURE;
    }

    vtrace("SOCKS5 Proxy: recv version %d method %d\n", ps.rbuf[0],
	    ps.rbuf[1]);

    if (ps.rbuf[1] == 0xff) {
	popup_an_error("SOCKS5 Proxy: authentication failure");
	return PX_FAILURE;
    }

    if (ps.user == NULL && ps.rbuf[1] != 0x00) {
	popup_an_error("SOCKS5 Proxy: bad authentication response");
	return PX_FAILURE;
    }

    if (ps.user != NULL && (ps.rbuf[1] != 0x00 && ps.rbuf[1] != 0x02)) {
	popup_an_error("SOCKS5 Proxy: bad authentication response");
	return PX_FAILURE;
    }

    /* Send the username/password. */
    if (ps.rbuf[1] == 0x02) {
	unsigned char upbuf[1 + 1 + 255 + 1 + 255 + 1];
	char *colon = strchr(ps.user, ':');

	if (colon == NULL ||
		colon == ps.user ||
		*(colon + 1) == '\0' ||
		colon - ps.user > 255 ||
		strlen(colon + 1) > 255) {
	    popup_an_error("SOCKS5 Proxy: invalid username:password");
	    return PX_FAILURE;
	}

	sprintf((char *)upbuf, "\001%c%.*s%c%s",
		(int)(colon - ps.user),	/* ULEN */
		(int)(colon - ps.user),	/* length of user */
		ps.user,		/* user */
		(int)strlen(colon + 1),	/* length of password */
		colon + 1);		/* password */
	vtrace("SOCKS5 Proxy: xmit version 1 ulen %d username '%.*s' plen %d "
		"password '%s'\n",
		(int)(colon - ps.user),
		(int)(colon - ps.user),
		ps.user,
		(int)strlen(colon + 1),
		colon + 1);
	trace_netdata('>', upbuf, strlen((char *)upbuf));
	if (send(ps.fd, (char *)upbuf, (int)strlen((char *)upbuf), 0) < 0) {
	    popup_a_sockerr("SOCKS5 Proxy: send error");
	    return PX_FAILURE;
	}

	ps.nread = 0;
	ps.phase = PROCESS_CRED_REPLY;
	return PX_WANTMORE;
    }

    return proxy_socks5_send_connect();
}

/* Process a reply to our sending the username and password. */
static proxy_negotiate_ret_t
proxy_socks5_process_cred_reply(void)
{
    /* Read the response. */
    for (;;) {
	ssize_t nr = recv(ps.fd, (char *)&ps.rbuf[ps.nread], 1, 0);

	if (nr < 0) {
	    if (socket_errno() == SE_EWOULDBLOCK) {
		if (ps.nread) {
		    trace_netdata('<', ps.rbuf, ps.nread);
		}
		return PX_WANTMORE;
	    }
	    popup_a_sockerr("SOCKS5 Proxy: receive error");
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    return PX_FAILURE;
	}
	if (nr == 0) {
	    popup_an_error("SOCKS5 Proxy: unexpected EOF");
	    if (ps.nread) {
		trace_netdata('<', ps.rbuf, ps.nread);
	    }
	    return PX_FAILURE;
	}
	if (++ps.nread >= REPLY_LEN) {
	    break;
	}
    }

    trace_netdata('<', ps.rbuf, ps.nread);

    if (ps.rbuf[0] != 0x01) {
	popup_an_error("SOCKS5 Proxy: bad username/password "
		"authentication response type, expected 1, got %d",
		ps.rbuf[0]);
	return PX_FAILURE;
    }

    if (ps.rbuf[1] != 0x00) {
	popup_an_error("SOCKS5 Proxy: bad username/password response %d",
		ps.rbuf[1]);
	return PX_FAILURE;
    }

    return proxy_socks5_send_connect();
}

/* Send a connect request to the server. */
static proxy_negotiate_ret_t
proxy_socks5_send_connect(void)
{
    /* Send the request to the server. */
    char nbuf[256];
    char *sbuf = Malloc(32 + strlen(ps.host));
    char *s = sbuf;

    *s++ = 0x05;		/* protocol version 5 */
    *s++ = 0x01;		/* CONNECT */
    *s++ = 0x00;		/* reserved */
    if (ps.use_name) {
	*s++ = 0x03;	/* domain name */
	*s++ = (char)strlen(ps.host);
	strcpy(s, ps.host);
	s += strlen(ps.host);
    } else if (ps.ha.sa.sa_family == AF_INET) {
	*s++ = 0x01;	/* IPv4 */
	memcpy(s, &ps.ha.sin.sin_addr, 4);
	s += 4;
	strcpy(nbuf, inet_ntoa(ps.ha.sin.sin_addr));
    } else {
	*s++ = 0x04;	/* IPv6 */
	memcpy(s, &ps.ha.sin6.sin6_addr, sizeof(struct in6_addr));
	s += sizeof(struct in6_addr);
	inet_ntop(AF_INET6, &ps.ha.sin6.sin6_addr, nbuf, sizeof(nbuf));
    }
    SET16(s, ps.port);

    vtrace("SOCKS5 Proxy: xmit version 5 connect %s %s port %u\n",
	    ps.use_name? "domainname":
		      ((ps.ha.sa.sa_family == AF_INET)? "IPv4": "IPv6"),
	    ps.use_name? ps.host: nbuf,
	    ps.port);
    trace_netdata('>', (unsigned char *)sbuf, s - sbuf);

    if (send(ps.fd, sbuf, (int)(s - sbuf), 0) < 0) {
	popup_a_sockerr("SOCKS5 Proxy: send error");
	Free(sbuf);
	return PX_FAILURE;
    }
    Free(sbuf);

    ps.nread = 0;
    ps.phase = PROCESS_CONNECT_REPLY;
    return PX_WANTMORE;
}

/* Process the reply to the connect request. */
static proxy_negotiate_ret_t
proxy_socks5_process_connect_reply(void)
{
    char nbuf[256];
    bool done = false;
    char *atype_name[] = {
	"",
	"IPv4",
	"",
	"domainname",
	"IPv6"
    };
    unsigned char *portp;
    unsigned short rport;

    /*
     * Process the reply.
     * Only the first two bytes of the response are interesting;
     * skip the rest.
     */
    while (!done) {
	unsigned char r;
	ssize_t nr = recv(ps.fd, (char *)&r, 1, 0);

	if (nr < 0) {
	    if (socket_errno() == SE_EWOULDBLOCK) {
		if (ps.nread) {
		    trace_netdata('<', ps.vrbuf, ps.nread);
		}
		return PX_WANTMORE;
	    }
	    if (ps.nread) {
		trace_netdata('<', ps.vrbuf, ps.nread);
	    }
	    popup_a_sockerr("SOCKS5 Proxy: receive error");
	    return PX_FAILURE;
	}
	if (nr == 0) {
	    if (ps.nread) {
		trace_netdata('<', ps.vrbuf, ps.nread);
	    }
	    popup_an_error("SOCKS5 Proxy: unexpected EOF");
	    return PX_FAILURE;
	}

	ps.vrbuf = Realloc(ps.vrbuf, ps.nread + 1);
	ps.vrbuf[ps.nread] = r;

	switch (ps.nread++) {
	case 0:
	    if (r != 0x05) {
		popup_an_error("SOCKS5 Proxy: incorrect reply version 0x%02x",
			r);
		if (ps.nread) {
		    trace_netdata('<', ps.vrbuf, ps.nread);
		}
		return PX_FAILURE;
	    }
	    break;
	case 1:
	    if (r != 0x00) {
		trace_netdata('<', ps.vrbuf, ps.nread);
	    }
	    switch (r) {
	    case 0x00:
		break;
	    case 0x01:
		popup_an_error("SOCKS5 Proxy: server failure");
		return PX_FAILURE;
	    case 0x02:
		popup_an_error("SOCKS5 Proxy: connection not allowed");
		return PX_FAILURE;
	    case 0x03:
		popup_an_error("SOCKS5 Proxy: network unreachable");
		return PX_FAILURE;
	    case 0x04:
		popup_an_error("SOCKS5 Proxy: host unreachable");
		return PX_FAILURE;
	    case 0x05:
		popup_an_error("SOCKS5 Proxy: connection refused");
		return PX_FAILURE;
	    case 0x06:
		popup_an_error("SOCKS5 Proxy: ttl expired");
		return PX_FAILURE;
	    case 0x07:
		popup_an_error("SOCKS5 Proxy: command not supported");
		return PX_FAILURE;
	    case 0x08:
		popup_an_error("SOCKS5 Proxy: address type not supported");
		return PX_FAILURE;
	    default:
		popup_an_error("SOCKS5 Proxy: unknown server error 0x%02x", r);
		return PX_FAILURE;
	    }
	    break;
	case 2:
	    break;
	case 3:
	    switch (r) {
	    case 0x01:
		ps.n2read = 6;
		break;
	    case 0x03:
		ps.n2read = -1;
		break;
	    case 0x04:
		ps.n2read = sizeof(struct in6_addr) + 2;
		break;
	    default:
		popup_an_error("SOCKS5 Proxy: unknown server address type "
			"0x%02x", r);
		if (ps.nread) {
		    trace_netdata('<', ps.vrbuf, ps.nread);
		}
		return PX_FAILURE;
	    }
	    break;
	default:
	    if (ps.n2read == -1) {
		ps.n2read = r + 2;
	    } else if (!--ps.n2read) {
		done = true;
	    }
	    break;
	}
    }

    trace_netdata('<', ps.vrbuf, ps.nread);
    switch (ps.vrbuf[3]) {
    case 0x01: /* IPv4 */
	memcpy(&ps.ha.sin.sin_addr, &ps.vrbuf[4], 4);
	strcpy(nbuf, inet_ntoa(ps.ha.sin.sin_addr));
	portp = &ps.vrbuf[4 + 4];
	break;
    case 0x03: /* domainname */
	strncpy(nbuf, (char *)&ps.vrbuf[5], ps.vrbuf[4]);
	nbuf[ps.vrbuf[4]] = '\0';
	portp = &ps.vrbuf[5 + ps.vrbuf[4]];
	break;
    case 0x04: /* IPv6 */
	memcpy(&ps.ha.sin6.sin6_addr, &ps.vrbuf[4],
		sizeof(struct in6_addr));
	inet_ntop(AF_INET6, &ps.ha.sin6.sin6_addr, nbuf, sizeof(nbuf));
	portp = &ps.vrbuf[4 + sizeof(struct in6_addr)];
	break;
    default:
	/* can't happen */
	nbuf[0] = '\0';
	portp = ps.vrbuf;
	break;
    }
    rport = (*portp << 8) + *(portp + 1);
    vtrace("SOCKS5 Proxy: recv version %d status 0x%02x address %s %s "
	    "port %u\n",
	    ps.vrbuf[0], ps.vrbuf[1],
	    atype_name[ps.vrbuf[3]],
	    nbuf,
	    rport);

    Replace(ps.vrbuf, NULL);
    return PX_SUCCESS;
}

/* SOCKS version 5 continuation. */
proxy_negotiate_ret_t
proxy_socks5_continue(void)
{
    return (*proxy_socks5_continues[ps.phase])();
}

/* SOCKS version 5 cleanup. */
void
proxy_socks5_close(void)
{
    ps.fd = INVALID_SOCKET;
    ps.port = 0;
    ps.nread = 0;
    ps.n2read = 0;
    Replace(ps.host, NULL);
    Replace(ps.user, NULL);
    Replace(ps.vrbuf, NULL);
    ps.phase = 0;
}
