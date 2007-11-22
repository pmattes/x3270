/*
 * Copyright 1996, 1999, 2000, 2001, 2002, 2003, 2005 by Paul Mattes.
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
 *	keymap.c
 *		This module handles keymaps.
 */

#include "globals.h"
#include <X11/IntrinsicP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Text.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/AsciiSrc.h>
#include <errno.h>
#include "appres.h"
#include "objects.h"
#include "resources.h"

#include "hostc.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "popupsc.h"
#include "screenc.h"
#include "statusc.h"
#include "utilc.h"

#define PA_ENDL		" " PA_END "()"
#define Res3270		"3270"

Boolean keymap_changed = False;
struct trans_list *trans_list = (struct trans_list *)NULL;
static struct trans_list **last_trans = &trans_list;
static struct trans_list *tkm_last;
struct trans_list *temp_keymaps;	/* temporary keymap list */
char *keymap_trace = CN;
static char *last_keymap = CN;
static Boolean last_nvt = False;
static Boolean last_3270 = False;

static void setup_keymaps(const char *km, Boolean do_popup);
static void add_keymap(const char *name, Boolean do_popup);
static void add_trans(const char *name, char *translations, char *pathname,
    Boolean is_from_server);
static char *get_file_keymap(const char *name, char **pathp);
static void keymap_3270_mode(Boolean);

/* Undocumented Xt function to convert translations to text. */
extern String _XtPrintXlations(Widget w, XtTranslations xlations,
    Widget accelWidget, Boolean includeRHS);

extern Widget *screen;

#if defined(X3270_MENUS) /*[*/
extern Pixmap diamond;
extern Pixmap no_diamond;

static enum { SORT_EVENT, SORT_KEYMAP, SORT_ACTION } sort = SORT_KEYMAP;

static Boolean km_isup = False;
static Boolean km_exists = False;
static Widget km_shell, sort_event, sort_keymap, sort_action, text;
static char km_file[128];
static void create_text(void);
static void km_up(Widget w, XtPointer client_data, XtPointer call_data);
static void km_down(Widget w, XtPointer client_data, XtPointer call_data);
static void km_done(Widget w, XtPointer client_data, XtPointer call_data);
static void do_sort_action(Widget w, XtPointer client_data,
    XtPointer call_data);
static void do_sort_keymap(Widget w, XtPointer client_data,
    XtPointer call_data);
static void do_sort_event(Widget w, XtPointer client_data, XtPointer call_data);
static void format_xlations(String s, FILE *f);
static int action_cmp(char *s1, char *s2);
static int keymap_cmp(char *k1, int l1, char *k2, int l2);
static int event_cmp(char *e1, char *e2);
static Boolean is_temp(char *k);
static char *pathname(char *k);
static Boolean from_server(char *k);
static void km_regen(void);
#else /*][*/
#define km_regen()
#endif /*]*/

char *current_keymap = CN;

/* Keymap initialization. */
void
keymap_init(const char *km, Boolean interactive)
{
	static Boolean initted = False;

	if (km == CN)
		if ((km = (char *)getenv("KEYMAP")) == CN)
			if ((km = (char *)getenv("KEYBD")) == CN)
				km = "@server";
	setup_keymaps(km, interactive);
	if (!initted) {
		initted = True;
		last_nvt = IN_ANSI;
		last_3270 = IN_3270;
		register_schange(ST_3270_MODE, keymap_3270_mode);
		register_schange(ST_CONNECT, keymap_3270_mode);
	} else {
		screen_set_keymap();
		keypad_set_keymap();
	}
	km_regen();

	/* Save the name(s) of the last keymap, so we can switch modes later. */
	if (km != last_keymap) {
		Replace(last_keymap, km? NewString(km): CN);
	}
}

/*
 * 3270/NVT mode change.
 */
static void
keymap_3270_mode(Boolean ignored unused)
{
	if (last_nvt != IN_ANSI || last_3270 != IN_3270) {
		last_nvt = IN_ANSI;
		last_3270 = IN_3270;

		/* Switch between 3270 and NVT keymaps. */
		keymap_init(last_keymap, False);
	}
}

/*
 * Set up a user keymap.
 */
