/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *	screentrace.c
 *		Screen tracing.
 *
 */

#include "globals.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"

#include "actions.h"
#include "codepage.h"
#include "ctlrc.h"
#include "find_console.h"
#include "fprint_screen.h"
#include "menubar.h"
#include "names.h"
#include "nvt.h"
#include "popups.h"
#include "print_command.h"
#include "print_screen.h"
#include "product.h"
#include "resources.h"
#include "save.h"
#include "task.h"
#include "telnet.h"
#include "telnet_core.h"
#include "toggles.h"
#include "trace.h"
#include "trace_gui.h"
#include "txa.h"
#include "utf8.h"
#include "utils.h"
#include "vstatus.h"
#if defined(_WIN32) /*[*/
# include <sys/stat.h>
# include "w3misc.h"
# include "windirs.h"
# include "winprint.h"
#endif /*]*/

#include "screentrace.h"

/* Typedefs */

/* Extended wait screen tracing context. */
typedef struct {
    ptype_t ptype;
    unsigned opts;
    char *caption;
} screentrace_t;

typedef struct {
    tss_t target;
    ptype_t ptype;
    unsigned opts;
} screentrace_opts_t;

/* Statics */
static char    *onetime_screentrace_name = NULL;
static screentrace_opts_t screentrace_default = {
    TSS_FILE, P_TEXT, 0
};
static screentrace_opts_t screentrace_current = {
    TSS_FILE, P_TEXT, 0
};

static char    *screentrace_name = NULL;
#if defined(_WIN32) /*[*/
static char    *screentrace_tmpfn;
#endif /*]*/
static int 	screentrace_count;

/* Globals */

/* Statics */
static FILE    *screentracef = NULL;
static fps_t	screentrace_fps = NULL;

/*
 * Screen trace function, called when the host clears the screen.
 */
static void
do_screentrace(bool always _is_unused)
{
    fps_status_t status;

    status = fprint_screen_body(screentrace_fps);
    if (FPS_IS_ERROR(status)) {
	popup_an_error("Screen trace failed");
    } else if (status == FPS_STATUS_SUCCESS) {
	vtrace("screentrace: nothing written\n");
    } else {
	vstatus_screentrace(++screentrace_count);
    }
}

void
trace_screen(bool is_clear)
{
    trace_skipping = false;

    if (!toggled(SCREEN_TRACE) || !screentracef) {
	return;
    }
    do_screentrace(is_clear);
}

/* Called from NVT emulation code to log a single character. */
void
trace_char(char c)
{
    if (!toggled(SCREEN_TRACE) || !screentracef) {
	return;
    }
    fputc(c, screentracef);
}

/*
 * Called when disconnecting in NVT mode, to finish off the trace file
 * and keep the next screen clear from re-recording the screen image.
 * (In a gross violation of data hiding and modularity, trace_skipping is
 * manipulated directly in ctlr_clear()).
 */
void
trace_nvt_disc(void)
{
    int i;

    fputc('\n', screentracef);
    for (i = 0; i < COLS; i++) {
	fputc('=', screentracef);
    }
    fputc('\n', screentracef);

    trace_skipping = true;
}

/*
 * Extended wait continue function for screen tracing.
 */
static void
screentrace_continue(void *context, bool cancel)
{
    screentrace_t *st = (screentrace_t *)context;
    int srv;

    if (cancel) {
	vtrace("Toggle(ScreenTrace) canceled\n");
	Free(st);
	return;
    }

    srv = fprint_screen_start(screentracef, st->ptype,
	    st->opts | FPS_DIALOG_COMPLETE,
	    st->caption, screentrace_name, &screentrace_fps, NULL);
    Free(st);
    if (FPS_IS_ERROR(srv)) {
	if (srv == FPS_STATUS_ERROR) {
	    popup_an_error("Screen trace start failed");
	} else if (srv == FPS_STATUS_CANCEL) {
	    vtrace("Screen trace canceled.\n");
	}
	fclose(screentracef);
	screentracef = NULL;
	return;
    }
    if (srv == FPS_STATUS_WAIT) {
	assert(srv != FPS_STATUS_WAIT);
	return;
    }

    /* We're really tracing, turn the flag on. */
    set_toggle(SCREEN_TRACE, true);
    menubar_retoggle(SCREEN_TRACE);
    vstatus_screentrace((screentrace_count = 0));
}

