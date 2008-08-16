/*
 * Copyright 2002, 2003, 2004, 2005, 2008 by Paul Mattes.
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

/*
 *	wide.c
 *		A 3270 Terminal Emulator for X11
 *		Wide character translation functions.
 */

#include "globals.h"
#include <errno.h>
#include <locale.h>
#include <langinfo.h>

#include "3270ds.h"
#if !defined(PR3287) /*[*/
#include "appres.h"
#endif /*]*/

#include "charsetc.h"
#include "popupsc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "unicodec.h"
#if !defined(PR3287) /*[*/
#include "utilc.h"
#endif /*]*/

#include "widec.h"

#define ICU_DATA	"ICU_DATA"

static UConverter *dbcs_converter = NULL;
static UConverter *sbcs_converter = NULL;
static UConverter *local_converter = NULL;
static Boolean same_converter = False;

static void
xlate1(unsigned char from0, unsigned char from1, unsigned char to_buf[],
    UConverter *from_cnv, const char *from_name,
    UConverter *to_cnv, const char *to_name)
{
	UErrorCode err = U_ZERO_ERROR;
	UChar Ubuf[2];
	char from_buf[4];
	int from_len;
	char tmp_to_buf[3];
	int32_t len;
#if defined(WIDE_DEBUG) /*[*/
	int i;
#endif /*]*/

	/* Do something reasonable in case of failure. */
	to_buf[0] = to_buf[1] = 0;

	/* Convert string from source to Unicode. */
	if (same_converter) {
		from_buf[0] = EBC_so;
		from_buf[1] = from0;
		from_buf[2] = from1;
		from_buf[3] = EBC_si;
		from_len = 4;
	} else {
		from_buf[0] = from0;
		from_buf[1] = from1;
		from_len = 2;
	}
	len = ucnv_toUChars(from_cnv, Ubuf, 2, from_buf, from_len, &err);
	if (err != U_ZERO_ERROR) {
		trace_ds("[%s toUnicode of DBCS X'%02x%02x' failed, ICU "
		    "error %d]\n", from_name, from0, from1, (int)err);
		return;
	}
	if (Ubuf[0] == 0xfffd) {
		/* No translation. */
		trace_ds("[%s toUnicode of DBCS X'%02x%02x' failed]\n",
		    from_name, from0, from1);
		return;
	}
#if defined(WIDE_DEBUG) /*[*/
	printf("Got Unicode %x\n", Ubuf[0]);
#endif /*]*/

	if (to_cnv != NULL) {
		/* Convert string from Unicode to Destination. */
		len = ucnv_fromUChars(to_cnv, tmp_to_buf, 3, Ubuf, len, &err);
		if (err != U_ZERO_ERROR) {
			trace_ds("[fromUnicode of U+%04x to %s failed, ICU "
			    "error %d]\n", Ubuf[0], to_name, (int)err);
			return;
		}
		to_buf[0] = tmp_to_buf[0];
		to_buf[1] = tmp_to_buf[1];
#if defined(WIDE_DEBUG) /*[*/
		printf("Got %u %s characters:", len, to_name);
		for (i = 0; i < len; i++) {
			printf(" %02x", to_buf[i]);
		}
		printf("\n");
#endif /*]*/
	} else {
		to_buf[0] = (Ubuf[0] >> 8) & 0xff;
		to_buf[1] = Ubuf[0] & 0xff;
	}
}

/* Translate a DBCS EBCDIC character to a 2-byte Unicode character. */
void
dbcs_to_unicode16(unsigned char ebc1, unsigned char ebc2, unsigned char c[])
{
	xlate1(ebc1, ebc2, c, dbcs_converter, "host DBCS", NULL, NULL);
}

/*
 * Translate a DBCS EBCDIC character to a local multi-byte character.
 * Returns -1 for error, or the mb length.  NULL terminates.
 */
int
dbcs_to_mb(unsigned char ebc1, unsigned char ebc2, char *mb)
{
	UErrorCode err = U_ZERO_ERROR;
	unsigned char w[2];
	UChar Ubuf;
	int len;

	if (local_converter == NULL) {
		*mb = '?';
		*(mb + 1) = '\0';
		return 1;
	}

	/* Translate to Unicode first. */
	dbcs_to_unicode16(ebc1, ebc2, w);
	Ubuf = (w[0] << 8) | w[1];

	/* Then translate to the local encoding. */
	len = ucnv_fromUChars(local_converter, mb, 16, &Ubuf, 1, &err);
	if (err != U_ZERO_ERROR) {
		trace_ds("[fromUnicode of U+%04x to local failed, ICU "
		    "error %d]\n", Ubuf, (int)err);
		return -1;
	}
	return len;
}

