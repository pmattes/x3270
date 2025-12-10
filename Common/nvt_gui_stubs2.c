/*
 * Copyright (c) 2014-2025 Paul Mattes.
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
 *	nvt_gui_stubs2.c
 *		Stubs for the NVT-mode GUI functions.
 */

#include "globals.h"

#include "model.h"
#include "xtwinops.h"

#include "nvt_gui.h"

void
xtwinops(unsigned short p1, unsigned short *p2, unsigned short *p3, unsigned short *rp1, unsigned short *rp2, const char **rtext)
{
    *rp1 = 0;
    *rp2 = 0;
    *rtext = NULL;

    switch (p1) {
    case XTW_8RESIZE_CHARACTERS: /* set screen size to h,w characters */
		/* omitted parameters re-use the current values */
		/* 0 parameters use the screen size (not supported) */
	if ((p2 && *p2 == 0) || (p3 && *p3 == 0)) {
	    /* Screen size not supported. */
	    return;
	}
	if (p2 || p3) {
	    live_change_oversize(p3? *p3: COLS, p2? *p2: ROWS);
	}
	break;
    default:
	if (p1 >= 24) {
	    live_change_oversize(COLS, p1);
	}
	break;
    }
}

void
get_screen_pixels(unsigned *height, unsigned *width)
{
    *height = 0;
    *width = 0;
}

void
get_window_pixels(unsigned *height, unsigned *width)
{
    *height = 0;
    *width = 0;
}

void
get_character_pixels(unsigned *height, unsigned *width)
{
    *height = 0;
    *width = 0;
}

void
get_window_location(int *x, int *y)
{
    *x = 0;
    *y = 0;
}

window_state_t
get_window_state(void)
{
    return WS_NONE;
}
