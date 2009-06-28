/*
 * Copyright (c) 2007-2009, Paul Mattes.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else /*][*/
/* Expose IPv6 structures and calls. */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#endif /*]*/

#include <stdio.h>
#include "resolverc.h"
#include "w3miscc.h"

#if defined(_MSC_VER) /*[*/
#include "Msc/deprecated.h"
#endif /*]*/

#if defined(_WIN32) /*[*/
static int win32_getaddrinfo(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res);
static void win32_freeaddrinfo(struct addrinfo *res);
#define getaddrinfo	win32_getaddrinfo
#define freeaddrinfo	win32_freeaddrinfo
#endif /*]*/

/*
 * Resolve a hostname and port.
 * Returns 0 for success, -1 for fatal error (name resolution impossible),
 *  -2 for simple error (cannot resolve the name).
 */
int
resolve_host_and_port(const char *host, char *portname, int ix _is_unused,
	unsigned short *pport, struct sockaddr *sa, socklen_t *sa_len,
	char *errmsg, int em_len, int *lastp)
{
#if defined(_WIN32) /*[*/
    	/* Figure out if we should use gethostbyname() or getaddrinfo(). */
    	OSVERSIONINFO info;
	Boolean has_getaddrinfo = False;

	memset(&info, '\0', sizeof(info));
	info.dwOSVersionInfoSize = sizeof(info);
	if (GetVersionEx(&info) == 0) {
	    	fprintf(stderr, "Can't get Windows version\n");
		exit(1);
	}
	has_getaddrinfo =
	    (info.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS &&
	     info.dwMajorVersion >= 5 &&
	     info.dwMinorVersion >= 1);

	if (has_getaddrinfo)
#endif /*]*/
	{
#if defined(AF_INET6) /*[*/
		struct addrinfo	 hints, *res0, *res;
		int		 rc;

		/* Use getaddrinfo() to resolve the hostname and port
		 * together.
		 */
		(void) memset(&hints, '\0', sizeof(struct addrinfo));
		hints.ai_flags = 0;
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		rc = getaddrinfo(host, portname, &hints, &res0);
		if (rc != 0) {
			snprintf(errmsg, em_len, "%s/%s:\n%s", host,
					portname? portname: "(none)",
					gai_strerror(rc));
			return -2;
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
			snprintf(errmsg, em_len, "%s/%s:\n%s", host,
				portname? portname: "(none)",
				gai_strerror(EAI_AGAIN));
			freeaddrinfo(res);
			return -2;
		}

		switch (res->ai_family) {
		case AF_INET:
			*pport = ntohs(((struct sockaddr_in *)
				    res->ai_addr)->sin_port);
			break;
		case AF_INET6:
			*pport = ntohs(((struct sockaddr_in6 *)
				    res->ai_addr)->sin6_port);
			break;
		default:
			snprintf(errmsg, em_len, "%s:\nunknown family %d", host,
				res->ai_family);
			freeaddrinfo(res);
			return -1;
		}
		(void) memcpy(sa, res->ai_addr, res->ai_addrlen);
		*sa_len = res->ai_addrlen;
		if (lastp != NULL)
			*lastp = (res->ai_next == NULL);
		freeaddrinfo(res0);
	}
#endif /*]*/
#if defined(_WIN32) /*[*/
	else
#endif /*]*/

#if defined(_WIN32) || !defined(AF_INET6) /*[*/
	{
		struct hostent	*hp;
		struct servent	*sp;
		unsigned short	 port;
		unsigned long	 lport;
		char		*ptr;
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;

		/* Get the port number. */
		lport = strtoul(portname, &ptr, 0);
		if (ptr == portname || *ptr != '\0' || lport == 0L ||
			    lport & ~0xffff) {
			if (!(sp = getservbyname(portname, "tcp"))) {
				snprintf(errmsg, em_len,
				    "Unknown port number or service: %s",
				    portname);
				return -1;
			}
			port = sp->s_port;
		} else
			port = htons((unsigned short)lport);
		*pport = ntohs(port);

		/* Use gethostbyname() to resolve the hostname. */
		hp = gethostbyname(host);
		if (hp == (struct hostent *) 0) {
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = inet_addr(host);
			if (sin->sin_addr.s_addr == (unsigned long)-1) {
				snprintf(errmsg, em_len,
					"Unknown host:\n%s", host);
				return -2;
			}
		} else {
		    	/* Todo: index info h_addr_list. */
			sin->sin_family = hp->h_addrtype;
			(void) memmove(&sin->sin_addr, hp->h_addr,
				hp->h_length);
		}
		sin->sin_port = port;
		*sa_len = sizeof(struct sockaddr_in);
		if (lastp != NULL)
			*lastp = TRUE;
	}

#endif /*]*/

	return 0;
}

#if defined(_WIN32) /*[*/
/*
 * Windows-specific versions of getaddrinfo(), freeaddrinfo() and
 * gai_strerror().
 * The symbols are resolved from ws2_32.dll at run-time, instead of
 * by linking against ws2_32.lib, because they are not defined on all
 * versions of Windows.
 */
typedef int gai_fn(const char *, const char *, const struct addrinfo *,
	struct addrinfo **);
typedef void fai_fn(struct addrinfo*);

/* Resolve a symbol in ws2_32.dll. */
static FARPROC
get_ws2_32(const char *symbol)
{
	static HMODULE ws2_32_handle = NULL;
    	FARPROC p;

	if (ws2_32_handle == NULL) {
		ws2_32_handle = LoadLibrary("ws2_32.dll");
		if (ws2_32_handle == NULL) {
			fprintf(stderr, "Can't load ws2_32.dll: %s\n",
				win32_strerror(GetLastError()));
			exit(1);
		}
	}
	p = GetProcAddress(ws2_32_handle, symbol);
	if (p == NULL) {
		fprintf(stderr, "Can't resolve %s in ws2_32.dll: %s\n",
			symbol, win32_strerror(GetLastError()));
		exit(1);
	}
	return p;
}

static int
win32_getaddrinfo(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res)
{
	static FARPROC gai_p = NULL;

    	if (gai_p == NULL)
		gai_p = get_ws2_32("getaddrinfo");
	return (*(gai_fn *)gai_p)(node, service, hints, res);
}

static void
win32_freeaddrinfo(struct addrinfo *res)
{
	static FARPROC fai_p = NULL;

    	if (fai_p == NULL)
		fai_p = get_ws2_32("freeaddrinfo");
	(*(fai_fn *)fai_p)(res);
}
#endif /*]*/
