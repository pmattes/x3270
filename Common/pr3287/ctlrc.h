/*
 * Copyright 1995, 1999, 2000, 2002, 2004 by Paul Mattes.
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
 *	ctlrc.h
 *		Global declarations for ctlr.c.
 */

enum pds {
	PDS_OKAY_NO_OUTPUT = 0,	/* command accepted, produced no output */
	PDS_OKAY_OUTPUT = 1,	/* command accepted, produced output */
	PDS_BAD_CMD = -1,	/* command rejected */
	PDS_BAD_ADDR = -2,	/* command contained a bad address */
	PDS_FAILED = -3		/* command failed */
};

extern void ctlr_add(unsigned char c, unsigned char cs, unsigned char gr);
extern void ctlr_write(unsigned char buf[], int buflen, Boolean erase);
extern int print_eoj(void);
extern void print_unbind(void);
extern enum pds process_ds(unsigned char *buf, int buflen);
extern enum pds process_scs(unsigned char *buf, int buflen);
