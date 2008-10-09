/*
 * Modifications Copyright 1993-2008 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	main.c
 *		A 3270 Terminal Emulator for X11
 *		Main proceudre.
 */

#include "globals.h"
#include <sys/wait.h>
#include <X11/StringDefs.h>
#include <X11/Core.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "ftc.h"
#include "hostc.h"
#include "idlec.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "printerc.h"
#include "resourcesc.h"
#include "savec.h"
#include "screenc.h"
#include "selectc.h"
#include "statusc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "utilc.h"

/* Externals */
#if defined(USE_APP_DEFAULTS) /*[*/
extern const char *app_defaults_version;
#endif /*]*/

/* Globals */
const char     *programname;
Display        *display;
int             default_screen;
Window          root_window;
int             screen_depth;
Widget          toplevel;
XtAppContext    appcontext;
Atom            a_delete_me, a_save_yourself, a_3270, a_registry, a_encoding,
		a_state;
char		full_model_name[13] = "IBM-";
char	       *model_name = &full_model_name[4];
Pixmap          gray;
XrmDatabase     rdb;
AppRes          appres;
int		children = 0;
Boolean		exiting = False;
char           *user_title = CN;

/* Statics */
static void	peek_at_xevent(XEvent *);
static XtErrorMsgHandler old_emh;
static void	trap_colormaps(String, String, String, String, String *,
			Cardinal *);
static Boolean  colormap_failure = False;
#if defined(LOCAL_PROCESS) /*[*/
static void	parse_local_process(int *argcp, char **argv, char **cmds);
#endif /*]*/
static int	parse_model_number(char *m);
static void	parse_set_clear(int *, char **);
static void	label_init(void);
static void	sigchld_handler(int);
static char    *user_icon_name = CN;

XrmOptionDescRec options[]= {
	{ OptActiveIcon,DotActiveIcon,	XrmoptionNoArg,		ResTrue },
	{ OptAplMode,	DotAplMode,	XrmoptionNoArg,		ResTrue },
#if defined(HAVE_LIBSSL) /*[*/
	{ OptCertFile,	DotCertFile,	XrmoptionSepArg,	NULL },
#endif /*]*/
	{ OptCharClass,	DotCharClass,	XrmoptionSepArg,	NULL },
	{ OptCharset,	DotCharset,	XrmoptionSepArg,	NULL },
	{ OptClear,	".xxx",		XrmoptionSkipArg,	NULL },
	{ OptColorScheme,DotColorScheme,XrmoptionSepArg,	NULL },
#if defined(X3270_TRACE) /*[*/
	{ OptDsTrace,	DotDsTrace,	XrmoptionNoArg,		ResTrue },
#endif /*]*/
	{ OptEmulatorFont,DotEmulatorFont,XrmoptionSepArg,	NULL },
	{ OptExtended,	DotExtended,	XrmoptionNoArg,		ResTrue },
	{ OptIconName,	".iconName",	XrmoptionSepArg,	NULL },
	{ OptIconX,	".iconX",	XrmoptionSepArg,	NULL },
	{ OptIconY,	".iconY",	XrmoptionSepArg,	NULL },
	{ OptKeymap,	DotKeymap,	XrmoptionSepArg,	NULL },
	{ OptKeypadOn,	DotKeypadOn,	XrmoptionNoArg,		ResTrue },
	{ OptM3279,	DotM3279,	XrmoptionNoArg,		ResTrue },
	{ OptModel,	DotModel,	XrmoptionSepArg,	NULL },
	{ OptMono,	DotMono,	XrmoptionNoArg,		ResTrue },
	{ OptNoScrollBar,DotScrollBar,	XrmoptionNoArg,		ResFalse },
	{ OptOnce,	DotOnce,	XrmoptionNoArg,		ResTrue },
	{ OptOversize,	DotOversize,	XrmoptionSepArg,	NULL },
	{ OptPort,	DotPort,	XrmoptionSepArg,	NULL },
#if defined(X3270_PRINTER) /*[*/
	{ OptPrinterLu,	DotPrinterLu,	XrmoptionSepArg,	NULL },
#endif /*]*/
	{ OptProxy,	DotProxy,	XrmoptionSepArg,	NULL },
	{ OptReconnect,	DotReconnect,	XrmoptionNoArg,		ResTrue },
	{ OptSaveLines,	DotSaveLines,	XrmoptionSepArg,	NULL },
	{ OptScripted,	DotScripted,	XrmoptionNoArg,		ResTrue },
	{ OptScrollBar,	DotScrollBar,	XrmoptionNoArg,		ResTrue },
	{ OptSet,	".xxx",		XrmoptionSkipArg,	NULL },
#if defined(X3270_SCRIPT) /*[*/
	{ OptSocket,	DotSocket,	XrmoptionNoArg,		ResTrue },
#endif /*]*/
	{ OptTermName,	DotTermName,	XrmoptionSepArg,	NULL },
#if defined(X3270_TRACE) /*[*/
	{ OptTraceFile,	DotTraceFile,	XrmoptionSepArg,	NULL },
	{ OptTraceFileSize,DotTraceFileSize,XrmoptionSepArg,	NULL },
#endif /*]*/
#if defined(X3270_DBCS) /*[*/
	{ OptInputMethod,DotInputMethod,XrmoptionSepArg,	NULL },
	{ OptPreeditType,DotPreeditType,XrmoptionSepArg,	NULL },
#endif /*]*/
	{ OptV,		DotV,		XrmoptionNoArg,		ResTrue },
	{ OptVersion,	DotV,		XrmoptionNoArg,		ResTrue },
	{ "-xrm",	NULL,		XrmoptionResArg,	NULL }
};
int num_options = XtNumber(options);

