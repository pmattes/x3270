/*
 * Copyright (c) 1999-2009, Paul Mattes.
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
extern void screen_suspend(void);
extern FILE *start_pager(void);
#if defined(WC3270) /*[*/
extern void pager_output(const char *s);
extern Boolean screen_wait_for_key(void);
#endif /*]*/
extern void toggle_monocase(struct toggle *t, enum toggle_type tt);

extern Boolean escaped;

extern void Escape_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Help_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Redraw_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Trace_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Show_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);

#if defined(WC3270) /*[*/
extern void Paste_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Title_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void screen_title(char *text);
extern int windows_cp;
#endif /*]*/

#if defined(C3270) /*[*/
extern void toggle_underscore(struct toggle *t, enum toggle_type type);
#endif /*]*/
