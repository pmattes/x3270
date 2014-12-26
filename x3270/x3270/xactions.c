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
 *	xactions.c
 *		x3270 Xt actions table and debugging code.
 */

#include "globals.h"
#include "appres.h"

#include "actionsc.h"
#include "dialogc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "printc.h"
#include "print_windowc.h"
#include "resources.h"
#include "scrollc.h"
#include "selectc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xactionsc.h"
#include "xioc.h"
#include "xkybdc.h"

#include "unicodec.h"
#include "ftc.h"
#include "keypadc.h"
#include "menubarc.h"
#include "screenc.h"

#include <X11/keysym.h>
#include <X11/XKBlib.h>

static void action_ndebug(const char *aname, XEvent *event, String *params,
	Cardinal *num_params);

#define MODMAP_SIZE	8
#define MAP_SIZE	13
#define MAX_MODS_PER	4
static struct {
    const char *name[MAX_MODS_PER];
    unsigned int mask;
    Boolean is_meta;
} skeymask[MAP_SIZE] = { 
    { { "Shift" }, ShiftMask, False },
    { { NULL } /* Lock */, LockMask, False },
    { { "Ctrl" }, ControlMask, False },
    { { NULL }, Mod1Mask, False },
    { { NULL }, Mod2Mask, False },
    { { NULL }, Mod3Mask, False },
    { { NULL }, Mod4Mask, False },
    { { NULL }, Mod5Mask, False },
    { { "Button1" }, Button1Mask, False },
    { { "Button2" }, Button2Mask, False },
    { { "Button3" }, Button3Mask, False },
    { { "Button4" }, Button4Mask, False },
    { { "Button5" }, Button5Mask, False }
};
static Boolean know_mods = False;

/* Actions that are aliases for other actions. */
static char *aliased_actions[] = {
    "Close", "HardPrint", "Open", NULL
};

/*
 * xtwrapper(foo) creates an Xt action named foo_action(), which calls
 * foo_eaction.
 */
