/*
 * Copyright (c) 2000-2024 Paul Mattes.
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

#include <assert.h>
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"
#include "toggles.h"

#include "actions.h"
#include "codepage.h"
#include "ctlrc.h"
#include "cmenubar.h"
#include "cstatus.h"
#include "glue.h"
#include "host.h"
#include "keymap.h"
#include "kybd.h"
#include "names.h"
#include "popups.h"
#include "screen.h"
#include "see.h"
#include "status.h"
#include "task.h"
#include "telnet.h"
#include "toupper.h"
#include "trace.h"
#include "txa.h"
#include "unicodec.h"
#include "utf8.h"
#include "utils.h"
#include "xio.h"
#include "xscroll.h"

#include "cscreen.h"

/*
 * The usual x3270 COLS variable (current number of columns in the 3270
 * display) is called cCOLS in c3270, to avoid a conflict with curses COLS (the
 * number of columns on the physical termal). For c3270, globals.h #defines
 * COLS as cCOLS, so common code can use COLS transparently -- everywhere but
 * here. In this module, we #undef COLS, after #including globals.h but before
 * #including curses.h, and we use (curses) COLS and (c3270) cCOLS explicitly.
 */
#undef COLS

#if defined(CURSES_WIDE) /*[*/
# include <wchar.h>
# define NCURSES_WIDECHAR 1
#endif /*]*/

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

/* Delay for meta-escape mode. */
#define ME_DELAY	25L

#if !defined(HAVE_TIPARM) /*[*/
#define tiparm		tparm
#endif /*]*/

typedef int curses_color;
typedef int curses_attr;
typedef int host_color_ix;
typedef int color_pair;

static color_pair cp[16][16][2];

static curses_color cmap8[16] = {
    COLOR_BLACK,	/* neutral black */
    COLOR_BLUE,		/* blue */
    COLOR_RED,		/* red */
    COLOR_MAGENTA,	/* pink */
    COLOR_GREEN,	/* green */
    COLOR_CYAN,		/* turquoise */
    COLOR_YELLOW,	/* yellow */
    COLOR_WHITE,	/* neutral white */

    COLOR_BLACK,	/* black */ /* alas, this may be gray */
    COLOR_BLUE,		/* deep blue */
    COLOR_YELLOW,	/* orange */
    COLOR_MAGENTA,	/* purple */
    COLOR_GREEN,	/* pale green */
    COLOR_CYAN,		/* pale turquoise */
    COLOR_BLACK,	/* gray */
    COLOR_WHITE		/* white */
};

static curses_color cmap8_rv[16] = {
    COLOR_WHITE,	/* neutral black (reversed) */
    COLOR_BLUE,		/* blue */
    COLOR_RED,		/* red */
    COLOR_MAGENTA,	/* pink */
    COLOR_GREEN,	/* green */
    COLOR_CYAN,		/* turquoise */
    COLOR_YELLOW,	/* yellow */
    COLOR_BLACK,	/* neutral white (reversed) */

    COLOR_BLACK,	/* black */ /* alas, this may be gray */
    COLOR_BLUE,		/* deep blue */
    COLOR_YELLOW,	/* orange */
    COLOR_MAGENTA,	/* purple */
    COLOR_GREEN,	/* pale green */
    COLOR_CYAN,		/* pale turquoise */
    COLOR_BLACK,	/* gray */
    COLOR_WHITE		/* white */
};

static curses_color cmap16[16] = {
    COLOR_BLACK,	/* neutral black */
    8 + COLOR_BLUE,	/* blue */
    COLOR_RED,		/* red */
    8 + COLOR_MAGENTA,	/* pink */
    8 + COLOR_GREEN,	/* green */
    8 + COLOR_CYAN,	/* turquoise */
    8 + COLOR_YELLOW,	/* yellow */
    8 + COLOR_WHITE,	/* neutral white */

    COLOR_BLACK,	/* black */ /* alas, this may be gray */
    COLOR_BLUE,		/* deep blue */
    8 + COLOR_RED,	/* orange */
    COLOR_MAGENTA,	/* purple */
    COLOR_GREEN,	/* pale green */
    COLOR_CYAN,		/* pale turquoise */
    COLOR_WHITE,	/* gray */
    8 + COLOR_WHITE	/* white */
};

static curses_color cmap16_rv[16] = {
    8 + COLOR_WHITE,	/* neutral black (reversed) */
    COLOR_BLUE,		/* blue */
    COLOR_RED,		/* red */
    8 + COLOR_MAGENTA,	/* pink */
    COLOR_GREEN,	/* green */
    COLOR_CYAN,		/* turquoise */
    COLOR_YELLOW,	/* yellow */
    COLOR_BLACK,	/* neutral white (reversed) */

    COLOR_BLACK,	/* black */ /* alas, this may be gray */
    COLOR_BLUE,		/* deep blue */
    8 + COLOR_RED,	/* orange */
    COLOR_MAGENTA,	/* purple */
    8 + COLOR_GREEN,	/* pale green */
    8 + COLOR_CYAN,	/* pale turquoise */
    8 + COLOR_WHITE,	/* gray */
    8 + COLOR_WHITE	/* white */
};

static curses_color *cmap = cmap8;
static curses_attr cattrmap[16] = {
    A_NORMAL, A_NORMAL, A_NORMAL, A_NORMAL,
    A_NORMAL, A_NORMAL, A_NORMAL, A_NORMAL,
    A_NORMAL, A_NORMAL, A_NORMAL, A_NORMAL,
    A_NORMAL, A_NORMAL, A_NORMAL, A_NORMAL
};
static int defcolor_offset = 0;

static curses_color field_colors8[4] = {
    COLOR_GREEN,	/* default */
    COLOR_RED,		/* intensified */
    COLOR_BLUE,		/* protected */
    COLOR_WHITE		/* protected, intensified */
};

static curses_color field_colors8_rv[4] = {
    COLOR_GREEN,	/* default */
    COLOR_RED,		/* intensified */
    COLOR_BLUE,		/* protected */
    COLOR_BLACK		/* protected, intensified */
};

static curses_color field_colors16[4] = {
    8 + COLOR_GREEN,	/* default */
    COLOR_RED,		/* intensified */
    8 + COLOR_BLUE,	/* protected */
    8 + COLOR_WHITE	/* protected, intensified */
};

static curses_color field_colors16_rv[4] = {
    COLOR_GREEN,	/* default */
    COLOR_RED,		/* intensified */
    COLOR_BLUE,		/* protected */
    COLOR_BLACK		/* protected, intensified */
};

static curses_color *field_colors = field_colors8;

static curses_attr field_cattrmap[4] = {
    A_NORMAL, A_NORMAL, A_NORMAL, A_NORMAL
};

static curses_color bg_color = COLOR_BLACK;

static curses_attr defattr = A_NORMAL;
static curses_attr xhattr = A_NORMAL;
static ioid_t input_id = NULL_IOID;

static int rmargin;

bool screen_initted = false;
bool escaped = true;
bool initscr_done = false;
int curs_set_state = -1;

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
    curses_color index;
} cc_name[] = {
    { "black",			COLOR_BLACK },
    { "red",			COLOR_RED },
    { "green",			COLOR_GREEN },
    { "yellow",			COLOR_YELLOW },
    { "blue",			COLOR_BLUE },
    { "magenta",    		COLOR_MAGENTA },
    { "cyan",			COLOR_CYAN },
    { "white",			COLOR_WHITE },
    { "intensified-black",	8 + COLOR_BLACK },
    { "intensified-red",	8 + COLOR_RED },
    { "intensified-green",	8 + COLOR_GREEN },
    { "intensified-yellow",	8 + COLOR_YELLOW },
    { "intensified-blue",	8 + COLOR_BLUE },
    { "intensified-magenta",    8 + COLOR_MAGENTA },
    { "intensified-cyan",	8 + COLOR_CYAN },
    { "intensified-white",	8 + COLOR_WHITE },
    { NULL,			0 }
};

