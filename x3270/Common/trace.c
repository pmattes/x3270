/*
 * Copyright (c) 1993-2015 Paul Mattes.
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
 *	trace.c
 *		3270 data stream tracing.
 *
 */

#include "globals.h"

#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include "3270ds.h"
#include "appres.h"
#include "objects.h"
#include "resources.h"
#include "ctlr.h"

#include "charsetc.h"
#include "childc.h"
#include "ctlrc.h"
#include "fprint_screenc.h"
#include "lazya.h"
#include "menubar.h"
#include "nvtc.h"
#include "popupsc.h"
#include "print_screen.h"
#include "save.h"
#include "statusc.h"
#include "telnetc.h"
#include "toggles.h"
#include "trace.h"
#include "trace_gui.h"
#include "utf8.h"
#include "util.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "winprint.h"
#endif /*]*/

/* Size of the data stream trace buffer. */
#define TRACE_DS_BUFSIZE	(4*1024)

/* Wrap column for data stream tracing. */
#define TRACE_DS_WRAP		75

/* Maximum size of a tracefile header. */
#define MAX_HEADER_SIZE		(32*1024)

/* Minimum size of a trace file. */
#define MIN_TRACEFILE_SIZE	(64*1024)
#define MIN_TRACEFILE_SIZE_NAME	"64K"

/* System calls which may not be there. */
#if !defined(HAVE_FSEEKO) /*[*/
#define fseeko(s, o, w)	fseek(s, (long)o, w)
#define ftello(s)	(off_t)ftell(s)
#endif /*]*/

/* Statics */
static int      dscnt = 0;
#if !defined(_WIN32) /*[*/
static int      tracewindow_pid = -1;
#else /*][*/
static HANDLE	tracewindow_handle = NULL;
#endif /*]*/
static FILE    *tracef = NULL;
static FILE    *tracef_pipe = NULL;
static char    *tracef_bufptr = NULL;
static off_t	tracef_size = 0;
static off_t	tracef_max = 0;
static char    *onetime_tracefile_name = NULL;
static tss_t	screentrace_how = TSS_FILE;
static ptype_t	screentrace_ptype = P_TEXT;
static tss_t	screentrace_last_how = TSS_FILE;
static char    *onetime_screentrace_name = NULL;
static void	vwtrace(Boolean do_ts, const char *fmt, va_list args);
static void	wtrace(Boolean do_ts, const char *fmt, ...);
static char    *create_tracefile_header(const char *mode);
static void	stop_tracing(void);
static char    *screentrace_name = NULL;
#if defined(_WIN32) /*[*/
static char    *screentrace_tmpfn;
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
static int 	screentrace_count;
#endif /*]*/

#if defined(_WIN32) /*[*/
# if defined(WC3270) /*[*/
#  define DEFAULT_TRACE_DIR	(mydesktop? mydesktop: myappdata)
# else /*][*/
#  define DEFAULT_TRACE_DIR	myappdata
# endif /*]*/
#endif /*]*/

/* Globals */
Boolean          trace_skipping = False;
char		*tracefile_name = NULL;

/* Statics */
static Boolean 	 wrote_ts = False;

/* display a (row,col) */
const char *
rcba(int baddr)
{
    return lazyaf("(%d,%d)", baddr/COLS + 1, baddr%COLS + 1);
}

/* Data Stream trace print, handles line wraps */

/*
 * This function is careful to do line breaks based on wchar_t's, not
 * bytes, so multi-byte characters are traced properly.
 * However, it doesn't know that DBCS characters are two columns wide, so it
 * will get those wrong and break too late.  To get that right, it needs some
 * sort of function to tell it that a wchar_t is double-width, which we lack at
 * the moment.
 *
 * If wchar_t's are Unicode, it could perhaps use some sort of heuristic based
 * on which plane the character is in.
 */
