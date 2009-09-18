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
 *	menubarc.h
 *		Global declarations for menubar.c.
 */

#if defined(X3270_MENUS) /*[*/

extern void HandleMenu_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
#if defined(X3270_SCRIPT) /*[*/
extern void menubar_as_set(Boolean sensitive);
#else /*][*/
#define menubar_as_set(n)
#endif /*]*/
extern void menubar_init(Widget container, Dimension overall_width,
    Dimension current_width);
extern void menubar_keypad_changed(void);
extern Dimension menubar_qheight(Dimension container_width);
extern void menubar_resize(Dimension width);
extern void menubar_retoggle(struct toggle *t);

#else /*][*/

#define menubar_as_set(n)
#define menubar_init(a, b, c)
#define menubar_keypad_changed()
#define menubar_qheight(n)	0
#define menubar_resize(n)
#define menubar_retoggle(t)
#define HandleMenu_action ignore_action

#endif /*]*/
