/*
 * Copyright 1996, 1999, 2002 by Paul Mattes.
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
 *	keymapc.h
 *		Global declarations for keymap.c.
 */

#define PA_KEYMAP_TRACE		PA_PFX "KeymapTrace"
#define PA_END			PA_PFX "End"

extern struct trans_list *temp_keymaps;

extern char *keymap_trace;

extern void do_keymap_display(Widget w, XtPointer userdata, XtPointer calldata);
extern void keymap_init(const char *km, Boolean interactive);
extern XtTranslations lookup_tt(const char *name, char *table);
extern void PA_End_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_KeymapTrace_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern int temporary_keymap(char *k);
