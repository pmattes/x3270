/*
 * Copyright (c) 2016-2026 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	winops.c
 *		A GUI back-end for a 3270 Terminal Emulator
 *		XTWINOPS support
 */

#include "globals.h"

#include <errno.h>
#include <inttypes.h>
#include <expat.h>
#include <limits.h>

#include "b3270proto.h"
#include "json.h"
#include "json_run.h"
#include "model.h"
#include "nvt_gui.h"
#include "trace.h"
#include "txa.h"
#include "ui_stream.h"
#include "xtwinops.h"

#include "winops.h"

static window_state_t window_state = WS_NORMAL;
static int location_x = 0, location_y = 0;
static unsigned character_width = 0, character_height = 0;
static unsigned screen_width = 0, screen_height = 0;
static unsigned window_width = 0, window_height = 0;
static char *window_title = NULL;

/* Parse a signed integer. Returns true for success. */
static bool
parse_int(const char *name, const char *text, int *ret)
{
    char *end;
    long l;

    errno = 0;
    l = strtol(text, &end, 10);

    if (*end != '\0' || errno == ERANGE || l < SHRT_MIN || l > SHRT_MAX) {
	ui_invalid_attribute(OperWindowChange, name, "must be a short integer");
	return false;
    }

    *ret = (int)l;
    return true;
}

/* Parse an unsigned integer. Returns true for success. */
static bool
parse_unsigned(const char *name, const char *text, unsigned *ret)
{
    char *end;
    unsigned long l;

    errno = 0;
    l = strtoul(text, &end, 10);

    if (*end != '\0' || errno == ERANGE || l > USHRT_MAX) {
	ui_invalid_attribute(OperWindowChange, name, "must be an unsigned short integer");
	return false;
    }

    *ret = (unsigned)l;
    return true;
}

/* Handle a window change, XML version. */
void
do_window_change(const char *name, const char **attrs)
{
    const char *operation = NULL;
    const char *state = NULL;
    const char *x = NULL;
    int x_val;
    const char *y = NULL;
    int y_val;
    const char *type = NULL;
    const char *width = NULL;
    unsigned width_val;
    const char *height = NULL;
    unsigned height_val;
    bool is_window = false;
    bool is_character = false;
    const char *text = NULL;
    int i;

    for (i = 0; attrs[i] != NULL; i += 2) {
	if (!strcasecmp(attrs[i], AttrOperation)) {
	    operation = attrs[i + 1];
	    /* State X Y Type Width Height */
	} else if (!strcasecmp(attrs[i], AttrState)) {
	    state = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrX)) {
	    x = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrY)) {
	    y = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrType)) {
	    type = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrWidth)) {
	    width = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrHeight)) {
	    height = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrText)) {
	    text = attrs[i + 1];
	} else {
	    ui_unknown_attribute(OperWindowChange, attrs[i]);
	    return;
	}
    }

    if (operation == NULL) {
	ui_missing_attribute(OperWindowChange, AttrOperation);
	return;
    }
    if (!strcasecmp(operation, WinState)) {
	if (state == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrState);
	    return;
	}
	if (!strcasecmp(state, StateNormal)) {
	    window_state = WS_NORMAL;
	} else if (!strcasecmp(state, StateIconified)) {
	    window_state = WS_ICONIFIED;
	} else if (!strcasecmp(state, StateMaximized)) {
	    window_state = WS_MAXIMIZED;
	} else if (!strcasecmp(state, StateFullScreen)) {
	    window_state = WS_FULLSCREEN;
	} else {
	    ui_invalid_attribute(OperWindowChange, AttrState, "unknown");
	}
    } else if (!strcasecmp(operation, WinMove)) {
	if (x == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrX);
	    return;
	}
	if (y == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrY);
	    return;
	}
	if (!parse_int(AttrX, x, &x_val) || !parse_int(AttrY, y, &y_val)) {
	    return;
	}
	location_x = x_val;
	location_y = y_val;
    } else if (!strcasecmp(operation, WinSize)) {
	if (type == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrType);
	    return;
	}
	if (!(is_window = !strcasecmp(SizeWindow, type)) &&
		!(is_character = !strcasecmp(SizeCharacter, type)) &&
		strcasecmp(SizeScreen, type)) {
	    ui_invalid_attribute(OperWindowChange, AttrType, "unknown");
	    return;
	}
	if (width == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrWidth);
	    return;
	}
	if (height == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrHeight);
	    return;
	}
	if (!parse_unsigned(AttrWidth, width, &width_val) || !parse_unsigned(AttrHeight, height, &height_val)) {
	    return;
	}
	if (is_window) {
	    window_width = width_val;
	    window_height = height_val;
	} else if (is_character) {
	    character_width = width_val;
	    character_height = height_val;
	} else {
	    screen_width = width_val;
	    screen_height = height_val;
	}
    } else if (!strcasecmp(operation, WinTitle)) {
	if (text == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrText);
	    return;
	}
	Replace(window_title, NewString(text));
    } else {
	ui_invalid_attribute(OperWindowChange, AttrOperation, "unknown");
    }
}

