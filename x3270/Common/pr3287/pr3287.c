/*
 * Copyright (c) 2000-2009, Paul Mattes.
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
 * pr3287: A 3270 printer emulator for TELNET sessions.
 *
 *	pr3287 [options] [lu[,lu...]@]host[:port]
 *	Options are:
 *	    -dameon
 *		become a daemon after negotiating
 *	    -assoc session
 *		associate with a session (TN3270E only)
 *	    -command "string"
 *		command to use to print (default "lpr", POSIX only)
 *          -charset name
 *		use the specified character set
 *          -crlf
 *		expand newlines to CR/LF (POSIX only)
 *          -nocrlf
 *		expand newlines to CR/LF (Windows only)
 *          -blanklines
 *		display blank lines even if they're empty (formatted LU3)
 *          -eojtimeout n
 *              time out end of job after n seconds
 *          -ffthru
 *		pass through SCS FF orders
 *          -ffskip
 *		skip FF at top of page
 *	    -printer "printer name"
 *	        printer to use (default is $PRINTER or system default,
 *	        Windows only)
 *	    -printercp n
 *	        Code page to use for output (Windows only)
 *	    -proxy "spec"
 *	    	proxy specification
 *          -reconnect
 *		keep trying to reconnect
 *	    -trace
 *		trace data stream to a file
 *          -tracedir dir
 *              directory to write trace file in (POSIX only)
 *          -trnpre file
 *          	file of transparent data to send before jobs
 *          -trnpost file
 *          	file of transparent data to send after jobs
 *          -v
 *              display version information and exit
 *          -V
 *		verbose output about negotiation
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#if !defined(_WIN32) /*[*/
#include <syslog.h>
#include <netdb.h>
#endif /*]*/
#include <sys/types.h>
#if !defined(_MSC_VER) /*[*/
#include <unistd.h>
#endif /*]*/
#if !defined(_WIN32) /*[*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else /*][*/
#include <winsock2.h>
#include <ws2tcpip.h>
#undef AF_INET6
#endif /*]*/
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#include "globals.h"
#include "charsetc.h"
#include "trace_dsc.h"
#include "ctlrc.h"
#include "popupsc.h"
#include "proxyc.h"
#include "resolverc.h"
#include "telnetc.h"
#include "utf8c.h"
#if defined(_WIN32) /*[*/
#include "wsc.h"
#include "windirsc.h"
#endif /*]*/

#if defined(_IOLBF) /*[*/
#define SETLINEBUF(s)	setvbuf(s, (char *)NULL, _IOLBF, BUFSIZ)
#else /*][*/
#define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

#if !defined(INADDR_NONE) /*[*/
#define INADDR_NONE	0xffffffffL
#endif /*]*/

/* Externals. */
extern char *build;
extern FILE *tracef;
#if defined(_WIN32) /*[*/
extern void sockstart(void);
#endif /*]*/

/* Globals. */
char *programname = NULL;	/* program name */
int blanklines = 0;
int ignoreeoj = 0;
int reconnect = 0;
#if defined(_WIN32) /*[*/
int crlf = 1;
int printercp = 0;
#else /*][*/
int crlf = 0;
#endif /*]*/
int ffthru = 0;
int ffskip = 0;
int verbose = 0;
int ssl_host = 0;
unsigned long eoj_timeout = 0L; /* end of job timeout */
char *trnpre_data = NULL;
size_t trnpre_size = 0;
char *trnpost_data = NULL;
size_t trnpost_size = 0;

/* User options. */
#if !defined(_WIN32) /*[*/
static enum { NOT_DAEMON, WILL_DAEMON, AM_DAEMON } bdaemon = NOT_DAEMON;
#endif /*]*/
static char *assoc = NULL;	/* TN3270 session to associate with */
#if !defined(_WIN32) /*[*/
const char *command = "lpr";	/* command to run for printing */
#else /*][*/
const char *printer = NULL;	/* printer to use */
#endif /*]*/
static int tracing = 0;		/* are we tracing? */
#if !defined(_WIN32) /*[*/
static char *tracedir = "/tmp";	/* where we are tracing */
#endif /*]*/
char *proxy_spec;		/* proxy specification */

