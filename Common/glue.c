/*
 * Copyright (c) 1993-2016 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	glue.c
 *		Common initialization logic, command-line parsing, reading
 *		resources, etc.
 */

#include "globals.h"
#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#endif /*]*/
#include <signal.h>
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actions.h"
#include "charset.h"
#include "popups.h" /* must come before child_popups.h */
#include "child_popups.h"
#include "ctlrc.h"
#include "glue.h"
#include "glue_gui.h"
#include "host.h"
#include "kybd.h"
#include "macros.h"
#include "nvt.h"
#include "opts.h"
#include "product.h"
#include "readres.h"
#include "screen.h"
#include "selectc.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"
#include "xio.h"

#if defined(_WIN32) /*[*/
# include "winvers.h"
#endif /*]*/

#define LAST_ARG	"--"

/* Typedefs */
typedef const char *ccp_t;

/* Statics */
static void no_minus(const char *arg);
#if defined(LOCAL_PROCESS) /*[*/
static void parse_local_process(int *argcp, const char **argv,
	const char **cmds);
#endif /*]*/
static void set_appres_defaults(void);
static void parse_options(int *argcp, const char **argv);
static void parse_set_clear(int *argcp, const char **argv);
static int parse_model_number(char *m);
static merge_profile_t *merge_profilep = NULL;
static char *session_suffix;
static size_t session_suffix_len;
#if defined(_WIN32) /*[*/
static char *session_short_suffix;
static size_t session_short_suffix_len;
#endif /*]*/
static opt_t *sorted_help = NULL;
unsigned sorted_help_count = 0;

/* Globals */
const char     *programname;
char		full_model_name[13] = "IBM-";
char	       *model_name = &full_model_name[4];
AppRes          appres;
int		children = 0;
bool		exiting = false;
char	       *command_string = NULL;
char	       *profile_name = NULL;
char	       *profile_path = NULL;
bool		any_error_output = false;

/* Register a profile merge function. */
void
register_merge_profile(merge_profile_t *m)
{
    merge_profilep = m;
}

/* Parse the command line and read in any session file. */
int
parse_command_line(int argc, const char **argv, const char **cl_hostname)
{
    size_t cl;
    int i;
    int hn_argc;
    size_t sl;
    size_t xcmd_len = 0;
    char *xcmd;
    int xargc;
    const char **xargv;
    bool read_session_or_profile = false;

    /* Figure out who we are */
#if defined(_WIN32) /*[*/
    programname = strrchr(argv[0], '\\');
#else /*][*/
    programname = strrchr(argv[0], '/');
#endif /*]*/
    if (programname) {
	++programname;
    } else {
	programname = argv[0];
    }

    /* Save the command string for tracing purposes. */
    cl = strlen(programname);
    for (i = 0; i < argc; i++) {
	cl += 1 + strlen(argv[i]);
    }
    cl++;
    command_string = Malloc(cl);
    (void) strcpy(command_string, programname);
    for (i = 0; i < argc; i++) {
	(void) strcat(strcat(command_string, " "), argv[i]);
    }

    /*
     * Save the command-line options so they can be reapplied after
     * the session file or profile has been read in.
     */
    xcmd_len = 0;
    for (i = 0; i < argc; i++) {
	xcmd_len += strlen(argv[i]) + 1;
    }
    xcmd = Malloc(xcmd_len + 1);
    xargv = (const char **)Malloc((argc + 1) * sizeof(char *));
    xcmd_len = 0;
    for (i = 0; i < argc; i++) {
	xargv[i] = xcmd + xcmd_len;
	(void) strcpy(xcmd + xcmd_len, argv[i]);
	xcmd_len += strlen(argv[i]) + 1;
    }
    xargv[i] = NULL;
    *(xcmd + xcmd_len) = '\0';
    xargc = argc;

#if defined(LOCAL_PROCESS) /*[*/ 
    /* Pick out the -e option. */
    parse_local_process(&argc, argv, cl_hostname);
#endif /*]*/    

    /* Set the defaults. */
    set_appres_defaults();

    /* Parse command-line options. */
    parse_options(&argc, argv);

    /* Pick out the remaining -set and -clear toggle options. */
    parse_set_clear(&argc, argv);

    /* Now figure out if there's a hostname. */
    for (hn_argc = 1; hn_argc < argc; hn_argc++) {
	if (!strcmp(argv[hn_argc], LAST_ARG)) {
	    break;
	}
    }

    /* Verify command-line syntax. */
    switch (hn_argc) {
    case 1:
	break;
    case 2:
	no_minus(argv[1]);
	*cl_hostname = argv[1];
	break;
    case 3:
	no_minus(argv[1]);
	no_minus(argv[2]);
	*cl_hostname = xs_buffer("%s:%s", argv[1], argv[2]);
	break;
    default:
	usage("Too many command-line arguments");
	break;
    }

    /* Delete the host name and any "--". */
    if (argv[hn_argc] != NULL && !strcmp(argv[hn_argc], LAST_ARG)) {
	hn_argc++;
    }
    if (hn_argc > 1) {
	for (i = 1; i < argc - hn_argc + 2; i++) {
	    argv[i] = argv[i + hn_argc - 1];
	}
    }

    /* Merge in the session. */
    if (session_suffix == NULL) {
	session_suffix = xs_buffer(".%s", app);
	session_suffix_len = strlen(session_suffix);
    }
#if defined(_WIN32) /*[*/
    if (session_short_suffix == NULL) {
	session_short_suffix = xs_buffer(".%.3s", app);
	session_short_suffix_len = strlen(session_short_suffix);
    }
#endif /*]*/
    if (*cl_hostname != NULL &&
	(((sl = strlen(*cl_hostname)) > session_suffix_len &&
	  !strcasecmp(*cl_hostname + sl - session_suffix_len, session_suffix))
#if defined(_WIN32) /*[*/
	 || ((sl = strlen(*cl_hostname)) > session_short_suffix_len &&
	  !strcasecmp(*cl_hostname + sl - session_short_suffix_len,
	      session_short_suffix))
#endif /*]*/
	 )) {

	const char *pname;

	if (!read_resource_file(*cl_hostname, true)) {
	    x3270_exit(1);
	}

	read_session_or_profile = true;

	pname = strrchr(*cl_hostname, '\\');
	if (pname != NULL) {
	    pname++;
	} else {
	    pname = *cl_hostname;
	}
	profile_name = NewString(pname);
	Replace(profile_path, NewString(profile_name));

	sl = strlen(profile_name);
	if (sl > session_suffix_len &&
		!strcasecmp(profile_name + sl - session_suffix_len,
		    session_suffix)) {
	    profile_name[sl - session_suffix_len] = '\0';
#if defined(_WIN32) /*[*/
	} else if (sl > session_short_suffix_len &&
		!strcasecmp(profile_name + sl - session_short_suffix_len,
			session_short_suffix)) {
	    profile_name[sl - session_short_suffix_len] = '\0';
#endif /*]*/
	}

	*cl_hostname = appres.hostname; /* might be NULL */
    } else {
	/* There is no session file. */

	/* For c3270 only, read in the c3270 profile (~/.c3270pro). */
	if (merge_profilep != NULL) {
	    read_session_or_profile = (*merge_profilep)();
	}

	/*
	 * If there was a hostname resource defined somewhere, but not
	 * as a positional command-line argument, pretend it was one,
	 * so we will connect to it at start-up.
	 */
	if (*cl_hostname == NULL && appres.hostname != NULL) {
	    *cl_hostname = appres.hostname;
	}
    }

    /*
     * Now parse the command-line arguments again, so they take
     * precedence over the session file or profile.
     */
    if (read_session_or_profile) {
	parse_options(&xargc, xargv);
	parse_set_clear(&xargc, xargv);
    }
    /* Can't free xcmd, parts of it are still in use. */
    Free((char *)xargv);

    /*
     * All right, we have all of the resources defined.
     * Sort out the contradictory and implicit settings.
     */

    if (appres.apl_mode) {
	appres.charset = Apl;
    }
    if (*cl_hostname == NULL) {
	appres.once = false;
    }
    if (!appres.debug_tracing) {
	/* debug_tracing was explicitly cleared */
	 set_toggle(TRACING, false);
    }
#if defined(_WIN32) /*[*/
    if (appres.utf8) {
	/* utf8 overrides local_cp */
	appres.local_cp = CP_UTF8;
    }
#endif /*]*/

    return argc;
}

