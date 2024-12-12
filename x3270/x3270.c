/**
 * @copyright
 * Copyright (c) 1993-2024 Paul Mattes.
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

/**
 *	@file x3270.c
 *		@par A 3270 Terminal Emulator for X11
 *		Main procedure.
 */

#include "globals.h"
#include <assert.h>
#include <sys/wait.h>
#include <X11/StringDefs.h>
#include <X11/Core.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actions.h"
#include "bind-opt.h"
#include "boolstr.h"
#include "codepage.h"
#include "cookiefile.h"
#include "ctlrc.h"
#include "ft.h"
#include "host.h"
#include "httpd-core.h"
#include "httpd-nodes.h"
#include "httpd-io.h"
#include "idle.h"
#include "keymap.h"
#include "kybd.h"
#include "login_macro.h"
#include "min_version.h"
#include "model.h"
#include "nvt.h"
#include "opts.h"
#include "popups.h"
#include "pr3287_session.h"
#include "prefer.h"
#include "print_screen.h"
#include "print_window.h"
#include "product.h"
#include "proxy_toggle.h"
#include "query.h"
#include "resolver.h"
#include "resourcesc.h"
#include "save_restore.h"
#include "screen.h"
#include "selectc.h"
#include "sio.h"
#include "sio_glue.h"
#include "status.h"
#include "task.h"
#include "telnet.h"
#include "telnet_new_environ.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
#include "screentrace.h"
#include "utils.h"
#include "vstatus.h"
#include "xactions.h"
#include "xappres.h"
#include "xglobals.h"
#include "xio.h"
#include "xkybd.h"
#include "xmenubar.h"
#include "xpopups.h"
#include "xsave.h"
#include "xscreen.h"
#include "xscroll.h"
#include "xselectc.h"
#include "xstatus.h"

/* Globals */
const char     *programname;
Display        *display;
int             default_screen;
Window          root_window;
int             screen_depth;
Widget          toplevel;
XtAppContext    appcontext;
Atom            a_delete_me, a_save_yourself, a_3270, a_registry, a_encoding,
		a_state, a_net_wm_state, a_net_wm_state_maximized_horz,
		a_net_wm_state_maximized_vert, a_net_wm_name, a_atom, a_spacing,
		a_pixel_size, a_font;
Pixmap          gray;
XrmDatabase     rdb;
AppRes		appres;
xappres_t	xappres;
bool		exiting = false;
char           *user_title = NULL;

/* Statics */
static void	peek_at_xevent(XEvent *);
static XtErrorMsgHandler old_emh;
static void	trap_colormaps(String, String, String, String, String *,
			Cardinal *);
static bool  colormap_failure = false;
#if defined(LOCAL_PROCESS) /*[*/
static void	parse_local_process(int *argcp, char **argv, char **cmds);
#endif /*]*/
#if defined(DEBUG_SET_CLEAR) /*[*/
static void	dump_argv(const char *, int, char **);
#endif /*]*/
static void	parse_set_clear(int *, char **);
static void	label_init(void);
static void	sigchld_handler(int);
static char    *user_icon_name = NULL;
static void	copy_xres_to_res_bool(void);

