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
 *	screenc.h
 *		Global declarations for screen.c.
 */

extern Boolean efont_changed;
extern const char *efont_charset;
extern Boolean efont_matches;
extern Pixmap icon;
extern Dimension main_width;
extern Boolean model_changed;
extern Boolean oversize_changed;
extern Boolean scheme_changed;
extern Window *screen_window;
extern Boolean scrollbar_changed;
extern const char *efont_charset_dbcs;
extern XIM im;
extern XIC ic;
extern Boolean xim_error;

void blink_start(void);
void cursor_move(int baddr);
unsigned display_height(void);
char *display_charset();
unsigned display_heightMM(void);
unsigned display_width(void);
unsigned display_widthMM(void);
void enable_cursor(Boolean on);
void font_init(void);
void icon_init(void);
void mcursor_locked(void);
void mcursor_normal(void);
void mcursor_waiting(void);
void PA_ConfigureNotify_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_EnterLeave_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_Expose_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_Focus_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_GraphicsExpose_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_KeymapNotify_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_StateChanged_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_VisibilityNotify_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void PA_WMProtocols_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void Redraw_xaction(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
void ring_bell(void);
void save_00translations(Widget w, XtTranslations *t00);
#define screen_80()
#define screen_132()
void screen_change_model(int mn, int ovc, int ovr);
void screen_disp(Boolean erasing);
void screen_extended(Boolean extended);
void screen_flip(void);
GC screen_gc(int color);
void screen_init(void);
GC screen_invgc(int color);
void screen_m3279(Boolean m3279);
Boolean screen_new_display_charsets(const char *display_charsets,
    const char *csnames);
void screen_newcharset(char *csname);
void screen_newfont(const char *fontname, Boolean do_popup,
	Boolean is_cs);
void screen_newscheme(char *s);
Boolean screen_obscured(void);
void screen_preinit(void);
void screen_scroll(void);
void screen_set_keymap(void);
void screen_set_temp_keymap(XtTranslations trans);
void screen_set_thumb(float top, float shown);
void screen_showikeypad(Boolean on);
void set_aicon_label(char *l);
void set_translations(Widget w, XtTranslations *t00, XtTranslations *t0);
void shift_event(int event_state);
unsigned long screen_window_number(void);
void screen_register(void);

/* font list */
struct font_list {
	char			*label;
	char			**parents;
	char			*font;
	struct font_list	*next;
	char			*mlabel;
};
struct font_list *font_list;
int font_count;

#define screen_has_bg_color()	False
