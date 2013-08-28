/*
 * Copyright (c) 1993-2009, 2013 Paul Mattes.
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

	/*
	 * Make sure the trace data is flushed, in case this process crashes or
	 * is terminated before it can close the file.
	 *
	 * On Unix, line buffering usually takes care of this, but on Windows,
	 * line buffering doesn't work.
	 */
	fflush(tracef);
}

void
trace_ds(const char *fmt, ...)
{
	va_list args;

	/* allocate buffer */
	if (tdsbuf == CN)
		tdsbuf = Malloc(4096);


	/* print out remainder of message */
	va_start(args, fmt);
	(void) vsnprintf(tdsbuf, 4096, fmt, args);
	va_end(args);

	trace_ds_s(tdsbuf);
}

void
trace_dsn(const char *fmt, ...)
{
	va_list args;
	size_t sl;

	if (tracef == NULL) {
		return;
	}

	/* allocate buffer */
	if (tdsbuf == CN)
		tdsbuf = Malloc(4096);

	/* print out remainder of message */
	va_start(args, fmt);
	(void) vsnprintf(tdsbuf, 4096, fmt, args);
	va_end(args);

	sl = strlen(tdsbuf);
	if (sl > 0) {
		if (tdsbuf[sl - 1] == '\n') {
			tdsbuf[sl - 1] = '\0';
		}
		if (dscnt) {
			fprintf(tracef, "\n");
			dscnt = 0;
		}
		fprintf(tracef, "%s\n", tdsbuf);
		fflush(tracef);
	}
}
