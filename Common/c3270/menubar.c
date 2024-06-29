/*
 * Copyright (c) 2010-2024 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	menubar.c
 *		A curses-based 3270 Terminal Emulator
 *		Menu system
 */

#include "globals.h"

#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actions.h"
#include "c3270.h"
#include "ckeypad.h"
#include "cmenubar.h"
#include "codepage.h"
#include "cscreen.h"
#include "ctlrc.h"
#include "unicodec.h"	/* must precede ft.h */
#include "ft.h"
#include "glue.h"
#include "host.h"
#include "keymap.h"
#include "kybd.h"
#include "menubar.h"
#include "names.h"
#include "popups.h"
#include "screen.h"
#include "task.h"
#include "toggles.h"
#include "trace.h"
#include "screentrace.h"
#include "utils.h"

#if defined(_WIN32) /*[*/
# include "wc3270.h"
#endif /*]*/

#if !defined(_WIN32) /*[*/
# if defined(HAVE_NCURSESW_NCURSES_H) /*[*/
#  include <ncursesw/ncurses.h>
# elif defined(HAVE_NCURSES_NCURSES_H) /*][*/
#  include <ncurses/ncurses.h>
# elif defined(HAVE_NCURSES_H) /*][*/
#  include <ncurses.h>
# else /*][*/
#  include <curses.h>
# endif /*]*/
#endif /*]*/

/*
 * The menus look like this:
 *
 *   File       Options   Keymap 
 * +----------+
 * | Fubar    |
 * |*Grill    |
 * | Woohoo   |
 * +----------+
 */

#define MENU_WIDTH 10

typedef void (*menu_callback)(void *);

typedef struct cmenu_item {
    struct cmenu_item *next;	/* Next item in list. */
    struct cmenu_item *prev;	/* Next item in list. */
    char *label;		/* What to display. */
    bool enabled;
    menu_callback action;	/* What to do. */
    void *param;		/* Callback parameter. */
    struct cmenu *cmenu;	/* Backpointer to cmenu. */
} cmenu_item_t;

typedef struct cmenu {
    struct cmenu *next;		/* Next menu in list. */
    struct cmenu *prev;
    char *title;		/* Menu title. */
    int offset;
    size_t width;
    menu_callback callback;
    void *param;
    cmenu_item_t *items;	/* Items. */
    cmenu_item_t *last;		/* Last item. */
} cmenu_t;

cmenu_t *menus;			/* List of menus. */
cmenu_t *menu_last;		/* Last menu. */
int current_offset;		/* Offset to next menu. */
cmenu_t *current_menu;		/* Currently displayed menu. */
cmenu_item_t*current_item;	/* Currently highlighted item. */

menu_callback after_callback;
void *after_param;

ucs4_t menu_screen[MODEL_2_COLS * MODEL_2_ROWS];
unsigned char menu_rv[MODEL_2_COLS * MODEL_2_ROWS];
unsigned char menu_acs[MODEL_2_COLS * MODEL_2_ROWS];
ucs4_t menu_topline[MODEL_2_COLS];
unsigned menu_is_up = 0;

/* Add a menu. */
cmenu_t *
add_menu(char *title)
{
    cmenu_t *c;

    c = (cmenu_t *)Malloc(sizeof(cmenu_t) + strlen(title) + 1);
    c->title = (char *)(c + 1);
    c->offset = current_offset;
    c->width = strlen(title) + 2;
    current_offset += MENU_WIDTH;
    strcpy(c->title, title);
    c->callback = NULL;
    c->param = NULL;
    c->items = NULL;
    c->last = NULL;

    c->prev = menu_last;
    c->next = NULL;
    if (menu_last != NULL) {
	menu_last->next = c;
    } else {
	menus = c;
    }
    menu_last = c;
    return c;
}

/* Remove a menu. */
static void
remove_menu(cmenu_t *cmenu)
{
    cmenu_t *c;
    cmenu_t *prev = NULL;
    cmenu_item_t *i;

    if (cmenu == NULL) {
	return;
    }

    /* Find the menu. */
    for (c = menus; c != NULL; c = c->next) {
	if (c == cmenu) {
	    break;
	}
	prev = c;
    }
    if (c == NULL) {
	return;
    }

    /* Free its items. */
    while ((i = cmenu->items) != NULL) {
	Free(i->label);
	cmenu->items = i->next;
	Free(i);
    }

    /* Restore the linked list. */
    if (prev != NULL) {
	prev->next = cmenu->next;
    }
    if (cmenu->next == NULL) {
	menu_last = prev;
    }

    /* Free it. */
    Free(cmenu);

    /* Correct the offsets. */
    current_offset = 0;
    for (c = menus; c != NULL; c = c->next) {
	c->offset = current_offset;
	current_offset += MENU_WIDTH;
    }
}

