/*
 * Copyright (c) 1993-2014, Paul Mattes.
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

#include "actionsc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "printc.h"
#if defined(X3270_DISPLAY) /*[*/
# include "print_windowc.h"
#endif /*]*/
#include "resources.h"
#include "scrollc.h"
#include "selectc.h"
#include "togglesc.h"
#include "trace.h"
#include "utilc.h"
#include "xioc.h"

#include "unicodec.h"
#include "ftc.h"
#if defined(X3270_INTERACTIVE) /*[*/
#include "keypadc.h"
#include "menubarc.h"
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
#include "screenc.h"
#endif /*]*/

enum iaction ia_cause;
const char *ia_name[] = {
    "String", "Paste", "Screen redraw", "Keypad", "Default", "Key",
    "Macro", "Script", "Peek", "Typeahead", "File transfer", "Command",
    "Keymap", "Idle"
};

/* The emulator action table. */
action_table_t all_actions[] = {
#if defined(C3270) /*[*/
    { "Abort",		Abort_action, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "AltCursor",  	AltCursor_action, NULL },
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
    { "Compose",	Compose_action, NULL },
#endif /*]*/
#if defined(WC3270) /*[*/
    { "Copy",		Copy_action, NULL },
#endif /*]*/
#if defined(WC3270) /*[*/
    { "Cut",		Cut_action, NULL },
#endif /*]*/
    { "HexString",	HexString_action, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Info",		Info_action, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "Keymap",		TemporaryKeymap_action, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Escape",		Escape_action, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "PrintWindow",	PrintWindow_action, NULL },
#endif /*]*/
    { "PrintText",	PrintText_action, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Flip",		Flip_action, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Redraw",		Redraw_action, NULL },
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
    { "Scroll",		Scroll_action, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "SetFont",	SetFont_action, NULL },
    { "TemporaryKeymap",TemporaryKeymap_action, NULL },
#endif /*]*/
#if !defined(TCL3270) /*[*/
    { "AnsiText",	AnsiText_action, NULL },
#endif /*]*/
    { "Ascii",		Ascii_action, NULL },
    { "AsciiField",	AsciiField_action, NULL },
    { "Attn",		Attn_action, NULL },
    { "BackSpace",	BackSpace_action, NULL },
    { "BackTab",	BackTab_action, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Bell",		Bell_action, NULL },
#endif /*]*/
    { "CircumNot",	CircumNot_action, NULL },
    { "Clear",		Clear_action, NULL },
    { "Close",		Disconnect_action, NULL },
#if !defined(TCL3270) /*[*/
    { "CloseScript",	CloseScript_action, NULL },
#endif /*]*/
    { "Connect",	Connect_action, NULL },
#if !defined(TCL3270) /*[*/
    { "ContinueScript",	ContinueScript_action, NULL },
#endif /*]*/
    { "CursorSelect",	CursorSelect_action, NULL },
    { "Delete", 	Delete_action, NULL },
    { "DeleteField",	DeleteField_action, NULL },
    { "DeleteWord",	DeleteWord_action, NULL },
    { "Disconnect",	Disconnect_action, NULL },
    { "Down",		Down_action, NULL },
    { "Dup",		Dup_action, NULL },
    { "Ebcdic",		Ebcdic_action, NULL },
    { "EbcdicField",	EbcdicField_action, NULL },
    { "Enter",		Enter_action, NULL },
    { "Erase",		Erase_action, NULL },
    { "EraseEOF",	EraseEOF_action, NULL },
    { "EraseInput",	EraseInput_action, NULL },
#if !defined(TCL3270) /*[*/
    { "Execute",	Execute_action, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Exit",		Quit_action, NULL },
#endif /*]*/
#if !defined(TCL3270) /*[*/
    { "Expect",		Expect_action, NULL },
#endif /*]*/
    { "FieldEnd",	FieldEnd_action, NULL },
    { "FieldMark",	FieldMark_action, NULL },
    { "HexString",	HexString_action},
#if defined(C3270) /*[*/
    { "Help",		Help_action},
#endif/*]*/
    { "Home",		Home_action, NULL },
#if defined(C3270) /*[*/
    { "ignore",		ignore_action, NULL },
#endif /*]*/
    { "Insert",		Insert_action, NULL },
    { "Interrupt",	Interrupt_action, NULL },
    { "Key",		Key_action, NULL },
#if defined(C3270) /*[*/
    { "Keypad",		Keypad_action, NULL },
#endif /*]*/
    { "Left",		Left_action, NULL },
    { "Left2", 		Left2_action, NULL },
#if !defined(TCL3270) /*[*/
    { "Macro", 		Macro_action, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Menu",		Menu_action, NULL },
#endif /*]*/
    { "MonoCase",	MonoCase_action, NULL },
    { "MoveCursor",	MoveCursor_action, NULL },
    { "Newline",	Newline_action, NULL },
    { "NextWord",	NextWord_action, NULL },
    { "Open",		Connect_action, NULL },
    { "PA",		PA_action, NULL },
    { "PF",		PF_action, NULL },
#if defined(WC3270) /*[*/
    { "Paste",		Paste_action, NULL },
#endif /*]*/
#if !defined(TCL3270) /*[*/
    { "PauseScript",	PauseScript_action, NULL },
#endif /*]*/
    { "PreviousWord",	PreviousWord_action, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Printer",	Printer_action, NULL },
#endif /*]*/
    { "Query",		Query_action, NULL },
    { "Quit",		Quit_action, NULL },
    { "ReadBuffer",	ReadBuffer_action, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Reconnect",	Reconnect_action, NULL },
#endif /*]*/
    { "Reset",		Reset_action, NULL },
    { "Right",		Right_action, NULL },
    { "Right2",		Right2_action, NULL },
#if defined(C3270) /*[*/
    { "ScreenTrace",	ScreenTrace_action, NULL },
#endif/*]*/
#if !defined(TCL3270) /*[*/
    { "Script",		Script_action, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Show",		Show_action, NULL },
#endif/*]*/
    { "Snap",		Snap_action, NULL },
#if !defined(TCL3270) /*[*/
    { "Source",		Source_action, NULL },
#endif /*]*/
#if defined(TCL3270) /*[*/
    { "Status",		Status_action, NULL },
#endif /*]*/
    { "String",		String_action, NULL },
    { "SysReq",		SysReq_action, NULL },
    { "Tab",		Tab_action, NULL },
#if defined(X3270_DISPLAY) || defined(WC3270) /*[*/
    { "Title",		Title_action, NULL },
#endif /*]*/
    { "Toggle",		Toggle_action, NULL },
    { "ToggleInsert",	ToggleInsert_action, NULL },
    { "ToggleReverse",	ToggleReverse_action, NULL },
#if defined(C3270) /*[*/
    { "Trace",		Trace_action, NULL },
#endif/*]*/
    { "Transfer",	Transfer_action, NULL },
    { "Up",		Up_action, NULL },
    { "Wait",		Wait_action, NULL },
#if defined(X3270_DISPLAY) /*[*/
    { "WindowState",	WindowState_action, NULL },
#endif /*]*/
};
action_table_t *action_table = all_actions;
unsigned num_actions = sizeof(all_actions) / sizeof(all_actions[0]);

/* Look up an action name in the suppressed actions resource. */
Boolean
action_suppressed(const char *name, const char *suppress)
{
    const char *s = suppress;
    char *t;

    while ((t = strstr(s, name)) != NULL) {
	char b;
	char e = t[strlen(name)];

	if (t == suppress) {
	    b = '\0';
	} else {
	    b = *(t - 1);
	}
	if ((b == '\0' || b == ')' || isspace(b)) &&
	    (e == '\0' || e == '(' || isspace(e))) {
	    return True;
	}
	s = t + strlen(name);
    }
    return False;
}

/*
 * Action table initialization.
 * Uses the suppressActions resource to prune the actions table.
 */
void
action_init(void)
{
#if 0
	char *suppress;
	int i;

	/* See if there are any filters at all. */
	suppress = get_resource(ResSuppressActions);
	if (suppress == NULL) {
		actions = all_actions;
		return;
	}

	/* Yes, we'll need to copy the table and prune it. */
	actions = (XtActionsRec *)Malloc(sizeof(all_actions));
	memcpy(actions, all_actions, sizeof(all_actions));
	for (i = 0; i < actioncount; i++) {
		if (action_suppressed(actions[i].string, suppress))
			actions[i].proc = suppressed_action;
	}
#endif
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
 * Run an emulator action.
 */
void
run_action(const char *name, enum iaction cause, const char *parm1,
	const char *parm2)
{
    unsigned i;
    action_t *action = NULL;
    unsigned count = 0;
    const char *parms[2];

    for (i = 0; i < num_actions; i++) {
	if (!strcasecmp(action_table[i].name, name)) {
	    action = action_table[i].action;
	    break;
	}
    }
    if (action == NULL) {
	return; /* XXX: And do something? */
    }

    if (parm1 != NULL) {
	parms[0] = parm1;
	count++;
	if (parm2 != NULL) {
	    parms[1] = parm2;
	    count++;
	}
    }

    ia_cause = cause;
    (void)(*action)(cause, count, parms);
}
