/*
 * Copyright (c) 2000-2026 Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	screen.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Screen drawing
 */

#include "globals.h"

#include <assert.h>
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"
#include "toggles.h"

#include "actions.h"
#include "cmenubar.h"
#include "cscreen.h"
#include "cstatus.h"
#include "ctlrc.h"
#include "glue.h"
#include "host.h"
#include "keymap.h"
#include "kybd.h"
#include "main_window.h"
#include "names.h"
#include "nvt.h"
#include "popups.h"
#include "query.h"
#include "screen.h"
#include "see.h"
#include "selectc.h"
#include "snap.h"
#include "status.h"
#include "task.h"
#include "telnet.h"
#include "trace.h"
#include "txa.h"
#include "unicodec.h"
#include "utils.h"
#include "varbuf.h"
#include "w3misc.h"
#include "wselectc.h"
#include "xio.h"
#include "xscroll.h"
#include "warning.h"

#include <wincon.h>
#include "winvers.h"

#define STATUS_SCROLL_START_MS	1500
#define STATUS_SCROLL_MS	100
#define STATUS_PUSH_MS		5000

#define CM (60*10)	/* csec per minute */

/*
 * How many extra rows we need:
 *  2 for the menubar, if there is one
 *  1 for the line above the OIA, if on WT
 *  2 for the OIA
 */
#define XTRA_ROWS	((2 * (appres.interactive.menubar == true)) + (2 * (appres.c3270.oia == true)))

#if !defined(COMMON_LVB_LEAD_BYTE) /*[*/
# define COMMON_LVB_LEAD_BYTE		0x100
#endif /*]*/
#if !defined(COMMON_LVB_TRAILING_BYTE) /*[*/
# define COMMON_LVB_TRAILING_BYTE	0x200
#endif /*]*/

/* Unicode line-drawing characters for crosshair. */
#define LINEDRAW_VERT	0x2502
#define LINEDRAW_CROSS	0x253c
#define LINEDRAW_HORIZ	0x2500

#define MAX_HOST_COLORS	16

#define CURSOR_BLINK_MS	500

#define WINDOW_TOO_SMALL	"wc3270 won't fit in a console window with %d rows and %d columns.\n\
Minimum is %d rows and %d columns."

#define OK	0
#define ERR	(-1)

/*
 * Map from host colors to Windows Console colors.
 *
 * N.B.: F0 "neutral black" means black on a screen (white-on-black device) and
 *         white on a printer (black-on-white device).
 *       F7 "neutral white" means white on a screen (white-on-black device) and
 *         black on a printer (black-on-white device).
 */
static int cmap_fg[MAX_HOST_COLORS] = {
    0,						/* F0 neutral black */
    FOREGROUND_INTENSITY | FOREGROUND_BLUE,	/* F1 blue */
    FOREGROUND_INTENSITY | FOREGROUND_RED,	/* F2 red */
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
						/* F3 pink */
    FOREGROUND_INTENSITY | FOREGROUND_GREEN,	/* F4 green */
    FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
						/* F5 turquoise */
    FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED,
						/* F6 yellow */
    FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
						/* F7 neutral white */
    0,						/* F8 black */
    FOREGROUND_BLUE,				/* F9 deep blue */
    FOREGROUND_INTENSITY | FOREGROUND_RED,	/* FA orange */
    FOREGROUND_RED | FOREGROUND_BLUE,		/* FB purple */
    FOREGROUND_GREEN,				/* FC pale green */
    FOREGROUND_GREEN | FOREGROUND_BLUE,		/* FD pale turquoise */
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
						/* FE gray */
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, /* FF white */
};
static int cmap_bg[MAX_HOST_COLORS] = {
    0,						/* neutral black */
    BACKGROUND_INTENSITY | BACKGROUND_BLUE,	/* blue */
    BACKGROUND_INTENSITY | BACKGROUND_RED,	/* red */
    BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE,
						/* pink */
    BACKGROUND_INTENSITY | BACKGROUND_GREEN,	/* green */
    BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_BLUE,
						/* turquoise */
    BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_RED,
						/* yellow */
    BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_BLUE,
						/* neutral white */
    0,						/* black */
    BACKGROUND_BLUE,				/* deep blue */
    BACKGROUND_INTENSITY | BACKGROUND_RED,	/* orange */
    BACKGROUND_RED | BACKGROUND_BLUE,		/* purple */
    BACKGROUND_GREEN,				/* pale green */
    BACKGROUND_GREEN | BACKGROUND_BLUE,		/* pale turquoise */
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE, /* gray */
    BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,	/* white */
};
static int field_colors[4] = {
    HOST_COLOR_GREEN,		/* default */
    HOST_COLOR_RED,		/* intensified */
    HOST_COLOR_BLUE,		/* protected */
    HOST_COLOR_NEUTRAL_WHITE	/* protected, intensified */
};

/* wx3270> prompt screen output color map. */
static WORD color_attr[] = {
    HOST_COLOR_NEUTRAL_WHITE,	/* PC_DEFAULT */
    HOST_COLOR_BLUE,		/* PC_PROMPT */
    HOST_COLOR_RED,		/* PC_ERROR */
    HOST_COLOR_NEUTRAL_WHITE,	/* PC_NORMAL */
};

static unsigned *rgb = rgbmap;

/* Default 3278 RGB colors. */
#define GREEN_3278	0x00c000
#define HIGH_GREEN_3278	0x00ff00

static int defattr = 0;
static int oia_attr = 0;
static int oia_red_attr = 0;
static int oia_white_attr = 0;
static int xhattr = 0;
static ioid_t input_id;

static bool screen_fully_initted = false;
bool isendwin = true;

enum ts ab_mode = TS_AUTO;

int windows_cp = 0;

static CHAR_INFO *onscreen;	/* what's on the screen now */
static CHAR_INFO *toscreen;	/* what's supposed to be on the screen */
static bool onscreen_valid = false; /* is onscreen valid? */
static char *done_array = NULL;

static int rmargin;

static ioid_t disabled_done_id = NULL_IOID;

/* Layered OIA messages. */
static char *disabled_msg = NULL;	/* layer 0 (top) */
static char *scrolled_msg = NULL;	/* layer 1 */
static char *info_msg = NULL;		/* layer 2 */
static char *other_msg = NULL;		/* layer 3 */
static int other_attr;			/* layer 3 color */

static char *info_base_msg = NULL;	/* original info message (unscrolled) */

static void kybd_input(iosrc_t fd, ioid_t id);
static void kybd_input2(INPUT_RECORD *ir);
static void draw_oia(void);
static void status_connect(bool ignored);
static void status_3270_mode(bool ignored);
static void status_printer(bool on);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char fa);
static void relabel(bool ignored);
static void init_user_colors(void);
static void init_user_attribute_colors(void);
static HWND get_console_hwnd(void);
static void codepage_changed(bool ignored);
static void crosshair_color_init(void);

static HANDLE chandle;	/* console input handle */
static HANDLE cohandle;	/* console screen buffer handle */

static HANDLE sbuf;	/* dynamically-allocated screen buffer */

HWND console_window;

static ctrlc_fn_t ctrlc_fn = NULL;

static int console_rows;	/* number of rows on the console */
static int console_cols;	/* number of columns on the console */

static int orig_model_num;	/* model number at start-up */
static int orig_ov_rows;	/* oversize rows at start-up */
static int orig_ov_cols;	/* oversize columns at start-up */
static bool orig_ov_auto;	/* auto-oversize at start-up */

static bool screen_swapped = false;

/* State for blinking text. */
static bool blink_on = true;		/* are we displaying them or not? */
static bool blink_ticking = false;	/* is the timeout pending? */
static ioid_t blink_id = NULL_IOID;	/* timeout ID */
static bool blink_wasticking = false;	/* endwin called while blinking */
static void blink_em(ioid_t id);	/* blink timeout */

/* State for blinking cursor. */
static struct {
    ioid_t id;		/* timeout ID */
    bool visible;	/* true if visible */
} cblink = { NULL_IOID, true };
static void cblink_timeout(ioid_t id);
static void set_cblink(bool mode);

static bool in_focus = true;

static int crosshair_color = HOST_COLOR_PURPLE;

static char *window_title;
static bool selecting;
static bool cursor_enabled = true;

static HANDLE cc_event;
static ioid_t cc_id;

static WORD base_wAttributes;
static DWORD base_coflags;
static DWORD updated_coflags;

/*
 * Mode test macros:
 *  USING_VT: Using VT escape sequences to control the console. This can be on Windows Terminal or Windows Console.
 *  ON_WT:    Running on Windows Terminal.
 *  ON_WC:    Running on Windows Console.
 */
#define USING_VT ((updated_coflags & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
#define ON_WT ((base_coflags & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
#define ON_WC ((base_coflags & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0)

static bool blue_configured = false;
static COLORREF original_blue;
static bool original_blue_valid;

static action_t Paste_action;
static action_t Redraw_action;
static action_t Title_action;

static ioid_t redraw_id = NULL_IOID;

static char *fatal_error = NULL;

static bool size_success = true;

static void
screen_perror_fatal(const char *fmt, ...)
{
    va_list ap;
    char *buf;

    /* Make the console behave sanely. */
    if (cohandle != NULL) {
	(void) SetConsoleActiveScreenBuffer(cohandle);
    }
    if (chandle != NULL) {
	(void) SetConsoleMode(chandle, ENABLE_LINE_INPUT |
				       ENABLE_PROCESSED_INPUT |
				       ENABLE_MOUSE_INPUT |
				       ENABLE_INSERT_MODE |
				       ENABLE_EXTENDED_FLAGS);
    }

    va_start(ap, fmt);
    buf = Vasprintf(fmt, ap);
    va_end(ap);
    win32_perror("\n%s", buf);
    fatal_error = buf;
    vctrace(TC_UI, "Fatal error: %s\n", buf);
    x3270_exit(1);
}

/*
 * Control-C handler registration.
 */
void
screen_set_ctrlc_fn(ctrlc_fn_t fn)
{
    ctrlc_fn = fn;
}

/*
 * Console event handler.
 */
BOOL WINAPI
cc_handler(DWORD type)
{
    if (type == CTRL_C_EVENT) {
	/* Set the synchronous event so we can process it in the main loop. */
	SetEvent(cc_event);
	return TRUE;
    } else if (type == CTRL_CLOSE_EVENT) {
	/* Exit gracefully. */
	vctrace(TC_UI, "Window closed\n");
	x3270_exit(0);
	return TRUE;
    } else {
	/* Let Windows process it. */
	return FALSE;
    }
}

/*
 * Synchronous ^C handler.
 */
static void
synchronous_cc(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    char *action;

    vctrace(TC_UI, "^C received %s\n", escaped? "at prompt": "in session");
    if (escaped) {
	if (ctrlc_fn) {
	    (*ctrlc_fn)();
	}
	return;
    }
    action = lookup_key(0x03, LEFT_CTRL_PRESSED);
    if (action != NULL) {
	if (strcmp(action, "[ignore]")) {
	    push_keymap_action(action);
	}
    } else {
	run_action(AnKey, IA_DEFAULT, "0x03", NULL);
    }
}

/*
 * Resize the newly-created console buffer (WC) or the whole window (WT). If
 * we don't do this (because of auto-oversize), the buffer will implicitly be
 * the same size as the default console buffer.
 *
 * This function may make the console bigger (if the model number or oversize
 * requests it) or may make it smaller (if it is larger than what the model
 * requires). It may also call set_cols_rows() to update other globals derived
 * from ov_cols and ov_rows.
 *
 * Returns NULL for success, error text for a failure.
 */
static char *
resize_console(void)
{
    COORD console_max;
    COORD want_bs;
    SMALL_RECT sr;
    bool ov_changed = false;

    console_max = GetLargestConsoleWindowSize(cohandle);
    if (console_max.Y < MODEL_2_ROWS || console_max.X < MODEL_2_COLS) {
	return Asprintf("The maximum console window is %d rows and %d columns.\n"
		"wc3270 needs at least %d rows and %d columns. Try a smaller font.",
		console_max.Y, console_max.X,
		MODEL_2_ROWS, MODEL_2_COLS);
    }

    /*
     * Calculate the rows and columns we want -- start with the
     * model-number-derived size, increase with oversize, decrease with
     * the physical limit of the console.
     */
    want_bs.Y = model_rows(model_num) + XTRA_ROWS;
    if (ov_rows + XTRA_ROWS > want_bs.Y) {
	want_bs.Y = ov_rows + XTRA_ROWS;
    }
    if (console_max.Y && want_bs.Y > console_max.Y) {
	want_bs.Y = console_max.Y;
    }

    want_bs.X = model_cols(model_num);
    if (ov_cols > want_bs.X) {
	want_bs.X = ov_cols;
    }
    if (console_max.X && want_bs.X > console_max.X) {
	want_bs.X = console_max.X;
    }

    if (want_bs.Y != console_rows || want_bs.X != console_cols) {
	/*
	 * If we are making anything smaller, we need to shrink the
	 * console window to the least common area first.
	 */
	if (want_bs.Y < console_rows || want_bs.X < console_cols) {
	    SMALL_RECT tsr;

	    tsr.Top = 0;
	    if (want_bs.Y < console_rows) {
		tsr.Bottom = want_bs.Y - 1;
	    } else {
		tsr.Bottom = console_rows - 1;
	    }
	    tsr.Left = 0;
	    if (want_bs.X < console_cols) {
		tsr.Right = want_bs.X - 1;
	    } else {
		tsr.Right = console_cols - 1;
	    }
	    if (SetConsoleWindowInfo(sbuf, TRUE, &tsr) == 0) {
		screen_perror_fatal("resize_console SetConsoleWindowInfo(1)");
	    }
	}

	/* Set the console buffer size. */
	if (SetConsoleScreenBufferSize(sbuf, want_bs) == 0) {
	    screen_perror_fatal("resize_console SetConsoleScreenBufferSize");
	}

	/* Set the console size. */
	sr.Top = 0;
	sr.Bottom = want_bs.Y - 1;
	sr.Left = 0;
	sr.Right = want_bs.X - 1;
	if (SetConsoleWindowInfo(sbuf, TRUE, &sr) == 0) {
	    screen_perror_fatal("resize_console SetConsoleWindowInfo(2)");
	}

	/* Remember the new physical screen dimensions. */
	console_rows = want_bs.Y;
	console_cols = want_bs.X;

	/* See if the Windows Terminal window needs resizing. */
	if (USING_VT) {
	    CONSOLE_SCREEN_BUFFER_INFO info;
	    int current_rows, current_cols;

	    if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
		screen_perror_fatal("GetConsoleScreenBufferInfo");
		return NULL;
	    }
	    current_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
	    current_cols = info.srWindow.Right - info.srWindow.Left + 1;
	    if (console_rows != current_rows || console_cols != current_cols) {
		char *e = Asprintf("\033[8;%d;%dt", console_rows, console_cols);

		/* Resize the window. */
		WriteConsoleA(cohandle, e, (int)strlen(e), NULL, NULL);
		Free(e);

		/* Hack. The screen image is not stable after the resize. So wait a bit. */
		Sleep(500);

		/* See if the escape sequence actually took. */
		if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
		    screen_perror_fatal("GetConsoleScreenBufferInfo");
		    return NULL;
		}
		if (console_rows != info.srWindow.Bottom - info.srWindow.Top + 1 ||
		    console_cols != info.srWindow.Right - info.srWindow.Left + 1) {
		    char *errmsg;

		    /*
		     * There is likely a second tab open and we can't make the window bigger. (It doesn't
		     * matter if we can't make it smaller.) Scale back to whatever is possible.
		     */
		    vctrace(TC_UI, "resize_console: Window size did not change, assuming Windows Terminal with multiple tabs\n");
		    console_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
		    console_cols = info.srWindow.Right - info.srWindow.Left + 1;
		    errmsg = screen_adapt(model_num, ov_auto, ov_rows, ov_cols, console_rows, console_cols);
		    if (errmsg != NULL) {
			return errmsg;
		    }
		    vctrace(TC_UI, "set_console_buffer: new model %d ov_rows %d ov_cols %d\n",
			    model_num, ov_rows, ov_cols);
		    return NULL;
		}
	    }
	}

	/*
	 * Calculate new oversize and maximum logical screen
	 * dimensions.
	 *
	 * This gets a bit tricky, because the menu bar and OIA can
	 * disappear if we are constrained by the physical screen, but
	 * we will not turn them off to make oversize fit.
	 */
	if (ov_cols > model_cols(model_num)) {
	    if (ov_cols > console_cols) {
		ov_cols = console_cols;
		popup_an_error("Oversize columns reduced to the maximum window width (%d).", ov_cols);
		ov_changed = true;
	    }
	}

	if (ov_rows > model_rows(model_num)) {
	    if (ov_rows + XTRA_ROWS > console_rows) {
		ov_rows = console_rows - XTRA_ROWS;
		if (ov_rows < model_rows(model_num)) {
		    ov_rows = model_rows(model_num);
		}
		popup_an_error("Oversize rows reduced to the maximum window height (%d).", ov_rows);
		ov_changed = true;
	    }
	}
    }

    if (ov_changed) {
	popup_an_error("Try a smaller font.");
	set_cols_rows(model_num, ov_cols, ov_rows);
    }

    set_status_row(console_rows, maxROWS);

    return NULL;
}

