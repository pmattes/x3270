/*
 * Copyright (c) 2000-2014, Paul Mattes.
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
 *	screen.c
 *		A curses-based 3270 Terminal Emulator
 *		Screen drawing
 */

#include "globals.h"
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "gluec.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "screenc.h"
#include "scrollc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"
#include "xioc.h"

#include "menubarc.h"

/*
 * The usual x3270 COLS variable (current number of columns in the 3270
 * display) is called cCOLS in c3270, to avoid a conflict with curses COLS (the
 * number of columns on the physical termal). For c3270, globals.h #defines
 * COLS as cCOLS, so common code can use COLS transparently -- everywhere but
 * here. In this module, we #undef COLS, after #including globals.h but before
 * #including curses.h, and we use (curses) COLS and (c3270) cCOLS explicitly.
 */
#undef COLS

#if defined(HAVE_NCURSESW_NCURSES_H) /*[*/
# include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H) /*][*/
# include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_H) /*][*/
# include <ncurses.h>
#else /*][*/
# include <curses.h>
#endif /*]*/
#if defined(HAVE_NCURSESW_TERM_H) /*[*/
# include <ncursesw/term.h>
#elif defined(HAVE_NCURSES_TERM_H) /*][*/
# include <ncurses/term.h>
#elif defined(HAVE_TERM_H) /*][*/
# include <term.h>
#endif /*]*/

/* Curses' 'COLS' becomes cursesCOLS, to remove any ambiguity. */
#define cursesCOLS	COLS
#define cursesLINES	LINES

#define STATUS_SCROLL_START_MS	1500
#define STATUS_SCROLL_MS	100
#define STATUS_PUSH_MS		5000

#define CM (60*10)	/* csec per minute */

static int cp[16][16][2];

static int cmap8[16] = {
	COLOR_BLACK,	/* neutral black */
	COLOR_BLUE,	/* blue */
	COLOR_RED,	/* red */
	COLOR_MAGENTA,	/* pink */
	COLOR_GREEN,	/* green */
	COLOR_CYAN,	/* turquoise */
	COLOR_YELLOW,	/* yellow */
	COLOR_WHITE,	/* neutral white */

	COLOR_BLACK,	/* black */ /* alas, this may be gray */
	COLOR_BLUE,	/* deep blue */
	COLOR_YELLOW,	/* orange */
	COLOR_MAGENTA,	/* purple */
	COLOR_GREEN,	/* pale green */
	COLOR_CYAN,	/* pale turquoise */
	COLOR_BLACK,	/* gray */
	COLOR_WHITE	/* white */
};

static int cmap16[16] = {
	COLOR_BLACK,	/* neutral black */
	8 + COLOR_BLUE,	/* blue */
	COLOR_RED,	/* red */
	8 + COLOR_MAGENTA,	/* pink */
	8 + COLOR_GREEN,	/* green */
	8 + COLOR_CYAN,	/* turquoise */
	8 + COLOR_YELLOW,	/* yellow */
	8 + COLOR_WHITE,	/* neutral white */

	COLOR_BLACK,	/* black */ /* alas, this may be gray */
	COLOR_BLUE,	/* deep blue */
	8 + COLOR_RED,	/* orange */
	COLOR_MAGENTA,	/* purple */
	COLOR_GREEN,	/* pale green */
	COLOR_CYAN,	/* pale turquoise */
	COLOR_WHITE,	/* gray */
	8 + COLOR_WHITE	/* white */
};

static int *cmap = cmap8;
static int defcolor_offset = 0;

static int field_colors8[4] = {
	COLOR_GREEN,	/* default */
	COLOR_RED,	/* intensified */
	COLOR_BLUE,	/* protected */
	COLOR_WHITE	/* protected, intensified */
};

static int field_colors16[4] = {
	8 + COLOR_GREEN,/* default */
	COLOR_RED,	/* intensified */
	8 + COLOR_BLUE,	/* protected */
	8 + COLOR_WHITE	/* protected, intensified */
};

static int *field_colors = field_colors8;

static int bg_color = COLOR_BLACK;

static int defattr = A_NORMAL;
static unsigned long input_id;

Boolean escaped = True;
Boolean initscr_done = False;

enum ts { TS_AUTO, TS_ON, TS_OFF };
enum ts me_mode = TS_AUTO;
enum ts ab_mode = TS_AUTO;

#if defined(C3270_80_132) /*[*/
struct screen_spec {
	int rows, cols;
	char *mode_switch;
} screen_spec;
struct screen_spec altscreen_spec, defscreen_spec;
static SCREEN *def_screen = NULL, *alt_screen = NULL;
static SCREEN *cur_screen = NULL;
static void parse_screen_spec(const char *str, struct screen_spec *spec);
#endif /*]*/

static struct {
	char *name;
	int index;
} cc_name[] = {
	{ "black",	COLOR_BLACK },
	{ "red",	COLOR_RED },
	{ "green",	COLOR_GREEN },
	{ "yellow",	COLOR_YELLOW },
	{ "blue",	COLOR_BLUE },
	{ "magenta",    COLOR_MAGENTA },
	{ "cyan",	COLOR_CYAN },
	{ "white",	COLOR_WHITE },
	{ NULL,	0 }
};

static int status_row = 0;	/* Row to display the status line on */
static int status_skip = 0;	/* Row to blank above the status line */
static int screen_yoffset = 0;	/* Vertical offset to top of screen.
				   If 0, there is no menu bar.
				   If nonzero (2, actually), menu bar is at the
				    top of the display. */

static Boolean curses_alt = False;
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
static Boolean default_colors = False;
#endif /*]*/
static Boolean screen_initted = False;

static void kybd_input(unsigned long fd, ioid_t id);
static void kybd_input2(int k, ucs4_t ucs4, int alt);
static void draw_oia(void);
static void screen_connect(Boolean connected);
static void status_half_connect(Boolean ignored);
static void status_connect(Boolean ignored);
static void status_3270_mode(Boolean ignored);
static void status_printer(Boolean on);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char);
static void set_status_row(int screen_rows, int emulator_rows);
static Boolean ts_value(const char *s, enum ts *tsp);
static void display_linedraw(unsigned char ebc);
static void display_ge(unsigned char ebc);
static void init_user_colors(void);
static void init_user_attribute_colors(void);
static void screen_init2(void);

/* Initialize the screen. */
void
screen_init(void)
{
    	menu_init();
	register_schange(ST_CONNECT, screen_connect);

#if !defined(C3270_80_132) /*[*/
	/* Disallow altscreen/defscreen. */
	if ((appres.altscreen != NULL) || (appres.defscreen != NULL)) {
		(void) fprintf(stderr, "altscreen/defscreen not supported\n");
		exit(1);
	}
#else /*][*/
	/* Parse altscreen/defscreen. */
	if ((appres.altscreen != NULL) ^ (appres.defscreen != NULL)) {
		(void) fprintf(stderr,
		    "Must specify both altscreen and defscreen\n");
		exit(1);
	}
	if (appres.altscreen != NULL) {
		parse_screen_spec(appres.altscreen, &altscreen_spec);
		if (altscreen_spec.rows < 27 || altscreen_spec.cols < 132) {
		    (void) fprintf(stderr, "Rows and/or cols too small on "
			"alternate screen (mininum 27x132)\n");
		    exit(1);
		}
		parse_screen_spec(appres.defscreen, &defscreen_spec);
		if (defscreen_spec.rows < 24 || defscreen_spec.cols < 80) {
		    (void) fprintf(stderr, "Rows and/or cols too small on "
			"default screen (mininum 24x80)\n");
		    exit(1);
		}
	}
#endif /*]*/

	/*
	 * See about keyboard Meta-key behavior.
	 *
	 * Note: Formerly, "auto" meant to use the terminfo 'km' capability (if
	 * set, then disable metaEscape).  But popular terminals like the
	 * Linux console and xterms are actually configurable, though they have
	 * fixed terminfo capabilities.  It is harmless to enable metaEscape
	 * when the terminal supports it, so the default is now 'on'.
	 *
	 * Setting the high bit for the Meta key is a pretty achaic idea, IMO,
	 * so we no loger support it.
	 */
	if (!ts_value(appres.meta_escape, &me_mode))
		popup_an_error("Invalid %s value: '%s', "
		    "assuming 'auto'\n", ResMetaEscape, appres.meta_escape);
	if (me_mode == TS_AUTO)
		me_mode = TS_ON;

	/* See about all-bold behavior. */
	if (appres.all_bold_on)
		ab_mode = TS_ON;
	else if (!ts_value(appres.all_bold, &ab_mode))
		popup_an_error("Invalid %s value: '%s', "
		    "assuming 'auto'\n", ResAllBold, appres.all_bold);
	if (ab_mode == TS_AUTO)
		ab_mode = (appres.m3279 && (appres.color8 || COLORS < 16))? 
		    TS_ON: TS_OFF;
	if (ab_mode == TS_ON)
		defattr |= A_BOLD;

	/*
	 * If they don't want ACS and they're not in a UTF-8 locale, switch
	 * to ASCII-art mode for box drawing.
	 */
	if (!appres.acs && !is_utf8)
		appres.ascii_box_draw = True;

	/* Pull in the user's color mappings. */
	init_user_colors();
	init_user_attribute_colors();
}

