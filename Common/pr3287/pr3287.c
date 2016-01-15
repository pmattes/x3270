/*
 * Copyright (c) 2000-2016 Paul Mattes.
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
 *	    -accepthostname any|DNS:name|IP:address
 *	        accept any certificate hostname, or a specific name, or an
 *	        IP address
 *	    -assoc session
 *		associate with a session (TN3270E only)
 *	    -cadir dir
 *	    -cafile file
 *	    -certfile file
 *	    -certfiletype type
 *	    -chainfile file
 *	    -command "string"
 *		command to use to print (default "lpr", POSIX only)
 *          -charset name
 *		use the specified character set
 *          -crlf
 *		expand newlines to CR/LF (POSIX only)
 *          -crthru
 *              pass through CRs in unformatted 3270 mode
 *	    -dameon
 *		become a daemon after negotiating
 *          -blanklines
 *		display blank lines even if they're empty (formatted LU3)
 *	    -emflush
 *	        flush printer output when an unformatted EM order arrives
 *	        (historical option, now the default)
 *	    -noemflush
 *	        do not flush printer output when an unformatted EM order arrives
 *          -eojtimeout n
 *              time out end of job after n seconds
 *          -ffeoj
 *          	assume an FF at the end of each job
 *          -ffthru
 *		pass through SCS FF orders
 *          -ffskip
 *		skip FF at top of page
 *          -keyfile file
 *          -keyfiletype type
 *          -keypasswd type:text
 *          -mpp n
 *              set the maximum presentation position (unformatted line length)
 *          -nocrlf
 *		expand newlines to CR/LF (Windows only)
 *	    -printer "printer name"
 *	        printer to use (default is $PRINTER or system default,
 *	        Windows only)
 *	    -printercp n
 *	        Code page to use for output (Windows only)
 *	    -proxy "spec"
 *	    	proxy specification
 *          -reconnect
 *		keep trying to reconnect
 *	    -selfsignedok
 *	        allow self-signed host certificates
 *	    -skipcc
 *	    	skip ASA carriage control characters in host output
 *          -syncport port
 *              TCP port for login session synchronization
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
 *          -verifycert
 *          	verify host certificates for SSL or SSL/TLS connections
 *          -V
 *		verbose output about negotiation
 *	    -xtable file
 *	        custom EBCDIC-to-ASCII translation table
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#if !defined(_WIN32) /*[*/
# include <syslog.h>
# include <netdb.h>
#endif /*]*/
#include <sys/types.h>
#if !defined(_MSC_VER) /*[*/
# include <unistd.h>
#endif /*]*/
#if !defined(_WIN32) /*[*/
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#else /*][*/
# include <winsock2.h>
# include <ws2tcpip.h>
# undef AF_INET6
#endif /*]*/
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#include "globals.h"
#include "charset.h"
#include "trace.h"
#include "ctlrc.h"
#include "popups.h"
#include "proxy.h"
#include "pr_telnet.h"
#include "resolver.h"
#include "telnet_core.h"
#include "utf8.h"
#include "utils.h"
#include "xtablec.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "wsc.h"
# include "windirs.h"
#endif /*]*/

#include "pr3287.h"

#if defined(_IOLBF) /*[*/
# define SETLINEBUF(s)	setvbuf(s, NULL, _IOLBF, BUFSIZ)
#else /*][*/
# define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

#if !defined(INADDR_NONE) /*[*/
# define INADDR_NONE	0xffffffffL
#endif /*]*/

/* Globals. */
options_t options;
socket_t syncsock = INVALID_SOCKET;
#if defined(_WIN32) /*[*/
char *instdir;
#endif /* ]*/

/* Locals. */
static char *programname = NULL;
static int proxy_type = 0;
static char *proxy_host = NULL;
static char *proxy_portname = NULL;
static unsigned short proxy_port = 0;

void pr3287_exit(int);
const char *build_options(void);