static void
trace_ds_s(char *s, Boolean can_break)
{
    int len = strlen(s);
    int len0 = len + 1;
    int wlen;
    Boolean nl = False;
    wchar_t *w_buf;		/* wchar_t translation of s */
    wchar_t *w_cur;		/* current wchar_t pointer */
    wchar_t *w_chunk;		/* transient wchar_t buffer */
    char *mb_chunk;		/* transient multibyte buffer */

    if (!toggled(TRACING) || tracef == NULL || !len) {
	return;
    }

    /* Allocate buffers for chunks of output data. */
    mb_chunk = Malloc(len0);
    w_chunk = (wchar_t *)Malloc(len0 * sizeof(wchar_t));

    /* Convert the input string to wchar_t's. */
    w_buf = (wchar_t *)Malloc(len0 * sizeof(wchar_t));
    wlen = mbstowcs(w_buf, s, len);
    if (wlen < 0) {
	Error("trace_ds_s: mbstowcs failed");
    }
    w_cur = w_buf;

    /* Check for a trailing newline. */
    if (len && s[len-1] == '\n') {
	wlen--;
	nl = True;
    }

    if (!can_break && dscnt + wlen >= TRACE_DS_WRAP) {
	wtrace(False, "...\n... ");
	dscnt = 0;
    }

    while (dscnt + wlen >= TRACE_DS_WRAP) {
	int plen = TRACE_DS_WRAP - dscnt;
	int mblen;

	if (plen) {
	    memcpy(w_chunk, w_cur, plen * sizeof(wchar_t));
	    w_chunk[plen] = 0;
	    mblen = wcstombs(mb_chunk, w_chunk, len0);
	    if (mblen <= 0) {
		Error("trace_ds_s: wcstombs 1 failed");
	    }
	} else {
	    mb_chunk[0] = '\0';
	    mblen = 0;
	}

	wtrace(False, "%.*s ...\n... ", mblen, mb_chunk);
	dscnt = 4;
	w_cur += plen;
	wlen -= plen;
    }
    if (wlen) {
	int mblen;

	memcpy(w_chunk, w_cur, wlen * sizeof(wchar_t));
	w_chunk[wlen] = 0;
	mblen = wcstombs(mb_chunk, w_chunk, len0);
	if (mblen <= 0)
	    Error("trace_ds_s: wcstombs 2 failed");
	wtrace(False, "%.*s", mblen, mb_chunk);
	dscnt += wlen;
    }
    if (nl) {
	wtrace(False, "\n");
	dscnt = 0;
    }
    Free(mb_chunk);
    Free(w_buf);
    Free(w_chunk);
}

/*
 * External interface to data stream tracing -- no timestamps, automatic line
 * wraps.
 */
void
trace_ds(const char *fmt, ...)
{
    va_list args;
    char *s;

    if (!toggled(TRACING) || tracef == NULL) {
	return;
    }

    /* print out remainder of message */
    va_start(args, fmt);
    s = xs_vbuffer(fmt, args);
    va_end(args);
    trace_ds_s(s, True);
    Free(s);
}

/* Conditional event trace. */
void
vtrace(const char *fmt, ...)
{
    va_list args;

    if (!toggled(TRACING) || tracef == NULL) {
	return;
    }

    /* print out message */
    va_start(args, fmt);
    vwtrace(True, fmt, args);
    va_end(args);
}

/* Conditional event trace. */
void
ntvtrace(const char *fmt, ...)
{
    va_list args;

    if (!toggled(TRACING) || tracef == NULL) {
	return;
    }

    /* print out message */
    va_start(args, fmt);
    vwtrace(False, fmt, args);
    va_end(args);
}

/*
 * Generate a timestamp for the trace file.
 */
static char *
gen_ts(void)
{
    struct timeval tv;
    time_t t;
    struct tm *tm;

    (void) gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    tm = localtime(&t);
    return lazyaf("%d%02d%02d.%02d%02d%02d.%03d ",
	    tm->tm_year + 1900,
	    tm->tm_mon + 1,
	    tm->tm_mday,
	    tm->tm_hour,
	    tm->tm_min,
	    tm->tm_sec,
	    (int)(tv.tv_usec / 1000L));
}

/*
 * Write to the trace file, varargs style.
 * This is the only function that actually does output to the trace file --
 * all others are wrappers around this function.
 */
