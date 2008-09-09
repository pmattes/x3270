/*
 * Copyright 2007-2008 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * x3270, c3270, wc3270, s3270, tcl3270, pr3287 and wpr3287 are distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the file LICENSE for more details.
 */

/*
 *	utf8c.h
 *		3270 Terminal Emulator
 *		UTF-8 conversions
 */

extern char *locale_codeset;
extern Boolean is_utf8;

extern void set_codeset(char *codeset_name);
extern int unicode_to_utf8(ucs4_t ucs4, char *utf8);
extern int utf8_to_unicode(const char *utf8, int len, ucs4_t *ucs4);
