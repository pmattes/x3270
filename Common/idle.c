/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *	idle.c
 *		This module handles the idle command.
 */

#include "globals.h"

#include <errno.h>

#include "appres.h"
#if defined(_WIN32) /*[*/
# include "unicodec.h"	/* needed for ft.h */
#endif /*]*/
#include "ft.h"
#include "host.h"
#include "idle.h"
#include "popups.h"
#include "resources.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "wincmn.h"

/* Macros. */
#define MSEC_PER_SEC	1000L
#define IDLE_SEC	1L
#define IDLE_MIN	60L
#define IDLE_HR	(60L * 60L)
#define IDLE_MS	(7L * IDLE_MIN * MSEC_PER_SEC)

/* Globals. */
bool idle_changed = false;
char *idle_command = NULL;
char *idle_timeout_string = NULL;
enum idle_enum idle_user_enabled = IDLE_DISABLED;

/* Statics. */
static bool idle_enabled = false;	/* validated and user-enabled */
static unsigned long idle_n = 0L;
static unsigned long idle_multiplier = IDLE_SEC;
static ioid_t idle_id;
static unsigned long idle_ms;
static bool idle_randomize = false;
static bool idle_ticking = false;

static void idle_in3270(bool in3270);
static void push_idle(char *s);

/**
 * Idle module registration.
 */
void
idle_register(void)
{
    /* Register for state changes. */
    register_schange(ST_3270_MODE, idle_in3270);
    register_schange(ST_CONNECT, idle_in3270);
}

/* Initialization. */
void
idle_init(void)
{
    char *cmd, *tmo;
    char *error;

    /* Get values from resources. */
    cmd = appres.idle_command;
    if (cmd != NULL) {
	while (isspace((int)*cmd)) {
	    cmd++;
	}
	if (!*cmd) {
	    cmd = NULL;
	}
    }
    idle_command = cmd? NewString(cmd): NULL;
    tmo = appres.idle_timeout;
    idle_timeout_string = tmo? NewString(tmo): NULL;
    if (appres.idle_command_enabled) {
	idle_user_enabled = IDLE_PERM;
    } else {
	idle_user_enabled = IDLE_DISABLED;
    }

    /* Validate. */
    if (idle_user_enabled) {
	if (idle_command == NULL) {
	    idle_user_enabled = IDLE_DISABLED;
	} else if (!validate_command(cmd, 0, &error)) {
	    popup_an_error("Invalid %s:\n%s", ResIdleCommand, error);
	    Free(error);
	    idle_user_enabled = IDLE_DISABLED;
	}
	if (!process_idle_timeout_value(idle_timeout_string)) {
	    idle_user_enabled = IDLE_DISABLED;
	}
    }

    /* Seed the random number generator (we seem to be the only user). */
#if defined(_WIN32) /*[*/
    srand((unsigned int)time(NULL));
#else /*][*/
    srandom(time(NULL));
#endif /*]*/
}

/*
 * Process a timeout value: <empty> or ~?[0-9]+[HhMmSs]
 * Returns true for success, false for failure.
 * Sets idle_enabled, idle_ms and idle_randomize as side-effects.
 *
 * Returns true for success, false for failure.
 */
bool
process_idle_timeout_value(const char *t)
{
    const char *s = t;
    char *ptr;

    if (s == NULL || *s == '\0') {
	idle_ms = IDLE_MS;
	idle_randomize = true;
	idle_enabled = true;
	return true;
    }

    if (*s == '~') {
	idle_randomize = true;
	s++;
    }
    idle_n = strtoul(s, &ptr, 0);
    if (idle_n <= 0) {
	goto bad_idle;
    }
    switch (*ptr) {
    case 'H':
    case 'h':
	idle_multiplier = IDLE_HR;
	break;
    case 'M':
    case 'm':
	idle_multiplier = IDLE_MIN;
	break;
    case 'S':
    case 's':
    case '\0':
	idle_multiplier = IDLE_SEC;
	break;
    default:
	goto bad_idle;
    }
    idle_ms = idle_n * idle_multiplier * MSEC_PER_SEC;
    idle_enabled = true;
    return true;

bad_idle:
    popup_an_error("Invalid idle timeout value '%s'", t);
    idle_ms = 0L;
    idle_randomize = false;
    return false;
}

