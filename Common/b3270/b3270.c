/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *	b3270.c
 *		A GUI back-end for a 3270 Terminal Emulator
 *		Main proceudre.
 */

#include "globals.h"
#include <assert.h>
#if !defined(_WIN32) /*[*/
# include <sys/wait.h>
# include <signal.h>
#endif /*]*/
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actions.h"
#include "b3270_popups.h"
#include "b3270proto.h"
#include "bind-opt.h"
#include "boolstr.h"
#include "bscreen.h"
#include "b_password.h"
#include "codepage.h"
#include "cookiefile.h"
#include "ctlr.h"
#include "ctlrc.h"
#include "unicodec.h"
#include "ft.h"
#include "glue.h"
#include "ui_stream.h"
#include "host.h"
#include "httpd-core.h"
#include "httpd-nodes.h"
#include "httpd-io.h"
#include "idle.h"
#include "json.h"
#include "kybd.h"
#include "login_macro.h"
#include "min_version.h"
#include "model.h"
#include "names.h"
#include "nvt.h"
#include "nvt_gui.h"
#include "opts.h"
#include "popups.h"
#include "pr3287_session.h"
#include "prefer.h"
#include "print_screen.h"
#include "product.h"
#include "proxy.h"
#include "proxy_toggle.h"
#include "query.h"
#include "save_restore.h"
#include "screen.h"
#include "selectc.h"
#include "sio.h"
#include "sio_glue.h"
#include "sio_internal.h"
#include "split_host.h"
#include "stats.h"
#include "status.h"
#include "task.h"
#include "telnet.h"
#include "telnet_new_environ.h"
#include "tls_passwd_gui.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
#include "screentrace.h"
#include "utils.h"
#include "varbuf.h"
#include "vstatus.h"
#include "xio.h"
#include "xscroll.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "windirs.h"
# include "winvers.h"
#endif /*]*/

#define STATS_POLL	(2 * 1000)
#define NC_DELAY	50

#if defined(_WIN32) /*[*/
char *instdir = NULL;
char *mydesktop = NULL;
char *mydocs3270 = NULL;
char *commondocs3270 = NULL;
unsigned windirs_flags;
#endif /*]*/

static int brcvd = 0;
static int rrcvd = 0;
static int bsent = 0;
static int rsent = 0;
static ioid_t stats_ioid = NULL_IOID;

static bool b3270_toggle_yet = false;
static char* crashptr;

static ioid_t csdelay_ioid = NULL_IOID;

static void b3270_toggle(toggle_index_t ix, enum toggle_type tt);
static toggle_register_t toggles[] = {
    { MONOCASE,		b3270_toggle,	TOGGLE_NEED_INIT },
    { ALT_CURSOR,	b3270_toggle,	TOGGLE_NEED_INIT },
    { CURSOR_BLINK,	b3270_toggle,	TOGGLE_NEED_INIT },
    { TRACING,		b3270_toggle,	TOGGLE_NEED_INIT },
    { VISIBLE_CONTROL,	b3270_toggle,	TOGGLE_NEED_INIT },
    { SCREEN_TRACE,	b3270_toggle,	TOGGLE_NEED_INIT },
    { CROSSHAIR,	b3270_toggle,	TOGGLE_NEED_INIT },
    { OVERLAY_PASTE,	b3270_toggle,	TOGGLE_NEED_INIT },
    { TYPEAHEAD,	b3270_toggle,	TOGGLE_NEED_INIT },
    { APL_MODE,		b3270_toggle,	TOGGLE_NEED_INIT },
    { ALWAYS_INSERT,	b3270_toggle,	TOGGLE_NEED_INIT },
    { SHOW_TIMING,	b3270_toggle,	TOGGLE_NEED_INIT },
};

static void b3270_register(void);
static void b3270_toggle_notify(const char *name, enum resource_type type, void **address, ia_t ia, unsigned flags);
static void delayed_nc_report(ioid_t id);

void
usage(const char *msg)
{
    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, "Usage: %s [options] [<session-file>.b3270]\n",
	    programname);
    fprintf(stderr, "Use " OptHelp1 " for the list of options\n");
    exit(1);
}

