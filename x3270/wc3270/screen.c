/*
 * Copyright 2000, 2001, 2002, 2004, 2005, 2006, 2007 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * c3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	screen.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Screen drawing
 */

#include "globals.h"
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "screenc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "w3miscc.h"
#include "widec.h"
#include "xioc.h"

#include <windows.h>
#include <wincon.h>
#include "winversc.h"

extern int screen_changed;
extern char *profile_name;

#define MAX_COLORS	16
static int cmap_fg[MAX_COLORS] = {
	0,						/* neutral black */
	FOREGROUND_INTENSITY | FOREGROUND_BLUE,		/* blue */
	FOREGROUND_INTENSITY | FOREGROUND_RED,		/* red */
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
							/* pink */
	FOREGROUND_INTENSITY | FOREGROUND_GREEN,	/* green */
	FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
							/* turquoise */
	FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED,
							/* yellow */
	FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
	0,						/* black */
	FOREGROUND_BLUE,				/* deep blue */
	FOREGROUND_INTENSITY | FOREGROUND_RED,		/* orange */
	FOREGROUND_RED | FOREGROUND_BLUE,		/* purple */
	FOREGROUND_GREEN,				/* pale green */
	FOREGROUND_GREEN | FOREGROUND_BLUE,		/* pale turquoise */
	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, /* gray */
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,							/* white */

							/* neutral white */
};
static int cmap_bg[MAX_COLORS] = {
	0,						/* neutral black */
	BACKGROUND_INTENSITY | BACKGROUND_BLUE,		/* blue */
	BACKGROUND_INTENSITY | BACKGROUND_RED,		/* red */
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
	BACKGROUND_INTENSITY | BACKGROUND_RED,		/* orange */
	BACKGROUND_RED | BACKGROUND_BLUE,		/* purple */
	BACKGROUND_GREEN,				/* pale green */
	BACKGROUND_GREEN | BACKGROUND_BLUE,		/* pale turquoise */
	BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE, /* gray */
	BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,							/* white */
};
static int field_colors[4] = {
	COLOR_GREEN,		/* default */
	COLOR_RED,		/* intensified */
	COLOR_BLUE,		/* protected */
	COLOR_NEUTRAL_WHITE	/* protected, intensified */
};
static struct {
	char *name;
	int index;
} host_color[] = {
	{ "NeutralBlack",	COLOR_NEUTRAL_BLACK },
	{ "Blue",		COLOR_BLUE },
	{ "Red",		COLOR_RED },
	{ "Pink",		COLOR_PINK },
	{ "Green",		COLOR_GREEN },
	{ "Turquoise",		COLOR_TURQUOISE },
	{ "Yellow",		COLOR_YELLOW },
	{ "NeutralWhite",	COLOR_NEUTRAL_WHITE },
	{ "Black",		COLOR_BLACK },
	{ "DeepBlue",		COLOR_DEEP_BLUE },
	{ "Orange",		COLOR_ORANGE },
	{ "Purple",		COLOR_PURPLE },
	{ "PaleGreen",		COLOR_PALE_GREEN },
	{ "PaleTurquoise",	COLOR_PALE_TURQUOISE },
	{ "Grey",		COLOR_GREY },
	{ "Gray",		COLOR_GREY }, /* alias */
	{ "White",		COLOR_WHITE },
	{ CN,			0 }
};

static int defattr = 0;
static unsigned long input_id;

Boolean escaped = True;

enum ts { TS_AUTO, TS_ON, TS_OFF };
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

static void kybd_input(void);
static void kybd_input2(INPUT_RECORD *ir);
static void draw_oia(void);
static void status_connect(Boolean ignored);
static void status_3270_mode(Boolean ignored);
static void status_printer(Boolean on);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char fa);
static void screen_init2(void);
static void set_status_row(int screen_rows, int emulator_rows);
static Boolean ts_value(const char *s, enum ts *tsp);
static int linedraw_to_acs(unsigned char c);
static int apl_to_acs(unsigned char c);
static void relabel(Boolean ignored);
static void check_aplmap(int codepage);
static void init_user_colors(void);
static void init_user_attribute_colors(void);

static HANDLE chandle;	/* console input handle */
static HANDLE cohandle;	/* console screen buffer handle */

static HANDLE *sbuf;	/* dynamically-allocated screen buffer */

static int console_rows;
static int console_cols;

static int screen_swapped = FALSE;

/*
 * Console event handler.
 */
static BOOL
cc_handler(DWORD type)
{
	if (type == CTRL_C_EVENT) {
		char *action;

		/* Process it as a Ctrl-C. */
		trace_event("Control-C received via Console Event Handler\n");
		action = lookup_key(0x03, LEFT_CTRL_PRESSED);
		if (action != CN) {
			if (strcmp(action, "[ignore]"))
				push_keymap_action(action);
		} else {
			String params[2];
			Cardinal one;

			params[0] = "0x03";
			params[1] = CN;
			one = 1;
			Key_action(NULL, NULL, params, &one);
		}

		return TRUE;
	} else {
		/* Let Windows have its way with it. */
		return FALSE;
	}
}

/*
 * Get a handle for the console.
 */
static HANDLE
initscr(void)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	size_t buffer_size;
	CONSOLE_CURSOR_INFO cursor_info;

	/* Get a handle to the console. */
	chandle = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL);
	if (chandle == NULL) {
		fprintf(stderr, "CreateFile(CONIN$) failed: %s\n",
			win32_strerror(GetLastError()));
		return NULL;
	}
	if (SetConsoleMode(chandle, ENABLE_PROCESSED_INPUT) == 0) {
		fprintf(stderr, "SetConsoleMode failed: %s\n",
			win32_strerror(GetLastError()));
		return NULL;
	}

	cohandle = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL);
	if (cohandle == NULL) {
		fprintf(stderr, "CreateFile(CONOUT$) failed: %s\n",
			win32_strerror(GetLastError()));
		return NULL;
	}

	/* Get its dimensions. */
	if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
		fprintf(stderr, "GetConsoleScreenBufferInfo failed: %s\n",
			win32_strerror(GetLastError()));
		return NULL;
	}
	console_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
	console_cols = info.srWindow.Right - info.srWindow.Left + 1;

	/* Get its cursor configuration. */
	if (GetConsoleCursorInfo(cohandle, &cursor_info) == 0) {
		fprintf(stderr, "GetConsoleCursorInfo failed: %s\n",
			win32_strerror(GetLastError()));
		return NULL;
	}

	/* Create the screen buffer. */
	sbuf = CreateConsoleScreenBuffer(
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CONSOLE_TEXTMODE_BUFFER,
		NULL);
	if (sbuf == NULL) {
		fprintf(stderr,
			"CreateConsoleScreenBuffer failed: %s\n",
			win32_strerror(GetLastError()));
		return NULL;
	}

	/* Set its cursor state. */
	if (SetConsoleCursorInfo(sbuf, &cursor_info) == 0) {
		fprintf(stderr, "SetConsoleScreenBufferInfo failed: %s\n",
			win32_strerror(GetLastError()));
		return NULL;
	}

	/* Define a console handler. */
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)cc_handler, TRUE)) {
		fprintf(stderr, "SetConsoleCtrlHandler failed: %s\n",
				win32_strerror(GetLastError()));
		return NULL;
	}

	/* Allocate and initialize the onscreen and toscreen buffers. */
	buffer_size = sizeof(CHAR_INFO) * console_rows * console_cols;
	onscreen = (CHAR_INFO *)Malloc(buffer_size);
	(void) memset(onscreen, '\0', buffer_size);
	onscreen_valid = FALSE;
	toscreen = (CHAR_INFO *)Malloc(buffer_size);
	(void) memset(toscreen, '\0', buffer_size);

	/* More will no doubt follow. */
	return chandle;
}

