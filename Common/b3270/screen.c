/*
 * Copyright (c) 2015-2024 Paul Mattes.
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
#include "resources.h"

#include "appres.h"
#include "b3270proto.h"
#include "ctlr.h"
#include "ctlrc.h"
#include "ui_stream.h"
#include "nvt.h"
#include "screen.h"
#include "see.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
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

#define XX_UNDERLINE	0x0001	/* underlined */
#define XX_BLINK	0x0002	/* blinking */
#define XX_HIGHLIGHT	0x0004	/* highlighted */
#define XX_SELECTABLE	0x0008	/* lightpen selectable */
#define XX_REVERSE	0x0010	/* reverse video (3278) */
#define XX_WIDE		0x0020	/* double-width character (DBCS) */
#define XX_ORDER	0x0040	/* visible order */
#define XX_PUA		0x0080	/* private use area */
#define XX_NO_COPY	0x0100	/* do not copy into paste buffer */
#define XX_WRAP		0x0200	/* NVT text wrapped here */

typedef struct {
    u_int ccode;	/* unicode character to display */
    u_char fg;		/* foreground color */
    u_char bg;		/* background color */
    u_short gr;		/* graphic representation */
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
see_gr(u_short gr)
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
	vb_appendf(&r, "%sprivate-use", sep);
	sep = ",";
    }
    if (gr & XX_NO_COPY) {
	vb_appendf(&r, "%sno-copy", sep);
	sep = ",";
    }
    if (gr & XX_WRAP) {
	vb_appendf(&r, "%swrap", sep);
	sep = ",";
    }
    return txdFree(vb_consume(&r));
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
	saved_s[i].fg = mode3279? HOST_COLOR_BLUE: HOST_COLOR_NEUTRAL_WHITE;
	saved_s[i].bg = HOST_COLOR_NEUTRAL_BLACK;
    }
}