/* Dump the current send/receive statistics. */
static void
dump_stats(void)
{
    ui_leaf(IndStats,
	    AttrBytesReceived, AT_INT, (int64_t)brcvd,
	    AttrRecordsReceived, AT_INT, (int64_t)rrcvd,
	    AttrBytesSent, AT_INT, (int64_t)bsent,
	    AttrRecordsSent, AT_INT, (int64_t)rsent,
	    NULL);
}

/* Dump the current send/receive stats out if they have changed. */
static void
stats_poll(ioid_t id _is_unused)
{
    if (brcvd != ns_brcvd ||
	rrcvd != ns_rrcvd ||
	bsent != ns_bsent ||
	rsent != ns_rsent) {
	brcvd = ns_brcvd;
	rrcvd = ns_rrcvd;
	bsent = ns_bsent;
	rsent = ns_rsent;
	dump_stats();
    }
    stats_ioid = NULL_IOID;
}

/* Send/receive statistics have changed. */
void
stats_poke(void)
{
    /* Schedule a timeout. */
    if (stats_ioid == NULL_IOID) {
	stats_ioid = AddTimeOut(STATS_POLL, stats_poll);
    }
}

/**
 * Respond to a change in the connection, 3270 mode, or line mode.
 */
static void
b3270_connect_common(bool direct_indication)
{       
    static enum cstate old_cstate = NOT_CONNECTED;

    /*
     * If this is a direct call from the state change indication, and we've gone from something other than
     * NOT_CONNECTED to NOT_CONNECTED, wait a moment before reporting that.  It's possible that NOT_CONNECTED
     * was transient, and we don't want to report it.
     *
     * This is simpler than replumbing the retry/reconnect logic to skip the NOT_CONNECTED state, even if it is
     * a bit awkward here.
     */
    if (direct_indication)
    {
	if (cstate == NOT_CONNECTED && old_cstate != NOT_CONNECTED && csdelay_ioid == NULL_IOID) {
	    /* xxx => NOT_CONNECTED and no timer running. Start it. */
	    csdelay_ioid = AddTimeOut(NC_DELAY, delayed_nc_report);
	    return;
	}
	if (cstate != NOT_CONNECTED && csdelay_ioid != NULL_IOID) {
	    /* Gone to something other than NOT_CONNECTED. Stop the timer, we want to report it. */
	    RemoveTimeOut(csdelay_ioid);
	    csdelay_ioid = NULL_IOID;
	}
    }

    if (cstate == old_cstate) {
	return;
    }

    /* If just disconnected, dump final stats. */
    if (cstate == NOT_CONNECTED && stats_ioid != NULL_IOID) {
	RemoveTimeOut(stats_ioid);
	stats_ioid = NULL_IOID;
	if (brcvd != ns_brcvd ||
	    rrcvd != ns_rrcvd ||
	    bsent != ns_bsent ||
	    rsent != ns_rsent) {
	    brcvd = ns_brcvd;
	    rrcvd = ns_rrcvd;
	    bsent = ns_bsent;
	    rsent = ns_rsent;
	    dump_stats();
	}
    }

    /* Tell the GUI about the new state. */
    if (cstate == NOT_CONNECTED) {
	ui_leaf(IndConnection,
		AttrState, AT_STRING, state_name[(int)cstate],
		NULL);
    } else {
	char *cause = NewString(ia_name[connect_ia]);
	char *s = cause;
	char c;

	/* Make sure unlock state is set correctly. */
	status_reset();

	while ((c = *s)) {
	    c = tolower((int)(unsigned char)c);
	    if (c == ' ') {
		c = '-';
	    }
	    *s++ = c;
	}
	ui_leaf(IndConnection,
		AttrState, AT_STRING, state_name[(int)cstate],
		AttrHost, AT_STRING, current_host,
		AttrCause, AT_STRING, cause,
		NULL);
	Free(cause);

	/* Clear the screen. */
	if (old_cstate == NOT_CONNECTED) {
	    ctlr_erase(true);
	}
    }

    /* If just connected, dump initial stats. */
    if (cstate != NOT_CONNECTED && stats_ioid == NULL_IOID &&
	(brcvd != ns_brcvd ||
	 rrcvd != ns_rrcvd ||
	 bsent != ns_bsent ||
	 rsent != ns_rsent)) {
	brcvd = ns_brcvd;
	rrcvd = ns_rrcvd;
	bsent = ns_bsent;
	rsent = ns_rsent;
	dump_stats();
    }

    old_cstate = cstate;
}

