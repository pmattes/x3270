/*
 * Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2007 by Paul Mattes.
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
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "screenc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utf8c.h"
#include "utilc.h"
#include "widec.h"
#include "xioc.h"

#undef COLS
extern int cCOLS;

#undef COLOR_BLACK
#undef COLOR_RED
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE
#undef COLOR_WHITE
#if defined(HAVE_LIBNCURSESW) /*[*/
#include <ncursesw/ncurses.h>
#else /*][*/
#if defined(HAVE_NCURSES_H) /*[*/
#include <ncursesw/ncurses.h>
#else /*][*/ 
#include <curses.h>
#endif /*]*/
#endif /*]*/

static int cp[8][8][2];
static int cmap[16] = {
	COLOR_BLACK,	/* neutral black */
	COLOR_BLUE,	/* blue */
	COLOR_RED,	/* red */
	COLOR_MAGENTA,	/* pink */
	COLOR_GREEN,	/* green */
	COLOR_CYAN,	/* turquoise */
	COLOR_YELLOW,	/* yellow */
	COLOR_WHITE,	/* neutral white */

	COLOR_BLACK,	/* black */ /* alas, in bold, this may be gray */
	COLOR_BLUE,	/* deep blue */
	COLOR_YELLOW,	/* orange */
	COLOR_BLUE,	/* deep blue */
	COLOR_GREEN,	/* pale green */
	COLOR_CYAN,	/* pale turquoise */
	COLOR_BLACK,	/* gray */
	COLOR_WHITE	/* white */
};
static int field_colors[4] = {
	COLOR_GREEN,	/* default */
	COLOR_RED,		/* intensified */
	COLOR_BLUE,		/* protected */
	COLOR_WHITE		/* protected, intensified */
};
static int defattr = A_NORMAL;
static unsigned long input_id;

Boolean escaped = True;

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
	{ "magenta", COLOR_MAGENTA },
	{ "cyan",	COLOR_CYAN },
	{ "white",	COLOR_WHITE },
	{ CN,	0 }
};

static int status_row = 0;	/* Row to display the status line on */
static int status_skip = 0;	/* Row to blank above the status line */

static Boolean curses_alt = False;

static void kybd_input(void);
static void kybd_input2(int k, Boolean derived);
static void draw_oia(void);
static void status_connect(Boolean ignored);
static void status_3270_mode(Boolean ignored);
static void status_printer(Boolean on);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char);
static void screen_init2(void);
static void set_status_row(int screen_rows, int emulator_rows);
static Boolean ts_value(const char *s, enum ts *tsp);
static int linedraw_to_acs(unsigned char c);
static int apl_to_acs(unsigned char c);
static void init_user_colors(void);
static void init_user_attribute_colors(void);

