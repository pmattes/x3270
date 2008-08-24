/*
 * Copyright 2008 by Paul Mattes.
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

int display8_init(char *cset);
int display8_lookup(int d8_ix, ucs4_t ucs4);
#if defined(X3270_DBCS) /*[*/
int display16_init(char *cset);
int display16_lookup(int d16_ix, ucs4_t ucs4);
#endif /*]*/
