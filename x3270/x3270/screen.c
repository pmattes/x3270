/*
 * Copyright (c) 1993-2009, Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR
 * GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	screen.c
 *		This module handles the X display.  It has been extensively
 *		optimized to minimize X drawing operations.
 */

#include "globals.h"
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>
#include <X11/Composite.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include "Husk.h"
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <errno.h>
#include <locale.h>
#include "3270ds.h"
#include "appres.h"
#include "screen.h"
#include "ctlr.h"
#include "cg.h"
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "display8c.h"
#include "hostc.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "savec.h"
#include "screenc.h"
#include "scrollc.h"
#include "seec.h"
#include "statusc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "unicodec.h"
#include "unicode_dbcsc.h"
#include "utf8c.h"
#include "utilc.h"
#include "xioc.h"

#if defined(HAVE_SYS_SELECT_H) /*[*/
#include <sys/select.h>		/* fd_set declaration */
#endif /*]*/

#include "x3270.bm"
#include "wait.bm"

#define SCROLLBAR_WIDTH	15

#define NO_BANG(s)	(((s)[0] == '!')? (s)+1: (s))

#if !defined(NBBY) /*[*/
#define NBBY 8
#endif /*]*/
#define BPW	(NBBY * sizeof(unsigned long))

/* Externals: main.c */
extern int      default_screen;

/* Externals: ctlr.c */
extern Boolean  screen_changed;
extern int      first_changed;
extern int      last_changed;

/* Globals */
unsigned char  *selected;	/* selection bitmap */
Dimension       main_width;
Boolean         scrollbar_changed = False;
Boolean         model_changed = False;
Boolean		efont_changed = False;
Boolean		oversize_changed = False;
Boolean		scheme_changed = False;
Pixel           keypadbg_pixel;
Boolean         flipped = False;
Pixmap          icon;
Boolean		shifted = False;
struct font_list *font_list = (struct font_list *) NULL;
int             font_count = 0;
char	       *efontname;
const char     *efont_charset;
const char     *efont_charset_dbcs;
Boolean		efont_matches = True;
char	       *full_efontname;
char	       *full_efontname_dbcs;
Boolean		visible_control = False;
unsigned	fixed_width, fixed_height;
int		hhalo = HHALO, vhalo = VHALO;

#define gray_width 2
#define gray_height 2
static char gray_bits[] = { 0x01, 0x02 };

/* Statics */
static Boolean	allow_resize;
static Dimension main_height;
static union sp *temp_image;	/* temporary for X display */
static Pixel	colorbg_pixel;
static Boolean	crosshair_enabled = True;
static Boolean  cursor_displayed = False;
static Boolean  cursor_enabled = True;
static Boolean  cursor_blink_pending = False;
static XtIntervalId cursor_blink_id;
static int	field_colors[4];
static Boolean  in_focus = False;
static Boolean  line_changed = False;
static Boolean  cursor_changed = False;
static Boolean  iconic = False;
static Widget   container;
static Widget   scrollbar;
static Dimension menubar_height;
#if defined(X3270_KEYPAD) /*[*/
static Dimension keypad_height;
static Dimension keypad_xwidth;
#endif /*]*/
static Dimension container_width;
static Dimension cwidth_nkp;
static Dimension container_height;
static Dimension scrollbar_width;
static char    *aicon_text = CN;
static XFontStruct *ailabel_font;
static Dimension aicon_label_height = 0;
static GC       ailabel_gc;
static Pixel    cpx[16];
static Boolean  cpx_done[16];
static Pixel    normal_pixel;
static Pixel    select_pixel;
static Pixel    bold_pixel;
static Pixel    selbg_pixel;
static Pixel    cursor_pixel;
static Boolean  text_blinking_on = True;
static Boolean  text_blinkers_exist = False;
static Boolean  text_blink_scheduled = False;
static XtIntervalId text_blink_id;
static XtTranslations screen_t00 = NULL;
static XtTranslations screen_t0 = NULL;
static XtTranslations container_t00 = NULL;
static XtTranslations container_t0 = NULL;
static XChar2b *rt_buf = (XChar2b *) NULL;
static char    *color_name[16] = {
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL,
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL,
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL,
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL
};
static Boolean	configure_ticking = False;
static XtIntervalId configure_id;

static Pixmap   inv_icon;
static Pixmap   wait_icon;
static Pixmap   inv_wait_icon;
static Boolean  icon_inverted = False;
static Widget	icon_shell;

static struct font_list *font_last = (struct font_list *) NULL;

#if defined(X3270_DBCS) /*[*/
static struct {
    Font font;
    XFontStruct *font_struct;
    Boolean unicode;
    int char_height;
    int char_width;
    int ascent;
    int descent;
    int xtra_width;
    int d16_ix;
} dbcs_font;
static void xim_init(void);
XIM im;
XIC ic;
Boolean xim_error = False;
char *locale_name = CN;
int ovs_offset = 1;
typedef struct {
	XIMStyle style;
	char *description;
} im_style_t;
static XIMStyle style;
char ic_focus;
static void send_spot_loc(void);
#endif /*]*/

/* Globals for undoing reconfigurations. */
static enum {
    REDO_NONE,
    REDO_FONT,
#if defined(X3270_MENUS) /*[*/
    REDO_MODEL,
#endif /*]*/
#if defined(X3270_KEYPAD) /*[*/
    REDO_KEYPAD,
#endif /*]*/
    REDO_SCROLLBAR,
    REDO_RESIZE
} screen_redo = REDO_NONE;
static char *redo_old_font = CN;
#if defined(X3270_MENUS) /*[*/
static int redo_old_model;
static int redo_old_ov_cols;
static int redo_old_ov_rows;
#endif /*]*/

static unsigned char blank_map[32];
#define BKM_SET(n)	blank_map[(n)/8] |= 1 << ((n)%8)
#define BKM_ISSET(n)	((blank_map[(n)/8] & (1 << ((n)%8))) != 0)

enum fallback_color { FB_WHITE, FB_BLACK };
static enum fallback_color ibm_fb = FB_WHITE;

static char *required_display_charsets;

#define CROSSABLE	(toggled(CROSSHAIR) && IN_3270 && \
			 cursor_enabled && crosshair_enabled)
#define CROSSED(b)	((BA_TO_COL(b) == cursor_col) || \
			 (BA_TO_ROW(b) == cursor_row))

/*
 * The screen state structure.  This structure is swapped whenever we switch
 * between normal and active-iconic states.
 */
#define NGCS	16
struct sstate {
	Widget          widget;	/* the widget */
	Window          window;	/* the window */
	union sp       *image;	/* what's on the X display */
	int             cursor_daddr;	/* displayed cursor address */
	Boolean         exposed_yet;	/* have we been exposed yet? */
	Boolean         overstrike;	/* are we overstriking? */
	Dimension       screen_width;	/* screen dimensions in pixels */
	Dimension       screen_height;
	GC              gc[NGCS * 2],	/* standard, inverted GCs */
	                selgc[NGCS],	/* color selected text GCs */
	                mcgc,		/* monochrome block cursor GC */
	                ucgc,		/* unique-cursor-color cursor GC */
	                invucgc,	/* inverse ucgc */
			clrselgc;	/* selected clearing GC */
	int             char_height;
	int             char_width;
	Font		fid;
	XFontStruct	*font;
	int		ascent;
	int		descent;
	int		xtra_width;
	Boolean         standard_font;
	Boolean		extended_3270font;
	Boolean         font_8bit;
	Boolean		font_16bit;
	Boolean		funky_font;
	Boolean         obscured;
	Boolean         copied;
	int		d8_ix;
	unsigned long	odd_width[256 / BPW];
	unsigned long	odd_lbearing[256 / BPW];
};
static struct sstate nss;
static struct sstate iss;
static struct sstate *ss = &nss;

#define	INIT_ODD(odd)	(void) memset(odd, '\0', sizeof(odd))
#define SET_ODD(odd, n)	(odd)[(n) / BPW] |= 1 << ((n) % BPW)
#define IS_ODD(odd, n)	((odd)[(n) / BPW] & 1 << ((n) % BPW))

#define DEFAULT_CHAR(f) (((f)->default_char >= (f)->min_char_or_byte2 && \
			  (f)->default_char <= (f)->max_char_or_byte2)? \
			    (f)->default_char: 32)
#define WHICH_CHAR(f, n) (((n) < (f)->min_char_or_byte2 || \
			   (n) > (f)->max_char_or_byte2)? \
			    DEFAULT_CHAR(f) : (n))
#define PER_CHAR(f, n)	((f)->per_char[WHICH_CHAR(f, n) - (f)->min_char_or_byte2])

/* Globals based on nss, used mostly by status and select routines. */
Widget         *screen = &nss.widget;
Window         *screen_window = &nss.window;
int            *char_width = &nss.char_width;
int            *char_height = &nss.char_height;
int            *ascent = &nss.ascent;
int            *descent = &nss.descent;
Boolean        *standard_font = &nss.standard_font;
Boolean        *font_8bit = &nss.font_8bit;
Boolean        *font_16bit = &nss.font_16bit;
Boolean        *extended_3270font = &nss.extended_3270font;
Boolean        *funky_font = &nss.funky_font;
int            *xtra_width = &nss.xtra_width;
Font           *fid = &nss.fid;

/* Mouse-cursor state */
enum mcursor_state { LOCKED, NORMAL, WAIT };
static enum mcursor_state mcursor_state = LOCKED;
static enum mcursor_state icon_cstate = NORMAL;

static void aicon_init(void);
static void aicon_reinit(unsigned cmask);
static void screen_focus(Boolean in);
static void make_gc_set(struct sstate *s, int i, Pixel fg, Pixel bg);
static void make_gcs(struct sstate *s);
static void put_cursor(int baddr, Boolean on);
static void resync_display(union sp *buffer, int first, int last);
static void draw_fields(union sp *buffer, int first, int last);
static void render_text(union sp *buffer, int baddr, int len,
    Boolean block_cursor, union sp *attrs);
static void cursor_pos(void);
static void cursor_on(void);
static void schedule_cursor_blink(void);
static void schedule_text_blink(void);
static void inflate_screen(void);
static int fa_color(unsigned char fa);
static Boolean cursor_off(void);
static void draw_aicon_label(void);
static void set_mcursor(void);
static void scrollbar_init(Boolean is_reset);
static void init_rsfonts(char *charset_name);
static void allocate_pixels(void);
static int fl_baddr(int baddr);
static GC get_gc(struct sstate *s, int color);
static GC get_selgc(struct sstate *s, int color);
static void default_color_scheme(void);
static Boolean xfer_color_scheme(char *cs, Boolean do_popup);
static void set_font_globals(XFontStruct *f, const char *ef, const char *fef,
    Font ff, Boolean is_dbcs);
static void screen_connect(Boolean ignored);
static void configure_stable(XtPointer closure, XtIntervalId *id);
static void cancel_blink(void);
static void render_blanks(int baddr, int height, union sp *buffer);
static void resync_text(int baddr, int len, union sp *buffer);
static void screen_reinit(unsigned cmask);
static void aicon_font_init(void);
static void aicon_size(Dimension *iw, Dimension *ih);
static void invert_icon(Boolean inverted);
static char *lff_single(const char *name, const char *reqd_display_charset,
    Boolean is_dbcs);
static char *load_fixed_font(const char *names, const char *reqd_charsets);
static void lock_icon(enum mcursor_state state);
static char *expand_cslist(const char *s);
static const char *name2cs_3270(const char *name);
static void hollow_cursor(int baddr);
static void revert_later(XtPointer closure _is_unused, XtIntervalId *id _is_unused);
#if defined(X3270_DBCS) /*[*/
static void xlate_dbcs(unsigned char, unsigned char, XChar2b *);
#endif /*]*/

/* Resize font list. */
struct rsfont {
	struct rsfont *next;
	char *name;
	int width;
	int height;
	int total_width;	/* transient */
	int total_height;	/* transient */
	int area;		/* transient */
};
static struct rsfont *rsfonts;

#define BASE_MASK		0x0f	/* mask for 16 actual colors */
#define INVERT_MASK		0x10	/* toggle for inverted colors */
#define GC_NONDEFAULT		0x20	/* distinguishes "color 0" from zeroed
					    memory */

#define COLOR_MASK		(GC_NONDEFAULT | BASE_MASK)
#define INVERT_COLOR(c)		((c) ^ INVERT_MASK)
#define NO_INVERT(c)		((c) & ~INVERT_MASK)

#define DEFAULT_PIXEL		(appres.m3279 ? COLOR_BLUE : FA_INT_NORM_NSEL)
#define PIXEL_INDEX(c)		((c) & BASE_MASK)


/*
 * Save 00 event translations.
 */
void
save_00translations(Widget w, XtTranslations *t00)
{
	*t00 = w->core.tm.translations;
}

/* 
 * Define our event translations
 */
void
set_translations(Widget w, XtTranslations *t00, XtTranslations *t0)
{
	struct trans_list *t;

	if (t00 != (XtTranslations *)NULL)
		XtOverrideTranslations(w, *t00);

	for (t = trans_list; t != NULL; t = t->next)
		XtOverrideTranslations(w, lookup_tt(t->name, CN));

	*t0 = w->core.tm.translations;
}

/*
 * Add or clear a temporary keymap.
 */
void
screen_set_temp_keymap(XtTranslations trans)
{
	if (trans != (XtTranslations)NULL) {
		XtOverrideTranslations(nss.widget, trans);
		XtOverrideTranslations(container, trans);
	} else {
		XtUninstallTranslations(nss.widget);
		XtOverrideTranslations(nss.widget, screen_t0);
		XtUninstallTranslations(container);
		XtOverrideTranslations(container, container_t0);
	}
}

/*
 * Change the baselevel keymap.
 */
void
screen_set_keymap(void)
{
	XtUninstallTranslations(nss.widget);
	set_translations(nss.widget, &screen_t00, &screen_t0);
	XtUninstallTranslations(container);
	set_translations(container, &container_t00, &container_t0);
}


/*
 * Initialize the screen.
 */
void
screen_init(void)
{
	register int i;

	if (!appres.m3279)
	    	appres.highlight_bold = True;

	visible_control = toggled(VISIBLE_CONTROL);

	/* Parse the fixed window size, if there is any. */
	if (appres.fixed_size) {
		char c;

		if (sscanf(appres.fixed_size, "%ux%u%c", &fixed_width,
					&fixed_height, &c) != 2 ||
				!fixed_width ||
				!fixed_height) {
			popup_an_error("Invalid fixed size");
			fixed_width = 0;
			fixed_height = 0;
		}
	}

	/* Initialize ss. */
	nss.cursor_daddr = 0;
	nss.exposed_yet = False;

	/* Initialize "gray" bitmap. */
	if (appres.mono)
		gray = XCreatePixmapFromBitmapData(display,
		    root_window, (char *)gray_bits,
		    gray_width, gray_height,
		    appres.foreground, appres.background, screen_depth);

	/* Initialize the blank map. */
	(void) memset((char *)blank_map, '\0', sizeof(blank_map));
	for (i = 0; i < 256; i++) {
		if (ebc2asc0[i] == 0x20 || ebc2asc0[i] == 0xa0)
			BKM_SET(i);
	}

	/* Register state change callbacks. */
	register_schange(ST_HALF_CONNECT, screen_connect);
	register_schange(ST_CONNECT, screen_connect);
	register_schange(ST_3270_MODE, screen_connect);

	/* Initialize the emulated 3270 controller hardware. */
	ctlr_init(ALL_CHANGE);

	/* Initialize the actve icon. */
	aicon_init();

	/* Initialize the status line. */
	status_init();

	/* Initialize the placement of the pop-up keypad. */
	keypad_placement_init();

	/* Now call the "reinitialize" function to set everything else up. */
	screen_reinit(ALL_CHANGE);
}

/*
 * Re-initialize the screen.
 */
static void
screen_reinit(unsigned cmask)
{
	Dimension cwidth_curr;
#if defined(X3270_KEYPAD) /*[*/
	Dimension mkw;
#endif /*]*/

	/* Allocate colors. */
	if (cmask & COLOR_CHANGE) {
		if (appres.m3279) {
			default_color_scheme();
			(void) xfer_color_scheme(appres.color_scheme, False);
		}
		allocate_pixels();
	}

	/* Define graphics contexts. */
	if (cmask & (FONT_CHANGE | COLOR_CHANGE))
		make_gcs(&nss);

	/* Reinitialize the controller. */
	ctlr_reinit(cmask);

	/* Allocate buffers. */
	if (cmask & MODEL_CHANGE) {
		/* Selection bitmap */
		Replace(selected,
		    (unsigned char *)XtCalloc(sizeof(unsigned char),
				              (maxROWS * maxCOLS + 7) / 8));

		/* X display image */
		Replace(nss.image, (union sp *)XtCalloc(sizeof(union sp),
							maxROWS * maxCOLS));
		Replace(temp_image, (union sp *)XtCalloc(sizeof(union sp),
							 maxROWS*maxCOLS));

		/* render_text buffers */
		Replace(rt_buf,
		    (XChar2b *)XtMalloc(maxCOLS * sizeof(XChar2b)));
	} else
		(void) memset((char *) nss.image, 0,
		              sizeof(union sp) * maxROWS * maxCOLS);

	/* Compute SBCS/DBCS size differences. */
#if defined(X3270_DBCS) /*[*/
	if ((cmask & FONT_CHANGE) && dbcs) {
		int wdiff, adiff, ddiff;
		char *xs;
		int xx;

#if defined(_ST) /*[*/
		printf("nss ascent %d descent %d\n"
		       "dbcs ascent %d descent %d\n",
		       nss.ascent, nss.descent,
		       dbcs_font.ascent, dbcs_font.descent);
#endif /*]*/

		/* Compute width difference. */
		wdiff = (2 * nss.char_width) - dbcs_font.char_width;
		if (wdiff > 0) {
			/* SBCS font is too wide */
			dbcs_font.xtra_width = wdiff;
#if defined(_ST) /*[*/
			printf("SBCS wider %d\n", wdiff);
#endif /*]*/
		} else if (wdiff < 0) {
			/* SBCS font is too narrow */
			if (wdiff % 2) {
				nss.xtra_width = (-wdiff)/2 + 1;
				dbcs_font.xtra_width = 1;
#if defined(_ST) /*[*/
				printf("SBCS odd\n");
#endif /*]*/
			} else
				nss.xtra_width = (-wdiff)/2;
#if defined(_ST) /*[*/
			printf("DBCS wider %d\n", -wdiff);
#endif /*]*/
		} else {
			dbcs_font.xtra_width = nss.xtra_width = 0;
#if defined(_ST) /*[*/
			printf("Width matches.\n");
#endif /*]*/
		}
		/* Add some extra on top of that. */
		if ((xs = getenv("X3270_XWIDTH")) != NULL) {
			xx = atoi(xs);
			if (xx && xx < 10) {
				nss.xtra_width += xx;
				dbcs_font.xtra_width += 2*xx;
			}
		}
		nss.char_width += nss.xtra_width;
		dbcs_font.char_width += dbcs_font.xtra_width;

		/*
		 * Compute height difference, doing ascent and descent
		 * separately.
		 */
		adiff = nss.ascent - dbcs_font.ascent;
		if (adiff > 0) {
#if defined(_ST) /*[*/
			printf("SBCS higher by %d\n", adiff);
			dbcs_font.ascent += adiff;
			dbcs_font.char_height += adiff;
#endif /*]*/
		} else if (adiff < 0) {
#if defined(_ST) /*[*/
			printf("DBCS higher by %d\n", -adiff);
			nss.ascent += -adiff;
			nss.char_height += -adiff;
#endif /*]*/
		} else {
#if defined(_ST) /*[*/
			printf("Ascent matches\n");
#endif /*]*/
		}
		ddiff = nss.descent - dbcs_font.descent;
		if (ddiff > 0) {
#if defined(_ST) /*[*/
			printf("SBCS lower by %d\n", ddiff);
#endif /*]*/
			dbcs_font.descent += ddiff;
			dbcs_font.char_height += ddiff;
		} else if (ddiff < 0) {
#if defined(_ST) /*[*/
			printf("DBCS lower by %d\n", -ddiff);
#endif /*]*/
			nss.descent += -ddiff;
			nss.char_height += -ddiff;
		} else {
#if defined(_ST) /*[*/
			printf("Descent matches\n");
#endif /*]*/
		}
		
		/* Add a constant to the height. */
		if ((xs = getenv("X3270_XHEIGHT")) != NULL) {
			xx = atoi(xs);
			if (xx && xx < 10) {
				dbcs_font.descent += xx;
				nss.descent += xx;
				nss.char_height += xx;
			}
		}
	}
#endif /*]*/

	/* Set up a container for the menubar, screen and keypad */

	if (toggled(SCROLL_BAR))
		scrollbar_width = SCROLLBAR_WIDTH;
	else
		scrollbar_width = 0;

	if (1/*cmask & (FONT_CHANGE | MODEL_CHANGE)*/) {
		if (fixed_width) {
			Dimension w, h;

			/* Compute the halos. */
			hhalo = 0;
			w = SCREEN_WIDTH(ss->char_width) + scrollbar_width;
			if (w > fixed_width) {
				if (screen_redo == REDO_NONE)
					Error("Font is too wide for fixed "
							"width");
				hhalo = HHALO;
				(void) XtAppAddTimeOut(appcontext, 10,
						       revert_later, 0);
			} else
				hhalo = (fixed_width - w) / 2;
			vhalo = 0;
			h = SCREEN_HEIGHT(ss->char_height);
			if (h > fixed_height) {
				if (screen_redo == REDO_NONE)
					Error("Font is too tall for fixed "
							"width");
				vhalo = VHALO;
				(void) XtAppAddTimeOut(appcontext, 10,
						       revert_later, 0);
			} else
				vhalo = (fixed_height - h) / 2;
		}
		nss.screen_width  = SCREEN_WIDTH(ss->char_width);
		nss.screen_height = SCREEN_HEIGHT((ss->char_height));
	}

	if (fixed_width)
		container_width = fixed_width;
	else
		container_width = nss.screen_width+2 + scrollbar_width;
	cwidth_nkp = container_width;
#if defined(X3270_KEYPAD) /*[*/
	mkw = min_keypad_width();
	if (kp_placement == kp_integral && container_width < mkw) {
		keypad_xwidth = mkw - container_width;
		container_width = mkw;
	} else
		keypad_xwidth = 0;
#endif /*]*/

	if (container == (Widget)NULL) {
		container = XtVaCreateManagedWidget(
		    "container", huskWidgetClass, toplevel,
		    XtNborderWidth, 0,
		    XtNwidth, container_width,
		    XtNheight, 10,
			/* XXX -- a temporary lie to make Xt happy */
		    NULL);
		save_00translations(container, &container_t00);
		set_translations(container, (XtTranslations *)NULL,
		    &container_t0);
		if (appres.mono)
			XtVaSetValues(container, XtNbackgroundPixmap, gray,
			    NULL);
		else
			XtVaSetValues(container, XtNbackground, keypadbg_pixel,
			    NULL);
	}

	/* Initialize the menu bar and integral keypad */

#if defined(X3270_KEYPAD) /*[*/
	cwidth_curr = appres.keypad_on ? container_width : cwidth_nkp;
#else /*][*/
	cwidth_curr = container_width;
#endif /*]*/
	menubar_height = menubar_qheight(cwidth_curr);
	menubar_init(container, container_width, cwidth_curr);

	if (fixed_height)
		container_height = fixed_height;
	else
		container_height = menubar_height + nss.screen_height+2;
#if defined(X3270_KEYPAD) /*[*/
	if (kp_placement == kp_integral) {
		(void) keypad_init(container, container_height,
		    container_width, False, False);
		keypad_height = keypad_qheight();
	} else
		keypad_height = 0;
	container_height += keypad_height;
#endif /*]*/

	/* Create screen and set container dimensions */
	inflate_screen();

	/* Create scrollbar */
	scrollbar_init((cmask & MODEL_CHANGE) != 0);

	XtRealizeWidget(toplevel);
	nss.window = XtWindow(nss.widget);
	set_mcursor();

	/* Reinitialize the active icon. */
	aicon_reinit(cmask);

	/* Reinitialize the status line. */
	status_reinit(cmask);

#if defined(X3270_DBCS) /*[*/
	/* Initialize the input method. */
	if ((cmask & CHARSET_CHANGE) && dbcs)
		xim_init();
#endif /*]*/

	cursor_changed = True;

	line_changed = True;

	/* Redraw the screen. */
	action_internal(PA_Expose_action, IA_REDRAW, CN, CN);
}