/* Check for a valid signed short value. */
static bool
valid_short(const json_t *element)
{
    int64_t v;

    if (!json_is_integer(element) || (v = json_integer_value(element)) < SHRT_MIN || v > SHRT_MAX) {
	ui_invalid_attribute(OperWindowChange, AttrX, "must be a short integer");
	return false;
    }
    return true;
}

/* Check for a valid unsigned short value. */
static bool
valid_ushort(const json_t *element)
{
    int64_t v;

    if (!json_is_integer(element) || (v = json_integer_value(element)) < 0 || v > USHRT_MAX) {
	ui_invalid_attribute(OperWindowChange, AttrX, "must be an unsigned short integer");
	return false;
    }
    return true;
}

/* Handle a window change, JSON version. */
void
do_jwindow_change(const json_t *j)
{
    const char *operation = NULL;
    const char *state = NULL;
    int *x = NULL;
    int v_x;
    int *y = NULL;
    int v_y;
    const char *type = NULL;
    int *width = NULL;
    int v_width;
    int *height = NULL;
    int v_height;
    bool is_window = false;
    bool is_character = false;
    const char *text = NULL;
    const char *key;
    size_t key_length;
    const json_t *element;

    if (!json_is_object(j)) {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING, IndWindowChange " parameter must be an object",
		NULL);
	return;
    }

    BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, element) {
	if (json_key_matches(key, key_length, AttrOperation)) {
	    if ((operation = get_jstring(element, OperWindowChange, AttrOperation)) == NULL) {
		return;
	    }
	} else if (json_key_matches(key, key_length, AttrState)) {
	    if ((state = get_jstring(element, OperWindowChange, AttrState)) == NULL) {
		return;
	    }
	} else if (json_key_matches(key, key_length, AttrX)) {
	    if (!valid_short(element)) {
		return;
	    }
	    v_x = (int)json_integer_value(element);
	    x = &v_x;
	} else if (json_key_matches(key, key_length, AttrY)) {
	    if (!valid_short(element)) {
		return;
	    }
	    v_y = (int)json_integer_value(element);
	    y = &v_y;
	} else if (json_key_matches(key, key_length, AttrType)) {
	    if ((type = get_jstring(element, OperWindowChange, AttrType)) == NULL) {
		return;
	    }
	} else if (json_key_matches(key, key_length, AttrWidth)) {
	    if (!valid_ushort(element)) {
		return;
	    }
	    v_width = (int)json_integer_value(element);
	    width = &v_width;
	} else if (json_key_matches(key, key_length, AttrHeight)) {
	    if (!valid_ushort(element)) {
		return;
	    }
	    v_height = (int)json_integer_value(element);
	    height = &v_height;
	} else if (json_key_matches(key, key_length, AttrText)) {
	    if ((text = get_jstring(element, OperWindowChange, AttrText)) == NULL) {
		return;
	    }
	} else {
	    ui_unknown_attribute(OperWindowChange, txAsprintf("%.*s", (int)key_length, key));
	    return;
	}
    } END_JSON_OBJECT_FOREACH(j, key, key_length, element);

    if (operation == NULL) {
	ui_missing_attribute(OperWindowChange, AttrOperation);
	return;
    }
    if (!strcasecmp(operation, WinState)) {
	if (state == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrState);
	    return;
	}
	if (!strcasecmp(state, StateNormal)) {
	    window_state = WS_NORMAL;
	} else if (!strcasecmp(state, StateIconified)) {
	    window_state = WS_ICONIFIED;
	} else if (!strcasecmp(state, StateMaximized)) {
	    window_state = WS_MAXIMIZED;
	} else if (!strcasecmp(state, StateFullScreen)) {
	    window_state = WS_FULLSCREEN;
	} else {
	    ui_invalid_attribute(OperWindowChange, AttrState, "unknown");
	}
    } else if (!strcasecmp(operation, WinMove)) {
	if (x == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrX);
	    return;
	}
	if (y == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrY);
	    return;
	}
	location_x = *x;
	location_y = *y;
    } else if (!strcasecmp(operation, WinSize)) {
	if (height == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrHeight);
	    return;
	}
	if (width == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrWidth);
	    return;
	}
	if (type == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrType);
	    return;
	}
	if (!(is_window = !strcasecmp(type, SizeWindow)) && !(is_character = !strcasecmp(type, SizeCharacter))
		&& strcasecmp(type, SizeScreen)) {
	    ui_invalid_attribute(OperWindowChange, AttrType, "unknown value");
	    return;
	}
	if (is_window) {
	    window_width = *width;
	    window_height = *height;
	} else if (is_character) {
	    character_width = *width;
	    character_height = *height;
	} else {
	    screen_width = *width;
	    screen_height = *height;
	}
    } else if (!strcasecmp(operation, WinTitle)) {
	if (text == NULL) {
	    ui_missing_attribute(OperWindowChange, AttrText);
	    return;
	}
	Replace(window_title, NewString(text));
    } else {
	ui_invalid_attribute(OperWindowChange, AttrOperation, "unknown value");
    }
}

