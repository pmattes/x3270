/*
 * Copyright (c) 1993-2012, 2014 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes nor the names of their
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND JEFF SPARKES "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR JEFF SPARKES BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	appres.h
 *		Application resource definitions for x3270, c3270, s3270 and
 *		tcl3270.
 */

/* Toggles */

enum toggle_type {
    TT_INITIAL,		/* at start-up */
    TT_INTERACTIVE,	/* at the prompt */
    TT_ACTION,		/* from a keymap, script or macro */
    TT_XMENU,		/* from a GUI menu */
    TT_FINAL		/* at shutdown */
};
struct toggle {
	Boolean value;		/* toggle value */
	Boolean changed;	/* has the value changed since init */
#if defined(X3270_MENUS) /*[*/
	Widget w[2];		/* the menu item widgets */
	const char *label[2];	/* labels */
#endif /*]*/
	void (*upcall)(struct toggle *, enum toggle_type); /* change value */
};

typedef enum {
    MONOCASE,		/* all-uppercase display */
#if defined(X3270_DISPLAY) /*[*/
    ALT_CURSOR,		/* block cursor */
    CURSOR_BLINK,	/* blinking cursor */
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
    SHOW_TIMING,	/* display command execution time in the OIA */
    CURSOR_POS,		/* display cursor position in the OIA */
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
    TRACING,		/* trace data and events */
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    SCROLL_BAR,		/* include scroll bar */
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
    LINE_WRAP,		/* NVT xterm line-wrap mode (auto-wraparound) */
#endif /*]*/
    BLANK_FILL,		/* treat trailing blanks like NULLs on input */
#if defined(X3270_TRACE) /*[*/
    SCREEN_TRACE,	/* trace screen contents to file or printer */
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
    MARGINED_PASTE,	/* respect left margin when pasting */
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    RECTANGLE_SELECT,	/* select by rectangles */
    CROSSHAIR,		/* display cursor crosshair */
    VISIBLE_CONTROL,	/* display visible control characters */
#endif /*]*/
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
    AID_WAIT,		/* make scripts wait for AIDs to complete */
#endif /*]*/
#if defined(C3270) /*[*/
    UNDERSCORE,		/* special c3270/wc3270 underscore display mode */
#endif /*]*/
    N_TOGGLES
} toggle_index_t;

#define toggled(ix)		(appres.toggle[ix].value)
#define toggle_toggle(t) \
	{ (t)->value = !(t)->value; (t)->changed = True; }

/* Application resources */

