/*
 * Copyright (c) 2015-2017 Paul Mattes.
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
 *		b3270's screen update logic.
 */

#include "globals.h"
#include "3270ds.h"

#include "appres.h"
#include "ctlr.h"
#include "ctlrc.h"
#include "ui_stream.h"
#include "lazya.h"
#include "screen.h"
#include "see.h"
#include "toggles.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"
#include "utf8.h"
#include "varbuf.h"
#include "xscroll.h"

/* Unicode circled A character. */
#define CIRCLED_A	0x24b6
/*
 * How many columns to span with redundant information to avoid near-adjacent
 * <attr> or <col> elements.
 */
#define RED_SPAN	16

/* How many columns of attr diff to join with a text diff. */
#define AM_MAX		16

#define XX_UNDERLINE	0x01	/* underlined */
#define XX_BLINK	0x02	/* blinking */
#define XX_HIGHLIGHT	0x04	/* highlighted */
#define XX_SELECTABLE	0x08	/* lightpen selectable */
#define XX_REVERSE	0x10	/* reverse video (3278) */
#define XX_WIDE		0x20	/* double-width character (DBCS) */
#define XX_ORDER	0x40	/* other visible order */
#define XX_PUA		0x80	/* private use area */

typedef struct {
    u_int ccode;	/* unicode character to display */
    u_char fg;		/* foreground color */
    u_char bg;		/* background color */
    u_char gr;		/* graphic representation */
    u_char xx;		/* unused (zero) */
} screen_t;

/* Row-difference region. */
typedef struct _rowdiff {
    struct _rowdiff *next;
    int start_col;
    int width;
    enum { RD_ATTR, RD_TEXT } reason;
} rowdiff_t;

static int saved_rows = 0;
static int saved_cols = 0;
static int last_rows = 0;
static int last_cols = 0;
static struct ea *saved_ea = NULL;
static screen_t *saved_s = NULL;
static bool saved_ea_is_empty = false;

static int sent_baddr = 0;
static int saved_baddr = 0;

static struct ea zero_ea;

static bool cursor_enabled = true;

static void screen_disp_cond(bool always);

/*
 * Compare two screen_t's for equality.
 */
static bool
ea_equal(screen_t *a, screen_t *b)
{
    return !memcmp(a, b, sizeof(screen_t));
}

/*
 * Compare just the attributes (not the character code) in two screen_t's
 * for equality.
 */
static bool
ea_equal_attrs(screen_t *a, screen_t *b)
{
    screen_t ax = *a;	/* struct copy */
    screen_t bx = *b;	/* struct copy */

    ax.ccode = bx.ccode = 0;
    return ea_equal(&ax, &bx);
}

static char *
see_gr(u_char gr)
{
    varbuf_t r;
    char *sep = "";

    if (gr == 0) {
	return "default";
    }

    vb_init(&r);
    if (gr & XX_UNDERLINE) {
	vb_appends(&r, "underline");
	sep = ",";
    }
    if (gr & XX_BLINK) {
	vb_appendf(&r, "%sblink", sep);
	sep = ",";
    }
    if (gr & XX_HIGHLIGHT) {
	vb_appendf(&r, "%shighlight", sep);
	sep = ",";
    }
    if (gr & XX_SELECTABLE) {
	vb_appendf(&r, "%sselectable", sep);
	sep = ",";
    }
    if (gr & XX_REVERSE) {
	vb_appendf(&r, "%sreverse", sep);
	sep = ",";
    }
    if (gr & XX_WIDE) {
	vb_appendf(&r, "%swide", sep);
	sep = ",";
    }
    if (gr & XX_ORDER) {
	vb_appendf(&r, "%sorder", sep);
	sep = ",";
    }
    if (gr & XX_PUA) {
	vb_appendf(&r, "%spua", sep);
	sep = ",";
    }
    return lazya(vb_consume(&r));
}

/* Save empty screen state. */
static void
save_empty(void)
{
    size_t se = ROWS * COLS * sizeof(struct ea);
    size_t ss = maxROWS * maxCOLS * sizeof(screen_t);
    int i;

    /* Zero saved_ea. */
    Replace(saved_ea, (struct ea *)Malloc(se));
    memset(saved_ea, 0, se);
    saved_rows = ROWS;
    saved_cols = COLS;
    saved_ea_is_empty = true;

    /* Erase saved_s. */
    Replace(saved_s, (screen_t *)Malloc(ss));
    memset(saved_s, 0, ss);
    for (i = 0; i < maxROWS * maxCOLS; i++) {
	saved_s[i].ccode = ' ';
	saved_s[i].fg = appres.m3279? HOST_COLOR_BLUE: HOST_COLOR_NEUTRAL_WHITE;
	saved_s[i].bg = HOST_COLOR_NEUTRAL_BLACK;
    }
}

