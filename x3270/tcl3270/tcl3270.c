/*
 * Modifications Copyright 1993-2008 by Paul Mattes.
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
 * tcl3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/* 
 * tclAppInit.c --
 *
 *	Provides a default version of the main program and Tcl_AppInit
 *	procedure for Tcl applications (without Tk).
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tcl3270.c,v 1.35 2007/07/17 15:58:53 pdm Exp $
 */

/*
 *	tcl3270.c
 *		A tcl-based 3270 Terminal Emulator
 *		Main proceudre.
 */

#include "tcl.h"

#include "globals.h"
#if defined(HAVE_TCLEXTEND_H) /*[*/
#include "tclExtend.h"
#endif /*]*/

#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

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
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"

/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */

#if defined(_sun) /*[*/
extern int matherr();
int *tclDummyMathPtr = (int *) matherr;
#endif /*]*/

static Tcl_ObjCmdProc x3270_cmd;
static Tcl_ObjCmdProc Rows_cmd, Cols_cmd;
static enum {
	NOT_WAITING,		/* Not waiting */
	AWAITING_CONNECT,	/* Connect (negotiation completion) */
	AWAITING_RESET,		/* Keyboard locked */
	AWAITING_FT,		/* File transfer in progress */
	AWAITING_IFIELD,	/* Wait InputField */
	AWAITING_3270,		/* Wait 3270Mode */
	AWAITING_NVT,		/* Wait NVTMode */
	AWAITING_OUTPUT,	/* Wait Output */
	AWAITING_SOUTPUT,	/* Snap Wait */
	AWAITING_DISCONNECT,	/* Wait Disconnect */
	AWAITING_UNLOCK		/* Wait Unlock */
} waiting = NOT_WAITING;
static const char *wait_name[] = {
	"not waiting",
	"connection incomplete",
	"keyboard locked",
	"file transfer in progress",
	"need input field",
	"need 3270 mode",
	"need NVT mode",
	"need host output",
	"need snap host output",
	"need host disconnect",
	"need keyboard unlock"
};
static const char *unwait_name[] = {
	"wasn't waiting",
	"connection complete",
	"keyboard unlocked",
	"file transfer complete",
	"input field found",
	"in 3270 mode",
	"in NVT mode",
	"host generated output",
	"host generated snap output",
	"host disconnected",
	"keyboard unlocked"
};
static unsigned long wait_id = 0L;
static unsigned long command_timeout_id = 0L;
static int cmd_ret;
static char *action = NULL;
static Boolean interactive = False;

/* Local prototypes. */
static void ps_clear(void);
static int tcl3270_main(int argc, const char *argv[]);
static void negotiate(void);
static char *tc_scatv(char *s);
static void snap_save(void);
static void wait_timed_out(void);

/* Macros.c stuff. */
static Boolean in_cmd = False;
static Tcl_Interp *sms_interp;
static Boolean output_wait_needed = False;
static char *pending_string = NULL;
static char *pending_string_ptr = NULL;
static Boolean pending_hex = False;
Boolean macro_output = False;

/* Is the keyboard is locked due to user input? */
#define KBWAIT	(kybdlock & (KL_OIA_LOCKED|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK))
#define CKBWAIT (appres.toggle[AID_WAIT].value && KBWAIT)

/* Is it safe to continue a script waiting for an input field? */
#define INPUT_OKAY ( \
    IN_SSCP || \
    (IN_3270 && formatted && cursor_addr && !CKBWAIT) || \
    (IN_ANSI && !(kybdlock & KL_AWAITING_FIRST)) \
)

/* Is is safe to continue a script waiting for the connection to complete? */
#define CONNECT_DONE	(IN_SSCP || IN_3270 || IN_ANSI)

/* Shorthand macro to unlock the current action. */
#define UNBLOCK() { \
	trace_event("Unblocked %s (%s)\n", action, unwait_name[waiting]); \
	waiting = NOT_WAITING; \
	if (wait_id != 0L) { \
		RemoveTimeOut(wait_id); \
		wait_id = 0L; \
	} \
}


/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tcl_Main never returns here, so this procedure never
 *	returns either.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