static int proxy_type = 0;
static char *proxy_host = CN;
static char *proxy_portname = CN;
static unsigned short proxy_port = 0;

#if defined(_WIN32) /*[*/
char appdata[MAX_PATH];
#endif /* ]*/

void pr3287_exit(int);
const char *build_options(void);

/* Print a usage message and exit. */
static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [options] [lu[,lu...]@]host[:port]\n"
"Options:\n%s%s%s%s", programname,
#if !defined(_WIN32) /*[*/
"  -daemon          become a daemon after connecting\n"
#endif /*]*/
"  -assoc <session> associate with a session (TN3270E only)\n"
"  -charset <name>  use built-in alternate EBCDIC-to-ASCII mappings\n",
#if !defined(_WIN32) /*[*/
"  -command \"<cmd>\" use <cmd> for printing (default \"lpr\")\n"
#endif /*]*/
"  -blanklines      display blank lines even if empty (formatted LU3)\n"
#if defined(_WIN32) /*[*/
"  -nocrlf          don't expand newlines to CR/LF\n"
#else /*][*/
"  -crlf            expand newlines to CR/LF\n"
#endif /*]*/
"  -eojtimeout <seconds>\n"
"                   time out end of print job\n"
"  -ffthru          pass through SCS FF orders\n"
"  -ffskip          skip FF orders at top of page\n",
"  -ignoreeoj       ignore PRINT-EOJ commands\n"
#if defined(_WIN32) /*[*/
"  -printer \"printer name\"\n"
"                   use specific printer (default is $PRINTER or the system\n"
"                   default printer)\n"
"  -printercp <codepage>\n"
"                   code page for output (default is system ANSI code page)\n"
#endif /*]*/
"  -proxy \"<spec>\"\n"
"                   connect to host via specified proxy\n"
"  -reconnect       keep trying to reconnect\n"
"  -trace           trace data stream to /tmp/x3trc.<pid>\n",
#if !defined(_WIN32) /*[*/
"  -tracedir <dir>  directory to keep trace information in\n"
#endif /*]*/
"  -trnpre <file>   file of transparent data to send before each job\n"
"  -trnpost <file>  file of transparent data to send after each job\n"
"  -v               display version information and exit\n"
"  -V               log verbose information about connection negotiation\n"
);
	pr3287_exit(1);
}

/* Print an error message. */
void
verrmsg(const char *fmt, va_list ap)
{
	static char buf[2][4096] = { "", "" };
	static int ix = 0;

	ix = !ix;
	(void) vsprintf(buf[ix], fmt, ap);
	trace_ds("Error: %s\n", buf[ix]);
	if (!strcmp(buf[ix], buf[!ix])) {
		if (verbose)
			(void) fprintf(stderr, "Suppressed error '%s'\n",
			    buf[ix]);
		return;
	}
#if !defined(_WIN32) /*[*/
	if (bdaemon == AM_DAEMON) {
		/* XXX: Need to put somethig in the Application Event Log. */
		syslog(LOG_ERR, "%s: %s", programname, buf[ix]);
	} else {
#endif /*]*/
		(void) fprintf(stderr, "%s: %s\n", programname, buf[ix]);
#if !defined(_WIN32) /*[*/
	}
#endif /*]*/
}

void
errmsg(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) verrmsg(fmt, args);
	va_end(args);
}

/* Memory allocation. */
void *
Malloc(size_t len)
{
	void *p = malloc(len);

	if (p == NULL) {
		errmsg("Out of memory");
		pr3287_exit(1);
	}
	return p;
}

void
Free(void *p)
{
	free(p);
}