static void
set_toplevel_sizes(void)
{
	Dimension tw, th;

#if defined(X3270_KEYPAD) /*[*/
	tw = container_width - (appres.keypad_on ? 0 : keypad_xwidth);
	th = container_height - (appres.keypad_on ? 0 : keypad_height);
#else /*][*/
	tw = container_width;
	th = container_height;
#endif /*]*/
	if (fixed_width) {
		XtVaSetValues(toplevel,
		    XtNwidth, fixed_width,
		    XtNheight, fixed_height,
		    NULL);
		XtVaSetValues(toplevel,
		    XtNbaseWidth, fixed_width,
		    XtNbaseHeight, fixed_height,
		    XtNminWidth, fixed_width,
		    XtNminHeight, fixed_height,
		    XtNmaxWidth, fixed_width,
		    XtNmaxHeight, fixed_height,
		    NULL);
		XtVaSetValues(container,
		    XtNwidth, fixed_width,
		    XtNheight, fixed_height,
		    NULL);
		main_width = fixed_width;
		main_height = fixed_height;
	} else {
		XtVaSetValues(toplevel,
		    XtNwidth, tw,
		    XtNheight, th,
		    NULL);
		if (!allow_resize)
			XtVaSetValues(toplevel,
			    XtNbaseWidth, tw,
			    XtNbaseHeight, th,
			    XtNminWidth, tw,
			    XtNminHeight, th,
			    XtNmaxWidth, tw,
			    XtNmaxHeight, th,
			    NULL);
		XtVaSetValues(container,
		    XtNwidth, container_width,
		    XtNheight, container_height,
		    NULL);
		main_width = tw;
		main_height = th;
	}

	/*
	 * Start a timer ticking, in case the window manager doesn't approve
	 * of the change.
	 */
	if (configure_ticking)
		XtRemoveTimeOut(configure_id);
	configure_id = XtAppAddTimeOut(appcontext, 500, configure_stable, 0);
	configure_ticking = True;

	keypad_move();
	{
	    	static Boolean first = True;
		if (first)
		    	first = False;
		else
		    	popups_move();
	}
}

static void
inflate_screen(void)
{
	/* Create the screen window */
	if (nss.widget == NULL) {
		nss.widget = XtVaCreateManagedWidget(
		    "screen", widgetClass, container,
		    XtNwidth, nss.screen_width,
		    XtNheight, nss.screen_height,
		    XtNx,
#if defined(X3270_KEYPAD) /*[*/
			appres.keypad_on ? (keypad_xwidth / 2) : 0,
#else /*][*/
			0,
#endif /*]*/
		    XtNy, menubar_height,
		    XtNbackground,
			appres.mono ? appres.background : colorbg_pixel,
		    NULL);
		save_00translations(nss.widget, &screen_t00);
		set_translations(nss.widget, (XtTranslations *)NULL,
		    &screen_t0);
	} else {
		XtVaSetValues(nss.widget,
		    XtNwidth, nss.screen_width,
		    XtNheight, nss.screen_height,
		    XtNx,
#if defined(X3270_KEYPAD) /*[*/
			appres.keypad_on ? (keypad_xwidth / 2) : 0,
#else /*][*/
			0,
#endif /*]*/
		    XtNy, menubar_height,
		    XtNbackground,
			appres.mono ? appres.background : colorbg_pixel,
		    NULL);
	}

	/* Set the container and toplevel dimensions */
	XtVaSetValues(container,
	    XtNwidth, container_width,
	    XtNheight, container_height,
	    NULL);

	set_toplevel_sizes();
}

/* Scrollbar support. */
void
screen_set_thumb(float top, float shown)
{
	if (toggled(SCROLL_BAR))
		XawScrollbarSetThumb(scrollbar, top, shown);
}

static void
screen_scroll_proc(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer position)
{
	scroll_proc((long)position, (int)nss.screen_height);
}

static void
screen_jump_proc(Widget w _is_unused, XtPointer client_data _is_unused,
    XtPointer percent_ptr)
{
	jump_proc(*(float *)percent_ptr);
}

/* Create, move, or reset the scrollbar. */
static void
scrollbar_init(Boolean is_reset)
{
	if (!scrollbar_width) {
		if (scrollbar != (Widget)NULL)
			XtUnmapWidget(scrollbar);
	} else {
		if (scrollbar == (Widget)NULL) {
			scrollbar = XtVaCreateManagedWidget(
			    "scrollbar", scrollbarWidgetClass,
			    container,
			    XtNx, nss.screen_width+1
#if defined(X3270_KEYPAD) /*[*/
				    + (appres.keypad_on ? (keypad_xwidth / 2) : 0)
#endif /*]*/
				    ,
			    XtNy, menubar_height,
			    XtNwidth, scrollbar_width-1,
			    XtNheight, nss.screen_height,
			    XtNbackground, appres.mono ?
				appres.background : keypadbg_pixel,
			    NULL);
			XtAddCallback(scrollbar, XtNscrollProc,
			    screen_scroll_proc, NULL);
			XtAddCallback(scrollbar, XtNjumpProc,
			    screen_jump_proc, NULL);
		} else {
			XtVaSetValues(scrollbar,
			    XtNx, nss.screen_width+1
#if defined(X3270_KEYPAD) /*[*/
				    + (appres.keypad_on ? (keypad_xwidth / 2) : 0)
#endif /*]*/
				    ,
			    XtNy, menubar_height,
			    XtNwidth, scrollbar_width-1,
			    XtNheight, nss.screen_height,
			    XtNbackground, appres.mono ?
				appres.background : keypadbg_pixel,
			    NULL);
			XtMapWidget(scrollbar);
		}
		XawScrollbarSetThumb(scrollbar, 0.0, 1.0);

	}

	/*
	 * If the screen dimensions have changed, reallocate the scroll
	 * save area.
	 */
	if (is_reset || !scroll_initted)
		scroll_init();
	else
		rethumb();
}

/* Turn the scrollbar on or off */
void
toggle_scrollBar(struct toggle *t _is_unused, enum toggle_type tt _is_unused)
{
	scrollbar_changed = True;

	if (toggled(SCROLL_BAR)) {
		scrollbar_width = SCROLLBAR_WIDTH;
		screen_redo = REDO_SCROLLBAR;
	} else {
		scroll_to_bottom();
		scrollbar_width = 0;
	}

	screen_reinit(SCROLL_CHANGE);
	if (toggled(SCROLL_BAR))
		rethumb();
}

/*
 * Called when a host connects, disconnects or changes ANSI/3270 mode.
 */
static void
screen_connect(Boolean ignored _is_unused)
{
	if (ea_buf == NULL)
		return;		/* too soon */

	if (CONNECTED) {
		/*
		 * Clear the screen.
		 * If we're in ANSI/NVT mode, go to the maximum screen
		 * dimensions, otherwise go to the default 24x80 for 3270
		 * or SSCP mode.
		 */
		ctlr_erase((IN_ANSI || IN_SSCP)? True: False);
		if (IN_3270)
			scroll_round();
		cursor_on();
		schedule_cursor_blink();
	} else {
		if (appres.disconnect_clear)
			ctlr_erase(True);
		(void) cursor_off();
	}
	if (toggled(CROSSHAIR)) {
		screen_changed = True;
		first_changed = 0;
		last_changed = ROWS*COLS;
		screen_disp(False);
	}

	mcursor_normal();
}

/*
 * Mouse cursor changes
 */

static void
set_mcursor(void)
{
	switch (mcursor_state) {
	    case LOCKED:
		XDefineCursor(display, nss.window, appres.locked_mcursor);
		break;
	    case NORMAL:
		XDefineCursor(display, nss.window, appres.normal_mcursor);
		break;
	    case WAIT:
		XDefineCursor(display, nss.window, appres.wait_mcursor);
		break;
	}
	lock_icon(mcursor_state);
}

void
mcursor_normal(void)
{
	if (CONNECTED)
		mcursor_state = NORMAL;
	else if (HALF_CONNECTED)
		mcursor_state = WAIT;
	else
		mcursor_state = LOCKED;
	set_mcursor();
}

void
mcursor_waiting(void)
{
	mcursor_state = WAIT;
	set_mcursor();
}

void
mcursor_locked(void)
{
	mcursor_state = LOCKED;
	set_mcursor();
}

#if defined(X3270_KEYPAD) /*[*/
/*
 * Called from the keypad button to expose or hide the integral keypad.
 */
void
screen_showikeypad(Boolean on)
{
	if (on)
		screen_redo = REDO_KEYPAD;

	inflate_screen();
	if (keypad_xwidth > 0) {
		if (scrollbar != (Widget) NULL)
			scrollbar_init(False);
		menubar_resize(on ? container_width : cwidth_nkp);
	}
}
#endif /*]*/


/*
 * The host just wrote a blinking character; make sure it blinks
 */
void
blink_start(void)
{
	text_blinkers_exist = True;
	if (!text_blink_scheduled) {
		/* Start in "on" state and start first iteration */
		text_blinking_on = True;
		schedule_text_blink();
	}
}

/*
 * Restore blanked blinking text
 */
static void
text_blink_it(XtPointer closure _is_unused, XtIntervalId *id _is_unused)
{
	/* Flip the state. */
	text_blinking_on = !text_blinking_on;

	/* Force a screen redraw. */
	ctlr_changed(0, ROWS*COLS);

	/* If there is still blinking text, schedule the next iteration */
	if (text_blinkers_exist)
		schedule_text_blink();
	else
		text_blink_scheduled = False;
}

/*
 * Schedule an event to restore blanked blinking text
 */
static void
schedule_text_blink(void)
{
	text_blink_scheduled = True;
	text_blink_id = XtAppAddTimeOut(appcontext, 500, text_blink_it, 0);
}


/*
 * Make the (displayed) cursor disappear.  Returns a Boolean indiciating if
 * the cursor was on before the call.
 */
static Boolean
cursor_off(void)
{
	if (cursor_displayed) {
		cursor_displayed = False;
		put_cursor(ss->cursor_daddr, False);
		return True;
	} else
		return False;
}



/*
 * Blink the cursor
 */
static void
cursor_blink_it(XtPointer closure _is_unused, XtIntervalId *id _is_unused)
{
	cursor_blink_pending = False;
	if (!CONNECTED || !toggled(CURSOR_BLINK))
		return;
	if (cursor_displayed) {
		if (in_focus)
			(void) cursor_off();
	} else
		cursor_on();
	schedule_cursor_blink();
}

/*
 * Schedule a cursor blink
 */
static void
schedule_cursor_blink(void)
{
	if (!toggled(CURSOR_BLINK) || cursor_blink_pending)
		return;
	cursor_blink_pending = True;
	cursor_blink_id = XtAppAddTimeOut(appcontext, 500, cursor_blink_it, 0);
}

/*
 * Cancel a cursor blink
 */
static void
cancel_blink(void)
{
	if (cursor_blink_pending) {
		XtRemoveTimeOut(cursor_blink_id);
		cursor_blink_pending = False;
	}
}

/*
 * Toggle cursor blinking (called from menu)
 */
void
toggle_cursorBlink(struct toggle *t _is_unused, enum toggle_type tt _is_unused)
{
	if (!CONNECTED)
		return;

	if (toggled(CURSOR_BLINK))
		schedule_cursor_blink();
	else
		cursor_on();
}

/*
 * Make the cursor visible at its (possibly new) location.
 */
static void
cursor_on(void)
{
	if (cursor_enabled && !cursor_displayed) {
		cursor_displayed = True;
		put_cursor(cursor_addr, True);
		ss->cursor_daddr = cursor_addr;
		cursor_changed = False;
	}
}


/*
 * Toggle the cursor (block/underline).
 */
void
toggle_altCursor(struct toggle *t, enum toggle_type tt _is_unused)
{
	Boolean was_on;

	/* do_toggle already changed the value; temporarily change it back */
	toggle_toggle(t);

	was_on = cursor_off();

	/* Now change it back again */
	toggle_toggle(t);

	if (was_on)
		cursor_on();
}


/*
 * Move the cursor to the specified buffer address.
 */
void
cursor_move(int baddr)
{
	cursor_addr = baddr;
	cursor_pos();
}

/*
 * Display the cursor position on the status line
 */
static void
cursor_pos(void)
{
	if (!toggled(CURSOR_POS) || !CONNECTED)
		return;
	status_cursor_pos(cursor_addr);
}

/*
 * Toggle the display of the cursor position
 */
void
toggle_cursorPos(struct toggle *t _is_unused, enum toggle_type tt _is_unused)
{
	if (toggled(CURSOR_POS))
		cursor_pos();
	else
		status_uncursor_pos();
}

/*
 * Enable or disable cursor display (used by scroll logic)
 */
void
enable_cursor(Boolean on)
{
	if ((cursor_enabled = on) && CONNECTED) {
		cursor_on();
		cursor_changed = True;
	} else
		(void) cursor_off();
}


/*
 * Toggle the crosshair cursor.
 */
void
toggle_crosshair(struct toggle *t, enum toggle_type tt _is_unused)
{
	screen_changed = True;
	first_changed = 0;
	last_changed = ROWS*COLS;
	screen_disp(False);
}


/*
 * Toggle visible control characters.
 */
void
toggle_visible_control(struct toggle *t, enum toggle_type tt _is_unused)
{
	visible_control = toggled(VISIBLE_CONTROL);
	screen_changed = True;
	first_changed = 0;
	last_changed = ROWS*COLS;
	screen_disp(False);
}


/*
 * Redraw the screen.
 */
static void
do_redraw(Widget w, XEvent *event, String *params _is_unused,
    Cardinal *num_params _is_unused)
{
	int	x, y, width, height;
	int	startcol, ncols;
	int	startrow, endrow, row;
	register int i;
	int     c0;

	if (w == nss.widget) {
		keypad_first_up();
		if (appres.active_icon && iconic) {
			ss = &nss;
			iconic = False;
		}
	} else if (appres.active_icon && w == iss.widget) {
		if (appres.active_icon && !iconic) {
			ss = &iss;
			iconic = True;
		}
	} else if (event)
		return;

	/* Only redraw as necessary for an expose event */
	if (event && event->type == Expose) {
		ss->exposed_yet = True;
		x = event->xexpose.x;
		y = event->xexpose.y;
		width = event->xexpose.width;
		height = event->xexpose.height;
		startrow = ssY_TO_ROW(y);
		if (startrow < 0)
			startrow = 0;
		if (startrow > 0)
			startrow--;
		endrow = ssY_TO_ROW(y+height);
		endrow = endrow >= maxROWS ? maxROWS : endrow + 1;
		startcol = ssX_TO_COL(x);
		if (startcol < 0)
			startcol = 0;
		if (startcol > 0)
			startcol--;
		if (startcol >= maxCOLS)
			goto no_draw;
		ncols = (width / ss->char_width) + 2;
		if (startcol + ncols > maxCOLS)
			ncols = maxCOLS - startcol;
		while ((ROWCOL_TO_BA(startrow, startcol) % maxCOLS) + ncols > maxCOLS)
			ncols--;
		for (row = startrow; row < endrow; row++) {
			(void) memset((char *) &ss->image[ROWCOL_TO_BA(row, startcol)],
			      0, ncols * sizeof(union sp));
			if (visible_control) {
				c0 = ROWCOL_TO_BA(row, startcol);

				for (i = 0; i < ncols; i++)
					ss->image[c0 + i].bits.cc = EBC_space;
			}
		}
	    no_draw:
		;
					    
	} else {
		XFillRectangle(display, ss->window,
		    get_gc(ss, INVERT_COLOR(0)),
		    0, 0, ss->screen_width, ss->screen_height);
		(void) memset((char *) ss->image, 0,
		              (maxROWS*maxCOLS) * sizeof(union sp));
		if (visible_control)
			for (i = 0; i < maxROWS*maxCOLS; i++)
				ss->image[i].bits.cc = EBC_space;
		ss->copied = False;
	}
	ctlr_changed(0, ROWS*COLS);
	cursor_changed = True;
	if (!appres.active_icon || !iconic) {
		line_changed = True;
		status_touch();
	}
}

/*
 * Explicitly redraw the screen (invoked from the keyboard).
 */
void
Redraw_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Redraw_action, event, params, num_params);
	do_redraw(w, event, params, num_params);
}

/*
 * Implicitly redraw the screen (triggered by Expose events).
 */
void
PA_Expose_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_Expose_action, event, params, num_params);
#endif /*]*/
	do_redraw(w, event, params, num_params);
}


/*
 * Redraw the changed parts of the screen.
 */

