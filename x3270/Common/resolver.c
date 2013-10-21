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
#include <arpa/inet.h>
#else /*][*/
#include <winsock2.h>
#include <ws2tcpip.h>
#endif /*]*/

#include <stdio.h>
#include "resolverc.h"
#include "w3miscc.h"

#if defined(_WIN32) /*[*/
static int win32_getaddrinfo(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res);
static void win32_freeaddrinfo(struct addrinfo *res);
static int win32_getnameinfo(const struct sockaddr *sa, socklen_t salen,
	char *host, size_t hostlen, char *serv, size_t servlen, int flags);
#undef getaddrinfo
#define getaddrinfo	win32_getaddrinfo
#undef freeaddrinfo
#define freeaddrinfo	win32_freeaddrinfo
#undef getnameinfo
#define getnameinfo	win32_getnameinfo
#endif /*]*/

/*
 * Resolve a hostname and port.
 * Returns 0 for success, -1 for fatal error (name resolution impossible),
 *  -2 for simple error (cannot resolve the name).
 */
int
resolve_host_and_port(const char *host, char *portname, int ix,
	unsigned short *pport, struct sockaddr *sa, socklen_t *sa_len,
	char *errmsg, int em_len, int *lastp)
{
#if defined(_WIN32) /*[*/
    	OSVERSIONINFO info;
	Boolean has_getaddrinfo = False;

    	/* Figure out if we should use gethostbyname() or getaddrinfo(). */
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

		/*
		 * Use getaddrinfo() to resolve the hostname and port
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
#endif /*]*/
	}
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
			if (lastp != NULL)
			    	*lastp = True;
		} else {
		    	int i;

			for (i = 0; i < ix; i++) {
			    	if (hp->h_addr_list[i] == NULL) {
					snprintf(errmsg, em_len,
						"Unknown host:\n%s", host);
					return -2;
				}
			}
			sin->sin_family = hp->h_addrtype;
			(void) memmove(&sin->sin_addr,
				hp->h_addr_list[i],
				hp->h_length);
			if (lastp != NULL)
			    	*lastp = (hp->h_addr_list[i + 1] == NULL);
		}
		sin->sin_port = port;
		*sa_len = sizeof(struct sockaddr_in);
	}

#endif /*]*/

	return 0;
}

/*
 * Resolve a sockaddr into a numeric hostname and port.
 * Returns 0 for success, -1 for failure.
 */
int
numeric_host_and_port(const struct sockaddr *sa, socklen_t salen, char *host,
	size_t hostlen, char *serv, size_t servlen, char *errmsg, int em_len)
{
#if defined(_WIN32) /*[*/
    	OSVERSIONINFO info;
	Boolean has_getnameinfo = False;

    	/* Figure out if we should use inet_ntoa() or getnameinfo(). */
	memset(&info, '\0', sizeof(info));
	info.dwOSVersionInfoSize = sizeof(info);
	if (GetVersionEx(&info) == 0) {
	    	fprintf(stderr, "Can't get Windows version\n");
		exit(1);
	}
	has_getnameinfo =
	    (info.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS &&
	     info.dwMajorVersion >= 5 &&
	     info.dwMinorVersion >= 1);

	if (has_getnameinfo)
#endif /*]*/
	{
#if defined(AF_INET6) /*[*/
		int	 rc;

		/* Use getnameinfo(). */
		rc = getnameinfo(sa, salen, host, hostlen, serv, servlen,
			NI_NUMERICHOST | NI_NUMERICSERV);
		if (rc != 0) {
			snprintf(errmsg, em_len, "%s", gai_strerror(rc));
			return -1;
		}
#endif /*]*/
	}
#if defined(_WIN32) /*[*/
	else
#endif /*]*/

#if defined(_WIN32) || !defined(AF_INET6) /*[*/
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;

		/* Use inet_ntoa() and snprintf(). */
		snprintf(host, hostlen, "%s", inet_ntoa(sin->sin_addr));
		snprintf(serv, servlen, "%u", ntohs(sin->sin_port));
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
typedef int (__stdcall *gai_fn)(const char *, const char *,
	const struct addrinfo *, struct addrinfo **);
typedef void (__stdcall *fai_fn)(struct addrinfo*);
typedef int (__stdcall *gni_fn)(const struct sockaddr *, socklen_t, char *,
	size_t, char *, size_t, int);

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
	return ((gai_fn)gai_p)(node, service, hints, res);
}

static void
win32_freeaddrinfo(struct addrinfo *res)
{
	static FARPROC fai_p = NULL;

    	if (fai_p == NULL)
		fai_p = get_ws2_32("freeaddrinfo");
	((fai_fn)fai_p)(res);
}

static int
win32_getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
	size_t hostlen, char *serv, size_t servlen, int flags)
{
	static FARPROC gni_p = NULL;

    	if (gni_p == NULL)
		gni_p = get_ws2_32("getnameinfo");
	return ((gni_fn)gni_p)(sa, salen, host, hostlen, serv, servlen, flags);
}
#endif /*]*/