/* Try to set the console output character set. */
void
set_display_charset(char *dcs)
{
	char *copy;
	char *s;
	char *cs;
	int want_cp = 0;

	windows_cp = GetConsoleCP();

	copy = strdup(dcs);
	s = copy;
	while ((cs = strtok(s, ",")) != NULL) {
		s = NULL;

		if (!strncmp(cs, "windows-", 8)) {
			want_cp = atoi(cs + 8);
			break;
		} else if (!strncmp(cs, "iso8859-", 8)) {
			want_cp = 28590 + atoi(cs + 8);
			break;
		} else if (!strcmp(cs, "koi8-r")) {
			want_cp = 20866;
			break;
		}
	}
	free(copy);

	if (want_cp != 0 && windows_cp != want_cp) {
	    	if (SetConsoleOutputCP(want_cp)) {
			(void)SetConsoleCP(want_cp);
		    	windows_cp = want_cp;
		} else {
			fprintf(stderr,
				"Unable to set output character set to '%s' "
				"(Windows code page %d).\n",
				dcs, want_cp);
		}
	}

	check_aplmap(windows_cp);
}

/*
 * Vitrual curses functions.
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
addch(int c)
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
		if (++cur_row >= console_rows)
			cur_row = 0;
	}
}

static void
printw(char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	char *s;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	for (s = buf; *s; s++) {
		addch(*s);
	}
	va_end(ap);
}

static void
mvprintw(int row, int col, char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	char *s;

	va_start(ap, fmt);
	cur_row = row;
	cur_col = col;
	vsprintf(buf, fmt, ap);
	for (s = buf; *s; s++) {
		addch(*s);
	}
	va_end(ap);
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
	    	memset(&done_array[ix(row, start_col)],
			1, end_col - start_col + 1);
	}
}

static int
tos_a(int row, int col)
{
    	/*return toscreen[ix(row, col)].Attributes;*/
	if (toscreen[ix(row, col)].Char.UnicodeChar & ~0xff)
		return toscreen[ix(row, col)].Attributes | 0x80000000;
	else
	    	return toscreen[ix(row, col)].Attributes;
}

