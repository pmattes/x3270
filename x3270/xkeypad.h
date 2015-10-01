/*
 * Copyright (c) 1995-2009, 2013-2015 Paul Mattes.
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
 *	xkeypad.h
 *		Global declarations for x3270 keypad.c.
 */

extern bool keypad_changed;
extern bool keypad_popped;

extern Widget keypad_shell;
extern enum kp_placement {
	kp_right, kp_left, kp_bottom, kp_integral, kp_inside_right
} kp_placement;

void ikeypad_destroy(void);
void keypad_first_up(void);
Widget keypad_init(Widget container, Dimension voffset, Dimension screen_width,
	bool floating, bool vert);
void keypad_move(void);
void keypad_placement_init(void);
void keypad_popup_init(void);
Dimension keypad_qheight(void);
void keypad_set_keymap(void);
void keypad_set_temp_keymap(XtTranslations trans);
void keypad_shift(void);
Dimension min_keypad_width(void);
void keypad_popdown(bool *was_up);
void keypad_popup(void);
