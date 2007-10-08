/*
 * Copyright 1995, 1996, 1999, 2001, 2002, 2003, 2005, 2006 by Paul Mattes.
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
 *	screenc.h
 *		Global declarations for screen.c.
 */

extern const char *efont_charset;
extern Boolean efont_matches;
extern Dimension main_width;
#if defined(X3270_DBCS) /*[*/
extern const char *efont_charset_dbcs;
extern XIM im;
extern XIC ic;
extern Boolean xim_error;
#endif /*]*/

extern void blink_start(void);
extern void cursor_move(int baddr);
extern unsigned display_height(void);
extern char *display_charset();
extern unsigned display_heightMM(void);
extern unsigned display_width(void);
extern unsigned display_widthMM(void);
extern void enable_cursor(Boolean on);
extern void font_init(void);
extern void icon_init(void);
extern void mcursor_locked(void);
extern void mcursor_normal(void);
extern void mcursor_waiting(void);
extern void PA_ConfigureNotify_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_EnterLeave_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_Expose_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_Focus_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_GraphicsExpose_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_KeymapNotify_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_StateChanged_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_VisibilityNotify_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_WMProtocols_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Redraw_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Title_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void WindowState_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ring_bell(void);
extern void save_00translations(Widget w, XtTranslations *t00);
#define screen_80()
#define screen_132()
extern void screen_change_model(int mn, int ovc, int ovr);
extern void screen_disp(Boolean erasing);
extern void screen_extended(Boolean extended);
extern void screen_flip(void);
extern GC screen_gc(int color);
extern void screen_init(void);
extern GC screen_invgc(int color);
extern void screen_m3279(Boolean m3279);
extern Boolean screen_new_display_charsets(const char *display_charsets,
    const char *csnames);
extern void screen_newcharset(char *csname);
extern void screen_newfont(char *fontname, Boolean do_popup, Boolean is_cs);
extern void screen_newscheme(char *s);
extern Boolean screen_obscured(void);
extern void screen_scroll(void);
extern void screen_set_keymap(void);
extern void screen_set_temp_keymap(XtTranslations trans);
extern void screen_set_thumb(float top, float shown);
extern void screen_showikeypad(Boolean on);
extern void SetFont_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void set_aicon_label(char *l);
extern void set_translations(Widget w, XtTranslations *t00, XtTranslations *t0);
extern void shift_event(int event_state);
extern void toggle_altCursor(struct toggle *t, enum toggle_type tt);
extern void toggle_crosshair(struct toggle *t, enum toggle_type tt);
extern void toggle_cursorBlink(struct toggle *t, enum toggle_type tt);
extern void toggle_cursorPos(struct toggle *t, enum toggle_type tt);
extern void toggle_monocase(struct toggle *t, enum toggle_type tt);
extern void toggle_scrollBar(struct toggle *t, enum toggle_type tt);
extern void toggle_visible_control(struct toggle *t, enum toggle_type tt);

/* font list */
struct font_list {
	char			*label;
	char			**parents;
	char			*font;
	struct font_list	*next;
};
extern struct font_list *font_list;
extern int font_count;
