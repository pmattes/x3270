/*
 * Copyright (c) 2009, 2013-2015, 2019-2020, 2022 Paul Mattes.
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
 *	keypad.c
 *		A curses-based 3270 Terminal Emulator
 *		Pop-up keypad
 */

#include "globals.h"

#include "actions.h"
#include "3270ds.h"
#include "appres.h"
#include "ckeypad.h"
#include "cmenubar.h"
#include "ctlrc.h"
#include "names.h"
#include "task.h"

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

/* Sensitivity map: A rectangular region and a callback function. */
typedef struct {
    unsigned char ul_x, ul_y;	/* upper left corner */
    unsigned char lr_x, lr_y;	/* lower right corner */
    char *callback;		/* callback macro string */
} sens_t;

/* Keymap descriptor for one character cell. */
typedef struct {
    unsigned char literal;	/* literal character, or 0 */
    unsigned char outline;	/* box-drawing character (ACS notation) */
    sens_t *sens;		/* sensitivity map element, for highlighting */
} keypad_desc_t;

/* Pull in the compiled keypad structures (sens, keypad_desc). */
#include "compiled_keypad.h"
#define KEYPAD_HEIGHT	(sizeof(keypad_desc)/sizeof(keypad_desc[0]))
#define NUM_SENSE	(sizeof(sens)/sizeof(sens[0]))

static sens_t *current_sens = NULL;
#if defined(XXX_DEBUG) || defined(YYY_DEBUG) || defined(ZZZ_DEBUG)
static FILE *xxx = NULL;
#endif

/* Return the keypad character on top of the screen. */
bool
keypad_char(int row, int col, ucs4_t *u, bool *highlighted,
	unsigned char *acs)
{
	keypad_desc_t *d;

	if ((menu_is_up & KEYPAD_IS_UP) &&
	    (unsigned)row < KEYPAD_HEIGHT && col < MODEL_2_COLS) {
		d = &keypad_desc[row][col];
		if (d->outline && d->outline != ' ') {
			map_acs(d->outline, u, acs);
			*highlighted = (d->sens != NULL) &&
			    (d->sens == current_sens);
#ifdef XXX_DEBUG
			fprintf(xxx, "row %d col %d outline 0x%x !highlight\n",
				row, col, *u);
#endif
			return true;
		}
		if (d->literal) {
			*u = d->literal;
			*highlighted = (d->sens != NULL) &&
			    (d->sens == current_sens);
#ifdef XXX_DEBUG
			fprintf(xxx, "row %d col %d literal '%c' d->sens %p "
				"%s current_sens %p %s\n",
				row, col, *u,
				(void *)d->sens,
				d->sens? d->sens->callback: "(null)",
				(void *)current_sens,
				current_sens->callback);
#endif
			return true;
		}
	}
	*u = 0;
	*highlighted = false;
	return false;
}

/* Report where to land the cursor when the keypad is up. */
void
keypad_cursor(int *row, int *col)
{
	if (menu_is_up & KEYPAD_IS_UP) {
		*row = current_sens->ul_y;
		*col = current_sens->ul_x;
	} else {
		*row = 0;
		*col = 0;
	}
}

/* Pop up the keypad. */
void
pop_up_keypad(bool up)
{
	if (up) {
		menu_is_up |= KEYPAD_IS_UP;
		current_sens = &sens[0];
#if defined(XXX_DEBUG) || defined(YYY_DEBUG) || defined(ZZZ_DEBUG)
		if (xxx == NULL) {
			xxx = fopen("/tmp/ccc", "a");
			if (xxx == NULL) {
				perror("/tmp/ccc");
				exit(1);
			}
		}
#endif
	} else {
		menu_is_up &= ~KEYPAD_IS_UP;
		current_sens = NULL;
	}
	screen_changed = true;
}

/*
 * Find the center of a button.  We deliberately round *up* here, so that when
 * two centers are compared, the bias is up and to the left.
 *
 * Here's the picture:
 * 0 +-----+
 * 1 | a   |
 * 2 |     | +------+
 * 3 +-----+ | c    |
 * 4 | b   | +------+
 * 5 |     |
 * 6 +-----+
 *
 * We want 'a' to be chosen when going left from 'c'.
 * We round up to make the center of 'a' 2 (not 1), and the center of 'b' 5
 * (not 4).  The center of 'c' is 3.
 * Because of the round-up, 'a' is better centered than 'b'.
 */
static int
find_center_x(sens_t *s)
{
	return s->ul_x + ((s->lr_x - s->ul_x + 1) / 2);
}

