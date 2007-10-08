/*
 * Copyright 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
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