/*
 * Finish screen initialization, when a host connects or when we go into
 * 'zombie' mode (no prompt, no connection).
 */
static void
finish_screen_init(void)
{
	int want_ov_rows = ov_rows;
	int want_ov_cols = ov_cols;
	Boolean oversize = False;
	char *cl;

	if (screen_initted)
		return;

	screen_initted = True;

	/* Clear the (original) screen first. */
#if defined(C3270_80_132) /*[*/
	if (appres.defscreen != NULL) {
		char nbuf[64];

		(void) sprintf(nbuf, "COLUMNS=%d", defscreen_spec.cols);
		putenv(NewString(nbuf));
		(void) sprintf(nbuf, "LINES=%d", defscreen_spec.rows);
		putenv(NewString(nbuf));
	}
#endif /*]*/
	(void) setupterm(NULL, fileno(stdout), NULL);
	if ((cl = tigetstr("clear")) != NULL)
	    	putp(cl);

#if !defined(C3270_80_132) /*[*/
	/* Initialize curses. */
	if (initscr() == NULL) {
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		exit(1);
	}
	initscr_done = True;
#else /*][*/
	/* Set up ncurses, and see if it's within bounds. */
	if (appres.defscreen != NULL) {
		char nbuf[64];

		(void) sprintf(nbuf, "COLUMNS=%d", defscreen_spec.cols);
		putenv(NewString(nbuf));
		(void) sprintf(nbuf, "LINES=%d", defscreen_spec.rows);
		putenv(NewString(nbuf));
		def_screen = newterm(NULL, stdout, stdin);
		initscr_done = True;
		if (def_screen == NULL) {
			(void) fprintf(stderr,
			    "Can't initialize %dx%d defscreen terminal.\n",
			    defscreen_spec.rows, defscreen_spec.cols);
			exit(1);
		}
		if (write(1, defscreen_spec.mode_switch,
		    strlen(defscreen_spec.mode_switch)) < 0) {
			endwin();
		    	exit(1);
		}
	}
	if (appres.altscreen) {
		char nbuf[64];

		(void) sprintf(nbuf, "COLUMNS=%d", altscreen_spec.cols);
		putenv(NewString(nbuf));
		(void) sprintf(nbuf, "LINES=%d", altscreen_spec.rows);
		putenv(NewString(nbuf));
	}
	alt_screen = newterm(NULL, stdout, stdin);
	if (alt_screen == NULL) {
		popup_an_error("Can't initialize terminal.\n");
		exit(1);
	}
	initscr_done = True;
	if (def_screen == NULL) {
	    	def_screen = alt_screen;
		cur_screen = def_screen;
	}
	if (appres.altscreen) {
		set_term(alt_screen);
		cur_screen = alt_screen;
	}

	/* If they want 80/132 switching, then they want a model 5. */
	if (def_screen != alt_screen && model_num != 5) {
		set_rows_cols(5, 0, 0);
	}
#endif /*]*/

	while (cursesLINES < maxROWS || cursesCOLS < maxCOLS) {
		/*
		 * First, cancel any oversize.  This will get us to the correct
		 * model number, if there is any.
		 */
		if ((ov_cols && ov_cols > cursesCOLS) ||
		    (ov_rows && ov_rows > cursesLINES)) {
			ov_cols = 0;
			ov_rows = 0;
			oversize = True;
			continue;
		}

		/* If we're at the smallest screen now, give up. */
		if (model_num == 2) {
			popup_an_error("Emulator won't fit on a %dx%d "
			    "display.\n", cursesLINES, cursesCOLS);
			exit(1);
		}

		/* Try a smaller model. */
		set_rows_cols(model_num - 1, 0, 0);
	}

	/*
	 * Now, if they wanted an oversize, but didn't get it, try applying it
	 * again.
	 */
	if (oversize) {
		if (want_ov_rows > cursesLINES - 2)
			want_ov_rows = cursesLINES - 2;
		if (want_ov_rows < maxROWS)
			want_ov_rows = maxROWS;
		if (want_ov_cols > cursesCOLS)
			want_ov_cols = cursesCOLS;
		set_rows_cols(model_num, want_ov_cols, want_ov_rows);
	}

	/*
	 * Finally, if they want automatic oversize, see if that's possible.
	 */
	if (ov_auto && (maxROWS < cursesLINES - 2 || maxCOLS < cursesCOLS))
		set_rows_cols(model_num, cursesCOLS, cursesLINES - 2);

#if defined(NCURSES_MOUSE_VERSION) /*[*/
	if (appres.mouse)
		if (mousemask(BUTTON1_RELEASED, NULL) == 0)
		    	appres.mouse = False;
#endif /*]*/

	/* Figure out where the status line goes, if it fits. */
#if defined(C3270_80_132) /*[*/
	if (def_screen != alt_screen) {
		/* Start out in defscreen mode. */
		set_status_row(defscreen_spec.rows, MODEL_2_ROWS);
	} else
#endif /*]*/
	{
		/* Start out in altscreen mode. */
		set_status_row(cursesLINES, maxROWS);
	}

	/* Set up callbacks for state changes. */
	register_schange(ST_HALF_CONNECT, status_half_connect);
	register_schange(ST_CONNECT, status_connect);
	register_schange(ST_3270_MODE, status_3270_mode);
	register_schange(ST_PRINTER, status_printer);

	/* Implement reverse video. */
	if (appres.reverse_video) {
	    	int c;

		bg_color = COLOR_WHITE;

		c = cmap8[HOST_COLOR_NEUTRAL_BLACK];
		cmap8[HOST_COLOR_NEUTRAL_BLACK] =
		    cmap8[HOST_COLOR_NEUTRAL_WHITE];
		cmap8[HOST_COLOR_NEUTRAL_WHITE] = c;

		c = cmap16[HOST_COLOR_NEUTRAL_BLACK];
		cmap16[HOST_COLOR_NEUTRAL_BLACK] =
		    cmap16[HOST_COLOR_NEUTRAL_WHITE];
		cmap16[HOST_COLOR_NEUTRAL_WHITE] = c;
	}

	/* Play with curses color. */
	if (!appres.mono) {
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
	    	char *colorterm;
#endif /*]*/
		start_color();
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
		if ((appres.default_fgbg ||
		     ((colorterm = getenv("COLORTERM")) != NULL &&
		      !strcmp(colorterm, "gnome-terminal"))) &&
		    use_default_colors() != ERR) {

		    default_colors = True;
		}
#endif /*]*/
		if (has_colors() && COLORS >= 8) {
		    	if (!appres.color8 && COLORS >= 16) {
				cmap = cmap16;
				field_colors = field_colors16;
				defcolor_offset = 8;
				if (appres.reverse_video)
					bg_color += defcolor_offset;
			}
		    	if (appres.m3279)
				defattr =
				    get_color_pair(
					    defcolor_offset + COLOR_BLUE,
					    bg_color);
			else
				defattr =
				    get_color_pair(
					    defcolor_offset + COLOR_GREEN,
					    bg_color);
			if (COLORS < 16)
			    	appres.color8 = True;
#if defined(C3270_80_132) && defined(NCURSES_VERSION)  /*[*/
			if (def_screen != alt_screen) {
				SCREEN *s = cur_screen;

				/*
				 * Initialize the colors for the other
				 * screen.
				 */
				if (s == def_screen)
					set_term(alt_screen);
				else
					set_term(def_screen);
				start_color();
				curses_alt = !curses_alt;
				(void) get_color_pair(field_colors[2],
						      bg_color);
				curses_alt = !curses_alt;
				set_term(s);

			}
#endif /*]*/
		}
		else {
		    	appres.mono = True;
			appres.m3279 = False;
			/* Get the terminal name right. */
			set_rows_cols(model_num, want_ov_cols, want_ov_rows);
		}
	}


	/* Set up the controller. */
	ctlr_init(-1);
	ctlr_reinit(-1);

	/* Set up the scrollbar. */
	scroll_init();

	screen_init2();
}