/* Console pre-initialization. */
void
screen_pre_init(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    /* Get the console handles. */
    assert(chandle == NULL);
    chandle = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
	    NULL, OPEN_EXISTING, 0, NULL);
    if (chandle == NULL) {
	screen_perror_fatal("screen_pre_init CreateFile(CONIN$)");
	return;
    }
    assert(cohandle == NULL);
    cohandle = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE,
	    NULL, OPEN_EXISTING, 0, NULL);
    if (cohandle == NULL) {
	screen_perror_fatal("screen_pre_init CreateFile(CONOUT$)");
	return;
    }

    /* Get the default console color. */
    if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
	screen_perror_fatal("screen_pre_init GetConsoleScreenBufferInfo");
	return;
    }
    base_wAttributes = info.wAttributes;
}

/* Remap blue on a Windows Console. */
static void
remap_blue(HANDLE buf, unsigned *map)
{
    CONSOLE_SCREEN_BUFFER_INFOEX infoex;

    if (blue_configured) {
	/* The user explicitly configured host color blue. Do not override. */
	return;
    }

    memset(&infoex, '\0', sizeof(CONSOLE_SCREEN_BUFFER_INFOEX));
    infoex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    if (GetConsoleScreenBufferInfoEx(buf, &infoex) == 0) {
	screen_perror_fatal("remap_blue GetConsoleScreenBufferInfoEx");
    }

    /* This works around a well-known bug in the API. */
    infoex.srWindow.Bottom++;
    infoex.srWindow.Right++;

    /* Change highlighted blue, it's too dark to read otherwise. */
    original_blue = infoex.ColorTable[cmap_fg[HOST_COLOR_BLUE]];
    original_blue_valid = true;
    infoex.ColorTable[cmap_fg[HOST_COLOR_BLUE]] = ((map[HOST_COLOR_BLUE] & 0xff) << 16) |
						  (map[HOST_COLOR_BLUE] & 0x00ff00) |
						  ((map[HOST_COLOR_BLUE] & 0xff0000) >> 16);

    /* Change the screen buffer. */
    if (SetConsoleScreenBufferInfoEx(buf, &infoex) == 0) {
	screen_perror_fatal("remap_blue SetConsoleScreenBufferInfoEx");
    }
}

/* Initialize reverse-video settings. */
static void
init_reverse_video(void)
{
    if (appres.c3270.reverse_video) {
	/* Pick the right RGB map. */
	rgb = rgbmap_rv;

	if (!USING_VT) {
	    int t;

	    /* For Windows Console, swap neutral black and neutral white. */
	    t = cmap_fg[HOST_COLOR_NEUTRAL_WHITE];
	    cmap_fg[HOST_COLOR_NEUTRAL_WHITE] = cmap_fg[HOST_COLOR_NEUTRAL_BLACK];
	    cmap_fg[HOST_COLOR_NEUTRAL_BLACK] = t;

	    t = cmap_bg[HOST_COLOR_NEUTRAL_WHITE];
	    cmap_bg[HOST_COLOR_NEUTRAL_WHITE] = cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
	    cmap_bg[HOST_COLOR_NEUTRAL_BLACK] = t;
	}
    }
}

/* Initialize the basic screen attributes. */
static void
init_default_attrs(void)
{
    if (mode3279) {
	defattr = get_color_pair(HOST_COLOR_NEUTRAL_WHITE, HOST_COLOR_NEUTRAL_BLACK);
	crosshair_color_init();
	xhattr = get_color_pair(crosshair_color, HOST_COLOR_NEUTRAL_BLACK);
	oia_attr = get_color_pair(HOST_COLOR_BLUE, HOST_COLOR_NEUTRAL_BLACK);
	oia_red_attr = get_color_pair(HOST_COLOR_RED, HOST_COLOR_NEUTRAL_BLACK);
	oia_white_attr = get_color_pair(HOST_COLOR_NEUTRAL_WHITE, HOST_COLOR_NEUTRAL_BLACK);
    }
    else {
	defattr = get_color_pair(HOST_COLOR_PALE_GREEN, HOST_COLOR_NEUTRAL_BLACK);
	xhattr = get_color_pair(HOST_COLOR_PALE_GREEN, HOST_COLOR_NEUTRAL_BLACK);
	oia_attr = get_color_pair(HOST_COLOR_PALE_GREEN, HOST_COLOR_NEUTRAL_BLACK);
	oia_red_attr = get_color_pair(HOST_COLOR_GREEN, HOST_COLOR_NEUTRAL_BLACK);
	oia_white_attr = oia_attr;
    }
}

/* (Re-)allocate the onscreen and toscreen buffers. */
static void
allocate_onscreen_toscreen(void)
{
    size_t buffer_size = sizeof(CHAR_INFO) * console_rows * console_cols;

    /* (Re-)allocate onscreen and toscreen. */
    Replace(onscreen, (CHAR_INFO *)Malloc(buffer_size));
    memset(onscreen, '\0', buffer_size);
    onscreen_valid = false;
    Replace(toscreen, (CHAR_INFO *)Malloc(buffer_size));
    memset(toscreen, '\0', buffer_size);

    /* (Re-)allocate the 'done' map. */
    Replace(done_array, Malloc(console_rows * console_cols));
    memset(done_array, '\0', console_rows * console_cols);
}

/*
 * Initialize pseudo-curses.
 */
static HANDLE
initscr(void)
{
    if (SetConsoleMode(chandle, ENABLE_PROCESSED_INPUT |
				ENABLE_MOUSE_INPUT |
				ENABLE_INSERT_MODE |
				ENABLE_EXTENDED_FLAGS) == 0) {
	screen_perror_fatal("initscr SetConsoleMode");
	return NULL;
    }

    /* Define a console handler. */
    if (!SetConsoleCtrlHandler(cc_handler, TRUE)) {
	screen_perror_fatal("SetConsoleCtrlHandler");
	return NULL;
    }
    cc_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (cc_event == NULL) {
	screen_perror_fatal("CreateEvent for ^C");
	return NULL;
    }
    cc_id = AddInput(cc_event, synchronous_cc);

    /* Allocate the buffers. */
    allocate_onscreen_toscreen();

    return chandle;
}

/*
 * Do the screen initialization that isn't needed until we connect.
 * Returns true for success.
 */
static bool
finish_screen_init(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    char *errmsg;

    if (screen_fully_initted) {
	return true;
    }

    /* Create the screen buffer for the 3270 display. */
    sbuf = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER,
	    NULL);
    if (sbuf == NULL) {
	screen_perror_fatal("finish_screen_init CreateConsoleScreenBuffer");
	return false;
    }

    /* Get the console dimensions. */
    if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
	screen_perror_fatal("finish_screen_init GetConsoleScreenBufferInfo");
	return false;
    }
    console_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    console_cols = info.srWindow.Right - info.srWindow.Left + 1;

    /* Set its dimensions. */
    errmsg = ov_auto?
	screen_adapt(model_num, ov_auto, ov_rows, ov_cols, console_rows, console_cols):
	resize_console();
    if (errmsg) {
	/* Screen is to small to proceed. */
	popup_an_error(errmsg);
	Free(errmsg);
	if (PCONNECTED) {
	    appres.retry = false;
	    run_action(AnDisconnect, IA_UI, NULL, NULL);
	}
	return false;
    }

    /* Initialilze pseudo-curses with the new screen dimensions. */
    initscr();

    screen_fully_initted = true;
    return true;
}

/*
 * Virtual curses functions.
 */
static int cur_row = 0;
static int cur_col = 0;
static int cur_attr = 0;

static int
move(int row, int col)
{
    if (row < console_rows && col < console_cols) {
	cur_row = row;
	cur_col = col;
	return OK;
    } else {
	return ERR;
    }
}

static void
attrset(int a)
{
    cur_attr = a;
}

static void
addch(ucs4_t c)
{
    CHAR_INFO *ch = &toscreen[(cur_row * console_cols) + cur_col];

    /* Save the desired character. */
    if (cur_row < console_rows && cur_col < console_cols) {
	if (ch->Char.UnicodeChar != c || ch->Attributes != cur_attr) {
	    ch->Char.UnicodeChar = c;
	    ch->Attributes = cur_attr;
	}
    }

    /* Increment and wrap. */
    if (++cur_col >= console_cols) {
	cur_col = console_cols - 1;
	if (++cur_row >= console_rows) {
	    cur_row = console_rows - 1;
	}
    }
}

static int
ix(int row, int col)
{
    if (row < console_rows && col < console_cols) {
	return (row * console_cols) + col;
    } else {
	return (console_rows * console_cols) - 1;
    }
}

static int
mvinch(int y, int x)
{
    if (move(y, x) == OK) {
	return toscreen[ix(y, x)].Char.UnicodeChar;
    } else {
	return 0;
    }
}

#define A_CHARTEXT 0xffff

#if 0 /* unused for now */
static void
printw(char *fmt, ...)
{
    va_list ap;
    char *buf;
    size_t sl;
    WCHAR *wbuf;
    int nc;
    int i;

    va_start(ap, fmt);
    buf = Vasprintf(fmt, ap);
    va_end(ap);
    sl = strlen(buf);

    wbuf = (WCHAR *)Malloc(sl * sizeof(WCHAR));
    nc = MultiByteToWideChar(CP_ACP, 0, buf, (int)sl, wbuf, (int)sl);
    Free(buf);
    for (i = 0; i < nc; i++) {
	addch(wbuf[i]);
    }
    Free(wbuf);
}
#endif

static int
mvprintw(int row, int col, char *fmt, ...)
{
    va_list ap;
    char *buf;
    size_t sl;
    WCHAR *wbuf;
    int nc;
    int i;

    if (row >= console_rows || col >= console_cols) {
	return ERR;
    }
    cur_row = row;
    cur_col = col;
    va_start(ap, fmt);
    buf = Vasprintf(fmt, ap);
    va_end(ap);
    sl = strlen(buf);

    wbuf = (WCHAR *)Malloc(sl * sizeof(WCHAR));
    nc = MultiByteToWideChar(CP_ACP, 0, buf, (int)sl, wbuf, (int)sl);
    Free(buf);
    for (i = 0; i < nc; i++) {
	addch(wbuf[i]);
    }
    Free(wbuf);
    return OK;
}


static void
none_done(void)
{
    assert(done_array != NULL);
    memset(done_array, '\0', console_rows * console_cols);
}

static int
is_done(int row, int col)
{
    return done_array[ix(row, col)];
}

static void
mark_done(int start_row, int end_row, int start_col, int end_col)
{
    int row;

    for (row = start_row; row <= end_row; row++) {
	memset(&done_array[ix(row, start_col)], 1, end_col - start_col + 1);
    }
}

static int
tos_a(int row, int col)
{
    return toscreen[ix(row, col)].Attributes;
}

/*
 * Local version of select_changed() that deals in screen coordinates, not
 * 3270 display buffer coordinates.
 */
static bool
select_changed_s(unsigned row, unsigned col, unsigned rows, unsigned cols)
{
    int row_adj, rows_adj;
    int cols_adj;

    /* Adjust for menu bar. */
    row_adj = row - screen_yoffset;
    rows_adj = rows;
    if (row_adj < 0) {
	rows_adj += row_adj;
	row_adj = 0;
	if (rows_adj <= 0) {
	    return false;
	}
    }

    /* Adjust for overflow at the bottom. */
    if (row_adj >= ROWS) {
	return false;
    }
    if (row_adj + rows_adj >= ROWS) {
	rows_adj = ROWS - row_adj;
	if (rows_adj <= 0) {
	    return false;
	}
    }

    /* Adjust for overflow at the right. */
    if ((int)col >= COLS) {
	return false;
    }
    cols_adj = cols;
    if ((int)(col + cols_adj) >= COLS) {
	cols_adj = COLS - col;
	if (cols_adj <= 0) {
	    return false;
	}
    }

    /* Now see if the area on the 3270 display has changed. */
    return select_changed(row_adj, col, rows_adj, cols_adj);
}

/*
 * Local version of select_sync() that deals in screen coordinates, not
 * 3270 display buffer coordinates.
 */
static void
select_sync_s(unsigned row, unsigned col, unsigned rows, unsigned cols)
{
    int row_adj, rows_adj;
    int cols_adj;

    /* Adjust for menu bar. */
    row_adj = row - screen_yoffset;
    rows_adj = rows;
    if (row_adj < 0) {
	rows_adj -= row_adj;
	row_adj = 0;
	if (rows_adj <= 0) {
	    return;
	}
    }

    /* Adjust for overflow at the bottom. */
    if (row_adj >= ROWS) {
	return;
    }
    if (row_adj + rows_adj >= ROWS) {
	rows_adj = ROWS - row_adj;
	if (rows_adj <= 0) {
	    return;
	}
    }

    /* Adjust for overflow at the right. */
    if ((int)col >= COLS) {
	return;
    }
    cols_adj = cols;
    if ((int)(col + cols_adj) >= COLS) {
	cols_adj = COLS - col;
	if (cols_adj <= 0) {
	    return;
	}
    }

    /* Now see if the area on the 3270 display has changed. */
    select_sync(row_adj, col, rows_adj, cols_adj);
}

#if defined(DEBUG_SCREEN_DRAW) /*[*/
static int
changed(int row, int col)
{
    return !onscreen_valid ||
	    memcmp(&onscreen[ix(row, col)], &toscreen[ix(row, col)],
		   sizeof(CHAR_INFO)) ||
	    select_changed_s(row, col, 1, 1);
}
#endif /*]*/

/*
 * Draw a rectangle of homogeneous text.
 */