void
screen_disp(Boolean erasing)
{
	/* No point in doing anything if we aren't visible yet. */
	if (!ss->exposed_yet)
		return;

	/*
	 * We don't set "cursor_changed" when the host moves the cursor,
	 * 'cause he might just move it back later.  Set it here if the cursor
	 * has moved since the last call to screen_disp.
	 */
	if (cursor_addr != ss->cursor_daddr)
		cursor_changed = True;

#if defined(X3270_DBCS) /*[*/
	/* If the cursor has moved, tell the input method. */
	if (cursor_changed && ic != NULL &&
			style == (XIMPreeditPosition|XIMStatusNothing)) {
#if defined(_ST) /*[*/
		printf("spot_loc%s\n", rcba(cursor_addr));
#endif /*]*/
		send_spot_loc();
	}
#endif /*]*/

	/*
	 * If only the cursor has changed (and not the screen image), draw it.
	 */
	if (cursor_changed && !screen_changed) {
		if (cursor_off())
			cursor_on();
		if (toggled(CROSSHAIR))
			screen_changed = True; /* repaint crosshair */
	}

	/*
	 * Redraw the parts of the screen that need refreshing, and redraw the
	 * cursor if necessary.
	 */
	if (screen_changed) {
		Boolean	was_on = False;

		/* Draw the new screen image into "temp_image" */
		if (screen_changed) {
			if (erasing)
				crosshair_enabled = False;
			draw_fields(temp_image, first_changed, last_changed);
			if (erasing)
				crosshair_enabled = True;
		}

		/* Set "cursor_changed" if the text under it has changed. */
		if (ss->image[fl_baddr(cursor_addr)].word !=
		    temp_image[fl_baddr(cursor_addr)].word)
			cursor_changed = True;

		/* Undraw the cursor, if necessary. */
		if (cursor_changed)
			was_on = cursor_off();

		/* Intelligently update the X display with the new text. */
		resync_display(temp_image, first_changed, last_changed);

		/* Redraw the cursor. */
		if (was_on)
			cursor_on();

		screen_changed = False;
		first_changed = -1;
		last_changed = -1;
	}

	if (!appres.active_icon || !iconic) {
		/* Refresh the status line. */
		status_disp();

		/* Refresh the line across the bottom of the screen. */
		if (line_changed) {
			XDrawLine(display, ss->window,
			    get_gc(ss, GC_NONDEFAULT | DEFAULT_PIXEL),
			    0,
			    ssROW_TO_Y(maxROWS-1)+SGAP-1,
			    ssCOL_TO_X(maxCOLS)+hhalo,
			    ssROW_TO_Y(maxROWS-1)+SGAP-1);
			line_changed = False;
		}
	}
	draw_aicon_label();
}


/*
 * Render a blank rectangle on the X display.
 */
static void
render_blanks(int baddr, int height, union sp *buffer)
{
	int x, y;

#if defined(_ST) /*[*/
	(void) printf("render_blanks(baddr=%s, height=%d)\n", rcba(baddr),
	    height);
#endif /*]*/

	x = ssCOL_TO_X(BA_TO_COL(baddr));
	y = ssROW_TO_Y(BA_TO_ROW(baddr));

	XFillRectangle(display, ss->window,
	    get_gc(ss, INVERT_COLOR(0)),
	    x, y - ss->ascent,
	    (ss->char_width * COLS) + 1, (ss->char_height * height));

	(void) memmove(&ss->image[baddr], &buffer[baddr],
	                   COLS * height *sizeof(union sp));
}

/*
 * Check if a region of the screen is effectively empty: all blanks or nulls
 * with no extended highlighting attributes and not selected.
 *
 * Works _only_ with non-debug fonts.
 */
static Boolean
empty_space(register union sp *buffer, int len)
{
	register int i;

	for (i = 0; i < len; i++, buffer++) {
		if (buffer->bits.gr ||
		    buffer->bits.sel ||
		    (buffer->bits.fg & INVERT_MASK) ||
		    (buffer->bits.cs != CS_BASE) ||
		    !BKM_ISSET(buffer->bits.cc))
			return False;
	}
	return True;
}


/*
 * Reconcile the differences between a region of 'buffer' (what we want on
 * the X display) and ss->image[] (what is on the X display now).  The region
 * must not span lines.
 */
static void
resync_text(int baddr, int len, union sp *buffer)
{
	static Boolean ever = False;
	static unsigned long cmask = 0L;
	static unsigned long gmask = 0L;

#if defined(_ST) /*[*/
	(void) printf("resync_text(baddr=%s, len=%d)\n", rcba(baddr), len);
#endif /*]*/

	/*
	 * If the region begins on the right half of a DBCS character, back
	 * up one.
	 */
	if (baddr % COLS) {
		enum dbcs_state d;

		d = ctlr_dbcs_state(baddr);
		if (IS_RIGHT(d)) {
			baddr--;
			len++;
		}
	}

	if (!ever) {
		union sp b;

		/* Create masks for the "important" fields in an sp. */
		b.word = 0L;
		b.bits.fg = COLOR_MASK | INVERT_MASK;
		b.bits.sel = 1;
		b.bits.gr = GR_UNDERLINE | GR_INTENSIFY;
		cmask = b.word;

		b.word = 0L;
		b.bits.fg = INVERT_MASK;
		b.bits.sel = 1;
		b.bits.gr = 0xf;
		gmask = b.word;

		ever = True;
	}

	if (!visible_control &&
	    len > 1 &&
	    empty_space(&buffer[baddr], len)) {
		int x, y;

		x = ssCOL_TO_X(BA_TO_COL(baddr));
		y = ssROW_TO_Y(BA_TO_ROW(baddr));

		/* All empty, fill a rectangle */
#if defined(_ST) /*[*/
		(void) printf("FillRectangle(baddr=%s, len=%d)\n", rcba(baddr),
		    len);
#endif /*]*/
		XFillRectangle(display, ss->window,
		    get_gc(ss, INVERT_COLOR(0)),
		    x, y - ss->ascent,
		    (ss->char_width * len) + 1, ss->char_height);
	} else {
		unsigned long attrs, attrs2;
		Boolean has_gr, has_gr2;
		Boolean empty, empty2;
		union sp ra;
		int i;
		int i0 = 0;

		ra = buffer[baddr];

		/* Note the characteristics of the beginning of the region. */
		attrs = buffer[baddr].word & cmask;
		has_gr = (buffer[baddr].word & gmask) != 0;
		empty = !has_gr && BKM_ISSET(buffer[baddr].bits.cc);

		for (i = 0; i < len; i++) {
			/* Note the characteristics of this character. */
			attrs2 = buffer[baddr+i].word & cmask;
			has_gr2 = (buffer[baddr+i].word & gmask) != 0;
			empty2 = !has_gr2 && BKM_ISSET(buffer[baddr+i].bits.cc);

			/* If this character has exactly the same attributes
			   as the current region, simply add it, noting that
			   the region might now not be empty. */
			if (attrs2 == attrs) {
				if (!empty2)
					empty = 0;
				continue;
			}

			/* If this character is empty, and the current region
			   has no GR attributes, pretend it matches. */
			if (empty2 && !has_gr)
				continue;

			/* If the current region is empty, this character
			   isn't empty, and this character has no GR
			   attributes, change the current region's attributes
			   to this character's attributes and add it. */
			if (empty && !empty2 && !has_gr2) {
				attrs = attrs2;
				has_gr = has_gr2;
				empty = empty2;
				ra = buffer[baddr+i];
				continue;
			}

			/* Dump the region and start a new one with this
			   character. */
#if defined(_ST) /*[*/
			printf("%s:%d: rt%s\n", __FUNCTION__, __LINE__,
			    rcba(baddr+i0));
#endif /*]*/
			render_text(&buffer[baddr+i0], baddr+i0, i - i0, False,
			    &ra);
			attrs = attrs2;
			has_gr = has_gr2;
			empty = empty2;
			i0 = i;
			ra = buffer[baddr+i];
		}

		/* Dump the remainder of the region. */
#if defined(_ST) /*[*/
		printf("%s:%d: rt%s\n", __FUNCTION__, __LINE__, rcba(baddr+i0));
#endif /*]*/
		render_text(&buffer[baddr+i0], baddr+i0, len - i0, False, &ra);
	}

	/* The X display is now correct; update ss->image[]. */
	(void) memmove(&ss->image[baddr], &buffer[baddr], len*sizeof(union sp));
}

/*
 * Get a font index for an EBCDIC character.
 * Returns a blank if there is no mapping.
 *
 * Note that the EBCDIC character can be 16 bits (DBCS), and the output might
 * be 16 bits as well.
 */
static unsigned short
font_index(ebc_t ebc, int d8_ix, Boolean upper)
{
	ucs4_t ucs4;
	int d;

	ucs4 = ebcdic_base_to_unicode(ebc, True, True);
	if (upper && ucs4 < 0x80 && islower(ucs4))
	    	ucs4 = toupper(ucs4);
	d = display8_lookup(d8_ix, ucs4);
	if (d < 0) {
	    	d = display8_lookup(d8_ix, ' ');
	}
	return d;
}

/*
 * Attempt to map an APL character to a DEC line-drawing character in the
 * first 32 bytes of an old 8-bit X11 font.
 */
static int
apl_to_linedraw(ebc_t c)
{
    	switch (c) {
	case 0xaf:	/* degree */
	    return 0x7;
	case 0xd4:	/* LR corner */
	    return 0xb;
	case 0xd5:	/* UR corner */
	    return 0xc;
	case 0xc5:	/* UL corner */
	    return 0xd;
	case 0xc4:	/* LL corner */
	    return 0xe;
	case 0xd3:	/* plus */
	    return 0xf;
	case 0xa2:	/* middle horizontal */
	    return 0x12;
	case 0xc6:	/* left tee */
	    return 0x15;
	case 0xd6:	/* right tee */
	    return 0x16;
	case 0xc7:	/* bottom tee */
	    return 0x17;
	case 0xd7:	/* top tee */
	    return 0x18;
	case 0xbf:	/* stile */
	case 0x85:	/* vertical line */
	    return 0x19;
	case 0x8c:	/* less or equal */
	    return 0x1a;
	case 0xae:	/* greater or equal */
	    return 0x1b;
	case 0xbe:	/* not equal */
	    return 0x1d;
	case 0xa3:	/* bullet */
	    return 0x1f;
	case 0xad:
	    return '[';
	case 0xbd:
	    return ']';
	default:
	    return -1;
	}
}

/* Map an APL character to the current display character set. */
static XChar2b
apl_to_udisplay(int d8_ix, unsigned char c)
{
	XChar2b x;
	int u = -1;
	int d = 0;

	/* Look it up. */
	u = apl_to_unicode(c);
	if (u != -1)
	    	d = display8_lookup(d8_ix, u);

	/* Default to a space. */
	if (d == 0)
		d = display8_lookup(d8_ix, ' ');

	/* Return it. */
	x.byte1 = (d >> 8) & 0xff;
	x.byte2 = d & 0xff;
	return x;
}

/* Map an APL character to the old first-32 8-bit X11 display character set. */
static XChar2b
apl_to_ldisplay(unsigned char c)
{
	XChar2b x;
	int u = -1;

	/* Look it up, defaulting to a space. */
	u = apl_to_linedraw(c);
	if (u == -1)
	    	u = ' ';

	/* Return it. */
	x.byte1 = 0;
	x.byte2 = u;
	return x;
}

/* Map a line-drawing character to the current display character set. */
static XChar2b
linedraw_to_udisplay(int d8_ix, unsigned char c)
{
	XChar2b x;
	int u = -1;
	int d = 0;

	/* Look it up. */
	u = linedraw_to_unicode(c);
	if (u != -1)
	    	d = display8_lookup(d8_ix, u);

	/* Default to a space. */
	if (d == 0)
		d = display8_lookup(d8_ix, ' ');

	/* Return it. */
	x.byte1 = (d >> 8) & 0xff;
	x.byte2 = d & 0xff;
	return x;
}

/*
 * Render text onto the X display.  The region must not span lines.
 */
static void
render_text(union sp *buffer, int baddr, int len, Boolean block_cursor,
    union sp *attrs)
{
	int color;
	int x, y;
	GC dgc = (GC)None;	/* drawing text */
	GC cleargc = (GC)None;	/* clearing under undersized characters */
	int sel = attrs->bits.sel;
	register int i, j;
	Boolean one_at_a_time = False;
	int d8_ix = ss->d8_ix;
	XTextItem16 text[64]; /* fixed size is a hack */
	int n_texts = -1;
	Boolean in_dbcs = False;
	int clear_len = 0;
	int n_sbcs = 0;
#if defined(X3270_DBCS) /*[*/
	int n_dbcs = 0;
#endif /*]*/

#if defined(_ST) /*[*/
	(void) printf("render_text(baddr=%s, len=%d)\n", rcba(baddr), len);
#endif /*]*/

	/*
	 * If the region starts with the right-hand side of a DBCS, back off
	 * one column.
	 */
	switch (ctlr_dbcs_state(baddr)) {
	case DBCS_RIGHT:
	    /*
	     * Lots of assumptions -- the buffer really does go back one byte,
	     * and baddr is greater than zero.
	     */
#if defined(_ST) /*[*/
	    (void) printf("render_text: backing off\n");
#endif /*]*/
	    buffer--;
	    baddr--;
	    len++;
	    break;
	default:
	    break;
	}

	for (i = 0, j = 0; i < len; i++) {
#if defined(X3270_DBCS) /*[*/
		if (buffer[i].bits.cs != CS_DBCS || !dbcs || iconic) {
#endif /*]*/
			if (n_texts < 0 || in_dbcs) {
				/* Switch from nothing or DBCS, to SBCS. */
#if defined(_ST) /*[*/
				fprintf(stderr, "SBCS starts at %s\n",
				    rcba(baddr + i));
#endif /*]*/
				in_dbcs = False;
				n_texts++;
				text[n_texts].chars = &rt_buf[j];
				text[n_texts].nchars = 0;
				text[n_texts].delta = 0;
				text[n_texts].font = ss->fid;
				n_sbcs++;
			}
			/* In SBCS. */
			clear_len += ss->char_width;
#if defined(X3270_DBCS) /*[*/
		} else {
			if (n_texts < 0 || !in_dbcs) {
				/* Switch from nothing or SBCS, to DBCS. */
#if defined(_ST) /*[*/
				fprintf(stderr, "DBCS starts at %s\n",
				    rcba(baddr + i));
#endif /*]*/
				in_dbcs = True;
				n_texts++;
				text[n_texts].chars = &rt_buf[j];
				text[n_texts].nchars = 0;
				text[n_texts].delta = 0;
				text[n_texts].font = dbcs_font.font;
				n_dbcs++;
			}
			/* In DBCS. */
			clear_len += 2 * ss->char_width;
		}
#endif /*]*/

		switch (buffer[i].bits.cs) {
		    case CS_BASE:	/* latin-1 */
			rt_buf[j].byte1 = 0;
			if (toggled(MONOCASE))
				rt_buf[j].byte2 =
				    font_index(buffer[i].bits.cc, d8_ix, True);
			else {
				if (visible_control) {
					if (buffer[i].bits.cc == EBC_so) {
						rt_buf[j].byte1 = 0;
						rt_buf[j].byte2 =
						    font_index(EBC_less, d8_ix,
							    False);
					} else if (buffer[i].bits.cc == EBC_si) {
						rt_buf[j].byte1 = 0;
						rt_buf[j].byte2 =
						    font_index(EBC_greater,
							    d8_ix, False);
					} else {
					    	unsigned short c = font_index(
							buffer[i].bits.cc,
							d8_ix, False);

						rt_buf[j].byte1 =
						    (c >> 8) & 0xff;
						rt_buf[j].byte2 = c & 0xff;
					}
				} else {
				    	unsigned short c = font_index(
						buffer[i].bits.cc, d8_ix,
						False);

					rt_buf[j].byte1 =
					    (c >> 8) & 0xff;
					rt_buf[j].byte2 = c & 0xff;
				}
			}
			j++;
			break;
		    case CS_APL:	/* GE (apl) */
		    case CS_BASE | CS_GE:
			if (ss->extended_3270font) {
				rt_buf[j].byte1 = 1;
				rt_buf[j].byte2 = ebc2cg0[buffer[i].bits.cc];
			} else {
			    	if (ss->font_16bit) {
				    	rt_buf[j] = apl_to_udisplay(d8_ix,
						buffer[i].bits.cc);
				} else {
				    	rt_buf[j] = apl_to_ldisplay(
						buffer[i].bits.cc);
				}
			}
			j++;
			break;
		    case CS_LINEDRAW:	/* DEC line drawing */
			if (ss->standard_font) {
			    	if (ss->font_16bit) {
				    	rt_buf[j] = linedraw_to_udisplay(
						d8_ix, buffer[i].bits.cc);
				} else {
				    	/*
					 * Assume the first 32 characters are
					 * line-drawing.
					 */
					rt_buf[j].byte1 = 0;
					rt_buf[j].byte2 = buffer[i].bits.cc;
				}
			} else {
				if (ss->extended_3270font) {
					rt_buf[j].byte1 = 2;
					rt_buf[j].byte2 = buffer[i].bits.cc;
				} else {
					rt_buf[j].byte1 = 0;
					rt_buf[j].byte2 = 0;
				}
			}
			j++;
			break;
		    case CS_DBCS:	/* DBCS */
#if defined(X3270_DBCS) /*[*/
			if (dbcs) {
				xlate_dbcs(buffer[i].bits.cc,
					   buffer[i+1].bits.cc,
					   &rt_buf[j]);
				/* Skip the next byte as well. */
				i++;
			} else {
				rt_buf[j].byte1 = 0;
				rt_buf[j].byte2 = font_index(EBC_space, d8_ix,
					False);
			}
#else /*][*/
			rt_buf[j].byte1 = 0;
			rt_buf[j].byte2 = font_index(EBC_space, d8_ix, False);
#endif /*]*/
			j++;
			break;
		}
		text[n_texts].nchars++;
	}
	n_texts++;

	/* Check for one-at-a-time mode. */
	if (ss->funky_font) {
		for (i = 0; i < len; i++) {
			if (!rt_buf[i].byte1 &&
			    (IS_ODD(ss->odd_width, rt_buf[i].byte2) ||
			     IS_ODD(ss->odd_lbearing, rt_buf[i].byte2))) {
				one_at_a_time = True;
				break;
			}
		}
	}

	x = ssCOL_TO_X(BA_TO_COL(baddr));
	y = ssROW_TO_Y(BA_TO_ROW(baddr));
	color = attrs->bits.fg;

	/* Select the GCs. */
	if (sel && !block_cursor) {
		/* Selected, but not a block cursor. */
		if (!appres.mono) {
			/* Color: Use the special select GCs. */
			dgc = get_selgc(ss, color);
			cleargc = ss->clrselgc;
		} else {
			/* Mono: Invert the color. */
			dgc = get_gc(ss, INVERT_COLOR(color));
			cleargc = get_gc(ss, color);
		}
	} else if (block_cursor && !(appres.mono && sel)) {
		/* Block cursor, but neither mono nor selected. */
		if (appres.use_cursor_color) {
			/* Use the specific-color inverted GC. */
			dgc = ss->invucgc;
			cleargc = ss->ucgc;
		} else {
			/* Just invert the specified color. */
			dgc = get_gc(ss, INVERT_COLOR(color));
			cleargc = get_gc(ss, color);
		}
	} else {
		/* Ordinary text, or a selected block cursor. */
		dgc = get_gc(ss, color);
		cleargc = get_gc(ss, INVERT_COLOR(color));
	}

	/* Draw the text */
	XFillRectangle(display, ss->window,
		cleargc,
		x, y - ss->ascent,
		clear_len, ss->char_height);
#if defined(_ST) /*[*/
	{
		int k, l;

		for (k = 0; k < n_texts; k++) {
			printf("text[%d]: %d chars, %s:", k,
			    text[k].nchars,
			    (text[k].font == dbcs_font.font)?
				"dbcs": "sbcs");
			for (l = 0; l < text[k].nchars; l++) {
			    printf(" %02x%02x", text[k].chars[l].byte1,
				text[k].chars[l].byte2);
			}
			printf("\n");
		}
	}
#endif /*]*/
	if (one_at_a_time ||
	    (n_sbcs && ss->xtra_width)
#if defined(X3270_DBCS) /*[*/
	     ||
	    (n_dbcs && dbcs_font.xtra_width)
#endif /*]*/
	   ) {
		int i, j;
		int xn = x;
		XTextItem16 text1;

		/* XXX: do overstrike */
		for (i = 0; i < n_texts; i++) {
#if defined(X3270_DBCS) /*[*/
			if (one_at_a_time || text[i].font == ss->fid) {
#endif /*]*/
				if (one_at_a_time || ss->xtra_width) {
					for (j = 0; j < text[i].nchars;
					     j++) {
						text1.chars =
						    &text[i].chars[j];
						text1.nchars = 1;
						text1.delta = 0;
						text1.font = ss->fid;
						XDrawText16(
						    display,
						    ss->window, dgc,
						    xn, y, &text1,
						    1);
						xn += ss->char_width;
					}
				} else {
					XDrawText16(display,
					    ss->window, dgc, xn, y,
					    &text[i], 1);
					xn += ss->char_width *
						text[i].nchars;
				}
#if defined(X3270_DBCS) /*[*/
			} else {
				if (dbcs_font.xtra_width) {
					for (j = 0; j < text[i].nchars;
					     j++) {
						text1.chars =
						    &text[i].chars[j];
						text1.nchars = 1;
						text1.delta = 0;
						text1.font =
						    dbcs_font.font;
						XDrawText16(
						    display,
						    ss->window, dgc,
						    xn,
						    y,
						    &text1,
						    1);
						xn += dbcs_font.char_width;
					}
				} else {
					XDrawText16(display,
					    ss->window, dgc, xn,
					    y,
					    &text[i], 1);
					xn += dbcs_font.char_width *
						text[i].nchars;
				}
			}
#endif /*]*/
		}
	} else {
		XDrawText16(display, ss->window, dgc, x, y, text, n_texts);
		if (ss->overstrike &&
		    ((attrs->bits.gr & GR_INTENSIFY) ||
		     ((appres.mono || (!appres.m3279 && appres.highlight_bold)) &&
		      ((color & BASE_MASK) == FA_INT_HIGH_SEL)))) {
			XDrawText16(display, ss->window, dgc, x+1, y,
			    text, n_texts);
		}
	}

