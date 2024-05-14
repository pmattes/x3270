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
 *	xactions.c
 *		x3270 Xt actions table and debugging code.
 */

#include "globals.h"
#include "xglobals.h"

#include <assert.h>

#include "appres.h"

#include "actions.h"
#include "dialog.h"
#include "keymap.h"
#include "names.h"
#include "popups.h"
#include "resources.h"
#include "screen.h"
#include "selectc.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "xactions.h"
#include "xkybd.h"
#include "xmenubar.h"
#include "xpopups.h"
#include "xscreen.h"
#include "xselectc.h"

#include <X11/keysym.h>
#include <X11/XKBlib.h>

#define N_WRAPPERS	100

static void xaction_ndebug(const char *aname, XEvent *event, String *params,
	Cardinal *num_params);

#define MODMAP_SIZE	8
#define MAP_SIZE	13
#define MAX_MODS_PER	4
static struct {
    const char *name[MAX_MODS_PER];
    unsigned int mask;
    bool is_meta;
} skeymask[MAP_SIZE] = { 
    { { "Shift" }, ShiftMask, false },
    { { NULL } /* Lock */, LockMask, false },
    { { "Ctrl" }, ControlMask, false },
    { { NULL }, Mod1Mask, false },
    { { NULL }, Mod2Mask, false },
    { { NULL }, Mod3Mask, false },
    { { NULL }, Mod4Mask, false },
    { { NULL }, Mod5Mask, false },
    { { "Button1" }, Button1Mask, false },
    { { "Button2" }, Button2Mask, false },
    { { "Button3" }, Button3Mask, false },
    { { "Button4" }, Button4Mask, false },
    { { "Button5" }, Button5Mask, false }
};
static bool know_mods = false;

/*
 * The set-up of Xt actions is not straightforward.
 *
 * Some actions exist only as Xt actions, such as Default(). They can only be
 * called from keymaps (Xt translation tables). These actions are statically
 * defined in the array xactions[].
 *
 * Other actions are wrappers around common emulator actions such as Enter().
 * These take a more convoluted path, for several reasons. First, the set of
 * emulator actions is established at runtime, not compile-time. Second, Xt
 * actions have no way of accessing their own names, so it is impossible to
 * write an Xt action that behaves differently based on what name was used to
 * run it. So there can't just be one XtActionProc function that wraps all of
 * the common emulator functions. Instead, there needs to be an unknown
 * number of distinct C action functions defined, one for each common emulator
 * function, and each of those functions has to figure out which common
 * emulator function to call. So here is what happens:
 * - 100 functions are defined with the signature of an XtActionProc. They are
 *   named wrapper0 through wrapper99. (Currently about 60 are in use.)
 * - The body of each wrapper function wrapper<n> calls the function
 *   xt_wrapper(), passing it <n> and its XtActionProc arguments (event,
 *   params, etc.).
 * - There is a fixed array of each of the wrapper<n> functions' addresses,
 *   called xt_mapped_actions[].
 * - There is a dynamically-allocated array of XtActionsRec structures called
 *   wrapper_actions[].
 * - Each element of wrapper_actions[] is populated with the name of a common
 *   emulator action and the address of a sequential element of the
 *   xt_mapped_actions[] array.
 * So assume (for example) that the common function "Enter" is the fifth
 * member of actions_list. So it is assigned to element 4 of wrapper_actions[].
 * The corresponding function is wrapper4(), which calls xt_wrapper() with the
 * value 4. xt_wrapper() indexes the wrapper_actions[] array and finds the
 * proper name ("Enter"). It uses that for debug tracing and to call
 * run_action() to run the real code for Enter(), which (by way of searching
 * actions_list) is Enter_action(), which was registered by kybd.c.
 *
 * If I could figure out a simpler way to do this, and still automatically get
 * an Xt action for each (eligible) common emulator action, I would do it.
 */