static void
setup_keymaps(const char *km, Boolean do_popup)
{
    	char *bkm;
	Boolean saw_apl_keymod = False;
	struct trans_list *t;
	struct trans_list *next;

	/* Make sure it starts with "base". */
	if (km == CN)
		bkm = XtNewString("base");
	else
		bkm = xs_buffer("base,%s", km);

	if (do_popup)
		keymap_changed = True;

	/* Clear out any existing translations. */
	Replace(current_keymap, CN);
	for (t = trans_list; t != (struct trans_list *)NULL; t = next) {
		next = t->next;
		Free(t->name);
		Free(t->pathname);
		Free(t);
	}
	trans_list = (struct trans_list *)NULL;
	last_trans = &trans_list;

	/* Build up the new list. */
	if (bkm != CN) {
		char *ns = XtNewString(bkm);
		char *n0 = ns;
		char *comma;

		do {
			comma = strchr(ns, ',');
			if (comma)
				*comma = '\0';
			if (!strcmp(ns, Apl))
				saw_apl_keymod = True;
			add_keymap(ns, do_popup);
			if (comma)
				ns = comma + 1;
			else
				ns = NULL;
		} while (ns);

		XtFree(n0);
	}
	if (appres.apl_mode && !saw_apl_keymod)
		add_keymap(Apl, do_popup);
	XtFree(bkm);
}

/*
 * Get a keymap from a file.
 */
static char *
get_file_keymap(const char *name, char **pathp)
{
	char *path;
	XrmDatabase dd = (XrmDatabase)NULL;
	char *resname;
	XrmValue value;
	char *type;
	char *r = CN;

	*pathp = CN;

	/* Look for a global keymap file. */
	if (dd == (XrmDatabase)NULL) {
		path = xs_buffer("%s/keymap.%s", appres.conf_dir, name);
		dd = XrmGetFileDatabase(path);
		if (dd != (XrmDatabase)NULL)
			*pathp = path;
		else {
			XtFree(path);
			return CN;
		}
	}

	/* Look up the resource in that file. */
	resname = xs_buffer("%s.%s.%s", XtName(toplevel), ResKeymap, name);
	if ((XrmGetResource(dd, resname, 0, &type, &value) == True) &&
		    *value.addr) {
		r = XtNewString(value.addr);
	} else {
		*pathp = CN;
	}
	XtFree(resname);
	XrmDestroyDatabase(dd);
	return r;
}

/*
 * Add to the list of user-specified keymap translations, finding both the
 * system and user versions of a keymap.
 */
