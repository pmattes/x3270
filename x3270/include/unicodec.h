/*
 * Copyright (c) 2008-2009, 2015 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	unicode.h
 *		Declarations for Unicode translation functions.
 */

extern bool dbcs_allowed;

/* EBCDIC-to-Unicode options */
#define EUO_NONE	0x00000000	/* no options */
#define EUO_BLANK_UNDEF	0x00000001	/* if undefined, return U+0020 */
#define EUO_UPRIV	0x00000002	/* translate FM/DUP/SUB/EO to UPRIV */
#define EUO_ASCII_BOX	0x00000004	/* use ASCII for box drawing */
#define EUO_APL_CIRCLED	0x00000010	/* map APL underscored capital letters
					   to DBCS circled alphabetics */

ucs4_t ebcdic_to_unicode(ebc_t e, unsigned char cs, unsigned flags);
ucs4_t ebcdic_base_to_unicode(ebc_t e, unsigned flags);
ebc_t unicode_to_ebcdic(ucs4_t u);
ebc_t unicode_to_ebcdic_ge(ucs4_t u, bool *ge);
bool set_uni(const char *csname, int local_cp, const char **host_codepage,
	const char **cgcsgid, const char **realnamep, bool *is_dbcs);
int linedraw_to_unicode(ebc_t e);
int apl_to_unicode(ebc_t e, unsigned flags);
#if !defined(_WIN32) && !defined(UNICODE_WCHAR) /*[*/
extern iconv_t i_u2mb;
extern iconv_t i_mb2u;
#endif /*]*/

size_t ebcdic_to_multibyte_x(ebc_t ebc, unsigned char cs, char mb[],
	size_t mb_len, unsigned flags, ucs4_t *uc);
size_t ebcdic_to_multibyte(ebc_t ebc, char mb[], size_t mb_len);
size_t ebcdic_to_multibyte_string(unsigned char *ebc, size_t ebc_len, char mb[],
	size_t mb_len);
int mb_max_len(int len);
enum me_fail {
    ME_NONE,		/* no error */
    ME_INVALID,		/* invalid sequence */
    ME_SHORT		/* incomplete sequence */
};
ucs4_t multibyte_to_unicode(const char *mb, size_t mb_len, int *consumedp,
	enum me_fail *errorp);
int multibyte_to_unicode_string(const char *mb, size_t mb_len, ucs4_t *ucs4,
	size_t u_len);
ebc_t multibyte_to_ebcdic(const char *mb, size_t mb_len, int *consumedp,
	enum me_fail *errorp);
int multibyte_to_ebcdic_string(char *mb, size_t mb_len, unsigned char *ebc,
	size_t ebc_len, enum me_fail *errorp);
int unicode_to_multibyte(ucs4_t ucs4, char *mb, size_t mb_len);