/* Add an item to a menu. */
cmenu_item_t *
add_item(cmenu_t *cmenu, char *label, void (*action)(void *), void *param)
{
    cmenu_item_t *i;

    i = (cmenu_item_t *)Malloc(sizeof(cmenu_item_t));
    i->label = Malloc(strlen(label) + 1);
    strcpy(i->label, label);
    i->action = action;
    i->param = param;
    i->enabled = true;
    i->next = NULL;
    i->prev = cmenu->last;
    i->cmenu = cmenu;
    if (cmenu->last) {
	cmenu->last->next = i;
    } else {
	cmenu->items = i;
    }
    cmenu->last = i;
    if (strlen(label) + 2 > cmenu->width) {
	cmenu->width = strlen(label) + 2;
    }
    return i;
}

void
enable_item(cmenu_item_t *i, bool enabled)
{
    i->enabled = enabled;
    /* TODO: Do more here. */
}

void
rename_item(cmenu_item_t *i, char *name)
{
    Replace(i->label, NewString(name));
    if (strlen(name) + 2 > i->cmenu->width) {
	i->cmenu->width = strlen(name) + 2;
    }
}

void
set_callback(cmenu_t *cmenu, void (*callback)(void *), void *param)
{
    cmenu->callback = callback;
    cmenu->param = param;
}

void
basic_menu_init(void)
{
    memset(menu_screen, 0, sizeof(ucs4_t) * MODEL_2_COLS * MODEL_2_ROWS);
    memset(menu_rv, 0, sizeof(unsigned char) * MODEL_2_COLS * MODEL_2_ROWS);
    current_menu = NULL;
    current_item = NULL;
    menu_is_up &= ~MENU_IS_UP;
    pop_up_keypad(false);
    screen_changed = true;
}

/* Undraw a menu. */
void
undraw_menu(cmenu_t *cmenu)
{
    int row, col;
    cmenu_item_t *i;

    screen_changed = true;

    /* Unhighlight the menu title. */
    for (col = cmenu->offset; col < cmenu->offset + MENU_WIDTH; col++) {
	menu_rv[(0 * MODEL_2_COLS) + col] = false;
    }

    if (!cmenu->items) {
	return;
    }

    /* Erase the top border. */
    row = 1;
    for (col = cmenu->offset;
	 (size_t)col < cmenu->offset + cmenu->width;
	 col++) {
	menu_screen[(row * MODEL_2_COLS) + col] = 0;
    }

    /* Erase the menu items. */
    row = 2;
    for (i = cmenu->items; i != NULL; i = i->next) {
	col = cmenu->offset;
	while ((size_t)col < cmenu->offset + cmenu->width + 2) {
	    menu_screen[(row * MODEL_2_COLS) + col] = 0;
	    menu_rv[(row * MODEL_2_COLS) + col] = false;
	    col++;
	}
	row++;
    }

    /* Erase the bottom border. */
    for (col = cmenu->offset;
	 (size_t)col < cmenu->offset + cmenu->width;
	 col++) {
	menu_screen[(row * MODEL_2_COLS) +col] = 0;
    }
}

/* Draw a menu. */
void
draw_menu(cmenu_t *cmenu)
{
    int row, col;
    cmenu_item_t *i;

    screen_changed = true;

    /* Highlight the title. */
    row = 0;
    for (col = cmenu->offset;
	 col < cmenu->offset + MENU_WIDTH - 1;
	 col++) {
	menu_rv[(row * MODEL_2_COLS) + col] = true;
    }
    if (!cmenu->items) {
	return;
    }

    /* Draw the top border. */
    row = 1;
    for (col = cmenu->offset;
	 (size_t)col < cmenu->offset + cmenu->width;
	 col++) {
	int ix = (row * MODEL_2_COLS) + col;

	if (col == cmenu->offset) {
	    map_acs('l', &menu_screen[ix], &menu_acs[ix]);
	} else if ((size_t)col < cmenu->offset + cmenu->width - 1) {
	    map_acs('q', &menu_screen[ix], &menu_acs[ix]);
	} else {
	    map_acs('k', &menu_screen[ix], &menu_acs[ix]);
	}
    }

    /* Draw the items. */
    row = 2;
    for (i = cmenu->items; i != NULL; i = i->next) {
	char *d;

	col = cmenu->offset;
	map_acs('x', &menu_screen[(row * MODEL_2_COLS) + col],
		&menu_acs[(row * MODEL_2_COLS) + col]);
	col++; /* start at column one */
	for (d = i->label; *d; d++) {
	    menu_screen[(row * MODEL_2_COLS) + col] = *d & 0xff;
	    menu_rv[(row * MODEL_2_COLS) + col] = (i == current_item);
	    col++;
	}
	while ((size_t)col < cmenu->offset + cmenu->width - 1) {
	    menu_screen[(row * MODEL_2_COLS) + col] = ' ';
	    menu_rv[(row * MODEL_2_COLS) + col] = (i == current_item);
	    col++;
	}
	map_acs('x', &menu_screen[(row * MODEL_2_COLS) + col],
		&menu_acs[(row * MODEL_2_COLS) + col]);
	row++;
    }

    /* Draw the bottom border. */
    for (col = cmenu->offset;
	 (size_t)col < cmenu->offset + cmenu->width;
	 col++) {
	int ix = (row * MODEL_2_COLS) + col;

	if (col == cmenu->offset) {
	    map_acs('m', &menu_screen[ix], &menu_acs[ix]);
	} else if ((size_t)col < cmenu->offset + cmenu->width - 1) {
	    map_acs('q', &menu_screen[ix], &menu_acs[ix]);
	} else {
	    map_acs('j', &menu_screen[ix], &menu_acs[ix]);
	}
    }
}

