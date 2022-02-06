/*
 * Copyright (c) 2007-2009, 2013, 2015, 2019 Paul Mattes.
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
 *	utf8.c
 *		3270 Terminal Emulator
 *		UTF-8 conversions
 */

#include "globals.h"

#include "utf8.h"

char *locale_codeset = NULL;
bool is_utf8 = false;

#if defined(_WIN32) /*[*/
# define _STR_HELPER(s) #s
# define CPNAME(x) "CP" _STR_HELPER(x)
#endif /*]*/

/*
 * Save the codeset from the locale, and set globals based on known values.
 */
void
set_codeset(char *codeset_name, bool force_utf8)
{
    /*
     * s3270 and ws3270 have a '-utf8' option and a utf8 resource to force
     * UTF-8 mode. tcl3270 always forces UTF-8 mode, because that's what the
     * TCL library uses.
     */
    if (force_utf8) {
	is_utf8 = true;

	/* Force the name. */
	codeset_name = "UTF-8";
    }

    /*
     * We're in UTF-8 mode if the codeset looks like 'UTF8', or on Windows,
     * if it is codepage 65001.
     */
    is_utf8 |= (!strcasecmp(codeset_name, "utf-8") ||
		!strcasecmp(codeset_name, "utf8") ||
		!strcasecmp(codeset_name, "utf_8")
#if defined(_WIN32) /*[*/
		|| !strcasecmp(codeset_name, CPNAME(CP_UTF8))
#endif /*]*/
		);

#if defined(_WIN32) /*[*/
    if (is_utf8) {
	/* Force the name, in case they specified it numerically. */
	codeset_name = "UTF-8";
    }
#endif /*]*/

    Replace(locale_codeset, NewString(codeset_name));
}

/*
 * Return the local codeset.
 */
const char *
get_codeset(void)
{
    return locale_codeset;
}

/*
 * Convert from UCS-4 to UTF-8.
 * Returns:
 *    >0: length of converted character
 *    -1: invalid UCS-4
 */
int
unicode_to_utf8(ucs4_t ucs4, char *utf8)
{
    if (ucs4 & 0x80000000) {
	return -1;
    }

    if (ucs4 <= 0x0000007f) {
	utf8[0] = ucs4 & 0x7f;				/*  7 bits */
	return 1;
    } else if (ucs4 <= 0x000007ff) {
	utf8[0] = 0xc0 | ((ucs4 >> 6)  & 0x1f);		/* upper 5 bits */
	utf8[1] = 0x80 | (ucs4         & 0x3f);		/* lower 6 bits */
	return 2;
    } else if (ucs4 <= 0x0000ffff) {
	utf8[0] = 0xe0 | ((ucs4 >> 12) & 0x0f);		/* upper 4 bits */
	utf8[1] = 0x80 | ((ucs4 >> 6)  & 0x3f);		/* next 6 bits */
	utf8[2] = 0x80 | (ucs4 &         0x3f);		/* last 6 bits */
	return 3;
    } else if (ucs4 <= 0x001fffff) {
	utf8[0] = 0xf0 | ((ucs4 >> 18) & 0x07);		/* upper 3 bits */
	utf8[1] = 0x80 | ((ucs4 >> 12) & 0x3f);		/* next 6 bits */
	utf8[2] = 0x80 | ((ucs4 >> 6)  & 0x3f);		/* next 6 bits */
	utf8[3] = 0x80 | (ucs4         & 0x3f);		/* last 6 bits */
	return 4;
    } else if (ucs4 <= 0x03ffffff) {
	utf8[0] = 0xf8 | ((ucs4 >> 24) & 0x03);		/* upper 2 bits */
	utf8[1] = 0x80 | ((ucs4 >> 18) & 0x3f);		/* next 6 bits */
	utf8[2] = 0x80 | ((ucs4 >> 12) & 0x3f);		/* next 6 bits */
	utf8[3] = 0x80 | ((ucs4 >> 6)  & 0x3f);		/* next 6 bits */
	utf8[4] = 0x80 | (ucs4 &         0x3f);		/* last 6 bits */
	return 5;
    } else {
	utf8[0] = 0xfc | ((ucs4 >> 30) & 0x01);		/* upper 1 bit */
	utf8[1] = 0x80 | ((ucs4 >> 24) & 0x3f);		/* next 6 bits */
	utf8[2] = 0x80 | ((ucs4 >> 18) & 0x3f);		/* next 6 bits */
	utf8[3] = 0x80 | ((ucs4 >> 12) & 0x3f);		/* next 6 bits */
	utf8[4] = 0x80 | ((ucs4 >> 6)  & 0x3f);		/* next 6 bits */
	utf8[5] = 0x80 | (ucs4         & 0x3f);		/* last 6 bits */
	return 6;
    }
}

