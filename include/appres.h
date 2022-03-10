/*
 * Copyright (c) 1993-2012, 2016-2022 Paul Mattes.
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

/*
 * Alas, a nested #include here, so everyone who wants the appres definitions
 * does not need to explicitly include tls_config.h.
 */
#include "tls_config.h"

/* Application resources */

typedef struct {
    /* Common options. */
    bool	 modified_sel;
    bool	 once;
    bool	 scripted;
    bool	 numeric_lock;
    bool	 secure;
    bool	 oerr_lock;
    bool	 debug_tracing;
    bool	 disconnect_clear;
    bool	 highlight_bold;
    bool	 bsd_tm;
    bool	 unlock_delay;
    bool	 qr_bg_color;
    bool	 bind_limit;
    bool	 new_environ;
    bool	 socket;
    bool	 trace_monitor;
    bool	 script_port_once;
    bool	 bind_unlock;
    bool	 contention_resolution;
    bool	 scripted_always;
    bool	 prefer_ipv4;
    bool	 prefer_ipv6;
    char	*script_port;
    char	*httpd_port;
    char	*dbcs_cgcsgid;
    char	*conf_dir;
    char	*model;
    char	*hostsfile;
    char	*port;
    char	*codepage;
    char	*charset;	/* deprecated */
    char	*sbcs_cgcsgid;
    char	*termname;
    char	*devname;	/* for 5250 */
    char	*user;		/* for 5250 */
    char	*login_macro;
    char	*macros;
    char	*trace_dir;
    char	*trace_file;
    char	*trace_file_size;
    char	*oversize;
    char	*ft_command;
    char	*connectfile_name;
    char	*idle_command;
    bool	 idle_command_enabled;
    char	*idle_timeout;
    char	*proxy;
    int		 unlock_delay_ms;
    char	*hostname;
    bool	 utf8;
    int	 	 max_recent;
    bool	 nvt_mode;
    char	*suppress_actions;
    char	*min_version;
    int		 connect_timeout;
    int		 nop_seconds;
    char	*alias;
#if defined(_WIN32) /*[*/
    int		 local_cp;
    int		 ft_cp;
#endif /*]*/

    /* Toggles. */
    bool toggle[N_TOGGLES];

    /* Line-mode TTY parameters. */
    struct {
	bool	 icrnl;
	bool	 inlcr;
	bool	 onlcr;
	char	*erase;
	char	*kill;
	char	*werase;
	char	*rprnt;
	char	*lnext;
	char	*intr;
	char	*quit;
	char	*eof;
    } linemode;

    /* TLS fields. */
    tls_config_t tls;

    /* Interactive (x3270/c3270/wc3270) fields. */
    struct {
	bool	 mono;
	bool	 reconnect;
	bool	 do_confirms;
	bool	 menubar;
	bool	 visual_bell;
	char	*key_map;
	char	*compose_map;
	char	*printer_lu;
	char	*printer_opts;
	int	 save_lines;
	char	*crosshair_color;
	char	*console;
	bool	 print_dialog;	/* Windows only */
	char	*no_telnet_input_mode;
    } interactive;

    /* File transfer fields. */
    struct {
	char	*allocation;
	int	 avblock;
	int	 blksize;
	char	*cr;
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
	int	 dft_buffer_size;
#if defined(_WIN32) /*[*/
	int	 codepage;
#endif /*]*/
    } ft;

    /* c3270/wc3270-specific fields. */
    struct {
	bool	 all_bold_on;
	bool	 ascii_box_draw;
	bool	 acs;
#if !defined(_WIN32) /*[*/
	bool	 default_fgbg;
	bool	 cbreak_mode;
	bool	 curses_keypad;
	bool	 mouse;
	bool	 reverse_video;
#else /*]*/
	bool	 auto_shortcut;
	bool	 lightpen_primary;
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
	bool	json;
	bool	indent;
	bool	wrapper_doc;
    } b3270;

} AppRes, *AppResptr;

extern AppRes appres;
