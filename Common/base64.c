/*
 * Copyright (c) 2018, 2021 Paul Mattes.
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
 *	base64.c
 *		Base64 encoding.
 */

#include "globals.h"

#include "base64.h"

#define BITS_PER_BYTE	8
#define BITS_PER_BASE64	6
#define MASK64		0x3f
#define MASK256		0xff
#define PAD_BITS	2
#define MAX_PAD		2
#define BYTES_PER_BLOCK	3

/* The output alphabet. */
static char *alphabet64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * Encode a string in base64.
 *
 * Returns a malloc'd buffer.
 */
char *
base64_encode(const char *s)
{
    /*
     * We need one output character for every 6 bits of input, plus up to two
     * padding characters, plus a terminaing NUL.
     */
    size_t nmalloc = (((strlen(s) * BITS_PER_BYTE) + (BITS_PER_BASE64 - 1)) / BITS_PER_BASE64) + MAX_PAD + 1;
    char *ret = Malloc(nmalloc);
    char *op = ret;
    char c;
    unsigned accum = 0;	/* overflow bits */
    int held_bits = 0;	/* number of significant bits in 'accum' */
    bool done = false;

    while (!done) {
	int i;
	unsigned over;
	unsigned pad_bits = 0;

	/* Get the next 3 octets. */
	for (i = 0; i < BYTES_PER_BLOCK; i++) {
	    if (!(c = *s++)) {
		done = true;
		break;
	    }
	    accum = (accum << BITS_PER_BYTE) | (unsigned char)c;
	    held_bits += BITS_PER_BYTE;
	}

	/* Pad to an even multiple of 6. */
	over = held_bits % BITS_PER_BASE64;
	if (over != 0) {
	    pad_bits = BITS_PER_BASE64 - over;
	}
	accum <<= pad_bits;
	held_bits += pad_bits;

	/* Emit the base64. */
	while (held_bits > 0) {
	    *op++ = alphabet64[(accum >> BITS_PER_BASE64 * ((held_bits / BITS_PER_BASE64) - 1))
		    & MASK64];
	    held_bits -= BITS_PER_BASE64;
	}

	/* Emit padding indicators. */
	while (pad_bits >= PAD_BITS) {
	    *op++ = '=';
	    pad_bits -= PAD_BITS;
	}

	/* Go around again. */
	accum = 0;
	held_bits = 0;
    }

    *op = '\0';
    return ret;
}

/*
 * Decode a base64 string.
 *
 * Returns a malloc'd buffer.
 */
char *
base64_decode(const char *s)
{
    /* Do a little overkill on the decode buffer. */
    char *ret = Malloc(strlen(s) + 1);
    char *op = ret;
    char c;
    unsigned accum = 0;
    unsigned accum_bits = 0;
    int eq = 0;

    while ((c = *s++)) {
	if (c == '=') {
	    /* Each '=' is 2 bits of padding. */
	    eq++;
	    accum <<= PAD_BITS;
	    accum_bits += PAD_BITS;
	} else {
	    char *ix = strchr(alphabet64, c);

	    if (eq > 0 || ix == NULL) {
		/*
		 * Nothing can follow '=', and anything else needs to be in the
		 * alphabet.
		 */
		Free(ret);
		return NULL;
	    }
	    accum = (accum << BITS_PER_BASE64) | (int)(ix - alphabet64);
	    accum_bits += BITS_PER_BASE64;
	}

	while (accum_bits >= BITS_PER_BYTE) {
	    *op++ = (accum >> (accum_bits % BITS_PER_BYTE)) & MASK256;
	    accum &= ~(MASK256 << (accum_bits % BITS_PER_BYTE));
	    accum_bits -= BITS_PER_BYTE;
	}

	if (eq > MAX_PAD) {
	    /* No more than 2 '='. */
	    Free(ret);
	    return NULL;
	}
    }

    *op = '\0';
    return ret;
}