/**
 * xterm window operation
 *
 * @param[in] p1	Parameter 1
 * @param[in] p2	Parameter 2
 * @param[in] p3	Parameter 3
 * @param[out] rp1	Return parameter 1
 * @param[out] rp2	Return parameter 2
 * @param[out] rtext	Returned text
 */
void
xtwinops(unsigned short p1, unsigned short *p2, unsigned short *p3, unsigned short *rp1, unsigned short *rp2, const char **rtext)
{
    *rp1 = 0;
    *rp2 = 0;
    *rtext = NULL;

    switch (p1) {
    case XTW_1DEICONIFY: /* Un-iconify */
    case XTW_2ICONIFY: /* Iconify */
	ui_leaf(IndWindowChange,
		AttrOperation, AT_STRING, WinState,
		AttrState, AT_STRING, (p1 == 2)? StateIconified: StateNormal,
		NULL);
	break;
    case XTW_3MOVE: /* Move to x,y */
	ui_leaf(IndWindowChange,
		AttrOperation, AT_STRING, WinMove,
		AttrX, AT_INT, *p2,
		AttrY, AT_INT, *p3,
		NULL);
	break;
    case XTW_4RESIZE_PIXELS: /* resize to window to h,w pixels */
	if (p2 && p3) {
	    ui_leaf(IndWindowChange,
		    AttrOperation, AT_STRING, WinSize,
		    AttrType, AT_STRING, SizeWindow,
		    AttrHeight, AT_INT, *p2,
		    AttrWidth, AT_INT, *p3,
		    NULL);
	} else if (p2) {
	    ui_leaf(IndWindowChange,
		    AttrOperation, AT_STRING, WinSize,
		    AttrType, AT_STRING, SizeWindow,
		    AttrHeight, AT_INT, *p2,
		    NULL);
	} else {
	    ui_leaf(IndWindowChange,
		    AttrOperation, AT_STRING, WinSize,
		    AttrType, AT_STRING, SizeWindow,
		    AttrWidth, AT_INT, *p3,
		    NULL);
	}
	break;
    case XTW_5RAISE: /* raise */
    case XTW_6LOWER: /* lower */
	ui_leaf(IndWindowChange,
		AttrOperation, AT_STRING, WinStack,
		AttrOrder, AT_STRING, (p1 == 5)? OrderRaise: OrderLower,
		NULL);
	break;
    case XTW_7REFRESH: /* refresh */
	ui_leaf(IndWindowChange,
		AttrOperation, AT_STRING, WinRefresh,
		NULL);
	break;
    case XTW_8RESIZE_CHARACTERS: /* set screen size to h,w characters */
	if ((p2 && !*p2) || (p3 && !*p3)) {
	    /* We don't do the whole screen size thing, at least not yet. */
	    return;
	}
	live_change_oversize(p3? *p3: COLS, p2? *p2: ROWS);
	break;
    case XTW_9MAXIMIZE: /* Un-maximize (0) or maximize (1). */
	{
	    const char *state;
	    switch (p2? *p2: 0) {
	    case XTW_9MAXIMIZE_0RESET:
		state = StateNormal;
		break;
	    case XTW_9MAXIMIZE_1SET:
		state = StateMaximized;
		break;
	    default:
		return;
	    }
	    ui_leaf(IndWindowChange,
		    AttrOperation, AT_STRING, WinState,
		    AttrState, AT_STRING, state,
		    NULL);
	}
	break;
    case XTW_10FULLSCREEN: /* full-screen (0), undo full-screen (1), toggle full-screen (2): */
	{
	    const char *state;
	    switch (p2? *p2: 0) {
	    case XTW_10FULLSCREEN_0RESET:
		state = StateNormal;
		break;
	    case XTW_10FULLSCREEN_1SET:
		state = StateFullScreen;
		break;
	    case XTW_10FULLSCREEN_2TOGGLE:
		state = StateToggleFullScreen;
		break;
	    default:
		return;
	    }
	    ui_leaf(IndWindowChange,
		    AttrOperation, AT_STRING, WinState,
		    AttrState, AT_STRING, state,
		    NULL);
	}
	break;
    case XTWR_11WINDOWSTATE: /* report window state */
	*rp1 = (window_state == WS_ICONIFIED)? XTW_2ICONIFY: XTW_1DEICONIFY;
	break;
    case XTWR_13WINDOWPOSITION: /* report window position x;y */
	*rp1 = location_x;
	*rp2 = location_y;
	break;
    case XTWR_14WINDOWSIZE_PIXELS:
	if (!p2 || *p2 == XTWR_14WINDOWSIZE_PIXELS_0TEXTAREA) {
	    /* report text area size in pixels */
	    *rp1 = maxROWS * character_height;
	    *rp2 = maxCOLS * character_width;
	} else if (p2 && *p2 == XTWR_14WINDOWSIZE_PIXELS_2WINDOW) {
	    /* report window size in pixels */
	    *rp1 = window_height;
	    *rp2 = window_width;
	}
	break;
    case XTWR_15SCREENSIZE_PIXELS: /* report screen size in pixels */
	*rp1 = screen_height;
	*rp2 = screen_width;
	break;
    case XTWR_16CHARACTERSIZE_PIXELS: /* report character cell size in pixels */
	*rp1 = character_height;
	*rp2 = character_width;
	break;
    case XTWR_19SCREENSIZE_PIXELS: /* report screen size in characters */
	*rp1 = screen_height? (character_height / screen_height): 0;
	*rp2 = screen_width? (character_width / screen_width): 0;
	break;
    case XTWR_21WINDOWLABEL: /* report window title */
	*rtext = window_title? NewString(window_title): NULL;
	break;
    default:
	if (p1 >= 24) {
	    /* set screen size to n lines */
	    live_change_oversize(COLS, p1);
	}
	break;
    }
}

/* Query() callbacks. */

void
get_screen_pixels(unsigned *height, unsigned *width)
{
    *height = screen_height;
    *width = screen_width;
}

void
get_window_pixels(unsigned *height, unsigned *width)
{
    *height = window_height;
    *width = window_width;
}

void
get_character_pixels(unsigned *height, unsigned *width)
{
    *height = character_height;
    *width = character_width;
}

void
get_window_location(int *x, int *y)
{
    *x = location_x;
    *y = location_y;
}

window_state_t
get_window_state(void)
{
    return window_state;
}
