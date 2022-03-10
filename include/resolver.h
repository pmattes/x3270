/*
 * Copyright (c) 2007-2022 Paul Mattes.
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
 *	resolver.h
 *		Hostname resolution.
 */

typedef enum {
    RHP_SUCCESS = 0,	/* success (synchronous) */
    RHP_PENDING = 1,	/* success (asynchronos pending) */
    RHP_FATAL = -1,	/* impossible to resolve */
    RHP_CANNOT_RESOLVE = -2	/* cannot resolve */
} rhp_t;
#define RHP_IS_ERROR(r)	((r) < 0)
rhp_t resolve_host_and_port(const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr);
rhp_t resolve_host_and_port_abs(const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr);
rhp_t resolve_host_and_port_a(const char *host, char *portname,
	unsigned short *pport, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, char **errmsg, int max, int *nr, int *slot,
	int pipe, iosrc_t event);
rhp_t collect_host_and_port(int slot, struct sockaddr *sa, size_t sa_len,
	socklen_t *sa_rlen, unsigned short *pport, char **errmsg, int max,
	int *nr);
void cleanup_host_and_port(int slot);

bool numeric_host_and_port(const struct sockaddr *sa, socklen_t salen,
	char *host, size_t hostlen, char *serv, size_t servlen, char **errmsg);

void set_46(bool prefer4, bool prefer6);
