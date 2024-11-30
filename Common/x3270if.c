/*
 * Copyright (c) 1995-2024 Paul Mattes.
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

#include <errno.h>
#include <fcntl.h>
#if !defined(_WIN32) /*[*/
# include <signal.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif /*]*/

#if !defined(_WIN32) /*[*/
# if defined(HAVE_NCURSESW_NCURSES_H) /*[*/
#  include <ncursesw/ncurses.h>
# elif defined(HAVE_NCURSES_NCURSES_H) /*][*/
#  include <ncurses/ncurses.h>
# elif defined(HAVE_NCURSES_H) /*][*/
#  include <ncurses.h>
# else /*][*/
#  include <curses.h>
# endif /*]*/
# if defined(HAVE_NCURSESW_TERM_H) /*[*/
#  include <ncursesw/term.h>
# elif defined(HAVE_NCURSES_TERM_H) /*][*/
#  include <ncurses/term.h>
# elif defined(HAVE_TERM_H) /*][*/
#  include <term.h>
# endif /*]*/
# if defined(HAVE_LIBREADLINE) /*[*/
#  include <readline/readline.h>
#  if defined(HAVE_READLINE_HISTORY_H) /*[*/
#   include <readline/history.h>
#  endif /*]*/
# endif /*]*/
#endif

#include "base64.h"
#include "names.h"
#include "s3270_proto.h"
#include "w3misc.h"

#define IBS	4096

#define NO_STATUS	(-1)
#define ALL_FIELDS	(-2)

#if defined(_WIN32) /*[*/
#define DIRSEP	'\\'
#define OPTS	"H:iI:L:s:St:v"
#define FD_ENV_REQUIRED	true
#else /*][*/
#define DIRSEP '/'
#define OPTS	"H:iI:L:p:Ps:St:v"
#define FD_ENV_REQUIRED	false
#endif /*]*/

#if !defined(HAVE_TIPARM) /*[*/
#define tiparm		tparm
#endif /*]*/

typedef enum {
    ITYPE_DATA,		/* data: */
    ITYPE_ERRD,		/* errd: */
    ITYPE_INPUT,	/* input: */
    ITYPE_PWINPUT	/* pwinput: */
} itype_t;

typedef enum {
    AUX_NONE,		/* new command */
    AUX_INPUT,		/* requested input */
    AUX_PWINPUT		/* password (no echo) input */
} aux_t;

static char *me;
static int verbose = 0;
static char *buf;
static size_t buf_size = 0;

static void iterative_io(int pid, unsigned short port);
static int single_io(int pid, unsigned short port, socket_t socket, int infd,
	int outfd, int fn, char *cmd, char **data_ret, char **errd_ret, char **prompt_ret,
	itype_t *itype);
static void interactive_io(int port, const char *emulator_name,
	const char *help_name, const char *localization);

#if defined(HAVE_LIBREADLINE) /*[*/
static char **attempted_completion(const char *text, int start, int end);
static char *completion_entry(const char *, int);
#endif /*]*/

/* Localization data. */
typedef struct i18n {
    struct i18n *next;
    char *key;
    char *translation;
} i18n_t;
i18n_t *i18n = NULL;
#define BANNER	"x3270if.banner"
#define QUIT	"x3270if.quit"

#if defined(_WIN32) /*[*/
static HANDLE conin = NULL;
#endif /*]*/

static void
x3270if_usage(void)
{
    fprintf(stderr, "\
usage:\n\
 %s [options] \"action[(param[,...])]\"\n\
   execute the named action\n\
 %s [options] -s field\n\
   display status field 0..12\n\
 %s [options] -S\n\
   display all status fields\n\
 %s [options] -i\n\
   shuttle commands and responses between stdin/stdout and emulator\n\
 %s [options] -I <emulator-name> [-H <help-action-name>]\n\
   interactive command window\n\
 %s --version\n\
options:\n\
 -v       verbose operation\n"
#if !defined(_WIN32) /*[*/
" -p pid   connect to process <pid>\n"
#endif /*]*/
" -t port  connect to TCP port <port>\n",
	    me, me, me, me, me, me);
    exit(__LINE__);
}

/* Get a file descriptor from the environment. */
static int
fd_env(const char *name, bool required)
{
    char *fdname;
    int fd;

    fdname = getenv(name);
    if (fdname == NULL) {
	if (required) {
	    fprintf(stderr, "%s: %s not set in the environment\n", me,
		    name);
	    exit(__LINE__);
	} else {
	    return -1;
	}
    }
    fd = atoi(fdname);
    if (fd <= 0) {
	fprintf(stderr, "%s: invalid value '%s' for %s\n", me, fdname,
		name);
	exit(__LINE__);
    }
    if (verbose) {
	fprintf(stderr, "%s is %d\n", name, fd);
    }
    return fd;
}