/* Report connection state after a short delay, it if transtioned to NOT_CONNECTED. */
static void delayed_nc_report(ioid_t id)
{
    RemoveTimeOut(csdelay_ioid);
    csdelay_ioid = NULL_IOID;
    b3270_connect_common(false);	/* suppress adding another delay */
}

/**
 * Respond to a change in the connection, 3270 mode, or line mode.
 */
static void
b3270_connect(bool ignored)
{
    b3270_connect_common(true);		/* add a short delay if NOT_CONNECTED */
}

static void
b3270_secure(bool ignored)
{
    static bool is_secure = false;

    if (net_secure_connection() == is_secure) {
	return;
    }
    is_secure = net_secure_connection();

     ui_leaf(IndTls,
	     AttrSecure, AT_BOOLEAN, net_secure_connection(),
	     AttrVerified,
		 net_secure_connection()? AT_BOOLEAN: AT_SKIP_BOOLEAN,
		 !net_secure_unverified(),
	     AttrSession, AT_STRING, net_session_info(),
	     AttrHostCert, AT_STRING, net_server_cert_info(),
	     NULL);
}

/* Report the terminal name. */
static void
report_terminal_name(void)
{
    static char *last_term_name = NULL;
    static bool last_override = false;

    if (last_term_name == NULL || strcmp(last_term_name, termtype)
	    || last_override != (appres.termname != NULL)) {
	Replace(last_term_name, NewString(termtype));
	last_override = appres.termname != NULL;
	ui_leaf(IndTerminalName,
		AttrText, AT_STRING, last_term_name,
		AttrOverride, AT_BOOLEAN, last_override,
		NULL);
    }
}

#if !defined(_WIN32) /*[*/
/* Empty SIGCHLD handler, ensuring that we can collect child exit status. */
static void
sigchld_handler(int ignored)
{
# if !defined(_AIX) /*[*/
    signal(SIGCHLD, sigchld_handler);
# endif /*]*/
}
#endif /*]*/

/* Dump the code page list. Called at initialization time. */
static void
dump_codepages(void)
{
    cpname_t *cpnames = get_cpnames();
    int i;

    if (cpnames == NULL) {
	return;
    }

    if (JSON_MODE) {
	uij_open_object(NULL);
	uij_open_array(IndCodePages);
    } else {
	uix_push(IndCodePages, NULL);
    }
    for (i = 0; cpnames[i].name != NULL; i++) {
	if (XML_MODE) {
	    /*
	     * In XML mode, it looks like:
	     *  name="foo", alias1="bar", alias3="baz", ...
	     */
	    int j;

	    uix_open_leaf(IndCodePage);
	    ui_add_element("name", AT_STRING, cpnames[i].name);
	    for (j = 0; j < cpnames[i].num_aliases; j++) {
		ui_add_element(txAsprintf("alias%d", j + 1), AT_STRING,
			cpnames[i].aliases[j]);
	    }
	    uix_close_leaf();
	} else {
	    /*
	     * In JSON mode, it looks like:
	     *  {
	     *    "name": "foo",
	     *    "aliases": [ "bar", "baz", ... ]
	     *  }
	     */
	    uij_open_object(NULL);
	    ui_add_element("name", AT_STRING, cpnames[i].name);
	    if (cpnames[i].num_aliases) {
		int j;

		uij_open_array("aliases");
		for (j = 0; j < cpnames[i].num_aliases; j++) {
		    ui_add_element(NULL, AT_STRING,
			    cpnames[i].aliases[j]);
		}
		uij_close_array();
	    }
	    uij_close_object();
	}
    }
    if (JSON_MODE) {
	uij_close_array();
	uij_close_object();
    } else {
	uix_pop();
    }
    free_cpnames(cpnames);
}

