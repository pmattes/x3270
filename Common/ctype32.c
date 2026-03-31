/*
 * Copyright (c) 2026 Paul Mattes.
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
 *      ctype32.h
 *              Partially Unicode-aware versions of the ctype functions.
 */

#include "globals.h"

#include "ctype32.h"

#define IS_8BIT(u)	(((u) & 0xff) == (u))

/**
 * Checks if a Unicode chararacter is alphabetic.
 *
 * @param[in] u	Unicode code point.
 *
 * @returns true if alphabetic.
 *
 * @note Does not consider anything above U+00ff as alphabetic, which is probably wrong.
 */
bool
isalpha32(ucs4_t u)
{
    return IS_8BIT(u) && isalpha((unsigned char)u);
}

/**
 * Checks if a Unicode chararacter is a control character.
 *
 * @param[in] u	Unicode code point.
 *
 * @returns true if control character.
 *
 * @note Checks for both C0 and C1 sets.
 */
bool
iscntrl32(ucs4_t u)
{
    return (IS_8BIT(u) && iscntrl((unsigned char)u)) || (u >= 0x80 && u < 0xa0);
}

/**
 * Checks if a Unicode chararacter is a numeric digit.
 *
 * @param[in] u	Unicode code point.
 *
 * @returns true if numeric ('0' through '9').
 */
bool
isdigit32(ucs4_t u)
{
    return IS_8BIT(u) && isdigit((unsigned char)u);
}

/**
 * Checks if a Unicode chararacter is printable.
 *
 * @param[in] u	Unicode code point.
 *
 * @returns true if printable.
 *
 * @note Does not check for the codepoint being undefined or having special semantics.
 */
bool
isprint32(ucs4_t u)
{
    return !iscntrl32(u) && !isspace32(u);
}

/**
 * Checks if a Unicode chararacter is whitespace.
 *
 * @param[in] u	Unicode code point.
 *
 * @returns true if whitespace.
 *
 * @note Matches the usual suspects below U+0021, plus the non-break and Ideographic space characters.
 */
bool
isspace32(ucs4_t u)
{
    return (IS_8BIT(u) && isspace((unsigned char)u)) || u == 0xa0 || u == 0x3000;
}

/**
 * Checks if a Unicode chararacter is a hexadecimal digit.
 *
 * @param[in] u	Unicode code point.
 *
 * @returns true if hexadecimal.
 */
bool
isxdigit32(ucs4_t u)
{
    return IS_8BIT(u) && isxdigit((unsigned char)u);
}
