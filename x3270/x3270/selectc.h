/*
 * Copyright 1995, 1999, 2000, 2001, 2004 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
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
