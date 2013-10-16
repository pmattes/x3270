/*
 * Copyright (c) 2013, Paul Mattes.
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
#define TEMPORARY

#include "globals.h"
#include <assert.h>
#include <wchar.h>
#include <windows.h>

#include "3270ds.h"
#include "appres.h"

#include "actionsc.h"
#include "ctlr.h"
#include "ctlrc.h"
#include "popupsc.h"
#ifdef TEMPORARY
#include "screenc.h"
#endif
#include "trace_dsc.h"
#include "unicodec.h"
#include "winversc.h"

#include "selectc.h"

extern int screen_changed;

static char *s_pending;
static char *s_onscreen;

/*
 * Initialize the selection logic, given the maximum screen dimensions.
 */
void
select_init(unsigned max_rows, unsigned max_cols)
{
	//trace_event("select_init(%d %d)\n", max_rows, max_cols);
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
	//trace_event("unselect(%d %d)\n", baddr, len);

    	/*
	 * Technically, only the specified area has changed, but intuitively,
	 * the whole selected rectangle has.
	 */
	/*memset(&s_pending[baddr], 0, len);*/
	memset(s_pending, 0, ROWS * COLS);
	screen_changed = True;
}

/*
 * Pass a mouse event to the select logic.
 */
void
select_event(unsigned row, unsigned col, Boolean pressed)
{
	/* XXX: do something useful here */
}

/*
 * NT version of copy-to-clipboard. Does Unicode text.
 */
static void
copy_clipboard_nt(LPTSTR lptstr)
{
	int r, c;
	Boolean any;
	wchar_t *bp = (wchar_t *)lptstr;
	enum dbcs_state d;
	int ch;
	unsigned char fa;

	/* Fill in the buffer. */
	fa = get_field_attribute(0);
	any = False;
	for (r = 0; r < ROWS; r++) {
		for (c = 0; c < COLS; c++) {
			int baddr = (r * COLS) + c;

			if (ea_buf[baddr].fa) {
				fa = ea_buf[baddr].fa;
			}
			if (!s_pending[baddr]) {
				continue;
			}
			any = True;
			d = ctlr_dbcs_state(baddr);
			if (IS_LEFT(d)) {
				int xbaddr = baddr;

				if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
				    *bp++ = 0x3000; /* ideographic space */
				    continue;
				}

				xbaddr = baddr;
				INC_BA(xbaddr);
				ch = ebcdic_to_unicode((ea_buf[baddr].cc << 8) |
						      ea_buf[xbaddr].cc,
						      CS_BASE, EUO_NONE);
				if (ch == 0) {
					ch = ' ';
				}
				*bp++ = ch;
			} else if (!IS_RIGHT(d)) {
				if (ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
					*bp++ = ' ';
					continue;
				}

				ch = ebcdic_to_unicode(ea_buf[baddr].cc,
					ea_buf[baddr].cs,
					appres.ascii_box_draw?
					    EUO_ASCII_BOX: 0);
				if (ch == 0) {
					ch = ' ';
				}
				if (toggled(MONOCASE) && islower(ch)) {
					ch = toupper(ch);
				}
				*bp++ = ch;
			}
		}
		if (any) {
			*bp++ = '\r';
			*bp++ = '\n';
			any = False;
		}
	}
	if (any) {
		*bp++ = '\r';
		*bp++ = '\n';
	}

	*bp = 0;
}

/*
 * Windows 98 version of copy-to-clipboard. Does 8-bit text only.
 */
static void
copy_clipboard_98(LPTSTR lptstr)
{
	int r, c;
	Boolean any;
	char *bp = lptstr;
	enum dbcs_state d;
	int ch;
	unsigned char fa;

	/* Fill in the buffer. */
	fa = get_field_attribute(0);
	any = False;
	for (r = 0; r < ROWS; r++) {
		for (c = 0; c < COLS; c++) {
			int baddr = (r * COLS) + c;
			char buf[16];
			int nc;

			if (ea_buf[baddr].fa) {
				fa = ea_buf[baddr].fa;
			}
			if (!s_pending[baddr]) {
				continue;
			}
			any = True;
			d = ctlr_dbcs_state(baddr);
			if (IS_LEFT(d) || IS_RIGHT(d) ||
				ea_buf[baddr].fa || FA_IS_ZERO(fa)) {
			    *bp++ = ' ';
			    continue;
			}
			nc = ebcdic_to_multibyte_x(ea_buf[baddr].cc,
				ea_buf[baddr].cs, buf, sizeof(buf),
				EUO_BLANK_UNDEF |
				    (appres.ascii_box_draw?
				     EUO_ASCII_BOX: 0),
				NULL);
			if (nc == 1) {
				ch = buf[0];
			} else {
				ch = ' ';
			}
			if (toggled(MONOCASE) && islower(ch)) {
				ch = toupper(ch);
			}
			*bp++ = ch;
		}
		if (any) {
			*bp++ = '\r';
			*bp++ = '\n';
			any = False;
		}
	}
	if (any) {
		*bp++ = '\r';
		*bp++ = '\n';
	}
	*bp = 0;
}

