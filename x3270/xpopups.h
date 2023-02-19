/*
 * Copyright (c) 1995-2023 Paul Mattes.
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
 *	xpopups.h
 *		x3270-specific global declarations for popups.c.
 */

/* window placement enumeration */
enum placement { Center, Bottom, Left, Right, InsideRight };
extern enum placement *CenterP;
extern enum placement *BottomP;
extern enum placement *LeftP;
extern enum placement *RightP;
extern enum placement *InsideRightP;

/* form input editing enumeration */
enum form_type { FORM_NO_WHITE, FORM_NO_CC, FORM_AS_IS };

Widget create_form_popup(const char *name, XtCallbackProc callback,
	XtCallbackProc callback2, enum form_type form_type);
void error_init(void);
void error_popup_init(void);
bool error_popup_visible(void);
void error_popup_resume(void);
void info_popup_init(void);
void PA_confirm_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void place_popup(Widget w, XtPointer client_data, XtPointer call_data);
void move_popup(Widget w, XtPointer client_data, XtPointer call_data);
void popdown_an_error(void);
void popup_popup(Widget shell, XtGrabKind grab);
void popups_move(void);
void printer_popup_init(void);
void toplevel_geometry(Position *x, Position *y, Dimension *width,
	Dimension *height);
void add_error_popdown_callback(void (*callback)(void));
void popups_register(void);
void popup_a_timed_info(int timeout_ms, const char *format, ...);
void unplace_popup(Widget w);
