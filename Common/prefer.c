/*
 * Copyright (c) 2022-2023 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor the names of his contributors may
 *       be used to endorse or promote products derived from this software
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
 *	prefer.c
 *		Extended toggle for the -4/-6 options.
 */

#include "globals.h"

#include "appres.h"
#include "boolstr.h"
#include "popups.h"
#include "prefer.h"
#include "resolver.h"
#include "resources.h"
#include "toggles.h"

/*
 * Toggle the value or prefer_ipv4 or prefer_ipv6.
 */
static toggle_upcall_ret_t
toggle_46(const char *name, const char *value, unsigned flags, ia_t ia)
{
    bool b;
    const char *errmsg = boolstr(value, &b);

    if (errmsg != NULL) {
	popup_an_error("'%s': %s", value, errmsg);
	return TU_FAILURE;
    }
    if (!strcasecmp(name, ResPreferIpv4)) {
	appres.prefer_ipv4 = b;
    } else if (!strcasecmp(name, ResPreferIpv6)) {
	appres.prefer_ipv6 = b;
    } else {
	popup_an_error("Unknown setting '%s'", name);
	return TU_FAILURE;
    }
    set_46(appres.prefer_ipv4, appres.prefer_ipv6);
    return TU_SUCCESS;
}

/*
 * Module registration.
 */
void
prefer_register(void)
{
    register_extended_toggle(ResPreferIpv4, toggle_46, NULL, NULL,
	    (void **)&appres.prefer_ipv4, XRM_BOOLEAN);
    register_extended_toggle(ResPreferIpv6, toggle_46, NULL, NULL,
	    (void **)&appres.prefer_ipv6, XRM_BOOLEAN);
}
