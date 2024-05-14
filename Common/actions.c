/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	actions.c
 *		The actions table and action debugging code.
 */

#include "globals.h"
#include "appres.h"

#include "actions.h"
#include "popups.h"
#include "resources.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "varbuf.h"
#include "vstatus.h"

llist_t actions_list = LLIST_INIT(actions_list);
unsigned actions_list_count;

enum iaction ia_cause;
const char *ia_name[] = {
    "none", "string", "paste", "screen-redraw", "keypad", "default", "macro",
    "script", "peek", "typeahead", "file-transfer", "command", "keymap",
    "idle", "password", "ui", "httpd"
};
static int keyboard_implicit_disables = 0;
static int keyboard_explicit_disables = 0;

const char *current_action_name;

typedef struct {
    llist_t list;
    char *name;
} suppress_t;
static llist_t suppressed = LLIST_INIT(suppressed);
static bool suppressed_initted = false;

/* Initialize the list of suppressed actions. */
static void
init_suppressed(const char *actions)
{
    char *a;
    char *action;
    suppress_t *s;

    if (actions == NULL) {
	return;
    }
    a = txdFree(NewString(actions));
    while ((action = strtok(a, " \t\r\n")) != NULL) {
	size_t sl = strlen(action);
	action_elt_t *e;
	bool found = false;

	/* Prime for the next strtok() call. */
	a = NULL;

	/* Chop off any trailing parentheses. */
	if (sl > 2 && !strcmp(action + sl - 2, "()")) {
	    sl -= 2;
	    *(action + sl) = '\0';
	}

	/* Make sure the action they are suppressing is real. */
	FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	    if (!strcasecmp(e->t.name, action)) {
		found = true;
		break;
	    }
	} FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
	if (!found) {
	    vtrace("Warning: action '%s' in %s not found\n", action,
		    ResSuppressActions);
	    continue;
	}

	/* Add it to the list. */
	s = (suppress_t *)Malloc(sizeof(suppress_t) + sl + 1);
	s->name = (char *)(s + 1);
	strcpy(s->name, action);
	llist_init(&s->list);
	LLIST_APPEND(&s->list, suppressed);
    }
}

/* Look up an action name in the suppressed actions resource. */
static bool
action_suppressed(const char *name)
{
    suppress_t *s;

    if (!suppressed_initted) {
	init_suppressed(appres.suppress_actions);
	suppressed_initted = true;
    }
    if (llist_isempty(&suppressed)) {
	return false;
    }

    FOREACH_LLIST(&suppressed, s, suppress_t *) {
	if (!strcasecmp(name, s->name)) {
	    return true;
	}
    } FOREACH_LLIST_END(&suppressed, s, suppress_t *);

    return false;
}

/*
 * Check the number of argument to an action, and possibly pop up a usage
 * message.
 *
 * Returns 0 if the argument count is correct, -1 otherwise.
 */
int
check_argc(const char *aname, unsigned nargs, unsigned nargs_min,
	unsigned nargs_max)
{
    bool has_paren = strchr(aname, '(') != NULL;

    if (nargs >= nargs_min && nargs <= nargs_max) {
	return 0;
    }
    if (nargs_min == nargs_max) {
	popup_an_error("%s%s requires %d argument%s",
		aname, has_paren? "": "()",
		nargs_min, nargs_min == 1 ? "" : "s");
    } else if (nargs_max == nargs_min + 1) {
	popup_an_error("%s%s requires %d or %d arguments",
		aname, has_paren? "": "()",
		nargs_min, nargs_max);
    } else {
	popup_an_error("%s%s requires %d to %d arguments",
		aname, has_paren? "": "()",
		nargs_min, nargs_max);
    }
    return -1;
}

/*
 * Trace the execution of an emulator action.
 */
void
action_debug(const char *aname, ia_t ia, unsigned argc, const char **argv)
{
    unsigned i;

    if (!toggled(TRACING)) {
	return;
    }
    vtrace("%s -> %s(", ia_name[(int)ia], aname);
    for (i = 0; i < argc; i++) {
	vtrace("%s%s", i? ", ": "", qscatv(argv[i]));
    }
    vtrace(")\n");

    trace_rollover_check();
}

/*
 * Display an error message about parameter names.
 */
bool
action_args_are(const char *aname, ...)
{
    va_list ap;
    const char *keyword;
    const char **keywords = NULL;
    int nkw = 0;
    int i;
    varbuf_t r;
    char *buf;

    va_start(ap, aname);
    while ((keyword = va_arg(ap, const char *)) != NULL) {
	keywords = (const char **)Realloc((void *)keywords, (nkw + 1) * sizeof(char *));
	keywords[nkw++] = keyword;
    }
    if (nkw == 0) {
	return false;
    }

    vb_init(&r);
    for (i = 0; i < nkw; i++) {
	const char *sep = "";

	if (nkw > 1) {
	    if (i == nkw - 1) {
		sep = " or ";
	    } else if (i > 0) {
		sep = ", ";
	    }
	}
	vb_appendf(&r, "%s%s", sep, keywords[i]);
    }
    buf = vb_consume(&r);
    popup_an_error("%s(): Parameter must be %s", aname, buf);
    Free(buf);
    Free((void *)keywords);
    return false;
}

