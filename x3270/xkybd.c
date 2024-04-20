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
 *	xkybd.c
 *		Xt-specific keyboard functions.
 */

#include "globals.h"
#include "xglobals.h"

#include <X11/Xatom.h>

#define XK_3270
#define XK_APL
#include <X11/keysym.h>

#include "resources.h"

#include "actions.h"
#include "apl.h"
#include "idle.h"
#include "keymap.h"
#include "keysym2ucs.h"
#include "kybd.h"
#include "names.h"
#include "popups.h"
#include "screen.h"
#include "selectc.h"
#include "task.h"
#include "toggles.h"
#include "trace.h"
#include "unicodec.h"
#include "xactions.h"
#include "xscreen.h"
#include "xselectc.h"

/*
 * Handle an ordinary character key, given its NULL-terminated multibyte
 * representation.
 */
static void
key_ACharacter(char *mb, enum keytype keytype, enum iaction cause)
{
    ucs4_t ucs4;
    int consumed;
    enum me_fail error;

    reset_idle_timer();

    /* Convert the multibyte string to UCS4. */
    ucs4 = multibyte_to_unicode(mb, strlen(mb), &consumed, &error);
    if (ucs4 == 0) {
	vtrace(" %s -> Key(?)\n", ia_name[(int) cause]);
	vtrace("  dropped (invalid multibyte sequence)\n");
	return;
    }

    key_UCharacter(ucs4, keytype, cause, false);
}

static bool
AltCursor_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnAltCursor, ia, argc, argv);
    if (check_argc(AnAltCursor, argc, 0, 0) < 0) {
	return false;
    }
    reset_idle_timer();
    do_toggle(ALT_CURSOR);
    return true;
}

/*
 * Cursor Select mouse action (light pen simulator).
 */
void
MouseSelect_xaction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{
    xaction_debug(MouseSelect_xaction, event, params, num_params);
    if (xcheck_usage(MouseSelect_xaction, *num_params, 0, 0) < 0) {
	return;
    }
    if (w != *screen) {
	return;
    }
    reset_idle_timer();
    if (kybdlock) {
	return;
    }
    if (IN_NVT) {
	return;
    }
    lightpen_select(mouse_baddr(w, event));
}

/*
 * MoveCursor/MoveCursor1 Xt action.  Depending on arguments, this is either a move to the
 * mouse cursor position, or to an absolute location.
 */
static void
MoveCursor_xcommon(char *name, Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    /* With arguments, this isn't a mouse call. */
    if (*num_params != 0) {
	if (*num_params > 2) {
	    popup_an_error("%s() takes 0, 1 or 2 arguments", name);
	    return;
	}
	run_action(name, IA_KEYMAP, params[0], params[1]);
	return;
    }

    /* If it is a mouse call, it only applies to the screen. */
    if (w != *screen) {
	return;
    }

    /* If the screen is locked, do nothing. */
    if (kybdlock) {
	return;
    }

    if (IN_NVT) {
	popup_an_error("%s() is not valid in NVT mode", name);
	return;
    }

    /* Move the cursor to where the mouse is. */
    reset_idle_timer();
    cursor_move(mouse_baddr(w, event));
}

/* MoveCursor Xt action. */
void
MoveCursor_xaction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    xaction_debug(MoveCursor_xaction, event, params, num_params);
    MoveCursor_xcommon(AnMoveCursor, w, event, params, num_params);
}

/* MoveCursor1 Xt action. */
void
MoveCursor1_xaction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    xaction_debug(MoveCursor1_xaction, event, params, num_params);
    MoveCursor_xcommon(AnMoveCursor1, w, event, params, num_params);
}

/*
 * Run a KeyPress through XIM.
 * Returns true if there is further processing to do, false otherwise.
 */
