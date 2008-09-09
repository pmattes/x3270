/*
 * Copyright 2000-2008 by Paul Mattes.
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

/* c3270 version of keymapc.h */

#define KM_CTRL		0x0001
#define KM_ALT		0x0002

extern void keymap_init(void);
extern char *lookup_key(int k, ucs4_t ucs4, int modifiers);
extern void keymap_dump(void);
extern const char *decode_key(int k, ucs4_t ucs4, int hint, char *buf);
