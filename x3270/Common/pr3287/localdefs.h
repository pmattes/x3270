/*
 * Copyright (c) 2000-2009, Paul Mattes.
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
 *	localdefs.h
 *		Local definitions for pr3287.
 *
 *		This file contains definitions for environment-specific
 *		facilities, such as memory allocation, I/O registration,
 *		and timers.
 */

/* These first definitions were cribbed from X11 -- but no X code is used. */
#define False 0
#define True 1
typedef void *XtPointer;
typedef void *Widget;
typedef void *XEvent;
typedef char Boolean;
typedef char *String;
typedef unsigned int Cardinal;
typedef unsigned long KeySym;
#define Bool int
typedef void (*XtActionProc)(
    Widget 		/* widget */,
    XEvent*		/* event */,
    String*		/* params */,
    Cardinal*		/* num_params */
);
typedef struct _XtActionsRec{
    String	 string;
    XtActionProc proc;
} XtActionsRec;
#define XtNumber(n)	(sizeof(n)/sizeof((n)[0]))
#define NoSymbol		0L

/* These are local functions with similar semantics to X functions. */
extern void *Malloc(size_t);
extern void Free(void *);
extern void *Calloc(size_t, size_t);
extern void *Realloc(void *, size_t);
extern char *NewString(const char *);
extern void Warning(const char *s);
extern void Error(const char *s);
#define X3270_TRACE 1

extern void errmsg(const char *, ...);