static void
add_keymap(const char *name, Boolean do_popup)
{
	char *translations, *translations_nvt, *translations_3270;
	char *buf, *buf_nvt, *buf_3270;
	int any = 0;
	char *path, *path_nvt, *path_3270;
	Boolean is_from_server = False;

	if (strcmp(name, "base")) {
		if (current_keymap == CN)
			current_keymap = XtNewString(name);
		else {

			Replace(current_keymap,
				xs_buffer("%s,%s", current_keymap, name));
		}
	}

	/* Translate '@server' to a vendor-specific keymap. */
	if (!strcmp(name, "@server")) {
		struct sk {
			struct sk *next;
			char *vendor;
			char *keymap;
		};
		static struct sk *sk_list = (struct sk *)NULL;
		struct sk *sk;

		if (sk_list == (struct sk *)NULL) {
			char *s, *vendor, *keymap;

			s = get_resource("serverKeymapList");
			if (s == CN)
				return;
			s = XtNewString(s);
			while (split_dresource(&s, &vendor, &keymap) == 1) {
				sk = (struct sk *)XtMalloc(sizeof(struct sk));
				sk->vendor = vendor;
				sk->keymap = keymap;
				sk->next = sk_list;
				sk_list = sk;
			}
		}
		for (sk = sk_list; sk != (struct sk *)NULL; sk = sk->next) {
			if (!strcmp(sk->vendor, ServerVendor(display))) {
				name = sk->keymap;
				is_from_server = True;
				break;
			}
		}
		if (sk == (struct sk *)NULL)
			return;
	}

	/* Try for a file first, then resources. */
	translations = get_file_keymap(name, &path);
	buf_nvt = xs_buffer("%s.%s", name, ResNvt);
	translations_nvt = get_file_keymap(buf_nvt, &path_nvt);
	buf_3270 = xs_buffer("%s.%s", name, Res3270);
	translations_3270 = get_file_keymap(buf_3270, &path_3270);
	if (translations != CN || translations_nvt != CN || translations_3270 != CN) {
		any++;
		if (translations != CN)
			add_trans(name, translations, path, is_from_server);
		if (IN_ANSI && translations_nvt != CN)
			add_trans(buf_nvt, translations_nvt, path_nvt,
			    is_from_server);
		if (IN_3270 && translations_3270 != CN)
			add_trans(buf_3270, translations_3270, path_3270,
			    is_from_server);
		XtFree(translations);
		XtFree(translations_nvt);
		XtFree(translations_3270);
		XtFree(buf_nvt);
		XtFree(buf_3270);
	} else {
		XtFree(buf_nvt);
		XtFree(buf_3270);

		/* Shared keymap. */
		buf = xs_buffer("%s.%s", ResKeymap, name);
		translations = get_resource(buf);
		buf_nvt = xs_buffer("%s.%s.%s", ResKeymap, name, ResNvt);
		translations_nvt = get_resource(buf_nvt);
		buf_3270 = xs_buffer("%s.%s.%s", ResKeymap, name, Res3270);
		translations_3270 = get_resource(buf_3270);
		if (translations != CN || translations_nvt != CN || translations_3270)
			any++;
		if (translations != CN)
			add_trans(name, translations, CN, is_from_server);
		if (IN_ANSI && translations_nvt != CN)
			add_trans(buf_nvt + strlen(ResKeymap) + 1,
			    translations_nvt, CN, is_from_server);
		if (IN_3270 && translations_3270 != CN)
			add_trans(buf_3270 + strlen(ResKeymap) + 1,
			    translations_3270, CN, is_from_server);
		XtFree(buf);
		XtFree(buf_nvt);
		XtFree(buf_3270);

		/* User keymap */
		buf = xs_buffer("%s.%s.%s", ResKeymap, name, ResUser);
		translations = get_resource(buf);
		buf_nvt = xs_buffer("%s.%s.%s.%s", ResKeymap, name, ResNvt,
		    ResUser);
		translations_nvt = get_resource(buf_nvt);
		buf_3270 = xs_buffer("%s.%s.%s.%s", ResKeymap, name, Res3270,
		    ResUser);
		translations_3270 = get_resource(buf_3270);
		if (translations != CN || translations_nvt != CN || translations_3270 != CN)
			any++;
		if (IN_ANSI && translations_nvt != CN)
			add_trans(buf_nvt + strlen(ResKeymap) + 1,
			    translations_nvt, CN, is_from_server);
		if (IN_3270 && translations_3270 != CN)
			add_trans(buf_3270 + strlen(ResKeymap) + 1,
			    translations_3270, CN, is_from_server);
		if (translations != CN)
			add_trans(buf, translations, CN, is_from_server);
		XtFree(buf);
		XtFree(buf_nvt);
		XtFree(buf_3270);
	}

	if (!any) {
		if (do_popup)
			popup_an_error("Cannot find %s \"%s\"", ResKeymap,
			    name);
		else
			xs_warning("Cannot find %s \"%s\"", ResKeymap, name);
	}
}

/*
 * Add a single keymap name and translation to the translation list.
 */
static void
add_trans(const char *name, char *translations, char *path_name,
    Boolean is_from_server)
{
	struct trans_list *t;

	t = (struct trans_list *)XtMalloc(sizeof(*t));
	t->name = XtNewString(name);
	t->pathname = path_name;
	t->is_temp = False;
	t->from_server = is_from_server;
	(void) lookup_tt(name, translations);
	t->next = NULL;
	*last_trans = t;
	last_trans = &t->next;
}

/*
 * Translation table expansion.
 */

/* Find the first unquoted newline an an action list. */
static char *
unquoted_newline(char *s)
{
	Boolean bs = False;
	enum { UQ_BASE, UQ_PLIST, UQ_Q } state = UQ_BASE;
	char c;

	for ( ; (c = *s); s++) {
		if (bs) {
			bs = False;
			continue;
		} else if (c == '\\') {
			bs = True;
			continue;
		}
		switch (state) {
		    case UQ_BASE:
			if (c == '(')
				state = UQ_PLIST;
			else if (c == '\n')
				return s;
			break;
		    case UQ_PLIST:
			if (c == ')')
				state = UQ_BASE;
			else if (c == '"')
				state = UQ_Q;
			break;
		    case UQ_Q:
			if (c == '"')
				state = UQ_PLIST;
			break;
		}
	}
	return CN;
}