/* Print a usage message and exit. */
static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [options] [lu[,lu...]@]host[:port]\n"
"Options:\n%s%s%s%s%s%s%s%s%s", programname,
#if defined(HAVE_LIBSSL) /*[*/
"  -accepthostname any|DNS:name|IP:addr\n"
"                   accept any name, specific name or address in host cert\n"
#endif /*]*/
"  -assoc <session> associate with a session (TN3270E only)\n",
#if defined(HAVE_LIBSSL) /*[*/
"  -cadir <dir>     find CA certificate database in <dir>\n"
"  -cafile <file>   find CA certificates in <file>\n"
"  -certfile <file> find client certificate in <file>\n"
"  -certfiletype pem|asn1\n"
"                   specify client certificate file type\n"
"  -chainfile <file>\n"
"                   specify client certificate chain file\n"
#endif /*]*/
"  -charset <name>  use built-in alternate EBCDIC-to-ASCII mappings\n",
#if !defined(_WIN32) /*[*/
"  -command \"<cmd>\" use <cmd> for printing (default \"lpr\")\n"
#endif /*]*/
"  -blanklines      display blank lines even if empty (formatted LU3)\n",
#if !defined(_WIN32) /*[*/
"  -daemon          become a daemon after connecting\n"
#endif /*]*/
"  -emflush         flush printer output when an unformatted EM order arrives\n"
"                   (historical option; this is now the default)\n"
"  -noemflush       do not flush printer output when an unformatted EM order\n"
"                   arrives\n"
#if defined(_WIN32) /*[*/
"  -nocrlf          don't expand newlines to CR/LF\n"
#else /*][*/
"  -crlf            expand newlines to CR/LF\n"
#endif /*]*/
"  -crthru          pass through CRs in unformatted 3270 mode\n"
"  -eojtimeout <seconds>\n"
"                   time out end of print job\n",
"  -ffeoj           assume FF at the end of each print job\n"
"  -ffthru          pass through SCS FF orders\n",
"  -ffskip          skip FF orders at top of page\n"
#if defined(HAVE_LIBSSL) /*[*/
"  -keyfile <file>  find certificate private key in <file>\n"
"  -keyfiletype pem|asn1\n"
"                   specify private key file type\n"
"  -keypasswd file:<file>|string:<string>\n"
"                   specify private key password\n"
#endif /*]*/
"  -ignoreeoj       ignore PRINT-EOJ commands\n",
"  -mpp <n>         define the Maximum Presentation Position (unformatted\n"
"                   line length)\n"
#if defined(_WIN32) /*[*/
"  -printer \"printer name\"\n"
"                   use specific printer (default is $PRINTER or the system\n"
"                   default printer)\n"
"  -printercp <codepage>\n"
"                   code page for output (default is system ANSI code page)\n"
#endif /*]*/
"  -proxy \"<spec>\"\n"
"                   connect to host via specified proxy\n"
"  -reconnect       keep trying to reconnect\n",
#if defined(HAVE_LIBSSL) /*[*/
"  -selfsignedok    allow self-signed host SSL certificates\n"
#endif /*]*/
"  -skipcc          skip ASA carriage control characters in unformatted host\n"
"                   output\n"
"  -syncport port   TCP port for login session synchronization\n"
#if defined(_WIN32) /*[*/
"  -trace           trace data stream to <wc3270appData>/x3trc.<pid>.txt\n",
#else /*][*/
"  -trace           trace data stream to /tmp/x3trc.<pid>\n",
#endif /*]*/
"  -tracedir <dir>  directory to keep trace information in\n"
"  -trnpre <file>   file of transparent data to send before each job\n"
"  -trnpost <file>  file of transparent data to send after each job\n"
"  -v               display version information and exit\n"
#if defined(HAVE_LIBSSL) /*[*/
"  -verfycert       verify host certificate for SSL and SSL/TLS connections\n"
#endif /*]*/
"  -V               log verbose information about connection negotiation\n"
"  -xtable <file>   specify a custom EBCDIC-to-ASCII translation table\n"
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
	vtrace("Error: %s\n", buf[ix]);
	if (!strcmp(buf[ix], buf[!ix])) {
		if (options.verbose)
			(void) fprintf(stderr, "Suppressed error '%s'\n",
			    buf[ix]);
		return;
	}