/*
 * Extract the type from a filename suffix.
 */
static ptype_t
type_from_file(const char *filename)
{
    size_t sl = strlen(filename);

    /* Infer the type from the suffix. */
    if ((sl > 5 && !strcasecmp(filename + sl - 5, ".html")) ||
	(sl > 4 && !strcasecmp(filename + sl - 4, ".htm"))) {
	return P_HTML;
    } else if (sl > 4 && !strcasecmp(filename + sl - 4, ".rtf")) {
	return P_RTF;
    } else {
	return P_NONE;
    }
}

#if !defined(_WIN32) /*[*/
/* Abort screen tracing because the printer process failed. */
static void
screentrace_abort(void)
{
    if (toggled(SCREEN_TRACE)) {
	vtrace("Turning off screen tracing due to print failure\n");
	do_toggle(SCREEN_TRACE);
    }
}
#endif /*]*/

/*
 * Begin screen tracing.
 * Returns true for success, false for failure.
 */
static bool
screentrace_go(tss_t target, ptype_t ptype, unsigned opts, char *tfn)
{
    char *xtfn = NULL;
    int srv;
    char *caption = NULL;
    unsigned full_opts;
    screentrace_t *st;

#if defined(_WIN32) /*[*/
    /*
     * If using the printer, but the printer name is a directory, switch to
     * target FILE, type TEXT, and print to a file in that directory.
     *
     * This allows pr3287, screen tracing and screen printing to print text
     * to files by setting printer.name to a directory name.
     */
    if (target == TSS_PRINTER && ptype == P_GDI) {
	char *printer_name = tfn;
	struct stat buf;

	if (printer_name == NULL) {
	    printer_name = screentrace_default_printer();
	}
	if (printer_name[0] &&
		stat(printer_name, &buf) == 0 &&
		(buf.st_mode & S_IFMT) == S_IFDIR) {
	    target = TSS_FILE;
	    ptype = P_TEXT;
	    opts |= FPS_NO_DIALOG;
	    tfn = print_file_name(printer_name);
	}
    }
#endif /*]*/

    if (target == TSS_FILE) {
	xtfn = do_subst(tfn, DS_VARS | DS_TILDE | DS_UNIQUE);
	screentracef = fopen(xtfn, "a");
	if (ptype == P_NONE) {
	    if (screentrace_default.ptype != P_NONE) {
		ptype = screentrace_default.ptype;
	    } else {
		ptype_t t = type_from_file(xtfn);

		if (t != P_NONE) {
		    ptype = t;
		} else {
		    ptype = P_TEXT;
		}
	    }
	}
    } else {
	/* Printer. */
#if !defined(_WIN32) /*[*/
	char *pct_e;

	if (tfn == NULL) {
	    tfn = screentrace_default_printer();
	}

	/* Do %E% substitution. */
	if ((pct_e = strstr(tfn, "%E%")) != NULL) {
	    xtfn = Asprintf("%.*s%s%s",
		    (int)(pct_e - tfn), tfn,
		    programname,
		    pct_e + 3);
	} else {
	    xtfn = NewString(tfn);
	}

	screentracef = printer_open(xtfn, screentrace_abort);
#else /*][*/
	int fd;

	fd = win_mkstemp(&screentrace_tmpfn, ptype);
	if (fd < 0) {
	    popup_an_errno(errno, "%s", "(temporary file)");
	    Free(tfn);
	    return false;
	}
	screentracef = fdopen(fd, (ptype == P_GDI)? "wb+": "w");
#endif /*]*/
	if (ptype == P_NONE) {
#if !defined(_WIN32) /*[*/
	    ptype = P_TEXT;
#else /*][*/
	    ptype = P_GDI;
#endif /*]*/
	}
    }
    if (screentracef == NULL) {
	if (target == TSS_FILE) {
	    popup_an_errno(errno, "%s", xtfn);
	} else {
#if !defined(_WIN32) /*[*/
	    popup_an_errno(errno, "%s", xtfn);
#else /*][*/
	    popup_an_errno(errno, "%s", "(temporary file)");
#endif /*]*/
	}
	Free(xtfn);
#if defined(_WIN32) /*[*/
	Free(screentrace_tmpfn);
	screentrace_tmpfn = NULL;
#endif /*]*/
	return false;
    }
    Replace(screentrace_name, NewString(xtfn)); /* Leak? */
    Free(tfn);
    SETLINEBUF(screentracef);
#if !defined(_WIN32) /*[*/
    fcntl(fileno(screentracef), F_SETFD, 1);
#endif /*]*/
    st = (screentrace_t *)Calloc(1, sizeof(screentrace_t));
    caption = default_caption();
    full_opts = opts | ((target == TSS_PRINTER)? FPS_FF_SEP: 0);
    srv = fprint_screen_start(screentracef, ptype, full_opts,
	    caption, screentrace_name, &screentrace_fps, st);
    if (FPS_IS_ERROR(srv)) {
	if (srv == FPS_STATUS_ERROR) {
	    popup_an_error("Screen trace start failed");
	} else if (srv == FPS_STATUS_CANCEL) {
	    popup_an_error("Screen trace canceled");
	}
	fclose(screentracef);
	screentracef = NULL;
	Free(st);
	return false;
    }
    if (srv == FPS_STATUS_WAIT) {
	/* Asynchronous. */
	st->ptype = ptype;
	st->opts = full_opts;
	st->caption = caption;
	task_xwait(st, screentrace_continue, "printing");
	return false; /* for now */
    }

    /* We're really tracing, turn the flag on. */
    set_toggle(SCREEN_TRACE, true);
    menubar_retoggle(SCREEN_TRACE);
    return true;
}

