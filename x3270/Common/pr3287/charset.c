/*
 * Copyright 2001, 2004, 2008 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * pr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	charset.c
 *		Limited character set support.
 */

#include "globals.h"

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#if !defined(_WIN32) /*[*/
#include <locale.h>
#include <langinfo.h>
#endif /*]*/

#include "3270ds.h"
#include "charsetc.h"
#include "unicodec.h"
#include "unicode_dbcsc.h"
#include "utf8c.h"

unsigned long cgcsgid = 0x02b90025;
unsigned long cgcsgid_dbcs = 0x02b90025;
int dbcs = 0;

char *encoding = CN;
char *converters = CN;

/*
 * Change character sets.
 * Returns 0 if the new character set was found, -1 otherwise.
 */
enum cs_result
charset_init(char *csname)
{
#if !defined(_WIN32) /*[*/
	char *codeset_name;
#endif /*]*/
    	const char *codepage;
	const char *display_charsets;

#if !defined(_WIN32) /*[*/
	setlocale(LC_ALL, "");
	codeset_name = nl_langinfo(CODESET);
	set_codeset(codeset_name);
#endif /*]*/

    	if (set_uni(csname, &codepage, &display_charsets) < 0)
		return CS_NOTFOUND;
	cgcsgid = strtoul(codepage, NULL, 0);
	if (!(cgcsgid & ~0xffff))
	    	cgcsgid |= 0x02b90000;

#if defined(X3270_DBCS) /*[*/
	if (set_uni_dbcs(csname, &codepage, &display_charsets) == 0) {
	    	dbcs = 1;
		cgcsgid_dbcs = strtoul(codepage, NULL, 0);
	}
#endif /*]*/

	return CS_OKAY;
}

/*
 * XXX: The balance of this file was copied from the common charset.c.
 * The rest of charset.c has no bearing on pr3287, so there's no point in
 * #ifdef'ing it to pieces.  It would be better if this logic was moved
 * to somewhere common, like unicode.c.
 */

/*
 * Translate an EBCDIC character to the current locale's multi-byte
 * representation.
 *
 * Returns the number of bytes in the multi-byte representation, including
 * the terminating NULL.  mb[] should be big enough to include the NULL
 * in the result.
 *
 * Also returns in 'ucp' the UCS-4 Unicode value of the EBCDIC character.
 *
 * If 'purpose' is TRANS_DISPLAY, the target of the translation is the
 * display window.  If it is TRANS_LOCAL, the target is the local file
 * system.  TRANS_DISPLAY is used only on Windows where the local file system
 * uses the ANSI code page, but the console uses the OEM code page.
 * At some point, TRANS_DISPLAY will apply to x3270 as well, indicating
 * translation to the current font's encoding.
 *
 * Note that 'ebc' is an uint16_t, not an unsigned char.  This is
 * so that DBCS values can be passed in as 16 bits (with the first byte
 * in the high-order bits).  There is no ambiguity because all valid EBCDIC
 * DBCS characters have a nonzero first byte.
 *
 * Returns 0 if 'blank_undef' is clear and there is no printable EBCDIC
 * translation for 'ebc'.
 *
 * Returns '?' in mb[] if there is no local multi-byte representation of
 * the EBCDIC character.
 */
