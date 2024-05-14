/*
 * Copyright (c) 2018-2024 Paul Mattes.
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

/* Man-in-the-middle trace daemon. */

#include "globals.h"

#include <errno.h>
#if defined(HAVE_GETOPT_H) /*[*/
# include <getopt.h>		/* why isn't this necessary elsewhere? */
#endif /*]*/
#if !defined(_WIN32) /*[*/
# include <netdb.h>
# include <signal.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#else /*][*/
# include "w3misc.h"
#endif /*]*/

#include "utils.h"

#if !defined(_WIN32) /*[*/
# define DIRSEP '/'
# define sockerr(s)	perror(s)
#else /*][*/
# define DIRSEP '\\'
# define sockerr(s)	win32_perror(s)
#endif /*]*/

static char *me;
#if 0
static void sockerr(const char *s);
#endif
static void netdump(FILE *f, char direction, unsigned char *buffer,
	size_t length);

/* Usage message. */
static void
mitm_usage(void)
{
    fprintf(stderr, "Usage: %s [-p listenport] [-f outfile]\n", me);
    exit(1);
}

int
main(int argc, char *argv[])
{
    int c;
    int port = 4200;
    char *file = NULL;
    FILE *f;
    struct sockaddr_in sin, sin_a;
    socket_t s;
    socket_t a;
    socket_t o;
    socklen_t a_len;
    char buf[16384];
    size_t nr;
    char thru_host[256];
    unsigned thru_port;
    struct hostent *h;
    bool a_open = true, o_open = true;
    int on = 1;
    int connect_result;
    time_t t;

#if defined(_WIN32) /*[*/
    if (sockstart() < 0) {
	exit(__LINE__);
    }
#endif /*]*/

    /* Identify yourself. */
    if ((me = strrchr(argv[0], DIRSEP)) != NULL) {
	me++;
    } else {
	me = argv[0];
    }

    if (argc > 1 && !strcmp(argv[1], "--version")) {
	printf("%s\n", build);
	return 0;
    }

    /* Parse options. */
    opterr = 0;
    while ((c = getopt(argc, argv, "p:f:")) != -1) {
	switch (c) {
	case 'p':
	    port = atoi(optarg);
	    if (port <= 0 || port > 0xffff) {
		fprintf(stderr, "Invalid port: %s\n", optarg);
		exit(1);
	    }
	    break;
	case 'f':
	    file = optarg;
	    break;
	default:
	    mitm_usage();
	    break;
	}
    }

    /* Validate positional arguments. */
    if (optind < argc) {
	mitm_usage();
    }

    /* Open the output file. */
    if (file == NULL) {
#if !defined(_WIN32) /*[*/
	file = Asprintf("/tmp/mitm.%d", (int)getpid());
#else /*][*/
	char desktop[MAX_PATH];
	HRESULT r;

	r = SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
		SHGFP_TYPE_CURRENT, desktop);
	if (r != S_OK) {
	    fprintf(stderr, "SHGetFolderPath(DESKTOPDIRECTORY) failed: 0x%x\n",
		    (int)r);
	    exit(1);
	}
	file = Asprintf("%s\\mitm.%d.txt", desktop, (int)getpid());
#endif /*]*/
    }
    f = fopen(file, "w");
    if (f == NULL) {
	perror(file);
	exit(1);
    }
    fprintf(f, "Recorded by %s\n", build);
    t = time(NULL);
    fprintf(f, "Started %s", asctime(gmtime(&t)));

    /* Wait for a connection. */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
	sockerr("socket");
	exit(1);
    }
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
	sockerr("setsockopt");
	exit(1);
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	sockerr("bind");
	exit(1);
    }
    if (listen(s, 1) < 0) {
	sockerr("listen");
	exit(1);
    }
    memset(&sin_a, 0, sizeof(sin_a));
    sin_a.sin_family = AF_INET;
    a_len = sizeof(sin_a);
    a = accept(s, (struct sockaddr *)&sin_a, &a_len);
    if (a == INVALID_SOCKET) {
	sockerr("accept");
	exit(1);
    }
    SOCK_CLOSE(s);

    /* Read the initial request. */
    nr = recv(a, buf, sizeof(buf), 0);
    if (nr < 0) {
	sockerr("recv");
	exit(1);
    }
    if (nr == 0) {
	fprintf(stderr, "Empty connection\n");
	exit(1);
    }
    if (nr < 2) {
	fprintf(stderr, "Short read\n");
	exit(1);
    }
    if (buf[nr - 2] != '\r' || buf[nr - 1] != '\n') {
	fprintf(stderr, "Request line does not end in CR/LF\n");
	exit(1);
    }
    buf[nr - 2] = '\0';
    if (sscanf(buf, "%256s %u", thru_host, &thru_port) != 2) {
	fprintf(stderr, "Malformed request line\n");
	exit(1);
    }

    /* Connect. */
    h = gethostbyname(thru_host);
    if (h == NULL) {
	fprintf(stderr, "gethostbyname(%s) failed\n", thru_host);
    }
    o = socket(h->h_addrtype, SOCK_STREAM, 0);
    if (o == INVALID_SOCKET) {
	sockerr("socket");
	exit(1);
    }
    if (h->h_addrtype == AF_INET) {
	struct sockaddr_in sin_o;

	memset(&sin_o, 0, sizeof(sin_o));
	sin_o.sin_family = AF_INET;
	memcpy(&sin_o.sin_addr, h->h_addr_list[0], h->h_length);
	sin_o.sin_port = htons(thru_port);
	connect_result = connect(o, (struct sockaddr *)&sin_o, sizeof(sin_o));
    } else if (h->h_addrtype == AF_INET6) {
	struct sockaddr_in6 sin6_o;

	memset(&sin6_o, 0, sizeof(sin6_o));
	sin6_o.sin6_family = AF_INET6;
	memcpy(&sin6_o.sin6_addr, h->h_addr_list[0], h->h_length);
	sin6_o.sin6_port = htons(thru_port);
	connect_result = connect(o, (struct sockaddr *)&sin6_o, sizeof(sin6_o));
    } else {
	fprintf(stderr, "Unknown address type %d\n", h->h_addrtype);
	exit(1);
    }
    if (connect_result < 0) {
	sockerr("connect");
	exit(1);
    }

    /* Ignore broken pipes. */
