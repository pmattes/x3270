/*
 * Copyright (c) 2009, Paul Mattes.
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
 *	cmenu.c
 *		A curses-based 3270 Terminal Emulator
 *		Menu system
 */

#include "globals.h"
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "gluec.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "screenc.h"
#include "togglesc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"
#include "xioc.h"

#include "menubarc.h"

#include "ckeypadc.h"

#if defined(HAVE_NCURSESW_NCURSES_H) /*[*/
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H) /*][*/
#include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_H) /*][*/
#include <ncurses.h>
#else /*][*/
#include <curses.h>
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
    Boolean enabled;
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
Boolean menu_is_up = False;

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
    if (menu_last != NULL)
	menu_last->next = c;
    else
	menus = c;
    menu_last = c;
    return c;
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
    i->enabled = True;
    i->next = NULL;
    i->prev = cmenu->last;
    i->cmenu = cmenu;
    if (cmenu->last)
	cmenu->last->next = i;
    else
	cmenu->items = i;
    cmenu->last = i;
    if (strlen(label) + 2 > cmenu->width)
	cmenu->width = strlen(label) + 2;
    return i;
}

void
enable_item(cmenu_item_t *i, Boolean enabled)
{
    i->enabled = enabled;
    /* TODO: Do more here. */
}

void
rename_item(cmenu_item_t *i, char *name)
{
    Replace(i->label, NewString(name));
    if (strlen(name) + 2 > i->cmenu->width)
	i->cmenu->width = strlen(name) + 2;
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
    menu_is_up = False;
    pop_up_keypad(False);
}

/* Undraw a menu. */
void
undraw_menu(cmenu_t *cmenu)
{
    int row, col;
    cmenu_item_t *i;

    /* Unhighlight the menu title. */
    for (col = cmenu->offset; col < cmenu->offset + MENU_WIDTH; col++)
	menu_rv[(0 * MODEL_2_COLS) + col] = False;

    if (!cmenu->items)
	    return;

    /* Erase the top border. */
    row = 1;
    for (col = cmenu->offset; (size_t)col < cmenu->offset + cmenu->width;
	    col++)
	menu_screen[(row * MODEL_2_COLS) + col] = 0;

    /* Erase the menu items. */
    row = 2;
    for (i = cmenu->items; i != NULL; i = i->next) {

	col = cmenu->offset;
	while ((size_t)col < cmenu->offset + cmenu->width + 2) {
	    menu_screen[(row * MODEL_2_COLS) + col] = 0;
	    menu_rv[(row * MODEL_2_COLS) + col] = False;
	    col++;
	}
	row++;
    }

    /* Erase the bottom border. */
    for (col = cmenu->offset; (size_t)col < cmenu->offset + cmenu->width;
	    col++)
	menu_screen[(row * MODEL_2_COLS) +col] = 0;
}

/* Draw a menu. */
void
draw_menu(cmenu_t *cmenu)
{
    char *t;
    int row, col;
    cmenu_item_t *i;

    /* Highlight the title. */
    row = 0;
    t = cmenu->title;
    for (col = cmenu->offset; *t++ && col < cmenu->offset + MENU_WIDTH; col++)
	    menu_rv[(row * MODEL_2_COLS) + col] = True;
    if (!cmenu->items)
	    return;

    /* Draw the top border. */
    row = 1;
    for (col = cmenu->offset; (size_t)col < cmenu->offset + cmenu->width;
	    col++) {
	if (col == cmenu->offset)
		menu_screen[(row * MODEL_2_COLS) + col] = ACS_ULCORNER;
	else if ((size_t)col < cmenu->offset + cmenu->width - 1)
		menu_screen[(row * MODEL_2_COLS) + col] = ACS_HLINE;
	else
		menu_screen[(row * MODEL_2_COLS) + col] = ACS_URCORNER;
    }

    /* Draw the items. */
    row = 2;
    for (i = cmenu->items; i != NULL; i = i->next) {
	char *d;

	col = cmenu->offset;
	menu_screen[(row * MODEL_2_COLS) + col] = ACS_VLINE;
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
	menu_screen[(row * MODEL_2_COLS) + col] = ACS_VLINE;
	row++;
    }

    /* Draw the bottom border. */
    for (col = cmenu->offset; (size_t)col < cmenu->offset + cmenu->width;
	    col++) {
	if (col == cmenu->offset)
		menu_screen[(row * MODEL_2_COLS) + col] = ACS_LLCORNER;
	else if ((size_t)col < cmenu->offset + cmenu->width - 1)
		menu_screen[(row * MODEL_2_COLS) + col] = ACS_HLINE;
	else
		menu_screen[(row * MODEL_2_COLS) + col] = ACS_LRCORNER;
    }
}

