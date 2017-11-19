/*
 * Copyright (c) 1994-2009, 2013-2017 Paul Mattes.
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
 *	scroll.c
 *		Scrollbar support
 */

#include "globals.h"
#include "appres.h"
#include "ctlr.h"

#include "3270ds.h"
#include "actions.h"
#include "ctlrc.h"
#include "kybd.h"
#include "popups.h"
#include "screen.h"
#include "scroll.h"
#include "selectc.h"
#include "status.h"
#include "trace.h"
#include "utils.h"

/* Globals */
bool	scroll_initted = false;

/* Statics */

/* Saved screens. */
static struct ea **ea_save = (struct ea **) NULL;

/* Number of lines saved. */
static int      n_saved = 0;
static int      scroll_next = 0;

static int      scrolled_back = 0;
static bool  need_saving = true;
static bool  vscreen_swapped = false;
static char    *sbuf = NULL;
static int      sa_bufsize;
static char    *zbuf = NULL;

/* Thumb state: */
/*   Fraction of blank area above thumb (0.0 to 1.0) */
static float    thumb_top = 0.0;
/*   How much blank area there is, above and below the thumb (maximum possible
 *   value for thumb_top) */
static float    thumb_top_base = 0.0;
/*   Fraction of thumb shown (1.0 - thumb_top_base) */
static float    thumb_shown = 1.0;

static void sync_scroll(int sb);
static void save_image(void);
static void scroll_reset(void);

/*
 * Initialize (or re-initialize) the scrolling parameters and save area.
 */
void
scroll_buf_init(void)
{
    register int i;
    int sa_size;
    unsigned char *s;

    if (appres.interactive.save_lines % maxROWS) {
	appres.interactive.save_lines =
	    ((appres.interactive.save_lines+maxROWS-1)/maxROWS) * maxROWS;
    }
    if (!appres.interactive.save_lines) {
	appres.interactive.save_lines = maxROWS;
    }
    if (sbuf != NULL) {
	Free(sbuf);
	Free(zbuf);
	Free(ea_save);
    }
    sa_size = appres.interactive.save_lines + maxROWS;
    ea_save = (struct ea **)Calloc(sizeof(struct ea *), sa_size);
    sa_bufsize = (sa_size *
	    (sizeof(unsigned char) + sizeof(struct ea))) * maxCOLS;
    sbuf = Malloc(sa_bufsize);
    zbuf = Malloc(maxCOLS);
    (void) memset(zbuf, '\0', maxCOLS);
    s = (unsigned char *)sbuf;
    for (i = 0; i < sa_size; s += (maxCOLS * sizeof(struct ea)), i++) {
	ea_save[i] = (struct ea *)s;
    }
    scroll_reset();
    scroll_initted = true;
}

static void
screen_set_thumb_traced(float top, float shown, int saved, int screen,
	int back)
{
#if defined(SCROLL_DEBUG) /*[*/
    vtrace(" -> screen_set_thumb(top %f, shown %f)\n", top, shown);
    vtrace(" -> top %f top_base %f shown %f\n",
	    thumb_top,
	    thumb_top_base,
	    thumb_shown);
#endif /*]*/
    screen_set_thumb(top, shown, saved, screen, back);
}

/*
 * Reset the scrolling parameters and erase the save area.
 */
static void
scroll_reset(void)
{
    (void) memset(sbuf, 0, sa_bufsize);
    scroll_next = 0;
    n_saved = 0;
    scrolled_back = 0;
    thumb_top_base = thumb_top = 0.0;
    thumb_shown = 1.0;
    need_saving = true;
    screen_set_thumb_traced(thumb_top, thumb_shown, n_saved, maxROWS,
	    scrolled_back);
    enable_cursor(true);
}

/*
 * Save <n> lines of data from the top of the screen.
 */
