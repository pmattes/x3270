/*
 * Copyright (c) 2007-2024 Paul Mattes.
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
 *	proxy_telnet.c
 *		Simple TELNET proxy.
 */

#include "globals.h"

#include "3270ds.h"
#include "popups.h"
#include "proxy.h"
#include "proxy_private.h"
#include "proxy_telnet.h"
#include "telnet_core.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "w3misc.h"

/* TELNET proxy. */
proxy_negotiate_ret_t
proxy_telnet(socket_t fd, const char *host, unsigned short port)
{
    char *sbuf = Asprintf("connect %s %u\r\n", host, port);

    vtrace("TELNET Proxy: xmit '%.*s'", (int)(strlen(sbuf) - 2), sbuf);
    trace_netdata('>', (unsigned char *)sbuf, strlen(sbuf));

    if (send(fd, sbuf, (int)strlen(sbuf), 0) < 0) {
	popup_a_sockerr("TELNET Proxy: send error");
	Free(sbuf);
	return PX_FAILURE;
    }
    Free(sbuf);

    return PX_SUCCESS;
}