/* Pop up a menu. */
void
popup_menu(int x)
{
    cmenu_t *cmenu;
    cmenu_t *c;
    int row, col;
    int next_col;

    /* Find which menu to start with. */
    for (cmenu = menus; cmenu != NULL; cmenu = cmenu->next) {
	if (x >= cmenu->offset && x < cmenu->offset + MENU_WIDTH)
	    break;
    }
    if (cmenu == NULL)
	return;

    /* Start with nothing. */
    basic_menu_init();

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
	while (current_item && !current_item->enabled)
	    current_item = current_item->next;
	draw_menu(cmenu);
    } else {
	current_item = NULL;
    }

    /* We're up. */
    menu_is_up = True;
}

#if defined(NCURSES_MOUSE_VERSION) /*[*/
/* Find a mouse click in the menu hierarchy and act on it. */
Boolean
find_mouse(int x, int y)
{
    cmenu_t *c = NULL;
    cmenu_item_t *i = NULL;
    int row;

    /* It's gotta be in the ballpark. */
    if (x >= MODEL_2_COLS || y >= MODEL_2_ROWS ||
	    menu_screen[(y * MODEL_2_COLS) + x] == 0)
	return False;

    if (y == 0) {
	/* Menu title. */
	for (c = menus; c != NULL; c = c->next) {
	    if (x >= c->offset && x < c->offset + MENU_WIDTH) {
		if (c->items == NULL)
		    goto selected;
		if (c == current_menu)
		    return True;
		undraw_menu(current_menu);
		current_menu = c;
		current_item = current_menu->items;
		while (current_item && !current_item->enabled) {
		    current_item = current_item->next;
		}
		draw_menu(current_menu);
		return True;
	    }
	}
	return False;
    }

    if (x < current_menu->offset ||
	    (size_t)x > current_menu->offset + current_menu->width)
	return False;
    if (y == 1) /* top border */
	return True;
    row = 2;
    for (i = current_menu->items; i != NULL; i = i->next) {
	if (y == row)
	    break;
	row++;
    }
    if (i != NULL) {
	if (i->enabled)
	    goto selected;
	else
	    return True;
    }
    if (y == row + 1)
	return True;

    return False;

selected:
    if (i == NULL) {
	if (c->callback)
	    (*c->callback)(c->param);
    } else {
	(*i->action)(i->param);
    }
    basic_menu_init();
    if (after_callback != NULL) {
	(*after_callback)(after_param);
	after_callback = NULL;
	after_param = NULL;
    }
    return True;
}
#endif /*]*/