XrmOptionDescRec base_options[]= {
    { OptActiveIcon,	DotActiveIcon,	XrmoptionNoArg,		ResTrue },
    { OptAplMode,	DotAplMode,	XrmoptionNoArg,		ResTrue },
    { OptAcceptHostname,DotAcceptHostname,XrmoptionSepArg,	NULL },
    { OptCaDir,		DotCaDir,	XrmoptionSepArg,	NULL },
    { OptCaFile,	DotCaFile,	XrmoptionSepArg,	NULL },
    { OptCertFile,	DotCertFile,	XrmoptionSepArg,	NULL },
    { OptCertFileType,	DotCertFileType,XrmoptionSepArg,	NULL },
    { OptChainFile,	DotChainFile,	XrmoptionSepArg,	NULL },
    { OptCharClass,	DotCharClass,	XrmoptionSepArg,	NULL },
    { OptCharset,	DotCodePage,	XrmoptionSepArg,	NULL },
    { OptClear,		".xxx",		XrmoptionSkipArg,	NULL },
    { OptClientCert,	DotClientCert,	XrmoptionSepArg,	NULL },
    { OptCodePage,	DotCodePage,	XrmoptionSepArg,	NULL },
    { OptColorScheme,	DotColorScheme,	XrmoptionSepArg,	NULL },
    { OptConnectTimeout,DotConnectTimeout,XrmoptionSepArg,	NULL },
    { OptCookieFile,	DotCookieFile,	XrmoptionSepArg,	NULL },
    { OptDevName,	DotDevName,	XrmoptionSepArg,	NULL },
    { OptTrace,		DotTrace,	XrmoptionNoArg,		ResTrue },
#if defined(LOCAL_PROCESS) /*[*/
    { OptLocalProcess,	NULL,		XrmoptionSkipLine,	NULL },
#endif /*]*/
    { OptEmulatorFont,	DotEmulatorFont,XrmoptionSepArg,	NULL },
    { OptHostsFile,	DotHostsFile,	XrmoptionSepArg,	NULL },
    { OptHttpd,		DotHttpd,	XrmoptionSepArg,	NULL },
    { OptIconName,	".iconName",	XrmoptionSepArg,	NULL },
    { OptIconX,		".iconX",	XrmoptionSepArg,	NULL },
    { OptIconY,		".iconY",	XrmoptionSepArg,	NULL },
    { OptKeyFile,	DotKeyFile,	XrmoptionSepArg,	NULL },
    { OptKeyFileType,	DotKeyFileType,	XrmoptionSepArg,	NULL },
    { OptKeymap,	DotKeymap,	XrmoptionSepArg,	NULL },
    { OptKeypadOn,	DotKeypadOn,	XrmoptionNoArg,		ResTrue },
    { OptKeyPasswd,	DotKeyPasswd,	XrmoptionSepArg,	NULL },
    { OptLoginMacro,	DotLoginMacro,	XrmoptionSepArg,	NULL },
    { OptTlsMaxProtocol,DotTlsMaxProtocol,XrmoptionSepArg,	NULL },
    { OptTlsMinProtocol,DotTlsMinProtocol,XrmoptionSepArg,	NULL },
    { OptMinVersion,	DotMinVersion,	XrmoptionSepArg,	NULL },
    { OptModel,		DotModel,	XrmoptionSepArg,	NULL },
    { OptMono,		DotMono,	XrmoptionNoArg,		ResTrue },
    { OptNoScrollBar,	DotScrollBar,	XrmoptionNoArg,		ResFalse },
    { OptNoVerifyHostCert,DotVerifyHostCert,XrmoptionNoArg,	ResFalse },
    { OptNvtMode,	DotNvtMode,	XrmoptionNoArg,		ResTrue },
    { OptOnce,		DotOnce,	XrmoptionNoArg,		ResTrue },
    { OptOversize,	DotOversize,	XrmoptionSepArg,	NULL },
    { OptPort,		DotPort,	XrmoptionSepArg,	NULL },
    { OptPreferIpv4,	DotPreferIpv4,	XrmoptionNoArg,		ResTrue },
    { OptPreferIpv6,	DotPreferIpv6,	XrmoptionNoArg,		ResTrue },
    { OptPrinterLu,	DotPrinterLu,	XrmoptionSepArg,	NULL },
    { OptProxy,		DotProxy,	XrmoptionSepArg,	NULL },
    { OptReconnect,	DotReconnect,	XrmoptionNoArg,		ResTrue },
    { OptSaveLines,	DotSaveLines,	XrmoptionSepArg,	NULL },
    { OptScripted,	DotScripted,	XrmoptionNoArg,		ResTrue },
    { OptScrollBar,	DotScrollBar,	XrmoptionNoArg,		ResTrue },
    { OptSecure,	DotSecure,	XrmoptionNoArg,		ResTrue },
    { OptSet,		".xxx",		XrmoptionSkipArg,	NULL },
    { OptSocket,	DotSocket,	XrmoptionNoArg,		ResTrue },
    { OptScriptPort,	DotScriptPort,	XrmoptionSepArg,	NULL },
    { OptScriptPortOnce,DotScriptPortOnce,XrmoptionNoArg,	ResTrue },
    { OptTermName,	DotTermName,	XrmoptionSepArg,	NULL },
    { OptTraceFile,	DotTraceFile,	XrmoptionSepArg,	NULL },
    { OptTraceFileSize,	DotTraceFileSize,XrmoptionSepArg,	NULL },
    { OptInputMethod,	DotInputMethod,	XrmoptionSepArg,	NULL },
    { OptPreeditType,	DotPreeditType,	XrmoptionSepArg,	NULL },
    { OptUser,		DotUser,	XrmoptionSepArg,	NULL },
    { OptUtEnv,		DotUtEnv,	XrmoptionNoArg,		ResTrue },
    { OptUtf8,		DotUtf8,	XrmoptionNoArg,		ResTrue },
    { OptVerifyHostCert,DotVerifyHostCert,XrmoptionNoArg,	ResTrue },
    { OptXrm,		NULL,		XrmoptionResArg,	NULL }
};
int num_base_options = XtNumber(base_options);

XrmOptionDescRec *options;
int num_options; /**< number of options */

