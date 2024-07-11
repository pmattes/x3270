/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *	split_host.h
 *		Host name parsing.
 */

/* Host flags. */
typedef enum {
    ANSI_HOST,          /* A: */
    NO_LOGIN_HOST,      /* C: */
    TLS_HOST,           /* L: */
    NON_TN3270E_HOST,   /* N: */
    PASSTHRU_HOST,      /* P: */
    STD_DS_HOST,        /* S: */
    BIND_LOCK_HOST,     /* B:, now a no-op */
    NO_VERIFY_CERT_HOST,/* Y: */
    NO_TELNET_HOST	/* T: */
} host_flags_t;
#define HOST_nFLAG(flags, t)    ((flags & (1 << t)) != 0)
#define SET_HOST_nFLAG(flags, t) flags |= 1 << t

extern const char *host_prefixes(void);
bool new_split_host(char *raw, char **lu, char **host, char **port,
	char **accept, unsigned *prefixes, char **error);
