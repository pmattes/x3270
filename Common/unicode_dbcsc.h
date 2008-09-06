/*
 * Copyright 2008 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

#if defined(X3270_DBCS) /*[*/
extern ucs4_t ebcdic_dbcs_to_unicode(ebc_t e, Boolean blank_undef);
extern ebc_t unicode_to_ebcdic_dbcs(ucs4_t u);
extern int set_uni_dbcs(const char *csname, const char **codepage,
	const char **display_charsets);
extern void charset_list_dbcs(void);
#endif /*]*/