#if defined(NCURSES_MOUSE_VERSION) || defined(_WIN32) /*[*/
/*
 * Find a mouse click in the menu hierarchy and act on it.
 *
 * Returns true if the coordinates are on a menu somewhere, false otherwise.
 */
bool
find_mouse(int x, int y)
{
    cmenu_t *c = NULL;
    cmenu_item_t *i = NULL;
    int row;

    /* It's gotta be in the ballpark. */
    if (x >= MODEL_2_COLS ||
	y >= MODEL_2_ROWS ||
	menu_screen[(y * MODEL_2_COLS) + x] == 0) {
	return false;
    }

    if (y == 0) {
	/* Menu title. */
	for (c = menus; c != NULL; c = c->next) {
	    if (x >= c->offset && x < c->offset + MENU_WIDTH) {
		if (c == current_menu) {
		    return false;
		}
		if (c->items == NULL) {
		    goto selected;
		}
		if (c == current_menu) {
		    return true;
		}
		undraw_menu(current_menu);
		current_menu = c;
		current_item = current_menu->items;
		while (current_item && !current_item->enabled) {
		    current_item = current_item->next;
		}
		draw_menu(current_menu);
		return true;
	    }
	}
	return false;
    }

    if (x < current_menu->offset ||
	(size_t)x > current_menu->offset + current_menu->width) {
	return false;
    }
    if (y == 1) { /* top border */
	return true;
    }
    row = 2;
    for (i = current_menu->items; i != NULL; i = i->next) {
	if (y == row) {
	    break;
	}
	row++;
    }
    if (i != NULL) {
	if (i->enabled) {
	    goto selected;
	} else {
	    return true;
	}
    }
    if (y == row + 1) {
	return true;
    }


    return false;

selected:
    if (i == NULL) {
	if (c->callback) {
	    (*c->callback)(c->param);
	}
    } else {
	(*i->action)(i->param);
    }
    basic_menu_init();
    if (after_callback != NULL) {
	(*after_callback)(after_param);
	after_callback = NULL;
	after_param = NULL;
    }
    return true;
}
#endif /*]*/

#if defined(_WIN32) /*[*/
void
menu_click(int x, int y)
{
    if (menu_is_up & KEYPAD_IS_UP) {
	keypad_click(x, y);
	return;
    }
    if (!find_mouse(x, y)) {
	basic_menu_init();
    }
}
#endif /*]*/

/*
 * Handle a key event for a menu.
 * With ncurses, this can include mouse events.
 */
