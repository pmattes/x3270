/*
 * Copyright (c) 1999-2010, 2014-2015 Paul Mattes.
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

/* c3270 version of screenc.h */

#define blink_start()
#define display_heightMM()	100
#define display_height()	1
#define display_widthMM()	100
#define display_width()		1
#define mcursor_locked()
#define mcursor_normal()
#define mcursor_waiting()
#define screen_obscured()	False
#define screen_scroll()

void cursor_move(int baddr);
void ring_bell(void);
void screen_132(void);
void screen_80(void);
void screen_disp(Boolean erasing);
void screen_init(void);
void screen_flip(void);
void screen_resume(void);
Boolean screen_suspend(void);
FILE *start_pager(void);
#if defined(WC3270) /*[*/
void screen_fixup(void);
void pager_output(const char *s);
Boolean screen_wait_for_key(char *c);
#endif /*]*/
#define screen_set_thumb(top, shown)
#define enable_cursor(on)

extern Boolean escaped;

#if defined(WC3270) /*[*/
void screen_title(const char *text);
extern int windows_cp;
#endif /*]*/

void screen_final(void);

#define screen_window_number()	0L

#define screen_has_bg_color()	True
