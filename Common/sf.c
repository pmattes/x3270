/*
 * Copyright (c) 1994-2024 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
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
 *     * Neither the names of Paul Mattes, Don Russell nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DON RUSSELL "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DON RUSSELL
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	sf.c
 *		This module handles 3270 structured fields.
 *
 */

#include "globals.h"
#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"

#include "codepage.h"
#include "ctlrc.h"
#include "ft_dft.h"
#include "ft_private.h"
#include "kybd.h"
#include "sf.h"	/* has to come before rpq.h */
#include "rpq.h"
#include "screen.h"
#include "see.h"
#include "telnet_core.h"
#include "trace.h"

#define SW_3279_2	0x09
#define SH_3279_2	0x0c
#define Xr_3279_2	0x000a02e5
#define Yr_3279_2	0x0002006f

/* Statics */
static bool qr_in_progress = false;
static enum pds sf_read_part(unsigned char buf[], unsigned buflen);
static enum pds sf_erase_reset(unsigned char buf[], int buflen);
static enum pds sf_set_reply_mode(unsigned char buf[], int buflen);
static enum pds sf_create_partition(unsigned char buf[], int buflen);
static enum pds sf_outbound_ds(unsigned char buf[], int buflen);
static void query_reply_start(void);
static void do_query_reply(unsigned char code);
static void query_reply_end(void);

typedef bool qr_multi_fn_t(unsigned *subindex, bool *more);

static qr_single_fn_t do_qr_summary, do_qr_usable_area, do_qr_alpha_part,
		      do_qr_charsets, do_qr_color, do_qr_highlighting,
		      do_qr_reply_modes, do_qr_imp_part, do_qr_null,
		      do_qr_dbcs_asia, do_qr_ddm;

static struct reply {
    unsigned char code;
    qr_single_fn_t *single_fn;
    qr_multi_fn_t *multi_fn;
} replies[] = {
    { QR_SUMMARY,      do_qr_summary,      NULL },		/* 0x80 */
    { QR_USABLE_AREA,  do_qr_usable_area,  NULL },		/* 0x81 */
    { QR_ALPHA_PART,   do_qr_alpha_part,   NULL },		/* 0x84 */
    { QR_CHARSETS,     do_qr_charsets,     NULL },		/* 0x85 */
    { QR_COLOR,        do_qr_color,        NULL },		/* 0x86 */
    { QR_HIGHLIGHTING, do_qr_highlighting, NULL },		/* 0x87 */
    { QR_REPLY_MODES,  do_qr_reply_modes,  NULL },		/* 0x88 */
    { QR_DBCS_ASIA,    do_qr_dbcs_asia,    NULL },		/* 0x91 */
    { QR_DDM,          do_qr_ddm,          NULL },		/* 0x95 */
    { QR_RPQNAMES,     do_qr_rpqnames,     NULL },		/* 0xa1 */
    { QR_IMP_PART,     do_qr_imp_part,     NULL },		/* 0xa6 */

    /* QR_NULL must be last in the table */
    { QR_NULL,         do_qr_null,         NULL },		/* 0xff */
};

/*
 * NSR_ALL is the number of query replies supported, including NULL.
 * NSR is the number of query replies supported, except for NULL.
 */
