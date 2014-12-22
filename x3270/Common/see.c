/*
 * Copyright (c) 1993-2009, 2013-2014 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC nor
 *       the names of their contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DON RUSSELL "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DON RUSSELL BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	see.c
 *		3270 data stream decode functions.
 *
 */

#include "globals.h"

#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include "3270ds.h"

#include "tablesc.h"
#include "utf8c.h"
#include "utilc.h"
#include "seec.h"
#include "unicodec.h"
#include "varbufc.h"

/**
 * Return an unknown value.
 *
 * @param[in] value	The value to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
static const char *
unknown(unsigned char value, char **rs)
{
    char *r;

    r = xs_buffer("unknown[0x%x]", value);
    Replace((*rs), r);
    return r;
}

/**
 * Encode an EBCDIC character.
 *
 * @param[in] ch	EBCDIC character to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
const char *
see_ebc(unsigned char ch)
{
    static char *rs = NULL;
    char *r;
    char mb[16];
    ucs4_t uc;

    switch (ch) {
    case FCORDER_NULL:
	return "NULL";
    case FCORDER_FF:
	return "FF";
    case FCORDER_CR:
	return "CR";
    case FCORDER_SO:
	return "SO";
    case FCORDER_SI:
	return "SI";
    case FCORDER_NL:
	return "NL";
    case FCORDER_EM:
	return "EM";
    case FCORDER_LF:
	return "LF";
    case FCORDER_DUP:
	return "DUP";
    case FCORDER_FM:
	return "FM";
    case FCORDER_SUB:
	return "SUB";
    case FCORDER_EO:
	return "EO";
    }

    if (ebcdic_to_multibyte_x(ch, CS_BASE, mb, sizeof(mb), EUO_NONE, &uc)
	    && (mb[0] != ' ' || ch == 0x40)) {
	r = NewString(mb);
    } else {
	r = xs_buffer("X'%02X'", ch);
    }
    Replace(rs, r);
    return r;
}

/**
 * Encode an AID code
 *
 * @param[in] code	AID value to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
const char *
see_aid(unsigned char code)
{
    static char *rs = NULL;

    switch (code) {
    case AID_NO: 
	return "NoAID";
    case AID_ENTER: 
	return "Enter";
    case AID_PF1: 
	return "PF1";
    case AID_PF2: 
	return "PF2";
    case AID_PF3: 
	return "PF3";
    case AID_PF4: 
	return "PF4";
    case AID_PF5: 
	return "PF5";
    case AID_PF6: 
	return "PF6";
    case AID_PF7: 
	return "PF7";
    case AID_PF8: 
	return "PF8";
    case AID_PF9: 
	return "PF9";
    case AID_PF10: 
	return "PF10";
    case AID_PF11: 
	return "PF11";
    case AID_PF12: 
	return "PF12";
    case AID_PF13: 
	return "PF13";
    case AID_PF14: 
	return "PF14";
    case AID_PF15: 
	return "PF15";
    case AID_PF16: 
	return "PF16";
    case AID_PF17: 
	return "PF17";
    case AID_PF18: 
	return "PF18";
    case AID_PF19: 
	return "PF19";
    case AID_PF20: 
	return "PF20";
    case AID_PF21: 
	return "PF21";
    case AID_PF22: 
	return "PF22";
    case AID_PF23: 
	return "PF23";
    case AID_PF24: 
	return "PF24";
    case AID_OICR: 
	return "OICR";
    case AID_MSR_MHS: 
	return "MSR_MHS";
    case AID_SELECT: 
	return "Select";
    case AID_PA1: 
	return "PA1";
    case AID_PA2: 
	return "PA2";
    case AID_PA3: 
	return "PA3";
    case AID_CLEAR: 
	return "Clear";
    case AID_SYSREQ: 
	return "SysReq";
    case AID_QREPLY:
	return "QueryReplyAID";
    default: 
	return unknown(code, &rs);
    }
}

/**
 * Encode a field attribute.
 *
 * @param[in] fa	Field attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
const char *
see_attr(unsigned char fa)
{
    static char *rs = NULL;
    varbuf_t r;
    const char *sep = "(";

    vb_init(&r);

    if (fa & FA_PROTECT) {
	vb_appendf(&r, "%sprotected", sep);
	sep = ",";
	if (fa & FA_NUMERIC) {
	    vb_appendf(&r, "%sskip", sep);
	    sep = ",";
	}
    } else if (fa & FA_NUMERIC) {
	vb_appendf(&r, "%snumeric", sep);
	sep = ",";
    }
    switch (fa & FA_INTENSITY) {
    case FA_INT_NORM_NSEL:
	break;
    case FA_INT_NORM_SEL:
	vb_appendf(&r, "%sdetectable", sep);
	sep = ",";
	break;
    case FA_INT_HIGH_SEL:
	vb_appendf(&r, "%sintensified", sep);
	sep = ",";
	break;
    case FA_INT_ZERO_NSEL:
	vb_appendf(&r, "%snondisplay", sep);
	sep = ",";
	break;
    }
    if (fa & FA_MODIFY) {
	vb_appendf(&r, "%smodified", sep);
	sep = ",";
    }
    if (strcmp(sep, "(")) {
	vb_appends(&r, ")");
    } else {
	vb_appends(&r, "(default)");
    }

    Replace(rs, vb_consume(&r));
    return rs;
}

/**
 * Encode a highlight attribute.
 *
 * @param[in] setting	Attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
static const char *
see_highlight(unsigned char setting)
{
    static char *rs = NULL;

    switch (setting) {
    case XAH_DEFAULT:
	return "default";
    case XAH_NORMAL:
	return "normal";
    case XAH_BLINK:
	return "blink";
    case XAH_REVERSE:
	return "reverse";
    case XAH_UNDERSCORE:
	return "underscore";
    case XAH_INTENSIFY:
	return "intensify";
    default:
	return unknown(setting, &rs);
    }
}

/**
 * Encode a color attribute.
 *
 * @param[in] setting	Attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
const char *
see_color(unsigned char setting)
{
    static char *rs = NULL;
    static const char *color_name[] = {
	"neutralBlack",
	"blue",
	"red",
	"pink",
	"green",
	"turquoise",
	"yellow",
	"neutralWhite",
	"black",
	"deepBlue",
	"orange",
	"purple",
	"paleGreen",
	"paleTurquoise",
	"grey",
	"white"
    };

    if (setting == XAC_DEFAULT) {
	return "default";
    } else if (setting < 0xf0) {
	return unknown(setting, &rs);
    } else {
	return color_name[setting - 0xf0];
    }
}

/**
 * Encode a transparency attribute.
 *
 * @param[in] setting	Attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
static const char *
see_transparency(unsigned char setting)
{
    static char *rs = NULL;

    switch (setting) {
    case XAT_DEFAULT:
	return "default";
    case XAT_OR:
	return "or";
    case XAT_XOR:
	return "xor";
    case XAT_OPAQUE:
	return "opaque";
    default:
	return unknown(setting, &rs);
    }
}

/**
 * Encode a validation attribute.
 *
 * @param[in] setting	Attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
static const char *
see_validation(unsigned char setting)
{
    static char *rs = NULL;
    varbuf_t r;
    const char *sep = "(";

    vb_init(&r);
    if (setting & XAV_FILL) {
	vb_appendf(&r, "%sfill", sep);
	sep = ",";
    }
    if (setting & XAV_ENTRY) {
	vb_appendf(&r, "%sentry", sep);
	sep = ",";
    }
    if (setting & XAV_TRIGGER) {
	vb_appendf(&r, "%strigger", sep);
	sep = ",";
    }
    if (strcmp(sep, "(")) {
	vb_appends(&r, ")");
    } else {
	vb_appends(&r, "(none)");
    }
    Replace(rs, vb_consume(&r));
    return rs;
}

/**
 * Encode an outlining attribute.
 *
 * @param[in] setting	Attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
static const char *
see_outline(unsigned char setting)
{
    static char *rs = NULL;
    varbuf_t r;
    const char *sep = "(";

    vb_init(&r);
    if (setting & XAO_UNDERLINE) {
	vb_appendf(&r, "%sunderline", sep);
	sep = ",";
    }
    if (setting & XAO_RIGHT) {
	vb_appendf(&r, "%sright", sep);
	sep = ",";
    }
    if (setting & XAO_OVERLINE) {
	vb_appendf(&r, "%soverline", sep);
	sep = ",";
    }
    if (setting & XAO_LEFT) {
	vb_appendf(&r, "%sleft", sep);
	sep = ",";
    }
    if (strcmp(sep, "(")) {
	vb_appends(&r, ")");
    } else {
	vb_appends(&r, "(none)");
    }
    Replace(rs, vb_consume(&r));
    return rs;
}

/**
 * Encode an input control attribute.
 *
 * @param[in] setting	Attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
static const char *
see_input_control(unsigned char setting)
{
    static char *rs = NULL;

    switch (setting) {
    case XAI_DISABLED:
	return "disabled";
    case XAI_ENABLED:
	return "enabled";
    default:
	return unknown(setting, &rs);
    }
}

/**
 * Encode an extended field attribute.
 *
 * @param[in] efa	Attribute to encode
 * @param[in] value	Value of attribute
 *
 * @return Encoded text representation of the value, good for only one call.
 */