static void
hdraw(int row, int lrow, int col, int lcol)
{
    COORD bufferSize;
    COORD bufferCoord;
    SMALL_RECT writeRegion;
    int xrow;

#if defined(DEBUG_SCREEN_DRAW) /*[*/
    /*
     * Trace what we've been asked to draw.
     * Drawn areas are 'h', done areas are 'd'.
     */
    {
	int trow, tcol;

	vctrace(TC_UI, "hdraw row %d-%d col %d-%d attr 0x%x:\n",
		row, lrow, col, lcol, tos_a(row, col));
	for (trow = 0; trow < console_rows; trow++) {
	    for (tcol = 0; tcol < console_cols; tcol++) {
		if (trow >= row && trow <= lrow &&
			tcol >= col && tcol <= lcol) {
		    vtrace("h");
		} else if (is_done(trow, tcol)) {
		    vtrace("d");
		} else {
		    vtrace(".");
		}
	    }
	    vtrace("\n");
	}
    }
#endif /*]*/

    /* Write it. */
    {
	int r, c;

	if (!USING_VT) {
	    /* Blast it out in one go with WriteConsoleOutput. */
	    bufferSize.X = console_cols;
	    bufferSize.Y = console_rows;
	    bufferCoord.X = col;
	    bufferCoord.Y = row;
	    writeRegion.Left = col;
	    writeRegion.Top = row;
	    writeRegion.Right = lcol;
	    writeRegion.Bottom = lrow;
	    if (WriteConsoleOutputW(sbuf, toscreen, bufferSize, bufferCoord, &writeRegion) == 0) {
		screen_perror_fatal("hdraw WriteConsoleOutput");
	    }
	} else {
	    /* Windows Terminal. Dribble it out in runs with common attributes using WriteConsole. */
	    CONSOLE_CURSOR_INFO cursor_info;
	    CONSOLE_SCREEN_BUFFER_INFO screen_buffer_info;
	    int last_attr = -1;
	    wchar_t *run = (wchar_t *)Malloc(console_cols * sizeof(wchar_t));
	    wchar_t *run_next = run;
	    bool cursor_was_visible = false;
	    int current_cols;

	    /* Get the current cursor info. */
	    if (GetConsoleCursorInfo(sbuf, &cursor_info) == 0) {
		screen_perror_fatal("hdraw GetConsoleCursorInfo");
	    }
	    if (GetConsoleScreenBufferInfo(sbuf, &screen_buffer_info) == 0) {
		screen_perror_fatal("hdraw GetConsoleScreenBufferInfo");
	    }
	    current_cols = screen_buffer_info.srWindow.Right - screen_buffer_info.srWindow.Left + 1;

	    /* Turn off the cursor, if it is on. */
	    if (cursor_info.bVisible) {
		cursor_was_visible = true;
		cursor_info.bVisible = FALSE;
		if (SetConsoleCursorInfo(sbuf, &cursor_info) == 0) {
		    screen_perror_fatal("hdraw SetConsoleCursorInfo");
		}
	    }

	    for (r = row; r <= lrow; r++) {
		COORD coord;

		/* Go to the beginning of the line. */
		coord.X = col;
		coord.Y = r;
		if (SetConsoleCursorPosition(sbuf, coord) == 0) {
		    vctrace(TC_UI, "hdraw: can't move to row %d col %d\n", r + 1, col + 1);
		    continue;
		}

		for (c = col; c <= lcol; c++) {
		    CHAR_INFO ch = toscreen[ix(r, c)];
		    int attr = ch.Attributes & ~COMMON_LVB_LEADING_BYTE;

		    if (attr & COMMON_LVB_TRAILING_BYTE) {
			continue;
		    }
		    if (last_attr != attr && run_next != run) {
			/* Write the pending text. */
			int fg = rgb[(last_attr >> 4) & 0xf];
			int bg = rgb[last_attr & 0xf];
			char *ct = Asprintf("\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm\033[%dm",
				(fg >> 16) & 0xff, (fg >> 8) & 0xff, fg & 0xff,
				(bg >> 16) & 0xff, (bg >> 8) & 0xff, bg & 0xff,
				(last_attr & COMMON_LVB_UNDERSCORE)? 4: 24);

			if (WriteConsoleA(sbuf, ct, (int)strlen(ct), NULL, NULL) == 0) {
			    screen_perror_fatal("hdraw WriteConsoleA");
			}
			Free(ct);
			if (WriteConsoleW(sbuf, run, (int)(run_next - run), NULL, NULL) == 0) {
			    screen_perror_fatal("hdraw WriteConsoleW");
			}
			run_next = run;
		    }

		    /* Store the attribute and character, if it fits. */
		    if (c < current_cols) {
			last_attr = attr;
			*run_next++ = ch.Char.UnicodeChar;
		    }
		}
		if (run_next != run) {
		    /* Write the final pending text. */
		    int fg = rgb[(last_attr >> 4) & 0xf];
		    int bg = rgb[last_attr & 0xf];
		    char *ct = Asprintf("\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm\033[%dm",
			    (fg >> 16) & 0xff, (fg >> 8) & 0xff, fg & 0xff,
			    (bg >> 16) & 0xff, (bg >> 8) & 0xff, bg & 0xff,
			    (last_attr & COMMON_LVB_UNDERSCORE)? 4: 24);

		    if (WriteConsoleA(sbuf, ct, (int)strlen(ct), NULL, NULL) == 0) {
			screen_perror_fatal("hdraw WriteConsoleA");
		    }
		    Free(ct);
		    if (WriteConsoleW(sbuf, run, (int)(run_next - run), NULL, NULL) == 0) {
			screen_perror_fatal("hdraw WriteConsoleW");
		    }
		    run_next = run;
		}
	    }

	    Free(run);

	    /* Put the cursor back where it was. */
	    if (SetConsoleCursorPosition(sbuf, screen_buffer_info.dwCursorPosition) == 0) {
		vctrace(TC_UI, "hdraw SetConsoleCursorPosition: %s", win32_strerror(GetLastError()));
	    }

	    /* Turn the cursor back on, if we turned it off. */
	    if (cursor_was_visible) {
		cursor_info.bVisible = TRUE;
		if (SetConsoleCursorInfo(sbuf, &cursor_info) == 0) {
		    screen_perror_fatal("hdraw SetConsoleCursorInfo");
		}
	    }
	}
    }

    /* Sync 'onscreen'. */
    for (xrow = row; xrow <= lrow; xrow++) {
	memcpy(&onscreen[ix(xrow, col)], &toscreen[ix(xrow, col)],
		sizeof(CHAR_INFO) * (lcol - col + 1));
    }
    select_sync_s(row, col, lrow - row + 1, lcol - col + 1);

    /* Mark the region as done. */
    mark_done(row, lrow, col, lcol);
}

/*
 * Draw a rectanglar region from 'toscreen' onto the screen, without regard to
 * what is already there.
 * If the attributes for the entire region are the same, we can draw it in
 * one go; otherwise we will need to break it into little pieces (fairly
 * stupidly) with common attributes.
 * When done, copy the region from 'toscreen' to 'onscreen'.
 */
static void
draw_rect(const char *why, int pc_start, int pc_end, int pr_start, int pr_end)
{
    int a;
    int ul_row, ul_col, xrow, xcol, lr_row, lr_col;

#if defined(DEBUG_SCREEN_DRAW) /*[*/
    /*
     * Trace what we've been asked to draw.
     * Modified areas are 'r', unmodified (excess) areas are 'x'.
     */
    {
	int trow, tcol;

	vctrace(TC_UI, "draw_rect %s row %d-%d col %d-%d\n",
		why, pr_start, pr_end, pc_start, pc_end);
	for (trow = 0; trow < console_rows; trow++) {
	    for (tcol = 0; tcol < console_cols; tcol++) {
		if (trow >= pr_start && trow <= pr_end &&
		    tcol >= pc_start && tcol <= pc_end) {
		    if (changed(trow, tcol)) {
			vtrace("r");
		    } else {
			vtrace("x");
		    }
		} else {
		    vtrace(".");
		}
	    }
	    vtrace("\n");
	}
    }
#endif /*]*/

    for (ul_row = pr_start; ul_row <= pr_end; ul_row++) {
	for (ul_col = pc_start; ul_col <= pc_end; ul_col++) {
	    int col_found = 0;

	    if (is_done(ul_row, ul_col)) {
		continue;
	    }

	    /*
	     * [ul_row,ul_col] is the upper left-hand corner of an
	     * undrawn region.
	     *
	     * Find the the lower right-hand corner of the
	     * rectangle with common attributes.
	     */
	    a = tos_a(ul_row, ul_col);
	    lr_col = pc_end;
	    lr_row = pr_end;
	    for (xrow = ul_row; !col_found && xrow <= pr_end; xrow++) {

		if (is_done(xrow, ul_col) || tos_a(xrow, ul_col) != a) {
		    lr_row = xrow - 1;
		    break;
		}
		for (xcol = ul_col; xcol <= lr_col; xcol++) {
		    if (is_done(xrow, xcol) || tos_a(xrow, xcol) != a) {
			lr_col = xcol - 1;
			lr_row = xrow;
			col_found = 1;
			break;
		    }
		}
	    }
	    if (tos_a(ul_row, ul_col) & COMMON_LVB_LEAD_BYTE) {
		continue;
	    }
	    hdraw(ul_row, lr_row, ul_col, lr_col);
	    if (tos_a(ul_row, ul_col) & COMMON_LVB_TRAILING_BYTE) {
		hdraw(ul_row, lr_row, ul_col-1, lr_col-1);
	    }
	}
    }
}

/*
 * Compare 'onscreen' (what's on the screen right now) with 'toscreen' (what
 * we want on the screen) and draw what's changed.  Hopefully it will be in
 * a reasonably optimized fashion.
 *
 * Windows lets us draw a rectangular areas with one call, provided that the
 * whole area has the same attributes.  We will take advantage of this where
 * it is relatively easy to figure out, by walking row by row, holding on to
 * and widening a vertical band of modified columns and drawing only when we
 * hit a row that needs no modifications.  This will cause us to miss some
 * easy-seeming cases that require recognizing multiple bands per row.
 */
static void
sync_onscreen(void)
{
    int row;
    int col;
    int pending = FALSE;	/* is there a draw pending? */
    int pc_start, pc_end;	/* first and last columns in pending band */
    int pr_start;		/* first row in pending band */

    /* Clear out the 'what we've seen' array. */
    none_done();

#if defined(DEBUG_SCREEN_DRAW) /*[*/
    /*
     * Trace what's been modified.
     * Modified areas are 'm'.
     */
    {
	int trow, tcol;

	vctrace(TC_UI, "sync_onscreen:\n");
	for (trow = 0; trow < console_rows; trow++) {
	    for (tcol = 0; tcol < console_cols; tcol++) {
		if (changed(trow, tcol)) {
		    vtrace("m");
		} else {
		    vtrace(".");
		}
	    }
	    vtrace("\n");
	}
    }
#endif /*]*/

    /* Sometimes you have to draw everything. */
    if (!onscreen_valid) {
	draw_rect("invalid", 0, console_cols - 1, 0, console_rows - 1);
	onscreen_valid = true;
	return;
    }

    for (row = 0; row < console_rows; row++) {

	/* Check the whole row for a match first. */
	if (!memcmp(&onscreen[ix(row, 0)],
		    &toscreen[ix(row, 0)],
		    sizeof(CHAR_INFO) * console_cols) &&
	     !select_changed_s(row, 0, 1, console_cols)) {
	    if (pending) {
		draw_rect("middle", pc_start, pc_end, pr_start, row - 1);
		pending = FALSE;
	    }
	    continue;
	}

	for (col = 0; col < console_cols; col++) {
	    if (memcmp(&onscreen[ix(row, col)],
		       &toscreen[ix(row, col)],
		       sizeof(CHAR_INFO)) ||
		select_changed_s(row, col, 1, 1)) {
		/*
		 * This column differs.
		 * Start or expand the band, and start pending.
		 */
		if (!pending || col < pc_start) {
		    pc_start = col;
		}
		if (!pending || col > pc_end) {
		    pc_end = col;
		}
		if (!pending) {
		    pr_start = row;
		    pending = TRUE;
		}
	    }
	}
    }

    if (pending) {
	draw_rect("end", pc_start, pc_end, pr_start, console_rows - 1);
    }
}

/*
 * Set the console cursor size.
 */
static void
set_cursor_size(HANDLE handle)
{
    if (screen_swapped) {
	CONSOLE_CURSOR_INFO cci;

	/*
	 * Ask Windows Console for a particular cursor size, and enable/disable it.
	 * This needs to happen when the screen buffer is swapped.
	 */
	memset(&cci, 0, sizeof(cci));
	cci.bVisible = (cursor_enabled && cblink.visible)? TRUE: FALSE;
	if (toggled(ALT_CURSOR)) {
	    cci.dwSize = 25;
	} else {
	    cci.dwSize = 100;
	}
	if (SetConsoleCursorInfo(handle, &cci) == 0) {
	    screen_perror_fatal("set_cursor_size SetConsoleCursorInfo");
	}

	/* Set the cursor type on Windows Terminal. */
	if (USING_VT && cursor_enabled) {
	    static const char *type_str[] = {
		"\033[2 q",	/* block cursor, not blinking */
		"\033[1 q",	/* block cursor, blinking */
		"\033[4 q",	/* underscore cursor, not blinking */
		"\033[3 q",	/* underscore cursor, blinking */
	    };
	    int ix = (toggled(ALT_CURSOR)? 2: 0) + (toggled(CURSOR_BLINK)? 1: 0);

	    if (WriteConsoleA(handle, type_str[ix], 5, NULL, NULL) == 0) {
		screen_perror_fatal("set_cursor_size WriteConsoleA");
	    }
	}
    }
}

/* Repaint the screen. */
static void
refresh(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD coord;
    bool wasendwin = isendwin;

    isendwin = false;

    /* Draw the differences between 'onscreen' and 'toscreen' into sbuf. */
    sync_onscreen();

    /* Set the cursor size (wt). */
    set_cursor_size(sbuf);

    /* Move the cursor. */
    coord.X = cur_col;
    coord.Y = cur_row;
    if (onscreen[ix(cur_row, cur_col)].Attributes & COMMON_LVB_TRAILING_BYTE) {
	coord.X--;
    }
    if (GetConsoleScreenBufferInfo(sbuf, &info) == 0) {
	screen_perror_fatal("refresh GetConsoleScreenBufferInfo");
    }
    if ((info.dwCursorPosition.X != coord.X ||
	 info.dwCursorPosition.Y != coord.Y)) {
	if (SetConsoleCursorPosition(sbuf, coord) == 0) {
	    vctrace(TC_UI, "refresh: SetConsoleCursorPosition(x=%d,y=%d): %s\n", coord.X, coord.Y,
		    win32_strerror(GetLastError()));
	}
    }

    /* Swap in this buffer. */
    if (!screen_swapped) {
	if (SetConsoleActiveScreenBuffer(sbuf) == 0) {
	    screen_perror_fatal("refesh SetConsoleActiveScreenBuffer");
	}
	screen_swapped = true;
    }

    /* Set the cursor size (wc). */
    set_cursor_size(sbuf);

    /* Start blinking again. */
    if (blink_wasticking) {
	blink_wasticking = false;
	blink_id = AddTimeOut(750, blink_em);
    }

    /* Restart cursor blinking. */
    if (wasendwin) {
	set_cblink(toggled(CURSOR_BLINK));
    }
}