/*
 * Translate an SBCS EBCDIC character to a local multi-byte character.
 * Returns -1 for error, or the mb length.  NULL terminates.
 */
int
sbcs_to_mb(unsigned char ebc, char *mb)
{
	UErrorCode err = U_ZERO_ERROR;
	UChar Ubuf;
	int len;

	if (sbcs_converter == NULL) {
	    	unsigned long uc;

		/* No SBCS converter, do EBCDIC to latin-1. */
		if (local_converter == NULL) {
			/* No local converter either, latin-1 is it. */
		    	len = ebcdic_to_multibyte(ebc, CS_BASE, mb, 16,
				True, TRANS_LOCAL, &uc);
			if (len > 0)
			    	len--;
			return len;
		}

		/* Have a local converter; use it below. */
		Ubuf = ebcdic_to_unicode(ebc, True, False);
	} else {
		/* Have an SBCS converter.  Convert from SBCS to Unicode. */
		err = U_ZERO_ERROR;
		len = ucnv_toUChars(sbcs_converter, &Ubuf, 1, (char *)&ebc, 1,
				&err);
		if (err != U_ZERO_ERROR &&
				err != U_STRING_NOT_TERMINATED_WARNING) {
			trace_ds("[toUChars failed, ICU error %d]\n",
			    (int)err);
			return -1;
		}
	}

	/* Convert from Unicode to the local encoding. */
	len = ucnv_fromUChars(local_converter, mb, 16, &Ubuf, 1, &err);
	if (err != U_ZERO_ERROR) {
		trace_ds("[fromUnicode of U+%04x to local failed, ICU "
		    "error %d]\n", Ubuf, (int)err);
		return -1;
	}
	return len;
}

/*
 * Translate a local multi-byte string to Unicode characters.
 * Returns -1 for error, or the length.  NULL terminates.
 */
int
mb_to_unicode(char *mb, int mblen, UChar *u, int ulen, UErrorCode *err)
{
	UErrorCode local_err;
	int len;
	Boolean print_errs = False;

	if (local_converter == NULL) {
		int i;

		for (i = 0; i < mblen; i++) {
			u[i] = mb[i] & 0xff;
		}
		return mblen;
	}
	if (err == NULL) {
		err = &local_err;
		print_errs = True;
	}
	*err = U_ZERO_ERROR;
	len = ucnv_toUChars(local_converter, u, ulen, mb, mblen, err);
	if (*err != U_ZERO_ERROR && *err != U_STRING_NOT_TERMINATED_WARNING) {
		if (print_errs)
			trace_ds("[toUChars failed, ICU error %d]\n",
			    (int)*err);
		return -1;
	}
	return len;
}

/*
 * Try to map a Unicode character to the Host SBCS character set.
 * Returns ASCII in cp[0].
 */
int
dbcs_map8(UChar u, unsigned char *cp)
{
	UErrorCode err = U_ZERO_ERROR;
	int len;

	if (!(u & ~0xff)) {
		*cp = u;
		return 1;
	}
	if (sbcs_converter != NULL) {
		len = ucnv_fromUChars(sbcs_converter, (char *)cp, 1, &u, 1,
		    &err);
		if ((err != U_ZERO_ERROR &&
		     err != U_STRING_NOT_TERMINATED_WARNING) ||
		    (*cp == '?' && u != '?')) {
		    	unsigned long uc;
			char mp[16];
			int len;

		    	len = ebcdic_to_multibyte(*cp, CS_BASE, mp, 16, True,
				TRANS_LOCAL, &uc);
			if (len <= 0)
			    	return 1;
			strcpy((char *)cp, mp); /* XXX: May not be big enough! */
			return 0;
		} else
			return 1;
	}
	return 0;
}

/*
 * Try to map a Unicode character to the Host DBCS character set.
 * Returns EBCDIC in cp[].
 */
int
dbcs_map16(UChar u, unsigned char *cp)
{
	UErrorCode err = U_ZERO_ERROR;
	int len;

	if (same_converter) {
		char tmp_cp[5];

		len = ucnv_fromUChars(dbcs_converter, tmp_cp, 5, &u, 1, &err);
		if (err != U_ZERO_ERROR ||
		    len < 3 ||
		    tmp_cp[0] != EBC_so)
			return 0;
		cp[0] = tmp_cp[1];
		cp[1] = tmp_cp[2];
		return 1;
	} else {
		len = ucnv_fromUChars(dbcs_converter, (char *)cp, 2, &u, 1,
				&err);
		return (err == U_ZERO_ERROR ||
			err == U_STRING_NOT_TERMINATED_WARNING);
	}
}