/** Option help. */
static struct option_help {
    char *opt;		/**< option name */
    char *args;		/**< arguments or NULL */
    char *help;		/**< help text */
    unsigned tls_flag;	/**< flags for conditional TLS options */
} option_help[] = {
    { OptAcceptHostname, "[DNS:]<name>",
	"Host name to accept from server certificate",
	TLS_OPT_ACCEPT_HOSTNAME },
    { OptActiveIcon, NULL, "Make icon a miniature of the display" },
    { OptAplMode, NULL,    "Turn on APL mode" },
    { OptCaDir, "<directory>", "TLS CA certificate database directory",
      TLS_OPT_CA_DIR },
    { OptCaFile, "<filename>", "TLS CA certificate file", TLS_OPT_CA_FILE },
    { OptCertFile, "<file>", "TLS certificate file", TLS_OPT_CERT_FILE },
    { OptCertFileType, "pem|asn1", "TLS certificate file type",
      TLS_OPT_CERT_FILE_TYPE },
    { OptChainFile, "<filename>", "TLS certificate chain file",
      TLS_OPT_CHAIN_FILE },
    { OptCharClass, "<spec>", "Define characters for word boundaries" },
    { OptCharset, "<name>", "Alias for " OptCodePage },
    { OptClear, "<toggle>", "Turn on <toggle>" },
    { OptClientCert, "<name>", "TLS client certificate name",
      TLS_OPT_CLIENT_CERT },
    { OptCodePage, "<name>", "Use host EBCDIC code page <name>" },
    { OptColorScheme, "<name>", "Use color scheme <name>" },
    { OptConnectTimeout, "<seconds>", "Timeout for host connect requests" },
    { OptCookieFile, "<path>", "Pathname of security cookie file" },
    { OptDevName, "<name>", "Device name (workstation ID)" },
#if defined(LOCAL_PROCESS) /*[*/
    { OptLocalProcess, "<command> [arg...]", "Run process instead of connecting to host" },
#endif /*]*/
    { OptEmulatorFont, "<font>", "Font for emulator window" },
    { OptHttpd, "[<addr>:]<port>", "TCP port to listen on for http requests" },
    { OptHostsFile, "<filename>", "Pathname of ibm_hosts file" },
    { OptIconName, "<name>", "Title for icon" },
    { OptIconX, "<x>", "X position for icon" },
    { OptIconY, "<y>", "Y position for icon" },
    { OptKeyFile, "<filename>", "Get TLS private key from <filename>",
      TLS_OPT_KEY_FILE },
    { OptKeyFileType, "pem|asn1", "TLS private key file type",
      TLS_OPT_KEY_FILE_TYPE },
    { OptKeymap, "<name>[,<name>...]", "Keyboard map name(s)" },
    { OptKeypadOn, NULL, "Turn on pop-up keypad at start-up" },
    { OptKeyPasswd, "file:<filename>|string:<text>",
	"TLS private key password", TLS_OPT_KEY_PASSWD },
    { OptLoginMacro, "Action([arg[,...]]) [...]", "Macro to run at login" },
    { OptMinVersion, "<version>", "Fail unless at this version or greater" },
    { OptModel, "[327{8,9}-]<n>", "Emulate a 3278 or 3279 model <n>" },
    { OptMono, NULL, "Do not use color" },
    { OptNoScrollBar, NULL, "Disable scroll bar" },
    { OptNoVerifyHostCert, NULL, "Do not verify TLS host certificate",
	TLS_OPT_VERIFY_HOST_CERT },
    { OptNvtMode, NULL, "Begin in NVT mode" },
    { OptOnce, NULL, "Exit as soon as the host disconnects" },
    { OptOversize,  "<cols>x<rows>", "Larger screen dimensions" },
    { OptPort, "<port>", "Default TELNET port" },
    { OptPreferIpv4, NULL, "Prefer IPv4 host addresses" },
    { OptPreferIpv6, NULL, "Prefer IPv6 host addresses" },
    { OptPrinterLu,  "<luname>",
	"Automatically start a pr3287 printer session to <luname>" },
    { OptProxy, "<type>:<host>[:<port>]", "Secify proxy type and server" },
    { OptReconnect, NULL, "Reconnect to host as soon as it disconnects" },
    { OptSaveLines, "<n>", "Number of lines to save for scroll bar" },
    { OptScripted, NULL, "Accept commands on standard input" },
    { OptScrollBar, NULL, "Turn on scroll bar" },
    { OptSet, "<toggle>", "Turn on <toggle>" },
    { OptSocket,  NULL, "Create socket for script control" },
    { OptScriptPort, "<port>",
	"Listen on TCP port <port> for script connections" },
    { OptScriptPortOnce, NULL, "Accept one script connection, then exit" },
    { OptSecure, NULL, "Set secure mode" },
    { OptTermName, "<name>", "Send <name> as TELNET terminal name" },
    { OptTlsMaxProtocol, "<protocol>", "TLS maximum protocol version" },
    { OptTlsMinProtocol, "<protocol>", "TLS minimum protocol version" },
    { OptTrace, NULL, "Enable tracing" },
    { OptTraceFile, "<file>", "Write traces to <file>" },
    { OptTraceFileSize, "<n>[KM]", "Limit trace file to <n> bytes" },
    { OptInputMethod, "<name>", "Multi-byte input method" },
    { OptPreeditType, "<style>", "Define input method pre-edit type" },
    { OptUser, "<name>", "User name for RFC 4777" },
    { OptUtEnv, NULL, "Allow unit test options in the environment" },
    { OptUtf8, NULL, "Force script I/O to use UTF-8" },
    { OptV, NULL, "Display build options and character sets" },
    { OptVerifyHostCert, NULL, "Verify TLS host certificate (enabled by default)",
	TLS_OPT_VERIFY_HOST_CERT },
    { OptVersion, NULL, "Display build options and character sets" },
    { OptXrm, "'x3270.<resource>: <value>'", "Set <resource> to <vale>" }
};

/* Fallback resources. */
static String fallbacks[] = {
    /* This should be overridden by real app-defaults. */
    "*adVersion: fallback",
    NULL
};

static void x3270_register(void);
static void poll_children(void);

/* Find an option in the help list. */
static struct option_help *
find_option_help(const char *opt)
{
    unsigned j;

    for (j = 0; j < XtNumber(option_help); j++) {
	if (!strcmp(opt, option_help[j].opt)) {
	    return &option_help[j];
	}
    }
    return NULL;
}

/* Set up the options array. */
static void
setup_options(void)
{
    unsigned tls_options = sio_all_options_supported();
    int i;
    int n_filtered = 0;

    /* Count the number of filtered options. */
    for (i = 0; i < num_base_options; i++) {
	struct option_help *help = find_option_help(base_options[i].option);
	if (help == NULL) {
	    Error(Asprintf("Option %s has no help", base_options[i].option));
	}

	if (!help->tls_flag || (help->tls_flag & tls_options)) {
	    n_filtered++;
	}
    }

    /* Allocate the new array. */
    options = (XrmOptionDescRec *)Malloc(n_filtered * sizeof(XrmOptionDescRec));
    num_options = n_filtered;

    /* Copy the filtered entries into the new array. */
    n_filtered = 0;
    for (i = 0; i < num_base_options; i++) {
	struct option_help *help = find_option_help(base_options[i].option);
	if (help == NULL) {
	    Error(Asprintf("Option %s has no help", base_options[i].option));
	}

	if (!help->tls_flag || (help->tls_flag & tls_options)) {
	    options[n_filtered++] = base_options[i]; /* struct copy */
	}
    }
}

