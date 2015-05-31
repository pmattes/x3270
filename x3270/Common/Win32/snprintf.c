/*
 * Copyright (c) 2013, 2015 Paul Mattes.
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

/*
 *	snprintf.c
 *		A safer version of snprintf for Windows.
 */

#if !defined(_WIN32) /*[*/
#error For Windows only.
#endif /*]*/

#define IS_SNPRINTF_C 1
#include "globals.h"

/*
 * Version of {,v}snprintf that work more like the standard versions, and
 * always NULL terminate. They do not, however, return the length that would
 * have been written if overflow did not occur -- they return -1, like the
 * Windows versions.
 */

int
safe_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    if (size > 0) {
	int len;

	len = vsnprintf(str, size, fmt, ap);
	str[size - 1] = '\0';
	return len;
    } else {
	return 0;
    }
}

int
safe_snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = safe_vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return len;
}