void
scroll_save(int n, bool trim_blanks)
{
    int i;

#if defined(SCROLL_DEBUG) /*[*/
    vtrace("scroll_save(%d, %s)\n", n, trim_blanks? "trim": "no trim");
#endif /*]*/

    /* Trim trailing blank lines from 'n', if requested */
    if (trim_blanks) {
	while (n) {
	    int i;

	    for (i = 0; i < COLS; i++) {
		if (ea_buf[(n-1)*COLS + i].cc) {
		    break;
		}
	    }
	    if (i < COLS) {
		break;
	    } else {
		n--;
	    }
	}
	if (!n) {
#if defined(SCROLL_DEBUG) /*[*/
	    vtrace(" -> nothing to save\n");
#endif /*]*/
	    return;
	}
    }

    /* Scroll to bottom on "output". */
    if (scrolled_back) {
	sync_scroll(0);
    }

    /* Save the screen contents. */
    for (i = 0; i < n; i++) {
	if (i < COLS) {
	    (void) memmove(ea_save[scroll_next],
		    (ea_buf+(i*COLS)),
		    COLS*sizeof(struct ea));
	    if (COLS < maxCOLS) {
		(void) memset((char *)(ea_save[scroll_next] + COLS), '\0',
			(maxCOLS - COLS) * sizeof(struct ea));
	    }
	} else {
	    (void) memset((char *)ea_save[scroll_next], '\0',
		    maxCOLS * sizeof(struct ea));
	}
	scroll_next = (scroll_next + 1) % appres.interactive.save_lines;
	if (n_saved < appres.interactive.save_lines) {
	    n_saved++;
	}
    }

#if defined(SCROLL_DEBUG) /*[*/
    vtrace(" -> n_saved %d\n", n_saved);
#endif /*]*/

    /* Reset the thumb. */
    thumb_top_base = thumb_top =
	((float)n_saved / (float)(appres.interactive.save_lines + maxROWS));
    thumb_shown = (float)(1.0 - thumb_top);
    screen_set_thumb_traced(thumb_top, thumb_shown, n_saved, maxROWS,
	    scrolled_back);
}

/*
 * Add blank lines to the scroll buffer to make it a multiple of the
 * screen size.
 */
static void
scroll_round(void)
{
    int n;

    if (!(n_saved % maxROWS)) {
	return;
    }

#if defined(SCROLL_DEBUG) /*[*/
    vtrace("scroll_round start n_saved %d\n", n_saved);
#endif /*]*/

    /* Zero the scroll buffer. */
    for (n = maxROWS - (n_saved % maxROWS); n; n--) {
	(void) memset((char *)ea_save[scroll_next], '\0',
		maxCOLS * sizeof(struct ea));
	scroll_next = (scroll_next + 1) % appres.interactive.save_lines;
	if (n_saved < appres.interactive.save_lines) {
	    n_saved++;
	}
    }

#if defined(SCROLL_DEBUG) /*[*/
    vtrace(" -> n_saved %d\n", n_saved);
#endif /*]*/

    /* Reset the thumb. */
    thumb_top_base = thumb_top =
	((float)n_saved / (float)(appres.interactive.save_lines + maxROWS));
    thumb_shown = (float)(1.0 - thumb_top);
    screen_set_thumb_traced(thumb_top, thumb_shown, n_saved, maxROWS,
	    scrolled_back);
}

/*
 * Jump to the bottom of the scroll buffer.
 */
void
scroll_to_bottom(void)
{
    if (scrolled_back) {
	sync_scroll(0);
    }
    need_saving = true;
}

/*
 * Save the current screen image, if it hasn't been saved since last updated.
 */
static void
save_image(void)
{
    int i;

    if (!need_saving) {
	return;
    }

#if defined(SCROLL_DEBUG) /*[*/
    vtrace("save_image: saving %d lines after the buffer, n_saved %d\n",
	    maxROWS, n_saved);
#endif /*]*/

    for (i = 0; i < maxROWS; i++) {
	(void) memmove(ea_save[appres.interactive.save_lines+i],
		(ea_buf + (i * COLS)), COLS*sizeof(struct ea));
    }
    need_saving = false;
}

/*
 * Redraw the display so it begins back <sb> lines.
 */
