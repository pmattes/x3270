/*
 * Copyright (c) 1995-2009, Paul Mattes.
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
 * Script interface utility for x3270, c3270 and s3270.
 *
 * Accesses an x3270 command stream on the file descriptors defined by the
 * environment variables X3270OUTPUT (output from x3270, input to script) and
 * X3270INPUT (input to x3270, output from script).
 *
 * Can also access a command stream via a socket, whose TCP port is defined by
 * the environment variable X3270PORT.
 */

#include "conf.h"
#if defined(_WIN32) /*[*/
#include <windows.h>
#include <io.h>
#endif /*]*/
#include <stdio.h>
#if !defined(_MSC_VER) /*[*/
#include <unistd.h>
#endif /*]*/
#if !defined(_WIN32) /*[*/
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#if defined(HAVE_SYS_SELECT_H) /*[*/
#include <sys/select.h>
#endif /*]*/
#if defined(HAVE_GETOPT_H) /*[*/
#include <getopt.h>
#endif /*]*/
#endif /*]*/

#if defined(_WIN32) /*[*/
#include "w3miscc.h"
#endif /*]*/

#define IBS	4096

#define NO_STATUS	(-1)
#define ALL_FIELDS	(-2)

extern int optind;
extern char *optarg;

static char *me;
static int verbose = 0;
static char buf[IBS];

#if !defined(_WIN32) /*[*/
static void iterative_io(int pid);
#endif /*]*/
static void single_io(int pid, unsigned short port, int fn, char *cmd);

static void
usage(void)
{
	(void) fprintf(stderr, "\
usage: %s [-v] [-S] [-s field] [-p pid] [-t port] [action[(param[,...])]]\n\
       %s -i\n", me, me);
	exit(2);
}

/* Get a file descriptor from the environment. */
static int
fd_env(const char *name)
{
	char *fdname;
	int fd;

	fdname = getenv(name);
	if (fdname == (char *)NULL) {
		(void) fprintf(stderr, "%s: %s not set in the environment\n",
				me, name);
		exit(2);
	}
	fd = atoi(fdname);
	if (fd <= 0) {
		(void) fprintf(stderr, "%s: invalid value '%s' for %s\n", me,
		    fdname, name);
		exit(2);
	}
	return fd;
}

int
main(int argc, char *argv[])
{
	int c;
	int fn = NO_STATUS;
	char *ptr;
	int iterative = 0;
	int pid = 0;
	unsigned short port = 0;

#if defined(_WIN32) /*[*/
	if (sockstart() < 0)
	    	exit(1);
#endif /*]*/

	/* Identify yourself. */
	if ((me = strrchr(argv[0], '/')) != (char *)NULL)
		me++;
	else
		me = argv[0];

	/* Parse options. */
	while ((c = getopt(argc, argv, "ip:s:St:v")) != -1) {
		switch (c) {
#if !defined(_WIN32) /*[*/
		    case 'i':
			if (fn >= 0)
				usage();
			iterative++;
			break;
		    case 'p':
			pid = (int)strtoul(optarg, &ptr, 0);
			if (ptr == optarg || *ptr != '\0' || pid <= 0) {
				(void) fprintf(stderr,
				    "%s: Invalid process ID: '%s'\n", me,
				    optarg);
				usage();
			}
			break;
#endif /*]*/
		    case 's':
			if (fn >= 0 || iterative)
				usage();
			fn = (int)strtol(optarg, &ptr, 0);
			if (ptr == optarg || *ptr != '\0' || fn < 0) {
				(void) fprintf(stderr,
				    "%s: Invalid field number: '%s'\n", me,
				    optarg);
				usage();
			}
			break;
		    case 'S':
			if (fn >= 0 || iterative)
				usage();
			fn = ALL_FIELDS;
			break;
		    case 't':
			port = (unsigned short)strtoul(optarg, &ptr, 0);
			if (ptr == optarg || *ptr != '\0' || port <= 0) {
				(void) fprintf(stderr,
				    "%s: Invalid port: '%s'\n", me,
				    optarg);
				usage();
			}
			break;
		    case 'v':
			verbose++;
			break;
		    default:
			usage();
			break;
		}
	}

	/* Validate positional arguments. */
	if (optind == argc) {
		/* No positional arguments. */
		if (fn == NO_STATUS && !iterative)
			usage();
	} else {
		/* Got positional arguments. */
		if (iterative)
			usage();
	}
	if (pid && port)
	    	usage();

#if !defined(_WIN32) /*[*/
	/* Ignore broken pipes. */
	(void) signal(SIGPIPE, SIG_IGN);
#endif /*]*/

	/* Do the I/O. */
#if !defined(_WIN32) /*[*/
	if (iterative)
		iterative_io(pid);
	else
#endif /*]*/
		single_io(pid, port, fn, argv[optind]);
	return 0;
}

