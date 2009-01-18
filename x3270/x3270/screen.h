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
 *	screen.h
 *
 *		Screen definitions for x3270.
 */

#define fCHAR_WIDTH(f)	((f)->max_bounds.width)
#define fCHAR_HEIGHT(f)	((f)->ascent + (f)->descent)

#define HHALO  2       /* number of pixels to pad screen left-right */
#define VHALO  1       /* number of pixels to pad screen top-bottom */

#define cwX_TO_COL(x_pos, cw) 	(((x_pos)-hhalo) / (cw))
#define chY_TO_ROW(y_pos, ch) 	(((y_pos)-vhalo) / (ch))
#define cwCOL_TO_X(col, cw)	(((col) * (cw)) + hhalo)
#define chROW_TO_Y(row, ch)	(((row)+1) * (ch) + vhalo)

#define ssX_TO_COL(x_pos) 	cwX_TO_COL(x_pos, ss->char_width)
#define ssY_TO_ROW(y_pos) 	chY_TO_ROW(y_pos, ss->char_height)
#define ssCOL_TO_X(col)		cwCOL_TO_X(col, ss->char_width)
#define ssROW_TO_Y(row)		chROW_TO_Y(row, ss->char_height)

#define X_TO_COL(x_pos) 	cwX_TO_COL(x_pos, *char_width)
#define Y_TO_ROW(y_pos) 	chY_TO_ROW(y_pos, *char_height)
#define COL_TO_X(col)		cwCOL_TO_X(col, *char_width)
#define ROW_TO_Y(row)		chROW_TO_Y(row, *char_height)

#define SGAP	(*descent+3) 	/* gap between screen and status line */

#define SCREEN_WIDTH(cw)	(cwCOL_TO_X(maxCOLS, cw) + hhalo)
#define SCREEN_HEIGHT(ch)	(chROW_TO_Y(maxROWS, ch) + vhalo+SGAP+vhalo)

/* selections */

#define SELECTED(baddr)		(selected[(baddr)/8] & (1 << ((baddr)%8)))
#define SET_SELECT(baddr)	(selected[(baddr)/8] |= (1 << ((baddr)%8)))

/*
 * Screen position structure.
 */
union sp {
	struct {
		unsigned cc  : 8;	/* character code */
		unsigned sel : 1;	/* selection status */
		unsigned fg  : 6;	/* foreground color (flag/inv/0-15) */
		unsigned gr  : 4;	/* graphic rendition */
		unsigned cs  : 3;	/* character set */
	} bits;
	unsigned long word;
};

/*
 * screen.c data structures. *
 */
extern int	 *char_width, *char_height;
extern unsigned char *selected;		/* selection bitmap */
extern int	 *ascent, *descent;
extern unsigned	 fixed_width, fixed_height;
extern int       hhalo, vhalo;
