/*
 * Copyright (c) 1994-2024 Paul Mattes.
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
 *	save.c
 *		Implements the response to the WM_SAVE_YOURSELF message and
 *		x3270 profiles.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <pwd.h>
#include <errno.h>
#include "appres.h"
#include "resources.h"

#include "codepage.h"
#if !defined(USE_APP_DEFAULTS) /*[*/
# include "fallbacks.h"
#endif /*]*/
#include "idle.h"
#include "keymap.h"
#include "popups.h"
#include "save.h"
#include "screen.h"
#include "toggles.h"
#include "txa.h"
#include "utils.h"
#include "xappres.h"
#include "xkeypad.h"
#include "xsave.h"
#include "xscreen.h"

/* Support for WM_SAVE_YOURSELF. */

char *command_string = NULL;

static char *cmd;
static int cmd_len;

#define NWORDS	1024

static char **tmp_cmd;
static int tcs;

static Status x_get_window_attributes(Window w, XWindowAttributes *wa);

/* Search for an option in the tmp_cmd array. */
static int
cmd_srch(const char *s)
{
    int i;

    for (i = 1; i < tcs; i++) {
	if (tmp_cmd[i] && !strcmp(tmp_cmd[i], s)) {
	    return i;
	}
    }
    return 0;
}

/* Replace an options in the tmp_cmd array. */
static void
cmd_replace(int ix, const char *s)
{
    XtFree(tmp_cmd[ix]);
    tmp_cmd[ix] = XtNewString(s);
}

/* Append an option to the tmp_cmd array. */
static void
cmd_append(const char *s)
{
    tmp_cmd[tcs++] = XtNewString(s);
    tmp_cmd[tcs] = (char *) NULL;
}

/* Delete an option from the tmp_cmd array. */
static void
cmd_delete(int ix)
{
    XtFree(tmp_cmd[ix]);
    tmp_cmd[ix] = (char *) NULL;
}

/* Save the screen geometry. */
static void
save_xy(void)
{
    char *tbuf;
    Window window, frame, child;
    XWindowAttributes wa;
    int x, y;
    int ix;

    window = XtWindow(toplevel);
    if (!x_get_window_attributes(window, &wa))
	return;
    XTranslateCoordinates(display, window, wa.root, 
	    -wa.border_width, -wa.border_width,
	    &x, &y, &child);

    frame = XtWindow(toplevel);
    while (true) {
	Window root, parent;
	Window *wchildren;
	unsigned int nchildren;

	int status = XQueryTree(display, frame, &root, &parent, &wchildren,
		&nchildren);
	if (status && wchildren) {
	    XFree((char *)wchildren);
	}
	if (parent == root || !parent || !status) {
	    break;
	}
	frame = parent;
    }
    if (frame != window) {
	if (!x_get_window_attributes(frame, &wa)) {
	    return;
	}
	x = wa.x;
	y = wa.y;
    }

    tbuf = txAsprintf("+%d+%d", x, y);
    if ((ix = cmd_srch("-geometry"))) {
	cmd_replace(ix + 1, tbuf);
    } else {
	cmd_append("-geometry");
	cmd_append(tbuf);
    }
}