#if defined(DEBUG_SCREEN_DRAW) /*[*/
static int
changed(int row, int col)
{
	return !onscreen_valid ||
		memcmp(&onscreen[ix(row, col)], &toscreen[ix(row, col)],
		       sizeof(CHAR_INFO));
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

		trace_event("hdraw row %d-%d col %d-%d attr 0x%x:\n",
			row, lrow, col, lcol, tos_a(row, col));
		for (trow = 0; trow < console_rows; trow++) {
			for (tcol = 0; tcol < console_cols; tcol++) {
				if (trow >= row && trow <= lrow &&
				    tcol >= col && tcol <= lcol)
					trace_event("h");
				else if (is_done(trow, tcol))
					trace_event("d");
				else
					trace_event(".");
			}
			trace_event("\n");
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
	if (toscreen[ix(row, col)].Char.UnicodeChar & ~0xff)
	    	rc = WriteConsoleOutputW(sbuf, toscreen, bufferSize,
			bufferCoord, &writeRegion);
	else
	    	rc = WriteConsoleOutputA(sbuf, toscreen, bufferSize,
			bufferCoord, &writeRegion);
	if (rc == 0) {

		fprintf(stderr, "WriteConsoleOutput failed: %s\n",
			win32_strerror(GetLastError()));
		x3270_exit(1);
	}

	/* Sync 'onscreen'. */
	for (xrow = row; xrow <= lrow; xrow++) {
	    	memcpy(&onscreen[ix(xrow, col)],
		       &toscreen[ix(xrow, col)],
		       sizeof(CHAR_INFO) * (lcol - col + 1));
	}

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
draw_rect(int pc_start, int pc_end, int pr_start, int pr_end)
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

		trace_event("draw_rect row %d-%d col %d-%d\n",
			pr_start, pr_end, pc_start, pc_end);
		for (trow = 0; trow < console_rows; trow++) {
			for (tcol = 0; tcol < console_cols; tcol++) {
				if (trow >= pr_start && trow <= pr_end &&
				    tcol >= pc_start && tcol <= pc_end) {
					if (changed(trow, tcol))
						trace_event("r");
					else
						trace_event("x");
				} else
					trace_event(".");
			}
			trace_event("\n");
		}
	}
#endif /*]*/

	for (ul_row = pr_start; ul_row <= pr_end; ul_row++) {
	    	for (ul_col = pc_start; ul_col <= pc_end; ul_col++) {
		    	int col_found = 0;

		    	if (is_done(ul_row, ul_col))
			    	continue;

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
			for (xrow = ul_row;
				!col_found && xrow <= pr_end;
				xrow++) {

				if (is_done(xrow, ul_col) ||
				    tos_a(xrow, ul_col) != a) {
					lr_row = xrow - 1;
					break;
				}
				for (xcol = ul_col; xcol <= lr_col; xcol++) {
				    	if (is_done(xrow, xcol) ||
					    tos_a(xrow, xcol) != a) {
						lr_col = xcol - 1;
						lr_row = xrow;
						col_found = 1;
						break;
					}
				}
			}
			hdraw(ul_row, lr_row, ul_col, lr_col);
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

		trace_event("sync_onscreen:\n");
		for (trow = 0; trow < console_rows; trow++) {
			for (tcol = 0; tcol < console_cols; tcol++) {
				if (changed(trow, tcol))
					trace_event("m");
				else
					trace_event(".");
			}
			trace_event("\n");
		}
	}
#endif /*]*/

	/* Sometimes you have to draw everything. */
	if (!onscreen_valid) {
	    	draw_rect(0, console_cols - 1, 0, console_rows - 1);
		onscreen_valid = TRUE;
		return;
	}

	for (row = 0; row < console_rows; row++) {

	    	/* Check the whole row for a match first. */
	    	if (!memcmp(&onscreen[ix(row, 0)],
			    &toscreen[ix(row, 0)],
			    sizeof(CHAR_INFO) * console_cols)) {
		    if (pending) {
			    draw_rect(pc_start, pc_end, pr_start, row - 1);
			    pending = FALSE;
		    }
		    continue;
		}

		for (col = 0; col < console_cols; col++) {
		    	if (memcmp(&onscreen[ix(row, col)],
				   &toscreen[ix(row, col)],
				   sizeof(CHAR_INFO))) {
			    	/*
				 * This column differs.
				 * Start or expand the band, and start pending.
				 */
			    	if (!pending || col < pc_start)
				    	pc_start = col;
				if (!pending || col > pc_end)
				    	pc_end = col;
				if (!pending) {
				    	pr_start = row;
					pending = TRUE;
				}
			}
		}
	}

	if (pending)
	    	draw_rect(pc_start, pc_end, pr_start, console_rows - 1);
}

/* Repaint the screen. */
static void
refresh(void)
{
	COORD coord;

	/*
	 * Draw the differences between 'onscreen' and 'toscreen' into
	 * sbuf.
	 */
	sync_onscreen();

	/* Move the cursor. */
	coord.X = cur_col;
	coord.Y = cur_row;
	if (SetConsoleCursorPosition(sbuf, coord) == 0) {
		fprintf(stderr,
			"\nrefresh: SetConsoleCursorPosition(x=%d,y=%d) "
			"failed: %s\n",
			coord.X, coord.Y, win32_strerror(GetLastError()));
		x3270_exit(1);
	}

	/* Swap in this buffer. */
	if (screen_swapped == FALSE) {
		if (SetConsoleActiveScreenBuffer(sbuf) == 0) {
			fprintf(stderr,
				"\nSetConsoleActiveScreenBuffer failed: %s\n",
				win32_strerror(GetLastError()));
			x3270_exit(1);
		}
		screen_swapped = TRUE;
	}
}

/* Go back to the original screen. */
static void
endwin(void)
{
	if (SetConsoleMode(chandle, ENABLE_ECHO_INPUT |
				    ENABLE_LINE_INPUT |
				    ENABLE_PROCESSED_INPUT) == 0) {
		fprintf(stderr, "\nSetConsoleMode(CONIN$) failed: %s\n",
			win32_strerror(GetLastError()));
		x3270_exit(1);
	}
	if (SetConsoleMode(cohandle, ENABLE_PROCESSED_OUTPUT |
				     ENABLE_WRAP_AT_EOL_OUTPUT) == 0) {
		fprintf(stderr, "\nSetConsoleMode(CONOUT$) failed: %s\n",
			win32_strerror(GetLastError()));
		x3270_exit(1);
	}

	/* Swap in the original buffer. */
	if (SetConsoleActiveScreenBuffer(cohandle) == 0) {
		fprintf(stderr, "\nSetConsoleActiveScreenBuffer failed: %s\n",
			win32_strerror(GetLastError()));
		x3270_exit(1);
	}

	screen_swapped = FALSE;
}

/* Initialize the screen. */
void
screen_init(void)
{
	int want_ov_rows = ov_rows;
	int want_ov_cols = ov_cols;
	Boolean oversize = False;

	/* Disallow altscreen/defscreen. */
	if ((appres.altscreen != CN) || (appres.defscreen != CN)) {
		(void) fprintf(stderr, "altscreen/defscreen not supported\n");
		x3270_exit(1);
	}
	/* Initialize the console. */
	if (initscr() == NULL) {
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		x3270_exit(1);
	}

	/*
	 * Respect the console size we are given.
	 */
	while (console_rows < maxROWS || console_cols < maxCOLS) {
		char buf[2];

		/*
		 * First, cancel any oversize.  This will get us to the correct
		 * model number, if there is any.
		 */
		if ((ov_cols && ov_cols > console_cols) ||
		    (ov_rows && ov_rows > console_rows)) {
			ov_cols = 0;
			ov_rows = 0;
			oversize = True;
			continue;
		}

		/* If we're at the smallest screen now, give up. */
		if (model_num == 2) {
			(void) fprintf(stderr, "Emulator won't fit on a %dx%d "
			    "display.\n", console_rows, console_cols);
			x3270_exit(1);
		}

		/* Try a smaller model. */
		(void) sprintf(buf, "%d", model_num - 1);
		appres.model = NewString(buf);
		set_rows_cols(model_num - 1, 0, 0);
	}

	/*
	 * Now, if they wanted an oversize, but didn't get it, try applying it
	 * again.
	 */
	if (oversize) {
		if (want_ov_rows > console_rows - 2)
			want_ov_rows = console_rows - 2;
		if (want_ov_rows < maxROWS)
			want_ov_rows = maxROWS;
		if (want_ov_cols > console_cols)
			want_ov_cols = console_cols;
		set_rows_cols(model_num, want_ov_cols, want_ov_rows);
	}

	/* Figure out where the status line goes, if it fits. */
	/* Start out in altscreen mode. */
	set_status_row(console_rows, maxROWS);

	/* Set up callbacks for state changes. */
	register_schange(ST_CONNECT, status_connect);
	register_schange(ST_3270_MODE, status_3270_mode);
	register_schange(ST_PRINTER, status_printer);

	register_schange(ST_HALF_CONNECT, relabel);
	register_schange(ST_CONNECT, relabel);
	register_schange(ST_3270_MODE, relabel);

	/* See about all-bold behavior. */
	if (appres.all_bold_on)
		ab_mode = TS_ON;
	else if (!ts_value(appres.all_bold, &ab_mode))
		(void) fprintf(stderr, "invalid %s value: '%s', "
		    "assuming 'auto'\n", ResAllBold, appres.all_bold);
	if (ab_mode == TS_AUTO)
		ab_mode = appres.m3279? TS_ON: TS_OFF;

	/* If the want monochrome, assume they want green. */
	if (!appres.m3279) {
	    	defattr |= FOREGROUND_GREEN;
		if (ab_mode == TS_ON)
			defattr |= FOREGROUND_INTENSITY;
	}

	/* Pull in the user's color mappings. */
	init_user_colors();
	init_user_attribute_colors();

	/* Set up the controller. */
	ctlr_init(-1);
	ctlr_reinit(-1);

	/* Set the window label. */
	if (appres.title != CN)
		screen_title(appres.title);
	else if (profile_name != CN)
	    	screen_title(profile_name);
	else
		screen_title("wc3270");

	/* Finish screen initialization. */
	screen_init2();
	screen_suspend();
}

/* Secondary screen initialization. */
static void
screen_init2(void)
{
	/*
	 * Finish initializing ncurses.  This should be the first time that it
	 * will send anything to the terminal.
	 */
	/* nothing to do... */

	escaped = False;

	/* Subscribe to input events. */
	input_id = AddInput((int)chandle, kybd_input);
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
}

/*
 * Parse a tri-state resource value.
 * Returns True for success, False for failure.
 */
static Boolean
ts_value(const char *s, enum ts *tsp)
{
	*tsp = TS_AUTO;

	if (s != CN && s[0]) {
		int sl = strlen(s);

		if (!strncasecmp(s, "true", sl))
			*tsp = TS_ON;
		else if (!strncasecmp(s, "false", sl))
			*tsp = TS_OFF;
		else if (strncasecmp(s, "auto", sl))
			return False;
	}
	return True;
}

/* Allocate a color pair. */
static int
get_color_pair(int fg, int bg)
{
    	int mfg = fg & 0xf;
    	int mbg = bg & 0xf;

	if (mfg >= MAX_COLORS)
	    	mfg = 0;
	if (mbg >= MAX_COLORS)
	    	mbg = 0;

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

	if ((r = get_resource(resname)) == CN)
		return;
	for (i = 0; host_color[i].name != CN; i++) {
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
	init_user_attribute_color(&field_colors[0],
		ResHostColorForDefault);
	init_user_attribute_color(&field_colors[1],
		ResHostColorForIntensified);
	init_user_attribute_color(&field_colors[2],
		ResHostColorForProtected);
	init_user_attribute_color(&field_colors[3],
		ResHostColorForProtectedIntensified);
}

/*
 * Map a field attribute to a 3270 color index.
 * Applies only to m3270 mode -- does not work for mono.
 */
static int
color3270_from_fa(unsigned char fa)
{
#	define DEFCOLOR_MAP(f) \
		((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))

	return field_colors[DEFCOLOR_MAP(fa)];
}

/* Map a field attribute to its default colors. */
static int
color_from_fa(unsigned char fa)
{
	if (appres.m3279) {
		int fg;

		fg = color3270_from_fa(fa);
		return get_color_pair(fg, COLOR_NEUTRAL_BLACK);
	} else
		return FOREGROUND_GREEN |
		    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))?
		     FOREGROUND_INTENSITY: 0);
}

