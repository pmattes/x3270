/*
 * Copyright (c) 1996-2025 Paul Mattes.
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
 *	ft_cut.c
 *		File transfer, data movement logic, CUT version
 */

#include "globals.h"

#include <errno.h>

#include "ctlr.h"
#include "3270ds.h"

#include "actions.h"
#include "ctlrc.h"
#include "ft_cut.h"
#include "ft_cut_ds.h"
#include "ft_private.h"
#include "unicodec.h"
#include "ft.h"
#include "names.h"
#include "tables.h"
#include "trace.h"
#include "utils.h"

static bool cut_xfer_in_progress = false;

/* Data stream conversion tables. */

#define NQ		4	/* number of quadrants */
#define NE		77	/* number of elements per quadrant */
#define OTHER_2		2	/* "OTHER 2" quadrant (includes NULL) */
#define XLATE_NULL	0xc1	/* translation of NULL */

static char alphas[NE + 1] =
" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789%&_()<+,-./:>?";

static struct {
    unsigned char selector;
    unsigned char xlate[NE];
} conv[NQ] = {
    {	0x5e,	/* ';' */
	{ 0x40,0xc1,0xc2,0xc3, 0xc4,0xc5,0xc6,0xc7, 0xc8,0xc9,0xd1,0xd2,
	  0xd3,0xd4,0xd5,0xd6, 0xd7,0xd8,0xd9,0xe2, 0xe3,0xe4,0xe5,0xe6,
	  0xe7,0xe8,0xe9,0x81, 0x82,0x83,0x84,0x85, 0x86,0x87,0x88,0x89,
	  0x91,0x92,0x93,0x94, 0x95,0x96,0x97,0x98, 0x99,0xa2,0xa3,0xa4,
	  0xa5,0xa6,0xa7,0xa8, 0xa9,0xf0,0xf1,0xf2, 0xf3,0xf4,0xf5,0xf6,
	  0xf7,0xf8,0xf9,0x6c, 0x50,0x6d,0x4d,0x5d, 0x4c,0x4e,0x6b,0x60,
	  0x4b,0x61,0x7a,0x6e, 0x6f }
    },
    {	0x7e,	/* '=' */
	{ 0x20,0x41,0x42,0x43, 0x44,0x45,0x46,0x47, 0x48,0x49,0x4a,0x4b,
	  0x4c,0x4d,0x4e,0x4f, 0x50,0x51,0x52,0x53, 0x54,0x55,0x56,0x57,
	  0x58,0x59,0x5a,0x61, 0x62,0x63,0x64,0x65, 0x66,0x67,0x68,0x69,
	  0x6a,0x6b,0x6c,0x6d, 0x6e,0x6f,0x70,0x71, 0x72,0x73,0x74,0x75,
	  0x76,0x77,0x78,0x79, 0x7a,0x30,0x31,0x32, 0x33,0x34,0x35,0x36,
	  0x37,0x38,0x39,0x25, 0x26,0x27,0x28,0x29, 0x2a,0x2b,0x2c,0x2d,
	  0x2e,0x2f,0x3a,0x3b, 0x3f }
    },
    {	0x5c,	/* '*' */
	{ 0x00,0x00,0x01,0x02, 0x03,0x04,0x05,0x06, 0x07,0x08,0x09,0x0a,
	  0x0b,0x0c,0x0d,0x0e, 0x0f,0x10,0x11,0x12, 0x13,0x14,0x15,0x16,
	  0x17,0x18,0x19,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	  0x00,0x00,0x00,0x00, 0x00,0x3c,0x3d,0x3e, 0x00,0xfa,0xfb,0xfc,
	  0xfd,0xfe,0xff,0x7b, 0x7c,0x7d,0x7e,0x7f, 0x1a,0x1b,0x1c,0x1d,
	  0x1e,0x1f,0x00,0x00, 0x00 }
    },
    {	0x7d,	/* '\'' */
	{ 0x00,0xa0,0xa1,0xea, 0xeb,0xec,0xed,0xee, 0xef,0xe0,0xe1,0xaa,
	  0xab,0xac,0xad,0xae, 0xaf,0xb0,0xb1,0xb2, 0xb3,0xb4,0xb5,0xb6,
	  0xb7,0xb8,0xb9,0x80, 0x00,0xca,0xcb,0xcc, 0xcd,0xce,0xcf,0xc0,
	  0x00,0x8a,0x8b,0x8c, 0x8d,0x8e,0x8f,0x90, 0x00,0xda,0xdb,0xdc,
	  0xdd,0xde,0xdf,0xd0, 0x00,0x00,0x21,0x22, 0x23,0x24,0x5b,0x5c,
	  0x00,0x5e,0x5f,0x00, 0x9c,0x9d,0x9e,0x9f, 0xba,0xbb,0xbc,0xbd,
	  0xbe,0xbf,0x9a,0x9b, 0x00 }
    }
};
static char table6[] =
    "abcdefghijklmnopqrstuvwxyz&-.,:+ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

