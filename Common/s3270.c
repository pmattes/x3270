/*
 * Copyright (c) 1993-2009, 2013-2016 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR
 * GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	s3270.c
 *		A displayless 3270 Terminal Emulator
 *		Main proceudre.
 */

#include "globals.h"
#if !defined(_WIN32) /*[*/
# include <sys/wait.h>
# include <signal.h>
#endif /*]*/
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actions.h"
#include "bind-opt.h"
#include "charset.h"
#include "ctlrc.h"
#include "unicodec.h"
#include "ft.h"
#include "glue.h"
#include "host.h"
#include "httpd-core.h"
#include "httpd-nodes.h"
#include "httpd-io.h"
#include "idle.h"
#include "kybd.h"
#include "macros.h"
#include "nvt.h"
#include "opts.h"
#include "popups.h"
#include "print_screen.h"
#include "product.h"
#include "screen.h"
#include "selectc.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "utils.h"
#include "xio.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "windirs.h"
# include "winvers.h"
#endif /*]*/

#if defined(_WIN32) /*[*/
char *instdir = NULL;
char *mydesktop = NULL;
char *mydocs3270 = NULL;
char *commondocs3270 = NULL;
unsigned windirs_flags;
#endif /*]*/

static void check_min_version(const char *min_version);
static void s3270_register(void);

void
usage(const char *msg)
{
    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, "Usage: %s [options] [ps:][LUname@]hostname[:port]\n",
	    programname);
    fprintf(stderr, "Options:\n");
    cmdline_help(false);
    exit(1);
}

static void
s3270_connect(bool ignored)
{       
    if (CONNECTED || appres.disconnect_clear) {
	ctlr_erase(true);
    }
} 

int
main(int argc, char *argv[])
{
    const char	*cl_hostname = NULL;

#if defined(_WIN32) /*[*/
    (void) get_version_info();
    if (!get_dirs("wc3270", &instdir, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, &windirs_flags)) {
	exit(1);
    }
    if (sockstart() < 0) {
	exit(1);
    }
#endif /*]*/

    /*
     * Call the module registration functions, to build up the tables of
     * actions, options and callbacks.
     */
    ctlr_register();
    ft_register();
    host_register();
    idle_register();
    kybd_register();
    macros_register();
    nvt_register();
    print_screen_register();
    s3270_register();
    toggles_register();
    trace_register();
    xio_register();

    argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

    if (appres.min_version != NULL) {
	check_min_version(appres.min_version);
    }

    if (charset_init(appres.charset) != CS_OKAY) {
	xs_warning("Cannot find charset \"%s\"", appres.charset);
	(void) charset_init(NULL);
    }
    model_init();
    ctlr_init(-1);
    ctlr_reinit(-1);
    idle_init();
    if (appres.httpd_port) {
	struct sockaddr *sa;
	socklen_t sa_len;

	if (!parse_bind_opt(appres.httpd_port, &sa, &sa_len)) {
	    xs_warning("Invalid -httpd port \"%s\"", appres.httpd_port);
	} else {
	    httpd_objects_init();
	    hio_init(sa, sa_len);
	}
    }
    ft_init();
    hostfile_init();

#if !defined(_WIN32) /*[*/
    /* Make sure we don't fall over any SIGPIPEs. */
    (void) signal(SIGPIPE, SIG_IGN);
#endif /*]*/

    /* Handle initial toggle settings. */
    initialize_toggles();

#if defined(HAVE_LIBSSL) /*[*/
    ssl_base_init(NULL, NULL);
#endif /*]*/

    /* Connect to the host. */
    if (cl_hostname != NULL) {
	if (!host_connect(cl_hostname)) {
	    exit(1);
	}
	/* Wait for negotiations to complete or fail. */
	while (!IN_NVT && !IN_3270) {
	    (void) process_events(true);
	    if (!PCONNECTED) {
		exit(1);
	    }
	}
    }

    /* Prepare to run a peer script. */
    peer_script_init();

    /* Process events forever. */
    while (1) {
	(void) process_events(true);

#if !defined(_WIN32) /*[*/
	if (children && waitpid(-1, (int *)0, WNOHANG) > 0) {
	    --children;
	}
#endif /*]*/
    }
}

/**
 * Set product-specific appres defaults.
 */
void
product_set_appres_defaults(void)
{
    appres.scripted = true;
}