static int
reverse_colors(int a)
{
    	int rv = 0;

	/* Move foreground colors to background colors. */
	if (a & FOREGROUND_RED)
	    	rv |= BACKGROUND_RED;
	if (a & FOREGROUND_BLUE)
	    	rv |= BACKGROUND_BLUE;
	if (a & FOREGROUND_GREEN)
	    	rv |= BACKGROUND_GREEN;
	if (a & FOREGROUND_INTENSITY)
	    	rv |= BACKGROUND_INTENSITY;

	/* And vice versa. */
	if (a & BACKGROUND_RED)
	    	rv |= FOREGROUND_RED;
	if (a & BACKGROUND_BLUE)
	    	rv |= FOREGROUND_BLUE;
	if (a & BACKGROUND_GREEN)
	    	rv |= FOREGROUND_GREEN;
	if (a & BACKGROUND_INTENSITY)
	    	rv |= FOREGROUND_INTENSITY;

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
	if (r == CN)
		r = get_fresource("%s%d", ResConsoleColorForHostColor, ix);
	if (r == CN)
	    	return;

	l = strtoul(r, &ptr, 0);
	if (ptr != r && *ptr == '\0' && l <= 15) {
	    	cmap_fg[ix] = (int)l;
	    	cmap_bg[ix] = (int)l + 16;
		return;
	}

	xs_warning("Invalid %s value '%s'", ResConsoleColorForHostColor, r);
}

static void
init_user_colors(void)
{
	int i;

	for (i = 0; host_color[i].name != CN; i++) {
	    	init_user_color(host_color[i].name, host_color[i].index);
	}
}

/*
 * Find the display attributes for a baddr, fa_addr and fa.
 */
static int
calc_attrs(int baddr, int fa_addr, int fa)
{
    	int fg, bg, gr, a;

	/* Compute the color. */

	/* Monochrome is easy, and so is color if nothing is specified. */
	if (!appres.m3279 ||
		(!ea_buf[baddr].fg &&
		 !ea_buf[fa_addr].fg &&
		 !ea_buf[baddr].bg &&
		 !ea_buf[fa_addr].bg)) {

	    	a = color_from_fa(fa);

	} else {

		/* The current location or the fa specifies the fg or bg. */
		if (ea_buf[baddr].fg)
			fg = ea_buf[baddr].fg & 0x0f;
		else if (ea_buf[fa_addr].fg)
			fg = ea_buf[fa_addr].fg & 0x0f;
		else
			fg = color3270_from_fa(fa);

		if (ea_buf[baddr].bg)
			bg = ea_buf[baddr].bg & 0x0f;
		else if (ea_buf[fa_addr].bg)
			bg = ea_buf[fa_addr].bg & 0x0f;
		else
			bg = COLOR_NEUTRAL_BLACK;

		a = get_color_pair(fg, bg);
	}

	/* Compute the display attributes. */

	if (ea_buf[baddr].gr)
		gr = ea_buf[baddr].gr;
	else if (ea_buf[fa_addr].gr)
		gr = ea_buf[fa_addr].gr;
	else
		gr = 0;

	if (appres.highlight_underline &&
		appres.m3279 &&
		(gr & (GR_BLINK | GR_UNDERLINE)) &&
		!(gr & GR_REVERSE) &&
		!bg) {

	    	a |= BACKGROUND_INTENSITY;
	}

	if (!appres.m3279 &&
		((gr & GR_INTENSIFY) || (ab_mode == TS_ON) || FA_IS_HIGH(fa))) {

		a |= FOREGROUND_INTENSITY;
	}

	if (gr & GR_REVERSE)
		a = reverse_colors(a);

	return a;
}

