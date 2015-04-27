/*
 * Copyright (c) 1994-2009, 2014 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Playback file facility for x3270
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <sys/select.h>

#define PORT		4001
#define BSIZE		16384
#define LINEDUMP_MAX	32

int port = PORT;
char *me;
static enum {
	NONE, WRONG, BASE,
	LESS, SPACE, ZERO, X, N, SPACE2, D1, D2
} pstate = NONE;
static enum {
	T_NONE, T_IAC
} tstate = T_NONE;
int fdisp = 0;

static void process(FILE *f, int s);
typedef enum {
	STEP_LINE,	/* step one line in the file */
	STEP_EOR,	/* step until IAC EOR */
	STEP_MARK	/* step until a mark (line starting with '+') */
} step_t;
static int step(FILE *f, int s, step_t type);
static int process_command(FILE *f, int s);

void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [-p port] file\n", me);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	FILE *f;
	int s;
	union {
	    struct sockaddr sa;
#if defined(AF_INET6) /*[*/
	    struct sockaddr_in6 sin6;
#else /*][*/
	    struct sockaddr_in sin;
#endif /*]*/
	} addr;
#if defined(AF_INET6) /*[*/
	int proto = AF_INET6;
#else /*][*/
	int proto = AF_INET;
#endif /*]*/
	int addrlen = sizeof(addr);
	int one = 1;
	socklen_t len;
	int flags;

	/* Parse command-line arguments */

	if ((me = strrchr(argv[0], '/')) != NULL) {
		me++;
	} else {
		me = argv[0];
	}

	while ((c = getopt(argc, argv, "p:")) != -1) {
		switch (c) {
		    case 'p':
			port = atoi(optarg);
			break;
		    default:
			usage();
		}
	}

	if (argc - optind != 1) {
		usage();
	}

	/* Open the file. */
	f = fopen(argv[optind], "r");
	if (f == NULL) {
		perror(argv[optind]);
		exit(1);
	}

	/* Listen on a socket. */
	s = socket(proto, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		exit(1);
	}
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
	    sizeof(one)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	(void) memset(&addr, '\0', sizeof(addr));
	addr.sa.sa_family = proto;
#if defined(AF_INET6) /*[*/
	addr.sin6.sin6_port = htons(port);
#else /*][*/
	addr.sin.sin_port = htons(port);
#endif /*]*/
	if (bind(s, &addr.sa, addrlen) < 0) {
		perror("bind");
		exit(1);
	}
	if (listen(s, 1) < 0) {
		perror("listen");
		exit(1);
	}
	if ((flags = fcntl(s, F_GETFL)) < 0) {
		perror("fcntl(F_GETFD)");
		exit(1);
	}

	if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("fcntl(F_SETFD)");
		exit(1);
	}
	(void) signal(SIGPIPE, SIG_IGN);

	/* Accept connections and process them. */
	for (;;) {
		int s2;
#if defined(AF_INET6) /*[*/
		char buf[INET6_ADDRSTRLEN];
#endif /*]*/

		(void) memset((char *)&addr, '\0', sizeof(addr));

		addr.sa.sa_family = proto;
		len = addrlen;
		(void) printf("Waiting for connection on port %u.\n", port);
		for (;;) {
			fd_set rfds;
			int ns;

			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			FD_SET(s, &rfds);

			printf("playback> ");
			fflush(stdout);
			ns = select(s + 1, &rfds, NULL, NULL, NULL);
			if (ns < 0) {
				perror("select");
				exit(1);
			}
			if (FD_ISSET(0, &rfds)) {
				process_command(NULL, -1);
			}
			if (FD_ISSET(s, &rfds)) {
				break;
			}
		}
		s2 = accept(s, &addr.sa, &len);
		if (s2 < 0) {
			perror("accept");
			continue;
		}
		(void) printf("\nConnection from %s, port %u.\n",
#if defined(AF_INET6) /*[*/
		    inet_ntop(proto, &addr.sin6.sin6_addr, buf,
			INET6_ADDRSTRLEN) +
			 (IN6_IS_ADDR_V4MAPPED(&addr.sin6.sin6_addr)? 7: 0),
		    ntohs(addr.sin6.sin6_port)
#else /*][*/
		    inet_ntoa(addr.sin.sin_addr),
		    ntohs(addr.sin.sin_port)
#endif /*]*/
		);
		rewind(f);
		pstate = BASE;
		fdisp = 0;
		process(f, s2);
	}
}

