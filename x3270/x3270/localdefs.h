/*
 * Copyright 2000, 2001 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	localdefs.h
 *		Local definitions for x3270.
 *
 *		This file contains definitions for environment-specific
 *		facilities, such as memory allocation, I/O registration,
 *		and timers.
 */

/* Use X for this stuff. */
#include <X11/Intrinsic.h>

#define Malloc(n)	XtMalloc(n)
#define Free(p)		XtFree((void *)p)
#define Calloc(n, s)	XtCalloc(n, s)
#define Realloc(p, s)	XtRealloc((XtPointer)p, s)
#define NewString(s)	XtNewString(s)

#define Error(s)	XtError(s)
#define Warning(s)	XtWarning(s)

/* "Required" optional parts. */
#define X3270_DISPLAY	1
