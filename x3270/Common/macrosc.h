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
 *	macrosc.h
 *		Global declarations for macros.c.
 */

/* macro definition */
struct macro_def {
	char			*name;
	char			**parents;
	char			*action;
	struct macro_def	*next;
};
extern struct macro_def *macro_defs;
extern Boolean macro_output;

extern void abort_script(void);
extern void Abort_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void AnsiText_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void AsciiField_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Ascii_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
#if defined(X3270_SCRIPT) /*[*/
extern void cancel_if_idle_command(void);
#else /*][*/
#define cancel_if_idle_command()
#endif /*]*/
extern void Bell_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void CloseScript_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ContinueScript_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void EbcdicField_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Ebcdic_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Execute_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void execute_action_option(Widget w, XtPointer client_data,
    XtPointer call_data);
extern void Expect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
#if defined(X3270_SCRIPT) && defined(X3270_PLUGIN) /*[*/
extern void plugin_aid(unsigned char aid);
#else /*][*/
#define plugin_aid(a)
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
extern void Plugin_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
#endif /*]*/
extern void login_macro(char *s);
extern void macros_init(void);
extern void Macro_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void macro_command(struct macro_def *m);
extern void PauseScript_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void peer_script_init(void);
extern void ps_set(char *s, Boolean is_hex);
extern void Printer_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void push_command(char *);
extern void push_idle(char *);
extern void push_keymap_action(char *);
extern void push_macro(char *, Boolean);
extern void Query_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ReadBuffer_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Script_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
#if defined(X3270_SCRIPT) /*[*/
extern void sms_accumulate_time(struct timeval *, struct timeval *);
#else /*][*/
#define sms_accumulate_time(a, b)
#endif /*]*/
extern Boolean sms_active(void);
extern void sms_connect_wait(void);
extern void sms_continue(void);
extern void sms_error(const char *msg);
extern void sms_host_output(void);
extern void sms_info(const char *fmt, ...) printflike(1, 2);
extern void sms_init(void);
extern Boolean sms_in_macro(void);
extern Boolean sms_redirect(void);
extern void sms_store(unsigned char c);
#if defined(X3270_SCRIPT) || defined(TCL3270) || defined(S3270) /*[*/
extern void Snap_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
#endif /*]*/
#if defined(TCL3270) /*[*/
extern void Status_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
#endif /*]*/
extern void Source_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Wait_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