/* End the screen trace. */
static void
end_screentrace(bool is_final _is_unused)
{
    fprint_screen_done(&screentrace_fps);
    fclose(screentracef);
    screentracef = NULL;

#if defined(_WIN32) /*[*/
    vtrace("Cleaning up screenTrace\n");
    if (screentrace_current.target == TSS_PRINTER) {
	/* Get rid of the temp file. */
	unlink(screentrace_tmpfn);
    }
#endif /*]*/
}

void
trace_set_screentrace_file(tss_t target, ptype_t ptype, unsigned opts,
	const char *name)
{
    screentrace_resource_setup();
    screentrace_current.target = target;
    screentrace_current.ptype = ptype;
    screentrace_current.opts = opts;
    Replace(onetime_screentrace_name, name? NewString(name): NULL);
}

tss_t
trace_get_screentrace_target(void)
{
    screentrace_resource_setup();
    return screentrace_current.target;
}

ptype_t
trace_get_screentrace_type(void)
{
    screentrace_resource_setup();
    return screentrace_current.ptype;
}

const char *
trace_get_screentrace_name(void)
{
    return (screentrace_name && screentrace_name[0])? screentrace_name:
	"(system default)";
}

/* Return the default filename for screen tracing. */
char *
screentrace_default_file(ptype_t ptype)
{
    const char *suffix;

    switch (ptype) {
    default:
    case P_TEXT:
	suffix = "txt";
	break;
    case P_HTML:
	suffix = "html";
	break;
    case P_RTF:
	suffix = "rtf";
	break;
    }
#if defined(_WIN32) /*[*/
    return Asprintf("%s%sx3scr.$UNIQUE.%s",
	    appres.trace_dir? appres.trace_dir: default_trace_dir(),
	    appres.trace_dir? "\\": "",
	    suffix);
#else /*][*/
    return Asprintf("%s/x3scr.$UNIQUE.%s", appres.trace_dir, suffix);
#endif /*]*/
}

/* Return the default printer for screen tracing. */
char *
screentrace_default_printer(void)
{
    char *name;

#if !defined(_WIN32) /*[*/
    name = get_resource(ResPrintTextCommand);
    if (name == NULL) {
	name = "lpr";
    }
#else /*][*/
    name = get_resource(ResPrinterName);
    if (name == NULL) {
	name = "";
    }
#endif /*]*/
    return NewString(name);
}

