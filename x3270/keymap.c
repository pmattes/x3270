/*
 * Copyright (c) 1996-2024 Paul Mattes.
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
 *	keymap.c
 *		This module handles keymaps.
 */

#include "globals.h"
#include "xglobals.h"

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

#include "host.h"
#include "keymap.h"
#include "kybd.h"
#include "popups.h"
#include "screen.h"
#include "status.h"
#include "utils.h"
#include "xactions.h"
#include "xkeypad.h"
#include "xmenubar.h"
#include "xscreen.h"
#include "xstatus.h"
#include "xpopups.h"

#define PA_ENDL		" " PA_END "()"
#define Res3270		"3270"

bool keymap_changed = false;
struct trans_list *trans_list = NULL;
static struct trans_list **last_trans = &trans_list;
static struct trans_list *tkm_last;
struct trans_list *temp_keymaps;	/* temporary keymap list */
char *keymap_trace = NULL;
static char *last_keymap = NULL;
static bool last_nvt = false;
static bool last_3270 = false;

static void setup_keymaps(const char *km, bool do_popup);
static void add_keymap(const char *name, bool do_popup);
static void add_trans(const char *name, char *translations, char *pathname,
    bool is_from_server);
static char *get_file_keymap(const char *name, char **pathp);
static void keymap_3270_mode(bool);

/* Undocumented Xt function to convert translations to text. */
extern String _XtPrintXlations(Widget w, XtTranslations xlations,
    Widget accelWidget, bool includeRHS);

static enum { SORT_EVENT, SORT_KEYMAP, SORT_ACTION } sort = SORT_KEYMAP;

static bool km_isup = false;
static bool km_exists = false;
static Widget km_shell, sort_event, sort_keymap, sort_byaction, text;
static char km_file[128];
static void create_text(void);
static void km_up(Widget w, XtPointer client_data, XtPointer call_data);
static void km_down(Widget w, XtPointer client_data, XtPointer call_data);
static void km_done(Widget w, XtPointer client_data, XtPointer call_data);
static void do_sort_byaction(Widget w, XtPointer client_data,
    XtPointer call_data);
static void do_sort_keymap(Widget w, XtPointer client_data,
    XtPointer call_data);
static void do_sort_event(Widget w, XtPointer client_data, XtPointer call_data);
static void format_xlations(String s, FILE *f);
static int action_cmp(char *s1, char *s2);
static int keymap_cmp(char *k1, int l1, char *k2, int l2);
static int event_cmp(char *e1, char *e2);
static bool is_temp(char *k);
static char *pathname(char *k);
static bool from_server(char *k);
static void km_regen(void);

char *current_keymap = NULL;

/**
 * Keymap module registration.
 */
void
keymap_register(void)
{
    register_schange(ST_3270_MODE, keymap_3270_mode);
    register_schange(ST_CONNECT, keymap_3270_mode);
}

/* Keymap initialization. */
void
keymap_init(const char *km, bool interactive)
{
    static bool initted = false;

    if (km == NULL &&
	(km = (char *)getenv("KEYMAP")) == NULL &&
	(km = (char *)getenv("KEYBD")) == NULL) {
	km = "@server";
    }
    setup_keymaps(km, interactive);
    if (!initted) {
	initted = true;
	last_nvt = IN_NVT;
	last_3270 = IN_3270;
    } else {
	struct trans_list *t;
	XtTranslations trans;

	screen_set_keymap();
	keypad_set_keymap();

	/* Re-apply any temporary keymaps. */
	for (t = temp_keymaps; t != NULL; t = t->next) {
	    trans = lookup_tt(t->name, NULL);
	    screen_set_temp_keymap(trans);
	    keypad_set_temp_keymap(trans);
	}
    }
    km_regen();

    /* Save the name(s) of the last keymap, so we can switch modes later. */
    if (km != last_keymap) {
	Replace(last_keymap, km? NewString(km): NULL);
    }
}

/*
 * 3270/NVT mode change.
 */
static void
keymap_3270_mode(bool ignored _is_unused)
{
    if (last_nvt != IN_NVT || last_3270 != IN_3270) {
	last_nvt = IN_NVT;
	last_3270 = IN_3270;

	/* Switch between 3270 and NVT keymaps. */
	keymap_init(last_keymap, false);
    }
}

/*
 * Set up a user keymap.
 */