#if !defined(_WIN32) /*[*/
    signal(SIGPIPE, SIG_IGN);
#endif /*]*/

    /* Shuffle and trace. */
    while (a_open || o_open) {
	fd_set rfds;
	int ns;
	int maxfd = 0;

	FD_ZERO(&rfds);
	if (a_open) {
	    FD_SET(a, &rfds);
	    if (a > maxfd) {
		maxfd = a;
	    }
	}
	if (o_open) {
	    FD_SET(o, &rfds);
	    if (o > maxfd) {
		maxfd = o;
	    }
	}

	ns = select(maxfd + 1, &rfds, NULL, NULL, NULL);
	if (ns < 0) {
	    sockerr("select");
	    exit(1);
	}
	if (a_open && FD_ISSET(a, &rfds)) {
	    nr = recv(a, buf, sizeof(buf), 0);
	    if (nr < 0) {
		sockerr("emulator recv");
		exit(1);
	    }
	    if (nr == 0) {
		fprintf(f, "Emulator EOF\n");
		shutdown(o, 1);
		a_open = false;
	    } else {
		netdump(f, '>', (unsigned char *)buf, nr);
		send(o, buf, nr, 0);
	    }
	}
	if (o_open && FD_ISSET(o, &rfds)) {
	    nr = recv(o, buf, sizeof(buf), 0);
	    if (nr < 0) {
		sockerr("host recv");
		exit(1);
	    }
	    if (nr == 0) {
		fprintf(f, "Host EOF\n");
		shutdown(a, 1);
		o_open = false;
	    } else {
		netdump(f, '<', (unsigned char *)buf, nr);
		send(a, buf, nr, 0);
	    }
	}
    }

    t = time(NULL);
    fprintf(f, "Stopped %s", asctime(gmtime(&t)));
    return 0;
}

#if 0
/* Socket error. */
static void
sockerr(const char *s)
{
#if !defined(_WIN32) /*[*/
    perror(s);
#else /*][*/
    unsigned err = WSAGetLastError();
    char buf[1024];

    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf),
		NULL) == 0) {
	snprintf(buf, sizeof(buf), "0x%x", err);
    }

    fprintf(stderr, "%s: socket error %s", s, buf);
#endif /*]*/
}
#endif

/* Display a hex dump of a buffer. */
static void
netdump(FILE *f, char direction, unsigned char *buffer, size_t length)
{
    size_t i, count, index;
    static char rgbDigits[] = "0123456789abcdef";
    char rgbLine[100];
    int cbLine;
#   define BYTES_PER_LINE	22

    for (index = 0; length; length -= count, buffer += count, index += count) {
	count = (length > BYTES_PER_LINE)? BYTES_PER_LINE: length;
	cbLine = sprintf(rgbLine, "0x%-3x ", (unsigned)index);

	for (i = 0; i < count; i++) {
	    rgbLine[cbLine++] = rgbDigits[buffer[i] >> 4];
	    rgbLine[cbLine++] = rgbDigits[buffer[i] & 0x0f];
	}
	for (; i < BYTES_PER_LINE; i++) {
	    rgbLine[cbLine++] = ' ';
	    rgbLine[cbLine++] = ' ';
	}
	rgbLine[cbLine++] = ' ';

	for (i = 0; i < count; i++) {
	    if (buffer[i] < 32 || buffer[i] > 126 || buffer[i] == '%') {
		rgbLine[cbLine++] = '.';
	    } else {
		rgbLine[cbLine++] = buffer[i];
	    }
	}
	rgbLine[cbLine++] = 0;
	fprintf(f, "%c %s\n", direction, rgbLine);
    }
}

/* Glue for library errors. */
void
Error(const char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(1);
}