/* Get the cookie from the cookie file. */
const char *
get_cookie(void)
{
    char *cookiefile = getenv(COOKIEFILE_ENV);
    int fd;
    char *buf;
    ssize_t nr;

    if (cookiefile == NULL) {
	return NULL;
    }
    fd = open(cookiefile, O_RDONLY);
    if (fd < 0) {
	return NULL;
    }
    buf = Malloc(1024);
    nr = read(fd, buf, 1023);
    if (nr < 0) {
	close(fd);
	Free(buf);
	return NULL;
    }

    /* Ignore trailing white space. */
    while (nr > 0 && isspace((int)buf[nr - 1])) {
	nr--;
    }

    close(fd);
    buf[nr] = '\0';
    return buf;
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
    const char *emulator_name = NULL;
    const char *help_name = NULL;
    const char *localization = NULL;
#if !defined(_WIN32) /*[*/
    bool force_pipes = false;
#endif /*]*/

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
    while ((c = getopt(argc, argv, OPTS)) != -1) {
	switch (c) {
	case 'H':
	    help_name = optarg;
	    break;
	case 'i':
	    if (fn >= 0) {
		x3270if_usage();
	    }
	    iterative++;
	    break;
	case 'I':
	    if (fn > 0) {
		x3270if_usage();
	    }
	    iterative++;
	    emulator_name = optarg;
	    break;
	case 'L':
	    localization = optarg;
	    break;
#if !defined(_WIN32) /*[*/
	case 'p':
	    pid = (int)strtoul(optarg, &ptr, 0);
	    if (ptr == optarg || *ptr != '\0' || pid <= 0) {
		fprintf(stderr, "%s: Invalid process ID: '%s'\n", me, optarg);
		x3270if_usage();
	    }
	    break;
	case 'P':
	    force_pipes = true;
	    break;
#endif /*]*/
	case 's':
	    if (fn >= 0 || iterative) {
		x3270if_usage();
	    }
	    fn = (int)strtol(optarg, &ptr, 0);
	    if (ptr == optarg || *ptr != '\0' || fn < 0) {
		fprintf(stderr, "%s: Invalid field number: '%s'\n", me, optarg);
		x3270if_usage();
	    }
	    break;
	case 'S':
	    if (fn >= 0 || iterative) {
		x3270if_usage();
	    }
	    fn = ALL_FIELDS;
	    break;
	case 't':
	    port = (unsigned short)strtoul(optarg, &ptr, 0);
	    if (ptr == optarg || *ptr != '\0' || port <= 0) {
		fprintf(stderr, "%s: Invalid port: '%s'\n", me, optarg);
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
	if (fn == NO_STATUS && !iterative) {
	    x3270if_usage();
	}
    } else {
	/* Got positional arguments. */
	if (iterative) {
	    x3270if_usage();
	}
	if (argc - optind > 1) {
	    x3270if_usage();
	}
    }
    if (pid && port) {
	x3270if_usage();
    }
    if (help_name != NULL && emulator_name == NULL) {
	x3270if_usage();
    }

#if !defined(_WIN32) /*[*/
    /* Ignore broken pipes. */
    signal(SIGPIPE, SIG_IGN);
#endif /*]*/

    /* Do the I/O. */
    if (iterative && emulator_name != NULL) {
	interactive_io(port, emulator_name, help_name, localization);
    } else if (iterative) {
	iterative_io(pid, port);
    } else {
	const char *cookie = get_cookie();
	int infd = -1;
	int outfd = -1;
	char *cmd;
	int rv;

#if !defined(_WIN32) /*[*/
	if (force_pipes) {
	    infd  = fd_env(OUTPUT_ENV, true);
	    outfd = fd_env(INPUT_ENV, true);
	    cookie = NULL;
	}
#endif /*]*/

	if (cookie) {
	    cmd = Malloc(strlen(AnCookie) + 1 + strlen(cookie) + 2 +
		    strlen(AnCapabilities) + 1 + strlen(KwErrd) + 2 + strlen(argv[optind]) + 1);
	    sprintf(cmd, AnCookie "(%s) " AnCapabilities "(" KwErrd ") %s", cookie, argv[optind]);
	} else {
	    cmd = Malloc(strlen(AnCapabilities) + 1 + strlen(KwErrd) + 2 + strlen(argv[optind]) + 1);
	    sprintf(cmd, AnCapabilities "(" KwErrd ") %s", argv[optind]);
	}
	rv = single_io(pid, port, INVALID_SOCKET, infd, outfd, fn, cmd, NULL, NULL, NULL, NULL);
	Free(cmd);
	return rv;
    }
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
	exit(__LINE__);
    }
    memset(&ssun, '\0', sizeof(struct sockaddr_un));
    ssun.sun_family = AF_UNIX;
    snprintf(ssun.sun_path, sizeof(ssun.sun_path), "/tmp/x3sck.%d", pid);
    if (connect(fd, (struct sockaddr *)&ssun, sizeof(ssun)) < 0) {
	perror("connect");
	exit(__LINE__);
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
	exit(__LINE__);
    }
    memset(&sin, '\0', sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
#if defined(_WIN32) /*[*/
	win32_perror("connect(%u)", port);
#else /*][*/
	perror("connect");
#endif /*]*/
	exit(__LINE__);
    }
    return fd;
}

/* Set or clear echo mode. */
static void
echo_mode(bool echo)
{
#if !defined(_WIN32) /*[*/
    struct termios t;

    tcgetattr(0, &t);
    if (echo) {
	t.c_lflag |= ECHO;
    } else {
	t.c_lflag &= ~ECHO;
    }
    tcsetattr(0, TCSANOW, &t);
#else /*][*/
    if (echo) {
	if (SetConsoleMode(conin, ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT) == 0) {
	    win32_perror("\nSetConsoleMode(CONIN$) failed");
	}
    } else {
	if (SetConsoleMode(conin, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT) == 0) {
	    win32_perror("\nSetConsoleMode(CONIN$) failed");
	}
    }
#endif /*]*/
}

/* Get the input type from a buffer. */
static itype_t
get_itype(const char *buf)
{
    if (!strncmp(buf, DATA_PREFIX, PREFIX_LEN)) {
	return ITYPE_DATA;
    }
    if (!strncmp(buf, ERROR_DATA_PREFIX, PREFIX_LEN)) {
	return ITYPE_ERRD;
    }
    if (!strncmp(buf, INPUT_PREFIX, PREFIX_LEN)) {
	return ITYPE_INPUT;
    }
    if (!strncmp(buf, PWINPUT_PREFIX, PREFIX_LEN)) {
	return ITYPE_PWINPUT;
    }
    return ITYPE_DATA; /* wrong */
}

/* Do a single command, and interpret the results. */
static int
single_io(int pid, unsigned short port, socket_t socket, int xinfd, int xoutfd,
	int fn, char *cmd, char **data_ret, char **errd_ret, char **prompt_ret, itype_t *itype)
{
    int port_env;
    int infd = -1, outfd = -1;
    socket_t insocket = INVALID_SOCKET, outsocket = INVALID_SOCKET;
    bool is_socket = false;
    char *status = NULL;
    int nr;
    int xs = -1;
    int nw = 0;
    char rbuf[IBS];
    size_t sl = 0;
    int done = 0;
    char *cmd_nl;
    char *wstr;
    size_t ret_sl = 0;
    size_t eret_sl = 0;
    itype_t input_itype = ITYPE_DATA;

    /* Verify the environment and open files. */
    if (socket != INVALID_SOCKET) {
	insocket = socket;
	outsocket = socket;
	is_socket = true;
    } else if (xinfd != -1 && xoutfd != -1) {
	infd = xinfd;
	outfd = xoutfd;
    } else {
#if !defined(_WIN32) /*[*/
	if (pid) {
	    insocket = outsocket = usock(pid);
	    is_socket = true;
	} else
#endif /*]*/
	if (port) {
	    insocket = outsocket = tsock(port);
	    is_socket = true;
	} else if ((port_env = fd_env(PORT_ENV, FD_ENV_REQUIRED)) >= 0) {
	    insocket = outsocket = tsock(port_env);
	    is_socket = true;
	} else {
#if defined(_WIN32) /*[*/
	    return -1;
#else /*][*/
	    infd  = fd_env(OUTPUT_ENV, true);
	    outfd = fd_env(INPUT_ENV, true);
#endif /*]*/
	}
	if ((!is_socket && infd < 0) || (is_socket && insocket == INVALID_SOCKET)) {
	    perror("x3270if: input");
	    exit(__LINE__);
	}
	if ((!is_socket && outfd < 0) ||
	    (is_socket && outsocket == INVALID_SOCKET)) {
	    perror("x3270if: output");
	    exit(__LINE__);
	}
    }

    if (prompt_ret != NULL) {
	*prompt_ret = NULL;
    }
    if (itype != NULL) {
	*itype = ITYPE_DATA;
    }

    /* Speak to x3270. */
    if (verbose) {
	fprintf(stderr, "i+ out %s\n", (cmd != NULL) ? cmd : "");
    }

    if (cmd != NULL) {
	cmd_nl = Malloc(strlen(cmd) + 2);
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
	if (is_socket) {
#if defined(_WIN32) /*[*/
	    win32_perror("x3270if: send");
#else /*][*/
	    perror("x3270if: send");
#endif /*]*/
	} else {
	    perror("x3270if: write");
	}
	exit(__LINE__);
    }
    if (cmd_nl != NULL) {
	Free(cmd_nl);
    }

    if (data_ret != NULL) {
	*data_ret = NULL;
    }
    if (errd_ret != NULL) {
	*errd_ret = NULL;
    }

#if defined(_WIN32) /*[*/
retry:
#endif /*]*/
    /* Get the answer. */
    while (!done &&
	    (nr = (is_socket? recv(insocket, rbuf, IBS, 0):
			      read(infd, rbuf, IBS))) > 0) {
	int i;
	bool get_more = false;

	i = 0;
	do {
	    /* Copy from rbuf into buf until '\n'. */
	    while (i < nr && rbuf[i] != '\n') {
		if (sl + 2 > buf_size) {
		    buf = Realloc(buf, buf_size + IBS);
		    buf_size += IBS;
		}
		buf[sl++] = rbuf[i++];
	    }
	    if (rbuf[i] == '\n') {
		i++;
	    } else {
		/* Go get more input. */
		get_more = true;
		break;
	    }

	    /* Process one line of output. */
	    buf[sl] = '\0';

	    if (verbose) {
		fprintf(stderr, "i+ in %s\n", buf);
	    }
	    if (buf[0] == '{') {
		/* JSON output. */
		printf("%s\n", buf);
		fflush(stdout);
		xs = 0;
		done = 1;
		break;
	    }
	    if (!strcmp(buf, PROMPT_OK)) {
		fflush(stdout);
		xs = 0;
		done = 1;
		break;
	    } else if (!strcmp(buf, PROMPT_ERROR)) {
		fflush(stdout);
		xs = 1;
		done = 1;
		break;
	    } else if (!strncmp(buf, DATA_PREFIX, PREFIX_LEN)
		    || !strncmp(buf, ERROR_DATA_PREFIX, PREFIX_LEN)
		    || !strncmp(buf, INPUT_PREFIX, PREFIX_LEN)
		    || !strncmp(buf, PWINPUT_PREFIX, PREFIX_LEN)) {
		itype_t this_itype;

		this_itype = get_itype(buf);
		input_itype = this_itype;

		/*
		 * The protocol is somewhat ambiguous: You could get multiple
		 * inpt: and inpw: in the same response.
		 * We only keep the last.
		 */
		if (data_ret != NULL) {
		    if (this_itype == ITYPE_INPUT || this_itype == ITYPE_PWINPUT) {
			if (*prompt_ret != NULL) {
			    Free(*prompt_ret);
			}
			*prompt_ret = NewString(buf + PREFIX_LEN);
		    } else {
			char **dret = (this_itype == ITYPE_DATA)? data_ret: errd_ret;
			size_t *sl = (this_itype == ITYPE_DATA)? &ret_sl: &eret_sl;

			*dret = Realloc(*dret, *sl + strlen(buf + PREFIX_LEN) + 2);
			*(*dret + *sl) = '\0';
			strcat(strcat(*dret, buf + PREFIX_LEN), "\n");
			*sl += strlen(buf + PREFIX_LEN) + 1;
		    }
		} else {
		    if (fprintf((input_itype == ITYPE_ERRD)? stderr: stdout, "%s\n", buf + PREFIX_LEN) < 0) {
			perror("x3270if: printf");
			exit(__LINE__);
		    }
		}
	    } else {
		Replace(status, NewString(buf));
	    }

	    /* Get ready for the next. */
	    sl = 0;
	} while (i < nr);

	if (get_more) {
	    get_more = false;
	    continue;
	}
    }
    if (nr < 0) {
	if (is_socket) {
#if defined(_WIN32) /*[*/
	    if (WSAGetLastError() == WSAEWOULDBLOCK) {
		goto retry;
	    }
	    win32_perror("x3270if: recv");
#else /*][*/
	    perror("recv");
#endif /*]*/
	} else {
	    perror("read");
	}
	exit(__LINE__);
    } else if (nr == 0) {
	fprintf(stderr, "x3270if: unexpected EOF\n");
	exit(__LINE__);
    }

    if (fflush(stdout) < 0) {
	perror("x3270if: fflush");
	exit(__LINE__);
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
		if (!fn--) {
		    break;
		}
		sf = strtok(sb, " \t");
		sb = NULL;
	    } while (sf != NULL);
	    rc = printf("%s\n", (sf != NULL) ? sf : "");
	}
	if (rc < 0) {
	    perror("x3270if: printf");
		exit(__LINE__);
	}
    }

    if (fflush(stdout) < 0) {
	perror("x3270if: fflush");
	exit(__LINE__);
    }

    if (is_socket && socket == INVALID_SOCKET) {
	shutdown(insocket, 2);
#if defined(_WIN32) /*[*/
	closesocket(insocket);
#else /*][*/
	close(insocket);
	if (verbose) {
	    fprintf(stderr, "closed %d\n", insocket);
	}
#endif /*]*/
    }

    if (itype != NULL) {
	*itype = input_itype;
    }

    Free(status);
    return xs;
}