typedef struct {
	/* Basic colors */
#if defined(X3270_DISPLAY) /*[*/
	Pixel	foreground;
	Pixel	background;
#endif /*]*/

	/* Options (not toggles) */
#if defined(X3270_INTERACTIVE) && !defined(_WIN32) /*[*/
	Boolean mono;
#endif /*]*/
	Boolean extended;
	Boolean m3279;
	Boolean modified_sel;
	Boolean	once;
#if defined(X3270_DISPLAY) || defined(WC3270) /*[*/
	Boolean visual_bell;
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
	Boolean menubar;
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	Boolean active_icon;
	Boolean label_icon;
	Boolean invert_kpshift;
	Boolean use_cursor_color;
	Boolean allow_resize;
	Boolean no_other;
	Boolean visual_select;
	Boolean suppress_host;
	Boolean suppress_font_menu;
# if defined(X3270_KEYPAD) /*[*/
	Boolean	keypad_on;
# endif /*]*/
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
	Boolean do_confirms;
	Boolean reconnect;
#endif /*]*/
#if defined(C3270) /*[*/
	Boolean all_bold_on;
	Boolean	curses_keypad;
	Boolean cbreak_mode;
	Boolean default_fgbg;
#if !defined(_WIN32) /*[*/
	Boolean reverse_video;
#endif /*]*/
#if defined(_WIN32) /*[*/
	Boolean auto_shortcut;
#endif /*]*/
#endif /*]*/
	Boolean	apl_mode;
	Boolean scripted;
	Boolean numeric_lock;
	Boolean secure;
	Boolean oerr_lock;
	Boolean	typeahead;
	Boolean debug_tracing;
	Boolean disconnect_clear;
	Boolean highlight_bold;
	Boolean color8;
	Boolean bsd_tm;
	Boolean unlock_delay;
	Boolean qr_bg_color;
	Boolean bind_limit;
	Boolean new_environ;
#if defined(X3270_SCRIPT) /*[*/
	Boolean socket;
	int	script_port;
#endif /*]*/

	/* Named resources */
#if defined(X3270_KEYPAD) /*[*/
	char	*keypad;
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
	char	*key_map;
	char	*compose_map;
	char	*printer_lu;
	char	*printer_opts;
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
	int	save_lines;
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	char	*efontname;
	char	*fixed_size;
	char	*icon_font;
	char	*icon_label_font;
	char	*normal_name;
	char	*select_name;
	char	*bold_name;
	char	*colorbg_name;
	char	*keypadbg_name;
	char	*selbg_name;
	char	*cursor_color_name;
	char    *color_scheme;
	int	bell_volume;
	char	*char_class;
	int	modified_sel_color;
	int	visual_select_color;
#if defined(X3270_DBCS) /*[*/
	char	*input_method;
	char	*preedit_type;
#endif /*]*/
#endif /*]*/
#if defined(X3270_DBCS) /*[*/
	char	*dbcs_cgcsgid;
#endif /*]*/
#if defined(C3270) /*[*/
	char	*meta_escape;
	char	*all_bold;
	char	*altscreen;
	char	*defscreen;
#if defined(CURSES_WIDE) /*[*/
	Boolean	acs;
#endif /*]*/
	Boolean ascii_box_draw;
	Boolean mouse;
#endif /*]*/
	char	*conf_dir;
	char	*model;
	char	*hostsfile;
	char	*port;
	char	*charset;
	char	*sbcs_cgcsgid;
	char	*termname;
	char	*devname;	/* for 5250 */
	char	*user;		/* for 5250 */
	char	*login_macro;
	char	*macros;
#if defined(X3270_TRACE) /*[*/
	char	*trace_dir;
	char	*trace_file;
	char	*screentrace_file;
	char	*trace_file_size;
	Boolean  dsTrace_bc;
	Boolean  eventTrace_bc;
# if defined(X3270_DISPLAY) || defined(WC3270) /*[*/
	Boolean	trace_monitor;
# endif /*]*/
#endif /*]*/
	char	*oversize;
#if defined(X3270_FT) /*[*/
	char	*ft_command;
	int	dft_buffer_size;
#endif /*]*/
	char	*connectfile_name;
	char	*idle_command;
	Boolean idle_command_enabled;
	char	*idle_timeout;
#if defined(X3270_SCRIPT) /*[*/
	char	*plugin_command;
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
	char	*ca_dir;
	char	*ca_file;
	char	*cert_file;
	char	*cert_file_type;
	char	*chain_file;
	char	*key_file;
	char	*key_file_type;
	char	*key_passwd;
	char	*accept_hostname;
	Boolean	 self_signed_ok;
	Boolean	 verify_host_cert;
	Boolean	 tls;
#endif /*]*/
	char	*proxy;
#if defined(TCL3270) /*[*/
	int	command_timeout;
#endif /*]*/
	int	unlock_delay_ms;
#if defined(WC3270) /*[*/
	char	*bell_mode;
#endif /*]*/

	/* Toggles */
	struct toggle toggle[N_TOGGLES];

#if defined(X3270_DISPLAY) /*[*/
	/* Simple widget resources */
	Cursor	normal_mcursor;
	Cursor	wait_mcursor;
	Cursor	locked_mcursor;
#endif /*]*/

#if defined(X3270_ANSI) /*[*/
	/* Line-mode TTY parameters */
	Boolean	icrnl;
	Boolean	inlcr;
	Boolean	onlcr;
	char	*erase;
	char	*kill;
	char	*werase;
	char	*rprnt;
	char	*lnext;
	char	*intr;
	char	*quit;
	char	*eof;
#endif /*]*/

	char	*hostname;

#if defined(WC3270) /*[*/
	char	*title;
#endif /*]*/

#if defined(_WIN32) /*[*/
	int	local_cp;
# if defined(X3270_FT) /*[*/
	int	ft_cp;
# endif /*]*/
#endif /*]*/
#if defined(S3270) /*[*/
	Boolean	utf8;
#endif /*]*/

#if defined(USE_APP_DEFAULTS) /*[*/
	/* App-defaults version */
	char	*ad_version;
#endif /*]*/

} AppRes, *AppResptr;

extern AppRes appres;
