/*
 * Copyright (c) 2020 Paul Mattes.
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
 *	print_command.c
 *		Print command support.
 */
#include "globals.h"

#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>

#include "popups.h"
#include "print_command.h"
#include "trace.h"
#include "utils.h"
#include "xio.h"

/* List of active printer commands. */
typedef struct {
    llist_t link;
    ioid_t id;
    int from_cmd;
    void (*fail_callback)(void);
} print_command_t;
llist_t print_commands = LLIST_INIT(print_commands);

/* Called when a printer command exits. */
static void
printer_exited(ioid_t id, int status)
{
    print_command_t *c;
    bool found = false;

    /* Find the associated record. */
    FOREACH_LLIST(&print_commands, c, print_command_t *) {
	if (c->id == id) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&print_commands, c, print_command_t *);
    assert(found);

    if (WIFEXITED(status)) {
	int exit_status = WEXITSTATUS(status);

	if (exit_status != 0) {
	    char *errout = NULL;
	    size_t nerr = 0, nerrbuf = 0;

	    /* Dump the command's stderr with the error message. */
	    while (true) {
		size_t nr;

		nerrbuf += 1024;
		errout = Realloc(errout, nerrbuf);
		nr = read(c->from_cmd, errout + nerr, nerrbuf - nerr);
		if (nr <= 0) {
		    break;
		}
		nerr += nr;
	    }

	    if (nerr > 0 && errout[nerr - 1] == '\n') {
		nerr--;
	    }
	    popup_an_error("%.*s%sPrinter process exited with status %d",
		    (int)nerr, errout,
		    nerr? "\n": "",
		    exit_status);
	    Free(errout);

	    if (c->fail_callback != NULL) {
		(*c->fail_callback)();
	    }
	}
    } else if (WIFSIGNALED(status)) {
	popup_an_error("Printer process killed by signal %d",
		WTERMSIG(status));
	if (c->fail_callback != NULL) {
	    (*c->fail_callback)();
	}
    } else {
	popup_an_error("Printer process stopped by unknown status %d", status);
    }

    close(c->from_cmd);
    llist_unlink(&c->link);
    Free(c);
}

/* Create an asynchronous printer session. */
FILE *
printer_open(const char *command, void (*fail_callback)(void))
{
    int to_cmd[2] = { -1, -1 };		/* data to printer command */
    int from_cmd[2] = { -1, -1 };	/* data from printer command */
    pid_t pid;
    print_command_t *c;

    /* Create a pipe for the command's stdin. */
    if (pipe(to_cmd) < 0) {
	popup_an_errno(errno, "pipe");
	goto fail;
	return NULL;
    }
    fcntl(to_cmd[1], F_SETFD, 1);

    /* Create a pipe for the command's stdout and stderr. */
    if (pipe(from_cmd) < 0) {
	popup_an_errno(errno, "pipe");
	goto fail;
    }
    fcntl(from_cmd[0], F_SETFD, 1);

    /* Create the command. */
    switch ((pid = fork())) {
    case -1:
	popup_an_errno(errno, "fork");
	goto fail;
    case 0:
	/* child */
	dup2(to_cmd[0], 0);
	dup2(from_cmd[1], 1);
	dup2(from_cmd[1], 2);
	execlp("/bin/sh", "sh", "-c", command, NULL);
	exit(1);
	break;
    default:
	/* parent */
	close(to_cmd[0]);
	close(from_cmd[1]);
	break;
    }

    /* Keep track of the running command. */
    c = (print_command_t *)Calloc(1, sizeof(print_command_t));
    llist_init(&c->link);
    c->from_cmd = from_cmd[0];
    c->id = AddChild(pid, printer_exited);
    c->fail_callback = fail_callback;
    llist_insert_before(&c->link, &print_commands);

    /* Return a file that can be written to. */
    return fdopen(to_cmd[1], "w");

fail:
    /* Clean up partial work in the event of a failure. */
    if (to_cmd[0] != -1) {
	close(to_cmd[0]);
    }
    if (to_cmd[1] != -1) {
	close(to_cmd[1]);
    }
    if (from_cmd[0] != -1) {
	close(from_cmd[0]);
    }
    if (from_cmd[1] != -1) {
	close(from_cmd[1]);
    }
    return NULL;
}
