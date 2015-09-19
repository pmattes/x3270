/*
 * Copyright (c) 1999-2009, 2013-2015 Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Memory allocation functions. */

#include "globals.h"

void *
Malloc(size_t len)
{
    char *r;

    r = malloc(len);
    if (r == NULL) {
	Error("Out of memory");
    }
    return r;
}

void *
Calloc(size_t nelem, size_t elsize)
{
    char *r;

    r = malloc(nelem * elsize);
    if (r == NULL) {
	Error("Out of memory");
    }
    return memset(r, '\0', nelem * elsize);
}

void *
Realloc(void *p, size_t len)
{
    p = realloc(p, len);
    if (p == NULL) {
	Error("Out of memory");
    }
    return p;
}

void
Free(void *p)
{
    if (p != NULL) {
	free(p);
    }
}

char *
NewString(const char *s)
{
    if (s != NULL) {
	return strcpy(Malloc(strlen(s) + 1), s);
    } else {
	return NULL;
    }
}
