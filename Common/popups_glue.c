/*
 * Copyright (c) 1993-2016, 2018 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	popups_glue.c
 *		Pop-up messages.
 */

#include "globals.h"

#include "glue.h"
#include "glue_gui.h"
#include "popups.h" /* must come before child_popups.h */
#include "child_popups.h"
#include "screen.h"
#include "task.h"
#include "trace.h"
#include "utils.h"

/* Pop up an error dialog. */
void
popup_an_error(const char *fmt, ...)
{
    va_list args;
    char *s;

    va_start(args, fmt);
    s = xs_vbuffer(fmt, args);
    va_end(args);

    /* Log to the trace file. */
    vtrace("error: %s\n", s);

    if (task_redirect()) {
	task_error(s);
    } else if (!glue_gui_error(s)) {
	(void) fprintf(stderr, "%s\n", s);
	fflush(stderr);
    }
    Free(s);
}

/* Pop up an error dialog, based on an error number. */
void
popup_an_errno(int errn, const char *fmt, ...)
{
    va_list args;
    char *s;

    va_start(args, fmt);
    s = xs_vbuffer(fmt, args);
    va_end(args);

    if (errn > 0) {
	popup_an_error("%s: %s", s, strerror(errn));
    } else {
	popup_an_error("%s", s);
    }
    Free(s);
}

void
action_output(const char *fmt, ...)
{
    va_list args;
    char *s;

    va_start(args, fmt);
    s = xs_vbuffer(fmt, args);
    va_end(args);
    if (task_redirect()) {
	task_info("%s", s);
    } else {
	if (!glue_gui_output(s)) {
	    (void) fprintf(stderr, "%s\n", s);
	    fflush(stderr);
	}
    }
    Free(s);
}

void
popup_printer_output(bool is_err _is_unused, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list args;
    char *m;

    va_start(args, fmt);
    m = xs_vbuffer(fmt, args);
    va_end(args);
    popup_an_error("Printer session: %s", m);
    Free(m);
}

void
popup_child_output(bool is_err _is_unused, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list args;
    char *m;

    va_start(args, fmt);
    m = xs_vbuffer(fmt, args);
    va_end(args);
    action_output("%s", m);
    Free(m);
}

void
child_popup_init(void)
{
}
