/*
 * Copyright (c) 1995-2009, Paul Mattes.
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
 *	keypadc.h
 *		Global declarations for keypad.c.
 */

extern Boolean keypad_changed;

#if defined(X3270_KEYPAD) /*[*/

extern enum kp_placement {
	kp_right, kp_left, kp_bottom, kp_integral, kp_inside_right
} kp_placement;

extern void keypad_first_up(void);
extern Widget keypad_init(Widget container, Dimension voffset,
    Dimension screen_width, Boolean floating, Boolean vert);
extern void keypad_move(void);
extern void keypad_placement_init(void);
extern void keypad_popup_init(void);
extern Dimension keypad_qheight(void);
extern void keypad_set_keymap(void);
extern void keypad_set_temp_keymap(XtTranslations trans);
extern void keypad_shift(void);
extern Dimension min_keypad_width(void);
extern void keypad_popdown(Boolean *was_up);
extern void keypad_popup(void);

#else /*][*/

#define keypad_qheight()	0
#define min_keypad_width()	0
#define keypad_first_up()
#define keypad_init(a, b, c, d, e)	0
#define keypad_move()
#define keypad_placement_init()
#define keypad_popup_init()
#define keypad_set_keymap()
#define keypad_set_temp_keymap(n)
#define keypad_shift()
#define keypad_popdown(w)
#define keypad_popup()

#endif /*]*/
