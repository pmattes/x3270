/*
 * Modifications Copyright 1996-2008 by Paul Mattes.
 * Copyright October 1995 by Dick Altenbern
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
 *	dialogc.h
 *		Global declarations for dialog.c.
 */

#if defined(X3270_MENUS) && defined(X3270_DISPLAY) /*[*/

typedef struct sr {
	struct sr *next;
	Widget w;
	Boolean *bvar1;
	Boolean bval1;
	Boolean *bvar2;
	Boolean bval2;
	Boolean *bvar3;
	Boolean bval3;
	Boolean is_value;
	Boolean has_focus;
} sr_t;

struct toggle_list {                    /* List of toggle widgets */
	Widget *widgets;
};  

typedef enum { T_NUMERIC, T_HOSTFILE, T_UNIXFILE, T_COMMAND } text_t;
extern text_t t_numeric;
extern text_t t_hostfile;
extern text_t t_unixfile;
extern text_t t_command;
extern Boolean s_true, s_false;

extern void dialog_set(sr_t **, Widget);
extern void dialog_apply_bitmap(Widget w, Pixmap p);
extern void dialog_check_sensitivity(Boolean *bvar);
extern void dialog_register_sensitivity(Widget w, Boolean *bvar1,
    Boolean bval1, Boolean *bvar2, Boolean bval2, Boolean *bvar3,
    Boolean bval3);
extern void dialog_flip_toggles(struct toggle_list *toggle_list, Widget w);
extern void dialog_text_callback(Widget w, XtPointer client_data,
    XtPointer call_data _is_unused);
extern void dialog_match_dimension(Widget w1, Widget w2, const char *n);
extern void dialog_mark_toggle(Widget w, Pixmap p);

extern void PA_dialog_focus_action(Widget w, XEvent *event, String *parms,
    Cardinal *num_parms);
extern void PA_dialog_next_action(Widget w, XEvent *event, String *parms,
    Cardinal *num_parms);

#endif /*]*/
