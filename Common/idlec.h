/*
 * Copyright 2002-2008 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	idlec.h
 *		Global declarations for idle.c.
 */

enum idle_enum {
	IDLE_DISABLED = 0,
	IDLE_SESSION = 1,
	IDLE_PERM = 2
};

#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
extern void cancel_idle_timer(void);
extern void idle_init(void);
extern void reset_idle_timer(void);
extern char *get_idle_command();
extern char *get_idle_timeout();
extern Boolean idle_changed;
extern char *idle_command;
extern char *idle_timeout_string;
extern enum idle_enum idle_user_enabled;
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
extern void popup_idle(void);
#endif /*]*/
#else /*][*/
#define cancel_idle_timer()
#define idle_init()
#define reset_idle_timer()
#endif /*]*/
