/*
 * Copyright (c) 1995-2009, 2014-2015 Paul Mattes.
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

void insert_selection_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void Cut_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void KybdSelect_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
int mouse_baddr(Widget w, XEvent *e);
void move_select_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void SelectAll_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void SelectDown_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void reclass(char *s);
void SelectDown_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void SelectMotion_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void SelectUp_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void select_end_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void select_extend_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void select_start_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void set_select_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void start_extend_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void Unselect_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