#if !defined(_WIN32) /*[*/
	if (options.bdaemon == AM_DAEMON) {
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

/* xs_warning() is an alias for errmsg() */
void
xs_warning(const char *fmt, ...)
{
    va_list args;
    char *b;

    va_start(args, fmt);
    b = xs_vbuffer(fmt, args);
    va_end(args);
    errmsg("%s", b);
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

void
Error(const char *msg)
{
    errmsg(msg);
    pr3287_exit(1);
}

/* Signal handler for SIGTERM, SIGINT and SIGHUP. */
static void
fatal_signal(int sig)
{
	/* Flush any pending data and exit. */
	vtrace("Fatal signal %d\n", sig);
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
	vtrace("Flush signal %d\n", sig);
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

	/* Close the synchronization socket gracefully. */
	if (syncsock != INVALID_SOCKET) {
		SOCK_CLOSE(syncsock);
		syncsock = INVALID_SOCKET;
	}

	exit(status);
}

static void
init_options(void)
{
    	/* Clear them all out, just in case. */
    	memset(&options, '\0', sizeof(options));

	/* Set individual defaults. */
	options.assoc			= NULL;
#if !defined(_WIN32) /*[*/
	options.bdaemon			= NOT_DAEMON;
#endif /*]*/
	options.blanklines		= 0;
	options.charset			= "us";
#if !defined(_WIN32) /*[*/
	options.command			= "lpr";
#endif /*]*/
#if !defined(_WIN32) /*[*/
	options.crlf			= 0;
#else /*][*/
	options.crlf			= 1;
#endif /*]*/
	options.crthru			= 0;
	options.emflush			= 1;
	options.eoj_timeout		= 0L;
	options.ffeoj			= 0;
	options.ffthru			= 0;
	options.ffskip			= 0;
	options.ignoreeoj		= 0;
#if defined(_WIN32) /*[*/
	if ((options.printer = getenv("PRINTER")) == NULL)
		options.printer = ws_default_printer();
	options.printercp		= 0;
#endif /*]*/
	options.proxy_spec		= NULL;
	options.reconnect		= 0;
	options.skipcc			= 0;
	options.mpp			= DEFAULT_UNF_MPP;
#if defined(HAVE_LIBSSL) /*[*/
	options.ssl.accept_hostname	= NULL;
	options.ssl.ca_dir		= NULL;
	options.ssl.ca_file		= NULL;
	options.ssl.cert_file		= NULL;
	options.ssl.cert_file_type	= NULL;
	options.ssl.chain_file		= NULL;
	options.ssl.key_file		= NULL;
	options.ssl.key_file_type	= NULL;
	options.ssl.key_passwd		= NULL;
	options.ssl.self_signed_ok	= 0;
	options.ssl.ssl_host		= 0;
	options.ssl.verify_cert		= 0;
#endif /*]*/
	options.syncport		= 0;
#if !defined(_WIN32) /*[*/
	options.tracedir		= "/tmp";
#else /*][*/
	options.tracedir		= NULL;
#endif /*]*/
	options.tracing			= 0;
	options.trnpre			= NULL;
	options.trnpost			= NULL;
	options.verbose			= 0;
}

int
main(int argc, char *argv[])
{
	int i;
	char *at, *colon;
	size_t len;
	char *lu = NULL;
	char *host = NULL;
	char *port = "23";
	char *xtable = NULL;
	unsigned short p;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if defined(AF_INET6) && defined(X3270_IPV6) /*[*/
		struct sockaddr_in6 sin6;
#endif /*]*/
	} ha;
	socklen_t ha_len = sizeof(ha);
	socket_t s = INVALID_SOCKET;
	int rc = 0;
	int report_success = 0;

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
	if (!get_dirs("wc3270", &instdir, NULL, NULL, NULL, NULL, NULL, NULL,
		    NULL, NULL, NULL))
	    	exit(1);

	if (sockstart() < 0)
	    	exit(1);
#endif /*]*/

	/* Gather the options. */
	init_options();
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
#if !defined(_WIN32) /*[*/
		if (!strcmp(argv[i], "-daemon"))
			options.bdaemon = WILL_DAEMON;
		else
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
		if (!strcmp(argv[i], "-accepthostname")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -accepthostname\n");
				usage();
			}
			options.ssl.accept_hostname = argv[i + 1];
			i++;
		} else
#endif /*]*/
		if (!strcmp(argv[i], "-assoc")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -assoc\n");
				usage();
			}
			options.assoc = argv[i + 1];
			i++;
		} else