static int quadrant = -1;
static unsigned long expanded_length;
static char *saved_errmsg = NULL;

#define XLATE_NBUF	32
static int xlate_buffered = 0;			/* buffer count */
static int xlate_buf_ix = 0;			/* buffer index */
static unsigned char xlate_buf[XLATE_NBUF];	/* buffer */
static bool cut_eof = false;

static void cut_control_code(void);
static void cut_data_request(void);
static void cut_retransmit(void);
static void cut_data(void);

static void cut_ack(void);
static void cut_abort(const char *s, unsigned short reason);

static unsigned from6(unsigned char c);
static int xlate_getc(void);

/*
 * Convert a buffer for uploading (host->local).
 * Returns the length of the converted data.
 * If there is a conversion error, calls cut_abort() and returns -1.
 */
static int
upload_convert(unsigned char *buf, int len, unsigned char *obuf,
	size_t obuf_len)
{
    unsigned char *ob0 = obuf;
    unsigned char *ob = ob0;
    size_t nx;

    while (len-- && obuf_len) {
	unsigned char c = *buf++;
	char *ixp;
	size_t ix;

    retry:
	if (quadrant < 0) {
	    /* Find the quadrant. */
	    for (quadrant = 0; quadrant < NQ; quadrant++) {
		if (c == conv[quadrant].selector) {
		    break;
		}
	    }
	    if (quadrant >= NQ) {
		cut_abort(get_message("ftCutConversionError"), SC_ABORT_XMIT);
		return -1;
	    }
	    continue;
	}

	/* Make sure it's in a valid range. */
	if (c < 0x40 || c > 0xf9) {
	    cut_abort(get_message("ftCutConversionError"), SC_ABORT_XMIT);
	    return -1;
	}

	/* Translate to a quadrant index. */
	ixp = strchr(alphas, ebc2asc0[c]);
	if (ixp == NULL) {
	    /* Try a different quadrant. */
	    quadrant = -1;
	    goto retry;
	}
	ix = ixp - alphas;

	/*
	 * See if it's mapped by that quadrant, handling NULLs
	 * specially.
	 */
	if (quadrant != OTHER_2 && c != XLATE_NULL &&
		!conv[quadrant].xlate[ix]) {
	    /* Try a different quadrant. */
	    quadrant = -1;
	    goto retry;
	}

	/* Map it. */
	c = conv[quadrant].xlate[ix];
	if (ftc->ascii_flag && ftc->cr_flag && (c == '\r' || c == 0x1a)) {
	    continue;
	}
	if (!(ftc->ascii_flag && ftc->remap_flag)) {
	    /* No further translation necessary. */
	    *ob++ = c;
	    obuf_len--;
	    continue;
	}

	/*
	 * Convert to local multi-byte.
	 * We do that by inverting the host's EBCDIC-to-ASCII map,
	 * getting back to EBCDIC, and converting to multi-byte from
	 * there.
	 */
	switch (fts.dbcs_state) {
	case FT_DBCS_NONE:
	    if (c == EBC_so) {
		fts.dbcs_state = FT_DBCS_SO;
		continue;
	    }
	    /* fall through to non-DBCS case below */
	    break;
	case FT_DBCS_SO:
	    if (c == EBC_si) {
		fts.dbcs_state = FT_DBCS_NONE;
	    } else {
		fts.dbcs_byte1 = i_asc2ft[c];
		fts.dbcs_state = FT_DBCS_LEFT;
	    }
	    continue;
	case FT_DBCS_LEFT:
	    if (c == EBC_si) {
		fts.dbcs_state = FT_DBCS_NONE;
		continue;
	    }
	    nx = ft_ebcdic_to_multibyte((fts.dbcs_byte1 << 8) | i_asc2ft[c],
		    (char *)ob, obuf_len);
	    if (nx && (ob[nx - 1] == '\0')) {
		nx--;
	    }
	    ob += nx;
	    obuf_len -= nx;
	    fts.dbcs_state = FT_DBCS_SO;
	    continue;
	}

	if (c < 0x20 || ((c >= 0x80 && c < 0xa0 && c != 0x9f))) {
	    /*
	     * Control code, treat it as Unicode.
	     *
	     * Note that IND$FILE and the VM 'TYPE' command think
	     * that EBCDIC X'E1' is a control code; IND$FILE maps
	     * it onto ASCII 0x9f.  So we skip it explicitly and
	     * treat it as printable here.
	     */
	    nx = ft_unicode_to_multibyte(c, (char *)ob, obuf_len);
	} else if (c == 0xff) {
	    nx = ft_unicode_to_multibyte(0x9f, (char *)ob, obuf_len);
	} else {
	    /* Displayable character, remap. */
	    c = i_asc2ft[c];
	    nx = ft_ebcdic_to_multibyte(c, (char *)ob, obuf_len);
	}
	if (nx && (ob[nx - 1] == '\0')) {
	    nx--;
	}
	ob += nx;
	obuf_len -= nx;
    }

    return (int)(ob - ob0);
}

