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
 *      stdinscript.c
 *              Reading script actions from stdin.
 */

#include "globals.h"

#include "wincmn.h"
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include "actions.h"
#include "json.h"
#include "json_run.h"
#include "kybd.h"
#include "names.h"
#include "popups.h"
#include "s3270_proto.h"
#include "source.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"
#include "w3misc.h"
#include "xio.h"

static void stdin_input(iosrc_t fd, ioid_t id);
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
static char stdin_buf[8192];
static ssize_t stdin_nr;
static int stdin_errno;
#endif /*]*/

static bool pushed_wait = false;
static bool enabled = true;
static struct {		/* pending JSON input state */
    char *buf;
} pj_in;
struct {		/* pending JSON output state */
    bool pending;
    json_t *json;
} pj_out;

/**
 * Check a string for (possibly incremental) JSON.
 * If the JSON object is complete, executes it.
 *
 * @param[in] buf	NUL-terminated buffer with text.
 *
 * @return true if in JSON format.
 */
static bool
json_input(char *buf)
{
    size_t len = strlen(buf);

    if (pj_in.buf != NULL) {
	/* Concatenate the input. */
	char *s = pj_in.buf;

	pj_in.buf = xs_buffer("%s%s", pj_in.buf, buf);
	Free(s);
    } else {
	char *s = buf;

	/* Check for JSON. */
	while (len && isspace((int)*s)) {
	    len--;
	    s++;
	}
	if (len && (*s == '{' || *s == '[' || *s == '"')) {
	    pj_in.buf = NewString(buf);
	}
    }

    if (pj_in.buf != NULL) {
	cmd_t **cmds;
	char *single;
	char *errmsg;
	hjparse_ret_t ret;

	/* Try JSON parsing. */
	ret = hjson_parse(pj_in.buf, strlen(pj_in.buf), &cmds, &single,
		&errmsg);
	if (ret != HJ_OK) {
	    /* Unsuccessful JSON. */
	    if (ret == HJ_BAD_SYNTAX || ret == HJ_BAD_CONTENT) {
		char *fail = xs_buffer(AnFail "(\"%s\")", errmsg);

		/* Defective JSON. */
		Free(errmsg);
		push_cb(fail, strlen(fail), &stdin_cb, NULL);
		Free(fail);
		Replace(pj_in.buf, NULL);
		pj_out.pending = (ret == HJ_BAD_CONTENT);
		return true;
	    }
	    /* Incomplete JSON. */
	    /* Enable more input. */
	    assert(ret == HJ_INCOMPLETE);
	    Free(errmsg);
#if !defined(_WIN32) /*[*/
	    stdin_id = AddInput(fileno(stdin), stdin_input);
#else /*][*/
	    stdin_nr = 0;
	    SetEvent(stdin_enable_event);
	    if (stdin_id == NULL_IOID) {
		stdin_id = AddInput(stdin_done_event, stdin_input);
	    }
#endif /*]*/
	    return true;
	}

	/* Successful JSON. */
	pj_out.pending = true;
	if (cmds != NULL) {
	    push_cb_split(cmds, &stdin_cb, NULL);
	} else {
	    push_cb(single, strlen(single), &stdin_cb, NULL);
	    Free(single);
	}
	Replace(pj_in.buf, NULL);
	return true;
    }

    /* Not JSON. */
    return false;
}

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
    if (!json_input(buf)) {
	pj_out.pending = false;
	push_cb(buf, len, &stdin_cb, NULL);
    }
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

    vtrace("s3stdin read '%.*s'\n", (int)stdin_nr, stdin_buf);
    if (!json_input(stdin_buf)) {
	if (stdin_nr > 0 && stdin_buf[stdin_nr] == '\n') {
	    stdin_nr--;
	}
	if (stdin_nr > 0 && stdin_buf[stdin_nr] == '\r') {
	    stdin_nr--;
	}
	push_cb(stdin_buf, stdin_nr, &stdin_cb, NULL);
    }
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
    /*
     * Enforce the implicit assumption that there are no newlines in the
     * output.
     */
    char *b;
    char *newline;

    while (len > 0 && buf[len - 1] == '\n') {
	len--;
    }

    b = xs_buffer("%.*s", (int)len, buf);
    while ((newline = strchr(b, '\n')) != NULL) {
	*newline = ' ';
    }

    if (pj_out.pending) {
	json_t *result_array;

	if (pj_out.json == NULL) {
            pj_out.json = json_object();
            result_array = json_array();
            json_object_set(pj_out.json, "result", NT, result_array);
        } else {
            assert(json_object_member(pj_out.json, "result", NT,
                        &result_array));
        }

        json_array_append(result_array, json_string(b, strlen(b)));
    } else {
	if (!pushed_wait) {
	    printf("%s%s\n", DATA_PREFIX, b);
	    fflush(stdout);
	} else {
	    fprintf(stderr, AnWait "(): %s\n", b);
	    fflush(stderr);
	}
    }
    Free(b);
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
    if (pj_out.pending) {
	char *w;
	char *prompt = task_cb_prompt(handle);

	if (pj_out.json == NULL) {
	    pj_out.json = json_object();
	    json_object_set(pj_out.json, "result", NT, json_array());
	}
	json_object_set(pj_out.json, "success", NT, json_boolean(success));
	json_object_set(pj_out.json, "status", NT, json_string(prompt, NT));
	printf("%s\n", w = json_write_o(pj_out.json, JW_ONE_LINE));
	fflush(stdout);
	Free(w);
	json_free(pj_out.json);
	pj_out.pending = false;
    } else {
	if (!pushed_wait) {
	    char *prompt = task_cb_prompt(handle);

	    vtrace("Output for s3stdin: %s/%s\n", prompt,
		    success? PROMPT_OK: PROMPT_ERROR);
	    printf("%s\n%s\n", prompt, success? PROMPT_OK: PROMPT_ERROR);
	    fflush(stdout);
	}
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
	    if (fgets(stdin_buf, sizeof(stdin_buf), stdin) == NULL) {
		if (feof(stdin)) {
		    stdin_nr = 0;
		}
		else {
		    stdin_nr = -1;
		    stdin_errno = errno;
		}
	    } else {
		stdin_nr = strlen(stdin_buf);
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
    static char *wait = AnWait "(" KwInputField ")";

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
