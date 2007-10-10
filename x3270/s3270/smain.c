/*
 * Modifications and original code Copyright 1993, 1994, 1995, 1996,
 *    1999, 2000, 2001, 2002, 2003, 2005 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *   All Rights Reserved.  GTRC hereby grants public use of this software.
 *   Derivative works based on this software must incorporate this copyright
 *   notice.
 *
 * s3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	smain.c
 *		A displayless 3270 Terminal Emulator
 *		Main proceudre.
 */

#include "globals.h"
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "ftc.h"
#include "gluec.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "screenc.h"
#include "selectc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "utilc.h"

void
usage(char *msg)
{
	if (msg != CN)
		Warning(msg);
	xs_error("Usage: %s [options] [ps:][LUname@]hostname[:port]",
		programname);
}

static void
main_connect(Boolean ignored)
{       
	if (CONNECTED || appres.disconnect_clear)
                ctlr_erase(True);
} 

int
main(int argc, char *argv[])
{
	const char	*cl_hostname = CN;

	argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

	if (charset_init(appres.charset) != CS_OKAY) {
		xs_warning("Cannot find charset \"%s\"", appres.charset);
		(void) charset_init(NULL);
	}
	action_init();
	ctlr_init(-1);
	ctlr_reinit(-1);
	kybd_init();
	ansi_init();
	sms_init();
	register_schange(ST_CONNECT, main_connect);
        register_schange(ST_3270_MODE, main_connect);
#if defined(X3270_FT) /*[*/
	ft_init();
#endif /*]*/

	/* Make sure we don't fall over any SIGPIPEs. */
	(void) signal(SIGPIPE, SIG_IGN);

	/* Handle initial toggle settings. */
#if defined(X3270_TRACE) /*[*/
	if (!appres.debug_tracing) {
		appres.toggle[DS_TRACE].value = False;
		appres.toggle[EVENT_TRACE].value = False;
	}
#endif /*]*/
	initialize_toggles();

	/* Connect to the host. */
	if (cl_hostname != CN) {
		if (host_connect(cl_hostname) < 0)
			exit(1);
		/* Wait for negotiations to complete or fail. */
		while (!IN_ANSI && !IN_3270) {
			(void) process_events(True);
			if (!PCONNECTED)
				exit(1);
		}
	}

	/* Prepare to run a peer script. */
	peer_script_init();

	/* Process events forever. */
	while (1) {
		(void) process_events(True);

		if (children && waitpid(0, (int *)0, WNOHANG) > 0)
			--children;
	}
}
