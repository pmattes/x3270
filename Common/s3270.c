/*
 * Copyright (c) 1993-2022 Paul Mattes.
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
#include "codepage.h"
#include "cookiefile.h"
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
#include "login_macro.h"
#include "min_version.h"
#include "model.h"
#include "nvt.h"
#include "opts.h"
#include "peerscript.h"
#include "popups.h"
#include "pr3287_session.h"
#include "prefer.h"
#include "print_screen.h"
#include "product.h"
#include "proxy_toggle.h"
#include "query.h"
#include "save_restore.h"
#include "screen.h"
#include "selectc.h"
#include "sio_glue.h"
#include "task.h"
#include "telnet.h"
#include "telnet_new_environ.h"
#include "toggles.h"
#include "trace.h"
#include "screentrace.h"
#include "utils.h"
#include "vstatus.h"
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

static void s3270_register(void);

void
usage(const char *msg)
{
    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, "Usage: %s [options] [prefix:][LUname@]hostname[:port]\n",
	    app);
    fprintf(stderr, "Use " OptHelp1 " for the list of options\n");
    exit(1);
}

static void
s3270_connect(bool ignored)
{       
    if (CONNECTED || appres.disconnect_clear) {
	ctlr_erase(true);
    }
} 

/**
 * Set up a callback session.
 */
static void
callback_init(void)
{
    struct sockaddr *sa;
    socklen_t sa_len;
    socket_t s;
    int nc = 1;
    char *cbstring = appres.scripting.callback;
    int i;

    if (appres.scripting.callback == NULL) {
	return;
    }

    if (!strncasecmp("2x", cbstring, 2)) {
	nc = 2;
	cbstring += 2;
    }

    for (i = 0; i < nc; i++) {

	if (!parse_bind_opt(cbstring, &sa, &sa_len)) {
	    Error("Cannot parse " ResCallback);
	}
	if ((s = socket(sa->sa_family, SOCK_STREAM, 0)) == INVALID_SOCKET) {
#if !defined(_WIN32) /*[*/
	    perror("socket");
#else /*][*/
	    fprintf(stderr, "socket: %s\n", win32_strerror(WSAGetLastError()));
	    fflush(stdout);
#endif /*]*/
	    exit(1);
	}
	if (connect(s, sa, sa_len) < 0) {
#if !defined(_WIN32) /*[*/
	    perror(ResCallback " connect");
#else /*][*/
	    fprintf(stderr, "connect: %s\n",
		    win32_strerror(WSAGetLastError()));
	    fflush(stdout);
#endif /*]*/
	    exit(1);
	}

	/* Get ready for I/O. */
	peer_accepted(s, NULL);
    }
}

int
main(int argc, char *argv[])
{
    const char	*cl_hostname = NULL;

#if defined(_WIN32) /*[*/
    get_version_info();
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
    codepage_register();
    ctlr_register();
    ft_register();
    host_register();
    idle_register();
    kybd_register();
    task_register();
    query_register();
    nvt_register();
    pr3287_session_register();
    print_screen_register();
    save_restore_register();
    s3270_register();
    toggles_register();
    trace_register();
    screentrace_register();
    xio_register();
    sio_glue_register();
    hio_register();
    proxy_register();
    model_register();
    net_register();
    login_macro_register();
    vstatus_register();
    prefer_register();
    telnet_new_environ_register();

    argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

    if (appres.min_version != NULL) {
	check_min_version(appres.min_version);
    }

    if (codepage_init(appres.codepage) != CS_OKAY) {
	xs_warning("Cannot find code page \"%s\"", appres.codepage);
	codepage_init(NULL);
    }
    model_init();
    ctlr_init(ALL_CHANGE);
    ctlr_reinit(ALL_CHANGE);
    idle_init();
    httpd_objects_init();
    if (appres.httpd_port) {
	struct sockaddr *sa;
	socklen_t sa_len;

	if (!parse_bind_opt(appres.httpd_port, &sa, &sa_len)) {
	    xs_warning("Invalid -httpd port \"%s\"", appres.httpd_port);
	} else {
	    hio_init(sa, sa_len);
	}
    }
    ft_init();
    hostfile_init();
    if (!cookiefile_init()) {
	exit(1);
    }

#if !defined(_WIN32) /*[*/
    /* Make sure we don't fall over any SIGPIPEs. */
    signal(SIGPIPE, SIG_IGN);
#endif /*]*/

    /* Handle initial toggle settings. */
    initialize_toggles();

    /* Connect to the host. */
    if (cl_hostname != NULL) {
	if (!host_connect(cl_hostname, IA_UI)) {
	    exit(1);
	}
	/* Wait for negotiations to complete or fail. */
	while (!IN_NVT && !IN_3270) {
	    process_events(true);
	    if (!PCONNECTED) {
		exit(1);
	    }
	}
    }

    /* Prepare to run a peer script. */
    peer_script_init();

    /* Prepare a callback session. */
    callback_init();

    /* Process events forever. */
    while (1) {
	process_events(true);
    }
}

/**
 * Set product-specific appres defaults.
 */
void
product_set_appres_defaults(void)
{
    appres.scripted = true;
    appres.oerr_lock = true;
}

static void
s3270_toggle(toggle_index_t ix, enum toggle_type tt)
{
}

bool
model_can_change(void)
{
    return true;
}

void
screen_init(void)
{
}

void
screen_change_model(int mn, int ovc, int ovr)
{
}

/**
 * Main module registration.
 */
static void
s3270_register(void)
{
    static toggle_register_t toggles[] = {
	{ MONOCASE,         s3270_toggle,   0 }
    };
    static opt_t s3270_opts[] = {
	{ OptScripted, OPT_NOP,     false, ResScripted,  NULL,
	    NULL, "Turn on scripting" },
	{ OptUtf8,     OPT_BOOLEAN, true,  ResUtf8,      aoffset(utf8),
	    NULL, "Force local codeset to be UTF-8" },
	{ OptCallback, OPT_STRING,  false, ResCallback,
	    aoffset(scripting.callback), NULL, "Callback address and port" },

    };
    static res_t s3270_resources[] = {
	{ ResCallback, aoffset(scripting.callback), XRM_STRING },
	{ ResIdleCommand,aoffset(idle_command),     XRM_STRING },
	{ ResIdleCommandEnabled,aoffset(idle_command_enabled),XRM_BOOLEAN },
	{ ResIdleTimeout,aoffset(idle_timeout),     XRM_STRING },
	{ ResScriptedAlways,aoffset(scripted_always),XRM_BOOLEAN },
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

    /* Register our toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register for state changes. */
    register_schange(ST_CONNECT, s3270_connect);
    register_schange(ST_3270_MODE, s3270_connect);

    /* Register our options. */
    register_opts(s3270_opts, array_count(s3270_opts));

    /* Register our resources. */
    register_resources(s3270_resources, array_count(s3270_resources));
    register_xresources(s3270_xresources, array_count(s3270_xresources));
}
