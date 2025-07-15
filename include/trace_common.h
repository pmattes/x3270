/*
 * Copyright (c) 1995-2025 Paul Mattes.
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
 *	trace_common.h
 *		Common global declarations for trace.c.
 */

/* Trace categories. */
typedef enum {
    TC_FT,		/* File transfer */
    TC_HTTPD,		/* HTTP server */
    TC_INFRA,		/* Infrastructure */
    TC_KYBD,		/* Keyboard */
    TC_PRINT,		/* Printing */
    TC_PROXY,		/* Proxy protocols */
    TC_SCHED,		/* Scheduler */
    TC_SCRIPT,		/* Script operations */
    TC_SOCKET,		/* Socket operations */
    TC_TASK,		/* Task operations */
    TC_TELNET,		/* TELNET protocol */
    TC_TLS,		/* TLS protocl */
    TC_TN3270,		/* TN3270 and TN3270E protocols */
    TC_UI,		/* User interface */
    NUM_TC
} tc_t;

extern const char *cats[];

const char *rcba(int baddr);
void trace_ds(const char *fmt, ...) printflike(1, 2);
void vtrace(const char *fmt, ...) printflike(1, 2);
void vctrace(tc_t, const char *fmt, ...) printflike(2, 3);