#if !defined(_WIN32) /*[*/
		if (!strcmp(argv[i], "-command")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -command\n");
				usage();
			}
			options.command = argv[i + 1];
			i++;
		} else
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
		if (!strcmp(argv[i], "-cadir")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -cadir\n");
				usage();
			}
			options.ssl.ca_dir = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-cafile")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -cafile\n");
				usage();
			}
			options.ssl.ca_file = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-certfile")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -certfile\n");
				usage();
			}
			options.ssl.cert_file = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-certfiletype")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -certfiletype\n");
				usage();
			}
			options.ssl.cert_file_type = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-chainfile")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -chainfile\n");
				usage();
			}
			options.ssl.chain_file = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-keyfile")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -keyfile\n");
				usage();
			}
			options.ssl.key_file = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-keyfiletype")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -keyfiletype\n");
				usage();
			}
			options.ssl.key_file_type = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-keypasswd")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -keypasswd\n");
				usage();
			}
			options.ssl.key_passwd = argv[i + 1];
			i++;
		} else
#endif /*]*/
		if (!strcmp(argv[i], "-charset")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -charset\n");
				usage();
			}
			options.charset = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-blanklines")) {
			options.blanklines = 1;
		} else if (!strcmp(argv[i], "-emflush")) {
			options.emflush = 1;
		} else if (!strcmp(argv[i], "-noemflush")) {
			options.emflush = 0;
#if defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-nocrlf")) {
			options.crlf = 0;
#else /*][*/
		} else if (!strcmp(argv[i], "-crlf")) {
			options.crlf = 1;
#endif /*]*/
		} else if (!strcmp(argv[i], "-crthru")) {
			options.crthru = 1;
		} else if (!strcmp(argv[i], "-eojtimeout")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -eojtimeout\n");
				usage();
			}
			options.eoj_timeout = strtoul(argv[i + 1], NULL, 0);
			i++;
		} else if (!strcmp(argv[i], "-ignoreeoj")) {
			options.ignoreeoj = 1;
		} else if (!strcmp(argv[i], "-ffeoj")) {
			options.ffeoj = 1;
		} else if (!strcmp(argv[i], "-ffthru")) {
			options.ffthru = 1;
		} else if (!strcmp(argv[i], "-ffskip")) {
			options.ffskip = 1;
#if defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-printer")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -printer\n");
				usage();
			}
			options.printer = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-printercp")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -printercp\n");
				usage();
			}
			options.printercp = (int)strtoul(argv[i + 1], NULL, 0);
			i++;
