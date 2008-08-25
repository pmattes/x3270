/*
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

extern ucs4_t ebcdic_to_unicode(ebc_t e, unsigned char cs,
	Boolean for_display);
extern ucs4_t ebcdic_base_to_unicode(ebc_t e,
	Boolean blank_undef, Boolean for_display);
extern ebc_t unicode_to_ebcdic(ucs4_t u);
extern ebc_t unicode_to_ebcdic_ge(ucs4_t u, Boolean *ge);
extern int set_uni(const char *csname, const char **codepage,
	const char **display_charsets);
extern int linedraw_to_unicode(ebc_t e);
extern int apl_to_unicode(ebc_t e);
#if !defined(_WIN32) && !defined(UNICODE_WCHAR) /*[*/
extern iconv_t i_u2mb;
extern iconv_t i_mb2u;
#endif /*]*/
