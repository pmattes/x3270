/*
 * Modifications Copyright 1993, 1994, 1995, 1999, 2000, 2002, 2003, 2005 by
 *   Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
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