/*
 * Store a download (local->host) character.
 * Returns the number of bytes stored.
 */
static int
store_download(unsigned char c, unsigned char *ob)
{
    unsigned char *ixp;
    size_t ix;
    int oq;

    /* Quadrant already defined. */
    if (quadrant >= 0) {
	ixp = (unsigned char *)memchr(conv[quadrant].xlate, c, NE);
	if (ixp != NULL) {
	    ix = ixp - conv[quadrant].xlate;
	    *ob++ = asc2ebc0[(int)alphas[ix]];
	    return 1;
	}
    }

    /* Locate a quadrant. */
    oq = quadrant;
    for (quadrant = 0; quadrant < NQ; quadrant++) {
	if (quadrant == oq) {
	    continue;
	}
	ixp = (unsigned char *)memchr(conv[quadrant].xlate, c, NE);
	if (ixp == NULL) {
		continue;
	}
	ix = ixp - conv[quadrant].xlate;
	*ob++ = conv[quadrant].selector;
	*ob++ = asc2ebc0[(int)alphas[ix]];
	return 2;
    }
    quadrant = -1;
    fprintf(stderr, "Oops\n");
    return 0;
}

/* Convert a buffer for downloading (local->host). */
static size_t
download_convert(unsigned const char *buf, unsigned len, unsigned char *xobuf)
{
    unsigned char *ob0 = xobuf;
    unsigned char *ob = ob0;

    while (len) {
	unsigned char c = *buf;
	int consumed;
	enum me_fail error;
	ebc_t e;
	ucs4_t u;

	/* Handle nulls separately. */
	if (!c) {
	    if (fts.last_dbcs) {
		ob += store_download(EBC_si, ob);
		fts.last_dbcs = false;
	    }
	    if (quadrant != OTHER_2) {
		quadrant = OTHER_2;
		*ob++ = conv[quadrant].selector;
	    }
	    *ob++ = XLATE_NULL;
	    buf++;
	    len--;
	    continue;
	}

	if (!(ftc->ascii_flag && ftc->remap_flag)) {
	    ob += store_download(c, ob);
	    buf++;
	    len--;
	    continue;
	}

	/*
	 * Translate.
	 *
	 * The host uses a fixed EBCDIC-to-ASCII translation table,
	 * which was derived empirically into i_ft2asc/i_asc2ft.
	 * Invert that so that when the host applies its conversion,
	 * it gets the right EBCDIC code.
	 *
	 * DBCS is a guess at this point, assuming that SO and SI
	 * are unmodified by IND$FILE.
	 */
	u = ft_multibyte_to_unicode((const char *)buf, len, &consumed, &error);
	if (u < 0x20 || ((u >= 0x80 && u < 0x9f))) {
	    e = i_asc2ft[u];
	} else if (u == 0x9f) {
	    e = 0xff;
	} else {
	    e = unicode_to_ebcdic(u);
	}
	if (e & 0xff00) {
	    if (!fts.last_dbcs) {
		ob += store_download(EBC_so, ob);
	    }
	    ob += store_download(i_ft2asc[(e >> 8) & 0xff], ob);
	    ob += store_download(i_ft2asc[e & 0xff], ob);
	    fts.last_dbcs = true;
	} else {
	    if (fts.last_dbcs) {
		ob += store_download(EBC_si, ob);
		fts.last_dbcs = false;
	    }
	    if (e == 0) {
		ob += store_download('?', ob);
	    } else {
		ob += store_download(i_ft2asc[e], ob);
	    }
	}
	buf += consumed;
	len -= consumed;
    }

    return ob - ob0;
}

