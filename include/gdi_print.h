/*
 * Copyright (c) 2014-2015, 2018 Paul Mattes.
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
 *	gdi_print.h
 *		GDI screen printing functions.
 */

#if !defined(_WIN32) /*[*/
#error For Windows ony
#endif /*]*/

/* Header for screen snapshots. */
typedef struct {
	unsigned signature;	/* Signature, to make sure we haven't gotten
				   lost */
	unsigned short rows;	/* Rows */
	unsigned short cols;	/* Columns */
} gdi_header_t;

/* Signature for GDI snapshot files. */
#define GDI_SIGNATURE		0x33323730

typedef enum {
	GDI_STATUS_SUCCESS = 0,
	GDI_STATUS_WAIT = 1,
	GDI_STATUS_ERROR = -1,
	GDI_STATUS_CANCEL = -2
} gdi_status_t;
#define GDI_STATUS_IS_ERROR(gs) ((int)gs < 0)

gdi_status_t gdi_print_start(const char *printer_name, unsigned opts,
	void *wait_context);
gdi_status_t gdi_print_finish(FILE *f, const char *caption);
