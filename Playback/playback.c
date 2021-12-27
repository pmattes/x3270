/*
 * Copyright (c) 1994-2009, 2014, 2019, 2021 Paul Mattes.
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
#include <stdbool.h>
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
bool fdisp = false;

static void process(FILE *f, int s);
typedef enum {
    STEP_LINE,	/* step one line in the file */
    STEP_EOR,	/* step until IAC EOR */
    STEP_MARK,	/* step until a mark (line starting with '+') */
    STEP_BIDIR,	/* step bidirectionally */
} step_t;
static bool step(FILE *f, int s, step_t type);
static int process_command(FILE *f, int s);
void trace_netdata(char *direction, unsigned char *buf, int len);

void
usage(void)
{
    fprintf(stderr, "usage: %s [-b] [-w] [-p port] file\n", me);
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
    bool bidir = false;
    bool wait = false;

    /* Parse command-line arguments */
    if ((me = strrchr(argv[0], '/')) != NULL) {
	    me++;
    } else {
	    me = argv[0];
    }

    while ((c = getopt(argc, argv, "bwp:")) != -1) {
	switch (c) {
	case 'b':
	    bidir = true;
	    break;
	case 'w':
	    wait = true;
	    break;
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
    memset(&addr, '\0', sizeof(addr));
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
    signal(SIGPIPE, SIG_IGN);

    /* Accept connections and process them. */
    for (;;) {
	int s2;
#if defined(AF_INET6) /*[*/
	char buf[INET6_ADDRSTRLEN];
#endif /*]*/

	memset((char *)&addr, '\0', sizeof(addr));

	addr.sa.sa_family = proto;
	len = addrlen;
	printf("Waiting for connection on port %u.\n", port);
	for (;;) {
	    fd_set rfds;
	    int ns;

	    FD_ZERO(&rfds);
	    if (!wait && !bidir) {
		FD_SET(0, &rfds);
	    }
	    FD_SET(s, &rfds);

	    if (!bidir) {
		printf("playback> ");
		fflush(stdout);
	    }
	    ns = select(s + 1, &rfds, NULL, NULL, NULL);
	    if (ns < 0) {
		perror("select");
		exit(2);
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
	printf("\nConnection from %s, port %u.\n",
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
	wait = false;
	rewind(f);
	pstate = BASE;
	fdisp = false;
	if (bidir) {
	    while (step(f, s2, STEP_BIDIR)) {
	    }
	    exit(0); /* needs to be smarter */
	} else {
	    process(f, s2);
	}
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
    int rx = 0;
    char buf[BUFSIZ];
    char *t;

    /* Get the input line, a character at a time. */
    while (true) {
	ssize_t nr;
	char c;

	nr = read(0, &c, 1);
	if (nr <= 0) {
	    printf("\n");
	    exit(0);
	}
	if (c == '\r') {
	    continue;
	}
	if (c == '\n') {
	    buf[rx] = '\0';
	    break;
	}
	if (rx < BUFSIZ - 1) {
	    buf[rx++] = c;
	}
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
	printf("Stepping one line\n");
	fflush(stdout);
	if (!step(f, s, STEP_LINE)) {
	    return -1;
	}
    } else if (!strncmp(t, "r", 1)) {	/* step record */
	if (f == NULL) {
	    printf("Not connected.\n");
	    return 0;
	}
	printf("Stepping to EOR\n");
	fflush(stdout);
	if (!step(f, s, STEP_EOR)) {
	    return -1;
	}
    } else if (!strncmp(t, "m", 1)) {	/* to mark */
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
	printf("Stepping to EOF\n");
	fflush(stdout);
	while (step(f, s, STEP_EOR)) {
	    usleep(1000000 / 4);
	}
	return -1;
    } else if (!strncmp(t, "c", 1)) {	/* comment */
	printf("Comment: %s\n", t);
	fflush(stdout);
    } else if (!strncmp(t, "t", 1)) {	/* timing mark */
	if (s >= 0) {
	    static unsigned char tm[] = { 0xff, 0xfd, 0x06 };

	    printf("Timing mark\n");
	    fflush(stdout);
	    if (send(s, tm, sizeof(tm), 0) < 0) {
		perror("send");
		exit(1);
	    }
	    trace_netdata("host", tm, sizeof(tm));
	} else {
	    printf("Not connected.\n");
	    fflush(stdout);
	}
    } else if (!strncmp(t, "q", 1)) {	/* quit */
	exit(0);
    } else if (!strncmp(t, "d", 1)) {	/* disconnect */
	if (f == NULL) {
	    printf("Not connected.\n");
	    return 0;
	}
	return -1;
    } else if (t[0] == '?' || t[0] == 'h') {
	printf("\
s: step line\n\
r: step record\n\
m: to mark\n\
e: play to EOF\n\
c: comment\n\
t: send TM to emulator\n\
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

    for (offset = 0; offset < len; offset++) {
	if (!(offset % LINEDUMP_MAX)) {
	    printf("%s%s 0x%-3x ", (offset ? "\n" : ""), direction, offset);
	}
	printf("%02x", buf[offset]);
    }
    printf("\n");
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

	printf("playback> ");
	fflush(stdout);

	FD_ZERO(&rfds);
	FD_SET(s, &rfds);
	FD_SET(0, &rfds);
	ns = select(s + 1, &rfds, NULL, NULL, NULL);
	if (ns < 0) {
	    perror("select");
	    exit(2);
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
		printf("\nEmulator disconnected.\n");
		break;
	    }
	    printf("\n");
	    trace_netdata("emul", (unsigned char *)buf, nr);
	    fdisp = false;
	}
	if (FD_ISSET(0, &rfds)) {
	    if (process_command(f, s) < 0) {
		break;
	    }
	}
    }

    close(s);
    pstate = NONE;
    tstate = T_NONE;
    fdisp = false;
    return;
}

/*
 * Step through the file.
 *
 * Returns false for EOF or error, true otherwise.
 */
static bool
step(FILE *f, int s, step_t type)
{
    int c = 0;
    static int d1;
    static char hexes[] = "0123456789abcdef";
#   define isxd(c) strchr(hexes, c)
    static bool again = false;
    char obuf[BSIZE];
    char *cp = obuf;
    bool at_mark = false;
    bool stop_eor = false;
    enum { FROM_HOST = 0, FROM_EMUL = 1 } direction = FROM_HOST;
    static char dchars[] = { '<', '>' };
    char dchar = dchars[direction];
    char other_dchar = dchars[!direction];
#   define NO_FDISP { if (fdisp) { printf("\n"); fdisp = false; } }

top:
    while (again || ((c = fgetc(f)) != EOF)) {
	if (c == '\r') {
	    continue;
	}
	if (!again) {
	    if (!fdisp || c == '\n') {
		printf("\nfile ");
		fdisp = true;
	    }
	    if (c != '\n') {
		putchar(c);
	    }
	}
	again = false;
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
	    if (c == '+' && type == STEP_MARK) {
		/* Hit the mark. */
		at_mark = true;
		goto run_it;
	    }
	    if (c == dchar) {
		pstate = LESS;
	    } else if (type == STEP_BIDIR && c == other_dchar) {
		NO_FDISP;
		printf("Switching direction\n");
		pstate = LESS;
		direction = !direction;
		dchar = dchars[direction];
		other_dchar = dchars[!direction];
	    } else {
		pstate = WRONG;
		again = true;
	    }
	    break;
	case LESS:
	    if (c == ' ') {
		pstate = SPACE;
	    } else {
		pstate = WRONG;
		again = true;
	    }
	    break;
	case SPACE:
	    if (c == '0') {
		pstate = ZERO;
	    } else {
		pstate = WRONG;
		again = true;
	    }
	    break;
	case ZERO:
	    if (c == 'x') {
		pstate = X;
	    } else {
		pstate = WRONG;
		again = true;
	    }
	    break;
	case X:
	    if (isxd(c)) {
		pstate = N;
	    } else {
		pstate = WRONG;
		again = true;
	    }
	    break;
	case N:
	    if (isxd(c)) {
		pstate = N;
	    } else if (c == ' ' || c == '\t') {
		pstate = SPACE2;
	    } else {
		pstate = WRONG;
		again = true;
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
		again = true;
	    }
	    break;
	case D1:
	    if (isxd(c)) {
		bool at_eor = false;

		*cp = ((d1*16)+(strchr(hexes,c)-hexes));
		pstate = D2;
		switch (tstate) {
		case T_NONE:
		    if (*(unsigned char *)cp == IAC) {
			tstate = T_IAC;
		    }
		    break;
		case T_IAC:
		    if (*(unsigned char *)cp == EOR && type == STEP_EOR) {
			at_eor = true;
		    }
		    tstate = T_NONE;
		    break;
		}
		cp++;
		if (at_eor && type == STEP_EOR) {
		    stop_eor = true;
		}
		if (at_eor || (cp - obuf >= BUFSIZ)) {
		    goto run_it;
		}
	    } else {
		NO_FDISP;
		printf("Non-hex char '%c' in playback file, skipping to "
			"newline.", c);
		pstate = WRONG;
		again = true;
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
		printf("Non-hex char '%c' in playback file, skipping to "
			"newline.", c);
		pstate = WRONG;
		again = true;
	    }
	    break;
	}
    }
    goto done;

run_it:
    NO_FDISP;
    if (type != STEP_BIDIR || direction == FROM_HOST) {
	trace_netdata("host", (unsigned char *)obuf, cp - obuf);
	if (write(s, obuf, cp - obuf) < 0) {
	    perror("socket write");
	    return false;
	}

	if (type == STEP_EOR && !stop_eor) {
	    cp = obuf;
	    goto top;
	}
    }

    if (type == STEP_BIDIR && direction == FROM_EMUL && (cp != obuf)) {
	char ibuf[BSIZE];
	ssize_t nr;
	ssize_t n2r = cp - obuf;
	size_t offset = 0;

	/* Match input from the emulator. */
	/* XXX: Probably need a timeout here. */
	while (n2r > 0) {
	    printf("Waiting for %u bytes from emulator\n", (unsigned)n2r);
	    fflush(stdout);
	    nr = read(s, ibuf + offset, n2r);
	    if (nr < 0) {
		perror("socket read");
		return false;
	    }
	    if (nr == 0) {
		fprintf(stderr, "Socket EOF\n");
		return false;
	    }
	    printf("Got %u bytes from emulator\n", (unsigned)nr);
	    trace_netdata("emul", (unsigned char *)ibuf, nr);
	    offset += nr;
	    n2r -= nr;
	}
	if (memcmp(ibuf, obuf, cp - obuf)) {
	    fprintf(stderr, "Emulator data mismatch\n");
	    exit(2);
	}
	printf("Matched %u bytes from emulator\n", (unsigned)(cp - obuf));
	fflush(stdout);
    }

    if ((type == STEP_MARK && !at_mark) || type == STEP_BIDIR) {
	cp = obuf;
	goto top;
    }

    return true;

done:
    if (c == EOF) {
	NO_FDISP;
	printf("Playback file EOF.\n");
    }

    return false;
}