/**
 * Parse a version number.
 * Version numbers are of the form: <major>.<minor>text<iteration>, such as
 *  3.4ga10 (3, 4, 10)
 *  3.5apha3 (3, 5, 3)
 * The version can be under-specified, e.g.:
 *  3.4 (3, 4, 0)
 *  3 (3, 0, 0)
 * Numbers are limited to 0..999.
 * @param[in] text		String to decode.
 * @param[out] major		Major number.
 * @param[out] minor		Minor number.
 * @param[out] iteration	Iteration.
 *
 * @return true if parse successful.
 */
#define MAX_VERSION 999
static bool
parse_version(const char *text, int *major, int *minor, int *iteration)
{
    const char *t = text;
    unsigned long n;
    char *ptr;

    *major = 0;
    *minor = 0;
    *iteration = 0;

    /* Parse the major number. */
    n = strtoul(t, &ptr, 10);
    if (ptr == t || (*ptr != '.' && *ptr != '\0') || n > MAX_VERSION) {
	return false;
    }
    *major = (int)n;

    if (*ptr == '\0') {
	/* Just a major number. */
	return true;
    }

    /* Parse the minor number. */
    t = ptr + 1;
    n = strtoul(t, &ptr, 10);
    if (ptr == text || n > MAX_VERSION) {
	return false;
    }
    *minor = (int)n;

    if (*ptr == '\0') {
	/* Just a major and minor number. */
	return true;
    }

    /* Parse the iteration. */
    t = ptr;
    while (!isdigit((unsigned char)*t) && *t != '\0')
    {
	t++;
    }
    if (*t == '\0') {
	return false;
    }

    n = strtoul(t, &ptr, 10);
    if (ptr == t || *ptr != '\0' || n > MAX_VERSION) {
	return false;
    }
    *iteration = (int)n;

    return true;
}

/**
 * Check the requested version against the actual version.
 * @param[in] min_version	Desired minimum version
 */
static void
check_min_version(const char *min_version)
{
    int our_major, our_minor, our_iteration;
    int min_major, min_minor, min_iteration;

    /* Parse our version. */
    if (!parse_version(build_rpq_version, &our_major, &our_minor,
		&our_iteration)) {
	fprintf(stderr, "Internal error: Can't parse version: %s\n",
		build_rpq_version);
	exit(1);
    }

    /* Parse the desired version. */
    if (!parse_version(min_version, &min_major, &min_minor, &min_iteration)) {
	fprintf(stderr, "Invalid %s: %s\n", ResMinVersion, min_version);
	exit(1);
    }

    /* Compare. */
    if (our_major < min_major ||
	our_minor < min_minor ||
	our_iteration < min_iteration)
    {
	fprintf(stderr, "Version %s < requested %s, aborting\n",
		build_rpq_version, min_version);
	exit(1);
    }
}

/**
 * Main module registration.
 */
static void
s3270_register(void)
{
    static opt_t s3270_opts[] = {
	{ OptScripted, OPT_NOP,     false, ResScripted,  NULL,
	    NULL, "Turn on scripting" },
	{ OptUtf8,     OPT_BOOLEAN, true,  ResUtf8,      aoffset(utf8),
	    NULL, "Force local codeset to be UTF-8" },
	{ OptMinVersion,OPT_STRING, false, ResMinVersion,aoffset(min_version),
	    "<version>", "Fail unless at this version or greater" }
    };
    static res_t s3270_resources[] = {
	{ ResIdleCommand,aoffset(idle_command),     XRM_STRING },
	{ ResIdleCommandEnabled,aoffset(idle_command_enabled),XRM_BOOLEAN },
	{ ResIdleTimeout,aoffset(idle_timeout),     XRM_STRING }
    };
    static xres_t s3270_xresources[] = {
	{ ResPrintTextScreensPerPage,	V_FLAT },
#if defined(_WIN32) /*[*/
	{ ResPrinterCodepage,		V_FLAT },
	{ ResPrinterName, 		V_FLAT },
	{ ResPrintTextFont, 		V_FLAT },
	{ ResPrintTextHorizontalMargin,	V_FLAT },
	{ ResPrintTextOrientation,	V_FLAT },
	{ ResPrintTextSize, 		V_FLAT },
	{ ResPrintTextVerticalMargin,	V_FLAT },
#else /*][*/
	{ ResPrintTextCommand,		V_FLAT },
#endif /*]*/
    };

    /* Register for state changes. */
    register_schange(ST_CONNECT, s3270_connect);
    register_schange(ST_3270_MODE, s3270_connect);

    /* Register our options. */
    register_opts(s3270_opts, array_count(s3270_opts));

    /* Register our resources. */
    register_resources(s3270_resources, array_count(s3270_resources));
    register_xresources(s3270_xresources, array_count(s3270_xresources));
}