/* Set up screen tracing resources. */
void
screentrace_resource_setup(void)
{
    static bool done = false;

    if (done) {
	return;
    }
    done = true;

    screentrace_default.ptype = P_NONE;
    if (appres.screentrace.type != NULL) {
	if (!strcasecmp(appres.screentrace.type, "text")) {
	    screentrace_default.ptype = P_TEXT;
	} else if (!strcasecmp(appres.screentrace.type, "html")) {
	    screentrace_default.ptype = P_HTML;
	} else if (!strcasecmp(appres.screentrace.type, "rtf")) {
	    screentrace_default.ptype = P_RTF;
	} else {
	    xs_warning("Unknown %s: %s", ResScreenTraceType,
		    appres.screentrace.type);
	}
    }

    if (appres.screentrace.target != NULL) {
	if (!strcasecmp(appres.screentrace.target, "file")) {
	    screentrace_default.target = TSS_FILE;
	} else if (!strcasecmp(appres.screentrace.target, "printer")) {
	    screentrace_default.target = TSS_PRINTER;
	} else {
	    xs_warning("Unknown %s: %s", ResScreenTraceTarget,
		    appres.screentrace.target);
	}
    }

    screentrace_default.opts = appres.interactive.print_dialog? 0:
	FPS_NO_DIALOG;

    screentrace_current = screentrace_default; /* struct copy */
}

/*
 * Turn screen tracing on or off.
 *
 * If turning it on, screentrace_current.target contains TSS_FILE or
 * TSS_PRINTER, and screentrace_name is NULL (use the default) or the name of
 * a file, printer command (Unix) or printer (Windows).
 */
static void
toggle_screenTrace(toggle_index_t ix _is_unused, enum toggle_type tt)
{
    char *tracefile_buf = NULL;
    char *tracefile;

    if (toggled(SCREEN_TRACE)) {
	/* Turn it on. */
	screentrace_resource_setup();
	vstatus_screentrace((screentrace_count = 0));
	if (onetime_screentrace_name != NULL) {
	    tracefile = tracefile_buf = onetime_screentrace_name;
	    onetime_screentrace_name = NULL;
	} else if (screentrace_current.target == TSS_FILE &&
		appres.screentrace.file != NULL) {
	    tracefile = appres.screentrace.file;
	} else {
	    if (screentrace_current.target == TSS_FILE) {
		tracefile = tracefile_buf =
		    screentrace_default_file(screentrace_current.ptype);
	    } else {
		tracefile = tracefile_buf = screentrace_default_printer();
	    }
	}
	if (!screentrace_go(screentrace_current.target,
		    screentrace_current.ptype,
		    screentrace_current.opts, NewString(tracefile))) {

	    set_toggle(SCREEN_TRACE, false);
	    vstatus_screentrace((screentrace_count = -1));
	}
    } else {
	/* Turn it off. */
	if (ctlr_any_data() && !trace_skipping) {
	    do_screentrace(false);
	}
	end_screentrace(tt == TT_FINAL);
	screentrace_current = screentrace_default; /* struct copy */
	vstatus_screentrace((screentrace_count = -1));
    }

    if (tracefile_buf != NULL) {
	Free(tracefile_buf);
    }

    trace_gui_toggle();
}

static bool
screentrace_show(bool as_info)
{
    char *message;

    if (toggled(SCREEN_TRACE)) {
	message = Asprintf("Screen tracing is enabled, %s: %s.",
		(screentrace_current.target == TSS_FILE)? "file":
#if !defined(_WIN32) /*[*/
		"with print command",
#else /*]*/
		"to printer",
#endif /*]*/
		trace_get_screentrace_name());
    } else {
	message = NewString("Screen tracing is disabled.");
    }
    if (as_info) {
	popup_an_info("%s", message);
    } else {
	action_output("%s", message);
    }
    Free(message);
    return true;
}

/*
 * Turn screen tracing off.
 */
static bool
screentrace_off(bool as_info)
{
    tss_t target;
    const char *name = NULL;
    char *message;

    if (!toggled(SCREEN_TRACE)) {
	popup_an_error("Screen tracing is already disabled.");
	return false;
    }

    /* Get the current parameters and turn it off. */
    target = screentrace_current.target;
    if (target == TSS_FILE) {
	name = txdFree(NewString(trace_get_screentrace_name()));
    }
    do_toggle(SCREEN_TRACE);

    /* Display what it was. */
    if (target == TSS_FILE) {
	message = Asprintf("Screen tracing complete. Trace file is %s.", name);
    } else {
	message = NewString("Screen tracing to printer complete.");
    }
    if (as_info) {
	popup_an_info("%s", message);
    } else {
	action_output("%s", message);
    }
    Free(message);

    return true;
}