/*
 * Main entry point from ctlr.c.
 * We have received what looks like an appropriate message from the host.
 */
void
ft_cut_data(void)
{
    if (ea_buf[O_SF].fa && FA_IS_SKIP(ea_buf[O_SF].fa)) {
	switch (ea_buf[O_FRAME_TYPE].ec) {
	case FT_CONTROL_CODE:
	    cut_control_code();
	    break;
	case FT_DATA_REQUEST:
	    cut_data_request();
	    break;
	case FT_RETRANSMIT:
	    cut_retransmit();
	    break;
	case FT_DATA:
	    cut_data();
	    break;
	default:
	    vctrace(TC_FT, "< unknown 0x%02x\n", ea_buf[O_FRAME_TYPE].ec);
	    cut_abort(get_message("ftCutUnknownFrame"), SC_ABORT_XMIT);
	    break;
	}
    }
}

/*
 * Process a control code from the host.
 */
static void
cut_control_code(void)
{
    unsigned short code;
    char *buf;
    char *bp;
    int i;

    vctrace(TC_FT, "< CONTROL_CODE ");
    code = (ea_buf[O_CC_STATUS_CODE].ec << 8) |
	    ea_buf[O_CC_STATUS_CODE + 1].ec;
    switch (code) {
    case SC_HOST_ACK:
	vctrace(TC_FT, "HOST_ACK\n");
	cut_xfer_in_progress = true;
	expanded_length = 0;
	quadrant = -1;
	xlate_buffered = 0;
	xlate_buf_ix = 0;
	cut_eof = false;
	cut_ack();
	ft_running(true);
	break;
    case SC_XFER_COMPLETE:
	vctrace(TC_FT, "XFER_COMPLETE\n");
	cut_ack();
	cut_xfer_in_progress = false;
	ft_complete(NULL);
	break;
    case SC_ABORT_FILE:
    case SC_ABORT_XMIT:
	vctrace(TC_FT, "ABORT\n");
	cut_xfer_in_progress = false;
	cut_ack();
	if (ft_state == FT_ABORT_SENT && saved_errmsg != NULL) {
	    buf = saved_errmsg;
	    saved_errmsg = NULL;
	} else {
	    size_t mb_len = 161;

	    bp = buf = Malloc(mb_len);
	    for (i = 0; i < 80; i++) {
		size_t xlen;

		xlen = ft_ebcdic_to_multibyte(ea_buf[O_CC_MESSAGE + i].ec, bp,
			mb_len);
		if (xlen) {
		    bp += xlen - 1;
		    mb_len -= xlen - 1;
		}
	    }
	    *bp-- = '\0';
	    while (bp >= buf && *bp == ' ') {
		*bp-- = '\0';
	    }
	    if (bp >= buf && *bp == '$') {
		*bp-- = '\0';
	    }
	    while (bp >= buf && *bp == ' ') {
		*bp-- = '\0';
	    }
	    if (!*buf) {
		strcpy(buf, get_message("ftHostCancel"));
	    }
	}
	ft_complete(buf);
	Free(buf);
	break;
    default:
	vctrace(TC_FT, "unknown 0x%04x\n", code);
	cut_abort(get_message("ftCutUnknownControl"), SC_ABORT_XMIT);
	break;
    }
}