static void
vwtrace(Boolean do_ts, const char *fmt, va_list args)
{
    int n2w_left, n2w, nw;
    char *ts;
    char *buf = NULL;
    char *bp;

    /* Ugly hack to write into a memory buffer. */
    if (tracef_bufptr != NULL) {
	if (do_ts) {
	    tracef_bufptr += sprintf(tracef_bufptr, "%s", gen_ts());
	}
	tracef_bufptr += vsprintf(tracef_bufptr, fmt, args);
	return;
    }

    if (tracef == NULL) {
	return;
    }

    ts = NULL;

    buf = xs_vbuffer(fmt, args);
    n2w_left = strlen(buf);
    bp = buf;

    while (n2w_left > 0) {
	char *nl;
	Boolean wrote_nl = False;

	if (do_ts && !wrote_ts) {
	    if (ts == NULL) {
		ts = gen_ts();
	    }
	    (void) fwrite(ts, strlen(ts), 1, tracef);
	    fflush(tracef);
	    if (tracef_pipe != NULL) {
		(void) fwrite(ts, strlen(ts), 1, tracef_pipe);
		fflush(tracef);
	    }
	    wrote_ts = True;
	}

	nl = strchr(bp, '\n');
	if (nl != NULL) {
	    wrote_nl = True;
	    n2w = nl - bp + 1;
	} else {
	    n2w = n2w_left;
	}

	nw = fwrite(bp, n2w, 1, tracef);
	if (nw == 1) {
	    fflush(tracef);
	} else {
	    if (errno != EPIPE
#if defined(EILSEQ) /*[*/
			       && errno != EILSEQ
#endif /*]*/
						 ) {
		popup_an_errno(errno, "Write to trace file failed");
	    }
#if defined(EILSEQ) /*[*/
	    if (errno != EILSEQ)
#endif /*]*/
	    {
		stop_tracing();
		goto done;
	    }
	}

	if (tracef_pipe != NULL) {
	    nw = fwrite(bp, n2w, 1, tracef_pipe);
	    if (nw != 1) {
		(void) fclose(tracef_pipe);
		tracef_pipe = NULL;
	    } else {
		fflush(tracef_pipe);
	    }
	}

	if (wrote_nl) {
	    wrote_ts = False;
	}

	bp += n2w;
	n2w_left -= n2w;
    }

    tracef_size = ftello(tracef);

done:
    if (buf != NULL) {
	Free(buf);
    }
    return;
}

/* Write to the trace file. */
static void
wtrace(Boolean do_ts, const char *fmt, ...)
{
    if (tracef != NULL) {
	va_list args;

	va_start(args, fmt);
	vwtrace(do_ts, fmt, args);
	va_end(args);
    }
}

static void
stop_tracing(void)
{
    if (tracef != NULL && tracef != stdout) {
	(void) fclose(tracef);
    }
    tracef = NULL;
    if (tracef_pipe != NULL) {
	(void) fclose(tracef_pipe);
	tracef_pipe = NULL;
    }
    if (toggled(TRACING)) {
	toggle_toggle(TRACING);
	menubar_retoggle(TRACING);
    }
}

/* Check for a trace file rollover event. */
void
trace_rollover_check(void)
{
    if (tracef == NULL || tracef_max == 0) {
	return;
    }

    /* See if we've reached a rollover point. */
    if (tracef_size >= tracef_max) {
	char *alt_filename;
	char *new_header;
#if defined(_WIN32) /*[*/
	char *period;
#endif /*]*/

	/* Close up this file. */
	wtrace(True, "Trace rolled over\n");
	fclose(tracef);
	tracef = NULL;

		/* Unlink and rename the alternate file. */
#if defined(_WIN32) /*[*/
	period = strrchr(tracefile_name, '.');
	if (period != NULL) {
	    alt_filename = xs_buffer("%.*s-%s", period - tracefile_name,
		    tracefile_name, period);
	} else
#endif /*]*/
	{
	    alt_filename = xs_buffer("%s-", tracefile_name);
	}
	(void) unlink(alt_filename);
	(void) rename(tracefile_name, alt_filename);
	Free(alt_filename);
	alt_filename = NULL;
	tracef = fopen(tracefile_name, "w");
	if (tracef == NULL) {
	    popup_an_errno(errno, "%s", tracefile_name);
	    return;
	}

	/* Initialize it. */
	tracef_size = 0L;
	(void) SETLINEBUF(tracef);
	new_header = create_tracefile_header("rolled over");
	wtrace(False, new_header);
	Free(new_header);
    }
}

static int trace_reason;

