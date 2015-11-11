/*
 * Copyright (c) 1993-2009, 2013-2015 Paul Mattes.
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

#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actions.h"
#include "charset.h"
#include "ctlrc.h"
#include "ft.h"
#include "glue.h"
#include "host.h"
#include "kybd.h"
#include "lazya.h"
#include "macros.h"
#include "nvt.h"
#include "opts.h"
#include "popups.h"
#include "print_screen.h"
#include "screen.h"
#include "selectc.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "unicodec.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"
#include "xio.h"

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
    NOT_WAITING,	/* Not waiting */
    AWAITING_CONNECT,	/* Connect (negotiation completion) */
    AWAITING_RESET,	/* Keyboard locked */
    AWAITING_FT,	/* File transfer in progress */
    AWAITING_IFIELD,	/* Wait InputField */
    AWAITING_3270,	/* Wait 3270Mode */
    AWAITING_NVT,	/* Wait NVTMode */
    AWAITING_OUTPUT,	/* Wait Output */
    AWAITING_SOUTPUT,	/* Snap Wait */
    AWAITING_DISCONNECT,/* Wait Disconnect */
    AWAITING_UNLOCK	/* Wait Unlock */
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
static ioid_t wait_id = NULL_IOID;
static ioid_t command_timeout_id = NULL_IOID;
static int cmd_ret;
static char *action = NULL;
static bool interactive = false;
static action_t Ascii_action;
static action_t AsciiField_action;
static action_t Ebcdic_action;
static action_t EbcdicField_action;
static action_t Status_action;
static action_t ReadBuffer_action;
static action_t Snap_action;
static action_t Wait_action;
static action_t Query_action;

/* Local prototypes. */
static void ps_clear(void);
static int tcl3270_main(int argc, const char *argv[]);
static void negotiate(void);
static char *tc_scatv(const char *s);
static void snap_save(void);
static void wait_timed_out(ioid_t);
static void tcl3270_register(void);

/* Macros.c stuff. */
static bool in_cmd = false;
static Tcl_Interp *sms_interp;
static bool output_wait_needed = false;
static char *pending_string = NULL;
static char *pending_string_ptr = NULL;
static bool pending_hex = false;
bool macro_output = false;

/* Is the keyboard is locked due to user input? */
#define KBWAIT	(kybdlock & (KL_OIA_LOCKED|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK))
#define CKBWAIT (toggled(AID_WAIT) && KBWAIT)

/* Is it safe to continue a script waiting for an input field? */
#define INPUT_OKAY ( \
    IN_SSCP || \
    (IN_3270 && formatted && cursor_addr && !CKBWAIT) || \
    (IN_NVT && !(kybdlock & KL_AWAITING_FIRST)) \
)

/* Is is safe to continue a script waiting for the connection to complete? */
#define CONNECT_DONE	(IN_SSCP || IN_3270 || IN_NVT)