/*
 * Initialize the model number and oversize. This needs to happen before the
 * screen is initialized.
 */
void
model_init(void)
{
    int model_number;
    int ovc, ovr;

    /*
     * Sort out model and color modes, based on the model number resource.
     */
    model_number = parse_model_number(appres.model);
    if (model_number < 0) {
	popup_an_error("Invalid model number: %s", appres.model);
	model_number = 0;
    }
    if (!model_number) {
#if defined(RESTRICT_3279) /*[*/
	model_number = 3;
#else /*][*/
	model_number = 4;
#endif /*]*/
    }
#if defined(RESTRICT_3279) /*[*/
    if (appres.m3279 && model_number == 4) {
	model_number = 3;
    }
#endif /*]*/
    if (appres.interactive.mono) {
	appres.m3279 = false;
    }

    if (!appres.extended) {
	appres.oversize = NULL;
    }

    ovc = 0;
    ovr = 0;
    if (appres.extended && appres.oversize != NULL) {
	if (product_auto_oversize() && !strcasecmp(appres.oversize, "auto")) {
	    ovc = -1;
	    ovr = -1;
	} else {
	    int x_ovc, x_ovr;
	    char junk;

	    if (sscanf(appres.oversize, "%dx%d%c", &x_ovc, &x_ovr,
			&junk) == 2) {
		ovc = x_ovc;
		ovr = x_ovr;
	    } else {
		xs_warning("Invalid %s value '%s'", ResOversize,
			appres.oversize);
	    }
	}
    }
    set_rows_cols(model_number, ovc, ovr);
    net_set_default_termtype();
}

static void
no_minus(const char *arg)
{
    if (arg[0] == '-') {
	usage(xs_buffer("Unknown or incomplete option: %s", arg));
    }
}

#if defined(LOCAL_PROCESS) /*[*/
/*
 * Pick out the -e option.
 */
static void
parse_local_process(int *argcp, const char **argv, const char **cmds)
{
    int i, j;
    int e_len = -1;
    char *cmds_buf = NULL;

    for (i = 1; i < *argcp; i++) {
	if (strcmp(argv[i], OptLocalProcess)) {
	    continue;
	}

	/* Matched.  Copy 'em. */
	e_len = strlen(OptLocalProcess) + 1;
	for (j = i+1; j < *argcp; j++) {
	    e_len += 1 + strlen(argv[j]);
	}
	e_len++;
	cmds_buf = Malloc(e_len);
	(void) strcpy(cmds_buf, OptLocalProcess);
	for (j = i+1; j < *argcp; j++) {
	    (void) strcat(strcat(cmds_buf, " "), argv[j]);
	}

	/* Stamp out the remaining args. */
	*argcp = i;
	argv[i] = NULL;
	break;
    }
    *cmds = cmds_buf;
}
#endif /*]*/

static void
set_appres_defaults(void)
{
    /* Set the defaults. */
    appres.extended = true;
    appres.m3279 = true;
    appres.typeahead = true;
    appres.debug_tracing = true;
    appres.conf_dir = LIBX3270DIR;

    appres.model = "4";
    appres.hostsfile = NULL;
    appres.port = "23";
    appres.charset = "bracket";
    appres.termname = NULL;
    appres.macros = NULL;
#if !defined(_WIN32) /*[*/
    appres.trace_dir = "/tmp";
#endif /*]*/
    appres.oversize = NULL;
    appres.bind_limit = true;
    appres.new_environ = true;
    appres.max_recent = 5;

    appres.linemode.icrnl = true;
    appres.linemode.onlcr = true;
    appres.linemode.erase = "^H";
    appres.linemode.kill = "^U";
    appres.linemode.werase = "^W";
    appres.linemode.rprnt = "^R";
    appres.linemode.lnext = "^V";
    appres.linemode.intr = "^C";
    appres.linemode.quit = "^\\";
    appres.linemode.eof = "^D";

    appres.unlock_delay = true;
    appres.unlock_delay_ms = 350;

    set_toggle(CURSOR_POS, true);
    set_toggle(AID_WAIT, true);

#if defined(_WIN32) /*[*/
    appres.local_cp = GetACP();
#endif /*]*/
    appres.devname = "x3270";

#if defined(HAVE_LIBSSL) /*[*/
    appres.ssl.tls = true;
#endif /*]*/

    /* Let the product set the ones it wants. */
    product_set_appres_defaults();
}