/* Create a trace file header. */
static char *
create_tracefile_header(const char *mode)
{
    char *buf;
    int i;

    /* Create a buffer and redirect output. */
    buf = Malloc(MAX_HEADER_SIZE);
    tracef_bufptr = buf;

    /* Display current status */
    wtrace(True, "Trace %s\n", mode);
    wtrace(False, " Version: %s\n", build);
    wtrace(False, " %s\n", build_options());
    save_yourself();
    wtrace(False, " Command: %s\n", command_string);
    wtrace(False, " Model %s, %d rows x %d cols", model_name, maxROWS, maxCOLS);
#if defined(X3270_INTERACTIVE) && !defined(_WIN32) /*[*/
    wtrace(False, ", %s display",
	    appres.interactive.mono? "monochrome": "color");
#endif /*]*/
    if (appres.extended) {
	wtrace(False, ", extended data stream");
    }
    wtrace(False, ", %s emulation", appres.m3279 ? "color" : "monochrome");
    wtrace(False, ", %s charset", get_charset_name());
    if (appres.apl_mode) {
	wtrace(False, ", APL mode");
    }
    wtrace(False, "\n");
#if !defined(_WIN32) /*[*/
    wtrace(False, " Locale codeset: %s\n", locale_codeset);
#else /*][*/
    wtrace(False, " ANSI codepage: %d\n", GetACP());
# if defined(_WIN32) /*[*/
    wtrace(False, " Local codepage: %d\n", appres.local_cp);
# endif /*]*/
#endif /*]*/
    wtrace(False, " Host codepage: %d", (int)(cgcsgid & 0xffff));
    if (dbcs) {
	wtrace(False, "+%d", (int)(cgcsgid_dbcs & 0xffff));
    }
    wtrace(False, "\n");
#if defined(_WIN32) && (defined(C3270) || defined(S3270)) /*[*/
    wtrace(False, " AppData: %s\n", myappdata? myappdata: "(null)");
    wtrace(False, " Install dir: %s\n", instdir? instdir: "(null)");
    wtrace(False, " Desktop: %s\n", mydesktop? mydesktop: "(null)");
#endif /*]*/
    wtrace(False, " Toggles:");
    for (i = 0; toggle_names[i].name != NULL; i++) {
	if (toggle_supported(toggle_names[i].index) &&
	    !toggle_names[i].is_alias &&
	    toggled(toggle_names[i].index)) {

	    wtrace(False, " %s", toggle_names[i].name);
	}
    }
    wtrace(False, "\n");

    if (CONNECTED) {
	wtrace(False, " Connected to %s, port %u\n", current_host,
		current_port);
    }

    /* Snap the current TELNET options. */
    if (net_snap_options()) {
	wtrace(False, " TELNET state:\n");
	trace_netdata('<', obuf, obptr - obuf);
    }

    /* Dump the screen contents and modes into the trace file. */
    if (CONNECTED) {
	/*
	 * Note that if the screen is not formatted, we do not
	 * attempt to save what's on it.  However, if we're in
	 * 3270 SSCP-LU or NVT mode, we'll do a dummy, empty
	 * write to ensure that the display is in the right
	 * mode.
	 */
	if (IN_3270) {
	    wtrace(False, " Screen contents (%s3270) %sformatted:\n",
		    IN_E? "TN3270E-": "",
		    formatted? "": "un");
	    obptr = obuf;
	    (void) net_add_dummy_tn3270e();
	    ctlr_snap_buffer();
	    space3270out(2);
	    net_add_eor(obuf, obptr - obuf);
	    obptr += 2;
	    trace_netdata('<', obuf, obptr - obuf);

	    obptr = obuf;
	    if (ctlr_snap_modes()) {
		wtrace(False, " 3270 modes:\n");
		space3270out(2);
		net_add_eor(obuf, obptr - obuf);
		obptr += 2;
		trace_netdata('<', obuf, obptr - obuf);
	    }
	} else if (IN_E) {
	    obptr = obuf;
	    (void) net_add_dummy_tn3270e();
	    wtrace(False, " Screen contents (%s):\n",
		    IN_SSCP? "SSCP-LU": "TN3270E-NVT");
	    if (IN_SSCP) {
		ctlr_snap_buffer_sscp_lu();
	    } else if (IN_NVT) {
		nvt_snap();
	    }
	    space3270out(2);
	    net_add_eor(obuf, obptr - obuf);
	    obptr += 2;
	    trace_netdata('<', obuf, obptr - obuf);
	    if (IN_NVT) {
		wtrace(False, " NVT modes:\n");
		obptr = obuf;
		nvt_snap_modes();
		trace_netdata('<', obuf, obptr - obuf);
	    }
	} else if (IN_NVT) {
	    obptr = obuf;
	    wtrace(False, " Screen contents (NVT):\n");
	    nvt_snap();
	    trace_netdata('<', obuf, obptr - obuf);
	    wtrace(False, " NVT modes:\n");
	    obptr = obuf;
	    nvt_snap_modes();
	    trace_netdata('<', obuf, obptr - obuf);
	}
    }

    wtrace(False, " Data stream:\n");

    /* Return the buffer. */
    tracef_bufptr = NULL;
    return buf;
}