/* Dump the model list. Called at initialization time. */
static void
dump_models(void)
{
    static struct {
	int model;
	int rows;
	int columns;
    } models[] = {
	{ 2, MODEL_2_ROWS, MODEL_2_COLS },
	{ 3, MODEL_3_ROWS, MODEL_3_COLS },
	{ 4, MODEL_4_ROWS, MODEL_4_COLS },
	{ 5, MODEL_5_ROWS, MODEL_5_COLS },
	{ 0, 0, 0 }
    };
    int i;

    if (XML_MODE) {
	uix_push(IndModels, NULL);
    } else {
	uij_open_object(NULL);
	uij_open_array(IndModels);
    }
    for (i = 0; models[i].model != 0; i++) {
	if (XML_MODE) {

	    ui_leaf(IndModel,
		    AttrModel, AT_INT, (int64_t)models[i].model,
		    AttrRows, AT_INT, (int64_t)models[i].rows,
		    AttrColumns, AT_INT, (int64_t)models[i].columns,
		    NULL);
	} else {
	    uij_open_object(NULL);
	    ui_add_element(AttrModel, AT_INT, (int64_t)models[i].model);
	    ui_add_element(AttrRows, AT_INT, (int64_t)models[i].rows);
	    ui_add_element(AttrColumns, AT_INT, (int64_t)models[i].columns);
	    uij_close_object();
	}
    }
    if (XML_MODE) {
	uix_pop();
    } else {
	uij_close_array();
	uij_close_object();
    }
}

/* Dump the proxy list. */
static void
dump_proxies(void)
{
    proxytype_t type;

    if (XML_MODE) {
	uix_push(IndProxies, NULL);
    } else {
	uij_open_object(NULL);
	uij_open_array(IndProxies);
    }

    for (type = PT_FIRST; type < PT_MAX; type++) {
	int default_port = proxy_default_port(type);

	if (XML_MODE) {
	    ui_leaf(IndProxy,
		    AttrName, AT_STRING, proxy_type_name(type),
		    AttrUsername, AT_BOOLEAN, proxy_takes_username(type),
		    AttrPort,
			default_port? AT_INT: AT_SKIP_INT,
			(int64_t)default_port,
		    NULL);
	} else {
	    uij_open_object(NULL);
	    ui_add_element(AttrName, AT_STRING, proxy_type_name(type));
	    ui_add_element(AttrUsername, AT_BOOLEAN,
		    proxy_takes_username(type));
	    if (default_port) {
		ui_add_element(AttrPort, AT_INT, (int64_t)default_port);
	    }
	    uij_close_object();
	}
    }
    if (XML_MODE) {
	uix_pop();
    } else {
	uij_close_array();
	uij_close_object();
    }
}

/* Dump the supported host prefix list. */
static void
dump_prefixes(void)
{
    ui_leaf(IndPrefixes,
	    AttrValue, AT_STRING, host_prefixes(),
	    NULL);
}

