/*
 * Modifications Copyright 1993-2008 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	toggles.c
 *		This module handles toggles.
 */

#include "globals.h"
#include "appres.h"

#include "ansic.h"
#include "actionsc.h"
#include "ctlrc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "screenc.h"
#include "trace_dsc.h"
#include "togglesc.h"


/*
 * Generic toggle stuff
 */
static void
do_toggle_reason(int ix, enum toggle_type reason)
{
	struct toggle *t = &appres.toggle[ix];

	/*
	 * Change the value, call the internal update routine, and reset the
	 * menu label(s).
	 */
	toggle_toggle(t);
	t->upcall(t, reason);
#if defined(X3270_MENUS) /*[*/
	menubar_retoggle(t);
#endif /*]*/
}

void
do_toggle(int ix)
{
	do_toggle_reason(ix, TT_INTERACTIVE);
}

/*
 * Called from system initialization code to handle initial toggle settings.
 */
void
initialize_toggles(void)
{
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
	appres.toggle[MONOCASE].upcall =         toggle_monocase;
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	appres.toggle[ALT_CURSOR].upcall =       toggle_altCursor;
	appres.toggle[CURSOR_BLINK].upcall =     toggle_cursorBlink;
	appres.toggle[SHOW_TIMING].upcall =      toggle_showTiming;
	appres.toggle[CURSOR_POS].upcall =       toggle_cursorPos;
	appres.toggle[MARGINED_PASTE].upcall =   toggle_nop;
	appres.toggle[RECTANGLE_SELECT].upcall = toggle_nop;
	appres.toggle[SCROLL_BAR].upcall =       toggle_scrollBar;
	appres.toggle[CROSSHAIR].upcall =        toggle_crosshair;
	appres.toggle[VISIBLE_CONTROL].upcall =  toggle_visible_control;
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
	appres.toggle[DS_TRACE].upcall =         toggle_dsTrace;
	appres.toggle[SCREEN_TRACE].upcall =     toggle_screenTrace;
	appres.toggle[EVENT_TRACE].upcall =      toggle_eventTrace;
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
	appres.toggle[LINE_WRAP].upcall =        toggle_lineWrap;
#endif /*]*/
	appres.toggle[BLANK_FILL].upcall =       toggle_nop;
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
	appres.toggle[AID_WAIT].upcall =         toggle_nop;
#endif /*]*/
#if defined(C3270) /*[*/
	appres.toggle[UNDERSCORE].upcall =	 toggle_underscore;
#endif /*]*/

#if defined(X3270_TRACE) /*[*/
	if (toggled(DS_TRACE))
		appres.toggle[DS_TRACE].upcall(&appres.toggle[DS_TRACE],
		    TT_INITIAL);
	if (toggled(EVENT_TRACE))
		appres.toggle[EVENT_TRACE].upcall(&appres.toggle[EVENT_TRACE],
		    TT_INITIAL);
	if (toggled(SCREEN_TRACE))
		appres.toggle[SCREEN_TRACE].upcall(&appres.toggle[SCREEN_TRACE],
		    TT_INITIAL);
#endif /*]*/
}

/*
 * Called from system exit code to handle toggles.
 */
void
shutdown_toggles(void)
{
#if defined(X3270_TRACE) /*[*/
	/* Clean up the data stream trace monitor window. */
	if (toggled(DS_TRACE)) {
		appres.toggle[DS_TRACE].value = False;
		toggle_dsTrace(&appres.toggle[DS_TRACE], TT_FINAL);
	}
	if (toggled(EVENT_TRACE)) {
		appres.toggle[EVENT_TRACE].value = False;
		toggle_dsTrace(&appres.toggle[EVENT_TRACE], TT_FINAL);
	}

	/* Clean up the screen trace file. */
	if (toggled(SCREEN_TRACE)) {
		appres.toggle[SCREEN_TRACE].value = False;
		toggle_screenTrace(&appres.toggle[SCREEN_TRACE], TT_FINAL);
	}
#endif /*]*/
}

void
Toggle_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	int j;

	action_debug(Toggle_action, event, params, num_params);
	if (check_usage(Toggle_action, *num_params, 1, 2) < 0)
		return;
	for (j = 0; j < N_TOGGLES; j++)
		if (toggle_names[j].index >= 0 &&
		    !strcasecmp(params[0], toggle_names[j].name)) {
			break;
		}
	if (j >= N_TOGGLES) {
		popup_an_error("%s: Unknown toggle name '%s'",
		    action_name(Toggle_action), params[0]);
		return;
	}

	if (*num_params == 1) {
		do_toggle_reason(j, TT_ACTION);
	} else if (!strcasecmp(params[1], "set")) {
		if (!toggled(j)) {
			do_toggle_reason(j, TT_ACTION);
		}
	} else if (!strcasecmp(params[1], "clear")) {
		if (toggled(j)) {
			do_toggle_reason(j, TT_ACTION);
		}
	} else {
		popup_an_error("%s: Unknown keyword '%s' (must be 'set' or "
		    "'clear')", action_name(Toggle_action), params[1]);
	}
}