	if (attrs->bits.gr & GR_UNDERLINE)
		XDrawLine(display,
		    ss->window,
		    dgc,
		    x,
		    y - ss->ascent + ss->char_height - 1,
		    x + clear_len,
		    y - ss->ascent + ss->char_height - 1);
}


#if defined(X3270_ANSI) /*[*/
Boolean
screen_obscured(void)
{
	return ss->obscured;
}

/*
 * Scroll the screen image one row.
 *
 * This is the optimized path from ctlr_scroll(); it assumes that ea_buf[] has
 * already been modified and that the screen can be brought into sync by
 * hammering ss->image and the bitmap.
 */
void
screen_scroll(void)
{
	Boolean was_on;

	if (!ss->exposed_yet)
		return;

	was_on = cursor_off();
	(void) memmove(&ss->image[0], &ss->image[COLS],
	                   (ROWS - 1) * COLS * sizeof(union sp));
	(void) memmove(&temp_image[0], &temp_image[COLS],
	                   (ROWS - 1) * COLS * sizeof(union sp));
	(void) memset((char *)&ss->image[(ROWS - 1) * COLS], 0,
	              COLS * sizeof(union sp));
	(void) memset((char *)&temp_image[(ROWS - 1) * COLS], 0,
	              COLS * sizeof(union sp));
	XCopyArea(display, ss->window, ss->window, get_gc(ss, 0),
	    ssCOL_TO_X(0),
	    ssROW_TO_Y(1) - ss->ascent,
	    ss->char_width * COLS,
	    ss->char_height * (ROWS - 1),
	    ssCOL_TO_X(0),
	    ssROW_TO_Y(0) - ss->ascent);
	ss->copied = True;
	XFillRectangle(display, ss->window, get_gc(ss, INVERT_COLOR(0)),
	    ssCOL_TO_X(0),
	    ssROW_TO_Y(ROWS - 1) - ss->ascent,
	    (ss->char_width * COLS) + 1,
	    ss->char_height);
	if (was_on)
		cursor_on();
}
#endif /*]*/


/*
 * Toggle mono-/dual-case mode.
 */
void
toggle_monocase(struct toggle *t _is_unused, enum toggle_type tt _is_unused)
{
	(void) memset((char *) ss->image, 0,
		      (ROWS*COLS) * sizeof(union sp));
	ctlr_changed(0, ROWS*COLS);
}

/*
 * Toggle screen flip
 */
void
screen_flip(void)
{
	/* Flip mode is broken in the DBCS version. */
#if !defined(X3270_DBCS) /*[*/
	flipped = !flipped;

	action_internal(PA_Expose_action, IA_REDRAW, CN, CN);
#endif /*]*/
}

/*
 * "Draw" ea_buf into a buffer
 */
static void
draw_fields(union sp *buffer, int first, int last)
{
	int	baddr = 0;
	int	faddr;
	unsigned char	fa;
	struct ea       *field_ea;
	struct ea	*sbp = ea_buf;
	int	field_color;
	int	zero;
	Boolean	any_blink = False;
	int	crossable = CROSSABLE;
	enum dbcs_state d;
	int	cursor_col, cursor_row;

	/* Set up cursor_col/cursor_row. */
	if (crossable) {
		cursor_col = BA_TO_COL(cursor_addr);
		cursor_row = BA_TO_ROW(cursor_addr);
	}

	/* If there is any blinking text, override the suggested boundaries. */
	if (text_blinkers_exist) {
		first = -1;
		last = -1;
	}

	/* Adjust pointers to start of region. */
	if (first > 0) {
		baddr += first;
		sbp += first;
		buffer += first;
	}
	faddr = find_field_attribute(baddr);
	fa = ea_buf[faddr].fa;
	field_ea = fa2ea(faddr);

	/* Adjust end of region. */
	if (last == -1 || last >= ROWS*COLS)
		last = 0;

	zero = FA_IS_ZERO(fa);
	if (field_ea->fg && (!appres.modified_sel || !FA_IS_MODIFIED(fa)))
		field_color = field_ea->fg & COLOR_MASK;
	else
		field_color = fa_color(fa);

	do {
		unsigned char	c = sbp->cc;
		union sp	b;
		Boolean		reverse = False;
		Boolean		is_selected = False;
		Boolean		is_crossed = False;

		b.word = 0;	/* clear out all fields */

		if (ea_buf[baddr].fa) {
			fa = ea_buf[baddr].fa;
			field_ea = sbp;
			zero = FA_IS_ZERO(fa);
			if (field_ea->fg && (!appres.modified_sel || !FA_IS_MODIFIED(fa)))
				field_color = field_ea->fg & COLOR_MASK;
			else
				field_color = fa_color(fa);
			if (visible_control) {
				if (FA_IS_PROTECTED(fa))
					b.bits.cc = EBC_P;
				else if (FA_IS_MODIFIED(fa))
					b.bits.cc = EBC_M;
				else
					b.bits.cc = EBC_U;
				b.bits.gr = GR_UNDERLINE;
			}
		} else {
			unsigned short gr;
			int e_color;
			Boolean is_vc = False;

			/* Find the right graphic rendition. */
			gr = sbp->gr;
			if (!gr)
				gr = field_ea->gr;
			if (gr & GR_BLINK)
				any_blink = True;
			if (appres.highlight_bold && FA_IS_HIGH(fa))
				gr |= GR_INTENSIFY;

			/* Find the right color. */
			if (sbp->fg)
				e_color = sbp->fg & COLOR_MASK;
			else if (appres.mono && (gr & GR_INTENSIFY))
				e_color = fa_color(FA_INT_HIGH_SEL);
			else
				e_color = field_color;
			if (gr & GR_REVERSE) {
				e_color = INVERT_COLOR(e_color);
				reverse = True;
			}
			if (!appres.mono)
				b.bits.fg = e_color;

			/* Find the right character and character set. */
			d = ctlr_dbcs_state(baddr);
			if (zero) {
				if (visible_control)
					b.bits.cc = EBC_space;
			} else if (((!visible_control || c != EBC_null) &&
				    (c != EBC_space || d != DBCS_NONE)) ||
			           (gr & (GR_REVERSE | GR_UNDERLINE)) ||
				   visible_control) {

				b.bits.fg = e_color;

				/*
				 * Replace blanked-out blinking text with
				 * spaces.
				 */
				if (!text_blinking_on && (gr & GR_BLINK))
					b.bits.cc = EBC_space;
				else {
					if (visible_control && c == EBC_null) {
						b.bits.cc = EBC_period;
						is_vc = True;
					} else if (visible_control &&
					    (c == EBC_so || c == EBC_si)) {
						b.bits.cc = (c == EBC_so)?
						    EBC_less:
						    EBC_greater;
						is_vc = True;
					} else
						b.bits.cc = c;
					if (sbp->cs)
						b.bits.cs = sbp->cs;
					else
						b.bits.cs = field_ea->cs;
					if (b.bits.cs & CS_GE)
						b.bits.cs = CS_APL;
					else if ((b.bits.cs & CS_MASK) !=
						    CS_DBCS ||
						 d != DBCS_NONE)
						b.bits.cs &= CS_MASK;
					else
						b.bits.cs = CS_BASE;
				}

			} /* otherwise, EBC_null */

			if (visible_control) {
				if (is_vc)
					b.bits.gr = GR_UNDERLINE;
			} else {
				b.bits.gr = gr & (GR_UNDERLINE | GR_INTENSIFY);
			}

			/* Check for SI/SO. */
			if (d == DBCS_LEFT || d == DBCS_RIGHT)
				b.bits.cs = CS_DBCS;
		}

		/*
		 * Compute selection state.
		 *
		 * DBCS characters always act as a unit, with the state
		 * determined by the selection status and crosshair
		 * intersection of either half.
		 * - If either half is selected, both are considered selected.
		 * - If either half lies in the crosshair, neither is
		 *   considered selected.
		 */

		is_selected = (SELECTED(baddr) != 0);
		switch (ctlr_dbcs_state(baddr)) {
		case DBCS_NONE:
		case DBCS_DEAD:
		case DBCS_LEFT_WRAP:
		case DBCS_RIGHT_WRAP:
		    break;
		case DBCS_LEFT:
		case DBCS_SI:
		    if ((baddr % COLS) != (COLS - 1) &&
			SELECTED(baddr + 1))
			    is_selected = True;
		    break;
		case DBCS_RIGHT:
		case DBCS_SB: /* XXX */
		    if ((baddr % COLS) && SELECTED(baddr - 1))
			    is_selected = True;
		    break;
		}

		if (crossable && !reverse) {
			is_crossed = CROSSED(baddr);
			switch (ctlr_dbcs_state(baddr)) {
			case DBCS_NONE:
			case DBCS_DEAD:
			case DBCS_LEFT_WRAP:
			case DBCS_RIGHT_WRAP:
			    break;
			case DBCS_LEFT:
			case DBCS_SI:
			    if ((baddr % COLS) != (COLS - 1) &&
				CROSSED(baddr + 1))
				    is_crossed = True;
			    break;
			case DBCS_RIGHT:
			case DBCS_SB: /* XXX */
			    if ((baddr % COLS) && CROSSED(baddr - 1))
				    is_crossed = True;
			    break;
			}
		}

		/*
		 * XOR the crosshair cursor with selections.
		 */
		if (crossable) {
			if (is_selected ^ is_crossed)
				b.bits.sel = 1;
		} else {
			if (is_selected)
				b.bits.sel = 1;
		}

		if (!flipped)
			*buffer++ = b;
		else
			*(buffer + fl_baddr(baddr)) = b;
		sbp++;
		INC_BA(baddr);
	} while (baddr != last);

	/* Cancel blink timeouts if none were seen this pass. */
	if (!any_blink)
		text_blinkers_exist = False;
}


/*
 * Resync the X display with the contents of 'buffer'
 */
static void
resync_display(union sp *buffer, int first, int last)
{
	register int	i, j;
	int		b = 0;
	int		i0 = -1;
	Boolean		ccheck;
	int		fca = fl_baddr(cursor_addr);
	int		first_row, last_row;
#	define SPREAD	10

	if (first < 0) {
		first_row = 0;
		last_row = ROWS;
	} else {
		first_row = first / COLS;
		b = first_row * COLS;
		last_row = (last + (COLS-1)) / COLS;
	}

	for (i = first_row; i < last_row; b += COLS, i++) {
		int d0 = -1;
		int s0 = -1;

		/* Has the line changed? */
		if (!memcmp((char *) &ss->image[b], (char *) &buffer[b],
		    COLS*sizeof(union sp))) {
			if (i0 >= 0) {
				render_blanks(i0 * COLS, i - i0, buffer);
				i0 = -1;
			}
			continue;
		}

		/* Is the new value empty? */
		if (!visible_control &&
		    !(fca >= b && fca < (b+COLS)) &&
		    empty_space(&buffer[b], COLS)) {
			if (i0 < 0)
				i0 = i;
			continue;
		}

		/* Yes, it changed, and it isn't blank.
		   Dump any pending blank lines. */
		if (i0 >= 0) {
			render_blanks(i0 * COLS, i - i0, buffer);
			i0 = -1;
		}

		/* New text.  Scan it. */
		ccheck = cursor_displayed &&
			 fca >= b &&
			 fca < (b+COLS);
		for (j = 0; j < COLS; j++) {
			if (ccheck && b+j == fca) {
				/* Don't repaint over the cursor. */

				/* Dump any pending "different" characters. */
				if (d0 >= 0)
					resync_text(b+d0, j-d0, buffer);

				/* Start over. */
				d0 = -1;
				s0 = -1;
				continue;
			}
			if (ss->image[b+j].word == buffer[b+j].word) {

				/* Character is the same. */

				if (d0 >= 0) {
					/* Something is pending... */
					if (s0 < 0) {
						/* Start of "same" area */
						s0 = j;
					} else {
						/* nth matching character */
						if (j - s0 > SPREAD) {
							/* too many */
							resync_text(b+d0,
							    s0-d0, buffer);
							d0 = -1;
							s0 = -1;
						}
					}
				}
			} else {

				/* Character is different. */

				/* Forget intermediate matches. */
				s0 = -1;

				if (d0 < 0)
					/* Mark the start. */
					d0 = j;
			}
		}

		/* Dump any pending "different" characters. */
		if (d0 >= 0)
			resync_text(b+d0, COLS-d0, buffer);
	}
	if (i0 >= 0)
		render_blanks(i0 * COLS, last_row - i0, buffer);
}


/*
 * Support code for cursor redraw.
 */

/*
 * Calculate a flipped baddr.
 */
static int
fl_baddr(int baddr)
{
	if (!flipped)
		return baddr;
	return ((baddr / COLS) * COLS) + (COLS - (baddr % COLS) - 1);
}

/*
 * Return the proper foreground color for a character position.
 */

static int
char_color(int baddr)
{
	int faddr;
	unsigned char fa;
	int color;

	faddr = find_field_attribute(baddr);
	fa = ea_buf[faddr].fa;

	/*
	 * Find the color of the character or the field.
	 */
	if (ea_buf[baddr].fg)
		color = ea_buf[baddr].fg & COLOR_MASK;
	else if (fa2ea(faddr)->fg && (!appres.modified_sel || !FA_IS_MODIFIED(fa)))
		color = fa2ea(faddr)->fg & COLOR_MASK;
	else
		color = fa_color(fa);

	/*
	 * Now apply reverse video.
	 *
	 * One bit of strangeness:
	 *  If the buffer is a field attribute and we aren't using the
	 *  debug font, it's displayed as a blank; don't invert.
	 */
	if (!((ea_buf[baddr].fa && !visible_control)) &&
	    ((ea_buf[baddr].gr & GR_REVERSE) || (fa2ea(faddr)->gr & GR_REVERSE)))
		color = INVERT_COLOR(color);

	/*
	 * In monochrome, apply selection status as well.
	 */
	if (appres.mono && SELECTED(baddr))
		color = INVERT_COLOR(color);

	return color;
}


/*
 * Select a GC for drawing a hollow or underscore cursor.
 */
static GC
cursor_gc(int baddr)
{
	/*
	 * If they say use a particular color, use it.
	 */
	if (appres.use_cursor_color)
		return ss->ucgc;
	else
		return get_gc(ss, char_color(baddr));
}

/*
 * Redraw one character.
 * If 'invert' is true, invert the foreground and background colors.
 */
static void
redraw_char(int baddr, Boolean invert)
{
	enum dbcs_state d;
	union sp buffer[2];
	int faddr;
	unsigned char fa;
	int gr;
	int blank_it = 0;
	int baddr2;
	int len = 1;

	/*
	 * Figure out the DBCS state of this position.  If it's the right-hand
	 * side of a DBCS character, repaint the left side instead.
	 */
	switch ((d = ctlr_dbcs_state(baddr))) {
	case DBCS_LEFT:
	case DBCS_SI:
	    len = 2;
	    break;
	case DBCS_RIGHT:
	    len = 2;
	    DEC_BA(baddr);
	    break;
	default:
	    break;
	}

	if (!invert) {
		int flb = fl_baddr(baddr);

		/*
		 * Put back what belongs there.
		 * Note that the cursor may have been covering a DBCS character that is no longer
		 * DBCS, so if we're not at the right margin, we should redraw two positions.
		 */
#if defined(_ST) /*[*/
		printf("%s:%d: rt%s\n", __FUNCTION__, __LINE__, rcba(flb));
#endif /*]*/
#if defined(X3270_DBCS) /*[*/
		if (dbcs && ((baddr % COLS) != (COLS - 1)) && len == 1)
			len = 2;
#endif /*]*/
		render_text(&ss->image[flb], flb, len, False, &ss->image[flb]);
		return;
	}

	baddr2 = baddr;
	INC_BA(baddr2);

	/*
	 * Fabricate the right thing.
	 * ss->image isn't going to help, because it may contain shortcuts
	 *  for faster display, so we have to construct a buffer to use.
	 */
	buffer[0].word = 0L;
	buffer[0].bits.cc = ea_buf[baddr].cc;
	buffer[0].bits.cs = ea_buf[baddr].cs;
	if (buffer[0].bits.cs & CS_GE)
		buffer[0].bits.cs = CS_APL;
	else
		buffer[0].bits.cs &= CS_MASK;

	faddr = find_field_attribute(baddr);
	if (d == DBCS_LEFT || d == DBCS_RIGHT)
		buffer[0].bits.cs = CS_DBCS;
	fa = ea_buf[faddr].fa;
	gr = ea_buf[baddr].gr;
	if (!gr)
		gr = fa2ea(faddr)->gr;
	if (ea_buf[baddr].fa) {
		if (!visible_control)
			blank_it = 1;
	} else if (FA_IS_ZERO(fa)) {
		blank_it = 1;
	} else if (text_blinkers_exist && !text_blinking_on) {
		if (gr & GR_BLINK)
			blank_it = 1;
	}
	if (blank_it) {
		buffer[0].bits.cc = EBC_space;
		buffer[0].bits.cs = 0;
	}
	buffer[0].bits.fg = char_color(baddr);
	buffer[0].bits.gr |= (gr & GR_INTENSIFY);
	if (len == 2) {
		buffer[1].word = buffer[0].word;
		if (!blank_it)
			buffer[1].bits.cc = ea_buf[baddr2].cc;
	}
	render_text(buffer, fl_baddr(baddr), len, True, buffer);

}

/*
 * Draw a hollow cursor.
 */
static void
hollow_cursor(int baddr)
{
	Dimension cwidth;
	enum dbcs_state d;

	d = ctlr_dbcs_state(baddr);

	switch (d) {
	case DBCS_RIGHT:
	    DEC_BA(baddr);
	    /* fall through... */
	case DBCS_LEFT:
	case DBCS_SI:
	    cwidth = (2 * ss->char_width) - 1;
	    break;
	default:
	    cwidth = ss->char_width - 1;
	    break;
	}

	XDrawRectangle(display,
	    ss->window,
	    cursor_gc(baddr),
	    ssCOL_TO_X(BA_TO_COL(fl_baddr(baddr))),
	    ssROW_TO_Y(BA_TO_ROW(baddr)) - ss->ascent +
		(appres.mono ? 1 : 0),
	    cwidth,
	    ss->char_height - (appres.mono ? 2 : 1));
}

/*
 * Draw an underscore cursor.
 */
static void
underscore_cursor(int baddr)
{
	Dimension cwidth;
	enum dbcs_state d;

	d = ctlr_dbcs_state(baddr);

	switch (d) {
	case DBCS_RIGHT:
	    DEC_BA(baddr);
	    /* fall through... */
	case DBCS_LEFT:
	case DBCS_SI:
	    cwidth = (2 * ss->char_width) - 1;
	    break;
	default:
	    cwidth = ss->char_width - 1;
	    break;
	}

	XDrawRectangle(display,
	    ss->window,
	    cursor_gc(baddr),
	    ssCOL_TO_X(BA_TO_COL(fl_baddr(baddr))),
	    ssROW_TO_Y(BA_TO_ROW(baddr)) - ss->ascent +
		ss->char_height - 2,
	    cwidth,
	    1);
}

/*
 * Invert a square over a character.
 */
static void
small_inv_cursor(int baddr)
{
	/* XXX: DBCS? */

	XFillRectangle(display,
	    ss->window,
	    ss->mcgc,
	    ssCOL_TO_X(BA_TO_COL(fl_baddr(baddr))),
	    ssROW_TO_Y(BA_TO_ROW(baddr)) - ss->ascent + 1,
	    ss->char_width,
	    (ss->char_height > 2) ? (ss->char_height - 2) : 1);
}

/*
 * Draw or remove the cursor.
 */
static void
put_cursor(int baddr, Boolean on)
{
	/*
	 * If the cursor is being turned off, simply redraw the text under it.
	 */
	if (!on) {
		redraw_char(baddr, False);
		return;
	}

	/*
	 * If underscore cursor, redraw the character and draw the underscore.
	 */
	if (toggled(ALT_CURSOR)) {
		redraw_char(baddr, False);
		underscore_cursor(baddr);
		return;
	}

	/*
	 * On, and not an underscore.
	 *
	 * If out of focus, either draw an empty box in its place (if block
	 * cursor) or redraw the underscore (if underscore).
	 */
	if (!in_focus) {
		hollow_cursor(baddr);
		return;
	}

	/*
	 * If monochrome, invert a small square over the characters.
	 */
	if (appres.mono) {
		small_inv_cursor(baddr);
		return;
	}

	/*
	 * Color: redraw the character in reverse video.
	 */
	redraw_char(baddr, True);
}


