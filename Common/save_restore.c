/*
 * Copyright (c) 2021 Paul Mattes.
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
 *      save_restore.c
 *              Input save and restore.
 */

#include "globals.h"
#include <errno.h>

#include "actions.h"
#include "ctlr.h"
#include "fprint_screen.h"
#include "kybd.h"
#include "names.h"
#include "popups.h"
#include "screen.h"
#include "toggles.h"
#include "trace.h"
#if defined(_WIN32) /*[*/
# include "winprint.h"
#endif /*]*/

#include "save_restore.h"

/* Saved screen contents. */
typedef struct saved_screen {
    char *name;		/* Name, or NULL */
    int rows;		/* Number of rows */
    int columns;	/* Number of columns */
    char *text;		/* Saved text */
    struct saved_screen *next;	/* Next element */
} saved_screen_t;

/* The set of saved screens. */
static saved_screen_t *saved_screens;

/**
 * Find a saved screen.
 *
 * @param[in] name	Screen name
 *
 * @return Saved screen, or null
 */
static saved_screen_t *
find_screen(const char *name)
{
    saved_screen_t *s;

    for (s = saved_screens; s != NULL; s = s->next) {
	if ((name == NULL && s->name == NULL) ||
	    (name != NULL && s->name != NULL && !strcasecmp(name, s->name))) {
	    return s;
	}
    }
    return NULL;
}

/**
 * Save a screen.
 *
 * @param[in] ia	Origin of action
 * @param[in] argc	Argument count
 * @param[in] argv	Arguments
 *
 * @return true for success, false for failure
 */
static bool
SaveInput_action(ia_t ia, unsigned argc, const char **argv)
{
    const char *name;
    saved_screen_t *s;
    bool found = true;
    int fd;
    char *temp_name;
    FILE *f;
    fps_status_t status;
    char buf[8192];
    size_t sz;

    action_debug(AnSaveInput, ia, argc, argv);
    if (check_argc(AnSaveInput, argc, 0, 1) < 0) {
        return false;
    }

    if (!IN_3270) {
	vtrace(AnSaveInput " not in 3270 mode, no-op\n");
	return true;
    }

    name = (argc > 0)? argv[0]: NULL;
    if ((s = find_screen(name)) == NULL) {
	s = (saved_screen_t *)Calloc(1,
		sizeof(saved_screen_t) +
		((name != NULL)? (strlen(name) + 1): 0));
	if (name != NULL) {
	    s->name = (char *)(s + 1);
	    strcpy(s->name, name);
	}
	found = false;
    }

    /* Write the screen contents into a file. */
#if defined(_WIN32) /*[*/
    fd = win_mkstemp(&temp_name, P_TEXT);
#else /*][*/
    temp_name = NewString("/tmp/x3hXXXXXX");
    fd = mkstemp(temp_name);
#endif /*]*/
    if (fd < 0) {
	popup_an_errno(errno, "mkstemp");
	if (!found) {
	    Free(s);
	}
	return false;
    }
    f = fdopen(fd, "w+");
    status = fprint_screen(f, P_TEXT,
	    FPS_EVEN_IF_EMPTY | FPS_INCLUDE_ZERO_INPUT, NULL, NULL, NULL);
    switch (status) {
    case FPS_STATUS_SUCCESS:
    case FPS_STATUS_SUCCESS_WRITTEN:
	break;
    default:
	popup_an_error(AnSaveInput ": Screen print failed");
	if (!found) {
	    Free(s);
	}
	return false;
    }

    /* Save the file contents. */
    fflush(f);
    rewind(f);
    sz = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) {
	s->text = Realloc(s->text, sz + strlen(buf) + 1);
	strcpy(s->text + sz, buf);
	sz += strlen(buf);
    }
    fclose(f);
    unlink(temp_name);
    Free(temp_name);

    s->rows = ROWS;
    s->columns = COLS;
    if (!found) {
	s->next = saved_screens;
	saved_screens = s;
    }

    return true;
}

/**
 * Restore a screen.
 *
 * @param[in] ia	Origin of action
 * @param[in] argc	Argument count
 * @param[in] argv	Arguments
 *
 * @return true for success, false for failure
 */
static bool
RestoreInput_action(ia_t ia, unsigned argc, const char **argv)
{
    saved_screen_t *s;
    const char *name;
    int old_cursor;
    bool is_toggled;

    action_debug(AnRestoreInput, ia, argc, argv);
    if (check_argc(AnRestoreInput, argc, 0, 1) < 0) {
        return false;
    }

    if (!IN_3270 || kybdlock) {
	vtrace(AnRestoreInput " not in 3270 mode or keyboard locked, no-op\n");
	return true;
    }

    /* Find the saved screen. */
    name = (argc > 0)? argv[0]: NULL;
    if ((s = find_screen(name)) == NULL) {
	popup_an_error(AnRestoreInput ": No such screen: %s",
		(name != NULL)? name: "(default)");
	return false;
    }

    if (s->rows != ROWS || s->columns != COLS) {
	popup_an_error(AnRestoreInput ": Rows/Columns mismatch");
	return false;
    }

    /* Paste it from (0, 0), with overlay paste set. */
    old_cursor = cursor_addr;
    cursor_addr = 0;
    if (!(is_toggled = toggled(OVERLAY_PASTE))) {
	toggle_toggle(OVERLAY_PASTE);
    }
    emulate_input(s->text, strlen(s->text), true, false);
    if (!is_toggled) {
	toggle_toggle(OVERLAY_PASTE);
    }
    cursor_move(old_cursor);
    return true;
}

/**
 * Save/restore module registration.
 */
void
save_restore_register(void)
{
    static action_table_t save_restore_actions[] = {
	{ AnSaveInput,		SaveInput_action,	ACTION_KE },
	{ AnRestoreInput,	RestoreInput_action,	ACTION_KE }
    };

    /* Register the actions. */
    register_actions(save_restore_actions, array_count(save_restore_actions));
}
