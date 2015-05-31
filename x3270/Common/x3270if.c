/*
 * Copyright (c) 1995-2009, 2013-2015 Paul Mattes.
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
 * Script interface utility for x3270, c3270, wc3270, s3270 and ws3270.
 *
 * Accesses an emulator command stream in one of several different ways:
 *
 * - (Unix only) Using the file descriptors defined by the environment
 *   variables X3270OUTPUT (output from the emulator, input to script) and
 *   X3270INPUT (input to the emulator, output from script). These are
 *   automatically passed to child scripts by the Unix emulators' Script()
 *   action.
 *
 * - Using a loopback IPv4 socket whose TCP port is defined by the
 *   environment variable X3270PORT. This is automatically passed to child
 *   scripts by the Windows emulators' Script() action.
 *
 * - (Unix only) Using the Unix-domain socket /tmp/x3sck.<x3270-pid>. This
 *   socket is created by the Unix emulators' -socket option.
 *
 * - Using a loopback IPv4 socket whose TCP port is passed in explicitly.
 *   This port is bound by the emulators by the -scriptport option.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <errno.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif /*]*/

#include "w3misc.h"

#define IBS	4096

#define NO_STATUS	(-1)
#define ALL_FIELDS	(-2)

#if defined(_WIN32) /*[*/
#define DIRSEP	'\\'
#else /*][*/
#define DIRSEP '/'
#endif /*]*/

static char *me;
static int verbose = 0;
static char buf[IBS];

static void iterative_io(int pid, unsigned short port);
static void single_io(int pid, unsigned short port, int fn, char *cmd);

static void
x3270if_usage(void)
{
	(void) fprintf(stderr, "\
usage:\n\
 %s [options] \"action[(param[,...])]\"\n\
   execute the named action\n\
 %s [options] -s field\n\
   display status field 0..12\n\
 %s [options] -S\n\
   display all status fields\n\
 %s [options] -i\n\
   shuttle commands and responses between stdin/stdout and emulator\n\
options:\n\
 -v       verbose operation\n"
#if !defined(_WIN32) /*[*/
" -p pid   connect to process <pid>\n"
#endif /*]*/
" -t port  connect to TCP port <port>\n",
	    me, me, me, me);
	exit(2);
}

/* Get a file descriptor from the environment. */
static int
fd_env(const char *name)
{
	char *fdname;
	int fd;

	fdname = getenv(name);
	if (fdname == NULL) {
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
	if ((me = strrchr(argv[0], DIRSEP)) != NULL) {
		me++;
	} else {
		me = argv[0];
	}

	/* Parse options. */
	opterr = 0;
	while ((c = getopt(argc, argv, "ip:s:St:v")) != -1) {
		switch (c) {
		    case 'i':
			if (fn >= 0)
				x3270if_usage();
			iterative++;
			break;
#if !defined(_WIN32) /*[*/
		    case 'p':
			pid = (int)strtoul(optarg, &ptr, 0);
			if (ptr == optarg || *ptr != '\0' || pid <= 0) {
				(void) fprintf(stderr,
				    "%s: Invalid process ID: '%s'\n", me,
				    optarg);
				x3270if_usage();
			}
			break;
#endif /*]*/
		    case 's':
			if (fn >= 0 || iterative)
				x3270if_usage();
			fn = (int)strtol(optarg, &ptr, 0);
			if (ptr == optarg || *ptr != '\0' || fn < 0) {
				(void) fprintf(stderr,
				    "%s: Invalid field number: '%s'\n", me,
				    optarg);
				x3270if_usage();
			}
			break;
		    case 'S':
			if (fn >= 0 || iterative)
				x3270if_usage();
			fn = ALL_FIELDS;
			break;
		    case 't':
			port = (unsigned short)strtoul(optarg, &ptr, 0);
			if (ptr == optarg || *ptr != '\0' || port <= 0) {
				(void) fprintf(stderr,
				    "%s: Invalid port: '%s'\n", me,
				    optarg);
				x3270if_usage();
			}
			break;
		    case 'v':
			verbose++;
			break;
		    default:
			x3270if_usage();
			break;
		}
	}

	/* Validate positional arguments. */
	if (optind == argc) {
		/* No positional arguments. */
		if (fn == NO_STATUS && !iterative)
			x3270if_usage();
	} else {
		/* Got positional arguments. */
		if (iterative)
			x3270if_usage();
		if (argc - optind > 1) {
		    x3270if_usage();
		}
	}
	if (pid && port) {
	    	x3270if_usage();
	}

#if !defined(_WIN32) /*[*/
	/* Ignore broken pipes. */
	(void) signal(SIGPIPE, SIG_IGN);
#endif /*]*/

	/* Do the I/O. */
	if (iterative)
		iterative_io(pid, port);
	else
		single_io(pid, port, fn, argv[optind]);
	return 0;
}

#if !defined(_WIN32) /*[*/
/* Connect to a Unix-domain socket. */
static socket_t
usock(int pid)
{
	struct sockaddr_un ssun;
	socket_t fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == INVALID_SOCKET) {
		perror("socket");
		exit(2);
	}
	(void) memset(&ssun, '\0', sizeof(struct sockaddr_un));
	ssun.sun_family = AF_UNIX;
	(void) snprintf(ssun.sun_path, sizeof(ssun.sun_path), "/tmp/x3sck.%d",
		pid);
	if (connect(fd, (struct sockaddr *)&ssun, sizeof(ssun)) < 0) {
		perror("connect");
		exit(2);
	}
	return fd;
}
#endif /*]*/

