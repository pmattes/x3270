/*
 * Copyright (c) 1993-2012, 2015 Paul Mattes.
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

/* Application resources */

typedef struct {
    /* Common options. */
    Boolean	 extended;
    Boolean	 m3279;
    Boolean	 modified_sel;
    Boolean	 once;
    Boolean	 apl_mode;
    Boolean	 scripted;
    Boolean	 numeric_lock;
    Boolean	 secure;
    Boolean	 oerr_lock;
    Boolean	 typeahead;
    Boolean	 debug_tracing;
    Boolean	 disconnect_clear;
    Boolean	 highlight_bold;
    Boolean	 color8;
    Boolean	 bsd_tm;
    Boolean	 unlock_delay;
    Boolean	 qr_bg_color;
    Boolean	 bind_limit;
    Boolean	 new_environ;
    Boolean	 socket;
    Boolean	 dsTrace_bc;
    Boolean	 eventTrace_bc;
    Boolean	 trace_monitor;
    char	*script_port;
    char	*httpd_port;
    char	*dbcs_cgcsgid;
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
    char	*trace_dir;
    char	*trace_file;
    char	*screentrace_file;
    char	*trace_file_size;
    char	*oversize;
    char	*ft_command;
    int		 dft_buffer_size;
    char	*connectfile_name;
    char	*idle_command;
    Boolean	 idle_command_enabled;
    char	*idle_timeout;
    char	*proxy;
    int		 unlock_delay_ms;
    char	*hostname;
    Boolean	 utf8;
    int	 	 max_recent;
#if defined(_WIN32) /*[*/
    int		 local_cp;
    int		 ft_cp;
#endif /*]*/

    /* Toggles. */
    Boolean toggle[N_TOGGLES];

    /* Line-mode TTY parameters. */
    struct {
	Boolean	 icrnl;
	Boolean	 inlcr;
	Boolean	 onlcr;
	char	*erase;
	char	*kill;
	char	*werase;
	char	*rprnt;
	char	*lnext;
	char	*intr;
	char	*quit;
	char	*eof;
    } linemode;

    /* x3270 fields. */
    struct {
	Boolean	 active_icon;
	Boolean	 label_icon;
	Boolean	 invert_kpshift;
	Boolean	 use_cursor_color;
	Boolean	 allow_resize;
	Boolean	 no_other;
	Boolean	 visual_select;
	Boolean	 suppress_host;
	Boolean	 suppress_font_menu;
	Boolean	 keypad_on;
	char	*keypad;
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
	char	*color_scheme;
	int	 bell_volume;
	char	*char_class;
	int	 modified_sel_color;
	int	 visual_select_color;
	char	*input_method;
	char	*preedit_type;
	char	*ad_version;
    } x3270;

    /* SSL fields. */
    struct {
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
    } ssl;

    /* Interactive (x3270/c3270/wc3270) fields. */
    struct {
	Boolean	 mono;
	Boolean	 reconnect;
	Boolean	 do_confirms;
	Boolean	 menubar;
	Boolean	 visual_bell;
	char	*key_map;
	char	*compose_map;
	char	*printer_lu;
	char	*printer_opts;
	int	 save_lines;
    } interactive;

    /* c3270/wc3270-specific fields. */
    struct {
	Boolean	 all_bold_on;
	Boolean	 ascii_box_draw;
	Boolean	 acs;
#if !defined(_WIN32) /*[*/
	Boolean	 default_fgbg;
	Boolean	 cbreak_mode;
	Boolean	 curses_keypad;
	Boolean	 mouse;
	Boolean	 reverse_video;
#else /*]*/
	Boolean	 auto_shortcut;
#endif /*]*/

	char	*all_bold;
#if !defined(_WIN32) /*[*/
	char	*altscreen;
	char	*defscreen;
	char	*meta_escape;
#else /*][*/
	char	*bell_mode;
	char	*title;
#endif /*]*/
    } c3270;

    /* tcl3270-specific fields. */
    struct {
	int	 command_timeout;
    } tcl3270;

} AppRes, *AppResptr;

extern AppRes appres;
