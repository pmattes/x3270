/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001, 2002,
 *  2003, 2004, 2005, 2006, 2007, 2008 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	charset.c
 *		This module handles character sets.
 */

#include "globals.h"

#include "3270ds.h"
#include "resources.h"
#include "appres.h"
#include "cg.h"

#include "charsetc.h"
#include "kybdc.h"
#include "popupsc.h"
#if defined(X3270_DISPLAY) || (defined(C3270) && !defined(_WIN32)) /*[*/
#include "screenc.h"
#endif /*]*/
#include "tablesc.h"
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"
#include "widec.h"

#include <errno.h>
#include <locale.h>
#if !defined(_WIN32) /*[*/
#include <langinfo.h>
#endif /*]*/

#if defined(_WIN32) /*[*/
#include <windows.h>
#endif /*]*/

#define EURO_SUFFIX	"-euro"
#define ES_SIZE		(sizeof(EURO_SUFFIX) - 1)

/* Globals. */
Boolean charset_changed = False;
#define DEFAULT_CGEN	0x02b90000
#define DEFAULT_CSET	0x00000025
unsigned long cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;
unsigned long cgcsgid_dbcs = 0L;
char *default_display_charset = "3270cg-1a,3270cg-1,iso8859-1";
char *converter_names;
char *encoding;

/* Statics. */
static enum cs_result charset_init2(char *csname, const char *codepage,
	const char *display_charsets);
static void set_cgcsgids(const char *spec);
static int set_cgcsgid(char *spec, unsigned long *idp);
static void set_charset_name(char *csname);

static char *charset_name = CN;

#if defined(X3270_DBCS) /*[*/
/*
 * Initialize the DBCS conversion functions, based on resource values.
 */
static int
wide_resource_init(char *csname)
{
	char *cn, *en;

	cn = get_fresource("%s.%s", ResDbcsConverters, csname);
	if (cn == CN)
		return 0;

	en = get_fresource("%s.%s", ResLocalEncoding, csname);
	if (en == CN)
		en = appres.local_encoding;
	Replace(converter_names, cn);
	Replace(encoding, en);

	return wide_init(cn, en);

}
#endif /*]*/

/*
 * Change character sets.
 */
enum cs_result
charset_init(char *csname)
{
    	enum cs_result rc;
#if !defined(_WIN32) /*[*/
	char *codeset_name;
#endif /*]*/
	const char *codepage;
	const char *display_charsets;

#if !defined(_WIN32) /*[*/
	/* Get all of the locale stuff right. */
	setlocale(LC_ALL, "");

	/* Figure out the locale code set (character set encoding). */
	codeset_name = nl_langinfo(CODESET);
	set_codeset(codeset_name);
#endif /*]*/

	/* Do nothing, successfully. */
	if (csname == CN || !strcasecmp(csname, "us")) {
		set_cgcsgids(CN);
		set_charset_name(CN);
#if defined(X3270_DISPLAY) || (defined(C3270) && !defined(_WIN32)) /*[*/
		(void) screen_new_display_charsets(default_display_charset,
		    "us");
#endif /*]*/
		(void) set_uni("us", &codepage, &display_charsets);
		return CS_OKAY;
	}

	if (set_uni(csname, &codepage, &display_charsets) < 0)
		return CS_NOTFOUND;

	rc = charset_init2(csname, codepage, display_charsets);
	if (rc != CS_OKAY) {
		return rc;
	}

#if defined(X3270_DBCS) /*[*/
	if (wide_resource_init(csname) < 0) {
		return CS_NOTFOUND;
	}
#endif /*]*/

	return CS_OKAY;
}

/* Set a CGCSGID.  Return 0 for success, -1 for failure. */
static int
set_cgcsgid(char *spec, unsigned long *r)
{
	unsigned long cp;
	char *ptr;

	if (spec != CN &&
	    (cp = strtoul(spec, &ptr, 0)) &&
	    ptr != spec &&
	    *ptr == '\0') {
		if (!(cp & ~0xffffL))
			*r = DEFAULT_CGEN | cp;
		else
			*r = cp;
		return 0;
	} else
		return -1;
}

/* Set the CGCSGIDs. */
static void
set_cgcsgids(const char *spec)
{
	int n_ids = 0;
	char *spec_copy;
	char *buf;
	char *token;

	if (spec != CN) {
		buf = spec_copy = NewString(spec);
		while (n_ids >= 0 && (token = strtok(buf, "+")) != CN) {
			unsigned long *idp = NULL;

			buf = CN;
			switch (n_ids) {
			case 0:
			    idp = &cgcsgid;
			    break;
#if defined(X3270_DBCS) /*[*/
			case 1:
			    idp = &cgcsgid_dbcs;
			    break;
#endif /*]*/
			default:
			    popup_an_error("Extra CGCSGID(s), ignoring");
			    break;
			}
			if (idp == NULL)
				break;
			if (set_cgcsgid(token, idp) < 0) {
				popup_an_error("Invalid CGCSGID '%s', ignoring",
				    token);
				n_ids = -1;
				break;
			}
			n_ids++;
		}
		Free(spec_copy);
		if (n_ids > 0)
			return;
	}

	cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;
#if defined(X3270_DBCS) /*[*/
	cgcsgid_dbcs = 0L;
#endif /*]*/
}

/* Set the global charset name. */
static void
set_charset_name(char *csname)
{
	if (csname == CN) {
		Replace(charset_name, NewString("us"));
		charset_changed = False;
		return;
	}
	if ((charset_name != CN && strcmp(charset_name, csname)) ||
	    (appres.charset != CN && strcmp(appres.charset, csname))) {
		Replace(charset_name, NewString(csname));
		charset_changed = True;
	}
}