void
menu_key(menu_key_t k, ucs4_t u)
{
    cmenu_item_t *i;
    bool selected = false;

    if (menu_is_up & KEYPAD_IS_UP) {
	keypad_key(k, u);
	return;
    }

    switch (k) {

#if defined(NCURSES_MOUSE_VERSION) /*[*/
    case MK_MOUSE: {
	MEVENT m;

	if (getmouse(&m) != OK) {
	    return;
	}
	if (!(m.bstate & (BUTTON1_PRESSED | BUTTON1_RELEASED))) {
	    return;
	}

	/* See if it lands somewhere we can figure out. */
	if (!find_mouse(m.x, m.y)) {
	    basic_menu_init();
	}
	break;
	}
#endif /*]*/

    case MK_UP:
	i = current_item;
	if (current_item && current_item->prev) {
	    current_item = current_item->prev;
	    while (current_item && !current_item->enabled) {
		    current_item = current_item->prev;
	    }
	    if (current_item == NULL) {
		current_item = i;
	    } else {
		draw_menu(current_menu);
	    }
	}
	break;

    case MK_DOWN:
	i = current_item;
	if (current_item && current_item->next) {
	    current_item = current_item->next;
	    while (current_item && !current_item->enabled) {
		current_item = current_item->next;
	    }
	    if (current_item == NULL) {
		current_item = i;
	    } else {
		draw_menu(current_menu);
	    }
	}
	break;

    case MK_LEFT:
	undraw_menu(current_menu);
	if (current_menu->prev) {
	    current_menu = current_menu->prev;
	} else {
	    current_menu = menus;
	}
	current_item = current_menu->items;
	while (current_item && !current_item->enabled) {
	    current_item = current_item->next;
	}
	draw_menu(current_menu);
	break;

    case MK_RIGHT:
	undraw_menu(current_menu);
	if (current_menu->next) {
	    current_menu = current_menu->next;
	} else {
	    current_menu = menus;
	}
	current_item = current_menu->items;
	while (current_item && !current_item->enabled) {
	    current_item = current_item->next;
	}
	draw_menu(current_menu);
	break;

    case MK_HOME:
	if (current_item) {
	    current_item = current_menu->items;
	    while (current_item && !current_item->enabled) {
		current_item = current_item->next;
	    }
	    draw_menu(current_menu);
	}
	break;

    case MK_END:
	i = current_item;
	while (current_item) {
	    current_item = current_item->next;
	    if (current_item && current_item->enabled) {
		i = current_item;
	    }
	}
	current_item = i;
	draw_menu(current_menu);
	break;

    case MK_ENTER:
	selected = true;
	break;

    case MK_NONE:
	switch (u) {
	case '\r':
	case '\n':
	    selected = true;
	    break;
	default:
	    basic_menu_init();
	}
	break;

	default:
    case MK_OTHER:
	basic_menu_init();
	break;
    }

    if (selected) {
	if (current_item) {
	    (*current_item->action)(current_item->param);
	} else if (!current_menu->items) {
	    (*current_menu->callback)(current_menu->param);
	}
	basic_menu_init();
	if (after_callback != NULL) {
	    (*after_callback)(after_param);
	    after_callback = NULL;
	    after_param = NULL;
	}
    }

    screen_changed = true;
}

/* Report a character back to the screen drawing logic. */
bool
menu_char(int row, int col, bool persistent, ucs4_t *u,
	bool *highlighted, unsigned char *acs)
{
    if (menu_is_up & KEYPAD_IS_UP) {
	return keypad_char(row, col, u, highlighted, acs);
    } else if (col >= MODEL_2_COLS) {
	return false;
    } else if ((menu_is_up & MENU_IS_UP) &&
	     row < MODEL_2_ROWS &&
	     col < MODEL_2_COLS &&
	 menu_screen[(row * MODEL_2_COLS) + col]) {
	*u = menu_screen[(row * MODEL_2_COLS) + col];
	*highlighted = menu_rv[(row * MODEL_2_COLS) + col];
	*acs = menu_acs[(row * MODEL_2_COLS) + col];
	return true;
    } else if (persistent && row == 0 && menu_topline[col]) {
	*u = menu_topline[col];
	*highlighted = 0;
	return true;
    } else {
	*u = 0;
	*highlighted = false;
	return false;
    }
}

/* Report where to land the cursor when a menu is up. */
void
menu_cursor(int *row, int *col)
{
    if (menu_is_up & KEYPAD_IS_UP) {
	keypad_cursor(row, col);
	return;
    }

    if (menu_is_up & MENU_IS_UP) {
	*row = 0;
	*col = current_menu->offset;
    } else {
	*row = 0;
	*col = 0;
    }
}

/* Functions specific to c3270. */

static void
fm_copyright(void *ignored _is_unused)
{
    push_macro(AnEscape "(\"" AnShow "(" KwCopyright ")\")");
}

static void
fm_status(void *ignored _is_unused)
{
    push_macro(AnEscape "(\"" AnShow "(" KwStatus ")\")");
}

static void
fm_about(void *ignored _is_unused)
{
    push_macro(AnEscape "(\"" AnShow "(" KwAbout ")\")");
}

static void
fm_prompt(void *ignored _is_unused)
{
    push_macro(AnEscape "()");
}

static void
fm_print(void *ignored _is_unused)
{
    push_macro(AnPrintText "()");
}

static void
fm_xfer(void *ignored _is_unused)
{
    if (ft_state == FT_NONE) {
	push_macro(AnEscape "(\"" AnTransfer "()\")");
    } else {
	push_macro(AnTransfer "(" KwCancel ")");
    }
}

