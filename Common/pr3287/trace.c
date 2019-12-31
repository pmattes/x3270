/*
 * Copyright (c) 1993-2009, 2013, 2015, 2019 Paul Mattes.
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
 *	trace.c
 *		3270 data stream tracing.
 *
 */

#include "globals.h"

#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include "3270ds.h"

#include "ctlrc.h"
#include "trace.h"

/* Maximum length of printer-data output. */
#define PD_MAX	77

/* Statics */
typedef enum { TM_BASE, TM_EVENT, TM_DS, TM_PD } tmode_t;
static tmode_t tmode = TM_BASE;

static size_t tscnt = 0;

/* Globals */
FILE *tracef = NULL;

static char *tdsbuf = NULL;
#define TDS_LEN	75

/* Transition from one mode to another. */
static void
clear_tmode(tmode_t desired)
{
    if (tmode == TM_BASE || tmode == desired) {
	return;
    }

    fputc('\n', tracef);
    tscnt = 0;
    tmode = TM_BASE;
}

/* Data Stream trace print, handles line wraps. */
void
trace_ds(const char *fmt, ...)
{
    va_list args;
    size_t len;
    const char *s;
    bool nl = false;

    if (tracef == NULL) {
	return;
    }

    /* Allocate buffer. */
    if (tdsbuf == NULL) {
	tdsbuf = Malloc(4096);
    }

    /* Print out remainder of message. */
    va_start(args, fmt);
    vsnprintf(tdsbuf, 4096, fmt, args);
    va_end(args);

    clear_tmode(TM_DS);

    /*
     * Skip leading newlines, if we're already at the beginning of a
     * line.
     */
    s = tdsbuf;
    if (tmode == TM_BASE) {
	while (*s == '\n') {
	    s++;
	}
    }

    len = strlen(s);
    if (len && s[len-1] == '\n') {
	len--;
	nl = true;
    }
    while (tscnt + len >= 75) {
	size_t plen = 75-tscnt;

	fprintf(tracef, "%.*s ...\n... ", (int)plen, s);
	tscnt = 4;
	s += plen;
	len -= plen;
    }
    if (len) {
	fprintf(tracef, "%.*s", (int)len, s);
	tscnt += len;
    }
    if (nl) {
	fputc('\n', tracef);
	tscnt = 0;
	tmode = TM_BASE;
    }
    fflush(tracef);

    if (tscnt) {
	tmode = TM_DS;
    }
}

/* Trace something that isn't the host or printer data stream. */
static void
vatrace(int do_ts, const char *fmt, va_list args)
{
    size_t sl;
    char *s;

    clear_tmode(TM_EVENT);

    /* Allocate buffer. */
    if (tdsbuf == NULL) {
	tdsbuf = Malloc(4096);
    }

    /* Print out remainder of message. */
    vsnprintf(tdsbuf, 4096, fmt, args);

    s = tdsbuf;

    /*
     * Skip leading newlines, if we're already at the beginning of a
     * line.
     */
    if (tmode == TM_BASE) {
	while (*s == '\n') {
	    s++;
	}
    }

    /* Start with a timestamp. */
    if (tmode == TM_BASE && do_ts) {
	struct timeval tv;
	time_t t;
	struct tm *tm;

	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
	tm = localtime(&t);
	fprintf(tracef, "%d%02d%02d.%02d%02d%02d.%03d ",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec,
		(int)(tv.tv_usec / 1000L));
	fflush(tracef);
    }

    sl = strlen(s);
    if (sl > 0) {
	bool nl = false;

	if (tdsbuf[sl - 1] == '\n') {
	    nl = true;
	}
	fprintf(tracef, "%s", tdsbuf);
	fflush(tracef);
	if (nl) {
	    tscnt = 0;
	    tmode = TM_BASE;
	} else {
	    tscnt += sl;
	    tmode = TM_EVENT;
	}
    }
}

/* Trace something that isn't host or printer data, with a timestamp. */
void
vtrace(const char *fmt, ...)
{
    va_list args;

    if (tracef == NULL) {
	return;
    }

    va_start(args, fmt);
    vatrace(1, fmt, args);
    va_end(args);
}

/* Trace something that isn't host or printer data, without a timestamp. */
void
vtrace_nts(const char *fmt, ...)
{
    va_list args;

    if (tracef == NULL) {
	return;
    }

    va_start(args, fmt);
    vatrace(0, fmt, args);
    va_end(args);
}

/* Trace a byte of data going to the raw print stream. */
void
trace_pdc(unsigned char c)
{
    if (tracef == NULL) {
	return;
    }

    clear_tmode(TM_PD);

    if (!tscnt) {
	tscnt = fprintf(tracef, "<Print> ");
    }
    tscnt += fprintf(tracef, "%02x", c);
    if (tscnt >= PD_MAX) {
	fputc('\n', tracef);
	tscnt = 0;
	tmode = TM_BASE;
    } else {
	tmode = TM_PD;
    }
}

/* Trace a string of data going to the raw print stream. */
void
trace_pds(unsigned char *s)
{
    unsigned char c;

    while ((c = *s++) != '\0') {
	trace_pdc(c);
    }
}

/* Trace a buffer full of data going to the raw print stream. */
void
trace_pdb(unsigned char *s, size_t len)
{
    while (len-- > 0) {
	trace_pdc(*s++);
    }
}
