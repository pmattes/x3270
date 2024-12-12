/*
 * Copyright (c) 2021-2024 Paul Mattes.
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
 *      popup_an_error.c
 *              Common support for error popups.
 */

#include "globals.h"

#include "glue_gui.h"
#include "host.h"
#include "popups.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "varbuf.h"

/**
 * Pop up an error message with a strerror appended.
 *
 * @param[in] errn	Error number
 * @param[in] fmt	Format
 */
void
popup_an_errno(int errn, const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = Vasprintf(fmt, ap);
    va_end(ap);
    if (errn > 0) {
	popup_an_xerror(ET_OTHER, "%s:%s%s", s, popup_separator,
		strerror(errn));
    } else {
	popup_an_xerror(ET_OTHER, "%s", s);
    }
    Free(s);
}

/**
 * Pop up a particular flavor of error message.
 *
 * @param[in] type	Error type
 * @param[in] fmt	Format
 */
void
popup_an_xerror(pae_t type, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    popup_a_vxerror(type, fmt, ap);
    va_end(ap);
}

/**
 * Pop up an error message.
 *
 * @param[in] fmt	Format
 */
void
popup_an_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    popup_a_vxerror(ET_OTHER, fmt, ap);
    va_end(ap);
}

/**
 * Pop up an error message, varags style.
 */
void
popup_a_vxerror(pae_t type, const char *fmt, va_list ap)
{
    char *s = Vasprintf(fmt, ap);

    trace_error(type, s);

    if (task_redirect()) {
        if (type == ET_CONNECT) {
            char *t = Asprintf("Connection failed%s:\n%s", host_retry_mode? ", retrying": "", s);

            task_error(t);
            Free(t);
        } else {
            task_error(s);
        }
    } else if (!glue_gui_error(type, s)) {
	fprintf(stderr, "%s%s%s%s\n",
		(type == ET_CONNECT)? "Connection failed": "",
		(type == ET_CONNECT && host_retry_mode)? ", retrying": "",
		(type == ET_CONNECT)? ":\n": "",
		s);
    }
    Free(s);
}

/**
 * Trace an error.
 *
 * @param[in] type	Error type
 * @param[in] message	Error message
 */
void
trace_error(pae_t type, const char *message)
{
    varbuf_t r;
    char c;
    char *res;

    vb_init(&r);
    while ((c = *message++) != '\0') {
	if (c == '\n') {
	    vb_appends(&r, "\\n");
	} else {
	    vb_appendf(&r, "%c", c);
	}
    }

    res = vb_consume(&r);
    vtrace("Error: %s%s\n", (type == ET_CONNECT)? "Connection failed:\\n": "", res);
    Free(res);
}
