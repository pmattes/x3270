/*
 * Copyright 1995-2008 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
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