/*
 * The Copy() action, generally mapped onto ^C.
 */
void
Copy_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	size_t sl;
	HGLOBAL hglb;
	LPTSTR lptstr;
	int r, c;
	Boolean any;

	action_debug(Copy_action, event, params, num_params);

	/* Make sure we have something to do. */
	if (memchr(s_pending, 1, COLS * ROWS) == NULL) {
		return;
	}

	/* Open the clipboard. */
	if (!OpenClipboard(NULL)) {
	    	return;
	}

	/* Compute the size of the output buffer. */
	sl = 0;
	any = False;
	for (r = 0; r < ROWS; r++) {
		for (c = 0; c < COLS; c++) {
			int baddr = (r * COLS) + c;

			if (s_pending[baddr]) {
				sl++;
				any = True;
			}
		}
		if (any) {
			sl += 2; /* CR/LF */
			any = False;
		}
	}
	if (any) {
		sl += 2; /* CR/LF */
	}
	sl++; /* NUL */

	/* Allocate the buffer. */
	hglb = GlobalAlloc(GMEM_MOVEABLE,
		sl * (is_nt? sizeof(wchar_t): sizeof(char)));
	if (hglb == NULL) {
		CloseClipboard(); 
		return;
	}

	/* Copy the screen data to it. */
	lptstr = GlobalLock(hglb); 
	if (is_nt) {
		copy_clipboard_nt(lptstr);
	} else {
		copy_clipboard_98(lptstr);
	}
	GlobalUnlock(hglb); 

	/* Place the handle on the clipboard. */
	SetClipboardData(is_nt? CF_UNICODETEXT: CF_TEXT, hglb);
	CloseClipboard();
	trace_event("Copy(): Put %ld %s characters on the clipboard\n",
		(long)sl, is_nt? "Unicode": "Text");

	/* Unselect. */
	unselect(0, ROWS * COLS);
}

/*
 * Return True if a cell in the specified region is out of sync with the
 * screen with regard to selection.
 */
Boolean
select_changed(unsigned row, unsigned col, unsigned rows, unsigned cols)
{
	int r;

	//trace_event("select_changed(%u %u %u %u)\n", row, col, rows, cols);
	assert(row + rows <= ROWS);
	assert(col + cols <= COLS);

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
	//trace_event("area_is_selected(%d %d)\n", baddr, len);
    	return memchr(&s_pending[baddr], 1, len) != NULL;
}

/*
 * Synchronize the pending and on-screen select state (copies pending to
 * on-screen).
 */
void
select_sync(unsigned row, unsigned col, unsigned rows, unsigned cols)
{
	int r;

	//trace_event("select_sync(%u %u %u %u)\n", row, col, rows, cols);

	assert(row + rows <= ROWS);
	assert(col + cols <= COLS);

	for (r = row; r < row + rows; r++) {
		memcpy(&s_onscreen[(r * COLS) + col],
		       &s_pending [(r * COLS) + col],
		       cols);
	}
}

#ifdef TEMPORARY
/*
 * Manual selection action, for testing before mouse stuff works.
 */
void
Selected_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    	unsigned row, col, rows, cols;
	unsigned r, c;

	action_debug(Selected_action, event, params, num_params);

	if (*num_params == 0) {
		unselect(0, maxROWS * maxCOLS);
		goto done;
	}

	if (*num_params != 4) {
		popup_an_error("Selected() needs 4 parameters: row, col, "
			"rows, cols");
		return;
	}

	row = atoi(params[0]);
	col = atoi(params[1]);
	rows = atoi(params[2]);
	cols = atoi(params[3]);
	if (!rows || !cols) {
	    	return;
	}

	if (row + rows > ROWS || col + cols > COLS) {
		popup_an_error("Selected() overflow");
		return;
	}

	for (r = row; r < row + rows; r++) {
	    	for (c = col; c < col + cols; c++) {
		    	s_pending[(r * COLS) + c] =
			    !s_pending[(r * COLS) + c];
		}
	}

    done:
	screen_changed = True;
	screen_disp(False);
}
#endif