/* Pure Xt actions. */
static XtActionsRec xactions[] = {
    { "Cut",		Cut_xaction },
    { "Default",	Default_xaction },
    { AnStepEfont,	StepEfont_xaction },
    { "HandleMenu",	HandleMenu_xaction },
    { "insert-selection",	insert_selection_xaction },
    { "KybdSelect",	KybdSelect_xaction },
    { "MouseSelect",	MouseSelect_xaction },
    { AnMoveCursor,	MoveCursor_xaction },
    { AnMoveCursor1,	MoveCursor1_xaction },
    { "move-select",	move_select_xaction },
    { PA_END,		PA_End_xaction },
    { PA_KEYMAP_TRACE,	PA_KeymapTrace_xaction },
    { PA_PFX "ConfigureNotify", PA_ConfigureNotify_xaction },
    { PA_PFX "confirm",	PA_confirm_xaction },
    { PA_PFX "dialog-copy", PA_dialog_copy_xaction },
    { PA_PFX "dialog-focus", PA_dialog_focus_xaction },
    { PA_PFX "dialog-next",	PA_dialog_next_xaction },
    { PA_PFX "EnterLeave",	PA_EnterLeave_xaction },
    { PA_PFX "Expose",	PA_Expose_xaction },
    { PA_PFX "Focus",	PA_Focus_xaction },
    { PA_PFX "GraphicsExpose", PA_GraphicsExpose_xaction },
    { PA_PFX "KeymapNotify", PA_KeymapNotify_xaction },
    { PA_PFX "Shift",	PA_Shift_xaction },
    { PA_PFX "StateChanged", PA_StateChanged_xaction },
    { PA_PFX "VisibilityNotify",PA_VisibilityNotify_xaction },
    { PA_PFX "WMProtocols",	PA_WMProtocols_xaction },
    { "Redraw",		Redraw_xaction },
    { "SelectAll",	SelectAll_xaction },
    { "SelectDown",	SelectDown_xaction },
    { "select-end",	select_end_xaction },
    { "select-extend",	select_extend_xaction },
    { "SelectMotion",	SelectMotion_xaction },
    { "select-start",	select_start_xaction },
    { "SelectUp",	SelectUp_xaction },
    { "set-select",	set_select_xaction },
    { "start-extend",	start_extend_xaction },
    { "Unselect",	Unselect_xaction }
};

static int xactioncount = XtNumber(xactions);

/* Table of Xt actions that wrap emulator actions. */
static XtActionsRec *wrapper_actions;
static int nwrappers = 0;

/* Xt action function for wrappers. */
static void
xt_wrapper(int n, Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    if (n < nwrappers) {
	const char **params_a;
	Cardinal i;

	/* Trace the Xt event. */
	xaction_ndebug(wrapper_actions[n].string, event, params, num_params);

	/* Run the emulator action. */
	params_a = (const char **)Malloc((*num_params + 1) *
		sizeof(const char *));
	for (i = 0; i < *num_params; i++) {
	    params_a[i] = params[i];
	}
	params_a[i] = NULL;
	run_action_a(wrapper_actions[n].string, IA_KEYMAP, *num_params,
		params_a);
	Free(params_a);
    }
}

/*
 * wrapper(n) creates an Xt action named mapped_action<n>(), which calls
 * xt_wrapper(n).
 */
#define wrapper(n) \
static void mapped_action ## n (Widget w _is_unused, XEvent *event, \
	String *params, Cardinal *num_params) \
{ \
    xt_wrapper(n, w, event, params, num_params); \
}

/* Create 100 wrapper functions, mapped_action0 through mapped_action99. */
wrapper(0)
wrapper(1)
wrapper(2)
wrapper(3)
wrapper(4)
wrapper(5)
wrapper(6)
wrapper(7)
wrapper(8)
wrapper(9)
wrapper(10)
wrapper(11)
wrapper(12)
wrapper(13)
wrapper(14)
wrapper(15)
wrapper(16)
wrapper(17)
wrapper(18)
wrapper(19)
wrapper(20)
wrapper(21)
wrapper(22)
wrapper(23)
wrapper(24)
wrapper(25)
wrapper(26)
wrapper(27)
wrapper(28)
wrapper(29)
wrapper(30)
wrapper(31)
wrapper(32)
wrapper(33)
wrapper(34)
wrapper(35)
wrapper(36)
wrapper(37)
wrapper(38)
wrapper(39)
wrapper(40)
wrapper(41)
wrapper(42)
wrapper(43)
wrapper(44)
wrapper(45)
wrapper(46)
wrapper(47)
wrapper(48)
wrapper(49)
wrapper(50)
wrapper(51)
wrapper(52)
wrapper(53)
wrapper(54)
wrapper(55)
wrapper(56)
wrapper(57)
wrapper(58)
wrapper(59)
wrapper(60)
wrapper(61)
wrapper(62)
wrapper(63)
wrapper(64)
wrapper(65)
wrapper(66)
wrapper(67)
wrapper(68)
wrapper(69)
wrapper(70)
wrapper(71)
wrapper(72)
wrapper(73)
wrapper(74)
wrapper(75)
wrapper(76)
wrapper(77)
wrapper(78)
wrapper(79)
wrapper(80)
wrapper(81)
wrapper(82)
wrapper(83)
wrapper(84)
wrapper(85)
wrapper(86)
wrapper(87)
wrapper(88)
wrapper(89)
wrapper(90)
wrapper(91)
wrapper(92)
wrapper(93)
wrapper(94)
wrapper(95)
wrapper(96)
wrapper(97)
wrapper(98)
wrapper(99)

