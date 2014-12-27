/*
 * Copyright (c) 1995-2010, 2013-2014 Paul Mattes.
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
 *	menubarc.h
 *		Global declarations for menubar.c.
 */

#if defined(X3270_DISPLAY) /*[*/

/* x3270 externs. */
extern Pixmap dot, no_dot;
extern Pixmap diamond, no_diamond;
extern Pixmap null;

extern void HandleMenu_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void menubar_as_set(Boolean sensitive);
extern void menubar_init(Widget container, Dimension overall_width,
    Dimension current_width);
extern void menubar_keypad_changed(void);
extern Dimension menubar_qheight(Dimension container_width);
extern void menubar_resize(Dimension width);
extern void menubar_retoggle(struct toggle *t, int ix);

#elif defined(C3270) /*][*/

typedef enum {
    MK_MOUSE = 1,	/* ncurses mouse event */
    MK_UP,		/* cursor up */
    MK_DOWN,		/* cursor down */
    MK_LEFT,		/* cursor left */
    MK_RIGHT,		/* cursor right */
    MK_HOME,		/* home */
    MK_END,		/* end */
    MK_ENTER,		/* enter or return */
    MK_NONE,		/* no symbolic key code */
    MK_OTHER		/* anything else */
} menu_key_t;

# define menubar_as_set(n)

/* c3270 externs. */
# define MENU_IS_UP	0x1
# define KEYPAD_IS_UP	0x2
extern unsigned menu_is_up;
extern void menu_init(void);
extern Boolean menu_char(int row, int col, Boolean persistent, ucs4_t *u,
	Boolean *highlighted, unsigned char *acs);
extern void menu_key(menu_key_t k, ucs4_t u);
# if defined(_WIN32) /*[*/
extern void menu_click(int x, int y);
# endif /*]*/
extern void popup_menu(int x, int click);
extern void menu_cursor(int *row, int *col);
extern void menubar_retoggle(struct toggle *t, int ix);
extern void map_acs(unsigned char c, ucs4_t *u, unsigned char *acs);
extern action_t Menu_action;

#else /*][*/
# define menubar_as_set(n)
# define menubar_retoggle(t, ix)
#endif /*]*/