#endif /*]*/
		} else if (!strcmp(argv[i], "-mpp")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -mpp\n");
				usage();
			}
			options.mpp = (int)strtoul(argv[i + 1], NULL, 0);
			if (options.mpp < MIN_UNF_MPP ||
			    options.mpp > MAX_UNF_MPP) {
				(void) fprintf(stderr,
				    "Invalid for -mpp\n");
				usage();
			}
			i++;
		} else if (!strcmp(argv[i], "-reconnect")) {
			options.reconnect = 1;
#if defined(HAVE_LIBSSL) /*[*/
		} else if (!strcmp(argv[i], "-selfsignedok")) {
		    	options.ssl.self_signed_ok = 1;
#endif /*]*/
		} else if (!strcmp(argv[i], "-v")) {
			printf("%s\n%s\n", build, build_options());
			charset_list();
			printf("\n\
Copyright 1989-%s, Paul Mattes, GTRC and others.\n\
See the source code or documentation for licensing details.\n\
Distributed WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n", cyear);
			exit(0);
#if defined(HAVE_LIBSSL) /*[*/
		} else if (!strcmp(argv[i], "-verifycert")) {
		    	options.ssl.verify_cert = 1;
#endif /*]*/
		} else if (!strcmp(argv[i], "-V")) {
			options.verbose = 1;
		} else if (!strcmp(argv[i], "-syncport")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -syncport\n");
				usage();
			}
			options.syncport = (int)strtoul(argv[i + 1], NULL, 0);
			i++;
		} else if (!strcmp(argv[i], "-trace")) {
			options.tracing = 1;
		} else if (!strcmp(argv[i], "-tracedir")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -tracedir\n");
				usage();
			}
			options.tracedir = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-trnpre")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -trnpre\n");
				usage();
			}
			options.trnpre = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-trnpost")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -trnpost\n");
				usage();
			}
			options.trnpost = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-proxy")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -proxy\n");
				usage();
			}
			options.proxy_spec = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-xtable")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -xtable\n");
				usage();
			}
			xtable = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-skipcc")) {
			options.skipcc = 1;
		} else if (!strcmp(argv[i], "--help")) {
			usage();
		} else
			usage();
	}
	if (argc != i + 1)
		usage();

	/*
	 * Pick apart the hostname, LUs and port.
	 * We allow "L:" and "<luname>@" in either order.
	 */
	host = argv[i];
#if defined(HAVE_LIBSSL) /*[*/
	while (!strncasecmp(host, "l:", 2)) {
		options.ssl.ssl_host = true;
		host += 2;
	}
#endif /*]*/
	if ((at = strchr(host, '@')) != NULL) {
		len = at - host;
		if (!len)
			usage();
		lu = Malloc(len + 1);
		(void) strncpy(lu, host, len);
		lu[len] = '\0';
		host = at + 1;
	}
#if defined(HAVE_LIBSSL) /*[*/
	while (!strncasecmp(host, "l:", 2)) {
		options.ssl.ssl_host = true;
		host += 2;
	}
#endif /*]*/
	if (!*host) {
		usage();
	}

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
	if (options.printercp == 0)
	    	options.printercp = GetACP();
