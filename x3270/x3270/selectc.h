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
 *	selectc.h
 *		Global declarations for select.c.
 */

extern Boolean area_is_selected(int baddr, int len);
extern void insert_selection_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Cut_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void KybdSelect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern int mouse_baddr(Widget w, XEvent *e);
extern void move_select_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void SelectAll_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void SelectDown_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void reclass(char *s);
extern void SelectDown_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void SelectMotion_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void SelectUp_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void select_end_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void select_extend_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void select_start_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void set_select_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void start_extend_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void unselect(int baddr, int len);
extern void Unselect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