static int
find_center_y(sens_t *s)
{
	return s->ul_y + ((s->lr_y - s->ul_y + 1) / 2);
}

/*
 * Find the best adjacent button.  xinc and yinc indicate the search direction:
 *  xinc yinc direction
 *  0    -1   up
 *  0    +1   down
 *  -1   0    left
 *  +1   0    right
 */
static void
find_adjacent(int xinc, int yinc)
{
	int ul_x, lr_x, ul_y, lr_y;
#	define N_MATCH 4
	sens_t *matches[N_MATCH];
	int n_matched = 0;

	if (yinc) {
		/* Searching up or down.  Spread x out. */
		ul_x = current_sens->ul_x - 1;
		lr_x = current_sens->lr_x + 1;
		if (yinc < 0) {
			/* Up. */
			ul_y = current_sens->ul_y - 1;
			lr_y = current_sens->ul_y - 1;
		} else {
			/* Down. */
			ul_y = current_sens->lr_y + 1;
			lr_y = current_sens->lr_y + 1;
		}
	} else {
		/* Searching left or right.  Spread y out. */
		ul_y = current_sens->ul_y - 1;
		lr_y = current_sens->lr_y + 1;
		if (xinc < 0) {
			/* Left. */
			ul_x = current_sens->ul_x - 1;
			lr_x = current_sens->ul_x - 1;
		} else {
			/* Right. */
			ul_x = current_sens->lr_x + 1;
			lr_x = current_sens->lr_x + 1;
		}
	}
#if defined(YYY_DEBUG)
	fprintf(xxx, "ul_y %d ul_x %d lr_y %d lr_x %d\n",
		ul_y, ul_x, lr_y, lr_x);
	fflush(xxx);
#endif

	while (true) {
		int x, y;

		for (y = ul_y; y <= lr_y; y++) {
			for (x = ul_x; x <= lr_x; x++) {
#if defined(YYY_DEBUG)
				fprintf(xxx, "searching row %d col %d\n", x, y);
				fflush(xxx);
#endif
				if (keypad_desc[y][x].sens != NULL &&
				    n_matched < N_MATCH) {
					int i;

					for (i = 0; i < n_matched; i++) {
						if (matches[i] ==
							keypad_desc[y][x].sens)
							break;
					}
					if (i >= n_matched) {
						matches[n_matched++] =
						    keypad_desc[y][x].sens;
					}
				}
			    }
		}
		if (n_matched) {
			int i;

#if defined(ZZZ_DEBUG)
			fprintf(xxx, "%d matches:", n_matched);
			for (i = 0; i < n_matched; i++) {
				fprintf(xxx, " %s", matches[i]->callback);
			}
			fprintf(xxx, "\n");
			fflush(xxx);
#endif
			if (n_matched == 0)
				current_sens = matches[0];
			else {
				int overlap[N_MATCH];
				int center[N_MATCH];
				int best_o = -1;
				int best_c = -1;
				int tie = 0;
				int curr_center;

				/* Find the best match. */
				for (i = 0; i < n_matched; i++) {
					int j;

					overlap[i] = 0;
					if (yinc) {
						/*
						 * Scanning up/down, measure X
						 * overlap.
						 */
						for (j = matches[i]->ul_x;
						     j <= matches[i]->lr_x;
						     j++) {
							if (j >= ul_x &&
							    j <= lr_x) {
								overlap[i]++;
							}
							if (j >= ul_x + 1 &&
							    j <= lr_x - 1) {
								overlap[i]++;
							}
						}
					} else {
						/*
						 * Scanning left/right, measure
						 * Y overlap.
						 */
						for (j = matches[i]->ul_y;
						     j <= matches[i]->lr_y;
						     j++) {
							if (j >= ul_y &&
							    j <= lr_y) {
								overlap[i]++;
							}
							if (j >= ul_y + 1 &&
							    j <= lr_y - 1) {
								overlap[i]++;
							}
						}
					}
				}
				for (i = 0; i < n_matched; i++) {
					if (best_o < 0 ||
					    overlap[i] > overlap[best_o])
						best_o = i;
				}
				for (i = 0; i < n_matched; i++) {
					if (i != best_o &&
					    overlap[i] == overlap[best_o])
						tie++;
					if (yinc)
						center[i] =
						    find_center_x(matches[i]);
					else
						center[i] =
						    find_center_y(matches[i]);
				}
#if defined(ZZZ_DEBUG)
				fprintf(xxx, "overlaps:");
				for (i = 0; i < n_matched; i++) {
					fprintf(xxx, " %d", overlap[i]);
					if (i == best_o)
						fprintf(xxx, "*");
				}
				fprintf(xxx, ", tie %d\n", tie);
				fflush(xxx);
#endif
				if (tie) {
					/*
					 * Pick the best-centered match.
					 * That's the one whose center is
					 * closest to the center of the current
					 * button.
					 */
					if (yinc)
						curr_center =
						    find_center_x(current_sens);
					else
						curr_center =
						    find_center_y(current_sens);
#if defined(ZZZ_DEBUG)
					fprintf(xxx, "curr_center is %d\n",
						curr_center);
					fflush(xxx);
#endif
					for (i = 0; i < n_matched; i++) {
						if (overlap[i] ==
							overlap[best_o]) {
							if (best_c < 0 ||
							    abs(curr_center - center[i]) <
							    abs(curr_center - center[best_c])) {
#if defined(ZZZ_DEBUG)
								fprintf(xxx,
									"center '%s' (%d) is better\n",
									matches[i]->callback,
									center[i]);
								fflush(xxx);
#endif
								best_c = i;
							}
						}
					}
					current_sens = matches[best_c];
				} else
					current_sens = matches[best_o];
			}
			break;
		}

		/* Keep looking. */
		ul_x += xinc;
		lr_x += xinc;
		ul_y += yinc;
		lr_y += yinc;
		if (ul_x < 0 || lr_x >= MODEL_2_COLS ||
		    ul_y < 0 || (size_t)lr_y >= KEYPAD_HEIGHT)
		    break;
	}
}

