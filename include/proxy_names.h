/*
 * Copyright (c) 2007-2009, 2015 Paul Mattes.
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
 *	proxy_names.h
 *		Common definitions for proxy.
 */

#define PROXY_PASSTHRU	"passthru"
#define NPORT_PASSTHRU	3514
#define PORT_PASSTHRU	STR(NPORT_PASSTHRU)

#define PROXY_HTTP	"http"
#define NPORT_HTTP	3128
#define PORT_HTTP	STR(NPORT_HTTP)

#define PROXY_TELNET	"telnet"

#define PROXY_SOCKS4	"socks4"
#define NPORT_SOCKS4	1080
#define PORT_SOCKS4	STR(NPORT_SOCKS4)

#define PROXY_SOCKS4A	"socks4a"
#define NPORT_SOCKS4A	NPORT_SOCKS4
#define PORT_SOCKS4A	PORT_SOCKS4

#define PROXY_SOCKS5	"socks5"
#define NPORT_SOCKS5	1080
#define PORT_SOCKS5	STR(NPORT_SOCKS5)

#define PROXY_SOCKS5D	"socks5d"
#define NPORT_SOCKS5D	NPORT_SOCKS5
#define PORT_SOCKS5D	PORT_SOCKS5
