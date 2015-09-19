/*
 * Copyright (c) 2015 Paul Mattes.
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
 *      lazya.c
 *              Lazy allocations
 */

#include "globals.h"

#include "trace.h"
#include "utils.h"

#include "lazya.h"

#define LAZY_RING  64	/* ring buffer size */

static char *lazy_ring[LAZY_RING];
static int lazy_ix = 0;

/**
 * Add a buffer to the lazy allocation ring.
 *
 * @param[in] buf	Buffer to store
 * 
 * @return buf, for convenience
 */
char *
lazya(char *buf)
{
    /*
     * Free whatever element we would overwrite, and store this buffer there.
     */
    Replace(lazy_ring[lazy_ix], buf);

    /* Advance to the next slot. */
    lazy_ix = (lazy_ix + 1) % LAZY_RING;

    return buf;
}

/**
 * Format a string into Malloc'd memory and put it into the lazy ring.
 *
 * @param[in] fmt	Format
 *
 * @return Buffer
 */
char *
lazyaf(const char *fmt, ...)
{
    va_list args;
    char *r;

    va_start(args, fmt);
    r = xs_vbuffer(fmt, args);
    va_end(args);
    return lazya(r);
}

/**
 * Format a string into Malloc'd memory and put it into the lazy ring.
 * Varargs version.
 *
 * @param[in] fmt	Format
 *
 * @return Buffer
 */
char *
vlazyaf(const char *fmt, va_list args)
{
    return lazya(xs_vbuffer(fmt, args));
}

/**
 * Flush the lazy allocation ring.
 */
void
lazya_flush(void)
{
    int i;
    int nf = 0;

    for (i = 0; i < LAZY_RING; i++) {
	if (lazy_ring[i]) {
	    nf++;
	}
	Replace(lazy_ring[i], NULL);
    }
    lazy_ix = 0;

    if (nf) {
	vtrace("lazya_flush: flushed %d elements\n", nf);
    }
}
