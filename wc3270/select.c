/*
 * Copyright (c) 2013-2024 Paul Mattes.
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
 *	select.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Screen selections
 */
#include "globals.h"
#include <assert.h>
#include <wchar.h>

#include "3270ds.h"
#include "appres.h"

#include "actions.h"
#include "cscreen.h"
#include "ctlr.h"
#include "ctlrc.h"
#include "kybd.h"
#include "names.h"
#include "nvt.h"
#include "popups.h"
#include "screen.h"
#include "toggles.h"
#include "toupper.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"
#include "winvers.h"
#include "wselectc.h"

#include "selectc.h"

/* Unicode DBCS (double-width) blank. */
#define IDEOGRAPHIC_SPACE	0x3000

#define HTTP_PREFIX    "http://"
#define HTTPS_PREFIX   "https://"

static char *s_pending;
static char *s_onscreen;

/* Event names. */
static char *event_name[] = {
    "BUTTON_DOWN",
    "RIGHT_BUTTON_DOWN",
    "BUTTON_UP",
    "MOVE",
    "DOUBLE_CLICK"
};

/* If true, we are rubber-banding a selection right now. */
static bool rubber_banding = false;

/* If true, we have a stored start point. */
static bool select_started = false;

/* If true, the current selection was from a double-click. */
static bool word_selected = false;

/* Start of selected area. */
static int select_start_row;
static int select_start_col;

/* End of selected area. */
static int select_end_row;
static int select_end_col;

/* True if the mouse moved while the button was down. */
static bool moved;

/*
 * Initialize the selection logic, given the maximum screen dimensions.
 */
void
select_init(unsigned max_rows, unsigned max_cols)
{
    s_pending = Malloc(max_rows * max_cols);
    s_onscreen = Malloc(max_rows * max_cols);
    unselect(0, max_rows * max_cols);
    memset(s_onscreen, 0, max_rows * max_cols);
}

/*
 * Inform the selection logic of a screen clear.
 * This does not include redisplaying the screen.
 */
void
unselect(int baddr, int len)
{
    /*
     * Technically, only the specified area has changed, but intuitively,
     * the whole selected rectangle has.
     */
    rubber_banding = false;
    select_started = false;
    word_selected = false;
    memset(s_pending, 0, ROWS * COLS);
    screen_changed = true;
    st_changed(ST_SELECTING, false);
}

static void
reselect(bool generate_event)
{
    bool any = false;

    /* Clear out the current selection. */
    memset(s_pending, 0, ROWS * COLS);

    /* Fill in from start to end, which may be backwards. */

    if (ever_3270) {
	/* Rectangular selections. */
	int rowA, colA, rowZ, colZ;
	int row, col;

	rowA = (select_start_row < select_end_row)?
	    select_start_row: select_end_row;
	rowZ = (select_start_row > select_end_row)?
	    select_start_row: select_end_row;
	colA = (select_start_col < select_end_col)?
	    select_start_col: select_end_col;
	colZ = (select_start_col > select_end_col)?
	    select_start_col: select_end_col;
	for (row = rowA; row <= rowZ; row++) {
	    for (col = colA; col <= colZ; col++) {
		s_pending[(row * COLS) + col] = 1;
		any = true;
	    }
	}
    } else {
	/* Continuous selections. */
	int baddrA = (select_start_row * COLS) + select_start_col;
	int baddrZ = (select_end_row * COLS) + select_end_col;
	int baddr;
	if (baddrA > baddrZ) {
	    baddr = baddrZ;

	    baddrZ = baddrA;
	    baddrA = baddr;
	}

	for (baddr = baddrA; baddr <= baddrZ; baddr++) {
	    s_pending[baddr] = 1;
	    any = true;
	}
    }

    screen_changed = true;
    if (generate_event && any) {
	st_changed(ST_SELECTING, true);
    }
}

/*
 * Returns true if the character at the given location is displayed as a
 * blank.
 */
