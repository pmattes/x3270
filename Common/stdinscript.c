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
#include "s3common.h"
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
static void stdin_setflags(task_cbh handle, unsigned flags);
static unsigned stdin_getflags(task_cbh handle);

/* Callback block for stdin. */
static tcb_t stdin_cb = {
    "s3stdin",
    IA_SCRIPT,
    CB_NEW_TASKQ,
    stdin_data,
    stdin_done,
    NULL,
    stdin_closescript,
    stdin_setflags,
    stdin_getflags
};

static ioid_t stdin_id = NULL_IOID;
static ssize_t stdin_nr;
#if !defined(_WIN32) /*[*/
static char *stdin_buf;
static bool stdin_eof;
#else /*][*/
static char stdin_buf[8192];
static HANDLE stdin_thread;
static HANDLE stdin_enable_event, stdin_done_event;
static int stdin_errno;
#endif /*]*/

static bool pushed_wait = false;
static bool enabled = true;
static char *pj_in;	/* pending JSON input */
static json_t *pj_out;		/* pending JSON output state */

static unsigned stdin_capabilities;

/**
 * Check a string for (possibly incremental) JSON.
 * If the JSON object is complete, executes it.
 *
 * @param[in] buf		NUL-terminated buffer with text.
 * @param[out] need_more	Returned true if more input needed.
 *
 * @return true if in JSON format.
 */
static bool
json_input(char *buf, bool *need_more)
{
    *need_more = false;

    if (pj_in != NULL) {
	/* Concatenate the input. */
	char *s = pj_in;

	pj_in = Asprintf("%s%s", pj_in, buf);
	Free(s);
    } else {
	char *s = buf;

	/* Check for JSON. */
	while (*s && isspace((int)*s)) {
	    s++;
	}
	if (*s == '{' || *s == '[' || *s == '"') {
	    pj_in = NewString(buf);
	}
    }

    if (pj_in != NULL) {
	cmd_t **cmds;
	char *single;
	char *errmsg;
	hjparse_ret_t ret;

	/* Try JSON parsing. */
	ret = hjson_parse(pj_in, strlen(pj_in), &cmds, &single,
		&errmsg);
	if (ret != HJ_OK) {
	    /* Unsuccessful JSON. */
	    if (ret == HJ_BAD_SYNTAX || ret == HJ_BAD_CONTENT) {
		char *fail = Asprintf(AnFail "(\"%s\")", errmsg);

		/* Defective JSON. */
		Free(errmsg);
		push_cb(fail, strlen(fail), &stdin_cb, NULL);
		Free(fail);
		Replace(pj_in, NULL);
		if (ret == HJ_BAD_CONTENT) {
		    pj_out = s3json_init();
		}
		return true;
	    }
	    /* Incomplete JSON. */
	    /* Enable more input. */
	    assert(ret == HJ_INCOMPLETE);
	    Free(errmsg);
	    *need_more = true;
	    return true;
	}

	/* Successful JSON. */
	pj_out = s3json_init();
	if (cmds != NULL) {
	    push_cb_split(cmds, &stdin_cb, NULL);
	} else {
	    push_cb(single, strlen(single), &stdin_cb, NULL);
	    Free(single);
	}
	Replace(pj_in, NULL);
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
    bool eol = false;
    bool need_more = false;

    while (!eol) {
	char c;
	int nr;
	fd_set rfds;
	struct timeval tv;
	int ns;

	FD_ZERO(&rfds);
	FD_SET(fileno(stdin), &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	ns = select(fileno(stdin) + 1, &rfds, NULL, NULL, &tv);
	if (ns == 0) {
	    vtrace("s3stdin read blocked\n");
	    return;
	}

	nr = read(fileno(stdin), &c, 1);
	if (nr < 0) {
	    vtrace("s3stdin read error: %s\n", strerror(errno));
	    x3270_exit(1);
	} else if (nr == 0) {
	    vtrace("s3stdin EOF\n");
	    if (stdin_nr == 0) {
		x3270_exit(0);
	    } else {
		stdin_eof = true;
		eol = true;
	    }
	} else if (c == '\r') {
	    continue;
	} else if (c == '\n') {
	    eol = true;
	} else {
	    stdin_buf = Realloc(stdin_buf, stdin_nr + 2);
	    stdin_buf[stdin_nr++] = c;
	}
    }

    /* Stop input. */
    RemoveInput(stdin_id);
    stdin_id = NULL_IOID;

    /* Run the command as a macro. */
    if (stdin_buf == NULL) {
	stdin_buf = Malloc(1);
    }
    stdin_buf[stdin_nr] = '\0';
    vtrace("s3stdin read '%s'\n", stdin_buf);
    if (!json_input(stdin_buf, &need_more)) {
	json_free(pj_out);
	push_cb(stdin_buf, strlen(stdin_buf), &stdin_cb, NULL);
    } else if (need_more) {
	/* Allow more input. */
	stdin_id = AddInput(fileno(stdin), stdin_input);
    }
    Replace(stdin_buf, NULL);
    stdin_nr = 0;
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
    bool need_more = false;

    if (stdin_nr < 0) {
	vtrace("s3stdin read error: %s\n", strerror(stdin_errno));
	x3270_exit(1);
    }
    if (stdin_nr == 0) {
	vtrace("s3stdin EOF\n");
	x3270_exit(0);
    }

    vtrace("s3stdin read '%.*s'\n", (int)stdin_nr, stdin_buf);
    if (!json_input(stdin_buf, &need_more)) {
	if (stdin_nr > 0 && stdin_buf[stdin_nr] == '\n') {
	    stdin_nr--;
	}
	if (stdin_nr > 0 && stdin_buf[stdin_nr] == '\r') {
	    stdin_nr--;
	}
	push_cb(stdin_buf, stdin_nr, &stdin_cb, NULL);
    } else if (need_more) {
	/* Get more input. */
	stdin_nr = 0;
	SetEvent(stdin_enable_event);
	if (stdin_id == NULL_IOID) {
	    stdin_id = AddInput(stdin_done_event, stdin_input);
	}
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
stdin_data(task_cbh handle _is_unused, const char *buf, size_t len, bool success)
{
    char *raw;
    char *cooked;

    s3data(buf, len, success, stdin_capabilities, pj_out, &raw, &cooked);
    if (pushed_wait) {
	fprintf(stderr, AnWait "(): %s\n", raw);
	fflush(stderr);
    } else if (cooked != NULL) {
	fputs(cooked, stdout);
	fflush(stdout);
    }
    Free(raw);
    Free(cooked);
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
    char *out;

    /* Print the output or the prompt. */
    s3done(handle, success, &pj_out, &out);
    if (!pushed_wait) {
	printf("%s", out);
	fflush(stdout);
    }
    Free(out);
    pushed_wait = false;

    /* Allow more. */
    if (enabled) {
#if !defined(_WIN32) /*[*/
	if (stdin_eof) {
	    x3270_exit(0);
	}
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

/* Set flags. */
static void
stdin_setflags(task_cbh handle _is_unused, unsigned flags)
{
    stdin_capabilities = flags;
}

/* Get flags. */
static unsigned
stdin_getflags(task_cbh handle _is_unused)
{
    return stdin_capabilities;
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