/* Calculate the tracefile maximum size. */
static void
get_tracef_max(void)
{
    static Boolean calculated = False;
    char *ptr;
    Boolean bad = False;

    if (calculated) {
	return;
    }

    calculated = True;

    if (appres.trace_file_size == NULL ||
	!strcmp(appres.trace_file_size, "0") ||
	!strncasecmp(appres.trace_file_size, "none",
		     strlen(appres.trace_file_size))) {
	tracef_max = 0;
	return;
    }

    tracef_max = strtoul(appres.trace_file_size, &ptr, 0);
    if (tracef_max == 0 || ptr == appres.trace_file_size || *(ptr + 1)) {
	bad = True;
    } else switch (*ptr) {
    case 'k':
    case 'K':
	tracef_max *= 1024;
	break;
    case 'm':
    case 'M':
	tracef_max *= 1024 * 1024;
	break;
    case '\0':
	break;
    default:
	bad = True;
	break;
    }

    if (bad) {
	tracef_max = MIN_TRACEFILE_SIZE;
	trace_gui_bad_size(MIN_TRACEFILE_SIZE_NAME);
    } else if (tracef_max < MIN_TRACEFILE_SIZE) {
	tracef_max = MIN_TRACEFILE_SIZE;
    }
}

/* Parse the name '/dev/fd<n>', so we can simulate it. */
static int
get_devfd(const char *pathname)
{
    unsigned long fd;
    char *ptr;

    if (strncmp(pathname, "/dev/fd/", 8)) {
	return -1;
    }
    fd = strtoul(pathname + 8, &ptr, 10);
    if (ptr == pathname + 8 || *ptr != '\0' || fd > INT_MAX) {
	return -1;
    }
    return fd;
}