void
usage(const char *msg)
{
    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }

    fprintf(stderr, "Usage: %s [options] [[prefix:][LUname@]hostname[:port]]\n",
	    programname);
    fprintf(stderr, "       %s [options] [<session-file>.x3270]\n",
	    programname);
    fprintf(stderr, "Use " OptHelp1 " for the list of options\n");
    exit(1);
}

static void
cmdline_help(void)
{
    unsigned i;
    unsigned tls_options = sio_all_options_supported();

    fprintf(stderr, "Usage: %s [options] [[prefix:][LUname@]hostname[:port]]\n",
	    programname);
    fprintf(stderr, "       %s [options] [<session-file>.x3270]\n",
	    programname);
    fprintf(stderr, "Options:\n");
    for (i = 0; i < XtNumber(option_help); i++) {
	if (option_help[i].tls_flag == 0
		|| (option_help[i].tls_flag & tls_options)) {
	    fprintf(stderr, " %s%s%s\n   %s\n",
		    option_help[i].opt,
		    option_help[i].args? " ": "",
		    option_help[i].args? option_help[i].args: "",
		    option_help[i].help);
	}
    }
    fprintf(stderr,
	    " Plus standard Xt options like '-title' and '-geometry'\n");
}

static void
no_minus(char *arg)
{
    if (arg[0] == '-') {
	usage(Asprintf("Unknown or incomplete option: '%s'", arg));
    }
}

/* Clean up Xt (close windows gracefully). */
static void
cleanup_Xt(bool b _is_unused)
{
    XtDestroyApplicationContext(appcontext);
}

/* Duplicate string resources so they can be reallocated later. */
static void
dup_resource_strings(void *ap, XtResourceList res, Cardinal num)
{
    Cardinal c;

    for (c = 0; c < num; c++) {
	char **value;
	XtResource *r = &res[c];

	if (r->resource_type != XtRString) {
	    continue;
	}
	value = (char **)(void *)((char *)ap + r->resource_offset);
	if (*value != NULL) {
	    *value = NewString(*value);
	}
    }
}

int
main(int argc, char *argv[])
{
#if !defined(USE_APP_DEFAULTS) /*[*/
    char *dname;
    int	i;
#endif /*]*/
    Atom protocols[2];
    char *cl_hostname = NULL;
    int	model_number;
    bool mono = false;
    char *session = NULL;
    XtResource *res;
    XtResource *xres;

    /*
     * Make sure the Xt and x3270 Boolean types line up.
     * This is needed because we use the Xt resource parser to fill in all of
     * the appres fields, including those that are used by common code. Xt uses
     * 'Boolean'; common code uses 'bool'. They need to be the same.
     *
     * This requirement is no worse than the alternative, which is defining our
     * own 'Boolean' type for the common code, and hand-crafting it to be sure
     * that it is the same type as Xt's Boolean. It has the benefit of keeping
     * Xt data types completely out of common code.
     *
     * The way to make this truly portable would be to extract the Boolean
     * resources into an x3270-specific structure that uses Boolean data types,
     * and then copy them, field by field, into the common appres structure.
     */
    assert(sizeof(Boolean) == sizeof(bool));
    assert(True == true);
    assert(False == false);

    /* Figure out who we are */
    programname = strrchr(argv[0], '/');
    if (programname) {
	char *path = getenv("PATH");

	/* Add our path to $PATH so we can find x3270if. */
	putenv(Asprintf("PATH=%.*s%s%s", 
		    (int)(programname - argv[0]), argv[0],
		    path? ":": "",
		    path? path: ""));
	++programname;
    } else {
	programname = argv[0];
    }

    /* Parse a lone "-v" or "--help" first, without contacting a server. */
    if (argc == 2 && (!strcmp(argv[1], OptV) ||
		      !strcmp(argv[1], OptVersion))) {
	dump_version();
    }
    if (argc == 2 && (!strcmp(argv[1], OptHelp1) ||
		      !strcmp(argv[1], OptHelp2))) {
	cmdline_help();
	exit(0);
    }

    /*
     * Call the module registration functions, to build up the tables of
     * actions, options and callbacks. These functions have no
     * interdependencies and cannot depend on resource values.
     */
    codepage_register();
    ctlr_register();
    ft_register();
    host_register();
    idle_register();
    keymap_register();
    kybd_register();
    task_register();
    query_register();
    menubar_register();
    nvt_register();
    popups_register();
    pr3287_session_register();
    print_screen_register();
    print_window_register();
    save_restore_register();
    screen_register();
    scroll_register();
    select_register();
    status_register();
    toggles_register();
    trace_register();
    screentrace_register();
    x3270_register();
    xio_register();
    sio_glue_register();
    hio_register();
    proxy_register();
    model_register();
    net_register();
    xkybd_register();
    login_macro_register();
    vstatus_register();
    prefer_register();
    telnet_new_environ_register();

    /* Translate and validate -set and -clear toggle options. */
#if defined(DEBUG_SET_CLEAR) /*[*/
    dump_argv("before", argc, argv);
#endif /*]*/
    parse_set_clear(&argc, argv);
#if defined(DEBUG_SET_CLEAR) /*[*/
    dump_argv("after", argc, argv);
#endif /*]*/

    /* Save a copy of the command-line args for merging later. */
    save_args(argc, argv);

#if !defined(USE_APP_DEFAULTS) /*[*/
    /*
     * Figure out which fallbacks to use, based on the "-mono"
     * switch on the command line, and the depth of the display.
     */
    dname = NULL;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-mono")) {
	    mono = true;
	} else if (!strcmp(argv[i], "-display") && argc > i) {
	    dname = argv[i+1];
	}
    }
    display = XOpenDisplay(dname);
    if (display == NULL) {
	XtError("Can't open display");
    }
    if (DefaultDepthOfScreen(XDefaultScreenOfDisplay(display)) == 1) {
	mono = true;
    }
    XCloseDisplay(display);
