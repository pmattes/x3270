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

extern int ebcdic_to_multibyte_x(ebc_t ebc, unsigned char cs,
	char mb[], int mb_len, int blank_undef, ucs4_t *uc);
extern int ebcdic_to_multibyte(ebc_t ebc, char mb[], int mb_len);
extern int ebcdic_to_multibyte_string(unsigned char *ebc, size_t ebc_len,
	char mb[], size_t mb_len);
extern int mb_max_len(int len);
enum me_fail {
    ME_NONE,		/* no error */
    ME_INVALID,		/* invalid sequence */
    ME_SHORT		/* incomplete sequence */
};
extern ucs4_t multibyte_to_unicode(const char *mb, size_t mb_len, 
	int *consumedp, enum me_fail *errorp);
extern int multibyte_to_unicode_string(char *mb, size_t mb_len,
	ucs4_t *ucs4, size_t u_len);
extern ebc_t multibyte_to_ebcdic(const char *mb, size_t mb_len, 
	int *consumedp, enum me_fail *errorp);
extern int multibyte_to_ebcdic_string(char *mb, size_t mb_len, 
	unsigned char *ebc, size_t ebc_len, enum me_fail *errorp);
extern int unicode_to_multibyte(ucs4_t ucs4, char *mb, size_t mb_len);