/*
 * Process a command on stdin.
 *
 * f is NULL and s is -1 if we are not connected.
 *
 * Returns 0 for no change, -1 to stop processing the file.
 */
static int
process_command(FILE *f, int s)
{
	char buf[BUFSIZ];
	size_t sl;
	char *t;

	if (fgets(buf, BUFSIZ, stdin) == NULL) {
		printf("\n");
		exit(0);
	}
	sl = strlen(buf);
	if (sl > 0 && buf[sl - 1] == '\n') {
		buf[sl - 1] = '\0';
	}
	t = buf;
	while (*t == ' ') {
		t++;
	}
	if (!*t) {
		return 0;
	}

	if (!strncmp(t, "s", 1)) {		/* step line */
		if (f == NULL) {
			printf("Not connected.\n");
			return 0;
		}
		if (!step(f, s, STEP_LINE)) {
			return -1;
		}
	} else if (!strncmp(t, "r", 1)) {	/* step record */
		if (f == NULL) {
			printf("Not connected.\n");
			return 0;
		}
		if (!step(f, s, STEP_EOR)) {
			return -1;
		}
	} else if (!strncmp(t, "t", 1)) {	/* to mark */
		if (f == NULL) {
			printf("Not connected.\n");
			return 0;
		}
		if (!step(f, s, STEP_MARK)) {
			return -1;
		}
	} else if (!strncmp(t, "e", 1)) {	/* to EOF */
		if (f == NULL) {
			printf("Not connected.\n");
			return 0;
		}
		while (step(f, s, STEP_EOR)) {
			usleep(1000000 / 4);
		}
		return -1;
	} else if (!strncmp(t, "q", 1)) {	/* quit */
		exit(0);
	} else if (!strncmp(t, "d", 1)) {	/* disconnect */
		if (f == NULL) {
			printf("Not connected.\n");
			return 0;
		}
		return -1;
	} else if (t[0] == '?' || t[0] == 'h') {
		(void) printf("\
s: step line\n\
r: step record\n\
t: to mark\n\
e: play to EOF\n\
q: quit\n\
d: disconnect\n\
?: help\n");
	} else {
		printf("%c? Use '?' for help.\n", *t);
	}

	return 0;
}

/* Trace data from the host or emulator. */
void
trace_netdata(char *direction, unsigned char *buf, int len)
{
	int offset;

	printf("\n");
	for (offset = 0; offset < len; offset++) {
		if (!(offset % LINEDUMP_MAX))
			(void) printf("%s%s 0x%-3x ",
			    (offset ? "\n" : ""), direction, offset);
		(void) printf("%02x", buf[offset]);
	}
	(void) printf("\n");
}

/*
 * Process commands until a file is exhausted or we get a 'quit' command or
 * EOF.
 */
static void
process(FILE *f, int s)
{
	char buf[BSIZE];

	/* Loop, looking for keyboard input or emulator response. */
	for (;;) {
		fd_set rfds;
		int ns;

		(void) printf("playback> ");
		(void) fflush(stdout);

		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		FD_SET(0, &rfds);
		ns = select(s+1, &rfds, NULL, NULL, NULL);
		if (ns < 0) {
			perror("select");
			exit(1);
		}
		if (ns == 0) {
			continue;
		}
		if (FD_ISSET(s, &rfds)) {
			int nr;

			nr = read(s, buf, BSIZE);
			if (nr < 0) {
				perror("read");
				break;
			}
			if (nr == 0) {
				(void) printf("Emulator disconnected.\n");
				break;
			}
			trace_netdata("emul", (unsigned char *)buf, nr);
		}
		if (FD_ISSET(0, &rfds)) {
			if (process_command(f, s) < 0) {
				break;
			}
		}
	}

	(void) close(s);
	pstate = NONE;
	tstate = T_NONE;
	fdisp = 0;
	return;
}