static bool
is_blank(int baddr)
{
    ucs4_t u;
    unsigned char fa;
    int xbaddr;
    int c;

    /* Check for FA or blanked field. */
    fa = get_field_attribute(baddr);
    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
	return true;
    }

    /* Handle NVT-mode text. */
    if ((u = ea_buf[baddr].ucs4)) {
	if (IS_LEFT(baddr)) {
	    return u == IDEOGRAPHIC_SPACE;
	} else if (IS_RIGHT(baddr)) {
	    xbaddr = baddr;
	    INC_BA(xbaddr);
	    return ea_buf[xbaddr].ucs4 == IDEOGRAPHIC_SPACE;
	} else {
	    return u == ' ' || u == 0xa0;
	}
    }

    /* Translate to Unicode, exactly as we would display it. */
    if (IS_LEFT(baddr)) {
	xbaddr = baddr;
	DEC_BA(xbaddr);
	c = ebcdic_to_unicode((ea_buf[xbaddr].ec << 8) |
			      ea_buf[baddr].ec,
			      CS_BASE, EUO_NONE);
	if (c == 0 || c == IDEOGRAPHIC_SPACE) {
	    return true;
	}
    } else if (IS_RIGHT(baddr)) {
	xbaddr = baddr;
	INC_BA(xbaddr);
	c = ebcdic_to_unicode((ea_buf[baddr].ec << 8) |
			      ea_buf[xbaddr].ec,
			      CS_BASE, EUO_NONE);
	if (c == 0 || c == IDEOGRAPHIC_SPACE) {
	    return true;
	}
    } else {
	c = ebcdic_to_unicode(ea_buf[baddr].ec,
		ea_buf[baddr].cs,
		appres.c3270.ascii_box_draw?
		    EUO_ASCII_BOX: 0);
	if (c == 0 || c == ' ') {
	    return true;
	}
    }

    return false;
}

/*
 * Test a character for being valid in a URL.
 */
static bool
is_url_char(ucs4_t u)
{
    return u > ' ' && u <= 0xff &&
	strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:@[]/", u) != NULL;
}

/*
 * Test a screen location for holding a valid URL character.
 */
static bool
is_url_ea(int baddr)
{
    if (ea_buf[baddr].cs != CS_BASE) {
	return false;
    }
    return is_url_char(ea_buf[baddr].ucs4?
		ea_buf[baddr].ucs4:
		ebcdic_to_unicode(ea_buf[baddr].ec, ea_buf[baddr].cs, EUO_NONE));
}

/*
 * Test a double-click for a URL, and execute it if found.
 *
 * Returns true if a URL was found.
 */
static bool
url_click(int row, int col)
{
    int baddr = (row * COLS) + col;
    int first, last;
    char *url;
    int i;
    char *command;
    int rc;

    if (!is_url_ea(baddr)) {
	return false;
    }

    /* Look left, including NVT-mode wrapping. */
    first = baddr;
    while (true) {
	if (!(first % COLS)) {
	    if (!first) {
		break;
	    }
	    if (!(ea_buf[first - 1].gr & GR_WRAP)) {
		break;
	    }
	}
	if (!is_url_ea(first - 1)) {
	    break;
	}
	first--;
    }

    /* Look right, including NVT-mode wrapping. */
    last = baddr;
    while (true) {
	if (!is_url_ea(last)) {
	    last--;
	    break;
	}
	if (!((last + 1) % COLS)) {
	    if (last == ROWS * COLS) {
		break;
	    }
	    if (!(ea_buf[last].gr & GR_WRAP)) {
		break;
	    }
	}
	last++;
    }

    /* Extract the string. */
    url = Malloc(last - first + 2);
    for (i = 0; i < last - first + 1; i++) {
	url[i] = (ea_buf[first + i].ucs4?
		ea_buf[first + i].ucs4:
		ebcdic_to_unicode(ea_buf[first + i].ec, ea_buf[first + i].cs, EUO_NONE)) & 0xff;
    }
    url[i] = '\0';

    if (strncmp(url, HTTP_PREFIX, strlen(HTTP_PREFIX)) &&
	    strncmp(url, HTTPS_PREFIX, strlen(HTTPS_PREFIX))) {
	Free(url);
	return false;
    }

    /* Launch the browser. */
    command = Asprintf("start \"browser\" \"%s\"", url);
    Free(url);
    vtrace("Starting URL: %s\n", url);
    rc = system(command);
    if (rc != 0) {
	popup_an_error("URL failed, return code %d", rc);
    }
    Free(command);

    /* Get back mouse events */
    screen_system_fixup();

    return true;
}