static void
fm_trace(void *ignored _is_unused)
{
    if (toggled(TRACING)) {
	push_macro(AnTrace "(" KwOff ")");
    } else {
	push_macro(AnTrace "(" KwOn ")");
    }
}

static void
fm_screentrace(void *ignored _is_unused)
{
    if (toggled(SCREEN_TRACE)) {
	push_macro(AnScreenTrace "(" KwOff "," KwInfo ")");
    } else {
	push_macro(AnScreenTrace "(" KwOn "," KwInfo ")");
    }
}

static void
fm_screentrace_printer(void *ignored _is_unused)
{
    if (toggled(SCREEN_TRACE)) {
	push_macro(AnScreenTrace "(" KwOff "," KwInfo ")");
    } else {
	push_macro(AnScreenTrace "(" KwOn "," KwInfo "," KwPrinter ")");
    }
}

static void
fm_save_input(void *ignored _is_unused)
{
    push_macro(AnSaveInput "()");
}

static void
fm_restore_input(void *ignored _is_unused)
{
    push_macro(AnRestoreInput "()");
}

static void
fm_keymap(void *ignored _is_unused)
{
    push_macro(AnEscape "(\"" AnShow "(" KwKeymap ")\")");
}

#if defined(HAVE_START) /*[*/
static void
fm_help(void *ignored _is_unused)
{
    start_html_help();
}
#endif /*]*/

#if defined(_WIN32) /*[*/
static void
fm_wizard(void *session)
{
    start_wizard((char *)session);
}
#endif /*]*/

static void
fm_reenable(void *ignored _is_unused)
{
    push_macro(AnKeyboardDisable "(" KwForceEnable ")");
}

static void
fm_disconnect(void *ignored _is_unused)
{
    push_macro(AnDisconnect "()");
}

static void
fm_quit(void *ignored _is_unused)
{
    push_macro(AnQuit "()");
}

/* File menu. */
typedef enum {
    FM_COPYRIGHT,
    FM_STATUS,
    FM_ABOUT,
    FM_PROMPT,
    FM_PRINT,
    FM_XFER,
    FM_TRACE,
    FM_SCREENTRACE,
    FM_SCREENTRACE_PRINTER,
    FM_SAVE_INPUT,
    FM_RESTORE_INPUT,
    FM_KEYMAP,
#if defined(HAVE_START) /*[*/
    FM_HELP,
#endif /*]*/
#if defined(_WIN32) /*[*/
    FM_WIZARD,
    FM_WIZARD_SESS,
#endif /*]*/
    FM_REENABLE,
    FM_DISC,
    FM_QUIT,
    FM_COUNT
} file_menu_enum;
cmenu_item_t *file_menu_items[FM_COUNT];
char *file_menu_names[FM_COUNT] = {
    "Copyright",
    "Status",
#if !defined(_WIN32) /*[*/
    "About c3270",
    "c3270> Prompt",
#else /*][*/
    "About wc3270",
    "wc3270> Prompt",
#endif /*]*/
    "Print Screen",
    "File Transfer",
    "Enable Tracing",
    "Save Screen Images in File",
    "Save Screen Images to Printer",
    "Save Input Fields",
    "Restore Input Fields",
    "Display Keymap",
#if defined(HAVE_START) /*[*/
    "Help",
#endif /*]*/
#if defined(_WIN32) /*[*/
    "Session Wizard",
    "Edit Session",
#endif /*]*/
    "Re-enable Keyboard",
    "Disconnect",
    "Quit"
};
menu_callback file_menu_actions[FM_COUNT] = {
    fm_copyright,
    fm_status,
    fm_about,
    fm_prompt,
    fm_print,
    fm_xfer,
    fm_trace,
    fm_screentrace,
    fm_screentrace_printer,
    fm_save_input,
    fm_restore_input,
    fm_keymap,
#if defined(HAVE_START) /*[*/
    fm_help,
#endif /*]*/
#if defined(_WIN32) /*[*/
    fm_wizard,
    fm_wizard,
#endif /*]*/
    fm_reenable,
    fm_disconnect,
    fm_quit
};

static file_menu_enum fm_insecure[] = {
    FM_PROMPT,
    FM_PRINT,
    FM_XFER,
    FM_TRACE,
    FM_SCREENTRACE,
    FM_SCREENTRACE_PRINTER,
    FM_SAVE_INPUT,
    FM_RESTORE_INPUT,
#if defined(HAVE_START) /*[*/
    FM_HELP,
#endif /*]*/
#if defined(_WIN32) /*[*/
    FM_WIZARD,
    FM_WIZARD_SESS,
#endif /*]*/
};
#define NUM_FM_INSECURE	((int)(sizeof(fm_insecure) / sizeof(file_menu_enum)))

