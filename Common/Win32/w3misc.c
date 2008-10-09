/*
 * Copyright 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * wc3270 and wpr3287 are distributed in the hope that they will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	w3misc.c
 *		Miscellaneous Win32 functions.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
#error This module is only for Win32.
#endif /*]*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <errno.h>

#include "w3miscc.h"

/* Convert a network address to a string. */
const char *
inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
    	union {
	    	struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} sa;
	DWORD ssz;
	DWORD sz = cnt;

	memset(&sa, '\0', sizeof(sa));

	switch (af) {
	case AF_INET:
	    	sa.sin = *(struct sockaddr_in *)src;	/* struct copy */
		ssz = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
	    	sa.sin6 = *(struct sockaddr_in6 *)src;	/* struct copy */
		ssz = sizeof(struct sockaddr_in6);
		break;
	default:
	    	if (cnt > 0)
			dst[0] = '\0';
		return NULL;
	}

	sa.sa.sa_family = af;

	if (WSAAddressToString(&sa.sa, ssz, NULL, dst, &sz) != 0) {
	    	if (cnt > 0)
			dst[0] = '\0';
		return NULL;
	}

	return dst;
}

/* Decode a Win32 error number. */
const char *
win32_strerror(int e)
{
	static char buffer[4096];

	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
	    NULL,
	    e,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	    buffer,
	    sizeof(buffer),
	    NULL) == 0) {
	    
	    sprintf(buffer, "Windows error %d", e);
	}

	return buffer;
}

#if defined(_MSC_VER) /*[*/

#include <windows.h>
#define SECS_BETWEEN_EPOCHS	11644473600ULL
#define SECS_TO_100NS		10000000ULL /* 10^7 */

int
gettimeofday(struct timeval *tv, void *ignored)
{
	FILETIME t;
	ULARGE_INTEGER u;

	GetSystemTimeAsFileTime(&t);
	memcpy(&u, &t, sizeof(ULARGE_INTEGER));

	/* Isolate seconds and move epochs. */
	tv->tv_sec = (DWORD)((u.QuadPart / SECS_TO_100NS) -
			       	SECS_BETWEEN_EPOCHS);
	tv->tv_usec = (u.QuadPart % SECS_TO_100NS) / 10ULL;
	return 0;
}

#endif /*]*/