/* Handle a key event for a menu. */
void
menu_key(int k, ucs4_t u)
{
    cmenu_item_t *i;
    Boolean selected = False;

    if (keypad_is_up) {
	keypad_key(k, u);
	return;
    }

    switch (k) {

#if defined(NCURSES_MOUSE_VERSION) /*[*/
    case KEY_MOUSE: {
	MEVENT m;

	if (getmouse(&m) != OK)
	    return;
	if (!(m.bstate & (BUTTON1_PRESSED || BUTTON1_RELEASED)))
	    return;

	/* See if it lands somewhere we can figure out. */
	if (!find_mouse(m.x, m.y))
	    basic_menu_init();
	break;
    }
#endif /*]*/

    case KEY_UP:
	i = current_item;
	if (current_item && current_item->prev) {
	    current_item = current_item->prev;
	    while (current_item && !current_item->enabled) {
		current_item = current_item->prev;
	    }
	    if (current_item == NULL)
		current_item = i;
	    else
		draw_menu(current_menu);
	}
	break;

    case KEY_DOWN:
	i = current_item;
	if (current_item && current_item->next) {
	    current_item = current_item->next;
	    while (current_item && !current_item->enabled) {
		current_item = current_item->next;
	    }
	    if (current_item == NULL)
		current_item = i;
	    else
		draw_menu(current_menu);
	}
	break;

    case KEY_LEFT:
	undraw_menu(current_menu);
	if (current_menu->prev)
	    current_menu = current_menu->prev;
	else
	    current_menu = menus;
	current_item = current_menu->items;
	while (current_item && !current_item->enabled) {
	    current_item = current_item->next;
	}
	draw_menu(current_menu);
	break;

    case KEY_RIGHT:
	undraw_menu(current_menu);
	if (current_menu->next)
	    current_menu = current_menu->next;
	else
	    current_menu = menus;
	current_item = current_menu->items;
	while (current_item && !current_item->enabled) {
	    current_item = current_item->next;
	}
	draw_menu(current_menu);
	break;

    case KEY_HOME:
	if (current_item) {
	    current_item = current_menu->items;
	    while (current_item && !current_item->enabled) {
		current_item = current_item->next;
	    }
	    draw_menu(current_menu);
	}
	break;

    case KEY_END:
	i = current_item;
	while (current_item) {
	    current_item = current_item->next;
	    if (current_item && current_item->enabled)
		i = current_item;
	}
	current_item = i;
	draw_menu(current_menu);
	break;

    case KEY_ENTER:
	selected = True;
	break;

    case 0:
	switch (u) {
	case '\r':
	case '\n':
	    selected = True;
	    break;
	default:
	    basic_menu_init();
	}
	break;

    default:
	basic_menu_init();
	break;
    }

    if (selected) {
	if (current_item)
	    (*current_item->action)(current_item->param);
	else if (!current_menu->items)
	    (*current_menu->callback)(current_menu->param);
	basic_menu_init();
	if (after_callback != NULL) {
	    (*after_callback)(after_param);
	    after_callback = NULL;
	    after_param = NULL;
	}
    }
}

/* Report a character back to the screen drawing logic. */
Boolean
menu_char(int row, int col, ucs4_t *u, Boolean *highlighted)
{
    if (keypad_is_up)
	return keypad_char(row, col, u, highlighted);

    if (menu_is_up &&
	    row < MODEL_2_ROWS &&
	    col < MODEL_2_COLS &&
	    menu_screen[(row * MODEL_2_COLS) + col]) {
	*u = menu_screen[(row * MODEL_2_COLS) + col];
	*highlighted = menu_rv[(row * MODEL_2_COLS) + col];
	return True;
    } else {
	*u = 0;
	*highlighted = False;
	return False;
    }
}

/* Report where to land the cursor when a menu is up. */
void
menu_cursor(int *row, int *col)
{
    if (keypad_is_up) {
	keypad_cursor(row, col);
	return;
    }

    if (menu_is_up) {
	*row = 0;
	*col = current_menu->offset;
    } else {
	*row = 0;
	*col = 0;
    }
}

/* Functions specific to c3270. */

static void
fm_status(void *ignored _is_unused)
{
    push_macro("Show(status)", False);
    sms_continue();
}

static void
fm_prompt(void *ignored _is_unused)
{
    push_macro("Escape", False);
}