#endif /*]*/

    /* Set up the command-line options and resources we support. */
    setup_options();

    /* Initialize. */
    toplevel = XtVaAppInitialize(
	    &appcontext,
	    "X3270",
	    options, num_options,
	    &argc, argv,
	    fallbacks,
	    XtNinput, True,
	    XtNallowShellResize, False,
	    NULL);
    display = XtDisplay(toplevel);
    rdb = XtDatabase(display);

    register_schange(ST_EXITING, cleanup_Xt);

    /*
     * Add the base translations to the toplevel object.
     * For some reason, these cannot be specified in the app-defaults.
     */
    XtVaSetValues(toplevel, XtNtranslations, XtParseTranslationTable("\
<Message>WM_PROTOCOLS:          PA-WMProtocols()\n\
<KeymapNotify>:                 PA-KeymapNotify()\n\
<PropertyNotify>WM_STATE:       PA-StateChanged()\n\
<FocusIn>:                      PA-Focus()\n\
<FocusOut>:                     PA-Focus()\n\
<ConfigureNotify>:              PA-ConfigureNotify()"), NULL);

#if defined(LOCAL_PROCESS) /*[*/
    /* Pick out the -e option. */
    parse_local_process(&argc, argv, &cl_hostname);
#endif /*]*/

    /* Verify command-line syntax. */
    switch (argc) {
    case 1:
	break;
    case 2:
	if (cl_hostname != NULL) {
	    usage(NULL);
	}
	no_minus(argv[1]);
	cl_hostname = argv[1];
	break;
    case 3:
	if (cl_hostname != NULL) {
	    usage(NULL);
	}
	no_minus(argv[1]);
	no_minus(argv[2]);
	cl_hostname = Asprintf("%s:%s", argv[1], argv[2]);
	break;
    default:
	for (i = 0; i < argc; i++) {
	    no_minus(argv[i]);
	}
	usage("Too many command-line options");
	break;
    }

    /* If the 'hostname' ends with .x3270, it is a session file. */
    if (cl_hostname != NULL &&
	strlen(cl_hostname) > strlen(".x3270") &&
	!strcmp(cl_hostname + strlen(cl_hostname) - strlen(".x3270"),
	    ".x3270")) {
	session = cl_hostname;
	cl_hostname = NULL;
    }

    /* Merge in the profile or session file. */
    merge_profile(&rdb, session, mono);

    /*
     * Save copies of resources, because it turns out that
     * XtGetApplicationResources overwrites it.
     */
    res = (XtResource *)Malloc(num_resources * sizeof(XtResource));
    memcpy(res, resources, num_resources * sizeof(XtResource));
    xres = (XtResource *)Malloc(num_xresources * sizeof(XtResource));
    memcpy(xres, xresources, num_xresources * sizeof(XtResource));

    /* Fill in appres. */
    old_emh = XtAppSetWarningMsgHandler(appcontext,
	    (XtErrorMsgHandler)trap_colormaps);
    XtGetApplicationResources(toplevel, (XtPointer)&appres, resources,
	    num_resources, 0, 0);
    XtGetApplicationResources(toplevel, (XtPointer)&xappres, xresources,
	    num_xresources, 0, 0);
    XtAppSetWarningMsgHandler(appcontext, old_emh);

    /* Copy bool values from xres to appres. */
    copy_xres_to_res_bool();

    /*
     * Handle the deprecated 'charset' resource. It is an alias for
     * 'codepage', but does not override it.
     */
    if (appres.codepage == NULL) {
	appres.codepage = appres.charset;
    }
    if (appres.codepage == NULL) {
	appres.codepage = "bracket";
    }

    /* Duplicate the strings in appres, so they can be reallocated later. */
    dup_resource_strings((void *)&appres, res, num_resources);
    dup_resource_strings((void *)&xappres, xres, num_xresources);

    /* Check the minimum version. */
    check_min_version(appres.min_version);

    /*
     * If the hostname is specified as a resource and not specified as a
     * positional argument, use the resource value.
     */
    if (cl_hostname == NULL && appres.hostname != NULL) {
	cl_hostname = appres.hostname;
    }

#if defined(USE_APP_DEFAULTS) /*[*/
    /* Check the app-defaults version. */
    if (!xappres.ad_version) {
	XtError("Outdated app-defaults file");
    } else if (!strcmp(xappres.ad_version, "fallback")) {
	XtError("No app-defaults file");
    } else if (strcmp(xappres.ad_version, app_defaults_version)) {
	xs_error("app-defaults version mismatch: want %s, got %s",
		app_defaults_version, xappres.ad_version);
    }
