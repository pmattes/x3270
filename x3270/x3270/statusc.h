/*
 * Copyright (c) 1995-2009, Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	statusc.h
 *		Global declarations for status.c.
 */

extern void status_compose(Boolean on, unsigned char c, enum keytype keytype);
extern void status_ctlr_done(void);
extern void status_cursor_pos(int ca);
extern void status_disp(void);
extern void status_init(void);
extern void status_insert_mode(Boolean on);
extern void status_kmap(Boolean on);
extern void status_kybdlock(void);
extern void status_lu(const char *);
extern void status_minus(void);
extern void status_oerr(int error_type);
extern void status_reinit(unsigned cmask);
extern void status_reset(void);
extern void status_reverse_mode(Boolean on);
extern void status_script(Boolean on);
extern void status_scrolled(int n);
extern void status_shift_mode(int state);
extern void status_syswait(void);
extern void status_timing(struct timeval *t0, struct timeval *t1);
extern void status_touch(void);
extern void status_twait(void);
extern void status_typeahead(Boolean on);
extern void status_uncursor_pos(void);
extern void status_untiming(void);