/* Options menu. */
typedef enum {
    OM_MONOCASE,
    OM_BLANKFILL,
    OM_TIMING,
    OM_CROSSHAIR,
    OM_UNDERSCORE,
#if defined(WC3270) /*[*/
    OM_CURSOR_BLINK,
    OM_MARGINED_PASTE,
    OM_OVERLAY_PASTE,
#endif /*]*/
    OM_VISIBLE_CONTROL,
    OM_TYPEAHEAD,
    OM_ALWAYS_INSERT,
    OM_UNDERSCORE_BLANK_FILL,
    OM_COUNT
} options_menu_enum;
cmenu_item_t *options_menu_items[OM_COUNT];
toggle_index_t option_index[OM_COUNT] = {
    MONOCASE,
    BLANK_FILL,
    SHOW_TIMING,
    CROSSHAIR,
    UNDERSCORE,
#if defined(WC3270) /*[*/
    CURSOR_BLINK,
    MARGINED_PASTE,
    OVERLAY_PASTE,
#endif /*]*/
    VISIBLE_CONTROL,
    TYPEAHEAD,
    ALWAYS_INSERT,
    UNDERSCORE_BLANK_FILL,
};
char *option_names[OM_COUNT] = {
    "Monocase",
    "Blank Fill",
    "Show Timing",
    "Crosshair Cursor",
    "Underscore Mode",
#if defined(WC3270) /*[*/
    "Cursor Blink",
    "Margined Paste",
    "Overlay Paste",
#endif /*]*/
    "Visible Control",
    "Typeahead",
    "Default Insert Mode",
    "Underscore Blank Fill",
};

cmenu_t *file_menu;
cmenu_t *options_menu;
cmenu_t *keypad_menu;
cmenu_t *macros_menu;

static struct macro_def **macro_save;
static int n_ms;

static void
toggle_option(void *param)
{
    int index = *(int *)param;

    do_toggle(index);
}

static void
really_popup_keypad(void *ignored _is_unused)
{
    pop_up_keypad(true);
}

static void
popup_keypad(void *ignored _is_unused)
{
    after_callback = really_popup_keypad;
    after_param = NULL;
}

/* Run an item from the Macros menu. */
static void
menu_run_macro(void *param)
{
    struct macro_def *m = (struct macro_def *)param;

    push_macro(m->action);
}

/* Draw the top line (the menu bar). */
static void
draw_topline(void)
{
    int col, next_col;
    cmenu_t *c;

    memset(menu_topline, 0, sizeof(menu_topline));
    col = 0;
    next_col = MENU_WIDTH;
    for (c = menus; c != NULL; c = c->next) {
	char *d;

	for (d = c->title; *d; d++) {
	    menu_topline[col] = *d & 0xff;
	    col++;
	}
	while (col < next_col) {
	    menu_topline[col] = ' ';
	    col++;
	}
	next_col += MENU_WIDTH;
    }
}

void
menu_init(void)
{
    file_menu_enum f;
    options_menu_enum o;

    basic_menu_init();

    file_menu = add_menu("File");
    for (f = 0; f < FM_COUNT; f++) {
	if (appres.secure) {
	    int fi;
	    bool secure = true;

	    for (fi = 0; fi < NUM_FM_INSECURE; fi++) {
		if (f == fm_insecure[fi]) {
		    secure = false;
		    break;
		}
	    }
	    if (!secure) {
		continue;
	    }
	}
#if defined(WC3270) /*[*/
	if (f == FM_WIZARD_SESS && profile_path == NULL) {
	    continue;
	}
	if (f == FM_WIZARD_SESS) {
	    char *text;

	    text = Asprintf("Edit Session %s", profile_name);

	    file_menu_items[f] = add_item(file_menu, text,
		    file_menu_actions[f], profile_path);
	} else
#endif /*]*/
	{
	    file_menu_items[f] = add_item(file_menu, file_menu_names[f],
		    file_menu_actions[f], NULL);
	}
    }
    options_menu = add_menu("Options");
    for (o = 0; o < OM_COUNT; o++) {
	char *name;

	name = Asprintf("%s %s",
		toggled(option_index[o])? "Disable": "Enable",
		option_names[o]);
	options_menu_items[o] = add_item(options_menu, name, toggle_option,
		&option_index[o]);
	Free(name);
    }
    keypad_menu = add_menu("Keypad");
    set_callback(keypad_menu, popup_keypad, NULL);

    /* Draw the menu names on the top line. */
    draw_topline();
}