/* Character set init, part 2. */
static enum cs_result
charset_init2(char *csname, const char *codepage, const char *display_charsets)
{
	const char *rcs = display_charsets;
	int n_rcs = 0;
	char *rcs_copy, *buf, *token;

	/* Isolate the pieces. */
	buf = rcs_copy = NewString(rcs);
	while ((token = strtok(buf, "+")) != CN) {
		buf = CN;
		switch (n_rcs) {
		case 0:
#if defined(X3270_DBCS) /*[*/
		case 1:
#endif /*]*/
		    break;
		default:
		    popup_an_error("Extra %s value(s), ignoring",
			ResDisplayCharset);
		    break;
		}
		n_rcs++;
	}

#if defined(X3270_DBCS) /*[*/
	/* Can't swap DBCS modes while connected. */
	if (IN_3270 && (n_rcs == 2) != dbcs) {
		popup_an_error("Can't change DBCS modes while connected");
		return CS_ILLEGAL;
	}
#endif /*]*/

#if defined(X3270_DISPLAY) || (defined(C3270) && !defined(_WIN32)) /*[*/
	if (!screen_new_display_charsets(
		    rcs? rcs: default_display_charset,
		    csname)) {
		return CS_PREREQ;
	}
#else /*][*/
#if !defined(_WIN32) /*[*/
	utf8_set_display_charsets(rcs? rcs: default_display_charset, csname);
#endif /*]*/
#if defined(X3270_DBCS) /*[*/
	if (n_rcs > 1)
		dbcs = True;
	else
		dbcs = False;
#endif /*]*/
#endif /*]*/

	/* Set up the cgcsgid. */
	set_cgcsgids(codepage);

	/* Set up the character set name. */
	set_charset_name(csname);

	return CS_OKAY;
}

/* Return the current character set name. */
char *
get_charset_name(void)
{
	return (charset_name != CN)? charset_name:
	    ((appres.charset != CN)? appres.charset: "us");
}

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
 * Note that 'ebc' is an unsigned short, not an unsigned char.  This is
 * so that DBCS values can be passed in as 16 bits (with the first byte
 * in the high-order bits).  There is no ambiguity because all valid EBCDIC
 * DBCS characters have a nonzero first byte.
 *
 * Returns 0 if 'blank_undef' is clear and there is no printable EBCDIC
 * translation for 'ebc'.
 *
 * Returns '?' in mb[] if there is no local multi-byte representation of
 * the EBCDIC character.
 *
 * XXX: For Tcl3270, this should always be a simple UTF-8 conversion.
 */
int
ebcdic_to_multibyte(unsigned short ebc, unsigned char cs, char mb[],
	int mb_len, int blank_undef, trans_t purpose, unsigned long *ucp)
{
    int xuc;
    unsigned long uc;

#if defined(_WIN32) /*[*/
    int nc;
    BOOL udc;
    wchar_t wuc;
#elif defined(UNICODE_WCHAR) /*][*/
    int nc;
    wchar_t wuc;
#else /*][*/
    char u8b[7];
    char *inbuf, *outbuf;
    size_t inbytesleft, outbytesleft;
    int nu8;
    size_t nc;
#endif /*]*/

    *ucp = 0;

    /* Control characters become blanks. */
    if (ebc <= 0x41 || ebc == 0xff) {
	mb[0] = ' ';
	mb[1] = '\0';
	return 2;
    }

    /* Do the initial translation from EBCDIC to Unicode. */
    if ((cs & CS_GE) || ((cs & CS_MASK) == CS_APL)) {
	xuc = apl_to_unicode(ebc);
	if (xuc == -1)
	    uc = 0;
	else {
	    uc = xuc;
	    *ucp = xuc;
	}
    } else if (cs == CS_LINEDRAW) {
	xuc = linedraw_to_unicode(ebc);
	if (xuc == -1)
	    uc = 0;
	else {
	    uc = xuc;
	    *ucp = xuc;
	}
    } else if (cs != CS_BASE) {
	uc = 0;
    } else {
	uc = ebcdic_to_unicode(ebc, blank_undef, (purpose == TRANS_DISPLAY));
	*ucp = uc;
    }
    if (uc == 0)
	return 0;

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
    /* wchar_t's are Unicode, so use wctomb(). */

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
		unsigned long uc;

		xlen = ebcdic_to_multibyte(*ebc, CS_BASE, mb, mb_len, True,
			TRANS_LOCAL, &uc);
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
unsigned long
multibyte_to_unicode(const char *mb, size_t mb_len, int *consumedp,
	enum me_fail *errorp)
{
    size_t nw;
    unsigned long ucs4;
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
 * Returns the number of UCS-4 characters stored.
 */
int
multibyte_to_unicode_string(char *mb, size_t mb_len, unsigned long *ucs4,
	size_t u_len)
{
    int consumed;
    enum me_fail error;
    int nr = 0;

    error = ME_NONE;

    while (u_len && (*ucs4++ = multibyte_to_unicode(mb, mb_len, &consumed,
		    &error)) != 0) {
	u_len--;
	mb += consumed;
	mb_len -= consumed;
	nr++;
    }
    if (u_len) {
	*ucs4 = 0;
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
unsigned short
multibyte_to_ebcdic(const char *mb, size_t mb_len, int *consumedp,
	enum me_fail *errorp)
{
    unsigned long ucs4;

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
	unsigned short e;
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
unicode_to_multibyte(unsigned long ucs4, char *mb, size_t mb_len)
{
#if defined(_WIN32) /*[*/
    wchar_t wuc = ucs4;
    BOOL udc;
    int nc;

    nc = WideCharToMultiByte(CP_ACP, 0, &wuc, 1, mb, mb_len, "?", &udc);
    return nc;
#elif defined(UNICODE_WCHAR) /*][*/
    int nc;

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
