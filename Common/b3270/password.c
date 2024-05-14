/*
 * Copyright (c) 2017-2024 Paul Mattes.
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
 *	b_password.c
 *		TLS password for b3270.
 */

#include "globals.h"

#include <errno.h>

#include "actions.h"
#include "b_password.h"
#include "host.h"
#include "task.h"
#include "telnet.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"

/* Macros. */
#define PASSWORD_PASSTHRU_NAME	"TlsKeyPassword"
#define PASSWORD_PASSTHRU_CALL	PASSWORD_PASSTHRU_NAME "()"

/* Globals. */

/* Statics. */

static void password_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool password_done(task_cbh handle, bool success, bool abort);

/* Callback block for actions. */
static tcb_t password_cb = {
    "password",
    IA_PASSWORD,
    CB_NEW_TASKQ,
    password_data,
    password_done,
    NULL
};

static char *password_result = NULL;

/**
 * Callback for data returned to password.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
password_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    if (handle != (tcb_t *)&password_cb) {
	vtrace("password_data: no match\n");
	return;
    }

    Replace(password_result, Asprintf("%.*s", (int)len, buf));
}

/*
 * Timeout (asynchrous call) for password error.
 */
static void
password_error(ioid_t ioid)
{
    connect_error("%s",
	    password_result? password_result: "Password failed");
    Replace(password_result, NULL);
}

/**
 * Callback for completion of password pass-through command.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if context is complete
 */
static bool
password_done(task_cbh handle, bool success, bool abort)
{
    if (handle != (tcb_t *)&password_cb) {
	vtrace("password_data: no match\n");
	return true;
    }

    if (success) {
	net_password_continue(password_result);
    } else {
	vtrace("Password command failed%s%s\n",
		password_result? ": ": "",
		password_result? password_result: "");
	AddTimeOut(1, password_error);
    }
    return true;
}

/**
 * Push a password command.
 * @return true if pass-through queued, false otherwise.
 */
bool
push_password(bool again)
{
    action_elt_t *e;
    bool found = false;
    char *cmd;

    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (!strcasecmp(e->t.name, PASSWORD_PASSTHRU_NAME)) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    if (!found) {
	return false;
    }

    /* No result yet. */
    Replace(password_result, NULL);

    /* Push a callback with a macro. */
    cmd = txAsprintf("%s(%s)", PASSWORD_PASSTHRU_NAME, again? "again": "");
    push_cb(cmd, strlen(cmd), &password_cb, (task_cbh)&password_cb);
    return true;
}

/**
 * Return restrictions based on a passthru command name.
 * @returns IA_NONE or IA_PASSWORD.
 */
ia_t
password_ia_restrict(const char *action)
{
    return strcasecmp(action, PASSWORD_PASSTHRU_NAME)? IA_NONE: IA_PASSWORD;
}