/* When the host connects, really initialize the screen. */
static void
screen_connect(Boolean connected)
{
	if (connected && !screen_initted)
	    	finish_screen_init();
}

/* Configure the TTY settings for a curses screen. */
static void
setup_tty(void)
{
	if (appres.cbreak_mode)
		cbreak();
	else
		raw();
	noecho();
	nonl();
	intrflush(stdscr,FALSE);
	if (appres.curses_keypad)
		keypad(stdscr, TRUE);
	meta(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	refresh();
}

#if defined(C3270_80_132) /*[*/
static void
swap_screens(SCREEN *new_screen)
{
	set_term(new_screen);
	cur_screen = new_screen;
}
#endif /*]*/

/* Secondary screen initialization. */
static void
screen_init2(void)
{
	escaped = False;

	/*
	 * Finish initializing ncurses.  This should be the first time that it
	 * will send anything to the terminal.
	 */

	/* Set up the keyboard. */
#if defined(C3270_80_132) /*[*/
	swap_screens(alt_screen);
#endif /*]*/
	setup_tty();
	scrollok(stdscr, FALSE);

#if defined(C3270_80_132) /*[*/
	if (def_screen != alt_screen) {
		/*
		 * The first setup_tty() set up altscreen.
		 * Set up defscreen now, and leave it as the
		 * current curses screen.
		 */
		swap_screens(def_screen);
		setup_tty();
		scrollok(stdscr, FALSE);
#if defined(NCURSES_MOUSE_VERSION) /*[*/
		if (appres.mouse)
			mousemask(BUTTON1_RELEASED, NULL);
#endif /*]*/
	}
#endif /*]*/

	/* Subscribe to input events. */
	input_id = AddInput(0, kybd_input);

	/* Ignore SIGINT and SIGTSTP. */
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

#if defined(C3270_80_132) /*[*/
	/* Ignore SIGWINCH -- it might happen when we do 80/132 changes. */
	if (def_screen != alt_screen)
		signal(SIGWINCH, SIG_IGN);
#endif /*]*/
}

/* Calculate where the status line goes now. */
static void
set_status_row(int screen_rows, int emulator_rows)
{
    	/* Check for OIA room first. */
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
	if (appres.menubar && appres.mouse) {
		if (screen_rows >= emulator_rows + (status_row != 0) + 2)
			screen_yoffset = 2;
		else if (screen_rows >= emulator_rows + (status_row != 0) + 1)
			screen_yoffset = 1;
		else
			screen_yoffset = 0;
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

	if (s != NULL && s[0]) {
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
	static int next_pair[2] = { 1, 1 };
	int pair;
#if defined(C3270_80_132) && defined(NCURSES_VERSION) /*[*/
		/* ncurses allocates colors for each screen. */
	int pair_index = !!curses_alt;
#else /*][*/
		/* curses allocates colors globally. */
	const int pair_index = 0;
#endif /*]*/
	int bg_arg = bg;
	int fg_arg = fg;

	if ((pair = cp[fg][bg][pair_index]))
		return COLOR_PAIR(pair);
	if (next_pair[pair_index] >= COLOR_PAIRS)
		return 0;
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
	/*
	 * Assume that by default, the terminal displays some sort of 'white'
	 * against some sort of 'black', and that looks better than the
	 * explicit curses COLOR_WHITE over COLOR_BLACK.
	 */
	if (default_colors) {
	    if (bg == COLOR_BLACK)
		bg_arg = -1; /* use the default background, not black */
	    if (fg == COLOR_WHITE)
		fg_arg = -1; /* use the default foreground, not white */
	}
#endif /*]*/
	if (init_pair(next_pair[pair_index], fg_arg, bg_arg) != OK)
		return 0;
	pair = cp[fg][bg][pair_index] = next_pair[pair_index]++;
	return COLOR_PAIR(pair);
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

    	if ((r = get_resource(resname)) == NULL)
		return;
	for (i = 0; cc_name[i].name != NULL; i++) {
	    	if (!strcasecmp(r, cc_name[i].name)) {
		    	*a = cc_name[i].index;
			return;
		}
	}
	l = strtoul(r, &ptr, 0);
	if (ptr == r || *ptr != '\0' || (int)l >= COLORS) {
	    	xs_warning("Invalid %s value: %s", resname, r);
	    	return;
	}
	*a = (int)l;
}

static void
init_user_attribute_colors(void)
{
	init_user_attribute_color(&field_colors[0],
		ResCursesColorForDefault);
	init_user_attribute_color(&field_colors[1],
		ResCursesColorForIntensified);
	init_user_attribute_color(&field_colors[2],
		ResCursesColorForProtected);
	init_user_attribute_color(&field_colors[3],
		ResCursesColorForProtectedIntensified);
}

/*
 * Map a field attribute to a curses color index.
 * Applies only to m3270 mode -- does not work for mono.
 */
static int
default_color_from_fa(unsigned char fa)
{
#	define DEFCOLOR_MAP(f) \
		((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))

	return field_colors[DEFCOLOR_MAP(fa)];
}

static int
color_from_fa(unsigned char fa)
{
	if (appres.m3279) {
		int fg;

		fg = default_color_from_fa(fa);
		return get_color_pair(fg, bg_color) |
		    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL);
	} else if (!appres.mono) {
		return get_color_pair(defcolor_offset + COLOR_GREEN,
			bg_color) |
		    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL);
	} else {
	    	/* No color at all. */
		return ((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL;
	}
}

/*
 * Set up the user-specified color mappings.
 */
/*static*/ void
init_user_color(const char *name, int ix)
{
    	char *r;
	int i;
	unsigned long l;
	char *ptr;

	r = get_fresource("%s%s", ResCursesColorForHostColor, name);
	if (r == NULL)
		r = get_fresource("%s%d", ResCursesColorForHostColor, ix);
	if (r == NULL)
	    	return;

	for (i = 0; cc_name[i].name != NULL; i++) {
	    	if (!strcasecmp(r, cc_name[i].name)) {
		    	cmap[ix] = cc_name[i].index;
			return;
		}
	}

	l = strtoul(r, &ptr, 0);
	if (ptr != r && *ptr == '\0' && (int)l < COLORS) {
	    	cmap[ix] = (int)l;
		return;
	}

	xs_warning("Invalid %s value '%s'", ResCursesColorForHostColor, r);
}

static void
init_user_colors(void)
{
	int i;

	for (i = 0; host_color[i].name != NULL; i++) {
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

	if (FA_IS_ZERO(fa)) {
		return color_from_fa(fa);
	}

	/* Compute the color. */

	/*
	 * Monochrome is easy, and so is color if nothing is
	 * specified.
	 */
	if (!appres.m3279 ||
		(!ea_buf[baddr].fg &&
		 !ea_buf[fa_addr].fg &&
		 !ea_buf[baddr].bg &&
		 !ea_buf[fa_addr].bg)) {

	    	a = color_from_fa(fa);

	} else {

		/* The current location or the fa specifies the fg or bg. */

		if (ea_buf[baddr].fg)
			fg = cmap[ea_buf[baddr].fg & 0x0f];
		else if (ea_buf[fa_addr].fg)
			fg = cmap[ea_buf[fa_addr].fg & 0x0f];
		else
			fg = default_color_from_fa(fa);

		if (ea_buf[baddr].bg)
			bg = cmap[ea_buf[baddr].bg & 0x0f];
		else if (ea_buf[fa_addr].bg)
			bg = cmap[ea_buf[fa_addr].bg & 0x0f];
		else
			bg = cmap[HOST_COLOR_NEUTRAL_BLACK];

		a = get_color_pair(fg, bg);

	}

	/* Compute the display attributes. */

	if (ea_buf[baddr].gr)
		gr = ea_buf[baddr].gr;
	else if (ea_buf[fa_addr].gr)
		gr = ea_buf[fa_addr].gr;
	else
		gr = 0;

	if (gr & GR_BLINK)
		a |= A_BLINK;
	if (gr & GR_REVERSE)
		a |= A_REVERSE;
	if (gr & GR_UNDERLINE)
		a |= A_UNDERLINE;
	if ((gr & GR_INTENSIFY) || (ab_mode == TS_ON) || FA_IS_HIGH(fa))
		a |= A_BOLD;

	return a;
}

/* Display what's in the buffer. */
void
screen_disp(Boolean erasing _is_unused)
{
	int row, col;
	int field_attrs;
	unsigned char fa;
	struct screen_spec *cur_spec;
#if defined(X3270_DBCS) /*[*/
	enum dbcs_state d;
#endif /*]*/
	int fa_addr;

	/* This may be called when it isn't time. */
	if (escaped)
		return;

#if defined(C3270_80_132) /*[*/
	/* See if they've switched screens on us. */
	if (def_screen != alt_screen && screen_alt != curses_alt) {
		if (screen_alt) {
			if (write(1, altscreen_spec.mode_switch,
			    strlen(altscreen_spec.mode_switch)) < 0)
			    	exit(1);
			vtrace("Switching to alt (%dx%d) screen.\n",
			    altscreen_spec.rows, altscreen_spec.cols);
			swap_screens(alt_screen);
			cur_spec = &altscreen_spec;
		} else {
			if (write(1, defscreen_spec.mode_switch,
			    strlen(defscreen_spec.mode_switch)) < 0)
			    	exit(1);
			vtrace("Switching to default (%dx%d) screen.\n",
			    defscreen_spec.rows, defscreen_spec.cols);
			swap_screens(def_screen);
			cur_spec = &defscreen_spec;
		}

		/* Figure out where the status line goes now, if it fits. */
		set_status_row(cur_spec->rows, ROWS);

		curses_alt = screen_alt;

		/* Tell curses to forget what may be on the screen already. */
		clear();
	}
#endif /*]*/

	/* If the menubar is separate, draw it first. */
	if (screen_yoffset) {
	    	ucs4_t u = 0;
		Boolean highlight;
		unsigned char acs;
		int norm, high;

		if (menu_is_up) {
			if (appres.m3279) {
				norm = get_color_pair(COLOR_WHITE, COLOR_BLACK);
				high = get_color_pair(COLOR_BLACK, COLOR_WHITE);
			} else {
				norm = defattr & ~A_BOLD;
				high = defattr | A_BOLD;
			}
		} else {
			if (appres.m3279) {
				norm = get_color_pair(COLOR_WHITE, COLOR_BLACK);
				high = get_color_pair(COLOR_WHITE, COLOR_BLACK);
			} else {
				norm = defattr & ~A_BOLD;
				high = defattr & ~A_BOLD;
			}
		}

		for (row = 0; row < screen_yoffset; row++) {
		    	move(row, 0);
			for (col = 0; col < cCOLS; col++) {
				if (menu_char(row, col, True, &u, &highlight,
					    &acs)) {
					char mb[16];

				    	(void) attrset(highlight? high: norm);
#if defined(CURSES_WIDE) /*[*/
					if (u < 0x100 || acs)
						addch(u);
					else if (unicode_to_multibyte(u,
						    mb, sizeof(mb)))
						addstr(mb);
					else
					    	addch(' ');
#else /*][*/
					addch(u);
#endif /*]*/
				} else {
					(void) attrset(norm);
					addch(' ');
				}
			}
		}
	}

	fa = get_field_attribute(0);
	fa_addr = find_field_attribute(0);
	field_attrs = calc_attrs(0, fa_addr, fa);
	for (row = 0; row < ROWS; row++) {
		int baddr;

		if (!flipped)
			move(row + screen_yoffset, 0);
		for (col = 0; col < cCOLS; col++) {
		    	Boolean underlined = False;
			int attr_mask =
			    toggled(UNDERSCORE)? (int)~A_UNDERLINE: -1;
			Boolean is_menu = False;
			ucs4_t u = 0;
			Boolean highlight = False;
			unsigned char acs = 0;

			if (flipped)
				move(row + screen_yoffset, cCOLS-1 - col);

			is_menu = menu_char(row + screen_yoffset,
				flipped? (cCOLS-1 - col): col,
				False,
				&u, &highlight, &acs);
			if (is_menu) {
			    	char mb[16];

				if (!u)
					abort();
				if (appres.m3279) {
					if (highlight)
						(void) attrset(
							get_color_pair(
						    HOST_COLOR_NEUTRAL_BLACK,
						    HOST_COLOR_NEUTRAL_WHITE));
					else
						(void) attrset(
							get_color_pair(
						    HOST_COLOR_NEUTRAL_WHITE,
						    HOST_COLOR_NEUTRAL_BLACK));
				} else {
					if (highlight)
						(void) attrset(defattr |
							       A_BOLD);
					else
						(void) attrset(defattr);
				}
#if defined(CURSES_WIDE) /*[*/
				if (u < 0x100 || acs)
					addch(u);
				else if (unicode_to_multibyte(u,
					    mb, sizeof(mb)))
					addstr(mb);
				else
					addch(' ');
#else /*][*/
				addch(u);
#endif /*]*/
			}

			baddr = row*cCOLS+col;
			if (ea_buf[baddr].fa) {
			    	fa_addr = baddr;
				fa = ea_buf[baddr].fa;
				field_attrs = calc_attrs(baddr, baddr, fa);
				if (!is_menu) {
					(void) attrset(defattr);
					addch(' ');
				}
			} else if (FA_IS_ZERO(fa)) {
			    	if (!is_menu) {
					(void) attrset(field_attrs & attr_mask);
					if (field_attrs & A_UNDERLINE)
					    underlined = True;
					addch(' ');
				}
			} else {
				char mb[16];
				int len;

				if (is_menu)
				    	continue;

				if (!(ea_buf[baddr].gr ||
				      ea_buf[baddr].fg ||
				      ea_buf[baddr].bg)) {

					(void) attrset(field_attrs & attr_mask);
					if (field_attrs & A_UNDERLINE)
					    underlined = True;

				} else {
				    	int buf_attrs;
		
				    	buf_attrs = calc_attrs(baddr, fa_addr,
						fa);
					(void) attrset(buf_attrs & attr_mask);
					if (buf_attrs & A_UNDERLINE)
					    underlined = True;
				}
#if defined(X3270_DBCS) /*[*/
				d = ctlr_dbcs_state(baddr);
				if (IS_LEFT(d)) {
					int xaddr = baddr;

					INC_BA(xaddr);
					len = ebcdic_to_multibyte(
						(ea_buf[baddr].cc << 8) |
						 ea_buf[xaddr].cc,
						mb, sizeof(mb));
					addstr(mb);
				} else if (!IS_RIGHT(d)) {
#endif /*]*/
					if (ea_buf[baddr].cs == CS_LINEDRAW) {
					    	display_linedraw(
							ea_buf[baddr].cc);
					} else if (ea_buf[baddr].cs == CS_APL ||
						   (ea_buf[baddr].cs & CS_GE)) {
						display_ge(ea_buf[baddr].cc);
					} else {
						len = ebcdic_to_multibyte_x(
							ea_buf[baddr].cc,
							CS_BASE, mb,
							sizeof(mb),
							EUO_BLANK_UNDEF |
				     (appres.ascii_box_draw? EUO_ASCII_BOX: 0),
							NULL);
						if (len > 0)
							len--;
						if (toggled(UNDERSCORE) &&
							underlined &&
							(len == 1) &&
							mb[0] == ' ') {
							mb[0] = '_';
						}
						if (toggled(MONOCASE) &&
							(len == 1) &&
							!(mb[0] & 0x80) &&
							islower(mb[0])) {
							mb[0] = toupper(mb[0]);
						}
#if defined(CURSES_WIDE) /*[*/
						addstr(mb);
#else /*][*/
						if (len > 1)
						    	addch(' ');
						else
						    	addch(mb[0] & 0xff);
#endif /*]*/
					}
#if defined(X3270_DBCS) /*[*/
				}
#endif /*]*/
			}
		}
	}
	if (status_row)
		draw_oia();
	(void) attrset(defattr);
	if (menu_is_up) {
		menu_cursor(&row, &col);
		move(row, col);
	} else {
		if (flipped)
			move((cursor_addr / cCOLS + screen_yoffset),
				cCOLS-1 - (cursor_addr % cCOLS));
		else
			move((cursor_addr / cCOLS) + screen_yoffset,
				cursor_addr % cCOLS);
	}
	refresh();
}

/* ESC processing. */
static unsigned long eto = 0L;
static Boolean meta_escape = False;

static void
escape_timeout(ioid_t id _is_unused)
{
	vtrace("Timeout waiting for key following Escape, processing "
	    "separately\n");
	eto = 0L;
	meta_escape = False;
	kybd_input2(0, 0x1b, 0);
}

/* Keyboard input. */
static void
kybd_input(unsigned long fd _is_unused, ioid_t id _is_unused)
{
	int k = 0;		/* KEY_XXX, or 0 */
	ucs4_t ucs4 = 0;	/* input character, or 0 */
	Boolean first = True;
	static Boolean failed_first = False;

	for (;;) {
		volatile int alt = 0;
		char dbuf[128];
#if defined(CURSES_WIDE) /*[*/
		wint_t wch;
		size_t sz;
#endif /*]*/

		if (isendwin())
			return;
		ucs4 = 0;
#if defined(CURSES_WIDE) /*[*/
		k = wget_wch(stdscr, &wch);
#else /*][*/
		k = wgetch(stdscr);
#endif /*]*/
		vtrace("k=%d "
# if defined(CURSES_WIDE) /*[*/
			            "wch=%u "
# endif /*]*/
				              "\n",
			                            k
# if defined(CURSES_WIDE) /*[*/
			                             , wch
# endif /*]*/
			                                  );
		if (k == ERR) {
			if (first) {
				if (failed_first) {
					vtrace("End of File, exiting.\n");
					x3270_exit(1);
				}
				failed_first = True;
			}
			vtrace("k == ERR, return\n");
			return;
		} else {
			failed_first = False;
		}
#if !defined(CURSES_WIDE) /*[*/
		/* Differentiate between KEY_XXX and regular input. */
		if (!(k & ~0xff)) {
			char mb[2];
			int consumed;
			enum me_fail error;

			/* Convert from local multi-byte to Unicode. */
			mb[0] = k;
			mb[1] = '\0';
			ucs4 = multibyte_to_unicode(mb, 1, &consumed, &error);
			if (ucs4 == 0) {
				vtrace("Invalid input char 0x%x\n", k);
				return;
			}
			k = 0;
		}
#endif /*]*/
#if defined(CURSES_WIDE) /*[*/
		if (k == KEY_CODE_YES)
			k = (int)wch;	/* KEY_XXX */
		else {
			char mbs[16];
			wchar_t wcs[2];

			k = 0;
			wcs[0] = wch;
			wcs[1] = 0;
			sz = wcstombs(mbs, wcs, sizeof(mbs));
			if (sz == (size_t)-1) {
				vtrace("Invalid input wchar 0x%x\n", wch);
				return;
			}
			if (sz == 1) {
				ucs4 = mbs[0] & 0xff;
			} else {
			    	int consumed;
				enum me_fail error;

			    	ucs4 = multibyte_to_unicode(mbs, sz, &consumed,
					&error);
				if (ucs4 == 0) {
					vtrace("Unsupported input "
						"wchar %x\n", wch);
					return;
				}
			}
		}
#endif /*]*/

#if defined(NCURSES_MOUSE_VERSION) /*[*/
		if (k == KEY_MOUSE) {
		    	MEVENT m;

			if (menu_is_up) {
			    menu_key(MK_MOUSE, 0);
			    return;
			}
			if (getmouse(&m) != OK)
			    return;
			if ((m.bstate & BUTTON1_RELEASED)) {
			    	vtrace("Mouse BUTTON1_RELEASED "
					"(x=%d,y=%d)\n",
					m.x, m.y);
				if (screen_yoffset != 0 && m.y == 0) {
					popup_menu(m.x, (screen_yoffset != 0));
					screen_disp(False);
				} else if (m.x < cCOLS &&
					   m.y - screen_yoffset >= 0 &&
					   m.y - screen_yoffset < ROWS) {
					if (flipped)
						cursor_move(((m.y - screen_yoffset) * cCOLS) +
							(cCOLS - m.x));
					else
						cursor_move(((m.y - screen_yoffset) * cCOLS) +
							m.x);
					move(m.y + screen_yoffset, m.x);
					refresh();
				}
			}
			return;
		}
#endif /*]*/

		/* Handle Meta-Escapes. */
		if (meta_escape) {
			if (eto != 0L) {
				RemoveTimeOut(eto);
				eto = 0L;
			}
			meta_escape = False;
			alt = KM_ALT;
		} else if (me_mode == TS_ON && ucs4 == 0x1b) {
			vtrace("Key '%s' (curses key 0x%x, char code 0x%x)\n",
				decode_key(k, ucs4, alt, dbuf), k, ucs4);
			eto = AddTimeOut(100L, escape_timeout);
			vtrace(" waiting to see if Escape is followed by"
			    " another key\n");
			meta_escape = True;
			continue;
		}
		vtrace("Key '%s' (curses key 0x%x, char code 0x%x)\n",
			decode_key(k, ucs4, alt, dbuf), k, ucs4);
		kybd_input2(k, ucs4, alt);
		first = False;
	}
}

/* Translate a curses key to a menubar abstract key. */
static menu_key_t
key_to_mkey(int k)
{
    switch (k) {
#if defined(NCURSES_MOUSE_VERSION) /*[*/
    case KEY_MOUSE:
	return MK_MOUSE;
#endif /*]*/
    case KEY_UP:
	return MK_UP;
    case KEY_DOWN:
	return MK_DOWN;
    case KEY_LEFT:
	return MK_LEFT;
    case KEY_RIGHT:
	return MK_RIGHT;
    case KEY_HOME:
	return MK_HOME;
    case KEY_END:
	return MK_END;
    case KEY_ENTER:
	return MK_ENTER;
    case 0:
	return MK_NONE;
    default:
	return MK_OTHER;
    }
}

static void
kybd_input2(int k, ucs4_t ucs4, int alt)
{
	char buf[16];
	char *action;
	int i;

	if (menu_is_up) {
	    menu_key(key_to_mkey(k), ucs4);
	    screen_disp(False);
	    return;
	}

	action = lookup_key(k, ucs4, alt);
	if (action != NULL) {
		if (strcmp(action, "[ignore]"))
			push_keymap_action(action);
		return;
	}
	ia_cause = IA_DEFAULT;

	/* These first cases apply to both 3270 and NVT modes. */
	switch (k) {
	case KEY_UP:
		action_internal(Up_action, IA_DEFAULT, NULL, NULL);
		return;
	case KEY_DOWN:
		action_internal(Down_action, IA_DEFAULT, NULL, NULL);
		return;
	case KEY_LEFT:
		action_internal(Left_action, IA_DEFAULT, NULL, NULL);
		return;
	case KEY_RIGHT:
		action_internal(Right_action, IA_DEFAULT, NULL, NULL);
		return;
	case KEY_HOME:
		action_internal(Home_action, IA_DEFAULT, NULL, NULL);
		return;
	default:
		break;
	}
	switch (ucs4) {
	case 0x1d:
		action_internal(Escape_action, IA_DEFAULT, NULL, NULL);
		return;
	}

	/* Then look for 3270-only cases. */
	if (IN_3270) {
	    	switch(k) {
		case KEY_DC:
			action_internal(Delete_action, IA_DEFAULT, NULL, NULL);
			return;
		case KEY_BACKSPACE:
			action_internal(BackSpace_action, IA_DEFAULT, NULL, NULL);
			return;
		case KEY_HOME:
			action_internal(Home_action, IA_DEFAULT, NULL, NULL);
			return;
		default:
			break;
		}
	    	switch(ucs4) {
		case 0x03:
			action_internal(Clear_action, IA_DEFAULT, NULL, NULL);
			return;
		case 0x12:
			action_internal(Reset_action, IA_DEFAULT, NULL, NULL);
			return;
		case 'L' & 0x1f:
			action_internal(Redraw_action, IA_DEFAULT, NULL, NULL);
			return;
		case '\t':
			action_internal(Tab_action, IA_DEFAULT, NULL, NULL);
			return;
		case 0177:
			action_internal(Delete_action, IA_DEFAULT, NULL, NULL);
			return;
		case '\b':
			action_internal(BackSpace_action, IA_DEFAULT, NULL, NULL);
			return;
		case '\r':
			action_internal(Enter_action, IA_DEFAULT, NULL, NULL);
			return;
		case '\n':
			action_internal(Newline_action, IA_DEFAULT, NULL, NULL);
			return;
		default:
			break;
		}

	}

	/* Do some NVT-only translations. */
	if (IN_NVT) switch (k) {
	case KEY_DC:
	    	ucs4 = 0x7f;
		k = 0;
		break;
	case KEY_BACKSPACE:
		ucs4 = '\b';
		k = 0;
		break;
	}

	/* Catch PF keys. */
	for (i = 1; i <= 24; i++) {
		if (k == KEY_F(i)) {
			(void) sprintf(buf, "%d", i);
			action_internal(PF_action, IA_DEFAULT, buf, NULL);
			return;
		}
	}

	/* Then any other 8-bit ASCII character. */
	if (ucs4) {
		char ks[16];
		String params[2];
		Cardinal one;

		sprintf(ks, "U+%04x", ucs4);
		params[0] = ks;
		params[1] = NULL;
		one = 1;
		Key_action(NULL, NULL, params, &one);
		return;
	}
	vtrace(" dropped (no default)\n");
}

Boolean
screen_suspend(void)
{
	static Boolean need_to_scroll = False;
	Boolean needed = False;

	if (!initscr_done) {
		return False;
	}

	if (!isendwin()) {
#if defined(C3270_80_132) /*[*/
		if (def_screen != alt_screen) {
			/*
			 * Call endwin() for the last-defined screen
			 * (altscreen) first.  Note that this will leave
			 * the curses screen set to defscreen when this
			 * function exits; if the 3270 is really in altscreen
			 * mode, we will have to switch it back when we resume
			 * the screen, below.
			 */
			if (!curses_alt)
				swap_screens(alt_screen);
			endwin();
			swap_screens(def_screen);
			endwin();
		} else {
			endwin();
		}
#else /*][*/
		endwin();
#endif /*]*/
		needed = True;
	}

	if (!escaped) {
		escaped = True;

		if (need_to_scroll)
			printf("\n");
		else
			need_to_scroll = True;
#if defined(C3270_80_132) /*[*/
		if (curses_alt && def_screen != alt_screen) {
			if (write(1, defscreen_spec.mode_switch,
			    strlen(defscreen_spec.mode_switch)) < 0)
			    	x3270_exit(1);
		}
#endif /*]*/
		RemoveInput(input_id);
	}

	return needed;
}

void
screen_resume(void)
{
    	char *cl;

	escaped = False;

	/*
	 * Clear the screen first, if possible, so future command output
	 * starts at the bottom of the screen.
	 */
	if ((cl = tigetstr("clear")) != NULL)
	    	putp(cl);

	/* Finish screen initialization. */
	if (!screen_initted)
	    	finish_screen_init();

#if defined(C3270_80_132) /*[*/
	if (def_screen != alt_screen && curses_alt) {
		/*
		 * When we suspended the screen, we switched to defscreen so
		 * that endwin() got called in the right order.  Switch back.
		 */
		swap_screens(alt_screen);
		if (write(1, altscreen_spec.mode_switch,
		    strlen(altscreen_spec.mode_switch)) < 0)
		    	x3270_exit(1);
	}
#endif /*]*/
	screen_disp(False);
	refresh();
	input_id = AddInput(0, kybd_input);
}

void
cursor_move(int baddr)
{
	cursor_addr = baddr;
}

void
toggle_monocase(struct toggle *t _is_unused, enum toggle_type tt _is_unused)
{
	screen_disp(False);
}

void
toggle_underscore(struct toggle *t _is_unused, enum toggle_type tt _is_unused)
{
	screen_disp(False);
}

/* Status line stuff. */

static Boolean status_ta = False;
static Boolean status_rm = False;
static Boolean status_im = False;
static enum {
    SS_INSECURE,
    SS_UNVERIFIED,
    SS_SECURE
} status_secure = SS_INSECURE;
static Boolean oia_boxsolid = False;
static Boolean oia_undera = True;
static Boolean oia_compose = False;
static Boolean oia_printer = False;
static unsigned char oia_compose_char = 0;
static enum keytype oia_compose_keytype = KT_STD;
#define LUCNT	8
static char oia_lu[LUCNT+1];
static char oia_timing[6]; /* :ss.s*/
static char oia_screentrace = ' ';

static char *status_msg = "X Not Connected";
static char *saved_status_msg = NULL;
static ioid_t saved_status_timeout = NULL_IOID;
static ioid_t oia_scroll_timeout = NULL_IOID;

static void
cancel_status_push(void)
{
    	saved_status_msg = NULL;
	if (saved_status_timeout != NULL_IOID) {
	    	RemoveTimeOut(saved_status_timeout);
		saved_status_timeout = NULL_IOID;
	}
	if (oia_scroll_timeout != NULL_IOID) {
		RemoveTimeOut(oia_scroll_timeout);
		oia_scroll_timeout = NULL_IOID;
	}
}

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

static void
status_pop(ioid_t id _is_unused)
{
    	status_msg = saved_status_msg;
	saved_status_msg = NULL;
	saved_status_timeout = NULL_IOID;
}

static void
oia_scroll(ioid_t id _is_unused)
{
	status_msg++;
	if (strlen(status_msg) > 35)
		oia_scroll_timeout = AddTimeOut(STATUS_SCROLL_MS,
			oia_scroll);
	else {
		saved_status_timeout = AddTimeOut(STATUS_PUSH_MS, status_pop);
		oia_scroll_timeout = NULL_IOID;
	}
}

void
status_push(char *msg)
{
    	if (saved_status_msg != NULL) {
	    	/* Already showing something. */
	    	RemoveTimeOut(saved_status_timeout);
		saved_status_timeout = NULL_IOID;
	} else {
	    	saved_status_msg = status_msg;
	}

	status_msg = msg;

	if (strlen(msg) > 35)
		oia_scroll_timeout = AddTimeOut(STATUS_SCROLL_START_MS,
			oia_scroll);
	else
		saved_status_timeout = AddTimeOut(STATUS_PUSH_MS, status_pop);
}

void
status_minus(void)
{
    	cancel_status_push();
	status_msg = "X -f";
}

void
status_oerr(int error_type)
{
    	cancel_status_push();

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
    	cancel_status_push();

	if (!CONNECTED)
	    	status_msg = "X Not Connected";
	else if (kybdlock & KL_ENTER_INHIBIT)
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
    	cancel_status_push();

	status_msg = "X SYSTEM";
}

void
status_twait(void)
{
    	cancel_status_push();

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
status_half_connect(Boolean half_connected)
{
	if (half_connected) {
		/* Push the 'Connecting' status under whatever is popped up. */
		if (saved_status_msg != NULL)
			saved_status_msg = "X Connecting";
		else
			status_msg = "X Connecting";
		oia_boxsolid = False;
		status_secure = SS_INSECURE;
	}
}

static void
status_connect(Boolean connected)
{
    	cancel_status_push();

	if (connected) {
		oia_boxsolid = IN_3270 && !IN_SSCP;
		if (kybdlock & KL_AWAITING_FIRST)
			status_msg = "X";
		else
			status_msg = "";
#if defined(HAVE_LIBSSL) /*[*/
		if (secure_connection) {
		    	if (secure_unverified)
			    	status_secure = SS_UNVERIFIED;
			else
			    	status_secure = SS_SECURE;
		} else
			status_secure = SS_INSECURE;
#endif /*]*/
	} else {
		oia_boxsolid = False;
		status_msg = "X Not Connected";
		status_secure = SS_INSECURE;
	}       
}

static void
status_3270_mode(Boolean ignored _is_unused)
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

void
status_timing(struct timeval *t0, struct timeval *t1)
{
	static char	no_time[] = ":??.?";

	if (t1->tv_sec - t0->tv_sec > (99*60)) {
	    	strcpy(oia_timing, no_time);
	} else {
		unsigned long cs;	/* centiseconds */

		cs = (t1->tv_sec - t0->tv_sec) * 10 +
		     (t1->tv_usec - t0->tv_usec + 50000) / 100000;
		if (cs < CM)
			(void) sprintf(oia_timing,
				":%02ld.%ld", cs / 10, cs % 10);
		else
			(void) sprintf(oia_timing,
				"%02ld:%02ld", cs / CM, (cs % CM) / 10);
	}
}

void
status_untiming(void)
{
    	oia_timing[0] = '\0';
}

void
status_scrolled(int n)
{
	static char ssbuf[128];

	cancel_status_push();
	if (n) {
		snprintf(ssbuf, sizeof(ssbuf), "X Scrolled %d", n);
		status_msg = ssbuf;
	} else {
	    	status_msg = "";
	}
}

void    
status_screentrace(int n)
{
    if (n < 0) {
	oia_screentrace = ' ';
    } else if (n < 10) {
	oia_screentrace = "0123456789"[n];
    } else {
	oia_screentrace = '+';
    }   
}     

static void
draw_oia(void)
{
	int rmargin;
	static Boolean filled_extra[2] = { False, False };

#if defined(C3270_80_132) /*[*/
	if (def_screen != alt_screen) {
		if (curses_alt)
			rmargin = altscreen_spec.cols - 1;
		else
			rmargin = defscreen_spec.cols - 1;
	} else
#endif /*]*/
	{
		rmargin = maxCOLS - 1;
	}

	/* Black out the parts of the screen we aren't using. */
	if (!appres.mono && !filled_extra[!!curses_alt]) {
	    	int r, c;

		(void) attrset(defattr);
		for (r = 0; r <= status_row; r++) {
		    	int c0;

			if (r >= maxROWS && r != status_row)
			    	c0 = 0;
			else
			    	c0 = maxCOLS;
		    	move(r + screen_yoffset, c0);
		    	for (c = c0; c < cursesCOLS; c++) {
			    	printw(" ");
			}
		}
	}

	/* Make sure the status line region is filled in properly. */
	if (!appres.mono) {
		int i;

		(void) attrset(defattr);
		if (status_skip) {
			move(status_skip + screen_yoffset, 0);
			for (i = 0; i < rmargin; i++) {
				printw(" ");
			}
		}
		move(status_row + screen_yoffset, 0);
		for (i = 0; i < rmargin; i++) {
			printw(" ");
		}
	}

/* The OIA looks like (in Model 2/3/4 mode):

          1         2         3         4         5         6         7
01234567890123456789012345678901234567890123456789012345678901234567890123456789
4AN     Status-Message--------------------- Cn TRIPS  LU-Name-   :ss.s  000/000
         7         6         5         4         3         2         1
98765432109876543210987654321098765432109876543210987654321098765432109876543210

   On wider displays, there is a bigger gap between TRIPS and LU-Name.

*/

	/*
	 * If there is at least one black line between the 3270 display and the
	 * OIA, draw a row of underlined blanks above the OIA. This is
	 * something c3270 can do that wc3270 cannot, since Windows consoles
	 * can't do underlining.
	 */
	if (status_row > screen_yoffset + maxROWS) {
		int i;
		(void) attrset(A_UNDERLINE | defattr);
		move(status_row - 1, 0);
		for (i = 0; i < rmargin; i++) {
			printw(" ");
		}
	}

	(void) attrset(A_REVERSE | defattr);
	mvprintw(status_row, 0, "4");
	(void) attrset(A_UNDERLINE | defattr);
	if (oia_undera)
		printw("%c", IN_E? 'B': 'A');
	else
		printw(" ");
	(void) attrset(A_REVERSE | defattr);
	if (IN_NVT) {
		printw("N");
	} else if (oia_boxsolid) {
		printw(" ");
	} else if (IN_SSCP) {
		printw("S");
	} else {
		printw("?");
	}

	(void) attrset(defattr);
	mvprintw(status_row, 8, "%-35.35s", status_msg);
	mvprintw(status_row, rmargin-35,
	    "%c%c %c%c%c%c",
	    oia_compose? 'C': ' ',
	    oia_compose? oia_compose_char: ' ',
	    status_ta? 'T': ' ',
	    status_rm? 'R': ' ',
	    status_im? 'I': ' ',
	    oia_printer? 'P': ' ');
	if (status_secure != SS_INSECURE) {
	    	if (appres.m3279)
			(void) attrset(get_color_pair(defcolor_offset +
				    ((status_secure == SS_SECURE)?
					COLOR_GREEN: COLOR_YELLOW),
				    bg_color) | A_BOLD);
		else
		    	(void) attrset(A_BOLD);
		printw("S");
	    	(void) attrset(defattr);
	} else
	    	printw(" ");
	printw("%c", oia_screentrace);

	mvprintw(status_row, rmargin-25, "%s", oia_lu);

	if (toggled(SHOW_TIMING))
	    mvprintw(status_row, rmargin-14, "%s", oia_timing);

	if (toggled(CURSOR_POS))
		mvprintw(status_row, rmargin-7,
		    "%03d/%03d ", cursor_addr/cCOLS + 1, cursor_addr%cCOLS + 1);
}

void
Redraw_action(Widget w _is_unused, XEvent *event _is_unused, String *params _is_unused,
    Cardinal *num_params _is_unused)
{
	if (!escaped) {
		endwin();
		refresh();
	}
}

void
ring_bell(void)
{
	beep();
}

void
screen_flip(void)
{
	flipped = !flipped;
	screen_disp(False);
}

#if defined(C3270_80_132) /*[*/
/* Alt/default screen spec parsing. */
static void
parse_screen_spec(const char *str, struct screen_spec *spec)
{
	char msbuf[3];
	char *s, *t, c;
	Boolean escaped = False;

	if (sscanf(str, "%dx%d=%2s", &spec->rows, &spec->cols, msbuf) != 3) {
		(void) fprintf(stderr, "Invalid screen screen spec '%s', must "
		    "be '<rows>x<cols>=<init_string>'\n", str);
		exit(1);
	}
	s = strchr(str, '=') + 1;
	spec->mode_switch = Malloc(strlen(s) + 1);
	t = spec->mode_switch;
	while ((c = *s++)) {
		if (escaped) {
			switch (c) {
			case 'E':
			    *t++ = 0x1b;
			    break;
			case 'n':
			    *t++ = '\n';
			    break;
			case 'r':
			    *t++ = '\r';
			    break;
			case 'b':
			    *t++ = '\b';
			    break;
			case 't':
			    *t++ = '\t';
			    break;
			case '\\':
			    *t++ = '\\';
			    break;
			default:
			    *t++ = c;
			    break;
			}
			escaped = False;
		} else if (c == '\\')
			escaped = True;
		else
			*t++ = c;
	}
	*t = '\0';
}
#endif /*]*/

void
screen_132(void)
{
#if defined(C3270_80_132) /*[*/
	if (cur_screen != alt_screen) {
		swap_screens(alt_screen);
		if (write(1, altscreen_spec.mode_switch,
		    strlen(altscreen_spec.mode_switch)) < 0)
		    	x3270_exit(1);
		ctlr_erase(True);
		screen_disp(True);
	}
#endif /*]*/
}

void
screen_80(void)
{
#if defined(C3270_80_132) /*[*/
	if (cur_screen != def_screen) {
		swap_screens(def_screen);
		if (write(1, defscreen_spec.mode_switch,
		    strlen(defscreen_spec.mode_switch)) < 0)
		    	x3270_exit(1);
		ctlr_erase(False);
		screen_disp(True);
	}
#endif /*]*/
}

/*
 * Translate an x3270 font line-drawing character (the first two rows of a
 * standard X11 fixed-width font) to a curses ACS character.
 *
 * Returns -1 if there is no translation.
 */
static int
linedraw_to_acs(unsigned char c)
{
	switch (c) {
#if defined(ACS_BLOCK) /*[*/
	case 0x0:
		return ACS_BLOCK;
#endif /*]*/
#if defined(ACS_DIAMOND) /*[*/
	case 0x1:
		return ACS_DIAMOND;
#endif /*]*/
#if defined(ACS_CKBOARD) /*[*/
	case 0x2:
		return ACS_CKBOARD;
#endif /*]*/
#if defined(ACS_DEGREE) /*[*/
	case 0x7:
		return ACS_DEGREE;
#endif /*]*/
#if defined(ACS_PLMINUS) /*[*/
	case 0x8:
		return ACS_PLMINUS;
#endif /*]*/
#if defined(ACS_BOARD) /*[*/
	case 0x9:
		return ACS_BOARD;
#endif /*]*/
#if defined(ACS_LANTERN) /*[*/
	case 0xa:
		return ACS_LANTERN;
#endif /*]*/
#if defined(ACS_LRCORNER) /*[*/
	case 0xb:
		return ACS_LRCORNER;
#endif /*]*/
#if defined(ACS_URCORNER) /*[*/
	case 0xc:
		return ACS_URCORNER;
#endif /*]*/
#if defined(ACS_ULCORNER) /*[*/
	case 0xd:
		return ACS_ULCORNER;
#endif /*]*/
#if defined(ACS_LLCORNER) /*[*/
	case 0xe:
		return ACS_LLCORNER;
#endif /*]*/
#if defined(ACS_PLUS) /*[*/
	case 0xf:
		return ACS_PLUS;
#endif /*]*/
#if defined(ACS_S1) /*[*/
	case 0x10:
		return ACS_S1;
#endif /*]*/
#if defined(ACS_S3) /*[*/
	case 0x11:
		return ACS_S3;
#endif /*]*/
#if defined(ACS_HLINE) /*[*/
	case 0x12:
		return ACS_HLINE;
#endif /*]*/
#if defined(ACS_S7) /*[*/
	case 0x13:
		return ACS_S7;
#endif /*]*/
#if defined(ACS_S9) /*[*/
	case 0x14:
		return ACS_S9;
#endif /*]*/
#if defined(ACS_LTEE) /*[*/
	case 0x15:
		return ACS_LTEE;
#endif /*]*/
#if defined(ACS_RTEE) /*[*/
	case 0x16:
		return ACS_RTEE;
#endif /*]*/
#if defined(ACS_BTEE) /*[*/
	case 0x17:
		return ACS_BTEE;
#endif /*]*/
#if defined(ACS_TTEE) /*[*/
	case 0x18:
		return ACS_TTEE;
#endif /*]*/
#if defined(ACS_VLINE) /*[*/
	case 0x19:
		return ACS_VLINE;
#endif /*]*/
#if defined(ACS_LEQUAL) /*[*/
	case 0x1a:
		return ACS_LEQUAL;
#endif /*]*/
#if defined(ACS_GEQUAL) /*[*/
	case 0x1b:
		return ACS_GEQUAL;
#endif /*]*/
#if defined(ACS_PI) /*[*/
	case 0x1c:
		return ACS_PI;
#endif /*]*/
#if defined(ACS_NEQUAL) /*[*/
	case 0x1d:
		return ACS_NEQUAL;
#endif /*]*/
#if defined(ACS_STERLING) /*[*/
	case 0x1e:
		return ACS_STERLING;
#endif /*]*/
#if defined(ACS_BULLET) /*[*/
	case 0x1f:
		return ACS_BULLET;
#endif /*]*/
	default:
		return -1;
	}
}

static void
display_linedraw(unsigned char ebc)
{
    	int c;
	char mb[16];
	int len;

#if defined(CURSES_WIDE) /*[*/
	if (appres.acs)
#endif /*]*/
	{
	    	/* Try UCS first. */
		c = linedraw_to_acs(ebc);
		if (c != -1) {
			addch(c);
			return;
		}
	}

	/* Then try Unicode. */
	len = ebcdic_to_multibyte_x(ebc, CS_LINEDRAW, mb, sizeof(mb),
		EUO_BLANK_UNDEF | (appres.ascii_box_draw? EUO_ASCII_BOX: 0),
		NULL);
	if (len > 0)
		len--;
#if defined(CURSES_WIDE) /*[*/
	addstr(mb);
#else /*][*/
	if (len > 1)
		addch(mb[0] & 0xff);
	else
		addch(' ');
#endif /*]*/
}

static int
apl_to_acs(unsigned char c)
{
	switch (c) {
#if defined(ACS_DEGREE) /*[*/
	case 0xaf: /* CG 0xd1 */
		return ACS_DEGREE;
#endif /*]*/
#if defined(ACS_LRCORNER) /*[*/
	case 0xd4: /* CG 0xac */
		return ACS_LRCORNER;
#endif /*]*/
#if defined(ACS_URCORNER) /*[*/
	case 0xd5: /* CG 0xad */
		return ACS_URCORNER;
#endif /*]*/
#if defined(ACS_ULCORNER) /*[*/
	case 0xc5: /* CG 0xa4 */
		return ACS_ULCORNER;
#endif /*]*/
#if defined(ACS_LLCORNER) /*[*/
	case 0xc4: /* CG 0xa3 */
		return ACS_LLCORNER;
#endif /*]*/
#if defined(ACS_PLUS) /*[*/
	case 0xd3: /* CG 0xab */
		return ACS_PLUS;
#endif /*]*/
#if defined(ACS_HLINE) /*[*/
	case 0xa2: /* CG 0x92 */
		return ACS_HLINE;
#endif /*]*/
#if defined(ACS_LTEE) /*[*/
	case 0xc6: /* CG 0xa5 */
		return ACS_LTEE;
#endif /*]*/
#if defined(ACS_RTEE) /*[*/
	case 0xd6: /* CG 0xae */
		return ACS_RTEE;
#endif /*]*/
#if defined(ACS_BTEE) /*[*/
	case 0xc7: /* CG 0xa6 */
		return ACS_BTEE;
#endif /*]*/
#if defined(ACS_TTEE) /*[*/
	case 0xd7: /* CG 0xaf */
		return ACS_TTEE;
#endif /*]*/
#if defined(ACS_VLINE) /*[*/
	case 0x85: /* CG 0xa84? */
		return ACS_VLINE;
#endif /*]*/
#if defined(ACS_LEQUAL) /*[*/
	case 0x8c: /* CG 0xf7 */
		return ACS_LEQUAL;
#endif /*]*/
#if defined(ACS_GEQUAL) /*[*/
	case 0xae: /* CG 0xd9 */
		return ACS_GEQUAL;
#endif /*]*/
#if defined(ACS_NEQUAL) /*[*/
	case 0xbe: /* CG 0x3e */
		return ACS_NEQUAL;
#endif /*]*/
#if defined(ACS_BULLET) /*[*/
	case 0xa3: /* CG 0x93 */
		return ACS_BULLET;
#endif /*]*/
	case 0xad:
		return '[';
	case 0xbd:
		return ']';
	default:
		return -1;
	}
}

static void
display_ge(unsigned char ebc)
{
    	int c;
	char mb[16];
	int len;

#if defined(CURSES_WIDE) /*[*/
	if (appres.acs)
#endif /*]*/
	{
	    	/* Try UCS first. */
		c = apl_to_acs(ebc);
		if (c != -1) {
			addch(c);
			return;
		}
	}

	/* Then try Unicode. */
	len = ebcdic_to_multibyte_x(ebc, CS_GE, mb, sizeof(mb),
		EUO_BLANK_UNDEF | (appres.ascii_box_draw? EUO_ASCII_BOX: 0),
		NULL);
	if (len > 0)
		len--;
#if defined(CURSES_WIDE) /*[*/
	addstr(mb);
#else /*][*/
	if (len > 1)
		addch(mb[0] & 0xff);
	else
		addch(' ');
#endif /*]*/
}

void
screen_final()
{
    	char *cl;

	if ((cl = tigetstr("clear")) != NULL)
	    	putp(cl);
}
