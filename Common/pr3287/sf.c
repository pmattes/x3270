/*
 * Copyright (c) 1994-2009, 2013-2014 Paul Mattes.
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
 *	sf.c
 *		This module handles 3270 structured fields.
 *
 */

#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include "globals.h"
#include "3270ds.h"
#include <string.h>

#include "ctlrc.h"
#if !defined(PR3287) /*[*/
# include "ft_dft.h"
#endif /*]*/
#include "sf.h"
#if defined(_WIN32) /*[*/
# include "ws2tcpip.h"
#else /*][*/
# include <sys/socket.h>
#endif /*]*/
#include "telnet_core.h"
#include "trace.h"

/* Statics */
static bool  qr_in_progress = false;
static enum pds sf_read_part(unsigned char buf[], unsigned buflen);
static enum pds sf_erase_reset(unsigned char buf[], int buflen);
static enum pds sf_set_reply_mode(unsigned char buf[], int buflen);
static enum pds sf_outbound_ds(unsigned char buf[], int buflen);
static void query_reply_start(void);
static void do_query_reply(unsigned char code);
static void query_reply_end(void);

/* Some permanent substitutions. */
#define maxROWS 72
#define maxCOLS 66
#define char_width 10
#define char_height 20
#define standard_font 0

static unsigned char supported_replies[] = {
    QR_SUMMARY,		/* 0x80 */
    QR_USABLE_AREA,	/* 0x81 */
    QR_ALPHA_PART,	/* 0x84 */
    QR_CHARSETS,	/* 0x85 */
    QR_COLOR,		/* 0x86 */
    QR_HIGHLIGHTING,	/* 0x87 */
    QR_REPLY_MODES,	/* 0x88 */
    QR_DBCS_ASIA,	/* 0x91 */
    QR_IMP_PART,	/* 0xa6 */
    QR_DDM,		/* 0x95 */
};
#define NSR	(sizeof(supported_replies)/sizeof(unsigned char))


/*
 * Process a 3270 Write Structured Field command
 */
enum pds
write_structured_field(unsigned char buf[], size_t buflen)
{
    size_t fieldlen;
    unsigned char *cp = buf;
    bool first = true;
    enum pds rv = PDS_OKAY_NO_OUTPUT;
    enum pds rv_this = PDS_OKAY_NO_OUTPUT;
    bool bad_cmd = false;

    /* Skip the WSF command itself. */
    cp++;
    buflen--;

    /* Interpret fields. */
    while (buflen > 0) {

	if (first) {
	    trace_ds(" ");
	} else {
	    trace_ds("< WriteStructuredField ");
	}
	first = false;

	/* Pick out the field length. */
	if (buflen < 2) {
	    trace_ds("error: single byte at end of message\n");
	    return rv ? rv : PDS_BAD_CMD;
	}
	fieldlen = (cp[0] << 8) + cp[1];
	if (fieldlen == 0) {
	    fieldlen = buflen;
	}
	if (fieldlen < 3) {
	    trace_ds("error: field length %d too small\n", (int)fieldlen);
	    return rv ? rv : PDS_BAD_CMD;
	}
	if (fieldlen > buflen) {
	    trace_ds("error: field length %d exceeds remaining "
		    "message length %d\n",
		    (int)fieldlen, (int)buflen);
	    return rv ? rv : PDS_BAD_CMD;
	}

	/* Dispatch on the ID. */
	switch (cp[2]) {
	case SF_READ_PART:
	    trace_ds("ReadPartition");
	    rv_this = sf_read_part(cp, (int)fieldlen);
	    break;
	case SF_ERASE_RESET:
	    trace_ds("EraseReset");
	    rv_this = sf_erase_reset(cp, (int)fieldlen);
	    break;
	case SF_SET_REPLY_MODE:
	    trace_ds("SetReplyMode");
	    rv_this = sf_set_reply_mode(cp, (int)fieldlen);
	    break;
	case SF_OUTBOUND_DS:
	    trace_ds("OutboundDS");
	    rv_this = sf_outbound_ds(cp, (int)fieldlen);
	    break;
#if !defined(PR3287) /*[*/
	case SF_TRANSFER_DATA:   /* File transfer data         */
	    trace_ds("FileTransferData");
	    ft_dft_data(cp, (int)fieldlen);
	    break;
#endif /*]*/
	default:
	    trace_ds("unsupported ID 0x%02x\n", cp[2]);
	    rv_this = PDS_BAD_CMD;
	    break;
	}

	/*
	 * Accumulate errors or output flags.
	 * One real ugliness here is that if we have already
	 * generated some output, then we have already positively
	 * acknowledged the request, so if we fail here, we have no
	 * way to return the error indication.
	 */
	if (rv_this < 0) {
	    bad_cmd = true;
	} else {
	    rv |= rv_this;
	}

	/* Skip to the next field. */
	cp += fieldlen;
	buflen -= fieldlen;
    }
    if (first) {
	trace_ds(" (null)\n");
    }

    if (bad_cmd && !rv) {
	return PDS_BAD_CMD;
    } else {
	return rv;
    }
}

