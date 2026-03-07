/*
 * Copyright (c) 2007-2026 Paul Mattes.
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
 *	nhp.c
 *		The numeric_host_and_port() function.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <netdb.h>
# else /*][*/
# include "w3misc.h"
#endif /*]*/

#include "gai_strerror.h"
#include "txa.h"

#include "nhp.h"

#if defined(_WIN32) /*[*/
# define LEN DWORD
#else /*][*/
# define LEN size_t
#endif /*]*/

/*
 * Resolve a sockaddr into a numeric hostname and port.
 * Returns True for success, false for failure.
 */
bool
numeric_host_and_port(const struct sockaddr *sa, socklen_t salen, char *host,
	size_t hostlen, char *serv, size_t servlen, const char **errmsg)
{
    int rc;

    /* Use getnameinfo(). */
    rc = getnameinfo(sa, salen, host, (LEN)hostlen, serv, (LEN)servlen,
	    NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0) {
	if (errmsg) {
	    *errmsg = txAsprintf("%s", my_gai_strerror(rc));
	}
	return false;
    }
    return true;
}