/* Shorthand macro to unlock the current action. */
#define UNBLOCK() { \
	vtrace("Unblocked %s (%s)\n", action, unwait_name[waiting]); \
	waiting = NOT_WAITING; \
	if (wait_id != NULL_IOID) { \
		RemoveTimeOut(wait_id); \
		wait_id = NULL_IOID; \
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
    Tcl_Main(argc, argv, Tcl_AppInit);
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
    unsigned i;
    int j;
    Tcl_Obj *argv_obj;
    action_elt_t *e;

    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /*
     * Call the module registration functions, to build up the tables of
     * actions, options and callbacks.
     */
    ctlr_register();
    ft_register();
    host_register();
    kybd_register();
    nvt_register();
    print_screen_register();
    tcl3270_register();
    toggles_register();
    trace_register();
    xio_register();

    /* Use argv and argv0 to figure out our command-line arguments. */
    s0 = Tcl_GetVar(interp, "argv0", 0);
    if (s0 == NULL) {
	return TCL_ERROR;
    }
    s = Tcl_GetVar(interp, "argv", 0);
    if (s == NULL) {
	return TCL_ERROR;
    }
    (void) Tcl_SplitList(interp, s, &tcl_argc, &tcl_argv);
    argc = tcl_argc + 1;
    argv = (const char **)Malloc((argc + 1) * sizeof(char *));
    argv[0] = s0;
    for (j = 0; j < tcl_argc; j++) {
	argv[1 + j] = tcl_argv[j];
    }
    argv[argc] = NULL;

    /* Find out if we're interactive. */
    s = Tcl_GetVar(interp, "tcl_interactive", 0);
    interactive = (s != NULL && !strcmp(s, "1"));

    /* Call main. */
    if (tcl3270_main(argc, argv) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /* Replace tcl's argc and argv with whatever was left. */
    argv_obj = Tcl_NewListObj(0, NULL);
    for (i = 1; argv[i] != NULL; i++) {
	Tcl_ListObjAppendElement(interp, argv_obj, Tcl_NewStringObj(argv[i],
		strlen(argv[i])));
    }
    Tcl_SetVar2Ex(interp, "argv", NULL, argv_obj, 0);
    Tcl_SetVar(interp, "argc", lazyaf("%d", i? i - 1: 0), 0);

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
    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (Tcl_CreateObjCommand(interp, e->t.name, x3270_cmd, NULL, NULL)
		== NULL) {
	    return TCL_ERROR;
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    if (Tcl_CreateObjCommand(interp, "Rows", Rows_cmd, NULL, NULL) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_CreateObjCommand(interp, "Cols", Cols_cmd, NULL, NULL) == NULL) {
	return TCL_ERROR;
    }

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
usage(const char *msg)
{
    const char *sn = "";

    if (!strcmp(programname, "tcl3270")) {
	sn = " [scriptname]";
    }

    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, "Usage: %s%s [tcl3270-options] [host] [-- script-args]\n"
"       <host> is [ps:][LUname@]hostname[:port]\n",
	    programname, sn);
    fprintf(stderr, "Options:\n");
    cmdline_help(false);
    exit(1);
}

/*
 * Called when the host connects, disconnects, or changes modes.
 * When we connect or change modes, clear the screen.
 * When we disconnect, clear the pending string, so we don't keep trying to
 * feed it to a dead host.
 */
static void
tcl3270_connect(bool ignored)
{
    if (CONNECTED) {
	ctlr_erase(true);
	/* Check for various wait conditions. */
	switch (waiting) {
	case AWAITING_CONNECT:
	    if (CONNECT_DONE) {
		    UNBLOCK();
	    }
	    break;
	case AWAITING_3270:
	    if (IN_3270) {
		UNBLOCK();
	    }
	    break;
	case AWAITING_NVT:
	    if (IN_NVT) {
		UNBLOCK();
	    }
	    break;
	default:
	    /* Nothing we can figure out here. */
	    break;
	}
    } else {
	if (appres.disconnect_clear) {
	    ctlr_erase(true);
	}
	ps_clear();

	/* Cause (almost) any pending Wait command to fail. */
	if (waiting != NOT_WAITING) {
	    if (waiting == AWAITING_DISCONNECT) {
		UNBLOCK();
	    } else {
		vtrace("Unblocked %s (was '%s') -- failure\n", action,
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
    const char	*cl_hostname = NULL;

    argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

    /* Set tcl3270-specific defaults. */
    appres.utf8 = true;

    if (charset_init(appres.charset) != CS_OKAY) {
	xs_warning("Cannot find charset \"%s\"", appres.charset);
	(void) charset_init(NULL);
    }
    model_init();
    ctlr_init(-1);
    ctlr_reinit(-1);
    ft_init();

    /* Make sure we don't fall over any SIGPIPEs. */
    (void) signal(SIGPIPE, SIG_IGN);

    /* Handle initial toggle settings. */
    initialize_toggles();

#if defined(HAVE_LIBSSL) /*[*/
    ssl_base_init(NULL, NULL);
#endif /*]*/

    /* Connect to the host, and wait for negotiation to complete. */
    if (cl_hostname != NULL) {
	action = NewString("[initial connection]");
	if (!host_connect(cl_hostname)) {
	    exit(1);
	}
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
    if (pending_string_ptr == NULL || waiting != NOT_WAITING) {
	return;
    }

    if (pending_hex) {
	hex_input(pending_string_ptr);
	ps_clear();
    } else {
	int len = strlen(pending_string_ptr);
	int len_left;

	len_left = emulate_input(pending_string_ptr, len, false);
	if (len_left) {
	    pending_string_ptr += len - len_left;
	    return;
	} else {
	    ps_clear();
	}
    }
    if (CKBWAIT) {
	vtrace("Blocked %s (keyboard locked)\n", action);
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
command_timed_out(ioid_t id _is_unused)
{
    popup_an_error("Command timed out after %ds.\n",
	    appres.tcl3270.command_timeout);
    command_timeout_id = NULL_IOID;

    /* Let the command complete unsuccessfully. */
    UNBLOCK();
}

/* The tcl "x3270" command: The root of all 3270 access. */
static int
x3270_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    unsigned j;
    unsigned count;
    const char **argv = NULL;
    int old_mode;
    action_elt_t *e;
    bool found;

    /* Set up ugly global variables. */
    in_cmd = true;
    sms_interp = interp;

    /* Synchronously run any pending I/O's and timeouts.  Ugly. */
    old_mode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    while (process_events(false)) {
	    ;
    }
    (void) Tcl_SetServiceMode(old_mode);

    /* Verify minimal command syntax. */
    if (objc < 1) {
	Tcl_SetResult(interp, "Missing action name", TCL_STATIC);
	return TCL_ERROR;
    }

    /* Look up the action. */
    Replace(action, NewString(Tcl_GetString(objv[0])));
    found = false;
    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (!strcmp(action, e->t.name)) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    if (!found) {
	Tcl_SetResult(interp, "No such action", TCL_STATIC);
	return TCL_ERROR;
    }

    /* Stage the arguments. */
    count = objc - 1;
    if (count) {
	argv = (const char **)Malloc(count*sizeof(char *));
	for (j = 0; j < count; j++) {
	    argv[j] = Tcl_GetString(objv[j + 1]);
	}
    }

    /* Trace what we're about to do. */
    if (toggled(TRACING)) {
	vtrace("Running %s", action);
	for (j = 0; j < count; j++) {
	    char *s;

	    s = tc_scatv(argv[j]);
	    vtrace(" %s", s);
	    Free(s);
	}
	vtrace("\n");
    }

    /* Set up more ugly global variables and run the action. */
    ia_cause = IA_SCRIPT;
    cmd_ret = TCL_OK;
    run_action_entry(e, IA_SCRIPT, count, argv);

    /* Set implicit wait state. */
    if (ft_state != FT_NONE) {
	waiting = AWAITING_FT;
    } else if ((waiting == NOT_WAITING) && CKBWAIT) {
	waiting = AWAITING_RESET;
    }

    if (waiting != NOT_WAITING) {
	vtrace("Blocked %s (%s)\n", action, wait_name[waiting]);
	if (appres.tcl3270.command_timeout) {
	    command_timeout_id = AddTimeOut(appres.tcl3270.command_timeout *
		    1000, command_timed_out);
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
	(void) process_events(true);

	/*
	 * Check for the completion of output-related wait conditions.
	 */
	switch (waiting) {
	case AWAITING_IFIELD:
	    if (INPUT_OKAY) {
		UNBLOCK();
	    }
	    break;
	case AWAITING_RESET:
	    if (!CKBWAIT) {
		UNBLOCK();
	    }
	    break;
	case AWAITING_FT:
	    if (ft_state == FT_NONE) {
		UNBLOCK();
	    }
	    break;
	case AWAITING_UNLOCK:
	    if (!KBWAIT) {
		UNBLOCK();
	    }
	default:
	    break;
	}

	/* Push more string text in. */
	process_pending_string();
    }
    if (command_timeout_id != NULL_IOID) {
	RemoveTimeOut(command_timeout_id);
	command_timeout_id = NULL_IOID;
    }
    if (toggled(TRACING)) {
	const char *s;
#	define TRUNC_LEN 40
	char s_trunc[TRUNC_LEN + 1];

	s = Tcl_GetStringResult(interp);
	vtrace("Completed %s (%s)", action,
		(cmd_ret == TCL_OK)? "ok": "error");
	if (s != NULL && *s) {
	    char buf[1024];

	    strncpy(s_trunc, s, TRUNC_LEN);
	    s_trunc[TRUNC_LEN] = '\0';
	    vtrace(" -> \"%s\"", scatv(s_trunc, buf, sizeof(buf)));
	    if (strlen(s) > TRUNC_LEN) {
		vtrace("...(%d chars)", (int)strlen(s));
	    }
	}
	vtrace("\n");
    }
    (void) Tcl_SetServiceMode(old_mode);
    in_cmd = false;
    sms_interp = NULL;
    if (argv) {
	Free(argv);
    }
    return cmd_ret;
}

/* Do initial connect negotiation. */
void
negotiate(void)
{
    int old_mode;

    old_mode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    while (CKBWAIT || (waiting == AWAITING_CONNECT && !CONNECT_DONE)) {
	(void) process_events(true);
	if (!PCONNECTED) {
	    exit(1);
	}
    }
    (void) Tcl_SetServiceMode(old_mode);
}

/* Indicates whether errors should go to stderr, or be returned to tcl. */
bool
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

/* Also a no-op. */
void
sms_accumulate_time(struct timeval *t0 _is_unused,
	struct timeval *t1 _is_unused)
{
}

/* Set the pending string.  Used by the 'String' action. */
void
ps_set(char *s, bool is_hex)
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
    output_wait_needed = false;
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
dump_range(int first, int len, bool in_ascii, struct ea *buf,
    int rel_rows _is_unused, int rel_cols)
{
    int i;
    Tcl_Obj *o = NULL;
    Tcl_Obj *row = NULL;
    bool is_zero = false;

    /*
     * The client has now 'looked' at the screen, so should they later
     * execute 'Wait(output)', they will actually need to wait for output
     * from the host.  output_wait_needed is cleared by sms_host_output,
     * which is called from the write logic in ctlr.c.
     */
    if (buf == ea_buf) {
	output_wait_needed = true;
    }

    is_zero = FA_IS_ZERO(get_field_attribute(first));

    for (i = 0; i < len; i++) {

	/* Check for a new row. */
	if (i && !((first + i) % rel_cols)) {
	    /* Done with this row. */
	    if (o == NULL) {
		o = Tcl_NewListObj(0, NULL);
	    }
	    Tcl_ListObjAppendElement(sms_interp, o, row);
	    row = NULL;
	}
	if (!row) {
	    if (in_ascii) {
		row = Tcl_NewObj();
	    } else {
		row = Tcl_NewListObj(0, NULL);
	    }
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
	    } else if (IS_LEFT(ctlr_dbcs_state(first + i))) {
		len = ebcdic_to_multibyte((buf[first + i].cc << 8) |
			    buf[first + i + 1].cc,
			mb, sizeof(mb));
	    } else if (IS_RIGHT(ctlr_dbcs_state(first + i))) {
		continue;
	    } else {
		len = ebcdic_to_multibyte_x(buf[first + i].cc,
			buf[first + i].cs & CS_MASK, mb, sizeof(mb),
			EUO_BLANK_UNDEF, &uc);
	    }
	    if (len > 0) {
		Tcl_AppendToObj(row, mb, len - 1);
	    }
	} else {
	    char *s;

	    s = xs_buffer("0x%02x", buf[first + i].cc);
	    Tcl_ListObjAppendElement(sms_interp, row, Tcl_NewStringObj(s, -1));
	    Free(s);
	}
    }

    /* Return it. */
    if (row) {
	if (o) {
	    Tcl_ListObjAppendElement(sms_interp, o, row);
	    Tcl_SetObjResult(sms_interp, o);
	} else {
	    Tcl_SetObjResult(sms_interp, row);
	}
    }
}

static void
dump_rectangle(int start_row, int start_col, int rows, int cols,
    bool in_ascii, struct ea *buf, int rel_cols)
{
    int r, c;
    Tcl_Obj *o = NULL;
    Tcl_Obj *row = NULL;

    /*
     * The client has now 'looked' at the screen, so should they later
     * execute 'Wait(output)', they will actually need to wait for output
     * from the host.  output_wait_needed is cleared by sms_host_output,
     * which is called from the write logic in ctlr.c.
     */
    if (buf == ea_buf) {
	output_wait_needed = true;
    }

    if (!rows || !cols) {
	return;
    }

    for (r = start_row; r < start_row + rows; r++) {
	/* New row. */
	if (o == NULL) {
	    o = Tcl_NewListObj(0, NULL);
	}
	if (row != NULL) {
	    Tcl_ListObjAppendElement(sms_interp, o, row);
	}
	if (in_ascii) {
	    row = Tcl_NewObj();
	} else {
	    row = Tcl_NewListObj(0, NULL);
	}

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
		} else if (IS_LEFT(ctlr_dbcs_state(loc))) {
		    len = ebcdic_to_multibyte((buf[loc].cc << 8) |
			    buf[loc + 1].cc, mb, sizeof(mb));
		} else if (IS_RIGHT(ctlr_dbcs_state(loc))) {
		    continue;
		} else {
		    len = ebcdic_to_multibyte_x(buf[loc].cc,
			    buf[loc].cs & CS_MASK, mb, sizeof(mb),
			    EUO_BLANK_UNDEF, &uc);
		}
		if (len > 0) {
		    Tcl_AppendToObj(row, mb, len - 1);
		}
	    } else {
		char *s;

		s = xs_buffer("0x%02x", buf[loc].cc);
		Tcl_ListObjAppendElement(sms_interp, row,
			Tcl_NewStringObj(s, -1));
		Free(s);
	    }

	}
    }

    /* Return it. */
    if (row) {
	if (o) {
	    Tcl_ListObjAppendElement(sms_interp, o, row);
	    Tcl_SetObjResult(sms_interp, o);
	} else {
	    Tcl_SetObjResult(sms_interp, row);
	}
    }
}

static bool
dump_fixed(const char **params, unsigned count, const char *name,
	bool in_ascii, struct ea *buf, int rel_rows, int rel_cols,
	int caddr)
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
	return false;
    }

    if (
	(row < 0 || row > rel_rows || col < 0 || col > rel_cols || len < 0) ||
	((count < 4)  && ((row * rel_cols) + col + len > rel_rows * rel_cols)) ||
	((count == 4) && (cols < 0 || rows < 0 ||
			  col + cols > rel_cols || row + rows > rel_rows))
       ) {
	popup_an_error("%s: Invalid argument", name);
	return false;
    }
    if (count < 4) {
	dump_range((row * rel_cols) + col, len, in_ascii, buf, rel_rows,
		rel_cols);
    } else {
	dump_rectangle(row, col, rows, cols, in_ascii, buf, rel_cols);
    }

    return true;
}

static bool
dump_field(unsigned count, const char *name, bool in_ascii)
{
    int start, baddr;
    int len = 0;

    if (count != 0) {
	popup_an_error("%s requires 0 arguments", name);
	return false;
    }
    if (!formatted) {
	popup_an_error("%s: Screen is not formatted", name);
	return false;
    }
    start = find_field_attribute(cursor_addr);
    INC_BA(start);
    baddr = start;
    do {
	if (ea_buf[baddr].fa) {
	    break;
	}
	len++;
	INC_BA(baddr);
    } while (baddr != start);
    dump_range(start, len, in_ascii, ea_buf, ROWS, COLS);
    return true;
}

static bool
Ascii_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Ascii", ia, argc, argv);
    return dump_fixed(argv, argc, "Ascii", true, ea_buf, ROWS, COLS,
	    cursor_addr);
}

static bool
AsciiField_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("AsciiField", ia, argc, argv);
    return dump_field(argc, "AsciiField", true);
}

static bool
Ebcdic_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Ebcdic", ia, argc, argv);
    return dump_fixed(argv, argc, "Ebcdic", false, ea_buf, ROWS, COLS,
	    cursor_addr);
}