static enum pds
sf_read_part(unsigned char buf[], unsigned buflen)
{
    unsigned char partition;
    unsigned i;
    int any = 0;
    const char *comma = "";

    if (buflen < 5) {
	trace_ds(" error: field length %d too small\n", buflen);
	return PDS_BAD_CMD;
    }

    partition = buf[3];
    trace_ds("(0x%02x)", partition);

    switch (buf[4]) {
    case SF_RP_QUERY:
	trace_ds(" Query");
	if (partition != 0xff) {
	    trace_ds(" error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
	trace_ds("\n");
	query_reply_start();
	for (i = 0; i < NSR; i++) {
	    if (dbcs || supported_replies[i] != QR_DBCS_ASIA) {
		do_query_reply(supported_replies[i]);
	    }
	}
	query_reply_end();
	break;
    case SF_RP_QLIST:
	trace_ds(" QueryList ");
	if (partition != 0xff) {
	    trace_ds("error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
	if (buflen < 6) {
	    trace_ds("error: missing request type\n");
	    return PDS_BAD_CMD;
	}
	query_reply_start();
	switch (buf[5]) {
	case SF_RPQ_LIST:
	    trace_ds("List(");
	    if (buflen < 7) {
		trace_ds(")\n");
		do_query_reply(QR_NULL);
	    } else {
		for (i = 6; i < buflen; i++) {
		    trace_ds("%s%s", comma, see_qcode(buf[i]));
		    comma = ",";
		}
		trace_ds(")\n");
		for (i = 0; i < NSR; i++) {
		    if (memchr((char *)&buf[6], (char)supported_replies[i],
				buflen - 6) &&
			    (dbcs || supported_replies[i] != QR_DBCS_ASIA)) {
			do_query_reply(supported_replies[i]);
			any++;
		    }
		}
		if (!any) {
		    do_query_reply(QR_NULL);
		}
	    }
	    break;
	case SF_RPQ_EQUIV:
	    trace_ds("Equivlent+List(");
	    for (i = 6; i < buflen; i++) {
		trace_ds("%s%s", comma, see_qcode(buf[i]));
		comma = ",";
	    }
	    trace_ds(")\n");
	    for (i = 0; i < NSR; i++) {
		if (dbcs || supported_replies[i] != QR_DBCS_ASIA) {
		    do_query_reply(supported_replies[i]);
		}
	    }
	    break;
	case SF_RPQ_ALL:
	    trace_ds("All\n");
	    for (i = 0; i < NSR; i++) {
		if (dbcs || supported_replies[i] != QR_DBCS_ASIA) {
		    do_query_reply(supported_replies[i]);
		}
	    }
	    break;
	default:
	    trace_ds("unknown request type 0x%02x\n", buf[5]);
	    return PDS_BAD_CMD;
	}
	query_reply_end();
	break;
    case SNA_CMD_RMA:
	trace_ds(" ReadModifiedAll");
	if (partition != 0x00) {
	    trace_ds(" error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
	trace_ds("\n");
	return PDS_BAD_CMD;
	break;
    case SNA_CMD_RB:
	trace_ds(" ReadBuffer");
	if (partition != 0x00) {
	    trace_ds(" error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
	trace_ds("\n");
	return PDS_BAD_CMD;
	break;
    case SNA_CMD_RM:
	trace_ds(" ReadModified");
	if (partition != 0x00) {
	    trace_ds(" error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
	trace_ds("\n");
	return PDS_BAD_CMD;
	break;
    default:
	trace_ds(" unknown type 0x%02x\n", buf[4]);
	return PDS_BAD_CMD;
    }

    return PDS_OKAY_OUTPUT;
}

static enum pds
sf_erase_reset(unsigned char buf[], int buflen)
{
    if (buflen != 4) {
	trace_ds(" error: wrong field length %d\n", buflen);
	return PDS_BAD_CMD;
    }

    switch (buf[3]) {
	case SF_ER_DEFAULT:
	    trace_ds(" Default\n");
	    break;
	case SF_ER_ALT:
	    trace_ds(" Alternate\n");
	    break;
	default:
	    trace_ds(" unknown type 0x%02x\n", buf[3]);
	    return PDS_BAD_CMD;
    }

    return PDS_OKAY_NO_OUTPUT;
}

static enum pds
sf_set_reply_mode(unsigned char buf[], int buflen)
{
    unsigned char partition;

    if (buflen < 5) {
	trace_ds(" error: wrong field length %d\n", buflen);
	return PDS_BAD_CMD;
    }

    partition = buf[3];
    trace_ds("(0x%02x)", partition);
    if (partition != 0x00) {
	trace_ds(" error: illegal partition\n");
	return PDS_BAD_CMD;
    }

    switch (buf[4]) {
    case SF_SRM_FIELD:
	trace_ds(" Field\n");
	break;
    case SF_SRM_XFIELD:
	trace_ds(" ExtendedField\n");
	break;
    case SF_SRM_CHAR:
	trace_ds(" Character");
	break;
    default:
	trace_ds(" unknown mode 0x%02x\n", buf[4]);
	return PDS_BAD_CMD;
    }

    return PDS_OKAY_NO_OUTPUT;
}

static enum pds
sf_outbound_ds(unsigned char buf[], int buflen)
{
    if (buflen < 5) {
	trace_ds(" error: field length %d too short\n", buflen);
	return PDS_BAD_CMD;
    }

    trace_ds("(0x%02x)", buf[3]);
    if (buf[3] != 0x00) {
	trace_ds(" error: illegal partition 0x%0x\n", buf[3]);
	return PDS_BAD_CMD;
    }

    switch (buf[4]) {
    case SNA_CMD_W:
	trace_ds(" Write");
	if (buflen > 5) {
	    ctlr_write(&buf[4], buflen-4, false);
	} else {
	    trace_ds("\n");
	}
	break;
    case SNA_CMD_EW:
	trace_ds(" EraseWrite");
	if (buflen > 5) {
	    ctlr_write(&buf[4], buflen-4, true);
	} else {
	    trace_ds("\n");
	}
	break;
    case SNA_CMD_EWA:
	trace_ds(" EraseWriteAlternate");
	if (buflen > 5) {
	    ctlr_write(&buf[4], buflen-4, true);
	} else {
	    trace_ds("\n");
	}
	break;
    case SNA_CMD_EAU:
	trace_ds(" EraseAllUnprotected\n");
	break;
    default:
	trace_ds(" unknown type 0x%02x\n", buf[4]);
	return PDS_BAD_CMD;
    }

    return PDS_OKAY_NO_OUTPUT;
}

static void
query_reply_start(void)
{
    obptr = obuf;
    space3270out(1);
    *obptr++ = AID_SF;
    qr_in_progress = true;
}

static void
do_query_reply(unsigned char code)
{
    size_t len;
    unsigned i;
    const char *comma = "";
    size_t obptr0 = obptr - obuf;
    unsigned char *obptr_len;
    unsigned short num, denom;

    if (qr_in_progress) {
	trace_ds("> StructuredField\n");
	qr_in_progress = false;
    }

    space3270out(4);
    obptr += 2;	/* skip length for now */
    *obptr++ = SFID_QREPLY;
    *obptr++ = code;
    switch (code) {

    case QR_CHARSETS:
	trace_ds("> QueryReply(CharacterSets)\n");
	space3270out(64);
	if (dbcs) {
	    *obptr++ = 0x8e;	/* flags: GE, CGCSGID, DBCS */
	} else {
	    *obptr++ = 0x82;	/* flags: GE, CGCSGID present */
	}
	*obptr++ = 0x00;		/* more flags */
	*obptr++ = char_width;		/* SDW */
	*obptr++ = char_height;		/* SDW */
	*obptr++ = 0x00;		/* no load PS */
	*obptr++ = 0x00;
	*obptr++ = 0x00;
	*obptr++ = 0x00;
	if (dbcs) {
	    *obptr++ = 0x0b;	/* DL (11 bytes) */
	} else {
	    *obptr++ = 0x07;	/* DL (7 bytes) */
	}

	*obptr++ = 0x00;		/* SET 0: */
	if (dbcs) {
	    *obptr++ = 0x00;	/*  FLAGS: non-load, single- */
	}				/*   plane, single-bute */
	else {
	    *obptr++ = 0x10;	/*  FLAGS: non-loadable, */
	}				/*    single-plane, single-byte,
					    no compare */
	*obptr++ = 0x00;		/*  LCID 0 */
	if (dbcs) {
	    *obptr++ = 0x00;	/*  SW 0 */
	    *obptr++ = 0x00;	/*  SH 0 */
	    *obptr++ = 0x00;	/*  SUBSN */
	    *obptr++ = 0x00;	/*  SUBSN */
	}
	SET32(obptr, cgcsgid);	/*  CGCSGID */
	if (!standard_font) {
	    /* special 3270 font, includes APL */
	    *obptr++ = 0x01;/* SET 1: */
	    *obptr++ = 0x10;/*  FLAGS: non-loadable, single-plane,
				 single-byte, no compare */
	    *obptr++ = 0xf1;/*  LCID */
	    if (dbcs) {
		*obptr++ = 0x00;/*  SW 0 */
		*obptr++ = 0x00;/*  SH 0 */
		*obptr++ = 0x00;/*  SUBSN */
		*obptr++ = 0x00;/*  SUBSN */
	    }
	    *obptr++ = 0x03;/*  CGCSGID: 3179-style APL2 */
	    *obptr++ = 0xc3;
	    *obptr++ = 0x01;
	    *obptr++ = 0x36;
	}
	if (dbcs) {
	    *obptr++ = 0x80;	/* SET 0x80: */
	    *obptr++ = 0x20;	/*  FLAGS: DBCS */
	    *obptr++ = 0xf8;	/*  LCID: 0xf8 */
	    *obptr++ = char_width * 2; /* SW */
	    *obptr++ = char_height; /* SH */
	    *obptr++ = 0x41;	/*  SUBSN */
	    *obptr++ = 0x7f;	/*  SUBSN */
	    SET32(obptr, cgcsgid_dbcs); /* CGCSGID */
	}
	break;

    case QR_IMP_PART:
	trace_ds("> QueryReply(ImplicitPartition)\n");
	space3270out(13);
	*obptr++ = 0x0;		/* reserved */
	*obptr++ = 0x0;
	*obptr++ = 0x0b;	/* length of display size */
	*obptr++ = 0x01;	/* "implicit partition size" */
	*obptr++ = 0x00;	/* reserved */
	SET16(obptr, 72);	/* implicit partition width */
	SET16(obptr, 66);	/* implicit partition height */
	SET16(obptr, maxCOLS);	/* alternate height */
	SET16(obptr, maxROWS);	/* alternate width */
	break;

    case QR_NULL:
	trace_ds("> QueryReply(Null)\n");
	break;

    case QR_SUMMARY:
	trace_ds("> QueryReply(Summary(");
	space3270out(NSR);
	for (i = 0; i < NSR; i++) {
	    if (dbcs || supported_replies[i] != QR_DBCS_ASIA) {
		trace_ds("%s%s", comma,
			see_qcode(supported_replies[i]));
		comma = ",";
		*obptr++ = supported_replies[i];
	    }
	}
	trace_ds("))\n");
	break;

    case QR_USABLE_AREA:
	trace_ds("> QueryReply(UsableArea)\n");
	space3270out(19);
	*obptr++ = 0x01;	/* 12/14-bit addressing */
	*obptr++ = 0x00;	/* no special character features */
	SET16(obptr, maxCOLS);	/* usable width */
	SET16(obptr, maxROWS);	/* usable height */
	*obptr++ = 0x01;	/* units (mm) */
	num = /*display_widthMM()*/ 8 * 5 / 4;
	denom = /*display_width()*/ 7 * 72;
	while (!(num % 2) && !(denom % 2)) {
	    num /= 2;
	    denom /= 2;
	}
	SET16(obptr, (int)num);	/* Xr numerator */
	SET16(obptr, (int)denom); /* Xr denominator */
	num = /*display_heightMM()*/ 11 * 5 / 4;
	denom = /*display_height()*/ 9 * 66;
	while (!(num % 2) && !(denom % 2)) {
	    num /= 2;
	    denom /= 2;
	}
	SET16(obptr, (int)num);	/* Yr numerator */
	SET16(obptr, (int)denom); /* Yr denominator */
	*obptr++ = char_width;	/* AW */
	*obptr++ = char_height;	/* AH */
	SET16(obptr, 0);	/* buffer */
	break;

    case QR_COLOR:
	trace_ds("> QueryReply(Color)\n");
	space3270out(4 + 2*15);
	*obptr++ = 0x00;	/* no options */
	*obptr++ = 16;		/* report on 16 colors */
	*obptr++ = 0x00;	/* default color: */
	*obptr++ = 0xf0 + HOST_COLOR_GREEN;	/*  green */
	for (i = 0xf1; i <= 0xff; i++) {
	    *obptr++ = i;
	    *obptr++ = i;
	}
	break;

    case QR_HIGHLIGHTING:
	trace_ds("> QueryReply(Highlighting)\n");
	space3270out(11);
	*obptr++ = 5;		/* report on 5 pairs */
	*obptr++ = XAH_DEFAULT;	/* default: */
	*obptr++ = XAH_NORMAL;	/*  normal */
	*obptr++ = XAH_BLINK;	/* blink: */
	*obptr++ = XAH_BLINK;	/*  blink */
	*obptr++ = XAH_REVERSE;	/* reverse: */
	*obptr++ = XAH_REVERSE;	/*  reverse */
	*obptr++ = XAH_UNDERSCORE; /* underscore: */
	*obptr++ = XAH_UNDERSCORE; /*  underscore */
	*obptr++ = XAH_INTENSIFY; /* intensify: */
	*obptr++ = XAH_INTENSIFY; /*  intensify */
	break;

    case QR_REPLY_MODES:
	trace_ds("> QueryReply(ReplyModes)\n");
	space3270out(3);
	*obptr++ = SF_SRM_FIELD;
	*obptr++ = SF_SRM_XFIELD;
	*obptr++ = SF_SRM_CHAR;
	break;

    case QR_DBCS_ASIA:
	/* XXX: Should we support this, even when not in DBCS mode? */
	trace_ds("> QueryReply(DbcsAsia)\n");
	space3270out(7);
	*obptr++ = 0x00;	/* flags (none) */
	*obptr++ = 0x03;	/* field length 3 */
	*obptr++ = 0x01;	/* SI/SO supported */
	*obptr++ = 0x80;	/* character set ID 0x80 */
	*obptr++ = 0x03;	/* field length 3 */
	*obptr++ = 0x02;	/* input control */
	*obptr++ = 0x01;	/* creation supported */
	break;

    case QR_ALPHA_PART:
	trace_ds("> QueryReply(AlphanumericPartitions)\n");
	space3270out(4);
	*obptr++ = 0;		/* 1 partition */
	SET16(obptr, maxROWS*maxCOLS);	/* buffer space */
	*obptr++ = 0;		/* no special features */
	break;

    case QR_DDM:
	trace_ds("> QueryReply(DistributedDataManagement)\n");
	space3270out(8);
	SET16(obptr,0);		/* set reserved field to 0 */
	SET16(obptr,2048);	/* set inbound length limit */
	SET16(obptr,2048);	/* set outbound length limit */
	SET16(obptr,0x0101);	/* NSS=01, DDMSS=01 */
	break;

    default:
	return;	/* internal error */
    }

    obptr_len = obuf + obptr0;
    len = (obptr - obuf) - obptr0;
    SET16(obptr_len, len);
}

static void
query_reply_end(void)
{
    net_output();
}