/* Fallback resources. */
static String fallbacks[] = {
	/* This should be overridden by real app-defaults. */
	"*adVersion: fallback",
	NULL
};

struct toggle_name toggle_names[N_TOGGLES] = {
	{ ResMonoCase,        MONOCASE },
	{ ResAltCursor,       ALT_CURSOR },
	{ ResCursorBlink,     CURSOR_BLINK },
	{ ResShowTiming,      SHOW_TIMING },
	{ ResCursorPos,       CURSOR_POS },
#if defined(X3270_TRACE) /*[*/
	{ ResDsTrace,         DS_TRACE },
#else /*][*/
	{ ResDsTrace,         -1 },
#endif /*]*/
	{ ResScrollBar,       SCROLL_BAR },
#if defined(X3270_ANSI) /*[*/
	{ ResLineWrap,        LINE_WRAP },
#else /*][*/
	{ ResLineWrap,        -1 },
#endif /*]*/
	{ ResBlankFill,       BLANK_FILL },
#if defined(X3270_TRACE) /*[*/
	{ ResScreenTrace,     SCREEN_TRACE },
	{ ResEventTrace,      EVENT_TRACE },
#else /*][*/
	{ ResScreenTrace,     -1 },
	{ ResEventTrace,      -1 },
#endif /*]*/
	{ ResMarginedPaste,   MARGINED_PASTE },
	{ ResRectangleSelect, RECTANGLE_SELECT },
	{ ResCrosshair,	      CROSSHAIR },
	{ ResVisibleControl,  VISIBLE_CONTROL },
#if defined(X3270_SCRIPT) /*[*/
	{ ResAidWait,         AID_WAIT },
#else /*][*/
	{ ResAidWait,         -1 },
#endif /*]*/
};


static void
usage(const char *msg)
{
	if (msg != CN)
		XtWarning(msg);
#if defined(X3270_MENUS) /*[*/
	xs_error("Usage: %s [options] [[ps:][LUname@]hostname[:port]]", programname);
#else /*][*/
	xs_error("Usage: %s [options] [ps:][LUname@]hostname[:port]", programname);
#endif /*]*/
}

static void
no_minus(char *arg)
{
	if (arg[0] == '-')
	    usage(xs_buffer("Unknown or incomplete option: %s", arg));
}

int
main(int argc, char *argv[])
{
#if !defined(USE_APP_DEFAULTS) /*[*/
	char	*dname;
	int	i;
#endif /*]*/
	Atom	protocols[2];
	char	*cl_hostname = CN;
	int	ovc, ovr;
	char	junk;
	int	model_number;
	Boolean	mono = False;

	/* Figure out who we are */
	programname = strrchr(argv[0], '/');
	if (programname)
		++programname;
	else
		programname = argv[0];

	/* Parse a lone "-v" first, without contacting a server. */
	if (argc == 2 && (!strcmp(argv[1], OptV) ||
		          !strcmp(argv[1], OptVersion))) {
	    	dump_version();
	}

	/* Save a copy of the command-line args for merging later. */
	save_args(argc, argv);

#if !defined(USE_APP_DEFAULTS) /*[*/
	/*
	 * Figure out which fallbacks to use, based on the "-mono"
	 * switch on the command line, and the depth of the display.
	 */
	dname = CN;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-mono"))
			mono = True;
		else if (!strcmp(argv[i], "-display") && argc > i)
			dname = argv[i+1];
	}
	display = XOpenDisplay(dname);
	if (display == (Display *)NULL)
		XtError("Can't open display");
	if (DefaultDepthOfScreen(XDefaultScreenOfDisplay(display)) == 1)
		mono = True;
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

	if (get_resource(ResV))
	    	dump_version();

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

	/* Merge in the profile. */
	merge_profile(&rdb, mono);

	old_emh = XtAppSetWarningMsgHandler(appcontext,
	    (XtErrorMsgHandler)trap_colormaps);
	XtGetApplicationResources(toplevel, (XtPointer)&appres, resources,
	    num_resources, 0, 0);
	(void) XtAppSetWarningMsgHandler(appcontext, old_emh);