/* Save the icon information: state, label, geometry. */
static void
save_icon(void)
{
    unsigned char *data;
    int iconX, iconY;
    char *tbuf;
    int ix;
    unsigned long nitems;

    {
	Atom actual_type;
	int actual_format;
	unsigned long leftover;

	if (XGetWindowProperty(display, XtWindow(toplevel), a_state, 0L, 2L,
		    False, a_state, &actual_type, &actual_format, &nitems,
		    &leftover, &data) != Success) {
	    return;
	}
	if (actual_type != a_state || actual_format != 32 || nitems < 1) {
	    goto done;
	}
    }

    ix = cmd_srch("-iconic");
    if (*(unsigned long *)data == IconicState) {
	if (!ix) {
	    cmd_append("-iconic");
	}
    } else {
	if (ix) {
	    cmd_delete(ix);
	}
    }

    if (nitems < 2) {
	goto done;
    }

    {
	Window icon_window;
	XWindowAttributes wa;
	Window child;

	icon_window = *(Window *)(data + sizeof(unsigned long));
	if (icon_window == None) {
	    goto done;
	}
	if (!x_get_window_attributes(icon_window, &wa)) {
	    goto done;
	}
	XTranslateCoordinates(display, icon_window, wa.root,
		-wa.border_width, -wa.border_width, &iconX, &iconY, &child);
	if (!iconX && !iconY) {
	    goto done;
	}
    }

    tbuf = txAsprintf("%d", iconX);
    ix = cmd_srch(OptIconX);
    if (ix) {
	cmd_replace(ix + 1, tbuf);
    } else {
	cmd_append(OptIconX);
	cmd_append(tbuf);
    }

    tbuf = txAsprintf("%d", iconY);
    ix = cmd_srch(OptIconY);
    if (ix) {
	cmd_replace(ix + 1, tbuf);
    } else {
	cmd_append(OptIconY);
	cmd_append(tbuf);
    }
done:
    XFree((char *)data);
    return;
}

/* Save the keymap information. */
static void
save_keymap(void)
{
   /* Note: keymap propogation is deliberately disabled, because it
      may vary from workstation to workstation.  The recommended
      way of specifying keymaps is through your .Xdefaults or the
      KEYMAP or KEYBD environment variables, which can be easily set
      in your .login or .profile to machine-specific values; the
      -keymap switch is really for debugging or testing keymaps.

      I'm sure I'll regret this.  */

#if defined(notdef) /*[*/
    if (current_keymap) {
	add_string(v, OptKeymap);
	add_string(v, current_keymap);
    }
#endif /*]*/
}

/* Save the model name. */
static void
save_model(void)
{
    int ix;

    if (!model_changed) {
	return;
    }
    if ((ix = cmd_srch(OptModel)) && strcmp(tmp_cmd[ix], appres.model)) {
	cmd_replace(ix + 1, appres.model);
    } else {
	cmd_append(OptModel);
	cmd_append(appres.model);
    }
}

/* Save the emulator font. */
static void
save_efont(void)
{
    int ix;

    if (!efont_changed) {
	return;
    }
    if ((ix = cmd_srch(OptEmulatorFont)) && strcmp(tmp_cmd[ix], efontname)) {
	cmd_replace(ix + 1, efontname);
    } else {
	cmd_append(OptEmulatorFont);
	cmd_append(efontname);
    }
}

/* Save the keypad state. */
static void
save_keypad(void)
{
    int ix;

    ix = cmd_srch(OptKeypadOn);
    if (xappres.keypad_on || keypad_popped) {
	if (!ix) {
	    cmd_append(OptKeypadOn);
	}
    } else {
	if (ix) {
	    cmd_delete(ix);
	}
    }
}

/* Save the scrollbar state. */
static void
save_scrollbar(void)
{
    int i_on, i_off;

    if (!scrollbar_changed) {
	return;
    }
    i_on = cmd_srch(OptScrollBar);
    i_off = cmd_srch(OptNoScrollBar);
    if (toggled(SCROLL_BAR)) {
	if (!i_on) {
	    if (i_off) {
		cmd_replace(i_off, OptScrollBar);
	    } else {
	    }
		cmd_append(OptScrollBar);
	}
    } else {
	if (!i_off) {
	    if (i_on) {
		cmd_replace(i_on, OptNoScrollBar);
	    } else {
		cmd_append(OptNoScrollBar);
	    }
	}
    }
}

/* Save the name of the host we are connected to. */
static void
save_host(void)
{
    char *space;

    if (!CONNECTED) {
	return;
    }
    space = strchr(full_current_host, ' ');
    if (space == (char *) NULL) {
	cmd_append(full_current_host);
    } else {
	char *tmp = XtNewString(full_current_host);
	char *port;

	space = strchr(tmp, ' ');
	*space = '\0';
	cmd_append(tmp);
	port = space + 1;
	while (*port == ' ') {
	    port++;
	}
	if (*port) {
	    cmd_append(port);
	}
	XtFree(tmp);
    }
}

