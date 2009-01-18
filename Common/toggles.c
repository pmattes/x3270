/*
 * Copyright (c) 1993-2009, Paul Mattes.
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
