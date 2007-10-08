/*
 * Copyright 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, wc3270, s3270, tcl3270, pr3287 and wpr3287 are distributed in
 * the hope that they will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the file LICENSE for more details.
 */

/*
 *	resolver.c
 *		Hostname resolution.
 */

/*
 * This file is compiled three different ways:
 *
 * - With no special #defines, it defines hostname resolution for the main
 *   program: resolve_host_and_port().  On non-Windows platforms, the name
 *   look-up is directly in the function.  On Windows platforms, the name
 *   look-up is done by the function dresolve_host_and_port() in an
 *   OS-specific DLL.
 *
 * - With W3N4 #defined, it defines dresolve_host_and_port() as IPv4-only
 *   hostname resolution for a Windows DLL.  This is for Windows 2000 or
 *   earlier.
 *
 * - With W3N46 #defined, it defines dresolve_host_and_port() as IPv4/IPv6
 *   hostname resolution for a Windows DLL.  This is for Windows XP or
 *   later.
 */

#include "globals.h"

#if defined(W3N4) || defined(W3N46) /*[*/
#if !defined(_WIN32)
#error W3N4/W3N46 valid only on Windows.
#endif /*]*/
#define ISDLL 1
#endif /*]*/

#if !defined(_WIN32) /*[*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else /*][*/
#if defined(W3N46) /*[*/
  /* Compiling DLL for WinXP or later: Expose getaddrinfo()/freeaddrinfo(). */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif /*]*/
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(W3N4) /*[*/
  /* Compiling DLL for Win2K or earlier: No IPv6. */
#undef AF_INET6
#endif /*]*/
#endif /*]*/

#include <stdio.h>
#include "resolverc.h"
#include "w3miscc.h"

#if defined(_WIN32) && !defined(ISDLL) /*[*/
typedef int rhproc(const char *, char *, unsigned short *, struct sockaddr *,
	socklen_t *, char *, int);
#define DLL_RESOLVER_NAME "dresolve_host_and_port"
#endif /*]*/

/*
 * Resolve a hostname and port.
 * Returns 0 for success, -1 for fatal error (name resolution impossible),
 *  -2 for simple error (cannot resolve the name).
 */
#if !defined(ISDLL) /*[*/
int
resolve_host_and_port(const char *host, char *portname, unsigned short *pport,
	struct sockaddr *sa, socklen_t *sa_len, char *errmsg, int em_len)
#else /*][*/
int
dresolve_host_and_port(const char *host, char *portname, unsigned short *pport,
	struct sockaddr *sa, socklen_t *sa_len, char *errmsg, int em_len)
#endif /*]*/
{
#if !defined(_WIN32) || defined(ISDLL) /*[*/

    	/* Non-Windows version, or Windows DLL version. */
#if defined(AF_INET6) /*[*/
	struct addrinfo	 hints, *res;
	int		 rc;

	/* Use getaddrinfo() to resolve the hostname and port together. */
	(void) memset(&hints, '\0', sizeof(struct addrinfo));
	hints.ai_flags = 0;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	rc = getaddrinfo(host, portname, &hints, &res);
	if (rc != 0) {
		snprintf(errmsg, em_len, "%s/%s: %s", host, portname,
				gai_strerror(rc));
		return -2;
	}
	switch (res->ai_family) {
	case AF_INET:
		*pport =
		    ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port);
		break;
	case AF_INET6:
		*pport =
		    ntohs(((struct sockaddr_in6 *)res->ai_addr)->sin6_port);
		break;
	default:
		snprintf(errmsg, em_len, "%s: unknown family %d", host,
			res->ai_family);
		freeaddrinfo(res);
		return -1;
	}
	(void) memcpy(sa, res->ai_addr, res->ai_addrlen);
	*sa_len = res->ai_addrlen;
	freeaddrinfo(res);

#else /*][*/

	struct hostent	*hp;
	struct servent	*sp;
	unsigned short	 port;
	unsigned long	 lport;
	char		*ptr;
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;

	/* Get the port number. */
	lport = strtoul(portname, &ptr, 0);
	if (ptr == portname || *ptr != '\0' || lport == 0L || lport & ~0xffff) {
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
			snprintf(errmsg, em_len, "Unknown host:\n%s", host);
			return -2;
		}
	} else {
		sin->sin_family = hp->h_addrtype;
		(void) memmove(&sin->sin_addr, hp->h_addr, hp->h_length);
	}
	sin->sin_port = port;
	*sa_len = sizeof(struct sockaddr_in);

#endif /*]*/
#else /*][*/

	/* Win32 version: Use the right DLL. */
	static int loaded = FALSE;
	static FARPROC p = NULL;

	if (!loaded) {
		OSVERSIONINFO info;
		HMODULE handle;
		char *dllname;

		/* Figure out if we are pre- or post XP. */
		memset(&info, '\0', sizeof(info));
		info.dwOSVersionInfoSize = sizeof(info);

		if (GetVersionEx(&info) == 0) {
			snprintf(errmsg, em_len,
				"Can't retrieve OS version: %s",
				win32_strerror(GetLastError()));
			return -1;
		}

		/*
		 * For pre-XP, load the IPv4-only DLL.
		 * For XP and later, use the IPv4/IPv6 DLL.
		 */
		if (info.dwMajorVersion < 5 ||
		    (info.dwMajorVersion == 5 && info.dwMinorVersion < 1))
		    	dllname = "w3n4.dll";
		else
		    	dllname = "w3n46.dll";
		handle = LoadLibrary(dllname);
		if (handle == NULL) {
			snprintf(errmsg, em_len, "Can't load %s: %s",
				dllname, win32_strerror(GetLastError()));
			return -1;
		}

		/* Look up the entry point we need. */
		p = GetProcAddress(handle, DLL_RESOLVER_NAME);
		if (p == NULL) {
			snprintf(errmsg, em_len,
				"Can't resolve " DLL_RESOLVER_NAME
				" in %s: %s", dllname,
				win32_strerror(GetLastError()));
		    	return -1;
		}

		loaded = TRUE;
	}

	/* Use the DLL function to resolve the hostname and port. */
	return ((rhproc *)p)(host, portname, pport, sa, sa_len, errmsg, em_len);
#endif /*]*/

	return 0;
}