/* Keyword masks. */
#define STK_ON		0x1
#define STK_OFF		0x2
#define STK_INFO	0x4
#define STK_FILE	0x8
#define STK_PRINTER	0x10
#define STK_TEXT	0x20
#define STK_HTML	0x40
#define STK_RTF		0x80
#define STK_GDI		0x100
#define STK_DIALOG	0x200
#define STK_NODIALOG	0x400
#define STK_WORDPAD	0x800
#define STK_NAME	0x1000

#define STK_TYPES	(STK_TEXT | STK_HTML | STK_RTF)
#define STK_FILE_SET	(STK_FILE | STK_TYPES)
#define STK_PRINTER_SET (STK_PRINTER | STK_GDI | STK_DIALOG | STK_NODIALOG | \
			    STK_WORDPAD)
#define STK_WINDOWS	(STK_GDI | STK_DIALOG | STK_NODIALOG | STK_WORDPAD)

/* Keyword database. */
typedef struct {
    const char *keyword;
    unsigned mask;
    unsigned mutex;
} stk_t;
stk_t stk[] = {
    { KwOn,	STK_ON,		STK_ON | STK_OFF },
    { KwOff,	STK_OFF,	STK_OFF | STK_ON | STK_FILE_SET |
				    STK_PRINTER_SET },
    { KwInfo,	STK_INFO,	STK_INFO },
    { KwFile,	STK_FILE,	STK_FILE | STK_OFF | STK_PRINTER_SET },
    { KwPrinter, STK_PRINTER,	STK_PRINTER | STK_OFF | STK_FILE_SET },
    { KwText,	STK_TEXT,	STK_TEXT | STK_OFF | STK_TYPES |
				    STK_PRINTER_SET },
    { KwHtml,	STK_HTML,	STK_HTML | STK_OFF | STK_TYPES |
				    STK_PRINTER_SET },
    { KwRtf,	STK_RTF,	STK_RTF | STK_OFF | STK_TYPES |
				    STK_PRINTER_SET },
    { KwGdi,	STK_GDI,	STK_GDI | STK_OFF | STK_FILE_SET },
    { KwDialog,	STK_DIALOG,	STK_DIALOG | STK_OFF | STK_NODIALOG |
				    STK_FILE_SET },
    { KwNoDialog, STK_NODIALOG,	STK_NODIALOG | STK_OFF | STK_DIALOG |
				    STK_FILE_SET },
    { KwWordPad, STK_WORDPAD,	STK_WORDPAD | STK_OFF | STK_FILE_SET },
    { "(name)",	STK_NAME,	STK_NAME | STK_OFF },
    { NULL, 0, 0 },
};

/* Return the first keyword present in the mask. */
const char *
stk_name(unsigned mask)
{
    int i;

    for (i = 0; stk[i].keyword != NULL; i++) {
	if (mask & stk[i].mask) {
	    return stk[i].keyword;
	}
    }
    return "(none)";
}

/*
 * ScreenTrace()
 * ScreenTrace(On[,Info])
 * ScreenTrace(On[,Info],filename)			 backwards-compatible
 * ScreenTrace(On[,Info],File[,Text|Html|Rtf],filename)	 preferred
 * ScreenTrace(On[,Info],Printer)
 * ScreenTrace(On[,Info],Printer,"print command")	 Unix
 * ScreenTrace(On[,Info],Printer[,Gdi[,Dialog|NoDialog]],printername) Windows
 * ScreenTrace(Off[,Info])
 */
