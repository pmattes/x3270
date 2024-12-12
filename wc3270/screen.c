/*
 * Copyright (c) 2000-2024 Paul Mattes.
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
#include "names.h"
#include "nvt.h"
#include "popups.h"
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

#include <wincon.h>
#include "winvers.h"

#define STATUS_SCROLL_START_MS	1500
#define STATUS_SCROLL_MS	100
#define STATUS_PUSH_MS		5000

#define CM (60*10)	/* csec per minute */

#define XTRA_ROWS	(1 + 2 * (appres.interactive.menubar == true))

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

#define MAX_COLORS	16

#define CURSOR_BLINK_MS	500

/*
 * N.B.: F0 "neutral black" means black on a screen (white-on-black device) and
 *         white on a printer (black-on-white device).
 *       F7 "neutral white" means white on a screen (white-on-black device) and
 *         black on a printer (black-on-white device).
 */
static int cmap_fg[MAX_COLORS] = {
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
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,							/* FF white */
};
static int cmap_bg[MAX_COLORS] = {
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
    BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,							/* white */
};
static int field_colors[4] = {
    HOST_COLOR_GREEN,		/* default */
    HOST_COLOR_RED,		/* intensified */
    HOST_COLOR_BLUE,		/* protected */
    HOST_COLOR_NEUTRAL_WHITE	/* protected, intensified */
};

static int defattr = 0;
static int oia_attr = 0;
static int oia_bold_attr = 0;
static int oia_red_attr = 0;
static int oia_white_attr = 0;
static int xhattr = 0;
static ioid_t input_id;

bool screen_initted = true;
bool escaped = true;
bool isendwin = true;

enum ts ab_mode = TS_AUTO;

int windows_cp = 0;

#if defined(MAYBE_SOMETIME) /*[*/
/*
 * A bit of a cheat.  We know that Windows console attributes are really just
 * colors, with bits 0-3 for foreground and bits 4-7 for background.  That
 * leaves 8 bits we can play with for our own devious purposes, as long as we
 * don't accidentally pass one of those bits to Windows.
 *
 * The attributes we define are:
 *  WCATTR_UNDERLINE: The character is underlined.  Windows does not support
 *    underlining, but we do, by displaying underlined spaces as underscores.
 *    Some people may find this absolutely maddening.
 */
#endif /*]*/

static CHAR_INFO *onscreen;	/* what's on the screen now */
static CHAR_INFO *toscreen;	/* what's supposed to be on the screen */
static int onscreen_valid = FALSE; /* is onscreen valid? */

static int status_row = 0;	/* Row to display the status line on */
static int status_skip = 0;	/* Row to blank above the status line */
static int screen_yoffset = 0;	/* Vertical offset to top of screen.
				   If 0, there is no menu bar.
				   If nonzero (2, actually), menu bar is at the
				   top of the display. */
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
static void set_status_row(int screen_rows, int emulator_rows);
static void relabel(bool ignored);
static void init_user_colors(void);
static void init_user_attribute_colors(void);
static HWND get_console_hwnd(void);
static void codepage_changed(bool ignored);

static HANDLE chandle;	/* console input handle */
static HANDLE cohandle;	/* console screen buffer handle */

static HANDLE sbuf;	/* dynamically-allocated screen buffer */

HWND console_window;

static ctrlc_fn_t ctrlc_fn = NULL;

static int console_rows;
static int console_cols;
static COORD console_max;

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

CONSOLE_SCREEN_BUFFER_INFO base_info;

static action_t Paste_action;
static action_t Redraw_action;
static action_t Title_action;

static ioid_t redraw_id = NULL_IOID;

