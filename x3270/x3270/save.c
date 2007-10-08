/*
 * Copyright 1994, 1995, 1999, 2000, 2001, 2002, 2003, 2005 by Paul Mattes.
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
 *	save.c
 *		Implements the response to the WM_SAVE_YOURSELF message and
 *		x3270 profiles.
 */

#include "globals.h"
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <pwd.h>
#include <errno.h>
#include "appres.h"
#include "resources.h"

#include "savec.h"
#include "charsetc.h"
#include "idlec.h"
#include "popupsc.h"
#include "utilc.h"


/* Support for WM_SAVE_YOURSELF. */

extern Boolean	keypad_changed;
extern Boolean  keypad_popped;
extern Boolean	model_changed;
extern Boolean	scrollbar_changed;
extern Boolean	efont_changed;
extern Boolean	oversize_changed;
extern Boolean	scheme_changed;
extern Boolean	keymap_changed;
extern Boolean	charset_changed;

char           *command_string = CN;

static char    *cmd;
static int      cmd_len;

#define NWORDS	1024

static char   **tmp_cmd;
static int      tcs;

static Status x_get_window_attributes(Window w, XWindowAttributes *wa);

/* Search for an option in the tmp_cmd array. */
static int
cmd_srch(const char *s)
{
	int i;

	for (i = 1; i < tcs; i++)
		if (tmp_cmd[i] && !strcmp(tmp_cmd[i], s))
			return i;
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
	char tbuf[64];
	Window window, frame, child;
	XWindowAttributes wa;
	int x, y;
	int ix;

	window = XtWindow(toplevel);
	if (!x_get_window_attributes(window, &wa))
		return;
	(void) XTranslateCoordinates(display, window, wa.root, 
		-wa.border_width, -wa.border_width,
		&x, &y, &child);

	frame = XtWindow(toplevel);
	while (True) {
		Window root, parent;
		Window *wchildren;
		unsigned int nchildren;

		int status = XQueryTree(display, frame, &root, &parent,
		    &wchildren, &nchildren);
		if (status && wchildren)
			XFree((char *)wchildren);
		if (parent == root || !parent || !status)
			break;
		frame = parent;
	}
	if (frame != window) {
		if (!x_get_window_attributes(frame, &wa))
			return;
		x = wa.x;
		y = wa.y;
	}

	(void) sprintf(tbuf, "+%d+%d", x, y);
	if ((ix = cmd_srch("-geometry")))
		cmd_replace(ix + 1, tbuf);
	else {
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
	char tbuf[64];
	int ix;
	unsigned long nitems;

	{
		Atom actual_type;
		int actual_format;
		unsigned long leftover;

		if (XGetWindowProperty(display, XtWindow(toplevel), a_state,
		    0L, 2L, False, a_state, &actual_type, &actual_format,
		    &nitems, &leftover, &data) != Success)
			return;
		if (actual_type != a_state ||
		    actual_format != 32 ||
		    nitems < 1)
			return;
	}

	ix = cmd_srch("-iconic");
	if (*(unsigned long *)data == IconicState) {
		if (!ix)
			cmd_append("-iconic");
	} else {
		if (ix)
			cmd_delete(ix);
	}

	if (nitems < 2)
		return;

	{
		Window icon_window;
		XWindowAttributes wa;
		Window child;

		icon_window = *(Window *)(data + sizeof(unsigned long));
		if (icon_window == None)
			return;
		if (!x_get_window_attributes(icon_window, &wa))
			return;
		(void) XTranslateCoordinates(display, icon_window, wa.root,
		    -wa.border_width, -wa.border_width, &iconX, &iconY,
		    &child);
		if (!iconX && !iconY)
			return;
	}

	(void) sprintf(tbuf, "%d", iconX);
	ix = cmd_srch(OptIconX);
	if (ix)
		cmd_replace(ix + 1, tbuf);
	else {
		cmd_append(OptIconX);
		cmd_append(tbuf);
	}

	(void) sprintf(tbuf, "%d", iconY);
	ix = cmd_srch(OptIconY);
	if (ix)
		cmd_replace(ix + 1, tbuf);
	else {
		cmd_append(OptIconY);
		cmd_append(tbuf);
	}
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
	if (appres.keymap) {
		add_string(v, OptKeymap);
		add_string(v, appres.keymap);
	}
#endif /*]*/
}

/* Save the model name. */
static void
save_model(void)
{
	int ix;

	if (!model_changed)
		return;
	if ((ix = cmd_srch(OptModel)) && strcmp(tmp_cmd[ix], model_name))
		cmd_replace(ix + 1, model_name);
	else {
		cmd_append(OptModel);
		cmd_append(model_name);
	}
}

/* Save the emulator font. */
static void
save_efont(void)
{
	int ix;

	if (!efont_changed)
		return;
	if ((ix = cmd_srch(OptEmulatorFont)) && strcmp(tmp_cmd[ix], efontname))
		cmd_replace(ix + 1, efontname);
	else {
		cmd_append(OptEmulatorFont);
		cmd_append(efontname);
	}
}

#if defined(X3270_KEYPAD) /*[*/
/* Save the keypad state. */
static void
save_keypad(void)
{
	int ix;

	ix = cmd_srch(OptKeypadOn);
	if (appres.keypad_on || keypad_popped) {
		if (!ix)
			cmd_append(OptKeypadOn);
	} else {
		if (ix)
			cmd_delete(ix);
	}
}
#endif /*]*/

/* Save the scrollbar state. */
static void
save_scrollbar(void)
{
	int i_on, i_off;

	if (!scrollbar_changed)
		return;
	i_on = cmd_srch(OptScrollBar);
	i_off = cmd_srch(OptNoScrollBar);
	if (toggled(SCROLL_BAR)) {
		if (!i_on) {
			if (i_off)
				cmd_replace(i_off, OptScrollBar);
			else
				cmd_append(OptScrollBar);
		}
	} else {
		if (!i_off) {
			if (i_on)
				cmd_replace(i_on, OptNoScrollBar);
			else
				cmd_append(OptNoScrollBar);
		}
	}
}

/* Save the name of the host we are connected to. */
static void
save_host(void)
{
	char *space;

	if (!CONNECTED)
		return;
	space = strchr(full_current_host, ' ');
	if (space == (char *) NULL)
		cmd_append(full_current_host);
	else {
		char *tmp = XtNewString(full_current_host);
		char *port;

		space = strchr(tmp, ' ');
		*space = '\0';
		cmd_append(tmp);
		port = space + 1;
		while (*port == ' ')
			port++;
		if (*port)
			cmd_append(port);
		XtFree(tmp);
	}
}

/* Save the settings of each of the toggles. */
static void
save_toggles(void)
{
	int i, j;
	int ix;

	for (i = 0; i < N_TOGGLES; i++) {
		if (toggle_names[i].index < 0 || !appres.toggle[i].changed)
			continue;

		/*
		 * Find the last "-set" or "-clear" for this toggle.
		 * If there is a preferred alias, delete them instead.
		 */
		ix = 0;
		for (j = 1; j < tcs; j++)
			if (tmp_cmd[j] &&
			    (!strcmp(tmp_cmd[j], OptSet) ||
			     !strcmp(tmp_cmd[j], OptClear)) &&
			    tmp_cmd[j+1] &&
			    !strcmp(tmp_cmd[j+1], toggle_names[i].name)) {
				if (i == SCROLL_BAR
#if defined(X3270_TRACE) /*[*/
				    || i == DS_TRACE
#endif /*]*/
				    ) {
					cmd_delete(j);
					cmd_delete(j + 1);
				} else
					ix = j;
		}

		/* Handle aliased switches. */
		switch (i) {
		    case SCROLL_BAR:
			continue;	/* +sb/-sb done separately */
#if defined(X3270_TRACE) /*[*/
		    case DS_TRACE:
			ix = cmd_srch(OptDsTrace);
			if (appres.toggle[DS_TRACE].value) {
				if (!ix)
					cmd_append(OptDsTrace);
			} else {
				if (ix)
					cmd_delete(ix);
			}
			continue;
#endif /*]*/
		}

		/* If need be, switch "-set" with "-clear", or append one. */
		if (appres.toggle[i].value) {
			if (ix && strcmp(tmp_cmd[ix], OptSet))
				cmd_replace(ix, OptSet);
			else if (!ix) {
				cmd_append(OptSet);
				cmd_append(toggle_names[i].name);
			}
		} else {
			if (ix && strcmp(tmp_cmd[ix], OptClear))
				cmd_replace(ix, OptClear);
			else if (!ix) {
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
	while (*c && c >= cmd)
		c--;
	if (strcmp(s, c + 1))
		XtError("Command-line switches must precede positional arguments");
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
	if (nitems == 0)
		XtError("Could not get initial XA_COMMAND property");
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

	Replace(command_string, CN);

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
#if defined(X3270_KEYPAD) /*[*/
	save_keypad();
#endif /*]*/
	save_scrollbar();
	save_toggles();
	save_host();

	/* Copy what's left into contiguous memory. */
	len = 0;
	for (i = 0; i < tcs; i++)
		if (tmp_cmd[i])
			len += strlen(tmp_cmd[i]) + 1;
	c = XtMalloc(len);
	c[0] = '\0';
	c2 = c;
	for (i = 0; i < tcs; i++)
		if (tmp_cmd[i]) {
			(void) strcpy(c2, tmp_cmd[i]);
			c2 += strlen(c2) + 1;
			XtFree(tmp_cmd[i]);
		}
	Free(tmp_cmd);

	/* Change the property. */
	XChangeProperty(display, XtWindow(toplevel), XA_WM_COMMAND,
	    XA_STRING, 8, PropModeReplace, (unsigned char *)c, len);

	/* Save a readable copy of the command string for posterity. */
	command_string = c;
	while (((c2 = strchr(c, '\0')) != CN) &&
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

extern XrmOptionDescRec options[];
extern int num_options;

char *profile_name = CN;
static char *xcmd;
static int xargc;
static char **xargv;

#if defined(X3270_MENUS) /*[*/

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
		if (!strcasecmp(c->name, charset))
			return;
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
	(void) fprintf(f, "! %s ", full_name);
	if (opt_name != CN)
		(void) fprintf(f, " (%s)", opt_name);
	(void) fprintf(f, "\n%s.%s: %s\n", XtName(toplevel), res_name, value);
}

/* Save the current options settings in a profile. */
int
save_options(char *n)
{
	FILE *f;
	Boolean exists = False;
	char *ct;
	int i;
	time_t clk;
	char buf[64];
	Boolean any_toggles = False;

	if (n == CN || *n == '\0')
		return -1;

	/* Open the file. */
	n = do_subst(n, True, True);
	f = fopen(n, "r");
	if (f != (FILE *)NULL) {
		(void) fclose(f);
		exists = True;
	}
	f = fopen(n, "a");
	if (f == (FILE *)NULL) {
		popup_an_errno(errno, "Cannot open %s", n);
		XtFree(n);
		return -1;
	}

	/* Save the name. */
	Replace(profile_name, n);

	/* Print the header. */
	clk = time((time_t *)0);
	ct = ctime(&clk);
	if (ct[strlen(ct)-1] == '\n')
		ct[strlen(ct)-1] = '\0';
	if (exists)
		(void) fprintf(f, "! File updated %s by %s\n", ct, build);
	else
		(void) fprintf(f,
"! x3270 profile\n\
! File created %s by %s\n\
! This file overrides xrdb and .Xdefaults.\n\
! To skip reading this file, set %s in the environment.\n\
!\n",
		    ct, build, NO_PROFILE_ENV);

	/* Save most of the toggles. */
	for (i = 0; i < N_TOGGLES; i++) {
		if (toggle_names[i].index < 0 || !appres.toggle[i].changed)
			continue;
#if defined(X3270_TRACE) /*[*/
		if (i == DS_TRACE || i == SCREEN_TRACE || i == EVENT_TRACE)
			continue;
#endif /*]*/
		if (!any_toggles) {
			(void) fprintf(f, "! toggles (%s, %s)\n",
			    OptSet, OptClear);
			any_toggles = True;
		}
		(void) fprintf(f, "%s.%s: %s\n", XtName(toplevel),
	            toggle_names[i].name,
		    appres.toggle[i].value ? ResTrue : ResFalse);
	}

#if defined(X3270_KEYPAD) /*[*/
	/* Save the keypad state. */
	if (keypad_changed)
		save_opt(f, "keypad state", OptKeypadOn, ResKeypadOn,
			(appres.keypad_on || keypad_popped) ?
			    ResTrue : ResFalse);
#endif /*]*/

	/* Save other menu-changeable options. */
	if (efont_changed)
		save_opt(f, "emulator font", OptEmulatorFont, ResEmulatorFont,
		    efontname);
	if (model_changed) {
		(void) sprintf(buf, "%d", model_num);
		save_opt(f, "model", OptModel, ResModel, buf);
	}
	if (oversize_changed) {
		(void) sprintf(buf, "%dx%d", ov_cols, ov_rows);
		save_opt(f, "oversize", OptOversize, ResOversize, buf);
	}
	if (scheme_changed && appres.color_scheme != CN)
		save_opt(f, "color scheme", OptColorScheme, ResColorScheme,
		    appres.color_scheme);
	if (keymap_changed && appres.key_map != (char *)NULL)
		save_opt(f, "keymap", OptKeymap, ResKeymap, appres.key_map);
	if (charset_changed)
		save_opt(f, "charset", OptCharset, ResCharset,
		    get_charset_name());
#if defined(X3270_SCRIPT) /*[*/
	if (idle_changed) {
		save_opt(f, "idle command", CN, ResIdleCommand, idle_command);
		save_opt(f, "idle timeout", CN, ResIdleTimeout,
				idle_timeout_string);
		save_opt(f, "idle enabled", CN, ResIdleCommandEnabled,
				(idle_user_enabled == IDLE_PERM)?
				    "True": "False");
	}
#endif /*]*/

	/* Done. */
	(void) fclose(f);

	return 0;
}

#endif /*]*/

/* Save a copy of the command-line options. */
void
save_args(int argc, char *argv[])
{
	int i;
	int len = 0;

	for (i = 0; i < argc; i++)
		len += strlen(argv[i]) + 1;
	xcmd = XtMalloc(len + 1);
	xargv = (char **)XtMalloc((argc + 1) * sizeof(char *));
	len = 0;
	for (i = 0; i < argc; i++) {
		xargv[i] = xcmd + len;
		(void) strcpy(xcmd + len, argv[i]);
		len += strlen(argv[i]) + 1;
	}
	xargv[i] = CN;
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
	Boolean eol = True;
	int nname = 0;
	int nlen;
	int flen;
	char *new_fallbacks;

	/* If the name is the same, do nothing. */
	if (!strcmp((tlname = XtName(toplevel)), DEF_NAME))
		return (char *)fallbacks;

	/* Count the number of instances of "x3270" in the fallbacks. */
	s = (char *)fallbacks;
	while (*s) {
		if (eol && (!strncmp(s, DOT_NAME, NLEN + 1) ||
			    !strncmp(s, STAR_NAME, NLEN + 1))) {
			nname++;
			s += NLEN;
			eol = False;
		} else if (*s == '\n')
			eol = True;
		else
			eol = False;
		s++;
	}
	if (!nname)
		return (char *)fallbacks;

	/* Allocate a buffer to do the substitution into. */
	if ((nlen = strlen(tlname)) > NLEN)
		flen = strlen((char *)fallbacks) + ((nlen - NLEN) * nname) + 1;
	else
		flen = strlen((char *)fallbacks) - ((NLEN - nlen) * nname) + 1;
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
			eol = False;
		} else if (*s == '\n')
			eol = True;
		else
			eol = False;
		*t++ = *s++;
	}
	*t = '\0';
	return new_fallbacks;
}
#endif /*]*/

/* Merge in the options settings from a profile. */
void
merge_profile(XrmDatabase *d, Boolean mono)
{
	const char *fname;
	char *env_resources;
	XrmDatabase dd;
#if !defined(USE_APP_DEFAULTS) /*[*/
	extern unsigned char common_fallbacks[], mono_fallbacks[],
		color_fallbacks[];
#endif /*]*/

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

	if (getenv(NO_PROFILE_ENV) != CN) {
		profile_name = do_subst(DEFAULT_PROFILE, True, True);
	} else {
		/* Open the file. */
		fname = getenv(PROFILE_ENV);
		if (fname == CN || *fname == '\0')
			fname = DEFAULT_PROFILE;
		profile_name = do_subst(fname, True, True);

		/* Create a resource database from the file. */
		dd = XrmGetFileDatabase(profile_name);
		if (dd != NULL) {
			/* Merge in the profile options. */
			XrmMergeDatabases(dd, d);
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
	XrmParseCommand(&dd, options, num_options, programname,
			&xargc, xargv);
	XrmMergeDatabases(dd, d);

	/* Free the saved command-line options. */
	XtFree(xcmd);
	xcmd = CN;
	Replace(xargv, (char **)NULL);
}

int
read_resource_file(const char *filename, Boolean fatal)
{
	XrmDatabase dd, rdb;

	dd = XrmGetFileDatabase(filename);
	if (dd == NULL)
		return -1;

	rdb = XtDatabase(display);
	XrmMergeDatabases(dd, &rdb);
	return 0;
}

/*
 * Safe routine for querying window attributes
 */
static int
dummy_error_handler(Display *d unused, XErrorEvent *e unused)
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
	if (!s)
		(void) fprintf(stderr, "Error: querying bad window 0x%lx\n", w);

	(void) XSetErrorHandler(old_handler);

	return s;
}