static bool
xim_lookup(XKeyEvent *event)
{
    static char *buf = NULL;
    static int buf_len = 0, rlen;
    KeySym k;
    Status status;
    int i;
    bool rv = false;
#define BASE_BUFSIZE 50

    if (ic == NULL) {
	return true;
    }

    if (buf == NULL) {
	buf_len = BASE_BUFSIZE;
	buf = Malloc(buf_len);
    }

    for (;;) {
	memset(buf, '\0', buf_len);
	rlen = XmbLookupString(ic, event, buf, buf_len - 1, &k, &status);
	if (status != XBufferOverflow) {
	    break;
	}
	buf_len += BASE_BUFSIZE;
	buf = Realloc(buf, buf_len);
    }

    switch (status) {
    case XLookupNone:
	rv = false;
	break;
    case XLookupKeySym:
	rv = true;
	break;
    case XLookupChars:
	vtrace("%d XIM char%s:", rlen, (rlen != 1)? "s": "");
	for (i = 0; i < rlen; i++) {
	    vtrace(" %02x", buf[i] & 0xff);
	}
	vtrace("\n");
	buf[rlen] = '\0';
	key_ACharacter(buf, KT_STD, ia_cause);
	rv = false;
	break;
    case XLookupBoth:
	rv = true;
	break;
    }
    return rv;
}

/*
 * X-dependent code starts here.
 */

/*
 * Translate a keymap (from an XQueryKeymap or a KeymapNotify event) into
 * a bitmap of Shift, Meta or Alt keys pressed.
 */
#define key_is_down(kc, bitmap) (kc && ((bitmap)[(kc)/8] & (1<<((kc)%8))))
int
state_from_keymap(char keymap[32])
{
    static bool initted = false;
    static KeyCode kc_Shift_L, kc_Shift_R;
    static KeyCode kc_Meta_L, kc_Meta_R;
    static KeyCode kc_Alt_L, kc_Alt_R;
    int	pseudo_state = 0;

    if (!initted) {
	kc_Shift_L = XKeysymToKeycode(display, XK_Shift_L);
	kc_Shift_R = XKeysymToKeycode(display, XK_Shift_R);
	kc_Meta_L  = XKeysymToKeycode(display, XK_Meta_L);
	kc_Meta_R  = XKeysymToKeycode(display, XK_Meta_R);
	kc_Alt_L   = XKeysymToKeycode(display, XK_Alt_L);
	kc_Alt_R   = XKeysymToKeycode(display, XK_Alt_R);
	initted = true;
    }
    if (key_is_down(kc_Shift_L, keymap) || key_is_down(kc_Shift_R, keymap)) {
	pseudo_state |= ShiftKeyDown;
    }
    if (key_is_down(kc_Meta_L, keymap) || key_is_down(kc_Meta_R, keymap)) {
	pseudo_state |= MetaKeyDown;
    }
    if (key_is_down(kc_Alt_L, keymap) || key_is_down(kc_Alt_R, keymap)) {
	pseudo_state |= AltKeyDown;
    }
    return pseudo_state;
}
#undef key_is_down

/*
 * Process shift keyboard events.  The code has to look for the raw Shift keys,
 * rather than using the handy "state" field in the event structure.  This is
 * because the event state is the state _before_ the key was pressed or
 * released.  This isn't enough information to distinguish between "left
 * shift released" and "left shift released, right shift still held down"
 * events, for example.
 *
 * This function is also called as part of Focus event processing.
 */
void
PA_Shift_xaction(Widget w _is_unused, XEvent *event _is_unused,
	String *params _is_unused, Cardinal *num_params _is_unused)
{
    char keys[32];

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
    xaction_debug(PA_Shift_xaction, event, params, num_params);
#endif /*]*/
    XQueryKeymap(display, keys);
    shift_event(state_from_keymap(keys));
}

/*
 * Called by the toolkit for any key without special actions.
 */