/* Save the settings of each of the toggles. */
static void
save_toggles(void)
{
    toggle_index_t i;
    int j;
    int ix;

    for (i = 0; i < N_TOGGLES; i++) {
	toggle_index_t tix = toggle_names[i].index;

	if (!toggle_changed(tix)) {
	    continue;
	}

	/*
	 * Find the last "-set" or "-clear" for this toggle.
	 * If there is a preferred alias, delete them instead.
	 */
	ix = 0;
	for (j = 1; j < tcs; j++) {
	    if (tmp_cmd[j] &&
		(!strcmp(tmp_cmd[j], OptSet) ||
		 !strcmp(tmp_cmd[j], OptClear)) &&
		tmp_cmd[j+1] &&
		!strcmp(tmp_cmd[j+1], toggle_names[i].name)) {

		if (toggle_names[i].is_alias) {
		    cmd_delete(j);
		    cmd_delete(j + 1);
		} else {
		    ix = j;
		}
	    }
	}

	/* Handle aliased switches. */
	switch (tix) {
	case SCROLL_BAR:
	    continue;	/* +sb/-sb done separately */
	case TRACING:
	    ix = cmd_srch(OptTrace);
	    if (toggled(TRACING)) {
		if (!ix) {
		    cmd_append(OptTrace);
		}
	    } else {
		if (ix) {
		    cmd_delete(ix);
		}
	    }
	    continue;
	default:
	    break;
	}

	/* If need be, switch "-set" with "-clear", or append one. */
	if (toggled(tix)) {
	    if (ix && strcmp(tmp_cmd[ix], OptSet)) {
		cmd_replace(ix, OptSet);
	    } else if (!ix) {
		cmd_append(OptSet);
		cmd_append(toggle_names[i].name);
	    }
	} else {
	    if (ix && strcmp(tmp_cmd[ix], OptClear)) {
		cmd_replace(ix, OptClear);
	    } else if (!ix) {
		cmd_append(OptClear);
		    cmd_append(toggle_names[i].name);
	    }
	}
    }
}

/* Remove a positional parameter from the command line. */
static void
remove_positional(char *s)
{
    char *c;

    c = cmd + cmd_len - 2;	/* last byte of last arg */
    while (*c && c >= cmd) {
	    c--;
    }
    if (strcmp(s, c + 1)) {
	XtError("Command-line switches must precede positional arguments");
    }
    cmd_len = c - cmd;
}

/* Save a copy of he XA_WM_COMMAND poperty. */
void
save_init(int argc, char *hostname, char *port)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;

    /*
     * Fetch the initial value of the XA_COMMAND property and store
     * it in 'cmd'.
     */
    XGetWindowProperty(display, XtWindow(toplevel), XA_WM_COMMAND,
	    0L, 1000000L, False, XA_STRING, &actual_type, &actual_format,
	    &nitems, &bytes_after, (unsigned char **)&cmd);
    if (nitems == 0) {
	XtError("Could not get initial XA_COMMAND property");
    }
    cmd_len = nitems * (actual_format / 8);

    /*
     * Now locate the hostname and port positional arguments, and
     * remove them.  If they aren't the last two components of the
     * command line, abort.
     */
    switch (argc) {
    case 3:
	remove_positional(port);
	/* fall through */
    case 2:
	remove_positional(hostname);
	break;
    }
}