/* Create an array of pointers to those functions. */
XtActionProc xt_mapped_actions[N_WRAPPERS] = {
    &mapped_action0, &mapped_action1, &mapped_action2, &mapped_action3,
    &mapped_action4, &mapped_action5, &mapped_action6, &mapped_action7,
    &mapped_action8, &mapped_action9, &mapped_action10, &mapped_action11,
    &mapped_action12, &mapped_action13, &mapped_action14, &mapped_action15,
    &mapped_action16, &mapped_action17, &mapped_action18, &mapped_action19,
    &mapped_action20, &mapped_action21, &mapped_action22, &mapped_action23,
    &mapped_action24, &mapped_action25, &mapped_action26, &mapped_action27,
    &mapped_action28, &mapped_action29, &mapped_action30, &mapped_action31,
    &mapped_action32, &mapped_action33, &mapped_action34, &mapped_action35,
    &mapped_action36, &mapped_action37, &mapped_action38, &mapped_action39,
    &mapped_action40, &mapped_action41, &mapped_action42, &mapped_action43,
    &mapped_action44, &mapped_action45, &mapped_action46, &mapped_action47,
    &mapped_action48, &mapped_action49, &mapped_action50, &mapped_action51,
    &mapped_action52, &mapped_action53, &mapped_action54, &mapped_action55,
    &mapped_action56, &mapped_action57, &mapped_action58, &mapped_action59,
    &mapped_action60, &mapped_action61, &mapped_action62, &mapped_action63,
    &mapped_action64, &mapped_action65, &mapped_action66, &mapped_action67,
    &mapped_action68, &mapped_action69, &mapped_action70, &mapped_action71,
    &mapped_action72, &mapped_action73, &mapped_action74, &mapped_action75,
    &mapped_action76, &mapped_action77, &mapped_action78, &mapped_action79,
    &mapped_action80, &mapped_action81, &mapped_action82, &mapped_action83,
    &mapped_action84, &mapped_action85, &mapped_action86, &mapped_action87,
    &mapped_action88, &mapped_action89, &mapped_action90, &mapped_action91,
    &mapped_action92, &mapped_action93, &mapped_action94, &mapped_action95,
    &mapped_action96, &mapped_action97, &mapped_action98, &mapped_action99
};

/*
 * Primary Xt action table initialization.
 */
void
xaction_init(void)
{
    /* Add the actions to Xt. */
    XtAppAddActions(appcontext, xactions, xactioncount);
}

/* Secondary Xt action table initialization. */
void
xaction_init2(void)
{
    action_elt_t *e;

    /* Allocate the array of Xt actions to add. */
    wrapper_actions = Malloc(actions_list_count * sizeof(XtActionsRec));

    /* Fill in the table. */
    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (strcmp(e->t.name, AnMoveCursor) && strcmp(e->t.name, AnMoveCursor1) && (e->t.flags & ACTION_KE)) {
	    wrapper_actions[nwrappers].string = (String)e->t.name;
	    wrapper_actions[nwrappers].proc = xt_mapped_actions[nwrappers];
	    nwrappers++;
	    assert(nwrappers <= N_WRAPPERS);
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);

    /* Add the actions to Xt. */
    XtAppAddActions(appcontext, wrapper_actions, nwrappers);
}

/*
 * Return a name for an Xt-only action.
 */