/* Display what's in the buffer. */
void
screen_disp(Boolean erasing unused)
{
	int row, col;
	int a;
	int c;
	unsigned char fa;
#if defined(X3270_DBCS) /*[*/
	enum dbcs_state d;
#endif /*]*/
	int fa_addr;

	/* This may be called when it isn't time. */
	if (escaped)
		return;

	if (!screen_changed) {

		/* Draw the status line. */
		if (status_row) {
			draw_oia();
		}

		/* Move the cursor. */
		if (flipped)
			move(cursor_addr / cCOLS,
				cCOLS-1 - (cursor_addr % cCOLS));
		else
			move(cursor_addr / cCOLS, cursor_addr % cCOLS);

		if (status_row)
		    	refresh();
		else {
			COORD coord;

			coord.X = cur_col;
			coord.Y = cur_row;
			if (SetConsoleCursorPosition(sbuf, coord) == 0) {
				fprintf(stderr,
					"\nscreen_disp: "
					"SetConsoleCursorPosition(x=%d,y=%d) "
					"failed: %s\n",
					coord.X, coord.Y,
					win32_strerror(GetLastError()));
				x3270_exit(1);
			}
		}

	    	return;
	}

	fa = get_field_attribute(0);
	a = color_from_fa(fa);
	fa_addr = find_field_attribute(0); /* may be -1, that's okay */
	for (row = 0; row < ROWS; row++) {
		int baddr;

		if (!flipped)
			move(row, 0);
		for (col = 0; col < cCOLS; col++) {
			if (flipped)
				move(row, cCOLS-1 - col);
			baddr = row*cCOLS+col;
			if (ea_buf[baddr].fa) {
			    	/* Field attribute. */
			    	fa_addr = baddr;
				fa = ea_buf[baddr].fa;
				a = calc_attrs(baddr, baddr, fa);
				attrset(defattr);
				addch(' ');
			} else if (FA_IS_ZERO(fa)) {
			    	/* Blank. */
				attrset(a);
				addch(' ');
			} else {
			    	/* Normal text. */
				if (!(ea_buf[baddr].gr ||
				      ea_buf[baddr].fg ||
				      ea_buf[baddr].bg)) {
					attrset(a);
				} else {
					int b;

					/*
					 * Override some of the field
					 * attributes.
					 */
					b = calc_attrs(baddr, fa_addr, fa);
					attrset(b);
				}
#if defined(X3270_DBCS) /*[*/
				d = ctlr_dbcs_state(baddr);
				if (IS_LEFT(d)) {
					int xaddr = baddr;
					char mb[16];
					int len;
					int i;

					INC_BA(xaddr);
					len = dbcs_to_mb(ea_buf[baddr].cc,
					    ea_buf[xaddr].cc,
					    mb);
					for (i = 0; i < len; i++) {
						addch(mb[i] & 0xff);
					}
				} else if (!IS_RIGHT(d)) {
#endif /*]*/
					if (ea_buf[baddr].cs == CS_LINEDRAW) {
						c = linedraw_to_acs(ea_buf[baddr].cc);
						if (c != -1)
							addch(c);
						else
							addch(' ');
					} else if (ea_buf[baddr].cs == CS_APL ||
						   (ea_buf[baddr].cs & CS_GE)) {
						c = apl_to_acs(ea_buf[baddr].cc);
						if (c != -1)
							addch(c);
						else
							addch(' ');
					} else {
						if (toggled(MONOCASE))
							addch(asc2uc[ebc2asc[ea_buf[baddr].cc]]);
						else
							addch(ebc2asc[ea_buf[baddr].cc]);
					}
#if defined(X3270_DBCS) /*[*/
				}
#endif /*]*/
			}
		}
	}
	if (status_row)
		draw_oia();
	attrset(defattr);
	if (flipped)
		move(cursor_addr / cCOLS, cCOLS-1 - (cursor_addr % cCOLS));
	else
		move(cursor_addr / cCOLS, cursor_addr % cCOLS);
	refresh();

	screen_changed = FALSE;
}

static const char *
decode_state(int state, Boolean limited, const char *skip)
{
	static char buf[128];
	char *s = buf;
	char *space = "";

	*s = '\0';
	if (skip == CN)
	    	skip = "";
	if (state & LEFT_CTRL_PRESSED) {
		state &= ~LEFT_CTRL_PRESSED;
	    	if (strcasecmp(skip, "LeftCtrl")) {
			s += sprintf(s, "%sLeftCtrl", space);
			space = " ";
		}
	}
	if (state & RIGHT_CTRL_PRESSED) {
		state &= ~RIGHT_CTRL_PRESSED;
	    	if (strcasecmp(skip, "RightCtrl")) {
			s += sprintf(s, "%sRightCtrl", space);
			space = " ";
		}
	}
	if (state & LEFT_ALT_PRESSED) {
		state &= ~LEFT_ALT_PRESSED;
	    	if (strcasecmp(skip, "LeftAlt")) {
			s += sprintf(s, "%sLeftAlt", space);
			space = " ";
		}
	}
	if (state & RIGHT_ALT_PRESSED) {
		state &= ~RIGHT_ALT_PRESSED;
	    	if (strcasecmp(skip, "RightAlt")) {
			s += sprintf(s, "%sRightAlt", space);
			space = " ";
		}
	}
	if (state & SHIFT_PRESSED) {
		state &= ~SHIFT_PRESSED;
	    	if (strcasecmp(skip, "Shift")) {
			s += sprintf(s, "%sShift", space);
			space = " ";
		}
	}
	if (state & NUMLOCK_ON) {
		state &= ~NUMLOCK_ON;
	    	if (!limited) {
			s += sprintf(s, "%sNumLock", space);
			space = " ";
		}
	}
	if (state & SCROLLLOCK_ON) {
		state &= ~SCROLLLOCK_ON;
		if (!limited) {
			s += sprintf(s, "%sScrollLock", space);
			space = " ";
		}
	}
	if (state & ENHANCED_KEY) {
		state &= ~ENHANCED_KEY;
		if (!limited) {
			s += sprintf(s, "%sEnhanced", space);
			space = " ";
		}
	}
	if (state & !limited) {
		s += sprintf(s, "%s?0x%x", space, state);
	}

	if (!buf[0]) {
	    	return "none";
	}

	return buf;
}

