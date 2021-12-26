/*
 * Copyright (c) 1996-2009, 2014, 2021 Paul Mattes.
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
 *	tn3270e.h
 *
 *		Header file for the TN3270E Protocol, RFC 2355.
 */

/* Negotiation operations. */
#define TN3270E_OP_ASSOCIATE		0
#define TN3270E_OP_CONNECT		1
#define TN3270E_OP_DEVICE_TYPE		2
#define TN3270E_OP_FUNCTIONS		3
#define TN3270E_OP_IS			4
#define TN3270E_OP_REASON		5
#define TN3270E_OP_REJECT		6
#define TN3270E_OP_REQUEST		7
#define TN3270E_OP_SEND			8

/* Negotiation reason-codes. */
#define TN3270E_REASON_CONN_PARTNER	0
#define TN3270E_REASON_DEVICE_IN_USE	1
#define TN3270E_REASON_INV_ASSOCIATE	2
#define TN3270E_REASON_INV_DEVICE_NAME	3
#define TN3270E_REASON_INV_DEVICE_TYPE	4
#define TN3270E_REASON_TYPE_NAME_ERROR	5
#define TN3270E_REASON_UNKNOWN_ERROR	6
#define TN3270E_REASON_UNSUPPORTED_REQ	7

/* Negotiation function Names. */
#define TN3270E_FUNC_BIND_IMAGE		0
#define TN3270E_FUNC_DATA_STREAM_CTL	1
#define TN3270E_FUNC_RESPONSES		2
#define TN3270E_FUNC_SCS_CTL_CODES	3
#define TN3270E_FUNC_SYSREQ		4
#define TN3270E_FUNC_CONTENTION_RESOLUTION 5
#define TN3270E_FUNC_SNA_SENSE		6

/* Header data type names. */
#define TN3270E_DT_3270_DATA		0x00
#define TN3270E_DT_SCS_DATA		0x01
#define TN3270E_DT_RESPONSE		0x02
#define TN3270E_DT_BIND_IMAGE		0x03
#define TN3270E_DT_UNBIND		0x04
#define TN3270E_DT_NVT_DATA		0x05
#define TN3270E_DT_REQUEST		0x06
#define TN3270E_DT_SSCP_LU_DATA		0x07
#define TN3270E_DT_PRINT_EOJ		0x08
#define TN3270E_DT_BID			0x09

/* Header request flags. */
#define TN3270E_RQF_ERR_COND_CLEARED	0x00
#define TN3270E_RQF_SEND_DATA		0x01
#define TN3270E_RQF_KEYBOARD_RESTORE	0x02
#define TN3270E_RQF_SIGNAL		0x04

/* Header response flags. */
#define TN3270E_RSF_NO_RESPONSE		0x00
#define TN3270E_RSF_ERROR_RESPONSE	0x01
#define TN3270E_RSF_ALWAYS_RESPONSE	0x02
#define TN3270E_RSF_POSITIVE_RESPONSE	0x00
#define TN3270E_RSF_NEGATIVE_RESPONSE	0x01
#define TN3270E_RSF_SNA_SENSE		0x02

/* Header response data. */
#define TN3270E_POS_DEVICE_END		0x00
#define TN3270E_NEG_COMMAND_REJECT	0x00
#define TN3270E_NEG_INTERVENTION_REQUIRED 0x01
#define TN3270E_NEG_OPERATION_CHECK	0x02
#define TN3270E_NEG_COMPONENT_DISCONNECTED 0x03

/* TN3270E data header. */
typedef struct {
    unsigned char data_type;
    unsigned char request_flag;
    unsigned char response_flag;
    unsigned char seq_number[2]; /* actually, 16 bits, unaligned (!) */
} tn3270e_header;

#define EH_SIZE 5

/* UNBIND types. */
#define TN3270E_UNBIND_NORMAL		0x01
#define TN3270E_UNBIND_BIND_FORTHCOMING	0x02
#define TN3270E_UNBIND_VR_INOPERATIVE	0x07
#define TN3270E_UNBIND_RX_INOPERATIVE	0x08
#define TN3270E_UNBIND_HRESET		0x09
#define TN3270E_UNBIND_SSCP_GONE	0x0a
#define TN3270E_UNBIND_VR_DEACTIVATED	0x0b
#define TN3270E_UNBIND_LU_FAILURE_PERM	0x0c
#define TN3270E_UNBIND_LU_FAILURE_TEMP	0x0e
#define TN3270E_UNBIND_CLEANUP		0x0f
#define TN3270E_UNBIND_BAD_SENSE	0xfe