#if defined(_WIN32) /*[*/
# define PR3287_NAME "wpr3287"
#else /*][*/
# define PR3287_NAME "pr3287"
#endif /*]*/

static opt_t base_opts[] = {
#if defined(HAVE_LIBSSL) /*[*/
{ OptAcceptHostname,OPT_STRING,false,ResAcceptHostname,aoffset(ssl.accept_hostname),
    "any|DNS:<name>|IP:<addr>","Host name to accept from server certificate" },
#endif /*]*/
{ OptAplMode,  OPT_BOOLEAN, true,  ResAplMode,   aoffset(apl_mode),
    NULL, "Turn on APL mode" },
#if defined(HAVE_LIBSSL) /*[*/
{ OptCaDir,    OPT_STRING,  false, ResCaDir,     aoffset(ssl.ca_dir),
    "<directory>","OpenSSL CA certificate database directory" },
{ OptCaFile,   OPT_STRING,  false, ResCaFile,    aoffset(ssl.ca_file),
    "<filename>", "OpenSSL CA certificate file" },
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
{ OptCertFile, OPT_STRING,  false, ResCertFile,  aoffset(ssl.cert_file),
    "<filename>", "OpenSSL certificate file" },
{ OptCertFileType,OPT_STRING,false,ResCertFileType,  aoffset(ssl.cert_file_type),
    "pem|asn1",   "OpenSSL certificate file type" },
{ OptChainFile,OPT_STRING,  false,ResChainFile,  aoffset(ssl.chain_file),
    "<filename>", "OpenSSL certificate chain file" },
#endif /*]*/
{ OptCharset,  OPT_STRING,  false, ResCharset,   aoffset(charset),
    "<name>", "Use host ECBDIC character set (code page) <name>"},
{ OptClear,    OPT_SKIP2,   false, NULL,         NULL,
    "<toggle>", "Turn on <toggle>" },
{ OptConnectTimeout, OPT_INT,false,ResConnectTimeout,aoffset(connect_timeout),
    "<seconds>", "Timeout for host connect requests" },
{ OptDevName,  OPT_STRING,  false, ResDevName,   aoffset(devname),
    "<name>", "Device name (workstation ID) for RFC 4777" },
#if defined(LOCAL_PROCESS) /*[*/
{ OptLocalProcess,OPT_SKIP2,false, NULL,         NULL,
    "<command> [<arg>...]", "Run <command> instead of making TELNET conection"
},
#endif /*]*/
{ OptHostsFile,OPT_STRING,  false, ResHostsFile, aoffset(hostsfile),
    "<filename>", "Use <hostname> as the ibm_hosts file" },
{ OptHttpd,    OPT_STRING,  false, ResHttpd,     aoffset(httpd_port),
    "[<addr>:]<port>", "TCP port to listen on for http requests" },
#if defined(HAVE_LIBSSL) /*[*/
{ OptKeyFile,  OPT_STRING,  false, ResKeyFile, aoffset(ssl.key_file),
    "<filename>", "Get OpenSSL private key from <filename>" },
{ OptKeyFileType,OPT_STRING,false, ResKeyFileType,aoffset(ssl.key_file_type),
    "pem|asn1",   "OpenSSL private key file type" },
{ OptKeyPasswd,OPT_STRING,  false, ResKeyPasswd,aoffset(ssl.key_passwd),
    "file:<filename>|string:<text>","OpenSSL private key password" },
#endif /*]*/
#if defined(_WIN32) /*[*/
{ OptLocalCp,  OPT_INT,	false, ResLocalCp,   aoffset(local_cp),
    "<codepage>", "Use <codepage> instead of ANSI codepage for local I/O"
},
#endif /*]*/
{ OptLoginMacro, OPT_STRING, false, ResLoginMacro, aoffset(login_macro),
    "Action([arg[,arg...]]) [...]"
},
{ OptModel,    OPT_STRING,  false, ResModel,     aoffset(model),
    "[327{8,9}-]<n>", "Emulate a 3278 or 3279 model <n>" },
{ OptNvtMode,  OPT_BOOLEAN, true,  ResNvtMode,   aoffset(nvt_mode),
    NULL,	"Begin in NVT mode" },
{ OptOversize, OPT_STRING,  false, ResOversize,  aoffset(oversize),
    "<cols>x<rows>", "Larger screen dimensions" },
{ OptPort,     OPT_STRING,  false, ResPort,      aoffset(port),
    "<port>", "Default TELNET port" },
{ OptProxy,    OPT_STRING,  false, ResProxy,     aoffset(proxy),
    "<type>:<host>[:<port>]", "Proxy type and server" },
{ OptScriptPort,OPT_STRING, false, ResScriptPort, aoffset(script_port),
    "[<addr>:]<port>", "TCP port to listen on for script commands" },
{ OptScriptPortOnce,OPT_BOOLEAN,true,ResScriptPortOnce,aoffset(script_port_once),
    NULL, "Accept one script connection, then exit" },
#if defined(HAVE_LIBSSL) /*[*/
{ OptSelfSignedOk, OPT_BOOLEAN, true, ResSelfSignedOk, aoffset(ssl.self_signed_ok),
    NULL, "Allow self-signed host certificates" },
#endif /*]*/
{ OptSet,      OPT_SKIP2,   false, NULL,         NULL,
    "<toggle>", "Turn on <toggle>" },
{ OptSocket,   OPT_BOOLEAN, true,  ResSocket,    aoffset(socket),
    NULL, "Create socket for script control" },
{ OptTermName, OPT_STRING,  false, ResTermName,  aoffset(termname),
    "<name>", "Send <name> as TELNET terminal name" },
{ OptTrace,    OPT_BOOLEAN, true,  ResTrace,     toggle_aoffset(TRACING),
    NULL, "Enable tracing" },
{ OptTraceFile,OPT_STRING,  false, ResTraceFile, aoffset(trace_file),
    "<file>", "Write traces to <file>" },
{ OptTraceFileSize,OPT_STRING,false,ResTraceFileSize,aoffset(trace_file_size),
    "<n>[KM]", "Limit trace file to <n> bytes" },
{ OptUser,     OPT_STRING,  false, ResUser,      aoffset(user),
    "<name>", "User name for RFC 4777" },
{ OptV,        OPT_V,	false, NULL,	     NULL,
    NULL, "Display build options and character sets" },
#if defined(HAVE_LIBSSL) /*[*/
{ OptVerifyHostCert,OPT_BOOLEAN,true,ResVerifyHostCert,aoffset(ssl.verify_host_cert),
    NULL, "Enable OpenSSL host certificate validation" },
#endif /*]*/
{ OptVersion,  OPT_V,	false, NULL,	     NULL,
    NULL, "Display build options and character sets" },
{ "-xrm",      OPT_XRM,     false, NULL,         NULL,
    "'*.<resource>: <value>'", "Set <resource> to <value>"
},
{ LAST_ARG,    OPT_DONE,    false, NULL,         NULL,
    NULL, "Terminate argument list" }
};