#endif /*]*/

	/* Set up the character set. */
	if (charset_init(options.charset) != CS_OKAY) {
		pr3287_exit(1);
	}

	/* Set up the custom translation table. */
	if (xtable != NULL && xtable_init(xtable) < 0)
		pr3287_exit(1);

	/* Try opening the trace file, if there is one. */
	if (options.tracing) {
		char tracefile[4096];
		time_t clk;
		int i;
		int u = 0;
		int fd;
#if defined(_WIN32) /*[*/
		size_t sl;
#endif /*]*/

		do {
			char dashu[32];

			if (u) {
			    	snprintf(dashu, sizeof(dashu), "-%d", u);
			} else {
				dashu[0] = '\0';
			}

#if defined(_WIN32) /*[*/
			if (options.tracedir == NULL) {
				options.tracedir = "";
			}
			sl = strlen(options.tracedir);
			(void) snprintf(tracefile, sizeof(tracefile),
				"%s%sx3trc.%d%s.txt",
				options.tracedir,
				sl? ((options.tracedir[sl - 1] == '\\')?
				    "": "\\"): "",
				getpid(), dashu);
#else /*][*/
			(void) snprintf(tracefile, sizeof(tracefile),
				"%s/x3trc.%u%s",
				options.tracedir, (unsigned)getpid(), dashu);
#endif /*]*/
			fd = open(tracefile, O_WRONLY | O_CREAT | O_EXCL, 0600);
			if (fd < 0) {
				if (errno != EEXIST) {
					perror(tracefile);
					pr3287_exit(1);
				}
				u++;
			}
		} while (fd < 0);

		tracef = fdopen(fd, "w");
		if (tracef == NULL) {
			perror(tracefile);
			pr3287_exit(1);
		}
		(void) SETLINEBUF(tracef);
		clk = time((time_t *)0);
		vtrace_nts("Trace started %s", ctime(&clk));
		vtrace_nts(" Version: %s\n %s\n", build, build_options());
#if !defined(_WIN32) /*[*/
		vtrace_nts(" Locale codeset: %s\n", locale_codeset);
#else /*][*/
		vtrace_nts(" ANSI codepage: %d, printer codepage: %d\n",
			GetACP(), options.printercp);
#endif /*]*/
		vtrace_nts(" Host codepage: %d", (int)(cgcsgid & 0xffff));
		if (dbcs) {
			vtrace_nts("+%d", (int)(cgcsgid_dbcs & 0xffff));
		}
		vtrace_nts("\n");
		vtrace_nts(" Command:");
		for (i = 0; i < argc; i++) {
			vtrace_nts(" %s", argv[i]);
		}
		vtrace_nts("\n");
#if defined(_WIN32) /*[*/
		vtrace_nts(" Instdir: %s\n", instdir? instdir: "(null)");
#endif /*]*/

		/* Dump the translation table. */
		if (xtable != NULL) {
			int ebc;
			unsigned char *x;

			vtrace_nts("Translation table:\n");
			for (ebc = 0; ebc <= 0xff; ebc++) {
				int nx = xtable_lookup(ebc, &x);

				if (nx >= 0) {
					int j;

					vtrace_nts(" ebcdic X'%02X' ascii",
						ebc);

					for (j = 0; j < nx; j++) {
						vtrace_nts(" 0x%02x",
							(unsigned char)x[j]);
					}
					vtrace_nts("\n");
				}
			}
		}
	}

#if defined(HAVE_LIBSSL) /*[*/
	pr_ssl_base_init();
#endif /*]*/