/* Set the console to 'cooked' mode. */
static void
set_console_cooked(void)
{
    if (SetConsoleMode(chandle, ENABLE_ECHO_INPUT |
				ENABLE_LINE_INPUT |
				ENABLE_PROCESSED_INPUT |
				ENABLE_MOUSE_INPUT |
				ENABLE_INSERT_MODE |
				ENABLE_EXTENDED_FLAGS) == 0) {
	screen_perror_fatal("set_console_cooked SetConsoleMode(CONIN$)");
    }
    if (SetConsoleMode(cohandle, ENABLE_PROCESSED_OUTPUT |
				 ENABLE_WRAP_AT_EOL_OUTPUT |
				 ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
	screen_perror_fatal("set_console_cooked SetConsoleMode(CONOUT$)");
    }
}

/* Toggle cooked echo/noecho modes. */
void
screen_echo_mode(bool echo)
{
    if (echo) {
	if (SetConsoleMode(chandle, ENABLE_ECHO_INPUT |
				    ENABLE_LINE_INPUT |
				    ENABLE_PROCESSED_INPUT |
				    ENABLE_MOUSE_INPUT |
				    ENABLE_INSERT_MODE |
				    ENABLE_EXTENDED_FLAGS) == 0) {
	    screen_perror_fatal("screen_echo_mode SetConsoleMode(CONIN$)");
	}
    } else {
	if (SetConsoleMode(chandle, ENABLE_LINE_INPUT |
				    ENABLE_PROCESSED_INPUT |
				    ENABLE_MOUSE_INPUT |
				    ENABLE_INSERT_MODE |
				    ENABLE_EXTENDED_FLAGS) == 0) {
	    screen_perror_fatal("screen_echo_mode SetConsoleMode(CONIN$)");
	}
    }
}

/* Go back to the original screen. */
static void
endwin(void)
{
    if (isendwin) {
	return;
    }

    isendwin = true;

    if (blink_ticking) {
	RemoveTimeOut(blink_id);
	blink_id = NULL_IOID;
	blink_ticking = false;
	blink_on = true;
	blink_wasticking = true;
    }

    /* Turn off the blinking cursor. */
    set_cblink(false);

    /* Turn echo back on. */
    set_console_cooked();

    /* Swap in the original buffer. */
    if (SetConsoleActiveScreenBuffer(cohandle) == 0) {
	screen_perror_fatal("endwin SetConsoleActiveScreenBuffer");
    }

    screen_swapped = false;

    if (USING_VT) {
	(void) WriteConsoleA(cohandle, "\033[2J\033[H", 7, NULL, NULL);
    } else {
	system("cls");
    }
    printf("[wc3270]\n\n");
    if (fatal_error != NULL) {
	screen_color(PC_ERROR);
	printf("Fatal error: %s\n", fatal_error);
	fflush(stdout);
	screen_color(PC_NORMAL);
	fatal_error = NULL;
    }
    fflush(stdout);
}

/*
 * Returns the absolute minimum number of rows needed for a given model -- the 'squeezed' format,
 * which can include removing the menubar and OIA altogether in some situations.
 */
int
model_min_xtra(int model)
{
    return (model == 2)? 0: (appres.interactive.menubar + appres.c3270.oia);
}



/* Set the minimum number of rows and columns. */
void
screen_set_minimum_rows_cols(int rows, int cols)
{
}

/* Possibly override the status row location. */
int
screen_map_rows(int hard_rows)
{
    return hard_rows;
}

/* Initialize the screen. */
void
screen_init(void)
{
    /* Get the initial console output flags. */
    if (GetConsoleMode(cohandle, &base_coflags) == 0) {
	screen_perror_fatal("screen_init GetConsoleMode");
	return;
    }

    /* If so configured, force VT mode. */
    updated_coflags = base_coflags;
    if (appres.c3270.vt) {
	updated_coflags |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (SetConsoleMode(cohandle, updated_coflags) == 0) {
	    screen_perror_fatal("screen_init SetConsoleMode");
	    return;
	}
    }

    if (appres.interactive.menubar) {
	menu_init();
    }

    windows_cp = GetConsoleCP();

    console_window = get_console_hwnd();
    set_main_window(console_window);

    /* Figure out where the status line goes, if it fits. */
    /* Start out in altscreen mode. */
    set_status_row(console_rows, maxROWS);

    /* Set up callbacks for state changes. */
    register_schange(ST_NEGOTIATING, status_connect);
    register_schange(ST_CONNECT, status_connect);
    register_schange(ST_3270_MODE, status_3270_mode);
    register_schange(ST_PRINTER, status_printer);

    register_schange(ST_CONNECT, relabel);
    register_schange(ST_3270_MODE, relabel);

    register_schange(ST_CODEPAGE, codepage_changed);

    /* See about all-bold behavior. */
    if (appres.c3270.all_bold_on) {
	ab_mode = TS_ON;
    } else if (!ts_value(appres.c3270.all_bold, &ab_mode)) {
	fprintf(stderr, "invalid %s value: '%s', assuming 'auto'\n",
		ResAllBold, appres.c3270.all_bold);
    }
    if (ab_mode == TS_AUTO) {
	ab_mode = mode3279? TS_ON: TS_OFF;
    }

    /* Pull in the user's color mappings. */
    init_user_colors();
    init_user_attribute_colors();

    /* Set up reverse video. */
    init_reverse_video();

    /* Now it's safe to set up default attributes. */
    init_default_attrs();

    /* Remap highlighted blue. */
    if (!USING_VT && !appres.c3270.reverse_video) {
	remap_blue(cohandle, rgbmap);
    }

    /* Set up the controller. */
    ctlr_init(ALL_CHANGE);

    /* Set the window label. */
    if (appres.c3270.title != NULL) {
	screen_title(appres.c3270.title);
    } else if (profile_name != NULL) {
	screen_title(profile_name);
    } else {
	screen_title("wc3270");
    }

    /* Finish screen initialization. */
    set_console_cooked();

    /* Remember the original screen configuration, so we can restore it after we disconnect. */
    orig_model_num = model_num;
    orig_ov_rows = ov_rows;
    orig_ov_cols = ov_cols;
    orig_ov_auto = ov_auto;

    screen_initted = true;
}

/* Allocate a color pair. */
static int
get_color_pair(int fg, int bg)
{
    int mfg = fg & 0xf;
    int mbg = bg & 0xf;

    if (mfg >= MAX_HOST_COLORS) {
	mfg = 0;
    }
    if (mbg >= MAX_HOST_COLORS) {
	mbg = 0;
    }

    if (USING_VT) {
	return (mfg << 4) | mbg;
    } else {
	return cmap_fg[mfg] | cmap_bg[mbg];
    }
}

/*
 * Initialize the user-specified attribute color mappings.
 */
static void
init_user_attribute_color(int *a, const char *resname)
{
    char *r;
    unsigned long l;
    char *ptr;
    int i;

    if ((r = get_resource(resname)) == NULL) {
	return;
    }
    for (i = 0; host_color[i].name != NULL; i++) {
	if (!strcasecmp(r, host_color[i].name)) {
	    *a = host_color[i].index;
	    return;
	}
    }
    l = strtoul(r, &ptr, 0);
    if (ptr == r || *ptr != '\0' || l >= MAX_HOST_COLORS) {
	xs_warning("Invalid %s value: %s", resname, r);
	return;
    }
    *a = (int)l;
}

static void
init_user_attribute_colors(void)
{
    init_user_attribute_color(&field_colors[0], ResHostColorForDefault);
    init_user_attribute_color(&field_colors[1], ResHostColorForIntensified);
    init_user_attribute_color(&field_colors[2], ResHostColorForProtected);
    init_user_attribute_color(&field_colors[3], ResHostColorForProtectedIntensified);
}

/*
 * Map a field attribute to a 3270 color index.
 * Applies only to 3279 mode -- does not work for mono.
 */
static int
color3270_from_fa(unsigned char fa)
{
#   define DEFCOLOR_MAP(f) \
		((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))

    return field_colors[DEFCOLOR_MAP(fa)];
}

/* Map a field attribute to its default colors. */
static int
color_from_fa(unsigned char fa)
{
    if (mode3279) {
	return get_color_pair(color3270_from_fa(fa), HOST_COLOR_NEUTRAL_BLACK);
    } else {
	return get_color_pair((ab_mode == TS_ON || FA_IS_HIGH(fa))? HOST_COLOR_GREEN: HOST_COLOR_PALE_GREEN,
	    HOST_COLOR_NEUTRAL_BLACK);
    }
}

/* Swap foreground and background colors. */
static int
reverse_colors(int a)
{
    if (USING_VT) {
	return ((a >> 4) & 0xf) | ((a & 0xf) << 4);
    } else {
	int rv = 0;

	/* Move foreground colors to background colors. */
	if (a & FOREGROUND_RED) {
	    rv |= BACKGROUND_RED;
	}
	if (a & FOREGROUND_BLUE) {
	    rv |= BACKGROUND_BLUE;
	}
	if (a & FOREGROUND_GREEN) {
	    rv |= BACKGROUND_GREEN;
	}
	if (a & FOREGROUND_INTENSITY) {
	    rv |= BACKGROUND_INTENSITY;
	}

	/* And vice versa. */
	if (a & BACKGROUND_RED) {
	    rv |= FOREGROUND_RED;
	}
	if (a & BACKGROUND_BLUE) {
	    rv |= FOREGROUND_BLUE;
	}
	if (a & BACKGROUND_GREEN) {
	    rv |= FOREGROUND_GREEN;
	}
	if (a & BACKGROUND_INTENSITY) {
	    rv |= FOREGROUND_INTENSITY;
	}

	return rv;
    }
}

/*
 * Set up the user-specified color mappings.
 */
static void
init_user_color(const char *name, int ix)
{
    char *r;
    unsigned long l;
    char *ptr;

    r = get_fresource("%s%s", ResConsoleColorForHostColor, name);
    if (r == NULL) {
	r = get_fresource("%s%d", ResConsoleColorForHostColor, ix);
    }
    if (r == NULL) {
	return;
    }

    l = strtoul(r, &ptr, 0);
    if (ptr != r && *ptr == '\0' && l <= 15) {
	cmap_fg[ix] = (int)l;
	cmap_bg[ix] = (int)l << 4;
	goto success;
    }

    if (r[0] == '#') {
	unsigned long rgbval;
	char *end;

	if (!USING_VT) {
	    xs_warning("Cannot use RGB color specification %s", r);
	    return;
	}
	rgbval = strtoul(r + 1, &end, 16);
	if (end == r + 1 || *end != '\0' || (rgbval & ~0xffffffUL) != 0) {
	    xs_warning("Invalid RGB color '%s'", r);
	    return;
	}
	rgb[ix] = (unsigned)rgbval;
	goto success;
    }

    xs_warning("Invalid %s value '%s'", ResConsoleColorForHostColor, r);
    return;

success:
    if (ix == HOST_COLOR_BLUE) {
	blue_configured = true;
    }
}

/*
 * Crosshair color init.
 */
static void
crosshair_color_init(void)
{
    int c;

    if (appres.interactive.crosshair_color != NULL) {
	c = decode_host_color(appres.interactive.crosshair_color);
	if (c >= 0) {
	    crosshair_color = c;
	    return;
	} else {
	    xs_warning("Invalid %s: %s", ResCrosshairColor,
		    appres.interactive.crosshair_color);
	}
    }
    crosshair_color = HOST_COLOR_PURPLE;
}

static void
init_user_colors(void)
{
    int i;

    /* Set up 3278 RGB colors. */
    if (!mode3279) {
	rgb[HOST_COLOR_PALE_GREEN] = GREEN_3278; /* Normal text. */
	rgb[HOST_COLOR_GREEN] = HIGH_GREEN_3278; /* Highlighted text. */
    }

    /* Look for user-defined overrides. */
    for (i = 0; host_color[i].name != NULL; i++) {
	init_user_color(host_color[i].name, host_color[i].index);
    }
}

/* Invert colors (selections). */
static int
invert_colors(int a)
{
    if (USING_VT) {
	int fg = (a >> 4) & 0xf;

	return (fg == HOST_COLOR_GREY)? get_color_pair(fg, HOST_COLOR_BLACK): get_color_pair(fg, HOST_COLOR_GREY);
    } else {
	unsigned char fg = a &
	    (FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);

	/*
	 * Make the background gray.
	 * If the foreground is gray, make it black.
	 * Otherwise leave it.
	 */
	if (fg == FOREGROUND_INTENSITY) {
	    fg = 0;
	}

	return (a & ~0xff) | BACKGROUND_INTENSITY | fg;
    }
}

/* Apply selection status. */
static int
apply_select(int attr, int baddr)
{
    if (area_is_selected(baddr, 1)) {
	return invert_colors(attr);
    } else {
	return attr;
    }
}

/*
 * Find the display attributes for a baddr, fa_addr and fa.
 */
static int
calc_attrs(int baddr, int fa_addr, int fa, bool *underlined,
	bool *blinking)
{
    int fg, bg, gr, a;

    /* Nondisplay fields are simply blank. */
    if (FA_IS_ZERO(fa)) {
	a = get_color_pair(HOST_COLOR_NEUTRAL_BLACK, HOST_COLOR_NEUTRAL_BLACK);
	goto done;
    }

    /* Compute the color. */

    /* Monochrome is easy, and so is color if nothing is specified. */
    if (!mode3279 ||
	    (!ea_buf[baddr].fg &&
	     !ea_buf[fa_addr].fg &&
	     !ea_buf[baddr].bg &&
	     !ea_buf[fa_addr].bg)) {

	a = color_from_fa(fa);

    } else {

	/* The current location or the fa specifies the fg or bg. */
	if (ea_buf[baddr].fg) {
	    fg = ea_buf[baddr].fg & 0x0f;
	} else if (ea_buf[fa_addr].fg) {
	    fg = ea_buf[fa_addr].fg & 0x0f;
	} else {
	    fg = color3270_from_fa(fa);
	}

	if (ea_buf[baddr].bg) {
	    bg = ea_buf[baddr].bg & 0x0f;
	} else if (ea_buf[fa_addr].bg) {
	    bg = ea_buf[fa_addr].bg & 0x0f;
	} else {
	    bg = HOST_COLOR_NEUTRAL_BLACK;
	}

	a = get_color_pair(fg, bg);
    }

    /* Compute the display attributes. */

    if (ea_buf[baddr].gr) {
	gr = ea_buf[baddr].gr;
    } else if (ea_buf[fa_addr].gr) {
	gr = ea_buf[fa_addr].gr;
    } else {
	gr = 0;
    }

    if (!USING_VT) {
	if (!toggled(UNDERSCORE) &&
		mode3279 &&
		(gr & (GR_BLINK | GR_UNDERLINE)) &&
		!(gr & GR_REVERSE) &&
		!bg) {
	    a |= BACKGROUND_INTENSITY;
	}

	if (!mode3279 &&
		((gr & GR_INTENSIFY) || (ab_mode == TS_ON) || FA_IS_HIGH(fa))) {
	    a |= FOREGROUND_INTENSITY;
	}
    }

    if (gr & GR_REVERSE) {
	a = reverse_colors(a);
    }

    if (toggled(UNDERSCORE)) {
	if (gr & GR_UNDERLINE) {
	    *underlined = true;
	} else {
	    *underlined = false;
	}
    } else {
	*underlined = false;
	if (gr & GR_UNDERLINE) {
	    a |= COMMON_LVB_UNDERSCORE;
	}
    }

    if (toggled(UNDERSCORE) && (gr & GR_BLINK)) {
	*blinking = true;
    } else {
	*blinking = false;
    }

done:
    return a;
}

/*
 * Blink timeout handler.
 */
static void
blink_em(ioid_t id _is_unused)
{
    vctrace(TC_UI, "blink timeout\n");

    /* We're not ticking any more. */
    blink_id = NULL_IOID;
    blink_ticking = false;
    blink_wasticking = false;

    /* Swap blink state and redraw the screen. */
    blink_on = !blink_on;
    screen_changed = true;
    screen_disp(false);
}

/*
 * Cursor blink handler.
 */
static void
cblink_timeout(ioid_t id _is_unused)
{
    vctrace(TC_UI, "cursor blink timeout\n");
    cblink.id = AddTimeOut(CURSOR_BLINK_MS, cblink_timeout);
    cblink.visible = !cblink.visible;
    set_cursor_size(sbuf);
}

/*
 * Map a character onto itself or a space, depending on whether it is supposed
 * to blink and the current global blink state.
 * Note that blinked-off spaces are underscores, if in underscore mode.
 * Also sets up the timeout for the next blink if needed.
 */
static ucs4_t
blinkmap(bool blinking, bool underlined, ucs4_t c)
{
    if (!blinking) {
	return c;
    }
    if (!blink_ticking) {
	blink_id = AddTimeOut(500, blink_em);
	blink_ticking = true;
    }
    return blink_on? c: (underlined? '_': ' ');
}

static ucs4_t
crosshair_blank(int baddr)
{
    if (in_focus && cursor_enabled && toggled(CROSSHAIR)) {
	bool same_row = ((baddr / cCOLS) == (cursor_addr / cCOLS));
	bool same_col = ((baddr % cCOLS) == (cursor_addr % cCOLS));

	if (same_row && same_col) {
	    return LINEDRAW_CROSS;
	} else if (same_row) {
	    return LINEDRAW_HORIZ;
	} else if (same_col) {
	    return LINEDRAW_VERT;
	}
    }
    return ' ';
}

/* Display what's in the buffer. */
void
screen_disp(bool erasing _is_unused)
{
    int row, col;
    int a;
    bool a_underlined = false;
    bool a_blinking = false;
    unsigned char fa;
    enum dbcs_state d;
    int fa_addr;

    /* This may be called when it isn't time yet. */
    if (escaped) {
	return;
    }
    if (!screen_fully_initted) {
	return;
    }

    if (!screen_changed) {

	/* Draw the status line. */
	if (status_row) {
	    draw_oia();
	}

	/* Move the cursor. */
	if (menu_is_up) {
	    menu_cursor(&row, &col);
	    move(row, col);
	} else if (flipped) {
	    move((cursor_addr / cCOLS) + screen_yoffset,
		    cCOLS-1 - (cursor_addr % cCOLS));
	} else {
	    move((cursor_addr / cCOLS) + screen_yoffset,
		    cursor_addr % cCOLS);
	}

	if (status_row) {
	    refresh();
	} else {
	    COORD coord;

	    coord.X = cur_col;
	    coord.Y = cur_row;
	    if (onscreen[ix(cur_row, cur_col)].Attributes &
		    COMMON_LVB_TRAILING_BYTE) {
		coord.X--;
	    }
	    if (SetConsoleCursorPosition(sbuf, coord) == 0) {
		vctrace(TC_UI, "screen_disp: SetConsoleCursorPosition(x=%d,y=%d): %s\n", coord.X, coord.Y,
			win32_strerror(GetLastError()));
	    }
	}

	return;
    }

    /* Draw the menubar first. */
    if (screen_yoffset) {
	ucs4_t u;
	bool highlight;
	unsigned char acs;
	int norm0, high0;
	int norm1, high1;

	if (menu_is_up) {
	    /*
	     * Menu is up. Both rows are white on black for normal,
	     * black on white for highlighted.
	     */
	    if (menu_is_up & KEYPAD_IS_UP) {
		high0 = high1 = get_color_pair(HOST_COLOR_NEUTRAL_BLACK, HOST_COLOR_NEUTRAL_WHITE);
		norm0 = norm1 = get_color_pair(HOST_COLOR_NEUTRAL_WHITE, HOST_COLOR_NEUTRAL_BLACK);
	    } else {
		norm0 = get_color_pair(HOST_COLOR_BLACK, HOST_COLOR_GREY);
		high0 = get_color_pair(HOST_COLOR_NEUTRAL_BLACK, HOST_COLOR_NEUTRAL_WHITE);
		norm1 = get_color_pair(HOST_COLOR_NEUTRAL_WHITE, HOST_COLOR_NEUTRAL_BLACK);
		high1 = get_color_pair(HOST_COLOR_NEUTRAL_BLACK, HOST_COLOR_NEUTRAL_WHITE);
	    }

	} else {
	    /*
	     * Menu is not up.
	     * Row 0 is a gray-background stripe.
	     * Row 1 has a black background.
	     */
	    norm0 = high0 = get_color_pair(
		    appres.c3270.reverse_video? HOST_COLOR_NEUTRAL_WHITE: HOST_COLOR_NEUTRAL_BLACK,
		    HOST_COLOR_GREY);
	    norm1 = high1 = get_color_pair(HOST_COLOR_GREY, HOST_COLOR_NEUTRAL_BLACK);
	}

	for (row = 0; row < screen_yoffset; row++) {
	    int norm, high;

	    move(row, 0);
	    if (row) {
		norm = norm1;
		high = high1;
	    } else {
		norm = norm0;
		high = high0;
	    }
	    for (col = 0; col < maxCOLS; col++) {
		if (menu_char(row, col, true, &u, &highlight, &acs)) {
		    attrset(highlight? high: norm);
		    addch(u);
		} else {
		    attrset(norm);
		    addch(' ');
		}
	    }
	}
    }

    fa = get_field_attribute(0);
    fa_addr = find_field_attribute(0); /* may be -1, that's okay */
    a = calc_attrs(fa_addr, fa_addr, fa, &a_underlined, &a_blinking);
    for (row = 0; row < ROWS; row++) {
	int baddr;

	if (!flipped) {
	    move(row + screen_yoffset, 0);
	}
	for (col = 0; col < cCOLS; col++) {
	    bool underlined = false;
	    bool blinking = false;
	    bool is_menu = false;
	    ucs4_t u;
	    bool highlight;
	    unsigned char acs;

	    if (flipped) {
		move(row + screen_yoffset, cCOLS-1 - col);
	    }

	    is_menu = menu_char(row + screen_yoffset,
		    flipped? (cCOLS-1 - col): col,
		    false,
		    &u, &highlight, &acs);
	    if (is_menu) {
		if (highlight) {
		    attrset(get_color_pair(HOST_COLOR_NEUTRAL_BLACK, HOST_COLOR_NEUTRAL_WHITE));
		} else {
		    attrset(get_color_pair(HOST_COLOR_NEUTRAL_WHITE, HOST_COLOR_NEUTRAL_BLACK));
		}
		addch(u);
	    }

	    baddr = row*cCOLS+col;
	    if (ea_buf[baddr].fa) {
		/* Field attribute. */
		fa_addr = baddr;
		fa = ea_buf[baddr].fa;
		a = calc_attrs(baddr, baddr, fa, &a_underlined, &a_blinking);
		if (!is_menu) {
		    if (toggled(VISIBLE_CONTROL)) {
			attrset(apply_select(get_color_pair(HOST_COLOR_YELLOW, HOST_COLOR_NEUTRAL_BLACK) | COMMON_LVB_UNDERSCORE,
				    baddr));
			addch(visible_fa(fa));
		    } else {
			u = crosshair_blank(baddr);
			if (u != ' ') {
			    attrset(apply_select(xhattr, baddr));
			} else {
			    attrset(apply_select(defattr, baddr));
			}
			addch(u);
		    }
		}
	    } else if (FA_IS_ZERO(fa)) {
		/* Blank. */
		if (!is_menu) {
		    u = crosshair_blank(baddr);
		    if (u == ' ') {
			attrset(apply_select(a, baddr));
		    } else {
			attrset(apply_select(xhattr, baddr));
		    }
		    addch(u);
		}
	    } else {
		int attr_this;

		if (is_menu) {
		    continue;
		}

		/* Normal text. */
		if (!(ea_buf[baddr].gr ||
		      ea_buf[baddr].fg ||
		      ea_buf[baddr].bg)) {
		    attr_this = apply_select(a, baddr);
		    underlined = a_underlined;
		    blinking = a_blinking;
		} else {
		    int b;
		    bool b_underlined;
		    bool b_blinking;

		    /*
		     * Override some of the field
		     * attributes.
		     */
		    b = calc_attrs(baddr, fa_addr, fa, &b_underlined,
			    &b_blinking);
		    attr_this = apply_select(b, baddr);
		    underlined = b_underlined;
		    blinking = b_blinking;
		}
		d = ctlr_dbcs_state(baddr);
		if (is_nvt(&ea_buf[baddr], appres.c3270.ascii_box_draw, &u)) {
		    /* NVT-mode text. */
		    if (IS_LEFT(d)) {
			attrset(attr_this);
			cur_attr |= COMMON_LVB_LEAD_BYTE;
			addch(ea_buf[baddr].ucs4);
			cur_attr &= ~COMMON_LVB_LEAD_BYTE;
			cur_attr |= COMMON_LVB_TRAILING_BYTE;
			addch(ea_buf[baddr].ucs4);
			cur_attr &= ~COMMON_LVB_TRAILING_BYTE;
		    } else if (!IS_RIGHT(d)) {
			if (u == ' ' && in_focus && cursor_enabled && toggled(CROSSHAIR)) {
			    u = crosshair_blank(baddr);
			    if (u != ' ') {
				attr_this = apply_select(xhattr, baddr);
			    }
			}
			if (underlined && u == ' ') {
			    u = '_';
			}
			if (toggled(MONOCASE) && iswlower((int)u)) {
			    u = (ucs4_t)towupper((int)u);
			}
			attrset(attr_this);
			addch(blinkmap(blinking, underlined, u));
		    }
		} else {
		    /* 3270-mode text. */
		    if (IS_LEFT(d)) {
			int xaddr = baddr;

			INC_BA(xaddr);
			if (d == DBCS_LEFT_WRAP) {
			    addch('.');
			} else if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_null &&
				ea_buf[xaddr].ec == EBC_null) {
			    attrset(apply_select(get_color_pair(HOST_COLOR_NEUTRAL_BLACK, HOST_COLOR_YELLOW) |
					COMMON_LVB_UNDERSCORE, baddr));
			    addch('.');
			    addch('.');
			} else {
			    if (menu_char(row + screen_yoffset,
					flipped? (cCOLS-1 - (col+1)): (col+1),
					false,
					&u, &highlight, &acs)) {
				addch(' ');
			    } else {
				u = ebcdic_to_unicode(
					(ea_buf[baddr].ec << 8) |
					ea_buf[xaddr].ec,
					CS_BASE, EUO_NONE);
				attrset(attr_this);
				cur_attr |= COMMON_LVB_LEAD_BYTE;
				addch(u);
				cur_attr &= ~COMMON_LVB_LEAD_BYTE;
				cur_attr |= COMMON_LVB_TRAILING_BYTE;
				addch(u);
				cur_attr &= ~COMMON_LVB_TRAILING_BYTE;
			    }
			}
		    } else if (!IS_RIGHT(d)) {
			if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_null) {
			    attr_this |= COMMON_LVB_UNDERSCORE;
			    u = '.';
			} else if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_so) {
			    attr_this |= COMMON_LVB_UNDERSCORE;
			    u = '<';
			} else if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_si) {
			    attr_this |= COMMON_LVB_UNDERSCORE;
			    u = '>';
			} else {
			    int cs = ea_buf[baddr].cs? ea_buf[baddr].cs:
				(ea_buf[fa_addr].cs? ea_buf[fa_addr].cs: 0);

			    u = ebcdic_to_unicode(ea_buf[baddr].ec, cs,
				    EUO_APL_CIRCLED |
				    (appres.c3270.ascii_box_draw? EUO_ASCII_BOX: 0));
			    if (u == 0) {
				u = crosshair_blank(baddr);
				if (u != ' ') {
				    attr_this = apply_select(xhattr, baddr);
				}
			    } else if (u == ' ' && in_focus && cursor_enabled && toggled(CROSSHAIR)) {
				u = crosshair_blank(baddr);
				if (u != ' ') {
				    attr_this = apply_select(xhattr, baddr);
				}
			    } else if (unicode_is_apl_circled(u)) {
				attr_this |= COMMON_LVB_UNDERSCORE;
				u = unicode_uncircle(u);
			    }
			    if (underlined && u == ' ') {
				u = '_';
			    }
			    if (toggled(MONOCASE) && iswlower((int)u)) {
				u = towupper((int)u);
			    }
			}
			attrset(attr_this);
			addch(blinkmap(blinking, underlined, u));
		    } else if (d == DBCS_RIGHT_WRAP) {
			addch('.');
		    }
		}
	    }
	}
    }
    if (status_row) {
	draw_oia();
    }
    attrset(defattr);
    if (flipped) {
	move((cursor_addr / cCOLS) + screen_yoffset,
		cCOLS-1 - (cursor_addr % cCOLS));
    } else {
	move((cursor_addr / cCOLS) + screen_yoffset,
		cursor_addr % cCOLS);
    }
    refresh();

    screen_changed = false;
}

