/*
 * Copyright (c) 2019-2024 Paul Mattes.
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
 *	proxy_toggle.c
 *		This module implements to proxy setting.
 */

#include "globals.h"

#include "appres.h"
#include "proxy.h"
#include "query.h"
#include "resources.h"
#include "toggles.h"
#include "txa.h"
#include "varbuf.h"

#include "proxy_toggle.h"

/*
 * Proxy query.
 */
static const char *
proxy_dump(void)
{
    varbuf_t r;
    proxytype_t type;

    vb_init(&r);
    for (type = PT_FIRST; type < PT_MAX; type++) {
	int port = proxy_default_port(type);

	vb_appendf(&r, "%s %s%s%s",
		proxy_type_name(type),
		proxy_takes_username(type)? "username": "no-username",
		port? txAsprintf(" %d", port): "",
		(type < PT_MAX - 1)? "\n": "");
    }

    return txdFree(vb_consume(&r));
}

/*
 * Proxy toggle.
 */
static toggle_upcall_ret_t
toggle_proxy(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    char *user, *host, *port;

    if (value == NULL || !*value) {
	Replace(appres.proxy, NULL);
	return TU_SUCCESS;
    }
    if (proxy_setup(value, &user, &host, &port) < 0) {
	return TU_FAILURE;
    }
    Replace(appres.proxy, NewString(value));
    return TU_SUCCESS;
}

/*
 * Proxy registration.
 */
void
proxy_register(void)
{
    static query_t queries[] = {
	{ "Proxies", proxy_dump, NULL, false, true }
    };

    /* Register the toggles. */
    register_extended_toggle(ResProxy, toggle_proxy, NULL, NULL,
	    (void **)&appres.proxy, XRM_STRING);

    /* Register the queries. */
    register_queries(queries, array_count(queries));
}