typedef struct optlist {
    struct optlist *next;
    opt_t *opts;
    unsigned count;
} optlist_t;
static optlist_t first_optlist = { NULL, base_opts, array_count(base_opts) };
static optlist_t *optlist = &first_optlist;
static optlist_t **last_optlist = &first_optlist.next;

/*
 * Register an additional set of options.
 */
void
register_opts(opt_t *opts, unsigned num_opts)
{
    optlist_t *o;

    o = Malloc(sizeof(optlist_t));

    o->next = NULL;
    o->opts = opts;
    o->count = num_opts;

    *last_optlist = o;
    last_optlist = &o->next;
}

/*
 * Pick out command-line options and set up appres.
 */
static void
parse_options(int *argcp, const char **argv)
{
    int i;
    unsigned j;
    int argc_out = 0;
    const char **argv_out =
	(const char **) Malloc((*argcp + 1) * sizeof(char *));
    optlist_t *o;
    opt_t *opts;

    /* Parse the command-line options. */
    argv_out[argc_out++] = argv[0];

    for (i = 1; i < *argcp; i++) {
	bool found = false;

	for (o = optlist; o != NULL && !found; o = o->next) {
	    opts = o->opts;
	    for (j = 0; j < o->count; j++) {
		if (!strcmp(argv[i], opts[j].name)) {
		    found = true;
		    break;
		}
	    }
	}
	if (!found) {
	    argv_out[argc_out++] = argv[i];
	    continue;
	}

	switch (opts[j].type) {
	case OPT_BOOLEAN:
	    *(bool *)opts[j].aoff = opts[j].flag;
	    if (opts[j].res_name != NULL) {
		add_resource(NewString(opts[j].name),
			opts[j].flag? "true": "false");
	    }
	    break;
	case OPT_STRING:
	    if (i == *argcp - 1) {	/* missing arg */
		popup_an_error("Missing value for '%s'", argv[i]);
		continue;
	    }
	    *(const char **)opts[j].aoff = argv[++i];
	    if (opts[j].res_name != NULL) {
		add_resource(NewString(opts[j].res_name), NewString(argv[i]));
	    }
	    break;
	case OPT_XRM:
	    if (i == *argcp - 1) {	/* missing arg */
		popup_an_error("Missing value for '%s'", argv[i]);
		continue;
	    }
	    parse_xrm(argv[++i], "-xrm");
	    break;
	case OPT_SKIP2:
	    argv_out[argc_out++] = argv[i++];
	    if (i < *argcp) {
		argv_out[argc_out++] = argv[i];
	    }
	    break;
	case OPT_NOP:
	    break;
	case OPT_INT:
	    if (i == *argcp - 1) {	/* missing arg */
		popup_an_error("Missing value for '%s'", argv[i]);
		continue;
	    }
	    *(int *)opts[j].aoff = atoi(argv[++i]);
	    if (opts[j].res_name != NULL) {
		add_resource(NewString(opts[j].name), NewString(argv[i]));
	    }
	    break;
	case OPT_V:
	    dump_version();
	    break;
	case OPT_DONE:
	    while (i < *argcp) {
		argv_out[argc_out++] = argv[i++];
	    }
	    break;
	}
    }
    *argcp = argc_out;
    argv_out[argc_out] = NULL;
    (void) memcpy((char *)argv, (char *)argv_out,
    (argc_out + 1) * sizeof(char *));
    Free((char *)argv_out);
}

/* Comparison function for help qsort. */
static int
help_cmp(const void *p1, const void *p2)
{
    const opt_t *s1 = (const opt_t *)p1;
    const opt_t *s2 = (const opt_t *)p2;
    const char *n1 = s1->name;
    const char *n2 = s2->name;

    /* Test for equality first. */
    if (!strcmp(n1, n2)) {
	return 0;
    }

    /* '--' is always last. */
    if (!strcmp(n1, "--")) {
	return 1;
    }
    if (!strcmp(n2, "--")) {
	return -1;
    }

    /* Skip leading dashes. */
    while (*n1 == '-') {
	n1++;
    }
    while (*n2 == '-') {
	n2++;
    }

    /* Do case-instensitive string compare. */
    return strcasecmp(n1, n2);
}

/**
 * Sort the list of command-line options, for display purposes.
 */
static void
sort_help(void)
{
    optlist_t *o;
    unsigned j;
    int oix = 0;

    if (sorted_help != NULL) {
	return;
    }

    /* Count how many slots we need. */
    for (o = optlist; o != NULL; o = o->next) {
	sorted_help_count += o->count;
    }

    /* Fill in the array of elements. */
    sorted_help = (opt_t *)Malloc(sorted_help_count * sizeof(opt_t));
    for (o = optlist; o != NULL; o = o->next) {
	for (j = 0; j < o->count; j++) {
	    sorted_help[oix++] = o->opts[j];
	}
    }

    /* Sort it. */
    qsort((void *)sorted_help, sorted_help_count, sizeof(opt_t), help_cmp);
}