static void
win32_perror_fatal(const char *fmt, ...)
{
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    buf = Vasprintf(fmt, ap);
    va_end(ap);
    win32_perror("%s", buf);
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
	vtrace("Window closed\n");
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

    vtrace("^C received %s\n", escaped? "at prompt": "in session");
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
 * Return the number of rows implied by the given model number.
 */
static int
model_rows(int m)
{
    switch (m) {
    default:
    case 2:
	return MODEL_2_ROWS;
    case 3:
	return MODEL_3_ROWS;
    case 4:
	return MODEL_4_ROWS;
    case 5:
	return MODEL_5_ROWS;
    }
}

/*
 * Return the number of colums implied by the given model number.
 */
static int
model_cols(int m)
{
    switch (m) {
    default:
    case 2:
	return MODEL_2_COLS;
    case 3:
	return MODEL_3_COLS;
    case 4:
	return MODEL_4_COLS;
    case 5:
	return MODEL_5_COLS;
    }
}

/*
 * Resize the newly-created console.
 *
 * This function may make the console bigger (if the model number or oversize
 * requests it) or may make it smaller (if it is larger than what the model
 * requires). It may also call set_rows_cols() to update other globals derived
 * from the ov_cols and ov_rows.
 */
static int
resize_console(void)
{
    COORD want_bs;
    SMALL_RECT sr;
    bool ov_changed = false;

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
		win32_perror("SetConsoleWindowInfo(1) failed");
		return -1;
	    }
	}

	/* Set the console buffer size. */
	if (SetConsoleScreenBufferSize(sbuf, want_bs) == 0) {
	    win32_perror("SetConsoleScreenBufferSize failed");
	    return -1;
	}

	/* Set the console window. */
	sr.Top = 0;
	sr.Bottom = want_bs.Y - 1;
	sr.Left = 0;
	sr.Right = want_bs.X - 1;
	if (SetConsoleWindowInfo(sbuf, TRUE, &sr) == 0) {
	    win32_perror("SetConsoleWindowInfo(2) failed");
	    return -1;
	}

	/* Remember the new physical screen dimensions. */
	console_rows = want_bs.Y;
	console_cols = want_bs.X;

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
		popup_an_error("Oversize columns (%d) truncated to maximum "
			"window width (%d)", ov_cols, console_cols);
			ov_cols = console_cols;
			ov_changed = true;
	    }
	}

	if (ov_rows > model_rows(model_num)) {
	    if (ov_rows + XTRA_ROWS > console_rows) {
		popup_an_error("Oversize rows (%d) truncated to maximum "
			"window height (%d) - %d -> %d rows", ov_rows,
			console_rows, XTRA_ROWS, console_rows - XTRA_ROWS);
		ov_rows = console_rows - XTRA_ROWS;
		if (ov_rows <= model_rows(model_num)) {
		    ov_rows = 0;
		}
		ov_changed = true;
	    }
	}
    }

    if (ov_changed) {
	set_rows_cols(model_num, ov_cols, ov_rows);
    }

    return 0;
}

/*
 * Get a handle for the console.
 */
static HANDLE
initscr(void)
{
    size_t buffer_size;
    CONSOLE_CURSOR_INFO cursor_info;

    /* Get a handle to the console. */
    chandle = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (chandle == NULL) {
	win32_perror("CreateFile(CONIN$) failed");
	return NULL;
    }
    if (SetConsoleMode(chandle, ENABLE_PROCESSED_INPUT |
				ENABLE_MOUSE_INPUT) == 0) {
	win32_perror("SetConsoleMode failed");
	return NULL;
    }

    cohandle = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (cohandle == NULL) {
	win32_perror("CreateFile(CONOUT$) failed");
	return NULL;
    }

    console_window = get_console_hwnd();

    /* Get its dimensions. */
    if (GetConsoleScreenBufferInfo(cohandle, &base_info) == 0) {
	win32_perror("GetConsoleScreenBufferInfo failed");
	return NULL;
    }
    console_rows = base_info.srWindow.Bottom - base_info.srWindow.Top + 1;
    console_cols = base_info.srWindow.Right - base_info.srWindow.Left + 1;

    /* Get its cursor configuration. */
    if (GetConsoleCursorInfo(cohandle, &cursor_info) == 0) {
	win32_perror("GetConsoleCursorInfo failed");
	return NULL;
    }

    /* Get its maximum dimensions. */
    console_max = GetLargestConsoleWindowSize(cohandle);

    /* Create the screen buffer. */
    sbuf = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER,
	    NULL);
    if (sbuf == NULL) {
	win32_perror("CreateConsoleScreenBuffer failed");
	return NULL;
    }

    /* Set its dimensions. */
    if (!ov_auto) {
	if (resize_console() < 0) {
	    return NULL;
	}
    }

    /* Define a console handler. */
    if (!SetConsoleCtrlHandler(cc_handler, TRUE)) {
	win32_perror("SetConsoleCtrlHandler failed");
	return NULL;
    }
    cc_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (cc_event == NULL) {
	win32_perror("CreateEvent for ^C failed");
	return NULL;
    }
    cc_id = AddInput(cc_event, synchronous_cc);

    /* Allocate and initialize the onscreen and toscreen buffers. */
    buffer_size = sizeof(CHAR_INFO) * console_rows * console_cols;
    onscreen = (CHAR_INFO *)Malloc(buffer_size);
    memset(onscreen, '\0', buffer_size);
    onscreen_valid = FALSE;
    toscreen = (CHAR_INFO *)Malloc(buffer_size);
    memset(toscreen, '\0', buffer_size);

    /* More will no doubt follow. */
    return chandle;
}