/*
 * Convert at most 'len' bytes from a UTF-8 string to one UCS-4 character.
 * Returns:
 *    >0: Number of characters consumed.
 *     0: Incomplete sequence.
 *    -1: Invalid sequence.
 *    -2: Illegal (too-long) encoding.
 *    -3: Invalid lead byte.
 *
 * An invalid sequence can be either improperly composed, or using the wrong
 * encoding length (often used to get past spam filters and such).
 */
int
utf8_to_unicode(const char *utf8, size_t len, ucs4_t *ucs4)
{
    /* No input is by definition incomplete. */
    if (!len) {
	return 0;
    }

    /* See if it's ASCII-7. */
    if ((utf8[0] & 0xff) < 0x80) {
	*ucs4 = utf8[0] & 0x7f;
	return 1;
    }

    /* Now check for specific UTF-8 leading bytes. */
    if ((utf8[0] & 0xe0) == 0xc0) {
	/* 110xxxxx 10xxxxxx
	 * 0x00000080-0x000007ff */
	if (len < 2) {
	    return 0;
	}
	if ((utf8[1] & 0xc0) != 0x80) {
	    return -1;
	}
	*ucs4 = ((utf8[0] << 6) & 0x7c0) |
	    	 (utf8[1] &       0x03f);
	return 2;
    }

    if ((utf8[0] & 0xf0) == 0xe0) {
	/* 1110xxxx 10xxxxxx 10xxxxxx
	 * 0x00000800-0x0000ffff */
	if (len < 3) {
	    return 0;
	}
	if (((utf8[1] & 0xc0) != 0x80) ||
	    ((utf8[2] & 0xc0) != 0x80)) {
	    return -1;
	}
	*ucs4 = ((utf8[0] << 12) & 0xf000) |
	        ((utf8[1] << 6)  & 0x0fc0) |
		((utf8[2])       & 0x003f);
	if (*ucs4 < 0x00000800) {
	    return -2;
	}
	return 3;
    }

    if ((utf8[0] & 0xf8) == 0xf0) {
	/* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
	 * 0x00010000-0x001fffff */
	if (len < 4) {
	    return 0;
	}
	if (((utf8[1] & 0xc0) != 0x80) ||
	    ((utf8[2] & 0xc0) != 0x80) ||
	    ((utf8[3] & 0xc0) != 0x80)) {
	    return -1;
	}
	*ucs4 = ((utf8[0] << 18) & 0x1c0000) |
		((utf8[1] << 12) & 0x03f000) |
	        ((utf8[2] << 6)  & 0x000fc0) |
		((utf8[3])       & 0x00003f);
	if (*ucs4 < 0x00010000) {
	    return -2;
	}
	return 4;
    }

    if ((utf8[0] & 0xfc) == 0xf8) {
	/* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
	 * 0x00200000-0x03ffffff */
	if (len < 5) {
	    return 0;
	}
	if (((utf8[1] & 0xc0) != 0x80) ||
	    ((utf8[2] & 0xc0) != 0x80) ||
	    ((utf8[3] & 0xc0) != 0x80) ||
	    ((utf8[4] & 0xc0) != 0x80)) {
	    return -1;
	}
	*ucs4 = ((utf8[0] << 24) & 0x3000000) |
		((utf8[1] << 18) & 0x0fc0000) |
		((utf8[2] << 12) & 0x003f000) |
	        ((utf8[3] << 6)  & 0x0000fc0) |
		((utf8[4])       & 0x000003f);
	if (*ucs4 < 0x00200000) {
	    return -2;
	}
	return 5;
    }

    if ((utf8[0] & 0xfe) == 0xfc) {
	/* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
	 * 0x04000000-0x7fffffff */
	if (len < 6) {
	    return 0;
	}
	if (((utf8[1] & 0xc0) != 0x80) ||
	    ((utf8[2] & 0xc0) != 0x80) ||
	    ((utf8[3] & 0xc0) != 0x80) ||
	    ((utf8[4] & 0xc0) != 0x80) ||
	    ((utf8[5] & 0xc0) != 0x80)) {
	    return -1;
	}
	*ucs4 = ((utf8[0] << 30) & 0x40000000) |
		((utf8[1] << 24) & 0x3f000000) |
		((utf8[2] << 18) & 0x00fc0000) |
		((utf8[3] << 12) & 0x0003f000) |
	        ((utf8[4] << 6)  & 0x00000fc0) |
		((utf8[5])       & 0x0000003f);
	if (*ucs4 < 0x04000000) {
	    return -2;
	}
	return 6;
    }

    return -3;
}
