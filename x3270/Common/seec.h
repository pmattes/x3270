/*
 * Copyright (c) 1993-2009, Paul Mattes.
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
 *	seec.h
 *		Declarations for see.c
 *
 */

#if defined(X3270_TRACE) /*[*/

extern const char *see_aid(unsigned char code);
extern const char *see_attr(unsigned char fa);
extern const char *see_color(unsigned char setting);
extern const char *see_ebc(unsigned char ch);
extern const char *see_efa(unsigned char efa, unsigned char value);
extern const char *see_efa_only(unsigned char efa);
extern const char *see_qcode(unsigned char id);
extern const char *unknown(unsigned char value);

#else /*][*/

#define see_aid 0 &&
#define see_attr 0 &&
#define see_color 0 &&
#define see_ebc 0 &&
#define see_efa 0 &&
#define see_efa_only 0 &&
#define see_qcode 0 &&
#define unknown 0 &&

#endif /*]*/
