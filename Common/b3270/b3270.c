/*
 * Copyright (c) 1993-2009, 2013-2019 Paul Mattes.
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
#include "b3270proto.h"
#include "bind-opt.h"
#include "boolstr.h"
#include "bscreen.h"
#include "b_password.h"
#include "codepage.h"
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
#include "kybd.h"
#include "lazya.h"
#include "min_version.h"
#include "nvt.h"
#include "nvt_gui.h"
#include "opts.h"
#include "popups.h"
#include "pr3287_session.h"
#include "print_screen.h"
#include "product.h"
#include "query.h"
#include "screen.h"
#include "selectc.h"
#include "sio.h"
#include "sio_glue.h"
#include "sio_internal.h"
#include "ssl_passwd_gui.h"
#include "status.h"
#include "task.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"
#include "xio.h"
#include "xscroll.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "windirs.h"
# include "winvers.h"
#endif /*]*/

#define STATS_POLL	(2 * 1000)

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
    { APL_MODE,		b3270_toggle,	TOGGLE_NEED_INIT }
};
static const char *cstate_name[] = {
    "not-connected",
    "reconnecting",
    "ssl-password-pending",
    "resolving",
    "pending",
    "negotiating",
    "connected-initial",
    "connected-nvt",
    "connected-nvt-charmode",
    "connected-3270",
    "connected-unbound",
    "connected-e-nvt",
    "connected-sscp",
    "connected-tn3270e"
};

static char *pending_model;
static char *pending_oversize;
static bool pending_extended;
static bool pending_extended_value;

static void b3270_register(void);
static void b3270_toggle_notify(const char *name, const char *value);

void
usage(const char *msg)
{
    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, "Usage: %s [options] [profile-file.b3270]\n", programname);
    fprintf(stderr, "Options:\n");
    cmdline_help(false);
    exit(1);
}

static void
dump_stats()
{
    ui_vleaf("stats",
	    "bytes-received", lazyaf("%d", brcvd),
	    "records-received", lazyaf("%d", rrcvd),
	    "bytes-sent", lazyaf("%d", bsent),
	    "records-sent", lazyaf("%d", rsent),
	    NULL);
}

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
    stats_ioid = AddTimeOut(STATS_POLL, stats_poll);
}

/**
 * Respond to a change in the connection, 3270 mode, or line mode.
 */
static void
b3270_connect(bool ignored)
{       
    static enum cstate old_cstate = (int)NOT_CONNECTED;

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
	ui_vleaf("connection",
		"state", cstate_name[(int)cstate],
		NULL);
    } else {
	char *cause = NewString(ia_name[connect_ia]);
	char *s = cause;
	char c;

	while ((c = *s)) {
	    c = tolower((int)(unsigned char)c);
	    if (c == ' ') {
		c = '-';
	    }
	    *s++ = c;
	}
	ui_vleaf("connection",
		"state", cstate_name[(int)cstate],
		"host", current_host,
		"cause", cause,
		NULL);
	Free(cause);

	/* Clear the screen. */
	if (old_cstate == NOT_CONNECTED) {
	    ctlr_erase(true);
	}
    }

    /* If just connected, dump initial stats. */
    if (cstate != NOT_CONNECTED && stats_ioid == NULL_IOID) {
	brcvd = 0;
	rrcvd = 0;
	bsent = 0;
	rsent = 0;
	dump_stats();
	stats_ioid = AddTimeOut(STATS_POLL, stats_poll);
    }

    old_cstate = cstate;
}

static void
b3270_secure(bool ignored)
{
    static bool is_secure = false;

    if (net_secure_connection() == is_secure) {
	return;
    }
    is_secure = net_secure_connection();

     ui_vleaf("tls",
	     "secure", net_secure_connection()? "true": "false",
	     "verified",
		 net_secure_connection()? (net_secure_unverified()? "false": "true"): NULL,
	     "session", net_session_info(),
	     "host-cert", net_server_cert_info(),
	     NULL);
}

/* Report the terminal name. */
static void
report_terminal_name(void)
{
    if (appres.termname != NULL) {
	ui_vleaf("terminal-name",
		"text", appres.termname,
		"override", "true",
		NULL);
    } else {
	ui_vleaf("terminal-name",
		"text", (ov_rows || ov_cols)? "IBM-DYNAMIC": full_model_name,
		"override", "false",
		NULL);
    }
}