/* Fetch the ports from the environment. */
static void
get_ports(socket_t *socket, int *infd, int *outfd)
{
#if !defined(_WIN32) /*[*/
    int socketport = -1;

    if (socket != NULL) {
	socketport = fd_env(PORT_ENV, false);
    }
    if (socketport == -1) {
	*infd = fd_env(OUTPUT_ENV, true);
	*outfd = fd_env(INPUT_ENV, true);
    } else {
	*socket = tsock(socketport);
    }
    if (verbose) {
	fprintf(stderr, "socket: %d, input: %d, output: %d\n",
		(socket != NULL)? *socket: -1,
		*infd, *outfd);
    }
#else /*][*/
    int socketport = fd_env(PORT_ENV, true);

    *socket = tsock(socketport);
    if (verbose) {
	fprintf(stderr, "port: %d\n", socketport);
    }
#endif /*]*/
}

#if !defined(_WIN32) /*[*/

/* Act as a passive pipe to the emulator. */
static void
iterative_io(int pid, unsigned short port)
{
# define N_IO 2
    struct {
	const char *name;
	int rfd, wfd;
	char buf[IBS];
	int offset, count;
    } io[N_IO];	/* [0] is script->emulator, [1] is emulator->script */
    fd_set rfds, wfds;
    int fd_max = 0;
    int i;
    int port_env = -1;

    /* Get the x3270 file descriptors. */
    io[0].name = "script->emulator";
    io[0].rfd = fileno(stdin);
    if (pid) {
	io[0].wfd = usock(pid);
    } else if (port) {
	io[0].wfd = tsock(port);
    } else if ((port_env = fd_env(PORT_ENV, FD_ENV_REQUIRED)) >= 0) {
	io[0].wfd = tsock(port_env);
    } else {
	io[0].wfd = fd_env(INPUT_ENV, true);
    }
    io[1].name = "emulator->script";
    if (pid || port || (port_env >= 0)) {
	io[1].rfd = dup(io[0].wfd);
    } else {
	io[1].rfd = fd_env(OUTPUT_ENV, true);
    }
    io[1].wfd = fileno(stdout);
    for (i = 0; i < N_IO; i++) {
	if (io[i].rfd > fd_max) {
	    fd_max = io[i].rfd;
	}
	if (io[i].wfd > fd_max) {
	    fd_max = io[i].wfd;
	}
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
		if (verbose) {
		    fprintf(stderr, "enabling output %s %d\n",
			    io[i].name, io[i].wfd);
		}
	    } else {
		FD_SET(io[i].rfd, &rfds);
		if (verbose) {
		    fprintf(stderr, "enabling input %s %d\n",
			    io[i].name, io[i].rfd);
		}
	    }
	}

	if ((rv = select(fd_max, &rfds, &wfds, NULL, NULL)) < 0) {
	    perror("x3270if: select");
	    exit(__LINE__);
	}
	if (verbose) {
	    fprintf(stderr, "select->%d\n", rv);
	}

	for (i = 0; i < N_IO; i++) {
	    if (io[i].count) {
		if (FD_ISSET(io[i].wfd, &wfds)) {
		    rv = write(io[i].wfd, io[i].buf + io[i].offset,
			    io[i].count);
		    if (rv < 0) {
			fprintf(stderr, "x3270if: write(%s): %s",
				io[i].name, strerror(errno));
			exit(__LINE__);
		    }
		    io[i].offset += rv;
		    io[i].count -= rv;
		    if (verbose) {
			fprintf(stderr, "write(%s)->%d\n", io[i].name,
				rv);
		    }
		}
	    } else if (FD_ISSET(io[i].rfd, &rfds)) {
		rv = read(io[i].rfd, io[i].buf, IBS);
		if (rv < 0) {
		    fprintf(stderr, "x3270if: read(%s): %s", io[i].name,
			    strerror(errno));
		    exit(__LINE__);
		}
		if (rv == 0) {
		    exit(0);
		}
		io[i].offset = 0;
		io[i].count = rv;
		if (verbose) {
		    fprintf(stderr, "read(%s)->%d\n", io[i].name, rv);
		}
	    }
	}
    }
}