/* Allocate a named color. */
static Boolean
alloc_color(char *name, enum fallback_color fb_color, Pixel *pixel)
{
	XColor cell, db;
	Screen *s;

	s = XtScreen(toplevel);

	if (name[0] == '#') {
		unsigned long rgb;
		char *endptr;

		rgb = strtoul(name + 1, &endptr, 16);
		if (endptr != name + 1 && !*endptr && !(rgb & ~0xffffff)) {
			(void) memset(&db, '\0', sizeof(db));
			db.red = (rgb >> 16) & 0xff;
			db.red |= (db.red << 8);
			db.green = (rgb >> 8) & 0xff;
			db.green |= (db.green << 8);
			db.blue = rgb & 0xff;
			db.blue |= (db.blue << 8);
			if (XAllocColor(display, XDefaultColormapOfScreen(s),
						&db) != 0) {
				*pixel = db.pixel;
				return True;
			}
		}
	} else {
		if (XAllocNamedColor(display, XDefaultColormapOfScreen(s), name,
		    &cell, &db) != 0) {
			*pixel = db.pixel;
			return True;
		}
	}
	switch (fb_color) {
	    case FB_WHITE:
		*pixel = XWhitePixelOfScreen(s);
		break;
	    case FB_BLACK:
		*pixel = XBlackPixelOfScreen(s);
		break;
	}
	return False;
}

/* Spell out a fallback color. */
static const char *
fb_name(enum fallback_color fb_color)
{
	switch (fb_color) {
	    case FB_WHITE:
		return "white";
	    case FB_BLACK:
		return "black";
	}
	return "chartreuse";	/* to keep Gcc -Wall happy */
}

/* Allocate color pixels. */
static void
allocate_pixels(void)
{

	if (appres.mono)
		return;

	/* Allocate constant elements. */
	if (!alloc_color(appres.colorbg_name, FB_BLACK, &colorbg_pixel))
		popup_an_error("Cannot allocate colormap \"%s\" for screen background, using \"black\"",
		    appres.colorbg_name);
	if (!alloc_color(appres.selbg_name, FB_BLACK, &selbg_pixel))
		popup_an_error("Cannot allocate colormap \"%s\" for select background, using \"black\"",
		    appres.selbg_name);
	if (!alloc_color(appres.keypadbg_name, FB_WHITE, &keypadbg_pixel))
		popup_an_error("Cannot allocate colormap \"%s\" for keypad background, using \"white\"",
		    appres.keypadbg_name);
	if (appres.use_cursor_color) {
		if (!alloc_color(appres.cursor_color_name, FB_WHITE, &cursor_pixel))
			popup_an_error("Cannot allocate colormap \"%s\" for cursor color, using \"white\"",
			    appres.cursor_color_name);
	}

	/* Allocate pseudocolors. */
	if (!appres.m3279) {
		if (!alloc_color(appres.normal_name, FB_WHITE, &normal_pixel))
			popup_an_error("Cannot allocate colormap \"%s\" for text, using \"white\"",
			    appres.normal_name);
		if (!alloc_color(appres.select_name, FB_WHITE, &select_pixel))
			popup_an_error("Cannot allocate colormap \"%s\" for selectable text, using \"white\"",
			    appres.select_name);
		if (!alloc_color(appres.bold_name, FB_WHITE, &bold_pixel))
			popup_an_error("Cannot allocate colormap \"%s\" for bold text, using \"white\"",
			    appres.bold_name);
		return;
	}
}

#if defined(X3270_MENUS) /*[*/
/* Deallocate pixels. */
static void
destroy_pixels(void)
{
	int i;

	/*
	 * It would make sense to deallocate many of the pixels here, but
	 * the only available call (XFreeColors) would deallocate cells
	 * that may be in use by other Xt widgets.  Occh.
	 */

	for (i = 0; i < 16; i++)
		cpx_done[i] = False;
}
#endif /*]*/

/*
 * Create graphics contexts.
 */
static void
make_gcs(struct sstate *s)
{
	XGCValues xgcv;

	if (appres.m3279) {
		int i;

		for (i = 0; i < NGCS; i++) {
			if (s->gc[i] != (GC)None) {
				XtReleaseGC(toplevel, s->gc[i]);
				s->gc[i] = (GC)None;
			}
			if (s->gc[i + NGCS] != (GC)None) {
				XtReleaseGC(toplevel, s->gc[i + NGCS]);
				s->gc[i + NGCS] = (GC)None;
			}
			if (s->selgc[i] != (GC)None) {
				XtReleaseGC(toplevel, s->selgc[i]);
				s->selgc[i] = (GC)None;
			}
		}
	} else {
		if (!appres.mono) {
			make_gc_set(s, FA_INT_NORM_NSEL, normal_pixel,
			    colorbg_pixel);
			make_gc_set(s, FA_INT_NORM_SEL,  select_pixel,
			    colorbg_pixel);
			make_gc_set(s, FA_INT_HIGH_SEL,  bold_pixel,
			    colorbg_pixel);
		} else {
			make_gc_set(s, FA_INT_NORM_NSEL, appres.foreground,
			    appres.background);
			make_gc_set(s, FA_INT_NORM_SEL,  appres.foreground,
			    appres.background);
			make_gc_set(s, FA_INT_HIGH_SEL,  appres.foreground,
			    appres.background);
		}
	}
	if (s->clrselgc != (GC)None) {
		XtReleaseGC(toplevel, s->clrselgc);
		s->clrselgc = (GC)None;
	}
	xgcv.foreground = selbg_pixel;
	s->clrselgc = XtGetGC(toplevel, GCForeground, &xgcv);

	/* Create monochrome block cursor GC. */
	if (appres.mono && s->mcgc == (GC)None) {
		if (screen_depth > 1)
			xgcv.function = GXinvert;
		else
			xgcv.function = GXxor;
		xgcv.foreground = 1L;
		s->mcgc = XtGetGC(toplevel, GCForeground|GCFunction, &xgcv);
	}

	/* Create explicit cursor color cursor GCs. */
	if (appres.use_cursor_color) {
		if (s->ucgc != (GC)None) {
			XtReleaseGC(toplevel, s->ucgc);
			s->ucgc = (GC)None;
		}
		xgcv.foreground = cursor_pixel;
		s->ucgc = XtGetGC(toplevel, GCForeground, &xgcv);

		if (s->invucgc != (GC)None) {
			XtReleaseGC(toplevel, s->invucgc);
			s->invucgc = (GC)None;
		}
		xgcv.foreground = colorbg_pixel;
		xgcv.background = cursor_pixel;
		xgcv.font = s->fid;
		s->invucgc = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont, &xgcv);
	}

	/* Set the flag for overstriking bold. */
	s->overstrike = (s->char_width > 1);
}

/* Set up a default color scheme. */
static void
default_color_scheme(void)
{
	static int default_attrib_colors[4] = {
	    GC_NONDEFAULT | COLOR_GREEN,/* default */
	    GC_NONDEFAULT | COLOR_RED,	/* intensified */
	    GC_NONDEFAULT | COLOR_BLUE,	/* protected */
	    GC_NONDEFAULT | COLOR_WHITE	/* protected, intensified */
	};
	int i;

	ibm_fb = FB_WHITE;
	for (i = 0; i < 16; i++) {
		XtFree(color_name[i]);
		color_name[i] = XtNewString("white");
	}
	for (i = 0; i < 4; i++)
		field_colors[i] = default_attrib_colors[i];
}

/* Transfer the colorScheme resource into arrays. */
static Boolean
xfer_color_scheme(char *cs, Boolean do_popup)
{
	int i;
	char *scheme_name = CN;
	char *s0 = CN, *scheme = CN;
	char *tk;

	char *tmp_color_name[16];
	enum fallback_color tmp_ibm_fb = FB_WHITE;
	char *tmp_colorbg_name = CN;
	char *tmp_selbg_name = CN;
	int tmp_field_colors[4];

	if (cs == CN)
		goto failure;
	scheme_name = xs_buffer("%s.%s", ResColorScheme, cs);
	s0 = get_resource(scheme_name);
	if (s0 == CN) {
		if (do_popup)
			popup_an_error("Can't find resource %s", scheme_name);
		else
			xs_warning("Can't find resource %s", scheme_name);
		goto failure;
	}
	scheme = s0 = XtNewString(s0);
	for (i = 0; (tk = strtok(scheme, " \t\n")) != CN; i++) {
		scheme = CN;
		if (i > 22) {
			popup_an_error("Ignoring excess data in %s resource",
			    scheme_name);
			break;
		}
		switch (i) {
		    case  0: case  1: case  2: case  3:
		    case  4: case  5: case  6: case  7:
		    case  8: case  9: case 10: case 11:
		    case 12: case 13: case 14: case 15:	/* IBM color name */
			tmp_color_name[i] = tk;
			break;
		    case 16:	/* default for IBM colors */
			if (!strcmp(tk, "white"))
				tmp_ibm_fb = FB_WHITE;
			else if (!strcmp(tk, "black"))
				tmp_ibm_fb = FB_BLACK;
			else {
				if (do_popup)
					popup_an_error("Invalid default color");
				else
					xs_warning("Invalid default color");
				goto failure;
			}
			break;
		    case 17:	/* screen background */
			tmp_colorbg_name = tk;
			break;
		    case 18:	/* select background */
			tmp_selbg_name = tk;
			break;
		    case 19: case 20: case 21: case 22:	/* attribute colors */
			tmp_field_colors[i-19] = atoi(tk);
			if (tmp_field_colors[i-19] < 0 ||
			    tmp_field_colors[i-19] > 0x0f) {
				if (do_popup)
					popup_an_error("Invalid %s resource, ignoring",
					    scheme_name);
				else
					xs_warning("Invalid %s resource, ignoring",
					    scheme_name);
				goto failure;
			}
			tmp_field_colors[i-19] |= GC_NONDEFAULT;
		}
	}
	if (i < 23) {
		if (do_popup)
			popup_an_error("Insufficient data in %s resource",
			    scheme_name);
		else
			xs_warning("Insufficient data in %s resource",
			    scheme_name);
		goto failure;
	}

	/* Success: transfer to live variables. */
	for (i = 0; i < 16; i++) {
		XtFree(color_name[i]);
		color_name[i] = XtNewString(tmp_color_name[i]);
	}
	ibm_fb = tmp_ibm_fb;
	appres.colorbg_name = XtNewString(tmp_colorbg_name);
	appres.selbg_name = XtNewString(tmp_selbg_name);
	for (i = 0; i < 4; i++)
		field_colors[i] = tmp_field_colors[i];

	/* Clean up and exit. */
	XtFree(scheme_name);
	XtFree(s0);
	return True;

    failure:
	XtFree(scheme_name);
	XtFree(s0);
	return False;
}

/* Look up a GC, allocating it if necessary. */
static GC
get_gc(struct sstate *s, int color)
{
	int pixel_index;
	XGCValues xgcv;
	GC r;
	static Boolean in_gc_error = False;

	if (color & GC_NONDEFAULT)
		color &= ~GC_NONDEFAULT;
	else
		color = (color & INVERT_MASK) | DEFAULT_PIXEL;

	if ((r = s->gc[color]) != (GC)None)
		return r;

	/* Allocate the pixel. */
	pixel_index = PIXEL_INDEX(color);
	if (!cpx_done[pixel_index]) {
		if (!alloc_color(color_name[pixel_index], ibm_fb,
				 &cpx[pixel_index])) {
			static char nbuf[16];

			(void) sprintf(nbuf, "%d", pixel_index);
			if (!in_gc_error) {
				in_gc_error = True;
				popup_an_error("Cannot allocate colormap \"%s\" for 3279 color %s (%s), using \"%s\"",
				    color_name[pixel_index], nbuf,
				    see_color((unsigned char)(pixel_index + 0xf0)),
				    fb_name(ibm_fb));
				in_gc_error = False;
			}
		}
		cpx_done[pixel_index] = True;
	}

	/* Allocate the GC. */
	xgcv.font = s->fid;
	if (!(color & INVERT_MASK)) {
		xgcv.foreground = cpx[pixel_index];
		xgcv.background = colorbg_pixel;
	} else {
		xgcv.foreground = colorbg_pixel;
		xgcv.background = cpx[pixel_index];
	}
	if (s == &nss && pixel_index == DEFAULT_PIXEL) {
		xgcv.graphics_exposures = True;
		r = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont|GCGraphicsExposures,
		    &xgcv);
	} else
		r = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont,
		    &xgcv);
	return s->gc[color] = r;
}

/* Look up a selection GC, allocating it if necessary. */
static GC
get_selgc(struct sstate *s, int color)
{
	XGCValues xgcv;
	GC r;

	if (color & GC_NONDEFAULT)
		color = PIXEL_INDEX(color);
	else
		color = DEFAULT_PIXEL;

	if ((r = s->selgc[color]) != (GC)None)
		return r;

	/* Allocate the pixel. */
	if (!cpx_done[color]) {
		if (!alloc_color(color_name[color], FB_WHITE, &cpx[color])) {
			static char nbuf[16];

			(void) sprintf(nbuf, "%d", color);
			popup_an_error("Cannot allocate colormap \"%s\" for 3279 color %s (%s), using \"white\"",
			    color_name[color], nbuf,
			    see_color((unsigned char)(color + 0xf0)));
		}
		cpx_done[color] = True;
	}

	/* Allocate the GC. */
	xgcv.font = s->fid;
	xgcv.foreground = cpx[color];
	xgcv.background = selbg_pixel;
	return s->selgc[color] =
	    XtGetGC(toplevel, GCForeground|GCBackground|GCFont, &xgcv);
}

/* External entry points for GC allocation. */

GC
screen_gc(int color)
{
	return get_gc(ss, color | GC_NONDEFAULT);
}

GC
screen_invgc(int color)
{
	return get_gc(ss, INVERT_COLOR(color | GC_NONDEFAULT));
}
 
/*
 * Preallocate a set of graphics contexts for a given color.
 *
 * This logic is used only in pseudo-color mode.  In full color mode,
 * GCs are allocated dynamically by get_gc().
 */
static void
make_gc_set(struct sstate *s, int i, Pixel fg, Pixel bg)
{
	XGCValues xgcv;

	if (s->gc[i] != (GC)None)
		XtReleaseGC(toplevel, s->gc[i]);
	xgcv.foreground = fg;
	xgcv.background = bg;
	xgcv.graphics_exposures = True;
	xgcv.font = s->fid;
	if (s == &nss && !i)
		s->gc[i] = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont|GCGraphicsExposures,
		    &xgcv);
	else
		s->gc[i] = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont, &xgcv);
	if (s->gc[NGCS + i] != (GC)None)
		XtReleaseGC(toplevel, s->gc[NGCS + i]);
	xgcv.foreground = bg;
	xgcv.background = fg;
	s->gc[NGCS + i] = XtGetGC(toplevel, GCForeground|GCBackground|GCFont,
	    &xgcv);
	if (!appres.mono) {
		if (s->selgc[i] != (GC)None)
			XtReleaseGC(toplevel, s->selgc[i]);
		xgcv.foreground = fg;
		xgcv.background = selbg_pixel;
		s->selgc[i] = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont, &xgcv);
	}
}


/*
 * Convert an attribute to a color index.
 */
static int
fa_color(unsigned char fa)
{
#	define DEFCOLOR_MAP(f) \
		((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))

	if (appres.m3279) {
		/*
		 * Color indices are the low-order 4 bits of a 3279 color
		 * identifier (0 through 15)
		 */
		if (appres.modified_sel && FA_IS_MODIFIED(fa)) {
			return GC_NONDEFAULT |
				(appres.modified_sel_color & 0xf);
		} else if (appres.visual_select &&
			 FA_IS_SELECTABLE(fa) &&
			 !FA_IS_INTENSE(fa)) {
			return GC_NONDEFAULT |
				(appres.visual_select_color & 0xf);
		} else {
			return field_colors[DEFCOLOR_MAP(fa)];
		}
	} else {
		/*
		 * Color indices are the intensity bits (0 through 2)
		 */
		if (FA_IS_ZERO(fa) ||
		    (appres.modified_sel && FA_IS_MODIFIED(fa)))
			return GC_NONDEFAULT | FA_INT_NORM_SEL;
		else
			return GC_NONDEFAULT | (fa & 0x0c);
	}
}



/*
 * Event handlers for toplevel FocusIn, FocusOut, KeymapNotify and
 * PropertyChanged events.
 */

static Boolean toplevel_focused = False;
static Boolean keypad_entered = False;

void
PA_Focus_action(Widget w _is_unused, XEvent *event, String *params _is_unused,
    Cardinal *num_params _is_unused)
{
	XFocusChangeEvent *fe = (XFocusChangeEvent *)event;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_Focus_action, event, params, num_params);
#endif /*]*/
	switch (fe->type) {
	    case FocusIn:
		if (fe->detail != NotifyPointer) {
			toplevel_focused = True;
			screen_focus(True);
		}
		break;
	    case FocusOut:
		toplevel_focused = False;
		if (!toplevel_focused && !keypad_entered)
			screen_focus(False);
		break;
	}
}

void
PA_EnterLeave_action(Widget w _is_unused, XEvent *event _is_unused,
    String *params _is_unused, Cardinal *num_params _is_unused)
{
	XCrossingEvent *ce = (XCrossingEvent *)event;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_EnterLeave_action, event, params, num_params);
#endif /*]*/
	switch (ce->type) {
	    case EnterNotify:
		keypad_entered = True;
		screen_focus(True);
		break;
	    case LeaveNotify:
		keypad_entered = False;
		if (!toplevel_focused && !keypad_entered)
			screen_focus(False);
		break;
	}
}

void
PA_KeymapNotify_action(Widget w _is_unused, XEvent *event, String *params _is_unused,
    Cardinal *num_params _is_unused)
{
	XKeymapEvent *k = (XKeymapEvent *)event;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_KeymapNotify_action, event, params, num_params);
#endif /*]*/
	shift_event(state_from_keymap(k->key_vector));
}

static void
query_window_state(void)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long leftover;
	unsigned char *data = NULL;
	static Boolean was_up = False;

	if (XGetWindowProperty(display, XtWindow(toplevel), a_state, 0L,
	    (long)BUFSIZ, False, a_state, &actual_type, &actual_format,
	    &nitems, &leftover, &data) != Success)
		return;
	if (actual_type == a_state && actual_format == 32) {
		if (*(unsigned long *)data == IconicState) {
			iconic = True;
			keypad_popdown(&was_up);
		} else {
			iconic = False;
			invert_icon(False);
			keypad_first_up();
			if (was_up) {
			    	keypad_popup();
				was_up = False;
			}
		}
	}
	XFree(data);
}

void
PA_StateChanged_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_StateChanged_action, event, params, num_params);
#endif /*]*/
	query_window_state();
}

/*
 * Handle Shift events (KeyPress and KeyRelease events, or KeymapNotify events
 * that occur when the mouse enters the window).
 */

void
shift_event(int event_state)
{
	static int old_state;
	Boolean shifted_now =
	    (event_state & (ShiftKeyDown | MetaKeyDown | AltKeyDown)) != 0;

	if (event_state != old_state) {
		old_state = event_state;
		status_shift_mode(event_state);
		if (shifted != shifted_now) {
			shifted = shifted_now;
			keypad_shift();
		}
	}
}

/*
 * Handle the mouse entering and leaving the window.
 */
static void
screen_focus(Boolean in)
{
#if defined(X3270_DBCS) /*[*/
	/*
	 * Update the input context focus.
	 */
	if (ic != NULL) {
		if (in)
			XSetICFocus(ic);
		else
			XUnsetICFocus(ic);
	}
#endif /*]*/

	/*
	 * Cancel any pending cursor blink.  If we just came into focus and
	 * have a blinking cursor, we will start a fresh blink cycle below, so
	 * the filled-in cursor is visible for a full turn.
	 */
	cancel_blink();

	/*
	 * If the cursor is disabled, simply change internal state.
	 */
	if (!CONNECTED) {
		in_focus = in;
		return;
	}

	/*
	 * Change the appearance of the cursor.  Make it hollow out or fill in
	 * instantly, even if it was blinked off originally.
	 */
	(void) cursor_off();
	in_focus = in;
	cursor_on();

	/*
	 * If we just came into focus and we're supposed to have a blinking
	 * cursor, schedule a blink.
	 */
	if (in_focus && toggled(CURSOR_BLINK))
		schedule_cursor_blink();
}

/*
 * Change fonts.
 */
void
SetFont_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(SetFont_action, event, params, num_params);
	if (check_usage(SetFont_action, *num_params, 1, 1) < 0)
		return;
	screen_newfont(params[0], True, False);
}

