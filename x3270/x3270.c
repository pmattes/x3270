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
 *	x3270.c
 *		A 3270 Terminal Emulator for X11
 *		Main proceudre.
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
#include "charset.h"
#include "ctlrc.h"
#include "ft.h"
#include "host.h"
#include "httpd-core.h"
#include "httpd-nodes.h"
#include "httpd-io.h"
#include "idle.h"
#include "keymap.h"
#include "kybd.h"
#include "macros.h"
#include "nvt.h"
#include "popups.h"
#include "pr3287_session.h"
#include "print_screen.h"
#include "print_window.h"
#include "product.h"
#include "resourcesc.h"
#include "screen.h"
#include "selectc.h"
#include "status.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "utils.h"
#include "xactions.h"
#include "xappres.h"
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
		a_net_wm_state_maximized_vert, a_atom;
char		full_model_name[13] = "IBM-";
char	       *model_name = &full_model_name[4];
Pixmap          gray;
XrmDatabase     rdb;
AppRes		appres;
xappres_t	xappres;
int		children = 0;
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
static int	parse_model_number(char *m);
static void	parse_set_clear(int *, char **);
static void	label_init(void);
static void	sigchld_handler(int);
static char    *user_icon_name = NULL;
static void	copy_xres_to_res_bool(void);

XrmOptionDescRec options[]= {
    { OptActiveIcon,	DotActiveIcon,	XrmoptionNoArg,		ResTrue },
    { OptAplMode,	DotAplMode,	XrmoptionNoArg,		ResTrue },
#if defined(HAVE_LIBSSL) /*[*/
    { OptAcceptHostname,DotAcceptHostname,XrmoptionSepArg,	NULL },
    { OptCaDir,		DotCaDir,	XrmoptionSepArg,	NULL },
    { OptCaFile,	DotCaFile,	XrmoptionSepArg,	NULL },
    { OptCertFile,	DotCertFile,	XrmoptionSepArg,	NULL },
    { OptCertFileType,	DotCertFileType,XrmoptionSepArg,	NULL },
    { OptChainFile,	DotChainFile,	XrmoptionSepArg,	NULL },
#endif /*]*/
    { OptCharClass,	DotCharClass,	XrmoptionSepArg,	NULL },
    { OptCharset,	DotCharset,	XrmoptionSepArg,	NULL },
    { OptClear,		".xxx",		XrmoptionSkipArg,	NULL },
    { OptColorScheme,	DotColorScheme,	XrmoptionSepArg,	NULL },
    { OptConnectTimeout,DotConnectTimeout,XrmoptionSepArg,	NULL },
    { OptDevName,	DotDevName,	XrmoptionSepArg,	NULL },
    { OptTrace,		DotTrace,	XrmoptionNoArg,		ResTrue },
    { OptEmulatorFont,	DotEmulatorFont,XrmoptionSepArg,	NULL },
    { OptExtended,	DotExtended,	XrmoptionNoArg,		ResTrue },
    { OptHostsFile,	DotHostsFile,	XrmoptionSepArg,	NULL },
    { OptHttpd,		DotHttpd,	XrmoptionSepArg,	NULL },
    { OptIconName,	".iconName",	XrmoptionSepArg,	NULL },
    { OptIconX,		".iconX",	XrmoptionSepArg,	NULL },
    { OptIconY,		".iconY",	XrmoptionSepArg,	NULL },
#if defined(HAVE_LIBSSL) /*[*/
    { OptKeyFile,	DotKeyFile,	XrmoptionSepArg,	NULL },
    { OptKeyFileType,	DotKeyFileType,	XrmoptionSepArg,	NULL },
#endif /*]*/
    { OptKeymap,	DotKeymap,	XrmoptionSepArg,	NULL },
    { OptKeypadOn,	DotKeypadOn,	XrmoptionNoArg,		ResTrue },
#if defined(HAVE_LIBSSL) /*[*/
    { OptKeyPasswd,	DotKeyPasswd,	XrmoptionSepArg,	NULL },
#endif /*]*/
    { OptLoginMacro,	DotLoginMacro,	XrmoptionSepArg,	NULL },
    { OptM3279,		DotM3279,	XrmoptionNoArg,		ResTrue },
    { OptModel,		DotModel,	XrmoptionSepArg,	NULL },
    { OptMono,		DotMono,	XrmoptionNoArg,		ResTrue },
    { OptNoScrollBar,	DotScrollBar,	XrmoptionNoArg,		ResFalse },
    { OptNvtMode,	DotNvtMode,	XrmoptionNoArg,		ResTrue },
    { OptOnce,		DotOnce,	XrmoptionNoArg,		ResTrue },
    { OptOversize,	DotOversize,	XrmoptionSepArg,	NULL },
    { OptPort,		DotPort,	XrmoptionSepArg,	NULL },
    { OptPrinterLu,	DotPrinterLu,	XrmoptionSepArg,	NULL },
    { OptProxy,		DotProxy,	XrmoptionSepArg,	NULL },
    { OptReconnect,	DotReconnect,	XrmoptionNoArg,		ResTrue },
    { OptSaveLines,	DotSaveLines,	XrmoptionSepArg,	NULL },
    { OptScripted,	DotScripted,	XrmoptionNoArg,		ResTrue },
    { OptScrollBar,	DotScrollBar,	XrmoptionNoArg,		ResTrue },
    { OptSecure,	DotSecure,	XrmoptionNoArg,		ResTrue },
#if defined(HAVE_LIBSSL) /*[*/
    { OptSelfSignedOk,DotSelfSignedOk,	XrmoptionNoArg,		ResTrue },
#endif /*]*/
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
    { OptV,		DotV,		XrmoptionNoArg,		ResTrue },
#if defined(HAVE_LIBSSL) /*[*/
    { OptVerifyHostCert,DotVerifyHostCert,XrmoptionNoArg,	ResTrue },
#endif /*]*/
    { OptVersion,	DotV,		XrmoptionNoArg,		ResTrue },
    { "-xrm",		NULL,		XrmoptionResArg,	NULL }
};
int num_options = XtNumber(options);