/* Expand a translation table with keymap tracing calls. */
static char *
expand_table(const char *name, char *table)
{
	char *cm, *t0, *t, *s;
	int nlines = 1;

	if (table == CN)
		return CN;

	/* Roughly count the number of lines in the table. */
	cm = table;
	while ((cm = strchr(cm, '\n')) != CN) {
		nlines++;
		cm++;
	}

	/* Allocate a new buffer. */
	t = t0 = (char *)XtMalloc(2 + strlen(table) +
	    nlines * (strlen(" " PA_KEYMAP_TRACE "(,nnnn) ") + strlen(name) +
		      strlen(PA_ENDL)));
	*t = '\0';

	/* Expand the table into it. */
	s = table;
	nlines = 0;
	while (*s) {
		/* Skip empty lines. */
		while (*s == ' ' || *s == '\t')
			s++;
		if (*s == '\n') {
			*t++ = '\n';
			*t = '\0';
			s++;
			continue;
		}

		/* Find the '>' from the event name, and copy up through it. */
		cm = strchr(s, '>');
		if (cm == CN) {
			while ((*t++ = *s++))
				;
			break;
		}
		while (s <= cm)
			*t++ = *s++;

		/* Find the ':' following, and copy up throught that. */
		cm = strchr(s, ':');
		if (cm == CN) {
			while ((*t++ = *s++))
				;
			break;
		}
		nlines++;
		while (s <= cm)
			*t++ = *s++;

#if defined(X3270_TRACE) /*[*/
		/* Insert a PA-KeymapTrace call. */
		(void) sprintf(t, " " PA_KEYMAP_TRACE "(%s,%d) ", name, nlines);
		t = strchr(t, '\0');
#endif /*]*/

		/*
		 * Copy to the next unquoted newline and append a PA-End call.
		 */
		cm = unquoted_newline(s);
		if (cm == CN) {
			while ((*t = *s)) {
				t++;
				s++;
			}
		} else {
			while (s < cm)
				*t++ = *s++;
		}
#if defined(X3270_TRACE) /*[*/
		(void) strcpy(t, PA_ENDL);
		t += strlen(PA_ENDL);
#endif /*]*/
		if (cm == CN)
			break;
		else
			*t++ = *s++;
	}
	*t = '\0';

	return t0;
}

#if defined(X3270_TRACE) /*[*/
/*
 * Trace a keymap.
 *
 * This function leaves a value in the global "keymap_trace", which is used
 * by the action_debug function when subsequent actions are called.
 */
void
PA_KeymapTrace_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	if (!toggled(EVENT_TRACE) || *num_params != 2)
		return;
	Replace(keymap_trace, XtMalloc(strlen(params[0]) + 1 +
				       strlen(params[1]) + 1));
	(void) sprintf(keymap_trace, "%s:%s", params[0], params[1]);
}
#endif /*]*/

/*
 * End a keymap trace.
 *
 * This function clears the value in the global "keymap_trace".
 */
void
PA_End_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params unused)
{
	Replace(keymap_trace, CN);
}

/*
 * Translation table cache.
 */
XtTranslations
lookup_tt(const char *name, char *table)
{
	struct tt_cache {
		char *name;
		XtTranslations trans;
		struct tt_cache *next;
	};
#	define TTN (struct tt_cache *)NULL
	static struct tt_cache *tt_cache = TTN;
	struct tt_cache *t;
	char *xtable;

	/* Look for an old one. */
	for (t = tt_cache; t != TTN; t = t->next)
		if (!strcmp(name, t->name))
			return t->trans;

	/* Allocate and translate a new one. */
	t = (struct tt_cache *)XtMalloc(sizeof(*t));
	t->name = XtNewString(name);
	xtable = expand_table(name, table);
	t->trans = XtParseTranslationTable(xtable);
	Free(xtable);
	t->next = tt_cache;
	tt_cache = t;

	return t->trans;
}
#undef TTN

/*
 * Set or clear a temporary keymap.
 *
 * If the parameter is CN, removes all keymaps.
 * Otherwise, toggles the keymap by that name.
 *
 * Returns 0 if the action was successful, -1 otherwise.
 *
 */