#if defined(USE_APP_DEFAULTS) /*[*/
	/* Check the app-defaults version. */
	if (!appres.ad_version)
		XtError("Outdated app-defaults file");
	else if (!strcmp(appres.ad_version, "fallback"))
		XtError("No app-defaults file");
	else if (strcmp(appres.ad_version, app_defaults_version))
		xs_error("app-defaults version mismatch: want %s, got %s",
		    app_defaults_version, appres.ad_version);
#endif /*]*/

#if defined(LOCAL_PROCESS) /*[*/
	/* Pick out the -e option. */
	parse_local_process(&argc, argv, &cl_hostname);
#endif /*]*/

	/* Pick out -set and -clear toggle options. */
	parse_set_clear(&argc, argv);

	/* Verify command-line syntax. */
	switch (argc) {
	    case 1:
#if !defined(X3270_MENUS) /*[*/
		if (cl_hostname == CN)
			usage(CN);
#endif /*]*/
		break;
	    case 2:
		if (cl_hostname != CN)
			usage(CN);
		no_minus(argv[1]);
		cl_hostname = argv[1];
		break;
	    case 3:
		if (cl_hostname != CN)
			usage(CN);
		no_minus(argv[1]);
		no_minus(argv[2]);
		cl_hostname = xs_buffer("%s:%s", argv[1], argv[2]);
		break;
	    default:
		usage(CN);
		break;
	}

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
	if (screen_depth <= 1 || colormap_failure)
		appres.mono = True;
	if (appres.mono) {
		appres.use_cursor_color = False;
		appres.m3279 = False;
	}
	if (!appres.extended)
		appres.oversize = CN;
	if (appres.secure)
		appres.disconnect_clear = True;

	a_delete_me = XInternAtom(display, "WM_DELETE_WINDOW", False);
	a_save_yourself = XInternAtom(display, "WM_SAVE_YOURSELF", False);
	a_3270 = XInternAtom(display, "3270", False);
	a_registry = XInternAtom(display, "CHARSET_REGISTRY", False);
	a_encoding = XInternAtom(display, "CHARSET_ENCODING", False);
	a_state = XInternAtom(display, "WM_STATE", False);

	action_init();
	XtAppAddActions(appcontext, actions, actioncount);

	keymap_init(appres.key_map, False);

	if (appres.apl_mode) {
		appres.compose_map = XtNewString(Apl);
		appres.charset = XtNewString(Apl);
	}
	switch (charset_init(appres.charset)) {
	    case CS_OKAY:
		break;
	    case CS_NOTFOUND:
		popup_an_error("Cannot find definition for host character set "
		    "\"%s\"", appres.charset);
		(void) charset_init(CN);
		break;
	    case CS_BAD:
		popup_an_error("Invalid definition for host character set "
		    "\"%s\"", appres.charset);
		(void) charset_init(CN);
		break;
	    case CS_PREREQ:
		popup_an_error("No fonts for host character set \"%s\"",
		    appres.charset);
		(void) charset_init(CN);
		break;
	    case CS_ILLEGAL:
		(void) charset_init(CN);
		break;
	}

	/* Initialize fonts. */
	font_init();

#if defined(RESTRICT_3279) /*[*/
	if (appres.m3279 && model_number == 4)
		model_number = 3;
#endif /*]*/
	if (!appres.extended || appres.oversize == CN ||
	    sscanf(appres.oversize, "%dx%d%c", &ovc, &ovr, &junk) != 2) {
		ovc = 0;
		ovr = 0;
	}
	set_rows_cols(model_number, ovc, ovr);
	if (appres.termname != CN)
		termtype = appres.termname;
	else
		termtype = full_model_name;

	hostfile_init();

	/* Initialize the icon. */
	icon_init();

	/*
	 * If no hostname is specified on the command line, ignore certain
	 * options.
	 */
	if (argc <= 1) {
#if defined(LOCAL_PROCESS) /*[*/
		if (cl_hostname == CN)
#endif /*]*/
			appres.once = False;
		appres.reconnect = False;
	}

#if !defined(X3270_MENUS) /*[*/
	/*
	 * If there are no menus, then -once is the default; let -reconnect
	 * override it.
	 */
	if (appres.reconnect)
		appres.once = False;
#endif /*]*/

	if (appres.char_class != CN)
		reclass(appres.char_class);

	screen_init();
	kybd_init();
	idle_init();
	ansi_init();
	sms_init();
	info_popup_init();
	error_popup_init();
