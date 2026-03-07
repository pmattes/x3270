/*
 * Copyright (c) 2026 Paul Mattes.
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

/* Dummy resolver_pipe. */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <netinet/in.h>
#endif /*]*/
#include "sockaddr_46.h"
#include "resolver_pipe.h"

/* Set up a resolver session. */
rp_t
rp_alloc(void *context, rp_complete_t *complete)
{
    return NULL;
}

/* Free a resolver session. */
void
rp_free(rp_t *rpp)
{
}

/* Resolve a hostname and port. */
rp_result_t
rp_resolve(rp_t rp, const char *host, const char *portname, int pf, const char **errmsg)
{
    return RP_FAIL;
}

/* Cancel a pending resolution. */
void
rp_cancel(rp_t rp)
{
}

/* Interrogate a completed resolution. */
int
rp_num_ha(rp_t rp)
{
    return 0;
}

unsigned short
rp_port(rp_t rp)
{
    return 0;
}

sockaddr_46_t *
rp_haddr(rp_t rp, int ix)
{
    return NULL;
}

socklen_t
rp_ha_len(rp_t rp, int ix)
{
    return 0;
}