int
main(int argc, char *argv[])
{
    const char *cl_hostname = NULL;
    toggle_index_t ix;

#if defined(_WIN32) /*[*/
    get_version_info();
    if (!get_dirs("wc3270", &instdir, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, &windirs_flags)) {
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
    b3270_register();
    save_restore_register();
    scroll_register();
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

    supports_cmdline_host = false;
    argc = parse_command_line(argc, (const char **)argv, &cl_hostname);
    if (cl_hostname != NULL) {
	usage("Unrecognized option(s)");
    }

    check_min_version(appres.min_version);

    ui_io_init();
    if (XML_MODE) {
	uix_push(IndInitialize, NULL);
    } else {
	uij_open_object(NULL);
	uij_open_array(IndInitialize);
    }
    ui_leaf(IndHello,
	    AttrVersion, AT_STRING, txAsprintf("%d.%d.%d", our_major, our_minor, our_iteration),
	    AttrBuild, AT_STRING, build,
	    AttrCopyright, AT_STRING,
txAsprintf("\
Copyright © 1993-%s, Paul Mattes.\n\
Copyright © 1990, Jeff Sparkes.\n\
Copyright © 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA\n\
 30332.\n\
All rights reserved.\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are met:\n\
    * Redistributions of source code must retain the above copyright\n\
      notice, this list of conditions and the following disclaimer.\n\
    * Redistributions in binary form must reproduce the above copyright\n\
      notice, this list of conditions and the following disclaimer in the\n\
      documentation and/or other materials provided with the distribution.\n\
    * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of\n\
      their contributors may be used to endorse or promote products derived\n\
      from this software without specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC \"AS IS\" AND\n\
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE\n\
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n\
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n\
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\n\
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n\
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n\
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n\
POSSIBILITY OF SUCH DAMAGE.", cyear),
	    NULL);

    if (codepage_init(appres.codepage) != CS_OKAY) {
	xs_warning("Cannot find code page \"%s\"", appres.codepage);
	codepage_init(NULL);
    }
    dump_codepages();
    dump_models();
    dump_proxies();
    dump_prefixes();
    model_init();
    status_reset();

    /*
     * Slam ROWS and COLS to the max right now. The ctlr code goes to a lot of
     * trouble to make these defROWS and defCOLS, probably so a host that does
     * starts out with a blind Write without an Erase will get a Model 2, but
     * I will let someone complain about that if it comes up in practice.
     *
     * b3270_connect() does an implied EraseWriteAlternate when a host
     * connects, so that would need to change, too.
     */
    ROWS = altROWS;
    COLS = altCOLS;

    screen_init();
    ctlr_init(ALL_CHANGE);
    ctlr_reinit(ALL_CHANGE);
    report_terminal_name();
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

    /* Collect child exit status. */
    signal(SIGCHLD, sigchld_handler);
#endif /*]*/

    /* Initialize the toggles. */
    initialize_toggles();

    /* Send TLS set-up. */
    if (sio_supported()) {
	if (XML_MODE) {
	    ui_leaf(IndTlsHello,
		    AttrSupported, AT_BOOLEAN, sio_supported(),
		    AttrProvider, AT_STRING, sio_provider(),
		    AttrOptions, AT_STRING, sio_option_names(),
		    NULL);
	} else {
	    char *option_names = NewString(sio_option_names());
	    char *str = option_names;
	    char *token;
	    char *saveptr = NULL;

	    uij_open_object(NULL);
	    uij_open_object(IndTlsHello);
	    ui_add_element(AttrSupported, AT_BOOLEAN, sio_supported());
	    ui_add_element(AttrProvider, AT_STRING, sio_provider());
	    uij_open_array(AttrOptions);
	    while ((token = strtok_r(str, " ", &saveptr)) != NULL) {
		str = NULL;
		ui_add_element(NULL, AT_STRING, token);
	    }
	    uij_close_array();
	    Free(option_names);
	    uij_close_object();
	    uij_close_object();
	}
    } else {
	ui_leaf(IndTlsHello,
		AttrSupported, AT_BOOLEAN, false,
		NULL);
    }

    /*
     * Register for extended toggle notifies, which will cause a dump of the
     * current values.
     *
     * Then dump the traditional toggles.
     */
    register_extended_toggle_notify(b3270_toggle_notify);
    b3270_toggle_yet = true;
    for (ix = MONOCASE; ix < N_TOGGLES; ix++) {
	if (toggle_supported(ix)) {
	    b3270_toggle(ix, TT_INITIAL);
	}
    }

    /* Prepare to run a peer script. */
    peer_script_init();

    /* Done with initialization.*/
    if (XML_MODE) {
	uix_pop();
    } else {
	uij_close_array();
	uij_close_object();
    }
    popups_dump();

    /* Process events forever. */
    while (1) {
	process_events(true);
	screen_disp(false);
    }
}

/*
 * ClearRegion action:
 *  ClearRegion row column rows columns
 * Row and column are 1-origin.
 * Used by the UI Cut action.
 */
static bool
ClearRegion_action(ia_t ia, unsigned argc, const char **argv)
{
    int row, column, rows, columns;
    int r, c;
    int baddr;
    int ba2;

    action_debug(AnClearRegion, ia, argc, argv);
    if (check_argc(AnClearRegion, argc, 4, 4) < 0) {
	return false;
    }

    row = atoi(argv[0]);
    column = atoi(argv[1]);
    rows = atoi(argv[2]);
    columns = atoi(argv[3]);

    if (row <= 0 || row > ROWS || column <= 0 || column > COLS) {
	popup_an_error(AnClearRegion "(): invalid coordinates");
    }

    if (rows < 0 || columns < 0 ||
	    row - 1 + rows > ROWS || column - 1 + columns > COLS) {
	popup_an_error(AnClearRegion "(): invalid size");
    }

    if (rows == 0 || columns == 0) {
	return true;
    }

    baddr = ROWCOL_TO_BA(row - 1, column - 1);
    for (r = row - 1; r < row - 1 + rows; r++) {
	for (c = column - 1; c < column - 1 + columns; c++) {
	    baddr = ROWCOL_TO_BA(r, c);
	    if (ea_buf[baddr].fa ||
		    FA_IS_PROTECTED(get_field_attribute(baddr)) ||
		    ea_buf[baddr].ec == EBC_so ||
		    ea_buf[baddr].ec == EBC_si) {
		continue;
	    }
	    switch (ctlr_dbcs_state(baddr)) {
		case DBCS_NONE:
		case DBCS_SB:
		    ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		    break;
		case DBCS_LEFT:
		    ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		    ba2 = baddr;
		    INC_BA(ba2);
		    ctlr_add(ba2, EBC_space, ea_buf[baddr].cs);
		    break;
		case DBCS_RIGHT:
		    ba2 = baddr;
		    DEC_BA(ba2);
		    ctlr_add(ba2, EBC_space, ea_buf[baddr].cs);
		    ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		    break;
		default:
		    break;
	    }
	    mdt_set(baddr);
	}
    }

    return true;
}

/*
 * Crash action. Used for debug purposes.
 */
static bool
Crash_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnCrash, ia, argc, argv);
    if (check_argc(AnCrash, argc, 1, 1) < 0) {
	return false;
    }
    if (!strcasecmp(argv[0], KwAssert)) {
	assert(false);
	popup_an_error(AnCrash "(): Assert did not work");
    } else if (!strcasecmp(argv[0], KwExit)) {
	exit(999);
	popup_an_error(AnCrash "(): Exit did not work");
    } else if (!strcasecmp(argv[0], KwNull)) {
	char c;
	printf("%c\n", c = *crashptr);
	popup_an_error(AnCrash "(): Null did not work");
    } else {
	popup_an_error(AnCrash "(): Must specify " KwAssert ", " KwExit " or "
		KwNull);
    }

    return false;
}