/*
 * Process a data request from the host.
 */
static void
cut_data_request(void)
{
    unsigned char seq = ea_buf[O_DR_FRAME_SEQ].ec;
    int count;
    unsigned char cs;
    int c;
    int i;
    unsigned char attr;

    vctrace(TC_FT, "< DATA_REQUEST %u\n", from6(seq));
    if (ft_state == FT_ABORT_WAIT) {
	cut_abort(get_message("ftUserCancel"), SC_ABORT_FILE);
	return;
    }

    /* Copy data into the screen buffer. */
    count = 0;
    while (count < O_UP_MAX && !cut_eof) {
	if ((c = xlate_getc()) == EOF) {
	    cut_eof = true;
	    break;
	}
	ctlr_add(O_UP_DATA + count, c, 0);
	count++;
    }

    /* Check for errors. */
    if (ferror(fts.local_file)) {
	int j;
	char *msg;

	/* Clean out any data we may have written. */
	for (j = 0; j < count; j++) {
	    ctlr_add(O_UP_DATA + j, 0, 0);
	}

	/* Abort the transfer. */
	msg = Asprintf("read(%s): %s", ftc->local_filename, strerror(errno));
	cut_abort(msg, SC_ABORT_FILE);
	Free(msg);
	return;
    }

    /* Send special data for EOF. */
    if (!count && cut_eof) {
	ctlr_add(O_UP_DATA, EOF_DATA1, 0);
	ctlr_add(O_UP_DATA+1, EOF_DATA2, 0);
	count = 2;
    }

    /* Compute the other fields. */
    ctlr_add(O_UP_FRAME_SEQ, seq, 0);
    cs = 0;
    for (i = 0; i < count; i++) {
	cs ^= ea_buf[O_UP_DATA + i].ec;
    }
    ctlr_add(O_UP_CSUM, asc2ebc0[(int)table6[cs & 0x3f]], 0);
    ctlr_add(O_UP_LEN, asc2ebc0[(int)table6[(count >> 6) & 0x3f]], 0);
    ctlr_add(O_UP_LEN+1, asc2ebc0[(int)table6[count & 0x3f]], 0);

    /* XXX: Change the data field attribute so it doesn't display. */
    attr = ea_buf[O_DR_SF].fa;
    attr = (attr & ~FA_INTENSITY) | FA_INT_ZERO_NSEL;
    ctlr_add_fa(O_DR_SF, attr, 0);

    /* Send it up to the host. */
    vctrace(TC_FT, "> DATA %u\n", from6(seq));
    ft_update_length();
    expanded_length += count;
    run_action(AnEnter, IA_FT, NULL, NULL);
}

/*
 * (Improperly) process a retransmit from the host.
 */
static void
cut_retransmit(void)
{
    vctrace(TC_FT, "< RETRANSMIT\n");
    cut_abort(get_message("ftCutRetransmit"), SC_ABORT_XMIT);
}

/*
 * Convert an encoded integer.
 */
static unsigned
from6(unsigned char c)
{
    char *p;

    c = ebc2asc0[c];
    p = strchr(table6, c);
    if (p == NULL) {
	return 0;
    }
    return (unsigned)(p - table6);
}

/*
 * Process data from the host.
 */