int
main(int argc, char **argv)
{
#if defined(HAVE_TCLEXTEND_H) /*[*/
    TclX_Main(argc, argv, Tcl_AppInit);
#else /*][*/
    Tcl_Main(argc, argv, Tcl_AppInit);
#endif /*]*/
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(Tcl_Interp *interp)
{
    const char *s0, *s;
    int tcl_argc;
    const char **tcl_argv;
    int argc;
    const char **argv;
    int i;
    Tcl_Obj *argv_obj;
    char argc_buf[32];

    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
#if defined(HAVE_TCLEXTEND_H) /*[*/
    if (Tclx_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
#endif /*]*/


    /* Use argv and argv0 to figure out our command-line arguments. */
    s0 = Tcl_GetVar(interp, "argv0", 0);
    if (s0 == NULL)
	return TCL_ERROR;
    s = Tcl_GetVar(interp, "argv", 0);
    if (s == NULL)
	return TCL_ERROR;
    (void) Tcl_SplitList(interp, s, &tcl_argc, &tcl_argv);
    argc = tcl_argc + 1;
    argv = (const char **)Malloc((argc + 1) * sizeof(char *));
    argv[0] = s0;
    for (i = 0; i < tcl_argc; i++) {
	argv[1 + i] = tcl_argv[i];
    }
    argv[argc] = NULL;

    /* Find out if we're interactive. */
    s = Tcl_GetVar(interp, "tcl_interactive", 0);
    interactive = (s != NULL && !strcmp(s, "1"));

    /* Call main. */
    if (tcl3270_main(argc, argv) == TCL_ERROR)
	return TCL_ERROR;

    /* Replace tcl's argc and argv with whatever was left. */
    argv_obj = Tcl_NewListObj(0, NULL);
    for (i = 1; argv[i] != NULL; i++) {
	Tcl_ListObjAppendElement(interp, argv_obj, Tcl_NewStringObj(argv[i],
		strlen(argv[i])));
    }
    Tcl_SetVar2Ex(interp, "argv", NULL, argv_obj, 0);
    (void) sprintf(argc_buf, "%d", i?i-1:0);
    Tcl_SetVar(interp, "argc", argc_buf, 0);

    /*
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.
     */

    /*
     * Call Tcl_CreateCommands for the application-specific commands, if
     * they weren't already created by the init procedures called above.
     */
    action_init();
    for (i = 0; i < actioncount; i++) {
	if (Tcl_CreateObjCommand(interp, actions[i].string, x3270_cmd, NULL,
		NULL) == NULL) {
	    return TCL_ERROR;
	}
    }
    if (Tcl_CreateObjCommand(interp, "Rows", Rows_cmd, NULL, NULL) == NULL)
	return TCL_ERROR;
    if (Tcl_CreateObjCommand(interp, "Cols", Cols_cmd, NULL, NULL) == NULL)
	return TCL_ERROR;

    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */
#if 0
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY);
#endif

    return TCL_OK;
}


void
usage(char *msg)
{
	const char *sn = "";

	if (msg != CN)
		Warning(msg);
	if (!strcmp(programname, "tcl3270"))
		sn = " [scriptname]";
	xs_error("Usage: %s%s [tcl3270-options] [host] [-- script-args]\n"
"       <host> is [ps:][LUname@]hostname[:port]",
			programname, sn);
}

/*
 * Called when the host connects, disconnects, or changes modes.
 * When we connect or change modes, clear the screen.
 * When we disconnect, clear the pending string, so we don't keep trying to
 * feed it to a dead host.
 */
static void
main_connect(Boolean ignored)
{
	if (CONNECTED) {
		ctlr_erase(True);
		/* Check for various wait conditions. */
		switch (waiting) {
		case AWAITING_CONNECT:
			if (CONNECT_DONE)
				UNBLOCK();
			break;
		case AWAITING_3270:
			if (IN_3270)
				UNBLOCK();
			break;
		case AWAITING_NVT:
			if (IN_ANSI)
				UNBLOCK();
			break;
		default:
			/* Nothing we can figure out here. */
			break;
		}
	} else {
		if (appres.disconnect_clear)
			ctlr_erase(True);
		ps_clear();

		/* Cause (almost) any pending Wait command to fail. */
		if (waiting != NOT_WAITING) {
			if (waiting == AWAITING_DISCONNECT) {
				UNBLOCK();
			} else {
				trace_event("Unblocked %s (was '%s') -- "
					"failure\n", action,
					wait_name[waiting]);
				popup_an_error("Host disconnected");
				waiting = NOT_WAITING;
			}
		}
	}
}

/* Initialization procedure for tcl3270. */
static int
tcl3270_main(int argc, const char *argv[])
{
	const char	*cl_hostname = CN;

	argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

	if (charset_init(appres.charset) != CS_OKAY) {
		xs_warning("Cannot find charset \"%s\"", appres.charset);
		(void) charset_init(CN);
	}
	ctlr_init(-1);
	ctlr_reinit(-1);
	kybd_init();
	ansi_init();
#if defined(X3270_FT) /*[*/
	ft_init();
#endif /*]*/

	register_schange(ST_CONNECT, main_connect);
	register_schange(ST_3270_MODE, main_connect);

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

	/* Connect to the host, and wait for negotiation to complete. */
	if (cl_hostname != CN) {
		action = NewString("[initial connection]");
		if (host_connect(cl_hostname) < 0)
			exit(1);
		if (CONNECTED || HALF_CONNECTED) {
			sms_connect_wait();
			negotiate();
		}
	}

	return TCL_OK;
}


/* Replacements for the logic in macros.c. */


/* Process the pending string (set by the String command). */
static void
process_pending_string(void)
{
	if (pending_string_ptr == NULL || waiting != NOT_WAITING)
		return;

	if (pending_hex) {
		hex_input(pending_string_ptr);
		ps_clear();
	} else {
		int len = strlen(pending_string_ptr);
		int len_left;

		len_left = emulate_input(pending_string_ptr, len, False);
		if (len_left) {
			pending_string_ptr += len - len_left;
			return;
		} else
			ps_clear();
	}
	if (CKBWAIT) {
		trace_event("Blocked %s (keyboard locked)\n", action);
		waiting = AWAITING_RESET;
	}
}

/* Clear out the pending string. */
static void
ps_clear(void)
{
	if (pending_string != NULL) {
		pending_string_ptr = NULL;
		Replace(pending_string, NULL);
	}
}

/* Command timeout function. */
static void
command_timed_out(void)
{
    	popup_an_error("Command timed out after %ds.\n",
		appres.command_timeout);
	command_timeout_id = 0L;

	/* Let the command complete unsuccessfully. */
	UNBLOCK();
}

/* The tcl "x3270" command: The root of all 3270 access. */
static int
x3270_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
		Tcl_Obj *CONST objv[])
{
	int i;
	unsigned j;
	unsigned count;
	char **argv = NULL;
	int old_mode;

	/* Set up ugly global variables. */
	in_cmd = True;
	sms_interp = interp;

	/* Synchronously run any pending I/O's and timeouts.  Ugly. */
	old_mode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
	while (process_events(False))
		;
	(void) Tcl_SetServiceMode(old_mode);

	/* Verify minimal command syntax. */
	if (objc < 1) {
		Tcl_SetResult(interp, "Missing action name", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Look up the action. */
	Replace(action, NewString(Tcl_GetString(objv[0])));
	for (i = 0; i < actioncount; i++) {
		if (!strcmp(action, actions[i].string))
			break;
	}
	if (i >= actioncount) {
		Tcl_SetResult(interp, "No such action", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Stage the arguments. */
	count = objc - 1;
	if (count) {
		argv = (char **)Malloc(count*sizeof(char *));
		for (j = 0; j < count; j++) {
			argv[j] = Tcl_GetString(objv[j + 1]);
		}
	}

#if defined(X3270_TRACE) /*[*/
	/* Trace what we're about to do. */
	if (toggled(EVENT_TRACE)) {
		trace_event("Running %s", action);
		for (j = 0; j < count; j++) {
			char *s;

			s = tc_scatv(argv[j]);
			trace_event(" %s", s);
			Free(s);
		}
		trace_event("\n");
	}
#endif /*]*/

	/* Set up more ugly global variables and run the action. */
	ia_cause = IA_SCRIPT;
	cmd_ret = TCL_OK;
	(*actions[i].proc)((Widget)NULL, (XEvent *)NULL, argv, &count);

	/* Set implicit wait state. */
#if defined(X3270_FT) /*[*/
	if (ft_state != FT_NONE)
		waiting = AWAITING_FT;
	else
#endif /*]*/
	if ((waiting == NOT_WAITING) && CKBWAIT)
		waiting = AWAITING_RESET;

	if (waiting != NOT_WAITING) {
		trace_event("Blocked %s (%s)\n", action, wait_name[waiting]);
		if (appres.command_timeout) {
			command_timeout_id = AddTimeOut(
				appres.command_timeout * 1000,
				command_timed_out);
		}
	}

	/*
	 * Process responses and push any pending string, until
	 * we can proceed.
	 */
	process_pending_string();
	old_mode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
	while (waiting != NOT_WAITING) {

		/* Process pending file I/O. */
		(void) process_events(True);

		/*
		 * Check for the completion of output-related wait conditions.
		 */
		switch (waiting) {
		case AWAITING_IFIELD:
			if (INPUT_OKAY)
				UNBLOCK();
			break;
		case AWAITING_RESET:
			if (!CKBWAIT)
				UNBLOCK();
			break;
#if defined(X3270_FT) /*[*/
		case AWAITING_FT:
			if (ft_state == FT_NONE)
				UNBLOCK();
			break;
#endif /*]*/
		case AWAITING_UNLOCK:
			if (!KBWAIT)
				UNBLOCK();
		default:
			break;
		}

		/* Push more string text in. */
		process_pending_string();
	}
	if (command_timeout_id != 0L) {
	    	RemoveTimeOut(command_timeout_id);
		command_timeout_id = 0L;
	}
#if defined(X3270_TRACE) /*[*/
	if (toggled(EVENT_TRACE)) {
		const char *s;
#		define TRUNC_LEN 40
		char s_trunc[TRUNC_LEN + 1];

		s = Tcl_GetStringResult(interp);
		trace_event("Completed %s (%s)", action,
			(cmd_ret == TCL_OK) ? "ok" : "error");
		if (s != CN && *s) {
			char buf[1024];

			strncpy(s_trunc, s, TRUNC_LEN);
			s_trunc[TRUNC_LEN] = '\0';
			trace_event(" -> \"%s\"",
			    scatv(s_trunc, buf, sizeof(buf)));
			if (strlen(s) > TRUNC_LEN)
				trace_event("...(%d chars)", strlen(s));
		}
		trace_event("\n");
	}
#endif /*]*/
	(void) Tcl_SetServiceMode(old_mode);
	in_cmd = False;
	sms_interp = NULL;
	if (argv)
		Free(argv);
	return cmd_ret;
}

/* Do initial connect negotiation. */
void
negotiate(void)
{
	int old_mode;

	old_mode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
	while (CKBWAIT || (waiting == AWAITING_CONNECT && !CONNECT_DONE)) {
		(void) process_events(True);
		if (!PCONNECTED)
			exit(1);
	}
	(void) Tcl_SetServiceMode(old_mode);
}

/* Indicates whether errors should go to stderr, or be returned to tcl. */
Boolean
sms_redirect(void)
{
	return in_cmd;
}

/* Returns an error to tcl. */
void
sms_error(const char *s)
{
	Tcl_SetResult(sms_interp, (char *)s, TCL_VOLATILE);
	cmd_ret = TCL_ERROR;
}

/* For now, a no-op.  Used to implement 'Expect'. */
void
sms_store(unsigned char c)
{
}

/* Set the pending string.  Used by the 'String' action. */
void
ps_set(char *s, Boolean is_hex)
{
	Replace(pending_string, NewString(s));
	pending_string_ptr = pending_string;
	pending_hex = is_hex;
}

/* Signal a new connection. */
void
sms_connect_wait(void)
{
	waiting = AWAITING_CONNECT;
}

/* Signal host output. */
void
sms_host_output(void)
{
	/* Release the script, if it is waiting now. */
	switch (waiting) {
	    case AWAITING_SOUTPUT:
		snap_save();
		/* fall through... */
	    case AWAITING_OUTPUT:
		UNBLOCK();
		break;
	    default:
		break;
	}

	/* If there was no script waiting, ensure that it won't later. */
	output_wait_needed = False;
}

/* More no-ops. */
void
login_macro(char *s)
{
}
void
sms_continue(void)
{
}

/* Data query actions. */

static void
dump_range(int first, int len, Boolean in_ascii, struct ea *buf,
    int rel_rows unused, int rel_cols)
{
	register int i;
	Tcl_Obj *o = NULL;
	Tcl_Obj *row = NULL;
	Boolean is_zero = False;

	/*
	 * The client has now 'looked' at the screen, so should they later
	 * execute 'Wait(output)', they will actually need to wait for output
	 * from the host.  output_wait_needed is cleared by sms_host_output,
	 * which is called from the write logic in ctlr.c.
	 */
	if (buf == ea_buf)
		output_wait_needed = True;

	is_zero = FA_IS_ZERO(get_field_attribute(first));

	for (i = 0; i < len; i++) {

		/* Check for a new row. */
		if (i && !((first + i) % rel_cols)) {
			/* Done with this row. */
			if (o == NULL)
				o = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(sms_interp, o, row);
			row = NULL;
		}
		if (!row) {
			if (in_ascii)
				row = Tcl_NewObj();
			else
				row = Tcl_NewListObj(0, NULL);
		}
		if (in_ascii) {
			int len;
			char mb[16];
			ucs4_t uc;

			mb[0] = ' ';
			mb[1] = '\0';
			len = 2;
			if (buf[first + i].fa) {
				is_zero = FA_IS_ZERO(buf[first + i].fa);
				/* leave mb[] as " " */
			} else if (is_zero) {
				/* leave mb[] as " " */
			} else
#if defined(X3270_DBCS) /*[*/
			if (IS_LEFT(ctlr_dbcs_state(first + i))) {
				len = ebcdic_to_multibyte(
					(buf[first + i].cc << 8) |
					 buf[first + i + 1].cc,
					mb, sizeof(mb));
			} else if (IS_RIGHT(ctlr_dbcs_state(first + i))) {
				continue;
			} else
#endif /*]*/
			{
			    	len = ebcdic_to_multibyte_x(buf[first + i].cc,
					buf[first + i].cs & CS_MASK,
					mb, sizeof(mb), True,
					&uc);
			}
			if (len > 0)
				Tcl_AppendToObj(row, mb, len - 1);
		} else {
			char s[5];

			(void) sprintf(s, "0x%02x", buf[first + i].cc);
			Tcl_ListObjAppendElement(sms_interp, row,
				Tcl_NewStringObj(s, -1));
		}
	}

	/* Return it. */
	if (row) {
		if (o) {
			Tcl_ListObjAppendElement(sms_interp, o, row);
			Tcl_SetObjResult(sms_interp, o);
		} else
			Tcl_SetObjResult(sms_interp, row);
	}
}

static void
dump_rectangle(int start_row, int start_col, int rows, int cols,
    Boolean in_ascii, struct ea *buf, int rel_cols)
{
	register int r, c;
	Tcl_Obj *o = NULL;
	Tcl_Obj *row = NULL;

	/*
	 * The client has now 'looked' at the screen, so should they later
	 * execute 'Wait(output)', they will actually need to wait for output
	 * from the host.  output_wait_needed is cleared by sms_host_output,
	 * which is called from the write logic in ctlr.c.
	 */
	if (buf == ea_buf)
		output_wait_needed = True;

	if (!rows || !cols)
		return;

	for (r = start_row; r < start_row + rows; r++) {
		/* New row. */
		if (o == NULL)
			o = Tcl_NewListObj(0, NULL);
		if (row != NULL)
			Tcl_ListObjAppendElement(sms_interp, o, row);
		if (in_ascii)
			row = Tcl_NewObj();
		else
			row = Tcl_NewListObj(0, NULL);

		for (c = start_col; c < start_col + cols; c++) {
			int loc = (r * rel_cols) + c;

			if (in_ascii) {
				int len;
				char mb[16];
				ucs4_t uc;

				if (FA_IS_ZERO(get_field_attribute(loc))) {
					mb[0] = ' ';
					mb[1] = '\0';
					len = 2;
				} else

#if defined(X3270_DBCS) /*[*/
				if (IS_LEFT(ctlr_dbcs_state(loc))) {
				    	len = ebcdic_to_multibyte(
						(buf[loc].cc << 8) |
						 buf[loc + 1].cc,
						mb, sizeof(mb));
				} else if (IS_RIGHT(ctlr_dbcs_state(loc))) {
					continue;
				} else
#endif /*]*/
				{
				    	len = ebcdic_to_multibyte_x(
						buf[loc].cc,
						buf[loc].cs & CS_MASK,
						mb, sizeof(mb), True,
						&uc);
				}
				if (len > 0)
					Tcl_AppendToObj(row, mb, len - 1);
			} else {
				char s[5];

				(void) sprintf(s, "0x%02x", buf[loc].cc);
				Tcl_ListObjAppendElement(sms_interp, row,
					Tcl_NewStringObj(s, -1));
			}

		}
	}

	/* Return it. */
	if (row) {
		if (o) {
			Tcl_ListObjAppendElement(sms_interp, o, row);
			Tcl_SetObjResult(sms_interp, o);
		} else
			Tcl_SetObjResult(sms_interp, row);
	}
}

static void
dump_fixed(String params[], Cardinal count, const char *name, Boolean in_ascii,
	struct ea *buf, int rel_rows, int rel_cols, int caddr)
{
	int row, col, len, rows = 0, cols = 0;

	switch (count) {
	    case 0:	/* everything */
		row = 0;
		col = 0;
		len = rel_rows*rel_cols;
		break;
	    case 1:	/* from cursor, for n */
		row = caddr / rel_cols;
		col = caddr % rel_cols;
		len = atoi(params[0]);
		break;
	    case 3:	/* from (row,col), for n */
		row = atoi(params[0]);
		col = atoi(params[1]);
		len = atoi(params[2]);
		break;
	    case 4:	/* from (row,col), for rows x cols */
		row = atoi(params[0]);
		col = atoi(params[1]);
		rows = atoi(params[2]);
		cols = atoi(params[3]);
		len = 0;
		break;
	    default:
		popup_an_error("%s requires 0, 1, 3 or 4 arguments", name);
		return;
	}

	if (
	    (row < 0 || row > rel_rows || col < 0 || col > rel_cols || len < 0) ||
	    ((count < 4)  && ((row * rel_cols) + col + len > rel_rows * rel_cols)) ||
	    ((count == 4) && (cols < 0 || rows < 0 ||
			      col + cols > rel_cols || row + rows > rel_rows))
	   ) {
		popup_an_error("%s: Invalid argument", name);
		return;
	}
	if (count < 4)
		dump_range((row * rel_cols) + col, len, in_ascii, buf,
			rel_rows, rel_cols);
	else
		dump_rectangle(row, col, rows, cols, in_ascii, buf, rel_cols);
}

static void
dump_field(Cardinal count, const char *name, Boolean in_ascii)
{
	int start, baddr;
	int len = 0;

	if (count != 0) {
		popup_an_error("%s requires 0 arguments", name);
		return;
	}
	if (!formatted) {
		popup_an_error("%s: Screen is not formatted", name);
		return;
	}
	start = find_field_attribute(cursor_addr);
	INC_BA(start);
	baddr = start;
	do {
		if (ea_buf[baddr].fa)
			break;
		len++;
		INC_BA(baddr);
	} while (baddr != start);
	dump_range(start, len, in_ascii, ea_buf, ROWS, COLS);
}

void
Ascii_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	dump_fixed(params, *num_params, action_name(Ascii_action), True,
		ea_buf, ROWS, COLS, cursor_addr);
}

void
AsciiField_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params)
{
	dump_field(*num_params, action_name(AsciiField_action), True);
}

void
Ebcdic_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	dump_fixed(params, *num_params, action_name(Ebcdic_action), False,
		ea_buf, ROWS, COLS, cursor_addr);
}

void
EbcdicField_action(Widget w unused, XEvent *event unused,
    String *params unused, Cardinal *num_params)
{
	dump_field(*num_params, action_name(EbcdicField_action), False);
}

/* "Status" action, returns the s3270 prompt. */
static char *
status_string(void)
{
	char kb_stat;
	char fmt_stat;
	char prot_stat;
	char *connect_stat = NULL;
	char em_mode;
	char s[1024];
	char *r;

	if (!kybdlock)
		kb_stat = 'U';
	else if (!CONNECTED || KBWAIT)
		kb_stat = 'L';
	else
		kb_stat = 'E';

	if (formatted)
		fmt_stat = 'F';
	else
		fmt_stat = 'U';

	if (!formatted)
		prot_stat = 'U';
	else {
		unsigned char fa;

		fa = get_field_attribute(cursor_addr);
		if (FA_IS_PROTECTED(fa))
			prot_stat = 'P';
		else
			prot_stat = 'U';
	}

	if (CONNECTED)
		connect_stat = xs_buffer("C(%s)", current_host);
	else
		connect_stat = NewString("N");

	if (CONNECTED) {
		if (IN_ANSI) {
			extern int linemode; /* XXX */
			if (linemode)
				em_mode = 'L';
			else
				em_mode = 'C';
		} else if (IN_SSCP)
			em_mode = 'S';
		else if (IN_3270)
			em_mode = 'I';
		else
			em_mode = 'P';
	} else
		em_mode = 'N';

	(void) sprintf(s, "%c %c %c %s %c %d %d %d %d %d",
	    kb_stat,
	    fmt_stat,
	    prot_stat,
	    connect_stat,
	    em_mode,
	    model_num,
	    ROWS, COLS,
	    cursor_addr / COLS, cursor_addr % COLS);
	r = NewString(s);
	Free(connect_stat);
	return r;
}

void
Status_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	char *s;

	s = status_string();
	Tcl_SetResult(sms_interp, s, TCL_VOLATILE);
	Free(s);
}