/* Keyboard input. */
static void
kybd_input(void)
{
	INPUT_RECORD ir;
	DWORD nr;
	const char *s;

	/* Get the next input event. */
	if (ReadConsoleInput(chandle, &ir, 1, &nr) == 0) {
		fprintf(stderr, "ReadConsoleInput failed: %s\n",
			win32_strerror(GetLastError()));
		x3270_exit(1);
	}
	if (nr == 0)
		return;

	switch (ir.EventType) {
	case FOCUS_EVENT:
		/*trace_event("Focus\n");*/
		return;
	case KEY_EVENT:
		if (!ir.Event.KeyEvent.bKeyDown) {
			/*trace_event("KeyUp\n");*/
			return;
		}
		s = lookup_cname(ir.Event.KeyEvent.wVirtualKeyCode << 16,
			False);
		if (s == NULL)
			s = "?";
		trace_event("Key%s vkey 0x%x (%s) scan 0x%x char 0x%x state 0x%x (%s)\n",
			ir.Event.KeyEvent.bKeyDown? "Down": "Up",
			ir.Event.KeyEvent.wVirtualKeyCode, s,
			ir.Event.KeyEvent.wVirtualScanCode,
			ir.Event.KeyEvent.uChar.AsciiChar & 0xff,
			(int)ir.Event.KeyEvent.dwControlKeyState,
			decode_state(ir.Event.KeyEvent.dwControlKeyState,
			    False, CN));
		break;
	case MENU_EVENT:
		trace_event("Menu\n");
		return;
	case MOUSE_EVENT:
		trace_event("Mouse\n");
		return;
	case WINDOW_BUFFER_SIZE_EVENT:
		trace_event("WindowBufferSize\n");
		return;
	default:
		trace_event("Unknown input event %d\n", ir.EventType);
		return;
	}

	if (!ir.Event.KeyEvent.bKeyDown) {
		return;
	}

	kybd_input2(&ir);
}

static void
trace_as_keymap(KEY_EVENT_RECORD *e)
{
    	const char *s, *t;
	char buf[256];

	buf[0] = '\0';
	t = lookup_cname(e->wVirtualKeyCode << 16, /*True*/False);
	s = decode_state(e->dwControlKeyState, True, t);
	if (strcmp(s, "none")) {
	    	strcat(buf, s);
		strcat(buf, " ");
	}
	strcat(buf, "<Key>");
	if (t != CN)
		strcat(buf, t);
	else if (e->uChar.AsciiChar) {
	    	if (e->uChar.AsciiChar == ':')
		    	strcat(buf, "colon");
		else if (e->uChar.AsciiChar == ' ')
		    	strcat(buf, "space");
		else if (e->uChar.AsciiChar > ' ' && e->uChar.AsciiChar < '~')
		    	sprintf(strchr(buf, '\0'), "%c",
				e->uChar.AsciiChar);
		else if (e->uChar.AsciiChar >= 0x1 &&
			 e->uChar.AsciiChar <= 0x1a)
		    	sprintf(strchr(buf, '\0'), "%c",
				e->uChar.AsciiChar - 1 + 'a');
		else
		    	sprintf(strchr(buf, '\0'), "0x%x", e->uChar.AsciiChar);
	} else {
		strcat(buf, "???");
	}
	trace_event(" %s ->", buf);
	/* Todo:
	 *  get rid of redundant state (lctrl when lctrl is pressed)
	 */
}

static void
kybd_input2(INPUT_RECORD *ir)
{
	int k;
	char buf[16];
	unsigned long xk;
	char *action;

	/*
	 * Translate the INPUT_RECORD into an integer we can match keymaps
	 * against.
	 * In theory, this is simple -- if it's a VK_xxx, put it in the high
	 * 16 bits and leave the low 16 clear; otherwise if there's an ASCII
	 * value, put it in the low 16; otherwise give up.
	 *
	 * In practice, this is harder, because some of the VK_ codes are
	 * aliases for ASCII characters and you get both.  So the rule becomes
	 * that if you get both VK_xxx and ASCII, and they are equal, ignore
	 * the VK.
	 */
	if (ir->Event.KeyEvent.uChar.AsciiChar != 0)
		xk = ir->Event.KeyEvent.uChar.AsciiChar & 0xffff;
	else if (ir->Event.KeyEvent.wVirtualKeyCode != 0)
		xk = (ir->Event.KeyEvent.wVirtualKeyCode << 16) & 0xffff0000;
	else
		xk = 0;

	if (xk) {
	    	trace_as_keymap(&ir->Event.KeyEvent);
		action = lookup_key(xk, ir->Event.KeyEvent.dwControlKeyState);
		if (action != CN) {
			if (strcmp(action, "[ignore]"))
				push_keymap_action(action);
			return;
		}
	}

	ia_cause = IA_DEFAULT;

	k = ir->Event.KeyEvent.wVirtualKeyCode;

	/* These first cases apply to both 3270 and NVT modes. */
	switch (k) {
	case VK_ESCAPE:
		action_internal(Escape_action, IA_DEFAULT, CN, CN);
		return;
	case VK_UP:
		action_internal(Up_action, IA_DEFAULT, CN, CN);
		return;
	case VK_DOWN:
		action_internal(Down_action, IA_DEFAULT, CN, CN);
		return;
	case VK_LEFT:
		action_internal(Left_action, IA_DEFAULT, CN, CN);
		return;
	case VK_RIGHT:
		action_internal(Right_action, IA_DEFAULT, CN, CN);
		return;
	case VK_HOME:
		action_internal(Home_action, IA_DEFAULT, CN, CN);
		return;
	default:
		break;
	}

	/* Then look for 3270-only cases. */
	if (IN_3270) switch(k) {
	/* These cases apply only to 3270 mode. */
#if 0
	case VK_OEM_CLEAR:
		action_internal(Clear_action, IA_DEFAULT, CN, CN);
		return;
	case 0x12:
		action_internal(Reset_action, IA_DEFAULT, CN, CN);
		return;
	case 'L' & 0x1f:
		action_internal(Redraw_action, IA_DEFAULT, CN, CN);
		return;
#endif
	case VK_TAB:
		action_internal(Tab_action, IA_DEFAULT, CN, CN);
		return;
	case VK_DELETE:
		action_internal(Delete_action, IA_DEFAULT, CN, CN);
		return;
	case VK_BACK:
		action_internal(BackSpace_action, IA_DEFAULT, CN, CN);
		return;
	case VK_RETURN:
		action_internal(Enter_action, IA_DEFAULT, CN, CN);
		return;
#if 0
	case '\n':
		action_internal(Newline_action, IA_DEFAULT, CN, CN);
		return;
#endif
	default:
		break;
	}

	/* Do some NVT-only translations. */
	if (IN_ANSI) switch(k) {
	case VK_DELETE:
		k = 0x7f;
		break;
	case VK_BACK:
		k = '\b';
		break;
	}

	/* Catch PF keys. */
	if (k >= VK_F1 && k <= VK_F24) {
		(void) sprintf(buf, "%d", k - VK_F1 + 1);
		action_internal(PF_action, IA_DEFAULT, buf, CN);
		return;
	}

	/* Then any other 8-bit ASCII character. */
	k = ir->Event.KeyEvent.uChar.AsciiChar & 0xff;
	if (k && !(k & ~0xff)) {
		char ks[6];
		String params[2];
		Cardinal one;

		if (k >= ' ') {
			ks[0] = k;
			ks[1] = '\0';
		} else {
			(void) sprintf(ks, "0x%x", k);
		}
		params[0] = ks;
		params[1] = CN;
		one = 1;
		Key_action(NULL, NULL, params, &one);
		return;
	}
	trace_event(" dropped (no default)\n");
}