/*
 * Find the starting and ending columns of a 'word'.
 *
 * The rules, from Windows, are a bit strange.
 * A 'word' is a block of non-blank text, plus one blank to the right.
 * So if you double-click on a blank, you get just the blank, unless it is to
 * the right of a non-blank, in which case you get the word to the left as
 * well.
 */
static void
find_word_end(int row, int col, int *start_rowp, int *start_colp, int *end_rowp,
	int *end_colp)
{
    int baddr = (row * COLS) + col;

    assert(row <= ROWS);
    assert(col <= COLS);

    *start_rowp = row;
    *end_rowp = row;

    /*
     * If on a blank now, return just that, or that plus the word to the
     * left.
     */
    if (is_blank(baddr)) {
	*end_colp = col;
	while (col && !is_blank((row * COLS) + (col - 1))) {
	    col--;
	}
	while (!col &&
		row &&
		(ea_buf[(row * COLS) - 1].gr & GR_WRAP) &&
		!is_blank((row * COLS) - 1)) {
	    row--;
	    col = COLS - 1;
	    while (col && !is_blank((row * COLS) + (col - 1))) {
		col--;
	    }
	}
	*start_rowp = row;
	*start_colp = col;
	return;
    }

    /* Search left. */
    while (col && !is_blank((row * COLS) + (col - 1))) {
	col--;
    }
    while (!col &&
	    row &&
	    (ea_buf[(row * COLS) - 1].gr & GR_WRAP) &&
	    !is_blank((row * COLS) - 1)) {
	row--;
	col = COLS - 1;
	while (col && !is_blank((row * COLS) + (col - 1))) {
	    col--;
	}
    }
    *start_rowp = row;
    *start_colp = col;

    /* Search right. */
    while (col < (COLS - 1) && !is_blank((row * COLS) + (col + 1))) {
	col++;
    }
    while (col == (COLS - 1) &&
	    (row < (ROWS - 1)) &&
	    (ea_buf[(row * COLS) + col].gr & GR_WRAP) &&
	    !is_blank(((row + 1) * COLS))) {
	row++;
	col = 0;
	while (col < (COLS - 1) && !is_blank((row * COLS) + (col + 1))) {
	    col++;
	}
    }

    if (col < (COLS -1)) {
	col++;
    }
    *end_rowp = row;
    *end_colp = col;
}

/*
 * Pass a mouse event to the select logic.
 *
 * Only the essentials of the event are passed in -- the row and column in
 * display coordinates (not screen coordinates), and the status of the left
 * mouse button. select_event() infers the user's actions from that.
 *
 * Returns true if the event was consumed, or false if it was a cursor-move
 * event (button up without movement).
 */