static unsigned char
calc_cs(unsigned char cs)
{
	switch (cs & CS_MASK) { 
	case CS_APL:
	    return 0xf1;
	case CS_LINEDRAW:
	    return 0xf2;
	case CS_DBCS:
	    return 0xf8;
	default:
	    return 0x00;
	}
}

/*
 * Internals of the ReadBuffer action.
 * Operates on the supplied 'buf' parameter, which might be the live
 * screen buffer 'ea_buf' or a copy saved with 'Snap'.
 */
static void
do_read_buffer(String *params, Cardinal num_params, struct ea *buf)
{
	Tcl_Obj *o = NULL;
	Tcl_Obj *row = NULL;
	register int	baddr;
	unsigned char	current_fg = 0x00;
	unsigned char	current_gr = 0x00;
	unsigned char	current_cs = 0x00;
	char field_buf[1024];
	Boolean in_ebcdic = False;

	if (num_params > 0) {
		if (num_params > 1) {
			popup_an_error("%s: extra agruments",
					action_name(ReadBuffer_action));
			return;
		}
		if (!strncasecmp(params[0], "Ascii", strlen(params[0])))
			in_ebcdic = False;
		else if (!strncasecmp(params[0], "Ebcdic", strlen(params[0])))
			in_ebcdic = True;
		else {
			popup_an_error("%s: first parameter must be "
					"Ascii or Ebcdic",
					action_name(ReadBuffer_action));
			return;
		}
					                                        
	}

	baddr = 0;
	do {
		if (!(baddr % COLS)) {
			/* New row. */
			if (o == NULL)
				o = Tcl_NewListObj(0, NULL);
			if (row != NULL)
				Tcl_ListObjAppendElement(sms_interp, o, row);
			row = Tcl_NewListObj(0, NULL);
		}
		if (buf[baddr].fa) {
			char *s = field_buf;
			s += sprintf(s, "SF(%02x=%02x", XA_3270,
					buf[baddr].fa);
			if (buf[baddr].fg)
				s += sprintf(s, ",%02x=%02x", XA_FOREGROUND,
						buf[baddr].fg);
			if (buf[baddr].gr)
				s += sprintf(s, ",%02x=%02x", XA_HIGHLIGHTING,
						buf[baddr].gr | 0xf0);
			if (buf[baddr].cs & CS_MASK)
				s += sprintf(s, ",%02x=%02x", XA_CHARSET,
					     calc_cs(buf[baddr].cs));
			s += sprintf(s, ")");
			Tcl_ListObjAppendElement(sms_interp, row,
				Tcl_NewStringObj(field_buf, -1));
		} else {
			if (buf[baddr].fg != current_fg) {
				sprintf(field_buf, "SA(%02x=%02x)",
						XA_FOREGROUND,
						buf[baddr].fg);
				Tcl_ListObjAppendElement(sms_interp, row,
					Tcl_NewStringObj(field_buf, -1));
				current_fg = buf[baddr].fg;
			}
			if (buf[baddr].gr != current_gr) {
				sprintf(field_buf, "SA(%02x=%02x)",
						XA_HIGHLIGHTING,
						buf[baddr].gr | 0xf0);
				Tcl_ListObjAppendElement(sms_interp, row,
					Tcl_NewStringObj(field_buf, -1));
				current_gr = buf[baddr].gr;
			}
			if ((buf[baddr].cs & ~CS_GE) !=
					(current_cs & ~CS_GE)) {
				sprintf(field_buf, "SA(%02x=%02x)",
						XA_CHARSET,
						calc_cs(buf[baddr].cs));
				Tcl_ListObjAppendElement(sms_interp, row,
					Tcl_NewStringObj(field_buf, -1));
				current_cs = buf[baddr].cs;
			}
			if (in_ebcdic) {
				if (buf[baddr].cs & CS_GE)
					sprintf(field_buf, "GE(%02x)",
							buf[baddr].cc);
				else
					sprintf(field_buf, "%02x",
							buf[baddr].cc);
				Tcl_ListObjAppendElement(sms_interp, row,
					Tcl_NewStringObj(field_buf, -1));
			} else {
				int len;
				char mb[16];
				int j;
				ucs4_t uc;

#if defined(X3270_DBCS) /*[*/
				if (IS_LEFT(ctlr_dbcs_state(baddr))) {
					len = ebcdic_to_multibyte(
						(buf[baddr].cc << 8) |
						 buf[baddr + 1].cc,
						mb, sizeof(mb));
					field_buf[0] = '\0';
					for (j = 0; j < len - 1; j++)
					    	sprintf(strchr(field_buf, '\0'),
							    "%02x",
							    mb[j] & 0xff);
				} else if (IS_RIGHT(ctlr_dbcs_state(baddr))) {
					strcpy(field_buf, " -");
				} else
#endif /*]*/
				if (buf[baddr].cc == EBC_null)
					strcpy(field_buf, "00");
				else {
					len = ebcdic_to_multibyte_x(
						buf[baddr].cc,
						buf[baddr].cs & CS_MASK,
						mb, sizeof(mb), True,
						&uc);
					field_buf[0] = '\0';
					for (j = 0; j < len - 1; j++)
					    	sprintf(strchr(field_buf, '\0'),
							    "%02x",
							    mb[j] & 0xff);
				}

				Tcl_ListObjAppendElement(sms_interp, row,
					Tcl_NewStringObj(field_buf, -1));
			}
		}
		INC_BA(baddr);
	} while (baddr != 0);

	if (row) {
		if (o) {
			Tcl_ListObjAppendElement(sms_interp, o, row);
			Tcl_SetObjResult(sms_interp, o);
		} else
			Tcl_SetObjResult(sms_interp, row);
	}
}