const char *
action_name(XtActionProc action)
{
    int i;

    for (i = 0; i < xactioncount; i++) {
	if (xactions[i].proc == action) {
	    return xactions[i].string;
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
	    bool is_meta = false;

	    kc = mm->modifiermap[(i * mm->max_keypermod) + j];
	    if (!kc) {
		continue;
	    }

	    switch(XkbKeycodeToKeysym(display, kc, 0, 0)) {
	    case XK_Meta_L:
	    case XK_Meta_R:
		name = "Meta";
		is_meta = true;
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
	    if (name == NULL) {
		continue;
	    }
	    if (is_meta) {
		skeymask[i].is_meta = true;
	    }

	    for (k = 0; k < MAX_MODS_PER; k++) {
		if (skeymask[i].name[k] == NULL) {
		    break;
		} else if (!strcmp(skeymask[i].name[k], name)) {
		    k = MAX_MODS_PER;
		}
	    }
	    if (k >= MAX_MODS_PER) {
		continue;
	    }
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
	know_mods = true;
    }

    if (*iteration == 0) {
	/* First time, build the table. */
	n_ix = 0;
	for (i = 0; i < MAP_SIZE; i++) {
	    if (skeymask[i].name[0] != NULL && (state & skeymask[i].mask)) {
		ix[i] = 0;
		state &= ~skeymask[i].mask;
		ix_ix[n_ix++] = i;
	    } else {
		ix[i] = MAX_MODS_PER;
	    }
	}
#if defined(VERBOSE_EVENTS) /*[*/
	leftover = state;
#endif /*]*/
    }

    /* Construct this result. */
    rs[0] = '\0';
    for (i = 0; i < n_ix;  i++) {
	strcat(rs, comma);
	strcat(rs, skeymask[ix_ix[i]].name[ix[ix_ix[i]]]);
	comma = " ";
    }
#if defined(VERBOSE_EVENTS) /*[*/
    if (leftover) {
	sprintf(strchr(rs, '\0'), "%s?%d", comma, state);
    }
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
	    if (i >= 0) {
		ix[ix_ix[i]]++;
	    }
	}
	*iteration = i >= 0;
    } else {
	*iteration = 0;
    }

    return rs;
}

/* Return whether or not an KeyPress event state includes the Meta key. */
bool
event_is_meta(int state)
{
    int i;

    /* Learn the modifier map. */
    if (!know_mods) {
	learn_modifiers();
	know_mods = true;
    }
    for (i = 0; i < MAP_SIZE; i++) {
	if (skeymask[i].name[0] != NULL &&
	    skeymask[i].is_meta &&
	    (state & skeymask[i].mask)) {
		return true;
	}
    }
    return false;
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
	    strcat(rs, comma);
	    strcat(rs, keymask[i].name);
	    comma = "|";
	    state &= ~keymask[i].mask;
	}
    }
    if (!rs[0]) {
	sprintf(rs, "%d", state);
    } else if (state) {
	sprintf(strchr(rs, '\0'), "%s?%d", comma, state);
    }
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
xcheck_usage(XtActionProc action, Cardinal nargs, Cardinal nargs_min,
	Cardinal nargs_max)
{
    if (nargs >= nargs_min && nargs <= nargs_max) {
	return 0;
    }
    if (nargs_min == nargs_max) {
	popup_an_error("%s requires %d argument%s", action_name(action),
		nargs_min, nargs_min == 1 ? "" : "s");
    } else {
	popup_an_error("%s requires %d or %d arguments", action_name(action),
		nargs_min, nargs_max);
    }
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
    XVisibilityEvent *vevent;
    const char *press = "Press";
    const char *direction = "Down";
    char dummystr[KSBUF+1];
    char *atom_name;
    int ambiguous = 0;
    int state;
    const char *symname = "";
    const char *viz[3] = { "Unobscured", "PartiallyObscured", "FullyObscured" };

    if (event == NULL) {
	vtrace(" %s", ia_name[(int)ia_cause]);
    } else switch (event->type) {
    case KeyRelease:
	press = "Release";
    case KeyPress:
	kevent = (XKeyEvent *)event;
	XLookupString(kevent, dummystr, KSBUF, &ks, NULL);
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
	    symname = txAsprintf("0x%lx", (unsigned long)ks);
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
    case VisibilityNotify:
	vevent = (XVisibilityEvent *)event;
	if (vevent->state >= VisibilityUnobscured &&
		vevent->state <= VisibilityFullyObscured) {
	    vtrace("VisibilityNotify [%s]", viz[vevent->state]);
	} else {
	    vtrace("VisibilityNotify [%d]", vevent->state);
	}
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
xaction_ndebug(const char *aname, XEvent *event, String *params,
	Cardinal *num_params)
{
    Cardinal i;

    if (!toggled(TRACING)) {
	return;
    }
    trace_event(event);
    vtrace(" -> %s(", aname);
    for (i = 0; i < *num_params; i++) {
	vtrace("%s%s",
		i? ", ": "",
		qscatv(params[i]));
    }
    vtrace(")\n");

    trace_rollover_check();
}

/*
 * Display an action debug message, given an action function.
 */
void
xaction_debug(XtActionProc action, XEvent *event, String *params,
	Cardinal *num_params)
{
    xaction_ndebug(action_name(action), event, params, num_params);
}

/*
 * Wrapper for calling an X11 action internally.
 */
void
xaction_internal(XtActionProc action, enum iaction cause, const char *parm1,
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