bool
select_event(unsigned row, unsigned col, select_event_t event, bool shift)
{
    static int click_cursor_addr = -1;

    assert((int)row <= ROWS);
    assert((int)col <= COLS);

    vtrace(" select_event(%u,%u,%s,%s)\n", row, col, event_name[event],
	    shift? "shift": "no-shift");

    if (!rubber_banding) {
	switch (event) {
	case SE_BUTTON_DOWN:
	    if (shift && select_started) {
		/* Extend selection. */
		vtrace("  Extending selection\n");
	    } else {
		vtrace("  New selection\n");
		select_start_row = row;
		select_start_col = col;

		/* If there was a previous selection, turn it off. */
		st_changed(ST_SELECTING, false);
	    }
	    rubber_banding = true;
	    select_started = true;
	    word_selected = false;
	    select_end_row = row;
	    select_end_col = col;
	    reselect(false);
	    moved = false;
	    break;
	case SE_DOUBLE_CLICK:
	    vtrace("  Word select\n");
	    if (toggled(SELECT_URL) && url_click(row, col)) {
		if (click_cursor_addr != -1) {
		    /* Move the cursor back from the first click. */
		    cursor_move(click_cursor_addr);
		    click_cursor_addr = -1;
		}
		break;
	    }
	    rubber_banding = false;
	    find_word_end(row, col, &select_start_row, &select_start_col, &select_end_row,
		    &select_end_col);
	    word_selected = true;
	    reselect(true);

	    /* If we moved the cursor for the first click, move it back now. */
	    if (click_cursor_addr != -1) {
		cursor_move(click_cursor_addr);
		    click_cursor_addr = -1;
	    }
	    break;
	case SE_RIGHT_BUTTON_DOWN:
	    if (memchr(s_pending, 1, COLS * ROWS) == NULL) {
		/* No selection pending: Paste. */
		vtrace("  Paste\n");
		run_action(AnPaste, IA_KEYMAP, NULL, NULL);
	    } else {
		/* Selection pending: Copy. */
		vtrace("  Copy\n");
		run_action(AnCopy, IA_KEYMAP, NULL, NULL);
	    }
	    break;
	default:
	    break;
	}
    } else {
	/* A selection is pending (rubber-banding). */
	switch (event) {
	case SE_BUTTON_UP:
	    rubber_banding = false;
	    word_selected = false;
	    if (row == select_start_row && col == select_start_col && !moved) {
		/*
		 * No movement. Call it a cursor move,
		 * but they might extend it later.
		 */
		s_pending[(row * COLS) + col] = 0;
		screen_changed = true;
		if (ever_3270) {
		    vtrace("  Cursor move\n");
		    click_cursor_addr = cursor_addr;
		}
		/* We did not consume the event. */
		return false;
	    }
	    vtrace("  Finish selection\n");
	    select_end_row = row;
	    select_end_col = col;
	    reselect(true);
	    moved = false;
	    break;
	case SE_MOVE:
	    /* Extend. */
	    vtrace("  Extend\n");
	    select_end_row = row;
	    select_end_col = col;
	    reselect(true);
	    moved = true;
	    break;
	default:
	    break;
	}
    }

    /* We consumed the event. */
    return true;
}

/**
 * Handle a Return key (usually marked Enter) for completing a select/copy
 * action.
 *
 * @return true if key consumed, false otherwise.
 */
bool
select_return_key(void)
{
    if (memchr(s_pending, 1, COLS * ROWS) != NULL) {
	run_action(AnCopy, IA_KEYMAP, NULL, NULL);
	return true;
    } else {
	return false;
    }
}

/*
 * Unicode text version of copy-to-clipboard.
 */