const char *
see_efa(unsigned char efa, unsigned char value)
{
    static char *urs = NULL;
    static char *rs = NULL;
    char *r;

    switch (efa) {
    case XA_ALL:
	r = xs_buffer(" all(%x)", value);
	break;
    case XA_3270:
	r = xs_buffer(" 3270%s", see_attr(value));
	break;
    case XA_VALIDATION:
	r = xs_buffer(" validation%s", see_validation(value));
	break;
    case XA_OUTLINING:
	r = xs_buffer(" outlining(%s)", see_outline(value));
	break;
    case XA_HIGHLIGHTING:
	r = xs_buffer(" highlighting(%s)", see_highlight(value));
	break;
    case XA_FOREGROUND:
	r = xs_buffer(" foreground(%s)", see_color(value));
	break;
    case XA_CHARSET:
	r = xs_buffer(" charset(%x)", value);
	break;
    case XA_BACKGROUND:
	r = xs_buffer(" background(%s)", see_color(value));
	break;
    case XA_TRANSPARENCY:
	r = xs_buffer(" transparency(%s)", see_transparency(value));
	break;
    case XA_INPUT_CONTROL:
	r = xs_buffer(" input-control(%s)", see_input_control(value));
	break;
    default:
	r = xs_buffer(" %s[0x%x]", unknown(efa, &urs), value);
	break;
    }

    Replace(rs, r);
    return r;
}

