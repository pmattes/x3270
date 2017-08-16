/*
 * Copyright (c) 2017 Paul Mattes.
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
 *	ssl_action.c
 *		The Ssl() action.
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
 * Ssl action.
 */
static bool
Ssl_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned options = sio_all_options_supported();
    int i;
    unsigned long l;
    char *s;

    action_debug("Ssl", ia, argc, argv);
    if (check_argc("Ssl", argc, 1, 3) < 0) {
	return false;
    }

    for (i = 0; i < n_sio_flagged_res; i++) {
	res_t *res = &sio_flagged_res[i].res;

	if (!(options & sio_flagged_res[i].flag) ||
		strcasecmp(argv[0], res->name)) {
	    continue;
	}
	if (argc != 2) {
	    popup_an_error("Ssl: missing or extra value after %s", argv[0]);
	    return false;
	}
	switch (res->type) {
	case XRM_STRING:
	    *(char **)res->address = NewString(argv[1]);
	    break;
	case XRM_BOOLEAN:
	    if (!strcasecmp(argv[1], "true")) {
		*(bool *)res->address = true;
	    } else if (!strcasecmp(argv[1], "false")) {
		*(bool *)res->address = false;
	    } else {
		popup_an_error("Ssl: %s requires True or False", argv[0]);
		return false;
	    }
	    break;
	case XRM_INT:
	    l = strtoul(argv[1], &s, 0);
	    if (s == argv[1] || *s != '\0') {
		popup_an_error("Ssl: invalid value for %s\n", argv[0]);
		return false;
	    }
	    *(int *)res->address = (int)l;
	    break;
	default:
	    continue;
	}

	/* Success. */
	return true;
    }

    if (!strcasecmp(argv[0], "SessionInfo")) {
	const char *info;

	if (argc != 1) {
	    popup_an_error("Ssl: extra value after SessionInfo");
	    return false;
	}
	info = net_session_info();
	if (info != NULL) {
	    action_output("%s", net_session_info());
	    return true;
	} else {
	    popup_an_error("No secure connection");
	    return false;
	}
    }

    if (!strcasecmp(argv[0], "CertInfo")) {
	const char *info;

	if (argc != 1) {
	    popup_an_error("Ssl: extra value after CertInfo");
	    return false;
	}
	info = net_server_cert_info();
	if (info != NULL) {
	    action_output("%s", net_server_cert_info());
	    return true;
	} else {
	    popup_an_error("No secure connection");
	    return false;
	}
    }

    action_output("Ssl: must specify one of %s SessionInfo CertInfo",
	    sio_option_names());


    return true;
}

/*
 * Register the Ssl() action.
 */
void
sio_register_actions(void)
{
    static action_table_t actions[] = {
	{ "Ssl", Ssl_action, 0 }
    };

    /* Register our actions. */
    if (sio_supported()) {
	register_actions(actions, array_count(actions));
    }
}
