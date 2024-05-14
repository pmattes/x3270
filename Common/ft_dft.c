/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	ft_dft.c
 *		File transfer: DFT-style data processing functions
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ft_dft_ds.h"

#include "kybd.h"
#include "ft_dft.h"
#include "ft_private.h"
#include "unicodec.h"
#include "ft.h"
#include "telnet_core.h"
#include "trace.h"
#include "utils.h"

#include <errno.h>

/* Macros. */
#define OPEN_MSG	"FT:MSG"	/* Open request for message */
#define END_TRANSFER	"TRANS03"	/* Message for xfer complete */

#define DFT_MAX_UNGETC	32

/* Typedefs. */
struct data_buffer {
    char sf_length[2];		/* SF length = 0x0023 */
    char sf_d0;			/* 0xD0 */
    char sf_request_type[2];	/* request type */
    char compress_indic[2];	/* 0xc080 */
    char begin_data;		/* 0x61 */
    char data_length[2];	/* Data Length in 3270 byte order+5 */
    char data[256];		/* The actual data */
};

/* Globals. */

/* Statics. */
static bool message_flag = false;	/* Open Request for msg received */
static int dft_eof;
static unsigned long recnum;
static char *abort_string = NULL;
static unsigned char *dft_savebuf = NULL;
static size_t dft_savebuf_len = 0;
static size_t dft_savebuf_max = 0;
static unsigned char dft_ungetc_cache[DFT_MAX_UNGETC];
static size_t dft_ungetc_count = 0;

static void dft_abort(const char *s, unsigned short code);
static void dft_close_request(void);
static void dft_data_insert(struct data_buffer *data_bufr);
static void dft_get_request(void);
static void dft_insert_request(void);
static void dft_open_request(unsigned short len, unsigned char *cp);
static void dft_set_cur_req(void);

/* Process a Transfer Data structured field from the host. */
void
ft_dft_data(unsigned char *data, int length _is_unused)
{
    struct data_buffer *data_bufr = (struct data_buffer *)data;
    unsigned short data_length, data_type;
    unsigned char *cp;

    if (ft_state == FT_NONE) {
	trace_ds(" (no transfer in progress)\n");
	return;
    }

    /* Get the length. */
    cp = (unsigned char *)(data_bufr->sf_length);
    GET16(data_length, cp);

    /* Get the function type. */
    cp = (unsigned char *)(data_bufr->sf_request_type);
    GET16(data_type, cp);

    /* Handle the requests */
    switch (data_type) {
    case TR_OPEN_REQ:
	dft_open_request(data_length, cp);
	break;
    case TR_INSERT_REQ:	/* Insert Request */
	dft_insert_request();
	break;
    case TR_DATA_INSERT:
	dft_data_insert(data_bufr);
	break;
    case TR_SET_CUR_REQ:
	dft_set_cur_req();
	break;
    case TR_GET_REQ:
	dft_get_request();
	break;
    case TR_CLOSE_REQ:
	dft_close_request();
	break;
    default:
	trace_ds(" Unsupported(0x%04x)\n", data_type);
	break;
    }
}

/* Process an Open request. */
static void
dft_open_request(unsigned short len, unsigned char *cp)
{
    char *name = "?";
    char namebuf[8];
    char *s;
    unsigned short recsz = 0;

    if (len == 0x23) {
	name = (char *)cp + 25;
    } else if (len == 0x29) {
	unsigned char *recszp;

	recszp = cp + 27;
	GET16(recsz, recszp);
	name = (char *)cp + 31;
    } else {
	dft_abort(get_message("ftDftUnknownOpen"), TR_OPEN_REQ);
	return;
    }

    memcpy(namebuf, name, 7);
    namebuf[7] = '\0';
    s = &namebuf[6];
    while (s >= namebuf && *s == ' ') {
	*s-- = '\0';
    }
    if (recsz) {
	trace_ds(" Open('%s',recsz=%u)\n", namebuf, recsz);
    } else {
	trace_ds(" Open('%s')\n", namebuf);
    }

    if (!strcmp(namebuf, OPEN_MSG)) {
	message_flag = true;
    } else {
	message_flag = false;
	ft_running(false);
    }
    dft_eof = false;
    recnum = 1;
    dft_ungetc_count = 0;

    /* Acknowledge the Open. */
    trace_ds("> WriteStructuredField FileTransferData OpenAck\n");
    obptr = obuf;
    space3270out(6);
    *obptr++ = AID_SF;
    SET16(obptr, 5);
    *obptr++ = SF_TRANSFER_DATA;
    SET16(obptr, 9);
    net_output();
}