static size_t
copy_clipboard_unicode(LPTSTR lptstr)
{
    int r, c;
    int any_row = -1;
    int ns = 0;
    bool last_cjk_space = false;
    wchar_t *bp = (wchar_t *)lptstr;
    enum dbcs_state d;
    ucs4_t u;
    unsigned char fa;

    /* Fill in the buffer. */
    fa = get_field_attribute(0);
    for (r = 0; r < ROWS; r++) {
	for (c = 0; c < COLS; c++) {
	    int baddr = (r * COLS) + c;

	    if (ea_buf[baddr].fa) {
		fa = ea_buf[baddr].fa;
	    }
	    if (!s_pending[baddr]) {
		continue;
	    }
	    if (any_row >= 0 && any_row != r && !(ea_buf[baddr - 1].gr & GR_WRAP)) {
		*bp++ = '\r';
		*bp++ = '\n';
		ns = 0;
		last_cjk_space = false;
	    }
	    any_row = r;

	    d = ctlr_dbcs_state(baddr);
	    if (is_nvt(&ea_buf[baddr], appres.c3270.ascii_box_draw, &u)) {
		if (!IS_RIGHT(d)) {
		    if (!IS_LEFT(d) && toggled(MONOCASE)) {
			u = u_toupper(u);
		    }
		} else {
		    continue;
		}
	    } else {
		if (IS_LEFT(d)) {
		    int xbaddr = baddr;

		    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
			u = IDEOGRAPHIC_SPACE;
		    } else {
			xbaddr = baddr;
			INC_BA(xbaddr);
			u = ebcdic_to_unicode(
				(ea_buf[baddr].ec << 8) |
				    ea_buf[xbaddr].ec,
				CS_BASE, EUO_NONE);
			if (u == 0) {
			    u = IDEOGRAPHIC_SPACE;
			}
		    }
		} else if (!IS_RIGHT(d)) {
		    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
			u = ' ';
		    } else {
			u = ebcdic_to_unicode(ea_buf[baddr].ec,
				    ea_buf[baddr].cs,
				    appres.c3270.ascii_box_draw?
					EUO_ASCII_BOX: 0);
			if (u == 0) {
			    u = ' ';
			}
			if (toggled(MONOCASE)) {
			    u = u_toupper(u);
			}
		    }
		} else {
		    continue;
		}
	    }

	    if (u == ' ') {
		if (!word_selected || last_cjk_space) {
		    *bp++ = ' ';
		} else {
		    ns++;
		}
	    } else {
		while (ns) {
		    *bp++ = ' ';
		    ns--;
		}
		*bp++ = u;
		last_cjk_space = (u == IDEOGRAPHIC_SPACE);
	    }
	}
    }

    *bp++ = 0;
    return bp - (wchar_t *)lptstr;
}

/*
 * OEM text version of copy-to-clipboard.
 */
static size_t
copy_clipboard_oemtext(LPTSTR lptstr)
{
    int r, c;
    int any_row = -1;
    int ns = 0;
    bool last_cjk_space = false;
    char *bp = lptstr;
    enum dbcs_state d;
    wchar_t ch;
    ucs4_t u;
    unsigned char fa;

    /* Fill in the buffer. */
    fa = get_field_attribute(0);
    for (r = 0; r < ROWS; r++) {
	for (c = 0; c < COLS; c++) {
	    int baddr = (r * COLS) + c;

	    if (ea_buf[baddr].fa) {
		fa = ea_buf[baddr].fa;
	    }
	    if (!s_pending[baddr]) {
		continue;
	    }
	    if (any_row >= 0 && any_row != r && !(ea_buf[baddr - 1].gr & GR_WRAP)) {
		*bp++ = '\r';
		*bp++ = '\n';
		ns = 0;
		last_cjk_space = false;
	    }
	    any_row = r;
	    d = ctlr_dbcs_state(baddr);
	    if (is_nvt(&ea_buf[baddr], appres.c3270.ascii_box_draw, &u)) {
		/* NVT-mode text */
		if (IS_LEFT(d)) {
		    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
			u = IDEOGRAPHIC_SPACE;
		    }
		    while (ns) {
			*bp++ = ' ';
			ns--;
		    }
		    ch = u;
		    bp += WideCharToMultiByte(CP_OEMCP, 0, &ch, 1,
			    bp, 1, "?", NULL);
		    last_cjk_space = (u == IDEOGRAPHIC_SPACE);
		} else if (!IS_RIGHT(d)) {
		    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
			u = ' ';
		    } else if (toggled(MONOCASE)) {
			u = u_toupper(u);
		    }
		    if (u == ' ') {
			if (!word_selected || last_cjk_space) {
			    *bp++ = ' ';
			} else {
			    ns++;
			}
		    } else {
			while (ns) {
			    *bp++ = ' ';
			    ns--;
			}
			ch = u;
			bp += WideCharToMultiByte(CP_OEMCP, 0,
				&ch, 1, bp, 1, "?", NULL);
			last_cjk_space = false;
		    }
		}
	    } else {
		/* 3270-mode text */
		if (IS_LEFT(d)) {
		    int xbaddr = baddr;

		    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
			u = IDEOGRAPHIC_SPACE;
		    } else {
			xbaddr = baddr;
			INC_BA(xbaddr);
			u = ebcdic_to_unicode(
				(ea_buf[baddr].ec << 8) |
				    ea_buf[xbaddr].ec,
				CS_BASE, EUO_NONE);
			if (u == 0) {
			    u = IDEOGRAPHIC_SPACE;
			}
		    }
		    while (ns) {
			*bp++ = ' ';
			ns--;
		    }
		    ch = u;
		    bp += WideCharToMultiByte(CP_OEMCP, 0, &ch, 1,
			    bp, 1, "?", NULL);
		    last_cjk_space = (u == IDEOGRAPHIC_SPACE);
		} else if (!IS_RIGHT(d)) {
		    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
			u = ' ';
		    } else {
			u = ebcdic_to_unicode(ea_buf[baddr].ec,
				ea_buf[baddr].cs,
				appres.c3270.ascii_box_draw?
				    EUO_ASCII_BOX: 0);
			if (u == 0) {
			    u = ' ';
			}
			if (toggled(MONOCASE)) {
			    u = u_toupper(u);
			}
		    }
		    if (u == ' ') {
			if (!word_selected || last_cjk_space) {
			    *bp++ = ' ';
			} else {
			    ns++;
			}
		    } else {
			while (ns) {
			    *bp++ = ' ';
			    ns--;
			}
			ch = u;
			bp += WideCharToMultiByte(CP_OEMCP, 0,
				&ch, 1, bp, 1, "?", NULL);
			last_cjk_space = false;
		    }
		}
	    }
	}
    }

    *bp++ = 0;
    return bp - lptstr;
}

