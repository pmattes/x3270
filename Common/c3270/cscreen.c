/*
 * Copyright (c) 2000-2026 Paul Mattes.
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
 *	cscreen.c
 *		A curses-based 3270 Terminal Emulator
 *		A Widows Console-based 3270 Terminal Emulator
 *		Common screen-drawing logic and state
 */

#include "globals.h"

#include "3270ds.h"
#include "appres.h"
#include "ctlrc.h"
#include "trace.h"
#include "utils.h"

#include "cscreen.h"

/* Macros. */
#if !defined(_WIN32) /*[*/
# define PROVIDER	"terminal"
# define IN_ON		"on"
#else /*][*/
# define PROVIDER	"console window"
# define IN_ON		"in"
#endif /*]*/
#define WONT_FIT	"%s won't fit " IN_ON " a " PROVIDER " with %d rows and %d columns.\n\
Minimum is %d rows and %d columns."

/* Globals. */
bool screen_initted = false;
bool escaped = true;

int status_row = 0;	/* Row to display the status line on */
int status_skip = 0;	/* Row to blank above the status line */
int screen_yoffset = 0;	/* Vertical offset to top of screen.
			   If 0, there is no menu bar.
			   If 1, there is a menu bar and no space under it.
			   If 2, there is a menu bar with a space under it. */

/* Default colors in RGB mode, in X11 format (00RRGGBB), indexed by host color. */
unsigned rgbmap[MAX_HOST_COLORS] = {
    0x101010,	/* neutral black */
    0x1e90ff,	/* blue */
    0xff0000,	/* red */
    0xff00ff,	/* pink */
    0x32cd32,	/* green */
    0x00ffff,	/* turquoise */
    0xffff00,	/* yellow */
    0xffffff,	/* neutral white */
    0x2f4f4f,	/* black */
    0x0000cd,	/* deep blue */
    0xffa500,	/* orange */
    0xa020f0,	/* purple */
    0x90ee90,	/* pale green */
    0x96cdcd,	/* pale turquoise */
    0x778899,	/* gray */
    0xf5f5f5,	/* white */
};
/* Default reverse-video colors in RGB mode. */
unsigned rgbmap_rv[MAX_HOST_COLORS] = {
    0xffffff,	/* neutral black (reversed) */
    0x0000ff,	/* blue */
    0xb22222,	/* red */
    0xee6aa7,	/* pink */
    0x00cc00,	/* green */
    0x40e0d0,	/* turquoise */
    0xcdcd00,	/* yellow */
    0x000000,	/* neutral white (reversed) */
    0x000000,	/* black */
    0x0000cd,	/* deep blue */
    0xffa500,	/* orange */
    0xa020f0,	/* purple */
    0x98fb98,	/* pale green */
    0x96cdcd,	/* pale turquoise */
    0xbebebe,	/* gray */
    0xf5f5f5,	/* white */
};

/*
 * Computes the number of rows we require for a given model.
 * It includes at least one row for the menubar and one row for the oia (if mouse support is present),
 * except on model 2 and (if doing 80/132 switching) model 5.
 */
static int
min_rows(int model)
{
    return model_rows(model) + model_min_xtra(model);
}

/*
 * Compute the maximum number of rows we can have, given a hard upper limit.
 */
static int
adjust_rows(int target_ov_rows, int target_model_num)
{
    if (appres.interactive.menubar && target_ov_rows > model_rows(target_model_num)) {
	target_ov_rows--;
    }
    if (appres.interactive.menubar && target_ov_rows > model_rows(target_model_num)) {
	target_ov_rows--;
    }
    if (appres.c3270.oia && target_ov_rows > model_rows(target_model_num)) {
	target_ov_rows--;
    }
    if (appres.c3270.oia && target_ov_rows > model_rows(target_model_num)) {
	target_ov_rows--;
    }
    return target_ov_rows;
}

/**
 * Adapt the screen size (model, oversize) to the curses terminal dimensions
 * Returns NULL for success, an error message for failure.
 *
 * @param[in] want_model_num	Desired model number
 * @param[in] want_ov_auto	True if automatic oversize is desired
 * @param[in] want_ov_rows	Desired non-automatic oversize rows
 * @param[in] want_ov_cols	Desired non-automatic oversize columns
 * @param[in] hard_rows		Hard constraint on rows
 * @param[in] hard_cols		Hard constraint on columns
 *
 * @returns NULL on success, error message text on failure
 */
