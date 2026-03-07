/*
 * Copyright (c) 2019-2026 Paul Mattes.
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
 *	resolver_pipe.h
 *		Integration logic for the asynchronous DNS resolver.
 */

/* Session handle type. */
typedef struct _rp *rp_t;

/* Completion callback. */
typedef void rp_complete_t(rp_t rp, void *context, bool success, const char *errmsg);

/* Set up a resolver session. */
rp_t rp_alloc(void *context, rp_complete_t *complete);

/* Free a resolver session. */
void rp_free(rp_t *rpp);

/* Resolve a hostname and port. */
typedef enum {
    RP_FAIL,		/* failed */
    RP_SUCCESS_SYNC,	/* success (synchronous) */
    RP_PENDING		/* asynchronous operation pending */
} rp_result_t;
rp_result_t rp_resolve(rp_t rp, const char *host, const char *portname, int pf, const char **errmsg);

/* Cancel a pending resolution. */
void rp_cancel(rp_t rp);

/* Interrogate a completed resolution. */
#if defined(_SOCKADDR_46) /*[*/
int rp_num_ha(rp_t rp);
unsigned short rp_port(rp_t rp);
sockaddr_46_t *rp_haddr(rp_t rp, int ix);
socklen_t rp_ha_len(rp_t rp, int ix);
#endif /*]*/

void resolver_pipe_register(void);
