/*
 * Copyright (c) 1993-2016, 2018-2020 Paul Mattes.
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
#include "child.h"
#include "ctlrc.h"
#include "find_console.h"
#include "fprint_screen.h"
#include "lazya.h"
#include "menubar.h"
#include "nvt.h"
#include "popups.h"
#include "print_screen.h"
#include "product.h"
#include "resources.h"
#include "save.h"
#include "status.h"
#include "task.h"
#include "telnet.h"
#include "telnet_core.h"
#include "toggles.h"
#include "trace.h"
#include "trace_gui.h"
#include "utf8.h"
#include "utils.h"
#if defined(_WIN32) /*[*/
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
static screentrace_opts_t screentrace_last = {
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
	status_screentrace(++screentrace_count);
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
    status_screentrace((screentrace_count = 0));
}

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

    if (target == TSS_FILE) {
	xtfn = do_subst(tfn, DS_VARS | DS_TILDE | DS_UNIQUE);
	screentracef = fopen(xtfn, "a");
    } else {
	/* Printer. */
#if !defined(_WIN32) /*[*/
	screentracef = popen(tfn, "w");
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
    }
    if (screentracef == NULL) {
	if (target == TSS_FILE) {
	    popup_an_errno(errno, "%s", xtfn);
	} else {
#if !defined(_WIN32) /*[*/
	    popup_an_errno(errno, "%s", tfn);
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
    if (target == TSS_FILE) {
	Replace(screentrace_name, NewString(xtfn));
    } else {
	Replace(screentrace_name, NewString(tfn));
    }
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

tss_t
trace_get_screentrace_last_target(void)
{
    screentrace_resource_setup();
    return screentrace_last.target;
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
    return xs_buffer("%s%sx3scr.$UNIQUE.%s",
	    appres.trace_dir? appres.trace_dir: default_trace_dir(),
	    appres.trace_dir? "\\": "",
	    suffix);
#else /*][*/
    return xs_buffer("%s/x3scr.$UNIQUE.%s", appres.trace_dir, suffix);
#endif /*]*/
}

/* Return the default printer for screen tracing. */
char *
screentrace_default_printer(void)
{
#if defined(_WIN32) /*[*/
    return NewString("");
#else /*][*/
    return NewString("lpr");
#endif /*]*/
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

    if (appres.screentrace.type != NULL) {
	if (!strcasecmp(appres.screentrace.type, "text")) {
	    screentrace_default.ptype = P_TEXT;
	} else if (!strcasecmp(appres.screentrace.type, "html")) {
	    screentrace_default.ptype = P_HTML;
	} else if (!strcasecmp(appres.screentrace.type, "rtf")) {
	    screentrace_default.ptype = P_RTF;
#if defined(_WIN32) /*]*/
	} else if (!strcasecmp(appres.screentrace.type, "gdi")) {
	    screentrace_default.ptype = P_GDI;
#endif /*]*/
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
	status_screentrace((screentrace_count = 0));
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
	    status_screentrace((screentrace_count = -1));
	}
    } else {
	/* Turn it off. */
	if (ctlr_any_data() && !trace_skipping) {
	    do_screentrace(false);
	}
	end_screentrace(tt == TT_FINAL);
	screentrace_last = screentrace_current; /* struct copy */
	screentrace_current = screentrace_default; /* struct copy */
	status_screentrace((screentrace_count = -1));
    }

    if (tracefile_buf != NULL) {
	Free(tracefile_buf);
    }

    trace_gui_toggle();
}

/*
 * ScreenTrace(On)
 * ScreenTrace(On,filename)			 backwards-compatible
 * ScreenTrace(On,File[,Text|Html|Rtf],filename)	 preferred
 * ScreenTrace(On,Printer)
 * ScreenTrace(On,Printer,"print command")	 Unix
 * ScreenTrace(On,Printer[,Gdi[,Dialog|NoDialog]],printername) Windows
 * ScreenTrace(Off)
 */
static bool
ScreenTrace_action(ia_t ia, unsigned argc, const char **argv)
{
    bool on = false;
    tss_t how = TSS_FILE;
    ptype_t ptype = P_TEXT;
    const char *name = NULL;
    unsigned px;
    unsigned opts = screentrace_default.opts;

    action_debug("ScreenTrace", ia, argc, argv);

    screentrace_resource_setup();
    ptype = screentrace_default.ptype;

    if (argc == 0) {
	how = trace_get_screentrace_target();
	if (toggled(SCREEN_TRACE)) {
	    action_output("Screen tracing is enabled, %s \"%s\".",
		    (how == TSS_FILE)? "file":
#if !defined(_WIN32) /*[*/
		    "with print command",
#else /*]*/
		    "to printer",
#endif /*]*/
		    trace_get_screentrace_name());
	} else {
	    action_output("Screen tracing is disabled.");
	}
	return true;
    }

    if (!strcasecmp(argv[0], "Off")) {
	if (!toggled(SCREEN_TRACE)) {
	    popup_an_error("Screen tracing is already disabled.");
	    return false;
	}
	on = false;
	if (argc > 1) {
	    popup_an_error("ScreenTrace(): Too many arguments for 'Off'");
	    return false;
	}
	goto toggle_it;
    }
    if (strcasecmp(argv[0], "On")) {
	popup_an_error("ScreenTrace(): Must be 'On' or 'Off'");
	return false;
    }

    /* Process 'On'. */
    if (toggled(SCREEN_TRACE)) {
	popup_an_error("Screen tracing is already enabled.");
	return true;
    }

    on = true;
    px = 1;

    if (px >= argc) {
	/*
	 * No more parameters. Trace to a file, and generate the name.
	 */
	goto toggle_it;
    }
    if (!strcasecmp(argv[px], "File")) {
	px++;
	if (px < argc && !strcasecmp(argv[px], "Text")) {
	    ptype = P_TEXT;
	    px++;
	} else if (px < argc && !strcasecmp(argv[px], "Html")) {
	    ptype = P_HTML;
	    px++;
	} else if (px < argc && !strcasecmp(argv[px], "Rtf")) {
	    ptype = P_RTF;
	    px++;
	}
    } else if (!strcasecmp(argv[px], "Printer") 
#if defined(WIN32) /*[*/
	    || strcasecmp(argv[px], "Gdi")
#endif /*]*/
	    ) {
	px++;
	how = TSS_PRINTER;
#if defined(WIN32) /*[*/
	ptype = P_GDI;
	if (px < argc && !strcasecmp(argv[px], "Gdi")) {
	    px++;
	}
#endif /*]*/
	if (px < argc && !strcasecmp(argv[px], "Dialog")) {
	    px++;
	    opts &= ~FPS_NO_DIALOG;
	}
	if (px < argc && !strcasecmp(argv[px], "NoDialog")) {
	    px++;
	    opts |= FPS_NO_DIALOG;
	}
    }
    if (px < argc) {
	name = argv[px];
	px++;
    }
    if (px < argc) {
	popup_an_error("ScreenTrace(): Too many arguments.");
	return false;
    }
    if (how == TSS_PRINTER && name == NULL) {
#if !defined(_WIN32) /*[*/
	name = get_resource(ResPrintTextCommand);
#else /*][*/
	name = get_resource(ResPrinterName);
#endif /*]*/
    }

toggle_it:
    if ((on && !toggled(SCREEN_TRACE)) || (!on && toggled(SCREEN_TRACE))) {
	if (on) {
	    trace_set_screentrace_file(how, ptype, opts, name);
	}
	do_toggle(SCREEN_TRACE);
    }
    if (on && !toggled(SCREEN_TRACE)) {
	return true;
    }

    name = trace_get_screentrace_name();
    if (name != NULL) {
	if (on) {
	    if (how == TSS_FILE) {
		if (ia_cause == IA_COMMAND) {
		    action_output("Trace file is %s.", name);
		} else {
		    popup_an_info("Trace file is %s.", name);
		}
	    } else {
		if (ia_cause == IA_COMMAND) {
		    action_output("Tracing to printer.");
		} else {
		    popup_an_info("Tracing to printer.");
		}
	    }
	} else {
	    if (trace_get_screentrace_last_target() == TSS_FILE) {
		if (ia_cause == IA_COMMAND) {
		    action_output("Tracing complete. Trace file is %s.", name);
		} else {
		    popup_an_info("Tracing complete. Trace file is %s.", name);
		}
	    } else {
		if (ia_cause == IA_COMMAND) {
		    action_output("Tracing to printer complete.");
		} else {
		    popup_an_info("Tracing to printer complete.");
		}
	    }
	}
    }
    return true;
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
	{ "ScreenTrace",        ScreenTrace_action,     ACTION_KE }
    };

    /* Register the toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register the actions. */
    register_actions(actions, array_count(actions));
}
