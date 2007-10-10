/*
 * Copyright 1999, 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/* Non-display verson of statusc.h */

#define status_compose(on, c, keytype)
#define status_typeahead(on)
#define status_script(on)
#define status_reverse_mode(on)
#define status_insert_mode(on)
#define status_syswait()
#define status_reset()
#define status_twait()
#define status_oerr(error_type)
#define status_timing(t0, t1)
#define status_ctlr_done()
#define status_untiming()
#define status_kybdlock()
#define status_minus()
#define status_lu(lu)
