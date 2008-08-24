/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001, 2002,
 *  2004, 2005, 2008 by Paul Mattes.
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
 *	charsetc.h
 *		Global declarations for charset.c
 */

extern Boolean charset_changed;
extern unsigned long cgcsgid;
#if defined(X3270_DBCS) /*[*/
extern unsigned long cgcsgid_dbcs;
#endif /*]*/
extern char *default_display_charset;
enum cs_result { CS_OKAY, CS_NOTFOUND, CS_BAD, CS_PREREQ, CS_ILLEGAL };
extern enum cs_result charset_init(char *csname);
extern char *get_charset_name(void);

typedef enum {
    TRANS_DISPLAY,	/* generating display output,
			   interpreting keyboard input */
    TRANS_LOCAL		/* generating logfiles or interactive output,
			   interpreting configuration information */
} trans_t;

extern int ebcdic_to_multibyte(ebc_t ebc, unsigned char cs,
	char mb[], int mb_len, int blank_undef, trans_t purpose,
	ucs4_t *uc);
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