static bool
EbcdicField_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("EbcdicField", ia, argc, argv);
    return dump_field(argc, "EbcdicField", false);
}

/* "Status" action, returns the s3270 prompt. */
static char *
status_string(void)
{
    char kb_stat;
    char fmt_stat;
    char prot_stat;
    char *connect_stat;
    char em_mode;

    if (!kybdlock) {
	kb_stat = 'U';
    } else if (!CONNECTED || KBWAIT) {
	kb_stat = 'L';
    } else {
	kb_stat = 'E';
    }

    if (formatted) {
	fmt_stat = 'F';
    } else {
	fmt_stat = 'U';
    }

    if (!formatted) {
	prot_stat = 'U';
    } else {
	unsigned char fa;

	fa = get_field_attribute(cursor_addr);
	if (FA_IS_PROTECTED(fa)) {
	    prot_stat = 'P';
	} else {
	    prot_stat = 'U';
	}
    }

    if (CONNECTED) {
	connect_stat = lazyaf("C(%s)", current_host);
    } else {
	connect_stat = "N";
    }

    if (CONNECTED) {
	if (IN_NVT) {
	    if (linemode) {
		em_mode = 'L';
	    } else {
		em_mode = 'C';
	    }
	} else if (IN_SSCP) {
	    em_mode = 'S';
	} else if (IN_3270) {
	    em_mode = 'I';
	} else {
	    em_mode = 'P';
	}
    } else {
	em_mode = 'N';
    }

    return xs_buffer("%c %c %c %s %c %d %d %d %d %d",
	kb_stat,
	fmt_stat,
	prot_stat,
	connect_stat,
	em_mode,
	model_num,
	ROWS, COLS,
	cursor_addr / COLS, cursor_addr % COLS);
}

