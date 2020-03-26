/*
 * Copyright (c) 2016, 2019-2020 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	popups.c
 *		A GUI back-end for a 3270 Terminal Emulator
 *		Error and info pop-ups.
 */

#include "globals.h"
#include "resources.h"

#include "b3270proto.h"
#include "lazya.h"
#include "popups.h"
#include "task.h"
#include "trace.h"
#include "ui_stream.h"

bool error_popup_visible = false;

/* Pop up an error message, given a va_list. */
void
popup_a_verror(const char *fmt, va_list ap)
{
    char *s;

    s = vlazyaf(fmt, ap);
    vtrace("Error: %s\n", s);
    if (task_redirect()) {
	task_error(s);
    } else {
	ui_vleaf(IndPopup,
		AttrType, "error",
		AttrText, s,
		NULL);
    }
}

/* Pop up an error message. */
void
popup_an_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    popup_a_verror(fmt, ap);
    va_end(ap);
}

/* Pop up an error message, with error decoding. */
void
popup_an_errno(int err, const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = vlazyaf(fmt, ap);
    va_end(ap);
    popup_an_error("%s: %s", s, strerror(err));
}

/* Pop up an info message. */
void
popup_an_info(const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = vlazyaf(fmt, ap);
    va_end(ap);
    if (task_redirect()) {
	task_info("%s", s);
    } else {
	ui_vleaf(IndPopup,
		AttrType, "info",
		AttrText, s,
		NULL);
    }
}

/* Output from an action. */
void
action_output(const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = vlazyaf(fmt, ap);
    va_end(ap);
    if (task_redirect()) {
	task_info("%s", s);
    } else {
	ui_vleaf(IndPopup,
		AttrType, "result",
		AttrText, s,
		NULL);
    }
}

/* Output from the printer process. */
void
popup_printer_output(bool is_err, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = vlazyaf(fmt, ap);
    va_end(ap);
    ui_vleaf(IndPopup,
	    AttrType, "printer",
	    AttrError, ValTrueFalse(is_err),
	    AttrText, s,
	    NULL);
}

/* Output from a child process. */
void
popup_child_output(bool is_err, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = vlazyaf(fmt, ap);
    va_end(ap);
    ui_vleaf(IndPopup,
	    AttrType, "child",
	    AttrError, ValTrueFalse(is_err),
	    AttrText, s,
	    NULL);
}

void
child_popup_init(void)
{
}