/*
 * Split an emulatorFontList resource entry, which looks like:
 *  [menu-name:] [#noauto] [#resize] font-name
 * Modifies the input string.
 */
static void
split_font_list_entry(char *entry, char **menu_name, Boolean *noauto,
    Boolean *resize, char **font_name)
{
	char *colon;
	char *s;
	Boolean any = False;

	if (menu_name != NULL)
		*menu_name = CN;
	if (noauto != NULL)
		*noauto = False;
	if (resize != NULL)
		*resize = False;

	colon = strchr(entry, ':');
	if (colon != CN) {
		if (menu_name != NULL)
			*menu_name = entry;
		*colon = '\0';
		s = colon + 1;
	} else
		s = entry;

	do {
		any = False;
		while (isspace(*s))
			s++;
		if (!strncmp(s, "#noauto", 7) &&
		    (!s[7] || isspace(s[7]))) {
			if (noauto != NULL)
				*noauto = True;
			s += 7;
			any = True;
		} else if (!strncmp(s, "#resize", 7) &&
			   (!s[7] || isspace(s[7]))) {
			if (resize != NULL)
				*resize = True;
			s += 7;
			any = True;
		}
	} while (any);

	*font_name = s;
}

/*
 * Load a font with a display character set required by a charset.
 * Returns True for success, False for failure.
 * If it succeeds, the caller is responsible for calling:
 *	screen_reinit(FONT_CHANGE)
 */
Boolean
screen_new_display_charsets(const char *display_charsets, const char *csnames)
{
	char *rl;
	char *s0, *s;
	char *fontname = CN;
	char *lff;
	Boolean font_found = False;

	/*
	 * If the emulator fonts already implement those charsets, we're done.
	 */
	if (efont_charset != CN && !strcmp(display_charsets, efont_charset))
		goto done;

	/*
	 * If the user chose an emulator font, but we haven't tried it yet,
	 * see if it implements the right charset.
	 */
	if (efontname == CN && appres.efontname != CN) {
		lff = load_fixed_font(appres.efontname, display_charsets);
		if (lff != CN) {
			if (strcmp(appres.efontname, "3270")) {
				popup_an_error(lff);
			}
			Free(lff);
		} else
			fontname = appres.efontname;
	}

	/*
	 * Otherwise, try to get a font from the resize lists.
	 */
	if (fontname == CN) {
		rl = get_fresource("%s.%s", ResEmulatorFontList,
				    display_charsets);
		if (rl != CN) {
			s0 = s = NewString(rl);
			while (!font_found &&
			       split_lresource(&s, &fontname) == 1) {
				Boolean noauto = False;
				char *fn = CN;

				split_font_list_entry(fontname, NULL, &noauto,
					NULL, &fn);
				if (noauto || !*fn)
					continue;

				lff = load_fixed_font(fn, display_charsets);
				if (lff != CN) {
					Free(lff);
				} else
					font_found = True;
			}
			Free(s0);
		}

		if (!font_found &&
		    (!strcasecmp(display_charsets, default_display_charset) ||
		     !strcasecmp(display_charsets, "iso8859-1"))) {
			/* Try "fixed". */
			if ((lff = load_fixed_font("!fixed",
			    display_charsets)) == CN) {
				font_found = True;
			} else {
				/* Fatal. */
				xs_error(lff);
				Free(lff);
				/*NOTREACHED*/
				return False;
			}
		}

		if (!font_found) {
			char *cs_dup;
			char *cs;
			char *buf;
			char *lasts;

			buf = cs_dup = NewString(display_charsets);
			while (!font_found &&
			       (cs = strtok_r(buf, ",", &lasts)) != CN) {
				char *wild;
				char *part1 = CN, *part2 = CN;
				int n_parts = 1;

				buf = CN;
				n_parts = split_dbcs_resource(cs, '+',
					    &part1, &part2);

				if (n_parts == 1 &&
					!strncasecmp(cs, "3270cg", 6)) {
				    	free(part1);
					continue;
				}

				if (n_parts == 2) {
					wild = xs_buffer(
					    "*-r-*-c-*-%s+*-r-*-c-*-%s",
					    part1, part2);
				} else {
					wild = xs_buffer("*-r-*-c-*-%s", cs);
				}
				lff = load_fixed_font(wild, cs);
				if (lff != CN)
					Free(lff);
				else
					font_found = True;
				Free(wild);
				if (!font_found) {
					if (n_parts == 2) {
						wild = xs_buffer(
						    "*-r-*-c-*-%s+*-r-*-c-*-%s",
						    part1, part2);
					} else {
						wild = xs_buffer("*-r-*-m-*-%s",
								    cs);
					}
					lff = load_fixed_font(wild, cs);
					if (lff != CN)
						Free(lff);
					else
						font_found = True;
					Free(wild);
				}
				if (part1 != CN)
					Free(part1);
				if (part2 != CN)
					Free(part2);
			}
			Free(cs_dup);
		}

		if (!font_found) {
			char *xs = expand_cslist(display_charsets);

			popup_an_error("No %s fonts found", xs);
			Free(xs);
			return False;
		}
	}
	allow_resize = appres.allow_resize;

    done:
	/* Set the appropriate global. */
	Replace(required_display_charsets,
	    display_charsets? NewString(display_charsets): CN);
	init_rsfonts(required_display_charsets);

	return True;
}

void
screen_newfont(char *fontnames, Boolean do_popup, Boolean is_cs)
{
	char *old_font;
	char *lff;

	/* Do nothing, successfully. */
	if (!is_cs && efontname && !strcmp(fontnames, efontname))
		return;

	/* Save the old font before trying the new one. */
	old_font = XtNewString(efontname);

	/* Try the new one. */
	if ((lff = load_fixed_font(fontnames, required_display_charsets)) != CN) {
		if (do_popup)
			popup_an_error(lff);
		Free(lff);
		XtFree(old_font);
		return;
	}

	/* Store the old name away, in case we have to go back to it. */
	Replace(redo_old_font, old_font);
	screen_redo = REDO_FONT;

	screen_reinit(FONT_CHANGE);
	efont_changed = True;
}

static Boolean
seems_scalable(const char *name)
{
	int i = 0;
	char *ndup = NewString(name);
	char *buf = ndup;
	char *dash;
	Boolean scalable = False;

	while ((dash = strchr(buf, '-')) != CN) {
		*dash = '\0';
		i++;
		if ((i == 8 || i == 9 || i == 13) &&
		    !strcmp(buf, "0")) {
			scalable = True;
			break;
		}
		buf = dash + 1;
	}
	Free(ndup);
	return scalable;
}

/*
 * Make sure a font implements a desired display character set.
 * Returns True for success, False for failure.
 */
static Boolean
check_charset(const char *name, XFontStruct *f, const char *dcsname,
    Boolean force, const char **font_csname, Boolean *scalable)
{
	unsigned long a_family_name, a_font_registry, a_font_encoding;
	char *font_registry = CN, *font_encoding = CN;
	const char *font_charset = CN;
	Boolean r = False;
	char *csn0, *ptr, *csn;
	char *lasts;

#if defined(DEBUG_FONTPICK) /*[*/
	printf("checking '%s' against %s\n", name, dcsname);
#endif /*]*/
	/* Check for scalability. */
	*scalable = False;
	if (!force) {
		*scalable = seems_scalable(name);
		if (*scalable) {
#if defined(DEBUG_FONTPICK) /*[*/
		    printf("'%s' seems scalable\n", name);
#endif /*]*/
		    return False;
		}
	}

	if (XGetFontProperty(f, a_registry, &a_font_registry))
		font_registry = XGetAtomName(display, a_font_registry);
	if (XGetFontProperty(f, a_encoding, &a_font_encoding))
		font_encoding = XGetAtomName(display, a_font_encoding);

	if ((font_registry != CN &&
	     (!strcmp(font_registry, "IBM 3270") ||
	     (!font_registry[0] &&
	      (XGetFontProperty(f, XA_FAMILY_NAME, &a_family_name) &&
	       !strcmp(XGetAtomName(display, a_family_name), "3270"))))) ||
            (font_registry == CN && !strncmp(name, "3270", 4))) {
		/* Old 3270 font. */
		font_charset = name2cs_3270(name);
		if (font_charset != CN)
			font_charset = NewString(font_charset);
		else
			font_charset = NewString("unknown-unknown");
	} else {
		char *encoding;

		if (font_encoding != CN && font_encoding[0] == '-')
			encoding = font_encoding + 1;
		else
			encoding = font_encoding;
		font_charset = xs_buffer("%s-%s",
		    font_registry? font_registry: "unknown",
		    encoding? encoding: "unknown");
	}

	ptr = csn0 = NewString(dcsname);
	while (!r && (csn = strtok_r(ptr, ",", &lasts)) != CN) {
		if (force || !strcasecmp(font_charset, csn)) {
#if defined(DEBUG_FONTPICK) /*[*/
			printf("'%s' says it implements '%s' (want '%s')%s\n", name, font_charset, csn, force? " (forced)": "");
#endif /*]*/
			r = True;
		}
		ptr = CN;
	}
	Free(csn0);
	if (font_csname != NULL)
		*font_csname = font_charset;
	else
		Free(font_charset);
#if defined(DEBUG_FONTPICK) /*[*/
	printf("'%s' seems %s\n", name, r? "okay": "bad");
#endif /*]*/

	if (font_registry != CN)
	    	XtFree(font_registry);
	if (font_encoding != CN)
	    	XtFree(font_encoding);
	return r;
}

/*
 * Expand a character set list into English.
 */
static char *
expand_cslist(const char *s)
{
	int commas = 0;
	const char *t;
	char *comma;
	char *r;

	/* Count the commas. */
	for (t = s; (comma = strchr(t, ',')) != CN; t = comma + 1) {
		commas++;
	}

	/* If there aren't any, there isn't any work to do. */
	if (!commas)
		return NewString(s);

	/* Allocate enough space for "a, b, c or d". */
	r = Malloc(strlen(s) + (commas * 2) + 2 + 1);
	*r = '\0';

	/* Copy and expand. */
	for (t = s; (comma = strchr(t, ',')) != CN; t = comma + 1) {
		int wl = comma - t;

		if (*r)
			(void) strcat(r, ", ");
		(void) strncat(r, t, wl);
	}
	return strcat(strcat(r, " or "), t);
}

/* Get the pixel size property from a font. */
static unsigned long
get_pixel_size(XFontStruct *f)
{
	Boolean initted = False;
	static Atom a_pixel_size;
	unsigned long v;

	if (!initted) {
		a_pixel_size = XInternAtom(display, "PIXEL_SIZE", True);
		if (a_pixel_size == None)
			return 0L;
		initted = True;
	}
	if (XGetFontProperty(f, a_pixel_size, &v))
		return v;
	else
		return 0L;
}

/* Get the weight property from a font. */
static unsigned long
get_weight(XFontStruct *f)
{
	Boolean initted = False;
	static Atom a_weight_name;
	unsigned long v;

	if (!initted) {
		a_weight_name = XInternAtom(display, "WEIGHT_NAME", True);
		if (a_weight_name == None)
			return 0L;
		initted = True;
	}
	if (XGetFontProperty(f, a_weight_name, &v))
		return v;
	else
		return 0L;
}

/*
 * Load and query a font.
 * Returns NULL (okay) or an error message.
 */
static char *
load_fixed_font(const char *names, const char *reqd_display_charsets)
{
	int num_names = 1, num_cs = 1;
	char *name1 = CN, *name2 = CN;
	char *charset1 = CN, *charset2 = CN;
	char *r;

#if defined(DEBUG_FONTPICK) /*[*/
	fprintf(stderr, "load_fixed_font(%s, %s)\n",
	    names, reqd_display_charsets);
#endif /*]*/

	/* Split out the names and character sets. */
	num_names = split_dbcs_resource(names, '+', &name1, &name2);
	num_cs = split_dbcs_resource(reqd_display_charsets, '+', &charset1,
			&charset2);
	if (num_names == 1 && num_cs >= 2) {
		Free(name1);
		Free(name2);
		Free(charset1);
		Free(charset2);
		return NewString("Must specify two font names (SBCS+DBCS)");
	}
	if (num_names == 2 && num_cs < 2) {
		Free(name2);
		name2 = CN;
	}

#if defined(X3270_DBCS) /*[*/
	/* If there's a DBCS font, load that first. */
	if (name2 != CN) {
		/* Load the second font. */
		r = lff_single(name2, charset2, True);
		if (r != CN) {
			Free(name1);
			Free(charset1);
			return r;
		}
	} else {
		dbcs_font.font_struct = NULL;
		dbcs_font.font = None;
		dbcs = False;
	}
#endif /*]*/

	/* Load the SBCS font. */
	r = lff_single(name1, charset1, False);

	/* Free the split-out names and return the final result. */
	Free(name1);
	Free(name2);
	Free(charset1);
	Free(charset2);
	return r;
}

/*
 * Load a list of fonts, possibly from the local cache.
 */
static char **
xlfwi(char *pattern, int max_names, int *count_return,
	XFontStruct **info_return)
{
    	struct fi_cache {
	    struct fi_cache *next;
	    char *pattern;
	    char **names;
	    int count;
	    XFontStruct *info;
	};
	static struct fi_cache *fi_cache = NULL, *fi_last = NULL;
	struct fi_cache *f;
	char **names;
	int count;
	XFontStruct *info;

	/* Check the cache. */
	for (f = fi_cache; f != NULL; f = f->next) {
	    	if (!strcmp(pattern, f->pattern)) {
#if defined(XLFWI_DEBUG) /*[*/
		    	printf("xlfwi cache hit on %s\n", pattern);
#endif /*]*/
		    	*count_return = f->count;
			*info_return = f->info;
		    	return f->names;
		}
	}
#if defined(XLFWI_DEBUG) /*[*/
	printf("xlfwi no hit on %s\n", pattern);
#endif /*]*/

	/* Ask the server. */
	names = XListFontsWithInfo(display, pattern, max_names, &count, &info);
	if (names == NULL)
	    	return NULL;

	/* Save the answer and return it. */
	f = (struct fi_cache *)XtMalloc(sizeof(struct fi_cache));
	f->pattern = XtNewString(pattern);
	f->names = names;
	f->count = *count_return = count;
	f->info = *info_return = info;
	f->next = NULL;
	if (fi_last != NULL)
	    	fi_last->next = f;
	else
	    	fi_cache = f;
	fi_last = f;

	return names;
}

/*
 * Load and query one font.
 * Returns NULL (okay) or an error message.
 */
static char *
lff_single(const char *name, const char *reqd_display_charset, Boolean is_dbcs)
{
	XFontStruct *f, *g;
	char **matches;
	int count, mod_count;
	Boolean force = False;
	char *r;
	const char *font_csname = "?";
	int i;
	int best = -1;
	char *best_weight = CN;
	unsigned long best_pixel_size = 0L;
	Boolean scalable;
	char *wname = CN;

#if defined(DEBUG_FONTPICK) /*[*/
	fprintf(stderr, "lff_single: name %s, cs %s, %s\n", name,
	    reqd_display_charset, is_dbcs? "dbcs": "sbcs");
#endif /*]*/

	if (*name == '!') {
		name++;
		force = True;
	}

	matches = xlfwi((char *)name, 1000, &count, &f);
	if (matches == (char **)NULL) {
#if defined(DEBUG_FONTPICK) /*[*/
		printf("Font '%s' not found\n", name);
#endif /*]*/
		return xs_buffer("Font %s\nnot found", name);
	}
#if defined(DEBUG_FONTPICK) /*[*/
	printf("%d fonts found for '%s'\n", count, name);
#endif /*]*/
	if (count > 1 && (strchr(name, '*') == NULL)) {
#if defined(DEBUG_FONTPICK) /*[*/
		printf("pretending 1\n");
#endif /*]*/
		mod_count = 1;
	} else
		mod_count = count;
	for (i = 0; i < mod_count; i++) {
#if defined(DEBUG_FONTPICK) /*[*/
		printf("  %s\n", matches[i]);
#endif /*]*/
		if (!check_charset(matches[i], &f[i], reqd_display_charset,
			    force, &font_csname, &scalable)) {
			char *xp = expand_cslist(reqd_display_charset);

			if (mod_count == 1) {
				if (scalable) {
					r = xs_buffer("Font '%s'\nappears to be "
						      "scalable\n"
						      "(Specify '!%s' to "
						      "override)",
						      name, name);
				} else {
					r = xs_buffer("Font '%s'\n"
					    "implements %s, not %s\n"
					    "(Specify '!%s' to override)",
					    name,
					    font_csname,
					    xp,
					    name);
					Free(font_csname);
					Free(xp);
				}
				if (wname != CN)
				    	XtFree(wname);
				return r;
			}
		} else {
			unsigned long pixel_size = get_pixel_size(&f[i]);
			Atom w = get_weight(&f[i]);

			/* Correct. */
			if (is_dbcs) {
				Replace(efont_charset_dbcs, font_csname);
			} else {
				Replace(efont_charset, font_csname);
			}
			if (w) {
				Replace(wname, XGetAtomName(display, w));
#if defined(DEBUG_FONTPICK) /*[*/
				printf("%s weight is %s\n", matches[i], wname);
#endif /*]*/
			}

#if defined(X3270_DBCS) /*[*/
			if (!is_dbcs && dbcs_font.font_struct != NULL) {
				/*
				 * When searching, we will accept only a
				 * perfect match.
				 * When not searching, we'll take whatever
				 * the user gives us, and adjust accordingly.
				 */
				if (mod_count == 1 ||
				    (pixel_size ==
					get_pixel_size(dbcs_font.font_struct) &&
				     (2 * f[i].max_bounds.width) ==
					dbcs_font.font_struct->max_bounds.width )) {
					best = i;
					break;
				} else
					continue;
			}
#endif /*]*/
			if (best < 0 ||
			    (labs(pixel_size - 14L) <
			     labs(best_pixel_size - 14L)) ||
			    (w &&
			     (best_weight == CN ||
			      (!strcasecmp(best_weight, "bold") &&
			       strcasecmp(wname, "bold"))))) {
				best = i;
				if (w)
					Replace(best_weight,
						XtNewString(wname));
				best_pixel_size = pixel_size;
#if defined(DEBUG_FONTPICK) /*[*/
				printf("best pixel size: %ld '%s'\n", pixel_size, matches[i]);
#endif /*]*/
			} else {
#if defined(DEBUG_FONTPICK) /*[*/
				printf("not so good: %ld '%s'\n", pixel_size, matches[i]);
#endif /*]*/
			}
		}
	}
	if (wname != NULL)
	    	XtFree(wname);
	if (best_weight != NULL)
	    	XtFree(best_weight);
	if (best < 0) {
	    return xs_buffer("None of the %d fonts matching\n"
			     "%s\n"
			     "appears to be appropriate",
			     count, name);
	}

	g = XLoadQueryFont(display, matches[best]);
	set_font_globals(g, name, matches[best], g->fid, is_dbcs);
	return CN;
}

/*
 * Figure out what sort of registry and encoding we want.
 */
char *
display_charset(void)
{
	return (required_display_charsets != CN)? required_display_charsets:
	    					 default_display_charset;
}

/*
 * Set globals based on font name and info
 */