static bool
Status_action(ia_t ia, unsigned argc, const char **argv)
{
    char *s;

    action_debug("Status", ia, argc, argv);
    if (check_argc("Status", argc, 0, 0) < 0) {
	return false;
    }

    s = status_string();
    Tcl_SetResult(sms_interp, s, TCL_VOLATILE);
    Free(s);
    return true;
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
static bool
do_read_buffer(const char **params, unsigned num_params, struct ea *buf)
{
    Tcl_Obj *o = NULL;
    Tcl_Obj *row = NULL;
    int	baddr;
    unsigned char current_fg = 0x00;
    unsigned char current_bg = 0x00;
    unsigned char current_gr = 0x00;
    unsigned char current_cs = 0x00;
    unsigned char current_ic = 0x00;
    varbuf_t r;
    char *rbuf;
    bool in_ebcdic = false;

    if (num_params > 0) {
	if (num_params > 1) {
	    popup_an_error("ReadBuffer: extra agruments");
	    return false;
	}
	if (!strncasecmp(params[0], "Ascii", strlen(params[0])))
	    in_ebcdic = false;
	else if (!strncasecmp(params[0], "Ebcdic", strlen(params[0])))
	    in_ebcdic = true;
	else {
	    popup_an_error("ReadBuffer: first parameter must be Ascii or "
		    "Ebcdic");
	    return false;
	}
    }

    vb_init(&r);
    baddr = 0;
    do {
	if (!(baddr % COLS)) {
	    /* New row. */
	    if (o == NULL) {
		o = Tcl_NewListObj(0, NULL);
	    }
	    if (row != NULL) {
		Tcl_ListObjAppendElement(sms_interp, o, row);
	    }
	    row = Tcl_NewListObj(0, NULL);
	}
	if (buf[baddr].fa) {
	    vb_appendf(&r, "SF(%02x=%02x", XA_3270, buf[baddr].fa);
	    if (buf[baddr].fg) {
		vb_appendf(&r, ",%02x=%02x", XA_FOREGROUND, buf[baddr].fg);
	    }
	    if (buf[baddr].bg) {
		vb_appendf(&r, ",%02x=%02x", XA_BACKGROUND, buf[baddr].bg);
	    }
	    if (buf[baddr].gr) {
		vb_appendf(&r, ",%02x=%02x", XA_HIGHLIGHTING,
			buf[baddr].gr | 0xf0);
	    }
	    if (buf[baddr].ic) {
		vb_appendf(&r, ",%02x=%02x", XA_INPUT_CONTROL, buf[baddr].ic);
	    }
	    if (buf[baddr].cs & CS_MASK) {
		vb_appendf(&r, ",%02x=%02x", XA_CHARSET,
			calc_cs(buf[baddr].cs));
	    }
	    vb_appends(&r, ")");
	    Tcl_ListObjAppendElement(sms_interp, row,
		    Tcl_NewStringObj(vb_consume(&r), -1));
	} else {
	    bool any_sa = false;
#           define SA_SEP (any_sa? ",": "SA(")

	    if (buf[baddr].fg != current_fg) {
		vb_appendf(&r, "%s%02x=%02x", SA_SEP, XA_FOREGROUND,
			buf[baddr].fg);
		current_fg = buf[baddr].fg;
		any_sa = true;
	    }
	    if (buf[baddr].bg != current_bg) {
		vb_appendf(&r, "%s%02x=%02x", SA_SEP, XA_BACKGROUND,
			buf[baddr].bg);
		current_bg = buf[baddr].bg;
		any_sa = true;
	    }
	    if (buf[baddr].gr != current_gr) {
		vb_appendf(&r, "%s%02x=%02x", SA_SEP, XA_HIGHLIGHTING,
			buf[baddr].gr | 0xf0);
		current_gr = buf[baddr].gr;
		any_sa = true;
	    }
	    if (buf[baddr].ic != current_ic) {
		vb_appendf(&r, "%s%02x=%02x", SA_SEP, XA_INPUT_CONTROL,
			buf[baddr].ic);
		current_ic = buf[baddr].ic;
		any_sa = true;
	    }
	    if ((buf[baddr].cs & ~CS_GE) != (current_cs & ~CS_GE)) {
		vb_appendf(&r, "%s%02x=%02x", SA_SEP, XA_CHARSET,
			calc_cs(buf[baddr].cs));
		current_cs = buf[baddr].cs;
		any_sa = true;
	    }
	    if (any_sa) {
		vb_appends(&r, ")");
		Tcl_ListObjAppendElement(sms_interp, row,
			Tcl_NewStringObj(vb_consume(&r), -1));
	    }
	    if (in_ebcdic) {
		if (buf[baddr].cs & CS_GE) {
		    rbuf = xs_buffer("GE(%02x)", buf[baddr].cc);
		} else {
		    rbuf = xs_buffer("%02x", buf[baddr].cc);
		}
		Tcl_ListObjAppendElement(sms_interp, row,
			Tcl_NewStringObj(rbuf, -1));
		Free(rbuf);
	    } else {
		int len;
		char mb[16];
		int j;
		ucs4_t uc;

		if (IS_LEFT(ctlr_dbcs_state(baddr))) {
		    len = ebcdic_to_multibyte((buf[baddr].cc << 8) |
			    buf[baddr + 1].cc,
			    mb, sizeof(mb));
		    for (j = 0; j < len - 1; j++) {
			vb_appendf(&r, "%02x", mb[j] & 0xff);
		    }
		} else if (IS_RIGHT(ctlr_dbcs_state(baddr))) {
		    vb_appends(&r, " -");
		} else if (buf[baddr].cc == EBC_null) {
		    vb_appends(&r, "00");
		} else {
		    len = ebcdic_to_multibyte_x(buf[baddr].cc,
			    buf[baddr].cs & CS_MASK,
			    mb, sizeof(mb), EUO_BLANK_UNDEF,
			    &uc);
		    for (j = 0; j < len - 1; j++) {
			vb_appendf(&r, "%02x", mb[j] & 0xff);
		    }
		}
		Tcl_ListObjAppendElement(sms_interp, row,
			Tcl_NewStringObj(vb_consume(&r), -1));
	    }
	}
	INC_BA(baddr);
    } while (baddr != 0);

    if (row) {
	if (o) {
	    Tcl_ListObjAppendElement(sms_interp, o, row);
	    Tcl_SetObjResult(sms_interp, o);
	} else {
	    Tcl_SetObjResult(sms_interp, row);
	}
    }
    return true;
}

/*
 * ReadBuffer action.
 */
static bool
ReadBuffer_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("ReadBuffer", ia, argc, argv);
    return do_read_buffer(argv, argc, ea_buf);
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
    output_wait_needed = true;
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
	    if (ea_buf[baddr].fa) {
		break;
	    }
	    snap_field_length++;
	    INC_BA(baddr);
	} while (baddr != snap_field_start);
    }
    snap_caddr = cursor_addr;
}