#if !defined(_WIN32) /*[*/
/* Connect to a Unix-domain socket. */
static int
usock(int pid)
{
	struct sockaddr_un ssun;
	int fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(2);
	}
	(void) memset(&ssun, '\0', sizeof(struct sockaddr_un));
	ssun.sun_family = AF_UNIX;
	(void) sprintf(ssun.sun_path, "/tmp/x3sck.%d", pid);
	if (connect(fd, (struct sockaddr *)&ssun, sizeof(ssun)) < 0) {
		perror("connect");
		exit(2);
	}
	return fd;
}
#endif /*]*/

/* Connect to a TCP socket. */
static int
tsock(unsigned short port)
{
	struct sockaddr_in sin;
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
#if defined(_WIN32) /*[*/
	    	fprintf(stderr, "socket: %s\n",
			win32_strerror(GetLastError()));
#else /*][*/
		perror("socket");
#endif /*]*/
		exit(2);
	}
	(void) memset(&sin, '\0', sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
#if defined(_WIN32) /*[*/
	    	fprintf(stderr, "connect: %s\n",
			win32_strerror(GetLastError()));
#else /*][*/
		perror("connect");
#endif /*]*/
		exit(2);
	}
	return fd;
}

/* Do a single command, and interpret the results. */
static void
single_io(int pid, unsigned short port, int fn, char *cmd)
{
    	char *port_env;
	int infd;
	int outfd;
	char status[IBS] = "";
	int nr;
	int xs = -1;
	int nw = 0;
	char rbuf[IBS];
	int sl = 0;
	int done = 0;

	/* Verify the environment and open files. */
#if !defined(_WIN32) /*[*/
	if (pid) {
		infd = outfd = usock(pid);
	} else
#endif /*]*/
	if (port) {
		infd = outfd = tsock(port);
	} else if ((port_env = getenv("X3270PORT")) != NULL) {
	    	infd = outfd = tsock(atoi(port_env));
	} else {
		infd  = fd_env("X3270OUTPUT");
		outfd = fd_env("X3270INPUT");
	}
	if (infd < 0) {
		perror("x3270if: input: fdopen");
		exit(2);
	}
	if (outfd < 0) {
		perror("x3270if: output: fdopen");
		exit(2);
	}

	/* Speak to x3270. */
	if (verbose)
		(void) fprintf(stderr, "i+ out %s\n",
		    (cmd != NULL) ? cmd : "");

	if (pid || port) {
	    	if (cmd != NULL)
			nw = send(outfd, cmd, strlen(cmd), 0);
		if (nw >= 0)
		    	nw = send(outfd, "\n", 1, 0);
	} else {
	    	if (cmd != NULL)
			nw = write(outfd, cmd, strlen(cmd));
		if (nw >= 0)
		    	nw = write(outfd, "\n", 1);
	}
	if (nw < 0) {
	    	if (pid || port)
#if defined(_WIN32) /*[*/
		    	fprintf(stderr, "x3270if: send: %s\n",
				win32_strerror(GetLastError()));
#else /*][*/
		    	perror("x3270if: send");
#endif /*]*/
		else
		    	perror("x3270if: write");
		exit(2);
	}

	/* Get the answer. */
	while (!done &&
		(nr = ((pid || port)? recv(infd, rbuf, IBS, 0):
				      read(infd, rbuf, IBS))) > 0) {
	    	int i;
		int get_more = 0;

		i = 0;
		do {
			/* Copy from rbuf into buf until '\n'. */
		    	while (i < nr && rbuf[i] != '\n') {
				if (sl < IBS - 1)
					buf[sl++] = rbuf[i++];
			}
			if (rbuf[i] == '\n')
			    	i++;
			else {
			    	/* Go get more input. */
			    	get_more = 1;
				break;
			}

			/* Process one line of output. */
			buf[sl] = '\0';

			if (verbose)
				(void) fprintf(stderr, "i+ in %s\n", buf);
			if (!strcmp(buf, "ok")) {
				(void) fflush(stdout);
				xs = 0;
				done = 1;
				break;
			} else if (!strcmp(buf, "error")) {
				(void) fflush(stdout);
				xs = 1;
				done = 1;
				break;
			} else if (!strncmp(buf, "data: ", 6)) {
				if (printf("%s\n", buf + 6) < 0) {
					perror("x3270if: printf");
					exit(2);
				}
			} else
				(void) strcpy(status, buf);

			/* Get ready for the next. */
			sl = 0;
		} while (i < nr);

		if (get_more) {
		    	get_more = 0;
			continue;
		}
	}
	if (nr < 0) {
	    	if (pid || port)
#if defined(_WIN32) /*[*/
			fprintf(stderr, "x3270if: recv: %s\n",
				win32_strerror(GetLastError()));
#else /*][*/
			perror("recv");
#endif /*]*/
		else
			perror("read");
		exit(2);
	} else if (nr == 0) {
	    	fprintf(stderr, "x3270if: unexpected EOF\n");
		exit(2);
	}

	if (fflush(stdout) < 0) {
		perror("x3270if: fflush");
		exit(2);
	}

	/* Print status, if that's what they want. */
	if (fn != NO_STATUS) {
		char *sf = (char *)NULL;
		char *sb = status;
		int rc;

		if (fn == ALL_FIELDS) {
			rc = printf("%s\n", status);
		} else {
			do {
				if (!fn--)
					break;
				sf = strtok(sb, " \t");
				sb = (char *)NULL;
			} while (sf != (char *)NULL);
			rc = printf("%s\n", (sf != (char *)NULL) ? sf : "");
		}
		if (rc < 0) {
			perror("x3270if: printf");
			exit(2);
		}
	}

	if (fflush(stdout) < 0) {
		perror("x3270if: fflush");
		exit(2);
	}

	exit(xs);
}