static void
codepage_changed(bool ignored _is_unused)
{
    screen_changed = true;
    screen_disp(false);
}

static const char *
decode_state(int state, bool limited, const char *skip)
{
    char *space = "";
    varbuf_t r;

    vb_init(&r);
    if (skip == NULL) {
	skip = "";
    }
    if (state & LEFT_CTRL_PRESSED) {
	state &= ~LEFT_CTRL_PRESSED;
	if (strcasecmp(skip, "LeftCtrl")) {
	    vb_appendf(&r, "%sLeftCtrl", space);
	    space = " ";
	}
    }
    if (state & RIGHT_CTRL_PRESSED) {
	state &= ~RIGHT_CTRL_PRESSED;
	if (strcasecmp(skip, "RightCtrl")) {
	    vb_appendf(&r, "%sRightCtrl", space);
	    space = " ";
	}
    }
    if (state & LEFT_ALT_PRESSED) {
	state &= ~LEFT_ALT_PRESSED;
	if (strcasecmp(skip, "LeftAlt")) {
	    vb_appendf(&r, "%sLeftAlt", space);
	    space = " ";
	}
    }
    if (state & RIGHT_ALT_PRESSED) {
	state &= ~RIGHT_ALT_PRESSED;
	if (strcasecmp(skip, "RightAlt")) {
	    vb_appendf(&r, "%sRightAlt", space);
	    space = " ";
	}
    }
    if (state & SHIFT_PRESSED) {
	state &= ~SHIFT_PRESSED;
	if (strcasecmp(skip, "Shift")) {
	    vb_appendf(&r, "%sShift", space);
	    space = " ";
	}
    }
    if (state & NUMLOCK_ON) {
	state &= ~NUMLOCK_ON;
	if (!limited) {
	    vb_appendf(&r, "%sNumLock", space);
	    space = " ";
	}
    }
    if (state & SCROLLLOCK_ON) {
	state &= ~SCROLLLOCK_ON;
	if (!limited) {
	    vb_appendf(&r, "%sScrollLock", space);
	    space = " ";
	}
    }
    if (state & ENHANCED_KEY) {
	state &= ~ENHANCED_KEY;
	if (!limited) {
	    vb_appendf(&r, "%sEnhanced", space);
	    space = " ";
	}
    }
    if (state & !limited) {
	vb_appendf(&r, "%s?0x%x", space, state);
    }

    if (vb_len(&r) == 0) {
	vb_free(&r);
	return "none";
    }
    return txdFree(vb_consume(&r));
}

