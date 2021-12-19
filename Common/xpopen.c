/*
 * Copyright (c) 2018, 2021 Paul Mattes.
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
 *	xpopen.c
 *		popen that exposes the child process ID
 */

#include "globals.h"

#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include "utils.h"
#include "xpopen.h"

/* Typedefs */
typedef struct {	/* xpopen context */
    llist_t llist;	/* linkage */
    FILE *fp;		/* file pointer */
    pid_t pid;		/* child process ID */
} xpopen_t;
llist_t xpopens = LLIST_INIT(xpopens);

/* Globals */

/* Statics */

/* Create a file pointer to a subprocess. */
FILE *
xpopen(const char *command, const char *mode, pid_t *pidp)
{
    char *r, *w;
    xpopen_t *xp = NULL;
    int rwmode; /* 0 for read, 1 for write */
    int pipes[2] = { -1, -1 };

    /* Check the mode. */
    r = strchr(mode, 'r');
    w = strchr(mode, 'w');
    if ((r == NULL) == (w == NULL)) {
	/* Both, or neither. */
	errno = EINVAL;
	goto fail;
    }
    rwmode = (w != NULL);

    /* Allocate the context. */
    xp = Calloc(1, sizeof(xpopen_t));

    /* Create the pipes. */
    if (pipe(pipes) < 0) {
	goto fail;
    }

    /* Add the file pointer. */
    xp->fp = fdopen(pipes[rwmode], mode);
    if (xp->fp == NULL) {
	goto fail;
    }

    /* Create the child process. */
    switch (xp->pid = fork()) {
    default:	/* parent */
	break;
    case 0:	/* child */
	/* Redirect I/O. */
	close(pipes[rwmode]);
	if (dup2(pipes[!rwmode], !rwmode) < 0) {
	    exit(1);
	}
	close(pipes[!rwmode]);

	/* Run the command. */
	if (execl("/bin/sh", "/bin/sh", "-c", command, NULL) < 0) {
	    exit(1);
	}
	break;
    case -1:	/* error */
	pipes[rwmode] = -1; /* belongs to xp->fp */
	goto fail;
    }

    /* Close the child's end of the pipe. */
    close(pipes[!rwmode]);

    /* Remember for xpclose. */
    llist_init(&xp->llist);
    LLIST_APPEND(&xp->llist, xpopens);

    /* Done. */
    *pidp = xp->pid;
    return xp->fp;

fail:
    /* Clean up after a failure. */
    if (xp != NULL) {
	if (xp->fp != NULL) {
	    fclose(xp->fp);
	}
	if (pipes[0] != -1) {
	    close(pipes[0]);
	}
	if (pipes[1] != -1) {
	    close(pipes[1]);
	}
	Free(xp);
    }
    return NULL;
}

/* Complete the subprocess, waiting for it to complete. */
int
xpclose(FILE *fp, unsigned flags)
{
    xpopen_t *xp;
    bool found = false;
    pid_t pid;
    int status = 0;

    FOREACH_LLIST(&xpopens, xp, xpopen_t *) {
	if (xp->fp == fp) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&xpopens, xp, xpopen_t *);

    if (!found) {
	errno = EINVAL;
	return -1;
    }

    /* Close the file. */
    fclose(fp);

    /* Free the context. */
    pid = xp->pid;
    llist_unlink(&xp->llist);
    Free(xp);

    /* Wait for the child to exit. */
    if (!(flags & XPC_NOWAIT) && waitpid(pid, &status, 0) < 0) {
	return -1;
    }

    return status;
}