#endif /*]*/

    /*
     * Before the call to error_init(), errors are generally fatal.
     * Afterwards, errors are stored, and popped up when the screen is
     * realized.  Should we exit without realizing the screen, they will
     * be dumped to stderr.
     */
    error_init();

    default_screen = DefaultScreen(display);
    root_window = RootWindow(display, default_screen);
    screen_depth = DefaultDepthOfScreen(XtScreen(toplevel));

    /*
     * Sort out model and color modes, based on the model number resource.
     */
    if (screen_depth <= 1 || colormap_failure) {
	appres.interactive.mono = true;
	xappres.use_cursor_color = False;
    }
    model_number = common_model_init();

    /* Do a bit of security init. */
    if (appres.secure) {
	appres.disconnect_clear = true;
    }

    /* Set up the resolver. */
    set_46(appres.prefer_ipv4, appres.prefer_ipv6);

    /* Set up atoms. */
    a_3270 = XInternAtom(display, "3270", False);
    a_atom = XInternAtom(display, "ATOM", False);
    a_delete_me = XInternAtom(display, "WM_DELETE_WINDOW", False);
    a_encoding = XInternAtom(display, "CHARSET_ENCODING", False);
    a_font = XInternAtom(display, "FONT", False);
    a_net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    a_net_wm_state_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    a_net_wm_state_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    a_net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    a_pixel_size = XInternAtom(display, "PIXEL_SIZE", False);
    a_registry = XInternAtom(display, "CHARSET_REGISTRY", False);
    a_save_yourself = XInternAtom(display, "WM_SAVE_YOURSELF", False);
    a_spacing = XInternAtom(display, "SPACING", False);
    a_state = XInternAtom(display, "WM_STATE", False);

    /* Add the Xt-only actions. */
    xaction_init();

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
    printer_popup_init();
    ft_init();

    /* Add the wrapped actions. */
    xaction_init2();

    /* Define the keymap. */
    keymap_init(appres.interactive.key_map, false);

    if (toggled(APL_MODE)) {
    }

    screen_preinit();

    switch (codepage_init(appres.codepage)) {
    case CS_OKAY:
	break;
    case CS_NOTFOUND:
	popup_an_error("Cannot find definition for host code page \"%s\"",
		appres.codepage);
	codepage_init(NULL);
	break;
    case CS_BAD:
	popup_an_error("Invalid definition for host code page \"%s\"",
		appres.codepage);
	codepage_init(NULL);
	break;
    case CS_PREREQ:
	popup_an_error("No fonts for host code page \"%s\"",
		appres.codepage);
	codepage_init(NULL);
	break;
    case CS_ILLEGAL:
	codepage_init(NULL);
	break;
    }

    /* Initialize fonts. */
    font_init();

    /* Set up the window and icon labels. */
    label_init();

    /* Set up oversize. */
    oversize_init(model_number);

    /* Initialize the icon. */
    icon_init();

    hostfile_init();
    if (!cookiefile_init()) {
        exit(1);
    }

    if (xappres.char_class != NULL) {
	reclass(xappres.char_class);
    }

    screen_init();
    info_popup_init();
    error_popup_init();
    macros_init();

    protocols[0] = a_delete_me;
    protocols[1] = a_save_yourself;
    XSetWMProtocols(display, XtWindow(toplevel), protocols, 2);

    /* Save the command line. */
    save_init(argc, argv[1], argv[2]);

    /* Make sure we don't fall over any SIGPIPEs. */
    signal(SIGPIPE, SIG_IGN);

    /*
     * Make sure that exited child processes become zombies, so we can
     * collect their exit status.
     */
    signal(SIGCHLD, sigchld_handler);

    /* Handle initial toggle settings. */
    if (!appres.debug_tracing) {
	set_toggle_initial(TRACING, false);
    }
    initialize_toggles();

    /* Connect to the host. */
    if (cl_hostname != NULL) {
	host_connect(cl_hostname, IA_UI);
    }

    /* Prepare to run a peer script. */
    peer_script_init();

    /* Initialize APL mode. */
    if (toggled(APL_MODE)) {
	temporary_keymap(Apl);
	temporary_compose_map(Apl, "Init");
    }

    /* Process X events forever. */
    while (1) {
	XEvent event;

	while (XtAppPending(appcontext) & (XtIMXEvent | XtIMTimer)) {
	    if (XtAppPeekEvent(appcontext, &event)) {
		peek_at_xevent(&event);
	    }
	    XtAppProcessEvent(appcontext, XtIMXEvent | XtIMTimer);
	}
	screen_disp(false);
	XtAppProcessEvent(appcontext, XtIMAll);

	/* Poll for exited children. */
	poll_children();

	/* Run tasks. */
	run_tasks();

	/* Free transaction memory. */
	txflush();
    }
}

/*
 * Empty SIGCHLD handler.
 * On newer POSIX systems, this ensures that exited child processes become
 * zombies, so we can collect their exit status.
 */
static void
sigchld_handler(int ignored)
{
#if !defined(_AIX) /*[*/
    signal(SIGCHLD, sigchld_handler);
#endif /*]*/
}

/* Change the window and icon labels. */
static void
relabel(bool ignored _is_unused)
{
    char *title;
    char icon_label[8];
    bool reconnect = host_retry_mode && reconnect_host != NULL;

    if (user_title != NULL && user_icon_name != NULL) {
	return;
    }
    title = XtMalloc(10 + ((PCONNECTED || reconnect)?
		strlen(reconnect_host): 0));
    if (PCONNECTED || reconnect) {
	sprintf(title, "x3270-%d%s %s", model_num, (IN_NVT ? "A" : ""),
		reconnect_host);
	if (user_title == NULL) {
	    screen_set_title(title);
	}
	if (user_icon_name == NULL) {
	    XtVaSetValues(toplevel, XtNiconName, reconnect_host, NULL);
	}
	set_aicon_label(reconnect_host);
    } else {
	sprintf(title, "x3270-%d", model_num);
	sprintf(icon_label, "x3270-%d", model_num);
	if (user_title == NULL) {
	    screen_set_title(title);
	}
	if (user_icon_name == NULL) {
	    XtVaSetValues(toplevel, XtNiconName, icon_label, NULL);
	}
	set_aicon_label(icon_label);
    }
    XtFree(title);
}