/* Start tracing, using the specified file. */
void
tracefile_ok(const char *tfn)
{
    int devfd = -1;
#if !defined(_WIN32) /*[*/
    int pipefd[2];
    Boolean just_piped = False;
#endif /*]*/
    char *buf;
    char *stfn;

    stfn = do_subst(tfn, DS_VARS | DS_TILDE | DS_UNIQUE);
    if (strchr(stfn, '\'') ||
	((int)strlen(stfn) > 0 && stfn[strlen(stfn)-1] == '\\')) {
	popup_an_error("Illegal file name: %s", tfn);
	Free(stfn);
	goto done;
    }

    tracef_max = 0;

    if (!strcmp(stfn, "stdout")) {
	tracef = stdout;
    } else {
#if !defined(_WIN32) /*[*/
	FILE *pipefile = NULL;

	if (!strcmp(stfn, "none") || !stfn[0]) {
	    just_piped = True;
	    if (!appres.trace_monitor) {
		popup_an_error("Must specify a trace file name");
		free(stfn);
		goto done;
	    }
	}

	if (appres.trace_monitor) {
	    if (pipe(pipefd) < 0) {
		popup_an_errno(errno, "pipe() failed");
		Free(stfn);
		goto done;
	    }
	    pipefile = fdopen(pipefd[1], "w");
	    if (pipefile == NULL) {
		popup_an_errno(errno, "fdopen() failed");
		(void) close(pipefd[0]);
		(void) close(pipefd[1]);
		Free(stfn);
		goto done;
	    }
	    (void) SETLINEBUF(pipefile);
	    (void) fcntl(pipefd[1], F_SETFD, 1);
	}

	if (just_piped) {
	    tracef = pipefile;
	} else
#endif /*]*/
	{
	    Boolean append = False;

#if !defined(_WIN32) /*[*/
	    tracef_pipe = pipefile;
#endif /*]*/
	    /* Get the trace file maximum. */
	    get_tracef_max();

	    /* Open and configure the file. */
	    if ((devfd = get_devfd(stfn)) >= 0)
		tracef = fdopen(dup(devfd), "a");
	    else if (!strncmp(stfn, ">>", 2)) {
		append = True;
		tracef = fopen(stfn + 2, "a");
	    } else {
		tracef = fopen(stfn, "w");
	    }
	    if (tracef == NULL) {
		popup_an_errno(errno, "%s", stfn);
#if !defined(_WIN32) /*[*/
		fclose(tracef_pipe);
		(void) close(pipefd[0]);
		(void) close(pipefd[1]);
#endif /*]*/
		Free(stfn);
		goto done;
	    }
	    tracef_size = ftello(tracef);
	    Replace(tracefile_name, NewString(append? stfn + 2: stfn));
	    (void) SETLINEBUF(tracef);
#if !defined(_WIN32) /*[*/
	    (void) fcntl(fileno(tracef), F_SETFD, 1);
#endif /*]*/
	}
    }

#if !defined(_WIN32) /*[*/
    /* Start the monitor window */
    if (tracef != stdout && appres.trace_monitor) {
	switch (tracewindow_pid = fork_child()) {
	case 0: {	/* child process */
	    (void) execlp("xterm", "xterm", "-title",
		    just_piped? "trace": stfn,
		    "-sb", "-e", "/bin/sh", "-c",
		    xs_buffer("cat <&%d", pipefd[0]),
		    NULL);
	    (void) perror("exec(xterm) failed");
	    _exit(1);
	    break;
	    }
	default:	/* parent */
	    (void) close(pipefd[0]);
	    ++children;
	    break;
	case -1:	/* error */
	    popup_an_errno(errno, "fork() failed");
	    break;
	}
    }
#endif /*]*/

#if defined(_WIN32) /*[*/
    /* Start the monitor window. */
    if (tracef != stdout && appres.trace_monitor
#if defined(WC3270) /*[*/
	    && is_installed
#endif /*]*/
	    ) {
	STARTUPINFO startupinfo;
	PROCESS_INFORMATION process_information;
	char *path;
	char *args;

	(void) memset(&startupinfo, '\0', sizeof(STARTUPINFO));
	startupinfo.cb = sizeof(STARTUPINFO);
	startupinfo.lpTitle = stfn;
	(void) memset(&process_information, '\0', sizeof(PROCESS_INFORMATION));
	path = xs_buffer("%scatf.exe", instdir);
	args = xs_buffer("\"%scatf.exe\" \"%s\"", instdir, stfn);
	if (CreateProcess(
	    path,
	    args,
	    NULL,
	    NULL,
	    FALSE,
	    CREATE_NEW_CONSOLE,
	    NULL,
	    NULL,
	    &startupinfo,
	    &process_information) == 0) {

	    popup_an_error("CreateProcess(%s) failed: %s",
		    path, win32_strerror(GetLastError()));
	    Free(path);
	    Free(args);
	} else {
	    Free(path);
	    Free(args);
	    tracewindow_handle = process_information.hProcess;
	    CloseHandle(process_information.hThread);
	}
    }
#endif /*]*/

    Free(stfn);

    /* We're really tracing, turn the flag on. */
    set_toggle(trace_reason, True);
    menubar_retoggle(trace_reason);

    /* Display current status. */
    buf = create_tracefile_header("started");
    wtrace(False, "%s", buf);
    Free(buf);
done:
    return;
}