char *
screen_adapt(int want_model_num, bool want_ov_auto, int want_ov_rows, int want_ov_cols, int hard_rows, int hard_cols)
{
    /* Make temporary copies of the dimensions. */
    int target_model_num = want_model_num;
    int target_ov_rows = want_ov_rows;
    int target_ov_cols = want_ov_cols;

    /* Shrink the basic model, if necessary. */
    while (min_rows(target_model_num) > hard_rows || model_cols(target_model_num) > hard_cols) {
	/* If we're at the smallest screen already, give up. */
	if (target_model_num == 2) {
	    return Asprintf(WONT_FIT, app, hard_rows, hard_cols, MODEL_2_ROWS, MODEL_2_COLS);
	}

	/* Go to a smaller model. */
	target_model_num--;
    }

    /*
     * At this point, we are guaranteed that the curses terminal is big enough to hold
     * the basic dimensions of the target model, plus extra rows for the minimum menubar
     * and OIA on models 3, 4 and (sometimes) 5.
     */
    if (want_ov_auto) {
	if (hard_rows * hard_cols <= MAX_ROWS_COLS) {
	    /*
	     * Apply auto-oversize.
	     * Unlike specific oversize, we will reduce the number of oversize rows to make
	     * more visual space for the menubar and OIA if we can.
	     */
	    target_ov_rows = adjust_rows(hard_rows - model_min_xtra(target_model_num), target_model_num);
	    target_ov_cols = hard_cols;

	    target_model_num = 2;
	} else {
	    vctrace(TC_UI, "screen_adapt: ignoring auto-oversize because the %s is too big\n", PROVIDER);
	}
    } else if (target_ov_rows > 0 || target_ov_cols > 0) {
	/*
	 * Apply specific oversize.
	 * We will squeeze the menubar and OIA to try to get as close to the requested
	 * oversize rows as possible.
	 */
	int max_rows = adjust_rows(hard_rows - model_min_xtra(target_model_num), target_model_num);

	if (max_rows < target_ov_rows) {
	    target_ov_rows = max_rows;
	}
	if (target_ov_cols > hard_cols) {
	    target_ov_cols = hard_cols;
	}

	target_model_num = 2;
    }

    /* Trace what changed. */
    if (target_model_num != want_model_num) {
	vctrace(TC_UI, "screen_adapt: model %d -> %d\n", want_model_num, target_model_num);
    }
    if (target_ov_cols != want_ov_cols || target_ov_rows != want_ov_rows) {
	vctrace(TC_UI, "screen_adapt: oversize rows/cols %d/%d -> %d/%d\n",
		want_ov_auto? -1: want_ov_rows,
		want_ov_auto? -1: want_ov_cols,
		target_ov_rows,
		target_ov_cols);
    }

    /* Set the new dimensions. */
    set_cols_rows(target_model_num, target_ov_cols, target_ov_rows);

    /* Set the minimum number of rows and columns. */
    screen_set_minimum_rows_cols(maxROWS, maxCOLS);

    /* Place the menubar and OIA. */
    set_status_row(screen_map_rows(hard_rows), maxROWS);

    /* Success. */
    return NULL;
}

/* Calculate where the status line goes now. */
void
set_status_row(int hard_rows, int emulator_rows)
{
    /* Start by assuming nothing will fit. */
    bool menubar = false;
    bool space_under_menubar = false;
    bool line_over_oia = false;
    bool oia = false;

    if (hard_rows > emulator_rows && (appres.interactive.menubar || appres.c3270.oia)) {
	/* There's room and they want decoration(s). Figure out what will fit. */
	if (appres.interactive.menubar && appres.c3270.oia) {
	    /* Both menubar and OIA wanted. */
	    if (hard_rows >= emulator_rows + 4) {
		/* Everything fits. */
		menubar = true;
		space_under_menubar = true;
		line_over_oia = true;
		oia = true;
	    } else if (hard_rows >= emulator_rows + 3) {
		/* No line above the OIA. */
		menubar = true;
		space_under_menubar = true;
		oia = true;
	    } else if (hard_rows >= emulator_rows + 2) {
		/* No space under the menubar, either. */
		menubar = true;
		oia = true;
	    } else if (hard_rows >= emulator_rows + 1) {
		/* No menubar. */
		oia = true;
	    }
	    /* Else no menubar or OIA. */
	} else if (appres.interactive.menubar) {
	    /* Just a menubar wanted. */
	    if (hard_rows >= emulator_rows + 2) {
		/* Everything fits. */
		menubar = true;
		space_under_menubar = true;
	    } else if (hard_rows >= emulator_rows + 1) {
		/* No space under the menubar. */
		menubar = true;
	    }
	    /* Else no menubar. */
	} else if (appres.c3270.oia) {
	    /* Just an OIA wanted. */
	    if (hard_rows >= emulator_rows + 2) {
		/* Everything fits. */
		line_over_oia = true;
		oia = true;
	    } else if (hard_rows >= emulator_rows + 1) {
		/* No line over the OIA. */
		oia = true;
	    }
	    /* Else no OIA. */
	}
    }

    screen_yoffset = menubar? (space_under_menubar? 2: 1): 0;
    status_skip = line_over_oia? hard_rows - 2: 0;
    status_row = oia? hard_rows - 1: 0;

    vctrace(TC_UI, "set_status_row: hard_rows %d emulator_rows %d -> status_skip %d status_row %d screen_yoffset %d\n",
            hard_rows, emulator_rows, status_skip, status_row, screen_yoffset);
}

/*
 * Return a visible control character for a field attribute.
 */
unsigned char
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