static void
x3270_connect(bool ignored _is_unused)
{
    if (PCONNECTED) {
	macros_init();
    }
    relabel(true);
}

/* Respect the user's label/icon wishes and set up the label/icon callbacks. */
static void
label_init(void)
{
    user_title = get_resource(XtNtitle);
    user_icon_name = get_resource(XtNiconName);
    if (user_icon_name != NULL) {
	set_aicon_label(user_icon_name);
    }

    /* Set the _NET_WM_NAME property, because Xt doesn't. */
    if (user_title != NULL) {
	screen_set_title(user_title);
    }
}

/*
 * x3270 module registration.
 */
static void
x3270_register(void)
{
    register_schange(ST_CONNECT, x3270_connect);
    register_schange(ST_3270_MODE, relabel);
    register_schange(ST_REMODEL, relabel);
}

/*
 * Peek at X events before Xt does, calling PA_KeymapNotify_xaction if we see a
 * KeymapEvent.  This is to get around an (apparent) server bug that causes
 * Keymap events to come in with a window id of 0, so Xt never calls our
 * event handler.
 *
 * If the bug is ever fixed, this code will be redundant but harmless.
 */
static void
peek_at_xevent(XEvent *e)
{
    static Cardinal zero = 0;

    if (e->type == KeymapNotify) {
	ia_cause = IA_PEEK;
	PA_KeymapNotify_xaction(NULL, e, NULL, &zero);
	ia_cause = IA_DEFAULT;
    }
}


/*
 * Warning message trap, for catching colormap failures.
 */
static void
trap_colormaps(String name, String type, String class, String defaultp,
	String *params, Cardinal *num_params)
{
    if (!strcmp(type, "cvtStringToPixel")) {
	colormap_failure = true;
    }
    (*old_emh)(name, type, class, defaultp, params, num_params);
}

#if defined(LOCAL_PROCESS) /*[*/
/*
 * Pick out the -e option.
 */
static void
parse_local_process(int *argcp, char **argv, char **cmds)
{
    int i, j;
    int e_len = -1;

    for (i = 1; i < *argcp; i++) {
	if (strcmp(argv[i], OptLocalProcess)) {
	    continue;
	}

	/* Matched.  Copy 'em. */
	e_len = strlen(OptLocalProcess) + 1;
	for (j = i + 1; j < *argcp; j++) {
		e_len += 1 + strlen(argv[j]);
	}
	e_len++;
	*cmds = XtMalloc(e_len);
	strcpy(*cmds, OptLocalProcess);
	for (j = i + 1; j < *argcp; j++) {
	    strcat(strcat(*cmds, " "), argv[j]);
	}

	/* Stamp out the remaining args. */
	*argcp = i;
	argv[i] = NULL;
	break;
    }
}
#endif /*]*/

#if defined(DEBUG_SET_CLEAR) /*[*/
/* Dump the contents of argc/argv. */
static void
dump_argv(const char *when, int argc, char **argv)
{
    int i;

    printf("%s: ", when);
    for (i = 0; i < argc; i++) {
	printf(" '%s'", argv[i]);
    }
    printf("\n");
}
#endif /*]*/

/* Double backslashes in a value so it gets past -xrm. */
static char *
requote(const char *s)
{
    char *ret = Malloc((strlen(s) * 2) + 1);
    char *r = ret;
    char c;

    while ((c = *s++) != '\0') {
	if ((*r++ = c) == '\\') {
	    *r++ = c;
	}
    }
    *r = '\0';
    txdFree(ret);
    return ret;
}

/*
 * Pick out -set and -clear toggle options.
 */
static void
parse_set_clear(int *argcp, char **argv)
{
    int i, j;
    int argc_out = 0;
    char **argv_out = (char **) XtMalloc((*argcp + 1) * sizeof(char *));

    argv_out[argc_out++] = argv[0];

    for (i = 1; i < *argcp; i++) {
	bool is_set = false;
	bool found = false;
	const char *eq = NULL;
	size_t nlen;
	bool bool_only;

	if (!strcmp(argv[i], OptSet)) {
	    is_set = true;
	} else if (strcmp(argv[i], OptClear)) {
	    argv_out[argc_out++] = argv[i];
	    continue;
	}

	if (i == *argcp - 1) {	/* missing arg */
	    continue;
	}

	i++;

	if (argv[i][0] != '=' && (eq = strchr(argv[i], '=')) != NULL) {
	    /* -set foo=bar */
	    if (!is_set) {
		fprintf(stderr, "Error: " OptClear
			" parameter cannot include a value\n");
		exit(1);
	    }
	    nlen = eq - argv[i];
	} else {
	    nlen = strlen(argv[i]);
	}
	bool_only = !is_set || !eq;

	for (j = 0; toggle_names[j].name != NULL; j++) {
	    if (toggle_supported(toggle_names[j].index) &&
		    !strncasecmp(argv[i], toggle_names[j].name, nlen) &&
		    toggle_names[j].name[nlen] == '\0') {
		bool value;

		if (eq == NULL) {
		    value = is_set;
		} else {
		    const char *err = boolstr(eq + 1, &value);

		    if (err != NULL) {
			fprintf(stderr, "Error: " OptSet " %s: %s\n", argv[i],
				err);
			exit(1);
		    }
		}
		argv_out[argc_out++] = OptXrm;
		argv_out[argc_out++] = Asprintf("x3270.%s: %s",
			toggle_names[j].name, value? ResTrue: ResFalse);
		found = true;
		break;
	    }
	}
	if (!found) {
	    const char *proper_name = argv[i];
	    int xt = init_extended_toggle(argv[i], nlen, bool_only,
		    eq? eq + 1: (is_set? ResTrue: ResFalse), &proper_name);

	    if (xt == 0 && eq) {
		proper_name = txAsprintf("%.*s", (int)(eq - argv[i]), argv[i]);
	    }
	    argv_out[argc_out++] = OptXrm;
	    argv_out[argc_out++] = txAsprintf("x3270.%s: %s", proper_name,
			(eq != NULL)? requote(eq + 1):
			    (is_set? ResTrue: ResFalse));
	}
    }

    *argcp = argc_out;
    argv_out[argc_out] = NULL;
    memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
    Free(argv_out);
}

