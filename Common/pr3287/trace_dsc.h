/*
 * Copyright 1995, 1999, 2000, 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * pr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	trace_dsc.h
 *		Global declarations for trace_ds.c.
 */

#if defined(X3270_TRACE) /*[*/

extern FILE *tracef;
extern Boolean trace_skipping;

const char *rcba(int baddr);
const char *see_aid(unsigned char code);
const char *see_attr(unsigned char fa);
const char *see_color(unsigned char setting);
const char *see_ebc(unsigned char ch);
const char *see_efa(unsigned char efa, unsigned char value);
const char *see_efa_only(unsigned char efa);
const char *see_qcode(unsigned char id);
void trace_ansi_disc(void);
void trace_char(char c);
void trace_ds(const char *fmt, ...);
void trace_dsn(const char *fmt, ...);
void trace_event(const char *fmt, ...);
void trace_screen(void);
const char *unknown(unsigned char value);

#else /*][*/

#define tracef 0
#define trace_ds 0 &&
#define trace_dsn 0 &&
#define trace_event 0 &&
#define rcba 0 &&
#define see_aid 0 &&
#define see_attr 0 &&
#define see_color 0 &&
#define see_ebc 0 &&
#define see_efa 0 &&
#define see_efa_only 0 &&
#define see_qcode 0 &&

#endif /*]*/
