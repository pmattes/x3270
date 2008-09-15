/*
 * Modifications Copyright 1993-2008 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *   All Rights Reserved.  GTRC hereby grants public use of this software.
 *   Derivative works based on this software must incorporate this copyright
 *   notice.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	glue.c
 *		A displayless 3270 Terminal Emulator
 *		Glue for missing parts.
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

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "gluec.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "screenc.h"
#include "selectc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "utilc.h"

#if defined(_WIN32) /*[*/
#include "winversc.h"
#endif /*]*/

extern void usage(char *);

#define LAST_ARG	"--"

#if defined(WC3270) /*[*/
#define PROFILE_SFX	".wc3270"
#define PROFILE_SFX_LEN	(sizeof(PROFILE_SFX) - 1)
#define PROFILE_SSFX	".wc3"
#define PROFILE_SSFX_LEN (sizeof(PROFILE_SSFX) - 1)
#endif /*]*/

#if defined(C3270) /*[*/
extern void merge_profile(void); /* XXX */
extern Boolean any_error_output;
#endif /*]*/

/* Statics */
static void no_minus(const char *arg);
#if defined(LOCAL_PROCESS) /*[*/
static void parse_local_process(int *argcp, const char **argv,
    const char **cmds);
#endif /*]*/
static void parse_options(int *argcp, const char **argv);
static void parse_set_clear(int *argcp, const char **argv);
static int parse_model_number(char *m);

/* Globals */
const char     *programname;
char		full_model_name[13] = "IBM-";
char	       *model_name = &full_model_name[4];
AppRes          appres;
int		children = 0;
Boolean		exiting = False;
char	       *command_string = CN;
static Boolean	sfont = False;
Boolean	       *standard_font = &sfont;
#if defined(WC3270) /*[*/
char	       *profile_name = CN;
#endif /*]*/

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
	{ ResCrosshair,       -1 },
	{ ResVisibleControl,  -1 },
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
	{ ResAidWait,         AID_WAIT },
#else /*][*/
	{ ResAidWait,         -1 },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResUnderscore,      UNDERSCORE },
#else /*][*/
	{ ResUnderscore,      -1 },
#endif /*]*/
};


int
parse_command_line(int argc, const char **argv, const char **cl_hostname)
{
	int cl, i;
	int ovc, ovr;
	char junk;
	int hn_argc;
	int model_number;
#if defined(WC3270) /*[*/
	int sl;
#endif /*]*/

	/* Figure out who we are */
#if defined(_WIN32) /*[*/
	programname = strrchr(argv[0], '\\');
#else /*][*/
	programname = strrchr(argv[0], '/');
#endif /*]*/
	if (programname)
		++programname;
	else
		programname = argv[0];

	/* Save the command string. */
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

#if defined(LOCAL_PROCESS) /*[*/ 
        /* Pick out the -e option. */
        parse_local_process(&argc, argv, cl_hostname);
#endif /*]*/    

	/* Parse command-line options. */
	parse_options(&argc, argv);

	/* Pick out the remaining -set and -clear toggle options. */
	parse_set_clear(&argc, argv);

	/* Now figure out if there's a hostname. */
	for (hn_argc = 1; hn_argc < argc; hn_argc++) {
		if (!strcmp(argv[hn_argc], LAST_ARG))
			break;
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
		usage(CN);
		break;
	}

	/* Delete the host name and any "--". */
	if (argv[hn_argc] != CN && !strcmp(argv[hn_argc], LAST_ARG))
		hn_argc++;
	if (hn_argc > 1) {
		for (i = 1; i < argc - hn_argc + 2; i++) {
			argv[i] = argv[i + hn_argc - 1];
		}
	}

#if defined(WC3270) /*[*/
	/* Merge in the profile. */
	if (*cl_hostname != CN &&
	    (((sl = strlen(*cl_hostname)) > PROFILE_SFX_LEN &&
	      !strcasecmp(*cl_hostname + sl - PROFILE_SFX_LEN, PROFILE_SFX)) ||
	     ((sl = strlen(*cl_hostname)) > PROFILE_SSFX_LEN &&
	      !strcasecmp(*cl_hostname + sl - PROFILE_SSFX_LEN, PROFILE_SSFX)))) {

		const char *pname;

		(void) read_resource_file(*cl_hostname, False);
		if (appres.hostname == CN) {
		    Error("Hostname not specified in session file.");
		}

		pname = strrchr(*cl_hostname, '\\');
		if (pname != CN)
		    	pname++;
		else
		    	pname = *cl_hostname;
		profile_name = NewString(pname);

		sl = strlen(profile_name);
		if (sl > PROFILE_SFX_LEN &&
			!strcasecmp(profile_name + sl - PROFILE_SFX_LEN,
				PROFILE_SFX)) {
			profile_name[sl - PROFILE_SFX_LEN] = '\0';
		} else if (sl > PROFILE_SSFX_LEN &&
			!strcasecmp(profile_name + sl - PROFILE_SSFX_LEN,
				PROFILE_SSFX)) {
			profile_name[sl - PROFILE_SSFX_LEN] = '\0';
		}

		*cl_hostname = appres.hostname;
	}
#endif /*]*/

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
#if defined(C3270) && !defined(_WIN32) /*[*/
	if (appres.mono)
		appres.m3279 = False;
#endif /*]*/
	if (!appres.extended)
		appres.oversize = CN;

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

	if (appres.apl_mode)
		appres.charset = Apl;
	if (*cl_hostname == CN)
		appres.once = False;
	if (appres.conf_dir == CN)
		appres.conf_dir = LIBX3270DIR;

	return argc;
}