static void
cut_data(void)
{
    static unsigned char cvbuf[O_RESPONSE - O_DT_DATA];
    static unsigned char cvobuf[4 * (O_RESPONSE - O_DT_DATA)];
    unsigned short raw_length;
    int conv_length;
    register int i;

    vctrace(TC_FT, "< DATA\n");
    if (ft_state == FT_ABORT_WAIT) {
	cut_abort(get_message("ftUserCancel"), SC_ABORT_FILE);
	return;
    }

    /* Copy and convert the data. */
    raw_length = from6(ea_buf[O_DT_LEN].ec) << 6 |
		 from6(ea_buf[O_DT_LEN + 1].ec);
    if ((int)raw_length > O_RESPONSE - O_DT_DATA) {
	cut_abort(get_message("ftCutOversize"), SC_ABORT_XMIT);
	return;
    }
    for (i = 0; i < (int)raw_length; i++) {
	cvbuf[i] = ea_buf[O_DT_DATA + i].ec;
    }

    if (raw_length == 2 && cvbuf[0] == EOF_DATA1 && cvbuf[1] == EOF_DATA2) {
	vctrace(TC_FT, "< EOF\n");
	cut_ack();
	return;
    }
    conv_length = upload_convert(cvbuf, raw_length, cvobuf, sizeof(cvobuf));
    if (conv_length < 0) {
	return;
    }

    /* Write it to the file. */
    if (fwrite((char *)cvobuf, conv_length, 1, fts.local_file) == 0) {
	char *msg;

	msg = Asprintf("write(%s): %s", ftc->local_filename, strerror(errno));
	cut_abort(msg, SC_ABORT_FILE);
	Free(msg);
    } else {
	fts.length += conv_length;
	ft_update_length();
	cut_ack();
    }
}

/*
 * Acknowledge a host command.
 */
static void
cut_ack(void)
{
    vctrace(TC_FT, "> ACK\n");
    run_action(AnEnter, IA_FT, NULL, NULL);
}

/*
 * Abort a transfer in progress.
 */
static void
cut_abort(const char *s, unsigned short reason)
{
    /* Save the error message. */
    Replace(saved_errmsg, NewString(s));

    /* Send the abort sequence. */
    ctlr_add(RO_FRAME_TYPE, RFT_CONTROL_CODE, 0);
    ctlr_add(RO_FRAME_SEQ, ea_buf[O_DT_FRAME_SEQ].ec, 0);
    ctlr_add(RO_REASON_CODE, HIGH8(reason), 0);
    ctlr_add(RO_REASON_CODE+1, LOW8(reason), 0);
    vctrace(TC_FT, "> CONTROL_CODE ABORT\n");
    run_action(AnPF, IA_FT, "2", NULL);

    /* Update the in-progress pop-up. */
    ft_aborting();
}

/*
 * Get the next translated character from the local file.
 * Returns the character (in EBCDIC), or EOF.
 */
static int
xlate_getc(void)
{
    int r;
    int c;
    unsigned char cbuf[32];
    size_t nc;
    int consumed;
    enum me_fail error;
    char mb[16];
    int mb_len = 0;

    /* If there is a data buffered, return it. */
    if (xlate_buffered) {
	r = xlate_buf[xlate_buf_ix];
	xlate_buf_ix++;
	xlate_buffered--;
	return r;
    }

    if (ftc->ascii_flag) {
	/*
	 * Get the next (possibly multi-byte) character from the file.
	 */
	do {
	    c = fgetc(fts.local_file);
	    if (c == EOF) {
		if (fts.last_dbcs) {
		    fts.last_dbcs = false;
		    return EBC_si;
		}
		return c;
	    }
	    fts.length++;
	    mb[mb_len++] = c;
	    error = ME_NONE;
	    ft_multibyte_to_unicode(mb, mb_len, &consumed, &error);
	    if (error == ME_INVALID) {
		mb[0] = '?';
		mb_len = 1;
		error = ME_NONE;
	    }
	} while (error == ME_SHORT);

	/* Expand it. */
	if (ftc->ascii_flag && ftc->cr_flag &&
	    !fts.last_cr && c == '\n') {
	    nc = download_convert((unsigned const char *)"\r", 1, cbuf);
	} else {
	    nc = 0;
	    fts.last_cr = (c == '\r');
	}

    } else {
	/* Binary, just read it. */
	c = fgetc(fts.local_file);
	if (c == EOF)
		return c;
	mb[0] = c;
	mb_len = 1;
	nc = 0;
	fts.length++;
    }

    /* Convert it. */
    nc += download_convert((unsigned char *)mb, mb_len, &cbuf[nc]);

    /* Return it and buffer what's left. */
    r = cbuf[0];
    if (nc > 1) {
	size_t i;

	for (i = 1; i < nc; i++) {
	    xlate_buf[xlate_buffered++] = cbuf[i];
	}
	xlate_buf_ix = 0;
    }
    return r;
}