static void
set_font_globals(XFontStruct *f, const char *ef, const char *fef, Font ff,
    Boolean is_dbcs)
{
	unsigned long svalue;
	unsigned i;
	char *family_name = NULL;
	char *font_encoding = NULL;
	char *fe = NULL;
	char *font_charset = NULL;

	if (XGetFontProperty(f, a_registry, &svalue))
		family_name = XGetAtomName(display, svalue);
	if (family_name == NULL)
	    	Error("Cannot get font family_name");
	if (XGetFontProperty(f, a_encoding, &svalue))
		font_encoding = XGetAtomName(display, svalue);
	if (font_encoding == NULL)
	    	Error("Cannot get font encoding");
	if (font_encoding[0] == '-')
		fe = font_encoding + 1;
	else
		fe = font_encoding;

#if defined(X3270_DBCS) /*[*/
	if (is_dbcs) {
		/* Hack. */
		dbcs_font.font_struct = f;
		dbcs_font.font = f->fid;
		dbcs_font.unicode = !strcasecmp(family_name, "iso10646");
		dbcs_font.ascent = f->max_bounds.ascent;
		dbcs_font.descent = f->max_bounds.descent;
		dbcs_font.char_width  = fCHAR_WIDTH(f);
		dbcs_font.char_height = dbcs_font.ascent + dbcs_font.descent;
		dbcs_font.d16_ix = display16_init(xs_buffer("%s-%s",
			    family_name, fe));
		dbcs = True;
		Replace(full_efontname_dbcs, XtNewString(fef));
		Free(family_name);
		Free(font_encoding);
		return;
	}
#endif /*]*/
	font_charset = xs_buffer("%s-%s", family_name, fe);
	Free(family_name);
	Free(font_encoding);
	Replace(efontname, XtNewString(ef));
	Replace(full_efontname, XtNewString(fef));

	/* Set the dimensions. */
	nss.char_width  = fCHAR_WIDTH(f);
	nss.char_height = fCHAR_HEIGHT(f);
	nss.fid = ff;
	if (nss.font != NULL)
		XFreeFontInfo(NULL, nss.font, 1);
	nss.font = f;
	nss.ascent = f->ascent;
	nss.descent = f->descent;

	/* Figure out if this is a 3270 font, or a standard X font. */
	if (XGetFontProperty(f, XA_FAMILY_NAME, &svalue))
		nss.standard_font = (Atom) svalue != a_3270;
	else if (!strncmp(efontname, "3270", 4))
		nss.standard_font = False;
	else
		nss.standard_font = True;

	/* Set other globals. */
	if (nss.standard_font) {
		nss.extended_3270font = False;
		nss.font_8bit = efont_matches;
		nss.font_16bit = (f->max_byte1 > 0);
		nss.d8_ix = display8_init(nss.font_8bit? font_charset:
							 "ascii-7");
	} else {
#if defined(BROKEN_MACH32)
		nss.extended_3270font = False;
#else
		nss.extended_3270font = f->max_byte1 > 0 ||
			f->max_char_or_byte2 > 255;
#endif
		nss.font_8bit = False;
		nss.font_16bit = False;
		nss.d8_ix = display8_init(font_charset);
	}
	Free(font_charset);
	font_charset = NULL;

	/* See if this font has any unusually-shaped characters. */
	INIT_ODD(nss.odd_width);
	INIT_ODD(nss.odd_lbearing);
	nss.funky_font = False;
	if (!nss.extended_3270font && f->per_char != NULL) {
		for (i = 0; i < 256; i++) {
			if (PER_CHAR(f, i).width == 0 &&
			    (PER_CHAR(f, i).rbearing |
			     PER_CHAR(f, i).lbearing |
			     PER_CHAR(f, i).ascent |
			     PER_CHAR(f, i).descent) == 0) {
				/* Missing character. */
				continue;
			}

			if (PER_CHAR(f, i).width != f->max_bounds.width) {
				SET_ODD(nss.odd_width, i);
				nss.funky_font = True;
			}
			if (PER_CHAR(f, i).lbearing < 0) {
				SET_ODD(nss.odd_lbearing, i);
				nss.funky_font = True;
			}
		}
	}

	/*
	 * If we've changed the rules for resizing, let the window manager
	 * know.
	 */
	if (container != NULL)
		set_toplevel_sizes();
}

/*
 * Font initialization.
 */
void
font_init(void)
{
}

#if defined(X3270_MENUS) /*[*/
/*
 * Change models.
 */
void
screen_change_model(int mn, int ovc, int ovr)
{
	if (CONNECTED ||
	    (model_num == mn && ovc == ov_cols && ovr == ov_rows))
		return;

	redo_old_model = model_num;
	redo_old_ov_cols = ov_cols;
	redo_old_ov_rows = ov_rows;
	screen_redo = REDO_MODEL;

	model_changed = True;
	if (ov_cols != ovc || ov_rows != ovr)
		oversize_changed = True;
	set_rows_cols(mn, ovc, ovr);
	st_changed(ST_REMODEL, True);
	screen_reinit(MODEL_CHANGE);
}

/*
 * Change emulation modes.
 */
void
screen_extended(Boolean extended _is_unused)
{
	set_rows_cols(model_num, ov_cols, ov_rows);
	model_changed = True;
}

void
screen_m3279(Boolean m3279 _is_unused)
{
	destroy_pixels();
	screen_reinit(COLOR_CHANGE);
	set_rows_cols(model_num, ov_cols, ov_rows);
	model_changed = True;
}

/*
 * Change color schemes.  Alas, this is destructive if it fails.
 */
void
screen_newscheme(char *s)
{
	Boolean xferred;

	if (!appres.m3279)
		return;

	destroy_pixels();
	xferred = xfer_color_scheme(s, True);
	if (xferred)
		appres.color_scheme = s;
	screen_reinit(COLOR_CHANGE);
	scheme_changed = True;
}

/*
 * Change character sets.
 */
void
screen_newcharset(char *csname)
{
	char *old_charset = NewString(get_charset_name());

	switch (charset_init(csname)) {
	    case CS_OKAY:
		/* Success. */
		Free(old_charset);
		st_changed(ST_CHARSET, True);
		screen_reinit(CHARSET_CHANGE | FONT_CHANGE);
		charset_changed = True;
		break;
	    case CS_NOTFOUND:
		Free(old_charset);
		popup_an_error("Cannot find definition of host character set \"%s\"",
		    csname);
		break;
	    case CS_BAD:
		Free(old_charset);
		popup_an_error("Invalid charset definition for \"%s\"", csname);
		break;
	    case CS_PREREQ:
		Free(old_charset);
		popup_an_error("No fonts for host character set \"%s\"", csname);
		break;
	    case CS_ILLEGAL:
		/* Error already popped up. */
		Free(old_charset);
		break;

	}
}
#endif /*]*/

/*
 * Visual or not-so-visual bell
 */
void
ring_bell(void)
{
	static XGCValues xgcv;
	static GC bgc;
	static int initted;
	struct timeval tv;

	/* Ring the real display's bell. */
	if (!appres.visual_bell)
		XBell(display, appres.bell_volume);

	/* If we're iconic, invert the icon and return. */
	if (!appres.active_icon) {
		query_window_state();
		if (iconic) {
			invert_icon(True);
			return;
		}
	}

	if (!appres.visual_bell || !ss->exposed_yet)
		return;

	/* Do a screen flash. */

	if (!initted) {
		xgcv.function = GXinvert;
		bgc = XtGetGC(toplevel, GCFunction, &xgcv);
		initted = 1;
	}
	screen_disp(False);
	XFillRectangle(display, ss->window, bgc,
	    0, 0, ss->screen_width, ss->screen_height);
	XSync(display, 0);
	tv.tv_sec = 0;
	tv.tv_usec = 125000;
	(void) select(0, NULL, NULL, NULL, &tv);
	XFillRectangle(display, ss->window, bgc,
	    0, 0, ss->screen_width, ss->screen_height);
	XSync(display, 0);
}

/*
 * Window deletion
 */
void
PA_WMProtocols_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	XClientMessageEvent *cme = (XClientMessageEvent *)event;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_WMProtocols_action, event, params, num_params);
#endif /*]*/
	if ((Atom)cme->data.l[0] == a_delete_me) {
		if (w == toplevel)
			x3270_exit(0);
		else
			XtPopdown(w);
	} else if ((Atom)cme->data.l[0] == a_save_yourself && w == toplevel) {
		save_yourself();
	}
}


/* Initialize the icon. */
void
icon_init(void)
{
	icon = XCreateBitmapFromData(display, root_window,
	    (char *) x3270_bits, x3270_width, x3270_height);

	if (appres.active_icon) {
		Dimension iw, ih;

		aicon_font_init();
		aicon_size(&iw, &ih);
		icon_shell =  XtVaAppCreateShell(
		    "x3270icon",
		    "X3270",
		    overrideShellWidgetClass,
		    display,
		    XtNwidth, iw,
		    XtNheight, ih,
		    XtNmappedWhenManaged, False,
		    NULL);
		XtRealizeWidget(icon_shell);
		XtVaSetValues(toplevel,
		    XtNiconWindow, XtWindow(icon_shell),
		    NULL);
		if (appres.active_icon) {
			XtVaSetValues(icon_shell,
			    XtNbackground, appres.mono ? appres.background
						       : colorbg_pixel,
			    NULL);
		}
	} else {
		unsigned i;

		for (i = 0; i < sizeof(x3270_bits); i++)
			x3270_bits[i] = ~x3270_bits[i];
		inv_icon = XCreateBitmapFromData(display, root_window,
		    (char *) x3270_bits, x3270_width, x3270_height);
		wait_icon = XCreateBitmapFromData(display, root_window,
		    (char *) wait_bits, wait_width, wait_height);
		for (i = 0; i < sizeof(wait_bits); i++)
			wait_bits[i] = ~wait_bits[i];
		inv_wait_icon = XCreateBitmapFromData(display, root_window,
		    (char *) wait_bits, wait_width, wait_height);
		XtVaSetValues(toplevel,
		    XtNiconPixmap, icon,
		    XtNiconMask, icon,
		    NULL);
	}
}

/*
 * Initialize the active icon font information.
 */
static void
aicon_font_init(void)
{
	XFontStruct *f;
	Font ff;
	char **matches;
	int count;

	if (!appres.active_icon) {
		appres.label_icon = False;
		return;
	}

	matches = XListFontsWithInfo(display, appres.icon_font, 1, &count, &f);
	if (matches == (char **)NULL) {
		popup_an_error("No font %s \"%s\"\nactiveIcon will not work",
		    ResIconFont, appres.icon_font);
		appres.active_icon = False;
		return;
	}
	ff = XLoadFont(display, matches[0]);
	iss.char_width = fCHAR_WIDTH(f);
	iss.char_height = fCHAR_HEIGHT(f);
	iss.fid = ff;
	iss.font = f;
	iss.ascent = f->ascent;
	iss.overstrike = False;
	iss.standard_font = True;
	iss.extended_3270font = False;
	iss.font_8bit = False;
	iss.obscured = True;
	iss.d8_ix = display8_init("ascii-7");
	if (appres.label_icon) {
		matches = XListFontsWithInfo(display, appres.icon_label_font,
		    1, &count, &ailabel_font);
		if (matches == (char **)NULL) {
			popup_an_error("Cannot load %s \"%s\" font\nlabelIcon will not work",
			    ResIconLabelFont, appres.icon_label_font);
			appres.label_icon = False;
			return;
		}
		ailabel_font->fid = XLoadFont(display, matches[0]);
		aicon_label_height = fCHAR_HEIGHT(ailabel_font) + 2;
	}
	INIT_ODD(iss.odd_width);
	INIT_ODD(iss.odd_lbearing);
	iss.funky_font = False;
}

/*
 * Determine the current size of the active icon.
 */
static void
aicon_size(Dimension *iw, Dimension *ih)
{
	XIconSize *is;
	int count;

	*iw = maxCOLS*iss.char_width + 2*VHALO;
	*ih = maxROWS*iss.char_height + 2*HHALO + aicon_label_height;
	if (XGetIconSizes(display, root_window, &is, &count)) {
		if (*iw > (unsigned) is[0].max_width)
			*iw = is[0].max_width;
		if (*ih > (unsigned) is[0].max_height)
			*ih = is[0].max_height;
	}
}

/*
 * Initialize the active icon.  Assumes that aicon_font_init has already been
 * called.
 */
static void
aicon_init(void)
{
	if (!appres.active_icon)
		return;

	iss.widget = icon_shell;
	iss.window = XtWindow(iss.widget);
	iss.cursor_daddr = 0;
	iss.exposed_yet = False;
	if (appres.label_icon) {
		XGCValues xgcv;

		xgcv.font = ailabel_font->fid;
		xgcv.foreground = appres.foreground;
		xgcv.background = appres.background;
		ailabel_gc = XtGetGC(toplevel,
		    GCFont|GCForeground|GCBackground,
		    &xgcv);
	}
}

/*
 * Reinitialize the active icon.
 */
static void
aicon_reinit(unsigned cmask)
{
	if (!appres.active_icon)
		return;

	if (cmask & (FONT_CHANGE | COLOR_CHANGE))
		make_gcs(&iss);

	if (cmask & MODEL_CHANGE) {
		aicon_size(&iss.screen_width, &iss.screen_height);
		Replace(iss.image,
		    (union sp *)XtMalloc(sizeof(union sp) * maxROWS * maxCOLS));
		XtVaSetValues(iss.widget,
		    XtNwidth, iss.screen_width,
		    XtNheight, iss.screen_height,
		    NULL);
	}
	if (cmask & (MODEL_CHANGE | FONT_CHANGE | COLOR_CHANGE))
		(void) memset((char *)iss.image, 0,
			      sizeof(union sp) * maxROWS * maxCOLS);
}

/* Draw the aicon label */
static void
draw_aicon_label(void)
{
	int len;
	Position x;

	if (!appres.label_icon || !iconic)
		return;

	XFillRectangle(display, iss.window,
	    get_gc(&iss, INVERT_COLOR(0)),
	    0, iss.screen_height - aicon_label_height,
	    iss.screen_width, aicon_label_height);
	len = strlen(aicon_text);
	x = ((int)iss.screen_width - XTextWidth(ailabel_font, aicon_text, len))
	     / 2;
	if (x < 0)
		x = 2;
	XDrawImageString(display, iss.window, ailabel_gc,
	    x,
	    iss.screen_height - aicon_label_height + ailabel_font->ascent,
	    aicon_text, len);
}

/* Set the aicon label */
void
set_aicon_label(char *l)
{
	Replace(aicon_text, XtNewString(l));
	draw_aicon_label();
}

/* Change the bitmap icon. */
static void
flip_icon(Boolean inverted, enum mcursor_state mstate)
{
	Pixmap p = icon;
	
	if (mstate == LOCKED)
		mstate = NORMAL;
	if (appres.active_icon
	    || (inverted == icon_inverted && mstate == icon_cstate))
		return;
	switch (mstate) {
	    case WAIT:
		if (inverted)
			p = inv_wait_icon;
		else
			p = wait_icon;
		break;
	    case LOCKED:
	    case NORMAL:
		if (inverted)
			p = inv_icon;
		else
			p = icon;
		break;
	}
	XtVaSetValues(toplevel,
	    XtNiconPixmap, p,
	    XtNiconMask, p,
	    NULL);
	icon_inverted = inverted;
	icon_cstate = mstate;
}

/*
 * Invert the icon.
 */
static void
invert_icon(Boolean inverted)
{
	flip_icon(inverted, icon_cstate);
}

/*
 * Change to the lock icon.
 */
static void
lock_icon(enum mcursor_state state)
{
	flip_icon(icon_inverted, state);
}


/* Check the font menu for an existing name. */
static Boolean
font_in_menu(char *font)
{
	struct font_list *g;

	for (g = font_list; g != NULL; g = g->next) {
		if (!strcasecmp(NO_BANG(font), NO_BANG(g->font)))
			return True;
	}
	return False;
}

/* Add a font to the font menu. */
static Boolean
add_font_to_menu(char *label, char *font)
{
	struct font_list *f;

	label = NewString(label);
	f = (struct font_list *)XtMalloc(sizeof(*f));
	if (!split_hier(label, &f->label, &f->parents)) {
		Free((XtPointer)f);
		return False;
	}
	f->font = NewString(font);
	f->next = NULL;
	/*f->label = label;*/
	if (font_list)
		font_last->next = f;
	else
		font_list = f;
	font_last = f;
	font_count++;
	return True;
}

/*
 * Resize font list parser.
 */
static void
init_rsfonts(char *charset_name)
{
	char *ms;
	struct rsfont *r;
	struct font_list *f;
	char *wild;
	int count, i;
	char **names;
	char *dupcsn, *csn, *buf;
	char *lasts;
	XFontStruct *fs;
	Boolean scalable;
	char *hier_name;

	/* Clear the old lists. */
	while (rsfonts != NULL) {
		r = rsfonts->next;
		Free(rsfonts);
		rsfonts = r;
	}
	while (font_list != NULL) {
		f = font_list->next;
		if (font_list->parents)
			Free(font_list->parents);
		/*Free(font_list->label);  a leak! */
		Free(font_list->font);
		Free(font_list);
		font_list = f;
	}
	font_last = NULL;
	font_count = 0;

	/* If there's no character set, we're done. */
	if (charset_name == CN)
		return;

	/* Get the emulatorFontList resource. */
	ms = get_fresource("%s.%s", ResEmulatorFontList, charset_name);
	if (ms != CN) {
		char *ns;
		char *line;
		char *label;
		char *font;
		Boolean resize;
		char **matches;
		int count;

		ns = ms = NewString(ms);
		while (split_lresource(&ms, &line) == 1) {

			/* Figure out what it's about. */
			split_font_list_entry(line, &label, NULL, &resize,
			    &font);
			if (!*font)
				continue;

			/* Search for duplicates. */
			if (font_in_menu(font))
				continue;

			/* Add it to the font_list (menu). */
			if (!add_font_to_menu((label != CN)? label:
						NO_BANG(font),
					    font))
				continue;

			/* Add it to the resize menu, if possible. */
			if (!resize)
				continue;
			matches = XListFontsWithInfo(display, NO_BANG(font), 1,
						     &count, &fs);
			if (matches == (char **)NULL)
				continue;
			r = (struct rsfont *)XtMalloc(sizeof(*r));
			r->name = XtNewString(font);
			r->width = fCHAR_WIDTH(fs);
			r->height = fCHAR_HEIGHT(fs);
			XFreeFontInfo(matches, fs, count);
			r->next = rsfonts;
			rsfonts = r;
		}
		free(ns);
	}

#if defined(X3270_DBCS) /*[*/
	/*
	 * In DBCS mode, if we've found at least one appropriate font from the
	 * list, we're done.
	 */
	if (dbcs)
		return;
#endif /*]*/

	/* Add 'fixed' to the menu, so there's at least one alternative. */
	(void) add_font_to_menu("fixed", "!fixed");

	/* Expand out wild-cards based on the display character set names. */
	buf = dupcsn = NewString(charset_name);
	while ((csn = strtok_r(buf, ",", &lasts)) != CN) {
		buf = CN;
	    	if (!strncasecmp(csn, "3270cg", 6))
		    	continue;
		wild = xs_buffer("*-r-*-c-*-%s", csn);
		count = 0;
		names = xlfwi(wild, 1000, &count, &fs);
		Free(wild);
		if (count != 0) {
			for (i = 0; i < count; i++) {
				if (check_charset(names[i], &fs[i], csn, False,
					NULL, &scalable) &&
				   !font_in_menu(names[i])) {
				    	char *dash1 = NULL, *dash2 = NULL;

					if (names[i][0] == '-') {
					    	dash1 = strchr(names[i] + 1,
							'-');
						if (dash1 != NULL)
						    	dash2 = strchr(
								dash1 + 1,
								'-');
					}
					if (dash2 != NULL) {
					    	hier_name =
						    xs_buffer("%s>%.*s>%s",
							csn,
							dash2 - names[i] - 1,
							 names[i] + 1,
							dash2 + 1);
					} else
						hier_name = xs_buffer("%s>%s",
								csn, names[i]);
					(void) add_font_to_menu(hier_name,
								names[i]);
					Free(hier_name);
				}
			}
		}
		wild = xs_buffer("*-r-*-m-*-%s", csn);
		count = 0;
		names = xlfwi(wild, 1000, &count, &fs);
		Free(wild);
		if (count != 0) {
			for (i = 0; i < count; i++) {
				if (check_charset(names[i], &fs[i], csn, False,
					NULL, &scalable) &&
				   !font_in_menu(names[i])) {
				    	char *dash1 = NULL, *dash2 = NULL;

					if (names[i][0] == '-') {
					    	dash1 = strchr(names[i] + 1,
							'-');
						if (dash1 != NULL)
						    	dash2 = strchr(
								dash1 + 1,
								'-');
					}
					if (dash2 != NULL) {
					    	hier_name =
						    xs_buffer("%s>%.*s>%s",
							csn,
							dash2 - names[i] - 1,
							 names[i] + 1,
							dash2 + 1);
					} else
						hier_name = xs_buffer("%s>%s",
								csn, names[i]);
					(void) add_font_to_menu(hier_name,
								names[i]);
					Free(hier_name);
				}
			}
		}
	}
	Free(dupcsn);
}

/*
 * Handle ConfigureNotify events.
 */
static struct {
	Boolean ticking;
	Dimension width, height;
	Position x, y;
	XtIntervalId id;
} cn = {
	False, 0, 0, 0, 0, 0
};
static Position main_x = 0, main_y = 0;

/*
 * Timeout routine called 0.5 sec after x3270 sets new screen dimensions.
 * We assume that if this happens, the window manager is happy with our new
 * size.
 */
static void
configure_stable(XtPointer closure _is_unused, XtIntervalId *id _is_unused)
{
	trace_event("Reconfigure timer expired\n");
	configure_ticking = False;
	if (!cn.ticking)
		screen_redo = REDO_NONE;
}