#else /*][*/

static HANDLE stdin_thread;
static HANDLE stdin_enable_event, stdin_done_event;
static char stdin_buf[1024];
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
	port_env = getenv(PORT_ENV);
	if (port_env == NULL) {
	    fprintf(stderr, "Must specify port or put port in " PORT_ENV ".\n");
	    exit(__LINE__);
	}
	port = atoi(port_env);
	if (port <= 0 || (port & ~0xffff)) {
	    fprintf(stderr, "Invalid " PORT_ENV ".\n");
	    exit(__LINE__);
	}
    }

    /* Open the socket. */
    s = socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) {
	win32_perror("socket");
	exit(__LINE__);
    }
    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	win32_perror("connect(%u) failed", port);
	exit(__LINE__);
    }
    if (verbose) {
	fprintf(stderr, "<connected to port %d>\n", port);
    }
    socket_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (socket_event == NULL) {
	win32_perror("CreateEvent failed");
	exit(__LINE__);
    }
    if (WSAEventSelect(s, socket_event, FD_READ | FD_CLOSE) != 0) {
	win32_perror("WSAEventSelect failed");
	exit(__LINE__);
    }

    /* Create a thread to read data from the socket. */
    stdin_enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    stdin_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    stdin_thread = CreateThread(NULL, 0, stdin_read, NULL, 0, NULL);
    if (stdin_thread == NULL) {
	win32_perror("CreateThread failed");
	exit(__LINE__);
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
		fprintf(stderr, "<%d byte%s from socket>\n", nr,
			(nr == 1)? "": "s");
	    }
	    if (nr < 0) {
		win32_perror("recv failed");
		exit(__LINE__);
	    }
	    if (nr == 0) {
		exit(__LINE__);
	    }
	    fwrite(buf, 1, nr, stdout);
	    fflush(stdout);
	    break;
	case WAIT_OBJECT_0 + 1: /* stdin input */
	    if (verbose) {
		fprintf(stderr, "<%d byte%s from stdin>\n", stdin_nr,
			(stdin_nr == 1)? "": "s");
		}
	    if (stdin_nr < 0) {
		fprintf(stderr, "stdin read failed: %s\n",
			win32_strerror(stdin_error));
		exit(__LINE__);
	    }
	    if (stdin_nr == 0) {
		exit(0);
	    }
	    send(s, stdin_buf, stdin_nr, 0);
	    SetEvent(stdin_enable_event);
	    break;
	case WAIT_FAILED:
	    win32_perror("WaitForMultipleObjects failed");
	    exit(__LINE__);
	default:
	    fprintf(stderr, "Unexpected return %d from "
		    "WaitForMultipleObjects\n", (int)ret);
	    exit(__LINE__);
	}
    }
}