static void
no_minus(const char *arg)
{
	if (arg[0] == '-')
	    usage(xs_buffer("Unknown or incomplete option: %s", arg));
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
		if (strcmp(argv[i], OptLocalProcess))
			continue;

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
		argv[i] = CN;
		break;
	}
	*cmds = cmds_buf;
}
#endif /*]*/

/*
 * Pick out command-line options and set up appres.
 */
static void
parse_options(int *argcp, const char **argv)
{
	int i, j;
	int argc_out = 0;
	const char **argv_out =
	    (const char **) Malloc((*argcp + 1) * sizeof(char *));
#       define offset(n) (void *)&appres.n
#       define toggle_offset(index) offset(toggle[index].value)
	static struct {
		const char *name;
		enum {
		    OPT_BOOLEAN, OPT_STRING, OPT_XRM, OPT_SKIP2, OPT_NOP,
		    OPT_INT, OPT_V, OPT_DONE
		} type;
		Boolean flag;
		const char *res_name;
		void *aoff;
	} opts[] = {
#if defined(C3270) /*[*/
    { OptAllBold,  OPT_BOOLEAN, True,  ResAllBold,   offset(all_bold_on) },
#endif /*]*/
#if defined(C3270) /*[*/
    { OptAltScreen,OPT_STRING,  False, ResAltScreen, offset(altscreen) },
#endif /*]*/
    { OptAplMode,  OPT_BOOLEAN, True,  ResAplMode,   offset(apl_mode) },
#if defined(C3270) /*[*/
    { OptCbreak,   OPT_BOOLEAN, True,  ResCbreak,    offset(cbreak_mode) },
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
    { OptCertFile, OPT_STRING,  False, ResCertFile,  offset(cert_file) },
#endif /*]*/
    { OptCharset,  OPT_STRING,  False, ResCharset,   offset(charset) },
    { OptClear,    OPT_SKIP2,   False, NULL,         NULL },
#if defined(C3270) /*[*/
    { OptDefScreen,OPT_STRING,  False, ResDefScreen, offset(defscreen) },
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
    { OptDsTrace,  OPT_BOOLEAN, True,  ResDsTrace,   toggle_offset(DS_TRACE) },
#endif /*]*/
    { OptHostsFile,OPT_STRING,  False, ResHostsFile, offset(hostsfile) },
#if defined(C3270) /*[*/
    { OptKeymap,   OPT_STRING,  False, ResKeymap,    offset(key_map) },
#endif /*]*/
    { OptModel,    OPT_STRING,  False, ResKeymap,    offset(model) },
#if defined(C3270) && !defined(_WIN32) /*[*/
    { OptMono,     OPT_BOOLEAN, True,  ResMono,      offset(mono) },
#endif /*]*/
    { OptOnce,     OPT_BOOLEAN, True,  ResOnce,      offset(once) },
    { OptOversize, OPT_STRING,  False, ResOversize,  offset(oversize) },
    { OptPort,     OPT_STRING,  False, ResPort,      offset(port) },
#if defined(C3270) /*[*/
    { OptPrinterLu,OPT_STRING,  False, ResPrinterLu, offset(printer_lu) },
#endif /*]*/
    { OptProxy,	   OPT_STRING,  False, ResProxy,     offset(proxy) },
#if defined(S3270) /*[*/
    { OptScripted, OPT_NOP,     False, ResScripted,  NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { OptSecure,   OPT_BOOLEAN, True,  ResSecure,    offset(secure) },
#endif /*]*/
    { OptSet,      OPT_SKIP2,   False, NULL,         NULL },
#if defined(X3270_SCRIPT) /*[*/
    { OptSocket,   OPT_BOOLEAN, True,  ResSocket,    offset(socket) },
#endif /*]*/
    { OptTermName, OPT_STRING,  False, ResTermName,  offset(termname) },
#if defined(WC3270) /*[*/
    { OptTitle,    OPT_STRING,  False, ResTitle,     offset(title) },
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
    { OptTraceFile,OPT_STRING,  False, ResTraceFile, offset(trace_file) },
    { OptTraceFileSize,OPT_STRING,False,ResTraceFileSize,offset(trace_file_size) },
#endif /*]*/
    { OptV,        OPT_V,	False, NULL,	     NULL },
    { OptVersion,  OPT_V,	False, NULL,	     NULL },
    { "-xrm",      OPT_XRM,     False, NULL,         NULL },
    { LAST_ARG,    OPT_DONE,    False, NULL,         NULL },
    { CN,          OPT_SKIP2,   False, NULL,         NULL }
};

	/* Set the defaults. */
#if defined(C3270) && !defined(_WIN32) /*[*/
	appres.mono = False;
#endif /*]*/
	appres.extended = True;
#if defined(C3270) /*[*/
	appres.m3279 = True;
#else /*][*/
	appres.m3279 = False;
#endif /*]*/
	appres.modified_sel = False;
	appres.apl_mode = False;
#if defined(C3270) || defined(TCL3270) /*[*/
	appres.scripted = False;
#else /*][*/
	appres.scripted = True;
#endif /*]*/
	appres.numeric_lock = False;
	appres.secure = False;
#if defined(C3270) /*[*/
	appres.oerr_lock = True;
#else /*][*/
	appres.oerr_lock = False;
#endif /*]*/
	appres.typeahead = True;
	appres.debug_tracing = True;
#if defined(C3270) /*[*/
	appres.compose_map = "latin1";
#endif /*]*/

	appres.model = "4";
	appres.hostsfile = CN;
	appres.port = "telnet";

#if !defined(_WIN32) /*[*/
	appres.charset = "bracket";
#else /*][*/
	if (is_nt)
		appres.charset = "bracket";
	else
		appres.charset = "bracket437";
#endif /*]*/

	appres.termname = CN;
	appres.macros = CN;
#if defined(X3270_TRACE) && !defined(_WIN32) /*[*/
	appres.trace_dir = "/tmp";
#endif /*]*/
#if defined(WC3270) /*[*/
	appres.trace_monitor = True;
#endif /*]*/
	appres.oversize = CN;
#if defined(C3270) /*[*/
	appres.meta_escape = "auto";
	appres.curses_keypad = True;
	appres.cbreak_mode = False;
#if defined(CURSES_WIDE) /*[*/
	appres.acs = True;
#endif /*]*/
#endif /*]*/

#if defined(X3270_ANSI) /*[*/
	appres.icrnl = True;
	appres.inlcr = False;
	appres.onlcr = True;
	appres.erase = "^H";
	appres.kill = "^U";
	appres.werase = "^W";
	appres.rprnt = "^R";
	appres.lnext = "^V";
	appres.intr = "^C";
	appres.quit = "^\\";
	appres.eof = "^D";
#endif /*]*/

	appres.unlock_delay = True;
	appres.unlock_delay_ms = 350;

#if defined(X3270_FT) /*[*/
	appres.dft_buffer_size = DFT_BUF;
#endif /*]*/

#if defined(C3270) /*[*/
	appres.toggle[CURSOR_POS].value = True;
#endif /*]*/
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
	appres.toggle[AID_WAIT].value = True;
#endif /*]*/
#if defined(C3270) && defined(_WIN32) /*[*/
	appres.toggle[UNDERSCORE].value = True;
#endif /*]*/

#if defined(C3270) && defined(X3270_SCRIPT) /*[*/
	appres.plugin_command = "x3270hist.pl";
#endif /*]*/

#if defined(C3270) && !defined(_WIN32) /*[*/
	/* Merge in the profile. */
	merge_profile();
#endif /*]*/

	/* Parse the command-line options. */
	argv_out[argc_out++] = argv[0];

	for (i = 1; i < *argcp; i++) {
		for (j = 0; opts[j].name != CN; j++) {
			if (!strcmp(argv[i], opts[j].name))
				break;
		}
		if (opts[j].name == CN) {
			argv_out[argc_out++] = argv[i];
			continue;
		}

		switch (opts[j].type) {
		    case OPT_BOOLEAN:
			*(Boolean *)opts[j].aoff = opts[j].flag;
			if (opts[j].res_name != CN)
				add_resource(NewString(opts[j].name),
					     opts[j].flag? "True": "False");
			break;
		    case OPT_STRING:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			*(const char **)opts[j].aoff = argv[++i];
			if (opts[j].res_name != CN)
				add_resource(NewString(opts[j].name),
					     NewString(argv[i]));
			break;
		    case OPT_XRM:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			parse_xrm(argv[++i], "-xrm");
			break;
		    case OPT_SKIP2:
			argv_out[argc_out++] = argv[i++];
			if (i < *argcp)
				argv_out[argc_out++] = argv[i];
			break;
		    case OPT_NOP:
			break;
		    case OPT_INT:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			*(int *)opts[j].aoff = atoi(argv[++i]);
			if (opts[j].res_name != CN)
				add_resource(NewString(opts[j].name),
					     NewString(argv[i]));
			break;
		    case OPT_V:
			dump_version();
			break;
		    case OPT_DONE:
			while (i < *argcp)
				argv_out[argc_out++] = argv[i++];
			break;
		}
	}
	*argcp = argc_out;
	argv_out[argc_out] = CN;
	(void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
	Free(argv_out);

#if defined(X3270_TRACE) /*[*/
	/* One isn't very useful without the other. */
	if (appres.toggle[DS_TRACE].value)
		appres.toggle[EVENT_TRACE].value = True;
#endif /*]*/
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
			if (toggle_names[j].index >= 0 &&
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

/*
 * Parse '-xrm' options.
 * Understands only:
 *   {c,s,tcl}3270.<resourcename>: value
 * Asterisks and class names need not apply.
 */

static struct {
	const char *name;
	void *address;
	enum resource_type { XRM_STRING, XRM_BOOLEAN, XRM_INT } type;
} resources[] = {
#if defined(C3270) /*[*/
	{ ResAllBold,	offset(all_bold),	XRM_STRING },
	{ ResAltScreen,	offset(altscreen),	XRM_STRING },
#endif /*]*/
	{ ResBsdTm,	offset(bsd_tm),		XRM_BOOLEAN },
#if defined(HAVE_LIBSSL) /*[*/
	{ ResCertFile,	offset(cert_file),	XRM_STRING },
#endif /*]*/
	{ ResCharset,	offset(charset),	XRM_STRING },
	{ ResColor8,	offset(color8),		XRM_BOOLEAN },
#if defined(TCL3270) /*[*/
	{ ResCommandTimeout, offset(command_timeout), XRM_INT },
#endif /*]*/
	{ ResConfDir,	offset(conf_dir),	XRM_STRING },
#if defined(C3270) /*[*/
	{ ResDefScreen,	offset(defscreen),	XRM_STRING },
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
	{ ResEof,	offset(eof),		XRM_STRING },
	{ ResErase,	offset(erase),		XRM_STRING },
#endif /*]*/
	{ ResExtended,	offset(extended),	XRM_BOOLEAN },
#if defined(X3270_FT) /*[*/
	{ ResDftBufferSize,offset(dft_buffer_size),XRM_INT },
#endif /*]*/
#if defined(WC3270) /*[*/
	{ "hostname",	offset(hostname),	XRM_STRING },
#endif /*]*/
	{ ResHostsFile,	offset(hostsfile),	XRM_STRING },
#if defined(X3270_ANSI) /*[*/
	{ ResIcrnl,	offset(icrnl),		XRM_BOOLEAN },
	{ ResInlcr,	offset(inlcr),		XRM_BOOLEAN },
	{ ResOnlcr,	offset(onlcr),		XRM_BOOLEAN },
	{ ResIntr,	offset(intr),		XRM_STRING },
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
	{ ResPluginCommand, offset(plugin_command), XRM_STRING },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResIdleCommand,offset(idle_command),	XRM_STRING },
	{ ResIdleCommandEnabled,offset(idle_command_enabled),	XRM_BOOLEAN },
	{ ResIdleTimeout,offset(idle_timeout),	XRM_STRING },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResKeymap,	offset(key_map),	XRM_STRING },
	{ ResMetaEscape,offset(meta_escape),	XRM_STRING },
	{ ResCursesKeypad,offset(curses_keypad),XRM_BOOLEAN },
	{ ResCbreak,	offset(cbreak_mode),	XRM_BOOLEAN },
#if defined(CURSES_WIDE) /*[*/
	{ ResAcs,	offset(acs),		XRM_BOOLEAN },
#endif /*]*/
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
	{ ResKill,	offset(kill),		XRM_STRING },
	{ ResLnext,	offset(lnext),		XRM_STRING },
#endif /*]*/
	{ ResLoginMacro,offset(login_macro),	XRM_STRING },
	{ ResM3279,	offset(m3279),		XRM_BOOLEAN },
	{ ResModel,	offset(model),		XRM_STRING },
	{ ResModifiedSel, offset(modified_sel),	XRM_BOOLEAN },
#if defined(C3270) && !defined(_WIN32) /*[*/
	{ ResMono,	offset(mono),		XRM_BOOLEAN },
#endif /*]*/
	{ ResNumericLock, offset(numeric_lock),	XRM_BOOLEAN },
	{ ResOerrLock,	offset(oerr_lock),	XRM_BOOLEAN },
	{ ResOversize,	offset(oversize),	XRM_STRING },
	{ ResPort,	offset(port),		XRM_STRING },
#if defined(C3270) /*[*/
	{ ResPrinterLu,	offset(printer_lu),	XRM_STRING },
	{ ResPrintTextCommand,	NULL,		XRM_STRING },
#endif /*]*/
	{ ResProxy,	offset(proxy),		XRM_STRING },
#if defined(X3270_ANSI) /*[*/
	{ ResQuit,	offset(quit),		XRM_STRING },
	{ ResRprnt,	offset(rprnt),		XRM_STRING },
#endif /*]*/
	{ ResSecure,	offset(secure),		XRM_BOOLEAN },
	{ ResTermName,	offset(termname),	XRM_STRING },
#if defined(WC3270) /*[*/
	{ ResTitle,	offset(title),		XRM_STRING },
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
#if !defined(_WIN32) /*[*/
	{ ResTraceDir,	offset(trace_dir),	XRM_STRING },
#endif /*]*/
	{ ResTraceFile,	offset(trace_file),	XRM_STRING },
	{ ResTraceFileSize,offset(trace_file_size),XRM_STRING },
#if defined(WC3270) /*[*/
	{ ResTraceMonitor,offset(trace_monitor),XRM_BOOLEAN },
#endif /*]*/
#endif /*]*/
	{ ResTypeahead,	offset(typeahead),	XRM_BOOLEAN },
	{ ResUnlockDelay,offset(unlock_delay),	XRM_BOOLEAN },
	{ ResUnlockDelayMs,offset(unlock_delay_ms),XRM_INT },
#if defined(X3270_ANSI) /*[*/
	{ ResWerase,	offset(werase),		XRM_STRING },
#endif /*]*/

	{ CN,		0,			XRM_STRING }
};

/*
 * Compare two strings, allowing the second to differ by uppercasing the
 * first character of the second.
 */
static int
strncapcmp(const char *known, const char *unknown, unsigned unk_len)
{
	if (unk_len != strlen(known))
		return -1;
	if (!strncmp(known, unknown, unk_len))
		return 0;
	if (unk_len > 1 &&
	    unknown[0] == toupper(known[0]) &&
	    !strncmp(known + 1, unknown + 1, unk_len - 1))
		return 0;
	return -1;
}


#if !defined(ME) /*[*/
#if defined(C3270) /*[*/
#if defined(WC3270) /*[*/
#define ME	"wc3270"
#else /*][*/
#define ME	"c3270"
#endif /*]*/
#elif defined(TCL3270) /*][*/
#define ME	"tcl3270"
#else /*][*/
#define ME	"s3270"
#endif /*]*/
#endif /*]*/

void
parse_xrm(const char *arg, const char *where)
{
	static char me_dot[] = ME ".";
	static char me_star[] = ME "*";
	unsigned match_len;
	const char *s;
	unsigned rnlen;
	int i;
	char *t;
	void *address = NULL;
	enum resource_type type = XRM_STRING;
#if defined(C3270) /*[*/
	char *add_buf = CN;
	char *hide;
	Boolean arbitrary = False;
#endif /*]*/

	/* Enforce "-3270." or "-3270*" or "*". */
	if (!strncmp(arg, me_dot, sizeof(me_dot)-1))
		match_len = sizeof(me_dot)-1;
	else if (!strncmp(arg, me_star, sizeof(me_star)-1))
		match_len = sizeof(me_star)-1;
	else if (arg[0] == '*')
		match_len = 1;
	else {
		xs_warning("%s: Invalid resource syntax '%.*s', name must "
		    "begin with '%s'",
		    where, sizeof(me_dot)-1, arg, me_dot);
		return;
	}

	/* Separate the parts. */
	s = arg + match_len;
	while (*s && *s != ':' && !isspace(*s))
		s++;
	rnlen = s - (arg + match_len);
	if (!rnlen) {
		xs_warning("%s: Invalid resource syntax, missing resource "
		    "name", where);
		return;
	}
	while (isspace(*s))
		s++;
	if (*s != ':') {
		xs_warning("%s: Invalid resource syntax, missing ':'", where);
		return;
	}
	s++;
	while (isspace(*s))
		s++;

	/* Look up the name. */
	for (i = 0; resources[i].name != CN; i++) {
		if (!strncapcmp(resources[i].name, arg + match_len, rnlen)) {
			address = resources[i].address;
			type = resources[i].type;
#if defined(C3270) /*[*/
			if (address == NULL) {
				add_buf = Malloc(strlen(s) + 1);
				address = add_buf;
			}
#endif /*]*/
			break;
		}
	}
	if (address == NULL) {
		for (i = 0; i < N_TOGGLES; i++) {
			if (toggle_names[i].index >= 0 &&
			    !strncapcmp(toggle_names[i].name, arg + match_len,
			    rnlen)) {
				address =
				    &appres.toggle[toggle_names[i].index].value;
				type = XRM_BOOLEAN;
				break;
			}
		}
	}
#if defined(C3270) /*[*/
	if (address == NULL) {
		if (!strncasecmp(ResKeymap ".", arg + match_len,
		                 strlen(ResKeymap ".")) ||
		    !strncasecmp("host.", arg + match_len, 5) ||
		    !strncasecmp("printer.", arg + match_len, 8) ||
#if defined(_WIN32) /*[*/
		    !strncasecmp(ResHostColorFor, arg + match_len,
			    strlen(ResHostColorFor)) ||
		    !strncasecmp(ResConsoleColorForHostColor, arg + match_len,
			    strlen(ResConsoleColorForHostColor))
#else /*][*/
		    !strncasecmp(ResCursesColorFor, arg + match_len,
			    strlen(ResCursesColorFor))
#endif /*]*/
		    ) {
			address = &hide;
			type = XRM_STRING;
			arbitrary = True;
		}
	}
#endif /*]*/
	if (address == NULL) {
		xs_warning("%s: Unknown resource name: %.*s",
		    where, (int)rnlen, arg + match_len);
		return;
	}
	switch (type) {
	case XRM_BOOLEAN:
		if (!strcasecmp(s, "true") ||
		    !strcasecmp(s, "t") ||
		    !strcmp(s, "1")) {
			*(Boolean *)address = True;
		} else if (!strcasecmp(s, "false") ||
		    !strcasecmp(s, "f") ||
		    !strcmp(s, "0")) {
			*(Boolean *)address = False;
		} else {
			xs_warning("%s: Invalid Boolean value: %s", where, s);
		}
		break;
	case XRM_STRING:
		t = Malloc(strlen(s) + 1);
		*(char **)address = t;
		if (*s == '"') {
			Boolean quoted = False;
			char c;

			s++;
			while ((c = *s++) != '\0') {
				if (quoted) {
					switch (c) {
					case 'n':
						*t++ = '\n';
						break;
					case 'r':
						*t++ = '\r';
						break;
					case 'b':
						*t++ = '\b';
						break;
					default:
						*t++ = c;
						break;
					}
					quoted = False;
				} else if (c == '\\') {
					quoted = True;
				} else if (c == '"') {
					break;
				} else {
					*t++ = c;
				}
			}
			*t = '\0';
		} else {
			(void) strcpy(t, s);
		}
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

#if defined(C3270) /*[*/
	/* Add a new, arbitrarily-named resource. */
	if (arbitrary) {
		char *rsname;

		rsname = Malloc(rnlen + 1);
		(void) strncpy(rsname, arg + match_len, rnlen);
		rsname[rnlen] = '\0';
		add_resource(rsname, hide);
	}
#endif /*]*/
}

/* Read resources from a file. */
int
read_resource_file(const char *filename, Boolean fatal)
{
	FILE *f;
	int ilen;
	char buf[4096];
	char *where;
	int lno = 0;

	f = fopen(filename, "r");
	if (f == NULL) {
		if (fatal)
			xs_warning("Cannot open '%s': %s", filename,
			    strerror(errno));
		return -1;
	}

	/* Merge in what's in the file into the resource database. */
	where = Malloc(strlen(filename) + 64);

	ilen = 0;
	while (fgets(buf + ilen, sizeof(buf) - ilen, f) != CN || ilen) {
		char *s, *t;
		unsigned sl;
		Boolean bsl;

		lno++;

		/* Stip any trailing newline. */
		sl = strlen(buf + ilen);
		if (sl && (buf + ilen)[sl-1] == '\n')
			(buf + ilen)[--sl] = '\0';

		/*
		 * Translate backslash-n to real newline characters, and
		 * remember if the last character is a backslash.
		 */
		for (bsl = False, s = buf + ilen, t = s; *s; s++) {
			if (bsl) {
				if (*s == 'n')
					*t++ = '\n';
				else
					*t++ = *s;
				bsl = False;
			} else if (*s == '\\')
				bsl = True;
			else {
				*t++ = *s;
				bsl = False;
			}
		}
		*t = '\0';

		/* Skip leading whitespace. */
		s = buf;
		while (isspace(*s))
			s++;

		/* Skip comments _before_ checking for line continuation. */
		if (*s == '!') {
		    ilen = 0;
		    continue;
		}
		if (*s == '#') {
			(void) sprintf(where, "%s:%d: Invalid profile "
			    "syntax ('#' ignored)", filename, lno);
			Warning(where);
			ilen = 0;
			continue;
		}

		/* If this line is a continuation, try again. */
		if (bsl) {
			ilen += strlen(buf + ilen);
			if ((unsigned)ilen >= sizeof(buf) - 1) {
				(void) sprintf(where, "%s:%d: Line too long\n",
				    filename, lno);
				Warning(where);
				break;
			}
			continue;
		}

		/* Strip trailing whitespace and check for empty lines. */
		sl = strlen(s);
		while (sl && isspace(s[sl-1]))
			s[--sl] = '\0';
		if (!sl) {
			ilen = 0;
			continue;
		}

		/* Digest it. */
		(void) sprintf(where, "%s:%d", filename, lno);
		parse_xrm(s, where);

		/* Get ready for the next iteration. */
		ilen = 0;
	}
	Free(where);
	return 0;
}

/* Screen globals. */

static int cw = 7;
int *char_width = &cw;

static int ch = 7;
int *char_height = &ch;

Boolean visible_control = False;

Boolean flipped = False;

/* Replacements for functions in popups.c. */

#include <stdarg.h>

Boolean error_popup_visible = False;

static char vmsgbuf[4096];

/* Pop up an error dialog. */
void
popup_an_error(const char *fmt, ...)
{
	va_list args;
	char *s;
	int sl;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);

	/*
	 * Multi-line messages are fine for X pop-ups, but they're no fun for
	 * text applications.
	 */
	s = vmsgbuf;
	while ((s = strchr(s, '\n')) != NULL) {
		*s++ = ' ';
	}
	while ((sl = strlen(vmsgbuf)) > 0 && vmsgbuf[sl-1] == ' ') {
		vmsgbuf[--sl] = '\0';
	}

	if (sms_redirect()) {
		sms_error(vmsgbuf);
		return;
	} else {
#if defined(C3270) || defined(WC3270) /*[*/
		screen_suspend();
		any_error_output = True;
#endif /*]*/
		(void) fprintf(stderr, "%s\n", vmsgbuf);
		macro_output = True;
	}
}

/* Pop up an error dialog, based on an error number. */
void
popup_an_errno(int errn, const char *fmt, ...)
{
	va_list args;
	char *s;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);
	s = NewString(vmsgbuf);

	if (errn > 0)
		popup_an_error("%s:\n%s", s, strerror(errn));
	else
		popup_an_error(s);
	Free(s);
}

void
action_output(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);
	if (sms_redirect()) {
		sms_info("%s", vmsgbuf);
		return;
	} else {
		FILE *aout;

#if defined(C3270) || defined(WC3270) /*[*/
		screen_suspend();
		aout = start_pager();
		any_error_output = True;
#else /*][*/
		aout = stdout;
#endif /*]*/
		(void) fprintf(aout, "%s\n", vmsgbuf);
		macro_output = True;
	}
}

#if defined(_WIN32) /*[*/

/* Missing parts for wc3270. */
#include <windows.h>
#define SECS_BETWEEN_EPOCHS	11644473600ULL
#define SECS_TO_100NS		10000000ULL /* 10^7 */

int
gettimeofday(struct timeval *tv, void *ignored)
{
	FILETIME t;
	ULARGE_INTEGER u;

	GetSystemTimeAsFileTime(&t);
	memcpy(&u, &t, sizeof(ULARGE_INTEGER));

	/* Isolate seconds and move epochs. */
	tv->tv_sec = (DWORD)((u.QuadPart / SECS_TO_100NS) -
			       	SECS_BETWEEN_EPOCHS);
	tv->tv_usec = (u.QuadPart % SECS_TO_100NS) / 10ULL;
	return 0;
}

#endif /*]*/
