/*
 * Copyright 1999, 2000, 2002 by Paul Mattes.
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

/* c3270 verson of statusc.h */

#define status_kybdlock()
#define status_script(on)
#define status_timing(t0, t1)
#define status_untiming()

extern void status_compose(Boolean on, unsigned char c, enum keytype keytype);
extern void status_ctlr_done(void);
extern void status_insert_mode(Boolean on);
extern void status_lu(const char *);
extern void status_minus(void);
extern void status_oerr(int error_type);
extern void status_reset(void);
extern void status_reverse_mode(Boolean on);
extern void status_syswait(void);
extern void status_twait(void);
extern void status_typeahead(Boolean on);