/* Handle mouse events. */
static void
handle_mouse_event(MOUSE_EVENT_RECORD *me)
{
    int x, y;
    int row, col;
    select_event_t event;
    bool is_alt;

    x = me->dwMousePosition.X;
    y = me->dwMousePosition.Y;

    /* Check for menu selection. */
    if (menu_is_up) {
	if (me->dwEventFlags == 0 &&
		me->dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) {
	    menu_click(x, y);
	}
	return;
    }

    /* Check for menu pop-up. */
    if (screen_yoffset && y == 0) {
	if (me->dwEventFlags == 0 &&
		me->dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) {
	    popup_menu(x, (screen_yoffset != 0));
	    screen_disp(false);
	    return;
	}
    }

    /* Check for TLS pop-up. */
    if (me->dwEventFlags == 0 &&
	    me->dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED &&
	    status_row &&
	    x == rmargin - 28 &&
	    y == status_row) {
	run_action(AnShow, IA_DEFAULT, KwStatus, NULL);
	return;
    }

    /* Figure out what sort of event it is. */
    if ((me->dwEventFlags & DOUBLE_CLICK) &&
	(me->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
	event = SE_DOUBLE_CLICK;
    } else if (me->dwEventFlags & MOUSE_MOVED) {
	event = SE_MOVE;
    } else if (me->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
	event = SE_BUTTON_DOWN;
    } else if (me->dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
	event = SE_RIGHT_BUTTON_DOWN;
    } else if (me->dwButtonState &
	    (FROM_LEFT_2ND_BUTTON_PRESSED |
	     FROM_LEFT_3RD_BUTTON_PRESSED |
	     FROM_LEFT_4TH_BUTTON_PRESSED)) {
	/* We only are about left and right button-down events. */
	return;
    } else {
	event = SE_BUTTON_UP;
    }

    /*
     * Check for out of bounds.
     *
     * Some events we just ignore, but others we map to the edge of the
     * display.
     */
    if ((x >= COLS) ||
	(y - screen_yoffset < 0) ||
	(y - screen_yoffset >= ROWS)) {
	if (event != SE_MOVE && event != SE_BUTTON_UP) {
	    return;
	}
	if (x >= COLS) {
	    x = COLS - 1;
	}
	if (y - screen_yoffset < 0) {
	    y = screen_yoffset;
	}
	if (y - screen_yoffset >= ROWS) {
	    y = screen_yoffset + ROWS - 1;
	}
    }

    /* Compute the buffer coordinates. */
    row = y - screen_yoffset;
    if (flipped) {
	col = COLS - x;
    } else {
	col = x;
    }

    /*
     * Check for lightpen select.
     *
     * The lightPenPrimary resource controls the meaning of left-click with and
     * without the Alt key:
     *
     *                          lightPenSelect
     * Event               false              true
     * --------------- ---------------- -----------------
     * Left-click      Cursor move      Lightpen select
     *                 or copy/select
     *
     * Alt-Left-click  Lightpen select  Cursor move
     *                                  or copy/select
     */
    is_alt = (me->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
	!= 0;
    if (appres.c3270.lightpen_primary? !is_alt: is_alt) {
	if (event == SE_BUTTON_DOWN) {
	    vctrace(TC_UI, " lightpen select\n");
	    lightpen_select((row * COLS) + col);
	}
	return;
    }

    /*
     * Pass it to the selection logic. If the event is not consumed, treat
     * it as a cursor move.
     */
    if (!select_event(row, col, event,
		(me->dwControlKeyState & SHIFT_PRESSED) != 0) && ever_3270) {
	vctrace(TC_UI, " cursor move\n");
	cursor_move((row * COLS) + col);
    }
}

typedef struct {
    const char *name;
    unsigned flag;
} decode_t;

static decode_t decode_button_state[] = {
    { "left1",         FROM_LEFT_1ST_BUTTON_PRESSED },
    { "left2",         FROM_LEFT_2ND_BUTTON_PRESSED },
    { "left3",         FROM_LEFT_3RD_BUTTON_PRESSED },
    { "left4",         FROM_LEFT_4TH_BUTTON_PRESSED },
    { "right",         RIGHTMOST_BUTTON_PRESSED },
    { NULL, 0 }
};
static decode_t decode_control_key_state[] = {
    { "capsLock",      CAPSLOCK_ON },
    { "enhanced",      ENHANCED_KEY },
    { "leftAlt",       LEFT_ALT_PRESSED },
    { "leftCtrl",      LEFT_CTRL_PRESSED },
    { "numLock",       NUMLOCK_ON },
    { "rightAlt",      RIGHT_ALT_PRESSED },
    { "rightCtrl",     RIGHT_CTRL_PRESSED },
    { "scrollLock",    SCROLLLOCK_ON },
    { "shift",         SHIFT_PRESSED },
    { NULL, 0 }
};
static decode_t decode_event_flags[] = {
    { "doubleClick",   DOUBLE_CLICK },
#if defined(MOUSE_HWHEELED) /*[*/
    { "mouseHwheeled", MOUSE_HWHEELED },
#endif /*]*/
    { "mouseMoved",    MOUSE_MOVED },
    { "mouseWheeled",  MOUSE_WHEELED },
    { NULL, 0 }
};

/* Mouse event decoders. */
static const char *
decode_mflags(DWORD flags, decode_t names[])
{
    unsigned f = flags;
    varbuf_t r;
    int i;
    bool any = false;

    vb_init(&r);
    vb_appendf(&r, "0x%x", (unsigned)f);
    for (i = 0; names[i].name != NULL; i++) {
	if (f & names[i].flag) {
	    vb_appendf(&r, "%s%s", any? "|": " ", names[i].name);
	    f &= ~names[i].flag;
	    any = true;
	}
    }
    if (f != 0 && f != flags) {
	vb_appendf(&r, "%s0x%x", any? "|": " ", f);
    }
    return txdFree(vb_consume(&r));
}

/* Return value from do_rr(). */
typedef enum {
    RR_SUCCESS_NOP,	/* success: did nothing */
    RR_SUCCESS_REDRAWN,	/* success: window has been re-drawn */
    RR_FAILURE		/* failure */
} rr_t;

/* Redraw the screen in response to a screen resize event, or when resuming a session. */
static rr_t
do_rr(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD bs;
    int new_console_rows, new_console_cols;
    bool too_short, too_narrow;
    const char *error_message = NULL;

    if (escaped) {
	return RR_SUCCESS_NOP;
    }

    if (GetConsoleScreenBufferInfo(sbuf, &info) == 0) {
	screen_perror_fatal("do_rr GetConsoleScreenBufferInfo");
    }

    new_console_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    new_console_cols = info.srWindow.Right - info.srWindow.Left + 1;
    vctrace(TC_UI, "do_rr old rows %d cols %d new rows %d cols %d 3270 rows %d cols %d size_success %s\n",
	    console_rows, console_cols,
	    new_console_rows, new_console_cols,
	    maxROWS, maxCOLS,
	    size_success? "true": "false");
    console_rows = new_console_rows;
    console_cols = new_console_cols;
    too_short = console_rows < maxROWS;
    too_narrow = console_cols < maxCOLS;
    if (too_narrow || too_short) {
	if (size_success) {
	    error_message = txAsprintf(WINDOW_TOO_SMALL "%s",
		    console_rows, console_cols, maxROWS, maxCOLS,
		    ON_WC? "\nThe display may be corrupt until the window is resized.": "");
	    size_success = false;
	}
    } else {
	size_success = true;
    }

    if (error_message != NULL) {
	vctrace(TC_UI, "do_rr: resize failed\n");
	popup_an_error(error_message);
    } else {
	system("cls");
	screen_system_fixup();
    }

    /* Undo any automatic scrolling. */
    bs.X = console_cols;
    bs.Y = console_rows;
    if (SetConsoleScreenBufferSize(sbuf, bs) == 0) {
	screen_perror_fatal("do_rr SetConsoleScreenBufferSize");
    }

    set_status_row(console_rows, maxROWS);

    allocate_onscreen_toscreen();
    screen_changed = true;
    screen_disp(false);

    return error_message? RR_FAILURE: RR_SUCCESS_REDRAWN;
}

/* Redraw the screen in response to a screen resize event. */
static void
resize_redraw(ioid_t ignored)
{
    redraw_id = NULL_IOID;
    (void) do_rr();
}

/* Keyboard input. */
static void
kybd_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    int rc;
    INPUT_RECORD ir;
    DWORD nr = 0;
    const char *s;
    SHORT x, y;

    /* Get the next input event. */
    vctrace(TC_UI, "Keyboard input: ");
    if (GetNumberOfConsoleInputEvents(chandle, &nr) == 0) {
	screen_perror_fatal("kybd_input GetNumberOfConsoleInputEvents");
    }
    if (nr == 0) {
	vctrace(TC_UI, "GetNumberOfConsoleInputEvents -> 0\n");
	return;
    }

    nr = 0;
    rc = ReadConsoleInputW(chandle, &ir, 1, &nr);
    if (rc == 0) {
	screen_perror_fatal("kybd_input ReadConsoleInput");
    }
    if (nr == 0) {
	vctrace(TC_UI, "ReadConsoleInput -> no events\n");
	return;
    }

    switch (ir.EventType) {
    case FOCUS_EVENT:
	vctrace(TC_UI, "Focus %s\n", ir.Event.FocusEvent.bSetFocus? "set": "unset");
	/*
	 * When we get a focus event, the system may have (incorrectly) redrawn
	 * our window.  Do it again ourselves.
	 *
	 * We also want to redraw to get the crosshair cursor to appear or
	 * disappear.
	 */
	in_focus = (ir.Event.FocusEvent.bSetFocus == TRUE);
	screen_changed = true;
	screen_disp(false);
	break;
    case KEY_EVENT:
	if (!ir.Event.KeyEvent.bKeyDown) {
	    vctrace(TC_UI, "KeyUp, ignoring\n");
	    return;
	}
	s = lookup_cname(ir.Event.KeyEvent.wVirtualKeyCode << 16);
	if (s == NULL) {
	    s = "?";
	}
	vctrace(TC_UI, "Key%s vkey 0x%x (%s) scan 0x%x char U+%04x state 0x%x (%s)\n",
		ir.Event.KeyEvent.bKeyDown? "Down": "Up",
		ir.Event.KeyEvent.wVirtualKeyCode, s,
		ir.Event.KeyEvent.wVirtualScanCode,
		ir.Event.KeyEvent.uChar.UnicodeChar,
		(int)ir.Event.KeyEvent.dwControlKeyState,
		decode_state(ir.Event.KeyEvent.dwControlKeyState, false,
		    NULL));
	if (!ir.Event.KeyEvent.bKeyDown) {
	    return;
	}
	kybd_input2(&ir);
	break;
    case MENU_EVENT:
	vctrace(TC_UI, "Menu\n");
	break;
    case MOUSE_EVENT:
	vctrace(TC_UI, "Mouse (%d,%d) ButtonState %s "
		"ControlKeyState %s EventFlags %s\n",
		ir.Event.MouseEvent.dwMousePosition.X,
		ir.Event.MouseEvent.dwMousePosition.Y,
		decode_mflags(ir.Event.MouseEvent.dwButtonState,
		    decode_button_state),
		decode_mflags(ir.Event.MouseEvent.dwControlKeyState,
		    decode_control_key_state),
		decode_mflags(ir.Event.MouseEvent.dwEventFlags,
		    decode_event_flags));
	handle_mouse_event(&ir.Event.MouseEvent);
	break;
    case WINDOW_BUFFER_SIZE_EVENT:
	x = ir.Event.WindowBufferSizeEvent.dwSize.X;
	y = ir.Event.WindowBufferSizeEvent.dwSize.Y;
	vctrace(TC_UI, "WindowBufferSize rows %d cols %d\n", y, x);
	if (redraw_id != NULL_IOID) {
	    RemoveTimeOut(redraw_id);
	}
	redraw_id = AddTimeOut(500, resize_redraw);
	/*resize_redraw(NULL_IOID);*/
	break;
    default:
	vctrace(TC_UI, "Unknown input event %d\n", ir.EventType);
	break;
    }
}

static void
trace_as_keymap(unsigned long xk, KEY_EVENT_RECORD *e)
{
    const char *s;
    varbuf_t r;

    vb_init(&r);
    vb_appendf(&r, "[xk 0x%lx] ", xk);
    s = decode_state(e->dwControlKeyState, true, NULL);
    if (strcmp(s, "none")) {
	vb_appendf(&r, "%s ", s);
    }
    if (xk & 0xffff0000) {
	const char *n = lookup_cname(xk);

	vb_appendf(&r, "<Key>%s", n? n: "???");
    } else if (xk > 0x7f) {
	wchar_t w = (wchar_t)xk;
	char c;
	BOOL udc = FALSE;

	/*
	 * Translate to the ANSI codepage for storage in the trace
	 * file.  It will be converted to OEM by 'catf' for display
	 * in the trace window.
	 */
	WideCharToMultiByte(CP_ACP, 0, &w, 1, &c, 1, "?", &udc);
	if (udc) {
	    vb_appendf(&r, "<Key>U+%04lx", xk);
	} else {
	    vb_appendf(&r, "<Key>%c", (unsigned char)xk);
	}
    } else if (xk < ' ') {
	/* assume dwControlKeyState includes Ctrl... */
	vb_appendf(&r, "<Key>%c", (unsigned char)xk + '@');
    } else if (xk == ' ') {
	vb_appendf(&r, "<Key>space");
    } else if (xk == ':') {
	vb_appendf(&r, "<Key>colon");
    } else {
	vb_appendf(&r, "<Key>%c", (unsigned char)xk);
    }
    vctrace(TC_UI, " %s ->", txdFree(vb_consume(&r)));
}

/* Translate a Windows virtual key to a menubar abstract key. */
static menu_key_t
key_to_mkey(int k)
{
    switch (k) {
    case VK_UP:
	return MK_UP;
    case VK_DOWN:
	return MK_DOWN;
    case VK_LEFT:
	return MK_LEFT;
    case VK_RIGHT:
	return MK_RIGHT;
    case VK_HOME:
	return MK_HOME;
    case VK_END:
	return MK_END;
    case VK_RETURN:
	return MK_ENTER;
    case 0:
	return MK_NONE;
    default:
	return MK_OTHER;
    }
}

static void
kybd_input2(INPUT_RECORD *ir)
{
    int k;
    unsigned long xk;
    char *action;

    /* First see if this is a select/copy completion. */
    if (ir->Event.KeyEvent.wVirtualKeyCode == VK_RETURN &&
	    select_return_key()) {
	return;
    }

    /*
     * Translate the INPUT_RECORD into an integer we can match keymaps
     * against.
     *
     * If VK and ASCII are the same and are a control char, use VK.
     * If VK is 0x6x, use VK.  These are aliases like ADD and NUMPAD0.
     * Otherwise, if there's Unicode, use it.
     * Otherwise, use VK.
     */
    if ((ir->Event.KeyEvent.wVirtualKeyCode ==
		    ir->Event.KeyEvent.uChar.AsciiChar) &&
		ir->Event.KeyEvent.wVirtualKeyCode < ' ') {
	xk = (ir->Event.KeyEvent.wVirtualKeyCode << 16) & 0xffff0000;
    } else if ((ir->Event.KeyEvent.wVirtualKeyCode & 0xf0) == 0x60) {
	xk = (ir->Event.KeyEvent.wVirtualKeyCode << 16) & 0xffff0000;
    } else if (ir->Event.KeyEvent.uChar.UnicodeChar) {
	xk = ir->Event.KeyEvent.uChar.UnicodeChar;
    } else if (ir->Event.KeyEvent.wVirtualKeyCode >= 0x30 &&
		    ir->Event.KeyEvent.wVirtualKeyCode <= 0x5a) {
	xk = ir->Event.KeyEvent.wVirtualKeyCode;
    } else {
	xk = (ir->Event.KeyEvent.wVirtualKeyCode << 16) & 0xffff0000;
    }

    if (menu_is_up) {
	menu_key(key_to_mkey(xk >> 16), xk & 0xffff);
	return;
    }

    if (xk) {
	trace_as_keymap(xk, &ir->Event.KeyEvent);
	action = lookup_key(xk, ir->Event.KeyEvent.dwControlKeyState);
	if (action != NULL) {
	    if (strcmp(action, "[ignore]")) {
		push_keymap_action(action);
	    }
	    return;
	}
    }

    ia_cause = IA_DEFAULT;

    k = ir->Event.KeyEvent.wVirtualKeyCode;

    /* These first cases apply to both 3270 and NVT modes. */
    switch (k) {
    case VK_ESCAPE:
	vctrace(TC_UI, " Default -> " AnEscape "()\n");
	run_action(AnEscape, IA_DEFAULT, NULL, NULL);
	return;
    case VK_UP:
	vctrace(TC_UI, " Default -> " AnUp "()\n");
	run_action(AnUp, IA_DEFAULT, NULL, NULL);
	return;
    case VK_DOWN:
	vctrace(TC_UI, " Default -> " AnDown "()\n");
	run_action(AnDown, IA_DEFAULT, NULL, NULL);
	return;
    case VK_LEFT:
	vctrace(TC_UI, " Default -> " AnLeft "()\n");
	run_action(AnLeft, IA_DEFAULT, NULL, NULL);
	return;
    case VK_RIGHT:
	vctrace(TC_UI, " Default -> " AnRight "()\n");
	run_action(AnRight, IA_DEFAULT, NULL, NULL);
	return;
    case VK_HOME:
	vctrace(TC_UI, " Default -> " AnHome "()\n");
	run_action(AnHome, IA_DEFAULT, NULL, NULL);
	return;
    default:
	break;
    }

    /* Then look for 3270-only cases. */
    if (IN_3270) {
	switch(k) {
	/* These cases apply only to 3270 mode. */
	case VK_TAB:
	    vctrace(TC_UI, " Default -> " AnTab "()\n");
	    run_action(AnTab, IA_DEFAULT, NULL, NULL);
	    return;
	case VK_DELETE:
	    vctrace(TC_UI, " Default -> " AnDelete "()\n");
	    run_action(AnDelete, IA_DEFAULT, NULL, NULL);
	    return;
	case VK_BACK:
	    vctrace(TC_UI, " Default -> " AnBackSpace "()\n");
	    run_action(AnBackSpace, IA_DEFAULT, NULL, NULL);
	    return;
	case VK_RETURN:
	    vctrace(TC_UI, " Default -> " AnEnter "()\n");
	    run_action(AnEnter, IA_DEFAULT, NULL, NULL);
	    return;
	default:
	    break;
	}
    }

    /* Catch PF keys. */
    if (k >= VK_F1 && k <= VK_F24) {
	const char *s = txAsprintf("%d", k - VK_F1 + 1);

	vctrace(TC_UI, " Default -> " AnEnter "(%s)\n", s);
	run_action(AnPF, IA_DEFAULT, s, NULL);
	return;
    }

    /* Then any other character. */
    if (ir->Event.KeyEvent.uChar.UnicodeChar) {
	const char *s = txAsprintf("U+%04x", ir->Event.KeyEvent.uChar.UnicodeChar);

	vctrace(TC_UI, " Default -> " AnKey "(%s)\n", s);
	run_action(AnKey, IA_DEFAULT, s, NULL);
    } else {
	vctrace(TC_UI, " dropped (no default)\n");
    }
}

/*
 * Undo the last step of screen initialization.
 *
 * This ensures that the next screen_resume() call will call finish_screen_init(), which
 * will cause all of the screen resizing logic to be run again.
 */
static void
screen_reset(void)
{
    if (sbuf != NULL) {
	vctrace(TC_UI, "screen_reset\n");
	CloseHandle(sbuf);
	sbuf = NULL;
	screen_fully_initted = false;
    }
}


bool
screen_suspend(void)
{
    static bool need_to_scroll = false;

    vctrace(TC_UI, "screen_suspend\n");

    if (!isendwin) {
	endwin();
    }

    if (!escaped) {
	escaped = true;

	if (need_to_scroll) {
	    printf("\n");
	} else {
	    need_to_scroll = true;
	}
	RemoveInput(input_id);

	if (!PCONNECTED) {
	    /* Start over with screen initialization. */
	    screen_reset();
	}
    }

    return false;
}

/*
 * Get mouse events back after calling system(), which apparently cancels them.
 */
void
screen_system_fixup(void)
{
    if (!escaped) {
	if (SetConsoleMode(chandle, ENABLE_PROCESSED_INPUT |
				    ENABLE_MOUSE_INPUT |
				    ENABLE_EXTENDED_FLAGS) == 0) {
	    vctrace(TC_UI, "screen_system_fixup SetConsoleMode: %s\n", win32_strerror(GetLastError()));
	}
    }
}

bool
screen_resume(void)
{
    vctrace(TC_UI, "screen_resume\n");
    if (!escaped) {
	return true;
    }

    if (!screen_fully_initted) {
	if (!finish_screen_init()) {
	    return false;
	}
    }

    escaped = false;

    /* Redraw the screen. */
    screen_changed = true;
    onscreen_valid = false;
    screen_disp(false);

    /*
     * On WT, bounce back to the prompt if the window is now too small.
     *
     * N.B.: We do not do this on Windows Console when it is running in VT mode. This is because one big
     *       difference between WC and WT is the relationship between screen buffers. Screen buffers in
     *       WC are independent -- switching between them changes the window size -- so resizing the
     *       window at the wc3270> prompt (cohandle) does not affect the size of the emulator screen
     *       buffer (sbuf).
     *
     *       By contrast, allocated screen buffers in WT are constrained by the default screen buffer.
     *       If they are smaller than the default buffer, they are letterboxed into the upper left, and
     *       if they are bigger than the default, they are clipped.
     *
     *       This is the only part of wc3270 screen behavior that is different between real WT and
     *       WC-in-VT-mode.
     */
    if (ON_WT) {
	size_success = true;
	switch (do_rr()) {
	case RR_SUCCESS_NOP:
	case RR_SUCCESS_REDRAWN:
	    break;
	case RR_FAILURE:
	    escaped = true;
	    return false;
	}
    }

    input_id = AddInput(chandle, kybd_input);

    /* Turn on mouse input and turn off quick edit mode. */
    if (SetConsoleMode(chandle, ENABLE_PROCESSED_INPUT |
				ENABLE_MOUSE_INPUT |
				ENABLE_EXTENDED_FLAGS) == 0) {
	screen_perror_fatal("screen_resume SetConsoleMode(CONIN$)");
    }

    /* Turn off line wrap. */
    if (SetConsoleMode(cohandle, ENABLE_PROCESSED_OUTPUT |
				 ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
	screen_perror_fatal("screen_resume SetConsoleMode(CONOUT$)");
    }
    if (SetConsoleMode(sbuf, ENABLE_PROCESSED_OUTPUT |
			     ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
	screen_perror_fatal("screen_resume SetConsoleMode(sbuf)");
    }

    vctrace(TC_UI, "screen_resume succeeded\n");
    return true;
}

void
cursor_move(int baddr)
{
    cursor_addr = baddr;
    if (in_focus && cursor_enabled && toggled(CROSSHAIR)) {
	screen_changed = true;
	screen_disp(false);
    }
}

static void
toggle_altCursor(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    if (!isendwin) {
	set_cursor_size(sbuf);
    }
}

/*
 * The internals of enabling or disabling cursor blink.
 */
static void
set_cblink(bool mode)
{
    if (!USING_VT) {
	vctrace(TC_UI, "set_cblink(%s)\n", mode? "true": "false");
	if (mode) {
	    /* Turn it on. */
	    if (cblink.id == NULL_IOID) {
		cblink.id = AddTimeOut(CURSOR_BLINK_MS, cblink_timeout);
	    }
	} else {
	    /* Turn it off. */
	    if (cblink.id != NULL_IOID) {
		RemoveTimeOut(cblink.id);
		cblink.id = NULL_IOID;
	    }
	    if (!cblink.visible) {
		cblink.visible = true;
		set_cursor_size(sbuf);
	    }
	}
    }
}

static void
toggle_cursorBlink(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    if (isendwin) {
	return;
    }
    set_cblink(toggled(CURSOR_BLINK));
}

static void
toggle_monocase(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    screen_changed = true;
    screen_disp(false);
}

static void
toggle_underscore(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    screen_changed = true;
    screen_disp(false);
}

static void
toggle_crosshair(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    screen_changed = true;
    screen_disp(false);
}

/**
 * Toggle timing display.
 */
static void
toggle_showTiming(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    if (!toggled(SHOW_TIMING)) {
	status_untiming();
    }
}

static void
toggle_visibleControl(toggle_index_t ix _is_unused,
	enum toggle_type tt _is_unused)
{
    screen_changed = true;
    screen_disp(false);
}

/* Status line stuff. */

static bool status_ta = false;
static bool status_rm = false;
static bool status_im = false;
static enum {
    SS_INSECURE,
    SS_UNVERIFIED,
    SS_SECURE
} status_secure = SS_INSECURE;
static bool oia_boxsolid = false;
static bool oia_undera = true;
static bool oia_compose = false;
static bool oia_printer = false;
static ucs4_t oia_compose_char = 0;
static enum keytype oia_compose_keytype = KT_STD;
#define LUCNT	8
static char oia_lu[LUCNT+1];
static char oia_timing[6]; /* :ss.s*/
static char oia_screentrace = ' ';
static char oia_script = ' ';

static ioid_t info_done_timeout = NULL_IOID;
static ioid_t info_scroll_timeout = NULL_IOID;

void
status_ctlr_done(void)
{
    oia_undera = true;
}

void
status_insert_mode(bool on)
{
    status_im = on;
}

/* Remove the info message. */
static void
info_done(ioid_t id _is_unused)
{
    Replace(info_base_msg, NULL);
    info_msg = NULL;
    info_done_timeout = NULL_IOID;
}

/* Scroll the info message. */
static void
info_scroll(ioid_t id _is_unused)
{
    ++info_msg;
    if (strlen(info_msg) > 35) {
	info_scroll_timeout = AddTimeOut(STATUS_SCROLL_MS, info_scroll);
    } else {
	info_done_timeout = AddTimeOut(STATUS_PUSH_MS, info_done);
	info_scroll_timeout = NULL_IOID;
    }
}

/* Pop up an info message. */
void
status_push(char *msg)
{
    char *new_msg = msg? NewString(msg): NULL;

    Replace(info_base_msg, new_msg);
    info_msg = info_base_msg;

    if (info_scroll_timeout != NULL_IOID) {
	RemoveTimeOut(info_scroll_timeout);
	info_scroll_timeout = NULL_IOID;
    }
    if (info_done_timeout != NULL_IOID) {
	RemoveTimeOut(info_done_timeout);
	info_done_timeout = NULL_IOID;
    }
}

/*
 * Reset the info message, so when it is revealed, it starts at the beginning.
 */
static void
reset_info(void)
{
    if (info_base_msg != NULL) {
	info_msg = info_base_msg;
    }

    /* Stop any timers. */
    if (info_scroll_timeout != NULL_IOID) {
	RemoveTimeOut(info_scroll_timeout);
	info_scroll_timeout = NULL_IOID;
    }
    if (info_done_timeout != NULL_IOID) {
	RemoveTimeOut(info_done_timeout);
	info_done_timeout = NULL_IOID;
    }
}

/*
 * The info message has been displayed. Set the timer to scroll or erase it.
 */
static void
set_info_timer(void)
{
    if (info_scroll_timeout != NULL_IOID || info_done_timeout != NULL_IOID) {
	return;
    }
    if (strlen(info_msg) > 35) {
	info_scroll_timeout = AddTimeOut(STATUS_SCROLL_START_MS, info_scroll);
    } else {
	info_done_timeout = AddTimeOut(STATUS_PUSH_MS, info_done);
    }
}

void
status_minus(void)
{
    other_msg = "X -f";
    other_attr = oia_red_attr;
}

void
status_oerr(int error_type)
{
    switch (error_type) {
    case KL_OERR_PROTECTED:
	other_msg = "X Protected";
	break;
    case KL_OERR_NUMERIC:
	other_msg = "X NUM";
	break;
    case KL_OERR_OVERFLOW:
	other_msg = "X Overflow";
	break;
    case KL_OERR_DBCS:
	other_msg = "X DBCS";
	break;
    }
    other_attr = oia_red_attr;
}

void
status_reset(void)
{
    status_connect(PCONNECTED);
}

void
status_reverse_mode(bool on)
{
    status_rm = on;
}

void
status_syswait(void)
{
    other_msg = "X SYSTEM";
    other_attr = oia_white_attr;
}

void
status_twait(void)
{
    oia_undera = false;
    other_msg = "X Wait";
    other_attr = oia_white_attr;
}

void
status_typeahead(bool on)
{
    status_ta = on;
}

void    
status_compose(bool on, ucs4_t ucs4, enum keytype keytype)
{
    oia_compose = on;
    oia_compose_char = ucs4;
    oia_compose_keytype = keytype;
}

void
status_lu(const char *lu)
{
    if (lu != NULL) {
	strncpy(oia_lu, lu, LUCNT);
	oia_lu[LUCNT] = '\0';
    } else {
	memset(oia_lu, '\0', sizeof(oia_lu));
    }
}

static void
status_connect(bool connected)
{
    if (connected) {
	oia_boxsolid = IN_3270 && !IN_SSCP;
	if (cstate == RECONNECTING) {
	    other_msg = "X Reconnecting";
	} else if (cstate == RESOLVING) {
	    other_msg = "X [DNS]";
	} else if (cstate == TCP_PENDING) {
	    other_msg = "X [TCP]";
	    oia_boxsolid = false;
	    status_secure = SS_INSECURE;
	} else if (cstate == TLS_PENDING) {
	    other_msg = "X [TLS]";
	    oia_boxsolid = false;
	    status_secure = SS_INSECURE;
	} else if (cstate == PROXY_PENDING) {
	    other_msg = "X [Proxy]";
	    oia_boxsolid = false;
	    status_secure = SS_INSECURE;
	} else if (cstate == TELNET_PENDING) {
	    other_msg = "X [TELNET]";
	    oia_boxsolid = false;
	    status_secure = SS_INSECURE;
	} else if (cstate == CONNECTED_UNBOUND) {
	    other_msg = "X [TN3270E]";
	} else if (kybdlock & KL_AWAITING_FIRST) {
	    other_msg = "X [Field]";
	} else if (kybdlock & (KL_ENTER_INHIBIT | KL_BID)) {
	    other_msg = "X Wait";
	} else if (kybdlock & KL_FT) {
	    other_msg = "X File Transfer";
	} else if (kybdlock & KL_DEFERRED_UNLOCK) {
	    other_msg = "X";
	} else {
	    other_msg = NULL;
	}
	if (net_secure_connection()) {
	    if (net_secure_unverified()) {
		status_secure = SS_UNVERIFIED;
	    } else {
		status_secure = SS_SECURE;
	    }
	} else {
	    status_secure = SS_INSECURE;
	}
    } else {
	oia_boxsolid = false;
	other_msg = "X Not Connected";
	status_secure = SS_INSECURE;
    }       
    other_attr = oia_white_attr;
    status_untiming();
}

static void
status_3270_mode(bool ignored _is_unused)
{
    oia_boxsolid = IN_3270 && !IN_SSCP;
    if (oia_boxsolid) {
	oia_undera = true;
    }
    status_connect(CONNECTED);
}

static void
status_printer(bool on)
{
    oia_printer = on;
}

void
status_timing(struct timeval *t0, struct timeval *t1)
{
    static char no_time[] = ":??.?";

    if (t1->tv_sec - t0->tv_sec > (99*60)) {
	strncpy(oia_timing, no_time, sizeof(oia_timing));
    } else {
	unsigned long cs;	/* centiseconds */

	cs = (t1->tv_sec - t0->tv_sec) * 10 +
	     (t1->tv_usec - t0->tv_usec + 50000) / 100000;
	if (cs < CM) {
	    snprintf(oia_timing, sizeof(oia_timing), ":%02ld.%ld", cs / 10, cs % 10);
	} else {
	    snprintf(oia_timing, sizeof(oia_timing), "%02ld:%02ld", cs / CM, (cs % CM) / 10);
	}
    }
    oia_timing[sizeof(oia_timing) - 1] = '\0';
}

void
status_untiming(void)
{
    oia_timing[0] = '\0';
}

void
status_scrolled(int n)
{
    if (n) {
	Replace(scrolled_msg, Asprintf("X Scrolled %d", n));
    } else {
	Replace(scrolled_msg, NULL);
    }
}

/* Remove 'X Disabled'. */
static void
disabled_done(ioid_t id _is_unused)
{
    disabled_msg = NULL;
    disabled_done_id = NULL_IOID;
}

/* Flash 'X Disabled' in the OIA. */
void
status_keyboard_disable_flash(void)
{
    if (disabled_done_id == NULL_IOID) {
	disabled_msg = "X Disabled";
    } else {
	RemoveTimeOut(disabled_done_id);
	disabled_done_id = NULL_IOID;
    }
    disabled_done_id = AddTimeOut(1000L, disabled_done);
}

void
status_screentrace(int n)
{
    if (n < 0) {
	oia_screentrace = ' ';
    } else if (n < 9) {
	oia_screentrace = "123456789"[n];
    } else {
	oia_screentrace = '+';
    }
}

void
status_script(bool on _is_unused)
{
    oia_script = on? 's': ' ';
}

static void
draw_oia(void)
{
    int i, j;
    int cursor_col = (cursor_addr % cCOLS);
    int fl_cursor_col = flipped? (console_cols - 1 - cursor_col): cursor_col;
    char *status_msg_now;
    int msg_attr;

    rmargin = maxCOLS - 1;

    /* Fill in the gap between the screen and the OIA. */
    attrset(defattr);
    for (i = maxROWS + screen_yoffset; i < status_row; i++) {
	move(i, 0);
	for (j = 0; j <= rmargin; j++) {
	    addch(' ');
	}
    }

    /* Extend or erase the crosshair. */
    attrset(xhattr);
    if (in_focus && cursor_enabled && toggled(CROSSHAIR)) {
	if (!menu_is_up &&
		(mvinch(0, fl_cursor_col) & A_CHARTEXT) == ' ') {
	    attrset(get_color_pair(crosshair_color, HOST_COLOR_GREY));
	    addch(LINEDRAW_VERT);
	    attrset(xhattr);
	}
	if (screen_yoffset > 1 &&
		(mvinch(1, fl_cursor_col) & A_CHARTEXT) == ' ') {
	    addch(LINEDRAW_VERT);
	}
    }
    for (i = ROWS + screen_yoffset; i < status_row; i++) {
	for (j = 0; j < maxCOLS && j < console_cols; j++) {
	    move(i, j);
	    if (in_focus && cursor_enabled && toggled(CROSSHAIR) && (j == fl_cursor_col)) {
		addch(LINEDRAW_VERT);
	    } else {
		addch(' ');
	    }
	}
    }
    for (i = 0; i < ROWS; i++) {
	for (j = cCOLS; j < maxCOLS && j < console_cols; j++) {
	    move(i + screen_yoffset, j);
	    if (in_focus && cursor_enabled && toggled(CROSSHAIR) && i == (cursor_addr / cCOLS)) {
		addch(LINEDRAW_HORIZ);
	    } else {
		addch(' ');
	    }
	}
    }

    /* Make sure the status line region is filled in properly. */
    attrset(defattr);
    if (status_skip) {
	move(status_skip, 0);
	attrset(COMMON_LVB_UNDERSCORE | oia_attr);
	for (j = 0; j <= rmargin; j++) {
	    if (j == cursor_col && in_focus && cursor_enabled && toggled(CROSSHAIR)) {
		attrset(xhattr);
		addch(LINEDRAW_VERT);
		attrset(COMMON_LVB_UNDERSCORE | oia_attr);
	    } else {
		addch(' ');
	    }
	}
    }

    if (!status_row) {
	return;
    }
    move(status_row, 0);
    attrset(defattr);
    for (j = 0; j <= rmargin; j++) {
	addch(' ');
    }

    /* Offsets 0, 1, 2 */
    attrset(mode3279? reverse_colors(oia_attr): reverse_colors(defattr));
    mvprintw(status_row, 0, "4");
    attrset(COMMON_LVB_UNDERSCORE | oia_attr);
    if (oia_undera) {
	addch(IN_E? 'B': 'A');
    } else {
	addch(' ');
    }
    attrset(mode3279? reverse_colors(oia_attr): reverse_colors(defattr));
    if (IN_NVT) {
	addch('N');
    } else if (oia_boxsolid) {
	addch(' ');
    } else if (IN_SSCP) {
	addch('S');
    } else {
	addch('?');
    }

    /* Figure out the status message. */
    msg_attr = oia_attr;
    if (disabled_msg != NULL) {
	msg_attr = oia_red_attr;
	status_msg_now = disabled_msg;
	reset_info();
    } else if (scrolled_msg != NULL) {
	msg_attr = oia_white_attr;
	status_msg_now = scrolled_msg;
	reset_info();
    } else if (info_msg != NULL) {
	msg_attr = oia_white_attr;
	status_msg_now = info_msg;
	set_info_timer();
    } else if (other_msg != NULL) {
	msg_attr = other_attr;
	status_msg_now = other_msg;
    } else {
	status_msg_now = "";
    }

    /* Offset 8 */
    attrset(msg_attr);
    mvprintw(status_row, 7, "%-35.35s", status_msg_now);
    attrset(oia_attr);
    mvprintw(status_row, rmargin-36,
	    "%c%c %c%c%c%c",
	    oia_compose? 'C': ' ',
	    oia_compose? oia_compose_char: ' ', /* XXX */
	    status_ta? 'T': ' ',
	    status_rm? 'R': ' ',
	    status_im? 'I': ' ',
	    oia_printer? 'P': ' ');
    if (status_secure != SS_INSECURE) {
	attrset(mode3279?
		get_color_pair((status_secure == SS_SECURE)? HOST_COLOR_GREEN: HOST_COLOR_YELLOW,
		    HOST_COLOR_NEUTRAL_BLACK):
		oia_attr);
	addch('S');
	attrset(oia_attr);
    } else {
	addch(' ');
    }
    addch(oia_screentrace);
    addch(oia_script);

    mvprintw(status_row, rmargin-25, "%s", oia_lu);

    mvprintw(status_row, rmargin-14, "%s", oia_timing);

    mvprintw(status_row, rmargin-7,
	    "%03d/%03d", cursor_addr/cCOLS + 1, cursor_addr%cCOLS + 1);

    /* Now fill in the crosshair cursor in the status line. */
    if (in_focus &&
	    cursor_enabled &&
	    toggled(CROSSHAIR) &&
	    cursor_col > 2 &&
	    fl_cursor_col < console_cols &&
	    (mvinch(status_row, fl_cursor_col) & A_CHARTEXT) == ' ') {
	attrset(xhattr);
	addch(LINEDRAW_VERT);
    }
}

static bool
Redraw_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Redraw", ia, argc, argv);
    if (check_argc("Redraw", argc, 0, 0) < 0) {
	return false;
    }

    if (!escaped) {
	onscreen_valid = false;
	refresh();
    }
    return true;
}

void
ring_bell(void)
{
    static enum {
	BELL_NOTHING = 0,	/* do nothing */
	BELL_KNOWN = 0x1,	/* have we decoded the option? */
	BELL_BEEP = 0x2,	/* ring the annoying console bell */
	BELL_FLASH = 0x4	/* flash the screen or icon */
    } bell_mode = 0;

    if (!(bell_mode & BELL_KNOWN)) {
	if (appres.c3270.bell_mode != NULL) {
	    /*
	     * New config: wc3270.bellMode
	     * 		none		do nothing
	     * 		beep		just beep
	     * 		flash		just flash
	     * 		beepFlash	beep and flash
	     * 		flashBeep	beep and flash
	     * 		anything else	do nothing
	     */
	    if (!strcasecmp(appres.c3270.bell_mode, "none")) {
		bell_mode = BELL_NOTHING;
	    } else if (!strcasecmp(appres.c3270.bell_mode, "beep")) {
		bell_mode = BELL_BEEP;
	    } else if (!strcasecmp(appres.c3270.bell_mode, "flash")) {
		bell_mode = BELL_FLASH;
	    } else if (!strcasecmp(appres.c3270.bell_mode, "beepFlash") ||
		       !strcasecmp(appres.c3270.bell_mode, "flashBeep")) {
		bell_mode = BELL_BEEP | BELL_FLASH;
	    } else {
		/*
		 * Should cough up a warning here, but it's
		 * a bit late.
		 */
		bell_mode = BELL_NOTHING;
	    }
	} else {
	    /*
	     * No config: beep and flash.
	     */
	    bell_mode = BELL_BEEP | BELL_FLASH;
	}

	/* In any case, only do this once. */
	bell_mode |= BELL_KNOWN;
    }

    if ((bell_mode & BELL_FLASH) && console_window != NULL) {
	FLASHWINFO w;

	memset(&w, '\0', sizeof(FLASHWINFO));
	w.cbSize = sizeof(FLASHWINFO);
	w.hwnd = console_window;
	w.dwFlags = FLASHW_ALL;
	w.uCount = 2;
	w.dwTimeout = 250; /* 1/4s */

	FlashWindowEx(&w);
    }

    if (bell_mode & BELL_BEEP) {
	MessageBeep(-1);
    }
}

void
screen_flip(void)
{
    flipped = !flipped;
    screen_changed = true;
    screen_disp(false);
}

bool
screen_flipped(void)
{
    return flipped;
}

/*
 * Windows-specific Paste action, that takes advantage of the existing x3270
 * instrastructure for multi-line paste.
 */
static bool
Paste_action(ia_t ia, unsigned argc, const char **argv)
{
    HGLOBAL hglb;
    LPTSTR lptstr;
    UINT format = CF_UNICODETEXT;
    bool margin = true;

    action_debug(AnPaste, ia, argc, argv);
    if (check_argc(AnPaste, argc, 0, 1) < 0) {
	return false;
    }

    if (argc > 0) {
	if (!strcasecmp(argv[0], KwNoMargin)) {
	    margin = false;
	} else {
	    popup_an_error(AnPaste "(): Unknown option");
	    return false;
	}
    }

    if (!IsClipboardFormatAvailable(format)) {
	return false;
    }
    if (!OpenClipboard(NULL)) {
	return false;
    }
    hglb = GetClipboardData(format);
    if (hglb != NULL) {
	lptstr = GlobalLock(hglb);
	if (lptstr != NULL) { 
	    int sl = 0;
	    wchar_t *w = (wchar_t *)lptstr;
	    ucs4_t *u;
	    ucs4_t *us;
	    int i;

	    for (i = 0; *w != 0x0000; i++, w++) {
		sl++;
	    }
	    us = u = Malloc(sl * sizeof(ucs4_t));

	    /*
	     * Expand from UCS-2 to UCS-4.
	     * XXX: It isn't UCS-2, it's UTF-16.
	     */
	    w = (wchar_t *)lptstr;
	    for (i = 0; i < sl; i++) {
		*us++ = *w++;
	    }
	    emulate_uinput(u, sl, true, margin);
	    Free(u);
	}
	GlobalUnlock(hglb); 
    }
    CloseClipboard(); 

    return true;
}

/* Set the window title. */
static void
set_console_title(const char *text, bool selecting)
{
    if (selecting) {
	SetConsoleTitle(txAsprintf("%s [select]", text));
    } else {
	SetConsoleTitle(text);
    }
}

void
screen_title(const char *text)
{
    Replace(window_title, NewString(text));
    set_console_title(text, selecting);
}

static bool
Title_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnTitle, ia, argc, argv);
    if (check_argc(AnTitle, argc, 1, 1) < 0) {
	return false;
    }

    screen_title(argv[0]);
    return true;
}