/* Process an Insert request. */
static void
dft_insert_request(void)
{
    trace_ds(" Insert\n");
    /* Doesn't currently do anything. */
}

/* Send an acknowledgement frame back. */
static void
dft_data_ack(void)
{
    trace_ds("> WriteStructuredField FileTransferData DataAck(rec=%lu)\n",
	    recnum);
    obptr = obuf;
    space3270out(12);
    *obptr++ = AID_SF;
    SET16(obptr, 11);
    *obptr++ = SF_TRANSFER_DATA;
    SET16(obptr, TR_NORMAL_REPLY);
    SET16(obptr, TR_RECNUM_HDR);
    SET32(obptr, recnum);
    recnum++;
    net_output();
}

/* Process a Data Insert request. */
static void
dft_data_insert(struct data_buffer *data_bufr)
{
    /* Received a data buffer, get the length and process it */
    int my_length;
    unsigned char *cp;

    if (!message_flag && ft_state == FT_ABORT_WAIT) {
	dft_abort(get_message("ftUserCancel"), TR_DATA_INSERT);
	return;
    }

    cp = (unsigned char *) (data_bufr->data_length);

    /* Get the data length in native format. */
    GET16(my_length, cp);

    /* Adjust for 5 extra count */
    my_length -= 5;

    trace_ds(" Data(rec=%lu) %d bytes\n", recnum, my_length);

    /*
     * First, check to see if we have message data or file data.
     * Message data will result in a popup.
     */
    if (message_flag) {
	/* Data is from a message */
	unsigned char *msgp;
	unsigned char *dollarp;

	/* Ack the message. */
	dft_data_ack();

	/* Get storage to copy the message. */
	msgp = (unsigned char *)Malloc(my_length + 1);

	/* Copy the message. */
	memcpy(msgp, data_bufr->data, my_length);

	/* Null terminate the string. */
	dollarp = (unsigned char *)memchr(msgp, '$', my_length);
	if (dollarp != NULL) {
	    *dollarp = '\0';
	} else {
	    *(msgp + my_length) = '\0';
	}

	/* If transfer completed ok, use our msg. */
	if (memcmp(msgp, END_TRANSFER, strlen(END_TRANSFER)) == 0) {
	    Free(msgp);
	    ft_complete(NULL);
	} else if (ft_state == FT_ABORT_SENT && abort_string != NULL) {
	    Free(msgp);
	    ft_complete(abort_string);
	    Replace(abort_string, NULL);
	} else {
	    ft_complete((char *)msgp);
	    Free(msgp);
	}

	return;
    }

    /* Process file data. */
    if (my_length > 0) {
	size_t rv = 1;

	/* Write the data out to the file. */
	if (ftc->ascii_flag && (ftc->remap_flag || ftc->cr_flag)) {
	    size_t obuf_len = 4 * my_length;
	    char *ob0 = Malloc(obuf_len);
	    char *ob = ob0;
	    unsigned char *s = (unsigned char *)data_bufr->data;
	    unsigned len = my_length;
	    size_t nx;

	    /* Copy and convert data_bufr->data to ob0. */
	    while (len-- && obuf_len) {
		unsigned char c = *s++;

		/* Strip CR's and ^Z's. */
		if (ftc->cr_flag && ((c == '\r' || c == 0x1a))) {
		    continue;
		}

		if (!ftc->remap_flag) {
		    *ob++ = c;
		    obuf_len--;
		    continue;
		}

		/*
		 * Convert to local multi-byte.
		 * We do that by inverting the host's
		 * EBCDIC-to-ASCII map, getting back to
		 * EBCDIC, and converting to multi-byte
		 * from there.
		 */

		switch (fts.dbcs_state) {
		case FT_DBCS_NONE:
		    if (c == EBC_so) {
			fts.dbcs_state = FT_DBCS_SO;
			continue;
		    }
		    /*
		     * fall through to non-DBCS case below
		     */
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
		    nx = ft_ebcdic_to_multibyte(
			    (fts.dbcs_byte1 << 8) | i_asc2ft[c],
			    (char *)ob, obuf_len);
		    if (nx && (ob[nx - 1] == '\0')) {
			nx--;
		    }
		    ob += nx;
		    obuf_len -= nx;
		    fts.dbcs_state = FT_DBCS_SO;
		    continue;
		}

		if (c < 0x20 || (c >= 0x80 && c < 0xa0 && c != 0x9f)) {
		    /*
		     * Control code, treat it as Unicode.
		     *
		     * Note that IND$FILE and the VM 'TYPE'
		     * command think that EBCDIC X'E1' is
		     * a control code; IND$FILE maps it
		     * onto ASCII 0x9f.  So we skip it
		     * explicitly and treat it as printable
		     * here.
		     */
		    nx = ft_unicode_to_multibyte(c, ob, obuf_len);
		} else if (c == 0xff) {
		    /* IND$FILE maps X'FF' to 0xff. We want U+009F. */
		    nx = ft_unicode_to_multibyte(0x9f, ob, obuf_len);
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

	    /* Write the result to the file. */
	    if (ob - ob0) {
		rv = fwrite(ob0, ob - ob0, (size_t)1, fts.local_file);
		fts.length += ob - ob0;
	    }
	    Free(ob0);
	} else {
	    /* Write the buffer to the file directly. */
	    rv = fwrite((char *)data_bufr->data, my_length, (size_t)1,
		    fts.local_file);
	    fts.length += my_length;
	}

	if (!rv) {
	    /* write failed */
	    char *buf;

	    buf = Asprintf("write(%s): %s", ftc->local_filename,
		    strerror(errno));

	    dft_abort(buf, TR_DATA_INSERT);
	    Free(buf);
	}

	/* Add up amount transferred. */
	ft_update_length();
    }

    /* Send an acknowledgement frame back. */
    dft_data_ack();
}

/* Process a Set Cursor request. */
static void
dft_set_cur_req(void)
{
    trace_ds(" SetCursor\n");
    /* Currently doesn't do anything. */
}

/* Store a byte inthe input buffer or ungetc cache. */
static void
store_inbyte(unsigned char c, unsigned char **bufptr, size_t *numbytes)
{
    if (*numbytes) {
	*(*bufptr) = c;
	(*bufptr)++;
	(*numbytes)--;
    } else {
	dft_ungetc_cache[dft_ungetc_count++] = c;
    }
}

/*
 * Read a character from a local file in ASCII mode.
 * Stores the data in 'bufptr' and returns the number of bytes stored.
 * Returns -1 for EOF.
 */
static size_t
dft_ascii_read(unsigned char *bufptr, size_t numbytes)
{
    char inbuf[16];
    int in_ix = 0;
    int c;
    enum me_fail error;
    ebc_t e;
    int consumed;
    ucs4_t u;

    /* Belt-n-suspenders. */
    if (!numbytes) {
	return 0;
    }

    /* Return data from the ungetc cache first. */
    if (dft_ungetc_count) {
	size_t nm = dft_ungetc_count;

	if (nm > numbytes) {
	    nm = numbytes;
	}
	memcpy(bufptr, dft_ungetc_cache, nm);
	if (dft_ungetc_count > nm) {
	    memmove(dft_ungetc_cache, &dft_ungetc_cache[nm],
		    dft_ungetc_count - nm);
	}
	dft_ungetc_count -= nm;
	return nm;
    }

    if (ftc->remap_flag) {
	/* Read bytes until we have a legal multibyte sequence. */
	do {
	    int consumed;

	    c = fgetc(fts.local_file);
	    if (c == EOF) {
		if (fts.last_dbcs) {
		    *bufptr = EBC_si;
		    fts.last_dbcs = false;
		    return 1;
		}
		return -1;
	    }
	    error = ME_NONE;
	    inbuf[in_ix++] = c;
	    ft_multibyte_to_unicode(inbuf, in_ix, &consumed, &error);
	    if (error == ME_INVALID) {
		inbuf[0] = '?';
		in_ix = 1;
		error = ME_NONE;
	    }
	} while (error == ME_SHORT);
    } else {
	/* Get a byte from the file. */
	c = fgetc(fts.local_file);
	if (c == EOF) {
	    return -1;
	}
    }

    /* Expand NL to CR/LF. */
    if (ftc->cr_flag && !fts.last_cr && c == '\n') {
	if (fts.last_dbcs) {
	    *bufptr = EBC_si;
	    dft_ungetc_cache[0] = '\r';
	    dft_ungetc_cache[1] = '\n';
	    dft_ungetc_count = 2;
	    fts.last_dbcs = false;
	    return 1;
	} else {
	    *bufptr = '\r';
	    dft_ungetc_cache[0] = '\n';
	    dft_ungetc_count = 1;
	}
	return 1;
    }
    fts.last_cr = (c == '\r');

    /* The no-remap case is pretty simple. */
    if (!ftc->remap_flag) {
	*bufptr = c;
	return 1;
    }

    /*
     * Translate, inverting the host's fixed EBCDIC-to-ASCII conversion
     * table and applying the host code page.
     * Control codes are treated as Unicode and mapped directly.
     * We also handle DBCS here.
     */
    u = ft_multibyte_to_unicode(inbuf, in_ix, &consumed, &error);
    if (u < 0x20 || ((u >= 0x80 && u < 0x9f))) {
	e = i_asc2ft[u];
    } else if (u == 0x9f) {
	e = 0xff;
    } else {
	e = unicode_to_ebcdic(u);
    }
    if (e & 0xff00) {
	unsigned char *bp0 = bufptr;

	if (!fts.last_dbcs) {
	    store_inbyte(EBC_so, &bufptr, &numbytes);
	}
	store_inbyte(i_ft2asc[(e >> 8) & 0xff], &bufptr, &numbytes);
	store_inbyte(i_ft2asc[e & 0xff],        &bufptr, &numbytes);
	fts.last_dbcs = true;
	return bufptr - bp0;
    } else {
	unsigned char nc = e? i_ft2asc[e]: '?';

	if (fts.last_dbcs) {
	    *bufptr = EBC_si;
	    dft_ungetc_cache[0] = nc;
	    dft_ungetc_count = 1;
	    fts.last_dbcs = false;
	} else {
	    *bufptr = nc;
	}
	return 1;
    }
}

/* Process a Get request. */
static void
dft_get_request(void)
{
    size_t numbytes;
    size_t numread;
    size_t total_read = 0;
    unsigned char *bufptr;

    trace_ds(" Get\n");

    if (!message_flag && ft_state == FT_ABORT_WAIT) {
	dft_abort(get_message("ftUserCancel"), TR_GET_REQ);
	return;
    }

    /* Read a buffer's worth. */
    space3270out(ftc->dft_buffersize);
    numbytes = ftc->dft_buffersize - 27; /* always read 5 bytes less than we're
				            allowed */
    bufptr = obuf + 17;
    while (!dft_eof && numbytes) {
	if (ftc->ascii_flag && (ftc->remap_flag || ftc->cr_flag)) {
	    numread = dft_ascii_read(bufptr, numbytes);
	    if (numread == (size_t)-1) {
		dft_eof = true;
		break;
	    }
	    bufptr += numread;
	    numbytes -= numread;
	    total_read += numread;
	} else {
	    /* Binary read. */
	    numread = fread(bufptr, 1, numbytes, fts.local_file);
	    if (numread <= 0) {
		break;
	    }
	    bufptr += numread;
	    numbytes -= numread;
	    total_read += numread;
	    if (feof(fts.local_file)) {
		dft_eof = true;
	    }
	    if (feof(fts.local_file) || ferror(fts.local_file)) {
		break;
	    }
	}
    }

    /* Check for read error. */
    if (ferror(fts.local_file)) {
	char *buf;

	buf = Asprintf("read(%s): %s", ftc->local_filename, strerror(errno));
	dft_abort(buf, TR_GET_REQ);
	Free(buf);
	return;
    }

    /* Set up SF header for Data or EOF. */
    obptr = obuf;
    *obptr++ = AID_SF;
    obptr += 2;	/* skip SF length for now */
    *obptr++ = SF_TRANSFER_DATA;

    if (total_read) {
	trace_ds("> WriteStructuredField FileTransferData Data(rec=%lu) %d "
		"bytes\n", recnum, (int)total_read);
	SET16(obptr, TR_GET_REPLY);
	SET16(obptr, TR_RECNUM_HDR);
	SET32(obptr, recnum);
	recnum++;
	SET16(obptr, TR_NOT_COMPRESSED);
	*obptr++ = TR_BEGIN_DATA;
	SET16(obptr, total_read + 5);
	obptr += total_read;

	fts.length += total_read;
    } else {
	trace_ds("> WriteStructuredField FileTransferData EOF\n");
	*obptr++ = HIGH8(TR_GET_REQ);
	*obptr++ = TR_ERROR_REPLY;
	SET16(obptr, TR_ERROR_HDR);
	SET16(obptr, TR_ERR_EOF);

	dft_eof = true;
    }

    /* Set the SF length. */
    bufptr = obuf + 1;
    SET16(bufptr, obptr - (obuf + 1));

    /* Save the data. */
    dft_savebuf_len = obptr - obuf;
    if (dft_savebuf_len > dft_savebuf_max) {
	dft_savebuf_max = dft_savebuf_len;
	Replace(dft_savebuf, (unsigned char *)Malloc(dft_savebuf_max));
    }
    memcpy(dft_savebuf, obuf, dft_savebuf_len);
    aid = AID_SF;

    /* Write the data. */
    net_output();
    ft_update_length();
}

/* Process a Close request. */
static void
dft_close_request(void)
{
    /*
     * Recieved a close request from the system.
     * Return a close acknowledgement.
     */
    trace_ds(" Close\n");
    trace_ds("> WriteStructuredField FileTransferData CloseAck\n");
    obptr = obuf;
    space3270out(6);
    *obptr++ = AID_SF;
    SET16(obptr, 5);	/* length */
    *obptr++ = SF_TRANSFER_DATA;
    SET16(obptr, TR_CLOSE_REPLY);
    net_output();
}

/* Abort a transfer. */
static void
dft_abort(const char *s, unsigned short code)
{
    Replace(abort_string, NewString(s));

    trace_ds("> WriteStructuredField FileTransferData Error\n");

    obptr = obuf;
    space3270out(10);
    *obptr++ = AID_SF;
    SET16(obptr, 9);	/* length */
    *obptr++ = SF_TRANSFER_DATA;
    *obptr++ = HIGH8(code);
    *obptr++ = TR_ERROR_REPLY;
    SET16(obptr, TR_ERROR_HDR);
    SET16(obptr, TR_ERR_CMDFAIL);
    net_output();

    /* Update the pop-up and state. */
    ft_aborting();
}

/* Processes a Read Modified command when there is upload data pending. */
void
dft_read_modified(void)
{
    if (dft_savebuf_len) {
	trace_ds("> WriteStructuredField FileTransferData\n");
	obptr = obuf;
	space3270out(dft_savebuf_len);
	memcpy(obptr, dft_savebuf, dft_savebuf_len);
	obptr += dft_savebuf_len;
	net_output();
    }
}

/* Default/bound the buffersize for generating a Query Reply. */
int
set_dft_buffersize(int size)
{
    /*
     * Pick the default:
     * - New resource
     * - Old resource
     * - Hard-coded default
     */
    if (!size && !(size = appres.ft.dft_buffer_size)) {
	size = DFT_BUF;
    }

    /* Bound the result. */
    if (size > DFT_MAX_BUF) {
	size = DFT_MAX_BUF;
    }
    if (size < DFT_MIN_BUF) {
	size = DFT_MIN_BUF;
    }
    return size;
}