/*
 * Step through the file.
 *
 * Returns 0 for EOF, nonzeo otherwise.
 */
static int
step(FILE *f, int s, step_t type)
{
	int c = 0;
	static int d1;
	static char hexes[] = "0123456789abcdef";
#	define isxd(c) strchr(hexes, c)
	static int again = 0;
	char obuf[BSIZE];
	char *cp = obuf;
	int at_mark = 0;
	int stop_eor = 0;
#	define NO_FDISP { if (fdisp) { printf("\n"); fdisp = 0; } }

    top:
	while (again || ((c = fgetc(f)) != EOF)) {
		if (c == '\r') {
			continue;
		}
		if (!again) {
			if (!fdisp || c == '\n') {
				printf("\nfile ");
				fdisp = 1;
			}
			if (c != '\n') {
				putchar(c);
			}
		}
		again = 0;
		switch (pstate) {
		    case NONE:
			assert(pstate != NONE);
			break;
		    case WRONG:
			if (c == '\n') {
				pstate = BASE;
			}
			break;
		    case BASE:
			if (c == '+' && (type == STEP_MARK)) {
				/* Hit the mark. */
				at_mark = 1;
				goto run_it;
			}
			if (c == '<') {
				pstate = LESS;
			} else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case LESS:
			if (c == ' ') {
				pstate = SPACE;
			} else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case SPACE:
			if (c == '0') {
				pstate = ZERO;
			} else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case ZERO:
			if (c == 'x') {
				pstate = X;
			} else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case X:
			if (isxd(c)) {
				pstate = N;
			} else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case N:
			if (isxd(c)) {
				pstate = N;
			} else if (c == ' ' || c == '\t') {
				pstate = SPACE2;
			} else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case SPACE2:
			if (isxd(c)) {
				d1 = strchr(hexes, c) - hexes;
				pstate = D1;
				cp = obuf;
			} else if (c == ' ' || c == '\t') {
				pstate = SPACE2;
			} else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case D1:
			if (isxd(c)) {
				int at_eor = 0;

				*cp = ((d1*16)+(strchr(hexes,c)-hexes));
				pstate = D2;
				switch (tstate) {
				    case T_NONE:
					if (*(unsigned char *)cp == IAC) {
					    tstate = T_IAC;
					}
					break;
				    case T_IAC:
					if (*(unsigned char *)cp == EOR &&
					    type == STEP_EOR) {
						at_eor = 1;
					}
					tstate = T_NONE;
					break;
				}
				cp++;
				if (at_eor && type == STEP_EOR) {
				    	stop_eor = 1;
				}
				if (at_eor || (cp - obuf >= BUFSIZ)) {
					goto run_it;
				}
			} else {
				NO_FDISP;
				(void) printf("Non-hex char '%c' in playback "
					"file, skipping to newline.", c);
				pstate = WRONG;
				again = 1;
			}
			break;
		    case D2:
			if (isxd(c)) {
				d1 = strchr(hexes, c) - hexes;
				pstate = D1;
			} else if (c == '\n') {
				pstate = BASE;
				goto run_it;
			} else {
				NO_FDISP;
				(void) printf("Non-hex char '%c' in playback "
					"file, skipping to newline.", c);
				pstate = WRONG;
				again = 1;
			}
			break;
		}
	}
	goto done;

    run_it:
	NO_FDISP;
	trace_netdata("host", (unsigned char *)obuf, cp - obuf);
	if (write(s, obuf, cp - obuf) < 0) {
		perror("socket write");
		return 0;
	}

	if (type == STEP_EOR && !stop_eor) {
		cp = obuf;
		goto top;
	}
	if (type == STEP_MARK && !at_mark) {
		cp = obuf;
		goto top;
	}
	return 1;

    done:
	if (c == EOF) {
		NO_FDISP;
		(void) printf("Playback file EOF.\n");
	}

	return 0;
}