/* Emit an erase indication. */
static void
emit_erase(int rows, int cols)
{
    bool switched = rows > 0 && cols > 0;

    ui_vleaf("erase",
	    "logical-rows", switched? lazyaf("%d", rows) : NULL,
	    "logical-cols", switched? lazyaf("%d", cols) : NULL,
	    "fg", appres.m3279? "blue": NULL,
	    "bg", appres.m3279? "neutralBlack": NULL,
	    NULL);
}

/* Toggle the VISIBLE_CONTROL setting. */
static void
toggle_visibleControl(toggle_index_t ix _is_unused,
	enum toggle_type tt _is_unused)
{
    screen_disp_cond(true);
}       


/* Internal screen initialization. */
static void
internal_screen_init(void)
{
    ui_vleaf("screen-mode",
	    "model", lazyaf("%d", model_num),
	    "rows", lazyaf("%d", maxROWS),
	    "cols", lazyaf("%d", maxCOLS),
	    "color", appres.m3279? "true": "false",
	    "oversize", ov_rows || ov_cols? "true": "false",
	    "extended", appres.extended? "true": "false",
	    NULL);

    emit_erase(maxROWS, maxCOLS);

    last_rows = maxROWS;
    last_cols = maxCOLS;

    scroll_buf_init(); /* XXX: What about changing the model? */

    save_empty();
}

/* Screen initialization. */
void
screen_init(void)
{
    static toggle_register_t toggles[] = {
	{ VISIBLE_CONTROL,	toggle_visibleControl, 0 }
    };

    /* Register toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Do internal initialization. */
    internal_screen_init();
}

/* Charset change handler. */
void
b3270_new_charset(bool unused _is_unused)
{
    screen_disp_cond(true);
}

/*
 * Map default 3279 colors.  This code is duplicated three times. ;-(
 */