static void
relabel(bool ignored _is_unused)
{
    if (appres.c3270.title != NULL) {
	return;
    }

    if (PCONNECTED) {
	char *hostname;

	if (profile_name != NULL) {
	    hostname = profile_name;
	} else {
	    hostname = reconnect_host;
	}

	screen_title(txAsprintf("%s - wc3270", hostname));
    } else {
	screen_title("wc3270");
    }
}

/**
 * Callback for changes in screen selection state.
 *
 * @param[in] now_selecting		true if selection in progress
 */
static void
screen_selecting_changed(bool now_selecting)
{
    selecting = now_selecting;
    set_console_title(window_title? window_title: "wc3270", selecting);
}

/**
 * Callback for exiting.
 *
 * @param[in] exiting		true if exiting
 */
static void
screen_exiting(bool exiting)
{
    /* Put back the original color map. */
    vctrace(TC_UI, "screen_exiting\n");
    if (original_blue_valid) {
	CONSOLE_SCREEN_BUFFER_INFOEX info;

	vctrace(TC_UI, "screen_exiting restoring color map\n");
	memset(&info, '\0', sizeof(CONSOLE_SCREEN_BUFFER_INFOEX));
	info.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	if (GetConsoleScreenBufferInfoEx(cohandle, &info) == 0) {
	    vctrace(TC_UI, "screen_exiting GetConsoleScreenBufferInfoEx: %s\n", win32_strerror(GetLastError()));
	    return;
	}
	info.srWindow.Bottom++; /* Well-known Windows bug. */
	info.ColorTable[cmap_fg[HOST_COLOR_BLUE]] = original_blue;
	if (SetConsoleScreenBufferInfoEx(cohandle, &info) == 0) {
	    vctrace(TC_UI, "screen_exiting SetConsoleScreenBufferInfoEx: %s\n", win32_strerror(GetLastError()));
	}
    }
}

