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
#include "trace_dsc.h"
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
eaction_table_t all_eactions[] = {
#if defined(C3270) /*[*/
    { "Abort",		Abort_eaction, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "AltCursor",  	AltCursor_eaction, NULL },
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
    { "Compose",	Compose_eaction, NULL },
#endif /*]*/
#if defined(WC3270) /*[*/
    { "Copy",		Copy_eaction, NULL },
#endif /*]*/
#if defined(WC3270) /*[*/
    { "Cut",		Cut_eaction, NULL },
#endif /*]*/
    { "HexString",	HexString_eaction, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Info",		Info_eaction, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "Keymap",		TemporaryKeymap_eaction, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Escape",		Escape_eaction, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "PrintWindow",	PrintWindow_eaction, NULL },
#endif /*]*/
    { "PrintText",	PrintText_eaction, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Flip",		Flip_eaction, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Redraw",		Redraw_eaction, NULL },
#endif /*]*/
#if defined(X3270_INTERACTIVE) /*[*/
    { "Scroll",		Scroll_eaction, NULL },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
    { "SetFont",	SetFont_eaction, NULL },
    { "TemporaryKeymap",TemporaryKeymap_eaction, NULL },
#endif /*]*/
#if !defined(TCL3270) /*[*/
    { "AnsiText",	AnsiText_eaction, NULL },
#endif /*]*/
    { "Ascii",		Ascii_eaction, NULL },
    { "AsciiField",	AsciiField_eaction, NULL },
    { "Attn",		Attn_eaction, NULL },
    { "BackSpace",	BackSpace_eaction, NULL },
    { "BackTab",	BackTab_eaction, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Bell",		Bell_eaction, NULL },
#endif /*]*/
    { "CircumNot",	CircumNot_eaction, NULL },
    { "Clear",		Clear_eaction, NULL },
    { "Close",		Disconnect_eaction, NULL },
#if !defined(TCL3270) /*[*/
    { "CloseScript",	CloseScript_eaction, NULL },
#endif /*]*/
    { "Connect",	Connect_eaction, NULL },
#if !defined(TCL3270) /*[*/
    { "ContinueScript",	ContinueScript_eaction, NULL },
#endif /*]*/
    { "CursorSelect",	CursorSelect_eaction, NULL },
    { "Delete", 	Delete_eaction, NULL },
    { "DeleteField",	DeleteField_eaction, NULL },
    { "DeleteWord",	DeleteWord_eaction, NULL },
    { "Disconnect",	Disconnect_eaction, NULL },
    { "Down",		Down_eaction, NULL },
    { "Dup",		Dup_eaction, NULL },
    { "Ebcdic",		Ebcdic_eaction, NULL },
    { "EbcdicField",	EbcdicField_eaction, NULL },
    { "Enter",		Enter_eaction, NULL },
    { "Erase",		Erase_eaction, NULL },
    { "EraseEOF",	EraseEOF_eaction, NULL },
    { "EraseInput",	EraseInput_eaction, NULL },
#if !defined(TCL3270) /*[*/
    { "Execute",	Execute_eaction, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Exit",		Quit_eaction, NULL },
#endif /*]*/
#if !defined(TCL3270) /*[*/
    { "Expect",		Expect_eaction, NULL },
#endif /*]*/
    { "FieldEnd",	FieldEnd_eaction, NULL },
    { "FieldMark",	FieldMark_eaction, NULL },
    { "HexString",	HexString_eaction},
#if defined(C3270) /*[*/
    { "Help",		Help_eaction},
#endif/*]*/
    { "Home",		Home_eaction, NULL },
    { "Insert",		Insert_eaction, NULL },
    { "Interrupt",	Interrupt_eaction, NULL },
    { "Key",		Key_eaction, NULL },
#if defined(C3270) /*[*/
    { "Keypad",		Keypad_eaction, NULL },
#endif /*]*/
    { "Left",		Left_eaction, NULL },
    { "Left2", 		Left2_eaction, NULL },
#if !defined(TCL3270) /*[*/
    { "Macro", 		Macro_eaction, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Menu",		Menu_eaction, NULL },
#endif /*]*/
    { "MonoCase",	MonoCase_eaction, NULL },
    { "MoveCursor",	MoveCursor_eaction, NULL },
    { "Newline",	Newline_eaction, NULL },
    { "NextWord",	NextWord_eaction, NULL },
    { "Open",		Connect_eaction, NULL },
    { "PA",		PA_eaction, NULL },
    { "PF",		PF_eaction, NULL },
#if defined(WC3270) /*[*/
    { "Paste",		Paste_eaction, NULL },
#endif /*]*/
#if !defined(TCL3270) /*[*/
    { "PauseScript",	PauseScript_eaction, NULL },
#endif /*]*/
    { "PreviousWord",	PreviousWord_eaction, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Printer",	Printer_eaction, NULL },
#endif /*]*/
    { "Query",		Query_eaction, NULL },
    { "Quit",		Quit_eaction, NULL },
    { "ReadBuffer",	ReadBuffer_eaction, NULL },
#if defined(X3270_INTERACTIVE) /*[*/
    { "Reconnect",	Reconnect_eaction, NULL },
#endif /*]*/
    { "Reset",		Reset_eaction, NULL },
    { "Right",		Right_eaction, NULL },
    { "Right2",		Right2_eaction, NULL },
#if defined(C3270) /*[*/
    { "ScreenTrace",	ScreenTrace_eaction, NULL },
#endif/*]*/
#if !defined(TCL3270) /*[*/
    { "Script",		Script_eaction, NULL },
#endif /*]*/
#if defined(C3270) /*[*/
    { "Show",		Show_eaction, NULL },
#endif/*]*/
    { "Snap",		Snap_eaction, NULL },
#if !defined(TCL3270) /*[*/
    { "Source",		Source_eaction, NULL },
#endif /*]*/
#if defined(TCL3270) /*[*/
    { "Status",		Status_eaction, NULL },
#endif /*]*/
    { "String",		String_eaction, NULL },
    { "SysReq",		SysReq_eaction, NULL },
    { "Tab",		Tab_eaction, NULL },
#if defined(X3270_DISPLAY) || defined(WC3270) /*[*/
    { "Title",		Title_eaction, NULL },
#endif /*]*/
    { "Toggle",		Toggle_eaction, NULL },
    { "ToggleInsert",	ToggleInsert_eaction, NULL },
    { "ToggleReverse",	ToggleReverse_eaction, NULL },
#if defined(C3270) /*[*/
    { "Trace",		Trace_eaction, NULL },
#endif/*]*/
    { "Transfer",	Transfer_eaction, NULL },
    { "Up",		Up_eaction, NULL },
    { "Wait",		Wait_eaction, NULL },
#if defined(X3270_DISPLAY) /*[*/
    { "WindowState",	WindowState_eaction, NULL },
#endif /*]*/
};
eaction_table_t *eaction_table = all_eactions;
unsigned num_eactions = sizeof(all_eactions) / sizeof(all_eactions[0]);

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
check_eusage(const char *aname, unsigned nargs, unsigned nargs_min,
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
eaction_debug(const char *aname, ia_t ia, unsigned argc, const char **argv)
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
run_eaction(const char *name, enum iaction cause, const char *parm1,
	const char *parm2)
{
    unsigned i;
    eaction_t *eaction = NULL;
    unsigned count = 0;
    const char *parms[2];

    for (i = 0; i < num_eactions; i++) {
	if (!strcasecmp(eaction_table[i].name, name)) {
	    eaction = eaction_table[i].eaction;
	    break;
	}
    }
    if (eaction == NULL) {
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
    (void)(*eaction)(cause, count, parms);
}
