/*
 * Copyright (c) 2018, 2021-2022 Paul Mattes.
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
 *	xpopen_test.c
 *		Unit tests for xpopen.
 */

#include "globals.h"

#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "sa_malloc.h"
#include "utils.h"
#include "xpopen.h"

void
nothing(int ignored)
{
}

int
main(int argc, char *argv[])
{
    char buf[1024];
    char outfile[256];
    char cmd[1024];
    pid_t pid;
    FILE *f;
    char *s;
    int rv, status;
    bool verbose = false;

    if (argc > 1 && !strcmp(argv[1], "-v")) {
	verbose = true;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, nothing);

    /* Try an input stream. */
    f = xpopen("cat /etc/hosts", "r", &pid);
    if (f == NULL) {
	perror("xpopen");
	exit(1);
    }
    while ((s = fgets(buf, sizeof(buf), f)) != NULL) {
	if (verbose) {
	    fputs(s, stdout);
	}
    }
    rv = xpclose(f, 0);
    if (rv != 0) {
	fprintf(stderr, "child exited with status %d\n", rv);
	exit(1);
    }

    if (verbose) {
	printf("\n========\n\n");
    }

    /* Try an input stream with explicit wait. */
    f = xpopen("cat /etc/hosts", "r", &pid);
    if (f == NULL) {
	perror("xpopen");
	exit(1);
    }
    while ((s = fgets(buf, sizeof(buf), f)) != NULL) {
	if (verbose) {
	    fputs(s, stdout);
	}
    }
    xpclose(f, XPC_NOWAIT);
    rv = waitpid(pid, &status, 0);
    if (rv < 0) {
	perror("waitpid");
    }
    if (status != 0) {
	fprintf(stderr, "child exited with status %d\n", status);
	exit(1);
    }

    if (verbose) {
	printf("\n========\n\n");
    }

    /* Try an output stream. */
    sprintf(outfile, "/tmp/xpopen.%d", getpid());
    sprintf(cmd, "tr A-Z a-z >%s", outfile);
    f = xpopen(cmd, "w", &pid);
    if (f == NULL) {
	perror("xpopen");
	exit(1);
    }
    fputs("Mixed Case\n", f);
    rv = xpclose(f, 0);
    if (rv != 0) {
	fprintf(stderr, "child exited with status %d\n", rv);
	exit(1);
    }
    f = fopen(outfile, "r");
    if (f == NULL) {
	perror(outfile);
	exit(1);
    }
    while ((s = fgets(buf, sizeof(buf), f)) != NULL) {
	if (verbose) {
	    fputs(s, stdout);
	}
    }
    fclose(f);
    unlink(outfile);

    sa_malloc_leak_check();

    printf("PASS\n");
}