/* Perform a resize operation. */
static void
do_resize(void)
{
	struct rsfont *r;
	struct rsfont *best = (struct rsfont *) NULL;

	/* What we're doing now is irreversible. */
	screen_redo = REDO_RESIZE;

	if (rsfonts == (struct rsfont *)NULL || !allow_resize) {
		/* Illegal or impossible. */
		if (rsfonts == (struct rsfont *)NULL)
			trace_event("  no fonts available for resize\n"
			    "    reasserting previous size\n");
		else
			trace_event("  resize prohibited by resource\n"
			    "    reasserting previous size\n");
		set_toplevel_sizes();
		return;
	}

	/*
	 * Recompute the resulting screen area for each font, based on the
	 * current keypad, model, and scrollbar settings.
	 */
	for (r = rsfonts; r != (struct rsfont *) NULL; r = r->next) {
		Dimension cw, ch;	/* container_width, container_height */

		cw = SCREEN_WIDTH(r->width)+2 + scrollbar_width;
#if defined(X3270_KEYPAD) /*[*/
		{
			Dimension mkw;

			mkw = min_keypad_width();
			if (kp_placement == kp_integral
			    && appres.keypad_on
			    && cw < mkw)
				cw = mkw;
		}

#endif /*]*/
		ch = SCREEN_HEIGHT(r->height)+2 + menubar_qheight(cw);
#if defined(X3270_KEYPAD) /*[*/
		if (kp_placement == kp_integral && appres.keypad_on)
			ch += keypad_qheight();
#endif /*]*/
		r->total_width = cw;
		r->total_height = ch;
		r->area = cw * ch;
	}

	/*
	 * Find the the best match for requested dimensions.
	 *
	 * In past, the "best" was the closest area that was larger or
	 * smaller than the current area, whichever was requested.
	 *
	 * Now, if they try to shrink the screen, "they" might be the
	 * window manager enforcing a size restriction, so the "best"
	 * match is the largest window that fits within the requested
	 * dimensions.
	 *
	 * If they try to grow the screen, then the "best" is the
	 * smallest font that lies between the current and requested
	 * length in the requested dimension(s).
	 *
	 * An ambiguous request (one dimension larger and the other smaller)
	 * is taken to be a "larger" request.
	 */

	if ((cn.width <= main_width && cn.height <= main_height) ||
	    (cn.width > main_width && cn.height > main_height)) {
		/*
		 * Shrink or two-dimensional grow: Find the largest font which
		 * fits within the new boundaries.
		 *
		 * Note that a shrink in one dimension, with the other
		 * dimension matching, is considered a two-dimensional
		 * shrink.
		 */
		for (r = rsfonts; r != (struct rsfont *) NULL; r = r->next) {
			if (r->total_width <= cn.width &&
			    r->total_height <= cn.height) {
				if (best == NULL || r->area > best->area)
					best = r;
			}
		}
		/*
		 * If no font is small enough, see if there is a font smaller
		 * than the current font along the requested dimension(s),
		 * which is better than doing nothing.
		 */
		if (best == NULL) {
			for (r = rsfonts;
			     r != (struct rsfont *) NULL;
			     r = r->next) {
				if (cn.width < main_width &&
				    r->total_width > main_width)
					continue;
				if (cn.height < main_height &&
				    r->total_height > main_height)
					continue;
				if (best == NULL ||
				    r->area < best->area) {
					best = r;
				}
			}
		}
	} else {
		/*
		 * One-dimensional grow: Find the largest font which fits
		 * within the lengthened boundary, and don't constrain the
		 * other dimension.
		 *
		 * Note than an ambiguous change (grow in one dimensional and
		 * shrink in the other) is considered a one-dimensional grow.
		 *
		 * In either case, the "other" dimension is considered
		 * unconstrained.
		 */
		if (cn.width > main_width) {
			/* Wider. */
			for (r = rsfonts;
			     r != (struct rsfont *) NULL;
			     r = r->next) {
				if (r->total_width <= cn.width &&
				    (best == NULL ||
				     r->total_width > best->total_width)) {
					best = r;
				}
			}
		} else {
			/* Taller. */
			for (r = rsfonts;
			     r != (struct rsfont *) NULL;
			     r = r->next) {
				if (r->total_height <= cn.height &&
				    (best == NULL ||
				     r->total_height > best->total_height)) {
					best = r;
				}
			}
		}
	}

	/* Change fonts. */
	if (!best || (efontname && !strcmp(best->name, efontname))) {
		if (cn.width > main_width || cn.height > main_height)
			trace_event("  no larger font available\n"
			    "    reasserting previous size\n");
		else
			trace_event("  no smaller font available\n"
			    "    reasserting previous size\n");
		set_toplevel_sizes();
	} else {
		trace_event("    switching to font '%s', new size %dx%d\n",
		    best->name, best->total_width, best->total_height);
		screen_newfont(best->name, False, False);

		/* screen_newfont() sets screen_redo to REDO_FONT. */
		screen_redo = REDO_RESIZE;
	}
}

static void
revert_screen(void)
{
	const char *revert = CN;

	/* If there's a reconfiguration pending, try to undo it. */
	switch (screen_redo) {
	    case REDO_FONT:
		revert = "font";
		screen_newfont(redo_old_font, False, False);
		break;
#if defined(X3270_MENUS) /*[*/
	    case REDO_MODEL:
		revert = "model number";
		screen_change_model(redo_old_model,
		    redo_old_ov_cols, redo_old_ov_rows);
		break;
#endif /*]*/
#if defined(X3270_KEYPAD) /*[*/
	    case REDO_KEYPAD:
		revert = "keypad configuration";
		screen_showikeypad(appres.keypad_on = False);
		break;
#endif /*]*/
	    case REDO_SCROLLBAR:
		revert = "scrollbar configuration";
		if (toggled(SCROLL_BAR)) {
			toggle_toggle(&appres.toggle[SCROLL_BAR]);
			toggle_scrollBar(&appres.toggle[SCROLL_BAR],
			    TT_INTERACTIVE);
		}
		break;
	    case REDO_RESIZE:
		/* Changed fonts in response to a previous user resize. */
		trace_event("  size reassertion failed, window truncated\n"
		    "    doing nothing\n");
		screen_redo = REDO_NONE;
		return;
	    case REDO_NONE:
	        /* Initial configuration, or user-generated resize. */
		do_resize();
		return;
	    default:
		break;
	}

	/* Tell the user what we're doing. */
	if (revert != CN) {
		trace_event("    reverting to previous %s\n", revert);
		popup_an_error("Main window does not fit on the "
		    "X display\n"
		    "Reverting to previous %s", revert);
	}

	screen_redo = REDO_NONE;
}

static void
revert_later(XtPointer closure _is_unused, XtIntervalId *id _is_unused)
{
	revert_screen();
}

/*
 * Timeout routine called 0.5 sec after x3270 receives the last ConfigureNotify
 * message.  This is for window managers that use 'continuous' move or resize
 * actions.
 */

static void
stream_end(XtPointer closure _is_unused, XtIntervalId *id _is_unused)
{
	Boolean needs_moving = False;

	trace_event("Stream timer expired %hux%hu+%hd+%hd\n",
		cn.width, cn.height, cn.x, cn.y);

	/* Not ticking any more. */
	cn.ticking = False;

	/* Save the new coordinates in globals for next time. */
	if (cn.x != main_x || cn.y != main_y) {
		main_x = cn.x;
		main_y = cn.y;
		needs_moving = True;
	}

	/*
	 * If the dimensions are correct, do nothing, forget about any
	 * reconfig we may need to revert, and get out.
	 */
	if (cn.width == main_width && cn.height == main_height) {
		trace_event("  width and height match\n    doing nothing\n");
		screen_redo = REDO_NONE;
		goto done;
	}

	/*
	 * If the dimensions are bigger, perhaps we've gotten some extra
	 * decoration.  Be persistent
	 */
	if (cn.width >= main_width && cn.height >= main_height) {
		trace_event("  bigger\n    asserting desired size\n");
		set_toplevel_sizes();
		screen_redo = REDO_NONE;
	}

	/* They're not correct. */
	trace_event("  size mismatch, want %ux%u", main_width, main_height);

	revert_screen();

    done:
	if (needs_moving && !iconic) {
		keypad_move();
		{
		    	static Boolean first = True;

			if (first)
			    	first = False;
			else
			    	popups_move();
		}
	}
}

void
PA_ConfigureNotify_action(Widget w _is_unused, XEvent *event, String *params _is_unused,
    Cardinal *num_params _is_unused)
{
	XConfigureEvent *re = (XConfigureEvent *) event;
	Position xx, yy;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_ConfigureNotify_action, event, params, num_params);
#endif /*]*/

	/*
	 * Get the new window coordinates.  If the configure event reports it
	 * as (0,0), ask for it explicitly.
	 */
	if (re->x || re->y) {
		xx = re->x;
		yy = re->y;
	} else {
		XtVaGetValues(toplevel, XtNx, &xx, XtNy, &yy, NULL);
	}
	trace_event("ConfigureNotify %hux%hu+%hd+%hd\n",
	    re->width, re->height, xx, yy);
	
	/* Save the latest values. */
	cn.x = xx;
	cn.y = yy;
	cn.width = re->width;
	cn.height = re->height;

	/* Set the stream timer for 0.5 sec from now. */
	if (cn.ticking)
		XtRemoveTimeOut(cn.id);
	cn.id = XtAppAddTimeOut(appcontext, 500, stream_end, 0);
	cn.ticking = True;
}

/*
 * Process a VisibilityNotify event, setting the 'visibile' flag in nss.
 * This will switch the behavior of screen scrolling.
 */
void
PA_VisibilityNotify_action(Widget w _is_unused, XEvent *event _is_unused,
    String *params _is_unused, Cardinal *num_params _is_unused)
{
	XVisibilityEvent *e;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_VisibilityNotify_action, event, params, num_params);
#endif /*]*/
	e = (XVisibilityEvent *)event;
	nss.obscured = (e->state != VisibilityUnobscured);
}

/*
 * Process a GraphicsExpose event, refreshing the screen if we have done
 * one or more failed XCopyArea calls.
 */
void
PA_GraphicsExpose_action(Widget w _is_unused, XEvent *event _is_unused,
    String *params _is_unused, Cardinal *num_params _is_unused)
{
	int i;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_GraphicsExpose_action, event, params, num_params);
#endif /*]*/

	if (nss.copied) {
		/*
		 * Force a screen redraw.
		 */
		(void) memset((char *) ss->image, 0,
		              (maxROWS*maxCOLS) * sizeof(union sp));
		if (visible_control)
			for (i = 0; i < maxROWS*maxCOLS; i++)
				ss->image[i].bits.cc = EBC_space;
		ctlr_changed(0, ROWS*COLS);
		cursor_changed = True;

		nss.copied = False;
	}
}

/* Display size functions. */
unsigned
display_width(void)
{
	return XDisplayWidth(display, default_screen);
}

unsigned
display_widthMM(void)
{
	return XDisplayWidthMM(display, default_screen);
}

unsigned
display_height(void)
{
	return XDisplayHeight(display, default_screen);
}

unsigned
display_heightMM(void)
{
	return XDisplayHeightMM(display, default_screen);
}

/* Charset mapping for older 3270 fonts. */
static struct {
	const char *name;
	const char *cg;
} name2cs[] = {
	{ "3270",		"3270cg-1a" },
	{ "3270-12",		"3270cg-1" },
	{ "3270-12bold",	"3270cg-1" },
	{ "3270-20",		"3270cg-1" },
	{ "3270-20bold",	"3270cg-1" },
	{ "3270bold",		"3270cg-1a" },
	{ "3270d",		"3270cg-1a" },
	{ "3270gr",		"3270cg-7" },
	{ "3270gt12",		"3270cg-1" },
	{ "3270gt12bold",	"3270cg-1" },
	{ "3270gt16",		"3270cg-1" },
	{ "3270gt16bold",	"3270cg-1" },
	{ "3270gt24",		"3270cg-1" },
	{ "3270gt24bold",	"3270cg-1" },
	{ "3270gt32",		"3270cg-1" },
	{ "3270gt32bold",	"3270cg-1" },
	{ "3270gt8",		"3270cg-1" },
	{ "3270h",		"3270cg-8" },
	{ NULL,			NULL }

};

static const char *
name2cs_3270(const char *name)
{
	int i;

	for (i = 0; name2cs[i].name != NULL; i++) {
		if (!strcasecmp(name, name2cs[i].name))
			return name2cs[i].cg;
	}
	return NULL;
}

#if defined(X3270_DBCS) /*[*/
/* Translate an EBCDIC DBCS character to a display character. */
static void
xlate_dbcs(unsigned char c0, unsigned char c1, XChar2b *r)
{
	unsigned long u;
	int d;

	/* Translate NULLs to spaces. */
	if (c0 == EBC_null && c1 == EBC_null) {
		c0 = EBC_space;
		c1 = EBC_space;
	}
	/* Then handle special cases. */
	if ((c0 < 0x41 && (c0 != EBC_space && c1 != EBC_space)) || c0 == 0xff) {
		/* Junk. */
		r->byte1 = 0;
		r->byte2 = 0;
	}
	u = ebcdic_dbcs_to_unicode((c0 << 8) | c1, True);
	d = display16_lookup(dbcs_font.d16_ix, u);
	if (d >= 0) {
		r->byte1 = (d >> 8) & 0xff;
		r->byte2 = d & 0xff;
	} else {
		r->byte1 = 0;
		r->byte2 = 0;
	}

#if defined(_ST) /*[*/
	printf("EBC %02x%02x -> X11 font %02x%02x\n",
		c0, c1, r->byte1, r->byte2);
#endif /*]*/
}

static void
destroy_callback_func(XIM current_ic, XPointer client_data, XPointer call_data)
{
	ic = NULL;
	im = NULL;
	ic_focus = 0;

#if defined(_ST) /*[*/
	printf("destroy_callback_func\n");
#endif /*]*/
}

#define OTS_LEN		(sizeof(PT_OVER_THE_SPOT) - 1)

static void
im_callback(Display *display, XPointer client_data, XPointer call_data)
{
	XIMStyles *xim_styles = NULL;
	XIMCallback destroy;
	int i, j;
	XVaNestedList preedit_attr = NULL;
	XPoint spot;
	XRectangle local_win_rect;
	static im_style_t im_styles[] = {
		{ XIMPreeditNothing  | XIMStatusNothing,    PT_ROOT },
		{ XIMPreeditPosition | XIMStatusNothing,    PT_OVER_THE_SPOT },
		{ XIMPreeditArea     | XIMStatusArea,       PT_OFF_THE_SPOT },
		{ XIMPreeditCallbacks| XIMStatusCallbacks,  PT_ON_THE_SPOT },
		{ (XIMStyle)0,                              NULL }
	};
	char *im_style = (appres.preedit_type != CN)?
	    strip_whitespace(appres.preedit_type): PT_OVER_THE_SPOT;
	char c;

#if defined(_ST) /*[*/
	printf("im_callback\n");
#endif /*]*/

	if (!strcasecmp(im_style, "None"))
		return;

	/* Parse the offset value for OverTheSpot. */
	if (!strncasecmp(im_style, PT_OVER_THE_SPOT, OTS_LEN) &&
	    ((c = im_style[OTS_LEN]) == '+' ||
	     c == '-')) {
		ovs_offset = atoi(im_style + OTS_LEN);
		im_style = NewString(im_style);
		im_style[OTS_LEN] = '\0';
	}

	/* Open connection to IM server. */
	if ((im = XOpenIM(display, NULL, NULL, NULL)) == NULL) {
		popup_an_error("XOpenIM failed\nXIM-based input disabled");
		goto error_return;
	}

	destroy.callback = (XIMProc)destroy_callback_func;
	destroy.client_data = NULL;
	XSetIMValues(im, XNDestroyCallback, &destroy, NULL);

	/* Detect the input style supported by XIM server. */
	if (XGetIMValues(im, XNQueryInputStyle, &xim_styles, NULL) != NULL ||
			xim_styles == NULL) {
		popup_an_error("Input method doesn't support any styles\n"
			       "XIM-based input disabled");
		goto error_return;
	}
	for (i = 0; i < xim_styles->count_styles; i++) {
		for (j = 0; im_styles[j].description != NULL; j++) {
			if (im_styles[j].style ==
					xim_styles->supported_styles[i]) {
#if defined(_ST) /*[*/
				printf("XIM server supports input_style %s\n",
						im_styles[j].description);
#endif /*]*/
				break;
			}
		}
#if defined(_ST) /*[*/
		if (im_styles[j].description == NULL)
			printf("XIM server supports unknown input style %x\n",
				(unsigned)(xim_styles->supported_styles[i]));
#endif /*]*/
	}

	/* Set my preferred style. */
	for (j = 0; im_styles[j].description != NULL; j++) {
		if (!strcasecmp(im_styles[j].description, im_style)) {
			style = im_styles[j].style;
			break;
		}
	}
	if (im_styles[j].description == NULL) {
		popup_an_error("Input style '%s' not supported\n"
			       "XIM-based input disabled", im_style);
		goto error_return;
	}

	if (style == (XIMPreeditPosition | XIMStatusNothing)) {
		char *fsname;
		XFontSet fontset;
		char **charset_list;
		int charset_count;
		char *def_string;

		fsname = xs_buffer("-*-%s,-*-iso8859-1", efont_charset_dbcs);
		for (;;) {
#if defined(_ST) /*[*/
			printf("trying fsname: %s\n", fsname);
#endif /*]*/
			fontset = XCreateFontSet(display, fsname,
					&charset_list, &charset_count,
					&def_string);
			if (charset_count || fontset == NULL) {
				if (charset_count > 0) {
					int i;

					for (i = 0; i < charset_count; i++) {
#if defined(_ST) /*[*/
						printf("missing: %s\n",
							charset_list[0]);
#endif /*]*/
						fsname = xs_buffer("%s,-*-%s",
							fsname,
							charset_list[i]);
					}
					continue;

				}
				popup_an_error("Cannot create fontset '%s' "
					"for input context\n"
					"XIM-based input disabled",
					fsname);
				goto error_return;
			} else
				break;
		};

		spot.x = 0;
		spot.y = ovs_offset * nss.char_height;
		local_win_rect.x = 1;
		local_win_rect.y = 1;
		local_win_rect.width  = main_width;
		local_win_rect.height = main_height;
		preedit_attr = XVaCreateNestedList(0,
					XNArea, &local_win_rect,
					XNSpotLocation, &spot,
					XNFontSet, fontset,
					NULL);
	}

	/* Create IC. */
	ic = XCreateIC(im, XNInputStyle, style,
			XNClientWindow, nss.window,
			XNFocusWindow, nss.window,
			(preedit_attr) ? XNPreeditAttributes : NULL,
			preedit_attr,
			NULL);
	if (ic == NULL) {
		popup_an_error("Cannot create input context\n"
		               "XIM-based input disabled");
		goto error_return;
	}
	return;

    error_return:
	if (im != NULL) {
		XCloseIM(im);
		im = NULL;
		xim_error = True;
	}
}

static void
cleanup_xim(Boolean b _is_unused)
{
	if (ic != NULL)
		XDestroyIC(ic);
	if (im != NULL)
		XCloseIM(im);
}

static void
xim_init(void)
{
	char buf[1024];
	static Boolean xim_initted = False;
	char *s;

	if (!dbcs || xim_initted)
		return;

	xim_initted = True;

	s = setlocale(LC_CTYPE, "");
	if (s != NULL)
		s = NewString(s);
	Replace(locale_name, s);
	if (s == CN) {
		popup_an_error("setlocale(LC_CTYPE) failed\n"
		    "XIM-based input disabled");
		xim_error = True;
		return;
	}

	(void) memset(buf, '\0', sizeof(buf));
	if (appres.input_method != CN)
		(void) sprintf(buf, "@im=%s", appres.input_method);
	if (XSetLocaleModifiers(buf) == CN) {
		popup_an_error("XSetLocaleModifiers failed\n"
		    "XIM-based input disabled");
		xim_error = True;
	} else if (XRegisterIMInstantiateCallback(display, NULL, NULL, NULL,
				im_callback, NULL) != True) {
		popup_an_error("XRegisterIMInstantiateCallback failed\n"
			       "XIM-based input disabled");
		xim_error = True;
	}
	register_schange(ST_EXITING, cleanup_xim);
	return;
}

static void
send_spot_loc(void)
{
	XPoint spot;
	XVaNestedList preedit_attr;

	spot.x = (cursor_addr % COLS) * nss.char_width + hhalo;
	spot.y = ((cursor_addr / COLS) + ovs_offset) * nss.char_height + vhalo;
	preedit_attr = XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);
	XSetICValues(ic, XNPreeditAttributes, preedit_attr, NULL);
	XFree(preedit_attr);
}
#endif /*]*/

/* Change the window title. */
void
Title_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Title_action, event, params, num_params);

	if (check_usage(Title_action, *num_params, 1, 1) < 0)
		return;

	user_title = NewString(params[0]);
	XtVaSetValues(toplevel, XtNtitle, user_title, NULL);
}

/* Change the window state. */
void
WindowState_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	int state;

	action_debug(WindowState_action, event, params, num_params);

	if (check_usage(WindowState_action, *num_params, 1, 1) < 0)
		return;

	if (!strcasecmp(params[0], "Iconic"))
		state = True;
	else if (!strcasecmp(params[0], "Normal"))
		state = False;
	else {
		popup_an_error("%s argument must be Iconic or Normal",
		    action_name(WindowState_action));
	       return;
	}
	XtVaSetValues(toplevel, XtNiconic, state, NULL);
}
