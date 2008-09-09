/*
 * Modifications Copyright 1993-2008 by Paul Mattes.
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
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	charsetc.h
 *		Global declarations for charset.c
 */

extern Boolean charset_changed;
extern unsigned long cgcsgid;
#if defined(X3270_DBCS) /*[*/
extern unsigned long cgcsgid_dbcs;
#endif /*]*/
extern char *default_display_charset;
enum cs_result { CS_OKAY, CS_NOTFOUND, CS_BAD, CS_PREREQ, CS_ILLEGAL };
extern enum cs_result charset_init(char *csname);
extern char *get_charset_name(void);
extern void charset_list(void);