int
temporary_keymap(char *k)
{
	char *km;
	XtTranslations trans;
	struct trans_list *t, *prev;
	char *path = CN;
#	define TN (struct trans_list *)NULL

	if (k == CN) {
		struct trans_list *next;

		/* Delete all temporary keymaps. */
		for (t = temp_keymaps; t != TN; t = next) {
			Free(t->name);
			Free(t->pathname);
			next = t->next;
			Free(t);
		}
		tkm_last = temp_keymaps = TN;
		screen_set_temp_keymap((XtTranslations)NULL);
		keypad_set_temp_keymap((XtTranslations)NULL);
		status_kmap(False);
		km_regen();
		return 0;
	}

	/* Check for deleting one keymap. */
	for (prev = TN, t = temp_keymaps; t != TN; prev = t, t = t->next)
		if (!strcmp(k, t->name))
			break;
	if (t != TN) {

		/* Delete the keymap from the list. */
		if (prev != TN)
			prev->next = t->next;
		else
			temp_keymaps = t->next;
		if (tkm_last == t)
			tkm_last = prev;
		Free(t->name);
		Free(t);

		/* Rebuild the translation tables from the remaining ones. */
		screen_set_temp_keymap((XtTranslations)NULL);
		keypad_set_temp_keymap((XtTranslations)NULL);
		for (t = temp_keymaps; t != TN; t = t->next) {
			trans = lookup_tt(t->name, CN);
			screen_set_temp_keymap(trans);
			keypad_set_temp_keymap(trans);
		}

		/* Update the status line. */
		if (temp_keymaps == TN)
			status_kmap(False);
		km_regen();
		return 0;
	}

	/* Add a keymap. */

	/* Try a file first. */
	km = get_file_keymap(k, &path);
	if (km == CN) {
		/* Then try a resource. */
		km = get_fresource("%s.%s", ResKeymap, k);
		if (km == CN)
			return -1;
	}

	/* Update the translation tables. */
	trans = lookup_tt(k, km);
	screen_set_temp_keymap(trans);
	keypad_set_temp_keymap(trans);

	/* Add it to the list. */
	t = (struct trans_list *)XtMalloc(sizeof(*t));
	t->name = XtNewString(k);
	t->pathname = path;
	t->is_temp = True;
	t->from_server = False;
	t->next = TN;
	if (tkm_last != TN)
		tkm_last->next = t;
	else
		temp_keymaps = t;
	tkm_last = t;

	/* Update the status line. */
	status_kmap(True);
	km_regen();

	/* Success. */
	return 0;
}
#undef TN

#if defined(X3270_MENUS) /*[*/
/* Create and pop up the current keymap pop-up. */
void
do_keymap_display(Widget w unused, XtPointer userdata unused,
    XtPointer calldata unused)
{
	Widget form, label, done;

	/* If it's already up, do nothing. */
	if (km_isup)
		return;
	if (km_exists) {
		popup_popup(km_shell, XtGrabNone);
		return;
	}

	/* Create the popup. */
	km_shell = XtVaCreatePopupShell(
	    "kmPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(km_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(km_shell, XtNpopupCallback, km_up, (XtPointer)NULL);
	XtAddCallback(km_shell, XtNpopdownCallback, km_down, (XtPointer)NULL);

	/* Create a form in the popup. */
	form = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, km_shell,
	    NULL);

	/* Create the title. */
	label = XtVaCreateManagedWidget("label", labelWidgetClass, form,
	    XtNborderWidth, 0,
	    NULL);

	/* Create the options. */
	sort_event = XtVaCreateManagedWidget("sortEventOption",
	    commandWidgetClass, form,
	    XtNborderWidth, 0,
	    XtNfromVert, label,
	    XtNleftBitmap, sort == SORT_EVENT ? diamond : no_diamond,
	    NULL);
	XtAddCallback(sort_event, XtNcallback, do_sort_event,
	    (XtPointer)NULL);
	sort_keymap = XtVaCreateManagedWidget("sortKeymapOption",
	    commandWidgetClass, form,
	    XtNborderWidth, 0,
	    XtNfromVert, sort_event,
	    XtNleftBitmap, sort == SORT_KEYMAP ? diamond : no_diamond,
	    NULL);
	XtAddCallback(sort_keymap, XtNcallback, do_sort_keymap,
	    (XtPointer)NULL);
	sort_action = XtVaCreateManagedWidget("sortActionOption",
	    commandWidgetClass, form,
	    XtNborderWidth, 0,
	    XtNfromVert, sort_keymap,
	    XtNleftBitmap, sort == SORT_ACTION ? diamond : no_diamond,
	    NULL);
	XtAddCallback(sort_action, XtNcallback, do_sort_action,
	    (XtPointer)NULL);

	/* Create a text widget attached to the file. */
	text = XtVaCreateManagedWidget(
	    "text", asciiTextWidgetClass, form,
	    XtNfromVert, sort_action,
	    XtNscrollHorizontal, XawtextScrollAlways,
	    XtNscrollVertical, XawtextScrollAlways,
	    XtNdisplayCaret, False,
	    NULL);
	create_text();

	/* Create the Done button. */
	done = XtVaCreateManagedWidget(ObjConfirmButton,
	    commandWidgetClass, form,
	    XtNfromVert, text,
	    NULL);
	XtAddCallback(done, XtNcallback, km_done, (XtPointer)NULL);

	/* Pop it up. */
	km_exists = True;
	popup_popup(km_shell, XtGrabNone);
}