/*
 * ReadBuffer action.
 */
void
ReadBuffer_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	do_read_buffer(params, *num_params, ea_buf);
}

/*
 * "Snap" action, maintains a snapshot for consistent multi-field comparisons:
 *
 *  Snap Save
 *	updates the saved image from the live image
 *  Snap Rows
 *	returns the number of rows
 *  Snap Cols
 *	returns the number of columns
 *  Snap Staus
 *  Snap Ascii ...
 *  Snap AsciiField (not yet)
 *  Snap Ebcdic ...
 *  Snap EbcdicField (not yet)
 *	runs the named command
 *  Snap Wait [tmo] Output
 *	waits for the screen to change
 */

static char *snap_status = NULL;
static struct ea *snap_buf = NULL;
static int snap_rows = 0;
static int snap_cols = 0;
static int snap_field_start = -1;
static int snap_field_length = -1;
static int snap_caddr = 0;

static void
snap_save(void)
{
	output_wait_needed = True;
	Replace(snap_status, status_string());

	Replace(snap_buf, (struct ea *)Malloc(sizeof(struct ea) * ROWS*COLS));
	(void) memcpy(snap_buf, ea_buf, sizeof(struct ea) * ROWS*COLS);

	snap_rows = ROWS;
	snap_cols = COLS;

	if (!formatted) {
		snap_field_start = -1;
		snap_field_length = -1;
	} else {
		int baddr;

		snap_field_length = 0;
		snap_field_start = find_field_attribute(cursor_addr);
		INC_BA(snap_field_start);
		baddr = snap_field_start;
		do {
			if (ea_buf[baddr].fa)
				break;
			snap_field_length++;
			INC_BA(baddr);
		} while (baddr != snap_field_start);
	}
	snap_caddr = cursor_addr;
}