#if !defined(_WIN32) /*[*/
/* Act as a passive pipe between 'expect' and x3270. */
static void
iterative_io(int pid)
{
#	define N_IO 2
	struct {
		const char *name;
		int rfd, wfd;
		char buf[IBS];
		int offset, count;
	} io[N_IO];	/* [0] is program->x3270, [1] is x3270->program */
	fd_set rfds, wfds;
	int fd_max = 0;
	int i;

#ifdef DEBUG
	if (verbose) {
		freopen("/tmp/x3270if.dbg", "w", stderr);
		setlinebuf(stderr);
	}
#endif

	/* Get the x3270 file descriptors. */
	io[0].name = "program->x3270";
	io[0].rfd = fileno(stdin);
	if (pid)
		io[0].wfd = usock(pid);
	else
		io[0].wfd = fd_env("X3270INPUT");
	io[1].name = "x3270->program";
	if (pid)
		io[1].rfd = dup(io[0].wfd);
	else
		io[1].rfd = fd_env("X3270OUTPUT");
	io[1].wfd = fileno(stdout);
	for (i = 0; i < N_IO; i++) {
		if (io[i].rfd > fd_max)
			fd_max = io[i].rfd;
		if (io[i].wfd > fd_max)
			fd_max = io[i].wfd;
		(void) fcntl(io[i].rfd, F_SETFL,
		    fcntl(io[i].rfd, F_GETFL, 0) | O_NDELAY);
		(void) fcntl(io[i].wfd, F_SETFL,
		    fcntl(io[i].wfd, F_GETFL, 0) | O_NDELAY);
		io[i].offset = 0;
		io[i].count = 0;
	}
	fd_max++;

	for (;;) {
		int rv;

		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		for (i = 0; i < N_IO; i++) {
			if (io[i].count) {
				FD_SET(io[i].wfd, &wfds);
#ifdef DEBUG
				if (verbose)
					(void) fprintf(stderr,
					    "enabling output %s %d\n",
					    io[i].name, io[i].wfd);
#endif
			} else {
				FD_SET(io[i].rfd, &rfds);
#ifdef DEBUG
				if (verbose)
					(void) fprintf(stderr,
					    "enabling input %s %d\n",
					    io[i].name, io[i].rfd);
#endif
			}
		}

		if ((rv = select(fd_max, &rfds, &wfds, (fd_set *)NULL,
				(struct timeval *)NULL)) < 0) {
			perror("x3270if: select");
			exit(2);
		}
		if (verbose)
			(void) fprintf(stderr, "select->%d\n", rv);

		for (i = 0; i < N_IO; i++) {
			if (io[i].count) {
				if (FD_ISSET(io[i].wfd, &wfds)) {
					rv = write(io[i].wfd,
					    io[i].buf + io[i].offset,
					    io[i].count);
					if (rv < 0) {
						(void) fprintf(stderr,
						    "x3270if: write(%s): %s",
						    io[i].name,
						    strerror(errno));
						exit(2);
					}
					io[i].offset += rv;
					io[i].count -= rv;
#ifdef DEBUG
					if (verbose)
						(void) fprintf(stderr,
						    "write(%s)->%d\n",
						    io[i].name, rv);
#endif
				}
			} else if (FD_ISSET(io[i].rfd, &rfds)) {
				rv = read(io[i].rfd, io[i].buf, IBS);
				if (rv < 0) {
					(void) fprintf(stderr,
					    "x3270if: read(%s): %s",
					    io[i].name, strerror(errno));
					exit(2);
				}
				if (rv == 0)
					exit(0);
				io[i].offset = 0;
				io[i].count = rv;
#ifdef DEBUG
				if (verbose)
					(void) fprintf(stderr,
					    "read(%s)->%d\n", io[i].name, rv);
#endif
			}
		}
	}
}
#endif /*]*/
