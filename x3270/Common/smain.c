/*
 * Copyright (c) 1993-2009, 2013 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR
 * GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "idlec.h"
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
#include "w3miscc.h"
#include "windirsc.h"
#include "winversc.h"
#endif /*]*/

#if defined(_WIN32) /*[*/
char *instdir = NULL;
char *myappdata = NULL;
#endif /*]*/

void
usage(char *msg)
{
	if (msg != CN)
	    	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "Usage: %s [options] [ps:][LUname@]hostname[:port]\n",
		programname);
	fprintf(stderr, "Options:\n");
	cmdline_help(False);
	exit(1);
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

#if defined(_WIN32) /*[*/
	(void) get_version_info();
	if (get_dirs(argv[0], "wc3270", &instdir, NULL, &myappdata,
		    NULL) < 0)
		exit(1);
	if (sockstart() < 0)
	    	exit(1);
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
	idle_init();
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

#if defined(HAVE_LIBSSL) /*[*/
	ssl_base_init(NULL, NULL);
#endif /*]*/

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
		if (children && waitpid(-1, (int *)0, WNOHANG) > 0)
			--children;
#endif /*]*/
	}
}