void
Snap_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	char nbuf[16];

	if (*num_params == 0) {
		snap_save();
		return;
	}

	/* Handle 'Snap Wait' separately. */
	if (!strcasecmp(params[0], action_name(Wait_action))) {
		long tmo = -1;
		char *ptr;
		unsigned maxp = 0;

		if (*num_params > 1 &&
		    (tmo = strtol(params[1], &ptr, 10)) >= 0 &&
		    ptr != params[0] &&
		    *ptr == '\0') {
			maxp = 3;
		} else {
			tmo = -1;
			maxp = 2;
		}
		if (*num_params > maxp) {
			popup_an_error("Too many arguments to %s %s",
			    action_name(Snap_action),
			    action_name(Wait_action));
			    return;
		}
		if (*num_params < maxp) {
			popup_an_error("Too few arguments to %s %s",
			    action_name(Snap_action),
			    action_name(Wait_action));
			    return;
		}
		if (strcasecmp(params[*num_params - 1], "Output")) {
			popup_an_error("Unknown parameter to %s %s",
			action_name(Snap_action),
			action_name(Wait_action));
			return;
		}

		/* Must be connected. */
		if (!(CONNECTED || HALF_CONNECTED)) {
			popup_an_error("%s: Not connected",
			    action_name(Snap_action));
			return;
		}

		/*
		 * Make sure we need to wait.
		 * If we don't, then Snap Wait Output is equivalen to Snap Save.
		 */
		if (!output_wait_needed) {
			snap_save();
			return;
		}

		/* Set the new state. */
		waiting = AWAITING_SOUTPUT;

		/* Set up a timeout, if they want one. */
		if (tmo >= 0)
			wait_id = AddTimeOut(tmo? (tmo * 1000): 1,
					wait_timed_out);
		return;
	}

	if (!strcasecmp(params[0], "Save")) {
		if (*num_params != 1) {
			popup_an_error("Extra argument(s)");
			return;
		}
		snap_save();
	} else if (!strcasecmp(params[0], "Status")) {
		if (*num_params != 1) {
			popup_an_error("Extra argument(s)");
			return;
		}
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		Tcl_SetResult(sms_interp, snap_status, TCL_VOLATILE);
	} else if (!strcasecmp(params[0], "Rows")) {
		if (*num_params != 1) {
			popup_an_error("Extra argument(s)");
			return;
		}
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		(void) sprintf(nbuf, "%d", snap_rows);
		Tcl_SetResult(sms_interp, nbuf, TCL_VOLATILE);
	} else if (!strcasecmp(params[0], "Cols")) {
		if (*num_params != 1)
			popup_an_error("extra argument(s)");
		(void) sprintf(nbuf, "%d", snap_cols);
		Tcl_SetResult(sms_interp, nbuf, TCL_VOLATILE);
	} else if (!strcasecmp(params[0], action_name(Ascii_action))) {
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		dump_fixed(params + 1, *num_params - 1,
			action_name(Ascii_action), True, snap_buf,
			snap_rows, snap_cols, snap_caddr);
	} else if (!strcasecmp(params[0], action_name(Ebcdic_action))) {
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		dump_fixed(params + 1, *num_params - 1,
			action_name(Ebcdic_action), False, snap_buf,
			snap_rows, snap_cols, snap_caddr);
	} else if (!strcasecmp(params[0], action_name(ReadBuffer_action))) {
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		do_read_buffer(params + 1, *num_params - 1, snap_buf);
	} else {
		popup_an_error("%s: Argument must be Save, Status, Rows, Cols, "
		    "%s, %s, %s or %s",
		    action_name(Snap_action),
		    action_name(Wait_action),
		    action_name(Ascii_action),
		    action_name(Ebcdic_action),
		    action_name(ReadBuffer_action));
	}
}