void *
Realloc(void *p, size_t len)
{
	void *pn;

	pn = realloc(p, len);
	if (pn == NULL) {
		errmsg("Out of memory");
		pr3287_exit(1);
	}
	return pn;
}

char *
NewString(const char *s)
{
	char *p;


	p = Malloc(strlen(s) + 1);
	return strcpy(p, s);
}

/* Signal handler for SIGTERM, SIGINT and SIGHUP. */
static void
fatal_signal(int sig)
{
	/* Flush any pending data and exit. */
	trace_ds("Fatal signal %d\n", sig);
	(void) print_eoj();
	errmsg("Exiting on signal %d", sig);
	exit(0);
}

#if !defined(_WIN32) /*[*/
/* Signal handler for SIGUSR1. */
static void
flush_signal(int sig)
{
	/* Flush any pending data and exit. */
	trace_ds("Flush signal %d\n", sig);
	(void) print_eoj();
}
#endif /*]*/

void
pr3287_exit(int status)
{
#if defined(_WIN32) && defined(NEED_PAUSE) /*[*/
	char buf[2];

	if (status) {
		printf("\n[Press <Enter>] ");
		fflush(stdout);
		(void) fgets(buf, 2, stdin);
	}
#endif /*]*/
	exit(status);
}

/* Read a transparent data file into memory. */
static void
read_trn(char *filename, char **data, size_t *size)
{
    	int fd;
	char buf[1024];
	int nr;

	if (filename == NULL)
	    	return;
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror(filename);
		pr3287_exit(1);
	}
	*size = 0;
	while ((nr = read(fd, buf, sizeof(buf))) > 0) {
	    	*size += nr;
	    	*data = realloc(*data, *size);
		if (*data == NULL) {
		    	fprintf(stderr, "Out of memory\n");
			pr3287_exit(1);
		}
		memcpy(*data + *size - nr, buf, nr);
	}
	if (nr < 0) {
		perror(filename);
		pr3287_exit(1);
	}
	close(fd);
}

int
main(int argc, char *argv[])
{
	int i;
	char *at, *colon;
	int len;
	char *charset = "us";
	char *lu = NULL;
	char *host = NULL;
	char *port = "telnet";
	unsigned short p;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if defined(AF_INET6) /*[*/
		struct sockaddr_in6 sin6;
#endif /*]*/
	} ha;
	socklen_t ha_len = sizeof(ha);
	int s = -1;
	int rc = 0;
	int report_success = 0;
#if defined(HAVE_LIBSSL) /*[*/
	int any_prefixes = False;
#endif /*]*/
	char *trnpre = NULL, *trnpost = NULL;

	/* Learn our name. */
#if defined(_WIN32) /*[*/
	if ((programname = strrchr(argv[0], '\\')) != NULL)
#else /*][*/
	if ((programname = strrchr(argv[0], '/')) != NULL)
#endif /*]*/
		programname++;
	else
		programname = argv[0];
#if !defined(_WIN32) /*[*/
	if (!programname[0])
		programname = "wpr3287";
#endif /*]*/

#if defined(_WIN32) /*[*/
	/*
	 * Get the printer name via the environment, because Windows doesn't
	 * let us put spaces in arguments.
	 */
	if ((printer = getenv("PRINTER")) == NULL) {
		printer = ws_default_printer();
	}

	if (get_dirs(NULL, appdata, "wc3270") < 0)
	    	exit(1);
#endif /*]*/

	/* Gather the options. */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
#if !defined(_WIN32) /*[*/
		if (!strcmp(argv[i], "-daemon"))
			bdaemon = WILL_DAEMON;
		else
#endif /*]*/
		if (!strcmp(argv[i], "-assoc")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -assoc\n");
				usage();
			}
			assoc = argv[i + 1];
			i++;
#if !defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-command")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -command\n");
				usage();
			}
			command = argv[i + 1];
			i++;
