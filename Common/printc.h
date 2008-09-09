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
 *	printc.h
 *		Global declarations for print.c.
 */

typedef enum { P_TEXT, P_HTML, P_RTF } ptype_t;
extern Boolean fprint_screen(FILE *f, Boolean even_if_empty, ptype_t ptype);
extern void PrintText_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PrintWindow_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void print_text_option(Widget w, XtPointer client_data,
    XtPointer call_data);
extern void print_window_option(Widget w, XtPointer client_data,
    XtPointer call_data);
extern void save_text_option(Widget w, XtPointer client_data,
    XtPointer call_data);