static bool
Snap_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Snap", ia, argc, argv);
    if (argc == 0) {
	snap_save();
	return true;
    }

    /* Handle 'Snap Wait' separately. */
    if (!strcasecmp(argv[0], "Wait")) {
	long tmo = -1;
	char *ptr;
	unsigned maxp = 0;

	if (argc > 1 &&
	    (tmo = strtol(argv[1], &ptr, 10)) >= 0 &&
	    ptr != argv[0] &&
	    *ptr == '\0') {
	    maxp = 3;
	} else {
	    tmo = -1;
	    maxp = 2;
	}
	if (argc > maxp) {
	    popup_an_error("Too many arguments to Snap(Wait)");
	    return false;
	}
	if (argc < maxp) {
	    popup_an_error("Too few arguments to Snap(Wait)");
	    return false;
	}
	if (strcasecmp(argv[argc - 1], "Output")) {
	    popup_an_error("Unknown parameter to Snap(Wait)");
	    return false;
	}

	/* Must be connected. */
	if (!(CONNECTED || HALF_CONNECTED)) {
	    popup_an_error("Snap: Not connected");
	    return false;
	}

	/*
	 * Make sure we need to wait.
	 * If we don't, then Snap Wait Output is equivalen to Snap Save.
	 */
	if (!output_wait_needed) {
	    snap_save();
	    return false;
	}

	/* Set the new state. */
	waiting = AWAITING_SOUTPUT;

	/* Set up a timeout, if they want one. */
	if (tmo >= 0) {
	    wait_id = AddTimeOut(tmo? (tmo * 1000): 1, wait_timed_out);
	}
	return true;
    }

    if (!strcasecmp(argv[0], "Save")) {
	if (argc != 1) {
	    popup_an_error("Extra argument(s)");
	    return false;
	}
	snap_save();
    } else if (!strcasecmp(argv[0], "Status")) {
	if (argc != 1) {
	    popup_an_error("Extra argument(s)");
	    return false;
	}
	if (snap_status == NULL) {
	    popup_an_error("No saved state");
	    return false;
	}
	Tcl_SetResult(sms_interp, snap_status, TCL_VOLATILE);
    } else if (!strcasecmp(argv[0], "Rows")) {
	if (argc != 1) {
	    popup_an_error("Extra argument(s)");
	    return false;
	}
	if (snap_status == NULL) {
	    popup_an_error("No saved state");
	    return false;
	}
	Tcl_SetResult(sms_interp, lazyaf("%d", snap_rows), TCL_VOLATILE);
    } else if (!strcasecmp(argv[0], "Cols")) {
	if (argc != 1) {
	    popup_an_error("extra argument(s)");
	    return false;
	}
	Tcl_SetResult(sms_interp, lazyaf("%d", snap_cols), TCL_VOLATILE);
    } else if (!strcasecmp(argv[0], "Ascii")) {
	if (snap_status == NULL) {
	    popup_an_error("No saved state");
	    return false;
	}
	return dump_fixed(argv + 1, argc - 1, "Ascii", true, snap_buf,
		snap_rows, snap_cols, snap_caddr);
    } else if (!strcasecmp(argv[0], "Ebcdic")) {
	if (snap_status == NULL) {
	    popup_an_error("No saved state");
	    return false;
	}
	return dump_fixed(argv + 1, argc - 1, "Ebcdic", false, snap_buf,
		snap_rows, snap_cols, snap_caddr);
    } else if (!strcasecmp(argv[0], "ReadBuffer")) {
	if (snap_status == NULL) {
	    popup_an_error("No saved state");
	    return false;
	}
	return do_read_buffer(argv + 1, argc - 1, snap_buf);
    } else {
	popup_an_error("Snap: Argument must be Save, Status, Rows, Cols, "
		"Wait, Ascii, Ebcdic or ReadBuffer");
	return false;
    }

    return true;
}