#endif /*]*/
		} else if (!strcmp(argv[i], "-charset")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -charset\n");
				usage();
			}
			charset = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-blanklines")) {
			blanklines = 1;
#if defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-nocrlf")) {
			crlf = 0;
#else /*][*/
		} else if (!strcmp(argv[i], "-crlf")) {
			crlf = 1;
#endif /*]*/
		} else if (!strcmp(argv[i], "-eojtimeout")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -charset\n");
				usage();
			}
			eoj_timeout = strtoul(argv[i + 1], NULL, 0);
			i++;
		} else if (!strcmp(argv[i], "-ignoreeoj")) {
			ignoreeoj = 1;
		} else if (!strcmp(argv[i], "-ffthru")) {
			ffthru = 1;
		} else if (!strcmp(argv[i], "-ffskip")) {
			ffskip = 1;
#if defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-printer")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -printer\n");
				usage();
			}
			printer = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-printercp")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -printer\n");
				usage();
			}
			printercp = (int)strtoul(argv[i + 1], NULL, 0);
			i++;
#endif /*]*/
		} else if (!strcmp(argv[i], "-reconnect")) {
			reconnect = 1;
		} else if (!strcmp(argv[i], "-v")) {
			printf("%s\n%s\n", build, build_options());
			exit(0);
		} else if (!strcmp(argv[i], "-V")) {
			verbose = 1;
		} else if (!strcmp(argv[i], "-trace")) {
			tracing = 1;
#if !defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-tracedir")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -tracedir\n");
				usage();
			}
			tracedir = argv[i + 1];
			i++;
#endif /*]*/
		} else if (!strcmp(argv[i], "-trnpre")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -trnpre\n");
				usage();
			}
			trnpre = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-trnpost")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -trnpost\n");
				usage();
			}
			trnpost = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-proxy")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -proxy\n");
				usage();
			}
			proxy_spec = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "--help")) {
			usage();
		} else
			usage();
	}
	if (argc != i + 1)
		usage();

	/* Pick apart the hostname, LUs and port. */
#if defined(_WIN32) /*[*/
	sockstart();
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
	do {
		if (!strncasecmp(argv[i], "l:", 2)) {
			ssl_host = True;
			argv[i] += 2;
			any_prefixes = True;
		} else
			any_prefixes = False;
	} while (any_prefixes);
#endif /*]*/
	if ((at = strchr(argv[i], '@')) != NULL) {
		len = at - argv[i];
		if (!len)
			usage();
		lu = Malloc(len + 1);
		(void) strncpy(lu, argv[i], len);
		lu[len] = '\0';
		host = at + 1;
	} else
		host = argv[i];

	/*
	 * Allow the hostname to be enclosed in '[' and ']' to quote any
	 * IPv6 numeric-address ':' characters.
	 */
	if (host[0] == '[') {
		char *tmp;
		char *rbracket;

		rbracket = strchr(host+1, ']');
		if (rbracket != NULL) {
			len = rbracket - (host+1);
			tmp = Malloc(len + 1);
			(void) strncpy(tmp, host+1, len);
			tmp[len] = '\0';
			host = tmp;
		}
		if (*(rbracket + 1) == ':') {
			port = rbracket + 2;
		}
	} else {
		colon = strchr(host, ':');
		if (colon != NULL) {
			char *tmp;

			len = colon - host;
			if (!len || !*(colon + 1))
				usage();
			port = colon + 1;
			tmp = Malloc(len + 1);
			(void) strncpy(tmp, host, len);
			tmp[len] = '\0';
			host = tmp;
		}
	}

#if defined(_WIN32) /*[*/
	/* Set the printer code page. */
	if (printercp == 0)
	    	printercp = GetACP();