/* Called when x3270 is exiting. */
static void
remove_keymap_file(Boolean ignored unused)
{
	(void) unlink(km_file);
}


/* Format the keymap into a text source. */
static void
create_text(void)
{
	String s;
	FILE *f;
	static Widget source = NULL;

	/* Ready a file. */
	(void) sprintf(km_file, "/tmp/km.%d", getpid());
	f = fopen(km_file, "w");
	if (f == (FILE *)NULL) {
		popup_an_errno(errno, "temporary file open");
		return;
	}

	s = _XtPrintXlations(*screen, (*screen)->core.tm.translations, NULL,
	    True);
	format_xlations(s, f);
	XtFree(s);
	fclose(f);
	if (source != NULL) {
		XtVaSetValues(source, XtNstring, km_file, NULL);
	} else {
		source = XtVaCreateWidget(
		    "source", asciiSrcObjectClass, text,
		    XtNtype, XawAsciiFile,
		    XtNstring, km_file,
		    XtNeditType, XawtextRead,
		    NULL);
		XawTextSetSource(text, source, (XawTextPosition)0);

		register_schange(ST_EXITING, remove_keymap_file);
	}
}

/* Refresh the keymap display, if it's up. */
static void
km_regen(void)
{
	if (km_exists)
		create_text();
}

/* Popup callback. */
static void
km_up(Widget w unused, XtPointer client_data unused, XtPointer call_data unused)
{
	km_isup = True;
}

/* Popdown callback. */
static void
km_down(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	km_isup = False;
}

/* Done button callback.  Pop down the widget. */
static void
km_done(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	XtPopdown(km_shell);
}

/* "Sort-by-event" button callback. */
static void
do_sort_event(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	if (sort != SORT_EVENT) {
		sort = SORT_EVENT;
		XtVaSetValues(sort_action, XtNleftBitmap, no_diamond, NULL);
		XtVaSetValues(sort_keymap, XtNleftBitmap, no_diamond, NULL);
		XtVaSetValues(sort_event, XtNleftBitmap, diamond, NULL);
		create_text();
	}
}

/* "Sort-by-keymap" button callback. */
static void
do_sort_keymap(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	if (sort != SORT_KEYMAP) {
		sort = SORT_KEYMAP;
		XtVaSetValues(sort_action, XtNleftBitmap, no_diamond, NULL);
		XtVaSetValues(sort_keymap, XtNleftBitmap, diamond, NULL);
		XtVaSetValues(sort_event, XtNleftBitmap, no_diamond, NULL);
		create_text();
	}
}

/* "Sort-by-action" button callback. */
static void
do_sort_action(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	if (sort != SORT_ACTION) {
		sort = SORT_ACTION;
		XtVaSetValues(sort_action, XtNleftBitmap, diamond, NULL);
		XtVaSetValues(sort_keymap, XtNleftBitmap, no_diamond, NULL);
		XtVaSetValues(sort_event, XtNleftBitmap, no_diamond, NULL);
		create_text();
	}
}

#define DASHES \
    "-------------------------- ---------------- ------------------------------------"

/*
 * Format translations for display.
 *
 * The data from _XtPrintXlations looks like this:
 *  [<space>]event:<space>[PA-KeymapTrace("keymap","line")<space>][action...]
 * with the delightful complication that embedded quotes are not quoted.
 *
 * What we want to do is to:
 *  remove all lines without PA-KeymapTrace
 *  remove the leading space
 *  sort by actions list
 *  reformat as:
 *    action... event (keymap:line)
 */