static struct {
    char *opt;
    char *args;
    char *help;
} option_help[] = {
#if defined(HAVE_LIBSSL) /*[*/
    { OptAcceptHostname, "any|DNS:<name>|IP:<addr>",
	"Host name to accept from server certificate" },
#endif /*]*/
    { OptActiveIcon, NULL, "Make icon a miniature of the display" },
    { OptAplMode, NULL,    "Turn on APL mode" },
#if defined(HAVE_LIBSSL) /*[*/
    { OptCaDir, "<directory>", "OpenSSL CA certificate database directory" },
    { OptCaFile, "<filename>", "OpenSSL CA certificate file" },
    { OptCertFile, "<file>", "OpenSSL certificate file" },
    { OptCertFileType, "pem|asn1", "OpenSSL certificate file type" },
    { OptChainFile, "<filename>", "OpenSSL certificate chain file" },
#endif /*]*/
    { OptCharClass, "<spec>", "Define characters for word boundaries" },
    { OptCharset, "<name>",
	"Use host EBCDIC character set (code page) <name>" },
    { OptClear, "<toggle>", "Turn on <toggle>" },
    { OptColorScheme, "<name>", "Use color scheme <name>" },
    { OptConnectTimeout, "<seconds>", "Timeout for host connect requests" },
    { OptDevName, "<name>", "Device name (workstation ID)" },
    { OptEmulatorFont, "<font>", "Font for emulator window" },
    { OptExtended, NULL, "Extended 3270 data stream (deprecated)" },
    { OptHttpd, "[<addr>:]<port>", "TCP port to listen on for http requests" },
    { OptHostsFile, "<filename>", "Pathname of ibm_hosts file" },
    { OptIconName, "<name>", "Title for icon" },
    { OptIconX, "<x>", "X position for icon" },
    { OptIconY, "<y>", "Y position for icon" },
#if defined(HAVE_LIBSSL) /*[*/
    { OptKeyFile, "<filename>", "Get OpenSSL private key from <filename>" },
    { OptKeyFileType, "pem|asn1", "OpenSSL private key file type" },
#endif /*]*/
    { OptKeymap, "<name>[,<name>...]", "Keyboard map name(s)" },
    { OptKeypadOn, NULL, "Turn on pop-up keypad at start-up" },
#if defined(HAVE_LIBSSL) /*[*/
    { OptKeyPasswd, "file:<filename>|string:<text>",
	"OpenSSL private key password" },
#endif /*]*/
    { OptLoginMacro, "Action([arg[,...]]) [...]", "Macro to run at login" },
    { OptM3279, NULL, "3279 emulation (deprecated)" },
    { OptModel, "[327{8,9}-]<n>", "Emulate a 3278 or 3279 model <n>" },
    { OptMono, NULL, "Do not use color" },
    { OptNoScrollBar, NULL, "Disable scroll bar" },
    { OptNvtMode, NULL, "Begin in NVT mode" },
    { OptOnce, NULL, "Exit as soon as the host disconnects" },
    { OptOversize,  "<cols>x<rows>", "Larger screen dimensions" },
    { OptPort, "<port>", "Default TELNET port" },
    { OptPrinterLu,  "<luname>",
	"Automatically start a pr3287 printer session to <luname>" },
    { OptProxy, "<type>:<host>[:<port>]", "Secify proxy type and server" },
    { OptReconnect, NULL, "Reconnect to host as soon as it disconnects" },
    { OptSaveLines, "<n>", "Number of lines to save for scroll bar" },
    { OptScripted, NULL, "Accept commands on standard input" },
    { OptScrollBar, NULL, "Turn on scroll bar" },
#if defined(HAVE_LIBSSL) /*[*/
    { OptSelfSignedOk, NULL, "Allow self-signed host certificates" },
#endif /*]*/
    { OptSet, "<toggle>", "Turn on <toggle>" },
    { OptSocket,  NULL, "Create socket for script control" },
    { OptScriptPort, "<port>",
	"Listen on TCP port <port> for script connections" },
    { OptScriptPortOnce, NULL, "Accept one script connection, then exit" },
    { OptSecure, NULL, "Set secure mode" },
    { OptTermName, "<name>", "Send <name> as TELNET terminal name" },
    { OptTrace, NULL, "Enable tracing" },
    { OptTraceFile, "<file>", "Write traces to <file>" },
    { OptTraceFileSize, "<n>[KM]", "Limit trace file to <n> bytes" },
    { OptInputMethod, "<name>", "Multi-byte input method" },
    { OptPreeditType, "<style>", "Define input method pre-edit type" },
    { OptUser, "<name>", "User name for RFC 4777" },
    { OptV, NULL, "Display build options and character sets" },
#if defined(HAVE_LIBSSL) /*[*/
    { OptVerifyHostCert, NULL, "Verify OpenSSL host certificate" },
#endif /*]*/
    { OptVersion, NULL, "Display build options and character sets" },
    { "-xrm", "'x3270.<resource>: <value>'", "Set <resource> to <vale>" }
};