/* Open the trace file. */
static void
tracefile_on(int reason, enum toggle_type tt)
{
    char *tracefile_buf = NULL;
    char *tracefile;

    if (tracef != NULL) {
	return;
    }

    trace_reason = reason;
    if (appres.secure && tt != TT_INITIAL) {
	tracefile_ok("none");
	return;
    }
    if (onetime_tracefile_name != NULL) {
	tracefile = tracefile_buf = onetime_tracefile_name;
	onetime_tracefile_name = NULL;
    } else if (appres.trace_file) {
	tracefile = appres.trace_file;
    } else {
#if defined(_WIN32) /*[*/
	tracefile_buf = xs_buffer("%s%sx3trc.$UNIQUE.txt",
		appres.trace_dir? appres.trace_dir: DEFAULT_TRACE_DIR,
		appres.trace_dir? "\\": "");
#else /*][*/
	tracefile_buf = xs_buffer("%s/x3trc.$UNIQUE", appres.trace_dir);
#endif /*]*/
	tracefile = tracefile_buf;
    }

    if (!trace_gui_on(reason, tt, tracefile)) {
	tracefile_ok(tracefile);
    } else {
	/* Turn the toggle _off_ until the popup succeeds. */
	set_toggle(reason, False);
    }

    if (tracefile_buf != NULL) {
	Free(tracefile_buf);
    }
}

/* Close the trace file. */
static void
tracefile_off(void)
{
    wtrace(True, "Trace stopped\n");
#if !defined(_WIN32) /*[*/
    if (tracewindow_pid != -1) {
	(void) kill(tracewindow_pid, SIGKILL);
	tracewindow_pid = -1;
    }
#else /*][*/
    if (tracewindow_handle != NULL) {
	TerminateProcess(tracewindow_handle, 0);
	CloseHandle(tracewindow_handle);
	tracewindow_handle = NULL;
    }

#endif /*]*/
    stop_tracing();
}

void
trace_set_trace_file(const char *path)
{
    Replace(onetime_tracefile_name, NewString(path));
}

static void
toggle_tracing(toggle_index_t ix _is_unused, enum toggle_type tt)
{
    /* If turning on trace and no trace file, open one. */
    if (toggled(TRACING) && tracef == NULL) {
	tracefile_on(TRACING, tt);
	if (tracef == NULL) {
	    set_toggle(TRACING, False);
	}
    } else if (!toggled(TRACING)) {
	/* If turning off trace and not still tracing events, close the
	   trace file. */
	tracefile_off();
    }
}

/* Screen trace file support. */
static FILE *screentracef = NULL;
static fps_t screentrace_fps = NULL;

/*
 * Screen trace function, called when the host clears the screen.
 */
static void
do_screentrace(Boolean always _is_unused)
{
    /*
     * XXX: We should do something smarter here should fprint_screen_body()
     * fail.
     */
    (void) fprint_screen_body(screentrace_fps);
    status_screentrace(++screentrace_count);
}

void
trace_screen(Boolean is_clear)
{
    trace_skipping = False;

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
    (void) fputc(c, screentracef);
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

    (void) fputc('\n', screentracef);
    for (i = 0; i < COLS; i++) {
	(void) fputc('=', screentracef);
    }
    (void) fputc('\n', screentracef);

    trace_skipping = True;
}

/*
 * Screen tracing callback.
 * Returns True for success, False for failure.
 */
static Boolean
screentrace_cb(tss_t how, ptype_t ptype, char *tfn)
{
	char *xtfn = NULL;
	int srv;

	if (how == TSS_FILE) {
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
			return False;
		}
		screentracef = fdopen(fd, (ptype == P_GDI)? "wb+": "w");
#endif /*]*/
	}
	if (screentracef == NULL) {
		if (how == TSS_FILE)
			popup_an_errno(errno, "%s", xtfn);
		else
#if !defined(_WIN32) /*[*/
			popup_an_errno(errno, "%s", tfn);
#else /*][*/
			popup_an_errno(errno, "%s", "(temporary file)");
#endif /*]*/
		Free(xtfn);
#if defined(_WIN32) /*[*/
		Free(screentrace_tmpfn);
		screentrace_tmpfn = NULL;
#endif /*]*/
		return False;
	}
	if (how == TSS_FILE)
		Replace(screentrace_name, NewString(xtfn));
	else
		Replace(screentrace_name, NewString(tfn));
	Free(tfn);
	(void) SETLINEBUF(screentracef);
#if !defined(_WIN32) /*[*/
	(void) fcntl(fileno(screentracef), F_SETFD, 1);