#define STATUS_RECONNECTING	"reconnecting"
#define STATUS_RESOLVING	"resolving"

/*
 * ForceStatus action. Used for debug purposes.
 */
static bool
ForceStatus_action(ia_t ia, unsigned argc, const char **argv)
{
    static const char *reasons[] = {
	OiaLockDeferred, OiaLockInhibit, OiaLockMinus, OiaLockNotConnected,
	OiaLockOerr, OiaLockScrolled, OiaLockSyswait, OiaLockTwait,
	OiaLockDisabled, STATUS_RECONNECTING, STATUS_RESOLVING, NULL
    };
    static const char *oerrs[] = {
	OiaOerrDbcs, OiaOerrNumeric, OiaOerrOverflow, OiaOerrProtected, NULL
    };
    int reason;

    action_debug("ForceStatus", ia, argc, argv);
    if (check_argc("ForceStatus", argc, 1, 2) < 0) {
	return false;
    }

    for (reason = 0; reasons[reason] != NULL; reason++) {
	if (!strcasecmp(argv[0], reasons[reason])) {
	    break;
	}
    }
    if (reasons[reason] == NULL) {
	popup_an_error("ForceStatus: Unknown reason '%s'", argv[0]);
	return false;
    }
    if (!strcmp(argv[0], OiaLockOerr)) {
	int oerr;

	if (argc < 2) {
	    popup_an_error("ForceStatus: Reason '%s' requires an argument",
		    reasons[reason]);
	    return false;
	}
	for (oerr = 0; oerrs[oerr] != NULL; oerr++) {
	    if (!strcasecmp(argv[1], oerrs[oerr])) {
		break;
	    }
	}
	if (oerrs[oerr] == NULL) {
	    popup_an_error("ForceStatus: Unknown %s type '%s'",
		    reasons[reason], argv[1]);
	    return false;
	}
	ui_leaf(IndOia,
		AttrField, AT_STRING, OiaLock,
		AttrValue, AT_STRING,
		    txAsprintf("%s %s", reasons[reason], oerrs[oerr]),
		NULL);
    } else if (!strcmp(argv[0], OiaLockScrolled)) {
	int n;

	if (argc < 2) {
	    popup_an_error("ForceStatus: Reason '%s' requires an argument",
		    reasons[reason]);
	    return false;
	}
	n = atoi(argv[1]);
	if (n < 1) {
	    popup_an_error("Invalid %s amount '%s'", reasons[reason], argv[1]);
	    return false;
	}
	
	ui_leaf(IndOia,
		AttrField, AT_STRING, OiaLock,
		AttrValue, AT_STRING, txAsprintf("%s %d", reasons[reason], n),
		NULL);
    } else if (argc > 1) {
	popup_an_error("ForceStatus: Reason '%s' does not take an argument",
		reasons[reason]);
	return false;
    } else {
	if (!strcmp(reasons[reason], STATUS_RECONNECTING)) {
	    ui_leaf(IndOia,
		    AttrField, AT_STRING, OiaLock,
		    AttrValue, AT_STRING, OiaLockNotConnected,
		    NULL);
	    ui_leaf(IndConnection,
		    AttrState, AT_STRING, state_name[(int)RECONNECTING],
		    NULL);
	} else if (!strcmp(reasons[reason], STATUS_RESOLVING)) {
	    ui_leaf(IndOia,
		    AttrField, AT_STRING, OiaLock,
		    AttrValue, AT_STRING, OiaLockNotConnected,
		    NULL);
	    ui_leaf(IndConnection,
		    AttrState, AT_STRING, state_name[(int)RESOLVING],
		    NULL);
	} else {
	    ui_leaf(IndOia,
		    AttrField, AT_STRING, OiaLock,
		    AttrValue, AT_STRING, reasons[reason],
		    NULL);
	}
    }

    return true;
}