/* Called when a host connects or disconnects. */
static void
idle_in3270(bool in3270 _is_unused)
{
    if (IN_3270) {
	reset_idle_timer();
    } else {
	/* Not in 3270 mode any more, turn off the timeout. */
	if (idle_ticking) {
	    RemoveTimeOut(idle_id);
	    idle_ticking = false;
	}

	/* If the user didn't want it to be permanent, disable it. */
	if (idle_user_enabled != IDLE_PERM) {
	    idle_user_enabled = IDLE_DISABLED;
	}
    }
}

/*
 * Idle timeout.
 */
static void
idle_timeout(ioid_t id _is_unused)
{
    vtrace("Idle timeout\n");
    idle_ticking = false;
    if (ft_state != FT_NONE) {
	/* Should not happen, but just in case. */
	vtrace("File transfer in progress, ignoring\n");
	return;
    } else {
	push_idle(idle_command);
    }
    reset_idle_timer();
}

/*
 * Reset (and re-enable) the idle timer.  Called when the user presses a key or
 * clicks with the mouse.
 */
void
reset_idle_timer(void)
{
    if (idle_enabled) {
	unsigned long idle_ms_now;

	if (idle_ticking) {
	    RemoveTimeOut(idle_id);
	    idle_ticking = false;
	}
	idle_ms_now = idle_ms;
	if (idle_randomize) {
	    idle_ms_now = idle_ms;
	    idle_ms_now -= random() % (idle_ms / 10L);
	}
#if defined(DEBUG_IDLE_TIMEOUT) /*[*/
	vtrace("Setting idle timeout to %lu\n", idle_ms_now);
#endif /*]*/
	idle_id = AddTimeOut(idle_ms_now, idle_timeout);
	idle_ticking = true;
    }
}

/*
 * Cancel the idle timer.  This is called when there is an error in
 * processing the idle command.
 */
void
cancel_idle_timer(void)
{
    if (idle_ticking) {
	RemoveTimeOut(idle_id);
	idle_ticking = false;
    }
    idle_enabled = false;
}

/*
 * Stop the idle timer when a file transfer starts.
 */
void
idle_ft_start(void)
{
    if (idle_ticking)
    {
	RemoveTimeOut(idle_id);
	idle_ticking = false;
    }
}

/*
 * Resume the idle timer when a file transfer completes.
 */
void
idle_ft_complete(void)
{
    if (idle_enabled)
    {
	reset_idle_timer();
    }
}

char *
get_idle_command(void)
{
    return idle_command;
}

char *
get_idle_timeout(void)
{
    return idle_timeout_string;
}

/* Action support. */
static void idle_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool idle_done(task_cbh handle, bool success, bool abort);

/* Callback block for actions. */
static tcb_t idle_cb = {
    "idle",
    IA_IDLE,
    CB_NEW_TASKQ,
    idle_data,
    idle_done,
    NULL
};
static char *idle_result = NULL;

/**
 * Callback for data returned to idle.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
idle_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    if (handle != (tcb_t *)&idle_cb) {
	vtrace("idle_data: no match\n");
	return;
    }

    Replace(idle_result, Asprintf("%.*s", (int)len, buf));
}

/**
 * Callback for completion of one command executed from an idle command .
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if context is complete
 */
static bool
idle_done(task_cbh handle, bool success, bool abort)
{
    if (handle != (tcb_t *)&idle_cb) {
	vtrace("idle_data: no match\n");
	return true;
    }

    if (!success) {
	popup_an_error("Idle command failed%s%s",
		idle_result? ": ": "",
		idle_result? idle_result: "");
	cancel_idle_timer();
    }
    Replace(idle_result, NULL);
    return true;
}

/**
 * Push an idle command.
 *
 * @param[in] s		Text of action.
 */
static void
push_idle(char *s)
{
    /* No result yet. */
    Replace(idle_result, NULL);

    /* Push a callback with a macro. */
    push_cb(s, strlen(s), &idle_cb, (task_cbh)&idle_cb);
}