static void
format_xlations(String s, FILE *f)
{
	char *t;
	char *t_next;
	struct xl {
		struct xl *next;
		char *actions;
		char *event;
		char *keymap;
		int km_line;
		char *full_keymap;
	} *xl_list = (struct xl *)NULL, *x, *xs, *xlp, *xn;
	char *km_last;
	int line_last = 0;

	/* Construct the list. */
	for (t = s; t != CN; t = t_next) {
		char *k, *a, *kk;
		int nq;
		static char cmps[] = ": " PA_KEYMAP_TRACE "(";

		/* Find the end of this rule and terminate this line. */
		t_next = strstr(t, PA_ENDL "\n");
		if (t_next != CN) {
			t_next += strlen(PA_ENDL);
			*t_next++ = '\0';
		}

		/* Remove the leading space. */
		while (*t == ' ')
			t++;

		/* Use only traced events. */
		k = strstr(t, cmps);
		if (k == CN)
			continue;
		*k = '\0';
		k += strlen(cmps);

		/* Find the rest of the actions. */
		a = strchr(k, ')');
		if (a == CN)
			continue;
		while (*(++a) == ' ')
			;
		if (!*a)
			continue;

		/* Remove the trailing PA-End call. */
		if (strlen(a) >= strlen(PA_ENDL) &&
		    !strcmp(a + strlen(a) - strlen(PA_ENDL), PA_ENDL))
			a[strlen(a) - strlen(PA_ENDL)] = '\0';

		/* Allocate the new element. */
		x = (struct xl *)XtCalloc(sizeof(struct xl), 1);
		x->actions = XtNewString(a);
		x->event = XtNewString(t);
		x->keymap = kk = (char *)XtMalloc(a - k + 1);
		x->km_line = 0;
		x->full_keymap = (char *)XtMalloc(a - k + 1);
		nq = 0;
		while (*k != ')') {
			if (*k == '"') {
				nq++;
			} else if (nq == 1) {
				*kk++ = *k;
			} else if (nq == 3) {
				x->km_line = atoi(k);
				break;
			}
			k++;
		}
		*kk = '\0';
		(void) sprintf(x->full_keymap, "%s:%d", x->keymap, x->km_line);

		/* Find where it should be inserted. */
		for (xs = xl_list, xlp = (struct xl *)NULL;
		     xs != (struct xl *)NULL;
		     xlp = xs, xs = xs->next) {
			int halt = 0;

			switch (sort) {
			    case SORT_EVENT:
				halt = (event_cmp(xs->event, x->event) > 0);
				break;
			    case SORT_KEYMAP:
				halt = (keymap_cmp(xs->keymap, xs->km_line,
						   x->keymap, x->km_line) > 0);
				break;
			    case SORT_ACTION:
				halt = (action_cmp(xs->actions, a) > 0);
				break;
			}
			if (halt)
				break;
		}

		/* Insert it. */
		if (xlp != (struct xl *)NULL) {
			x->next = xlp->next;
			xlp->next = x;
		} else {
			x->next = xl_list;
			xl_list = x;
		}
	}

	/* Walk it. */
	if (sort != SORT_KEYMAP)
		(void) fprintf(f, "%-26s %-16s %s\n%s\n",
		    get_message("kmEvent"),
		    get_message("kmKeymapLine"),
		    get_message("kmActions"),
		    DASHES);
	km_last = CN;
	for (xs = xl_list; xs != (struct xl *)NULL; xs = xs->next) {
		switch (sort) {
		    case SORT_EVENT:
			if (km_last != CN) {
				char *l;

				l = strchr(xs->event, '<');
				if (l != CN) {
					if (strcmp(km_last, l))
						(void) fprintf(f, "\n");
					km_last = l;
				}
			} else
				km_last = strchr(xs->event, '<');
			break;
		    case SORT_KEYMAP:
			if (km_last == CN || strcmp(xs->keymap, km_last)) {
				char *p;

				(void) fprintf(f, "%s%s '%s'%s",
				    km_last == CN ? "" : "\n",
				    get_message(is_temp(xs->keymap) ?
						    "kmTemporaryKeymap" :
						    "kmKeymap"),
				    xs->keymap,
				    from_server(xs->keymap) ?
					get_message("kmFromServer") : "");
				if ((p = pathname(xs->keymap)) != CN)
					(void) fprintf(f, ", %s %s",
					    get_message("kmFile"), p);
				else
					(void) fprintf(f,
					    ", %s %s.%s.%s",
					    get_message("kmResource"),
					    programname, ResKeymap, xs->keymap);
				(void) fprintf(f,
				    "\n%-26s %-16s %s\n%s\n",
				    get_message("kmEvent"),
				    get_message("kmKeymapLine"),
				    get_message("kmActions"),
				    DASHES);
				km_last = xs->keymap;
				line_last = 0;
			}
			while (xs->km_line != ++line_last) {
				(void) fprintf(f, "%-26s %s:%d\n",
				    get_message("kmOverridden"),
				    xs->keymap, line_last);
			}
			break;
		    case SORT_ACTION:
			break;
		}
		(void) fprintf(f, "%-26s %-16s ",
		    xs->event, xs->full_keymap);
		(void) fcatv(f, xs->actions);
		(void) fputc('\n', f);
	}

	/* Free it. */
	for (xs = xl_list; xs != (struct xl *)NULL; xs = xn) {
		xn = xs->next;
		Free(xs->actions);
		Free(xs->event);
		Free(xs->keymap);
		Free(xs->full_keymap);
		Free(xs);
	}
}

