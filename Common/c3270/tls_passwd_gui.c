/*
 * Copyright (c) 1993-2018, 2020 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC
 *       nor their contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, JEFF SPARKES AND
 * GTRC "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES,
 * DON RUSSELL, JEFF SPARKES OR GTRC BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	tls_passwd_gui.c
 *		TLS certificate password dialog for c3270.
 */

#include "globals.h"

#include "appres.h"
#include "host.h"
#include "popups.h"
#include "task.h"
#include "telnet.h"
#include "tls_passwd_gui.h"

/* Statics. */

/* Proceed with password input. */
static bool
tls_passwd_continue_input(void *handle, const char *text)
{
    /* Send the password back to TLS. */
    net_password_continue(text);
    return true;
}

/* Password input aborted. */
static void
tls_passwd_abort_input(void *handle)
{
    connect_error("Password input aborted");
    host_disconnect(true);
}

/* Password callback. */
tls_passwd_ret_t
tls_passwd_gui_callback(char *buf, int size, bool again)
{
    if (again) {
	action_output("Password is incorrect.");
    } else {
	action_output("TLS certificate private key requires a password.");
    }
    if (task_request_input("Connect", "Enter password: ",
		tls_passwd_continue_input, tls_passwd_abort_input, NULL,
		true)) {
	return SP_PENDING;
    }
    return SP_FAILURE;
}
