/*
 * Copyright (c) 2014-2015 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC
 *       nor their contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
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
 *	b8.c
 *		256-bit bitmap manipulation functions.
 */

#include "globals.h"

#include <stdint.h>

#include "b8.h"

/* Zero a bitmap. */
void
b8_zero(b8_t *b)
{
    int i;

    for (i = 0; i < NU8; i++) {
	b->u[i] = 0;
    }
}

/* 1's complement a bitmap. */
void
b8_not(b8_t *b)
{
    int i;

    for (i = 0; i < NU8; i++) {
	b->u[i] = ~b->u[i];
    }
}

/* AND two objects. */
void
b8_and(b8_t *r, b8_t *a, b8_t *b)
{
    int i;

    for (i = 0; i < NU8; i++) {
	r->u[i] = a->u[i] & b->u[i];
    }
}

/* Set a bit in a bitmap. */
void
b8_set_bit(b8_t *b, unsigned bit)
{
    if (bit < MX8) {
	b->u[bit / NB8] |= (uint64_t)1 << (bit % NB8);
    }
}

/* Test a bit in a bitmap. */
bool
b8_bit_is_set(b8_t *b, unsigned bit)
{
    if (bit < MX8) {
	return (b->u[bit / NB8] & ((uint64_t)1 << (bit % NB8))) != 0;
    } else {
	return false;
    }
}

/* Test a bitmap for all zeroes. */
bool
b8_is_zero(b8_t *b)
{
    int i;

    for (i = 0; i < NU8; i++) {
	if (b->u[i]) {
	    return false;
	}
    }
    return true;
}

/* Copy one bitmap to another. */
void
b8_copy(b8_t *to, b8_t *from)
{
    *to = *from; /* struct copy */
}

/* Check for bits added to a bitmap. */
bool
b8_none_added(b8_t *want, b8_t *got)
{
    b8_t t;

    /*
     * The basic arithmetic is:
     *  !(got & ~want)
     */
    b8_copy(&t, want);
    b8_not(&t);
    b8_and(&t, got, &t);
    return b8_is_zero(&t);
}
