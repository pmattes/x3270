/*
 * Copyright (c) 1999-2025 Paul Mattes.
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
 * 	XtGlue.c
 * 		Replacements for Xt library code.
 */

#include "globals.h"
#include "glue.h"
#include "latin1.h"
#include "trace.h"

#include <stdio.h>

void (*Error_redirect)(const char *) = NULL;
void (*Warning_redirect)(const char *) = NULL;

/* Replacement for the Xt Error() function -- write an error message to stdout and exit. */
void
Error(const char *s)
{
    vtrace("Error: %s\n", s);
    if (Error_redirect != NULL) {
	(*Error_redirect)(s);
	return;
    }
    fprintf(stderr, "Error: %s\n", s);
    fflush(stderr);
    exit(1);
}

/* Replacement for the Xt Warning() function -- write an error message to stdout and exit. */
void
Warning(const char *s)
{
    vtrace("Warning: %s\n", s);
    if (Warning_redirect != NULL) {
	(*Warning_redirect)(s);
    } else {
	fprintf(stderr, "Warning: %s\n", s);
	fflush(stderr);
    }
}

/* Support for Xt keysym names. */

static struct {
    /*const*/ char *name;	/* not const because of ancient X11 API */
    latin1_symbol_t key;
} latin1[] = {
    /* HTML entities and X11 KeySym names. */
    { "sp",            latin1_sp },
    {  "space",        latin1_sp },
    { "excl",          latin1_excl },
    {  "exclam",       latin1_excl },
    { "quot",          latin1_quot },
    {  "quotedbl",     latin1_quot },
    { "num",           latin1_num },
    {  "numbersign",   latin1_num },
    { "dollar",        latin1_dollar },
    { "percnt",        latin1_percnt },
    {  "percent",      latin1_percnt },
    { "amp",           latin1_amp },
    {  "ampersand",    latin1_amp },
    { "apos",          latin1_apos },
    {  "apostrophe",   latin1_apos },
    {  "quoteright",   latin1_apos },
    { "lpar",          latin1_lpar },
    {  "parenleft",    latin1_lpar },
    { "rpar",          latin1_rpar },
    {  "parenright",   latin1_rpar },
    { "ast",           latin1_ast },
    {  "asterisk",     latin1_ast },
    { "plus",          latin1_plus },
    { "comma",         latin1_comma },
    { "minus",         latin1_minus },
    {  "hyphen",       latin1_minus }, /* There is a conflict here between
					  HTML and X11, which uses 'hyphen'
					  for shy (U+00AD). HTML wins. */
    { "period",        latin1_period },
    { "sol",           latin1_sol },
    {  "slash",        latin1_sol },
    { "0",             latin1_0 },
    { "1",             latin1_1 },
    { "2",             latin1_2 },
    { "3",             latin1_3 },
    { "4",             latin1_4 },
    { "5",             latin1_5 },
    { "6",             latin1_6 },
    { "7",             latin1_7 },
    { "8",             latin1_8 },
    { "9",             latin1_9 },
    { "colon",         latin1_colon },
    { "semi",          latin1_semi },
    {  "semicolon",    latin1_semi },
    { "lt",            latin1_lt },
    {  "less",         latin1_lt },
    { "equals",        latin1_equals },
    {  "equal",        latin1_equals },
    { "gr",            latin1_gt },
    {  "greater",      latin1_gt },
    { "quest",         latin1_quest },
    {  "question",     latin1_quest },
    { "commat",        latin1_commat },
    {  "at",           latin1_commat },
    { "A",             latin1_A },
    { "B",             latin1_B },
    { "C",             latin1_C },
    { "D",             latin1_D },
    { "E",             latin1_E },
    { "F",             latin1_F },
    { "G",             latin1_G },
    { "H",             latin1_H },
    { "I",             latin1_I },
    { "J",             latin1_J },
    { "K",             latin1_K },
    { "L",             latin1_L },
    { "M",             latin1_M },
    { "N",             latin1_N },
    { "O",             latin1_O },
    { "P",             latin1_P },
    { "Q",             latin1_Q },
    { "R",             latin1_R },
    { "S",             latin1_S },
    { "T",             latin1_T },
    { "U",             latin1_U },
    { "V",             latin1_V },
    { "W",             latin1_W },
    { "X",             latin1_X },
    { "Y",             latin1_Y },
    { "Z",             latin1_Z },
    { "lsqb",          latin1_lsqb },
    {  "bracketleft",  latin1_lsqb },
    { "bsol",          latin1_bsol },
    {  "backslash",    latin1_bsol },
    { "rsqb",          latin1_rsqb },
    {  "bracketright", latin1_rsqb },
    { "circ",          latin1_circ },
    {  "asciicircum",  latin1_circ },
    { "lowbar",        latin1_lowbar },
    {  "horbar",       latin1_lowbar },
    {  "underscore",   latin1_lowbar },
    { "grave",         latin1_grave },
    {  "quoteleft",    latin1_grave },
    { "a",             latin1_a },
    { "b",             latin1_b },
    { "c",             latin1_c },
    { "d",             latin1_d },
    { "e",             latin1_e },
    { "f",             latin1_f },
    { "g",             latin1_g },
    { "h",             latin1_h },
    { "i",             latin1_i },
    { "j",             latin1_j },
    { "k",             latin1_k },
    { "l",             latin1_l },
    { "m",             latin1_m },
    { "n",             latin1_n },
    { "o",             latin1_o },
    { "p",             latin1_p },
    { "q",             latin1_q },
    { "r",             latin1_r },
    { "s",             latin1_s },
    { "t",             latin1_t },
    { "u",             latin1_u },
    { "v",             latin1_v },
    { "w",             latin1_w },
    { "x",             latin1_x },
    { "y",             latin1_y },
    { "z",             latin1_z },
    { "lcub",          latin1_lcub },
    {  "braceleft",    latin1_lcub },
    { "verbar",        latin1_verbar },
    {  "bar",          latin1_verbar },
    { "rcub",          latin1_rcub },
    {  "braceright",   latin1_rcub },
    { "tilde",         latin1_tilde },
    {  "asciitilde",   latin1_tilde },
    { "nbsp",          latin1_nbsp },
    {  "nobreakspace", latin1_nbsp },
    { "iexcl",         latin1_iexcl },
    {  "exclamdown",   latin1_iexcl },
    { "cent",          latin1_cent },
    { "pound",         latin1_pound },
    {  "sterling",     latin1_pound },
    { "curren",        latin1_curren },
    {  "currency",     latin1_curren },
    { "yen",           latin1_yen },
    { "brkbar",        latin1_brkbar },
    {  "brvbar",       latin1_brkbar },
    {  "brokenbar",    latin1_brkbar },
    { "sect",          latin1_sect },
    {  "section",      latin1_sect },
    { "uml",           latin1_uml },
    {  "die",          latin1_uml },
    {  "diaeresis",    latin1_uml },
    { "copy",          latin1_copy },
    {  "copyright",    latin1_copy },
    { "ordf",          latin1_ordf },
    {  "ordfeminine",  latin1_ordf },
    { "laquo",         latin1_laquo },
    {  "guillemotleft",latin1_laquo },
    { "not",           latin1_not },
    {  "notsign",      latin1_not },
    { "shy",           latin1_shy },
    { "reg",           latin1_reg },
    {  "registered",   latin1_reg },
    { "macr",          latin1_macr },
    {  "hibar",        latin1_macr },
    {  "macron",       latin1_macr },
    { "deg",           latin1_deg },
    {  "degree",       latin1_deg },
    { "plusmn",        latin1_plusmn },
    {  "plusminus",    latin1_plusmn },
    { "sup2",          latin1_sup2 },
    {  "twosuperior",  latin1_sup2 },
    { "sup3",          latin1_sup3 },
    {  "threesuperior",latin1_sup3 },
    { "acute",         latin1_acute },
    { "micro",         latin1_micro },
    {  "mu",           latin1_micro },
    { "para",          latin1_para },
    {  "paragraph",    latin1_para },
    { "middot",        latin1_middot },
    {  "periodcentered",latin1_middot },
    { "cedil",         latin1_cedil },
    {  "cedilla",      latin1_cedil },
    { "sup1",          latin1_sup1 },
    {  "onesuperior",  latin1_sup1 },
    { "ordm",          latin1_ordm },
    {  "masculine",    latin1_ordm },
    { "raquo",         latin1_raquo },
    {  "guillemotright",latin1_raquo },
    { "frac14",        latin1_frac14 },
    {  "onequarter",   latin1_frac14 },
    { "frac12",        latin1_frac12 },
    {  "half",         latin1_frac12 },
    {  "onehalf",      latin1_frac12 },
    { "frac34",        latin1_frac34 },
    {  "threequarters",latin1_frac34 },
    { "iquest",        latin1_iquest },
    {  "questiondown", latin1_iquest },
    { "Agrave",        latin1_Agrave },
    { "Aacute",        latin1_Aacute },
    { "Acirc",         latin1_Acirc },
    {  "Acircumflex",  latin1_Acirc },
    { "Atilde",        latin1_Atilde },
    { "Auml",          latin1_Auml },
    {  "Adiaeresis",   latin1_Auml },
    { "Aring",         latin1_Aring },
    { "AElig",         latin1_AElig },
    {  "AE",           latin1_AElig },
    { "Ccedil",        latin1_Ccedil },
    {  "Ccedilla",     latin1_Ccedil },
    { "Egrave",        latin1_Egrave },
    { "Eacute",        latin1_Eacute },
    { "Ecirc",         latin1_Ecirc },
    {  "Ecircumflex",  latin1_Ecirc },
    { "Euml",          latin1_Euml },
    {  "Ediaeresis",   latin1_Euml },
    { "Igrave",        latin1_Igrave },
    { "Iacute",        latin1_Iacute },
    { "Icirc",         latin1_Icirc },
    {  "Icircumflex",  latin1_Icirc },
    { "Iuml",          latin1_Iuml },
    {  "Idiaeresis",   latin1_Iuml },
    { "ETH",           latin1_ETH },
    {  "Eth",          latin1_ETH },
    { "Ntilde",        latin1_Ntilde },
    { "Ograve",        latin1_Ograve },
    { "Oacute",        latin1_Oacute },
    { "Ocirc",         latin1_Ocirc },
    {  "Ocircumflex",  latin1_Ocirc },
    { "Otilde",        latin1_Otilde },
    { "Ouml",          latin1_Ouml },
    {  "Odiaeresis",   latin1_Ouml },
    { "times",         latin1_times },
    {  "multiply",     latin1_times },
    { "Oslash",        latin1_Oslash },
    {  "Ooblique",     latin1_Oslash },
    { "Ugrave",        latin1_Ugrave },
    { "Uacute",        latin1_Uacute },
    { "Ucirc",         latin1_Ucirc },
    {  "Ucircumflex",  latin1_Ucirc },
    { "Uuml",          latin1_Uuml },
    {  "Udiaeresis",   latin1_Uuml },
    { "Yacute",        latin1_Yacute },
    { "THORN",         latin1_THORN },
    {  "Thorn",        latin1_THORN },
    { "szlig",         latin1_szlig },
    {  "ssharp",       latin1_szlig },
    { "agrave",        latin1_agrave },
    { "aacute",        latin1_aacute },
    { "acirc",         latin1_acirc },
    {  "acircumflex",  latin1_acirc },
    { "atilde",        latin1_atilde },
    { "auml",          latin1_auml },
    {  "adiaeresis",   latin1_auml },
    { "aring",         latin1_aring },
    { "aelig",         latin1_aelig },
    {  "ae",           latin1_aelig },
    { "ccedil",        latin1_ccedil },
    {  "ccedilla",     latin1_ccedil },
    { "egrave",        latin1_egrave },
    { "eacute",        latin1_eacute },
    { "ecirc",         latin1_ecirc },
    {  "ecircumflex",  latin1_ecirc },
    { "euml",          latin1_euml },
    {  "ediaeresis",   latin1_euml },
    { "igrave",        latin1_igrave },
    { "iacute",        latin1_iacute },
    { "icirc",         latin1_icirc },
    {  "icircumflex",  latin1_icirc },
    { "iuml",          latin1_iuml },
    {  "idiaeresis",   latin1_iuml },
    { "eth",           latin1_eth },
    { "ntilde",        latin1_ntilde },
    { "ograve",        latin1_ograve },
    { "oacute",        latin1_oacute },
    { "ocirc",         latin1_ocirc },
    {  "ocircumflex",  latin1_ocirc },
    { "otilde",        latin1_otilde },
    { "ouml",          latin1_ouml },
    {  "odiaeresis",   latin1_ouml },
    { "divide",        latin1_divide },
    {  "division",     latin1_divide },
    { "oslash",        latin1_oslash },
    { "ugrave",        latin1_ugrave },
    { "uacute",        latin1_uacute },
    { "ucirc",         latin1_ucirc },
    {  "ucircumflex",  latin1_ucirc },
    { "uuml",          latin1_uuml },
    {  "udiaeresis",   latin1_uuml },
    { "yacute",        latin1_yacute },
    { "thorn",         latin1_thorn },
    { "yuml",          latin1_yuml },
    {  "ydiaeresis",   latin1_yuml },

    /*
     * The following are, umm, hacks to allow symbolic names for
     * control codes.
     */
#if !defined(_WIN32) /*[*/
    { "BackSpace",     0x08 },
    { "Tab",           0x09 },
    { "LineFeed",      0x0a },
    { "Return",        0x0d },
    { "Escape",        0x1b },
    { "Delete",        0x7f },
#endif /*]*/

    { NULL,            0 }
};

ks_t
string_to_key(char *s)
{
    int i;

    if (strlen(s) == 1 && (*(unsigned char *)s & 0x7f) > ' ') {
	return *(unsigned char *)s;
    }
    for (i = 0; latin1[i].name != NULL; i++) {
	if (!strcmp(s, latin1[i].name)) {
	    return latin1[i].key;
	}
    }
    return KS_NONE;
}

char *
key_to_string(ks_t k)
{
    int i;

    for (i = 0; latin1[i].name != NULL; i++) {
	if (latin1[i].key == k) {
	    return latin1[i].name;
	}
    }
    return NULL;
}