#endif /*]*/

	/* Set up the character set. */
	if (charset_init(charset) < 0)
		pr3287_exit(1);

	/* Read in the transparent pre- and post- files. */
	read_trn(trnpre, &trnpre_data, &trnpre_size);
	read_trn(trnpost, &trnpost_data, &trnpost_size);

	/* Try opening the trace file, if there is one. */
	if (tracing) {
		char tracefile[256];
		time_t clk;
		int i;

#if defined(_WIN32) /*[*/
		(void) sprintf(tracefile, "%sx3trc.%d.txt", appdata, getpid());
#else /*][*/
		(void) sprintf(tracefile, "%s/x3trc.%d", tracedir, getpid());
#endif /*]*/
		tracef = fopen(tracefile, "a");
		if (tracef == NULL) {
			perror(tracefile);
			pr3287_exit(1);
		}
		(void) SETLINEBUF(tracef);
		clk = time((time_t *)0);
		(void) fprintf(tracef, "Trace started %s", ctime(&clk));
		(void) fprintf(tracef, " Version: %s\n %s\n", build,
			       build_options());
#if !defined(_WIN32) /*[*/
		(void) fprintf(tracef, " Locale codeset: %s\n", locale_codeset);
#else /*][*/
		(void) fprintf(tracef, " ANSI codepage: %d, "
			       "printer codepage: %d\n", GetACP(), printercp);
#endif /*]*/
		(void) fprintf(tracef, " Host codepage: %d",
			       (int)(cgcsgid & 0xffff));
#if defined(X3270_DBCS) /*[*/
		if (dbcs)
			(void) fprintf(tracef, "+%d",
				       (int)(cgcsgid_dbcs & 0xffff));
#endif /*]*/
		(void) fprintf(tracef, "\n");
		(void) fprintf(tracef, " Command:");
		for (i = 0; i < argc; i++) {
			(void) fprintf(tracef, " %s", argv[i]);
		}
		(void) fprintf(tracef, "\n");
	}

#if !defined(_WIN32) /*[*/
	/* Become a daemon. */
	if (bdaemon != NOT_DAEMON) {
		switch (fork()) {
			case -1:
				perror("fork");
				exit(1);
				break;
			case 0:
				/* Child: Break away from the TTY. */
				if (setsid() < 0)
					exit(1);
				bdaemon = AM_DAEMON;
				break;
			default:
				/* Parent: We're all done. */
				exit(0);
				break;

		}
	}
#endif /*]*/

	/* Handle signals. */
	(void) signal(SIGTERM, fatal_signal);
	(void) signal(SIGINT, fatal_signal);
#if !defined(_WIN32) /*[*/
	(void) signal(SIGHUP, fatal_signal);
	(void) signal(SIGUSR1, flush_signal);
	(void) signal(SIGPIPE, SIG_IGN);
