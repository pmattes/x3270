/*
 * Copyright (c) 1996-2024 Paul Mattes.
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
 *	ft_cut_ds.c
 *		Data Stream definitions for CUT-style file transfers.
 */

#define O_SF			1919	/* offset to CUT SF */

/* Primary Area */
#define O_FRAME_TYPE		0	/* offset to frame type */
#define  FT_CONTROL_CODE	0xc3	/*  frame type: control code (host->) */
#define   O_CC_FRAME_SEQ	1	/*   offset to frame sequence */
#define   O_CC_STATUS_CODE	2	/*   offset to status code */
#define    SC_HOST_ACK		0x8181	/*    ack of IND$FILE command */
#define    SC_XFER_COMPLETE	0x8189	/*    file transfer complete */
#define    SC_ABORT_FILE	0x8194	/*    abort, file error */
#define    SC_ABORT_XMIT	0x8198	/*    abort, transmission error */
#define   O_CC_MESSAGE		4	/*   offset of message text */
#define  FT_DATA_REQUEST	0xc2	/*  frame type: data request (host->) */
#define   O_DR_SF		1	/*   offset to start field */
#define   O_DR_DATA_CODE	2	/*   offset to data code */
#define   O_DR_FRAME_SEQ	3	/*   offset to frame sequence */
#define  FT_RETRANSMIT		0x4c	/*  frame type: retransmit (host->) */
#define  FT_DATA		0xc1	/*  frame type: data (bidirectional) */
#define   O_DT_FRAME_SEQ	1	/*   offset to frame sequence */
#define   O_DT_CSUM		2	/*   offset to checksum */
#define   O_DT_LEN		3	/*   offset to length */
#define   O_DT_DATA		5	/*   offset to data */

/* Response Area */
#define O_RESPONSE		(O_SF-5)	/* offset to response area */
#define RO_FRAME_TYPE		(O_RESPONSE+1)	/* response frame type */
#define  RFT_RETRANSMIT		0x4c	/* response frame type: retransmit */
#define  RFT_CONTROL_CODE	0xc3	/* response frame type: control code */
#define RO_FRAME_SEQ		(O_RESPONSE+2)	/* response frame sequence */
#define RO_REASON_CODE		(O_RESPONSE+3)	/* response reason code */

/* Special Data */
#define EOF_DATA1		0x5c	/* special data for EOF */
#define EOF_DATA2		0xa9

/* Acknowledgement AIDs */
#define ACK_OK			AID_ENTER
#define ACK_RETRANSMIT		AID_PF1
#define ACK_RESYNC_VM		AID_CLEAR
#define ACK_RESYNC_TSO		AID_PA2
#define ACK_ABORT		AID_PF2

/* Data area for uploads. */
#define O_UP_DATA_CODE		2	/* offset to data code */
#define O_UP_FRAME_SEQ		3	/* offset to frame sequence */
#define O_UP_CSUM		4	/* offset to checksum */
#define O_UP_LEN		5	/* offset to length */
#define O_UP_DATA		7	/* offset to start of data */
#define O_UP_MAX		(O_SF-O_UP_DATA)	/* max upload data */
