/*
 * Modifications and original code Copyright 1993-2008 by Paul Mattes.
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
#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#include <signal.h>
#endif /*]*/
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

#if defined(_WIN32) /*[*/
#include "windirsc.h"
#include "winversc.h"
#endif /*]*/

#if defined(_WIN32) /*[*/
char *instdir = NULL;
char myappdata[MAX_PATH];
#endif /*]*/

void
usage(char *msg)
{
	if (msg != CN)
		Warning(msg);
	xs_error("Usage: %s [options] [ps:][LUname@]hostname[:port]",
		programname);
}

#if defined(_WIN32) /*[*/
/*
 * Figure out the install directory and our data directory.
 */
static void
save_dirs(char *argv0)
{
    	char *bsl;

	/* Extract the installation directory from argv[0]. */
	bsl = strrchr(argv0, '\\');
	if (bsl != NULL) {
	    	instdir = NewString(argv0);
		instdir[(bsl - argv0) + 1] = '\0';
	} else
	    	instdir = "";

	/* Figure out the application data directory. */
	if (get_dirs(NULL, myappdata, "ws3270") < 0)
	    	exit(1);
}
#endif /*]*/

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

#if defined(_WIN32) /*[*/
	(void) get_version_info();
	(void) save_dirs(argv[0]);
#endif /*]*/

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

#if !defined(_WIN32) /*[*/
	/* Make sure we don't fall over any SIGPIPEs. */
	(void) signal(SIGPIPE, SIG_IGN);
#endif /*]*/

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

#if !defined(_WIN32) /*[*/
		if (children && waitpid(0, (int *)0, WNOHANG) > 0)
			--children;
#endif /*]*/
	}
}