static int status_row = 0;	/* Row to display the status line on */
static int status_skip = 0;	/* Row to blank above the status line */
static int screen_yoffset = 0;	/* Vertical offset to top of screen.
				   If 0, there is no menu bar.
				   If nonzero (2, actually), menu bar is at the
				    top of the display. */

static host_color_ix crosshair_color = HOST_COLOR_PURPLE;
static bool curses_alt = false;
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
static bool default_colors = false;
#endif /*]*/

static ioid_t disabled_done_id = NULL_IOID;

/* Layered OIA messages. */
static char *disabled_msg = NULL;	/* layer 0 (top) */
static char *scrolled_msg = NULL;	/* layer 1 */
static char *info_msg = NULL;		/* layer 2 */
static char *other_msg = NULL;		/* layer 3 */
static curses_attr other_attr;		/* layer 3 color */

static char *info_base_msg = NULL;	/* original info message (unscrolled) */

/* Terminfo state. */
static struct {
    int colors;		/* number of colors */
    char *op;		/* original pair (restore color) */
    char *setaf;	/* set foreground */
    char *sgr;		/* set graphic rendition */
    char *sgr0;		/* reset graphic rendition */
    bool bold;		/* use bold SGR for certain colors */
} ti;

static void kybd_input(iosrc_t fd, ioid_t id);
static void kybd_input2(int k, ucs4_t ucs4, int alt);
static void draw_oia(void);
static void status_connect(bool ignored);
static void status_3270_mode(bool ignored);
static void status_printer(bool on);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char);
static void set_status_row(int screen_rows, int emulator_rows);
static void display_linedraw(ucs4_t ucs);
static void display_ge(unsigned char ebc);
static void init_user_colors(void);
static void init_user_attribute_colors(void);
static void screen_init2(void);

static action_t Redraw_action;

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