/* Connect state change callback for the menu bar. */
static void
menubar_connect(bool connected)
{
    static bool created_menu = false;

    if (connected) {
	if (macro_defs != NULL && !created_menu) {
	    struct macro_def *m;

	    /* Create the macros menu. */
	    macros_menu = add_menu("Macros");
	    n_ms = 0;
	    for (m = macro_defs; m != NULL; m = m->next) {
		struct macro_def *mm = (struct macro_def *)
		    Malloc(sizeof(struct macro_def) + strlen(m->name) + 1 +
			    strlen(m->action) + 1);

		/*
		 * Save a copy of the macro definition, since it could change
		 * at any time.
		 */
		mm->name = (char *)(mm + 1);
		strcpy(mm->name, m->name);
		mm->action = mm->name + strlen(mm->name) + 1;
		strcpy(mm->action, m->action);
		add_item(macros_menu, m->name, menu_run_macro, mm);

		macro_save = (struct macro_def **)Realloc(macro_save,
			(n_ms + 1) * sizeof(struct macro_def *));
		macro_save[n_ms++] = mm;
	    }

	    /* Re-create the menu bar and force a screen redraw. */
	    draw_topline();
	    screen_changed = true;
	    created_menu = true;
	}
    } else {
	int i;

	/* Free the saved macro definitions. */
	for (i = 0; i < n_ms; i++) {
	    Free(macro_save[i]);
	}
	Replace(macro_save, NULL);
	n_ms = 0;

	/*
	 * Remove the macros menu, re-draw the menu bar and force a screen
	 * redraw.
	 */
	remove_menu(macros_menu);
	macros_menu = NULL;
	draw_topline();
	screen_changed = true;
	created_menu = false;
    }
}

void
menubar_retoggle(toggle_index_t ix)
{
    int j;
    char *s;

    if (!appres.interactive.menubar) {
	return;
    }

    /* Search the options menu. */
    for (j = 0; j < OM_COUNT; j++) {
	if (option_index[j] == ix) {
	    break;
	}
    }
    if (j < OM_COUNT) {
	s = Asprintf("%sable %s", toggled(ix)? "Dis": "En", option_names[j]);
	rename_item(options_menu_items[j], s);
	Free(s);
	return;
    }
    if (ix == TRACING && !appres.secure) {
	s = Asprintf("%sable Tracing", (toggled(TRACING))? "Dis": "En");
	rename_item(file_menu_items[FM_TRACE], s);
	Free(s);
    }
    if (ix == SCREEN_TRACE) {
	if (toggled(SCREEN_TRACE)) {
	    switch (trace_get_screentrace_target()) {
	    case TSS_FILE:
		rename_item(file_menu_items[FM_SCREENTRACE],
			"Stop Saving Screen Images");
		enable_item(file_menu_items[FM_SCREENTRACE_PRINTER], false);
		break;
	    case TSS_PRINTER:
		enable_item(file_menu_items[FM_SCREENTRACE], false);
		rename_item(file_menu_items[FM_SCREENTRACE_PRINTER],
			"Stop Saving Screen Images");
		break;
	    }
	} else {
	    rename_item(file_menu_items[FM_SCREENTRACE],
		    "Save Screen Images in File");
	    enable_item(file_menu_items[FM_SCREENTRACE], true);
	    rename_item(file_menu_items[FM_SCREENTRACE_PRINTER],
		    "Save Screen Images to Printer");
	    enable_item(file_menu_items[FM_SCREENTRACE_PRINTER], true);
	}
    }
}

/* Pop up a menu. */
void
popup_menu(int x, int click)
{
    cmenu_t *cmenu;
    cmenu_t *c;
    int row, col;
    int next_col;

    if (!appres.interactive.menubar) {
	return;
    }

    /* Find which menu to start with. */
    for (cmenu = menus; cmenu != NULL; cmenu = cmenu->next) {
	if (x >= cmenu->offset && x < cmenu->offset + MENU_WIDTH) {
	    break;
	}
    }
    if (cmenu == NULL) {
	return;
    }

    /* If it was a direct click, see if the menu has a direct callback. */
    if (click && cmenu->callback != NULL) {
	(*cmenu->callback)(cmenu->param);
	if (after_callback != NULL) {
	    (*after_callback)(after_param);
	    after_callback = NULL;
	    after_param = NULL;
	}
	return;
    }

    /* Start with nothing. */
    basic_menu_init();

    /* Switch the name of the File Transfer menu. */
    if (!appres.secure) {
	rename_item(file_menu_items[FM_XFER],
		(ft_state == FT_NONE)? "File Transfer": "Cancel File Transfer");
    }

    /*
     * Draw the menu names on the top line, with the active one highlighted.
     */
    row = 0;
    col = 0;
    next_col = MENU_WIDTH;
    for (c = menus; c != NULL; c = c->next) {
	char *d;

	for (d = c->title; *d; d++) {
	    menu_screen[(row * MODEL_2_COLS) + col] = *d & 0xff;
	    menu_rv[(row * MODEL_2_COLS) + col] = (c == cmenu);
	    col++;
	}
	while (col < next_col) {
	    menu_screen[(row * MODEL_2_COLS) + col] = ' ';
	    col++;
	}
	next_col += MENU_WIDTH;
    }
    current_menu = cmenu;

    /* Draw the current menu, with the active item highlighted. */
    if (cmenu->items) {
	current_item = cmenu->items;
	while (current_item && !current_item->enabled) {
	    current_item = current_item->next;
	}
	draw_menu(cmenu);
    } else {
	current_item = NULL;
    }

    /* We're up. */
    menu_is_up |= MENU_IS_UP;
}