/*
 * 8-bit text version of clipboard copy.
 */
static size_t
copy_clipboard_text(LPTSTR lptstr)
{
    int r, c;
    int any_row = -1;
    int ns = 0;
    char *bp = lptstr;
    enum dbcs_state d;
    unsigned char fa;

    /* Fill in the buffer. */
    fa = get_field_attribute(0);
    for (r = 0; r < ROWS; r++) {
	for (c = 0; c < COLS; c++) {
	    int baddr = (r * COLS) + c;
	    char buf[16];
	    size_t nc;
	    ucs4_t u;

	    if (ea_buf[baddr].fa) {
		fa = ea_buf[baddr].fa;
	    }
	    if (!s_pending[baddr]) {
		continue;
	    }
	    if (any_row >= 0 && any_row != r && !(ea_buf[baddr - 1].gr & GR_WRAP)) {
		*bp++ = '\r';
		*bp++ = '\n';
		ns = 0;
	    }
	    any_row = r;
	    d = ctlr_dbcs_state(baddr);
	    if (IS_LEFT(d) || IS_RIGHT(d) ||
		    ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
		u = ' ';
	    } else {
		if (is_nvt(&ea_buf[baddr], appres.c3270.ascii_box_draw, &u)) {
		    nc = unicode_to_multibyte(u, buf, sizeof(buf));
		} else {
		    nc = ebcdic_to_multibyte_x(ea_buf[baddr].ec,
			    ea_buf[baddr].cs, buf, sizeof(buf),
			    EUO_BLANK_UNDEF |
				(appres.c3270.ascii_box_draw?
				 EUO_ASCII_BOX: 0),
			    &u);
		}
		if (nc == 2) {
		    u = buf[0];
		} else {
		    u = ' ';
		}
		if (toggled(MONOCASE)) {
		    u = u_toupper(u);
		}
	    }

	    if (u == ' ' && word_selected) {
		ns++;
	    } else {
		while (ns) {
		    *bp++ = ' ';
		    ns--;
		}
		*bp++ = u;
	    }
	}
    }

    *bp++ = 0;
    return bp - lptstr;
}

/*
 * Common code for Copy and Cut.
 */