/* Emit an erase indication. */
static void
emit_erase(int rows, int cols)
{
    bool switched = rows > 0 && cols > 0;

    ui_leaf(IndErase,
	    AttrLogicalRows, switched? AT_INT: AT_SKIP_INT, (int64_t)rows,
	    AttrLogicalColumns, switched? AT_INT: AT_SKIP_INT, (int64_t)cols,
	    AttrFg, AT_STRING, mode3279? "blue": NULL,
	    AttrBg, AT_STRING, mode3279? "neutralBlack": NULL,
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
    ui_leaf(IndScreenMode,
	    AttrModel, AT_INT, (int64_t)model_num,
	    AttrRows, AT_INT, (int64_t)maxROWS,
	    AttrColumns, AT_INT, (int64_t)maxCOLS,
	    AttrColor, AT_BOOLEAN, mode3279,
	    AttrOversize, AT_BOOLEAN, ov_rows || ov_cols,
	    AttrExtended, AT_BOOLEAN, appres.extended_data_stream,
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

/* Change models. */
void
screen_change_model(int mn, int ovc, int ovr)
{
    internal_screen_init();
}

/* Codepage change handler. */
void
b3270_new_codepage(bool unused _is_unused)
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

    if (mode3279) {
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
    ucs4_t uc;
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
	s[i].fg = mode3279? HOST_COLOR_BLUE : HOST_COLOR_NEUTRAL_WHITE;
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
	bool pua = false;
	bool no_copy = false;

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
	    if (is_nvt(&ea[i], false, &uc)) {
		/* NVT-mode text. */
		switch (ctlr_dbcs_state(i)) {
		case DBCS_RIGHT:
		    uc = 0;
		    dbcs = true;
		    break;
		case DBCS_LEFT:
		    dbcs = true;
		    /* fall through */
		default:
		    if (uc >= UPRIV2_Aunderbar && uc <= UPRIV2_Zunderbar) {
			uc -= UPRIV2;
			pua = true;
			extra_underline = true;
		    }
		    break;
		}
	    } else {
		/* Convert EBCDIC to Unicode. */
		switch (ctlr_dbcs_state(i)) {
		case DBCS_NONE:
		case DBCS_SI:
		case DBCS_SB:
		    switch (ea[i].ec) {
		    case EBC_null:
			if (toggled(VISIBLE_CONTROL)) {
			    uc = '.';
			    order = true;
			} else {
			    uc = ' ';
			}
			break;
		    case EBC_so:
			if (toggled(VISIBLE_CONTROL)) {
			    uc = '<';
			    order = true;
			    no_copy = true;
			} else {
			    uc = ' ';
			}
			break;
		    case EBC_si:
			if (toggled(VISIBLE_CONTROL)) {
			    uc = '>';
			    order = true;
			    no_copy = true;
			} else {
			    uc = ' ';
			}
			break;
		    case EBC_dup:
			uc = '*';
			pua = true;
			order = true;
			break;
		    case EBC_fm:
			uc = ';';
			pua = true;
			order = true;
			break;
		    }
		    if (!order) {
			uc = ebcdic_to_unicode(ea[i].ec, ea[i].cs,
				EUO_APL_CIRCLED);
			if (is_apl_underlined(ea[i].cs, uc)) {
			    uc = uncircle(uc);
			    extra_underline = true;
			    pua = true;
			}
			if (uc == 0) {
			    uc = ' ';
			}
		    }
		    break;
		case DBCS_LEFT:
		    uc = ebcdic_to_unicode((ea[i].ec << 8) | ea[i + 1].ec,
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
	if (!ea[i].fa && ((fa_gr | ea[i].gr) & GR_REVERSE)) {
	    int tmp;

	    tmp = fg_color;
	    fg_color = bg_color;
	    bg_color = tmp;
	}

	if ((fa_gr | ea[i].gr) & GR_INTENSIFY) {
	    high = true;
	} else {
	    high = fa_high;
	}

	/* Draw this position. */
	{
	    int si = ((i / COLS) * maxCOLS) + (i % COLS);

	    s[si].ccode = (toggled(VISIBLE_CONTROL) && ea[i].fa)?
		visible_fa(ea[i].fa): uc;
	    s[si].fg = mode3279? fg_color: HOST_COLOR_NEUTRAL_WHITE;
	    s[si].bg = mode3279? bg_color: HOST_COLOR_NEUTRAL_BLACK;

	    if (!ea[i].fa &&
		    !FA_IS_ZERO(fa) &&
		    ((fa_gr | ea[i].gr) & GR_UNDERLINE)) {
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
	    if (!mode3279 && ((fa_gr | ea[i].gr) & GR_REVERSE)) {
		s[si].gr |= XX_REVERSE;
	    }
	    if (dbcs) {
		s[si].gr |= XX_WIDE;
	    }
	    if (order || (toggled(VISIBLE_CONTROL) && ea[i].fa)) {
		s[si].gr |= XX_ORDER;
	    }
	    if (!ea[i].fa && !FA_IS_ZERO(fa) && extra_underline) {
		s[si].gr |= XX_UNDERLINE;
	    }
	    if (pua) {
		s[si].gr |= XX_PUA;
	    }
	    if (no_copy) {
		s[si].gr |= XX_NO_COPY;
	    }
	    if (ea[i].gr & GR_WRAP) {
		s[si].gr |= XX_WRAP;
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
	if (XML_MODE) {
	    uix_open_leaf((d->reason == RD_TEXT)? IndChar: IndAttr);
	} else {
	    uij_open_object(NULL);
	}
	ui_add_element(AttrColumn, AT_INT, (int64_t)(d->start_col + 1));
	if (oldr[d->start_col].fg != newr[d->start_col].fg) {
	    ui_add_element(AttrFg, AT_STRING,
		    see_color(0xf0 | newr[d->start_col].fg));
	}
	if (oldr[d->start_col].bg != newr[d->start_col].bg) {
	    ui_add_element(AttrBg, AT_STRING,
		    see_color(0xf0 | newr[d->start_col].bg));
	}
	if (oldr[d->start_col].gr != newr[d->start_col].gr) {
	    ui_add_element("gr", AT_STRING, see_gr(newr[d->start_col].gr));
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
	    ccode_value = vb_consume(&r);
	    txdFree(ccode_value);
	    ui_add_element(AttrText, AT_STRING, ccode_value);
	} else {
	    ui_add_element(AttrCount, AT_INT, (int64_t)d->width);
	}

	if (XML_MODE) {
	    uix_close_leaf();
	} else {
	    uij_close_object();
	}
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
 * Emit a cursor move indication, with or without its own screen update
 * wrapper.
 */
static void
emit_cursor_cond(bool with_screen)
{
    /* Check for a cursor move. */
    if (cursor_enabled && sent_baddr != saved_baddr) {
	if (with_screen) {
	    if (XML_MODE) {
		uix_push(IndScreen, NULL);
	    } else {
		uij_open_object(NULL);
		uij_open_object(IndScreen);
	    }
	}
	ui_leaf(IndCursor,
	    AttrEnabled, AT_BOOLEAN, true,
	    AttrRow, AT_INT, (int64_t)((saved_baddr / COLS) + 1),
	    AttrColumn, AT_INT, (int64_t)((saved_baddr % COLS) + 1),
	    NULL);
	sent_baddr = saved_baddr;
	if (with_screen) {
	    if (XML_MODE) {
		uix_pop();
	    } else {
		uij_close_object();
		uij_close_object();
	    }
	}
    }
}

/*
 * Emit the diff between two screens.
 */
static void
emit_diff(screen_t *old, screen_t *new)
{
    int row;

    if (XML_MODE) {
	uix_push(IndScreen, NULL);
    } else {
	uij_open_object(NULL);
	uij_open_object(IndScreen);
    }
    emit_cursor_cond(false);
    if (JSON_MODE) {
	uij_open_array(IndRows);
    }

    for (row = 0; row < maxROWS; row++) {

	if (memcmp(old + (row * maxCOLS), new + (row * maxCOLS),
		sizeof(screen_t) * maxCOLS)) {
	    if (XML_MODE) {
		uix_push(IndRow,
		    AttrRow, AT_INT, (int64_t)(row + 1),
		    NULL);
	    } else {
		uij_open_object(NULL);
		ui_add_element(AttrRow, AT_INT, (int64_t)(row + 1));
		uij_open_array(IndChanges);
	    }
	    emit_row(&old[row * maxCOLS], &new[row * maxCOLS]);
	    if (XML_MODE) {
		uix_pop();
	    } else {
		uij_close_array();
		uij_close_object();
	    }
	}
    }

    if (XML_MODE) {
	uix_pop();
    } else {
	uij_close_array();
	uij_close_object();
	uij_close_object();
    }
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

    /* Check for no change. */
    if (!always &&
	saved_rows == ROWS &&
	saved_cols == COLS &&
	!memcmp(saved_ea, ea_buf, se)) {
	emit_cursor_cond(true);
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
	emit_cursor_cond(true);
	return;
    }

    if (formatted != xformatted) {
	ui_leaf(IndFormatted,
		AttrState, AT_BOOLEAN, formatted,
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

/*
 * Scroll the screen.
 */
void
screen_scroll(unsigned char fg, unsigned char bg)
{
    int i;

    if (!fg) {
	fg = mode3279? HOST_COLOR_BLUE: HOST_COLOR_NEUTRAL_WHITE;
    }
    if (!bg) {
	bg = HOST_COLOR_NEUTRAL_BLACK;
    }

    /* Scroll saved_ea. */
    if (!saved_ea_is_empty) {
	memmove(saved_ea, saved_ea + COLS,
		(ROWS - 1) * COLS * sizeof(struct ea));
	memset(saved_ea + (ROWS - 1) * COLS, 0, COLS * sizeof(struct ea));
	for (i = 0; i < COLS; i++) {
	    saved_ea[((ROWS - 1) * COLS) + i].fg = 0xf0 | fg;
	    saved_ea[((ROWS - 1) * COLS) + i].bg = 0xf0 | bg;
	}
    }

    /* Scroll saved_s. */
    memmove(saved_s, saved_s + COLS,
	    (maxROWS - 1) * maxCOLS * sizeof(screen_t));
    memset(saved_s + (maxROWS - 1) * maxCOLS, 0, maxCOLS * sizeof(screen_t));
    for (i = 0; i < maxCOLS; i++) {
	int j = ((maxROWS - 1) * maxCOLS) + i;

	saved_s[j].ccode = ' ';
	saved_s[j].fg = fg & ~0xf0;
	saved_s[j].bg = bg & ~0xf0;
    }

    /* Tell the UI. */
    ui_leaf(IndScroll,
	    AttrFg, AT_STRING, see_color(0xf0 | fg),
	    AttrBg, AT_STRING, see_color(0xf0 | bg),
	    NULL);
}

/* Left-to-right swap support. */
void
screen_flip(void)
{
    flipped = !flipped;
    ui_leaf(IndFlipped,
	    AttrValue, AT_BOOLEAN, flipped,
	    NULL);
}

bool
screen_flipped(void)
{
    return flipped;
}

/* Scrollbar support. */

/**
 * Enable or disable the cursor.
 *
 * @param[in] on	Enable (true) or disable (false) the cursor display.
 */
void
enable_cursor(bool on)
{
    if (on != cursor_enabled) {
	if (!(cursor_enabled = on)) {
	    if (XML_MODE) {
		uix_push(IndScreen, NULL);
	    } else {
		uij_open_object(NULL);
		uij_open_object(IndScreen);
	    }
	    ui_leaf(IndCursor,
		AttrEnabled, AT_BOOLEAN, false,
		NULL);
	    if (XML_MODE) {
		uix_pop();
	    } else {
		uij_close_object();
		uij_close_object();
	    }
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
    ui_leaf(IndThumb,
	    AttrTop, AT_DOUBLE, (double)top,
	    AttrShown, AT_DOUBLE, (double)shown,
	    AttrSaved, AT_INT, (int64_t)saved,
	    AttrScreen, AT_INT, (int64_t)screen,
	    AttrBack, AT_INT, (int64_t)back,
	    NULL);
}
