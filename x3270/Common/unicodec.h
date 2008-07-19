/*x
 * Copyright 2008 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * wc3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

extern unsigned long ebcdic_to_unicode(unsigned short e, int blank_undef);
extern unsigned short unicode_to_ebcdic(unsigned long u);
extern int set_uni(const char *csname);
extern int linedraw_to_unicode(unsigned short e);
extern int apl_to_unicode(unsigned short e);
extern Boolean is_utf8;
#if defined(USE_ICONV) /*[*/
extern iconv_t i_u2mb;
#endif /*]*/