/* Initialize the screen. */
void
screen_init(void)
{
	int want_ov_rows = ov_rows;
	int want_ov_cols = ov_cols;
	Boolean oversize = False;

#if !defined(C3270_80_132) /*[*/
	/* Disallow altscreen/defscreen. */
	if ((appres.altscreen != CN) || (appres.defscreen != CN)) {
		(void) fprintf(stderr, "altscreen/defscreen not supported\n");
		exit(1);
	}
	/* Initialize curses. */
	if (initscr() == NULL) {
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		exit(1);
	}
#else /*][*/
	/* Parse altscreen/defscreen. */
	if ((appres.altscreen != CN) ^ (appres.defscreen != CN)) {
		(void) fprintf(stderr,
		    "Must specify both altscreen and defscreen\n");
		exit(1);
	}
	if (appres.altscreen != CN) {
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

	/* Set up ncurses, and see if it's within bounds. */
	if (appres.defscreen != CN) {
		char nbuf[64];

		(void) sprintf(nbuf, "COLUMNS=%d", defscreen_spec.cols);
		putenv(NewString(nbuf));
		(void) sprintf(nbuf, "LINES=%d", defscreen_spec.rows);
		putenv(NewString(nbuf));
		def_screen = newterm(NULL, stdout, stdin);
		if (def_screen == NULL) {
			(void) fprintf(stderr,
			    "Can't initialize %dx%d defscreen terminal.\n",
			    defscreen_spec.rows, defscreen_spec.cols);
			exit(1);
		}
		(void) write(1, defscreen_spec.mode_switch,
		    strlen(defscreen_spec.mode_switch));
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
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		exit(1);
	}
	if (appres.altscreen) {
		set_term(alt_screen);
		cur_screen = alt_screen;
	}

	/* If they want 80/132 switching, then they want a model 5. */
	if (def_screen != NULL && model_num != 5) {
		appres.model = NewString("5");
		set_rows_cols(5, 0, 0);
	}
#endif /*]*/

	while (LINES < maxROWS || COLS < maxCOLS) {
		char buf[2];

		/*
		 * First, cancel any oversize.  This will get us to the correct
		 * model number, if there is any.
		 */
		if ((ov_cols && ov_cols > COLS) ||
		    (ov_rows && ov_rows > LINES)) {
			ov_cols = 0;
			ov_rows = 0;
			oversize = True;
			continue;
		}

		/* If we're at the smallest screen now, give up. */
		if (model_num == 2) {
			(void) fprintf(stderr, "Emulator won't fit on a %dx%d "
			    "display.\n", LINES, COLS);
			exit(1);
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
		if (want_ov_rows > LINES - 2)
			want_ov_rows = LINES - 2;
		if (want_ov_rows < maxROWS)
			want_ov_rows = maxROWS;
		if (want_ov_cols > COLS)
			want_ov_cols = COLS;
		set_rows_cols(model_num, want_ov_cols, want_ov_rows);
	}

	/* Figure out where the status line goes, if it fits. */
#if defined(C3270_80_132) /*[*/
	if (def_screen != NULL) {
		/* Start out in defscreen mode. */
		set_status_row(defscreen_spec.rows, 24);
	} else
#endif /*]*/
	{
		/* Start out in altscreen mode. */
		set_status_row(LINES, maxROWS);
	}

	/* Set up callbacks for state changes. */
	register_schange(ST_CONNECT, status_connect);
	register_schange(ST_3270_MODE, status_3270_mode);
	register_schange(ST_PRINTER, status_printer);

	/* Play with curses color. */
	if (!appres.mono) {
		start_color();
		if (has_colors() && COLORS >= 8) {
		    	if (appres.m3279)
				defattr =
				    get_color_pair(COLOR_BLUE, COLOR_BLACK);
			else
				defattr =
				    get_color_pair(COLOR_GREEN, COLOR_BLACK);
			if (COLORS < 16)
			    	appres.color8 = True;
#if defined(C3270_80_132) && defined(NCURSES_VERSION)  /*[*/
			if (def_screen != NULL) {
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
				(void) get_color_pair(COLOR_BLUE, COLOR_BLACK);
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

	/* See about keyboard Meta-key behavior. */
	if (!ts_value(appres.meta_escape, &me_mode))
		(void) fprintf(stderr, "invalid %s value: '%s', "
		    "assuming 'auto'\n", ResMetaEscape, appres.meta_escape);
	if (me_mode == TS_AUTO)
		me_mode = tigetflag("km")? TS_OFF: TS_ON;

	/* See about all-bold behavior. */
	if (appres.all_bold_on)
		ab_mode = TS_ON;
	else if (!ts_value(appres.all_bold, &ab_mode))
		(void) fprintf(stderr, "invalid %s value: '%s', "
		    "assuming 'auto'\n", ResAllBold, appres.all_bold);
	if (ab_mode == TS_AUTO)
		ab_mode = appres.m3279? TS_ON: TS_OFF;
	if (ab_mode == TS_ON)
		defattr |= A_BOLD;

	/* Pull in the user's color mappings. */
	init_user_colors();
	init_user_attribute_colors();

	/* Set up the controller. */
	ctlr_init(-1);
	ctlr_reinit(-1);

	/* Finish screen initialization. */
	screen_init2();
	screen_suspend();
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
	/*
	 * Finish initializing ncurses.  This should be the first time that it
	 * will send anything to the terminal.
	 */
	escaped = False;

	/* Set up the keyboard. */
	setup_tty();
	scrollok(stdscr, FALSE);
#if defined(C3270_80_132) /*[*/
	if (def_screen != NULL) {
		/*
		 * The first setup_tty() set up altscreen.
		 * Set up defscreen now, and leave it as the
		 * current curses screen.
		 */
		swap_screens(def_screen);
		setup_tty();
		scrollok(stdscr, FALSE);
	}
#endif /*]*/

	/* Subscribe to input events. */
	input_id = AddInput(0, kybd_input);

	/* Ignore SIGINT and SIGTSTP. */
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

#if defined(C3270_80_132) /*[*/
	/* Ignore SIGWINCH -- it might happen when we do 80/132 changes. */
	if (def_screen != NULL)
		signal(SIGWINCH, SIG_IGN);
#endif /*]*/
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
	static int next_pair[2] = { 1, 1 };
	int pair;
#if defined(C3270_80_132) && defined(NCURSES_VERSION) /*[*/
		/* ncurses allocates colors for each screen. */
	int pair_index = !!curses_alt;
#else /*][*/
		/* curses allocates colors globally. */
	const int pair_index = 0;
#endif /*]*/

	if ((pair = cp[fg][bg][pair_index]))
		return COLOR_PAIR(pair);
	if (next_pair[pair_index] >= COLOR_PAIRS)
		return 0;
	if (init_pair(next_pair[pair_index], fg, bg) != OK)
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

    	if ((r = get_resource(resname)) == CN)
		return;
	for (i = 0; cc_name[i].name != CN; i++) {
	    	if (!strcasecmp(r, cc_name[i].name)) {
		    	*a = cc_name[i].index;
			return;
		}
	}
	l = strtoul(r, &ptr, 0);
	if (ptr == r || *ptr != '\0' || l > 7) {
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
		return get_color_pair(fg, COLOR_BLACK) |
		    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL);
	} else if (!appres.mono) {
		return get_color_pair(COLOR_GREEN, COLOR_BLACK) |
		    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL);
	} else {
	    	/* No color at all. */
		return ((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL;
	}
}

/*
 * Set up the user-specified color mappings.
 */
static void
init_user_color(const char *name, int ix)
{
    	char *r;
	int i;
	unsigned long l;
	char *ptr;

	r = get_fresource("%s%s", ResCursesColorForHostColor, name);
	if (r == CN)
		r = get_fresource("%s%d", ResCursesColorForHostColor, ix);
	if (r == CN)
	    	return;

	for (i = 0; cc_name[i].name != CN; i++) {
	    	if (!strcasecmp(r, cc_name[i].name)) {
		    	cmap[ix] = cc_name[i].index;
			return;
		}
	}

	l = strtoul(r, &ptr, 0);
	if (ptr != r && *ptr == '\0' && l <= 7) {
	    	cmap[ix] = (int)l;
		return;
	}

	xs_warning("Invalid %s value '%s'", ResCursesColorForHostColor, r);
}

static void
init_user_colors(void)
{
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

#if defined(CURSES_WIDE) /*[*/
static void
myaddch(int c)
{
    	if (c & 0x80) {
	    	addstr(utf8_expand(c));
	} else {
		addch(c);
	}
}
#else /*][*/
#define myaddch(c) addch(c)
#endif /*]*/

/* Display what's in the buffer. */
void
screen_disp(Boolean erasing unused)
{
	int row, col;
	int field_attrs;
	int c;
	unsigned char fa;
	extern Boolean screen_alt;
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
	if (def_screen != NULL && screen_alt != curses_alt) {
		if (screen_alt) {
			(void) write(1, altscreen_spec.mode_switch,
			    strlen(altscreen_spec.mode_switch));
			trace_event("Switching to alt (%dx%d) screen.\n",
			    altscreen_spec.rows, altscreen_spec.cols);
			swap_screens(alt_screen);
			cur_spec = &altscreen_spec;
		} else {
			(void) write(1, defscreen_spec.mode_switch,
			    strlen(defscreen_spec.mode_switch));
			trace_event("Switching to default (%dx%d) screen.\n",
			    defscreen_spec.rows, defscreen_spec.cols);
			swap_screens(def_screen);
			cur_spec = &defscreen_spec;
		}

		/* Figure out where the status line goes now, if it fits. */
		set_status_row(cur_spec->rows, ROWS);

		curses_alt = screen_alt;

		/* Tell curses to forget what may be on the screen already. */
		endwin();
		erase();
	}
#endif /*]*/

	fa = get_field_attribute(0);
	fa_addr = find_field_attribute(0);
	field_attrs = calc_attrs(0, fa_addr, fa);
	for (row = 0; row < ROWS; row++) {
		int baddr;

		if (!flipped)
			move(row, 0);
		for (col = 0; col < cCOLS; col++) {
			if (flipped)
				move(row, cCOLS-1 - col);
			baddr = row*cCOLS+col;
			if (ea_buf[baddr].fa) {
			    	fa_addr = baddr;
				fa = ea_buf[baddr].fa;
				field_attrs = calc_attrs(baddr, baddr, fa);
				(void) attrset(defattr);
				myaddch(' ');
			} else if (FA_IS_ZERO(fa)) {
				(void) attrset(field_attrs);
				myaddch(' ');
			} else {
				if (!(ea_buf[baddr].gr ||
				      ea_buf[baddr].fg ||
				      ea_buf[baddr].bg)) {

					(void) attrset(field_attrs);

				} else {
				    	int buf_attrs;
		
				    	buf_attrs = calc_attrs(baddr, fa_addr,
						fa);
					(void) attrset(buf_attrs);
				}
#if defined(X3270_DBCS) /*[*/
				d = ctlr_dbcs_state(baddr);
				if (IS_LEFT(d)) {
					int xaddr = baddr;
					char mb[16];
					int len;
					INC_BA(xaddr);
					len = dbcs_to_mb(ea_buf[baddr].cc,
					    ea_buf[xaddr].cc,
					    mb);
					addstr(mb);
				} else if (!IS_RIGHT(d)) {
#endif /*]*/
					if (ea_buf[baddr].cs == CS_LINEDRAW) {
						c = linedraw_to_acs(ea_buf[baddr].cc);
						if (c != -1)
							myaddch(c);
						else
							myaddch(' ');
					} else if (ea_buf[baddr].cs == CS_APL ||
						   (ea_buf[baddr].cs & CS_GE)) {
						c = apl_to_acs(ea_buf[baddr].cc);
						if (c != -1)
							myaddch(c);
						else
							myaddch(' ');
					} else {
						if (toggled(MONOCASE))
							myaddch(asc2uc[ebc2asc[ea_buf[baddr].cc]]);
						else
							myaddch(ebc2asc[ea_buf[baddr].cc]);
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
	if (flipped)
		move(cursor_addr / cCOLS, cCOLS-1 - (cursor_addr % cCOLS));
	else
		move(cursor_addr / cCOLS, cursor_addr % cCOLS);
	refresh();
}

/* ESC processing. */
static unsigned long eto = 0L;
static Boolean meta_escape = False;

static void
escape_timeout(void)
{
	trace_event("Timeout waiting for key following Escape, processing "
	    "separately\n");
	eto = 0L;
	meta_escape = False;
	kybd_input2(0x1b, False);
}

#if defined(X3270_DBCS) /*[*/
/*
 * Accept a character of DBCS input.
 * The character is a multi-byte string encoded according to the current
 * locale.
 */
static int
dbcs_input(char *mbs)
{
    	UChar Ubuf[16];
	int wlen;
	unsigned char asc;
	unsigned char ebc[2];
	/* XXX */
	extern Boolean key_WCharacter(unsigned char code[], Boolean *skipped);
	extern void key_ACharacter(unsigned char c, enum keytype keytype,
		enum iaction cause, Boolean *skipped);

    	if (!dbcs)
	    	return -1;
	wlen = mb_to_unicode(mbs, strlen(mbs), Ubuf, 16, NULL);
	if (wlen < 0)
	    	return -1;
	if (dbcs_map8(Ubuf[0], &asc)) {
	    	trace_event("U+%04x -> EBCDIC SBCS X'%02x'\n",
			Ubuf[0] & 0xffff, asc2ebc[asc]);
	    	key_ACharacter(asc, KT_STD, ia_cause, NULL);
	} else if (dbcs_map16(Ubuf[0], ebc)) {
	    	trace_event("U+%04x -> EBCDIC DBCS X'%02x%02x'\n",
			Ubuf[0] & 0xffff, ebc[0], ebc[1]);
	    	key_WCharacter(ebc, NULL);
	} else {
	    	trace_event("Cannot convert U+%04x to EBCDIC\n",
			Ubuf[0] & 0xffff);
	    	return -1;
	}
	return 0;
}
#endif /*]*/

/* Keyboard input. */
static void
kybd_input(void)
{
	int k = 0;
	Boolean first = True;
	static Boolean failed_first = False;

	for (;;) {
		Boolean derived = False;
		char dbuf[128];
#if defined(CURSES_WIDE) /*[*/
		wint_t wch;
		size_t sz;
#endif /*]*/

		if (isendwin())
			return;
#if defined(CURSES_WIDE) /*[*/
		k = wget_wch(stdscr, &wch);
#else /*][*/
		k = wgetch(stdscr);
#endif /*]*/
		if (k == ERR) {
			if (first) {
				if (failed_first) {
					trace_event("End of File, exiting.\n");
					x3270_exit(1);
				}
				failed_first = True;
			}
			return;
		} else {
			failed_first = False;
		}
#if defined(CURSES_WIDE) /*[*/
		if (k == KEY_CODE_YES)
			k = (int)wch;
		else {
			char mbs[16];
			wchar_t wcs[2];

			wcs[0] = wch;
			wcs[1] = 0;
			sz = wcstombs(mbs, wcs, sizeof(mbs));
			if (sz < 0) {
				trace_event("Invalid input wchar %x\n", wch);
				return;
			}
			if (sz == 1) {
				k = mbs[0];
			} else {
			    	k = utf8_lookup(mbs, NULL, NULL);
				if (k == 0) {
#if defined(X3270_DBCS) /*[*/
					if (dbcs_input(mbs) == 0)
						return;
#endif /*]*/
					trace_event("Unsupported input "
						"wchar %x\n", wch);
					return;
				}
			}
		}
#endif /*]*/
		trace_event("Key %s (0x%x)\n", decode_key(k, 0, dbuf), k);

		/* Handle Meta-Escapes. */
		if (meta_escape) {
			if (eto != 0L) {
				RemoveTimeOut(eto);
				eto = 0L;
			}
			meta_escape = False;
			k |= 0x80;
			derived = True;
		} else if (me_mode == TS_ON && k == 0x1b) {
			eto = AddTimeOut(100L, escape_timeout);
			trace_event(" waiting to see if Escape is followed by"
			    " another key\n");
			meta_escape = True;
			continue;
		}
		kybd_input2(k, derived);
		first = False;
	}
}

static void
kybd_input2(int k, Boolean derived)
{
	char buf[16];
	char *action;
	char dbuf1[128], dbuf2[128];

	if (derived)
		trace_event(" combining <Key>Escape and %s into %s (0x%x)\n",
		    decode_key(k & 0x7f, 0, dbuf1),
		    decode_key(k, KM_META, dbuf2), k);
	action = lookup_key(k);
	if (action != CN) {
		if (strcmp(action, "[ignore]"))
			push_keymap_action(action);
		return;
	}
	ia_cause = IA_DEFAULT;

	/* These first cases apply to both 3270 and NVT modes. */
	switch (k) {
	case 0x1d:
		action_internal(Escape_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_UP:
		action_internal(Up_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_DOWN:
		action_internal(Down_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_LEFT:
		action_internal(Left_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_RIGHT:
		action_internal(Right_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_HOME:
		action_internal(Home_action, IA_DEFAULT, CN, CN);
		return;
	default:
		break;
	}

	/* Then look for 3270-only cases. */
	if (IN_3270) switch(k) {
	/* These cases apply only to 3270 mode. */
	case 0x03:
		action_internal(Clear_action, IA_DEFAULT, CN, CN);
		return;
	case 0x12:
		action_internal(Reset_action, IA_DEFAULT, CN, CN);
		return;
	case 'L' & 0x1f:
		action_internal(Redraw_action, IA_DEFAULT, CN, CN);
		return;
	case '\t':
		action_internal(Tab_action, IA_DEFAULT, CN, CN);
		return;
	case 0177:
	case KEY_DC:
		action_internal(Delete_action, IA_DEFAULT, CN, CN);
		return;
	case '\b':
	case KEY_BACKSPACE:
		action_internal(BackSpace_action, IA_DEFAULT, CN, CN);
		return;
	case '\r':
		action_internal(Enter_action, IA_DEFAULT, CN, CN);
		return;
	case '\n':
		action_internal(Newline_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_HOME:
		action_internal(Home_action, IA_DEFAULT, CN, CN);
		return;
	default:
		break;
	}

	/* Do some NVT-only translations. */
	if (IN_ANSI) switch(k) {
	case KEY_DC:
		k = 0x7f;
		break;
	case KEY_BACKSPACE:
		k = '\b';
		break;
	}

	/* Catch PF keys. */
	if (k >= KEY_F(1) && k <= KEY_F(24)) {
		(void) sprintf(buf, "%d", k - KEY_F0);
		action_internal(PF_action, IA_DEFAULT, buf, CN);
		return;
	}

	/* Then any other 8-bit ASCII character. */
	if (!(k & ~0xff)) {
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
#if defined(C3270_80_132) /*[*/
		if (def_screen != NULL) {
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

		if (need_to_scroll)
			printf("\n");
		else
			need_to_scroll = True;
#if defined(C3270_80_132) /*[*/
		if (curses_alt && def_screen != NULL) {
			(void) write(1, defscreen_spec.mode_switch,
			    strlen(defscreen_spec.mode_switch));
		}
#endif /*]*/
		RemoveInput(input_id);
	}
}

void
screen_resume(void)
{
	escaped = False;

#if defined(C3270_80_132) /*[*/
	if (def_screen != NULL && curses_alt) {
		/*
		 * When we suspended the screen, we switched to defscreen so
		 * that endwin() got called in the right order.  Switch back.
		 */
		swap_screens(alt_screen);
		(void) write(1, altscreen_spec.mode_switch,
		    strlen(altscreen_spec.mode_switch));
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

/*static*/ void
draw_oia(void)
{
	int rmargin;
	static Boolean filled_extra[2] = { False, False };

#if defined(C3270_80_132) /*[*/
	if (def_screen != NULL) {
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

		attrset(defattr);
		for (r = 0; r <= status_row; r++) {
		    	int c0;

			if (r >= maxROWS && r != status_row)
			    	c0 = 0;
			else
			    	c0 = maxCOLS;
		    	move(r, c0);
		    	for (c = c0; c < COLS; c++) {
			    	printw(" ");
			}
		}
	}

	/* Make sure the status line region is filled in properly. */
	if (!appres.mono) {
		int i;

		(void) attrset(defattr);
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

	(void) attrset(A_REVERSE | defattr);
	mvprintw(status_row, 0, "4");
	(void) attrset(A_UNDERLINE | defattr);
	if (oia_undera)
		printw("%c", IN_E? 'B': 'A');
	else
		printw(" ");
	(void) attrset(A_REVERSE | defattr);
	if (IN_ANSI)
		printw("N");
	else if (oia_boxsolid)
		printw(" ");
	else if (IN_SSCP)
		printw("S");
	else
		printw("?");

	(void) attrset(defattr);
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
	    	if (appres.m3279)
			attrset(get_color_pair(COLOR_GREEN, COLOR_BLACK) |
				A_BOLD);
		else
		    	attrset(A_BOLD);
		printw("S");
	    	attrset(defattr);
	} else
	    	printw(" ");

	mvprintw(status_row, rmargin-25, "%s", oia_lu);
	mvprintw(status_row, rmargin-7,
	    "%03d/%03d ", cursor_addr/cCOLS + 1, cursor_addr%cCOLS + 1);
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
		(void) write(1, altscreen_spec.mode_switch,
		    strlen(altscreen_spec.mode_switch));
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
		(void) write(1, defscreen_spec.mode_switch,
		    strlen(defscreen_spec.mode_switch));
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

Boolean
screen_new_display_charsets(char *cslist, char *csname)
{
    	/* It's all in the UTF-8 logic. */
    	return utf8_set_display_charsets(cslist, csname);
}