void
screen_suspend(void)
{
	static Boolean need_to_scroll = False;

	if (!escaped) {
		escaped = True;
		endwin();

		if (need_to_scroll)
			printf("\n");
		else
			need_to_scroll = True;
		RemoveInput(input_id);
	}
}

void
screen_resume(void)
{
	escaped = False;

	screen_disp(False);
	refresh();
	input_id = AddInput((int)chandle, kybd_input);
}

void
cursor_move(int baddr)
{
	cursor_addr = baddr;
}

void
toggle_monocase(struct toggle *t unused, enum toggle_type tt unused)
{
	screen_disp(False);
}

/* Status line stuff. */

static Boolean status_ta = False;
static Boolean status_rm = False;
static Boolean status_im = False;
static Boolean status_secure = False;
static Boolean oia_boxsolid = False;
static Boolean oia_undera = True;
static Boolean oia_compose = False;
static Boolean oia_printer = False;
static unsigned char oia_compose_char = 0;
static enum keytype oia_compose_keytype = KT_STD;
#define LUCNT	8
static char oia_lu[LUCNT+1];

static char *status_msg = "";

void
status_ctlr_done(void)
{
	oia_undera = True;
}

void
status_insert_mode(Boolean on)
{
	status_im = on;
}

void
status_minus(void)
{
	status_msg = "X -f";
}

void
status_oerr(int error_type)
{
	switch (error_type) {
	case KL_OERR_PROTECTED:
		status_msg = "X Protected";
		break;
	case KL_OERR_NUMERIC:
		status_msg = "X Numeric";
		break;
	case KL_OERR_OVERFLOW:
		status_msg = "X Overflow";
		break;
	}
}

void
status_reset(void)
{
	if (kybdlock & KL_ENTER_INHIBIT)
		status_msg = "X Inhibit";
	else if (kybdlock & KL_DEFERRED_UNLOCK)
		status_msg = "X";
	else
		status_msg = "";
}

void
status_reverse_mode(Boolean on)
{
	status_rm = on;
}

void
status_syswait(void)
{
	status_msg = "X SYSTEM";
}

void
status_twait(void)
{
	oia_undera = False;
	status_msg = "X Wait";
}

void
status_typeahead(Boolean on)
{
	status_ta = on;
}

void    
status_compose(Boolean on, unsigned char c, enum keytype keytype)
{
        oia_compose = on;
        oia_compose_char = c;
        oia_compose_keytype = keytype;
}

void
status_lu(const char *lu)
{
	if (lu != NULL) {
		(void) strncpy(oia_lu, lu, LUCNT);
		oia_lu[LUCNT] = '\0';
	} else
		(void) memset(oia_lu, '\0', sizeof(oia_lu));
}

static void
status_connect(Boolean connected)
{
	if (connected) {
		oia_boxsolid = IN_3270 && !IN_SSCP;
		if (kybdlock & KL_AWAITING_FIRST)
			status_msg = "X";
		else
			status_msg = "";
#if defined(HAVE_LIBSSL) /*[*/
		status_secure = secure_connection;
#endif /*]*/
	} else {
		oia_boxsolid = False;
		status_msg = "X Disconnected";
		status_secure = False;
	}       
}

static void
status_3270_mode(Boolean ignored unused)
{
	oia_boxsolid = IN_3270 && !IN_SSCP;
	if (oia_boxsolid)
		oia_undera = True;
}

static void
status_printer(Boolean on)
{
	oia_printer = on;
}

static void
draw_oia(void)
{
	int rmargin;

	rmargin = maxCOLS - 1;

	/* Make sure the status line region is filled in properly. */
	if (appres.m3279) {
		int i;

		attrset(defattr);
		if (status_skip) {
			move(status_skip, 0);
			for (i = 0; i < rmargin; i++) {
				printw(" ");
			}
		}
		move(status_row, 0);
		for (i = 0; i < rmargin; i++) {
			printw(" ");
		}
	}

	if (appres.m3279)
	    	attrset(BACKGROUND_INTENSITY);
	else
		attrset(reverse_colors(defattr));
	mvprintw(status_row, 0, "4");
	if (oia_undera)
		printw("%c", IN_E? 'B': 'A');
	else
		printw(" ");
	if (appres.m3279)
	    	attrset(BACKGROUND_INTENSITY);
	else
		attrset(reverse_colors(defattr));
	if (IN_ANSI)
		printw("N");
	else if (oia_boxsolid)
		printw(" ");
	else if (IN_SSCP)
		printw("S");
	else
		printw("?");

	attrset(FOREGROUND_INTENSITY);
	mvprintw(status_row, 8, "%-11s", status_msg);
	mvprintw(status_row, rmargin-36,
	    "%c%c %c  %c%c%c",
	    oia_compose? 'C': ' ',
	    oia_compose? oia_compose_char: ' ',
	    status_ta? 'T': ' ',
	    status_rm? 'R': ' ',
	    status_im? 'I': ' ',
	    oia_printer? 'P': ' ');
	if (status_secure) {
	    	attrset(get_color_pair(COLOR_GREEN, COLOR_NEUTRAL_BLACK) |
			FOREGROUND_INTENSITY);
		printw("S");
		attrset(FOREGROUND_INTENSITY);
	} else
	    	printw(" ");

	mvprintw(status_row, rmargin-25, "%s", oia_lu);
	mvprintw(status_row, rmargin-7,
	    "%03d/%03d", cursor_addr/cCOLS + 1, cursor_addr%cCOLS + 1);
}

void
Redraw_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params unused)
{
	if (!escaped) {
		endwin();
		refresh();
	}
}

void
ring_bell(void)
{
	/*Beep(750, 300);*/
}

void
screen_flip(void)
{
	flipped = !flipped;
	screen_disp(False);
}

void
screen_132(void)
{
}

void
screen_80(void)
{
}

/*
 * Translate an x3270 font line-drawing character (the first two rows of a
 * standard X11 fixed-width font) to an ASCII-art equivalent.
 *
 * Returns -1 if there is no translation.
 */