static void
wait_timed_out(ioid_t id _is_unused)
{
	popup_an_error("Wait timed out");
	wait_id = NULL_IOID;
	UNBLOCK();
}

static bool
Wait_action(ia_t ia, unsigned argc, const char **argv)
{
    float tmo = -1.0;
    char *ptr;
    unsigned np;
    const char **pr;

    action_debug("Wait", ia, argc, argv);

    if (argc > 0 &&
	(tmo = strtof(argv[0], &ptr)) >= 0.0 &&
	 ptr != argv[0] &&
	 *ptr == '\0') {
	np = argc - 1;
	pr = argv + 1;
     } else {
	tmo = -1.0;
	np = argc;
	pr = argv;
    }

    if (np == 0) {
	if (!CONNECTED) {
	    popup_an_error("Not connected");
	    return false;
	}
	if (!INPUT_OKAY) {
	    waiting = AWAITING_IFIELD;
	}
	return true;
    }
    if (np != 1) {
	popup_an_error("Too many parameters");
	return true;
    }
    if (!strcasecmp(pr[0], "InputField")) {
	/* Same as no parameters. */
	if (!CONNECTED) {
	    popup_an_error("Not connected");
	    return false;
	}
	if (!INPUT_OKAY) {
	    waiting = AWAITING_IFIELD;
	}
    } else if (!strcasecmp(pr[0], "Output")) {
	if (!CONNECTED) {
	    popup_an_error("Not connected");
	    return false;
	}
	if (output_wait_needed) {
	    waiting = AWAITING_OUTPUT;
	}
    } else if (!strcasecmp(pr[0], "3270") ||
	       !strcasecmp(pr[0], "3270Mode")) {
	if (!CONNECTED) {
	    popup_an_error("Not connected");
	    return false;
	}
	if (!IN_3270) {
	    waiting = AWAITING_3270;
	}
    } else if (!strcasecmp(pr[0], "ansi") ||
	       !strcasecmp(pr[0], "NVTMode")) {
	if (!CONNECTED) {
	    popup_an_error("Not connected");
	    return false;
	}
	if (!IN_NVT) {
	    waiting = AWAITING_NVT;
	}
    } else if (!strcasecmp(pr[0], "Disconnect")) {
	if (CONNECTED) {
	    waiting = AWAITING_DISCONNECT;
	}
    } else if (!strcasecmp(pr[0], "Unlock")) {
	if (CONNECTED && KBWAIT) {
	    waiting = AWAITING_UNLOCK;
	}
    } else {
	popup_an_error("Unknown Wait type: %s", pr[0]);
	return false;
    }

    if (waiting != NOT_WAITING && tmo >= 0.0) {
	unsigned long tmo_msec = tmo * 1000;

	if (tmo_msec == 0) {
	    tmo_msec = 1;
	}
	wait_id = AddTimeOut(tmo_msec, wait_timed_out);
    }
    return true;
}

