/*
 * Copyright (c) 2015-2024 Paul Mattes.
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
 *      txa.c
 *              Transaction allocator.
 */

#include "globals.h"

#if defined(HAVE_MALLOC_H) /*[*/
# include <malloc.h>
#endif /*]*/

#include "trace.h"
#include "utils.h"

#include "txa.h"

#define BLOCK_SLOTS  1024	/* slots per block */

typedef struct txa_block {
    struct txa_block *next;
    void *slot[BLOCK_SLOTS];
} txa_block_t;
static txa_block_t *blocks;
static txa_block_t **last_block = &blocks;
static txa_block_t *current_block;
static int slot_ix = 0;

/**
 * Do a deferred free on a malloc'd block of memory.
 *
 * @param[in] buf	Buffer to store
 * 
 * @return buf, for convenience
 */
char *
txdFree(void *buf)
{
    if (current_block == NULL || slot_ix >= BLOCK_SLOTS) {
	/* Allocate a new block. */
	current_block = (txa_block_t *)Calloc(1, sizeof(txa_block_t));
	*last_block = current_block;
	last_block = &current_block->next;
	slot_ix = 0;
    }

    /* Remember this element. */
    current_block->slot[slot_ix++] = buf;
    return buf;
}

/**
 * Format a string into malloc'd memory and do a deferred free on it.
 *
 * @param[in] fmt	Format
 *
 * @return Buffer
 */
char *
txAsprintf(const char *fmt, ...)
{
    va_list args;
    char *r;

    va_start(args, fmt);
    r = Vasprintf(fmt, args);
    va_end(args);
    return txdFree(r);
}

/**
 * Format a string into malloc'd memory and do a deferred free on it.
 * Varargs version.
 *
 * @param[in] fmt	Format
 * @param[in] args	Arguments
 *
 * @return Buffer
 */
char *
txVasprintf(const char *fmt, va_list args)
{
    return txdFree(Vasprintf(fmt, args));
}

/**
 * Perform the deferred free operations at the end of a transaction.
 */
void
txflush(void)
{
    unsigned nf = 0;
#if defined(HAVE_MALLOC_USABLE_SIZE) /*[*/
    size_t nb = 0;
#endif /*]*/
    txa_block_t *r, *next = NULL;

    for (r = blocks; r != NULL; r = next) {
	int i;

	next = r->next;
	for (i = 0; i < BLOCK_SLOTS; i++) {
	    if (r->slot[i] != NULL) {
#if defined(HAVE_MALLOC_USABLE_SIZE) /*[*/
		nb += malloc_usable_size(r->slot[i]);
#endif /*]*/
		Free(r->slot[i]);
		nf++;
	    }
	}
	Free(r);
    }

    current_block = NULL;
    blocks = NULL;
    last_block = &blocks;
    slot_ix = 0;

#if defined(HAVE_MALLOC_USABLE_SIZE) /*[*/
    if (nf > 10 || nb > 1024) {
	vtrace("txflush: %u slot%s, %zu bytes\n", nf, (nf == 1)? "": "s",
		nb);
    }
#else /*][*/
    if (nf > 10) {
	vtrace("txflush: %u slot%s\n", nf, (nf == 1)? "": "s");
    }
#endif /*]*/
}
