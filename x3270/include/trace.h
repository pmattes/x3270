/*
 * Copyright (c) 1995-2015 Paul Mattes.
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
 *	trace.h
 *		Global declarations for trace.c.
 */

typedef enum {
    TSS_FILE,	/* trace to file */
    TSS_PRINTER	/* trace to printer */
} tss_t;

extern bool trace_skipping;
extern char *tracefile_name;
extern struct timeval ds_ts;

const char *rcba(int baddr);
char *screentrace_default_file(ptype_t ptype);
char *screentrace_default_printer(void);
void trace_nvt_disc(void);
void trace_char(char c);
void trace_ds(const char *fmt, ...) printflike(1, 2);
void vtrace(const char *fmt, ...) printflike(1, 2);
void ntvtrace(const char *fmt, ...) printflike(1, 2);
tss_t trace_get_screentrace_how(void);
tss_t trace_get_screentrace_last_how(void);
const char *trace_get_screentrace_name(void);
void trace_set_trace_file(const char *path);
void trace_set_screentrace_file(tss_t how, ptype_t ptype, const char *name);
void trace_screen(bool is_clear);
void trace_rollover_check(void);
void tracefile_ok(const char *tfn);
void trace_register(void);
