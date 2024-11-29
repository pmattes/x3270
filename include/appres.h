/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990 Jeff Sparkes.
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

/*
 * Alas, a nested #include here, so everyone who wants the appres definitions
 * does not need to explicitly include tls_config.h.
 */
#include "tls_config.h"

/* Application resources */

typedef struct {
    /* Common options. */
    char	*alias;
    bool	 bind_limit;
    bool	 bind_unlock;
    bool	 bsd_tm;
    char	*charset;	/* deprecated */
    char	*codepage;
    char	*conf_dir;
    int		 connect_timeout;
    char	*connectfile_name;
    bool	 contention_resolution;
    char	*dbcs_cgcsgid;
    bool	 debug_tracing;
    char	*devname;	/* for 5250 */
    bool	 disconnect_clear;
    bool	 extended_data_stream;
    char	*ft_command;
#if defined(_WIN32) /*[*/
    int		 ft_cp;
#endif /*]*/
    bool	 highlight_bold;
    char	*hostname;
    char	*hostsfile;
    char	*httpd_port;
    char	*idle_command;
    bool	 idle_command_enabled;
    char	*idle_timeout;
#if defined(_WIN32) /*[*/
    int		 local_cp;
#endif /*]*/
    char	*login_macro;
    bool	 oerr_lock;
    char	*macros;
    int	 	 max_recent;
    char	*min_version;
    char	*model;
    bool	 modified_sel;
    bool	 new_environ;
    int		 nop_seconds;
    bool	 numeric_lock;
    bool	 nvt_mode;
    bool	 once;
    char	*oversize;
    char	*port;
    bool	 prefer_ipv4;
    bool	 prefer_ipv6;
    char	*proxy;
    bool	 qr_bg_color;
    bool	 reconnect;
    bool	 retry;
    char	*sbcs_cgcsgid;
    char	*script_port;
    bool	 script_port_once;
    bool	 scripted;
    bool	 scripted_always;
    bool	 secure;
    bool	 socket;
    char	*suppress_actions;
    char	*termname;
    char	*trace_dir;
    char	*trace_file;
    char	*trace_file_size;
    bool	 trace_monitor;
    bool	 unlock_delay;
    int		 unlock_delay_ms;
    char	*user;		/* for 5250 */
    bool	 utf8;
    bool	 wrong_terminal_name;
    bool	 tls992;
    char	*cookie_file;
    bool	 ut_env;

    /* Toggles. */
    bool toggle[N_TOGGLES];

    /* Line-mode TTY parameters. */
    struct {
	char	*eof;
	char	*erase;
	bool	 icrnl;
	bool	 inlcr;
	char	*intr;
	char	*kill;
	char	*lnext;
	bool	 onlcr;
	char	*quit;
	char	*rprnt;
	char	*werase;
    } linemode;

    /* TLS fields. */
    tls_config_t tls;

    /* Interactive (x3270/c3270/wc3270/b3270) fields. */
    struct {
	char	*compose_map;
	char	*console;
	char	*crosshair_color;
	bool	 do_confirms;
	char	*key_map;
	bool	 menubar;
	bool	 mono;
	char	*no_telnet_input_mode;
	bool	 print_dialog;	/* Windows only */
	char	*printer_lu;
	char	*printer_opts;
	int	 save_lines;
	bool	 visual_bell;
    } interactive;

    /* File transfer fields. */
    struct {
	char	*allocation;
	int	 avblock;
	int	 blksize;
#if defined(_WIN32) /*[*/
	int	 codepage;
#endif /*]*/
	char	*cr;
	int	 dft_buffer_size;
	char	*direction;
	char	*exist;
	char	*host;
	char	*host_file;
	char	*local_file;
	int	 lrecl;
	char	*mode;
	int	 primary_space;
	char	*recfm;
	char	*remap;
	int	 secondary_space;
	char	*other_options;
    } ft;

    /* c3270/wc3270-specific fields. */
    struct {
	char	*all_bold;
	bool	 all_bold_on;
#if !defined(_WIN32) /*[*/
	char	*altscreen;
#endif /*]*/
	bool	 ascii_box_draw;
	bool	 acs;
#if defined(_WIN32) /*[*/
	bool	 auto_shortcut;
	char	*bell_mode;
#endif /*]*/
#if !defined(_WIN32) /*[*/
	bool	 cbreak_mode;
	bool	 curses_keypad;
	bool	 default_fgbg;
	char	*defscreen;
#endif /*]*/
#if defined(_WIN32) /*[*/
	bool	 lightpen_primary;
#endif /*]*/
#if !defined(_WIN32) /*[*/
	char	*meta_escape;
	bool	 mouse;
	bool	 reverse_video;
#endif /*]*/
#if defined(_WIN32) /*[*/
	char	*title;
#endif /*]*/
    } c3270;

    /* screen tracing */
    struct {
	char	*file;
	char	*target;
	char	*type;
    } screentrace;

    /* scripting-specific fields. */
    struct {
	char	*callback;
    } scripting;

    /* b3270-specific fields. */
    struct {
	bool	indent;
	bool	json;
	bool	wrapper_doc;
    } b3270;

} AppRes, *AppResptr;

extern AppRes appres;