#endif /*]*/

#if defined(HAVE_LIBREADLINE) /*[*/
static char **
attempted_completion(const char *text, int start, int end)
{
    /*
     * At some point, we may get the action list from the emulator, but for
     * now, just fail.
     */
    return NULL;
}

static char *
completion_entry(const char *text, int state)
{
    /*
     * At some point, we may get the action list from the emulator, but for
     * now, just fail.
     */
    return NULL;
}

/* The command line read by readline. */
static char *readline_command;

/* True if readline is finished reading a command. */
static bool readline_done = false;

/* Handle a command line. */
static void
rl_handler(char *command)
{
    /*
     * readline's callback handler API doesn't allow context to be passed in or
     * out of the handler. So the only way for it to communicate with the
     * function that calls rl_callback_read_char() is through global variables.
     */
    readline_done = true;
    readline_command = command;

    /*
     * Remove the callback handler. If we don't remove it, readline() will
     * display the prompt as soon as this function returns.
     */
    rl_callback_handler_remove();
}
#endif /*]*/

#if defined(_WIN32) /*[*/
static void
set_text_attribute(HANDLE out, WORD attributes)
{
    if (!SetConsoleTextAttribute(out, attributes)) {
	win32_perror("Can't set console text attribute");
	exit(__LINE__);
    }
}
#endif /*[*/