void
Default_xaction(Widget w _is_unused, XEvent *event, String *params,
	Cardinal *num_params)
{
    XKeyEvent	*kevent = (XKeyEvent *)event;
    char	buf[32];
    KeySym	ks;
    int		ll;

    xaction_debug(Default_xaction, event, params, num_params);
    if (xcheck_usage(Default_xaction, *num_params, 0, 0) < 0) {
	return;
    }
    switch (event->type) {
    case KeyPress:
	if (!xim_lookup((XKeyEvent *)event)) {
	    return;
	}
	ll = XLookupString(kevent, buf, 32, &ks, NULL);
	buf[ll] = '\0';
	if (ll > 1) {
	    key_ACharacter(buf, KT_STD, IA_DEFAULT);
	    return;
	}
	if (ll == 1) {
	    /* Remap certain control characters. */
	    if (!IN_NVT) switch (buf[0]) {
		case '\t':
		    run_action(AnTab, IA_DEFAULT, NULL, NULL);
		    break;
	       case '\177':
		    run_action(AnDelete, IA_DEFAULT, NULL, NULL);
		    break;
		case '\b':
		    run_action(AnErase, IA_DEFAULT, NULL, NULL);
		    break;
		case '\r':
		    run_action(AnEnter, IA_DEFAULT, NULL, NULL);
		    break;
		case '\n':
		    run_action(AnNewline, IA_DEFAULT, NULL, NULL);
		    break;
		default:
		    key_ACharacter(buf, KT_STD, IA_DEFAULT);
		    break;
	    } else {
		key_ACharacter(buf, KT_STD, IA_DEFAULT);
	    }
	    return;
	}

	/* Pick some other reasonable defaults. */
	switch (ks) {
	case XK_Up:
	    run_action(AnUp, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Down:
	    run_action(AnDown, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Left:
	    run_action(AnLeft, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Right:
	    run_action(AnRight, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Insert:
#if defined(XK_KP_Insert) /*[*/
	case XK_KP_Insert:
#endif /*]*/
	    run_action(AnInsert, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Delete:
	    run_action(AnDelete, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Home:
	    run_action(AnHome, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Tab:
	    run_action(AnTab, IA_DEFAULT, NULL, NULL);
	    break;
#if defined(XK_ISO_Left_Tab) /*[*/
	case XK_ISO_Left_Tab:
	    run_action(AnBackTab, IA_DEFAULT, NULL, NULL);
	    break;
#endif /*]*/
	case XK_Clear:
	    run_action(AnClear, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_Sys_Req:
	    run_action(AnSysReq, IA_DEFAULT, NULL, NULL);
	    break;
#if defined(XK_EuroSign) /*[*/
	case XK_EuroSign:
	    run_action(AnKey, IA_DEFAULT, "currency", NULL);
	    break;
#endif /*]*/

#if defined(XK_3270_Duplicate) /*[*/
	/* Funky 3270 keysyms. */
	case XK_3270_Duplicate:
	    run_action(AnDup, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_FieldMark:
	    run_action(AnFieldMark, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_Right2:
	    run_action(AnRight2, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_Left2:
	    run_action(AnLeft2, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_BackTab:
	    run_action(AnBackTab, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_EraseEOF:
	    run_action(AnEraseEOF, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_EraseInput:
	    run_action(AnEraseInput, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_Reset:
	    run_action(AnReset, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_PA1:
	    run_action(AnPA, IA_DEFAULT, "1", NULL);
	    break;
	case XK_3270_PA2:
	    run_action(AnPA, IA_DEFAULT, "2", NULL);
	    break;
	case XK_3270_PA3:
	    run_action(AnPA, IA_DEFAULT, "3", NULL);
	    break;
	case XK_3270_Attn:
	    run_action(AnAttn, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_AltCursor:
	    run_action(AnToggle, IA_DEFAULT, ResAltCursor, NULL);
	    break;
	case XK_3270_CursorSelect:
	    run_action(AnCursorSelect, IA_DEFAULT, NULL, NULL);
	    break;
	case XK_3270_Enter:
	    run_action(AnEnter, IA_DEFAULT, NULL, NULL);
	    break;
#endif /*]*/

	/* Funky APL keysyms. */
	case XK_downcaret:
	    run_action(AnKey, IA_DEFAULT, "apl_downcaret", NULL);
	    break;
	case XK_upcaret:
	    run_action(AnKey, IA_DEFAULT, "apl_upcaret", NULL);
	    break;
	case XK_overbar:
	    run_action(AnKey, IA_DEFAULT, "apl_overbar", NULL);
	    break;
	case XK_downtack:
	    run_action(AnKey, IA_DEFAULT, "apl_downtack", NULL);
	    break;
	case XK_upshoe:
	    run_action(AnKey, IA_DEFAULT, "apl_upshoe", NULL);
	    break;
	case XK_downstile:
	    run_action(AnKey, IA_DEFAULT, "apl_downstile", NULL);
	    break;
	case XK_underbar:
	    run_action(AnKey, IA_DEFAULT, "apl_underbar", NULL);
	    break;
	case XK_jot:
	    run_action(AnKey, IA_DEFAULT, "apl_jot", NULL);
	    break;
	case XK_quad:
	    run_action(AnKey, IA_DEFAULT, "apl_quad", NULL);
	    break;
	case XK_uptack:
	    run_action(AnKey, IA_DEFAULT, "apl_uptack", NULL);
	    break;
	case XK_circle:
	    run_action(AnKey, IA_DEFAULT, "apl_circle", NULL);
	    break;
	case XK_upstile:
	    run_action(AnKey, IA_DEFAULT, "apl_upstile", NULL);
	    break;
	case XK_downshoe:
	    run_action(AnKey, IA_DEFAULT, "apl_downshoe", NULL);
	    break;
	case XK_rightshoe:
	    run_action(AnKey, IA_DEFAULT, "apl_rightshoe", NULL);
	    break;
	case XK_leftshoe:
	    run_action(AnKey, IA_DEFAULT, "apl_leftshoe", NULL);
	    break;
	case XK_lefttack:
	    run_action(AnKey, IA_DEFAULT, "apl_lefttack", NULL);
	    break;
	case XK_righttack:
	    run_action(AnKey, IA_DEFAULT, "apl_righttack", NULL);
	    break;

	default:
	    if (ks >= XK_F1 && ks <= XK_F24) {
		snprintf(buf, sizeof(buf), "%ld", ks - XK_F1 + 1);
		run_action(AnPF, IA_DEFAULT, buf, NULL);
	    } else {
		ucs4_t ucs4;

		ucs4 = keysym2ucs(ks);
		if (ucs4 != (ucs4_t)-1) {
		    key_UCharacter(ucs4, KT_STD, IA_DEFAULT, false);
		} else {
		    vtrace(" Default: dropped (unknown keysym)\n");
		}
	    }
	    break;
	}
	break;

    case ButtonPress:
    case ButtonRelease:
	vtrace(" Default: dropped (no action configured)\n");
	break;
    default:
	vtrace(" Default: dropped (unknown event type)\n");
	break;
    }
}

/*
 * Set or clear a temporary keymap.
 *
 *   TemporaryKeymap(x)		toggle keymap "x" (add "x" to the keymap, or if
 *				"x" was already added, remove it)
 *   TemporaryKeymap()		removes the previous keymap, if any
 *   TemporaryKeymap(None)	removes the previous keymap, if any
 */
static bool
Keymap_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnKeymap, ia, argc, argv);
    if (check_argc(AnKeymap, argc, 0, 1) < 0) {
	return false;
    }

    reset_idle_timer();

    if (argc == 0 || !strcmp(argv[0], KwNone)) {
	temporary_keymap(NULL);
	return true;
    }

    if (!temporary_keymap(argv[0])) {
	popup_an_error(AnKeymap "(): Can't find %s %s", ResKeymap, argv[0]);
	return false;
    }
    return true;
}

/**
 * X keyboard module registration.
 */
void
xkybd_register(void)
{
    static action_table_t xkybd_actions[] = {
	{ AnAltCursor,		AltCursor_action,	ACTION_KE },
	{ AnKeymap,		Keymap_action,		ACTION_KE },
	{ AnTemporaryKeymap,	Keymap_action,		ACTION_KE }
    };

    /* Register the actions. */
    register_actions(xkybd_actions, array_count(xkybd_actions));
}
