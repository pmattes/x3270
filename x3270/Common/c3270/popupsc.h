/*
 * Copyright 1999, 2000, 2006 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * c3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/* c3270 version of popupsc.h */

extern void action_output(const char *fmt, ...) printflike(1, 2);
extern void popup_an_errno(int errn, const char *fmt, ...) printflike(2, 3);
extern void popup_an_error(const char *fmt, ...) printflike(1, 2);
