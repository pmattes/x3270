/*
 * Copyright (c) 2001-2009, 2013, 2015, 2018-2020 Paul Mattes.
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
 *	child.c
 *		Child process output support.
 */
#include "globals.h"

#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include "child.h"
#include "popups.h" /* must be before child_popups.h */
#include "child_popups.h"
#include "trace.h"
#include "utils.h"
#include "xio.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

#define CHILD_BUF	1024

static bool child_initted = false;
static bool child_broken = false;
static bool child_discarding = false;
#if !defined(_WIN32) /*[*/
static int child_outpipe[2];
static int child_errpipe[2];
#else /*]*/
static HANDLE child_stdout_rd = INVALID_HANDLE_VALUE;
static HANDLE child_stdout_wr = INVALID_HANDLE_VALUE;
static HANDLE child_stderr_rd = INVALID_HANDLE_VALUE;
static HANDLE child_stderr_wr = INVALID_HANDLE_VALUE;
#endif /*]*/

#if !defined(_WIN32) /*[*/
static struct pr3o {
    int fd;			/* file descriptor */
    ioid_t input_id;		/* input ID */
    ioid_t timeout_id; 		/* timeout ID */
    int count;			/* input count */
    char buf[CHILD_BUF];	/* input buffer */
} child_stdout = { -1, 0L, 0L, 0 },
  child_stderr = { -1, 0L, 0L, 0 };
#else /*][*/
typedef struct {
    HANDLE pipe_handle;
    HANDLE enable_event;
    HANDLE done_event;
    HANDLE thread;
    char buf[CHILD_BUF];
    DWORD nr;
    int error;
    bool is_stderr;
} cr_t;
cr_t cr_stdout, cr_stderr;
#endif /*]*/

#if !defined(_WIN32) /*[*/
static void child_output(iosrc_t fd, ioid_t id);
static void child_error(iosrc_t fd, ioid_t id);
static void child_otimeout(ioid_t id);
static void child_etimeout(ioid_t id);
static void child_dump(struct pr3o *p, bool is_err);
#endif /*]*/

#if defined(_WIN32) /*[*/
static DWORD WINAPI child_read_thread(LPVOID parameter);
static void cr_output(iosrc_t fd, ioid_t id);
#endif /*]*/