#if !defined(_WIN32) /*[*/
/* Empty SIGCHLD handler, ensuring that we can collect child exit status. */
static void
sigchld_handler(int ignored)
{
# if !defined(_AIX) /*[*/
    (void) signal(SIGCHLD, sigchld_handler);
# endif /*]*/
}
#endif /*]*/

/* Dump the character set list. Called at initialization time. */
static void
dump_charsets(void)
{
    csname_t *csnames = get_csnames();

    if (csnames != NULL) {
	int i;

	for (i = 0; csnames[i].name != NULL; i++) {
	    const char **params = Calloc(2 + (2 * csnames[i].num_aliases) + 1,
		    sizeof(char *));
	    int j;

	    params[0] = "name";
	    params[1] = csnames[i].name;
	    for (j = 0; j < csnames[i].num_aliases; j++) {
		params[2 + (j * 2)] = lazyaf("alias%d", j + 1);
		params[2 + (j * 2) + 1] = csnames[i].aliases[j];
	    }
	    ui_leaf(IndCharset, params);
	    Free(params);
	}
	free_csnames(csnames);
    }
}

int
main(int argc, char *argv[])
{
    const char *cl_hostname = NULL;

    if (sizeof(cstate_name)/sizeof(cstate_name[0]) != NUM_CSTATE) {
	Error("b3270 cstate_name has the wrong number of elements");
    }

#if defined(_WIN32) /*[*/
    (void) get_version_info();
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
    scroll_register();
    toggles_register();
    trace_register();
    xio_register();
    sio_glue_register();
    hio_register();

    argc = parse_command_line(argc, (const char **)argv, &cl_hostname);
    if (cl_hostname != NULL) {
	usage("Unrecognized option(s)");
    }

    check_min_version(appres.min_version);

    ui_io_init();
    ui_vleaf("hello",
	    "version", lazyaf("%d.%d.%d", our_major, our_minor, our_iteration),
	    "build", build,
	    "copyright",
lazyaf("\
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
	(void) codepage_init(NULL);
    }
    dump_charsets();
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
    ctlr_init(-1);
    ctlr_reinit(-1);
    report_terminal_name();
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

    /* Collect child exit status. */
    (void) signal(SIGCHLD, sigchld_handler);
#endif /*]*/

    /* Handle initial toggle settings. */
    initialize_toggles();

    /* Send SSL set-up */
    ui_vleaf("tls-hello",
	    "supported", sio_supported()? "true": "false",
	    "provider", sio_provider(),
	    "options", sio_option_names(),
	    NULL);

    /*
     * Register for extended toggle notifies, which will cause a dump of the
     * current values.
     */
    register_extended_toggle_notify(b3270_toggle_notify);

    /* Prepare to run a peer script. */
    peer_script_init();

    ui_vleaf("ready", NULL);

    /* Process events forever. */
    while (1) {
	(void) process_events(true);
	screen_disp(false);
    }
}

/*
 * Canonical representation of the model, given specific defaults for
 * color mode and extended mode.
 */
static char *
canonical_modelx(const char *res, bool color, bool extended)
{
    size_t sl;
    char *digitp;
    char *colorp = color? "9": "8";

    if (res == NULL) {
	return NULL;
    }
    sl = strlen(res);

    if ((sl != 1 && sl != 6 && sl != 8) ||
	(sl == 1 &&
	 (digitp = strchr("2345", res[0])) == NULL) ||
	(((sl == 6) || (sl == 8)) &&
	 (strncmp(res, "327", 3) ||
	  (colorp = strchr("89", res[3])) == NULL ||
	  res[4] != '-' ||
	  (digitp = strchr("2345", res[5])) == NULL)) ||
	((sl == 8) &&
	 (res[6] != '-' || strchr("Ee", res[7]) == NULL))) {
	return NULL;
    }
    if (sl == 8) {
	extended = true;
    }
    return xs_buffer("327%c-%c%s", *colorp, *digitp, extended? "-E": "");
}

/*
 * Canonical representation of the model.
 */
static char *
canonical_model(const char *res)
{
    return canonical_modelx(res, appres.m3279, appres.extended);
}

/*
 * Toggle the model.
 */
static bool
toggle_model(const char *name _is_unused, const char *value)
{
    Replace(pending_model, *value? NewString(value): NULL);
    return true;
}

/*
 * Toggle oversize.
 */
static bool
toggle_oversize(const char *name _is_unused, const char *value)
{
    Replace(pending_oversize, NewString(value));
    return true;
}

/*
 * Toggle extended mode.
 */
static bool
toggle_extended(const char *name _is_unused, const char *value)
{
    const char *errmsg = boolstr(value, &pending_extended_value);

    if (errmsg != NULL) {
	popup_an_error("%s %s", ResExtended, errmsg);
	return false;
    }

    pending_extended = true;
    return true;
}

static bool
toggle_nop_seconds(const char *name _is_unused, const char *value)
{
    unsigned long l;
    char *end;
    int secs;

    if (!*value) {
	appres.nop_seconds = 0;
	net_nop_seconds();
	return true;
    }

    l = strtoul(value, &end, 10);
    secs = (int)l;
    if (*end != '\0' || (unsigned long)secs != l || secs < 0) {
	popup_an_error("Invalid %s value", ResNopSeconds);
	return false;
    }
    appres.nop_seconds = secs;
    net_nop_seconds();
    return true;
}

/*
 * Done function for changing the model, oversize and extended mode.
 */
static bool
toggle_model_done(bool success)
{
    char color = '9';
    char digit;
    unsigned ovr = 0, ovc = 0;
    int model_number;
    bool extended;
    struct {
	int model_num;
	int rows;
	int cols;
	int ov_cols;
	int ov_rows;
	bool m3279;
	bool alt;
	bool extended;
    } old;
    bool oversize_was_pending = (pending_oversize != NULL);
    bool res = true;
    bool implicit_extended_change = false;

    if (!success ||
	    (pending_model == NULL &&
	     pending_oversize == NULL &&
	     !pending_extended)) {
	goto done;
    }

    if (PCONNECTED) {
	popup_an_error("Toggle: Cannot change %s, %s or %s while "
		"connected", ResModel, ResOversize, ResExtended);
	goto fail;
    }

    /* Reconcile simultaneous changes. */
    if (pending_model != NULL) {
	char *canon = canonical_modelx(pending_model, appres.m3279,
		pending_extended? pending_extended_value: appres.extended);

	if (canon == NULL) {
	    popup_an_error("Toggle(%s): value must be 327{89}-{2345}[-E]",
		    ResModel);
	    goto fail;
	}

	Replace(pending_model, canon);
	color = pending_model[3];
	digit = pending_model[5];

	/* Adding -E to the model will implicitly turn on extended mode. */
	if (strlen(pending_model) == 8 && 
		!pending_extended &&
		!appres.extended) {
	    pending_extended = true;
	    pending_extended_value = true;
	    implicit_extended_change = true;
	}
    }

    if (pending_extended) {
	extended = pending_extended_value;
	if (!pending_extended_value) {
	    /* Without extended, no oversize. */
	    Replace(pending_oversize, NewString(""));
	}
    } else {
	extended = appres.extended;
    }

    if (pending_oversize != NULL) {
	if (*pending_oversize) {
	    char x, junk;
	    if (sscanf(pending_oversize, "%u%c%u%c", &ovc, &x, &ovr, &junk) != 3
		    || x != 'x') {
		popup_an_error("Toggle(%s): Oversize must be <cols>x<rows>",
			ResOversize);
		goto fail;
	    }
	} else {
	    ovc = 0;
	    ovr = 0;
	}
    } else {
	ovc = ov_cols;
	ovr = ov_rows;
    }

    /* Save the current settings. */
    old.model_num = model_num;
    old.rows = ROWS;
    old.cols = COLS;
    old.ov_rows = ov_rows;
    old.ov_cols = ov_cols;
    old.m3279 = appres.m3279;
    old.alt = screen_alt;
    old.extended = appres.extended;

    /* Change settings. */
    if (pending_model != NULL) {
	model_number = digit - '0';
	appres.m3279 = color == '9';
    } else {
	model_number = model_num;
    }
    appres.extended = extended;
    set_rows_cols(model_number, ovc, ovr);

    if (model_num != model_number ||
	    ov_rows != (int)ovr ||
	    ov_cols != (int)ovc) {
	/* Failed. Restore the old settings. */
	appres.m3279 = old.m3279;
	set_rows_cols(old.model_num, old.ov_cols, old.ov_rows);
	ROWS = old.rows;
	COLS = old.cols;
	screen_alt = old.alt;
	appres.extended = old.extended;
	return false;
    }

    ROWS = maxROWS;
    COLS = maxCOLS;
    ctlr_reinit(MODEL_CHANGE);

    /* Reset the screen state. */
    screen_init();
    ctlr_erase(true);

    /* Report the new terminal name. */
    if (appres.termname == NULL) {
	report_terminal_name();
    }

    if (pending_model != NULL) {
	Replace(appres.model, pending_model);
    }
    if (pending_extended && pending_model == NULL) {
	force_toggle_notify(ResModel);
    }
    pending_model = NULL;
    if (implicit_extended_change) {
	force_toggle_notify(ResExtended);
    }
    if (pending_oversize != NULL) {
	if (*pending_oversize) {
	    Replace(appres.oversize, pending_oversize);
	    pending_oversize = NULL;
	} else {
	    bool force = !oversize_was_pending && appres.oversize != NULL;

	    Replace(appres.oversize, NULL);
	    if (force) {
		/* Turning off extended killed oversize. */
		force_toggle_notify(ResOversize);
	    }
	}
    }

goto done;

fail:
    res = false;

done:
    Replace(pending_model, NULL);
    Replace(pending_oversize, NULL);
    pending_extended = false;
    pending_extended_value = false;
    return res;
}

/*
 * Terminal name toggle.
 */
static bool
toggle_terminal_name(const char *name _is_unused, const char *value)
{
    if (PCONNECTED) {
	popup_an_error("Toggle(%s): Cannot change while connected",
		ResTermName);
	return false;
    }

    appres.termname = clean_termname(*value? value: NULL);
    report_terminal_name();
    return true;
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

    action_debug("ClearRegion", ia, argc, argv);
    if (check_argc("ClearRegion", argc, 4, 4) < 0) {
	return false;
    }

    row = atoi(argv[0]);
    column = atoi(argv[1]);
    rows = atoi(argv[2]);
    columns = atoi(argv[3]);

    if (row <= 0 || row > ROWS || column <= 0 || column > COLS) {
	popup_an_error("ClearRegion: invalid coordinates");
    }

    if (rows < 0 || columns < 0 ||
	    row - 1 + rows > ROWS || column - 1 + columns > COLS) {
	popup_an_error("ClearRegion: invalid size");
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
    action_debug("Crash", ia, argc, argv);
    if (check_argc("Crash", argc, 1, 1) < 0) {
	return false;
    }
    if (!strcasecmp(argv[0], "Assert")) {
	assert(false);
	popup_an_error("Crash: Assert did not work");
    } else if (!strcasecmp(argv[0], "Exit")) {
	exit(999);
	popup_an_error("Crash: Exit did not work");
    } else if (!strcasecmp(argv[0], "Null")) {
	char *s = NULL;
	char c;

	printf("%c\n", c = *s);
	popup_an_error("Crash: Null did not work");
    } else {
	popup_an_error("Crash: Must specify Assert, Exit or Null");
    }

    return false;
}

/**
 * xterm text escape
 *
 * @param[in] opcode    Operation to perform
 * @param[in] text      Associated text
 */
void
xterm_text_gui(int code, const char *text)
{
    if (code == 0 || code == 1) {
	ui_vleaf("icon-name",
		"text", text,
		NULL);
    }
    if (code == 0 || code == 2) {
	ui_vleaf("window-title",
		"text", text,
		NULL);
    }

    if (code == 50) {
	ui_vleaf("font",
		"text", text,
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
     * Set defaults like s3270 -- operator error locks the keyboard and
     * no unlock delay.
     *
     * TODO: I need a way to change these from the UI.
     */
    appres.oerr_lock = true;
    appres.unlock_delay = false;
    appres.interactive.save_lines = 4096;
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

    for (i = 0; toggle_names[i].name; i++) {
	if (toggle_names[i].index == ix) {
	    break;
	}
    }
    if (!toggle_names[i].name) {
	return;
    }

    ui_vleaf(IndSetting,
	    AttrName, toggle_names[i].name,
	    AttrValue, toggled(ix)? ValTrue: ValFalse,
	    NULL);

    if (ix == TRACING) {
	ui_vleaf(IndTraceFile,
		AttrName, (toggled(ix) && tracefile_name != NULL)?
		    tracefile_name: NULL,
		NULL);
    }
}

/**
 * Handle a generic toggle change.
 */
static void
b3270_toggle_notify(const char *name, const char *value)
{
    ui_vleaf(IndSetting,
	    AttrName, name,
	    AttrValue, value,
	    NULL);
}

/**
 * SSL password GUI.
 * @param[out] buf	Returned password
 * @param[in] size	Buffer size
 * @param[in] again	true if this is a re-prompt (old password was bad)
 * @return SP_SUCCESS if password entered, SP_FAILURE to abort, SP_PENDING to
 *  indicate that a prompt was displayed and there is no answer yet,
 *  SP_NOT_SUPPORTED to indicate that password prompting is not supported.
 */
ssl_passwd_ret_t
ssl_passwd_gui_callback(char *buf, int size, bool again)
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
    ui_vleaf(IndOia,
	    "field", "printer-session",
	    "value", on? "true": "false",
	    "lu", on? pr3287_session_lu(): NULL,
	    NULL);
}

/**
 * Main module registration.
 */
static void
b3270_register(void)
{
    static action_table_t actions[] = {
	{ "ClearRegion",ClearRegion_action,0 },
	{ "Crash",	Crash_action,	0 }
    };
    static opt_t b3270_opts[] = {
	{ OptScripted, OPT_NOP,     false, ResScripted,  NULL,
	    NULL, "Turn on scripting" },
	{ OptUtf8,     OPT_BOOLEAN, true,  ResUtf8,      aoffset(utf8),
	    NULL, "Force local codeset to be UTF-8" }
    };
    static res_t b3270_resources[] = {
	{ ResIdleCommand,aoffset(idle_command),     XRM_STRING },
	{ ResIdleCommandEnabled,aoffset(idle_command_enabled),XRM_BOOLEAN },
	{ ResIdleTimeout,aoffset(idle_timeout),     XRM_STRING },
	{ ResUtf8,		aoffset(utf8),      XRM_BOOLEAN }
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
    register_extended_toggle(ResTermName, toggle_terminal_name, NULL, NULL,
	    (void **)&appres.termname, XRM_STRING);
    register_extended_toggle(ResModel, toggle_model, toggle_model_done,
	    canonical_model, (void **)&appres.model, XRM_STRING);
    register_extended_toggle(ResOversize, toggle_oversize, toggle_model_done,
	    NULL, (void **)&appres.oversize, XRM_STRING);
    register_extended_toggle(ResExtended, toggle_extended, toggle_model_done,
	    NULL, (void **)&appres.extended, XRM_BOOLEAN);
    register_extended_toggle(ResNopSeconds, toggle_nop_seconds, NULL,
	    NULL, (void **)&appres.nop_seconds, XRM_INT);

    /* Register for state changes. */
    register_schange(ST_CONNECT, b3270_connect);
    register_schange(ST_HALF_CONNECT, b3270_connect);
    register_schange(ST_3270_MODE, b3270_connect);
    register_schange(ST_LINE_MODE, b3270_connect);
    register_schange(ST_SECURE, b3270_secure);
    register_schange(ST_CODEPAGE, b3270_new_codepage);
    register_schange(ST_PRINTER, b3270_printer);

    /* Register our actions. */
    register_actions(actions, array_count(actions));

    /* Register our options. */
    register_opts(b3270_opts, array_count(b3270_opts));

    /* Register our resources. */
    register_resources(b3270_resources, array_count(b3270_resources));
    register_xresources(b3270_xresources, array_count(b3270_xresources));
}