#define NSR_ALL	(sizeof(replies)/sizeof(struct reply))
#define NSR	(NSR_ALL - 1)

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
	    trace_ds("error: field length %d too small\n",
		    (int)fieldlen);
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
	case SF_CREATE_PART:
	    trace_ds("CreatePartition");
	    rv_this = sf_create_partition(cp, (int)fieldlen);
	    break;
	case SF_OUTBOUND_DS:
	    trace_ds("OutboundDS");
	    rv_this = sf_outbound_ds(cp, (int)fieldlen);
	    break;
	case SF_TRANSFER_DATA:
	    trace_ds("FileTransferData");
	    ft_dft_data(cp, (int)fieldlen);
	    break;
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
	    if (dbcs || replies[i].code != QR_DBCS_ASIA) {
		do_query_reply(replies[i].code);
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
		    if (memchr(&buf[6], (char)replies[i].code, buflen-6) &&
			(dbcs || replies[i].code != QR_DBCS_ASIA)) {
			do_query_reply(replies[i].code);
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
		if (dbcs || replies[i].code != QR_DBCS_ASIA) {
		    do_query_reply(replies[i].code);
		}
	    }
	    break;
	case SF_RPQ_ALL:
	    trace_ds("All\n");
	    for (i = 0; i < NSR; i++) {
		if (dbcs || replies[i].code != QR_DBCS_ASIA) {
		    do_query_reply(replies[i].code);
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
	ctlr_read_modified(AID_QREPLY, true);
	break;
    case SNA_CMD_RB:
	trace_ds(" ReadBuffer");
	if (partition != 0x00) {
	    trace_ds(" error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
	trace_ds("\n");
	ctlr_read_buffer(AID_QREPLY);
	break;
    case SNA_CMD_RM:
	trace_ds(" ReadModified");
	if (partition != 0x00) {
	    trace_ds(" error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
	trace_ds("\n");
	ctlr_read_modified(AID_QREPLY, false);
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
	ctlr_erase(false);
	break;
    case SF_ER_ALT:
	trace_ds(" Alternate\n");
	ctlr_erase(true);
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
    int i;
    const char *comma = "(";

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
    reply_mode = buf[4];
    if (buf[4] == SF_SRM_CHAR) {
	crm_nattr = buflen - 5;
	for (i = 5; i < buflen; i++) {
	    crm_attr[i - 5] = buf[i];
	    trace_ds("%s%s", comma, see_efa_only(buf[i]));
	    comma = ",";
	}
	trace_ds("%s\n", crm_nattr ? ")" : "");
    }
    return PDS_OKAY_NO_OUTPUT;
}

static enum pds
sf_create_partition(unsigned char buf[], int buflen)
{
    unsigned char pid;
    unsigned char uom;		/* unit of measure */
    unsigned char am;		/* addressing mode */
    unsigned char flags;		/* flags */
    unsigned short h;		/* height of presentation space */
    unsigned short w;		/* width of presentation space */
    unsigned short rv;		/* viewport origin row */
    unsigned short cv;		/* viewport origin column */
    unsigned short hv;		/* viewport height */
    unsigned short wv;		/* viewport width */
    unsigned short rw;		/* window origin row */
    unsigned short cw;		/* window origin column */
    unsigned short rs;		/* scroll rows */
    /* hole */
    unsigned short pw;		/* character cell point width */
    unsigned short ph;		/* character cell point height */

    static const char *bit4[16] = {
	"0000", "0001", "0010", "0011",
	"0100", "0101", "0110", "0111",
	"1000", "1001", "1010", "1011",
	"1100", "1101", "1110", "1111"
    };

    if (buflen > 3) {
	trace_ds("(");

	/* Partition. */
	pid = buf[3];
	trace_ds("pid=0x%02x", pid);
	if (pid != 0x00) {
	    trace_ds(") error: illegal partition\n");
	    return PDS_BAD_CMD;
	}
    } else {
	pid = 0x00;
    }

    if (buflen > 4) {
	uom = (buf[4] & 0xf0) >> 4;
	trace_ds(",uom=B'%s'", bit4[uom]);
	if (uom != 0x0 && uom != 0x02) {
	    trace_ds(") error: illegal units\n");
	    return PDS_BAD_CMD;
	}
	am = buf[4] & 0x0f;
	trace_ds(",am=B'%s'", bit4[am]);
	if (am > 0x2) {
	    trace_ds(") error: illegal a-mode\n");
	    return PDS_BAD_CMD;
	}
    } else {
	uom = 0;
	am = 0;
    }

    if (buflen > 5) {
	flags = buf[5];
	trace_ds(",flags=0x%02x", flags);
    } else {
	flags = 0;
    }

    if (buflen > 7) {
	GET16(h, &buf[6]);
	trace_ds(",h=%d", h);
    } else {
	h = maxROWS;
    }

    if (buflen > 9) {
	GET16(w, &buf[8]);
	trace_ds(",w=%d", w);
    } else {
	w = maxCOLS;
    }

    if (buflen > 11) {
	GET16(rv, &buf[10]);
	trace_ds(",rv=%d", rv);
    } else {
	rv = 0;
    }

    if (buflen > 13) {
	GET16(cv, &buf[12]);
	trace_ds(",cv=%d", cv);
    } else {
	cv = 0;
    }

    if (buflen > 15) {
	GET16(hv, &buf[14]);
	trace_ds(",hv=%d", hv);
    } else {
	hv = (h > maxROWS)? maxROWS: h;
    }

    if (buflen > 17) {
	GET16(wv, &buf[16]);
	trace_ds(",wv=%d", wv);
    } else {
	wv = (w > maxCOLS)? maxCOLS: w;
    }

    if (buflen > 19) {
	GET16(rw, &buf[18]);
	trace_ds(",rw=%d", rw);
    } else {
	rw = 0;
    }

    if (buflen > 21) {
	GET16(cw, &buf[20]);
	trace_ds(",cw=%d", cw);
    } else {
	cw = 0;
    }

    if (buflen > 23) {
	GET16(rs, &buf[22]);
	trace_ds(",rs=%d", rs);
    } else {
	rs = (h > hv)? 1: 0;
    }

    if (buflen > 27) {
	GET16(pw, &buf[26]);
	trace_ds(",pw=%d", pw);
    } else {
	pw = *char_width;
    }

    if (buflen > 29) {
	GET16(ph, &buf[28]);
	trace_ds(",ph=%d", ph);
    } else {
	ph = *char_height;
    }
    trace_ds(")\n");

    cursor_move(0);
    buffer_addr = 0;

    return PDS_OKAY_NO_OUTPUT;
}

static enum pds
sf_outbound_ds(unsigned char buf[], int buflen)
{
    enum pds rv;

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
	    if ((rv = ctlr_write(&buf[4], buflen-4, false)) < 0) {
		return rv;
	    }
	} else {
	    trace_ds("\n");
	}
	break;
    case SNA_CMD_EW:
	trace_ds(" EraseWrite");
	ctlr_erase(screen_alt);
	if (buflen > 5) {
	    if ((rv = ctlr_write(&buf[4], buflen-4, true)) < 0)
		return rv;
	} else {
	    trace_ds("\n");
	}
	break;
    case SNA_CMD_EWA:
	trace_ds(" EraseWriteAlternate");
	ctlr_erase(screen_alt);
	if (buflen > 5) {
	    if ((rv = ctlr_write(&buf[4], buflen-4, true)) < 0) {
		return rv;
	    }
	} else {
	    trace_ds("\n");
	}
	break;
    case SNA_CMD_EAU:
	trace_ds(" EraseAllUnprotected\n");
	ctlr_erase_all_unprotected();
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
    unsigned i;
    unsigned subindex = 0;
    bool more = false;

    /* Find the right entry in the reply table. */
    for (i = 0; i < NSR_ALL; i++) {
	if (replies[i].code == code) {
	    break;
	}
    }
    if (i >= NSR_ALL ||
	    (replies[i].single_fn == NULL && replies[i].multi_fn == NULL)) {
	return;
    }

    if (qr_in_progress) {
	trace_ds("> StructuredField\n");
	qr_in_progress = false;
    }

    do {
	size_t obptr0 = obptr - obuf;
	bool full = true;

	space3270out(4);
	obptr += 2;	/* skip length for now */
	*obptr++ = SFID_QREPLY;
	*obptr++ = code;

	more = false;
	if (replies[i].single_fn) {
	    replies[i].single_fn();
	} else {
	    full = replies[i].multi_fn(&subindex, &more);
	}

	if (full) {
	    size_t len;
	    unsigned char *obptr_len;

	    /* Fill in the length. */
	    obptr_len = obuf + obptr0;
	    len = (obptr - obuf) - obptr0;
	    SET16(obptr_len, len);
	} else {
	    /* Back over the header. */
	    obptr -= 4;
	}
    } while (more);
}

static void
do_qr_null(void)
{
    trace_ds("> QueryReply(Null)\n");
}

static void
do_qr_summary(void)
{
    unsigned i;
    const char *comma = "";

    trace_ds("> QueryReply(Summary(");
    space3270out(NSR);
    for (i = 0; i < NSR; i++) {
	if (dbcs || replies[i].code != QR_DBCS_ASIA) {
	    trace_ds("%s%s", comma, see_qcode(replies[i].code));
	    comma = ",";
	    *obptr++ = replies[i].code;
	}
    }
    trace_ds("))\n");
}

static void
do_qr_usable_area(void)
{
    trace_ds("> QueryReply(UsableArea)\n");
    space3270out(19);
    *obptr++ = 0x01;		/* 12/14-bit addressing */
    *obptr++ = 0x00;		/* no special character features */
    SET16(obptr, maxCOLS);	/* usable width */
    SET16(obptr, maxROWS);	/* usable height */
    *obptr++ = 0x01;		/* units (mm) */
    SET32(obptr, Xr_3279_2);	/* Xr, canned from 3279-2 */
    SET32(obptr, Yr_3279_2);	/* Yr, canned from 3279-2 */

				/*
				 * If we ever implement graphics, these will
				 * need to change.
				 */
    *obptr++ = SW_3279_2;	/* AW, canned from 3279-2 */
    *obptr++ = SH_3279_2;	/* AH, canned from 3279-2 */

    SET16(obptr, maxCOLS*maxROWS); /* buffer, questionable */
}

static void
do_qr_color(void)
{
    int i;
    int color_max = 16;

    trace_ds("> QueryReply(Color)\n");

    space3270out(4 + 2*15);
    *obptr++ = 0x00;		/* no options */
    *obptr++ = color_max;	/* 16 colors */
    *obptr++ = 0x00;		/* default color: */
    *obptr++ = 0xf0 + HOST_COLOR_GREEN;	/*  green */
    for (i = 0xf1; i < 0xf1 + color_max - 1; i++) {
	*obptr++ = i;
	if (mode3279) {
	    *obptr++ = i;
	} else {
	    *obptr++ = 0x00;
	}
    }

    if (screen_has_bg_color()) {
	/* Add background color. */
	if (mode3279 && appres.qr_bg_color) {
	    space3270out(4);
	    *obptr++ = 4;	/* length */
	    *obptr++ = 0x02;	/* background color */
	    *obptr++ = 0x00;	/* attribute */
	    *obptr++ = 0xf0;	/* default color */
	}
    }
}

static void
do_qr_highlighting(void)
{
    trace_ds("> QueryReply(Highlighting)\n");
    space3270out(11);
    *obptr++ = 5;		/* report on 5 pairs */
    *obptr++ = XAH_DEFAULT;	/* default: */
    *obptr++ = XAH_NORMAL;	/*  normal */
    *obptr++ = XAH_BLINK;	/* blink: */
    *obptr++ = XAH_BLINK;	/*  blink */
    *obptr++ = XAH_REVERSE;	/* reverse: */
    *obptr++ = XAH_REVERSE;	/*  reverse */
    *obptr++ = XAH_UNDERSCORE;	/* underscore: */
    *obptr++ = XAH_UNDERSCORE;	/*  underscore */
    *obptr++ = XAH_INTENSIFY;	/* intensify: */
    *obptr++ = XAH_INTENSIFY;	/*  intensify */
}

static void
do_qr_reply_modes(void)
{
    trace_ds("> QueryReply(ReplyModes)\n");
    space3270out(3);
    *obptr++ = SF_SRM_FIELD;
    *obptr++ = SF_SRM_XFIELD;
    *obptr++ = SF_SRM_CHAR;
}

static void
do_qr_dbcs_asia(void)
{
    /* XXX: Should we support this, even when not in DBCS mode? */
    trace_ds("> QueryReply(DbcsAsia)\n");
    space3270out(7);
    *obptr++ = 0x00;		/* flags (none) */
    *obptr++ = 0x03;		/* field length 3 */
    *obptr++ = 0x01;		/* SI/SO supported */
    *obptr++ = 0x80;		/* character set ID 0x80 */
    *obptr++ = 0x03;		/* field length 3 */
    *obptr++ = 0x02;		/* input control */
    *obptr++ = 0x01;		/* creation supported */
}

static void
do_qr_alpha_part(void)
{
    trace_ds("> QueryReply(AlphanumericPartitions)\n");
    space3270out(4);
    *obptr++ = 0;		/* 1 partition */
    SET16(obptr, maxROWS*maxCOLS); /* buffer space */
    *obptr++ = 0;		/* no special features */
}

static void
do_qr_charsets(void)
{
    trace_ds("> QueryReply(CharacterSets)\n");
    space3270out(64);
    if (dbcs) {
	*obptr++ = 0x8e;	/* flags: GE, CGCSGID, DBCS */
    } else {
	*obptr++ = 0x82;	/* flags: GE, CGCSGID present */
    }
    *obptr++ = 0x00;		/* more flags */
    *obptr++ = SW_3279_2;		/* SDW, canned from 3279-2 */
    *obptr++ = SH_3279_2;		/* SDW, canned from 3279-2 */
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
	*obptr++ = 0x00;	/*  FLAGS: non-load, single-
					plane, single-byte */
    } else {
	*obptr++ = 0x10;	/*  FLAGS: non-loadable,
					single-plane, single-byte,
					no compare */
    }
    *obptr++ = 0x00;		/*  LCID 0 */
    if (dbcs) {
	*obptr++ = 0x00;	/*  SW 0 */
	*obptr++ = 0x00;	/*  SH 0 */
	*obptr++ = 0x00;	/*  SUBSN */
	*obptr++ = 0x00;	/*  SUBSN */
    }
    SET32(obptr, cgcsgid);	/*  CGCSGID */

    /* special 3270 font, includes APL */
    *obptr++ = 0x01;		/* SET 1: */
    *obptr++ = 0x00;		/*  FLAGS: non-loadable, single-plane,
					single-byte, no compare */
    *obptr++ = 0xf1;		/*  LCID */
    if (dbcs) {
	*obptr++ = 0x00;	/*  SW 0 */
	*obptr++ = 0x00;	/*  SH 0 */
	*obptr++ = 0x00;	/*  SUBSN */
	*obptr++ = 0x00;	/*  SUBSN */
    }
    *obptr++ = 0x03;		/*  CGCSGID: 3179-style APL2 */
    *obptr++ = 0xc3;
    *obptr++ = 0x01;
    *obptr++ = 0x36;
    if (dbcs) {
	*obptr++ = 0x80;	/* SET 0x80: */
	*obptr++ = 0x20;	/*  FLAGS: DBCS */
	*obptr++ = 0xf8;	/*  LCID: 0xf8 */
	*obptr++ = SW_3279_2 * 2; /* SW, canned from 3279-2 */
	*obptr++ = SH_3279_2;	/* SH, canned from 3279-2 */
	*obptr++ = 0x41;	/*  SUBSN */
	*obptr++ = 0x7f;	/*  SUBSN */
	SET32(obptr, cgcsgid_dbcs); /* CGCSGID */
    }
}

static void
do_qr_ddm(void)
{
    int size;

    if (ftc != NULL) {
	size = ftc->dft_buffersize;
    } else {
	size = set_dft_buffersize(0);
    }

    trace_ds("> QueryReply(DistributedDataManagement INLIM/OUTLIM=%d)\n",
	    size);
    space3270out(8);
    SET16(obptr,0);		/* set reserved field to 0 */
    SET16(obptr, size);		/* set inbound length limit INLIM */
    SET16(obptr, size);		/* set outbound length limit OUTLIM */
    SET16(obptr, 0x0101);	/* NSS=01, DDMSS=01 */
}

static void
do_qr_imp_part(void)
{
    trace_ds("> QueryReply(ImplicitPartition)\n");
    space3270out(13);
    *obptr++ = 0x0;		/* reserved */
    *obptr++ = 0x0;
    *obptr++ = 0x0b;		/* length of display size */
    *obptr++ = 0x01;		/* "implicit partition size" */
    *obptr++ = 0x00;		/* reserved */
    SET16(obptr, 80);		/* implicit partition width */
    SET16(obptr, 24);		/* implicit partition height */
    SET16(obptr, maxCOLS);	/* alternate height */
    SET16(obptr, maxROWS);	/* alternate width */
}

static void
query_reply_end(void)
{
    net_output();
    kybd_inhibit(true);
}