static void
init_child(void)
{
#if defined(_WIN32) /*[*/
    SECURITY_ATTRIBUTES sa;
    DWORD mode;
#endif /*]*/

    /* If initialization failed, there isn't much we can do. */
    if (child_broken) {
	return;
    }

#if !defined(_WIN32) /*[*/
    /* Create pipes. */
    if (pipe(child_outpipe) < 0) {
	popup_an_errno(errno, "pipe()");
	child_broken = true;
	return;
    }
    if (pipe(child_errpipe) < 0) {
	popup_an_errno(errno, "pipe()");
	close(child_outpipe[0]);
	close(child_outpipe[1]);
	child_broken = true;
	return;
    }
    vtrace("init_child: child_outpipe is %d %d\n", child_outpipe[0], child_outpipe[1]);

    /* Make sure their read ends are closed in child processes. */
    fcntl(child_outpipe[0], F_SETFD, 1);
    fcntl(child_errpipe[0], F_SETFD, 1);

    /* Initialize the pop-ups. */
    child_popup_init();

    /* Express interest in their output. */
    child_stdout.fd = child_outpipe[0];
    child_stdout.input_id = AddInput(child_outpipe[0], child_output);
    child_stderr.fd = child_errpipe[0];
    child_stderr.input_id = AddInput(child_errpipe[0], child_error);

#else /*][*/

    /* Create pipes. */
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&child_stdout_rd, &child_stdout_wr, &sa, 0)) {
	popup_an_error("CreatePipe(stdout) failed: %s",
		win32_strerror(GetLastError()));
	child_broken = true;
	return;
    }
    if (!SetHandleInformation(child_stdout_rd, HANDLE_FLAG_INHERIT, 0)) {
	popup_an_error("SetHandleInformation(stdout) failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(child_stdout_rd);
	CloseHandle(child_stdout_wr);
	child_broken = true;
	return;
    }
    mode = PIPE_READMODE_BYTE;
    if (!SetNamedPipeHandleState(child_stdout_rd, &mode, NULL, NULL)) {
	popup_an_error("SetNamedPipeHandleState(stdout) failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(child_stdout_rd);
	CloseHandle(child_stdout_wr);
	child_broken = true;
	return;
    }

    if (!CreatePipe(&child_stderr_rd, &child_stderr_wr, &sa, 0)) {
	popup_an_error("CreatePipe(stderr) failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(child_stdout_rd);
	CloseHandle(child_stdout_wr);
	child_broken = true;
	return;
    }
    if (!SetHandleInformation(child_stderr_rd, HANDLE_FLAG_INHERIT, 0)) {
	popup_an_error("SetHandleInformation(stderr) failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(child_stdout_rd);
	CloseHandle(child_stdout_wr);
	CloseHandle(child_stderr_rd);
	CloseHandle(child_stderr_wr);
	child_broken = true;
	return;
    }
    mode = PIPE_READMODE_BYTE;
    if (!SetNamedPipeHandleState(child_stderr_rd, &mode, NULL, NULL)) {
	popup_an_error("SetNamedPipeHandleState(stderr) failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(child_stdout_rd);
	CloseHandle(child_stdout_wr);
	CloseHandle(child_stderr_rd);
	CloseHandle(child_stderr_wr);
	child_broken = true;
	return;
    }

    /* Initialize the pop-ups. */
    child_popup_init();

    /* Express interest in their output. */
    cr_stdout.pipe_handle = child_stdout_rd;
    cr_stdout.enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    cr_stdout.done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    cr_stdout.thread = CreateThread(NULL, 0, child_read_thread, &cr_stdout, 0,
	    NULL);
    AddInput(cr_stdout.done_event, cr_output);
    SetEvent(cr_stdout.enable_event);

    cr_stderr.pipe_handle = child_stderr_rd;
    cr_stderr.enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    cr_stderr.done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    cr_stderr.thread = CreateThread(NULL, 0, child_read_thread, &cr_stderr, 0,
	    NULL);
    cr_stderr.is_stderr = true;
    AddInput(cr_stderr.done_event, cr_output);
    SetEvent(cr_stderr.enable_event);

#endif /*]*/

    child_initted = true;
}

#if !defined(_WIN32) /*[*/
/*
 * Fork a child process, with its stdout/stderr connected to pop-up windows.
 * Returns -1 for an error, 0 for child context, pid for parent context.
 */
int
fork_child(void)
{
    pid_t pid;

    /* Do initialization, if it hasn't been done already. */
    if (!child_initted) {
	init_child();
    }

    /* If output was being dumped, turn it back on now. */
    if (child_discarding) {
	child_discarding = false;
    }

    /* Fork and rearrange output. */
    pid = fork();
    if (pid == 0) {
	/* Child. */
	dup2(child_outpipe[1], 1);
	close(child_outpipe[1]);
	dup2(child_errpipe[1], 2);
	close(child_errpipe[1]);
    }
    return pid;
}
#else /*][*/
/* Get the stdout and sterr redirect handles. */
void
get_child_handles(HANDLE *out, HANDLE *err)
{

    /* Do initialization, if it hasn't been done already. */
    if (!child_initted) {
	init_child();
    }

    /* If output was being dumped, turn it back on now. */
    if (child_discarding) {
	child_discarding = false;
    }

    /* Return the handles. */
    *out = child_stdout_wr;
    *err = child_stderr_wr;
}
#endif /*]*/

#if !defined(_WIN32) /*[*/
/* There's data from a child. */
static void
child_data(struct pr3o *p, bool is_err)
{
    int space;
    ssize_t nr;

    /*
     * If we're discarding output, pull it in and drop it on the floor.
     */
    if (child_discarding) {
	nr = read(p->fd, p->buf, CHILD_BUF);
	return;
    }

    /* Read whatever there is. */
    space = CHILD_BUF - p->count - 1;
    nr = read(p->fd, p->buf + p->count, space);
    if (nr < 0) {
	popup_an_errno(errno, "child session pipe input");
	return;
    }

    /* Add it to the buffer, and add a NULL. */
    p->count += nr;
    p->buf[p->count] = '\0';

    /*
     * If there's no more room in the buffer, dump it now.  Otherwise,
     * give it a second to generate more output.
     */
    if (p->count >= CHILD_BUF - 1) {
	child_dump(p, is_err);
    } else if (p->timeout_id == 0L) {
	p->timeout_id = AddTimeOut(1000,
		is_err? child_etimeout: child_otimeout);
    }
}

/* The child process has some output for us. */
static void
child_output(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    child_data(&child_stdout, false);
}

/* The child process has some error output for us. */
static void
child_error(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    child_data(&child_stderr, true);
}

/* Timeout from child output or error output. */
static void
child_timeout(struct pr3o *p, bool is_err)
{
    /* Forget the timeout ID. */
    p->timeout_id = 0L;

    /* Dump the output. */
    child_dump(p, is_err);
}

/* Timeout from child output. */
static void
child_otimeout(ioid_t id _is_unused)
{
    child_timeout(&child_stdout, false);
}

/* Timeout from child error output. */
static void
child_etimeout(ioid_t id _is_unused)
{
    child_timeout(&child_stderr, true);
}

/*
 * Abort button from child output.
 * Ignore output from the child process, so the user can abort it.
 */
void
child_ignore_output(void)
{
    /* Pitch pending output. */
    child_stdout.count = 0;
    child_stderr.count = 0;

    /* Remove pendnig timeouts. */
    if (child_stdout.timeout_id) {
	RemoveTimeOut(child_stdout.timeout_id);
	child_stdout.timeout_id = 0L;
    }
    if (child_stderr.timeout_id) {
	RemoveTimeOut(child_stderr.timeout_id);
	child_stderr.timeout_id = 0L;
    }

    /* Remember it. */
    child_discarding = true;
}

/* Dump pending child process output. */
static void
child_dump(struct pr3o *p, bool is_err)
{
    if (p->count) {
	/*
	 * Strip any trailing newline, and make sure the buffer is
	 * NULL terminated.
	 */
	if (p->buf[p->count - 1] == '\n') {
	    p->buf[--(p->count)] = '\0';
	} else if (p->buf[p->count]) {
	    p->buf[p->count] = '\0';
	}

	/* Dump it and clear the buffer. */
	popup_child_output(is_err, child_ignore_output, "%s", p->buf);

	p->count = 0;
    }
}
#endif /*]*/

#if defined(_WIN32) /*[*/
/* Read from the child's stdout or stderr. */
static DWORD WINAPI
child_read_thread(LPVOID parameter)
{
    cr_t *cr = (cr_t *)parameter;
    DWORD success;

    for (;;) {
	DWORD rv = WaitForSingleObject(cr->enable_event, INFINITE);
	switch (rv) {
	    case WAIT_OBJECT_0:
		success = ReadFile(cr->pipe_handle, cr->buf, CHILD_BUF,
			&cr->nr, NULL);
		if (!success) {
		    cr->nr = 0;
		    cr->error = GetLastError();
		} else {
		    cr->error = 0;
		}
		SetEvent(cr->done_event);
		break;
	    default:
		cr->nr = 0;
		cr->error = ERROR_NO_DATA;
		SetEvent(cr->done_event);
		break;
	}
    }
    return 0;
}

/* The child stdout or stderr thread produced output. */
static void
cr_output(iosrc_t fd, ioid_t id)
{
    cr_t *cr;

    /* Find the descriptor. */
    if (fd == cr_stdout.done_event) {
	cr = &cr_stdout;
    } else if (fd == cr_stderr.done_event) {
	cr = &cr_stderr;
    } else {
	vtrace("cr_output: unknown handle\n");
	return;
    }

    if (cr->nr == 0) {
	fprintf(stderr, "cr_output failed: error %s\n",
		win32_strerror(cr->error));
	x3270_exit(1);
    }

    popup_child_output(cr->is_stderr, NULL, "%.*s", (int)cr->nr, cr->buf);

    /* Ready for more. */
    SetEvent(cr->enable_event);
}
#endif /*]*/
