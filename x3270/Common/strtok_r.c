/*
 * Copyright (c) 2002-2009, Paul Mattes.
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
 *	strtok_r.c
 *		A standard C library function that isn't available everywhere.
 */

#include <string.h>

/*
 * Isolate sequential tokens in a null-terminated string, str.  These tokens
 * are separated in the string by at least one of the characters in sep.  The
 * first time that strtok() is called, str should be specified; subsequent
 * calls, wishing to obtain further tokens from the same string, should pass
 * a null pointer instead.  The separator string, sep, must be supplied each
 * time, and may change between calls.
 *
 * strtok_r() is reentrant.  The context pointer last must be provided on
 * each call.  strtok_r() may also be used to nest two parsing loops within
 * one another, as long as separate context pointers are used.
 */

char *
strtok_r(char *str, const char *sep, char **last)
{
	char *r, *e;

	if (str != NULL)
		*last = str;
	r = *last + strspn(*last, sep);
	e = r + strcspn(r, sep);
	if (*e)
		*e++ = '\0';
	*last = e;
	return *r? r: NULL;
}