#endif /*]*/

	/* Set up the proxy. */
	if (proxy_spec != CN) {
	    	proxy_type = proxy_setup(&proxy_host, &proxy_portname);
		if (proxy_type < 0)
			pr3287_exit(1);
	}

	/*
	 * One-time initialization is now complete.
	 * (Most) everything beyond this will now be retried, if the -reconnect
	 * option is in effect.
	 */
	for (;;) {
		char errtxt[1024];

		/* Resolve the host name. */
	    	if (proxy_type > 0) {
		    	unsigned long lport;
			char *ptr;
			struct servent *sp;

			if (resolve_host_and_port(proxy_host, proxy_portname,
				    &proxy_port, &ha.sa, &ha_len, errtxt,
				    sizeof(errtxt)) < 0) {
			    popup_an_error("%s/%s: %s", proxy_host,
				    proxy_portname, errtxt);
			    rc = 1;
			    goto retry;
			}

			lport = strtoul(port, &ptr, 0);
			if (ptr == port || *ptr != '\0' || lport == 0L ||
				    lport & ~0xffff) {
				if (!(sp = getservbyname(port, "tcp"))) {
					popup_an_error("Unknown port number "
						"or service: %s", port);
					rc = 1;
					goto retry;
				}
				p = ntohs(sp->s_port);
			} else
				p = (unsigned short)lport;
		} else {
			if (resolve_host_and_port(host, port, &p, &ha.sa,
				    &ha_len, errtxt, sizeof(errtxt)) < 0) {
			    popup_an_error("%s/%s: %s", host, port, errtxt);
			    rc = 1;
			    goto retry;
			}
		}

		/* Connect to the host. */
		s = socket(ha.sa.sa_family, SOCK_STREAM, 0);
		if (s < 0) {
			popup_a_sockerr("socket");
			pr3287_exit(1);
		}

		if (connect(s, &ha.sa, ha_len) < 0) {
			popup_a_sockerr("%s", (proxy_type > 0)? proxy_host:
								host);
			rc = 1;
			goto retry;
		}

		if (proxy_type > 0) {
		    	/* Connect to the host through the proxy. */
		    	if (verbose) {
			    	(void) fprintf(stderr, "Connected to proxy "
					       "server %s, port %u\n",
					       proxy_host, proxy_port);
			}
		    	if (proxy_negotiate(proxy_type, s, host, p) < 0) {
				rc = 1;
				goto retry;
			}
		}

		/* Say hello. */
		if (verbose) {
			(void) fprintf(stderr, "Connected to %s, port %u%s\n",
			    host, p,
			    ssl_host? " via SSL": "");
			if (assoc != NULL)
				(void) fprintf(stderr, "Associating with LU "
				    "%s\n", assoc);
			else if (lu != NULL)
				(void) fprintf(stderr, "Connecting to LU %s\n",
				    lu);
#if !defined(_WIN32) /*[*/
			(void) fprintf(stderr, "Command: %s\n", command);
#else /*][*/
			(void) fprintf(stderr, "Printer: %s\n",
				       printer? printer: "(none)");
#endif /*]*/
		}

		/* Negotiate. */
		if (negotiate(s, lu, assoc) < 0) {
			rc = 1;
			goto retry;
		}

		/* Report sudden success. */
		if (report_success) {
			errmsg("Connected to %s, port %u", host, p);
			report_success = 0;
		}

		/* Process what we're told to process. */
		if (process(s) < 0) {
			rc = 1;
			if (verbose)
				(void) fprintf(stderr,
				    "Disconnected (error).\n");
			goto retry;
		}
		if (verbose)
			(void) fprintf(stderr, "Disconnected (eof).\n");

	    retry:
		/* Flush any pending data. */
		(void) print_eoj();

		/* Close the socket. */
		if (s >= 0) {
			net_disconnect();
			s = -1;
		}

		if (!reconnect)
			break;
		report_success = 1;

		/* Wait a while, to reduce thrash. */
		if (rc)
#if !defined(_WIN32) /*[*/
			sleep(5);
#else /*][*/
			Sleep(5 * 1000000);
#endif /*]*/

		rc = 0;
	}

	pr3287_exit(rc);

	return rc;
}

/* Tracing function. */
void
trace_str(const char *s)
{
	if (tracef)
		fprintf(tracef, "%s", s);
}

/* Error pop-ups. */
void
popup_an_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) verrmsg(fmt, args);
	va_end(args);
}

void
popup_an_errno(int err, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (err > 0) {
		char msgbuf[4096];

		(void) vsprintf(msgbuf, fmt, args);
		errmsg("%s: %s", msgbuf, strerror(err));

	} else {
		(void) verrmsg(fmt, args);
	}
	va_end(args);
}

const char *
build_options(void)
{
    	return "Options:"
#if defined(X3270_DBCS) /*[*/
	    " --enable-dbcs"
#else /*][*/
	    " --disable-dbcs"
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
	    " --with-ssl"
#else /*][*/
	    " --without-ssl"
#endif /*]*/
#if defined(USE_ICONV) /*[*/
	    " --with-iconv"
#else /*][*/
	    " --without-iconv"
#endif /*]*/
	    ;
}