/* Handle a WM_SAVE_YOURSELF ICCM. */
void
save_yourself(void)
{
    int i;
    char *c, *c2;
    int len;

    Replace(command_string, NULL);

    /* Copy the original command line into tmp_cmd. */
    tmp_cmd = (char **) XtMalloc(sizeof(char *) * NWORDS);
    tcs = 0;
    i = 0;
    c = cmd;
    while (i < cmd_len) {
	c = cmd + i;
	tmp_cmd[tcs++] = XtNewString(c);
	i += strlen(c);
	i++;
    }
    tmp_cmd[tcs] = (char *) NULL;

    /* Replace the first element with the program name. */
    cmd_replace(0, programname);

    /* Save options. */
    save_xy();
    save_icon();
    save_keymap();
    save_model();
    save_efont();
    save_keypad();
    save_scrollbar();
    save_toggles();
    save_host();

    /* Copy what's left into contiguous memory. */
    len = 0;
    for (i = 0; i < tcs; i++) {
	if (tmp_cmd[i]) {
	    len += strlen(tmp_cmd[i]) + 1;
	}
    }
    c = XtMalloc(len);
    c[0] = '\0';
    c2 = c;
    for (i = 0; i < tcs; i++) {
	if (tmp_cmd[i]) {
	    strcpy(c2, tmp_cmd[i]);
	    c2 += strlen(c2) + 1;
	    XtFree(tmp_cmd[i]);
	}
    }
    Free(tmp_cmd);

    /* Change the property. */
    XChangeProperty(display, XtWindow(toplevel), XA_WM_COMMAND,
	    XA_STRING, 8, PropModeReplace, (unsigned char *)c, len);

    /* Save a readable copy of the command string for posterity. */
    command_string = c;
    while (((c2 = strchr(c, '\0')) != NULL) &&
	   (c2 - command_string < len-1)) {
	*c2 = ' ';
	c = c2 + 1;
    }
}

/* Support for x3270 profiles. */

#define PROFILE_ENV	"X3270PRO"
#define NO_PROFILE_ENV	"NOX3270PRO"
#define RDB_ENV		"X3270RDB"
#define DEFAULT_PROFILE	"~/.x3270pro"

char *profile_name = NULL;
static char *xcmd;
static int xargc;
static char **xargv;

typedef struct scs {
    struct scs *next;
    char *name;
} scs_t;
scs_t *cc_list = NULL;

void
charset_list_changed(char *charset)
{
    scs_t *c;

    for (c = cc_list; c != NULL; c = c->next) {
	if (!strcasecmp(c->name, charset)) {
	    return;
	}
    }
    c = (scs_t *)Malloc(sizeof(scs_t));
    c->name = NewString(charset);
    c->next = cc_list;
    cc_list = c;
}

/* Save one option in the file. */
static void
save_opt(FILE *f, const char *full_name, const char *opt_name,
	const char *res_name, const char *value)
{
    fprintf(f, "! %s ", full_name);
    if (opt_name != NULL) {
	fprintf(f, " (%s)", opt_name);
    }
    fprintf(f, "\n%s.%s: %s\n", XtName(toplevel), res_name, value);
}

