/*
 * Copyright (c) 2018 Paul Mattes.
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
 *      boolstr.c
 *              Boolean value parsing.
 */

#include "globals.h"

#include "boolstr.h"

/**
 * Parse a boolean string.
 *
 * @param[in] text	String to parse.
 * @param[in] result	Parsed value.
 *
 * @return NULL if parsed, error string if not
 */
const char *
boolstr(const char *text, bool *result)
{
    if (!strcasecmp(text, "true")
	    || !strcasecmp(text, "t")
	    || !strcasecmp(text, "set")
	    || !strcasecmp(text, "on")
	    || !strcasecmp(text, "1")) {
	*result = true;
	return NULL;
    }

    if (!strcasecmp(text, "false")
	    || !strcasecmp(text, "f")
	    || !strcasecmp(text, "clear")
	    || !strcasecmp(text, "off")
	    || !strcasecmp(text, "0")) {
	*result = false;
	return NULL;
    }

    return "value must be true or false";
}