/* Get the window handle for the console. */
static HWND
get_console_hwnd(void)
{
    return GetConsoleWindow();
}

/*
 * Read and discard a (printable) key-down event from the console.
 * Returns true if the key is 'q'.
 */
bool
screen_wait_for_key(char *c)
{
    INPUT_RECORD ir;
    DWORD nr;

    /* Get the next keyboard input event. */
    do {
	ReadConsoleInputA(chandle, &ir, 1, &nr);
    } while ((ir.EventType != KEY_EVENT) || !ir.Event.KeyEvent.bKeyDown);

    if (c != NULL) {
	*c = ir.Event.KeyEvent.uChar.AsciiChar;
    }

    return (ir.Event.KeyEvent.uChar.AsciiChar == 'q') ||
	   (ir.Event.KeyEvent.uChar.AsciiChar == 'Q');
}

/**
 * Check if an area of the screen is selected.
 *
 * @param[in] baddr	Buffer address.
 *
 * @return true if cell is selected
 */
bool
screen_selected(int baddr)
{
    return area_is_selected(baddr, 1);
}

void
screen_final(void)
{
}

/**
 * Get the current dimensions of the console.
 *
 * @param[out] rows	Returned rows
 * @param[out] cols	Returned cols
 */
void
get_console_size(int *rows, int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
	*rows = 25;
	*cols = 80;
	return;
    }
    *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    *cols = info.srWindow.Right - info.srWindow.Left + 1;
}

/**
 * Set the scrollbar thumb.
 *
 * @param[in] top	Where the top of the scrollbar should be (percentage)
 * @param[in] shown	How much of the scrollbar to show (percentage)
 * @param[in] saved	Number of lines saved
 * @param[in] screen	Size of a screen
 * @param[in] back	Number of lines scrolled back
 */
void
screen_set_thumb(float top _is_unused, float shown _is_unused,
	int saved _is_unused, int screen _is_unused, int back _is_unused)
{
}

/**
 * Change the model number, from a script.
 *
 * @param[in] mn        Model number
 * @param[in] ovc       Oversize columns
 * @param[in] ovr       Oversize rowa.
 */
void
screen_change_model(int mn, int ovc, int ovr)
{
    assert(false);
}

/**
 * Enable or disable the cursor.
 *
 * @param[in] on	Enable (true) or disable (false) the cursor display.
 */
void
enable_cursor(bool on)
{
    cursor_enabled = on;
    set_cursor_size(sbuf);
}

/**
 * Send yourself an ESC, to cancel any pending input.
 */
void
screen_send_esc(void)
{
    if (console_window != NULL) {
	PostMessage(console_window, WM_KEYDOWN, VK_ESCAPE, 0);
    }
}

/* Change the screen output color. */
void
screen_color(pc_t pc)
{
    int color = color_attr[pc];

    if (cohandle == NULL) {
	return;
    }

    if (!USING_VT) {
	if (color == HOST_COLOR_NEUTRAL_WHITE) {
	    (void) SetConsoleTextAttribute(cohandle, base_wAttributes);
	} else {
	    if (color == HOST_COLOR_BLUE && appres.c3270.reverse_video) {
		/* Blue is unreadable, switch to green. */
		color = HOST_COLOR_PALE_GREEN;
	    }
	    (void) SetConsoleTextAttribute(cohandle, cmap_fg[color] |
		    (base_wAttributes & (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)));
	}
    } else {
	if (color == HOST_COLOR_NEUTRAL_WHITE) {
	    (void) WriteConsoleA(cohandle, "\033[39m", 5, NULL, NULL);
	} else {
	    unsigned fg = rgbmap[color];
	    char *ct = Asprintf("\033[38;2;%d;%d;%dm",
		    (fg >> 16) & 0xff, (fg >> 8) & 0xff, fg & 0xff);

	    (void) WriteConsoleA(cohandle, ct, (int)strlen(ct), NULL, NULL);
	    Free(ct);
	}
    }
}

/**
 * Report the console info.
 *
 * @returns Console.
 */
static const char *
console_dump(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD cmax;
    int w_rows = 0, w_cols = 0;
    int b_rows, b_cols;

    if (sbuf) {
	if (GetConsoleScreenBufferInfo(sbuf, &info) == 0) {
	    return NULL;
	}
	w_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
	w_cols = info.srWindow.Right - info.srWindow.Left + 1;
    }

    if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
	return NULL;
    }
    b_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    b_cols = info.srWindow.Right - info.srWindow.Left + 1;

    cmax = GetLargestConsoleWindowSize(cohandle);

    return txAsprintf("Provider: Windows %s%s\n3270 screen buffer: %s\n\
Console window: rows %d columns %d\n\
Maximum: rows %d columns %d",
	    ON_WC? "Console": "Terminal",
	    (ON_WC && USING_VT)? " (VT mode)": "",
	    (w_rows || w_cols)? txAsprintf("rows %d columns %d", w_rows, w_cols): "uninitialized",
	    b_rows, b_cols, cmax.Y, cmax.X);
}

/* Connection state change handler. */
static void
screen_connect(bool ignored)
{
    if (!PCONNECTED) {
	/* Put back the original screen dimensions. */
	set_cols_rows(orig_model_num, orig_ov_auto? -1: orig_ov_cols, orig_ov_auto? -1: orig_ov_rows);

	/* Allow the screen to be resized when we connect again. */
	if (escaped && sbuf != NULL) {
	    screen_reset();
	}
    }
}

/**
 * Screen module registration.
 */
void
screen_register(void)
{
    static query_t queries[] = {
	{ KwConsole, console_dump, NULL, false, true }
    };
    static toggle_register_t toggles[] = {
	{ ALT_CURSOR,		toggle_altCursor,	0 },
	{ CURSOR_BLINK,		toggle_cursorBlink,	0 },
	{ MONOCASE,		toggle_monocase,	0 },
	{ SHOW_TIMING,		toggle_showTiming,	0 },
	{ UNDERSCORE,		toggle_underscore,	0 },
	{ MARGINED_PASTE,	NULL,			0 },
	{ OVERLAY_PASTE,	NULL,			0 },
	{ VISIBLE_CONTROL,	toggle_visibleControl,	0 },
	{ CROSSHAIR,		toggle_crosshair,	0 },
	{ TYPEAHEAD,		NULL,			0 }
    };
    static action_table_t screen_actions[] = {
	{ AnPaste,	Paste_action,	ACTION_KE },
	{ AnRedraw,	Redraw_action,	ACTION_KE },
	{ AnSnapScreen,	SnapScreen_action, ACTION_KE },
	{ AnTitle,	Title_action,	ACTION_KE }
    };

    /* Register the toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register the actions. */
    register_actions(screen_actions, array_count(screen_actions));

    /* Register for state changes. */
    register_schange(ST_SELECTING, screen_selecting_changed);
    register_schange(ST_EXITING, screen_exiting);
    register_schange(ST_CONNECT, screen_connect);

    /* Register our queries. */
    register_queries(queries, array_count(queries));
}