/* Disply command-line help. */
void
cmdline_help (bool as_action)
{
    unsigned i;
    
    sort_help();
    for (i = 0; i < sorted_help_count; i++) {
	char *h = sorted_help[i].help_opts;
	char *ht;
	char *hx = NULL;
	char *star;

	if (sorted_help[i].type == OPT_XRM &&
		h != NULL && (star = strchr(h, '*')) != NULL) {
	    ht = hx = xs_buffer("%.*s%s%s", (int)(star - h), h, app,
		    star + 1);
	} else {
	    ht = h;
	}

	if (as_action) {
	    action_output("  %s%s%s",
		    sorted_help[i].name,
		    ht? " ": "",
		    ht? ht: "");
	    action_output("    %s", sorted_help[i].help_text);
	} else {
	    fprintf(stderr, "  %s%s%s\n     %s\n",
		    sorted_help[i].name,
		    ht? " ": "",
		    ht? ht: "",
		    sorted_help[i].help_text);
	}
	if (hx != NULL) {
	    Free(hx);
	}
    }
}

/* Comparison function for toggle name qsort. */
static int
name_cmp(const void *p1, const void *p2)
{
    const char *s1 = *(char *const *)p1;
    const char *s2 = *(char *const *)p2;

    return strcmp(s1, s2);
}

/*
 * Pick out -set and -clear toggle options.
 */