#if !defined(_WIN32) /*[*/
	/* Become a daemon. */
	if (options.bdaemon != NOT_DAEMON) {
		switch (fork()) {
			case -1:
				perror("fork");
				exit(1);
				break;
			case 0:
				/* Child: Break away from the TTY. */
				if (setsid() < 0)
					exit(1);
				options.bdaemon = AM_DAEMON;
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
	if (options.proxy_spec != NULL) {
	    	proxy_type = proxy_setup(options.proxy_spec, &proxy_host,
			&proxy_portname);
		if (proxy_type < 0)
			pr3287_exit(1);
	}

	/* Set up the synchronization socket. */
	if (options.syncport) {
		struct sockaddr_in sin;

		memset(&sin, '\0', sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sin.sin_port = htons(options.syncport);

		syncsock = socket(PF_INET, SOCK_STREAM, 0);
		if (syncsock == INVALID_SOCKET) {
			popup_a_sockerr("socket(syncsock)");
			pr3287_exit(1);
		}
		if (connect(syncsock, (struct sockaddr *)&sin,
			    sizeof(sin)) < 0) {
			popup_a_sockerr("connect(syncsock)");
			pr3287_exit(1);
		}
		vtrace("Connected to sync port %d.\n", options.syncport);
	}

	/*
	 * One-time initialization is now complete.
	 * (Most) everything beyond this will now be retried, if the -reconnect
	 * option is in effect.
	 */
	for (;;) {
		char *errtxt;

		/* Resolve the host name. */
	    	if (proxy_type > 0) {
		    	unsigned long lport;
			char *ptr;
			struct servent *sp;

			if (resolve_host_and_port(proxy_host, proxy_portname,
				    0, &proxy_port, &ha.sa, &ha_len, &errtxt,
				    NULL) < 0) {
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
			if (resolve_host_and_port(host, port, 0, &p, &ha.sa,
				    &ha_len, &errtxt, NULL) < 0) {
			    popup_an_error("%s/%s: %s", host, port, errtxt);
			    rc = 1;
			    goto retry;
			}
		}

		/* Connect to the host. */
		s = socket(ha.sa.sa_family, SOCK_STREAM, 0);
		if (s == INVALID_SOCKET) {
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
		    	if (options.verbose) {
			    	(void) fprintf(stderr, "Connected to proxy "
					       "server %s, port %u\n",
					       proxy_host, proxy_port);
			}
		    	if (!proxy_negotiate(proxy_type, s, host, p)) {
				rc = 1;
				goto retry;
			}
		}

		/* Say hello. */
		if (options.verbose) {
			(void) fprintf(stderr, "Connected to %s, port %u%s\n",
			    host, p,
#if defined(HAVE_LIBSSL) /*[*/
			    options.ssl.ssl_host? " via SSL": ""
#else /*][*/
			    ""
#endif /*]*/
			    					);
			if (options.assoc != NULL)
				(void) fprintf(stderr, "Associating with LU "
				    "%s\n", options.assoc);
			else if (lu != NULL)
				(void) fprintf(stderr, "Connecting to LU %s\n",
				    lu);
#if !defined(_WIN32) /*[*/
			(void) fprintf(stderr, "Command: %s\n",
				options.command);
#else /*][*/
			(void) fprintf(stderr, "Printer: %s\n",
				       options.printer? options.printer:
						        "(none)");
#endif /*]*/
		}
		vtrace("Connected to %s, port %u%s\n", host, p,
#if defined(HAVE_LIBSSL) /*[*/
			options.ssl.ssl_host? " via SSL": ""
#else /*][*/
			""
#endif /*]*/
							    );
		if (options.assoc != NULL) {
			vtrace("Associating with LU %s\n", options.assoc);
		} else if (lu != NULL) {
			vtrace("Connecting to LU %s\n", lu);
		}
#if !defined(_WIN32) /*[*/
		vtrace("Command: %s\n", options.command);
#else /*][*/
		vtrace("Printer: %s\n", options.printer? options.printer:
							 "(none)");
#endif /*]*/

		/* Negotiate. */
		if (!pr_net_negotiate(host, &ha.sa, ha_len, s, lu,
			    options.assoc)) {
			rc = 1;
			goto retry;
		}

		/* Report sudden success. */
		if (report_success) {
			errmsg("Connected to %s, port %u", host, p);
			report_success = 0;
		}

		/* Process what we're told to process. */
		if (!pr_net_process(s)) {
			rc = 1;
			if (options.verbose)
				(void) fprintf(stderr,
				    "Disconnected (error).\n");
			goto retry;
		}
		if (options.verbose)
			(void) fprintf(stderr, "Disconnected (eof).\n");

	    retry:
		/* Flush any pending data. */
		(void) print_eoj();

		/* Close the socket. */
		if (s != INVALID_SOCKET) {
			net_disconnect();
			s = INVALID_SOCKET;
		}

		if (!options.reconnect)
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

#if defined(_MSC_VER) /*[*/
#define xstr(s) str(s)
#define str(s)  #s
#endif /*]*/

const char *
build_options(void)
{
    	return "Build options:"
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
#if defined(_MSC_VER) /*[*/
	    " via MSVC " xstr(_MSC_VER)
#endif /*]*/
#if defined(__GNUC__) /*[*/
	    " via gcc " __VERSION__
#endif /*]*/
#if defined(__LP64__) || defined(__LLP64__) /*[*/
	    " 64-bit"
#else /*][*/
	    " 32-bit"
#endif /*]*/

	    ;
}