static int
Rows_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
		Tcl_Obj *CONST objv[])
{
    char *buf;

    if (objc > 1) {
	Tcl_SetResult(interp, "Too many arguments", TCL_STATIC);
	return TCL_ERROR;
    }
    buf = xs_buffer("%d", ROWS);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    Free(buf);
    return TCL_OK;
}

static int
Cols_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
		Tcl_Obj *CONST objv[])
{
    char *buf;

    if (objc > 1) {
	Tcl_SetResult(interp, "Too many arguments", TCL_STATIC);
	return TCL_ERROR;
    }
    buf = xs_buffer("%d", COLS);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    Free(buf);
    return TCL_OK;
}

static bool
Query_action(ia_t ia, unsigned argc, const char **argv)
{
    Tcl_Obj *q_obj;
    char *s;
    char *t;

    static struct {
	char *name;
	const char *(*fn)(void);
	char *string;
    } queries[] = {
	{ "BindPluName", net_query_bind_plu_name, NULL },
	{ "ConnectionState", net_query_connection_state, NULL },
	{ "CodePage", get_host_codepage, NULL },
	{ "Cursor", ctlr_query_cursor, NULL },
	{ "Formatted", ctlr_query_formatted, NULL },
	{ "Host", net_query_host, NULL },
	{ "LocalEncoding", get_codeset, NULL },
	{ "LuName", net_query_lu_name, NULL },
	{ "Model", NULL, full_model_name },
	{ "ScreenCurSize", ctlr_query_cur_size, NULL },
	{ "ScreenMaxSize", ctlr_query_max_size, NULL },
	{ "Ssl", net_query_ssl, NULL },
	{ NULL, NULL }
    };
    int i;

    action_debug("Query", ia, argc, argv);
    if (check_argc("Query", argc, 0, 1) < 0) {
	return false;
    }

    switch (argc) {
    case 0:
	q_obj = Tcl_NewListObj(0, NULL);
	for (i = 0; queries[i].name != NULL; i++) {
	    t = (char *)(queries[i].fn? (*queries[i].fn)(): queries[i].string);
	    if (t && *t) {
		s = xs_buffer("%s %s", queries[i].name, t);
	    } else {
		s = xs_buffer("%s", queries[i].name);
	    }
	    Tcl_ListObjAppendElement(sms_interp, q_obj,
		    Tcl_NewStringObj(s, strlen(s)));
	    Free(s);
	}
	Tcl_SetObjResult(sms_interp, q_obj);
	break;
    case 1:
	for (i = 0; queries[i].name != NULL; i++) {
	    if (!strcasecmp(argv[0], queries[i].name)) {
		s = (char *)(queries[i].fn? (*queries[i].fn)():
			queries[i].string);
		Tcl_SetResult(sms_interp, *s? s: "", TCL_VOLATILE);
		return true;
	    }
	}
	popup_an_error("Query: Unknown parameter");
	return false;
    }
    return true;
}

