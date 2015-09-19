/*
 * Copyright (c) 1993-2009, 2013-2015 Paul Mattes.
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
 *	toggles.c
 *		This module handles toggles.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include "actions.h"
#include "menubar.h"
#include "popups.h"
#include "toggles.h"
#include "utils.h"

/* Live state of toggles. */
typedef struct {
    bool changed;		/* has the value changed since init */
    bool supported;		/* is the toggle supported */
    unsigned flags;		/* miscellaneous flags */
    toggle_upcall_t *upcall;	/* notify folks it has changed */
} toggle_t;
static toggle_t toggle[N_TOGGLES];

/* Toggle name dictionary. */
toggle_name_t toggle_names[] = {
    { ResMonoCase,        MONOCASE,		false },
    { ResAltCursor,       ALT_CURSOR,		false },
    { ResCursorBlink,     CURSOR_BLINK,		false },
    { ResShowTiming,      SHOW_TIMING,		false },
    { ResCursorPos,       CURSOR_POS,		false },
    { ResTrace,           TRACING,		false },
    { ResDsTrace,         TRACING,		true }, /* compatibility */
    { ResScrollBar,       SCROLL_BAR,		false },
    { ResLineWrap,        LINE_WRAP,		false },
    { ResBlankFill,       BLANK_FILL,		false },
    { ResScreenTrace,     SCREEN_TRACE,		false },
    { ResEventTrace,      TRACING,		true }, /* compatibility */
    { ResMarginedPaste,   MARGINED_PASTE,	false },
    { ResRectangleSelect, RECTANGLE_SELECT,	false },
    { ResCrosshair,	  CROSSHAIR,		false },
    { ResVisibleControl,  VISIBLE_CONTROL,	false },
    { ResAidWait,         AID_WAIT,		false },
    { ResUnderscore,	  UNDERSCORE,		false },
    { ResOverlayPaste,    OVERLAY_PASTE,	false },
    { NULL,               0,			false }
};

/*
 * Generic toggle stuff
 */
static void
do_toggle_reason(toggle_index_t ix, enum toggle_type reason)
{
    toggle_t *t = &toggle[ix];

    /*
     * Change the value, call the internal update routine, and reset the
     * menu label(s).
     */
    toggle_toggle(ix);
    if (t->upcall != NULL) {
	t->upcall(ix, reason);
    }
    menubar_retoggle(ix);
}

void
do_toggle(int ix)
{
    do_toggle_reason(ix, TT_INTERACTIVE);
}

void
do_menu_toggle(int ix)
{
    do_toggle_reason(ix, TT_XMENU);
}

/*
 * Called from system initialization code to handle initial toggle settings.
 */
void
initialize_toggles(void)
{
    toggle_index_t ix;

    for (ix = 0; ix < N_TOGGLES; ix++) {
	if (toggled(ix) && (toggle[ix].flags & TOGGLE_NEED_INIT)) {
	    /* Make the upcall. */
	    toggle[ix].upcall(ix, TT_INITIAL);

	    /* It might have failed. Fix up the menu if it did. */
	    if (!toggled(ix)) {
		menubar_retoggle(ix);
	    }
	}
    }
}

/*
 * Called from system exit code to handle toggles.
 */
void
toggle_exiting(bool mode _is_unused)
{
    toggle_index_t ix;

    for (ix = 0; ix < N_TOGGLES; ix++) {
	if (toggled(ix) && toggle[ix].flags & TOGGLE_NEED_CLEANUP) {
	    set_toggle(ix, false);
	    toggle[ix].upcall(ix, TT_FINAL);
	}
    }
}

bool
Toggle_action(ia_t ia, unsigned argc, const char **argv)
{
    int j;
    int ix;

    action_debug("Toggle", ia, argc, argv);
    if (check_argc("Toggle", argc, 1, 2) < 0) {
	return false;
    }
    for (j = 0; toggle_names[j].name != NULL; j++) {
	if (!toggle_supported(toggle_names[j].index)) {
	    continue;
	}
	if (!strcasecmp(argv[0], toggle_names[j].name)) {
	    ix = toggle_names[j].index;
	    break;
	}
    }
    if (toggle_names[j].name == NULL) {
	popup_an_error("Toggle: Unknown toggle name '%s'", argv[0]);
	return false;
    }

    if (argc == 1) {
	do_toggle_reason(ix, TT_ACTION);
    } else if (!strcasecmp(argv[1], "set")) {
	if (!toggled(ix)) {
	    do_toggle_reason(ix, TT_ACTION);
	}
    } else if (!strcasecmp(argv[1], "clear")) {
	if (toggled(ix)) {
	    do_toggle_reason(ix, TT_ACTION);
	}
    } else {
	popup_an_error("Toggle: Unknown keyword '%s' (must be 'set' or "
		"'clear')", argv[1]);
	return false;
    }
    return true;
}

/**
 * Toggles module registration.
 */
void
toggles_register(void)
{
    static action_table_t toggle_actions[] = {
	{ "Toggle",		Toggle_action,	ACTION_KE }
    };

    /* Register the cleanup routine. */
    register_schange(ST_EXITING, toggle_exiting);

    /* Register the actions. */
    register_actions(toggle_actions, array_count(toggle_actions));
}

/**
 * Flip the value of a toggle without notifying anyone.
 *
 * @param ix	Toggle index
 */
void
toggle_toggle(toggle_index_t ix)
{
    set_toggle(ix, !toggled(ix));
}

/**
 * Set the value of a toggle, without notifying anyone.
 *
 * @param ix	Toggle index
 */
void
set_toggle(toggle_index_t ix, bool value)
{
    appres.toggle[ix] = value;
    toggle[ix].changed = true;
}

/**
 * Set the initial value of a toggle, which does not include marking it
 * changed or notifying anyone.
 *
 * @param ix	Toggle index
 */
void
set_toggle_initial(toggle_index_t ix, bool value)
{
    appres.toggle[ix] = value;
}

/**
 * Return current state of a toggle.
 *
 * @param ix	Toggle index
 *
 * @return Toggle state
 */
bool
toggled(toggle_index_t ix)
{
    return appres.toggle[ix];
}

/**
 * Return change status of a toggle.
 *
 * @param ix	Toggle index
 *
 * @return true if changed, false otherwise
 */
bool
toggle_changed(toggle_index_t ix)
{
    return toggle[ix].changed;
}

/**
 * Check for a toggle being supported in this app.
 *
 * @param[in] ix	Toggle index
 *
 * @return true if supported, false otherwise.
 */
bool
toggle_supported(toggle_index_t ix)
{
    return toggle[ix].supported;
}

/**
 * Register a group of toggle callbacks.
 *
 * @param[in] toggles	Array of callbacks to register
 * @param[in] count	Number of elements in toggles[]
 */
void
register_toggles(toggle_register_t toggles[], unsigned count)
{
    unsigned i;

    for (i = 0; i < count; i++) {
	toggle[toggles[i].ix].supported = true;
	toggle[toggles[i].ix].upcall = toggles[i].upcall;
	toggle[toggles[i].ix].flags = toggles[i].flags;
    }
}