void
copy_cut_action(bool cutting)
{
    size_t sl;
    HGLOBAL hglb;
    LPTSTR lptstr;
    int r, c;
    int any_row = -1;
#define NUM_TYPES 3
    struct {
	const char *name;
	int type;
	size_t esize;
	size_t (*copy_fn)(LPTSTR);
    } types[NUM_TYPES] = {
	{ "Unicode",
	  CF_UNICODETEXT,
	  sizeof(wchar_t),
	  copy_clipboard_unicode
	},
	{ "OEM text",
	  CF_OEMTEXT,
	  sizeof(char),
	  copy_clipboard_oemtext
	},
	{ "text",
	  CF_TEXT,
	  sizeof(char),
	  copy_clipboard_text
	},
    };
    int i;

    /* Make sure we have something to do. */
    if (memchr(s_pending, 1, COLS * ROWS) == NULL) {
	return;
    }

    vtrace("Word %sselected\n", word_selected? "": "not ");

    /* Open the clipboard. */
    if (!OpenClipboard(console_window)) {
	return;
    }
    EmptyClipboard();

    /* Compute the size of the output buffer. */
    sl = 0;
    for (r = 0; r < ROWS; r++) {
	for (c = 0; c < COLS; c++) {
	    int baddr = (r * COLS) + c;

	    if (s_pending[baddr]) {
		if (any_row >= 0 && any_row != r) {
		    sl += 2; /* CR/LF */
		}
		any_row = r;
		sl++;
	    }
	}
    }
    sl++; /* NUL */

    /* Copy it out in the formats we understand. */
    for (i = 0; i < NUM_TYPES; i++) {

	/* Allocate the buffer. */
	hglb = GlobalAlloc(GMEM_MOVEABLE, sl * types[i].esize);
	if (hglb == NULL) {
	    break;
	}

	/* Copy the screen data to it. */
	lptstr = GlobalLock(hglb); 
	sl = (types[i].copy_fn)(lptstr);
	GlobalUnlock(hglb); 

	/* Place the handle on the clipboard. */
	SetClipboardData(types[i].type, hglb);
	vtrace("Copy(): Put %ld %s characters on the clipboard\n",
		(long)sl, types[i].name);
    }

    CloseClipboard();

    /* Do the 'cutting' part of 'Cut'. */
    if (cutting) {
	unsigned char fa;
	int baddr;
	int ba2;
	char *sp_save;

	/*
	 * Save the contents of s_pending and use the copy instead of
	 * s_pending, because the first call to ctlr_add() will zero
	 * it.
	 */
	sp_save = Malloc(ROWS * COLS);
	memcpy(sp_save, s_pending, ROWS * COLS);

	fa = get_field_attribute(0);
	for (baddr = 0; baddr < ROWS * COLS; baddr++) {
	    if (ea_buf[baddr].fa) {
		fa = ea_buf[baddr].fa;
	    } else {
		if (!sp_save[baddr]
			|| FA_IS_PROTECTED(fa)
			|| ea_buf[baddr].ec == EBC_so
			|| ea_buf[baddr].ec == EBC_si) {
		    continue;
		}
		switch (ctlr_dbcs_state(baddr)) {
		case DBCS_NONE:
		case DBCS_SB:
		    ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		    break;
		case DBCS_LEFT:
		    ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		    ba2 = baddr;
		    INC_BA(ba2);
		    ctlr_add(ba2, EBC_space, ea_buf[baddr].cs);
		    break;
		case DBCS_RIGHT:
		    ba2 = baddr;
		    DEC_BA(ba2);
		    ctlr_add(ba2, EBC_space, ea_buf[baddr].cs);
		    ctlr_add(baddr, EBC_space, ea_buf[baddr].cs);
		    break;
		default:
		    break;
		}
		mdt_set(baddr);
	    }
	}
	Free(sp_save);
    }

    /* Unselect. */
    unselect(0, ROWS * COLS);
}

/*
 * The Copy() action, generally mapped onto ^C.
 */
static bool
Copy_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnCopy, ia, argc, argv);
    if (check_argc(AnCopy, argc, 0, 0) < 0) {
	return false;
    }
    copy_cut_action(false);
    return true;
}

/*
 * The Cut() action, generally mapped onto ^X.
 */
static bool
Cut_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnCut, ia, argc, argv);
    if (check_argc(AnCut, argc, 0, 0) < 0) {
	return false;
    }
    copy_cut_action(true);
    return true;
}