/*
 * Utility function to map ACS codes (l, m, j, etc.) to the right kind of
 * line-drawing character.
 */
void
map_acs(unsigned char c, ucs4_t *u, unsigned char *is_acs)
{
#if defined(CURSES_WIDE) || defined(_WIN32) /*[*/
    /*
     * If we have wide curses thus can do ACS, or if we are on Windows,
     * then do ASCII art only if the user requests it.
     *
     * Otherwise (no wide curses, no Windows), ASCII art is all we can do.
     */
    if (appres.c3270.ascii_box_draw)
#endif /*]*/
    {
	/* ASCII art. */
	*is_acs = 0;
	switch (c) {
	case 'l':
	case 'm':
	case 'k':
	case 'j':
	case 't':
	case 'u':
	case 'v':
	case 'w':
	case 'n':
	    *u = '+';
	    break;
	case 'q':
	    *u = '-';
	    break;
	case 'x':
	    *u = '|';
	    break;
	case 's':
	    *u = ' ';
	    break;
	default:
	    *u = '?';
	    break;
	}
	return;
    }
#if defined(CURSES_WIDE) /*[*/
    else if (appres.c3270.acs) {
	/* ncurses ACS. */
	*is_acs = 1;
	switch (c) {
	case 'l':
	    *u = ACS_ULCORNER;
	    break;
	case 'm':
	    *u = ACS_LLCORNER;
	    break;
	case 'k':
	    *u = ACS_URCORNER;
	    break;
	case 'j':
	    *u = ACS_LRCORNER;
	    break;
	case 't':
	    *u = ACS_LTEE;
	    break;
	case 'u':
	    *u = ACS_RTEE;
	    break;
	case 'v':
	    *u = ACS_BTEE;
	    break;
	case 'w':
	    *u = ACS_TTEE;
	    break;
	case 'q':
	    *u = ACS_HLINE;
	    break;
	case 'x':
	    *u = ACS_VLINE;
	    break;
	case 'n':
	    *u = ACS_PLUS;
	    break;
	case 's':
	    *u = ' ';
	    *is_acs = 0;
	    break;
	default:
	    *u = '?';
	    *is_acs = 0;
	    break;
	}
    }
#endif /*]*/
#if defined(CURSES_WIDE) || defined(_WIN32) /*[*/
   else {
	/* Unicode. */
	*is_acs = 0;
	switch (c) {
	case 'l':
	    *u = 0x250c;
	    break;
	case 'm':
	    *u = 0x2514;
	    break;
	case 'k':
	    *u = 0x2510;
	    break;
	case 'j':
	    *u = 0x2518;
	    break;
	case 't':
	    *u = 0x251c;
	    break;
	case 'u':
	    *u = 0x2524;
	    break;
	case 'v':
	    *u = 0x2534;
	    break;
	case 'w':
	    *u = 0x252c;
	    break;
	case 'q':
	    *u = 0x2500;
	    break;
	case 'x':
	    *u = 0x2502;
	    break;
	case 'n':
	    *u = 0x253c;
	    break;
	case 's':
	    *u = ' ';
	    break;
	default:
	    *u = '?';
	break;
	}
   }
#endif /*]*/
}

bool
Menu_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnMenu, ia, argc, argv);
    if (check_argc(AnMenu, argc, 0, 0) < 0) {
	return false;
    }

    popup_menu(0, false);
    return true;
}

void
menubar_as_set(bool sensitive _is_unused)
{
    /* Do nothing, there is no Abort Script. */
}

/**
 * Menu bar module registration.
 */
void
menubar_register(void)
{
    static action_table_t menubar_actions[] = {
	{ AnMenu,	Menu_action,	ACTION_KE }
    };

    /* Register for events. */
    register_schange_ordered(ST_CONNECT, menubar_connect, ORDER_LAST);

    /* Register our actions. */
    register_actions(menubar_actions, array_count(menubar_actions));
}