/*
 * Disable or re-enable the keyboard.
 */
void
disable_keyboard(bool disable, bool explicit, const char *why)
{
    bool disabled_before, disabled_after, would_enable;
    int *countp = explicit?
	&keyboard_explicit_disables:
	&keyboard_implicit_disables;
    int incr = disable? 1 : -1;

    if (*countp + incr < 0) {
	vtrace("Redundant %splicit keyboard enable ignored\n",
		explicit? "ex": "im");
	return;
    }

    vtrace("Keyboard %sabled %splicitly by %s (%d->%d)",
	    disable? "dis": "en",
	    explicit? "ex": "im",
	    why,
	    *countp,
	    *countp + incr);

    disabled_before = keyboard_explicit_disables || keyboard_implicit_disables;
    *countp += incr;
    disabled_after = keyboard_explicit_disables || keyboard_implicit_disables;
    would_enable = (*countp == 0);

    vtrace(", %s %sabled",
	  (disabled_before == disabled_after)? "still": "now",
	  disabled_after? "dis": "en");
    if (would_enable && disabled_after) {
	vtrace(" %splicitly", explicit? "im": "ex");
    }
    vtrace("\n");

    st_changed(ST_KBD_DISABLE, keyboard_disabled());
}

/*
 * Force a keyboard enable (both explicit and implicit).
 */
void
force_enable_keyboard(void)
{
    vtrace("Forcing keyboard enable\n");
    keyboard_implicit_disables = 0;
    keyboard_explicit_disables = 0;
    st_changed(ST_KBD_DISABLE, keyboard_disabled());
}

/*
 * Test for keyboard disable.
 */
bool
keyboard_disabled(void)
{
    return keyboard_implicit_disables || keyboard_explicit_disables;
}

/*
 * Run an action by entry.
 * This is where action suppression happens.
 */
bool
run_action_entry(action_elt_t *e, enum iaction cause, unsigned count,
	const char **parms)
{
    bool ret;

    if (action_suppressed(e->t.name)) {
	vtrace("%s() [suppressed]\n", e->t.name);
	return false;
    }

    if ((keyboard_explicit_disables || keyboard_implicit_disables) &&
	    IA_IS_KEY(cause)) {
	vtrace("%s() [suppressed, keyboard disabled]\n", e->t.name);
	vstatus_keyboard_disable_flash();
	return false;
    }

    ia_cause = cause;
    current_action_name = e->t.name;
    ret = (*e->t.action)(cause, count, parms);
    current_action_name = NULL;
    return ret;
}

/*
 * Register a group of actions.
 *
 * Duplicate actions will override older ones.
 * The memory for the table is not kept, but the memory for the strings
 * referenced by the table will be re-used.
 */
void
register_actions(action_table_t *new_actions, unsigned count)
{
    unsigned i;

    for (i = 0; i < count; i++) {
	action_elt_t *e;
	action_elt_t *before;

	before = NULL;
	FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	    int cmp;

	    cmp = strcasecmp(e->t.name, new_actions[i].name);
	    if (cmp == 0) {
		/* Replace. */
		e->t = new_actions[i]; /* struct copy */
		return;
	    } else if (cmp < 0) {
		/* Goes ahead of this one. */
		before = e;
		break;
	    }
	} FOREACH_LLIST_END(&actions_list, e, action_elt_t *);

	e = Malloc(sizeof(action_elt_t));
	e->t = new_actions[i]; /* struct copy */
	llist_init(&e->list);

	if (before) {
	    /* Insert before found element. */
	    llist_insert_before(&e->list, &before->list);
	} else {
	    /* Append. */
	    LLIST_APPEND(&e->list, actions_list);
	}

	actions_list_count++;
    }
}

/**
 * Compare two action names.
 *
 * @param[in] a		First action name.
 * @param[in] b		Second action name.
 *
 * @returns 0 if equal, -1 if a < b, 1 if a > b
 */
static int
action_cmp(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/**
 * Return the names of all defined actions.
 *
 * @returns Names of all actions.
 */
const char *
all_actions(void)
{
    static const char *actions = NULL;
    action_elt_t *e;
    const char **names;
    unsigned i;
    varbuf_t r;

    if (actions != NULL) {
	return actions;
    }

    /* Gather the names. */
    names = Calloc(actions_list_count, sizeof(const char *));
    i = 0;
    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	names[i++] = e->t.name;
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);

    /* Sort them. */
    qsort((void *)names, actions_list_count, sizeof(const char *), action_cmp);

    /* Emit them. */
    vb_init(&r);
    for (i = 0; i < actions_list_count; i++) {
	vb_appendf(&r, "%s%s()", i? " ": "", names[i]);
    }
    actions = vb_consume(&r);
    vb_free(&r);

    /* Done. */
    Free((void *)names);
    return actions;
}
