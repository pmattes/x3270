/*
 * Copyright (c) 2007-2009, 2013, 2015, 2019, 2021 Paul Mattes.
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

#include "trace.h"

/* Local multi-byte code page for displaying Windows error messages. */
static int local_cp = CP_ACP;

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

/*
 * Set the local Windows codepage.
 * When running on a full emulator, this is called when the configured output
 * code page is set. For standalone code like x3270if, the codepage remains at
 * the default of CP_ACP.
 */
void
set_local_cp(int cp)
{
    local_cp = cp;
}

/*
 * Converts a Windows WCHAR string to local multi-byte.
 * Returns length (not including trailing NUL) or -1 for failure.
 */
static int
wchar_to_multibyte_string(WCHAR *string, char *mb, size_t mb_len)
{
    int nc;
    BOOL udc;

    nc = WideCharToMultiByte(local_cp, 0, string, -1, mb, (int)mb_len,
	    (local_cp == CP_UTF8)? NULL: "?",
	    (local_cp == CP_UTF8)? NULL: &udc);
    if (nc > 0 && mb[nc - 1] == '\0') {
	--nc;
    }

    return nc;
}

/* Decode a Win32 error number. */
const char *
win32_strerror(int e)
{
#   define SBUF_SIZE 4096
    WCHAR wbuffer[SBUF_SIZE];
    static char buffer[SBUF_SIZE];
    bool success = false;

    if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
	    NULL,
	    e,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	    wbuffer,
	    SBUF_SIZE,
	    NULL)) {
	int nc;

	/* Convert from Windows Unicode to the right output format. */
	nc = wchar_to_multibyte_string(wbuffer, buffer, SBUF_SIZE);
	if (nc > 0) {
	    char c;

	    /* Get rid of trailing CRLF. */
	    while (nc > 0 && ((c = buffer[--nc]) == '\r' || c == '\n')) {
		buffer[nc] = '\0';
	    }
	    success = true;
	}
    }

    if (!success) {
	sprintf(buffer, "Windows error %d", e);
    }

    return buffer;
}

/* Translate a CP_ACP multi-byte string to the selected local codepage. */
const char *
to_localcp(const char *s)
{
    static WCHAR *w_buf = NULL;
    static int w_len = 0;
    static char *mb_buf = NULL;
    static int mb_len = 0;
    int nc;
    BOOL udc;

    if (GetACP() == local_cp) {
	return s;
    }

    /* Allocate the wide character buffer. */
    nc = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
    if (nc == 0) {
	return s;
    }
    if (nc > w_len) {
	w_len = nc;
	Replace(w_buf, (WCHAR *)Malloc(w_len * sizeof(WCHAR)));
    }

    /* Convert the error string to wide characters. */
    nc = MultiByteToWideChar(CP_ACP, 0, s, -1, w_buf, w_len);
    if (nc == 0) {
	return s;
    }

    /* Allocate the multi-byte buffer. */
    nc = WideCharToMultiByte(local_cp, 0, w_buf, -1, NULL, 0,
	    (local_cp == CP_UTF8)? NULL: "?",
            (local_cp == CP_UTF8)? NULL: &udc);
    if (nc == 0) {
	return s;
    }

    /* Convert the wide character string to multi-byte. */
    if (nc > mb_len) {
	mb_len = nc;
	Replace(mb_buf, (char *)Malloc(mb_len));
    }
    nc = WideCharToMultiByte(local_cp, 0, w_buf, -1, mb_buf, mb_len,
            (local_cp == CP_UTF8)? NULL: "?",
            (local_cp == CP_UTF8)? NULL: &udc);
    if (nc == 0) {
	return s;
    }

    return mb_buf;
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
    vasprintf(&buf, fmt, ap);
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