/* Copy and translate a translation. */
static void
xlcpy(char *dest, const char *src)
{
    bool backslash = false;
    char c;

    /* Skip spaces. */
    while ((c = *src) == ' ')
    {
	src++;
    }

    /* Copy, translating certain escape sequences. */
    while ((c = *src++) != '\0') {
	if (backslash) {
	    if (c == 'n') {
		*dest++ = '\n';
	    } else if (c != 'r') {
		*dest++ = c;
	    }
	    backslash = false;
	} else if (c == '\\') {
	    backslash = true;
	} else {
	    *dest++ = c;
	}
    }
    *dest = '\0';
}

/* Read the localization file. */
static void
read_localization(const char *filename)
{
    FILE *f = fopen(filename, "r");
    char buf[1024];
    char *s;
    int line = 1;

    if (f == NULL) {
	perror(filename);
	exit(__LINE__);
    }

    while ((s = fgets(buf, sizeof(buf), f)) != NULL) {
	size_t sl;
	char *colon;
	i18n_t *ie;

	sl = strlen(s);
	if (sl > 0 && s[sl - 1] == '\n') {
	    s[sl - 1] = '\0';
	}
	colon = strchr(s, ':');
	if (colon == NULL || colon == s) {
	    fprintf(stderr, "%s, line %d: bad format\n", filename, line);
	    exit(__LINE__);
	}

	ie = (i18n_t *)Malloc(sizeof(i18n_t) + sl + 2);
	ie->key = (char *)(ie + 1);
	strncpy(ie->key, s, colon - s);
	ie->key[colon - s] = '\0';
	ie->translation = ie->key + strlen(ie->key) + 1;
	xlcpy(ie->translation, colon + 1);
	ie->next = i18n;
	i18n = ie;
    }

    fclose(f);
}

/* Get a localized string. */
static const char *
i18n_get(const char *key)
{
    i18n_t *ie;

    for (ie = i18n; ie != NULL; ie = ie->next) {
	if (!strcmp(key, ie->key))
	{
	    return ie->translation;
	}
    }
    return NULL;
}

#if !defined(_WIN32) /*[*/
static char *
tigetstr_def(const char *name, char *def)
{
    char *s = tigetstr((char *)name);

    if (s != NULL && s != (char *)-1) {
	return s;
    }
    return def;
}

/* Get an ANSI color setting attribute. */
static const char *
xsetaf(const char *setaf, int color, const char *sgr)
{
    static char *x_ret = NULL;

    /* Clean up the previous value. */
    if (x_ret != NULL) {
	Free(x_ret);
	x_ret = NULL;
    }

    if (setaf != NULL) {
	char *a, *s;

	/* Encode AF. */
	a = tiparm((char *)setaf, color);
	if (sgr == NULL) {
	    return a;
	}

	/* Save encoded AF and encode SGR. */
	a = NewString(a);
	s = tiparm((char *)sgr, 0, 0, 0, 0, 0, 1, 0, 0, 0);
	x_ret = Malloc(strlen(a) + strlen(s) + 1);
	sprintf(x_ret, "%s%s", s, a);
	Free(a);
	
	/* Return combined SGR and AF. */
	return x_ret;
    } else {
	return "";
    }
}
#endif /*]*/

