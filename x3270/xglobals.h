/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 2005, Don Russell.
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
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes nor the
 *       names of their contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL AND JEFF SPARKES
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, DON RUSSELL OR JEFF
 * SPARKES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	xglobals.h
 *		Common definitions for x3270.
 */

extern Atom		a_3270, a_registry, a_encoding;
extern XtAppContext	appcontext;
extern int		default_screen;
extern bool		*extended_3270font;
extern Font		*fid;
extern bool		*font_8bit;
extern bool		*full_apl_font;
extern char		*locale_name;
extern Widget		toplevel;
extern Atom		a_atom;
extern Atom		a_delete_me;
extern Atom		a_font;
extern Atom		a_net_wm_name;
extern Atom		a_net_wm_state;
extern Atom		a_net_wm_state_maximized_horz;
extern Atom		a_net_wm_state_maximized_vert;
extern Atom		a_pixel_size;
extern Atom		a_save_yourself;
extern Atom		a_spacing;
extern Atom		a_state;
extern Display		*display;
extern Pixmap		gray;
extern Pixel		keypadbg_pixel;
extern XrmDatabase	rdb;
extern Window		root_window;
extern char		*user_title;

extern XrmOptionDescRec *options;
extern int num_options;

#if defined(USE_APP_DEFAULTS) /*[*/
extern const char	*app_defaults_version;
#endif /*]*/