/**
 * xterm text escape
 *
 * @param[in] code	Operation to perform
 * @param[in] text	Associated text
 */
void
xterm_text_gui(int code, const char *text)
{
    if (code == 0 || code == 1) {
	ui_leaf(IndIconName,
		AttrText, AT_STRING, text,
		NULL);
    }
    if (code == 0 || code == 2) {
	ui_leaf(IndWindowTitle,
		AttrText, AT_STRING, text,
		NULL);
    }

    if (code == 50) {
	ui_leaf(IndFont,
		AttrText, AT_STRING, text,
		NULL);
    }
}

/**
 * Set product-specific appres defaults.
 */
void
product_set_appres_defaults(void)
{
    /*
     * Set defaults like x3270 -- operator error locks the keyboard.
     * But also set unlock_delay to false, to help with responsiveness.
     */
    appres.oerr_lock = true;
    appres.interactive.save_lines = 4096;

    /* XML is wrapped by default. */
    appres.b3270.wrapper_doc = true;

    /* UTF-8 by default. */
    appres.utf8 = true;
}

/**
 * Handle a toggle change.
 * @param[in] ix	Toggle index
 * @param[in] tt	Toggle type
 */
static void
b3270_toggle(toggle_index_t ix, enum toggle_type tt)
{
    int i;

    if (!b3270_toggle_yet || tt != TT_INITIAL) {
	return;
    }

    for (i = 0; toggle_names[i].name; i++) {
	if (toggle_names[i].index == ix) {
	    break;
	}
    }
    if (!toggle_names[i].name) {
	return;
    }

    ui_leaf(IndSetting,
	    AttrName, AT_STRING, toggle_names[i].name,
	    AttrValue, AT_BOOLEAN, toggled(ix),
	    NULL);

    if (ix == TRACING) {
	ui_leaf(IndTraceFile,
		AttrName, AT_STRING, (toggled(ix) && tracefile_name != NULL)?
		    tracefile_name: NULL,
		NULL);
    }
    if (ix == SHOW_TIMING && !toggled(SHOW_TIMING)) {
	status_untiming();
    }
}

void
menubar_retoggle(toggle_index_t ix)
{
    b3270_toggle(ix, TT_INITIAL);
}

void
menubar_as_set(bool sensitive _is_unused)
{
}

/**
 * Handle a generic toggle change.
 */
static void
b3270_toggle_notify(const char *name, enum resource_type type, void **address, ia_t cause, unsigned flags)
{
    if (address == NULL) {
	return;
    }

    switch (type) {
    case XRM_STRING:
	ui_leaf(IndSetting,
		AttrName, AT_STRING, name,
		AttrValue, AT_STRING, *(char **)address,
		AttrCause, AT_STRING, ia_name[cause],
		NULL);
	break;
    case XRM_INT:
	ui_leaf(IndSetting,
		AttrName, AT_STRING, name,
		AttrValue, AT_INT, (int64_t)*(int *)address,
		AttrCause, AT_STRING, ia_name[cause],
		NULL);
	break;
    case XRM_BOOLEAN:
	ui_leaf(IndSetting,
		AttrName, AT_STRING, name,
		AttrValue, AT_BOOLEAN, *(bool *)address,
		AttrCause, AT_STRING, ia_name[cause],
		NULL);
	break;
    }
}