/* Save the current options settings in a profile. */
bool
save_options(char *n)
{
    FILE *f;
    bool exists = false;
    char *ct;
    toggle_index_t i;
    time_t clk;
    char *buf;
    bool any_toggles = false;

    if (n == NULL || *n == '\0') {
	return false;
    }

    /* Open the file. */
    n = do_subst(n, DS_VARS | DS_TILDE);
    f = fopen(n, "r");
    if (f != NULL) {
	fclose(f);
	exists = true;
    }
    f = fopen(n, "a");
    if (f == NULL) {
	popup_an_errno(errno, "Cannot open %s", n);
	XtFree(n);
	return false;
    }

    /* Save the name. */
    Replace(profile_name, n);

    /* Print the header. */
    clk = time((time_t *)0);
    ct = ctime(&clk);
    if (ct[strlen(ct)-1] == '\n') {
	ct[strlen(ct)-1] = '\0';
    }
    if (exists) {
	fprintf(f, "! File updated %s by %s\n", ct, build);
    } else {
	fprintf(f,
"! x3270 profile\n\
! File created %s by %s\n\
! This file overrides xrdb and .Xdefaults.\n\
! To skip reading this file, set %s in the environment.\n\
!\n",
		ct, build, NO_PROFILE_ENV);
    }

    /* Save most of the toggles. */
    for (i = 0; toggle_names[i].name; i++) {
	toggle_index_t tix = toggle_names[i].index;

	if (toggle_names[i].is_alias || !toggle_changed(tix)) {
	    continue;
	}
	if (!any_toggles) {
	    fprintf(f, "! toggles (%s, %s)\n", OptSet, OptClear);
	    any_toggles = true;
	}
	fprintf(f, "%s.%s: %s\n", XtName(toplevel),
		toggle_names[i].name,
		toggled(tix)? ResTrue: ResFalse);
    }

    /* Save the keypad state. */
    if (keypad_changed) {
	save_opt(f, "keypad state", OptKeypadOn, ResKeypadOn,
		(xappres.keypad_on || keypad_popped)? ResTrue: ResFalse);
    }

    /* Save other menu-changeable options. */
    if (efont_changed) {
	save_opt(f, "emulator font", OptEmulatorFont, ResEmulatorFont,
		efontname);
    }
    if (model_changed) {
	buf = Asprintf("%d", model_num);
	save_opt(f, "model", OptModel, ResModel, buf);
	Free(buf);
    }
    if (oversize_changed) {
	buf = Asprintf("%dx%d", ov_cols, ov_rows);
	save_opt(f, "oversize", OptOversize, ResOversize, buf);
	Free(buf);
    }
    if (scheme_changed && xappres.color_scheme != NULL) {
	save_opt(f, "color scheme", OptColorScheme, ResColorScheme,
	    xappres.color_scheme);
    }
    if (keymap_changed && current_keymap != NULL) {
	save_opt(f, "keymap", OptKeymap, ResKeymap, current_keymap);
    }
    if (codepage_changed) {
	save_opt(f, "codepage", OptCodePage, ResCodePage, get_codepage_name());
    }
    if (idle_changed) {
	save_opt(f, "idle command", NULL, ResIdleCommand, idle_command);
	save_opt(f, "idle timeout", NULL, ResIdleTimeout, idle_timeout_string);
	save_opt(f, "idle enabled", NULL, ResIdleCommandEnabled,
		(idle_user_enabled == IDLE_PERM)?  ResTrue: ResFalse);
    }

    /* Done. */
    fclose(f);

    return true;
}

/* Save a copy of the command-line options. */
void
save_args(int argc, char *argv[])
{
    int i;
    int len = 0;

    for (i = 0; i < argc; i++) {
	len += strlen(argv[i]) + 1;
    }
    xcmd = XtMalloc(len + 1);
    xargv = (char **)XtMalloc((argc + 1) * sizeof(char *));
    len = 0;
    for (i = 0; i < argc; i++) {
	xargv[i] = xcmd + len;
	strcpy(xcmd + len, argv[i]);
	len += strlen(argv[i]) + 1;
    }
    xargv[i] = NULL;
    *(xcmd + len) = '\0';
    xargc = argc;
}

#if !defined(USE_APP_DEFAULTS) /*[*/
#define DEF_NAME	"x3270"
#define NLEN		(sizeof(DEF_NAME) - 1)
#define DOT_NAME	DEF_NAME "."
#define STAR_NAME	DEF_NAME "*"

/* Substitute an alternate name in the fallback resource definitions. */
static char *
subst_name(unsigned char *fallbacks)
{
    char *tlname;
    char *s, *t;
    bool eol = true;
    int nname = 0;
    size_t nlen;
    int flen;
    char *new_fallbacks;

    /* If the name is the same, do nothing. */
    if (!strcmp((tlname = XtName(toplevel)), DEF_NAME)) {
	return (char *)fallbacks;
    }

    /* Count the number of instances of "x3270" in the fallbacks. */
    s = (char *)fallbacks;
    while (*s) {
	if (eol && (!strncmp(s, DOT_NAME, NLEN + 1) ||
		    !strncmp(s, STAR_NAME, NLEN + 1))) {
	    nname++;
	    s += NLEN;
	    eol = false;
	} else if (*s == '\n') {
	    eol = true;
	} else {
	    eol = false;
	}
	s++;
    }
    if (!nname) {
	return (char *)fallbacks;
    }

    /* Allocate a buffer to do the substitution into. */
    if ((nlen = strlen(tlname)) > NLEN) {
	flen = strlen((char *)fallbacks) + ((nlen - NLEN) * nname) + 1;
    } else {
	flen = strlen((char *)fallbacks) - ((NLEN - nlen) * nname) + 1;
    }
    new_fallbacks = Malloc(flen);

    /* Substitute. */
    s = (char *)fallbacks;
    t = new_fallbacks;
    while (*s) {
	if (eol && (!strncmp(s, DOT_NAME, NLEN + 1) ||
		    !strncmp(s, STAR_NAME, NLEN + 1))) {
	    strcpy(t, tlname);
	    t += nlen;
	    s += NLEN;
	    eol = false;
	} else if (*s == '\n') {
	    eol = true;
	} else {
	    eol = false;
	}
	*t++ = *s++;
    }
    *t = '\0';
    return new_fallbacks;
}
#endif /*]*/