/*
 * Comparison function for actions.  Basically, a strcmp() that handles "PF"
 * and "PA" specially.
 */
#define PA	"PA("
#define PF	"PF("
static int
action_cmp(char *s1, char *s2)
{
	if ((!strncmp(s1, PA, 3) && !strncmp(s2, PA, 3)) ||
	    (!strncmp(s1, PF, 3) && !strncmp(s2, PF, 3)))
		return atoi(s1 + 4) - atoi(s2 + 4);
	else
		return strcmp(s1, s2);
	    
}

/* Return a keymap's index in the lists. */
static int
km_index(char *n)
{
	struct trans_list *t;
	int ix = 0;

	for (t = trans_list; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, n))
			return ix;
		ix++;
	}
	for (t = temp_keymaps; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, n))
			return ix;
		ix++;
	}
	return ix;
}

/* Return whether or not a keymap is temporary. */
static Boolean
is_temp(char *k)
{
	struct trans_list *t;

	for (t = trans_list; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, k))
			return t->is_temp;
	}
	for (t = temp_keymaps; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, k))
			return t->is_temp;
	}
	return False;
}

/* Return the pathname associated with a keymap. */
static char *
pathname(char *k)
{
	struct trans_list *t;

	for (t = trans_list; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, k))
			return t->pathname;
	}
	for (t = temp_keymaps; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, k))
			return t->pathname;
	}
	return CN;
}

/* Return whether or not a keymap was translated from "@server". */
static Boolean
from_server(char *k)
{
	struct trans_list *t;

	for (t = trans_list; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, k))
			return t->from_server;
	}
	for (t = temp_keymaps; t != (struct trans_list *)NULL; t = t->next) {
		if (!strcmp(t->name, k))
			return t->from_server;
	}
	return False;
}

/*
 * Comparison function for keymaps.
 */
static int
keymap_cmp(char *k1, int l1, char *k2, int l2)
{
	/* If the keymaps are the same, do a numerical comparison. */
	if (!strcmp(k1, k2))
		return l1 - l2;


	/* The keymaps are different.  Order them according to trans_list. */
	return km_index(k1) - km_index(k2);
}

/*
 * strcmp() that handles <KeyPress>Fnn numercally.
 */
static int
Fnn_strcmp(char *s1, char *s2)
{
	static char kp[] = "<KeyPress>F";
#	define KPL (sizeof(kp) - 1)

	if (strncmp(s1, kp, KPL) || !isdigit(s1[KPL]) ||
	    strncmp(s2, kp, KPL) || !isdigit(s2[KPL]))
		return strcmp(s1, s2);
	return atoi(s1 + KPL) - atoi(s2 + KPL);
}

/*
 * Comparison function for events.
 */
static int
event_cmp(char *e1, char *e2)
{
	char *l1, *l2;
	int r;

	/* If either has a syntax problem, do a straight string compare. */
	if ((l1 = strchr(e1, '<')) == CN ||
	    (l2 = strchr(e2, '<')) == CN)
		return strcmp(e1, e2);

	/*
	 * If the events are different, sort on the event only.  Otherwise,
	 * sort on the modifier(s).
	 */
	r = Fnn_strcmp(l1, l2);
	if (r)
		return r;
	else
		return strcmp(e1, e2);
}
#endif /*]*/
