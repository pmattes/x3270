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

/*
 *	mock_resolver_stubs.h
 *		Stubd for the mock hostname resolver.
 */

#include "globals.h"

#include "resolver.h"
#include "mock_resolver.h"

bool
mock_resolver_ready(void)
{
    return false;
}

rhp_t mock_collect_host_and_port(int slot, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, unsigned short *pport, const char **errmsg, int max,
	int *nr)
{
    return RHP_CANNOT_RESOLVE;
}
void
mock_cleanup_host_and_port(int slot)
{
}

rhp_t mock_resolve_host_and_port_async(const char *host, const char *portname, int pf,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len, socklen_t *sa_rlen,
	const char **errmsg, int max, int *nr, int *slot, int pipe, iosrc_t event)
{
    return RHP_CANNOT_RESOLVE;
}
