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
 *	xappres.h
 *		x3270-specific application resource definitions
 */

typedef struct {
    /* Basic colors */
    Pixel	 foreground;
    Pixel	 background;

    /* Simple widget resources */
    Cursor	 normal_mcursor;
    Cursor	 wait_mcursor;
    Cursor	 locked_mcursor;

    /* Miscellany. */
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
    Boolean	 apl_circled_alpha;
    Boolean	 xquartz_hack;
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
    int		 bell_volume;
    char	*char_class;
    int		 modified_sel_color;
    int		 visual_select_color;
    char	*input_method;
    char	*preedit_type;
    char	*ad_version;
    char	*dpi;

    /*
     * Common resources that have type 'bool', which we need to parse as type
     * 'Boolean' and then copy into the appropriate fields (of type 'bool') in
     * appres. This is needed because in x3270, we have to parse all resources
     * with libXt calls. LibXt uses 'Boolean', but the common resources have
     * type 'bool'. We don't know if libXt's 'Boolean' and <stdbool.h>'s 'bool'
     * are the same type or not.
     */
    struct {
	Boolean bind_limit;
	Boolean bind_unlock;
	Boolean bsd_tm;
	Boolean contention_resolution;
	Boolean debug_tracing;
	Boolean disconnect_clear;
	Boolean highlight_bold;
	Boolean idle_command_enabled;
	Boolean modified_sel;
	Boolean new_environ;
	Boolean numeric_lock;
	Boolean nvt_mode;
	Boolean oerr_lock;
	Boolean once;
	Boolean prefer_ipv4;
	Boolean prefer_ipv6;
	Boolean reconnect;
	Boolean retry;
	Boolean script_port_once;
	Boolean scripted;
	Boolean scripted_always;
	Boolean secure;
	Boolean socket;
	Boolean trace_monitor;
	Boolean unlock_delay;
	Boolean utf8;
	Boolean wrong_terminal_name;
	Boolean tls992;
	Boolean ut_env;
	Boolean extended_data_stream;
	struct {
	    Boolean do_confirms;
	    Boolean menubar;
	    Boolean mono;
	    Boolean visual_bell;
	} interactive;
	Boolean toggle[N_TOGGLES];
	struct {
	    Boolean icrnl;
	    Boolean inlcr;
	    Boolean onlcr;
	} linemode;
	struct {
	    Boolean starttls;
	    Boolean verify_host_cert;
	} tls;
    } bools;
} xappres_t, *xappresptr_t;

extern xappres_t xappres;