/*
 * Wrappers around X11 library functions that common code may use under a
 * non-X11 name.
 */
void
Error(const char *s)
{
    XtError(s);
}

void
Warning(const char *s)
{
    XtWarning(s);
}

/*
 * Product information functions.
 */
bool
product_has_display(void)
{
    return true;
}

/*
 * Copy xappres Boolean resources to appres bool resources.
 *
 * This is needed because we have to parse all resources with libXt calls.
 * LibXt uses 'Boolean', but the common resources have type 'bool'. We don't
 * know if libXt's 'Boolean' and <stdbool.h>'s 'bool' are the same type or not.
 */
static void
copy_xres_to_res_bool(void)
{
    int i;
#   define copy_bool(field)	appres.field = xappres.bools.field

    copy_bool(bind_limit);
    copy_bool(bind_unlock);
    copy_bool(bsd_tm);
    copy_bool(contention_resolution);
    copy_bool(debug_tracing);
    copy_bool(disconnect_clear);
    copy_bool(extended_data_stream);
    copy_bool(highlight_bold);
    copy_bool(idle_command_enabled);
    copy_bool(modified_sel);
    copy_bool(numeric_lock);
    copy_bool(new_environ);
    copy_bool(nvt_mode);
    copy_bool(oerr_lock);
    copy_bool(once);
    copy_bool(prefer_ipv4);
    copy_bool(prefer_ipv6);
    copy_bool(reconnect);
    copy_bool(retry);
    copy_bool(script_port_once);
    copy_bool(scripted);
    copy_bool(scripted_always);
    copy_bool(secure);
    copy_bool(socket);
    copy_bool(trace_monitor);
    copy_bool(unlock_delay);
    copy_bool(utf8);
    copy_bool(wrong_terminal_name);
    copy_bool(tls992);
    copy_bool(ut_env);

    copy_bool(interactive.do_confirms);
    copy_bool(interactive.mono);
    copy_bool(interactive.menubar);
    copy_bool(interactive.visual_bell);

    for (i = 0; i < N_TOGGLES; i++) {
	copy_bool(toggle[i]);
    }

    copy_bool(linemode.icrnl);
    copy_bool(linemode.inlcr);
    copy_bool(linemode.onlcr);

    copy_bool(tls.starttls);
    copy_bool(tls.verify_host_cert);
}

/* Child exit callbacks. */

/** Child exit state */
typedef struct child_exit {
    struct child_exit *next;	/**< Linkage. */
    pid_t pid;			/**< Child process ID. */
    childfn_t proc;		/**< Function to call on exit.  */
} child_exit_t;
static child_exit_t *child_exits = NULL;

/**
 * Add a function to be called when a child exits.
 * @param[in] pid	Child process ID.
 * @param[in] fn	Function to call on exit.
 * @return @ref ioid_t to identify this callback
 */
ioid_t
AddChild(pid_t pid, childfn_t fn)
{
    child_exit_t *cx;

    assert(pid != 0 && pid != -1);

    cx = (child_exit_t *)Malloc(sizeof(child_exit_t));
    cx->pid = pid;
    cx->proc = fn;
    cx->next = child_exits;
    child_exits = cx;
    return (ioid_t)cx;
}


/**
 * Poll for exited children and call registered callbacks.
 */
static void
poll_children(void)
{
    pid_t pid;
    int status = 0;
    child_exit_t *c;
    child_exit_t *next = NULL;
    child_exit_t *prev = NULL;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	for (c = child_exits; c != NULL; c = next) {
	    next = c->next;
	    if (c->pid == pid) {
		(*c->proc)((ioid_t)c, status);
		if (prev) {
		    prev->next = next;
		} else {
		    child_exits = next;
		}
		Free(c);
	    } else {
		prev = c;
	    }
	}
    }
}

/* Glue for redundant functions normally supplied by glue.c. */

typedef struct dummy_reg {
   struct dummy_reg *next;
   void *value;
} dummy_reg_t;
static dummy_reg_t *dummy_reg;

/* Put a little piece of allocated memory somewhere Valgrind can find it. */
static void
valkeep(void *p)
{
    dummy_reg_t *d = Malloc(sizeof(dummy_reg_t));

    d->next = dummy_reg;
    d->value = p;
    dummy_reg = d;
}

void
register_opts(opt_t *opts, unsigned num_opts)
{
    valkeep((void *)opts);
}

void
register_resources(res_t *res, unsigned num_res)
{
    valkeep((void *)res);
}

void
register_xresources(xres_t *res, unsigned num_xres)
{
    valkeep((void *)res);
}