static int
linedraw_to_acs(unsigned char c)
{
    	int r;

	/* FIXME: Need aplmap equivalent functionality for xterm linedraw. */

	/* Use Unicode. */
	switch (c) {
	case 0x0:	/* '_', block */
		r = -1;
		break;
	case 0x1:	/* '`', diamond */
		r = 0x25c6;
		break;
	case 0x2:	/* 'a', checkerboard */
		r = -1;
		break;
	case 0x7:	/* 'f', degree */
		r = 0xb0;
		break;
	case 0x8:	/* 'g', plusminus */
		r = 0xb1;
		break;
	case 0x9:	/* 'h', board? */
		r = -1;
		break;
	case 0xa:	/* 'i', lantern? */
		r = -1;
		break;
	case 0xb:	/* 'j', LR corner */
		r = 0x2518;
		break;
	case 0xc:	/* 'k', UR corner */
		r = 0x2510;
		break;
	case 0xd:	/* 'l', UL corner */
		r = 0x250c;
		break;
	case 0xe:	/* 'm', LL corner */
		r = 0x2514;
		break;
	case 0xf:	/* 'n', plus */
		r = 0x253c;
		break;
	case 0x10:	/* 'o', top horizontal */
		r = '-';
		break;
	case 0x11:	/* 'p', row 3 horizontal */
		r = '-';
		break;
	case 0x12:	/* 'q', middle horizontal */
		r = 0x2500;
		break;
	case 0x13:	/* 'r', row 7 horizontal */
		r = '-';
		break;
	case 0x14:	/* 's', bottom horizontal */
		r = '_';
		break;
	case 0x15:	/* 't', left tee */
		r = 0x251c;
		break;
	case 0x16:	/* 'u', right tee */
		r = 0x2524;
		break;
	case 0x17:	/* 'v', bottom tee */
		r = 0x2534;
		break;
	case 0x18:	/* 'w', top tee */
		r = 0x252c;
		break;
	case 0x19:	/* 'x', vertical line */
		r = 0x2502;
		break;
	case 0x1a:	/* 'y', less or equal */
		r = 0x2264;
		break;
	case 0x1b:	/* 'z', greater or equal */
		r = 0x2265;
		break;
	case 0x1c:	/* '{', pi */
		r = 0x03c0;
		break;
	case 0x1d:	/* '|', not equal */
		r = 0x2260;
		break;
	case 0x1e:	/* '}', sterling */
		r = 0xa3;
		break;
	case 0x1f:	/* '~', bullet */
		r = 0x2022;
		break;
	default:
		r = -1;
		break;
	}

	/* If we're pre-NT, we can't assume that Unicode works. */
	if (!is_nt && (r & ~0xff))
	    	r = -1;

	return r;
}

int have_aplmap = 0;
unsigned char aplmap[256];

static int
apl_to_acs(unsigned char c)
{
    	int r;

	/* If there's an explicit map for this Windows code page, use it. */
	if (have_aplmap) {
	    	r = aplmap[c];
		return r? r: -1;
	}

	/* Use Unicode. */
	switch (c) {
	case 0xaf: /* CG 0xd1, degree */
		r = 0xb0;	/* XXX may not map to bullet in current
				       codepage */
		break;
	case 0xd4: /* CG 0xac, LR corner */
		r = 0x2518;
		break;
	case 0xd5: /* CG 0xad, UR corner */
		r = 0x2510;
		break;
	case 0xc5: /* CG 0xa4, UL corner */
		r = 0x250c;
		break;
	case 0xc4: /* CG 0xa3, LL corner */
		r = 0x2514;
		break;
	case 0xd3: /* CG 0xab, plus */
		r = 0x253c;
		break;
	case 0xa2: /* CG 0x92, horizontal line */
		r = 0x2500;
		break;
	case 0xc6: /* CG 0xa5, left tee */
		r = 0x251c;
		break;
	case 0xd6: /* CG 0xae, right tee */
		r = 0x2524;
		break;
	case 0xc7: /* CG 0xa6, bottom tee */
		r = 0x2534;
		break;
	case 0xd7: /* CG 0xaf, top tee */
		r = 0x252c;
		break;
	case 0xbf: /* CG 0x15b, stile */
	case 0x85: /* CG 0x184, vertical line */
		r = 0x2502;
		break;
	case 0x8c: /* CG 0xf7, less or equal */
		r = 0x2264;
		break;
	case 0xae: /* CG 0xd9, greater or equal */
		r = 0x2265;
		break;
	case 0xbe: /* CG 0x3e, not equal */
		r = 0x2260;
		break;
	case 0xa3: /* CG 0x93, bullet */
		r = 0x2022;
		break;
	case 0xad:
		r = '[';
		break;
	case 0xbd:
		r = ']';
		break;
	default:
		r = -1;
		break;
	}

	/* If pre-NT, we can't assume that Unicode works. */
	if (!is_nt && (r & ~0xff))
	    	r = -1;

	return r;
}

/* Read the aplMap.<windows-codepage> resource into aplmap[]. */
static void
check_aplmap(int codepage)
{
	char *r = get_fresource("%s.%d", ResAplMap, codepage);
	char *s;
	char *left, *right;

	if (r == CN) {
	    	return;
	}

	have_aplmap = 1;
	r = NewString(r);
	s = r;
	while (split_dresource(&s, &left, &right) == 1) {
	    	unsigned long l, r;

		l = strtoul(left, NULL, 0);
		r = strtoul(right, NULL, 0);
		if (l > 0 && l <= 0xff && r > 0 && r <= 0xff) {
		    	aplmap[l] = (unsigned char)r;
		}
	}
	Free(r);
}

/*
 * Windows-specific Paste action, that takes advantage of the existing x3270
 * instrastructure for multi-line paste.
 */
void
Paste_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
    	HGLOBAL hglb;
	LPTSTR lptstr;

    	action_debug(Paste_action, event, params, num_params);
	if (check_usage(Paste_action, *num_params, 0, 0) < 0)
	    	return;

    	if (!IsClipboardFormatAvailable(CF_TEXT))
		return; 
	if (!OpenClipboard(NULL))
		return;
	hglb = GetClipboardData(CF_TEXT);
	if (hglb != NULL) {
		lptstr = GlobalLock(hglb);
		if (lptstr != NULL) { 
			emulate_input(lptstr, strlen(lptstr), True);
			GlobalUnlock(hglb); 
		}
	}
	CloseClipboard(); 
}

/* Set the window title. */
void
screen_title(char *text)
{
	(void) SetConsoleTitle(text);
}

void
Title_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
    	action_debug(Title_action, event, params, num_params);
	if (check_usage(Title_action, *num_params, 1, 1) < 0)
	    	return;

	screen_title(params[0]);
}

static void
relabel(Boolean ignored unused)
{
	char *title;

	if (appres.title != CN)
	    	return;

	if (PCONNECTED) {
	    	char *hostname;

		if (profile_name != CN)
		    	hostname = profile_name;
		else
		    	hostname = reconnect_host;

		title = Malloc(10 + (PCONNECTED ? strlen(hostname) : 0));
	    	(void) sprintf(title, "%s - wc3270", hostname);
		screen_title(title);
		Free(title);
	} else {
	    	screen_title("wc3270");
	}
}
