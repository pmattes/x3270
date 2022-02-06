/*
 * Copyright (c) 2021-2022 Paul Mattes.
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
 *      sa_malloc.c
 *              Standalone instrumented malloc for tests and standalone
 *              programs
 */

#include "globals.h"

#include <assert.h>

#include "asprintf.h"
#include "lazya.h"
#include "sa_malloc.h"

static size_t allocated;

/* Preamble saved before each malloc'd block. */
typedef struct {
    size_t signature;	/* signature for verification */
    size_t len;		/* size for Realloc */
} pre_t;
#define SS		sizeof(pre_t)
#define SIGNATURE	0x4a534f4e

/* Lazy allocator record. */
typedef struct lazya {
    struct lazya *next;
    void *buf;
} lazya_t;
static lazya_t *lazya_list = NULL;

/* Increment the allocated memory count. */
static void
inc_allocated(const char *why, size_t len)
{
    allocated += len;
}

/* Decrement the allocated memory count. */
static void
dec_allocated(const char *why, size_t len)
{
    allocated -= len;
}

void *
Malloc(size_t len)
{
    void *ret;
    pre_t *p;

    inc_allocated("Malloc", len);
    ret = malloc(SS + len);
    assert(ret != NULL);
    p = (pre_t *)ret;
    p->signature = SIGNATURE;
    p->len = len;
    return (void *)(p + 1);
}

void *
Realloc(void *buf, size_t len)
{
    pre_t *p;

    if (buf == NULL) {
	return Malloc(len);
    }

    p = (pre_t *)buf - 1;
    assert(p->signature == SIGNATURE);
    dec_allocated("Realloc", p->len);
    inc_allocated("Realloc", len);
    p = realloc(p, SS + len);
    assert(p->signature == SIGNATURE);
    p->len = len;
    return (void *)(p + 1);
}

void *
Calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ret;

    ret = Malloc(total);
    memset(ret, 0, total);
    return ret;
}

void
Free(void *buf)
{
    if (buf != NULL) {
	pre_t *p = (pre_t *)buf - 1;

	assert(p->signature == SIGNATURE);
	dec_allocated("Free", p->len);
	free(p);
    }
}

char *
NewString(const char *s)
{
    char *buf = Malloc(strlen(s) + 1);

    return strcpy(buf, s);
}

char *
xs_buffer(const char *fmt, ...)
{
    va_list ap;
    char *ret;
    char *copy;

    va_start(ap, fmt);
    vasprintf(&ret, fmt, ap);
    va_end(ap);

    copy = Malloc(strlen(ret) + 1);
    strcpy(copy, ret);
    free(ret);
    return copy;
}

char *
lazyaf(const char *fmt, ...)
{
    va_list ap;
    char *ret;
    char *copy;

    va_start(ap, fmt);
    vasprintf(&ret, fmt, ap);
    va_end(ap);

    copy = Malloc(strlen(ret) + 1);
    strcpy(copy, ret);
    free(ret);
    return lazya(copy);
}

char *
lazya(void *buf)
{
    lazya_t *l = (lazya_t *)Malloc(sizeof(lazya_t));
    l->next = lazya_list;
    l->buf = buf;
    return buf;
}

/* Free all of the lazya blocks. */
void
lazya_free(void)
{
    lazya_t *l;

    while ((l = lazya_list) != NULL) {
	lazya_t *next = l->next;

	Free(l->buf);
	Free(l);
	lazya_list = next;
    }
}

#if !defined(_WIN32) /*[*/
int
vscprintf(const char *fmt, va_list ap)
{
    return vsnprintf(NULL, 0, fmt, ap);
}
#endif /*]*/

/* Check for memory leaks. */
void
sa_malloc_leak_check(void)
{
    lazya_free();
    assert(allocated == 0);
}