#if defined(X3270_FT) && !defined(X3270_MENUS) /*[*/
	ft_init();
#endif /*]*/
#if defined(X3270_PRINTER) /*[*/
	printer_init();
#endif /*]*/

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
#if defined(X3270_TRACE) /*[*/
	if (!appres.debug_tracing) {
		appres.toggle[DS_TRACE].value = False;
		appres.toggle[EVENT_TRACE].value = False;
	}
#endif /*]*/
	initialize_toggles();

	/* Connect to the host. */
	if (cl_hostname != CN)
		(void) host_connect(cl_hostname);

	/* Prepare to run a peer script. */
	peer_script_init();

	/* Process X events forever. */
	while (1) {
		XEvent		event;

		while (XtAppPending(appcontext) & (XtIMXEvent | XtIMTimer)) {
			if (XtAppPeekEvent(appcontext, &event))
				peek_at_xevent(&event);
			XtAppProcessEvent(appcontext,
			    XtIMXEvent | XtIMTimer);
		}
		screen_disp(False);
		XtAppProcessEvent(appcontext, XtIMAll);

		if (children && waitpid(0, (int *)0, WNOHANG) > 0)
			--children;
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
			appres.m3279 = False;
		} else if (!strncmp(m, "3279", 4)) {
			appres.m3279 = True;
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
relabel(Boolean ignored _is_unused)
{
	char *title;
	char icon_label[8];

	if (user_title != CN && user_icon_name != CN)
		return;
	title = XtMalloc(10 + ((PCONNECTED || appres.reconnect) ?
						strlen(reconnect_host) : 0));
	if (PCONNECTED || appres.reconnect) {
		(void) sprintf(title, "x3270-%d%s %s", model_num,
		    (IN_ANSI ? "A" : ""), reconnect_host);
		if (user_title == CN)
			XtVaSetValues(toplevel, XtNtitle, title, NULL);
		if (user_icon_name == CN)
			XtVaSetValues(toplevel,
			    XtNiconName, reconnect_host,
			    NULL);
		set_aicon_label(reconnect_host);
	} else {
		(void) sprintf(title, "x3270-%d", model_num);
		(void) sprintf(icon_label, "x3270-%d", model_num);
		if (user_title == CN)
			XtVaSetValues(toplevel, XtNtitle, title, NULL);
		if (user_icon_name == CN)
			XtVaSetValues(toplevel, XtNiconName, icon_label, NULL);
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
	if (user_icon_name != CN)
		set_aicon_label(user_icon_name);

	register_schange(ST_HALF_CONNECT, relabel);
	register_schange(ST_CONNECT, relabel);
	register_schange(ST_3270_MODE, relabel);
	register_schange(ST_REMODEL, relabel);
}

/*
 * Peek at X events before Xt does, calling PA_KeymapNotify_action if we see a
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
		PA_KeymapNotify_action((Widget)NULL, e, (String *)NULL, &zero);
	}
}


/*
 * Warning message trap, for catching colormap failures.
 */
static void
trap_colormaps(String name, String type, String class, String defaultp,
    String *params, Cardinal *num_params)
{
    if (!strcmp(type, "cvtStringToPixel"))
	    colormap_failure = True;
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
		if (strcmp(argv[i], OptLocalProcess))
			continue;

		/* Matched.  Copy 'em. */
		e_len = strlen(OptLocalProcess) + 1;
		for (j = i+1; j < *argcp; j++) {
			e_len += 1 + strlen(argv[j]);
		}
		e_len++;
		*cmds = XtMalloc(e_len);
		(void) strcpy(*cmds, OptLocalProcess);
		for (j = i+1; j < *argcp; j++) {
			(void) strcat(strcat(*cmds, " "), argv[j]);
		}

		/* Stamp out the remaining args. */
		*argcp = i;
		argv[i] = CN;
		break;
	}
}
#endif /*]*/

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
		Boolean is_set = False;

		if (!strcmp(argv[i], OptSet))
			is_set = True;
		else if (strcmp(argv[i], OptClear)) {
			argv_out[argc_out++] = argv[i];
			continue;
		}

		if (i == *argcp - 1)	/* missing arg */
			continue;

		/* Delete the argument. */
		i++;

		for (j = 0; j < N_TOGGLES; j++)
			if (toggle_names[i].index >= 0 &&
			    !strcmp(argv[i], toggle_names[j].name)) {
				appres.toggle[toggle_names[j].index].value =
				    is_set;
				break;
			}
		if (j >= N_TOGGLES)
			usage("Unknown toggle name");

	}
	*argcp = argc_out;
	argv_out[argc_out] = CN;
	(void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
	Free(argv_out);
}
