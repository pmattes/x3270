/*
 * Copyright (c) 1993-2024 Paul Mattes.
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

#include "utf8.h"
#include "utils.h"
#include "see.h"
#include "txa.h"
#include "unicodec.h"
#include "varbuf.h"

/**
 * Return an unknown value.
 *
 * @param[in] value	The value to encode
 *
 * @return Encoded text representation of the value, good for only one call.
 */
static const char *
unknown(unsigned char value)
{
    return txAsprintf("unknown[0x%x]", value);
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
	return txdFree(NewString(mb));
    } else {
	return txAsprintf("X'%02X'", ch);
    }
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
	return unknown(code);
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

    return txdFree(vb_consume(&r));
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

    if (setting == XAH_DEFAULT) {
	return "default";
    }
    if ((setting & 0xf0) != 0xf0) {
	return unknown(setting);
    }
    {
	varbuf_t r;
	char *sep = "";

	vb_init(&r);
	if ((setting & XAH_BLINK) == XAH_BLINK) {
	    vb_appendf(&r, "%s%s", sep, "blink");
	    sep = ",";
	}
	if ((setting & XAH_REVERSE) == XAH_REVERSE) {
	    vb_appendf(&r, "%s%s", sep, "reverse");
	    sep = ",";
	}
	if ((setting & XAH_UNDERSCORE) == XAH_UNDERSCORE) {
	    vb_appendf(&r, "%s%s", sep, "underscore");
	    sep = ",";
	}
	if ((setting & XAH_INTENSIFY) == XAH_INTENSIFY) {
	    vb_appendf(&r, "%s%s", sep, "intensify");
	    sep = ",";
	}

	return txdFree(vb_consume(&r));
    }
}

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
    "white",
    NULL
};

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
    if (setting == XAC_DEFAULT) {
	return "default";
    } else if (setting < 0xf0) {
	return unknown(setting);
    } else {
	return color_name[setting - 0xf0];
    }
}

/**
 * Decode a host color name or index.
 *
 * @param[in] name	Color name or index
 *
 * @return Color index (HOST_COLOR_xxx) or -1.
 */
int
decode_host_color(const char *name)
{
    int i;
    unsigned long l;
    char *ptr;

    /* Check for invalid parameter. */
    if (name == NULL || !*name) {
	return -1;
    }

    /* Check for a symbolic match. */
    for (i = 0; color_name[i] != NULL; i++) {
	if (!strcasecmp(name, color_name[i])) {
	    return i;
	}
    }

    /* Check for a number. */
    l = strtoul(name, &ptr, 0);
    if (l > 0xf || ptr == name || *ptr != '\0') {
	return -1;
    }
    return (int)l;
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
	return unknown(setting);
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
    return txdFree(vb_consume(&r));
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
    return txdFree(vb_consume(&r));
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
    switch (setting) {
    case XAI_DISABLED:
	return "disabled";
    case XAI_ENABLED:
	return "enabled";
    default:
	return unknown(setting);
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
    switch (efa) {
    case XA_ALL:
	return txAsprintf(" all(%x)", value);
    case XA_3270:
	return txAsprintf(" 3270%s", see_attr(value));
    case XA_VALIDATION:
	return txAsprintf(" validation%s", see_validation(value));
    case XA_OUTLINING:
	return txAsprintf(" outlining(%s)", see_outline(value));
    case XA_HIGHLIGHTING:
	return txAsprintf(" highlighting(%s)", see_highlight(value));
    case XA_FOREGROUND:
	return txAsprintf(" foreground(%s)", see_color(value));
    case XA_CHARSET:
	return txAsprintf(" charset(%x)", value);
    case XA_BACKGROUND:
	return txAsprintf(" background(%s)", see_color(value));
    case XA_TRANSPARENCY:
	return txAsprintf(" transparency(%s)", see_transparency(value));
    case XA_INPUT_CONTROL:
	return txAsprintf(" input-control(%s)", see_input_control(value));
    default:
	return txAsprintf(" %s[0x%x]", unknown(efa), value);
    }
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
	return unknown(efa);
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
	return unknown(id);
    }
}
