/*
 * Copyright 1995, 1999, 2000 by Paul Mattes.
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
 *	scrollc.h
 *		Global declarations for scroll.c.
 */

extern void jump_proc(float top);
extern void rethumb(void);
extern void scroll_init(void);
extern void scroll_proc(int n, int total);
extern void scroll_round(void);
extern void scroll_save(int n, Boolean trim_blanks);
extern void scroll_to_bottom(void);
