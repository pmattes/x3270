/*
 * Copyright 1993, 1994, 1995, 1999, 2000, 2001, 2002, 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	trace_ds.c
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
#include "seec.h"
#include "tablesc.h"
#include "trace_dsc.h"

/* Statics */
static int      dscnt = 0;

/* Globals */
FILE           *tracef = (FILE *) 0;

/* Data Stream trace print, handles line wraps */

static char *tdsbuf = CN;
#define TDS_LEN	75

static void
trace_ds_s(char *s)
{
	int len = strlen(s);
	Boolean nl = False;

	if (tracef == NULL)
		return;

	if (s && s[len-1] == '\n') {
		len--;
		nl = True;
	}
	while (dscnt + len >= 75) {
		int plen = 75-dscnt;

		(void) fprintf(tracef, "%.*s ...\n... ", plen, s);
		dscnt = 4;
		s += plen;
		len -= plen;
	}
	if (len) {
		(void) fprintf(tracef, "%.*s", len, s);
		dscnt += len;
	}
	if (nl) {
		(void) fprintf(tracef, "\n");
		dscnt = 0;
	}
}

void
trace_ds(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	/* allocate buffer */
	if (tdsbuf == CN)
		tdsbuf = Malloc(4096);

	/* print out remainder of message */
	(void) vsprintf(tdsbuf, fmt, args);
	trace_ds_s(tdsbuf);
	va_end(args);
}

void
trace_dsn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	/* allocate buffer */
	if (tdsbuf == CN)
		tdsbuf = Malloc(4096);

	/* print out remainder of message */
	(void) vsprintf(tdsbuf, fmt, args);
	strcat(tdsbuf, "\n");
	trace_ds_s(tdsbuf);
	va_end(args);
}
