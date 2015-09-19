/*
 * Copyright (c) 2007-2009, 2013, 2015 Paul Mattes.
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
 *	w3misc.c
 *		Miscellaneous Win32 functions.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
#error This module is only for Win32.
#endif /*]*/

#include <stdio.h>
#include <errno.h>

#include "asprintf.h"
#include "w3misc.h"

/* Initialize Winsock. */
int
sockstart(void)
{
    static int initted = 0;
    WORD wVersionRequested;
    WSADATA wsaData;

    if (initted) {
	return 0;
    }

    initted = 1;

    wVersionRequested = MAKEWORD(2, 2);

    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	fprintf(stderr, "WSAStartup failed: %s\n",
		win32_strerror(GetLastError()));
	return -1;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
	fprintf(stderr, "Bad winsock version: %d.%d\n",
		LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
	return -1;
    }

    return 0;
}

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

    *dst = '\0';
    memset(&sa, '\0', sizeof(sa));

    switch (af) {
    case AF_INET:
	sa.sin.sin_addr = *(struct in_addr *)src;	/* struct copy */
	ssz = sizeof(struct sockaddr_in);
	break;
    case AF_INET6:
	sa.sin6.sin6_addr = *(struct in6_addr *)src;	/* struct copy */
	ssz = sizeof(struct sockaddr_in6);
	break;
    default:
	if (cnt > 0) {
	    dst[0] = '\0';
	}
	return NULL;
    }

    sa.sa.sa_family = af;

    if (WSAAddressToString(&sa.sa, ssz, NULL, dst, &sz) != 0) {
	if (cnt > 0) {
	    dst[0] = '\0';
	}
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

/*
 * Windows version of perror().
 */
void
win32_perror(const char *fmt, ...)
{
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    (void) vasprintf(&buf, fmt, ap);
    va_end(ap);

    fprintf(stderr, "%s: %s\n", buf, win32_strerror(GetLastError()));
    fflush(stderr);
    free(buf);
}

#if defined(_MSC_VER) /*[*/

/* MinGW has gettimofday(), but MSVC does not. */

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

/* MinGW has getopt(), but MSVC does not. */

char *optarg;
int optind = 1, opterr = 1, optopt;
static const char *nextchar = NULL;

int
getopt(int argc, char * const argv[], const char *optstring)
{
    char c;
    const char *s;

    if (optind == 1) {
	nextchar = argv[optind++];
    }

    do {
	if (nextchar == argv[optind - 1]) {
	    if (optind > argc) {
		--optind; /* went too far */
		return -1;
	    }
	    if (nextchar == NULL) {
		--optind; /* went too far */
		return -1;
	    }
	    if (!strcmp(nextchar, "--")) {
		return -1;
	    }
	    if (*nextchar++ != '-') {
		--optind;
		return -1;
	    }
	}

	if ((c = *nextchar++) == '\0') {
	    nextchar = argv[optind++];
	}
    } while (nextchar == argv[optind - 1]);

    s = strchr(optstring, c);
    if (s == NULL) {
	if (opterr) {
	    fprintf(stderr, "Unknown option '%c'\n", c);
	}
	return '?';
    }
    if (*(s + 1) == ':') {
	if (*nextchar) {
	    optarg = (char *)nextchar;
	    nextchar = argv[optind++];
	    return c;
	} else if (optind < argc && argv[optind] != NULL) {
	    optarg = (char *)argv[optind++];
	    nextchar = argv[optind++];
	    return c;
	} else {
	    if (opterr) {
		fprintf(stderr, "Missing value after '%c'\n", c);
	    }
	    return -1;
	}
    } else {
	return c;
    }
}

#endif /*]*/