static void
wait_timed_out(void)
{
	popup_an_error("Wait timed out");
	wait_id = 0L;
	UNBLOCK();
}

void
Wait_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	long tmo = -1;
	char *ptr;
	Cardinal np;
	String *pr;

	if (*num_params > 0 &&
	    (tmo = strtol(params[0], &ptr, 10)) >= 0 &&
	     ptr != params[0] &&
	     *ptr == '\0') {
		np = *num_params - 1;
		pr = params + 1;
	 } else {
		tmo = -1;
		np = *num_params;
		pr = params;
	}

	if (np == 0) {
		if (!CONNECTED) {
			popup_an_error("Not connected");
			return;
		}
		if (!INPUT_OKAY)
			waiting = AWAITING_IFIELD;
		return;
	}
	if (np != 1) {
		popup_an_error("Too many parameters");
		return;
	}
	if (!strcasecmp(pr[0], "InputField")) {
		/* Same as no parameters. */
		if (!CONNECTED) {
			popup_an_error("Not connected");
			return;
		}
		if (!INPUT_OKAY)
			waiting = AWAITING_IFIELD;
	} else if (!strcasecmp(pr[0], "Output")) {
		if (!CONNECTED) {
			popup_an_error("Not connected");
			return;
		}
		if (output_wait_needed)
			waiting = AWAITING_OUTPUT;
	} else if (!strcasecmp(pr[0], "3270") ||
		   !strcasecmp(pr[0], "3270Mode")) {
		if (!CONNECTED) {
			popup_an_error("Not connected");
			return;
		}
		if (!IN_3270)
			waiting = AWAITING_3270;
	} else if (!strcasecmp(pr[0], "ansi") ||
		   !strcasecmp(pr[0], "NVTMode")) {
		if (!CONNECTED) {
			popup_an_error("Not connected");
			return;
		}
		if (!IN_ANSI)
			waiting = AWAITING_NVT;
	} else if (!strcasecmp(pr[0], "Disconnect")) {
		if (CONNECTED)
			waiting = AWAITING_DISCONNECT;
	} else if (!strcasecmp(pr[0], "Unlock")) {
		if (CONNECTED && KBWAIT)
			waiting = AWAITING_UNLOCK;
	} else {
		popup_an_error("Unknown Wait type: %s", pr[0]);
		return;
	}

	if (waiting != NOT_WAITING && tmo >= 0)
		wait_id = AddTimeOut(tmo? (tmo * 1000L): 1, wait_timed_out);
}