static void
fm_print(void *ignored _is_unused)
{
    push_macro("PrintText", False);
}

static void
fm_xfer(void *ignored _is_unused)
{
    push_macro("Escape() Transfer()", False);
}

static void
fm_trace(void *ignored _is_unused)
{
    if (toggled(DS_TRACE) || toggled(EVENT_TRACE))
	push_macro("Trace off", False);
    else
	push_macro("Trace on", False);
}

static void
fm_disconnect(void *ignored _is_unused)
{
    push_macro("Disconnect", False);
}

/* File menu. */
typedef enum {
    FM_STATUS,
    FM_PROMPT,
    FM_PRINT,
    FM_XFER,
    FM_TRACE,
    FM_DISC,
    FM_COUNT
} file_menu_enum;
cmenu_item_t *file_menu_items[FM_COUNT];
char *file_menu_names[FM_COUNT] = {
    "Status",
    "c3270> Prompt",
    "Print Screen",
    "File Transfer",
    "Enable Tracing",
    "Disconnect"
};
menu_callback file_menu_actions[FM_COUNT] = {
    fm_status,
    fm_prompt,
    fm_print,
    fm_xfer,
    fm_trace,
    fm_disconnect
};

/* Options menu. */
typedef enum {
    OM_MONOCASE,
    OM_BLANKFILL,
    OM_TIMING,
    OM_CURSOR,
    OM_COUNT
} options_menu_enum;
cmenu_item_t *options_menu_items[OM_COUNT];
int option_index[OM_COUNT] = {
    MONOCASE,
    BLANK_FILL,
    SHOW_TIMING,
    CURSOR_POS
};
char *option_names[OM_COUNT] = {
    "Monocase",
    "Blank Fill",
    "Show Timing",
    "Track Cursor"
};

cmenu_t *file_menu;
cmenu_t *options_menu;
cmenu_t *keypad_menu;

static void
toggle_option(void *param)
{
    int index = *(int *)param;

    do_toggle(index);
}

static void
really_popup_keypad(void *ignored _is_unused)
{
    pop_up_keypad(True);
}

static void
popup_keypad(void *ignored _is_unused)
{
    after_callback = really_popup_keypad;
    after_param = NULL;
}

void
menu_init(void)
{
    int j;

    basic_menu_init();

    file_menu = add_menu("File");
    for (j = 0; j < FM_COUNT; j++) {
	file_menu_items[j] = add_item(file_menu, file_menu_names[j],
		file_menu_actions[j], NULL);
    }
    options_menu = add_menu("Options");
    for (j = 0; j < OM_COUNT; j++) {
	int k;
	char *name;

	for (k = 0; k < N_TOGGLES; k++) {
	    if (toggle_names[k].index == option_index[j])
		break;
	}
	name = xs_buffer("%s %s", toggled(option_index[j])? "Disable": "Enable",
		option_names[j]);
	options_menu_items[j] = add_item(options_menu, name, toggle_option,
		&option_index[j]);
	Free(name);
    }
    keypad_menu = add_menu("Keypad");
    set_callback(keypad_menu, popup_keypad, NULL);
}

void
menubar_retoggle(struct toggle *t, int ix)
{
    int j;
    char *s;

    /* Search the options menu. */
    for (j = 0; j < OM_COUNT; j++) {
	if (option_index[j] == ix)
	    break;
    }
    if (j < OM_COUNT) {
	s = xs_buffer("%sable %s", toggled(ix)? "Dis": "En", option_names[j]);
	rename_item(options_menu_items[j], s);
	Free(s);
	return;
    }
    if (ix == EVENT_TRACE || ix == DS_TRACE) {
	s = xs_buffer("%sable Tracing",
		(toggled(EVENT_TRACE) || toggled(DS_TRACE))? "Dis": "En");
	rename_item(file_menu_items[FM_TRACE], s);
	Free(s);
    }
}
