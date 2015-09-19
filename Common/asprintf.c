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
 *      asprintf.c
 *              Implementations of vcsprintf, asprintf and vasprintf for
 *              platforms that are missing them.
 */

#include "globals.h"

#include "asprintf.h"

#if !defined(_WIN32) /*[*/
/**
 * Linux implementation of the Windows vcsprintf() function.
 * Returns the number of bytes needed to represent a formatted string.
 *
 * @param[in] fmt	printf format
 * @param[in] ap	arguments
 *
 * @return number of bytes needed
 */
int
vscprintf(const char *fmt, va_list ap)
{
    return vsnprintf(NULL, 0, fmt, ap);
}
#endif /*]*/

#if !defined(HAVE_VASPRINTF) /*[*/
/**
 * vasprintf: print a string into an automatically malloc'd buffer, varargs
 * version
 *
 * @param[out] bufp	returned buffer
 * @param[in] fmt	printf format
 * @param[in] ap	arguments
 *
 * @return length, not including NUL
 */
int
my_vasprintf(char **bufp, const char *fmt, va_list ap)
{
    va_list ap_copy;
    int buflen;
    char *buf;

    va_copy(ap_copy, ap);
    buflen = vscprintf(fmt, ap_copy);
    va_end(ap_copy);
    buf = malloc(buflen + 1);
    vsnprintf(buf, buflen + 1, fmt, ap);
    *bufp = buf;
    return buflen;
}

/**
 * asprintf: print a string into an automatically malloc'd buffer
 *
 * @param[out] bufp	returned buffer
 * @param[in] fmt	printf format
 *
 * @return length, not including NUL
 */
int
my_asprintf(char **bufp, const char *fmt, ...)
{
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = my_vasprintf(bufp, fmt, ap);
    va_end(ap);
    return len;
}
#endif /*]*/