static void
interactive_io(int port, const char *emulator_name, const char *help_name,
	const char *localization)
{
    char *prompt, *real_prompt;
    socket_t s = INVALID_SOCKET;
    int infd = -1, outfd = -1;
    const char *cookie;
    char *cmd;
    size_t prompt_len;
    char *data_ret;
    char *errd_ret;
    char *prompt_ret;
    aux_t aux_input = AUX_NONE;
    itype_t itype;
    const char *l;
#if defined(_WIN32) /*[*/
    HANDLE conout;
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE socket_event;
#else /*][*/
    int colors;
    char *setaf;
    char *op;
    char *sgr;
    char *sgr0;
    char *prompt_setaf;
    int color_offset = 0;
#endif /*]*/

#if !defined(_WIN32) /*[*/
    /* Set up terminfo and check for ANSI color. */
    setupterm(NULL, fileno(stdout), NULL);
    colors = tigetnum("colors");
    setaf = tigetstr_def("setaf", NULL);
    op = tigetstr_def("op", "");
    if (!op[0]) {
	setaf = NULL;
    }
    sgr = tigetstr_def("sgr", NULL);
    sgr0 = tigetstr_def("sgr0", "");
    if (!sgr0[0]) {
	sgr = NULL;
    }
    if (colors < 8 || setaf == NULL) {
	/* No usable color. */
	setaf = NULL;
	op = "";
	sgr = NULL;
	sgr0 = "";
    } else if (colors >= 16 && sgr != NULL) {
	/* Use brighter colors. */
	color_offset = 8;
	sgr = NULL;
	sgr0 = "";
    }
    if (op[0] && sgr0[0]) {
	/* Combine OP and SGR0. */
	char *s = Malloc(strlen(op) + strlen(sgr0) + 1);

	sprintf(s, "%s%s", op, sgr0);
	op = s;
    }
#endif

    cookie = get_cookie();
    if (port) {
	s = tsock(port);
    } else {
#if !defined(_WIN32) /*[*/
	get_ports(&s, &infd, &outfd);
	if (infd != -1 && outfd != -1) {
	    cookie = NULL;
	}
#else /*][*/
	get_ports(&s, NULL, NULL);
#endif /*]*/
    }

#if defined(HAVE_LIBREADLINE) /*[*/
# define MLEN	1
# define LEFT	"\001"
# define RIGHT	"\002"
#else /*]*/
# define MLEN	0
# define LEFT	""
# define RIGHT	""
#endif /*]*/

    /* Localize. */
    if (localization != NULL) {
	read_localization(localization);
    }

    /* Set the cookie and announce our capabilities. */
#   define CAP_STRING AnCapabilities "(" KwInteractive "," KwPwInput "," KwErrd ")"
    if (cookie) {
	size_t len = strlen(AnCookie) + 1 + strlen(cookie) + 2 + strlen(CAP_STRING) + 1;

	cmd = Malloc(len);
	snprintf(cmd, len, AnCookie "(%s) " CAP_STRING, cookie);
	cmd[len - 1] = '\0';
    } else {
	cmd = CAP_STRING;
    }
    data_ret = NULL;
    errd_ret = NULL;
    single_io(0, 0, s, infd, outfd, NO_STATUS, cmd, &data_ret, &errd_ret, NULL, &itype);

    /* Set up the prompt. */
#if !defined(_WIN32) /*[*/
    prompt_setaf = (char *)xsetaf(setaf, color_offset + COLOR_BLUE, sgr);
    prompt_setaf = Malloc(strlen(prompt_setaf) + 1);
    strcpy(prompt_setaf, xsetaf(setaf, color_offset + COLOR_BLUE, sgr));
    prompt_len = MLEN + strlen(prompt_setaf) + MLEN + strlen(emulator_name)
	+ strlen("> ") + MLEN + strlen(op) + MLEN + 1;
    prompt = Malloc(prompt_len);
    snprintf(prompt, prompt_len, LEFT "%s" RIGHT "%s> " LEFT "%s" RIGHT,
	    prompt_setaf, emulator_name, op);
#else /*][*/
    prompt_len = strlen(emulator_name) + strlen("> ") + 1;
    prompt = Malloc(prompt_len);
    snprintf(prompt, prompt_len, "%s> ", emulator_name);
#endif /*]*/
    real_prompt = prompt;

# if defined(HAVE_LIBREADLINE) /*[*/
    /* Set up readline. */
    rl_readline_name = (char *)emulator_name;
    rl_initialize();
    rl_attempted_completion_function = attempted_completion;
#  if defined(RL_READLINE_VERSION) && (RL_READLINE_VERSION > 0x0402) /*[*/
    rl_completion_entry_function = completion_entry;
#  else /*][*/
    rl_completion_entry_function = (Function *)completion_entry;
#  endif /*]*/
# endif /*]*/

#if defined(_WIN32) /*[*/
    /* Open the console handle. */
    conout = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (conout == NULL) {
	win32_perror("Can't open console output handle");
	exit(__LINE__);
    }
    if (!GetConsoleScreenBufferInfo(conout, &info)) {
	win32_perror("Can't get console info");
	exit(__LINE__);
    }
    conin = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (conin == NULL) {
        win32_perror("Can't popen console input handle");
        exit(__LINE__);
    }

    /* wx3270 speaks Unicode. */
    SetConsoleOutputCP(65001);

    /* Set the title. */
    SetConsoleTitle(prompt);

    /* Set up the stdin thread. */
    stdin_enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    stdin_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    stdin_thread = CreateThread(NULL, 0, stdin_read, NULL, 0, NULL);
    if (stdin_thread == NULL) {
	win32_perror("Cannot create stdin thread");
	exit(__LINE__);
    }

    /* Set up the socket event. */
    socket_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (socket_event == NULL) {
	win32_perror("Cannot create socket event");
	exit(__LINE__);
    }
    if (WSAEventSelect(s, socket_event, FD_CLOSE) != 0) {
	win32_perror("Cannot set socket events");
	exit(__LINE__);
    }
#endif /*]*/

    /* Introduce yourself. */
    l = i18n_get(BANNER);
    if (l != NULL) {
	printf("%s\n", l);
    } else {
	printf("%s Prompt\n\n", emulator_name);
	printf("To execute one action and close this window, end the command line with '/'.\n");
	printf("To close this window, enter just '/' as the command line.\n");
	if (help_name != NULL) {
	    printf("To get help, use the '%s()' action.\n", help_name);
	}
    }
#if !defined(_WIN32) /*[*/
    printf("%s", xsetaf(setaf, color_offset + COLOR_YELLOW, sgr));
# else /*][*/
    fflush(stdout);
    set_text_attribute(conout, FOREGROUND_GREEN | FOREGROUND_RED);
#endif /*]*/
    l = i18n_get(QUIT);
    if (l != NULL) {
	printf("%s", l);
    } else {
	printf("Note: The 'Quit()' action will cause %s to exit.",
		emulator_name);
    }
#if !defined(_WIN32) /*[*/
    printf("%s", op);
# else /*][*/
    fflush(stdout);
    set_text_attribute(conout, info.wAttributes);
#endif /*]*/
    printf("\n\n");

    for (;;) {
	char *command;
	char *nl;
	size_t sl;
	bool done = false;
#if !defined(_WIN32) /*[*/
# if !defined(HAVE_LIBREADLINE) /*[*/
	char inbuf[1024];
# endif /*]*/
# else /*][*/
	HANDLE ha[2];
	DWORD rv;
#endif /*]*/

	if (aux_input == AUX_PWINPUT) {
	    echo_mode(false);
	}

	/* Display the prompt. */
#if !defined(_WIN32) /*[*/
# if defined(HAVE_LIBREADLINE) /*[*/
	rl_callback_handler_install(prompt, &rl_handler);
# else /*][*/
	fputs(prompt, stdout);
	fflush(stdout);
# endif /*]*/
#else /*][*/
	if (aux_input == AUX_NONE) {
	    set_text_attribute(conout, FOREGROUND_INTENSITY | FOREGROUND_BLUE);
	}
	fputs(prompt, stdout);
	fflush(stdout);
	if (aux_input == AUX_NONE) {
	    set_text_attribute(conout, info.wAttributes);
	}

	/* Enable console input. */
	SetEvent(stdin_enable_event);
#endif /*]*/

	/* Wait for socket or console input. */
#if !defined(_WIN32) /*[*/
	do {
	    fd_set rfds;
	    int mfd = (s == INVALID_SOCKET)? infd: s;

	    FD_ZERO(&rfds);
	    FD_SET(0, &rfds);
	    FD_SET(mfd, &rfds);
	    select(mfd + 1, &rfds, NULL, NULL, NULL);
	    if (FD_ISSET(mfd, &rfds)) {
		/* Pipe input (EOF). */
		done = true;
		break;
	    }
	    if (FD_ISSET(0, &rfds)) {
		/* Keyboard input. */
# if defined(HAVE_LIBREADLINE) /*[*/
		rl_callback_read_char();
		if (!readline_done) {
		    /* No input yet. */
		    continue;
		}
		command = readline_command;
# else /*][*/
		command = fgets(inbuf, sizeof(inbuf), stdin);
# endif /*]*/
		if (command == NULL) {
		    done = true;
		}
		break;
	    }
	} while (true);

	if (done) {
# if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
# endif /*]*/
	    exit(0);
	}

# if defined(HAVE_LIBREADLINE) /*[*/
	readline_command = NULL;
	readline_done = false;
# endif /*]*/

# else /*][*/
	ha[0] = socket_event;
	ha[1] = stdin_done_event;
	rv = WaitForMultipleObjects(2, ha, FALSE, INFINITE);
	switch (rv) {
	    case WAIT_OBJECT_0:		/* socket close */
		exit(0);
		break;
	    case WAIT_OBJECT_0 + 1:	/* console input */
		if (stdin_nr <= 0) {
		    exit(0);
		}
		command = stdin_buf;
		break;
	    case WAIT_FAILED:
		win32_perror("WaitForMultipleObjects failed");
		exit(__LINE__);
		break;
	    default:
		fprintf(stderr, "Unexpected return %d from "
			"WaitForMultipleObjects\n", (int)rv);
		fflush(stderr);
		exit(__LINE__);
		break;
	}
#endif /*]*/

	/* We have a line of input. */
	if ((nl = strchr(command, '\n')) != NULL) {
	    *nl = '\0';
	}
	sl = strlen(command);
	if (sl > 0 && command[sl - 1] == '/') {
	    command[--sl] = '\0';
	    done = true;
	}
# if defined(HAVE_LIBREADLINE) /*[*/
	if (aux_input == AUX_NONE && command[0]) {
	    add_history(command);
	}
# endif /*]*/

	data_ret = NULL;
	errd_ret = NULL;
	prompt_ret = NULL;
	if (aux_input == AUX_NONE) {
	    single_io(0, 0, s, infd, outfd, NO_STATUS, command,
		    &data_ret, &errd_ret, &prompt_ret, &itype);
	} else {
	    char *command_base64 = base64_encode(command);
	    char *response = Malloc(strlen(command_base64) + 128);

	    if (response == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(__LINE__);
	    }
	    sprintf(response, RESUME_INPUT "(%s)",
		    command_base64[0]? command_base64: "\"\"");
	    Free(command_base64);
	    single_io(0, 0, s, infd, outfd, NO_STATUS, response,
		    &data_ret, &errd_ret, &prompt_ret, &itype);
	    Free(response);
	    Free(prompt);
	    prompt = real_prompt;
	    if (aux_input == AUX_PWINPUT) {
		echo_mode(true);
#if !defined(_WIN32) /*[*/
		fputc('\n', stdout);
#endif /*[*/
	    }
	    aux_input = AUX_NONE;
	}
# if defined(HAVE_LIBREADLINE) /*[*/
	Free(command);
# endif /*]*/

	if (prompt_ret != NULL) {
	    prompt = base64_decode(prompt_ret);
	    Free(prompt_ret);
	    aux_input = (itype == ITYPE_PWINPUT)? AUX_PWINPUT: AUX_INPUT;
	}

	/* Display the output or prompt. */
	if (data_ret != NULL) {
	    if ((sl = strlen(data_ret)) > 0 && data_ret[sl - 1] == '\n') {
		data_ret[sl - 1] = '\0';
	    }
	    if (*data_ret) {
		printf("%s\n", data_ret);
	    }
	    Free(data_ret);
	}
	if (errd_ret != NULL) {
	    if ((sl = strlen(errd_ret)) > 0 && errd_ret[sl - 1] == '\n') {
		errd_ret[sl - 1] = '\0';
	    }
	    if (*errd_ret) {
#if !defined(_WIN32) /*[*/
		printf("%s%s%s\n", xsetaf(setaf, color_offset + COLOR_RED, sgr), errd_ret, op);
#else /*][*/
		set_text_attribute(conout, FOREGROUND_INTENSITY | FOREGROUND_RED);
		fputs(errd_ret, stdout);
		fflush(stdout);
		set_text_attribute(conout, info.wAttributes);
		fputc('\n', stdout);
#endif /*]*/
	    }
	    Free(errd_ret);
	}
	fflush(stdout);

	if (done) {
	    exit(0);
	}
    }
}

void
Error(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}
