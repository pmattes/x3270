/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR
 * GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	@file xscreen.h
 *
 *		Screen definitions for x3270.
 */

#define fCHAR_WIDTH(f)	((f)->max_bounds.width)
#define fCHAR_HEIGHT(f)	((f)->ascent + (f)->descent)

#define HHALO  rescale(2)       /* number of pixels to pad screen left-right */
#define VHALO  rescale(1)       /* number of pixels to pad screen top-bottom */

#define cwX_TO_COL(x_pos, cw) 	(((x_pos)-hhalo) / (cw))
#define chY_TO_ROW(y_pos, ch) 	(((y_pos)-vhalo) / (ch))
#define cwCOL_TO_X(col, cw, _hhalo)	(((col) * (cw)) + _hhalo)
#define chROW_TO_Y(row, ch, _vhalo)	(((row)+1) * (ch) + _vhalo)

#define ssX_TO_COL(x_pos) 	cwX_TO_COL(x_pos, ss->char_width)
#define ssY_TO_ROW(y_pos) 	chY_TO_ROW(y_pos, ss->char_height)
#define ssCOL_TO_X(col)		cwCOL_TO_X(col, ss->char_width, hhalo)
#define ssROW_TO_Y(row)		chROW_TO_Y(row, ss->char_height, vhalo)

#define X_TO_COL(x_pos) 	cwX_TO_COL(x_pos, *char_width)
#define Y_TO_ROW(y_pos) 	chY_TO_ROW(y_pos, *char_height)
#define COL_TO_X(col)		cwCOL_TO_X(col, *char_width, hhalo)
#define ROW_TO_Y(row)		chROW_TO_Y(row, *char_height, vhalo)

#define SGAP(descent)	(descent+3) 	/* gap between screen and status line */

#define SCREEN_WIDTH(cw, _hhalo) \
    			(cwCOL_TO_X(maxCOLS, cw, _hhalo) + _hhalo)
#define SCREEN_HEIGHT(ch, descent, _vhalo) \
			(chROW_TO_Y(maxROWS, ch, _vhalo) + \
			 _vhalo+SGAP(descent)+_vhalo)

/* keyboard modifer bitmap */
#define ShiftKeyDown 0x01
#define MetaKeyDown  0x02
#define AltKeyDown   0x04
#define AplMode      0x08

/* selections */

void screen_set_select(int baddr);
void screen_unselect_all(void);

/*
 * Screen position structure.
 */
struct sp {
    union {
	struct {
	    unsigned ec  : 8;	/* EBCDIC character code */
	    unsigned sel : 1;	/* selection status */
	    unsigned fg  : 6;	/* foreground color (flag/inv/0-15) */
	    unsigned gr  : 4;	/* graphic rendition */
	    unsigned cs  : 3;	/* character set */
	} bits;
	unsigned long word;
    } u;
    ucs4_t ucs4;		/* NVT-mode character */
};

/*
 * screen.c data structures. *
 */
extern int	 *ascent, *descent;
extern Dimension *screen_height;
extern unsigned	 fixed_width, fixed_height;
extern int       hhalo, vhalo;
extern Widget	 *screen;

extern bool efont_changed;
extern char *efont_charset;
extern bool efont_matches;
extern Pixmap x3270_icon;
extern Dimension main_width;
extern bool model_changed;
extern bool oversize_changed;
extern bool scheme_changed;
extern Window *screen_window;
extern bool scrollbar_changed;
extern char *efont_charset_dbcs;
extern XIM im;
extern XIC ic;
extern bool xim_error;

char *display_charset(void);
void font_init(void);
void icon_init(void);
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
void StepEfont_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params);
void save_00translations(Widget w, XtTranslations *t00);
GC screen_crosshair_gc(void);
void screen_disp(bool erasing);
void screen_extended(bool extended);
GC screen_gc(int color);
GC screen_invgc(int color);
void screen_m3279(bool m3279);
void screen_newcodepage(char *cpname);
void screen_newfont(const char *fontname, bool do_popup, bool is_cs);
void screen_newscheme(char *s);
bool screen_obscured(void);
void screen_preinit(void);
void screen_remodel(int mn, int ovc, int ovr);
void screen_set_keymap(void);
void screen_set_temp_keymap(XtTranslations trans);
void screen_showikeypad(bool on);
void screen_snap_size(void);
void set_aicon_label(char *l);
void set_translations(Widget w, XtTranslations *t00, XtTranslations *t0);
void shift_event(int event_state);
void screen_register(void);
XChar2b screen_vcrosshair(void);
Dimension rescale(Dimension d);
void screen_set_title(const char *title);

/* font list */
struct font_list {
    char		*label;
    char		**parents;
    char		*font;
    struct font_list	*next;
    char		*mlabel;
};
extern struct font_list *font_list;
extern int font_count;