#endif /*]*/
	srv = fprint_screen_start(screentracef, ptype,
		(how == TSS_PRINTER)? FPS_FF_SEP: 0,
		default_caption(), screentrace_name, &screentrace_fps);
	if (FPS_IS_ERROR(srv)) {
		if (srv == FPS_STATUS_ERROR) {
			popup_an_error("Screen trace start failed.");
		} else if (srv == FPS_STATUS_CANCEL) {
			popup_an_error("Screen trace canceled.");
		}
		fclose(screentracef);
		return False;
	}

	/* We're really tracing, turn the flag on. */
	set_toggle(SCREEN_TRACE, True);
	menubar_retoggle(SCREEN_TRACE);
	return True;
}

/* End the screen trace. */
static void
end_screentrace(Boolean is_final _is_unused)
{
	fprint_screen_done(&screentrace_fps);
	(void) fclose(screentracef);
	screentracef = NULL;

#if defined(_WIN32) /*[*/
	if (screentrace_how == TSS_PRINTER) {
		if (screentrace_ptype == P_RTF) {
			/* Start up WordPad to print the file. */
			if (is_final) {
				start_wordpad_sync("ScreenTrace",
					screentrace_tmpfn, screentrace_name);
			} else {
				start_wordpad_async("ScreenTrace",
					screentrace_tmpfn, screentrace_name);
			}
		} else {
			/* Get rid of the temp file. */
			unlink(screentrace_tmpfn);
		}
	}
#endif /*]*/
}

void
trace_set_screentrace_file(tss_t how, ptype_t ptype, const char *name)
{
	screentrace_how = how;
	screentrace_ptype = ptype;
    	Replace(onetime_screentrace_name, name? NewString(name): NULL);
}

tss_t
trace_get_screentrace_how(void)
{
	return screentrace_how;
}

tss_t
trace_get_screentrace_last_how(void)
{
	return screentrace_last_how;
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
		appres.trace_dir? appres.trace_dir: DEFAULT_TRACE_DIR,
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

/*
 * Turn screen tracing on or off.
 *
 * If turning it on, screentrace_how contains TSS_FILE or TSS_PRINTER,
 *  and screentrace_name is NULL (use the default) or the name of a
 *  file, printer command (Unix) or printer (Windows).
 */
static void
toggle_screenTrace(toggle_index_t ix _is_unused, enum toggle_type tt)
{
    char *tracefile_buf = NULL;
    char *tracefile;

    if (toggled(SCREEN_TRACE)) {
	/* Turn it on. */
	status_screentrace((screentrace_count = 0));
	if (onetime_screentrace_name != NULL) {
	    tracefile = tracefile_buf = onetime_screentrace_name;
	    onetime_screentrace_name = NULL;
	} else if (screentrace_how == TSS_FILE &&
		appres.screentrace_file != NULL) {
	    tracefile = appres.screentrace_file;
	} else {
	    if (screentrace_how == TSS_FILE) {
		tracefile = tracefile_buf =
		    screentrace_default_file(screentrace_ptype);
	    } else {
		tracefile = tracefile_buf = screentrace_default_printer();
	    }
	}
	if (!screentrace_cb(screentrace_how, screentrace_ptype,
		    NewString(tracefile))) {

	    set_toggle(SCREEN_TRACE, False);
	    status_screentrace((screentrace_count = -1));
	}
    } else {
	/* Turn it off. */
	if (ctlr_any_data() && !trace_skipping) {
	    do_screentrace(False);
	}
	end_screentrace(tt == TT_FINAL);
	screentrace_last_how = screentrace_how;
	screentrace_how = TSS_FILE; /* back to the default */
	screentrace_ptype = P_TEXT; /* back to the default */
	status_screentrace((screentrace_count = -1));
    }

    if (tracefile_buf != NULL) {
	Free(tracefile_buf);
    }

    trace_gui_toggle();
}

/**
 * Trace module registration.
 */
void
trace_register(void)
{
    static toggle_register_t toggles[] = {
	{ TRACING,
	  toggle_tracing,
	  TOGGLE_NEED_INIT | TOGGLE_NEED_CLEANUP },
	{ SCREEN_TRACE,
	  toggle_screenTrace,
	  TOGGLE_NEED_INIT | TOGGLE_NEED_CLEANUP }
    };

    register_toggles(toggles, array_count(toggles));
}
