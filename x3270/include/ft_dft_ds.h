/*
 * Copyright (c) 1996-2009, 2015 Paul Mattes.
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
 *	ft_dft_ds.h
 *		DFT-style file transfer codes.
 */

/* Host requests. */
#define TR_OPEN_REQ		0x0012	/* open request */
#define TR_CLOSE_REQ		0x4112	/* close request */

#define TR_SET_CUR_REQ		0x4511	/* set cursor request */
#define TR_GET_REQ		0x4611	/* get request */
#define TR_INSERT_REQ		0x4711	/* insert request */

#define TR_DATA_INSERT		0x4704	/* data to insert */

/* PC replies. */
#define TR_GET_REPLY		0x4605	/* data for get */
#define TR_NORMAL_REPLY		0x4705	/* insert normal reply */
#define TR_ERROR_REPLY		0x08	/* error reply (low 8 bits) */
#define TR_CLOSE_REPLY		0x4109	/* close acknowledgement */

/* Other headers. */
#define TR_RECNUM_HDR		0x6306	/* record number header */
#define TR_ERROR_HDR		0x6904	/* error header */
#define TR_NOT_COMPRESSED	0xc080	/* data not compressed */
#define TR_BEGIN_DATA		0x61	/* beginning of data */

/* Error codes. */
#define TR_ERR_EOF		0x2200	/* get past end of file */
#define TR_ERR_CMDFAIL		0x0100	/* command failed */
