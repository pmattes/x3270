/*
 * Copyright (c) 2017-2018 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
 *	tls_action.c
 *		The Tls() action.
 */

#include "globals.h"

#include <stdarg.h>

#include "appres.h"
#include "resources.h"

#include "actions.h"
#include "opts.h"
#include "popups.h"
#include "utils.h"
#include "sio.h"
#include "sio_internal.h"
#include "telnet.h"

/* Typedefs */

/* Statics */

/* Globals */

/*
 * Tls action.
 */
static bool
Tls_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Tls", ia, argc, argv);
    if (check_argc("Tls", argc, 1, 1) < 0) {
	return false;
    }

    if (!strcasecmp(argv[0], "SessionInfo")) {
	const char *info = net_session_info();

	if (info != NULL) {
	    action_output("%s", net_session_info());
	    return true;
	} else {
	    popup_an_error("No secure connection");
	    return false;
	}
    }

    if (!strcasecmp(argv[0], "CertInfo")) {
	const char *info = net_server_cert_info();
	if (info != NULL) {
	    action_output("%s", net_server_cert_info());
	    return true;
	} else {
	    popup_an_error("No secure connection");
	    return false;
	}
    }

    popup_an_error("Tls: must specify SessionInfo or CertInfo");
    return false;
}

/*
 * Register the Tls() action.
 */
void
sio_register_actions(void)
{
    static action_table_t actions[] = {
	{ "Tls", Tls_action, 0 }
    };

    /* Register our actions. */
    if (sio_supported()) {
	register_actions(actions, array_count(actions));
    }
}
