/*
 * Copyright (c) 1993-2009, 2014-2015 Paul Mattes.
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

#include "latin1.h"
#include "apl.h"


/*
 * APL key translation.
 *
 * This code looks a little odd because of how an APL font is implemented.
 * An APL font has APL graphics in place of the various accented letters and
 * special symbols in a regular font.  APL key translation consists of
 * taking the key name for an APL symbol (these names are meaningful only to
 * x3270) and translating it into the key for the regular symbol that the
 * desired APL symbol _replaces_.
 *
 * For example, an APL font has the APL "jot" symbol where a regular font has
 * the "registered" symbol.  So we take the key name "jot" and translate it
 * into the key latin1_reg.  When the latin1_reg symbol is displayed
 * with an APL font, it appears as a "jot".
 *
 * The specification of which APL symbols replace which regular symbols is in
 * IBM GA27-3831, 3174 Establishment Controller Character Set Reference.
 *
 * In addition, several standard characters have different names for APL,
 * for example, "period" becomes "dot".  These are included in the table as
 * well.
 */

static struct {
	const char *name;
	latin1_symbol_t key;
	int is_ge;
} axl[] = {
	{ "Aunderbar",		latin1_nbsp,		1 },
	{ "Bunderbar",		latin1_acirc,		1 },
	{ "Cunderbar",		latin1_auml,		1 },
	{ "Dunderbar",		latin1_agrave,		1 },
	{ "Eunderbar",		latin1_aacute,		1 },
	{ "Funderbar",		latin1_atilde,		1 },
	{ "Gunderbar",		latin1_aring,		1 },
	{ "Hunderbar",		latin1_ccedil,		1 },
	{ "Iunderbar",		latin1_ntilde,		1 },
	{ "Junderbar",		latin1_eacute,		1 },
	{ "Kunderbar",		latin1_ecirc,		1 },
	{ "Lunderbar",		latin1_euml,		1 },
	{ "Munderbar",		latin1_egrave,		1 },
	{ "Nunderbar",		latin1_iacute,		1 },
	{ "Ounderbar",		latin1_icirc,		1 },
	{ "Punderbar",		latin1_iuml,		1 },
	{ "Qunderbar",		latin1_igrave,		1 },
	{ "Runderbar",		latin1_szlig,		1 },
	{ "Sunderbar",		latin1_Acirc,		1 },
	{ "Tunderbar",		latin1_Auml,		1 },
	{ "Uunderbar",		latin1_Agrave,		1 },
	{ "Vunderbar",		latin1_Aacute,		1 },
	{ "Wunderbar",		latin1_Atilde,		1 },
	{ "Xunderbar",		latin1_Aring,		1 },
	{ "Yunderbar",		latin1_Ccedil,		1 },
	{ "Zunderbar",		latin1_Ntilde,		1 },
	{ "alpha",		latin1_circ,		1 },
	{ "bar",		latin1_minus,		0 },
	{ "braceleft",		latin1_lcub,		1 },
	{ "braceright",		latin1_rcub,		1 },
	{ "bracketleft",	latin1_Yacute,		1 },
	{ "bracketright", 	latin1_uml,		1 },
	{ "circle",		latin1_cedil,		1 },
	{ "circlebar",		latin1_Ograve,		1 },
	{ "circleslope",	latin1_otilde,		1 },
	{ "circlestar",		latin1_Ugrave,		1 },
	{ "circlestile",	latin1_ograve,		1 },
	{ "colon",		latin1_colon,		0 },
	{ "comma",		latin1_comma,		0 },
	{ "commabar",		latin1_W,		1 }, /* soliton */
	{ "del",		latin1_lsqb,		1 },
	{ "delstile",		latin1_uuml,		1 },
	{ "delta",		latin1_rsqb,		1 },
	{ "deltastile",		latin1_ugrave,		1 },
	{ "deltaunderbar",	latin1_Uuml,		1 },
	{ "deltilde",		latin1_Ucirc,		1 },
	{ "diaeresis",		latin1_Ecirc,		1 },
	{ "diaeresiscircle",	latin1_V,		1 }, /* soliton */
	{ "diaeresisdot",	latin1_Ouml,		1 },
	{ "diaeresisjot",	latin1_U,		1 }, /* soliton */
	{ "diamond",		latin1_oslash,		1 },
	{ "dieresis",		latin1_Ecirc,		1 },
	{ "dieresisdot",	latin1_Ouml,		1 },
	{ "divide",		latin1_frac12,		1 },
	{ "dot",		latin1_period,		0 },
	{ "downarrow",		latin1_raquo,		1 },
	{ "downcaret",		latin1_Igrave,		1 },
	{ "downcarettilde",	latin1_ocirc,		1 },
	{ "downshoe",		latin1_iquest,		1 },
	{ "downstile",		latin1_thorn,		1 },
	{ "downtack",		latin1_ETH,		1 },
	{ "downtackjot",	latin1_Uacute,		1 },
	{ "downtackup",		latin1_sup1,		1 },
	{ "downtackuptack",	latin1_sup1,		1 },
	{ "epsilon",		latin1_pound,		1 },
	{ "epsilonunderbar",	latin1_Iacute,		1 },
	{ "equal",		latin1_equals,		0 },
	{ "equalunderbar",	latin1_bsol,		1 },
	{ "euro",		latin1_X,		1 }, /* soliton */
	{ "greater",		latin1_gt,		0 },
	{ "iota",		latin1_yen,		1 },
	{ "iotaunderbar",	latin1_Egrave,		1 },
	{ "jot",		latin1_reg,		1 },
	{ "leftarrow",		latin1_curren,		1 },
	{ "leftbracket",	latin1_Yacute,		1 },
	{ "leftparen",		latin1_lpar,		0 },
	{ "leftshoe",		latin1_ordm,		1 },
	{ "lefttack",		latin1_Icirc,		1 },
	{ "less",		latin1_lt,		0 },
	{ "multiply",		latin1_para,		1 },
	{ "notequal",		latin1_acute,		1 },
	{ "notgreater",		latin1_eth,		1 },
	{ "notless",		latin1_THORN,		1 },
	{ "omega",		latin1_copy,		1 },
	{ "overbar",		latin1_micro,		1 },
	{ "plus",		latin1_plus,		0 },
	{ "plusminus",		latin1_AElig,		1 },
	{ "quad",		latin1_deg,		1 },
	{ "quaddivide",		latin1_Oacute,		1 },
	{ "quadjot",		latin1_Euml,		1 },
	{ "quadquote",		latin1_uacute,		1 },
	{ "quadslope",		latin1_oacute,		1 },
	{ "query",		latin1_quest,		0 },
	{ "quote",		latin1_apos,		0 },
	{ "quotedot",		latin1_ucirc,		1 },
	{ "rho",		latin1_middot,		1 },
	{ "rightarrow",		latin1_plusmn,		1 },
	{ "rightbracket", 	latin1_uml,		1 },
	{ "rightparen",		latin1_rpar,		0 },
	{ "rightshoe",		latin1_ordf,		1 },
	{ "righttack",		latin1_Iuml,		1 },
	{ "semicolon",		latin1_semi,		0 },
	{ "slash",		latin1_sol,		0 },
	{ "slashbar",		latin1_sup2,		1 },
	{ "slope",		latin1_frac14,		1 },
	{ "slopebar",		latin1_Ocirc,		1 },
	{ "slopequad",		latin1_oacute,		1 },
	{ "splat",		latin1_aelig,		1 },
	{ "squad",		latin1_ouml,		1 },
	{ "star",		latin1_ast,		0 },
	{ "stile",		latin1_times,		1 },
	{ "tilde",		latin1_Oslash,		1 },
	{ "times",		latin1_para,		1 },
	{ "underbar",		latin1_lowbar,		0 },
	{ "uparrow",		latin1_laquo,		1 },
	{ "upcaret",		latin1_Eacute,		1 },
	{ "upcarettilde",	latin1_shy,		1 },
	{ "upshoe",		latin1_iexcl,		1 },
	{ "upshoejot",		latin1_yuml,		1 },
	{ "upstile",		latin1_yacute,		1 },
	{ "uptack",		latin1_macr,		1 },
	{ "uptackjot",		latin1_Otilde,		1 },
	{ 0, 0 }
};

/*
 * Translation from APL key names to indirect APL keys.
 */
ks_t
apl_string_to_key(const char *s, int *is_gep)
{
    int i;

    if (strncmp(s, "apl_", 4)) {
	return KS_NONE;
    }
    s += 4;
    for (i = 0; axl[i].name; i++)
	if (!strcmp(axl[i].name, s)) {
	    *is_gep = axl[i].is_ge;
	    return axl[i].key;
	}
    return KS_NONE;
}

/*
 * Translation from latin1 symbol to APL character name.
 */
const char *
key_to_apl_string(ks_t k)
{
    int i;

    for (i = 0; axl[i].name; i++) {
	if (axl[i].key == k) {
	    return axl[i].name;
	}
    }
    return NULL;
}
