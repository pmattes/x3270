/*
 * (from) $XConsortium: SmeLineP.h,v 1.3 89/12/11 15:20:20 kit Exp $
 *
 * Modifications Copyright 1995, 1999, 2000, 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 *
 * Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Chris D. Peterson, MIT X Consortium
 */

/* 
 * CmeLineP.h - Private definitions for CmeLine widget
 * (from) SmeLineP.h - Private definitions for SmeLine widget
 * 
 */

#ifndef _XawCmeLineP_h
#define _XawCmeLineP_h

/***********************************************************************
 *
 * CmeLine Widget Private Data
 *
 ***********************************************************************/

#include "CmeP.h"
#include "CmeLine.h"

/************************************************************
 *
 * New fields for the CmeLine widget class record.
 *
 ************************************************************/

typedef struct _CmeLineClassPart {
  XtPointer extension;
} CmeLineClassPart;

/* Full class record declaration */
typedef struct _CmeLineClassRec {
    RectObjClassPart    rect_class;
    CmeClassPart	cme_class;
    CmeLineClassPart	cme_line_class;
} CmeLineClassRec;

extern CmeLineClassRec cmeLineClassRec;

/* New fields for the CmeLine widget record */
typedef struct {
    /* resources */
    Pixel foreground;		/* Foreground color. */
    Pixmap stipple;		/* Line Stipple. */
    Dimension line_width;	/* Width of the line. */

    /* private data.  */

    GC gc;			/* Graphics context for drawing line. */
} CmeLinePart;

/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _CmeLineRec {
  ObjectPart     object;
  RectObjPart    rectangle;
  CmePart	 cme;
  CmeLinePart	 cme_line;
} CmeLineRec;

/************************************************************
 *
 * Private declarations.
 *
 ************************************************************/

#endif /* _XawCmeLineP_h */