/* Generate a response to a script command. */
void
sms_info(const char *fmt, ...)
{
    va_list args;
    char *buf;

    va_start(args, fmt);
    buf = xs_vbuffer(fmt, args);
    va_end(args);
    Tcl_AppendResult(sms_interp, buf, NULL);
    Free(buf);
}

/*
 * Return true if there is a pending macro.
 */
bool
sms_in_macro(void)
{
    return pending_string != NULL;
}

/* Like fcatv, but goes to a dynamically-allocated buffer. */
static char *
tc_scatv(const char *s)
{
    char c;
    varbuf_t r;

    vb_init(&r);

    while ((c = *s++))  {
	switch (c) {
	case '\n':
	    vb_appends(&r, "\\n");
	    break;
	case '\t':
	    vb_appends(&r, "\\t");
	    break;
	case '\b':
	    vb_appends(&r, "\\b");
	    break;
	case '\f':
	    vb_appends(&r, "\\f");
	    break;
	case ' ':
	    vb_appends(&r, "\\ ");
	    break;
	default:
	    if ((c & 0x7f) < ' ') {
		vb_appendf(&r, "\\%03o", c & 0xff);
		break;
	    } else {
		vb_append(&r, &c, 1);
	    }
	}
    }
    return vb_consume(&r);
}

/* Dummy version of function in macros.c. */
void
cancel_if_idle_command(void)
{
}

/* Dummy idle.c function. */
void
idle_ft_complete(void)
{
}

/* Dummy idle.c function. */
void
idle_ft_start(void)
{
}

/**
 * Registration for tcl3270 main module.
 */
static void
tcl3270_register(void)
{
    static toggle_register_t toggles[] = {
	{ AID_WAIT,	NULL,	0 }
    };
    static action_table_t actions[] = {
	{ "Ascii",		Ascii_action,		ACTION_KE },
	{ "AsciiField",		AsciiField_action,	ACTION_KE },
	{ "Ebcdic",		Ebcdic_action,		ACTION_KE },
	{ "EbcdicField",	EbcdicField_action,	ACTION_KE },
	{ "Status",		Status_action,		ACTION_KE },
	{ "ReadBuffer",		ReadBuffer_action,	ACTION_KE },
	{ "Snap",		Snap_action,		ACTION_KE },
	{ "Wait",		Wait_action,		ACTION_KE },
	{ "Query",		Query_action,		ACTION_KE }
    };
    static res_t tcl3270_resources[] = {
	{ ResCommandTimeout, aoffset(tcl3270.command_timeout), XRM_INT }
    };

    /* Register our toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register for state changes. */
    register_schange(ST_CONNECT, tcl3270_connect);
    register_schange(ST_3270_MODE, tcl3270_connect);

    /* Register our actions. */
    register_actions(actions, array_count(actions));

    /* Register our resources. */
    register_resources(tcl3270_resources, array_count(tcl3270_resources));
}
