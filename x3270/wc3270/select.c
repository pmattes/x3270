/*
 * Copyright (c) 2013-2015 Paul Mattes.
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
#include "popups.h"
#include "trace.h"
#include "unicodec.h"
#include "winversc.h"
#include "wselectc.h"

#include "selectc.h"

/* Unicode DBCS (double-width) blank. */
#define IDEOGRAPHIC_SPACE	0x3000

static char *s_pending;
static char *s_onscreen;

/* Event names. */
static char *event_name[] = {
	"BUTTON_DOWN",
	"BUTTON_UP",
	"MOVE",
	"DOUBLE_CLICK"
};

/* If True, we are rubber-banding a selection right now. */
static Boolean select_pending = False;

/* If True, we have a stored start point. */
static Boolean select_started = False;

/* If True, the current selection was from a double-click. */
static Boolean word_selected = False;

/* Start of selected area. */
static int select_start_row;
static int select_start_col;

/* End of selected area. */
static int select_end_row;
static int select_end_col;

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
	select_pending = False;
	select_started = False;
	word_selected = False;
	memset(s_pending, 0, ROWS * COLS);
	screen_changed = True;
}

static void
reselect(void)
{
	int rowA, colA, rowZ, colZ;
	int row, col;

	/* Clear out the current selection. */
	memset(s_pending, 0, ROWS * COLS);

	/* Fill in from start to end, which may be backwards. */
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
		}
	}

	screen_changed = True;
}

/*
 * Returns True if the character at the given location is displayed as a
 * blank.
 */
static Boolean
is_blank(int baddr)
{
    unsigned char fa;
    int xbaddr;
    int c;

    /* Check for FA or blanked field. */
    fa = get_field_attribute(baddr);
    if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
	return True;
    }

    /* Translate to Unicode, exactly as we would display it. */
    if (IS_LEFT(baddr)) {
	xbaddr = baddr;
	DEC_BA(xbaddr);
	c = ebcdic_to_unicode((ea_buf[xbaddr].cc << 8) |
			      ea_buf[baddr].cc,
			      CS_BASE, EUO_NONE);
	if (c == 0 || c == IDEOGRAPHIC_SPACE) {
	    return True;
	}
    } else if (IS_RIGHT(baddr)) {
	xbaddr = baddr;
	INC_BA(xbaddr);
	c = ebcdic_to_unicode((ea_buf[baddr].cc << 8) |
			      ea_buf[xbaddr].cc,
			      CS_BASE, EUO_NONE);
	if (c == 0 || c == IDEOGRAPHIC_SPACE) {
	    return True;
	}
    } else {
	c = ebcdic_to_unicode(ea_buf[baddr].cc,
		ea_buf[baddr].cs,
		appres.c3270.ascii_box_draw?
		    EUO_ASCII_BOX: 0);
	if (c == 0 || c == ' ') {
	    return True;
	}
    }

    return False;
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
find_word_end(int row, int col, int *startp, int *endp)
{
	int baddr = (row * COLS) + col;

	assert(row <= ROWS);
	assert(col <= COLS);

	/*
	 * If on a blank now, return just that, or that plus the word to the
	 * left.
	 */
	if (is_blank(baddr)) {
		*endp = col;
		while (col && !is_blank((row * COLS) + (col - 1))) {
			col--;
		}
		*endp = col;
		return;
	}

	/* Search left. */
	while (col && !is_blank((row * COLS) + (col - 1))) {
		col--;
	}
	*startp = col;

	/* Search right. */
	while (col < (COLS - 1) && !is_blank((row * COLS) + (col + 1))) {
		col++;
	}
	if (col < (COLS -1)) {
		col++;
	}
	*endp = col;
}

/*
 * Pass a mouse event to the select logic.
 *
 * Only the essentials of the event are passed in -- the row and column in
 * display coordinates (not screen coordinates), and the status of the left
 * mouse button. select_event() infers the user's actions from that.
 *
 * Returns True if the event was consumed, or False if it was a cursor-move
 * event (button up without movement).
 */