static void
sync_scroll(int sb)
{
    int slop;
    int i;
    int scroll_first;
    float tt0;

#if defined(SCROLL_DEBUG) /*[*/
    vtrace("sync_scroll(sb=%d) n_saved=%d, scrolled_back=%d\n", sb, n_saved,
	    scrolled_back);
#endif /*]*/

    unselect(0, ROWS * COLS);

    /*
     * If in 3270 mode, round to a multiple of the screen size and
     * set the scroll bar.
     */
    if (ever_3270) {
	/* XXX: When disconnected, ever_3270 is false, so we might scroll
	 * into some very strange places. */
	if ((slop = (sb % maxROWS))) {
	    if (slop <= maxROWS / 2) {
		sb -= slop;
	    } else {
		sb += maxROWS - slop;
	    }
	}
	if (sb) {
	    kybd_scroll_lock(true);
	} else {
	    kybd_scroll_lock(false);
	}
    }

    /* Update the status line. */
    if (ever_3270) {
	status_scrolled(sb / maxROWS);
    } else {
	status_scrolled(0);
    }

    /* Swap screen sizes. */
    if (sb && !scrolled_back && ((COLS < maxCOLS) || (ROWS < maxROWS))) {
#if defined(SCROLL_DEBUG) /*[*/
	vtrace("sync_scroll: primary -> alt\n");
#endif /*]*/
	COLS = maxCOLS;
	ROWS = maxROWS;
	vscreen_swapped = true;
    } else if (!sb && scrolled_back && vscreen_swapped) {
#if defined(SCROLL_DEBUG) /*[*/
	vtrace("sync_scroll: alt -> primary\n");
#endif /*]*/
	ctlr_shrink();
	COLS = MODEL_2_COLS;
	ROWS = MODEL_2_ROWS;
	vscreen_swapped = false;
    }

    scroll_first = (scroll_next + appres.interactive.save_lines - sb) %
	appres.interactive.save_lines;
#if defined(SCROLL_DEBUG) /*[*/
    vtrace("sync_scroll: scroll_first is %d\n", scroll_first);
#endif /*]*/

    /* Update the screen. */
    for (i = 0; i < maxROWS; i++) {
	if (i < sb) {
	    (void) memmove((ea_buf + (i * COLS)), ea_save[(scroll_first + i) %
		    appres.interactive.save_lines], COLS * sizeof(struct ea));
	} else {
	    (void) memmove((ea_buf + (i * COLS)),
		    ea_save[appres.interactive.save_lines + i - sb],
		    COLS * sizeof(struct ea));
	}
    }

    /* Disable the cursor if we're scrolled back, enable it if not. */
    enable_cursor(sb == 0);

    scrolled_back = sb;
    ctlr_changed(0, ROWS * COLS);
    blink_start();

    tt0 = ((float)n_saved /
	    (float)(appres.interactive.save_lines + maxROWS));
    thumb_shown = (float)(1.0 - tt0);
    thumb_top = ((float)(n_saved-sb) /
	    (float)(appres.interactive.save_lines + maxROWS));
    screen_set_thumb_traced(thumb_top, thumb_shown, n_saved, maxROWS,
	    scrolled_back);
}

/*
 * Fixed-amount scroll action.
 */
static void
scroll_n(int nss, int direction)
{
    int nsr;

    if (!n_saved) {
	return;
    }

    if (!nss) {
	nss = 1;
    }
    save_image();
    if (direction > 0) {	/* scroll forward */
	if (nss > scrolled_back) {
	    sync_scroll(0);
	} else {
	    nsr = scrolled_back - nss;
	    if (ever_3270 && (nsr % maxROWS)) {
		nsr -= nsr % maxROWS;
	    }
	    sync_scroll(nsr);
	}
    } else {	/* scroll back */
	if (scrolled_back + nss > n_saved) {
	    sync_scroll(n_saved);
	} else {
	    nsr = scrolled_back + nss;
	    if (ever_3270 && (nsr % maxROWS)) {
		nsr += maxROWS - (nsr % maxROWS);
	    }
	    sync_scroll(nsr);
	}
    }

    screen_set_thumb_traced((float)(n_saved - scrolled_back) /
	    (float)(appres.interactive.save_lines + maxROWS), thumb_shown,
	    n_saved, maxROWS, scrolled_back);
}

