/*
 * Copyright (c) 1994-2015 Paul Mattes.
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
 *	fprint_screen.h
 *		Screen printing functions.
 */

#define FPS_EVEN_IF_EMPTY	0x1	/* print even if screen is blank */
#define FPS_MODIFIED_ITALIC	0x2	/* print modified fields in italic */
#define FPS_FF_SEP		0x4	/* use FFs to divide pages in text */
#define FPS_NO_HEADER		0x8	/* do not generate HTML header */
#define FPS_NO_DIALOG		0x10	/* do not use Windows print dialog */

typedef struct _fps *fps_t;

typedef enum {
	FPS_STATUS_SUCCESS = 0,
	FPS_STATUS_SUCCESS_WRITTEN = 1,
	FPS_STATUS_ERROR = -1,
	FPS_STATUS_CANCEL = -2
} fps_status_t;
#define FPS_IS_ERROR(fps) ((int)fps < 0)

fps_status_t fprint_screen(FILE *f, ptype_t ptype, unsigned opts,
	const char *caption, const char *printer_name);
fps_status_t fprint_screen_start(FILE *f, ptype_t ptype, unsigned opts,
	const char *caption, const char *printer_name, fps_t *fps);
fps_status_t fprint_screen_body(fps_t fps);
fps_status_t fprint_screen_done(fps_t *fps);
