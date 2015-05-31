/*
 * Copyright (c) 1995-2009, 2013, 2015 Paul Mattes.
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
 *	ctlrc.h
 *		Global declarations for ctlr.c.
 */

enum pds {
    PDS_OKAY_NO_OUTPUT = 0,	/* command accepted, produced no output */
    PDS_OKAY_OUTPUT = 1,	/* command accepted, produced output */
    PDS_BAD_CMD = -1,		/* command rejected */
    PDS_BAD_ADDR = -2,		/* command contained a bad address */
    PDS_FAILED = -3		/* command failed */
};

void ctlr_add(unsigned char ebc, ucs4_t c, unsigned char cs, unsigned char gr);
void ctlr_write(unsigned char buf[], size_t buflen, bool erase);
int print_eoj(void);
void print_unbind(void);
enum pds process_ds(unsigned char *buf, size_t buflen);
enum pds process_scs(unsigned char *buf, size_t buflen);
