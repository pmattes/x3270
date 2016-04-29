/*
 * Copyright (c) 1993-2016 Paul Mattes.
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
#include "lazya.h"
#include "macros.h"
#include "popups.h"
#include "resources.h"
#include "trace.h"
#include "utils.h"

llist_t actions_list = LLIST_INIT(actions_list);
unsigned actions_list_count;

enum iaction ia_cause;
const char *ia_name[] = {
    "String", "Paste", "Screen redraw", "Keypad", "Default", "Key",
    "Macro", "Script", "Peek", "Typeahead", "File transfer", "Command",
    "Keymap", "Idle"
};

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
    a = lazya(NewString(actions));
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
	llist_insert_before(&s->list, &suppressed);
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
    if (nargs >= nargs_min && nargs <= nargs_max) {
	return 0;
    }
    if (nargs_min == nargs_max) {
	popup_an_error("%s requires %d argument%s",
		aname, nargs_min, nargs_min == 1 ? "" : "s");
    } else {
	popup_an_error("%s requires %d or %d arguments",
		aname, nargs_min, nargs_max);
    }
    cancel_if_idle_command();
    return -1;
}

/*
 * Trace the execution of an emulator action.
 */
void
action_debug(const char *aname, ia_t ia, unsigned argc, const char **argv)
{
    unsigned i;
    char pbuf[1024];

    if (!toggled(TRACING)) {
	return;
    }
    vtrace("%s -> %s(", ia_name[(int)ia], aname);
    for (i = 0; i < argc; i++) {
	vtrace("%s\"%s\"",
		i? ", ": "",
		scatv(argv[i], pbuf, sizeof(pbuf)));
    }
    vtrace(")\n");

    trace_rollover_check();
}

/*
 * Run an emulator action by name, given 0, 1 or 2 parameters.
 */
bool
run_action(const char *name, enum iaction cause, const char *parm1,
	const char *parm2)
{
    action_elt_t *e;
    action_t *action = NULL;
    unsigned count = 0;
    const char *parms[2];

    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (!strcasecmp(e->t.name, name)) {
	    action = e->t.action;
	    break;
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    if (action == NULL) {
	return false; /* XXX: And do something? */
    }

    if (parm1 != NULL) {
	parms[0] = parm1;
	count++;
	if (parm2 != NULL) {
	    parms[1] = parm2;
	    count++;
	}
    }

    return run_action_entry(e, cause, count, parms);
}

/*
 * Run an emulator action by name, given an array of parameters.
 */
bool
run_action_a(const char *name, enum iaction cause, unsigned count,
	const char **parms)
{
    action_elt_t *e;
    action_t *action = NULL;

    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (!strcasecmp(e->t.name, name)) {
	    action = e->t.action;
	    break;
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    if (action == NULL) {
	return false; /* XXX: And do something? */
    }

    return run_action_entry(e, cause, count, parms);
}

/*
 * Run an action by entry.
 * This is where action suppression happens.
 */
bool
run_action_entry(action_elt_t *e, enum iaction cause, unsigned count,
	const char **parms)
{
    if (action_suppressed(e->t.name)) {
	vtrace("%s() [suppressed]\n", e->t.name);
	return false;
    }

    ia_cause = cause;
    return (*e->t.action)(cause, count, parms);
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
		/* Insert before head (at the end of the list). */
	    llist_insert_before(&e->list, actions_list.next);
	}

	actions_list_count++;
    }
}