/**
 * Encode just an extended attribute.
 *
 * @param[in] efa	Attribute to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
const char *
see_efa_only(unsigned char efa)
{
    static char *rs = NULL;

    switch (efa) {
    case XA_ALL:
	return "all";
    case XA_3270:
	return "3270";
    case XA_VALIDATION:
	return "validation";
    case XA_OUTLINING:
	return "outlining";
    case XA_HIGHLIGHTING:
	return "highlighting";
    case XA_FOREGROUND:
	return "foreground";
    case XA_CHARSET:
	return "charset";
    case XA_BACKGROUND:
	return "background";
    case XA_TRANSPARENCY:
	return "transparency";
    default:
	return unknown(efa, &rs);
    }
}

/**
 * Encode a query reply code.
 *
 * @param[in] id	Code to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
const char *
see_qcode(unsigned char id)
{
    static char *rs = NULL;

    switch (id) {
    case QR_CHARSETS:
	return "CharacterSets";
    case QR_IMP_PART:
	return "ImplicitPartition";
    case QR_SUMMARY:
	return "Summary";
    case QR_USABLE_AREA:
	return "UsableArea";
    case QR_COLOR:
	return "Color";
    case QR_HIGHLIGHTING:
	return "Highlighting";
    case QR_REPLY_MODES:
	return "ReplyModes";
    case QR_DBCS_ASIA:
	return "DbcsAsia";
    case QR_ALPHA_PART:
	return "AlphanumericPartitions";
    case QR_DDM:
	return "DistributedDataManagement";
    case QR_RPQNAMES:
	return "RPQNames";
    default:
	return unknown(id, &rs);
    }
}
