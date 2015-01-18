/*
 * Copyright (c) 2014-2015 Paul Mattes.
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
 *      varbuf.c
 *              x3270 variable-length buffer library
 */

#include "globals.h"

#include "asprintf.h"

#include "varbuf.h"

#define RA_BASE  16	/* initial allocation size */

/**
 * Initialize a buffer.
 *
 * @param[in,out] r	Varbuf to initialize.
 */
void
vb_init(varbuf_t *r)
{
    memset(r, 0, sizeof(*r));
}

/**
 * Expand a buffer.
 */
static void
vb_expand(varbuf_t *r, size_t len)
{
    if (r->len + len > r->alloc_len) {
	if (r->alloc_len == 0) {
	    r->alloc_len = RA_BASE;
	}
	/* Yes, there are cleverer ways to find the nearest power of 2. */
	while (r->len + len > r->alloc_len) {
	    r->alloc_len *= 2;
	}
	r->buf = Realloc(r->buf, r->alloc_len);
    }
}

/**
 * Append a string to a buffer, restricted by a length.
 *
 * @param[in,out] r	Varbuf to modify
 * @param[in] buf	Buffer to append
 * @param[in] len	Length of buffer to append
 */
void
vb_append(varbuf_t *r, const char *buf, size_t len)
{
    /*
     * Allocate more space, if needed.
     * We allocate an extra byte for the NUL terminator here.
     */
    vb_expand(r, len + 1);

    /* Append the response. */
    memcpy(r->buf + r->len, buf, len);
    r->len += len;

    /* Add the NUL terminator. */
    r->buf[r->len] = '\0';
}

/**
 * Append a string to a buffer.
 *
 * @param[in,out] r	Varbuf to modify
 * @param[in] buf	NUL-terminated buffer to append
 */
void
vb_appends(varbuf_t *r, const char *buf)
{
    vb_append(r, buf, strlen(buf));
}

/**
 * Append a printf format to a buffer, varargs style.
 *
 * @param[in,out] r	Varbuf to modify
 * @param[in] format	Printf format
 * @param[in] ap	Varargs
 */
void
vb_vappendf(varbuf_t *r, const char *format, va_list ap)
{
    va_list ap_copy;
    int len;

    /* Figure out how much to add. */
    va_copy(ap_copy, ap);
    len = vscprintf(format, ap_copy);
    va_end(ap_copy);

    /* Expand. */
    vb_expand(r, len + 1);

    /* Expand the text. */
    vsnprintf(r->buf + r->len, len + 1, format, ap);
    r->len += len;
}

/**
 * Append a printf format to a buffer.
 *
 * @param[in,out] r	Varbuf to modify
 * @param[in] format	Printf format
 */
void
vb_appendf(varbuf_t *r, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vb_vappendf(r, format, ap);
    va_end(ap);
}

/**
 * Return the buffer.
 *
 * @param[in] r		Varbuf to query
 *
 * @return Buffer
 */
const char *
vb_buf(const varbuf_t *r)
{
    return r->buf;
}

/**
 * Return the buffer length.
 *
 * @param[in] r		Varbuf to query
 *
 * @return Buffer length
 */
size_t
vb_len(const varbuf_t *r)
{
    return r->len;
}

/**
 * Reset the length of a buffer.
 *
 * @param[in,out] r	Varbuf to reset
 */
void
vb_reset(varbuf_t *r)
{
    r->len = 0;
}

/**
 * Consume a buffer (free it and return the contents).
 *
 * This function is guaranteed never to return NULL, even if nothing was ever
 * added.
 *
 * @param[in,out] r	Varbuf to consume
 *
 * @return contents
 */
char *
vb_consume(varbuf_t *r)
{
    char *ret;

    ret = r->buf;
    vb_init(r);
    return ret? ret: NewString("");
}

/**
 * Free a buffer.
 *
 * @param[in,out] r	Varbuf to free
 */
void
vb_free(varbuf_t *r)
{
    Free(r->buf);
    vb_init(r);
}