static void
setup_keymaps(const char *km, bool do_popup)
{
    char *bkm;
    struct trans_list *t;
    struct trans_list *next;

    /* Make sure it starts with "base". */
    if (km == NULL) {
	bkm = XtNewString("base");
    } else {
	bkm = Asprintf("base,%s", km);
    }

    if (do_popup) {
	keymap_changed = true;
    }

    /* Clear out any existing translations. */
    Replace(current_keymap, NULL);
    for (t = trans_list; t != NULL; t = next) {
	next = t->next;
	Free(t->name);
	Free(t->pathname);
	Free(t);
    }
    trans_list = NULL;
    last_trans = &trans_list;

    /* Build up the new list. */
    if (bkm != NULL) {
	char *ns = XtNewString(bkm);
	char *n0 = ns;
	char *comma;

	do {
	    comma = strchr(ns, ',');
	    if (comma) {
		*comma = '\0';
	    }
	    add_keymap(ns, do_popup);
	    if (comma) {
		ns = comma + 1;
	    } else {
		ns = NULL;
	    }
	} while (ns);

	XtFree(n0);
    }
    XtFree(bkm);
}

/*
 * Get a keymap from a file.
 */
static char *
get_file_keymap(const char *name, char **pathp)
{
    char *path;
    XrmDatabase dd = NULL;
    char *resname;
    XrmValue value;
    char *type;
    char *r = NULL;

    *pathp = NULL;

    /* Look for a global keymap file. */
    if (dd == NULL) {
	path = Asprintf("%s/keymap.%s", appres.conf_dir, name);
	dd = XrmGetFileDatabase(path);
	if (dd != NULL) {
	    *pathp = path;
	} else {
	    XtFree(path);
	    return NULL;
	}
    }

    /* Look up the resource in that file. */
    resname = Asprintf("%s.%s.%s", XtName(toplevel), ResKeymap, name);
    if ((XrmGetResource(dd, resname, 0, &type, &value) == True) &&
	    *value.addr) {
	r = XtNewString(value.addr);
    } else {
	*pathp = NULL;
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
add_keymap(const char *name, bool do_popup)
{
    char *translations, *translations_nvt, *translations_3270;
    char *buf, *buf_nvt, *buf_3270;
    int any = 0;
    char *path, *path_nvt, *path_3270;
    bool is_from_server = false;

    if (strcmp(name, "base")) {
	if (current_keymap == NULL) {
	    current_keymap = XtNewString(name);
	} else {
	    Replace(current_keymap, Asprintf("%s,%s", current_keymap, name));
	}
    }

    /* Translate '@server' to a vendor-specific keymap. */
    if (!strcmp(name, "@server")) {
	struct sk {
	    struct sk *next;
	    char *vendor;
	    char *keymap;
	};
	static struct sk *sk_list = NULL;
	struct sk *sk;

	if (sk_list == NULL) {
	    char *s, *vendor, *keymap;

	    s = get_resource("serverKeymapList");
	    if (s == NULL) {
		return;
	    }
	    s = XtNewString(s);
	    while (split_dresource(&s, &vendor, &keymap) == 1) {
		sk = (struct sk *)XtMalloc(sizeof(struct sk));
		sk->vendor = vendor;
		sk->keymap = keymap;
		sk->next = sk_list;
		sk_list = sk;
	    }
	}
	for (sk = sk_list; sk != NULL; sk = sk->next) {
	    if (!strcmp(sk->vendor, ServerVendor(display))) {
		name = sk->keymap;
		is_from_server = true;
		break;
	    }
	}
	if (sk == NULL) {
	    return;
	}
    }

    /* Try for a file first, then resources. */
    translations = get_file_keymap(name, &path);
    buf_nvt = Asprintf("%s.%s", name, ResNvt);
    translations_nvt = get_file_keymap(buf_nvt, &path_nvt);
    buf_3270 = Asprintf("%s.%s", name, Res3270);
    translations_3270 = get_file_keymap(buf_3270, &path_3270);
    if (translations != NULL || translations_nvt != NULL ||
	    translations_3270 != NULL) {
	any++;
	if (translations != NULL) {
	    add_trans(name, translations, path, is_from_server);
	}
	if (IN_NVT && translations_nvt != NULL) {
	    add_trans(buf_nvt, translations_nvt, path_nvt, is_from_server);
	}
	if (IN_3270 && translations_3270 != NULL) {
	    add_trans(buf_3270, translations_3270, path_3270, is_from_server);
	}
	XtFree(translations);
	XtFree(translations_nvt);
	XtFree(translations_3270);
	XtFree(buf_nvt);
	XtFree(buf_3270);
    } else {
	XtFree(buf_nvt);
	XtFree(buf_3270);

	/* Shared keymap. */
	buf = Asprintf("%s.%s", ResKeymap, name);
	translations = get_resource(buf);
	buf_nvt = Asprintf("%s.%s.%s", ResKeymap, name, ResNvt);
	translations_nvt = get_resource(buf_nvt);
	buf_3270 = Asprintf("%s.%s.%s", ResKeymap, name, Res3270);
	translations_3270 = get_resource(buf_3270);
	if (translations != NULL || translations_nvt != NULL ||
		translations_3270) {
	    any++;
	}
	if (translations != NULL) {
	    add_trans(name, translations, NULL, is_from_server);
	}
	if (IN_NVT && translations_nvt != NULL) {
	    add_trans(buf_nvt + strlen(ResKeymap) + 1, translations_nvt, NULL,
		    is_from_server);
	}
	if (IN_3270 && translations_3270 != NULL) {
	    add_trans(buf_3270 + strlen(ResKeymap) + 1, translations_3270,
		    NULL, is_from_server);
	}
	XtFree(buf);
	XtFree(buf_nvt);
	XtFree(buf_3270);

	/* User keymap */
	buf = Asprintf("%s.%s.%s", ResKeymap, name, ResUser);
	translations = get_resource(buf);
	buf_nvt = Asprintf("%s.%s.%s.%s", ResKeymap, name, ResNvt, ResUser);
	translations_nvt = get_resource(buf_nvt);
	buf_3270 = Asprintf("%s.%s.%s.%s", ResKeymap, name, Res3270, ResUser);
	translations_3270 = get_resource(buf_3270);
	if (translations != NULL || translations_nvt != NULL ||
		translations_3270 != NULL) {
	    any++;
	}
	if (IN_NVT && translations_nvt != NULL) {
	    add_trans(buf_nvt + strlen(ResKeymap) + 1, translations_nvt, NULL,
		    is_from_server);
	}
	if (IN_3270 && translations_3270 != NULL) {
	    add_trans(buf_3270 + strlen(ResKeymap) + 1, translations_3270,
		    NULL, is_from_server);
	}
	if (translations != NULL) {
	    add_trans(buf, translations, NULL, is_from_server);
	}
	XtFree(buf);
	XtFree(buf_nvt);
	XtFree(buf_3270);
    }

    if (!any) {
	if (do_popup) {
	    popup_an_error("Cannot find %s \"%s\"", ResKeymap, name);
	} else {
	    xs_warning("Cannot find %s \"%s\"", ResKeymap, name);
	}
    }
}

/*
 * Add a single keymap name and translation to the translation list.
 */
static void
add_trans(const char *name, char *translations, char *path_name,
	bool is_from_server)
{
    struct trans_list *t;

    t = (struct trans_list *)XtMalloc(sizeof(*t));
    t->name = XtNewString(name);
    t->pathname = path_name;
    t->is_temp = false;
    t->from_server = is_from_server;
    lookup_tt(name, translations);
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
    bool bs = false;
    enum { UQ_BASE, UQ_PLIST, UQ_Q } state = UQ_BASE;
    char c;

    for ( ; (c = *s); s++) {
	if (bs) {
	    bs = false;
	    continue;
	} else if (c == '\\') {
	    bs = true;
	    continue;
	}
	switch (state) {
	case UQ_BASE:
	    if (c == '(') {
		state = UQ_PLIST;
	    } else if (c == '\n') {
		return s;
	    }
	    break;
	case UQ_PLIST:
	    if (c == ')') {
		state = UQ_BASE;
	    }
	    else if (c == '"') {
		state = UQ_Q;
	    }
	    break;
	case UQ_Q:
	    if (c == '"') {
		state = UQ_PLIST;
	    }
	    break;
	}
    }
    return NULL;
}

/* Expand a translation table with keymap tracing calls. */
static char *
expand_table(const char *name, char *table)
{
    char *cm, *t0, *t, *s;
    int nlines = 1;

    if (table == NULL) {
	return NULL;
    }

    /* Roughly count the number of lines in the table. */
    cm = table;
    while ((cm = strchr(cm, '\n')) != NULL) {
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
	while (*s == ' ' || *s == '\t') {
	    s++;
	}
	if (*s == '\n') {
	    *t++ = '\n';
	    *t = '\0';
	    s++;
	    continue;
	}

	/* Find the '>' from the event name, and copy up through it. */
	cm = strchr(s, '>');
	if (cm == NULL) {
	    while ((*t++ = *s++))
		;
	    break;
	}
	while (s <= cm) {
	    *t++ = *s++;
	}

	/* Find the ':' following, and copy up throught that. */
	cm = strchr(s, ':');
	if (cm == NULL) {
	    while ((*t++ = *s++))
		;
	    break;
	}
	nlines++;
	while (s <= cm) {
	    *t++ = *s++;
	}

	/* Insert a PA-KeymapTrace call. */
	sprintf(t, " " PA_KEYMAP_TRACE "(%s,%d) ", name, nlines);
	t = strchr(t, '\0');

	/*
	 * Copy to the next unquoted newline and append a PA-End call.
	 */
	cm = unquoted_newline(s);
	if (cm == NULL) {
	    while ((*t = *s)) {
		t++;
		s++;
	    }
	} else {
	    while (s < cm) {
		*t++ = *s++;
	    }
	}
	strcpy(t, PA_ENDL);
	t += strlen(PA_ENDL);
	if (cm == NULL) {
	    break;
	} else {
	    *t++ = *s++;
	}
    }
    *t = '\0';

    return t0;
}

/*
 * Trace a keymap.
 *
 * This function leaves a value in the global "keymap_trace", which is used
 * by the xaction_debug function when subsequent actions are called.
 */
void
PA_KeymapTrace_xaction(Widget w _is_unused, XEvent *event _is_unused,
	String *params, Cardinal *num_params)
{
    if (!toggled(TRACING) || *num_params != 2) {
	return;
    }
    Replace(keymap_trace, XtMalloc(strlen(params[0]) + 1 +
				   strlen(params[1]) + 1));
    sprintf(keymap_trace, "%s:%s", params[0], params[1]);
}

/*
 * End a keymap trace.
 *
 * This function clears the value in the global "keymap_trace".
 */
void
PA_End_xaction(Widget w _is_unused, XEvent *event _is_unused,
	String *params _is_unused, Cardinal *num_params _is_unused)
{
    Replace(keymap_trace, NULL);
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
    static struct tt_cache *tt_cache = NULL;
    struct tt_cache *t;
    char *xtable;

    /* Look for an old one. */
    for (t = tt_cache; t != NULL; t = t->next) {
	if (!strcmp(name, t->name)) {
	    return t->trans;
	}
    }

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

/*
 * Set or clear a temporary keymap.
 *
 * If the parameter is NULL, removes all keymaps.
 * Otherwise, toggles the keymap by that name.
 *
 * Returns true if the action was successful, false otherwise.
 *
 */
bool
temporary_keymap(const char *k)
{
    char *km;
    XtTranslations trans;
    struct trans_list *t, *prev;
    char *path = NULL;

    if (k == NULL) {
	struct trans_list *next;

	/* Delete all temporary keymaps. */
	for (t = temp_keymaps; t != NULL; t = next) {
	    Free(t->name);
	    Free(t->pathname);
	    next = t->next;
	    Free(t);
	}
	tkm_last = temp_keymaps = NULL;
	screen_set_temp_keymap(NULL);
	keypad_set_temp_keymap(NULL);
	status_kmap(false);
	km_regen();
	return true;
    }

    /* Check for deleting one keymap. */
    for (prev = NULL, t = temp_keymaps; t != NULL; prev = t, t = t->next) {
	if (!strcmp(k, t->name)) {
	    break;
	}
    }
    if (t != NULL) {

	/* Delete the keymap from the list. */
	if (prev != NULL) {
	    prev->next = t->next;
	} else {
	    temp_keymaps = t->next;
	}
	if (tkm_last == t) {
	    tkm_last = prev;
	}
	Free(t->name);
	Free(t);

	/* Rebuild the translation tables from the remaining ones. */
	screen_set_temp_keymap(NULL);
	keypad_set_temp_keymap(NULL);
	for (t = temp_keymaps; t != NULL; t = t->next) {
	    trans = lookup_tt(t->name, NULL);
	    screen_set_temp_keymap(trans);
	    keypad_set_temp_keymap(trans);
	}

	/* Update the status line. */
	if (temp_keymaps == NULL) {
	    status_kmap(false);
	}
	km_regen();
	return true;
    }

    /* Add a keymap. */

    /* Try a file first. */
    km = get_file_keymap(k, &path);
    if (km == NULL) {
	/* Then try a resource. */
	km = get_fresource("%s.%s", ResKeymap, k);
	if (km == NULL) {
	    return false;
	}
    }

    /* Update the translation tables. */
    trans = lookup_tt(k, km);
    screen_set_temp_keymap(trans);
    keypad_set_temp_keymap(trans);

    /* Add it to the list. */
    t = (struct trans_list *)XtMalloc(sizeof(*t));
    t->name = XtNewString(k);
    t->pathname = path;
    t->is_temp = true;
    t->from_server = false;
    t->next = NULL;
    if (tkm_last != NULL) {
	tkm_last->next = t;
    } else {
	temp_keymaps = t;
    }
    tkm_last = t;

    /* Update the status line. */
    status_kmap(true);
    km_regen();

    /* Success. */
    return true;
}

/* Create and pop up the current keymap pop-up. */
void
do_keymap_display(Widget w _is_unused, XtPointer userdata _is_unused,
    XtPointer calldata _is_unused)
{
    Widget form, label, done;

    /* If it's already up, do nothing. */
    if (km_isup) {
	return;
    }
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
    XtAddCallback(km_shell, XtNpopupCallback, km_up, NULL);
    XtAddCallback(km_shell, XtNpopdownCallback, km_down, NULL);

    /* Create a form in the popup. */
    form = XtVaCreateManagedWidget(ObjDialog, formWidgetClass, km_shell,
	    NULL);

    /* Create the title. */
    label = XtVaCreateManagedWidget("label", labelWidgetClass, form,
	    XtNborderWidth, 0, NULL);

    /* Create the options. */
    sort_event = XtVaCreateManagedWidget("sortEventOption",
	    commandWidgetClass, form,
	    XtNborderWidth, 0,
	    XtNfromVert, label,
	    XtNleftBitmap, sort == SORT_EVENT ? diamond : no_diamond,
	    NULL);
    XtAddCallback(sort_event, XtNcallback, do_sort_event, NULL);
    sort_keymap = XtVaCreateManagedWidget("sortKeymapOption",
	    commandWidgetClass, form,
	    XtNborderWidth, 0,
	    XtNfromVert, sort_event,
	    XtNleftBitmap, sort == SORT_KEYMAP ? diamond : no_diamond,
	    NULL);
    XtAddCallback(sort_keymap, XtNcallback, do_sort_keymap, NULL);
    sort_byaction = XtVaCreateManagedWidget("sortActionOption",
	    commandWidgetClass, form,
	    XtNborderWidth, 0,
	    XtNfromVert, sort_keymap,
	    XtNleftBitmap, sort == SORT_ACTION ? diamond : no_diamond,
	    NULL);
    XtAddCallback(sort_byaction, XtNcallback, do_sort_byaction, NULL);

    /* Create a text widget attached to the file. */
    text = XtVaCreateManagedWidget(
	    "text", asciiTextWidgetClass, form,
	    XtNfromVert, sort_byaction,
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
    XtAddCallback(done, XtNcallback, km_done, NULL);

    /* Pop it up. */
    km_exists = true;
    popup_popup(km_shell, XtGrabNone);
}

/* Called when x3270 is exiting. */
static void
remove_keymap_file(bool ignored _is_unused)
{
    unlink(km_file);
}


/* Format the keymap into a text source. */
static void
create_text(void)
{
    String s;
    FILE *f;
    static Widget source = NULL;

    /* Ready a file. */
    snprintf(km_file, sizeof(km_file), "/tmp/km.%u", (unsigned)getpid());
    f = fopen(km_file, "w");
    if (f == NULL) {
	popup_an_errno(errno, "temporary file open");
	return;
    }

    s = _XtPrintXlations(*screen, (*screen)->core.tm.translations, NULL, True);
    format_xlations(s, f);
    XtFree(s);
    fclose(f);
    if (source != NULL) {
	XtVaSetValues(source, XtNstring, km_file, NULL);
    } else {
	source = XtVaCreateWidget("source", asciiSrcObjectClass, text,
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
    if (km_exists) {
	create_text();
    }
}

/* Popup callback. */
static void
km_up(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    km_isup = true;
}

/* Popdown callback. */
static void
km_down(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    km_isup = false;
}

/* Done button callback.  Pop down the widget. */
static void
km_done(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(km_shell);
}

/* "Sort-by-event" button callback. */
static void
do_sort_event(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (sort != SORT_EVENT) {
	sort = SORT_EVENT;
	XtVaSetValues(sort_byaction, XtNleftBitmap, no_diamond, NULL);
	XtVaSetValues(sort_keymap, XtNleftBitmap, no_diamond, NULL);
	XtVaSetValues(sort_event, XtNleftBitmap, diamond, NULL);
	create_text();
    }
}

/* "Sort-by-keymap" button callback. */
static void
do_sort_keymap(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (sort != SORT_KEYMAP) {
	sort = SORT_KEYMAP;
	XtVaSetValues(sort_byaction, XtNleftBitmap, no_diamond, NULL);
	XtVaSetValues(sort_keymap, XtNleftBitmap, diamond, NULL);
	XtVaSetValues(sort_event, XtNleftBitmap, no_diamond, NULL);
	create_text();
    }
}

/* "Sort-by-action" button callback. */
static void
do_sort_byaction(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (sort != SORT_ACTION) {
	sort = SORT_ACTION;
	XtVaSetValues(sort_byaction, XtNleftBitmap, diamond, NULL);
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
    } *xl_list = NULL, *x, *xs, *xlp, *xn;
    char *km_last;
    int line_last = 0;

    /* Construct the list. */
    for (t = s; t != NULL; t = t_next) {
	char *k, *a, *kk;
	int nq;
	static char cmps[] = ": " PA_KEYMAP_TRACE "(";

	/* Find the end of this rule and terminate this line. */
	t_next = strstr(t, PA_ENDL "\n");
	if (t_next != NULL) {
	    t_next += strlen(PA_ENDL);
	    *t_next++ = '\0';
	}

	/* Remove the leading space. */
	while (*t == ' ') {
	    t++;
	}

	/* Use only traced events. */
	k = strstr(t, cmps);
	if (k == NULL) {
	    continue;
	}
	*k = '\0';
	k += strlen(cmps);

	/* Find the rest of the actions. */
	a = strchr(k, ')');
	if (a == NULL) {
	    continue;
	}
	while (*(++a) == ' ')
	    ;
	if (!*a) {
	    continue;
	}

	/* Remove the trailing PA-End call. */
	if (strlen(a) >= strlen(PA_ENDL) &&
		!strcmp(a + strlen(a) - strlen(PA_ENDL), PA_ENDL)) {
	    a[strlen(a) - strlen(PA_ENDL)] = '\0';
	}

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
	sprintf(x->full_keymap, "%s:%d", x->keymap, x->km_line);

	/* Find where it should be inserted. */
	for (xs = xl_list, xlp = NULL;
	     xs != NULL;
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
	    if (halt) {
		break;
	    }
	}

	/* Insert it. */
	if (xlp != NULL) {
	    x->next = xlp->next;
	    xlp->next = x;
	} else {
	    x->next = xl_list;
	    xl_list = x;
	}
    }

    /* Walk it. */
    if (sort != SORT_KEYMAP) {
	fprintf(f, "%-26s %-16s %s\n%s\n",
		get_message("kmEvent"),
		get_message("kmKeymapLine"),
		get_message("kmActions"),
		DASHES);
    }
    km_last = NULL;
    for (xs = xl_list; xs != NULL; xs = xs->next) {
	switch (sort) {
	case SORT_EVENT:
	    if (km_last != NULL) {
		char *l;

		l = strchr(xs->event, '<');
		if (l != NULL) {
		    if (strcmp(km_last, l)) {
			fprintf(f, "\n");
		    }
		    km_last = l;
		}
	    } else {
		km_last = strchr(xs->event, '<');
	    }
	    break;
	case SORT_KEYMAP:
	    if (km_last == NULL || strcmp(xs->keymap, km_last)) {
		char *p;

		fprintf(f, "%s%s '%s'%s", km_last == NULL ? "" : "\n",
			get_message(is_temp(xs->keymap) ?
				    "kmTemporaryKeymap" :
				    "kmKeymap"),
			xs->keymap,
			from_server(xs->keymap) ?
			    get_message("kmFromServer") : "");
		if ((p = pathname(xs->keymap)) != NULL) {
		    fprintf(f, ", %s %s", get_message("kmFile"), p);
		} else {
		    fprintf(f, ", %s %s.%s.%s", get_message("kmResource"),
			    programname, ResKeymap, xs->keymap);
		}
		fprintf(f, "\n%-26s %-16s %s\n%s\n",
			get_message("kmEvent"),
			get_message("kmKeymapLine"),
			get_message("kmActions"),
			DASHES);
		km_last = xs->keymap;
		line_last = 0;
	    }
	    while (xs->km_line != ++line_last) {
		fprintf(f, "%-26s %s:%d\n", get_message("kmOverridden"),
			xs->keymap, line_last);
	    }
	    break;
	case SORT_ACTION:
	    break;
	}
	fprintf(f, "%-26s %-16s %s\n", xs->event, xs->full_keymap,
		scatv(xs->actions));
    }

    /* Free it. */
    for (xs = xl_list; xs != NULL; xs = xn) {
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
	    (!strncmp(s1, PF, 3) && !strncmp(s2, PF, 3))) {
	return atoi(s1 + 4) - atoi(s2 + 4);
    } else {
	return strcmp(s1, s2);
    }
}

/* Return a keymap's index in the lists. */
static int
km_index(char *n)
{
    struct trans_list *t;
    int ix = 0;

    for (t = trans_list; t != NULL; t = t->next) {
	if (!strcmp(t->name, n)) {
	    return ix;
	}
	ix++;
    }
    for (t = temp_keymaps; t != NULL; t = t->next) {
	if (!strcmp(t->name, n)) {
	    return ix;
	}
	ix++;
    }
    return ix;
}

/* Return whether or not a keymap is temporary. */
static bool
is_temp(char *k)
{
    struct trans_list *t;

    for (t = trans_list; t != NULL; t = t->next) {
	if (!strcmp(t->name, k)) {
	    return t->is_temp;
	}
    }
    for (t = temp_keymaps; t != NULL; t = t->next) {
	if (!strcmp(t->name, k)) {
	    return t->is_temp;
	}
    }
    return false;
}

/* Return the pathname associated with a keymap. */
static char *
pathname(char *k)
{
    struct trans_list *t;

    for (t = trans_list; t != NULL; t = t->next) {
	if (!strcmp(t->name, k)) {
	    return t->pathname;
	}
    }
    for (t = temp_keymaps; t != NULL; t = t->next) {
	if (!strcmp(t->name, k)) {
	    return t->pathname;
	}
    }
    return NULL;
}

/* Return whether or not a keymap was translated from "@server". */
static bool
from_server(char *k)
{
    struct trans_list *t;

    for (t = trans_list; t != NULL; t = t->next) {
	if (!strcmp(t->name, k)) {
	    return t->from_server;
	}
    }
    for (t = temp_keymaps; t != NULL; t = t->next) {
	if (!strcmp(t->name, k)) {
	    return t->from_server;
	}
    }
    return false;
}

/*
 * Comparison function for keymaps.
 */
static int
keymap_cmp(char *k1, int l1, char *k2, int l2)
{
    /* If the keymaps are the same, do a numerical comparison. */
    if (!strcmp(k1, k2)) {
	return l1 - l2;
    }

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
#   define KPL (sizeof(kp) - 1)

    if (strncmp(s1, kp, KPL) || !isdigit((unsigned char)s1[KPL]) ||
	    strncmp(s2, kp, KPL) || !isdigit((unsigned char)s2[KPL])) {
	return strcmp(s1, s2);
    }
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
    if ((l1 = strchr(e1, '<')) == NULL || (l2 = strchr(e2, '<')) == NULL) {
	return strcmp(e1, e2);
    }

    /*
     * If the events are different, sort on the event only.  Otherwise,
     * sort on the modifier(s).
     */
    r = Fnn_strcmp(l1, l2);
    if (r) {
	return r;
    } else {
	return strcmp(e1, e2);
    }
}