/*
 * Callback for "scroll" action (incrementing the thumb in one direction).
 */
void
scroll_proc(int n, int total)
{
    float pct;
    int nss;

    if (!n_saved) {
	return;
    }

    if (n < 0) {
	pct = (float)(-n) / (float)total;
    } else {
	pct = (float)n / (float)total;
    }
#if defined(SCROLL_DEBUG) /*[*/
    vtrace("scroll_proc(%d, %d) -> %f%%\n", n, total, pct);
#endif /*]*/
    nss = (int)(pct * thumb_shown * n_saved);
    scroll_n(nss, n);
}

/*
 * Callback for "jump" action (moving the thumb to a particular spot).
 */
void
jump_proc(float top)
{
#if defined(SCROLL_DEBUG) /*[*/
    vtrace("jump_proc(%f)\n", top);
#endif /*]*/
    if (!n_saved) {
	screen_set_thumb_traced(thumb_top, thumb_shown, n_saved, maxROWS,
		scrolled_back);
	return;
    }
    if (top > thumb_top_base) {	/* too far down */
	screen_set_thumb_traced(thumb_top_base, thumb_shown, n_saved,
		maxROWS, scrolled_back);
	sync_scroll(0);
    } else {
	save_image();
	sync_scroll((int)((1.0 - top) * n_saved));
    }
}

/*
 * Resynchronize the thumb (called when the scrollbar is turned on).
 */
void
rethumb(void)
{
    screen_set_thumb_traced(thumb_top, thumb_shown, n_saved, maxROWS,
	    scrolled_back);
}

static bool
Scroll_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Scroll", ia, argc, argv);
    if (check_argc("Scroll", argc, 1, 2) < 0) {
	return false;
    }

    if (argc == 1 && !strcasecmp(argv[0], "Forward")) {
	scroll_n(maxROWS, +1);
    } else if (argc == 1 && !strcasecmp(argv[0], "Backward")) {
	scroll_n(maxROWS, -1);
    } else if (argc == 1 && !strcasecmp(argv[0], "Reset")) {
	scroll_reset();
    } else if (argc == 2 && !strcasecmp(argv[0], "Set")) {
	int n;
	char *ptr;
	int curr = scrolled_back / maxROWS;

	n = strtol(argv[1], &ptr, 10);
	if (n < 0 || ptr == argv[1] || *ptr != '\0') {
	    popup_an_error("Invalid Scroll(Set,n) value");
	    return false;
	}
	if (n > n_saved / maxROWS) {
	    vtrace("scroll set: %d -> overflow\n", n);
	    n = n_saved / maxROWS;
	}
	if (n > curr) {
	    /* Scroll back further. */
	    scroll_n((n - curr) * maxROWS, -1);
	} else if (n < curr) {
	    /* Scroll back less. */
	    scroll_n((curr - n) * maxROWS, +1);
	}
    } else {
	popup_an_error("Scroll parameter must be Forward, Backward, Reset or "
		"Set,<n>");
	return false;
    }
    return true;
}

/*
 * Called when a host connects, disconnects or changes NVT/3270 modes.
 */
static void
scroll_connect(bool ignored _is_unused)
{
    if (CONNECTED) {
	if (IN_3270) {
	    scroll_round();
	}
    }
}

/**
 * Scrollbar module registration.
 */
void
scroll_register(void)
{
    static action_table_t scroll_actions[] = {
	{ "Scroll",		Scroll_action,	ACTION_KE }
    };

    /* Register the actions. */
    register_actions(scroll_actions, array_count(scroll_actions));

    /* Register the state change callbacks. */
    register_schange(ST_HALF_CONNECT, scroll_connect);
    register_schange(ST_CONNECT, scroll_connect);
    register_schange(ST_3270_MODE, scroll_connect);
}