static void
parse_set_clear(int *argcp, const char **argv)
{
    int i, j;
    int argc_out = 0;
    const char **argv_out =
	(const char **) Malloc((*argcp + 1) * sizeof(char *));

    argv_out[argc_out++] = argv[0];

    for (i = 1; i < *argcp; i++) {
	bool is_set = false;

	if (!strcmp(argv[i], OptSet)) {
	    is_set = true;
	} else if (strcmp(argv[i], OptClear)) {
	    argv_out[argc_out++] = argv[i];
	    continue;
	}

	if (i == *argcp - 1) {	/* missing arg */
	    continue;
	}

	/* Delete the argument. */
	i++;

	for (j = 0; toggle_names[j].name != NULL; j++) {
	    if (!toggle_supported(toggle_names[j].index)) {
		continue;
	    }
	    if (!strcasecmp(argv[i], toggle_names[j].name)) {
		appres.toggle[toggle_names[j].index] = is_set;
		break;
	    }
	}
	if (toggle_names[j].name == NULL) {
	    ccp_t *tn;
	    int ntn = 0;

	    tn = (ccp_t *)Calloc(N_TOGGLES, sizeof(ccp_t));
	    for (j = 0; toggle_names[j].name != NULL; j++) {
		if (!toggle_supported(toggle_names[j].index)) {
		    continue;
		}
		if (!toggle_names[j].is_alias) {
		    tn[ntn++] = toggle_names[j].name;
		}
	    }
	    qsort((void *)tn, ntn, sizeof(const char *), name_cmp);
	    fprintf(stderr, "Unknown toggle name '%s'. Toggle names are:\n",
		    argv[i]);
	    for (j = 0; j < ntn; j++) {
		fprintf(stderr, " %s", tn[j]);
	    }
	    fprintf(stderr, "\n");
	    Free((void *)tn);
	    exit(1);
	}

    }
    *argcp = argc_out;
    argv_out[argc_out] = NULL;
    (void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
    Free((char *)argv_out);
}

/*
 * Parse the model number.
 * Returns -1 (error), 0 (default), or the specified number.
 */
static int
parse_model_number(char *m)
{
    size_t sl;
    int n;

    sl = strlen(m);

    /* An empty model number is no good. */
    if (!sl) {
	return 0;
    }

    if (sl > 1) {
	/*
	 * If it's longer than one character, it needs to start with
	 * '327[89]', and it sets the m3279 resource.
	 */
	if (!strncmp(m, "3278", 4)) {
	    appres.m3279 = false;
	} else if (!strncmp(m, "3279", 4)) {
	    appres.m3279 = true;
	} else {
	    return -1;
	}
	m += 4;
	sl -= 4;

	/* Check more syntax.  -E is allowed, but ignored. */
	switch (m[0]) {
	case '\0':
	    /* Use default model number. */
	    return 0;
	case '-':
	    /* Model number specified. */
	    m++;
	    sl--;
	    break;
	default:
	    return -1;
	}
	switch (sl) {
	case 1: /* n */
	    break;
	case 3:	/* n-E */
	    if (strcasecmp(m + 1, "-E")) {
		return -1;
	    }
	    break;
	default:
	    return -1;
	}
    }

    /* Check the numeric model number. */
    n = atoi(m);
    if (n >= 2 && n <= 5) {
	return n;
    } else {
	return -1;
    }
}

/*
 * Parse '-xrm' options.
 * Understands only:
 *   {c,s,tcl}3270.<resourcename>: value
 *   *<resourcename>: value
 * Class names need not apply.
 */

static res_t base_resources[] = {
    { ResBindLimit,	aoffset(bind_limit),	XRM_BOOLEAN },
    { ResBindUnlock,	aoffset(bind_unlock),	XRM_BOOLEAN },
    { ResBsdTm,		aoffset(bsd_tm),		XRM_BOOLEAN },
#if defined(HAVE_LIBSSL) /*[*/
    { ResAcceptHostname,aoffset(ssl.accept_hostname),XRM_STRING },
    { ResCaDir,		aoffset(ssl.ca_dir),	XRM_STRING },
    { ResCaFile,	aoffset(ssl.ca_file),	XRM_STRING },
    { ResCertFile,	aoffset(ssl.cert_file),	XRM_STRING },
    { ResCertFileType,aoffset(ssl.cert_file_type),	XRM_STRING },
    { ResChainFile,	aoffset(ssl.chain_file),	XRM_STRING },
#endif /*]*/
    { ResCharset,	aoffset(charset),	XRM_STRING },
    { ResColor8,	aoffset(color8),	XRM_BOOLEAN },
    { ResConfDir,	aoffset(conf_dir),	XRM_STRING },
    { ResConnectTimeout,aoffset(connect_timeout),XRM_INT },
    { ResCrosshairColor,aoffset(interactive.crosshair_color),	XRM_STRING },
    { ResDbcsCgcsgid, aoffset(dbcs_cgcsgid),	XRM_STRING },
    { ResDftBufferSize,aoffset(ft.dft_buffer_size_bc),XRM_INT },/* deprecated */
    { ResEof,		aoffset(linemode.eof),	XRM_STRING },
    { ResErase,		aoffset(linemode.erase),	XRM_STRING },
    { ResExtended,	aoffset(extended),	XRM_BOOLEAN },
    { ResFtAllocation,	aoffset(ft.allocation),	XRM_STRING },
    { ResFtAvblock,	aoffset(ft.avblock),	XRM_INT },
    { ResFtBlksize,	aoffset(ft.blksize),	XRM_INT },
    { ResFtBufferSize,aoffset(ft.dft_buffer_size),XRM_INT },
#if defined(_WIN32) /*[*/
    { ResFtCodePage,	aoffset(ft.codepage_bc),XRM_INT }, /* deprecated */
    { ResFtWindowsCodePage,aoffset(ft.codepage),XRM_INT },
#endif /*]*/
    { ResFtCr,		aoffset(ft.cr),		XRM_STRING },
    { ResFtDirection,	aoffset(ft.direction),	XRM_STRING },
    { ResFtExist,	aoffset(ft.exist),	XRM_STRING },
    { ResFtHost,	aoffset(ft.host),	XRM_STRING },
    { ResFtHostFile,	aoffset(ft.host_file),	XRM_STRING },
    { ResFtLocalFile,	aoffset(ft.local_file),	XRM_STRING },
    { ResFtLrecl,	aoffset(ft.lrecl),	XRM_INT },
    { ResFtMode,	aoffset(ft.mode),	XRM_STRING },
    { ResFtPrimarySpace,aoffset(ft.primary_space),XRM_INT },
    { ResFtRecfm,	aoffset(ft.recfm),	XRM_STRING },
    { ResFtRemap,	aoffset(ft.remap),	XRM_STRING },
    { ResFtSecondarySpace,aoffset(ft.secondary_space),XRM_INT },
    { ResHostname,	aoffset(hostname),	XRM_STRING },
    { ResHostsFile,	aoffset(hostsfile),	XRM_STRING },
    { ResIcrnl,		aoffset(linemode.icrnl),	XRM_BOOLEAN },
    { ResInlcr,		aoffset(linemode.inlcr),	XRM_BOOLEAN },
    { ResOnlcr,		aoffset(linemode.onlcr),	XRM_BOOLEAN },
    { ResIntr,		aoffset(linemode.intr),	XRM_STRING },
#if defined(HAVE_LIBSSL) /*[*/
    { ResKeyFile,	aoffset(ssl.key_file),	XRM_STRING },
    { ResKeyFileType,	aoffset(ssl.key_file_type),XRM_STRING },
    { ResKeyPasswd,	aoffset(ssl.key_passwd),	XRM_STRING },
#endif /*]*/
    { ResKill,		aoffset(linemode.kill),	XRM_STRING },
    { ResLnext,		aoffset(linemode.lnext),	XRM_STRING },
#if defined(_WIN32) /*[*/
    { ResLocalCp,	aoffset(local_cp),	XRM_INT },
#endif /*]*/
    { ResLoginMacro,aoffset(login_macro),	XRM_STRING },
    { ResM3279,	aoffset(m3279),			XRM_BOOLEAN },
    { ResModel,	aoffset(model),			XRM_STRING },
    { ResModifiedSel, aoffset(modified_sel),	XRM_BOOLEAN },
    { ResNewEnviron,aoffset(new_environ),	XRM_BOOLEAN },
    { ResNumericLock, aoffset(numeric_lock),	XRM_BOOLEAN },
    { ResOerrLock,	aoffset(oerr_lock),	XRM_BOOLEAN },
    { ResOversize,	aoffset(oversize),	XRM_STRING },
    { ResPort,	aoffset(port),			XRM_STRING },
    { ResProxy,		aoffset(proxy),		XRM_STRING },
    { ResQrBgColor,	aoffset(qr_bg_color),	XRM_BOOLEAN },
    { ResQuit,		aoffset(linemode.quit),	XRM_STRING },
    { ResRprnt,		aoffset(linemode.rprnt),	XRM_STRING },
    { ResScreenTraceFile,aoffset(screentrace_file),XRM_STRING },
    { ResSecure,	aoffset(secure),		XRM_BOOLEAN },
#if defined(HAVE_LIBSSL) /*[*/
    { ResSelfSignedOk,aoffset(ssl.self_signed_ok),XRM_BOOLEAN },
#endif /*]*/
    { ResSbcsCgcsgid, aoffset(sbcs_cgcsgid),	XRM_STRING },
    { ResScriptPort,aoffset(script_port),	XRM_STRING },
    { ResSuppressActions,aoffset(suppress_actions),XRM_STRING },
    { ResTermName,	aoffset(termname),	XRM_STRING },
#if defined(HAVE_LIBSSL) /*[*/
    { ResTls,	aoffset(ssl.tls),		XRM_BOOLEAN },
#endif /*]*/
    { ResTraceDir,	aoffset(trace_dir),	XRM_STRING },
    { ResTraceFile,	aoffset(trace_file),	XRM_STRING },
    { ResTraceFileSize,aoffset(trace_file_size),	XRM_STRING },
    { ResTraceMonitor,aoffset(trace_monitor),	XRM_BOOLEAN },
    { ResTypeahead,	aoffset(typeahead),	XRM_BOOLEAN },
    { ResUnlockDelay,aoffset(unlock_delay),	XRM_BOOLEAN },
    { ResUnlockDelayMs,aoffset(unlock_delay_ms),	XRM_INT },
#if defined(HAVE_LIBSSL) /*[*/
    { ResVerifyHostCert,aoffset(ssl.verify_host_cert),XRM_BOOLEAN },
#endif /*]*/
    { ResWerase,	aoffset(linemode.werase),XRM_STRING }
};

typedef struct reslist {
    struct reslist *next;
    res_t *resources;
    unsigned count;
} reslist_t;
static reslist_t first_reslist = {
    NULL, base_resources, array_count(base_resources)
};
static reslist_t *reslist = &first_reslist;
static reslist_t **last_reslist = &first_reslist.next;

/*
 * Register an additional set of resources.
 */
void
register_resources(res_t *res, unsigned num_res)
{
    reslist_t *r;

    r = Malloc(sizeof(reslist_t));

    r->next = NULL;
    r->resources = res;
    r->count = num_res;

    *last_reslist = r;
    last_reslist = &r->next;
}

/*
 * Compare two strings, allowing the second to differ by uppercasing the
 * first character of the second.
 */
static int
strncapcmp(const char *known, const char *unknown, size_t unk_len)
{
    if (unk_len != strlen(known)) {
	return -1;
    }
    if (!strncmp(known, unknown, unk_len)) {
	return 0;
    }
    if (unk_len > 1 &&
	unknown[0] == toupper((unsigned char)known[0]) &&
	!strncmp(known + 1, unknown + 1, unk_len - 1)) {
	return 0;
    }
    return -1;
}

typedef struct xreslist {
    struct xreslist *next;
    xres_t *xresources;
    unsigned count;
} xreslist_t;
static xreslist_t *xreslist = NULL;
static xreslist_t **last_xreslist = &xreslist;

void
register_xresources(xres_t *xres, unsigned num_xres)
{
    xreslist_t *x;

    x = Malloc(sizeof(xreslist_t));

    x->next = NULL;
    x->xresources = xres;
    x->count = num_xres;

    *last_xreslist = x;
    last_xreslist = &x->next;
}

struct host_color host_color[] = {
    { "NeutralBlack",	HOST_COLOR_NEUTRAL_BLACK },
    { "Blue",		HOST_COLOR_BLUE },
    { "Red",		HOST_COLOR_RED },
    { "Pink",		HOST_COLOR_PINK },
    { "Green",		HOST_COLOR_GREEN },
    { "Turquoise",	HOST_COLOR_TURQUOISE },
    { "Yellow",		HOST_COLOR_YELLOW },
    { "NeutralWhite",	HOST_COLOR_NEUTRAL_WHITE },
    { "Black",		HOST_COLOR_BLACK },
    { "DeepBlue",	HOST_COLOR_DEEP_BLUE },
    { "Orange",		HOST_COLOR_ORANGE },
    { "Purple",		HOST_COLOR_PURPLE },
    { "PaleGreen",	HOST_COLOR_PALE_GREEN },
    { "PaleTurquoise",	HOST_COLOR_PALE_TURQUOISE },
    { "Grey",		HOST_COLOR_GREY },
    { "Gray",		HOST_COLOR_GREY }, /* alias */
    { "White",		HOST_COLOR_WHITE },
    { NULL,		0 }
};

/*
 * Validate a resource that is fetched explicitly, rather than via appres.
 */
static int
valid_explicit(const char *resname, size_t len)
{
    xreslist_t *x;
    unsigned i;
    int j;

    for (x = xreslist; x != NULL; x = x->next) {
	for (i = 0; i < x->count; i++) {
	    size_t sl = strlen(x->xresources[i].name);

	    switch (x->xresources[i].type) {
	    case V_FLAT:
		/* Exact match. */
		if (len == sl &&
		    !strncmp(x->xresources[i].name, resname, sl)) {
		    return 0;
		}
		break;
	    case V_WILD:
		/* xxx.* match. */
		if (len > sl + 1 &&
		    resname[sl] == '.' &&
		    !strncmp(x->xresources[i].name, resname, sl)) {
		    return 0;
		}
		break;
	    case V_COLOR:
		/* xxx<host-color> or xxx<host-color-index> match. */
		for (j = 0; host_color[j].name != NULL; j++) {
		    char *xbuf;

		    xbuf = xs_buffer("%s%s", x->xresources[i].name,
			    host_color[j].name);
		    if (strlen(xbuf) == len &&
			!strncmp(xbuf, resname, len)) {
			    Free(xbuf);
			    return 0;
		    }
		    Free(xbuf);
		    xbuf = xs_buffer("%s%d", x->xresources[i].name,
			    host_color[j].index);
		    if (strlen(xbuf) == len &&
			!strncmp(xbuf, resname, len)) {
			    Free(xbuf);
			    return 0;
		    }
		    Free(xbuf);
		}
		break;
	    }
	}
    }

    return -1;
}

void
parse_xrm(const char *arg, const char *where)
{
    const char *name;
    size_t rnlen;
    const char *s;
    unsigned i;
    char *t;
    void *address = NULL;
    enum resource_type type = XRM_STRING;
    bool quoted;
    char c;
    reslist_t *r;
    char *hide;
    bool arbitrary = false;

    /* Validate and split. */
    if (validate_and_split_resource(where, arg, &name, &rnlen, &s) < 0) {
	return;
    }

    /* Look up the name. */
    for (r = reslist; r != NULL; r = r->next) {
	bool found = false;

	for (i = 0; i < r->count && !found; i++) {
	    if (!strncapcmp(r->resources[i].name, name, rnlen)) {
		address = r->resources[i].address;
		type = r->resources[i].type;
		found = true;
		break;
	    }
	}
    }

    if (address == NULL) {
	for (i = 0; toggle_names[i].name != NULL; i++) {
	    if (!toggle_supported(toggle_names[i].index)) {
		continue;
	    }
	    if (!strncapcmp(toggle_names[i].name, name, rnlen)) {
		address = &appres.toggle[toggle_names[i].index];
		type = XRM_BOOLEAN;
		break;
	    }
	}
    }
    if (address == NULL && valid_explicit(name, rnlen) == 0) {
	/* Handle resources that are accessed only via get_resource(). */
	address = &hide;
	type = XRM_STRING;
	arbitrary = true;
    }
    if (address == NULL) {
	xs_warning("%s: Unknown resource name: %.*s", where, (int)rnlen, name);
	return;
    }
    switch (type) {
    case XRM_BOOLEAN:
	if (!strcasecmp(s, "true") || !strcasecmp(s, "t") || !strcmp(s, "1")) {
	    *(bool *)address = true;
	} else if (!strcasecmp(s, "false") || !strcasecmp(s, "f") ||
		!strcmp(s, "0")) {
	    *(bool *)address = false;
	} else {
	    xs_warning("%s: Invalid bool value: %s", where, s);
	    *(bool *)address = false;
	}
	break;
    case XRM_STRING:
	t = Malloc(strlen(s) + 1);
	*(char **)address = t;
	quoted = false;
#if defined(_WIN32) /*[*/
	/*
	 * Ugly hack to allow unquoted UNC-path printer names from older
	 * versions of the Session Wizard to continue to work, even though the
	 * rules now require quoted backslashes in resource values.
	 */
	if (!strncapcmp(ResPrinterName, name, rnlen) &&
	    s[0] == '\\' &&
	    s[1] == '\\' &&
	    s[2] != '\\' &&
	    strchr(s + 2, '\\') != NULL) {

	    strcpy(t, s);
	    break;
	}
#endif /*]*/

	while ((c = *s++) != '\0') {
	    if (quoted) {
		switch (c) {
		case 'b':
		    *t++ = '\b';
		    break;
		case 'f':
		    *t++ = '\f';
		    break;
		case 'n':
		    *t++ = '\n';
		    break;
		case 'r':
		    *t++ = '\r';
		    break;
		case 't':
		    *t++ = '\t';
		    break;
		case '\\':
		    /* Quote the backslash. */
		    *t++ = '\\';
		    break;
		default:
		    /* Eat the backslash. */
		    *t++ = c;
		    break;
		}
		quoted = false;
	    } else if (c == '\\') {
		quoted = true;
	    } else {
		*t++ = c;
	    }
	}
	*t = '\0';
	break;
    case XRM_INT: {
	long n;
	char *ptr;

	n = strtol(s, &ptr, 0);
	if (*ptr != '\0') {
	    xs_warning("%s: Invalid Integer value: %s", where, s);
	} else {
	    *(int *)address = (int)n;
	}
	break;
	}
    }

    /* Add a new, arbitrarily-named resource. */
    if (arbitrary) {
	char *rsname;

	rsname = Malloc(rnlen + 1);
	(void) strncpy(rsname, name, rnlen);
	rsname[rnlen] = '\0';
	add_resource(rsname, hide);
    }
}

/*
 * Clean up a string for display (undo what parse_xrm does).
 */
char *
safe_string(const char *s)
{
    char *t = Malloc(1);
    int tlen = 1;

    *t = '\0';

    /*
     * Translate the string to UCS4 a character at a time.
     * If the result is a control code or backslash, expand it.
     * Otherwise, translate it back to the local encoding and
     * append it to the output.
     */
    while (*s) {
	ucs4_t u;
	int consumed;
	enum me_fail error;

	u = multibyte_to_unicode(s, strlen(s), &consumed, &error);
	if (u == 0) {
	    break;
	}
	if (u < ' ') {
	    char c = 0;
	    int inc = 0;

	    switch (u) {
	    case '\b':
		c = 'b';
		inc = 2;
		break;
	    case '\f':
		c = 'f';
		inc = 2;
		break;
	    case '\n':
		c = 'n';
		inc = 2;
		break;
	    case '\r':
		c = 'r';
		inc = 2;
		break;
	    case '\t':
		c = 't';
		inc = 2;
		break;
	    default:
		inc = 6;
		break;
	    }

	    t = Realloc(t, tlen + inc);
	    if (inc == 2) {
		*(t + tlen - 1) = '\\';
		*(t + tlen) = c;
	    } else {
		sprintf(t, "\\u%04x", u);
	    }
	    tlen += inc;
	} else {
	    t = Realloc(t, tlen + consumed);
	    memcpy(t + tlen - 1, s, consumed);
	    tlen += consumed;
	}
	s += consumed;
    }
    *(t + tlen - 1) = '\0';
    return t;
}

/* Read resources from a file. */
bool
read_resource_file(const char *filename, bool fatal)
{
    return read_resource_filex(filename, fatal);
}

/* Screen globals. */

bool visible_control = false;

bool flipped = false;

/* Replacements for functions in popups.c. */

bool error_popup_visible = false;

/* Pop up an error dialog. */
void
popup_an_error(const char *fmt, ...)
{
    va_list args;
    char *s;

    va_start(args, fmt);
    s = xs_vbuffer(fmt, args);
    va_end(args);

    /* Log to the trace file. */
    vtrace("%s\n", s);

    if (sms_redirect()) {
	sms_error(s);
    } else {
	screen_suspend();
	(void) fprintf(stderr, "%s\n", s);
	fflush(stderr);
	any_error_output = true;
	macro_output = true;
    }
    Free(s);
}

/* Pop up an error dialog, based on an error number. */
void
popup_an_errno(int errn, const char *fmt, ...)
{
    va_list args;
    char *s;

    va_start(args, fmt);
    s = xs_vbuffer(fmt, args);
    va_end(args);

    if (errn > 0) {
	popup_an_error("%s: %s", s, strerror(errn));
    } else {
	popup_an_error("%s", s);
    }
    Free(s);
}

void
action_output(const char *fmt, ...)
{
    va_list args;
    char *s;

    va_start(args, fmt);
    s = xs_vbuffer(fmt, args);
    va_end(args);
    if (sms_redirect()) {
	sms_info("%s", s);
    } else {
	if (!glue_gui_output(s)) {
	    (void) printf("%s\n", s);
	}
	any_error_output = true;
	macro_output = true;
    }
    Free(s);
}

void
popup_printer_output(bool is_err _is_unused, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list args;
    char *m;

    va_start(args, fmt);
    m = xs_vbuffer(fmt, args);
    va_end(args);
    action_output("%s", m);
    Free(m);
}

void
popup_child_output(bool is_err _is_unused, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list args;
    char *m;

    va_start(args, fmt);
    m = xs_vbuffer(fmt, args);
    va_end(args);
    action_output("%s", m);
    Free(m);
}

void
child_popup_init(void)
{
}