static int
color_from_fa(unsigned char fa)
{
    static int field_colors[4] = {
	HOST_COLOR_GREEN,        	/* default */
	HOST_COLOR_RED,          	/* intensified */
	HOST_COLOR_BLUE,         	/* protected */
	HOST_COLOR_NEUTRAL_WHITE	/* protected, intensified */
#       define DEFCOLOR_MAP(f) \
	((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))
};

    if (appres.m3279) {
	return field_colors[DEFCOLOR_MAP(fa)];
    } else {
	return HOST_COLOR_NEUTRAL_WHITE;
    }
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

/*
 * Test a character for an APL underlined alphabetic mapped to a circled
 * alphabetic.
 */
static bool
is_apl_underlined(unsigned char cs, unsigned long uc)
{
    return ((cs & CS_GE) || ((cs & CS_MASK) == CS_APL)) &&
	uc >= CIRCLED_A &&
	uc < CIRCLED_A + 26;
}

/*
 * Remap a circled alphabetic to a plain alphabetic.
 */
static unsigned long
uncircle(unsigned long uc)
{
    return 'A' + (uc - CIRCLED_A);
}

/*
 * Render the screen into a buffer.
 *
 * ea: ROWS*COLS screen buffer to render
 * s: maxROWS*maxCOLS screen_t to render into
 */
void
render_screen(struct ea *ea, screen_t *s)
{
    int i;
    unsigned long uc;
    int fa_addr = find_field_attribute(0);
    unsigned char fa = ea[fa_addr].fa;
    int fa_fg;
    int fa_bg;
    int fa_gr;
    bool fa_high;

    /* Start with all blanks, blue on black. */
    memset(s, 0, maxROWS * maxCOLS * sizeof(screen_t));
    for (i = 0; i < maxROWS * maxCOLS; i++) {
	s[i].ccode = ' ';
	s[i].fg = appres.m3279? HOST_COLOR_BLUE : HOST_COLOR_NEUTRAL_WHITE;
	s[i].bg = HOST_COLOR_NEUTRAL_BLACK;
    }

    if (ea[fa_addr].fg) {
	fa_fg = ea[fa_addr].fg & 0x0f;
    } else {
	fa_fg = color_from_fa(fa);
    }

    if (ea[fa_addr].bg) {
	fa_bg = ea[fa_addr].bg & 0x0f;
    } else {
	fa_bg = HOST_COLOR_NEUTRAL_BLACK;
    }

    if (ea[fa_addr].gr & GR_INTENSIFY) {
	fa_high = true;
    } else {
	fa_high = FA_IS_HIGH(fa);
    }

    fa_gr = ea[fa_addr].gr;

    for (i = 0; i < ROWS * COLS; i++) {
	int fg_color, bg_color;
	bool high;
	bool dbcs = false;
	bool order = false;
	bool extra_underline = false;

	uc = 0;

	if (ea[i].fa) {
	    uc = ' ';
	    fa = ea[i].fa;
	    if (ea[i].fg) {
		fa_fg = ea[i].fg & 0x0f;
	    } else {
		fa_fg = color_from_fa(fa);
	    }
	    if (ea[i].bg) {
		fa_bg = ea[i].bg & 0x0f;
	    } else {
		fa_bg = HOST_COLOR_NEUTRAL_BLACK;
	    }
	    if (ea[i].gr & GR_INTENSIFY) {
		fa_high = true;
	    } else {
		fa_high = FA_IS_HIGH(fa);
	    }
	    fa_gr = ea[i].gr;
	} else if (FA_IS_ZERO(fa)) {
	    if (ctlr_dbcs_state(i) == DBCS_LEFT) {
		uc = 0x3000;
		dbcs = true;
	    } else {
		uc = ' ';
	    }
	} else {
	    /* Convert EBCDIC to Unicode. */
	    switch (ctlr_dbcs_state(i)) {
	    case DBCS_NONE:
	    case DBCS_SI:
	    case DBCS_SB:
		if (toggled(VISIBLE_CONTROL)) {
		    switch (ea[i].cc) {
		    case EBC_null:
			uc = '.';
			order = true;
			break;
		    case EBC_so:
			uc = '<';
			order = true;
			break;
		    case EBC_si:
			uc = '>';
			order = true;
			break;
		    }
		}
		switch (ea[i].cc) {
		case EBC_dup:
		    uc = '*';
		    extra_underline = true;
		    order = true;
		    break;
		case EBC_fm:
		    uc = ';';
		    extra_underline = true;
		    order = true;
		    break;
		}
		if (!order) {
		    uc = ebcdic_to_unicode(ea[i].cc, ea[i].cs, EUO_APL_CIRCLED);
		    if (is_apl_underlined(ea[i].cs, uc)) {
			uc = uncircle(uc);
			extra_underline = true;
		    }
		    if (uc == 0) {
			uc = ' ';
		    }
		}
		break;
	    case DBCS_LEFT:
		uc = ebcdic_to_unicode((ea[i].cc << 8) | ea[i + 1].cc,
			CS_BASE, EUO_NONE);
		if (uc == 0) {
		    uc = 0x3000;
		}
		dbcs = true;
		break;
	    case DBCS_RIGHT:
		uc = 0;
		dbcs = true;
		break;
	    default:
		uc = ' ';
		break;
	    }
	}

	if (ea[i].fg) {
	    fg_color = ea[i].fg & 0x0f;
	} else {
	    fg_color = fa_fg;
	}
	if (ea[i].bg) {
	    bg_color = ea[i].bg & 0x0f;
	} else {
	    bg_color = fa_bg;
	}
	if (ea[i].gr & GR_REVERSE) {
	    int tmp;

	    tmp = fg_color;
	    fg_color = bg_color;
	    bg_color = tmp;
	}

	if (ea[i].gr & GR_INTENSIFY) {
	    high = true;
	} else {
	    high = fa_high;
	}

	/* Draw this position. */
	{
	    int si = ((i / COLS) * maxCOLS) + (i % COLS);

	    s[si].ccode = (toggled(VISIBLE_CONTROL) && ea[i].fa)?
		visible_fa(ea[i].fa): uc;
	    s[si].fg = appres.m3279? fg_color: HOST_COLOR_NEUTRAL_WHITE;
	    s[si].bg = appres.m3279? bg_color: HOST_COLOR_NEUTRAL_BLACK;
	    s[si].xx = 0;
	    s[si].gr = 0;

	    if ((fa_gr | ea[i].gr) & GR_UNDERLINE) {
		s[si].gr |= XX_UNDERLINE;
	    }
	    if ((fa_gr | ea[i].gr) & GR_BLINK) {
		s[si].gr |= XX_BLINK;
	    }
	    if (high) {
		s[si].gr |= XX_HIGHLIGHT;
	    }
	    if (FA_IS_SELECTABLE(fa)) {
		s[si].gr |= XX_SELECTABLE;
	    }
	    if (!appres.m3279 && ((fa_gr | ea[i].gr) & GR_REVERSE)) {
		s[si].gr |= XX_REVERSE;
	    }
	    if (dbcs) {
		s[si].gr |= XX_WIDE;
	    }
	    if (order || (toggled(VISIBLE_CONTROL) && ea[i].fa)) {
		s[si].gr |= XX_ORDER;
	    }
	    if (extra_underline) {
		s[si].gr |= XX_UNDERLINE | XX_PUA;
	    }
	}
    }
}

/* Generate one row's worth of raw diffs. */
static rowdiff_t *
generate_rowdiffs(screen_t *oldr, screen_t *newr)
{
    int col;
    rowdiff_t *diffs = NULL;
    rowdiff_t *last_diff = NULL;

    for (col = 0; col < maxCOLS; col++) {
	rowdiff_t *d;

	if (ea_equal(&oldr[col], &newr[col])) {
	    continue;
	}

	d = (rowdiff_t *)Malloc(sizeof(rowdiff_t));
	d->next = NULL;
	d->start_col = col;
	d->width = 1;

	if (oldr[col].ccode != newr[col].ccode) {
	    /* Text diff. */
	    int xcol;

	    d->reason = RD_TEXT;
	    for (xcol = col + 1; xcol < maxCOLS; xcol++) {

		if (oldr[xcol].ccode != newr[xcol].ccode &&
			ea_equal_attrs(&newr[col], &newr[xcol]) &&
			ea_equal_attrs(&oldr[col], &oldr[xcol])) {
		    d->width++;
		} else {
		    break;
		}
	    }
	} else {
	    /* Attr diff. */
	    int xcol;

	    d->reason = RD_ATTR;
	    for (xcol = col + 1; xcol < maxCOLS; xcol++) {
		if (oldr[xcol].ccode == newr[xcol].ccode &&
			ea_equal_attrs(&newr[col], &newr[xcol]) &&
			ea_equal_attrs(&oldr[col], &oldr[xcol])) {
		    d->width++;
		} else {
		    break;
		}
	    }
	}
	if (last_diff != NULL) {
	    last_diff->next = d;
	} else {
	    diffs = d;
	}
	last_diff = d;
	
	/* Skip over what we just generated. */
	col += d->width - 1;
    }

    return diffs;
}

/*
 * Compare the attributes between the end of 'd' and the beginning of 'next'.
 */
static bool
ea_equal_attrs_span(screen_t *oldr, screen_t *newr, rowdiff_t *d,
	rowdiff_t *next)
{
    int i;

    for (i = d->start_col + d->width; i < next->start_col; i++) {
	if (!ea_equal_attrs(&oldr[i], &oldr[d->start_col]) ||
            !ea_equal_attrs(&newr[i], &newr[d->start_col])) {
	    return false;
	}
    }
    return true;
}

/* Merge adjacent sets of diffs to minimize output. */
static rowdiff_t *
merge_adjacent(rowdiff_t *diffs, screen_t *oldr, screen_t *newr)
{
    rowdiff_t *d;
    rowdiff_t *next;

    for (d = diffs; d != NULL; d = next) {
	next = d->next;
	if (next == NULL) {
	    break;
	}

	/*
	 * Merge two text diffs if they are joined by a span of RED_SPAN or
	 * fewer matching cells and have the same attributes.
	 *
	 * But what if the intervening areas have different attributes from
	 * the first text diff?
	 */
	if (d->reason == RD_TEXT &&
		next->reason == RD_TEXT &&
		next->start_col - (d->start_col + d->width) <= RED_SPAN &&
		ea_equal_attrs(&oldr[d->start_col], &oldr[next->start_col]) &&
		ea_equal_attrs(&newr[d->start_col], &newr[next->start_col]) &&
		ea_equal_attrs_span(oldr, newr, d, next)) {

	    rowdiff_t *nx;

	    d->width = next->start_col + next->width - d->start_col;
	    nx = next;
	    d->next = next->next;
	    Free(nx);

	    /* Consider d again. */
	    next = d;
	    continue;
	}

	/*
	 * Merge a text diff with a small adjacent attr diff if their attrs
	 * match.
	 */
	if (d->reason == RD_TEXT &&
		next->reason == RD_ATTR &&
		next->width <= AM_MAX &&
		next->start_col == d->start_col + d->width &&
		ea_equal_attrs(&oldr[d->start_col], &oldr[next->start_col]) &&
		ea_equal_attrs(&newr[d->start_col], &newr[next->start_col])) {

	    rowdiff_t *nx;

	    d->width += next->width;
	    nx = next;
	    d->next = next->next;
	    Free(nx);

	    /* Consider d again. */
	    next = d;
	    continue;
	}

	/*
	 * Merge a small attr diff with an adjacent text diff if their attrs
	 * match, changing to a text diff when merging.
	 */
	if (d->reason == RD_ATTR &&
		d->width <= AM_MAX &&
		next->reason == RD_TEXT &&
		next->start_col == d->start_col + d->width &&
		ea_equal_attrs(&oldr[d->start_col], &oldr[next->start_col]) &&
		ea_equal_attrs(&newr[d->start_col], &newr[next->start_col])) {

	    rowdiff_t *nx;

	    d->reason = RD_TEXT;
	    d->width += next->width;
	    nx = next;
	    d->next = next->next;
	    Free(nx);

	    /* Consider d again. */
	    next = d;
	    continue;
	}
    }

    return diffs;
}

/* Emit encoded diffs. */
static void
emit_rowdiffs(screen_t *oldr, screen_t *newr, rowdiff_t *diffs)
{
    rowdiff_t *d;

    for (d = diffs; d != NULL; d = d->next) {
	const char *args[13]; /* col, fg, bg, gr, text, count, NULL */
	int aix = 0;
	char *col_value;

	args[aix++] = "col";
	col_value = xs_buffer("%d", d->start_col + 1);
	args[aix++] = col_value; /* will explicitly lazya below */
	if (oldr[d->start_col].fg != newr[d->start_col].fg) {
	    args[aix++] = "fg";
	    args[aix++] = see_color(0xf0 | newr[d->start_col].fg);
	}
	if (oldr[d->start_col].bg != newr[d->start_col].bg) {
	    args[aix++] = "bg";
	    args[aix++] = see_color(0xf0 | newr[d->start_col].bg);
	}
	if (oldr[d->start_col].gr != newr[d->start_col].gr) {
	    args[aix++] = "gr";
	    args[aix++] = see_gr(newr[d->start_col].gr);
	}

	if (d->reason == RD_TEXT) {
	    int i;
	    varbuf_t r;
	    char *ccode_value;
	    char utf8_buf[6];
	    int utf8_len;

	    vb_init(&r);
	    for (i = 0; i < d->width; i++) {
		if (newr[d->start_col + i].ccode == 0) {
		    /* DBCS right, skip it. */
		    continue;
		}
		utf8_len = unicode_to_utf8(newr[d->start_col + i].ccode,
			utf8_buf);
		vb_appendf(&r, "%.*s", utf8_len, utf8_buf);
	    }
	    args[aix++] = "text";
	    ccode_value = vb_consume(&r);
	    args[aix++] = ccode_value;
	    lazya(ccode_value);
	} else {
	    args[aix++] = "count";
	    args[aix++] = lazyaf("%d", d->width);
	}

	args[aix++] = NULL;
	ui_leaf((d->reason == RD_TEXT)? "char": "attr", args);
	lazya(col_value);
    }
}

static rowdiff_t *
free_rowdiffs(rowdiff_t *diffs)
{
    /* Free the diffs. */
    while (diffs != NULL) {
	rowdiff_t *next = diffs->next;
	Free(diffs);
	diffs = next;
    }
    return NULL;
}

/* Emit one row's worth of diffs. */
static void
emit_row(screen_t *oldr, screen_t *newr)
{
    rowdiff_t *diffs = NULL;

    /* Construct the sets of raw diffs. */
    diffs = generate_rowdiffs(oldr, newr);

    /* Merge adjacent diffs where it makes sense. */
    diffs = merge_adjacent(diffs, oldr, newr);

    /* Emit the diffs. */
    emit_rowdiffs(oldr, newr, diffs);

    /* Free the diffs. */
    diffs = free_rowdiffs(diffs);
}

/*
 * Emit the diff between two screens.
 */
static void
emit_diff(screen_t *old, screen_t *new)
{
    int row;

    ui_vpush("screen", NULL);

    for (row = 0; row < maxROWS; row++) {

	if (memcmp(old + (row * maxCOLS), new + (row * maxCOLS),
		sizeof(screen_t) * maxCOLS)) {
	    ui_vpush("row",
		    "row", lazyaf("%d", row + 1),
		    NULL);
	    emit_row(&old[row * maxCOLS], &new[row * maxCOLS]);
	    ui_pop();
	}
    }

    ui_pop();
}

/*
 * Move the cursor.
 */
void
cursor_move(int baddr)
{
    saved_baddr = baddr;
    cursor_addr = baddr;
}

/*
 * Display a changed screen, perhaps unconditionally.
 */
static void
screen_disp_cond(bool always)
{
    bool sent_erase = false;
    size_t se = ROWS * COLS * sizeof(struct ea);
    size_t ss = maxROWS * maxCOLS * sizeof(screen_t);
    bool empty;
    int i;
    screen_t *s;
    static bool xformatted = false;

    /* Check for a size change. */
    if (ROWS != last_rows || COLS != last_cols) {
	emit_erase(ROWS, COLS);
	last_rows = ROWS;
	last_cols = COLS;
	sent_erase = true;
	xformatted = false;
	save_empty();
    }

    /* Check for a cursor move. */
    if (cursor_enabled && sent_baddr != saved_baddr) {
	ui_vleaf("cursor",
	    "enabled", "true",
	    "row", lazyaf("%d", (saved_baddr / COLS) + 1),
	    "col", lazyaf("%d", (saved_baddr % COLS) + 1),
	    NULL);
	sent_baddr = saved_baddr;
    }

    /* Check for no change. */
    if (!always &&
	saved_rows == ROWS &&
	saved_cols == COLS &&
	!memcmp(saved_ea, ea_buf, se)) {
	return;
    }

    /* Check for now empty. */
    empty = true;
    for (i = 0; i < ROWS * COLS; i++) {
	if (memcmp(&ea_buf[i], &zero_ea, sizeof(struct ea))) {
	    empty = false;
	    break;
	}
    }
    if (empty) {
	if (!saved_ea_is_empty) {
	    /* Screen was not empty -- erase it now. */
	    if (!sent_erase) {
		emit_erase(-1, -1);
	    }
	    xformatted = false;
	}
	/* Remember that the screen is empty. */
	save_empty();
	return;
    }

    if (formatted != xformatted) {
	ui_vleaf("formatted",
		"state", formatted? "true": "false",
		NULL);
	xformatted = formatted;
    }

    /* Render the new screen. */
    s = Malloc(ss);
    render_screen(ea_buf, s);

    /* Tell them what the screen looks like now. */
    emit_diff(saved_s, s);

    /* Save the screen for next time. */
    Replace(saved_ea, Malloc(se));
    memcpy(saved_ea, ea_buf, se);
    saved_ea_is_empty = false;
    Replace(saved_s, s);
    saved_rows = ROWS;
    saved_cols = COLS;
}

/*
 * Display a changed screen.
 */
void
screen_disp(bool erasing _is_unused)
{
    screen_disp_cond(false);
}

/* Scrollbar support. */

/*
 * Enable or disable the cursor (scrolling)
 */
void
enable_cursor(bool on)
{
    if (on != cursor_enabled) {
	if (!(cursor_enabled = on)) {
	    ui_vleaf("cursor",
		"enabled", "false",
		NULL);
	    sent_baddr = -1;
	}
    }
}

/**
 * Set the scrollbar thumb.
 *
 * @param[in] top	Where the top of the scrollbar should be (percentage)
 * @param[in] shown	How much of the scrollbar to show (percentage)
 * @param[in] saved	Number of lines saved
 * @param[in] screen	Size of a screen
 * @param[in] back	Number of lines scrolled back
 *
 */
void
screen_set_thumb(float top, float shown, int saved, int screen, int back)
{
    static float last_top = -1.0;
    static float last_shown = -1.0;
    static int last_saved = -1;
    static int last_back = -1;

    if (top == last_top &&
	    shown == last_shown &&
	    saved == last_saved &&
	    back == last_back)
    {
	return;
    }
    last_top = top;
    last_shown = shown;
    last_saved = saved;
    last_back = back;
    ui_vleaf("thumb",
	    "top", lazyaf("%.5f", top),
	    "shown", lazyaf("%.5f", shown),
	    "saved", lazyaf("%d", saved),
	    "screen", lazyaf("%d", screen),
	    "back", lazyaf("%d", back),
	    NULL);
}