int
ebcdic_to_multibyte_x(ebc_t ebc, unsigned char cs, char mb[],
	int mb_len, int blank_undef, trans_t purpose, ucs4_t *ucp)
{
    ucs4_t uc;

#if defined(_WIN32) /*[*/
    int nc;
    BOOL udc;
    wchar_t wuc;
#elif defined(UNICODE_WCHAR) /*][*/
    int nc;
    wchar_t wuc;
#else /*][*/
    char u8b[7];
    int nu8;
    char *inbuf, *outbuf;
    size_t inbytesleft, outbytesleft;
    size_t nc;
#endif /*]*/

    /* Translate from EBCDIC to Unicode. */
    uc = ebcdic_to_unicode(ebc, cs, (purpose == TRANS_DISPLAY));
    *ucp = uc;
    if (uc == 0) {
	if (blank_undef) {
	    mb[0] = ' ';
	    mb[1] = '\0';
	    return 2;
	} else {
	    return 0;
	}
    }

    /* Translae from Unicode to local multibyte. */

#if defined(_WIN32) /*[*/
    /*
     * wchar_t's are Unicode.
     * If TRANS_DISPLAY, use the OEM code page.
     * If TRANS_LOCAL, use the ANSI code page.  (Trace files are converted
     *  from ANSI to OEM by 'catf' for display in the pop-up console window.)
     */
    wuc = uc;
    nc = WideCharToMultiByte((purpose == TRANS_LOCAL)? CP_ACP: CP_OEMCP,
	    0, &wuc, 1, mb, 1, "?", &udc);
    if (nc != 0) {
	mb[1] = '\0';
	return 2;
    } else {
	mb[0] = '?';
	mb[1] = '\0';
	return 2;
    }

#elif defined(UNICODE_WCHAR) /*][*/
    /*
     * wchar_t's are Unicode.
     * If 'is_utf8' is set, use unicode_to_utf8().  This allows us to set
     *  'is_utf8' directly, ignoring the locale, for Tcl.
     * Otherwise, use wctomb().
     */
    if (is_utf8) {
	nc = unicode_to_utf8(uc, mb);
	if (nc < 0)
	    return 0;
	mb[nc++] = '\0';
	return nc;
    }

    /*
     * N.B.: This code assumes TRANS_LOCAL.
     * TRANS_DISPLAY will need special translation tables (and a different
     * function?) to go from Unicode to the various display character sets
     * supported by x3270.
     */
    wuc = uc;
    nc = wctomb(mb, uc);
    if (nc > 0) {
	/* Return to the initial shift state and null-terminate. */
	nc += wctomb(mb + nc, 0);
	return nc;
    } else {
	mb[0] = '?';
	mb[1] = '\0';
	return 2;
    }
#else /*][*/
    /*
     * Use iconv.
     * As with the UNICODE_WCHAR case above, this code assumes TRANS_LOCAL.
     */

    /* Translate the wchar_t we got from UCS-4 to UTF-8. */
    nu8 = unicode_to_utf8(uc, u8b);
    if (nu8 < 0)
	return 0;

    /* Local multi-byte might be UTF-8, in which case, we're done. */
    if (is_utf8) {
	memcpy(mb, u8b, nu8);
	mb[nu8++] = '\0';
	return nu8;
    }

    /* Let iconv translate from UTF-8 to local multi-byte. */
    inbuf = u8b;
    inbytesleft = nu8;
    outbuf = mb;
    outbytesleft = mb_len;
    nc = iconv(i_u2mb, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    if (nc < 0) {
	mb[0] = '?';
	mb[1] = '\0';
	return 2;
    }

    /* Return to the initial shift state. */
    nc = iconv(i_u2mb, NULL, NULL, &outbuf, &outbytesleft);
    if (nc < 0) {
	mb[0] = '?';
	mb[1] = '\0';
	return 0;
    }

    /* Null-terminate the return the length. */
    mb[mb_len - outbytesleft--] = '\0';
    return mb_len - outbytesleft;

#endif /*]*/
}

/* Commonest version of ebcdic_to_multibyte_x:
 *  cs is CS_BASE
 *  blank_undef is True
 *  purpose is TRANS_LOCAL
 *  ucp is ignored
 */
int
ebcdic_to_multibyte(ebc_t ebc, char mb[], int mb_len)
{
	ucs4_t ucs4;

    	return ebcdic_to_multibyte_x(ebc, CS_BASE, mb, mb_len, True,
		TRANS_LOCAL, &ucs4);
}

/*
 * Convert an EBCDIC string to a multibyte string.
 * Makes lots of assumptions: standard character set, TRANS_LOCAL, blank_undef.
 * Returns the length of the multibyte string.
 */
int
ebcdic_to_multibyte_string(unsigned char *ebc, size_t ebc_len, char mb[],
	size_t mb_len)
{
    	int nmb = 0;

    	while (ebc_len && mb_len) {
	    	int xlen;

		xlen = ebcdic_to_multibyte(*ebc, mb, mb_len);
		if (xlen) {
		    	mb += xlen - 1;
			mb_len -= (xlen - 1);
			nmb += xlen - 1;
		}
		ebc++;
		ebc_len--;
	}
	return nmb;
}

/*
 * Return the maximum buffer length needed to translate 'len' EBCDIC characters
 * in the current locale.
 */
int
mb_max_len(int len)
{
#if defined(_WIN32) /*[*/
    /*
     * On Windows, it's 1:1 (we don't do DBCS, and we don't support locales
     * like UTF-8).
     */
    return len + 1;
#elif defined(UNICODE_WCHAR) /*][*/
    /* Allocate enough space for shift-state transitions. */
    return (MB_CUR_MAX * (len * 2)) + 1;
#else /*]*/
    if (is_utf8)
	return (len * 6) + 1;
    else
	/*
	 * We don't actually know.  Guess that MB_CUR_MAX is 16, and compute
	 * as for UNICODE_WCHAR.
	 */
	return (16 * (len * 2)) + 1;
#endif /*]*/
}

/*
 * Translate a multi-byte character in the current locale to UCS-4.
 *
 * Returns a UCS-4 character or 0, indicating an error in translation.
 * Also returns the number of characters consumed.
 */
ucs4_t
multibyte_to_unicode(const char *mb, size_t mb_len, int *consumedp,
	enum me_fail *errorp)
{
    size_t nw;
    ucs4_t ucs4;
#if defined(_WIN32) /*[*/
    wchar_t wc[3];

    /*
     * Use MultiByteToWideChar() to get from the ANSI codepage to UTF-16.
     * Note that we pass in 1 for the MB length because we assume 8-bit code
     * pages.  If someone somehow sets the ANSI codepage to UTF-7 or UTF-8,
     * this won't work.
     */
    nw = MultiByteToWideChar(CP_ACP, 0, mb, 1, wc, 3);
    if (nw == 0) {
	*errorp = ME_INVALID;
	return 0;
    }
    *consumedp = 1;
    ucs4 = wc[0];
#elif defined(UNICODE_WCHAR) /*][*/
    wchar_t wc[3];
    /* wchar_t's are Unicode. */

    if (is_utf8) {
	int nc;

	/*
	 * Use utf8_to_unicode() instead of mbtowc(), so we can set is_utf8
	 * directly and ignore the locale for Tcl.
	 */
	nc = utf8_to_unicode(mb, mb_len, &ucs4);
	if (nc > 0) {
	    *errorp = ME_NONE;
	    *consumedp = nc;
	    return ucs4;
	} else if (nc == 0) {
	    *errorp = ME_SHORT;
	    return 0;
	} else {
	    *errorp = ME_INVALID;
	    return 0;
	}
    }

    /* mbtowc() will translate to Unicode. */
    nw = mbtowc(wc, mb, mb_len);
    if (nw == (size_t)-1) {
	if (errno == EILSEQ)
	    *errorp = ME_INVALID;
	else
	    *errorp = ME_SHORT;
	(void) mbtowc(NULL, NULL, 0);
	return 0;
    }

    /*
     * Reset the shift state.
     * XXX: Doing this will ruin the shift state if this function is called
     * repeatedly to process a string.  There should probably be a parameter
     * passed in to control whether or not to reset the shift state, or
     * perhaps there should be a function to translate a string.
     */
    (void) mbtowc(NULL, NULL, 0);
    *consumedp = nw;

    ucs4 = wc[0];
#else /*][*/
    /* wchar_t's have unknown encoding. */
    if (!is_utf8) {
	char *inbuf, *outbuf;
	size_t inbytesleft, outbytesleft;
	char utf8buf[16];

	/* Translate from local MB to UTF-8 using iconv(). */
	inbuf = (char *)mb;
	outbuf = utf8buf;
	inbytesleft = mb_len;
	outbytesleft = sizeof(utf8buf);
	nw = iconv(i_mb2u, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	if (nw < 0) {
	    if (errno == EILSEQ)
		*errorp = ME_INVALID;
	    else
		*errorp = ME_SHORT;
	    (void) iconv(i_mb2u, NULL, NULL, NULL, NULL);
	    return 0;
	}
	*consumedp = mb_len - inbytesleft;

	/* Translate from UTF-8 to UCS-4. */
	(void) utf8_to_unicode(utf8buf, sizeof(utf8buf) - outbytesleft, &ucs4);
    } else {
	/* Translate from UTF-8 to UCS-4. */
	nw = utf8_to_unicode(mb, mb_len, &ucs4);
	if (nw < 0) {
	    *errorp = ME_INVALID;
	    return 0;
	}
	if (nw == 0) {
	    *errorp = ME_SHORT;
	    return 0;
	}
	*consumedp = nw;
    }
#endif /*]*/

    /* Translate from UCS4 to EBCDIC. */
    return ucs4;
}

/*
 * Convert a multi-byte string to a UCS-4 string.
 * Does not NULL-terminate the result.
 * Returns the number of UCS-4 characters stored.
 */
int
multibyte_to_unicode_string(char *mb, size_t mb_len, ucs4_t *ucs4,
	size_t u_len)
{
    int consumed;
    enum me_fail error;
    int nr = 0;

    error = ME_NONE;

    while (u_len && mb_len &&
	    (*ucs4++ = multibyte_to_unicode(mb, mb_len, &consumed,
					    &error)) != 0) {
	u_len--;
	mb += consumed;
	mb_len -= consumed;
	nr++;
    }

    if (error != ME_NONE)
	return -1;
    else
	return nr;
}

/*
 * Translate a multi-byte character in the current locale to an EBCDIC
 * character.
 *
 * Returns an 8-bit (SBCS) or 16-bit (DBCS) EBCDIC character, or 0, indicating
 * an error in translation.  Also returns the number of characters consumed.
 *
 * Unlike ebcdic_to_multibyte, there is no 'purpose' parameter.  On Windows,
 * this function is only used to translate local file system strings to
 * EBCDIC (TRANS_LOCAL).  Input keystrokes (which would use TRANS_DISPLAY)
 * are UTF-16 strings, not OEM-codepage bytes.
 */
ebc_t
multibyte_to_ebcdic(const char *mb, size_t mb_len, int *consumedp,
	enum me_fail *errorp)
{
    ucs4_t ucs4;

    ucs4 = multibyte_to_unicode(mb, mb_len, consumedp, errorp);
    if (ucs4 == 0)
	return 0;
    return unicode_to_ebcdic(ucs4);
}

/*
 * Convert a local multi-byte string to an EBCDIC string.
 * Returns the length of the resulting EBCDIC string, or -1 if there is a
 * conversion error.
 */
int
multibyte_to_ebcdic_string(char *mb, size_t mb_len, unsigned char *ebc,
	size_t ebc_len, enum me_fail *errorp)
{
    int ne = 0;
    Boolean in_dbcs = False;

    while (mb_len > 0 && ebc_len > 0) {
	ebc_t e;
	int consumed;

	e = multibyte_to_ebcdic(mb, mb_len, &consumed, errorp);
	if (e == 0)
	    return -1;
	if (e & 0xff00) {
	    /* DBCS. */
	    if (!in_dbcs) {
		/* Make sure there's room for SO, b1, b2, SI. */
		if (ebc_len < 4)
		    return ne;
		*ebc++ = EBC_so;
		ebc_len++;
		ne++;
		in_dbcs = True;
	    }
	    /* Make sure there's room for b1, b2, SI. */
	    if (ebc_len < 3) {
		*ebc++ = EBC_si;
		ne++;
		return ne;
	    }
	    *ebc++ = (e >> 8) & 0xff;
	    *ebc++ = e & 0xff;
	    ebc_len -= 2;
	    ne += 2;
	} else {
	    /* SBCS. */
	    if (in_dbcs) {
		*ebc++ = EBC_si;
		ne++;
		if (!--ebc_len)
		    return ne;
		in_dbcs = False;
	    }
	    *ebc++ = e & 0xff;
	    ebc_len--;
	    ne++;
	}
	mb += consumed;
	mb_len -= consumed;
    }

    /*
     * Terminate the DBCS string, if we end inside it.
     * We're guaranteed to have space for the SI; we checked before adding
     * the last DBCS character.
     */
    if (in_dbcs) {
	*ebc++ = EBC_si;
	ne++;
    }

    return ne;
}

/*
 * Translate a UCS-4 character to a local multi-byte string.
 */
int
unicode_to_multibyte(ucs4_t ucs4, char *mb, size_t mb_len)
{
#if defined(_WIN32) /*[*/
    wchar_t wuc = ucs4;
    BOOL udc;
    int nc;

    nc = WideCharToMultiByte(CP_ACP, 0, &wuc, 1, mb, mb_len, "?", &udc);
    return nc;
#elif defined(UNICODE_WCHAR) /*][*/
    int nc;

    if (is_utf8) {
	nc = unicode_to_utf8(ucs4, mb);
	if (nc < 0)
	    return 0;
	mb[nc++] = '\0';
	return nc;
    }

    nc = wctomb(mb, ucs4);
    if (nc > 0) {
	/* Return to the initial shift state and null-terminate. */
	nc += wctomb(mb + nc, 0);
	return nc;
    } else {
	mb[0] = '?';
	mb[1] = '\0';
	return 2;
    }
#else /*][*/
    int nu8;
    char u8b[16];
    char *inbuf, *outbuf;
    size_t inbytesleft, outbytesleft;
    size_t nc;

    /* Use iconv. */

    /* Translate the wchar_t we got from UCS-4 to UTF-8. */
    nu8 = unicode_to_utf8(ucs4, u8b);
    if (nu8 < 0)
	return 0;

    /* Local multi-byte might be UTF-8, in which case, we're done. */
    if (is_utf8) {
	memcpy(mb, u8b, nu8);
	mb[nu8++] = '\0';
	return nu8;
    }

    /* Let iconv translate from UTF-8 to local multi-byte. */
    inbuf = u8b;
    inbytesleft = nu8;
    outbuf = mb;
    outbytesleft = mb_len;
    nc = iconv(i_u2mb, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    if (nc == (size_t)-1) {
	mb[0] = '?';
	mb[1] = '\0';
	return 2;
    }

    /* Return to the initial shift state. */
    nc = iconv(i_u2mb, NULL, NULL, &outbuf, &outbytesleft);
    if (nc == (size_t)-1) {
	mb[0] = '?';
	mb[1] = '\0';
	return 0;
    }

    /* Null-terminate the return the length. */
    mb[mb_len - outbytesleft--] = '\0';
    return mb_len - outbytesleft;
#endif /*]*/
}