/* Initialize the screen. */
void
screen_init(void)
{
    setupterm(NULL, fileno(stdout), NULL);

    menu_init();

#if defined(C3270_80_132) /*[*/
    /* Parse altscreen/defscreen. */
    if ((appres.c3270.altscreen != NULL) ^
	(appres.c3270.defscreen != NULL)) {
	fprintf(stderr, "Must specify both altscreen and defscreen\n");
	exit(1);
    }
    if (appres.c3270.altscreen != NULL) {
	parse_screen_spec(appres.c3270.altscreen, &altscreen_spec);
	if (altscreen_spec.rows < 27 || altscreen_spec.cols < 132) {
	    fprintf(stderr, "Rows and/or cols too small on "
		"alternate screen (minimum 27x132)\n");
	    exit(1);
	}
	parse_screen_spec(appres.c3270.defscreen, &defscreen_spec);
	if (defscreen_spec.rows < 24 || defscreen_spec.cols < 80) {
	    fprintf(stderr, "Rows and/or cols too small on "
		"default screen (minimum 24x80)\n");
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
    if (!ts_value(appres.c3270.meta_escape, &me_mode))
	popup_an_error("Invalid %s value: '%s', assuming 'auto'\n",
		ResMetaEscape, appres.c3270.meta_escape);
    if (me_mode == TS_AUTO) {
	me_mode = TS_ON;
    }

    /*
     * If they don't want ACS and they're not in a UTF-8 locale, switch
     * to ASCII-art mode for box drawing.
     */
    if (
#if defined(CURSES_WIDE) /*[*/
	!appres.c3270.acs &&
#endif /*]*/
			     !is_utf8) {
	appres.c3270.ascii_box_draw = true;
    }

    /* Initialize the controller. */
    ctlr_init(ALL_CHANGE);
}

/*
 * Find and save a terminfo string.
 */
static char *
ti_save(const char *name)
{
    char *str = tigetstr((char *)name);

    if (str != NULL && str != (char *)-1) {
	return NewString(str);
    }
    return NULL;
}

/*
 * Returns true if the screen supports ANSI color sequences.
 */
bool
screen_has_ansi_color(void)
{
    /* Check for disqualifying conditions. */
    if (appres.interactive.mono ||
        ((ti.colors = tigetnum("colors")) < 8) ||
        ((ti.setaf = ti_save("setaf")) == NULL) ||
        ((ti.op = ti_save("op")) == NULL)) {
	return false;
    }

    /* Save the other strings, which are optional. */
    ti.sgr = ti_save("sgr");
    ti.sgr0 = ti_save("sgr0");

    /* Figure out bold mode. */
    if (ti.sgr != NULL && ti.sgr0 != NULL) {
	if (appres.c3270.all_bold_on) {
	    ti.bold = true;
	} else {
	    enum ts ab;

	    if (!ts_value(appres.c3270.all_bold, &ab)) {
		ab = TS_AUTO;
	    }
	    if (ab == TS_AUTO) {
		ti.bold = ti.colors < 16;
	    } else {
		ti.bold = (ab == TS_ON);
	    }
	}
    }

    /* Recompute 'op'. */
    if (ti.op != NULL && ti.sgr0 != NULL) {
	char *s = Asprintf("%s%s", ti.op, ti.sgr0);

	Replace(ti.op, s);
    }

    return true;
}

/*
 * Returns the "op" (original pair) string.
 */
const char *
screen_op(void)
{
    return ti.op;
}

/*
 * Returns the sequence to set a foreground color.
 */
const char *
screen_setaf(acolor_t color)
{
    static int color_map8[] = { COLOR_BLUE, COLOR_RED, COLOR_YELLOW };
    static int color_map16[] = { 8 + COLOR_BLUE, COLOR_RED, 8 + COLOR_YELLOW };
    char *setaf;

    setaf = tiparm(ti.setaf,
	    (ti.colors >= 16)? color_map16[color]: color_map8[color]);
    setaf = txdFree(NewString(setaf));
    if (ti.bold && color_map16[color] >= 8) {
	char *sgr = tiparm(ti.sgr, 0, 0, 0, 0, 0, 1, 0, 0, 0);

	return txAsprintf("%s%s", sgr, setaf);
    } else {
	return setaf;
    }
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
    bool oversize = false;
    char *cl;

    /* Dummy globals so Valgrind doesn't think the arguments to putenv() are leaked. */
    static char *pe_columns = NULL;
    static char *pe_lines = NULL;
    static char *pe_escdelay = NULL;

    if (screen_initted) {
	return;
    }

    screen_initted = true;

    /* Clear the (original) screen first. */
#if defined(C3270_80_132) /*[*/
    if (appres.c3270.defscreen != NULL) {
	putenv((pe_columns = Asprintf("COLUMNS=%d", defscreen_spec.cols)));
	putenv((pe_lines = Asprintf("LINES=%d", defscreen_spec.rows)));
    }
#endif /*]*/
    if ((cl = tigetstr("clear")) != NULL) {
	putp(cl);
    }

    if (getenv("ESCDELAY") == NULL) {
	putenv((pe_escdelay = Asprintf("ESCDELAY=%ld", ME_DELAY)));
    }

#if !defined(C3270_80_132) /*[*/
    /* Initialize curses. */
    if (initscr() == NULL) {
	fprintf(stderr, "Can't initialize terminal.\n");
	exit(1);
    }
    initscr_done = true;
#else /*][*/
    /* Set up ncurses, and see if it's within bounds. */
    if (appres.c3270.defscreen != NULL) {
	putenv(Asprintf("COLUMNS=%d", defscreen_spec.cols));
	putenv(Asprintf("LINES=%d", defscreen_spec.rows));
	def_screen = newterm(NULL, stdout, stdin);
	initscr_done = true;
	if (def_screen == NULL) {
	    fprintf(stderr, "Can't initialize %dx%d defscreen terminal.\n",
		    defscreen_spec.rows, defscreen_spec.cols);
	    exit(1);
	}
	if (write(1, defscreen_spec.mode_switch,
		    strlen(defscreen_spec.mode_switch)) < 0) {
	    endwin();
	    exit(1);
	}
    }
    if (appres.c3270.altscreen) {
	putenv(Asprintf("COLUMNS=%d", altscreen_spec.cols));
	putenv(Asprintf("LINES=%d", altscreen_spec.rows));
    }
    alt_screen = newterm(NULL, stdout, stdin);
    if (alt_screen == NULL) {
	popup_an_error("Can't initialize terminal.\n");
	exit(1);
    }
    initscr_done = true;
    if (def_screen == NULL) {
	def_screen = alt_screen;
	cur_screen = def_screen;
    }
    if (appres.c3270.altscreen) {
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
	    oversize = true;
	    continue;
	}

	/* If we're at the smallest screen now, give up. */
	if (model_num == 2) {
	    popup_an_error("Emulator won't fit on a %dx%d display.\n",
		    cursesLINES, cursesCOLS);
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
	if (want_ov_rows > cursesLINES - 2) {
	    want_ov_rows = cursesLINES - 2;
	}
	if (want_ov_rows < maxROWS) {
	    want_ov_rows = maxROWS;
	}
	if (want_ov_cols > cursesCOLS) {
	    want_ov_cols = cursesCOLS;
	}
	set_rows_cols(model_num, want_ov_cols, want_ov_rows);
    }

    /*
     * Finally, if they want automatic oversize, see if that's possible.
     */
    if (ov_auto && (maxROWS < cursesLINES - 3 || maxCOLS < cursesCOLS)) {
	set_rows_cols(model_num, cursesCOLS, cursesLINES - 3);
    }

#if defined(NCURSES_MOUSE_VERSION) /*[*/
    if (appres.c3270.mouse && mousemask(BUTTON1_RELEASED, NULL) == 0) {
	appres.c3270.mouse = false;
    }
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

    /* Implement reverse video. */
    if (appres.c3270.reverse_video) {
	bg_color = COLOR_WHITE;
    }

    /* Play with curses color. */
    if (!appres.interactive.mono) {
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
	char *colorterm;
#endif /*]*/
	start_color();
	if (has_colors() && COLORS >= 16) {
	    cmap = appres.c3270.reverse_video? cmap16_rv: cmap16;
	    field_colors = appres.c3270.reverse_video? field_colors16_rv:
		field_colors16;
	    if (appres.c3270.reverse_video) {
		bg_color += 8;
	    } else {
		defcolor_offset = 8;
	    }
	} else if (appres.c3270.reverse_video) {
	    cmap = cmap8_rv;
	    field_colors = field_colors8_rv;
	}

	init_user_colors();
	init_user_attribute_colors();
	crosshair_color_init();

	/* See about all-bold behavior. */
	if (appres.c3270.all_bold_on) {
	    ab_mode = TS_ON;
	} else if (!ts_value(appres.c3270.all_bold, &ab_mode)) {
	    popup_an_error("Invalid %s value: '%s', assuming 'auto'\n",
		    ResAllBold, appres.c3270.all_bold);
	}
	if (ab_mode == TS_AUTO) {
	    ab_mode = (mode3279 && (COLORS < 16) &&
		    !appres.c3270.reverse_video)? TS_ON: TS_OFF;
	}
	if (ab_mode == TS_ON) {
	    int i;

	    defattr |= A_BOLD;
	    for (i = 0; i < 4; i++) {
		field_cattrmap[i] = A_BOLD;
	    }
	}

#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
	if ((appres.c3270.default_fgbg ||
	     (((colorterm = getenv("COLORTERM")) != NULL &&
	      !strcmp(colorterm, "gnome-terminal")) ||
	      getenv("VTE_VERSION") != NULL)) &&
	    use_default_colors() != ERR) {

	    default_colors = true;
	}
#endif /*]*/
	if (has_colors() && COLORS >= 8) {
	    if (mode3279) {
		/* Use 'protected' attributes for the OIA. */
		defattr = get_color_pair(field_colors[2], bg_color) |
		    field_cattrmap[2];
		xhattr = get_color_pair(defcolor_offset + cmap[crosshair_color],
			bg_color) | cattrmap[crosshair_color];
	    } else {
		defattr = get_color_pair(defcolor_offset + COLOR_GREEN,
			bg_color);
		xhattr = get_color_pair(defcolor_offset + COLOR_GREEN,
			bg_color);
	    }
#if defined(C3270_80_132) && defined(NCURSES_VERSION)  /*[*/
	    if (def_screen != alt_screen) {
		SCREEN *s = cur_screen;

		/* Initialize the colors for the other screen. */
		if (s == def_screen) {
		    set_term(alt_screen);
		} else {
		    set_term(def_screen);
		}
		start_color();
		curses_alt = !curses_alt;
		get_color_pair(field_colors[2], bg_color);
		curses_alt = !curses_alt;
		set_term(s);

	    }
#endif /*]*/
	} else {
	    appres.interactive.mono = true;
	    mode3279 = false;
	    /* Get the terminal name right. */
	    set_rows_cols(model_num, want_ov_cols, want_ov_rows);
	}
    }

    /* Set up the scrollbar. */
    scroll_buf_init();

    screen_init2();
}

/* Configure the TTY settings for a curses screen. */
static void
setup_tty(void)
{
    extern void pause_for_errors(void);

    if (appres.c3270.cbreak_mode) {
	cbreak();
    } else {
	raw();
    }
    noecho();
    nonl();
    intrflush(stdscr,FALSE);
    if (appres.c3270.curses_keypad) {
	keypad(stdscr, TRUE);
    }
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
    escaped = false;

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
	if (appres.c3270.mouse) {
	    mousemask(BUTTON1_RELEASED, NULL);
	}
#endif /*]*/
    }
#endif /*]*/

    /* Subscribe to input events. */
    if (input_id == NULL_IOID) {
	input_id = AddInput(0, kybd_input);
    }

    /* Ignore SIGINT and SIGTSTP. */
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

#if defined(C3270_80_132) /*[*/
    /* Ignore SIGWINCH -- it might happen when we do 80/132 changes. */
    if (def_screen != alt_screen) {
	signal(SIGWINCH, SIG_IGN);
    }
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
    if (appres.interactive.menubar && appres.c3270.mouse) {
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
static curses_attr
get_color_pair(curses_color fg, curses_color bg)
{
    static int next_pair[2] = { 1, 1 };
    color_pair pair;
#if defined(C3270_80_132) && defined(NCURSES_VERSION) /*[*/
	    /* ncurses allocates colors for each screen. */
    int pair_index = !!curses_alt;
#else /*][*/
	    /* curses allocates colors globally. */
    const int pair_index = 0;
#endif /*]*/
    curses_color bg_arg = bg;
    curses_color fg_arg = fg;

    if ((pair = cp[fg][bg][pair_index])) {
	return COLOR_PAIR(pair);
    }
    if (next_pair[pair_index] >= COLOR_PAIRS) {
	return 0;
    }
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
    /*
     * Assume that by default, the terminal displays some sort of 'white'
     * against some sort of 'black', and that looks better than the
     * explicit curses COLOR_WHITE over COLOR_BLACK.
     */
    if (default_colors) {
	if (bg == COLOR_BLACK) {
	    bg_arg = -1; /* use the default background, not black */
	}
	if (fg == COLOR_WHITE) {
	    fg_arg = -1; /* use the default foreground, not white */
	}
    }
#endif /*]*/
    if (init_pair(next_pair[pair_index], fg_arg, bg_arg) != OK) {
	return 0;
    }
    pair = cp[fg][bg][pair_index] = next_pair[pair_index]++;
    return COLOR_PAIR(pair);
}

/*
 * Initialize the user-specified attribute color mappings.
 */
static void
init_user_attribute_color(curses_color *color, curses_attr *attr,
	const char *resname)
{
    char *r;
    unsigned long l;
    char *ptr;
    int i;

    if ((r = get_resource(resname)) == NULL) {
	return;
    }
    for (i = 0; cc_name[i].name != NULL; i++) {
	if (!strcasecmp(r, cc_name[i].name)) {
	    if (cc_name[i].index < COLORS) {
		*color = cc_name[i].index;
	    } else {
		*color = cc_name[i].index - 8;
		*attr = A_BOLD;
	    }
	    return;
	}
    }
    l = strtoul(r, &ptr, 0);
    if (ptr == r || *ptr != '\0') {
	xs_warning("Invalid %s value: %s", resname, r);
	return;
    }
    if ((int)l >= COLORS) {
	if (l < 16 && COLORS == 8) {
	    *color = (int)l;
	    *attr = A_BOLD;
	} else {
	    xs_warning("Invalid %s value %s exceeds maximum color index %d",
		    resname, r, COLORS - 1);
	    return;
	}
    }
    *color = (int)l;
}

static void
init_user_attribute_colors(void)
{
    init_user_attribute_color(&field_colors[0], &field_cattrmap[0],
	    ResCursesColorForDefault);
    init_user_attribute_color(&field_colors[1], &field_cattrmap[0],
	    ResCursesColorForIntensified);
    init_user_attribute_color(&field_colors[2], &field_cattrmap[2],
	    ResCursesColorForProtected);
    init_user_attribute_color(&field_colors[3], &field_cattrmap[3],
	    ResCursesColorForProtectedIntensified);
}

/*
 * Map a field attribute to a curses color index.
 * Applies only to 3279 mode -- does not work for mono.
 */
#define DEFCOLOR_MAP(f) \
	((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))
static curses_color
default_color_from_fa(unsigned char fa)
{
    return field_colors[DEFCOLOR_MAP(fa)];
}

static int
attrmap_from_fa(unsigned char fa)
{
    return DEFCOLOR_MAP(fa);
}

static curses_attr
color_from_fa(unsigned char fa)
{
    if (mode3279) {
	int ai = attrmap_from_fa(fa);
	curses_color fg = default_color_from_fa(fa);

	return get_color_pair(fg, bg_color) |
	    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL) |
	    field_cattrmap[ai];
    } else if (!appres.interactive.mono) {
	return get_color_pair(defcolor_offset + COLOR_GREEN, bg_color) |
	    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL);
    } else {
	/* No color at all. */
	return ((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL;
    }
}

/*
 * Set up the user-specified color mappings.
 */
void
init_user_color(const char *name, host_color_ix ix)
{
    char *r;
    int i;
    unsigned long l;
    curses_color il;
    char *ptr;

    r = get_fresource("%s%s", ResCursesColorForHostColor, name);
    if (r == NULL) {
	r = get_fresource("%s%d", ResCursesColorForHostColor, ix);
    }
    if (r == NULL) {
	return;
    }

    for (i = 0; cc_name[i].name != NULL; i++) {
	if (!strcasecmp(r, cc_name[i].name)) {
	    cmap[ix] = cc_name[i].index;
	    if (COLORS < 16 && cmap[ix] > 8) {
		/*
		 * When there are only 8 colors, the intensified colors are
		 * mapped to bold.
		 */
		cmap[ix] -= 8;
		cattrmap[ix] = A_BOLD;
	    }
	    return;
	}
    }

    l = strtoul(r, &ptr, 0);
    if (ptr == r || *ptr == '\0') {
	xs_warning("Invalid %s value '%s'", ResCursesColorForHostColor, r);
	return;
    }
    il = (curses_color)l;
    if (COLORS < 16 && il > 8 && il <= 16) {
	/* Map 9..15 to 0..8 + bold as above. */
	cmap[ix] = il - 8;
	cattrmap[ix] = A_BOLD;
	return;
    }
    if (il < COLORS) {
	cmap[ix] = il;
	return;
    }

    xs_warning("Out of range %s value '%s'", ResCursesColorForHostColor, r);
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
static curses_attr
calc_attrs(int baddr, int fa_addr, int fa)
{
    curses_color fg, bg;
    int gr;
    curses_attr a;

    if (FA_IS_ZERO(fa)) {
	return color_from_fa(fa);
    }

    /* Compute the color. */

    /*
     * Monochrome is easy, and so is color if nothing is
     * specified.
     */
    if (!mode3279 ||
	    (!ea_buf[baddr].fg &&
	     !ea_buf[fa_addr].fg &&
	     !ea_buf[baddr].bg &&
	     !ea_buf[fa_addr].bg)) {

	a = color_from_fa(fa);

    } else {
	host_color_ix ix;
	curses_attr attr;

	/* The current location or the fa specifies the fg or bg. */
	if (ea_buf[baddr].fg) {
	    ix = ea_buf[baddr].fg & 0x0f;
	    fg = cmap[ix];
	    attr = cattrmap[ix];
	} else if (ea_buf[fa_addr].fg) {
	    ix = ea_buf[fa_addr].fg & 0x0f;
	    fg = cmap[ix];
	    attr = cattrmap[ix];
	} else {
	    ix = attrmap_from_fa(fa);
	    fg = default_color_from_fa(fa);
	    attr = field_cattrmap[attrmap_from_fa(fa)];
	}

	if (ea_buf[baddr].bg) {
	    bg = cmap[ea_buf[baddr].bg & 0x0f];
	} else if (ea_buf[fa_addr].bg) {
	    bg = cmap[ea_buf[fa_addr].bg & 0x0f];
	} else {
	    bg = cmap[HOST_COLOR_NEUTRAL_BLACK];
	}

	a = get_color_pair(fg, bg) | attr;
    }

    /* Compute the display attributes. */
    if (ea_buf[baddr].gr) {
	gr = ea_buf[baddr].gr;
    } else if (ea_buf[fa_addr].gr) {
	gr = ea_buf[fa_addr].gr;
    } else {
	gr = 0;
    }

    if (gr & GR_BLINK) {
	a |= A_BLINK;
    }
    if (gr & GR_REVERSE) {
	a |= A_REVERSE;
    }
    if (gr & GR_UNDERLINE) {
	a |= A_UNDERLINE;
    }
    if ((gr & GR_INTENSIFY) || (ab_mode == TS_ON) || FA_IS_HIGH(fa)) {
	a |= A_BOLD;
    }

    return a;
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

/**
 * Return a space or a line-drawing character, depending on whether the
 * given buffer address has a crosshair cursor on it.
 *
 * @param[in] baddr		Buffer address
 * @param[out] acs		Returned true if the returned character is a
 * 				curses ACS code
 *
 * @return Blank if not a crosshair region, possibly an ACS code (if acs
 *         returned true), possibly an ASCII-art character (if asciiBoxDraw is
 *         set), possibly a Unicode line-drawing character.
 */
static ucs4_t
crosshair_blank(int baddr, unsigned char *acs)
{
    ucs4_t u = ' ';

    *acs = 0;
    if (toggled(CROSSHAIR)) {
	bool same_row = ((baddr / cCOLS) == (cursor_addr / cCOLS));
	bool same_col = ((baddr % cCOLS) == (cursor_addr % cCOLS));

	if (same_row && same_col) {
	    map_acs('n', &u, acs); /* cross */
	} else if (same_row) {
	    map_acs('q', &u, acs); /* horizontal */
	} else if (same_col) {
	    map_acs('x', &u, acs); /* vertical */
	}
    }
    return u;
}

/**
 * Draw a crosshair line-drawing character returned by crosshair_blank().
 *
 * @param[in] u		Line-drawing character
 * @param[in] acs	true if u is a curses ACS code
 */
static void
draw_crosshair(ucs4_t u, bool acs)
{
    char mb[16];

    attrset(xhattr);
#if defined(CURSES_WIDE) /*[*/
    if (u < 0x100 || acs) {
	addch(u);
    } else if (unicode_to_multibyte(u, mb, sizeof(mb))) {
	addstr(mb);
    } else {
	addch(' ');
    }
#else /*][*/
    addch(u);
#endif /*]*/
}

/* Display what's in the buffer. */
void
screen_disp(bool erasing _is_unused)
{
    int row, col;
    curses_attr field_attrs;
    unsigned char fa;
    struct screen_spec *cur_spec;
    enum dbcs_state d;
    int fa_addr;
    char mb[16];

    /* This may be called when it isn't time. */
    if (escaped) {
	return;
    }

#if defined(C3270_80_132) /*[*/
    /* See if they've switched screens on us. */
    if (def_screen != alt_screen && screen_alt != curses_alt) {
	if (screen_alt) {
	    if (write(1, altscreen_spec.mode_switch,
			strlen(altscreen_spec.mode_switch)) < 0) {
		exit(1);
	    }
	    vtrace("Switching to alt (%dx%d) screen.\n",
		    altscreen_spec.rows, altscreen_spec.cols);
	    swap_screens(alt_screen);
	    cur_spec = &altscreen_spec;
	} else {
	    if (write(1, defscreen_spec.mode_switch,
			strlen(defscreen_spec.mode_switch)) < 0) {
		exit(1);
	    }
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
	bool highlight;
	unsigned char acs;
	curses_attr norm, high;

	if (menu_is_up) {
	    if (mode3279) {
		norm = get_color_pair(COLOR_WHITE, COLOR_BLACK);
		high = get_color_pair(COLOR_BLACK, COLOR_WHITE);
	    } else {
		norm = defattr & ~A_BOLD;
		high = defattr | A_BOLD;
	    }
	} else {
	    if (mode3279) {
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
		if (menu_char(row, col, true, &u, &highlight, &acs)) {
		    attrset(highlight? high: norm);
#if defined(CURSES_WIDE) /*[*/
		    if (u < 0x100 || acs) {
			addch(u);
		    } else if (unicode_to_multibyte(u, mb, sizeof(mb))) {
			addstr(mb);
		    } else {
			addch(' ');
		    }
#else /*][*/
		    addch(u);
#endif /*]*/
		} else {
		    attrset(norm);
		    addch(' ');
		}
	    }
	}
    }

    fa = get_field_attribute(0);
    fa_addr = find_field_attribute(0);
    field_attrs = calc_attrs(fa_addr, fa_addr, fa);
    for (row = 0; row < ROWS; row++) {
	int baddr;

	if (!flipped) {
	    move(row + screen_yoffset, 0);
	}
	for (col = 0; col < cCOLS; col++) {
	    bool underlined = false;
	    curses_attr attr_mask = toggled(UNDERSCORE)? (int)~A_UNDERLINE: -1;
	    bool is_menu = false;
	    ucs4_t u = 0;
	    bool highlight = false;
	    unsigned char acs = 0;

	    if (flipped) {
		move(row + screen_yoffset, cCOLS-1 - col);
	    }

	    is_menu = menu_char(row + screen_yoffset,
		    flipped? (cCOLS-1 - col): col,
		    false,
		    &u, &highlight, &acs);
	    if (is_menu) {

		if (!u) {
		    abort();
		}
		if (mode3279) {
		    if (highlight) {
			attrset(get_color_pair(HOST_COLOR_NEUTRAL_BLACK,
				    HOST_COLOR_NEUTRAL_WHITE));
		    } else {
			attrset(get_color_pair(HOST_COLOR_NEUTRAL_WHITE,
				    HOST_COLOR_NEUTRAL_BLACK));
		    }
		} else {
		    if (highlight) {
			attrset(defattr | A_BOLD);
		    } else {
			attrset(defattr);
		    }
		}
#if defined(CURSES_WIDE) /*[*/
		if (u < 0x100 || acs) {
		    addch(u);
		} else if (unicode_to_multibyte(u, mb, sizeof(mb))) {
		    addstr(mb);
		} else {
		    addch(' ');
		}
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
		    if (toggled(VISIBLE_CONTROL)) {
			attrset(get_color_pair(COLOR_YELLOW,
				    COLOR_BLACK) | A_BOLD | A_UNDERLINE);
			addch(visible_fa(fa));
		    } else {
			u = crosshair_blank(baddr, &acs);
			if (u == ' ') {
			    attrset(defattr);
			    addch(' ');
			} else {
			    draw_crosshair(u, acs);
			}
		    }
		}
	    } else if (FA_IS_ZERO(fa)) {
		if (!is_menu) {
		    u = crosshair_blank(baddr, &acs);
		    if (u == ' ') {
			attrset(field_attrs & attr_mask);
			addch(' ');
		    } else {
			draw_crosshair(u, acs);
		    }
		    if (field_attrs & A_UNDERLINE) {
			underlined = true;
		    }
		}
	    } else {
		char mb[16];
		int len;
		int attrs;

		if (is_menu) {
		    continue;
		}

		if (!(ea_buf[baddr].gr ||
		      ea_buf[baddr].fg ||
		      ea_buf[baddr].bg)) {
		    attrs = field_attrs & attr_mask;
		    attrset(attrs);
		    if (field_attrs & A_UNDERLINE) {
			underlined = true;
		    }

		} else {
		    int buf_attrs;

		    buf_attrs = calc_attrs(baddr, fa_addr, fa);
		    attrs = buf_attrs & attr_mask;
		    attrset(attrs);
		    if (buf_attrs & A_UNDERLINE) {
			underlined = true;
		    }
		}
		d = ctlr_dbcs_state(baddr);
		if (IS_LEFT(d)) {
		    int xaddr = baddr;

		    INC_BA(xaddr);
		    if (toggled(VISIBLE_CONTROL) &&
			    ea_buf[baddr].ec == EBC_null &&
			    ea_buf[xaddr].ec == EBC_null) {
			attrset(attrs | A_UNDERLINE);
			addstr("..");
		    } else {
			if (ea_buf[baddr].ucs4 != 0) {
			    len = unicode_to_multibyte(ea_buf[baddr].ucs4,
				    mb, sizeof(mb));
			} else {
			    len = ebcdic_to_multibyte(
				    (ea_buf[baddr].ec << 8) |
				     ea_buf[xaddr].ec,
				    mb, sizeof(mb));
			}
			addstr(mb);
		    }
		} else if (!IS_RIGHT(d)) {
		    if (toggled(VISIBLE_CONTROL) &&
			    ea_buf[baddr].ucs4 == 0 &&
			    ea_buf[baddr].ec == EBC_null) {
			attrset(attrs | A_UNDERLINE);
			addstr(".");
		    } else if (toggled(VISIBLE_CONTROL) &&
			    ea_buf[baddr].ec == EBC_so) {
			attrset(attrs | A_UNDERLINE);
			addstr("<");
		    } else if (toggled(VISIBLE_CONTROL) &&
			    ea_buf[baddr].ec == EBC_si) {
			attrset(attrs | A_UNDERLINE);
			addstr(">");
		    } else if (ea_buf[baddr].cs == CS_LINEDRAW) {
			display_linedraw(ea_buf[baddr].ucs4);
		    } else if (ea_buf[baddr].cs == CS_APL ||
			    (ea_buf[baddr].cs & CS_GE)) {
			display_ge(ea_buf[baddr].ec);
		    } else {
			bool done_sbcs = false;
			ucs4_t uu;

			if ((uu = ea_buf[baddr].ucs4) != 0) {
			    if (toggled(MONOCASE)) {
				uu = u_toupper(uu);
			    }
			    len = unicode_to_multibyte(uu, mb, sizeof(mb));
			} else {
			    unsigned flags = EUO_BLANK_UNDEF |
			       (appres.c3270.ascii_box_draw? EUO_ASCII_BOX: 0) |
			       (toggled(MONOCASE)? EUO_TOUPPER: 0);

			    len = ebcdic_to_multibyte_x(
					ea_buf[baddr].ec,
					CS_BASE, mb,
					sizeof(mb),
					flags,
					NULL);
			}
			if (len > 0) {
			    len--;
			}
			if ((len == 1) && (mb[0] == ' ')) {
			    u = crosshair_blank(baddr, &acs);
			    if (u != ' ') {
				draw_crosshair(u, acs);
				done_sbcs = true;
			    }
			}
			if (!done_sbcs) {
			    if (toggled(UNDERSCORE) && underlined &&
				    (len == 1) && mb[0] == ' ') {
				mb[0] = '_';
			    }
#if defined(CURSES_WIDE) /*[*/
			    addstr(mb);
#else /*][*/
			    if (len > 1) {
				addch(' ');
			    } else {
				addch(mb[0] & 0xff);
			    }
#endif /*]*/
			}
		    }
		}
	    }
	}
    }
    if (status_row) {
	draw_oia();
    }
    attrset(defattr);
    if (menu_is_up) {
	menu_cursor(&row, &col);
	move(row, col);
    } else {
	if (flipped) {
	    move((cursor_addr / cCOLS + screen_yoffset),
		    cCOLS-1 - (cursor_addr % cCOLS));
	} else {
	    move((cursor_addr / cCOLS) + screen_yoffset,
		    cursor_addr % cCOLS);
	}
    }
    refresh();
}

/* ESC processing. */
static unsigned long eto = 0L;
static bool meta_escape = false;

static void
escape_timeout(ioid_t id _is_unused)
{
    vtrace("Timeout waiting for key following Escape, processing separately\n");
    eto = 0L;
    meta_escape = false;
    kybd_input2(0, 0x1b, 0);
}

/* Keyboard input. */
static void
kybd_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    int k = 0;		/* KEY_XXX, or 0 */
    ucs4_t ucs4 = 0;	/* input character, or 0 */
    bool first = true;
    static bool failed_first = false;

    for (;;) {
	volatile int alt = 0;
	char dbuf[128];
#if defined(CURSES_WIDE) /*[*/
	wint_t wch;
	size_t sz;
#endif /*]*/

	if (!initscr_done || isendwin()) {
	    return;
	}
	ucs4 = 0;
#if defined(CURSES_WIDE) /*[*/
	vtrace("kybd_input: calling wget_wch()\n");
	k = wget_wch(stdscr, &wch);
#else /*][*/
	vtrace("kybd_input: calling wgetch()\n");
	k = wgetch(stdscr);
#endif /*]*/
	vtrace("kbd_input: k=%d "
# if defined(CURSES_WIDE) /*[*/
		       "wch=%lu "
# endif /*]*/
				  "\n",
					k
# if defined(CURSES_WIDE) /*[*/
					 , (unsigned long)wch
# endif /*]*/
						             );
	if (k == ERR) {
	    if (first) {
		if (failed_first) {
		    vtrace("End of File, exiting.\n");
		    x3270_exit(1);
		}
		failed_first = true;
	    }
	    vtrace("kbd_input: k == ERR, return\n");
	    return;
	} else {
	    failed_first = false;
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
		vtrace("Invalid input wchar 0x%lx\n", (unsigned long)wch);
		return;
	    }
	    if (sz == 1) {
		ucs4 = mbs[0] & 0xff;
	    } else {
		int consumed;
		enum me_fail error;

		ucs4 = multibyte_to_unicode(mbs, sz, &consumed, &error);
		if (ucs4 == 0) {
		    vtrace("Unsupported input wchar 0x%lx\n",
			    (unsigned long)wch);
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
	    if (getmouse(&m) != OK) {
		return;
	    }
	    if ((m.bstate & BUTTON1_RELEASED)) {
		vtrace("Mouse BUTTON1_RELEASED (x=%d,y=%d)\n", m.x, m.y);
		if (screen_yoffset != 0 && m.y == 0) {
		    popup_menu(m.x, (screen_yoffset != 0));
		    screen_disp(false);
		} else if (status_row &&
		    m.x == rmargin - 28 &&
		    m.y == status_row) {
			run_action(AnShow, IA_DEFAULT, KwStatus, NULL);
		} else if (m.x < cCOLS &&
			   m.y - screen_yoffset >= 0 &&
			   m.y - screen_yoffset < ROWS) {
		    if (flipped) {
			cursor_move(((m.y - screen_yoffset) * cCOLS) +
				(cCOLS - m.x));
		    } else {
			cursor_move(((m.y - screen_yoffset) * cCOLS) + m.x);
		    }
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
	    meta_escape = false;
	    alt = KM_ALT;
	} else if (me_mode == TS_ON && ucs4 == 0x1b) {
	    vtrace("Key '%s' (curses key 0x%x, char code 0x%x)\n",
		    decode_key(k, ucs4, alt, dbuf), k, ucs4);
	    eto = AddTimeOut(ME_DELAY, escape_timeout);
	    vtrace(" waiting to see if Escape is followed by another key\n");
	    meta_escape = true;
	    continue;
	}
	vtrace("Key '%s' (curses key 0x%x, char code 0x%x)\n",
		decode_key(k, ucs4, alt, dbuf), k, ucs4);
	kybd_input2(k, ucs4, alt);
	first = false;
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
	screen_disp(false);
	return;
    }

    action = lookup_key(k, ucs4, alt);
    if (action != NULL) {
	if (strcmp(action, "[ignore]")) {
	    push_keymap_action(action);
	}
	return;
    }
    ia_cause = IA_DEFAULT;

    /* These first cases apply to both 3270 and NVT modes. */
    switch (k) {
    case KEY_UP:
	run_action(AnUp, IA_DEFAULT, NULL, NULL);
	return;
    case KEY_DOWN:
	run_action(AnDown, IA_DEFAULT, NULL, NULL);
	return;
    case KEY_LEFT:
	run_action(AnLeft, IA_DEFAULT, NULL, NULL);
	return;
    case KEY_RIGHT:
	run_action(AnRight, IA_DEFAULT, NULL, NULL);
	return;
    case KEY_HOME:
	run_action(AnRight, IA_DEFAULT, NULL, NULL);
	return;
    default:
	break;
    }
    switch (ucs4) {
    case 0x1d:
	run_action(AnEscape, IA_DEFAULT, NULL, NULL);
	return;
    }

    /* Then look for 3270-only cases. */
    if (IN_3270) {
	switch (k) {
	case KEY_DC:
	    run_action(AnDelete, IA_DEFAULT, NULL, NULL);
	    return;
	case KEY_BACKSPACE:
	    run_action(AnBackSpace, IA_DEFAULT, NULL, NULL);
	    return;
	case KEY_HOME:
	    run_action(AnHome, IA_DEFAULT, NULL, NULL);
	    return;
	default:
	    break;
	}
	switch (ucs4) {
	case 0x03:
	    run_action(AnClear, IA_DEFAULT, NULL, NULL);
	    return;
	case 0x12:
	    run_action(AnReset, IA_DEFAULT, NULL, NULL);
	    return;
	case 'L' & 0x1f:
	    run_action(AnRedraw, IA_DEFAULT, NULL, NULL);
	    return;
	case '\t':
	    run_action(AnTab, IA_DEFAULT, NULL, NULL);
	    return;
	case 0177:
	    run_action(AnDelete, IA_DEFAULT, NULL, NULL);
	    return;
	case '\b':
	    run_action(AnBackSpace, IA_DEFAULT, NULL, NULL);
	    return;
	case '\r':
	    run_action(AnEnter, IA_DEFAULT, NULL, NULL);
	    return;
	case '\n':
	    run_action(AnNewline, IA_DEFAULT, NULL, NULL);
	    return;
	default:
	    break;
	}
    }

    /* Do some NVT-only translations. */
    if (IN_NVT) {
	switch (k) {
	case KEY_DC:
	    ucs4 = 0x7f;
	    k = 0;
	    break;
	case KEY_BACKSPACE:
	    ucs4 = '\b';
	    k = 0;
	    break;
	}
    }

    /* Catch PF keys. */
    for (i = 1; i <= 24; i++) {
	if (k == KEY_F(i)) {
	    sprintf(buf, "%d", i);
	    run_action(AnPF, IA_DEFAULT, buf, NULL);
	    return;
	}
    }

    /* Then any other 8-bit ASCII character. */
    if (ucs4) {
	char ks[16];

	sprintf(ks, "U+%04x", ucs4);
	run_action(AnKey, IA_DEFAULT, ks, NULL);
	return;
    }

    vtrace(" dropped (no default)\n");
}

bool
screen_suspend(void)
{
    static bool need_to_scroll = false;
    bool needed = false;

    if (!initscr_done) {
	return false;
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
	    if (!curses_alt) {
		swap_screens(alt_screen);
	    }
	    curs_set_state = curs_set(1);
	    endwin();
	    swap_screens(def_screen);
	    (void) curs_set(1);
	    endwin();
	} else {
	    curs_set_state = curs_set(1);
	    endwin();
	}
#else /*][*/
	curs_set_state = curs_set(1);
	endwin();
#endif /*]*/
	needed = true;
    }

    if (!escaped) {
	escaped = true;

	if (need_to_scroll) {
	    printf("\n");
	} else {
	    need_to_scroll = true;
	}
#if defined(C3270_80_132) /*[*/
	if (curses_alt && def_screen != alt_screen) {
	    if (write(1, defscreen_spec.mode_switch,
			strlen(defscreen_spec.mode_switch)) < 0) {
		x3270_exit(1);
	    }
	}
#endif /*]*/
	RemoveInput(input_id);
	input_id = NULL_IOID;
    }

    return needed;
}

void
screen_resume(void)
{
    char *cl;

    if (!escaped) {
	return;
    }

    escaped = false;

    /* Ignore signals we don't like. */
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    /*
     * Clear the screen first, if possible, so future command output
     * starts at the bottom of the screen.
     */
    if ((cl = tigetstr("clear")) != NULL) {
	putp(cl);
    }

    /* Finish screen initialization. */
    if (!screen_initted) {
	finish_screen_init();
    }

#if defined(C3270_80_132) /*[*/
    if (def_screen != alt_screen && curses_alt) {
	/*
	 * When we suspended the screen, we switched to defscreen so
	 * that endwin() got called in the right order.  Switch back.
	 */
	swap_screens(alt_screen);
	if (write(1, altscreen_spec.mode_switch,
	    strlen(altscreen_spec.mode_switch)) < 0) {
	    x3270_exit(1);
	}
    }
#endif /*]*/
    screen_disp(false);
    refresh();
    if (curs_set_state != -1) {
	curs_set(curs_set_state);
	curs_set_state = -1;
    }
    if (input_id == NULL_IOID) {
	input_id = AddInput(0, kybd_input);
    }
}

void
cursor_move(int baddr)
{
    cursor_addr = baddr;
}

static void
toggle_monocase(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    screen_disp(false);
}

static void
toggle_underscore(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
    screen_disp(false);
}

static void
toggle_visibleControl(toggle_index_t ix _is_unused,
	enum toggle_type tt _is_unused)
{
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

/**
 * Toggle crosshair cursor.
 */
static void
toggle_crosshair(toggle_index_t ix _is_unused, enum toggle_type tt _is_unused)
{
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
static char oia_timing[32]; /* :ss.s*/
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

/* Compute the color pair for an OIA field. */
static curses_attr
status_colors(curses_color fg)
{
    if (appres.c3270.reverse_video &&
	    (fg == COLOR_WHITE || fg == defcolor_offset + COLOR_WHITE)) {
	fg = COLOR_BLACK;
    }
    return mode3279? get_color_pair(fg, bg_color): defattr;
}

void
status_minus(void)
{
    other_msg = "X -f";
    other_attr = status_colors(defcolor_offset + COLOR_RED) | A_BOLD;
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
    other_attr = status_colors(defcolor_offset + COLOR_RED) | A_BOLD;
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
    other_attr = status_colors(defcolor_offset + COLOR_WHITE) | A_BOLD;
}

void
status_twait(void)
{
    oia_undera = false;
    other_msg = "X Wait";
    other_attr = status_colors(defcolor_offset + COLOR_WHITE) | A_BOLD;
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
    other_attr = status_colors(defcolor_offset + COLOR_WHITE) | A_BOLD;
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
	strcpy(oia_timing, no_time);
    } else {
	unsigned long cs;	/* centiseconds */

	cs = (t1->tv_sec - t0->tv_sec) * 10 +
	     (t1->tv_usec - t0->tv_usec + 50000) / 100000;
	if (cs < CM) {
	    snprintf(oia_timing, sizeof(oia_timing),
		    ":%02ld.%ld", cs / 10, cs % 10);
	} else {
	    snprintf(oia_timing, sizeof(oia_timing),
		    "%02ld:%02ld", cs / CM, (cs % CM) / 10);
	}
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
status_script(bool on)
{
    oia_script = on? 's': ' ';
}

static void
draw_oia(void)
{
    static bool filled_extra[2] = { false, false };
    int i, j;
    int cursor_row = cursor_addr / cCOLS;
    int cursor_col = cursor_addr % cCOLS;
    int fl_cursor_col = flipped? (cursesCOLS - 1 - cursor_col): cursor_col;
    char *status_msg_now;
    int msg_attr;
    static struct {
	ucs4_t u;
	unsigned char acs;
    } vbar, hbar;
    static bool bars_done = false;

    /* Prepare the line-drawing characters for the crosshair. */
    if (toggled(CROSSHAIR) && !bars_done) {
	map_acs('x', &vbar.u, &vbar.acs);
	map_acs('q', &hbar.u, &hbar.acs);
	bars_done = true;
    }

#if defined(C3270_80_132) /*[*/
    if (def_screen != alt_screen) {
	if (curses_alt) {
	    rmargin = altscreen_spec.cols - 1;
	} else {
	    rmargin = defscreen_spec.cols - 1;
	}
    } else
#endif /*]*/
    {
	rmargin = maxCOLS - 1;
    }

    /* Black out the parts of the screen we aren't using. */
    if (!appres.interactive.mono && !filled_extra[!!curses_alt]) {
	int r, c;

	attrset(defattr);
	for (r = 0; r <= status_row; r++) {
	    int c0;

	    if (r >= maxROWS && r != status_row) {
		c0 = 0;
	    } else {
		c0 = maxCOLS;
	    }
	    move(r + screen_yoffset, c0);
	    for (c = c0; c < cursesCOLS; c++) {
		printw(" ");
	    }
	}
    }

    /* Make sure the status line region is filled in properly. */
    if (!appres.interactive.mono) {
	int i;

	attrset(defattr);
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

    /* Draw or undraw the crosshair cursor outside the primary display. */
    attrset(xhattr);

    /* Draw the crosshair over the menubar line. */
    if (screen_yoffset && toggled(CROSSHAIR) && !menu_is_up &&
	    (mvinch(0, fl_cursor_col) & A_CHARTEXT) == ' ') {
	draw_crosshair(vbar.u, vbar.acs);
    }

    /* Draw the crosshair between the menubar and display. */
    if (!menu_is_up && screen_yoffset > 1) {
	for (j = 0; j < cursesCOLS; j++) {
	    move(1, j);
	    if (toggled(CROSSHAIR) && j == fl_cursor_col) {
		draw_crosshair(vbar.u, vbar.acs);
	    } else {
		addch(' ');
	    }
	}
    }

    /* Draw the crosshair to the right of the display. */
    for (i = 0; i < ROWS; i++) {
	for (j = cCOLS; j < cursesCOLS; j++) {
	    move(i + screen_yoffset, j);
	    if (toggled(CROSSHAIR) && i == cursor_row) {
		draw_crosshair(hbar.u, hbar.acs);
	    } else {
		addch(' ');
	    }
	}
    }

    /*
     * Draw the crosshair between the bottom of the display and the
     * OIA.
     */
    for (i = screen_yoffset + ROWS; i < status_row; i++) {
	for (j = 0; j < cursesCOLS; j++) {
	    move(i, j);
	    if (toggled(CROSSHAIR) && j == fl_cursor_col) {
		draw_crosshair(vbar.u, vbar.acs);
	    } else {
		addch(' ');
	    }
	}
    }

/* The OIA looks like (in Model 2/3/4 mode):

          1         2         3         4         5         6         7
01234567890123456789012345678901234567890123456789012345678901234567890123456789
4AN    Status-Message--------------------- Cn TRIPS+s LU-Name-   :ss.s  000/000
         7         6         5         4         3         2         1
98765432109876543210987654321098765432109876543210987654321098765432109876543210
                                                                        ^ -7
                                                                 ^ -14
                                                      ^-25

   On wider displays, there is a bigger gap between TRIPSs and LU-Name.

*/

    /*
     * If there is at least one black line between the 3270 display and the
     * OIA, draw a row of underlined blanks above the OIA. This is
     * something c3270 can do that wc3270 cannot, since Windows consoles
     * can't do underlining.
     */
    if (status_row > screen_yoffset + maxROWS) {
	int i;
	attrset(A_UNDERLINE | defattr);
	move(status_row - 1, 0);
	for (i = 0; i < rmargin; i++) {
	    if (toggled(CROSSHAIR) && i == fl_cursor_col) {
		move(status_row - 1, i + 1);
	    } else {
		printw(" ");
	    }
	}
    }

    /* Clean up the OIA first, from a possible previous crosshair cursor. */
    {
	int i;

	move(status_row, 0);
	attrset(defattr);
	for (i = 0; i < cursesCOLS - 1; i++) {
	    printw(" ");
	}
    }

    attrset(A_REVERSE | defattr);
    mvprintw(status_row, 0, "4");
    attrset(A_UNDERLINE | defattr);
    if (oia_undera) {
	printw("%c", IN_E? 'B': 'A');
    } else {
	printw(" ");
    }
    attrset(A_REVERSE | defattr);
    if (IN_NVT) {
	printw("N");
    } else if (oia_boxsolid) {
	printw(" ");
    } else if (IN_SSCP) {
	printw("S");
    } else {
	printw("?");
    }

    /* Figure out the status message. */
    msg_attr = defattr;
    if (disabled_msg != NULL) {
	msg_attr = status_colors(defcolor_offset + COLOR_RED) | A_BOLD;
	status_msg_now = disabled_msg;
	reset_info();
    } else if (scrolled_msg != NULL) {
	msg_attr = status_colors(defcolor_offset + COLOR_WHITE) | A_BOLD;
	status_msg_now = scrolled_msg;
	reset_info();
    } else if (info_msg != NULL) {
	msg_attr = status_colors(defcolor_offset + COLOR_WHITE) | A_BOLD;
	status_msg_now = info_msg;
	set_info_timer();
    } else if (other_msg != NULL) {
	status_msg_now = other_msg;
	msg_attr = other_attr;
    } else {
	status_msg_now = "";
    }

    attrset(msg_attr);
    mvprintw(status_row, 7, "%-35.35s", status_msg_now);
    attrset(defattr);
    mvprintw(status_row, rmargin-35,
	"%c%c %c%c%c%c",
	oia_compose? 'C': ' ',
	oia_compose? oia_compose_char: ' ',
	status_ta? 'T': ' ',
	status_rm? 'R': ' ',
	status_im? 'I': ' ',
	oia_printer? 'P': ' ');
    if (status_secure != SS_INSECURE) {
	attrset(status_colors(defcolor_offset +
		    ((status_secure == SS_SECURE)? COLOR_GREEN: COLOR_YELLOW))
		| A_BOLD);
	printw("S");
	attrset(defattr);
    } else {
	printw(" ");
    }
    printw("%c%c", oia_screentrace,oia_script);

    mvprintw(status_row, rmargin-25, "%s", oia_lu);

    mvprintw(status_row, rmargin-14, "%s", oia_timing);

    mvprintw(status_row, rmargin-7, "%03d/%03d ", cursor_addr/cCOLS + 1,
	    cursor_addr%cCOLS + 1);

    /* Draw the crosshair in the OIA. */
    if (toggled(CROSSHAIR) &&
	    cursor_col > 2 &&
	    (mvinch(status_row, fl_cursor_col) & A_CHARTEXT) == ' ') {
	draw_crosshair(vbar.u, vbar.acs);
    }
}

bool
Redraw_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnRedraw, ia, argc, argv);
    if (check_argc(AnRedraw, argc, 0, 0) < 0) {
	return false;
    }

    if (!escaped) {
	endwin();
	refresh();
    }
    return true;
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
    screen_disp(false);
}

bool
screen_flipped(void)
{
    return flipped;
}

#if defined(C3270_80_132) /*[*/
/* Alt/default screen spec parsing. */
static void
parse_screen_spec(const char *str, struct screen_spec *spec)
{
    char msbuf[3];
    char *s, *t, c;
    bool escaped = false;

    if (sscanf(str, "%dx%d=%2s", &spec->rows, &spec->cols, msbuf) != 3) {
	fprintf(stderr, "Invalid screen screen spec '%s', must "
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
	    escaped = false;
	} else if (c == '\\') {
	    escaped = true;
	} else {
	    *t++ = c;
	}
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
		    strlen(altscreen_spec.mode_switch)) < 0) {
	    x3270_exit(1);
	}
	ctlr_erase(true);
	screen_disp(true);
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
		    strlen(defscreen_spec.mode_switch)) < 0) {
	    x3270_exit(1);
	}
	ctlr_erase(false);
	screen_disp(true);
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
display_linedraw(ucs4_t u)
{
    char mb[16];
    int len;

#if defined(CURSES_WIDE) /*[*/
    if (appres.c3270.acs)
#endif /*]*/
    {
	/* Try ACS first. */
	int c = linedraw_to_acs(u);

	if (c != -1) {
	    addch(c);
	    return;
	}
    }

    /* Then try Unicode. */
    len = unicode_to_multibyte(
	    linedraw_to_unicode(u, appres.c3270.ascii_box_draw),
	    mb, sizeof(mb));
    if (len > 0) {
	len--;
    }
#if defined(CURSES_WIDE) /*[*/
    addstr(mb);
#else /*][*/
    if (len > 1) {
	addch(mb[0] & 0xff);
    } else {
	addch(' ');
    }
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
    if (appres.c3270.acs)
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
	    EUO_BLANK_UNDEF | (appres.c3270.ascii_box_draw? EUO_ASCII_BOX: 0),
	    NULL);
    if (len > 0) {
	len--;
    }
#if defined(CURSES_WIDE) /*[*/
    addstr(mb);
#else /*][*/
    if (len > 1) {
	addch(mb[0] & 0xff);
    } else {
	addch(' ');
    }
#endif /*]*/
}

void
screen_final(void)
{
    char *cl;

    if ((cl = tigetstr("clear")) != NULL) {
	putp(cl);
    }
}

/**
 * Check if an area of the screen is selected.
 *
 * @param[in] baddr	Buffer address.
 *
 * @return true if cell is selected
 */
bool
screen_selected(int baddr _is_unused)
{
    return false;
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
 * @param[in] mn	Model number
 * @param[in] ovc	Oversize columns
 * @param[in] ovr	Oversize rowa.
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
    if (initscr_done && !isendwin()) {
	curs_set(on? 1: 0);
    }
}

/**
 * Screen module registration.
 */
void
screen_register(void)
{
    static toggle_register_t toggles[] = {
	{ MONOCASE,	toggle_monocase,	0 },
	{ SHOW_TIMING,	toggle_showTiming,	0 },
	{ UNDERSCORE,	toggle_underscore,	0 },
	{ VISIBLE_CONTROL, toggle_visibleControl, 0 },
	{ CROSSHAIR,	toggle_crosshair,	0 },
	{ TYPEAHEAD,	NULL,			0 }
    };
    static action_table_t screen_actions[] = {
	{ AnRedraw,	Redraw_action,	ACTION_KE }
    };

    /* Register the toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register for state changes. */
    register_schange(ST_NEGOTIATING, status_connect);
    register_schange(ST_CONNECT, status_connect);
    register_schange(ST_3270_MODE, status_3270_mode);
    register_schange(ST_PRINTER, status_printer);

    /* Register the actions. */
    register_actions(screen_actions, array_count(screen_actions));
}