static int
Rows_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
		Tcl_Obj *CONST objv[])
{
	char buf[32];

	if (objc > 1) {
		Tcl_SetResult(interp, "Too many arguments", TCL_STATIC);
		return TCL_ERROR;
	}
	(void) sprintf(buf, "%d", ROWS);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_OK;
}

static int
Cols_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
		Tcl_Obj *CONST objv[])
{
	char buf[32];

	if (objc > 1) {
		Tcl_SetResult(interp, "Too many arguments", TCL_STATIC);
		return TCL_ERROR;
	}
	(void) sprintf(buf, "%d", COLS);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_OK;
}

/* Generate a response to a script command. */
void
sms_info(const char *fmt, ...)
{
	va_list args;
	char buf[4096];

	va_start(args, fmt);
	(void) vsprintf(buf, fmt, args);
	va_end(args);
	Tcl_AppendResult(sms_interp, buf, NULL);
}

/*
 * Return True if there is a pending macro.
 */
Boolean
sms_in_macro(void)
{
	return pending_string != NULL;
}

/* Like fcatv, but goes to a dynamically-allocated buffer. */
static char *
tc_scatv(char *s)
{
#define ALLOC_INC	1024
	char *buf;
	int buflen;
	int bufused = 0;
	char c;
#define add_space(n) \
		if (bufused + (n) >= buflen) { \
			buflen += ALLOC_INC; \
			buf = Realloc(buf, buflen); \
		} \
		bufused += (n);

	buf = Malloc(ALLOC_INC);
	buflen = ALLOC_INC;
	*buf = '\0';

	while ((c = *s++))  {
		switch (c) {
		case '\n':
			add_space(2);
			(void) strcat(buf, "\\n");
			break;
		case '\t':
			add_space(2);
			(void) strcat(buf, "\\t");
			break;
		case '\b':
			add_space(2);
			(void) strcat(buf, "\\b");
			break;
		case '\f':
			add_space(2);
			(void) strcat(buf, "\\f");
			break;
		case ' ':
			add_space(2);
			(void) strcat(buf, "\\ ");
			break;
		default:
			if ((c & 0x7f) < ' ') {
				add_space(4);
				(void) sprintf(buf + bufused, "\\%03o",
					c & 0xff);
				break;
			} else {
				add_space(1);
				*(buf + bufused - 1) = c;
				*(buf + bufused) = '\0';
			}
		}
	}
	return buf;
#undef add_space
}