/* Fallback resources. */
static String fallbacks[] = {
    /* This should be overridden by real app-defaults. */
    "*adVersion: fallback",
    NULL
};

static void x3270_register(void);


void
usage(const char *msg)
{
    unsigned i;

    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }

    fprintf(stderr, "Usage: %s [options] [[ps:][LUname@]hostname[:port]]\n",
	    programname);
    fprintf(stderr, "Options:\n");
    for (i = 0; i < XtNumber(option_help); i++) {
	fprintf(stderr, " %s%s%s\n   %s\n",
		option_help[i].opt,
		option_help[i].args? " ": "",
		option_help[i].args? option_help[i].args: "",
		option_help[i].help);
    }
    fprintf(stderr,
	    " Plus standard Xt options like '-title' and '-geometry'\n");
    exit(1);
}

static void
no_minus(char *arg)
{
    if (arg[0] == '-') {
	usage(xs_buffer("Unknown or incomplete option: %s", arg));
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
    int	ovc, ovr;
    char junk;
    int	model_number;
    bool mono = false;
    char *session = NULL;
    bool pending = false;

    if (XtNumber(options) != XtNumber(option_help)) {
	Error("Help mismatch");
    }

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
	++programname;
    } else {
	programname = argv[0];
    }

    /* Parse a lone "-v" first, without contacting a server. */
    if (argc == 2 && (!strcmp(argv[1], OptV) ||
		      !strcmp(argv[1], OptVersion))) {
	dump_version();
    }

    /*
     * Call the module registration functions, to build up the tables of
     * actions, options and callbacks. These functions have no
     * interdependencies and cannot depend on resource values.
     */
    ctlr_register();
    ft_register();
    host_register();
    idle_register();
    keymap_register();
    kybd_register();
    macros_register();
    menubar_register();
    nvt_register();
    popups_register();
    pr3287_session_register();
    print_screen_register();
    print_window_register();
    screen_register();
    scroll_register();
    select_register();
    status_register();
    toggles_register();
    trace_register();
    x3270_register();
    xio_register();
    xkybd_register();

    /* Translate and validate -set and -clear toggle options. */
    parse_set_clear(&argc, argv);

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

    /* Initialize. */
    toplevel = XtVaAppInitialize(
	    &appcontext,
#if defined(USE_APP_DEFAULTS) /*[*/
	    "X3270",
#else /*][*/
	    "X3270xad",	/* explicitly _not_ X3270 */
#endif /*]*/
	    options, num_options,
	    &argc, argv,
	    fallbacks,
	    XtNinput, True,
	    XtNallowShellResize, False,
	    NULL);
    display = XtDisplay(toplevel);
    rdb = XtDatabase(display);

    if (get_resource(ResV)) {
	dump_version();
    }

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
	cl_hostname = xs_buffer("%s:%s", argv[1], argv[2]);
	break;
    default:
	usage(NULL);
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

    /* Fill in appres. */
    old_emh = XtAppSetWarningMsgHandler(appcontext,
	    (XtErrorMsgHandler)trap_colormaps);
    XtGetApplicationResources(toplevel, (XtPointer)&appres, resources,
	    num_resources, 0, 0);
    XtGetApplicationResources(toplevel, (XtPointer)&xappres, xresources,
	    num_xresources, 0, 0);
    copy_xres_to_res_bool();
    (void) XtAppSetWarningMsgHandler(appcontext, old_emh);

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
    if (screen_depth <= 1 || colormap_failure) {
	appres.interactive.mono = true;
    }
    if (appres.interactive.mono) {
	xappres.use_cursor_color = False;
	appres.m3279 = false;
    }
    if (!appres.extended) {
	appres.oversize = NULL;
    }
    if (appres.secure) {
	appres.disconnect_clear = true;
    }

    a_delete_me = XInternAtom(display, "WM_DELETE_WINDOW", False);
    a_save_yourself = XInternAtom(display, "WM_SAVE_YOURSELF", False);
    a_3270 = XInternAtom(display, "3270", False);
    a_registry = XInternAtom(display, "CHARSET_REGISTRY", False);
    a_encoding = XInternAtom(display, "CHARSET_ENCODING", False);
    a_state = XInternAtom(display, "WM_STATE", False);
    a_net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    a_net_wm_state_maximized_horz = XInternAtom(display,
	    "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    a_net_wm_state_maximized_vert = XInternAtom(display,
	    "_NET_WM_STATE_MAXIMIZED_VERT", False);
    a_atom = XInternAtom(display, "ATOM", False);

    /* Add the Xt-only actions. */
    xaction_init();

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
    printer_popup_init();
    ft_init();

    /* Add the wrapped actions. */
    xaction_init2();

    /* Define the keymap. */
    keymap_init(appres.interactive.key_map, false);

    if (appres.apl_mode) {
	appres.interactive.compose_map = XtNewString(Apl);
	appres.charset = XtNewString(Apl);
    }

    screen_preinit();

    switch (charset_init(appres.charset)) {
    case CS_OKAY:
	break;
    case CS_NOTFOUND:
	popup_an_error("Cannot find definition for host character set \"%s\"",
		appres.charset);
	(void) charset_init(NULL);
	break;
    case CS_BAD:
	popup_an_error("Invalid definition for host character set \"%s\"",
		appres.charset);
	(void) charset_init(NULL);
	break;
    case CS_PREREQ:
	popup_an_error("No fonts for host character set \"%s\"",
		appres.charset);
	(void) charset_init(NULL);
	break;
    case CS_ILLEGAL:
	(void) charset_init(NULL);
	break;
    }

    /* Initialize fonts. */
    font_init();

#if defined(RESTRICT_3279) /*[*/
    if (appres.m3279 && model_number == 4) {
	model_number = 3;
    }
#endif /*]*/
    if (!appres.extended || appres.oversize == NULL ||
	sscanf(appres.oversize, "%dx%d%c", &ovc, &ovr, &junk) != 2) {
	ovc = 0;
	ovr = 0;
    }
    set_rows_cols(model_number, ovc, ovr);
    net_set_default_termtype();

    hostfile_init();

    /* Initialize the icon. */
    icon_init();

    /*
     * If no hostname is specified on the command line, ignore certain
     * options.
     */
    if (argc <= 1) {
#if defined(LOCAL_PROCESS) /*[*/
	if (cl_hostname == NULL)
#endif /*]*/
	{
	    appres.once = false;
	}
	appres.interactive.reconnect = false;
    }

    if (xappres.char_class != NULL) {
	reclass(xappres.char_class);
    }

    screen_init();
    info_popup_init();
    error_popup_init();

    protocols[0] = a_delete_me;
    protocols[1] = a_save_yourself;
    XSetWMProtocols(display, XtWindow(toplevel), protocols, 2);

    /* Save the command line. */
    save_init(argc, argv[1], argv[2]);

    /* Make sure we don't fall over any SIGPIPEs. */
    (void) signal(SIGPIPE, SIG_IGN);

    /*
     * Make sure that exited child processes become zombies, so we can
     * collect their exit status.
     */
    (void) signal(SIGCHLD, sigchld_handler);

    /* Set up the window and icon labels. */
    label_init();

    /* Handle initial toggle settings. */
    if (appres.dsTrace_bc || appres.eventTrace_bc) {
	/* Backwards compatibility with old resource names. */
	set_toggle_initial(TRACING, true);
    }
    if (!appres.debug_tracing) {
	set_toggle_initial(TRACING, false);
    }
    initialize_toggles();

#if defined(HAVE_LIBSSL) /*[*/
    ssl_base_init(cl_hostname, &pending);
#endif /*]*/

    /* Connect to the host. */
    if (cl_hostname != NULL && !pending)
	    (void) host_connect(cl_hostname);

    /* Prepare to run a peer script. */
    peer_script_init();

    /* Process X events forever. */
    while (1) {
	XEvent event;
	pid_t pid;
	int status;

	while (XtAppPending(appcontext) & (XtIMXEvent | XtIMTimer)) {
	    if (XtAppPeekEvent(appcontext, &event)) {
		peek_at_xevent(&event);
	    }
	    XtAppProcessEvent(appcontext, XtIMXEvent | XtIMTimer);
	}
	screen_disp(false);
	XtAppProcessEvent(appcontext, XtIMAll);

	if (children && (pid = waitpid(-1, &status, WNOHANG)) > 0) {
	    pr3287_session_check(pid, status);
	    --children;
	}
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
    (void) signal(SIGCHLD, sigchld_handler);
#endif /*]*/
}

/*
 * Parse the model number.
 * Returns -1 (error), 0 (default), or the specified number.
 */
static int
parse_model_number(char *m)
{
    int sl;
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

/* Change the window and icon labels. */
static void
relabel(bool ignored _is_unused)
{
    char *title;
    char icon_label[8];

    if (user_title != NULL && user_icon_name != NULL) {
	return;
    }
    title = XtMalloc(10 + ((PCONNECTED || appres.interactive.reconnect)?
					    strlen(reconnect_host): 0));
    if (PCONNECTED || appres.interactive.reconnect) {
	(void) sprintf(title, "x3270-%d%s %s", model_num, (IN_NVT ? "A" : ""),
		reconnect_host);
	if (user_title == NULL) {
	    XtVaSetValues(toplevel, XtNtitle, title, NULL);
	}
	if (user_icon_name == NULL) {
	    XtVaSetValues(toplevel, XtNiconName, reconnect_host, NULL);
	}
	set_aicon_label(reconnect_host);
    } else {
	(void) sprintf(title, "x3270-%d", model_num);
	(void) sprintf(icon_label, "x3270-%d", model_num);
	if (user_title == NULL) {
	    XtVaSetValues(toplevel, XtNtitle, title, NULL);
	}
	if (user_icon_name == NULL) {
	    XtVaSetValues(toplevel, XtNiconName, icon_label, NULL);
	}
	set_aicon_label(icon_label);
    }
    XtFree(title);
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
}

/*
 * x3270 module registration.
 */
static void
x3270_register(void)
{
    register_schange(ST_HALF_CONNECT, relabel);
    register_schange(ST_CONNECT, relabel);
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
	(void) strcpy(*cmds, OptLocalProcess);
	for (j = i + 1; j < *argcp; j++) {
	    (void) strcat(strcat(*cmds, " "), argv[j]);
	}

	/* Stamp out the remaining args. */
	*argcp = i;
	argv[i] = NULL;
	break;
    }
}
#endif /*]*/

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
parse_set_clear(int *argcp, char **argv)
{
    int i, j;
    int argc_out = 0;
    char **argv_out = (char **) XtMalloc((*argcp + 1) * sizeof(char *));

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

	/* Match the name. */
	i++;
	for (j = 0; toggle_names[j].name != NULL; j++)
	    if (!strcasecmp(argv[i], toggle_names[j].name)) {
		appres.toggle[toggle_names[j].index] = is_set;
		break;
	    }
	if (toggle_names[j].name == NULL) {
	    const char **tn;
	    int ntn = 0;

	    tn = (const char **)Calloc(N_TOGGLES, sizeof(char **));
	    for (j = 0; toggle_names[j].name != NULL; j++) {
		if (!toggle_names[j].is_alias) {
		    tn[ntn++] = toggle_names[j].name;
		}
	    }
	    qsort(tn, ntn, sizeof(const char *), name_cmp);
	    fprintf(stderr, "Unknown toggle name '%s'. Toggle names are:\n",
		    argv[i]);
	    for (j = 0; j < ntn; j++) {
		fprintf(stderr, " %s", tn[j]);
	    }
	    fprintf(stderr, "\n");
	    Free(tn);
	    exit(1);
	}

	/* Substitute. */
	argv_out[argc_out++] = "-xrm";
	argv_out[argc_out++] = xs_buffer("x3270.%s: %s",
		toggle_names[j].name, is_set? ResTrue: ResFalse);
    }

    *argcp = argc_out;
    argv_out[argc_out] = NULL;
    (void) memcpy((char *)argv, (char *)argv_out,
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

    copy_bool(extended);
    copy_bool(m3279);
    copy_bool(apl_mode);
    copy_bool(once);
    copy_bool(scripted);
    copy_bool(modified_sel);
    copy_bool(unlock_delay);
    copy_bool(bind_limit);
    copy_bool(bind_unlock);
    copy_bool(new_environ);
    copy_bool(socket);
    copy_bool(numeric_lock);
    copy_bool(secure);
    copy_bool(oerr_lock);
    copy_bool(typeahead);
    copy_bool(debug_tracing);
    copy_bool(disconnect_clear);
    copy_bool(highlight_bold);
    copy_bool(color8);
    copy_bool(bsd_tm);
    copy_bool(trace_monitor);
    copy_bool(idle_command_enabled);
    copy_bool(nvt_mode);
    copy_bool(dsTrace_bc);
    copy_bool(eventTrace_bc);
    copy_bool(script_port_once);

    copy_bool(interactive.mono);
    copy_bool(interactive.menubar);
    copy_bool(interactive.visual_bell);
    copy_bool(interactive.reconnect);
    copy_bool(interactive.do_confirms);

    for (i = 0; i < N_TOGGLES; i++) {
	copy_bool(toggle[i]);
    }

    copy_bool(linemode.icrnl);
    copy_bool(linemode.inlcr);
    copy_bool(linemode.onlcr);

    copy_bool(ssl.self_signed_ok);
    copy_bool(ssl.tls);
    copy_bool(ssl.verify_host_cert);
}