#define xtwrapper(name) \
    void name ## _action (Widget w _is_unused, XEvent *event, String *params, \
	    Cardinal *num_params) \
{ \
    action_ndebug(#name, event, params, num_params); \
    (void) name ## _eaction(IA_KEYMAP, *num_params, (const char **)params); \
}

/* Xt action wrappers for emulator actions. */
xtwrapper(AltCursor)
xtwrapper(Attn)
xtwrapper(BackSpace)
xtwrapper(BackTab)
xtwrapper(Bell)
xtwrapper(CircumNot)
xtwrapper(Clear)
xtwrapper(Compose)
xtwrapper(Connect)
xtwrapper(ContinueScript)
xtwrapper(CursorSelect)
xtwrapper(Delete)
xtwrapper(DeleteField)
xtwrapper(DeleteWord)
xtwrapper(Disconnect)
xtwrapper(Down)
xtwrapper(Dup)
xtwrapper(Enter)
xtwrapper(Erase)
xtwrapper(EraseEOF)
xtwrapper(EraseInput)
xtwrapper(Execute)
xtwrapper(FieldEnd)
xtwrapper(FieldMark)
xtwrapper(Flip)
xtwrapper(HexString)
xtwrapper(Home)
xtwrapper(Insert)
xtwrapper(Interrupt)
xtwrapper(Key)
xtwrapper(Left)
xtwrapper(Left2)
xtwrapper(Macro)
xtwrapper(MonoCase)
xtwrapper(Newline)
xtwrapper(NextWord)
xtwrapper(PA)
xtwrapper(PF)
xtwrapper(PreviousWord)
xtwrapper(Printer)
xtwrapper(PrintText)
xtwrapper(PrintWindow)
xtwrapper(Quit)
xtwrapper(Reconnect)
xtwrapper(Reset)
xtwrapper(Right)
xtwrapper(Right2)
xtwrapper(Script)
xtwrapper(Scroll)
xtwrapper(SetFont)
xtwrapper(Source)
xtwrapper(String)
xtwrapper(SysReq)
xtwrapper(Tab)
xtwrapper(Title)
xtwrapper(TemporaryKeymap)
xtwrapper(Toggle)
xtwrapper(ToggleInsert)
xtwrapper(ToggleReverse)
xtwrapper(Transfer)
xtwrapper(Up)
xtwrapper(Wait)
xtwrapper(WindowState)

XtActionsRec all_actions[] = {
    { "AltCursor",  	AltCursor_action },
    { "Attn",		Attn_action },
    { "BackSpace",	BackSpace_action },
    { "BackTab",	BackTab_action },
    { "Bell",		Bell_action },
    { "CircumNot",	CircumNot_action },
    { "Clear",		Clear_action },
    { "Compose",	Compose_action },
    { "Connect",	Connect_action },
    { "ContinueScript",	ContinueScript_action },
    { "CursorSelect",	CursorSelect_action },
    { "Cut",		Cut_action },
    { "Default",	Default_action },
    { "Delete", 	Delete_action },
    { "DeleteField",	DeleteField_action },
    { "DeleteWord",	DeleteWord_action },
    { "Disconnect",	Disconnect_action },
    { "Down",		Down_action },
    { "Dup",		Dup_action },
    { "Enter",		Enter_action },
    { "EraseEOF",	EraseEOF_action },
    { "Erase",		Erase_action },
    { "EraseInput",	EraseInput_action },
    { "Execute",	Execute_action },
    { "FieldEnd",	FieldEnd_action },
    { "FieldMark",	FieldMark_action },
    { "Flip",		Flip_action },
    { "HandleMenu",	HandleMenu_action },
    { "HexString",	HexString_action},
    { "Home",		Home_action },
    { "ignore",		ignore_action },
    { "Insert",		Insert_action },
    { "insert-selection",	insert_selection_action },
    { "Interrupt",	Interrupt_action },
    { "Key",		Key_action },
    { "Keymap",		TemporaryKeymap_action },
    { "KybdSelect",	KybdSelect_action },
    { "Left2", 		Left2_action },
    { "Left",		Left_action },
    { "Macro", 		Macro_action },
    { "MonoCase",	MonoCase_action },
    { "MouseSelect",	MouseSelect_action },
    { "MoveCursor",	MoveCursor_action },
    { "move-select",	move_select_action },
    { "Newline",	Newline_action },
    { "NextWord",	NextWord_action },
    { "Open",		Connect_action },
    { PA_END,		PA_End_action },
    { PA_KEYMAP_TRACE,	PA_KeymapTrace_action },
    { "PA",		PA_action },
    { PA_PFX "ConfigureNotify", PA_ConfigureNotify_action },
    { PA_PFX "confirm",	PA_confirm_action },
    { PA_PFX "dialog-focus", PA_dialog_focus_action },
    { PA_PFX "dialog-next",	PA_dialog_next_action },
    { PA_PFX "EnterLeave",	PA_EnterLeave_action },
    { PA_PFX "Expose",	PA_Expose_action },
    { PA_PFX "Focus",	PA_Focus_action },
    { PA_PFX "GraphicsExpose", PA_GraphicsExpose_action },
    { PA_PFX "KeymapNotify", PA_KeymapNotify_action },
    { PA_PFX "Shift",	PA_Shift_action },
    { PA_PFX "StateChanged", PA_StateChanged_action },
    { PA_PFX "VisibilityNotify",PA_VisibilityNotify_action },
    { PA_PFX "WMProtocols",	PA_WMProtocols_action },
    { "PF",		PF_action },
    { "PreviousWord",	PreviousWord_action },
    { "Printer",	Printer_action },
    { "PrintText",	PrintText_action },
    { "PrintWindow",	PrintWindow_action },
    { "Quit",		Quit_action },
    { "Reconnect",	Reconnect_action },
    { "Redraw",		Redraw_action },
    { "Reset",		Reset_action },
    { "Right2",		Right2_action },
    { "Right",		Right_action },
    { "Script",		Script_action },
    { "Scroll",		Scroll_action },
    { "SelectAll",	SelectAll_action },
    { "SelectDown",	SelectDown_action },
    { "select-end",	select_end_action },
    { "select-extend",	select_extend_action },
    { "SelectMotion",	SelectMotion_action },
    { "select-start",	select_start_action },
    { "SelectUp",	SelectUp_action },
    { "SetFont",	SetFont_action },
    { "set-select",	set_select_action },
    { "Source",		Source_action },
    { "start-extend",	start_extend_action },
    { "String",		String_action },
    { "SysReq",		SysReq_action },
    { "Tab",		Tab_action },
    { "TemporaryKeymap",TemporaryKeymap_action },
    { "Title",		Title_action },
    { "ToggleInsert",	ToggleInsert_action },
    { "ToggleReverse",	ToggleReverse_action },
    { "Toggle",		Toggle_action },
    { "Transfer",	Transfer_action },
    { "Unselect",	Unselect_action },
    { "Up",		Up_action },
    { "Wait",		Wait_action },
    { "WindowState",	WindowState_action },
};

int xactioncount = XtNumber(all_actions);
XtActionsRec *xactions = NULL;

/* No-op action for suppressed actions. */
static void
suppressed_action(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
	action_debug(suppressed_action, event, params, num_params);
}

/*
 * Xt action table initialization.
 * Uses the suppressActions resource to prune the actions table.
 */
void
xaction_init(void)
{
    char *suppress;
    int i;

    /* See if there are any filters at all. */
    suppress = get_resource(ResSuppressActions);
    if (suppress == NULL) {
	xactions = all_actions;
	return;
    }

    /* Yes, we'll need to copy the table and prune it. */
    xactions = (XtActionsRec *)Malloc(sizeof(all_actions));
    memcpy(xactions, all_actions, sizeof(all_actions));
    for (i = 0; i < xactioncount; i++) {
	if (action_suppressed(xactions[i].string, suppress)) {
	    xactions[i].proc = suppressed_action;
	}
    }
}

/*
 * Return a name for an action.
 */
const char *
action_name(XtActionProc action)
{
    int i;

    /*
     * XXX: It would be better if the real name could be displayed, with a
     * message indicating it is suppressed.
     */
    if (action == suppressed_action) {
	return "(suppressed)";
    }

    for (i = 0; i < xactioncount; i++) {
	if (xactions[i].proc == action) {
	    int j;
	    Boolean aliased = False;

	    for (j = 0; aliased_actions[j] != NULL; j++) {
		if (!strcmp(aliased_actions[j], xactions[i].string)) {
		    aliased = True;
		    break;
		}
	    }
	    if (!aliased) {
		return xactions[i].string;
	    }
	}
    }

    return "(unknown)";
}

/*
 * Search the modifier map to learn the modifier bits for Meta, Alt, Hyper and
 *  Super.
 */
static void
learn_modifiers(void)
{
	XModifierKeymap *mm;
	int i, j, k;
	static char *default_modname[] = {
	    NULL, NULL, "Ctrl",
	    "Mod1", "Mod2", "Mod3", "Mod4", "Mod5",
	    "Button1", "Button2", "Button3", "Button4", "Button5"
	};

	mm = XGetModifierMapping(display);

	for (i = 0; i < MODMAP_SIZE; i++) {
		for (j = 0; j < mm->max_keypermod; j++) {
			KeyCode kc;
			const char *name = NULL;
			Boolean is_meta = False;

			kc = mm->modifiermap[(i * mm->max_keypermod) + j];
			if (!kc)
				continue;

			switch(XkbKeycodeToKeysym(display, kc, 0, 0)) {
			    case XK_Meta_L:
			    case XK_Meta_R:
				name = "Meta";
				is_meta = True;
				break;
			    case XK_Alt_L:
			    case XK_Alt_R:
				name = "Alt";
				break;
			    case XK_Super_L:
			    case XK_Super_R:
				name = "Super";
				break;
			    case XK_Hyper_L:
			    case XK_Hyper_R:
				name = "Hyper";
				break;
			    default:
				break;
			}
			if (name == NULL)
				continue;
			if (is_meta)
				skeymask[i].is_meta = True;

			for (k = 0; k < MAX_MODS_PER; k++) {
				if (skeymask[i].name[k] == NULL)
					break;
				else if (!strcmp(skeymask[i].name[k], name))
					k = MAX_MODS_PER;
			}
			if (k >= MAX_MODS_PER)
				continue;
			skeymask[i].name[k] = name;
		}
	}
	for (i = 0; i < MODMAP_SIZE; i++) {
		if (skeymask[i].name[0] == NULL) {
			skeymask[i].name[0] = default_modname[i];
		}
	}
	XFreeModifiermap(mm);
}

/*
 * Return the symbolic name for the modifier combination (i.e., "Meta" instead
 * of "Mod2".  Note that because it is possible to map multiple keysyms to the
 * same modifier bit, the answer may be ambiguous; we return the combinations
 * iteratively.
 */
static char *
key_symbolic_state(unsigned int state, int *iteration)
{
	static char rs[64];
	static int ix[MAP_SIZE];
	static int ix_ix[MAP_SIZE];
	static int n_ix = 0;
#if defined(VERBOSE_EVENTS) /*[*/
	static int leftover = 0;
#endif /*]*/
	const char *comma = "";
	int i;

	if (!know_mods) {
		learn_modifiers();
		know_mods = True;
	}

	if (*iteration == 0) {
		/* First time, build the table. */
		n_ix = 0;
		for (i = 0; i < MAP_SIZE; i++) {
			if (skeymask[i].name[0] != NULL &&
			    (state & skeymask[i].mask)) {
				ix[i] = 0;
				state &= ~skeymask[i].mask;
				ix_ix[n_ix++] = i;
			} else
				ix[i] = MAX_MODS_PER;
		}
#if defined(VERBOSE_EVENTS) /*[*/
		leftover = state;
#endif /*]*/
	}

	/* Construct this result. */
	rs[0] = '\0';
	for (i = 0; i < n_ix;  i++) {
		(void) strcat(rs, comma);
		(void) strcat(rs, skeymask[ix_ix[i]].name[ix[ix_ix[i]]]);
		comma = " ";
	}
#if defined(VERBOSE_EVENTS) /*[*/
	if (leftover)
		(void) sprintf(strchr(rs, '\0'), "%s?%d", comma, state);
#endif /*]*/

	/*
	 * Iterate to the next.
	 * This involves treating each slot like an n-ary number, where n is
	 * the number of elements in the slot, iterating until the highest-
	 * ordered slot rolls back over to 0.
	 */
	if (n_ix) {
		i = n_ix - 1;
		ix[ix_ix[i]]++;
		while (i >= 0 &&
		       (ix[ix_ix[i]] >= MAX_MODS_PER ||
			skeymask[ix_ix[i]].name[ix[ix_ix[i]]] == NULL)) {
			ix[ix_ix[i]] = 0;
			i = i - 1;
			if (i >= 0)
				ix[ix_ix[i]]++;
		}
		*iteration = i >= 0;
	} else
		*iteration = 0;

	return rs;
}

/* Return whether or not an KeyPress event state includes the Meta key. */
Boolean
event_is_meta(int state)
{
    int i;

    /* Learn the modifier map. */
    if (!know_mods) {
	learn_modifiers();
	know_mods = True;
    }
    for (i = 0; i < MAP_SIZE; i++) {
	if (skeymask[i].name[0] != NULL &&
	    skeymask[i].is_meta &&
	    (state & skeymask[i].mask)) {
		return True;
	}
    }
    return False;
}

#if defined(VERBOSE_EVENTS) /*[*/
static char *
key_state(unsigned int state)
{
	static char rs[64];
	const char *comma = "";
	static struct {
		const char *name;
		unsigned int mask;
	} keymask[] = {
		{ "Shift", ShiftMask },
		{ "Lock", LockMask },
		{ "Control", ControlMask },
		{ "Mod1", Mod1Mask },
		{ "Mod2", Mod2Mask },
		{ "Mod3", Mod3Mask },
		{ "Mod4", Mod4Mask },
		{ "Mod5", Mod5Mask },
		{ "Button1", Button1Mask },
		{ "Button2", Button2Mask },
		{ "Button3", Button3Mask },
		{ "Button4", Button4Mask },
		{ "Button5", Button5Mask },
		{ NULL, 0 },
	};
	int i;

	rs[0] = '\0';
	for (i = 0; keymask[i].name; i++) {
		if (state & keymask[i].mask) {
			(void) strcat(rs, comma);
			(void) strcat(rs, keymask[i].name);
			comma = "|";
			state &= ~keymask[i].mask;
		}
	}
	if (!rs[0])
		(void) sprintf(rs, "%d", state);
	else if (state)
		(void) sprintf(strchr(rs, '\0'), "%s?%d", comma, state);
	return rs;
}
#endif /*]*/

/*
 * Check the number of argument to an action, and possibly pop up a usage
 * message.
 *
 * Returns 0 if the argument count is correct, -1 otherwise.
 */
int
check_usage(XtActionProc action, Cardinal nargs, Cardinal nargs_min,
    Cardinal nargs_max)
{
	if (nargs >= nargs_min && nargs <= nargs_max)
		return 0;
	if (nargs_min == nargs_max)
		popup_an_error("%s requires %d argument%s",
		    action_name(action), nargs_min, nargs_min == 1 ? "" : "s");
	else
		popup_an_error("%s requires %d or %d arguments",
		    action_name(action), nargs_min, nargs_max);
	cancel_if_idle_command();
	return -1;
}

/**
 * Trace the event that caused an action to be called.
 */
#define KSBUF	256
static void
trace_event(XEvent *event)
{
    XKeyEvent *kevent;
    KeySym ks;
    XButtonEvent *bevent;
    XMotionEvent *mevent;
    XConfigureEvent *cevent;
    XClientMessageEvent *cmevent;
    XExposeEvent *exevent;
    const char *press = "Press";
    const char *direction = "Down";
    char dummystr[KSBUF+1];
    char *atom_name;
    int ambiguous = 0;
    int state;
    const char *symname = "";
    char snbuf[11];

    if (event == NULL) {
	vtrace(" %s", ia_name[(int)ia_cause]);
    } else switch (event->type) {
    case KeyRelease:
	press = "Release";
    case KeyPress:
	kevent = (XKeyEvent *)event;
	(void) XLookupString(kevent, dummystr, KSBUF, &ks, NULL);
	state = kevent->state;

	/*
	 * If the keysym is a printable ASCII character, ignore the
	 * Shift key.
	 */
	if (ks != ' ' && !(ks & ~0xff) && isprint(ks)) {
	    state &= ~ShiftMask;
	} if (ks == NoSymbol) {
	    symname = "NoSymbol";
	} else if ((symname = XKeysymToString(ks)) == NULL) {
	    (void) snprintf(snbuf, sizeof(snbuf), "0x%lx", (unsigned long)ks);
	    symname = snbuf;
	}
	do {
	    int was_ambiguous = ambiguous;

	    vtrace("%s ':%s<Key%s>%s'",
		    was_ambiguous? " or": "Event",
		    key_symbolic_state(state, &ambiguous),
		    press,
		    symname);
	} while (ambiguous);

	/*
	 * If the keysym is an alphanumeric ASCII character, show the
	 * case-insensitive alternative, sans the colon.
	 */
	if (!(ks & ~0xff) && isalpha(ks)) {
	    ambiguous = 0;
	    do {
		int was_ambiguous = ambiguous;

		vtrace(" %s '%s<Key%s>%s'",
			was_ambiguous? "or": "(case-insensitive:",
			key_symbolic_state(state, &ambiguous),
			press,
			symname);
	    } while (ambiguous);
	    vtrace(")");
	}
#if defined(VERBOSE_EVENTS) /*[*/
	vtrace("\nKey%s [state %s, keycode %d, keysym "
		    "0x%lx \"%s\"]",
		    press, key_state(kevent->state),
		    kevent->keycode, ks,
		    symname);
#endif /*]*/
	break;
    case ButtonRelease:
	press = "Release";
	direction = "Up";
	/* fall through */
    case ButtonPress:
	bevent = (XButtonEvent *)event;
	do {
	    int was_ambiguous = ambiguous;

	    vtrace("%s '%s<Btn%d%s>'",
		    was_ambiguous? " or": "Event",
		    key_symbolic_state(bevent->state, &ambiguous),
		    bevent->button,
		    direction);
	} while (ambiguous);
#if defined(VERBOSE_EVENTS) /*[*/
	vtrace("\nButton%s [state %s, button %d]",
		press, key_state(bevent->state),
		bevent->button);
#endif /*]*/
	break;
    case MotionNotify:
	mevent = (XMotionEvent *)event;
	do {
	    int was_ambiguous = ambiguous;

	    vtrace("%s '%s<Motion>'",
		    was_ambiguous? " or": "Event",
		    key_symbolic_state(mevent->state, &ambiguous));
	} while (ambiguous);
#if defined(VERBOSE_EVENTS) /*[*/
	vtrace("\nMotionNotify [state %s]",
		    key_state(mevent->state));
#endif /*]*/
	break;
    case EnterNotify:
	vtrace("EnterNotify");
	break;
    case LeaveNotify:
	vtrace("LeaveNotify");
	break;
    case FocusIn:
	vtrace("FocusIn");
	break;
    case FocusOut:
	vtrace("FocusOut");
	break;
    case KeymapNotify:
	vtrace("KeymapNotify");
	break;
    case Expose:
	exevent = (XExposeEvent *)event;
	vtrace("Expose [%dx%d+%d+%d]",
		exevent->width, exevent->height, exevent->x, exevent->y);
	break;
    case PropertyNotify:
	vtrace("PropertyNotify");
	break;
    case ClientMessage:
	cmevent = (XClientMessageEvent *)event;
	atom_name = XGetAtomName(display, (Atom)cmevent->data.l[0]);
	vtrace("ClientMessage [%s]",
		(atom_name == NULL)? "(unknown)": atom_name);
	break;
    case ConfigureNotify:
	cevent = (XConfigureEvent *)event;
	vtrace("ConfigureNotify [%dx%d+%d+%d]",
		cevent->width, cevent->height, cevent->x, cevent->y);
	break;
    default:
	vtrace("Event %d", event->type);
	break;
    }
    if (keymap_trace != NULL) {
	vtrace(" via %s", keymap_trace);
    }
}

/*
 * Display an action debug message, given an action name.
 */
static void
action_ndebug(const char *aname, XEvent *event, String *params,
	Cardinal *num_params)
{
    Cardinal i;
    char pbuf[1024];

    if (!toggled(TRACING)) {
	return;
    }
    trace_event(event);
    vtrace(" -> %s(", aname);
    for (i = 0; i < *num_params; i++) {
	vtrace("%s\"%s\"",
		i? ", ": "",
		scatv(params[i], pbuf, sizeof(pbuf)));
    }
    vtrace(")\n");

    trace_rollover_check();
}

/*
 * Display an action debug message, given an action function.
 */
void
action_debug(XtActionProc action, XEvent *event, String *params,
	Cardinal *num_params)
{
    action_ndebug(action_name(action), event, params, num_params);
}

/*
 * Wrapper for calling an X11 action internally.
 */
void
action_internal(XtActionProc action, enum iaction cause, const char *parm1,
    const char *parm2)
{
	Cardinal count = 0;
	String parms[2];

	/* Duplicate the parms, because XtActionProc doesn't grok 'const'. */
	if (parm1 != NULL) {
		parms[0] = NewString(parm1);
		count++;
		if (parm2 != NULL) {
			parms[1] = NewString(parm2);
			count++;
		}
	}

	ia_cause = cause;
	(*action)((Widget) NULL, (XEvent *) NULL,
	    count ? parms : (String *) NULL,
	    &count);

	/* Free the parm copies. */
	switch (count) {
	    case 2:
		Free(parms[1]);
		/* fall through... */
	    case 1:
		Free(parms[0]);
		break;
	    default:
		break;
	}
}