/**
 * TLS password GUI.
 * @param[out] buf	Returned password
 * @param[in] size	Buffer size
 * @param[in] again	true if this is a re-prompt (old password was bad)
 * @return SP_SUCCESS if password entered, SP_FAILURE to abort, SP_PENDING to
 *  indicate that a prompt was displayed and there is no answer yet,
 *  SP_NOT_SUPPORTED to indicate that password prompting is not supported.
 */
tls_passwd_ret_t
tls_passwd_gui_callback(char *buf, int size, bool again)
{
    if (push_password(again)) {
	return SP_PENDING;
    } else {
	return SP_NOT_SUPPORTED;
    }
}

/* State change for the printer session. */
static void
b3270_printer(bool on)
{
    ui_leaf(IndOia,
	    AttrField, AT_STRING, "printer-session",
	    AttrValue, AT_BOOLEAN, on,
	    AttrLu, AT_STRING, on? pr3287_session_lu(): NULL,
	    NULL);
}

/* State change for the terminal name. */
static void
b3270_terminal_name(bool on _is_unused)
{
    report_terminal_name();
}

/* Give the model change logic permission to run. */
bool
model_can_change(void)
{
    return true;
}

/**
 * Main module registration.
 */
static void
b3270_register(void)
{
    static action_table_t actions[] = {
	{ AnClearRegion,	ClearRegion_action,	0 },
	{ AnCrash,		Crash_action,		ACTION_HIDDEN },
	{ "ForceStatus",	ForceStatus_action,	ACTION_HIDDEN },
    };
    static opt_t b3270_opts[] = {
	{ OptCallback, OPT_STRING,  false, ResCallback,
	    aoffset(scripting.callback), NULL, "Callback address and port" },
	{ OptIndent,   OPT_BOOLEAN, true,  ResIndent,    aoffset(b3270.indent),
	    NULL, "Indent ouput" },
	{ OptJson,     OPT_BOOLEAN, true,  ResJson,      aoffset(b3270.json),
	    NULL, "Use JSON format" },
	{ OptNoWrapperDoc,OPT_BOOLEAN,false,ResWrapperDoc,aoffset(b3270.wrapper_doc),
	    NULL, "Do not use an XML wrapper document" },
	{ OptUtf8,     OPT_BOOLEAN, true,  ResUtf8,      aoffset(utf8),
	    NULL, "Force local codeset to be UTF-8" },
	{ OptXml,      OPT_BOOLEAN, false, ResJson,      aoffset(b3270.json),
	    NULL, "Use XML format" },
    };
    static res_t b3270_resources[] = {
	{ ResCallback,		aoffset(scripting.callback), XRM_STRING },
	{ ResIdleCommand,aoffset(idle_command),     XRM_STRING },
	{ ResIdleCommandEnabled,aoffset(idle_command_enabled),XRM_BOOLEAN },
	{ ResIdleTimeout,aoffset(idle_timeout),     XRM_STRING },
	{ ResIndent,		aoffset(b3270.indent),XRM_BOOLEAN },
	{ ResJson,		aoffset(b3270.json),XRM_BOOLEAN },
	{ ResUtf8,		aoffset(utf8),      XRM_BOOLEAN },
	{ ResWrapperDoc,	aoffset(b3270.wrapper_doc), XRM_BOOLEAN },
    };
    static xres_t b3270_xresources[] = {
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

    /* Register the toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register for state changes. */
    register_schange(ST_CONNECT, b3270_connect);
    register_schange(ST_NEGOTIATING, b3270_connect);
    register_schange(ST_3270_MODE, b3270_connect);
    register_schange(ST_LINE_MODE, b3270_connect);
    register_schange(ST_SECURE, b3270_secure);
    register_schange(ST_CODEPAGE, b3270_new_codepage);
    register_schange(ST_PRINTER, b3270_printer);
    register_schange(ST_TERMINAL_NAME, b3270_terminal_name);

    /* Register our actions. */
    register_actions(actions, array_count(actions));

    /* Register our options. */
    register_opts(b3270_opts, array_count(b3270_opts));

    /* Register our resources. */
    register_resources(b3270_resources, array_count(b3270_resources));
    register_xresources(b3270_xresources, array_count(b3270_xresources));
}
