/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2007 by Paul
 *   Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	xio.c
 *		Low-level I/O setup functions and exit code.
 */

#include "globals.h"

#include "actionsc.h"
#include "hostc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "utilc.h"
#include "xioc.h"

/* Statics. */
static unsigned long ns_read_id;
static unsigned long ns_exception_id;
static Boolean reading = False;
static Boolean excepting = False;

/*
 * Called to set up input on a new network connection.
 */
void
x_add_input(int net_sock)
{
	ns_exception_id = AddExcept(net_sock, net_exception);
	excepting = True;
	ns_read_id = AddInput(net_sock, net_input);
	reading = True;
}

/*
 * Called when an exception is received to disable further exceptions.
 */
void
x_except_off(void)
{
	if (excepting) {
		RemoveInput(ns_exception_id);
		excepting = False;
	}
}

/*
 * Called when exception processing is complete to re-enable exceptions.
 * This includes removing and restoring reading, so the exceptions are always
 * processed first.
 */
void
x_except_on(int net_sock)
{
	if (excepting)
		return;
	if (reading)
		RemoveInput(ns_read_id);
	ns_exception_id = AddExcept(net_sock, net_exception);
	excepting = True;
	if (reading)
		ns_read_id = AddInput(net_sock, net_input);
}

/*
 * Called to disable input on a closing network connection.
 */
void
x_remove_input(void)
{
	if (reading) {
		RemoveInput(ns_read_id);
		reading = False;
	}
	if (excepting) {
		RemoveInput(ns_exception_id);
		excepting = False;
	}
}

/*
 * Application exit, with cleanup.
 */
void
x3270_exit(int n)
{
	static Boolean already_exiting = 0;

	/* Handle unintentional recursion. */
	if (already_exiting)
		return;
	already_exiting = True;

	/* Turn off toggle-related activity. */
	shutdown_toggles();

	/* Shut down the socket gracefully. */
	host_disconnect(False);

	/* Tell anyone else who's interested. */
	st_changed(ST_EXITING, True);

#if defined(WC3270) /*[*/
	if (n) {
		char buf[2];

		printf("\n[Press <Enter>] ");
		fflush(stdout);
		(void) fgets(buf, sizeof(buf), stdin);

	}
#endif /*]*/

	exit(n);
}

void
Quit_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Quit_action, event, params, num_params);
	if (!w || !CONNECTED) {
		x3270_exit(0);
	}
}
