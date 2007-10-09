/*
 * Copyright 2000, 2001, 2002, 2006, 2007 by Paul Mattes.
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

/* wc3270 version of keymapc.h */

#define KM_SHIFT	0x0001
#define KM_LCTRL	0x0002
#define KM_RCTRL	0x0004
#define KM_CTRL		(KM_LCTRL | KM_RCTRL)
#define KM_LALT		0x0008
#define KM_RALT		0x0010
#define KM_ALT		(KM_LALT | KM_RALT)

extern void keymap_init(void);
extern char *lookup_key(unsigned long xk, unsigned long state);
extern void keymap_dump(void);
extern const char *decode_key(int k, int hint, char *buf);
extern const char *lookup_cname(unsigned long ccode, Boolean special_only);
