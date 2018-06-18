/*
 * Copyright (c) 1993-2009, 2014-2018 Paul Mattes.
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
 *	apl.c
 *		This module handles APL-specific actions.
 */

#include "globals.h"

#include <assert.h>

#include "3270ds.h"
#include "apl.h"
#include "unicodec.h"

/*
 * APL translation table.
 * Translates a symbolic key name to a Unicode code point, and to an EBCDIC
 * value with a GE indicator.
 *
 * The UPRIV2 range is used to represent APL characters with no Unicode code
 * points (underlined alphabetics). Some fonts use circled alphabetics for
 * these, but this is non-standard.
 *
 * (1) Not on Code Page 310
 *
 * Reference: https://aplwiki.com/UnicodeAplTable
 *            https://en.wikipedia.org/wiki/Code_page_310
 *            https://www.tachyonsoft.com/cp00310.htm
 *
 * Note that Tachyonsoft and Wikipedia disagree on X'DB'. Wikipedia translates
 * it to U+0021 ('!'), but Tachyonsoft translates it to U+01c3, Latin Letter
 * Retroflex Click. I am going with Wikipedia for now.
 *
 * Note: This table is partially redundant with apl2uc[] in unicode.c, and 
 * needs to be kept consistent with it. apl2uc[] has Unicode translations for
 * additional line-drawing code points that are not intended for keyboard
 * input.
 */

