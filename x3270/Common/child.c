/*
 * Copyright (c) 2001-2009, Paul Mattes.
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

#include "popupsc.h"
#include "utilc.h"

#define CHILD_BUF	1024

static Boolean child_initted = False;
static Boolean child_broken = False;
static Boolean child_discarding = False;
static int child_outpipe[2];
static int child_errpipe[2];

static struct pr3o {
	int fd;			/* file descriptor */
	unsigned long input_id;	/* input ID */
	unsigned long timeout_id; /* timeout ID */
	int count;		/* input count */
	char buf[CHILD_BUF];	/* input buffer */
} child_stdout = { -1, 0L, 0L, 0 },
  child_stderr = { -1, 0L, 0L, 0 };

static void child_output(void);
static void child_error(void);
static void child_otimeout(void);
static void child_etimeout(void);
static void child_dump(struct pr3o *p, Boolean is_err);

static void
init_child(void)
{
	/* If initialization failed, there isn't much we can do. */
	if (child_broken)
		return;

	/* Create pipes. */
	if (pipe(child_outpipe) < 0) {
		popup_an_errno(errno, "pipe()");
		child_broken = True;
		return;
	}
	if (pipe(child_errpipe) < 0) {
		popup_an_errno(errno, "pipe()");
		close(child_outpipe[0]);
		close(child_outpipe[1]);
		child_broken = True;
		return;
	}

	/* Make sure their read ends are closed in child processes. */
	(void) fcntl(child_outpipe[0], F_SETFD, 1);
	(void) fcntl(child_errpipe[0], F_SETFD, 1);

#if defined(X3270_DISPLAY) /*[*/
	/* Initialize the pop-ups. */
	child_popup_init();
#endif

	/* Express interest in their output. */
	child_stdout.fd = child_outpipe[0];
	child_stdout.input_id = AddInput(child_outpipe[0], child_output);
	child_stderr.fd = child_errpipe[0];
	child_stderr.input_id = AddInput(child_errpipe[0], child_error);

	child_initted = True;
}

/*
 * Fork a child process, with its stdout/stderr connected to pop-up windows.
 * Returns -1 for an error, 0 for child context, pid for parent context.
 */
int
fork_child(void)
{
	pid_t pid;

	/* Do initialization, if it hasn't been done already. */
	if (!child_initted)
		init_child();

	/* If output was being dumped, turn it back on now. */
	if (child_discarding)
		child_discarding = False;

	/* Fork and rearrange output. */
	pid = fork();
	if (pid == 0) {
		/* Child. */
		(void) dup2(child_outpipe[1], 1);
		(void) close(child_outpipe[1]);
		(void) dup2(child_errpipe[1], 2);
		(void) close(child_errpipe[1]);
	}
	return pid;
}

/* There's data from a child. */
static void
child_data(struct pr3o *p, Boolean is_err)
{
	int space;
	int nr;
	static char exitmsg[] = "Printer session exited";

	/*
	 * If we're discarding output, pull it in and drop it on the floor.
	 */
	if (child_discarding) {
		(void) read(p->fd, p->buf, CHILD_BUF);
		return;
	}

	/* Read whatever there is. */
	space = CHILD_BUF - p->count - 1;
	nr = read(p->fd, p->buf + p->count, space);

	/* Handle read errors and end-of-file. */
	if (nr < 0) {
		popup_an_errno(errno, "child session pipe input");
		return;
	}
	if (nr == 0) {
		if (child_stderr.timeout_id != 0L) {
			/*
			 * Append a termination error message to whatever the
			 * child process said, and pop it up.
			 */
			p = &child_stderr;
			space = CHILD_BUF - p->count - 1;
			if (p->count && *(p->buf + p->count - 1) != '\n') {
				*(p->buf + p->count) = '\n';
				p->count++;
				space--;
			}
			(void) strncpy(p->buf + p->count, exitmsg, space);
			p->count += strlen(exitmsg);
			if (p->count >= CHILD_BUF)
				p->count = CHILD_BUF - 1;
			child_dump(p, True);
		} else {
			popup_an_error(exitmsg);
		}
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
child_output(void)
{
	child_data(&child_stdout, False);
}

/* The child process has some error output for us. */
static void
child_error(void)
{
	child_data(&child_stderr, True);
}

/* Timeout from child output or error output. */
static void
child_timeout(struct pr3o *p, Boolean is_err)
{
	/* Forget the timeout ID. */
	p->timeout_id = 0L;

	/* Dump the output. */
	child_dump(p, is_err);
}

/* Timeout from child output. */
static void
child_otimeout(void)
{
	child_timeout(&child_stdout, False);
}

/* Timeout from child error output. */
static void
child_etimeout(void)
{
	child_timeout(&child_stderr, True);
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
	child_discarding = True;
}

/* Dump pending child process output. */
static void
child_dump(struct pr3o *p, Boolean is_err)
{
	if (p->count) {
		/*
		 * Strip any trailing newline, and make sure the buffer is
		 * NULL terminated.
		 */
		if (p->buf[p->count - 1] == '\n')
			p->buf[--(p->count)] = '\0';
		else if (p->buf[p->count])
			p->buf[p->count] = '\0';

		/* Dump it and clear the buffer. */
#if defined(X3270_DISPLAY) /*[*/
		popup_child_output(is_err, child_ignore_output, "%s", p->buf);
#else /*][*/
		action_output("%s", p->buf);
#endif
		p->count = 0;
	}
}