static bool
ScreenTrace_action(ia_t ia, unsigned argc, const char **argv)
{
    tss_t target = TSS_FILE;
    ptype_t ptype = P_NONE;
    const char *name = NULL;
    unsigned i;
    int kx;
    unsigned opts = screentrace_default.opts;
    bool as_info = false;
    unsigned kw_mask = 0;

    action_debug(AnScreenTrace, ia, argc, argv);

    screentrace_resource_setup();

    if (argc == 0) {
	/* Display current status. */
	return screentrace_show(as_info);
    }

    /* Parse the arguments. */
    for (i = 0; i < argc; i++) {
	for (kx = 0; stk[kx].keyword != NULL; kx++) {
	    if ((stk[kx].mask != STK_NAME &&
			!strcasecmp(argv[i], stk[kx].keyword))
		    || (i == argc - 1 && stk[kx].mask == STK_NAME)) {
		unsigned bad_match = kw_mask & stk[kx].mutex;

		if (bad_match) {
		    popup_an_error(AnScreenTrace "(): Keyword conflict (%s, %s)",
			    stk_name(bad_match), stk[kx].keyword);
		    return false;
		}

#if !defined(_WIN32) /*[*/
		if (stk[kx].mask & STK_WINDOWS) {
		    popup_an_error(AnScreenTrace "(): %s is for Windows only",
			    stk[kx].keyword);
		    return false;
		}
#endif /*]*/

		kw_mask |= stk[kx].mask;
		if (stk[kx].mask == STK_NAME) {
		    name = argv[i];
		}

		break;
	    }
	}
	if (stk[kx].keyword == NULL) {
	    popup_an_error(AnScreenTrace "(): Syntax error");
	    return false;
	}
    }

    /* Sort them out. Conflicts have already been caught. */
    if (kw_mask & STK_INFO) {
	as_info = true;
    }
    if (kw_mask & STK_OFF) {
	return screentrace_off(as_info);
    }
    if (kw_mask & (STK_PRINTER | STK_GDI | STK_DIALOG | STK_NODIALOG)) {
	/* Send to printer. */
	if (kw_mask & STK_WORDPAD) {
	    popup_an_error(AnScreenTrace "(): WordPad printing is not supported");
	    return false;
	}
	target = TSS_PRINTER;
#if !defined(_WIN32) /*[*/
	ptype = P_TEXT;
#else /*][*/
	ptype = P_GDI;
#endif /*]*/
	if (kw_mask & STK_DIALOG) {
	    opts &= ~FPS_NO_DIALOG;
	} else if (kw_mask & STK_NODIALOG) {
	    opts |= FPS_NO_DIALOG;
	}
	if (name == NULL) {
#if !defined(_WIN32) /*[*/
	    name = get_resource(ResPrintTextCommand);
#else /*][*/
	    name = get_resource(ResPrinterName);
#endif /*]*/
	}
    } else {
	/* Send to a file. */
	target = TSS_FILE;
	if (kw_mask & STK_TEXT) {
	    ptype = P_TEXT;
	} else if (kw_mask & STK_HTML) {
	    ptype = P_HTML;
	} else if (kw_mask & STK_RTF) {
	    ptype = P_RTF;
	} else if (screentrace_default.ptype == P_NONE && name != NULL) {
	    ptype_t t = type_from_file(name);

	    if (t != P_NONE) {
		ptype = t;
	    }
	}
	if (ptype == P_NONE) {
	    ptype = (screentrace_default.ptype != P_NONE)?
		screentrace_default.ptype: P_TEXT;
	}
    }

    if (toggled(SCREEN_TRACE)) {
	popup_an_error(AnScreenTrace "(): Screen tracing is already enabled.");
	return false;
    }

    /* Attempt to turn on tracing. */
    trace_set_screentrace_file(target, ptype, opts, name);
    do_toggle(SCREEN_TRACE);

    if (!toggled(SCREEN_TRACE)) {
	/* Failed to turn it on. */
	return false;
    }

    /* Display the result. */
    return screentrace_show(as_info);
}

/**
 * Screentrace module registration.
 */
void
screentrace_register(void)
{
    static toggle_register_t toggles[] = {
	{ SCREEN_TRACE,
	  toggle_screenTrace,
	  TOGGLE_NEED_INIT | TOGGLE_NEED_CLEANUP }
    };
    static action_table_t actions[] = {
	{ AnScreenTrace,        ScreenTrace_action,     ACTION_KE }
    };

    /* Register the toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register the actions. */
    register_actions(actions, array_count(actions));
}
