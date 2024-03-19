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
 *	proxy.h
 *		Declarations for proxy.c.
 */

/*
 * Supported proxy types.
 */
typedef enum {
    PT_NONE,
    PT_PASSTHRU,/* Sun telnet-passthru */
    PT_HTTP,    /* RFC 2817 CONNECT tunnel */
    PT_TELNET,  /* 'connect host port' proxy */
    PT_SOCKS4,  /* SOCKS version 4 (or 4A if necessary) */
    PT_SOCKS4A, /* SOCKS version 4A (force remote name resolution) */
    PT_SOCKS5,  /* SOCKS version 5 (RFC 1928) */
    PT_SOCKS5D, /* SOCKS version 5D (force remote name resolution) */
    PT_MAX,
    PT_ERROR = -1,
    PT_FIRST = PT_PASSTHRU
} proxytype_t;

/*
 * Proxy negotiate return codes.
 */
typedef enum {
    PX_SUCCESS,		/* success */
    PX_FAILURE,		/* failure */
    PX_WANTMORE		/* more input needed */
} proxy_negotiate_ret_t;

typedef void *proxy_t;
int proxy_setup(const char *spec, char **puser, char **phost, char **pport);
proxy_negotiate_ret_t proxy_negotiate(socket_t fd, const char *user,
	const char *host, unsigned short port, bool blocking);
proxy_negotiate_ret_t proxy_continue(void);
void proxy_close(void);
const char *proxy_type_name(proxytype_t type);
bool proxy_takes_username(proxytype_t type);
int proxy_default_port(proxytype_t type);
