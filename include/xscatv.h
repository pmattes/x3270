/*
 * Copyright (c) 2025-2026 Paul Mattes.
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
 *      xscatv.h
 *              Declarations for xscatv.c.
 */

enum xscatv_quote {
    XSCQ_NONE = 0,		/* no quoting */
    XSCQ_QUOTE = 1,		/* unconditional quoting */
    XSCQ_ARGQUOTE = 2,		/* automatic x3270 action quote */
    XSCQ_SHELLQUOTE = 3		/* automatic shell command quote */
};
#define XSCF_DEFAULT	0x0	/* default behavior */
#define XSCF_NLTHRU	0x1	/* leave newlines intact */
char *xscatv(const char *s, size_t len, ssize_t ulimit, enum xscatv_quote qtype, unsigned opts);

#define XSCC_WHITESPACE	0x1
#define XSCC_CONTROLS	0x2
#define XSCC_NBSP	0x4
#define XSCC_DBSPACE	0x8
#define XSCC_ALL	((unsigned)-1)
bool xscatv_safe(const char *s, size_t len, unsigned opts);