/*
 * Virtual curses functions.
 */
static int cur_row = 0;
static int cur_col = 0;
static int cur_attr = 0;

static void
move(int row, int col)
{
    cur_row = row;
    cur_col = col;
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
    if (ch->Char.UnicodeChar != c || ch->Attributes != cur_attr) {
	ch->Char.UnicodeChar = c;
	ch->Attributes = cur_attr;
    }

    /* Increment and wrap. */
    if (++cur_col >= console_cols) {
	cur_col = 0;
	if (++cur_row >= console_rows) {
	    cur_row = 0;
	}
    }
}

static int
mvinch(int y, int x)
{
    move(y, x);
    return toscreen[(y * console_cols) + x].Char.UnicodeChar;
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

static void
mvprintw(int row, int col, char *fmt, ...)
{
    va_list ap;
    char *buf;
    size_t sl;
    WCHAR *wbuf;
    int nc;
    int i;

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
}

static int
ix(int row, int col)
{
    return (row * console_cols) + col;
}

static char *done_array = NULL;

static void
none_done(void)
{
    if (done_array == NULL) {
	done_array = Malloc(console_rows * console_cols);
    }
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
    int rc;

#if defined(DEBUG_SCREEN_DRAW) /*[*/
    /*
     * Trace what we've been asked to draw.
     * Drawn areas are 'h', done areas are 'd'.
     */
    {
	int trow, tcol;

	vtrace("hdraw row %d-%d col %d-%d attr 0x%x:\n",
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
    bufferSize.X = console_cols;
    bufferSize.Y = console_rows;
    bufferCoord.X = col;
    bufferCoord.Y = row;
    writeRegion.Left = col;
    writeRegion.Top = row;
    writeRegion.Right = lcol;
    writeRegion.Bottom = lrow;
    rc = WriteConsoleOutputW(sbuf, toscreen, bufferSize, bufferCoord,
	    &writeRegion);
    if (rc == 0) {
	win32_perror_fatal("WriteConsoleOutput failed");
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

	vtrace("draw_rect %s row %d-%d col %d-%d\n",
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

	vtrace("sync_onscreen:\n");
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

#if 0
    hdraw(0, console_rows - 1, 0, console_cols - 1);
    onscreen_valid = TRUE;
#endif

    /* Sometimes you have to draw everything. */
    if (!onscreen_valid) {
	draw_rect("invalid", 0, console_cols - 1, 0, console_rows - 1);
	onscreen_valid = TRUE;
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
    CONSOLE_CURSOR_INFO cci;
	
    memset(&cci, 0, sizeof(cci));
    cci.bVisible = (cursor_enabled && cblink.visible)? TRUE: FALSE;
    if (toggled(ALT_CURSOR)) {
	cci.dwSize = 25;
    } else {
	cci.dwSize = 100;
    }
    if (SetConsoleCursorInfo(handle, &cci) == 0) {
	win32_perror_fatal("\nSetConsoleCursorInfo failed");
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

    /* Move the cursor. */
    coord.X = cur_col;
    coord.Y = cur_row;
    if (onscreen[ix(cur_row, cur_col)].Attributes & COMMON_LVB_TRAILING_BYTE) {
	coord.X--;
    }
    if (GetConsoleScreenBufferInfo(sbuf, &info) == 0) {
	win32_perror_fatal("\nrefresh: GetConsoleScreenBufferInfo failed");
    }
    if ((info.dwCursorPosition.X != coord.X ||
	 info.dwCursorPosition.Y != coord.Y)) {
	if (SetConsoleCursorPosition(sbuf, coord) == 0) {
	    win32_perror_fatal("\nrefresh: SetConsoleCursorPosition(x=%d,y=%d) "
		    "failed", coord.X, coord.Y);
	}
    }

    /* Swap in this buffer. */
    if (!screen_swapped) {
	if (SetConsoleActiveScreenBuffer(sbuf) == 0) {
	    win32_perror_fatal("\nSetConsoleActiveScreenBuffer failed");
	}
	screen_swapped = true;
    }

    /* Set the cursor size. */
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
				ENABLE_MOUSE_INPUT) == 0) {
	win32_perror_fatal("\nSetConsoleMode(CONIN$) failed");
    }
    if (SetConsoleMode(cohandle, ENABLE_PROCESSED_OUTPUT |
				 ENABLE_WRAP_AT_EOL_OUTPUT) == 0) {
	win32_perror_fatal("\nSetConsoleMode(CONOUT$) failed");
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
				    ENABLE_MOUSE_INPUT) == 0) {
	    win32_perror_fatal("\nSetConsoleMode(CONIN$) failed");
	}
    } else {
	if (SetConsoleMode(chandle, ENABLE_LINE_INPUT |
				    ENABLE_PROCESSED_INPUT |
				    ENABLE_MOUSE_INPUT) == 0) {
	    win32_perror_fatal("\nSetConsoleMode(CONIN$) failed");
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

    set_console_cooked();

    /* Swap in the original buffer. */
    if (SetConsoleActiveScreenBuffer(cohandle) == 0) {
	win32_perror_fatal("\nSetConsoleActiveScreenBuffer failed");
    }

    screen_swapped = false;

    system("cls");
    printf("[wc3270]\n\n");
    fflush(stdout);
}

/* Initialize the screen. */
void
screen_init(void)
{
    int want_ov_rows;
    int want_ov_cols;
    bool oversize = false;

    if (appres.interactive.menubar) {
	menu_init();
    }

    /* Initialize the console. */
    if (initscr() == NULL) {
	fprintf(stderr, "Can't initialize terminal.\n");
	x3270_exit(1);
    }
    want_ov_rows = ov_rows;
    want_ov_cols = ov_cols;
    windows_cp = GetConsoleCP();

    /*
     * Respect the console size we are given.
     */
    while (console_rows < maxROWS || console_cols < maxCOLS) {
	/*
	 * First, cancel any oversize.  This will get us to the correct
	 * model number, if there is any.
	 */
	if ((ov_cols && ov_cols > console_cols) ||
	    (ov_rows && ov_rows > console_rows)) {

	    ov_cols = 0;
	    ov_rows = 0;
	    oversize = true;
	}

	/* If we're at the smallest screen now, give up. */
	if (model_num == 2) {
	    fprintf(stderr, "Emulator won't fit on a %dx%d display.\n",
		    console_rows, console_cols);
	    x3270_exit(1);
	}

	/* Try a smaller model. */
	set_rows_cols(model_num - 1, 0, 0);
    }

    /*
     * Now, if they wanted an oversize, but didn't get it, try applying it
     * again.
     */
    if (oversize) {
	if (want_ov_rows > console_rows - 2) {
	    want_ov_rows = console_rows - 2;
	}
	if (want_ov_rows < maxROWS) {
	    want_ov_rows = maxROWS;
	}
	if (want_ov_cols > console_cols) {
	    want_ov_cols = console_cols;
	}
	set_rows_cols(model_num, want_ov_cols, want_ov_rows);
    }

    /*
     * Finally, if they want automatic oversize, see if that's possible.
     */
    if (ov_auto && (maxROWS < console_rows - 3 || maxCOLS < console_cols)) {
	set_rows_cols(model_num, console_cols, console_rows - 3);
    }

    /* Figure out where the status line goes, if it fits. */
    /* Start out in altscreen mode. */
    set_status_row(console_rows, maxROWS);

    /* Initialize selections. */
    select_init(maxROWS, maxCOLS);

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

    /* If the want monochrome, assume they want green. */
    /* XXX: I believe that init_user_colors makes this a no-op. */
    if (!mode3279) {
	defattr |= FOREGROUND_GREEN;
	xhattr |= FOREGROUND_GREEN;
	if (ab_mode == TS_ON) {
	    defattr |= FOREGROUND_INTENSITY;
	}
    }

    /* Pull in the user's color mappings. */
    init_user_colors();
    init_user_attribute_colors();

    if (mode3279) {
	oia_attr = cmap_fg[HOST_COLOR_GREY] | cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
	oia_bold_attr = oia_attr; /* not used */
	oia_red_attr = FOREGROUND_RED | FOREGROUND_INTENSITY |
	    cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
	oia_white_attr = cmap_fg[HOST_COLOR_NEUTRAL_WHITE] |
	    FOREGROUND_INTENSITY | cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
    } else {
	oia_attr = defattr;
	oia_bold_attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY |
	    cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
	oia_red_attr = oia_bold_attr;
	oia_white_attr = oia_bold_attr;
    }

    /* Set up the controller. */
    ctlr_init(ALL_CHANGE);

    /* Set up the scrollbar. */
    scroll_buf_init();

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
}

/* Calculate where the status line goes now. */
static void
set_status_row(int screen_rows, int emulator_rows)
{
    if (screen_rows < emulator_rows + 1) {
	status_row = status_skip = 0;
    } else if (screen_rows == emulator_rows + 1) {
	status_skip = 0;
	status_row = emulator_rows;
    } else {
	status_skip = screen_rows - 2;
	status_row = screen_rows - 1;
    }

    /* Then check for menubar room.  Use 2 rows, 1 in a pinch. */
    if (appres.interactive.menubar) {
	if (screen_rows >= emulator_rows + (status_row != 0) + 2) {
	    screen_yoffset = 2;
	} else if (screen_rows >= emulator_rows + (status_row != 0) + 1) {
	    screen_yoffset = 1;
	} else {
	    screen_yoffset = 0;
	}
    }
}

/* Allocate a color pair. */
static int
get_color_pair(int fg, int bg)
{
    int mfg = fg & 0xf;
    int mbg = bg & 0xf;

    if (mfg >= MAX_COLORS) {
	mfg = 0;
    }
    if (mbg >= MAX_COLORS) {
	mbg = 0;
    }

    return cmap_fg[mfg] | cmap_bg[mbg];
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
    if (ptr == r || *ptr != '\0' || l >= MAX_COLORS) {
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
    init_user_attribute_color(&field_colors[3],
	    ResHostColorForProtectedIntensified);
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
	int fg;

	fg = color3270_from_fa(fa);
	return get_color_pair(fg, HOST_COLOR_NEUTRAL_BLACK);
    } else
	return FOREGROUND_GREEN |
	    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))?
	     FOREGROUND_INTENSITY: 0) |
	    cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
}

/* Swap foreground and background colors. */
static int
reverse_colors(int a)
{
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
	return;
    }

    xs_warning("Invalid %s value '%s'", ResConsoleColorForHostColor, r);
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

    for (i = 0; host_color[i].name != NULL; i++) {
	init_user_color(host_color[i].name, host_color[i].index);
    }

    if (mode3279) {
	defattr = cmap_fg[HOST_COLOR_NEUTRAL_WHITE] |
		  cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
	crosshair_color_init();
	xhattr = cmap_fg[crosshair_color] |
		  cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
    } else {
	defattr = cmap_fg[HOST_COLOR_PALE_GREEN] |
		  cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
	xhattr = cmap_fg[HOST_COLOR_PALE_GREEN] |
		  cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
    }
}

/* Invert colors (selections). */
static int
invert_colors(int a)
{
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

    if (gr & GR_REVERSE) {
	a = reverse_colors(a);
    }

    if (toggled(UNDERSCORE) && (gr & GR_UNDERLINE)) {
	*underlined = true;
    } else {
	*underlined = false;
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
    vtrace("blink timeout\n");

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
    vtrace("cursor blink timeout\n");
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

/*
 * Return a visible control character for a field attribute.
 */
static unsigned char
visible_fa(unsigned char fa)
{
    static unsigned char varr[32] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

    unsigned ix;

    /*
     * This code knows that:
     *  FA_PROTECT is   0b100000, and we map it to 0b010000
     *  FA_NUMERIC is   0b010000, and we map it to 0b001000
     *  FA_INTENSITY is 0b001100, and we map it to 0b000110
     *  FA_MODIFY is    0b000001, and we copy to   0b000001
     */
    ix = ((fa & (FA_PROTECT | FA_NUMERIC | FA_INTENSITY)) >> 1) |
	(fa & FA_MODIFY);
    return varr[ix];
}

static ucs4_t
crosshair_blank(int baddr)
{
    if (in_focus && toggled(CROSSHAIR)) {
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

    /* This may be called when it isn't time. */
    if (escaped) {
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
		win32_perror_fatal("\nscreen_disp: "
			"SetConsoleCursorPosition(x=%d,y=%d) failed",
			coord.X, coord.Y);
	    }
	}

	return;
    }

    /* If the menubar is separate, draw it first. */
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
		high0 = high1 = cmap_fg[HOST_COLOR_NEUTRAL_BLACK] |
				cmap_bg[HOST_COLOR_NEUTRAL_WHITE];
		norm0 = norm1 = cmap_fg[HOST_COLOR_NEUTRAL_WHITE] |
				cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
	    } else {
		norm0 = cmap_bg[HOST_COLOR_GREY] |
			cmap_fg[HOST_COLOR_NEUTRAL_BLACK];
		high0 = cmap_bg[HOST_COLOR_NEUTRAL_WHITE] | 
			cmap_fg[HOST_COLOR_NEUTRAL_BLACK];
		norm1 = cmap_fg[HOST_COLOR_NEUTRAL_WHITE] |
			cmap_bg[HOST_COLOR_NEUTRAL_BLACK];
		high1 = cmap_fg[HOST_COLOR_NEUTRAL_BLACK] | 
			cmap_bg[HOST_COLOR_NEUTRAL_WHITE];
	    }

	} else {
	    /*
	     * Menu is not up.
	     * Row 0 is a gray-background stripe.
	     * Row 1 has a black background.
	     */
	    norm0 = high0 = cmap_bg[HOST_COLOR_GREY] |
			    cmap_fg[HOST_COLOR_NEUTRAL_BLACK];
	    norm1 = high1 = cmap_bg[HOST_COLOR_NEUTRAL_BLACK] |
			    cmap_fg[HOST_COLOR_GREY];
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
	    for (col = 0; col < cCOLS; col++) {
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
		    attrset(cmap_fg[HOST_COLOR_NEUTRAL_BLACK] |
			    cmap_bg[HOST_COLOR_NEUTRAL_WHITE]);
		} else {
		    attrset(cmap_fg[HOST_COLOR_NEUTRAL_WHITE] |
			    cmap_bg[HOST_COLOR_NEUTRAL_BLACK]);
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
			attrset(apply_select(cmap_fg[HOST_COLOR_NEUTRAL_BLACK] |
				             cmap_bg[HOST_COLOR_YELLOW],
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
			if (u == ' ' && in_focus && toggled(CROSSHAIR)) {
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
			if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_null &&
				ea_buf[xaddr].ec == EBC_null) {
			    attrset(apply_select(cmap_fg[HOST_COLOR_NEUTRAL_BLACK] |
						 cmap_bg[HOST_COLOR_YELLOW],
						 baddr));
			    addch('.');
			    addch('.');
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
		    } else if (!IS_RIGHT(d)) {
			if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_null) {
			    u = '.';
			} else if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_so) {
			    u = '<';
			} else if (toggled(VISIBLE_CONTROL) &&
				ea_buf[baddr].ec == EBC_si) {
			    u = '>';
			} else {
			    u = ebcdic_to_unicode(ea_buf[baddr].ec,
				    ea_buf[baddr].cs,
				    appres.c3270.ascii_box_draw?
					EUO_ASCII_BOX: 0);
			    if (u == 0) {
				u = crosshair_blank(baddr);
				if (u != ' ') {
				    attr_this = apply_select(xhattr, baddr);
				}
			    } else if (u == ' ' && in_focus && toggled(CROSSHAIR)) {
				u = crosshair_blank(baddr);
				if (u != ' ') {
				    attr_this = apply_select(xhattr, baddr);
				}
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
	    vtrace(" lightpen select\n");
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
	vtrace(" cursor move\n");
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

/* Redraw the screen in response to a screen resize event. */
static void
resize_redraw(ioid_t ignored)
{
    static const char *no_argv[1] = { NULL };

    redraw_id = NULL_IOID;
    if (!escaped) {
	system("cls");
	screen_system_fixup();
	Redraw_action(IA_NONE, 0, no_argv);
    }
}

/* Keyboard input. */
static void
kybd_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    int rc;
    INPUT_RECORD ir;
    DWORD nr;
    const char *s;
    SHORT x, y;

    /* Get the next input event. */
    rc = ReadConsoleInputW(chandle, &ir, 1, &nr);
    if (rc == 0) {
	win32_perror_fatal("ReadConsoleInput failed");
    }
    if (nr == 0) {
	return;
    }

    switch (ir.EventType) {
    case FOCUS_EVENT:
	vtrace("Focus %s\n", ir.Event.FocusEvent.bSetFocus? "set": "unset");
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
	    return;
	}
	s = lookup_cname(ir.Event.KeyEvent.wVirtualKeyCode << 16);
	if (s == NULL) {
	    s = "?";
	}
	vtrace("Key%s vkey 0x%x (%s) scan 0x%x char U+%04x state 0x%x (%s)\n",
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
	vtrace("Menu\n");
	break;
    case MOUSE_EVENT:
	vtrace("Mouse (%d,%d) ButtonState %s "
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
	vtrace("WindowBufferSize X %d Y %d\n", x, y);
	if (redraw_id != NULL_IOID) {
	    RemoveInput(redraw_id);
	}
	redraw_id = AddTimeOut(500, resize_redraw);
	break;
    default:
	vtrace("Unknown input event %d\n", ir.EventType);
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
    vtrace(" %s ->", txdFree(vb_consume(&r)));
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
	run_action(AnEscape, IA_DEFAULT, NULL, NULL);
	return;
    case VK_UP:
	run_action(AnUp, IA_DEFAULT, NULL, NULL);
	return;
    case VK_DOWN:
	run_action(AnDown, IA_DEFAULT, NULL, NULL);
	return;
    case VK_LEFT:
	run_action(AnLeft, IA_DEFAULT, NULL, NULL);
	return;
    case VK_RIGHT:
	run_action(AnRight, IA_DEFAULT, NULL, NULL);
	return;
    case VK_HOME:
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
	    run_action(AnTab, IA_DEFAULT, NULL, NULL);
	    return;
	case VK_DELETE:
	    run_action(AnDelete, IA_DEFAULT, NULL, NULL);
	    return;
	case VK_BACK:
	    run_action(AnBackSpace, IA_DEFAULT, NULL, NULL);
	    return;
	case VK_RETURN:
	    run_action(AnEnter, IA_DEFAULT, NULL, NULL);
	    return;
	default:
	    break;
	}
    }

    /* Catch PF keys. */
    if (k >= VK_F1 && k <= VK_F24) {
	run_action(AnPF, IA_DEFAULT, txAsprintf("%d", k - VK_F1 + 1), NULL);
	return;
    }

    /* Then any other character. */
    if (ir->Event.KeyEvent.uChar.UnicodeChar) {
	run_action(AnKey, IA_DEFAULT,
		txAsprintf("U+%04x", ir->Event.KeyEvent.uChar.UnicodeChar),
		NULL);
    } else {
	vtrace(" dropped (no default)\n");
    }
}

bool
screen_suspend(void)
{
    static bool need_to_scroll = false;

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
				    ENABLE_MOUSE_INPUT) == 0) {
	    win32_perror("SetConsoleMode failed");
	}
    }
}

void
screen_resume(void)
{
    if (!escaped) {
	return;
    }
    escaped = false;

    screen_disp(false);
    onscreen_valid = FALSE;
    refresh();
    input_id = AddInput(chandle, kybd_input);

    if (SetConsoleMode(chandle, ENABLE_PROCESSED_INPUT |
				ENABLE_MOUSE_INPUT) == 0) {
	win32_perror("SetConsoleMode failed");
    }
}

void
cursor_move(int baddr)
{
    cursor_addr = baddr;
    if (in_focus && toggled(CROSSHAIR)) {
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
    vtrace("set_cblink(%s)\n", mode? "true": "false");
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
	} else if (kybdlock & KL_ENTER_INHIBIT) {
	    other_msg = "X Inhibit";
	} else if (kybdlock & KL_BID) {
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

    /* Extend or erase the crosshair. */
    attrset(xhattr);
    if (in_focus && toggled(CROSSHAIR)) {
	if (!menu_is_up &&
		(mvinch(0, fl_cursor_col) & A_CHARTEXT) == ' ') {
	    attrset(cmap_fg[crosshair_color] | cmap_bg[HOST_COLOR_GREY]);
	    addch(LINEDRAW_VERT);
	    attrset(xhattr);
	}
	if (screen_yoffset > 1 &&
		(mvinch(1, fl_cursor_col) & A_CHARTEXT) == ' ') {
	    addch(LINEDRAW_VERT);
	}
    }
    for (i = ROWS + screen_yoffset; i < status_row; i++) {
	for (j = 0; j < maxCOLS; j++) {
	    move(i, j);
	    if (in_focus && toggled(CROSSHAIR) && (j == fl_cursor_col)) {
		addch(LINEDRAW_VERT);
	    } else {
		addch(' ');
	    }
	}
    }
    for (i = 0; i < ROWS; i++) {
	for (j = cCOLS; j < maxCOLS; j++) {
	    move(i + screen_yoffset, j);
	    if (in_focus && toggled(CROSSHAIR) && i == (cursor_addr / cCOLS)) {
		addch(LINEDRAW_HORIZ);
	    } else {
		addch(' ');
	    }
	}
    }

    /* Make sure the status line region is filled in properly. */
    attrset(defattr);
    move(maxROWS + screen_yoffset, 0);
    for (i = maxROWS + screen_yoffset; i < status_row; i++) {
	for (j = 0; j <= rmargin; j++) {
	    addch(' ');
	}
    }
    move(status_row, 0);
    attrset(defattr);
    for (i = 0; i <= rmargin; i++) {
	addch(' ');
    }

    /* Offsets 0, 1, 2 */
    if (mode3279) {
	attrset(cmap_fg[HOST_COLOR_NEUTRAL_BLACK] | cmap_bg[HOST_COLOR_GREY]);
    } else {
	attrset(reverse_colors(defattr));
    }
    mvprintw(status_row, 0, "4");
    if (oia_undera) {
	addch(IN_E? 'B': 'A');
    } else {
	addch(' ');
    }
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
    mvprintw(status_row, rmargin-35,
	    "%c%c %c%c%c%c",
	    oia_compose? 'C': ' ',
	    oia_compose? oia_compose_char: ' ', /* XXX */
	    status_ta? 'T': ' ',
	    status_rm? 'R': ' ',
	    status_im? 'I': ' ',
	    oia_printer? 'P': ' ');
    if (status_secure != SS_INSECURE) {
	attrset(mode3279?
		    (cmap_fg[(status_secure == SS_SECURE)?
				HOST_COLOR_GREEN: HOST_COLOR_YELLOW] |
		    cmap_bg[HOST_COLOR_NEUTRAL_BLACK]):
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
	    toggled(CROSSHAIR) &&
	    cursor_col > 2 &&
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
	onscreen_valid = FALSE;
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

    action_debug(AnPaste, ia, argc, argv);
    if (check_argc(AnPaste, argc, 0, 0) < 0) {
	return false;
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
	    emulate_uinput(u, sl, true);
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

/* Get the window handle for the console. */
static HWND
get_console_hwnd(void)
{
#   define MY_BUFSIZE 1024	/* buffer size for console window titles */
    HWND hwnd_found;        		/* returned to the caller */
    char new_window_title[MY_BUFSIZE];	/* fabricated WindowTitle */
    char old_window_title[MY_BUFSIZE];	/* original WindowTitle */

    /* Fetch current window title. */
    GetConsoleTitle(old_window_title, MY_BUFSIZE);

    /* Format a "unique" NewWindowTitle. */
    wsprintf(new_window_title, "%d/%d", GetTickCount(), GetCurrentProcessId());

    /* Change current window title. */
    SetConsoleTitle(new_window_title);

    /* Ensure window title has been updated. */
    Sleep(40);

    /* Look for NewWindowTitle. */
    hwnd_found = FindWindow(NULL, new_window_title);

    /* Restore original window title. */
    SetConsoleTitle(old_window_title);
    return hwnd_found;
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

/* Screen output color map. */
static WORD color_attr[] = {
    0,						/* PC_DEFAULT */
    FOREGROUND_INTENSITY | FOREGROUND_BLUE,	/* PC_PROMPT */
    FOREGROUND_INTENSITY | FOREGROUND_RED,	/* PC_ERROR */
    0,						/* PC_NORMAL */
};

/* Change the screen output color. */
void
screen_color(pc_t pc)
{
    if (!SetConsoleTextAttribute(cohandle,
		color_attr[pc]? color_attr[pc]: base_info.wAttributes)) {
	win32_perror("Can't set console text attribute");
	exit(1);
    }
}

/**
 * Screen module registration.
 */
void
screen_register(void)
{
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
	{ "SnapScreen", SnapScreen_action, ACTION_KE },
	{ AnTitle,	Title_action,	ACTION_KE }
    };

    /* Register the toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register the actions. */
    register_actions(screen_actions, array_count(screen_actions));

    /* Register for selection state changes. */
    register_schange(ST_SELECTING, screen_selecting_changed);
}