static struct {
    const char *name;
    ucs4_t ucs4;
    unsigned char ebc;
    bool ge;
} au[] = {
    /* APL Name		Unicode Value	EBCDIC  GE         Unicode Name */
    { "Aunderbar",	UPRIV2 + 'A',	0x41, true },
    { "Bunderbar",	UPRIV2 + 'B',	0x42, true },
    { "Cunderbar",	UPRIV2 + 'C',	0x43, true },
    { "Dunderbar",	UPRIV2 + 'D',	0x44, true },
    { "Eunderbar",	UPRIV2 + 'E',	0x45, true },
    { "Funderbar",	UPRIV2 + 'F',	0x46, true },
    { "Gunderbar",	UPRIV2 + 'G',	0x47, true },
    { "Hunderbar",	UPRIV2 + 'H',	0x48, true },
    { "Iunderbar",	UPRIV2 + 'I',	0x49, true },
    { "dot",		'.',		0x4b, false },	/* Full Stop */
    { "less",		'<', 		0x4c, false },	/* Less-than Sign */
    { "leftparen",	'(',		0x4d, false },	/* Left Parenthesis */
    { "plus",		'+',		0x4e, false },	/* Plus Sign */
    { "Junderbar",	UPRIV2 + 'J',	0x51, true },
    { "Kunderbar",	UPRIV2 + 'K',	0x52, true },
    { "Lunderbar",	UPRIV2 + 'L',	0x53, true },
    { "Munderbar",	UPRIV2 + 'M',	0x54, true },
    { "Nunderbar",	UPRIV2 + 'N',	0x55, true },
    { "Ounderbar",	UPRIV2 + 'O',	0x56, true },
    { "Punderbar",	UPRIV2 + 'P',	0x57, true },
    { "Qunderbar",	UPRIV2 + 'Q',	0x58, true },
    { "Runderbar",	UPRIV2 + 'R',	0x59, true },
    { "star",		'*',		0x5c, false },	/* Asterisk */
    { "rightparen",	')',		0x5d, false },	/* Right Parentheses */
    { "semicolon",	';',		0x5e, false },	/* Semicolon */
    { "bar",		'-',		0x60, false },	/* Hyphen-minus */
    { "slash",		'/',		0x61, false },	/* Solidus */
    { "Sunderbar",	UPRIV2 + 'S',	0x62, true },
    { "Tunderbar",	UPRIV2 + 'T',	0x63, true },
    { "Uunderbar",	UPRIV2 + 'U',	0x64, true },
    { "Vunderbar",	UPRIV2 + 'V',	0x65, true },
    { "Wunderbar",	UPRIV2 + 'W',	0x66, true },
    { "Xunderbar",	UPRIV2 + 'Z',	0x67, true },
    { "Yunderbar",	UPRIV2 + 'Y',	0x68, true },
    { "Zunderbar",	UPRIV2 + 'Z',	0x69, true },
    { "comma",		',',		0x6b, false },	/* Comma */
    { "underbar",	'_',		0x6d, false },	/* Low Line */
    { "greater",	'>',		0x6e, false },	/* Greater-than Sign */
    { "query",		'?',		0x6f, false },	/* Question Mark */
    { "diamond",	0x22c4,		0x70, true },	/* Diamond Operator */
    { "upcaret",	0x2227,		0x71, true },	/* Logical AND */
    { "diaeresis",	0x00a8,		0x72, true },	/* Diaeresis */
    { "dieresis",	0x00a8,		0x72, true },	/* Diaeresis */
    { "quadjot",	0x233b,		0x73, true },	/* APL Functional
							   Symbol Quad Jot */
    { "iotaunderbar",	0x2378,		0x74, true },	/* APL Functional
							   Symbol Iota
							   Underbar */
    { "epsilonunderbar",0x2377,		0x75, true },	/* APL Functional
							   Symbol Epsilon
							   Underbar */
    { "righttack",	0x22a2,		0x76, true },	/* Right Tack */
    { "lefttack",	0x22a3,		0x77, true },	/* Left Tack */
    { "downcaret",	0x2228,		0x78, true },	/* Logical Or */
    { "colon",		':',		0x7a, false },	/* Colon */
    { "quote",		'\'',		0x7d, false },	/* Apostrophe */
    { "equal",		'=',		0x7e, false },	/* Equals Sign */
    { "tilde",		0x223c,		0x80, true },	/* Tilde Operator */
    { "uparrow",	0x2191,		0x8a, true },	/* Upwards Arrow */
    { "downarrow",	0x2193,		0x8b, true },	/* Downwards Arrow */
    { "notgreater",	0x2264,		0x8c, true },	/* Less-than Or Equal
							   To */
    { "upstile",	0x2308,		0x8d, true },	/* Left Ceiling */
    { "downstile",	0x230a,		0x8e, true },	/* Left Floor */
    { "rightarrow",	0x2192,		0x8f, true },	/* Rightwards Arrow */
    { "quad",		0x2395,		0x90, true },	/* APL Functional
							   Symbol Quad */
    { "rightshoe",	0x2283,		0x9a, true },	/* Superset Of */
    { "leftshoe",	0x2282,		0x9b, true },	/* Subset Of */
    { "splat",		0x00a4,		0x9c, true },	/* Currency Sign */
    { "circle",		0x25cb,		0x9d, true },	/* White Circle */
    { "plusminus",	0x00b1,		0x9e, true },	/* Plus Minus Sign */
    { "leftarrow",	0x2190,		0x9f, true },	/* Leftwards Arrow */
    { "overbar",	0x00af,		0xa0, true },	/* Macron */
    { "degree",		0x00b0,		0xa1, true },	/* Degree Sign */
    { "upshoe",		0x2229,		0xaa, true },	/* Intersection */
    { "downshoe",	0x222a,		0xab, true },	/* Union */
    { "uptack",		0x22a5,		0xac, true },	/* Up Tack */
    { "bracketleft",	'[',		0xad, true },	/* Left Square
							   Bracket */
    { "leftbracket",	'[',		0xad, true },	/* Left Square
							   Bracket */
    { "notless",	0x2265, 	0xae, true },	/* Greater-than Or
							   Equal To */
    { "jot",		0x2218,		0xaf, true },	/* Ring operator */
    { "alpha",		0x237a,		0xb0, true },	/* APL Functional
							   Symbol Alpha */
    { "epsilon",	0x220a,		0xb1, true },	/* Small Element Of */
    { "iota",		0x2373,		0xb2, true },	/* APL Functional
							   Symbol Iota */
    { "rho",		0x2374,		0xb3, true },	/* APL Functional
							   Symbol Rho */
    { "omega",		0x2375,		0xb4, true },	/* APL Functional
							   Symbol Omega */
    { "multiply",	0x00d7,		0xb6, true },	/* Multiplication
							   Sign */
    { "times",		0x00d7,		0xb6, true },	/* Multiplication
							   Sign */
    { "slope",		'\\',		0xb7, true },	/* Reverse Solidus */
    { "divide",		0x00f7,		0xb8, true },	/* Division Sign */
    { "del",		0x2207,		0xba, true },	/* Nabla */
    { "delta",		0x2206,		0xbb, true },	/* Increment */
    { "downtack",	0x22a4,		0xbc, true },	/* Down Tack */
    { "bracketright", 	']',		0xbd, true },	/* Right Square
							   Bracket */
    { "rightbracket", 	']',		0xbd, true },	/* Right Square
							   Bracket */
    { "notequal",	0x2260,		0xbe, true },	/* Not Equal To */
    { "stile",		0x2223,		0xbf, true },	/* Divides */
    { "braceleft",	'{',		0xc0, true },	/* Left Curly
							   Bracket */
    { "section",	0x00a7,		0xc8, true },	/* Section Sign */
    { "upcarettilde",	0x2372,		0xca, true },	/* APL Functional
							   Symbol Up Caret
							   Tilde */
    { "downcarettilde",	0x2371,		0xcb, true },	/* APL Functional
							   Symbol Down Caret
							   Tilde */
    { "squad",		0x2337,		0xcc, true },	/* APL Functional
							   Symbol Squish
							   Quad */
    { "circlestile",	0x233d,		0xcd, true },	/* APL Functional
							   Symbol Circle
							   Stile */
    { "quadslope",	0x2342,		0xce, true },	/* APL Functional
							   Symbol Quad
							   Backslash */
    { "slopequad",	0x2342,		0xce, true },	/* APL Functional
							   Symbol Quad
							   Backslash */
    { "circleslope",	0x2349,		0xcf, true },	/* APL Functional
							   Symbol Circle
							   Backslash */
    { "braceright",	'}',		0xd0, true },	/* Right Curly
							   Bracket */
    { "paragraph",	0x00b6,		0xd8, true },	/* Pilcrow sign */
    { "downtackup",	0x2336,		0xda, true },	/* APL Functional
							   Symbol I-beam */
    { "downtackuptack",	0x2336,		0xda, true },	/* APL Functional
							   Symbol I-beam */
    { "quotedot",	'!',		0xdb, true },	/* Exclamation Mark */
    { "delstile",	0x2352,		0xdc, true },	/* APL Functional
							   Symbol Del Stile */
    { "deltastile",	0x234b,		0xdd, true },	/* APL Functional
							   Symbol Delta
							   Stile */
    { "quadquote",	0x235e, 	0xde, true },	/* APL Functional
							   Symbol Quote Quad */
    { "upshoejot",	0x235d,		0xdf, true },	/* APL Functional
							   Symbol Up Shoe
							   Jot */
    { "equalunderbar",	0x2261,		0xe0, true },	/* Identical To */
    { "equiv",		0x2261,		0xe0, true },	/* Identical To */
    { "diaeresisjot",	0x2364,		0xe4, true },	/* (1) APL Functional
							   Symbol Jot
							   Diaeresis */
    { "dieresisjot",	0x2364,		0xe4, true },	/* (1) APL Functional
    							   Symbol Jot
							   Diaeresis */
    { "diaeresiscircle",0x2365,		0xe5, true },	/* (1) APL Functional
							   Symbol Cicrle
							   Diaeresis */
    { "dieresiscircle",	0x2365,		0xe5, true },	/* (1) APL Functional
							   Symbol Cicrle
							   Diaeresis */
    { "commabar",	0x236a,		0xe6, true },	/* (1) APL Functional
							   Symbol Comma Bar */
    { "euro",		0x20ac,		0xe7, true },	/* (1) Euro Sign */
    { "slashbar",	0x233f,		0xea, true },	/* APL Functional
							   Symbol Slash Bar */
    { "slopebar",	0x2340,		0xeb, true },	/* APL Functional
							   Symbol Backslash
							   Bar */
    { "diaeresisdot",	0x2235,		0xec, true },	/* Because */
    { "dieresisdot",	0x2235,		0xec, true },	/* Because */
    { "circlebar",	0x2296,		0xed, true },	/* Circled Minus */
    { "quaddivide",	0x2339,		0xee, true },	/* APL Functional
							   Symbol Quad
							   Divide */
    { "uptackjot",	0x2355,		0xef, true },	/* APL Functional
							   Symbol Up Tack Jot */
    { "deltilde",	0x236b,		0xfb, true },	/* APL Functional
							   Symbol Del Tilde */
    { "deltaunderbar",	0x2359,		0xfc, true },	/* APL Functional
							   Symbol Delta
							   Underbar */
    { "circlestar",	0x235f,		0xfd, true },	/* APL Functional
							   Symbol Circle Star */
    { "downtackjot",	0x234e,		0xfe, true },	/* APL Functional
							   Symbol Down Tack
							   Jot */
    { NULL, 0 }
};

/*
 * Check the consistency of au[] and apl2uc[].
 */
void
check_apl_consistency(ucs4_t apl2uc[])
{
    int i;

    for (i = 0; au[i].name; i++) {
	if (au[i].ucs4 > 0x7f && au[i].ucs4 < UPRIV2) {
	    assert(apl2uc[au[i].ebc] == au[i].ucs4);
	}
    }
}

/*
 * Translate a symbolic APL key name to a Unicode code point.
 */
ucs4_t
apl_key_to_ucs4(const char *s, bool *is_ge)
{
    int i;

    if (strncmp(s, "apl_", 4)) {
	return 0;
    }
    s += 4;
    for (i = 0; au[i].name; i++) {
	if (!strcmp(au[i].name, s)) {
	    *is_ge = au[i].ge;
	    return au[i].ucs4;
	}
    }
    return 0;
}

/*
 * Translate from a Unicode code point to APL character name (without the
 * "apl_" prefix).
 */
const char *
ucs4_to_apl_key(ucs4_t ucs4)
{
    int i;

    for (i = 0; au[i].name; i++) {
	if (au[i].ucs4 == ucs4) {
	    return au[i].name;
	}
    }
    return NULL;
}