/* Merge in the options settings from a profile. */
void
merge_profile(XrmDatabase *d, char *session, bool mono)
{
    const char *fname;
    char *env_resources;
    XrmDatabase dd;

#if !defined(USE_APP_DEFAULTS) /*[*/
    /* Start with the fallbacks. */
    dd = XrmGetStringDatabase(subst_name(common_fallbacks));
    if (dd == NULL) {
	XtError("Can't parse common fallbacks");
    }
    XrmMergeDatabases(dd, d);
    dd = XrmGetStringDatabase(subst_name(mono? mono_fallbacks:
					       color_fallbacks));
    if (dd == NULL) {
	XtError("Can't parse mono/color fallbacks");
    }
    XrmMergeDatabases(dd, d);
#endif /*]*/

    if (session == NULL && getenv(NO_PROFILE_ENV) != NULL) {
	profile_name = do_subst(DEFAULT_PROFILE, DS_VARS | DS_TILDE);
    } else {
	/* Open the file. */
	if (session != NULL) {
	    fname = session;
	} else {
	    fname = getenv(PROFILE_ENV);
	}
	if (fname == NULL || *fname == '\0') {
	    fname = DEFAULT_PROFILE;
	}
	profile_name = do_subst(fname, DS_VARS | DS_TILDE);

	/* Create a resource database from the file. */
	dd = XrmGetFileDatabase(profile_name);
	if (dd != NULL) {
	    /* Merge in the profile options. */
	    XrmMergeDatabases(dd, d);
	} else if (session != NULL) {
	    Error("Session file not found");
	}
    }

    /* See if there are any environment resources. */
    env_resources = getenv(RDB_ENV);
    if (env_resources != NULL) {
	dd = XrmGetStringDatabase(env_resources);
	if (dd != NULL) {
	    XrmMergeDatabases(dd, d);
	}
    }

    /* Merge the saved command-line options back on top of those. */
    dd = NULL;
    XrmParseCommand(&dd, options, num_options, programname, &xargc, xargv);
    XrmMergeDatabases(dd, d);

    /* Free the saved command-line options. */
    XtFree(xcmd);
    xcmd = NULL;
    Replace(xargv, NULL);
}

bool
read_resource_file(const char *filename, bool fatal)
{
    XrmDatabase dd, rdb;

    dd = XrmGetFileDatabase(filename);
    if (dd == NULL) {
	return false;
    }

    rdb = XtDatabase(display);
    XrmMergeDatabases(dd, &rdb);
    return true;
}

/*
 * Safe routine for querying window attributes
 */
static int
dummy_error_handler(Display *d _is_unused, XErrorEvent *e _is_unused)
{
    return 0;
}

static Status
x_get_window_attributes(Window w, XWindowAttributes *wa)
{
    XErrorHandler old_handler;
    Status s;

    old_handler = XSetErrorHandler(dummy_error_handler);

    s = XGetWindowAttributes(display, w, wa);
    if (!s) {
	fprintf(stderr, "Error: querying bad window 0x%lx\n", w);
    }

    XSetErrorHandler(old_handler);

    return s;
}
