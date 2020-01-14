/*
 * Copyright (c) 1993-2016, 2018-2020 Paul Mattes.
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
 *      stdinscript.c
 *              Reading script actions from stdin.
 */

#include "globals.h"

#include "wincmn.h"
#include <errno.h>
#include <fcntl.h>

#include "actions.h"
#include "kybd.h"
#include "popups.h"
#include "s3270_proto.h"
#include "source.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"
#include "w3misc.h"
#include "xio.h"

static void stdin_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool stdin_done(task_cbh handle, bool success, bool abort);
static void stdin_closescript(task_cbh handle);

/* Callback block for stdin. */
static tcb_t stdin_cb = {
    "s3stdin",
    IA_SCRIPT,
    CB_NEW_TASKQ,
    stdin_data,
    stdin_done,
    NULL,
    stdin_closescript
};

static ioid_t stdin_id = NULL_IOID;
#if defined(_WIN32) /*[*/
static HANDLE stdin_thread;
static HANDLE stdin_enable_event, stdin_done_event;
static char stdin_buf[256];
int stdin_nr;
int stdin_errno;
#endif /*]*/

static bool pushed_wait = false;
static bool enabled = true;

#if !defined(_WIN32) /*[*/
/**
 * Read the next command from stdin (Unix version).
 * @param[in] fd	File descriptor
 * @param[in] id	I/O identifier
 */
static void
stdin_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    varbuf_t r;
    char *buf;
    size_t len = 0;

    /* Stop input. */
    RemoveInput(stdin_id);
    stdin_id = NULL_IOID;

    vb_init(&r);

    while (true) {
	char c;
	int nr;

	nr = read(fileno(stdin), &c, 1);
	if (nr < 0) {
	    vtrace("s3stdin read error: %s\n", strerror(errno));
	    vb_free(&r);
	    x3270_exit(1);
	}
	if (nr == 0) {
	    if (len == 0) {
		vtrace("s3stdin EOF\n");
		vb_free(&r);
		x3270_exit(0);
	    } else {
		break;
	    }
	}
	if (c == '\r') {
	    continue;
	}
	if (c == '\n') {
	    break;
	}
	vb_append(&r, &c, 1);
	len++;
    }

    /* Run the command as a macro. */
    buf = vb_consume(&r);
    vtrace("s3stdin read '%s'\n", buf);
    push_cb(buf, len, &stdin_cb, NULL);
    Free(buf);
}

#else /*][*/

/**
 * Read the next command from stdin (Windows version).
 * @param[in] fd	File descriptor
 * @param[in] id	I/O identifier
 */
static void
stdin_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    if (stdin_nr < 0) {
	vtrace("s3stdin read error: %s\n", strerror(stdin_errno));
	x3270_exit(1);
    }
    if (stdin_nr == 0) {
	vtrace("s3stdin EOF\n");
	x3270_exit(0);
    }

    if (stdin_nr > 0 && stdin_buf[stdin_nr] == '\n') {
	stdin_nr--;
    }
    if (stdin_nr > 0 && stdin_buf[stdin_nr] == '\r') {
	stdin_nr--;
    }
    vtrace("s3stdin read '%.*s'\n", stdin_nr, stdin_buf);
    push_cb(stdin_buf, stdin_nr, &stdin_cb, NULL);
}

#endif /*]*/

/**
 * Callback for data returned to stdin.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
stdin_data(task_cbh handle _is_unused, const char *buf, size_t len,
	bool success)
{
    printf(DATA_PREFIX "%.*s\n", (int)len, buf);
    fflush(stdout);
}

/**
 * Callback for completion of one command executed from stdin.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if context is complete
 */
static bool
stdin_done(task_cbh handle, bool success, bool abort)
{
    /* Print the prompt. */
    if (!pushed_wait) {
	char *prompt = task_cb_prompt(handle);

	vtrace("Output for s3stdin: %s/%s\n", prompt,
		success? PROMPT_OK: PROMPT_ERROR);
	printf("%s\n%s\n", prompt, success? PROMPT_OK: PROMPT_ERROR);
	fflush(stdout);
    }
    pushed_wait = false;

    /* Allow more. */
    if (enabled) {
#if !defined(_WIN32) /*[*/
	stdin_id = AddInput(fileno(stdin), stdin_input);
#else /*][*/
	stdin_nr = 0;
	SetEvent(stdin_enable_event);
	if (stdin_id == NULL_IOID) {
	    stdin_id = AddInput(stdin_done_event, stdin_input);
	}
#endif /*]*/
    }

    /* Future commands will be async. */
    return true;
}

#if defined(_WIN32) /*[*/
/* stdin input thread */
static DWORD WINAPI
stdin_read(LPVOID lpParameter _is_unused)
{
    for (;;) {
	DWORD rv;

	rv = WaitForSingleObject(stdin_enable_event, INFINITE);
	switch (rv) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
	case WAIT_FAILED:
	    stdin_nr = -1;
	    stdin_errno = EINVAL;
	    SetEvent(stdin_done_event);
	    break;
	case WAIT_OBJECT_0:
	    stdin_nr = read(0, stdin_buf, sizeof(stdin_buf));
	    if (stdin_nr < 0) {
		stdin_errno = errno;
	    }
	    SetEvent(stdin_done_event);
	    break;
	}
    }
    return 0;
}
#endif /*]*/

/**
 * Back end of the CloseScript action. Stop accepting input from stdin.
 */
static void
stdin_closescript(task_cbh handle _is_unused)
{
    enabled = false;
}

/**
 * Initialize reading commands from stdin.
 */
void
stdin_init(void)
{
    static char *wait = "Wait(InputField)";

#if defined(_WIN32) /*[*/
    /* Set up the thread that reads from stdin. */
    stdin_enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    stdin_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    stdin_thread = CreateThread(NULL, 0, stdin_read, NULL, 0, NULL);
    if (stdin_thread == NULL) {
	popup_an_error("Cannot create s3stdin read thread: %s\n",
		win32_strerror(GetLastError()));
    }
#endif /*]*/

    /* If not connected yet, wait for one before enabling input. */
    /* XXX: We might need to add a Wait(Connect) action. */
    if ((HALF_CONNECTED || (CONNECTED && (kybdlock & KL_AWAITING_FIRST)))) {
	push_cb(wait, strlen(wait), &stdin_cb, NULL);
	pushed_wait = true;
    } else {
	/* Allow input. */
#if !defined(_WIN32) /*[*/
	stdin_id = AddInput(fileno(stdin), stdin_input);
#else /*][*/
	SetEvent(stdin_enable_event);
	stdin_id = AddInput(stdin_done_event, stdin_input);
#endif /*]*/
    }
}