/* Connect to a TCP socket. */
static socket_t
tsock(unsigned short port)
{
	struct sockaddr_in sin;
	socket_t fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == INVALID_SOCKET) {
#if defined(_WIN32) /*[*/
		win32_perror("socket");
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
	    	win32_perror("connect(%u)", port);
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
	int infd, outfd;
	socket_t insocket, outsocket;
	bool is_socket = false;
	char status[IBS] = "";
	int nr;
	int xs = -1;
	int nw = 0;
	char rbuf[IBS];
	int sl = 0;
	int done = 0;
	char *cmd_nl;
	char *wstr;

	/* Verify the environment and open files. */
#if !defined(_WIN32) /*[*/
	if (pid) {
		insocket = outsocket = usock(pid);
		is_socket = true;
	} else
#endif /*]*/
	if (port) {
		insocket = outsocket = tsock(port);
		is_socket = true;
	} else if ((port_env = getenv("X3270PORT")) != NULL) {
	    	insocket = outsocket = tsock(atoi(port_env));
		is_socket = true;
	} else {
		infd  = fd_env("X3270OUTPUT");
		outfd = fd_env("X3270INPUT");
	}
	if ((!is_socket && infd < 0) ||
		(is_socket && insocket == INVALID_SOCKET)) {
		perror("x3270if: input");
		exit(2);
	}
	if ((!is_socket && outfd < 0) ||
		(is_socket && outsocket == INVALID_SOCKET)) {
		perror("x3270if: output");
		exit(2);
	}

	/* Speak to x3270. */
	if (verbose)
		(void) fprintf(stderr, "i+ out %s\n",
		    (cmd != NULL) ? cmd : "");

	if (cmd != NULL) {
		cmd_nl = malloc(strlen(cmd) + 2);
		if (cmd_nl == NULL) {
			fprintf(stderr, "Out of memory\n");
			exit(2);
		}
		sprintf(cmd_nl, "%s\n", cmd);
		wstr = cmd_nl;
	} else {
	    	cmd_nl = NULL;
	    	wstr = "\n";
	}

	if (is_socket) {
		nw = send(outsocket, wstr, (int)strlen(wstr), 0);
	} else {
		nw = write(outfd, wstr, (int)strlen(wstr));
	}
	if (nw < 0) {
	    	if (is_socket)
#if defined(_WIN32) /*[*/
		    	win32_perror("x3270if: send");
#else /*][*/
		    	perror("x3270if: send");
#endif /*]*/
		else
		    	perror("x3270if: write");
		exit(2);
	}
	if (cmd_nl != NULL)
	    	free(cmd_nl);

	/* Get the answer. */
	while (!done &&
		(nr = (is_socket? recv(insocket, rbuf, IBS, 0):
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
	    	if (is_socket)
#if defined(_WIN32) /*[*/
			win32_perror("x3270if: recv");
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
		char *sf = NULL;
		char *sb = status;
		int rc;

		if (fn == ALL_FIELDS) {
			rc = printf("%s\n", status);
		} else {
			do {
				if (!fn--)
					break;
				sf = strtok(sb, " \t");
				sb = NULL;
			} while (sf != NULL);
			rc = printf("%s\n", (sf != NULL) ? sf : "");
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

	if (is_socket) {
	    	shutdown(insocket, 2);
#if defined(_WIN32) /*[*/
		closesocket(insocket);
#else /*][*/
		close(insocket);
#endif /*]*/
	}

	exit(xs);
}

#if !defined(_WIN32) /*[*/

/* Act as a passive pipe to the emulator. */
static void
iterative_io(int pid, unsigned short port)
{
#	define N_IO 2
	struct {
		const char *name;
		int rfd, wfd;
		char buf[IBS];
		int offset, count;
	} io[N_IO];	/* [0] is script->emulator, [1] is emulator->script */
	fd_set rfds, wfds;
	int fd_max = 0;
	int i;
	char *port_env;

#ifdef DEBUG
	if (verbose) {
		freopen("/tmp/x3270if.dbg", "w", stderr);
		setlinebuf(stderr);
	}
#endif

	/* Get the x3270 file descriptors. */
	io[0].name = "script->emulator";
	io[0].rfd = fileno(stdin);
#if !defined(_WIN32) /*[*/
	if (pid)
		io[0].wfd = usock(pid);
	else
#endif /*]*/
	if (port) {
		io[0].wfd = tsock(port);
	} else if ((port_env = getenv("X3270PORT")) != NULL) {
		io[0].wfd = tsock(atoi(port_env));
	} else {
		io[0].wfd = fd_env("X3270INPUT");
	}
	io[1].name = "emulator->script";
	if (pid || port || (port_env != NULL)) {
		io[1].rfd = dup(io[0].wfd);
	} else {
		io[1].rfd = fd_env("X3270OUTPUT");
	}
	io[1].wfd = fileno(stdout);
	for (i = 0; i < N_IO; i++) {
		if (io[i].rfd > fd_max)
			fd_max = io[i].rfd;
		if (io[i].wfd > fd_max)
			fd_max = io[i].wfd;
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

		if ((rv = select(fd_max, &rfds, &wfds, NULL, NULL)) < 0) {
			perror("x3270if: select");
			exit(2);
		}
		if (verbose) {
			(void) fprintf(stderr, "select->%d\n", rv);
		}

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
					if (verbose) {
						(void) fprintf(stderr,
						    "write(%s)->%d\n",
						    io[i].name, rv);
					}
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
				if (verbose) {
					(void) fprintf(stderr,
					    "read(%s)->%d\n", io[i].name, rv);
				}
#endif
			}
		}
	}
}

#else /*][*/

static HANDLE stdin_thread;
static HANDLE stdin_enable_event, stdin_done_event;
static char stdin_buf[256];
static int stdin_nr;
static int stdin_error;

/*
 * stdin input thread
 *
 * Endlessly:
 * - waits for stdin_enable_event
 * - reads from stdin
 * - leaves the input in stdin_buf and the length in stdin_nr
 * - sets stdin_done_event
 *
 * If there is a read error, leaves -1 in stdin_nr and a Windows error code in
 * stdin_error.
 */
static DWORD WINAPI
stdin_read(LPVOID lpParameter)
{
	for (;;) {
		DWORD rv;

		rv = WaitForSingleObject(stdin_enable_event, INFINITE);
		switch (rv) {
		case WAIT_ABANDONED:
		case WAIT_TIMEOUT:
		case WAIT_FAILED:
			stdin_nr = -1;
			stdin_error = GetLastError();
			SetEvent(stdin_done_event);
			break;
		case WAIT_OBJECT_0:
			stdin_nr = read(0, stdin_buf, sizeof(stdin_buf));
			if (stdin_nr < 0) {
				stdin_error = GetLastError();
			}
			SetEvent(stdin_done_event);
			break;
		}
	}
	return 0;
}

/* Act as a passive pipe to the emulator. */
static void
iterative_io(int pid, unsigned short port)
{
	char *port_env;
    	socket_t s;
	struct sockaddr_in sin;
	HANDLE socket_event;
	HANDLE ha[2];
	DWORD ret;
	char buf[1024];
	int nr;

	if (!port) {
		port_env = getenv("X3270PORT");
		if (port_env == NULL) {
			fprintf(stderr, "Must specify port or put port in "
				"X3270PORT.\n");
			exit(2);
		}
		port = atoi(port_env);
		if (port <= 0 || (port & ~0xffff)) {
			fprintf(stderr, "Invalid X3270PORT.\n");
			exit(2);
		}
	}

	/* Open the socket. */
	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "socket failed: error 0x%x\n",
			(unsigned)WSAGetLastError());
		exit(2);
	}
	memset(&sin, '\0', sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		win32_perror("connect(%u) failed", port);
		exit(2);
	}
	if (verbose) {
		fprintf(stderr, "<connected to port %d>\n", port);
	}
	socket_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (socket_event == NULL) {
		win32_perror("CreateEvent failed");
		exit(2);
	}
	if (WSAEventSelect(s, socket_event, FD_READ|FD_CLOSE) != 0) {
		win32_perror("WSAEventSelect failed");
		exit(2);
	}

	/* Create a thread to read data from the socket. */
	stdin_enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	stdin_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	stdin_thread = CreateThread(NULL, 0, stdin_read, NULL, 0, NULL);
	if (stdin_thread == NULL) {
		win32_perror("CreateThread failed");
		exit(2);
	}
	SetEvent(stdin_enable_event);

	ha[0] = socket_event;
	ha[1] = stdin_done_event;
	for (;;) {
		ret = WaitForMultipleObjects(2, ha, FALSE, INFINITE);
		switch (ret) {
		case WAIT_OBJECT_0: /* socket input */
			nr = recv(s, buf, sizeof(buf), 0);
			if (verbose) {
				fprintf(stderr, "<%d byte%s from socket>\n",
					nr, (nr == 1)? "": "s");
			}
			if (nr < 0) {
				win32_perror("recv failed");
				exit(2);
			}
			if (nr == 0) {
				exit(0);
			}
			fwrite(buf, 1, nr, stdout);
			break;
		case WAIT_OBJECT_0 + 1: /* stdin input */
			if (verbose) {
				fprintf(stderr, "<%d byte%s from stdin>\n",
					stdin_nr, (stdin_nr == 1)? "": "s");
			}
			if (stdin_nr < 0) {
				fprintf(stderr, "stdin read failed: %s\n",
					win32_strerror(stdin_error));
				exit(2);
			}
			if (stdin_nr == 0) {
				exit(0);
			}
			(void) send(s, stdin_buf, stdin_nr, 0);
			SetEvent(stdin_enable_event);
			break;
		case WAIT_FAILED:
			win32_perror("WaitForMultipleObjects failed");
			exit(2);
		default:
			fprintf(stderr, "Unexpected return %d from "
				"WaitForMultipleObjects\n", (int)ret);
			exit(2);
		}
	}
}

#endif /*]*/