/*
 * Return true if a cell in the specified region is out of sync with the
 * screen with regard to selection.
 */
bool
select_changed(unsigned row, unsigned col, unsigned rows, unsigned cols)
{
    unsigned r;

    assert((int)(row + rows) <= ROWS);
    assert((int)(col + cols) <= COLS);

    for (r = row; r < row + rows; r++) {
	if (memcmp(&s_pending [(r * COLS) + col],
		    &s_onscreen[(r * COLS) + col],
		    cols)) {
	    return true;
	}
    }
    return false;
}

/*
 * Return TRUE if any cell in a region is selected.
 */
bool
area_is_selected(int baddr, int len)
{
    return memchr(&s_pending[baddr], 1, len) != NULL;
}

/*
 * Synchronize the pending and on-screen select state (copies pending to
 * on-screen).
 */
void
select_sync(unsigned row, unsigned col, unsigned rows, unsigned cols)
{
    unsigned r;

    assert((int)(row + rows) <= ROWS);
    assert((int)(col + cols) <= COLS);

    for (r = row; r < row + rows; r++) {
	memcpy(&s_onscreen[(r * COLS) + col],
		&s_pending [(r * COLS) + col],
		cols);
    }
}

/*
 * Add the current location to the selection. Called from the four SelectXxx
 * actions.
 */
static void
keyboard_cursor_select(void)
{
    if (select_started) {
	/* Extend selection. */
	vtrace("  Extending selection\n");
    } else {
	vtrace("  New selection\n");
	select_start_row = cursor_addr / COLS;
	select_start_col = cursor_addr % COLS;
    }
    /*rubber_banding = true;*/
    select_started = true;
    word_selected = false;
    select_end_row = cursor_addr / COLS;
    select_end_col = cursor_addr % COLS;
    reselect(true);
}

/*
 * SelectLeft adds the current column to the selection and moves the cursor
 * to the left.
 */
static bool
SelectLeft_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnSelectLeft, ia, argc, argv);
    if (check_argc(AnSelectLeft, argc, 0, 0) < 0) {
	return false;
    }

    keyboard_cursor_select();
    Left_action(ia, 0, NULL);

    return true;
}

/*
 * SelectRight adds the current column to the selection and moves the cursor
 * to the right.
 */
static bool
SelectRight_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnSelectRight, ia, argc, argv);
    if (check_argc(AnSelectRight, argc, 0, 0) < 0) {
	return false;
    }

    keyboard_cursor_select();
    Right_action(ia, 0, NULL);

    return true;
}

/*
 * SelectUp adds the current row to the selection and moves the cursor up.
 */
static bool
SelectUp_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnSelectUp, ia, argc, argv);
    if (check_argc(AnSelectUp, argc, 0, 0) < 0) {
	return false;
    }

    keyboard_cursor_select();
    Up_action(ia, 0, NULL);

    return true;
}

/*
 * SelectDown adds the current row to the selection and moves the cursor down.
 */
static bool
SelectDown_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnSelectDown, ia, argc, argv);
    if (check_argc(AnSelectDown, argc, 0, 0) < 0) {
	return false;
    }

    keyboard_cursor_select();
    Down_action(ia, 0, NULL);

    return true;
}

/**
 * Selection module registration.
 */
void
select_register(void)
{
    static action_table_t select_actions[] = {
	{ AnCopy,	Copy_action,		ACTION_KE },
	{ AnCut,	Cut_action,		ACTION_KE },
	{ AnSelectDown,	SelectDown_action,	ACTION_KE },
	{ AnSelectLeft,	SelectLeft_action,	ACTION_KE },
	{ AnSelectRight,SelectRight_action,	ACTION_KE },
	{ AnSelectUp,	SelectUp_action,	ACTION_KE }
    };
    static toggle_register_t toggles[] = {
	{ SELECT_URL, NULL, 0 }
    };

    register_actions(select_actions, array_count(select_actions));
    register_toggles(toggles, array_count(toggles));
}