#if defined(_WIN32) /*[*/
void
keypad_click(int x, int y)
{
	size_t i;

	if (!(menu_is_up & KEYPAD_IS_UP))
	    return;

	/* Find it. */
	for (i = 0; i < NUM_SENSE; i++) {
		if (x >= sens[i].ul_x && y >= sens[i].ul_y &&
		    x <= sens[i].lr_x && y <= sens[i].lr_y) {
			push_keypad_action(sens[i].callback);
			break;
		}
	}
	pop_up_keypad(false);
}
#endif /*]*/

/* Process a key event while the keypad is up. */
void
keypad_key(int k, ucs4_t u)
{
	if (!(menu_is_up & KEYPAD_IS_UP))
	    return;

	switch (k) {

#if defined(NCURSES_MOUSE_VERSION) /*[*/
	case MK_MOUSE: {
		MEVENT m;
		size_t i;

		if (getmouse(&m) != OK)
			return;
		if (!(m.bstate & (BUTTON1_PRESSED | BUTTON1_RELEASED)))
			return;
		/* Find it. */
		for (i = 0; i < NUM_SENSE; i++) {
			if (m.x >= sens[i].ul_x && m.y >= sens[i].ul_y &&
			    m.x <= sens[i].lr_x && m.y <= sens[i].lr_y) {
				push_keypad_action(sens[i].callback);
				break;
			}
		}
		pop_up_keypad(false);
		break;
	    }
#endif /*]*/

	case MK_UP:
		find_adjacent(0, -1);
		break;

	case MK_DOWN:
		find_adjacent(0, 1);
		break;

	case MK_LEFT:
		find_adjacent(-1, 0);
		break;

	case MK_RIGHT:
		find_adjacent(1, 0);
		break;

	case MK_HOME:
		/* Find the first entry. */
		current_sens = &sens[0];
		break;

	case MK_END:
		/* Find the last entry. */
		current_sens = &sens[NUM_SENSE - 1];
		break;

	case MK_ENTER:
		push_keypad_action(current_sens->callback);
		pop_up_keypad(false);
		break;

	case MK_NONE:
		switch (u) {
		case '\r':
		case '\n':
			push_keypad_action(current_sens->callback);
			break;
		default:
			break;
		}
		pop_up_keypad(false);
		break;

	default:
	case MK_OTHER:
		pop_up_keypad(false);
		break;
	}

	screen_changed = true;
}

bool
Keypad_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnKeypad, ia, argc, argv);
    if (check_argc(AnKeypad, argc, 0, 0) < 0) {
	return false;
    }
    pop_up_keypad(true);
    return true;
}

/**
 * Keypad module registration.
 */
void
keypad_register(void)
{
    static action_table_t keypad_actions[] = {
	{ AnKeypad,	Keypad_action,	ACTION_KE }
    };

    /* Register the actions. */
    register_actions(keypad_actions, array_count(keypad_actions));
}
