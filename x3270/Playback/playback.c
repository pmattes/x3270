/*
 * Copyright (c) 1994-2009, Paul Mattes.
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
 * playback file facility for x3270
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <stdlib.h>
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

extern int optind;
extern char *optarg;

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
		struct sockaddr_in sin;
#if defined(AF_INET6) /*[*/
		struct sockaddr_in6 sin6;
#endif /*]*/
	} addr;
	int addrlen = sizeof(struct sockaddr_in);
	int one = 1;
	socklen_t len;
	int proto = AF_INET;

	/* Parse command-line arguments */

	if (me = strrchr(argv[0], '/'))
		me++;
	else
		me = argv[0];

	while ((c = getopt(argc, argv, "p:x")) != -1)
		switch (c) {
		    case 'p':
			port = atoi(optarg);
			break;
#if defined(AF_INET6) /*[*/
		    case 'x':
			proto = AF_INET6;
			addrlen = sizeof(struct sockaddr_in6);
			break;
#endif /*]*/
		    default:
			usage();
		}

	if (argc - optind != 1)
		usage();

	/* Open the file. */
	f = fopen(argv[optind], "r");
	if (f == (FILE *)NULL) {
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
	if (proto == AF_INET6) {
		addr.sin6.sin6_port = htons(port);
	} else
#endif /*]*/
	{
		addr.sin.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin.sin_port = htons(port);
	}
	if (bind(s, &addr.sa, addrlen) < 0) {
		perror("bind");
		exit(1);
	}
	if (listen(s, 1) < 0) {
		perror("listen");
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
		(void) printf("Waiting for connection.\n");
		s2 = accept(s, &addr.sa, &len);
		if (s2 < 0) {
			perror("accept");
			continue;
		}
		(void) printf("Connection from %s %u.\n",
#if defined(AF_INET6) /*[*/
		    inet_ntop(proto,
			      (proto == AF_INET)?
				 (void *)&addr.sin.sin_addr:
				 (void *)&addr.sin6.sin6_addr,
			      buf, INET6_ADDRSTRLEN),
		    ntohs((proto == AF_INET)?
				addr.sin.sin_port:
				addr.sin6.sin6_port)
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

void
trace_netdata(char *direction, unsigned char *buf, int len)
{
	int offset;

	for (offset = 0; offset < len; offset++) {
		if (!(offset % LINEDUMP_MAX))
			(void) printf("%s%s 0x%-3x ",
			    (offset ? "\n" : ""), direction, offset);
		(void) printf("%02x", buf[offset]);
	}
	(void) printf("\n");
}

int
process(FILE *f, int s)
{
	char buf[BSIZE];
	int prompt = 1;

	/* Loop, looking for keyboard input or emulator response. */
	for (;;) {
		fd_set rfds;
		struct timeval t;
		int ns;

		if (prompt == 1) {
			(void) printf("playback> ");
			(void) fflush(stdout);
		}

		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		FD_SET(0, &rfds);
		t.tv_sec = 0;
		t.tv_usec = 500000;
		ns = select(s+1, &rfds, (fd_set *)NULL, (fd_set *)NULL, &t);
		if (ns < 0) {
			perror("select");
			exit(1);
		}
		if (ns == 0) {
			prompt++;
			continue;
		}
		if (FD_ISSET(s, &rfds)) {
			int nr;

			(void) printf("\n");
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
			prompt = 0;
		}
		if (FD_ISSET(0, &rfds)) {
			if (fgets(buf, BSIZE, stdin) == (char *)NULL) {
				(void) printf("\n");
				exit(0);
			}
			if (!strncmp(buf, "s", 1)) {		/* step line */
				if (!step(f, s, 0))
					break;
			} else if (!strncmp(buf, "r", 1)) {	/* step record */
				if (!step(f, s, 1))
					break;
			} else if (!strncmp(buf, "t", 1)) {	/* to mark */
				if (!step(f, s, 2))
					break;
			} else if (!strncmp(buf, "e", 1)) {	/* to EOF */
				FD_ZERO(&rfds);
				while (step(f, s, 1)) {
					t.tv_sec = 0;
					t.tv_usec = 1000000 / 4;
					(void) select(0, NULL, NULL, NULL, &t);
				}
				break;
			} else if (!strncmp(buf, "q", 1)) {	/* quit */
				exit(0);
			} else if (!strncmp(buf, "d", 1)) {	/* disconnect */
				break;
			} else if (buf[0] == '?') {
				(void) printf("\
s: step line\n\
r: step record\n\
t: to mark\n\
e: play to EOF\n\
q: quit\n\
d: disconnect\n\
?: help\n");
			} else if (buf[0] != '\n') {		/* junk */
				(void) printf("%c?\n", buf[0]);
			}
			prompt = 0;
		}
	}

	(void) close(s);
	pstate = NONE;
	tstate = T_NONE;
	fdisp = 0;
	return;
}

int
step(FILE *f, int s, int to_eor)
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
		if (c == '\r')
			continue;
		if (!again) {
			if (!fdisp || c == '\n') {
				printf("\nfile ");
				fdisp = 1;
			}
			if (c != '\n')
				putchar(c);
		}
		again = 0;
		switch (pstate) {
		    case WRONG:
			if (c == '\n')
				pstate = BASE;
			break;
		    case BASE:
			if (c == '+' && (to_eor == 2)) {
				/* Hit the mark. */
				at_mark = 1;
				goto run_it;
			}
			if (c == '<')
				pstate = LESS;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case LESS:
			if (c == ' ')
				pstate = SPACE;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case SPACE:
			if (c == '0')
				pstate = ZERO;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case ZERO:
			if (c == 'x')
				pstate = X;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case X:
			if (isxd(c))
				pstate = N;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case N:
			if (isxd(c))
				pstate = N;
			else if (c == ' ' || c == '\t')
				pstate = SPACE2;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case SPACE2:
			if (isxd(c)) {
				d1 = strchr(hexes, c) - hexes;
				pstate = D1;
				cp = obuf;
			} else if (c == ' ' || c == '\t')
				pstate = SPACE2;
			else {
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
					if (*(unsigned char *)cp == IAC)
					    tstate = T_IAC;
					break;
				    case T_IAC:
					if (*(unsigned char *)cp == EOR &&
					    (to_eor == 1))
						at_eor = 1;
					tstate = T_NONE;
					break;
				}
				cp++;
				if (at_eor && (to_eor == 1))
				    	stop_eor = 1;
				if (at_eor || (cp - obuf >= BUFSIZ))
					goto run_it;
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

	if ((to_eor == 1) && !stop_eor) {
		cp = obuf;
		goto top;
	}
	if ((to_eor == 2) && !at_mark) {
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
