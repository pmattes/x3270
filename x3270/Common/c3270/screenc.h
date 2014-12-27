/*
 * Copyright (c) 1999-2010, 2014 Paul Mattes.
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

extern void cursor_move(int baddr);
extern void ring_bell(void);
extern void screen_132(void);
extern void screen_80(void);
extern void screen_disp(Boolean erasing);
extern void screen_init(void);
extern void screen_flip(void);
extern void screen_resume(void);
extern Boolean screen_suspend(void);
extern FILE *start_pager(void);
#if defined(WC3270) /*[*/
extern void screen_fixup(void);
extern void pager_output(const char *s);
extern Boolean screen_wait_for_key(char *c);
#endif /*]*/
extern void toggle_monocase(struct toggle *t, enum toggle_type tt);
#define screen_set_thumb(top, shown)
#define enable_cursor(on)

extern Boolean escaped;

extern action_t Escape_action;
extern action_t Help_action;
extern action_t Redraw_action;
extern action_t ScreenTrace_action;
extern action_t Show_action;
extern action_t Trace_action;

#if defined(WC3270) /*[*/
extern action_t Paste_action;
extern action_t Title_action;
extern void screen_title(const char *text);
extern int windows_cp;
#endif /*]*/

#if defined(C3270) /*[*/
extern void toggle_underscore(struct toggle *t, enum toggle_type type);
extern void screen_final(void);
#endif /*]*/

#define screen_window_number()	0L
