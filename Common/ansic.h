/*
 * Copyright 1995, 1999, 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	ansic.h
 *		Global declarations for ansi.c.
 */

#if defined(X3270_ANSI) /*[*/

extern void ansi_init(void);
extern void ansi_process(unsigned int c);
extern void ansi_send_clear(void);
extern void ansi_send_down(void);
extern void ansi_send_home(void);
extern void ansi_send_left(void);
extern void ansi_send_pa(int nn);
extern void ansi_send_pf(int nn);
extern void ansi_send_right(void);
extern void ansi_send_up(void);
extern void toggle_lineWrap(struct toggle *t, enum toggle_type type);

#else /*][*/

#define ansi_init()
#define ansi_process(n)
#define ansi_send_clear()
#define ansi_send_down()
#define ansi_send_home()
#define ansi_send_left()
#define ansi_send_pa(n)
#define ansi_send_pf(n)
#define ansi_send_right()
#define ansi_send_up()

#endif /*]*/