Boolean
select_event(unsigned row, unsigned col, select_event_t event, Boolean shift)
{
	assert((int)row <= ROWS);
	assert((int)col <= COLS);

	vtrace("select_event(%u %u %s %s)\n", row, col, event_name[event],
		shift? "shift": "no-shift");

	if (!select_pending) {
		switch (event) {
		case SE_BUTTON_DOWN:
			if (shift && select_started) {
				/* Extend selection. */
				vtrace("Extending selection\n");
			} else {
				vtrace("New selection\n");
				select_start_row = row;
				select_start_col = col;
			}
			select_pending = True;
			select_started = True;
			word_selected = False;
			select_end_row = row;
			select_end_col = col;
			reselect();
			break;
		case SE_DOUBLE_CLICK:
			vtrace("Word select\n");
			select_pending = False;
			select_start_row = row;
			select_end_row = row;
			find_word_end(row, col, &select_start_col,
				&select_end_col);
			word_selected = True;
			reselect();
			break;
		default:
			break;
		}
	} else {
		/* A selection is pending (rubber-banding). */
		switch (event) {
		case SE_BUTTON_UP:
			select_pending = False;
			word_selected = False;
			if (row == select_start_row &&
			    col == select_start_col) {
				/*
				 * No movement. Call it a cursor move,
				 * but they might extend it later.
				 */
				vtrace("Cursor move\n");
				s_pending[(row * COLS) + col] = 0;
				screen_changed = True;
				/* We did not consume the event. */
				return False;
			}
			vtrace("Finish selection.\n");
			select_end_row = row;
			select_end_col = col;
			reselect();
			break;
		case SE_MOVE:
			/* Extend. */
			vtrace("Extend\n");
			select_end_row = row;
			select_end_col = col;
			reselect();
			break;
		default:
			break;
		}
	}

	/* We consumed the event. */
	return True;
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
    Boolean last_cjk_space = False;
    wchar_t *bp = (wchar_t *)lptstr;
    enum dbcs_state d;
    int ch;
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
	    if (any_row >= 0 && any_row != r) {
		*bp++ = '\r';
		*bp++ = '\n';
		ns = 0;
		last_cjk_space = False;
	    }
	    any_row = r;

	    d = ctlr_dbcs_state(baddr);
	    if (IS_LEFT(d)) {
		int xbaddr = baddr;

		if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
		    ch = IDEOGRAPHIC_SPACE;
		} else {
		    xbaddr = baddr;
		    INC_BA(xbaddr);
		    ch = ebcdic_to_unicode(
			    (ea_buf[baddr].cc << 8) |
				ea_buf[xbaddr].cc,
			    CS_BASE, EUO_NONE);
		    if (ch == 0) {
			ch = IDEOGRAPHIC_SPACE;
		    }
		}
	    } else if (!IS_RIGHT(d)) {
		if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
		    ch = ' ';
		} else {
		    ch = ebcdic_to_unicode(ea_buf[baddr].cc,
				ea_buf[baddr].cs,
				appres.c3270.ascii_box_draw?
				    EUO_ASCII_BOX: 0);
		    if (ch == 0) {
			ch = ' ';
		    }
		    if (toggled(MONOCASE) && islower(ch)) {
			ch = toupper(ch);
		    }
		}
	    }

	    if (ch == ' ') {
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
		*bp++ = ch;
		last_cjk_space = (ch == IDEOGRAPHIC_SPACE);
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
    Boolean last_cjk_space = False;
    char *bp = lptstr;
    enum dbcs_state d;
    wchar_t ch;
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
	    if (any_row >= 0 && any_row != r) {
		*bp++ = '\r';
		*bp++ = '\n';
		ns = 0;
		last_cjk_space = False;
	    }
	    any_row = r;
	    d = ctlr_dbcs_state(baddr);
	    if (IS_LEFT(d)) {
		int xbaddr = baddr;

		if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
		    ch = IDEOGRAPHIC_SPACE;
		} else {
		    xbaddr = baddr;
		    INC_BA(xbaddr);
		    ch = ebcdic_to_unicode(
			    (ea_buf[baddr].cc << 8) |
				ea_buf[xbaddr].cc,
			    CS_BASE, EUO_NONE);
		    if (ch == 0) {
			ch = IDEOGRAPHIC_SPACE;
		    }
		}
		while (ns) {
		    *bp++ = ' ';
		    ns--;
		}
		bp += WideCharToMultiByte(CP_OEMCP, 0, &ch, 1,
			bp, 1, "?", NULL);
		last_cjk_space = (ch == IDEOGRAPHIC_SPACE);
	    } else if (!IS_RIGHT(d)) {
		if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
		    ch = ' ';
		} else {
		    ch = ebcdic_to_unicode(ea_buf[baddr].cc,
			    ea_buf[baddr].cs,
			    appres.c3270.ascii_box_draw?
				EUO_ASCII_BOX: 0);
		    if (ch == 0) {
			ch = ' ';
		    }
		    if (toggled(MONOCASE) && islower(ch)) {
			ch = toupper(ch);
		    }
		}
		if (ch == ' ') {
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
		    bp += WideCharToMultiByte(CP_OEMCP, 0,
			    &ch, 1, bp, 1, "?", NULL);
		    last_cjk_space = False;
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
    int ch;
    unsigned char fa;

    /* Fill in the buffer. */
    fa = get_field_attribute(0);
    for (r = 0; r < ROWS; r++) {
	for (c = 0; c < COLS; c++) {
	    int baddr = (r * COLS) + c;
	    char buf[16];
	    int nc;
	    ucs4_t u;

	    if (ea_buf[baddr].fa) {
		fa = ea_buf[baddr].fa;
	    }
	    if (!s_pending[baddr]) {
		continue;
	    }
	    if (any_row >= 0 && any_row != r) {
		*bp++ = '\r';
		*bp++ = '\n';
		ns = 0;
	    }
	    any_row = r;
	    d = ctlr_dbcs_state(baddr);
	    if (IS_LEFT(d) || IS_RIGHT(d) ||
		    ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
		ch = ' ';
	    } else {
		nc = ebcdic_to_multibyte_x(ea_buf[baddr].cc,
			ea_buf[baddr].cs, buf, sizeof(buf),
			EUO_BLANK_UNDEF |
			    (appres.c3270.ascii_box_draw?
			     EUO_ASCII_BOX: 0),
			&u);
		if (nc == 2) {
		    ch = buf[0];
		} else {
		    ch = ' ';
		}
		if (toggled(MONOCASE) && islower(ch)) {
		    ch = toupper(ch);
		}
	    }

	    if (ch == ' ' && word_selected) {
		ns++;
	    } else {
		while (ns) {
		    *bp++ = ' ';
		    ns--;
		}
		*bp++ = ch;
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
copy_cut_action(Boolean cutting)
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
		if (!sp_save[baddr] || FA_IS_PROTECTED(fa)) {
		    continue;
		}
		switch (ctlr_dbcs_state(baddr)) {
		case DBCS_NONE:
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
static Boolean
Copy_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Copy", ia, argc, argv);
    if (check_argc("Copy", argc, 0, 0) < 0) {
	return False;
    }
    copy_cut_action(False);
    return True;
}

/*
 * The Cut() action, generally mapped onto ^X.
 */
static Boolean
Cut_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Cut", ia, argc, argv);
    if (check_argc("Cut", argc, 0, 0) < 0) {
	return False;
    }
    copy_cut_action(True);
    return True;
}

/*
 * Return True if a cell in the specified region is out of sync with the
 * screen with regard to selection.
 */
Boolean
select_changed(unsigned row, unsigned col, unsigned rows, unsigned cols)
{
	unsigned r;

	assert((int)(row + rows) <= ROWS);
	assert((int)(col + cols) <= COLS);

	for (r = row; r < row + rows; r++) {
		if (memcmp(&s_pending [(r * COLS) + col],
			   &s_onscreen[(r * COLS) + col],
			   cols)) {
			return True;
		}
	}
	return False;
}

/*
 * Return TRUE if any cell in a region is selected.
 */
Boolean
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

/**
 * Selection module registration.
 */
void
select_register(void)
{
    static action_table_t select_actions[] = {
	{ "Copy",	Copy_action,	ACTION_KE },
	{ "Cut",	Cut_action,	ACTION_KE }
    };

    register_actions(select_actions, array_count(select_actions));
}
