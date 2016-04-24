/*
 * Copyright (c) 1993-2016 Paul Mattes.
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
 *      macros.c
 *              This module handles string, macro and script (sms) processing.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#include <signal.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else /*][*/
#include "wincmn.h"
#endif /*]*/
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"
#include "resources.h"
#include "toggles.h"

#include "actions.h"
#include "bind-opt.h"
#include "charset.h"
#include "child.h"
#include "ctlrc.h"
#include "unicodec.h"
#include "ft.h"
#include "host.h"
#include "idle.h"
#include "kybd.h"
#include "lazya.h"
#include "macros.h"
#include "menubar.h"
#include "popups.h"
#include "pr3287_session.h"
#include "product.h"
#include "screen.h"
#include "status.h"
#include "telnet.h"
#include "trace.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"
#include "xio.h"

#include "w3misc.h"

#define NVT_SAVE_SIZE	4096

#if defined(_WIN32) /*[*/
#define SOCK_CLOSE(s)	closesocket(s)
#else /*][*/
#define SOCK_CLOSE(s)	close(s)
#endif /*[*/

/* Maximum size of a macro (a line of script input). */
#define MSC_BUF	1024

/* Globals */
struct macro_def *macro_defs = NULL;
bool macro_output = false;

/* Statics */
typedef struct sms {
    struct sms *next;	/* next sms on the stack */
    char	*msc;	/* input buffer */
    size_t	msc_len;/* length of input buffer */
    char   *dptr;	/* data pointer (macros only) */
    enum sms_state {
	SS_IDLE,	/* no command active (scripts only) */
	SS_INCOMPLETE,	/* command(s) buffered and ready to run */
	SS_RUNNING,	/* command executing */
	SS_KBWAIT,	/* command awaiting keyboard unlock */
	SS_CONNECT_WAIT,/* command awaiting connection to complete */
	SS_FT_WAIT,	/* command awaiting file transfer to complete */
	SS_TIME_WAIT,   /* command awaiting simple timeout */
	SS_PAUSED,	/* stopped in PauseScript action */
	SS_WAIT_NVT,	/* awaiting completion of Wait(NVTMode) */
	SS_WAIT_3270,	/* awaiting completion of Wait(3270Mode) */
	SS_WAIT_OUTPUT,	/* awaiting completion of Wait(Output) */
	SS_SWAIT_OUTPUT,/* awaiting completion of Snap(Wait) */
	SS_WAIT_DISC,	/* awaiting completion of Wait(Disconnect) */
	SS_WAIT_IFIELD,	/* awaiting completion of Wait(InputField) */
	SS_WAIT_UNLOCK,	/* awaiting completion of Wait(Unlock) */
	SS_EXPECTING,	/* awaiting completion of Expect() */
	SS_CLOSING	/* awaiting completion of Close() */
    } state;
    enum sms_type {
	ST_STRING,	/* string */
	ST_MACRO,	/* macro */
	ST_COMMAND,	/* interactive command */
	ST_KEYMAP,	/* keyboard map */
	ST_IDLE,	/* idle command */
	ST_CHILD,	/* child process */
	ST_PEER,	/* peer (external) process */
	ST_FILE,	/* read commands from file */
	ST_CB		/* callback (httpd or other) */
#define NUM_ST (ST_CB + 1)
    } type;
    bool	success;
    bool	need_prompt;
    bool	is_login;
    bool	is_hex;		/* flag for ST_STRING only */
    bool output_wait_needed;
    bool executing;	/* recursion avoidance */
    bool accumulated;	/* accumulated time flag */
    bool idle_error;	/* idle command caused an error */
    bool is_socket;	/* I/O is via a socket */
    bool is_transient;	/* I/O is via a transient socket */
    bool is_external;	/* I/O is via a transient socket to -socket */
    unsigned long msec;	/* total accumulated time */
    FILE   *outfile;
    int	infd;
    socket_t insocket;
#if defined(_WIN32) /*[*/
    HANDLE inhandle;
    HANDLE child_handle;
    ioid_t exit_id;
    ioid_t listen_id;
#endif /*]*/
    int	pid;
    ioid_t expect_id;
    ioid_t wait_id;

    struct sms_cbx {	/* ST_CB context: */
	const sms_cb_t *cb;	/*  callback block */
	sms_cbh handle;	/*  handle */
    } cbx;
} sms_t;
static sms_t *sms = NULL;
static int sms_depth = 0;
static socket_t socketfd = INVALID_SOCKET;
static ioid_t socket_id = NULL_IOID;
#if defined(_WIN32) /*[*/
static HANDLE socket_event = NULL;
#endif /*]*/

#define VALID_INFD(sms) \
    ((!(sms)->is_socket && (sms)->infd >= 0) || \
     ((sms)->is_socket && (sms)->insocket != INVALID_SOCKET))

static const char *sms_state_name[] = {
    "IDLE",
    "INCOMPLETE",
    "RUNNING",
    "KBWAIT",
    "CONNECT_WAIT",
    "FT_WAIT",
    "TIME_WAIT",
    "PAUSED",
    "WAIT_NVT",
    "WAIT_3270",
    "WAIT_OUTPUT",
    "SWAIT_OUTPUT",
    "WAIT_DISC",
    "WAIT_IFIELD",
    "WAIT_UNLOCK",
    "EXPECTING",
    "CLOSING"
};

static struct macro_def *macro_last = (struct macro_def *) NULL;
static ioid_t stdin_id = NULL_IOID;
static unsigned char *nvt_save_buf;
static size_t   nvt_save_cnt = 0;
static int      nvt_save_ix = 0;
static char    *expect_text = NULL;
static size_t	expect_len = 0;
static const char *st_name[NUM_ST] = {
    "String",		/* STRING */
    "Macro",		/* MACRO */
    "Command",		/* COMMAND */
    "KeymapAction",	/* KEYMAP */
    "IdleCommand",	/* IDLE */
    "ChildScript",	/* CHILD */
    "PeerScript",	/* PEER */
    "File",		/* FILE */
    "Callback"		/* CB */
};
static enum iaction st_cause[NUM_ST] = {
    IA_MACRO,		/* STRING */
    IA_MACRO,		/* MACRO */
    IA_COMMAND,		/* COMMAND */
    IA_KEYMAP,		/* KEYMAP */
    IA_IDLE,		/* IDLE */
    IA_MACRO,		/* CHILD */
    IA_MACRO,		/* PEER */
    IA_MACRO,		/* FILE */
    IA_MACRO		/* CB (unused) */
};
#define ST_sNAME(s)	st_name[(int)(s)->type]
#define ST_NAME \
    ((sms->type == ST_CB) ? sms->cbx.cb->shortname : ST_sNAME(sms))

#if defined(_WIN32) /*[*/
static HANDLE peer_thread;
static HANDLE peer_enable_event, peer_done_event;
static char peer_buf[256];
int peer_nr;
int peer_errno;
#endif /*]*/

static void cleanup_socket(bool b);
static void script_prompt(bool success);
static void script_input(iosrc_t fd, ioid_t id);
static void sms_pop(bool can_exit);
static void socket_connection(iosrc_t fd, ioid_t id);
#if defined(_WIN32) /*[*/
static void child_socket_connection(iosrc_t fd, ioid_t id);
static void child_exited(iosrc_t fd, ioid_t id);
#endif /*]*/
static void wait_timed_out(ioid_t id);
static void read_from_file(void);
static sms_t *sms_redirect_to(void);

/* Macro that defines that the keyboard is locked due to user input. */
#define KBWAIT	(kybdlock & (KL_OIA_LOCKED|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK|KL_ENTER_INHIBIT|KL_AWAITING_FIRST))
#define CKBWAIT	(toggled(AID_WAIT) && KBWAIT)

/* Macro that defines when it's safe to continue a Wait()ing sms. */
#define CAN_PROCEED ( \
    IN_SSCP || \
    (IN_3270 && (HOST_FLAG(NO_LOGIN_HOST) || \
		 (formatted && cursor_addr)) && !CKBWAIT) || \
    (IN_NVT && !(kybdlock & KL_AWAITING_FIRST)) \
)

static action_t Abort_action;
static action_t AnsiText_action;
static action_t Ascii_action;
static action_t AsciiField_action;
static action_t CloseScript_action;
static action_t ContinueScript_action;
static action_t Ebcdic_action;
static action_t EbcdicField_action;
static action_t Execute_action;
static action_t Expect_action;
static action_t Macro_action;
static action_t PauseScript_action;
static action_t Query_action;
static action_t ReadBuffer_action;
static action_t Script_action;
static action_t Snap_action;
static action_t Source_action;
static action_t Wait_action;

static action_t Bell_action;
static action_t Printer_action;

static void
trace_script_output(const char *fmt, ...)
{
    va_list args;
    char *msgbuf;
    char *s;
    char *m;
    char c;

    if (!toggled(TRACING)) {
	return;
    }

    va_start(args, fmt);
    msgbuf = xs_vbuffer(fmt, args);
    va_end(args);

    m = msgbuf;
    s = msgbuf;
    while ((c = *s++)) {
	if (c == '\n') {
	    vtrace("Output for %s[%d]: '%.*s'\n", ST_NAME, sms_depth,
		    (int)((s - 1) - m), m);
	    m = s;
	    continue;
	}
    }
    Free(msgbuf);
}

/* Callbacks for state changes. */
static void
sms_connect(bool connected)
{
    /* Hack to ensure that disconnects don't cause infinite recursion. */
    if (sms != NULL && sms->executing) {
	return;
    }

    if (!connected) {
	while (sms != NULL && sms->is_login) {
#if !defined(_WIN32) /*[*/
	    if (sms->type == ST_CHILD && sms->pid > 0) {
		(void) kill(sms->pid, SIGTERM);
	    }
#endif /*]*/
	    sms_pop(false);
	}
    }
    sms_continue();
}

static void
sms_in3270(bool in3270)
{
    if (in3270 || IN_SSCP) {
	sms_continue();
    }
}

/**
 * Macros module registration.
 */
void
macros_register(void)
{
    static action_table_t macros_actions[] = {
	{ "Abort",		Abort_action, ACTION_KE },
	{ "AnsiText",		AnsiText_action, 0 },
	{ "Ascii",		Ascii_action, 0 },
	{ "AsciiField",		AsciiField_action, 0 },
	{ "Bell",		Bell_action, 0 },
	{ "CloseScript",	CloseScript_action, 0 },
	{ "ContinueScript",	ContinueScript_action, ACTION_KE },
	{ "Ebcdic",		Ebcdic_action, 0 },
	{ "EbcdicField",	EbcdicField_action, 0 },
	{ "Execute",		Execute_action, ACTION_KE },
	{ "Expect",		Expect_action, 0 },
	{ "Macro",		Macro_action, ACTION_KE },
	{ "PauseScript",	PauseScript_action, 0 },
	{ "Query",		Query_action, 0 },
	{ "ReadBuffer",		ReadBuffer_action, 0 },
	{ "Script",		Script_action, ACTION_KE },
	{ "Snap",		Snap_action, 0 },
	{ "Source",		Source_action, ACTION_KE },
	{ "Wait",		Wait_action, ACTION_KE }
    };
    static action_table_t macros_dactions[] = {
	{ "Printer",		Printer_action, ACTION_KE },
    };
    static toggle_register_t toggles[] = {
	{ AID_WAIT,	NULL,	0 }
    };

    /* Register for state changes. */
    register_schange_ordered(ST_CONNECT, sms_connect, 2000);
    register_schange_ordered(ST_3270_MODE, sms_in3270, 2000);

    /* Register actions.*/
    register_actions(macros_actions, array_count(macros_actions));
    if (product_has_display()) {
	register_actions(macros_dactions, array_count(macros_dactions));
    }

    /* Register toggles. */
    register_toggles(toggles, array_count(toggles));
}

/* Parse the macros resource into the macro list */
void
macros_init(void)
{
    char *s = NULL;
    char *name, *action;
    struct macro_def *m;
    int ns;
    int ix = 1;
    static char *last_s = NULL;

    /* Free the previous macro definitions. */
    while (macro_defs) {
	m = macro_defs->next;
	Free(macro_defs);
	macro_defs = m;
    }
    macro_defs = NULL;
    macro_last = NULL;
    if (last_s) {
	Free(last_s);
	last_s = NULL;
    }

    /* Search for new ones. */
    if (PCONNECTED) {
	char *rname;
	char *space;

	rname = NewString(current_host);
	if ((space = strchr(rname, ' '))) {
	    *space = '\0';
	}
	s = get_fresource("%s.%s", ResMacros, rname);
	Free(rname);
    }
    if (s == NULL) {
	if (appres.macros == NULL) {
	    return;
	}
	s = NewString(appres.macros);
    } else {
	s = NewString(s);
    }
    last_s = s;

    while ((ns = split_dresource(&s, &name, &action)) == 1) {
	m = (struct macro_def *)Malloc(sizeof(*m));
	if (!split_hier(name, &m->name, &m->parents)) {
	    Free(m);
	    continue;
	}
	m->action = action;
	if (macro_last) {
	    macro_last->next = m;
	} else {
	    macro_defs = m;
	}
	m->next = NULL;
	macro_last = m;
	ix++;
    }
    if (ns < 0) {
	xs_warning("Error in macro %d", ix);
    }
}

/*
 * Enable input from a script.
 */
static void
script_enable(void)
{
#if defined(_WIN32) /*[*/
    /* Windows child scripts are listening sockets. */
    if (sms->type == ST_CHILD && sms->inhandle != INVALID_HANDLE_VALUE) {
	sms->listen_id = AddInput(sms->inhandle, child_socket_connection);
	return;
    }
#endif /*]*/

    if (VALID_INFD(sms) && stdin_id == NULL_IOID) {
	vtrace("Enabling input for %s[%d]\n", ST_NAME, sms_depth);
#if defined(_WIN32) /*[*/
	stdin_id = AddInput(sms->inhandle, script_input);
#else /*][*/
	if (sms->is_socket) {
	    stdin_id = AddInput(sms->insocket, script_input);
	} else {
	    stdin_id = AddInput(sms->infd, script_input);
	}
#endif /*]*/
    }
}

/*
 * Disable input from a script.
 */
static void
script_disable(void)
{
    if (stdin_id != NULL_IOID) {
	vtrace("Disabling input for %s[%d]\n", ST_NAME, sms_depth);
	RemoveInput(stdin_id);
	stdin_id = NULL_IOID;
    }
}

/* Allocate a new sms. */
static sms_t *
new_sms(enum sms_type type)
{
    sms_t *s;

    s = (sms_t *)Calloc(1, sizeof(sms_t));

    s->state = SS_IDLE;
    s->type = type;
    s->success = true;
    s->need_prompt = false;
    s->is_login = false;
    s->outfile = NULL;
    s->infd = -1;
    s->insocket = INVALID_SOCKET;
#if defined(_WIN32) /*[*/
    s->inhandle = INVALID_HANDLE_VALUE;
    s->child_handle = INVALID_HANDLE_VALUE;
#endif /*]*/
    s->pid = -1;
    s->expect_id = NULL_IOID;
    s->wait_id = NULL_IOID;
    s->output_wait_needed = false;
    s->executing = false;
    s->accumulated = false;
    s->idle_error = false;
    s->msec = 0L;

    return s;
}

/*
 * Push an sms definition on the stack.
 * Returns whether or not that is legal.
 */
static bool
sms_push(enum sms_type type)
{
    sms_t *s;

    /* Preempt any running sms. */
    if (sms != NULL) {
	/* Remove the running sms's input. */
	script_disable();
    }

    s = new_sms(type);
    if (sms != NULL) {
	s->is_login = sms->is_login;	/* propagate from parent */
    }
    s->next = sms;
    sms = s;

    /* Enable the abort button on the menu and the status indication. */
    if (++sms_depth == 1) {
	menubar_as_set(true);
	status_script(true);
    }

    if (nvt_save_buf == NULL) {
	nvt_save_buf = (unsigned char *)Malloc(NVT_SAVE_SIZE);
    }
    return true;
}

/*
 * Add an sms definition to the _bottom_ of the stack.
 */
static sms_t *
sms_enqueue(enum sms_type type)
{
    sms_t *s, *t, *t_prev = NULL;

    /* Allocate and initialize a new structure. */
    s = new_sms(type);

    /* Find the bottom of the stack. */
    for (t = sms; t != NULL; t = t->next) {
	t_prev = t;
    }

    if (t_prev == NULL) {	/* Empty stack. */
	s->next = sms;
	sms = s;

	/*
	 * Enable the abort button on the menu and the status
	 * line indication.
	 */
	menubar_as_set(true);
	status_script(true);
    } else {			/* Add to bottom. */
	s->next = NULL;
	t_prev->next = s;
    }

    sms_depth++;

    if (nvt_save_buf == NULL) {
	nvt_save_buf = (unsigned char *)Malloc(NVT_SAVE_SIZE);
    }

    return s;
}

/* Pop an sms definition off the stack. */
static void
sms_pop(bool can_exit)
{
    sms_t *s;

    vtrace("%s[%d] complete\n", ST_NAME, sms_depth);

    /* When you pop the peer script, that's the end of x3270. */
    if (sms->type == ST_PEER && !sms->is_transient && can_exit) {
	x3270_exit(0);
    }

    /* If this is a callback or script macro, propagate the state. */
    if (sms->next != NULL &&
	    (sms->next->type == ST_CB ||
	     sms->next->type == ST_CHILD ||
	     sms->next->type == ST_PEER)) {
	sms->next->success = sms->success;
    }

    /* Remove the input event. */
    script_disable();

    /* Close the files. */
    if (sms->outfile != NULL) {
	fclose(sms->outfile);
    }
    if (VALID_INFD(sms)) {
	if (sms->is_socket) {
	    SOCK_CLOSE(sms->insocket);
	} else {
	    close(sms->infd);
	}
    }

    /* Cancel any pending timeouts. */
    if (sms->expect_id != NULL_IOID) {
	RemoveTimeOut(sms->expect_id);
    }
    if (sms->wait_id != NULL_IOID) {
	RemoveTimeOut(sms->wait_id);
    }

    /*
     * If this was an idle command that generated an error, now is the
     * time to announce that.  (If we announced it when the error first
     * occurred, we might be telling the wrong party, such as a script.)
     */
    if (sms->idle_error) {
	popup_an_error("Idle command disabled due to error");
    }

    /* If this was a -socket peer, get ready for another connection. */
    if (sms->type == ST_PEER && sms->is_external) {
	if (appres.script_port_once) { 
	    vtrace("EOF on socket with %s set, exiting\n", ResScriptPortOnce);
	    x3270_exit(0);
	}
#if defined(_WIN32) /*[*/
	socket_id = AddInput(socket_event, socket_connection);
#else /*][*/
	socket_id = AddInput(socketfd, socket_connection);
#endif /*]*/
    }

    /* Release the memory. */
    s = sms;
    if (s->msc != NULL) {
	Free(s->msc);
    }
    sms = s->next;
    Free(s);
    sms_depth--;

    if (sms == NULL) {
	/* Turn off the menu option. */
	menubar_as_set(false);
	status_script(false);
    } else if (CKBWAIT && (int)sms->state < (int)SS_KBWAIT) {
	/* The child implicitly blocked the parent. */
	sms->state = SS_KBWAIT;
	vtrace("%s[%d] implicitly paused %s\n", ST_NAME, sms_depth,
		sms_state_name[sms->state]);
    } else if (sms->state == SS_IDLE && sms->type != ST_FILE) {
	/* The parent needs to be restarted. */
	script_enable();
    } else if (sms->type == ST_FILE) {
	read_from_file();
    }

#if defined(_WIN32) /*[*/
    /* If the new top sms is an exited script, pop it, too. */
    if (sms != NULL &&
	sms->type == ST_CHILD &&
	sms->child_handle == INVALID_HANDLE_VALUE) {
	sms_pop(false);
    }
#endif /*]*/
}

#if defined(_WIN32) /*[*/
/* stdin input thread */
static DWORD WINAPI
peer_read(LPVOID lpParameter _is_unused)
{
    for (;;) {
	DWORD rv;

	rv = WaitForSingleObject(peer_enable_event, INFINITE);
	switch (rv) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
	case WAIT_FAILED:
	    peer_nr = -1;
	    peer_errno = EINVAL;
	    SetEvent(peer_done_event);
	    break;
	case WAIT_OBJECT_0:
	    peer_nr = read(0, peer_buf, sizeof(peer_buf));
	    if (peer_nr < 0) {
		peer_errno = errno;
	    }
	    SetEvent(peer_done_event);
	    break;
	}
    }
    return 0;
}
#endif /*]*/

/*
 * Peer script initialization.
 *
 * Must be called after the initial call to connect to the host from the
 * command line, so that the initial state can be set properly.
 */
void
peer_script_init(void)
{
    sms_t *s;
    bool on_top;

    if (appres.script_port) {
	struct sockaddr *sa;
	socklen_t sa_len;
	int on = 1;
	char hostbuf[256];

	if (!parse_bind_opt(appres.script_port, &sa, &sa_len)) {
	    popup_an_error("Invalid script port value '%s', "
		    "ignoring", appres.script_port);
	    return;
	}
#if !defined(_WIN32) /*[*/
	if (appres.socket) {
	    xs_warning("-scriptport overrides -socket");
	}
#endif /*]*/

	/* -scriptport overrides -script */
	appres.scripted = false;

	/* Create the listening socket. */
	socketfd = socket(sa->sa_family, SOCK_STREAM, 0);
	if (socketfd == INVALID_SOCKET) {
#if !defined(_WIN32) /*[*/
	    popup_an_errno(errno, "script port socket()");
#else /*][*/
	    popup_an_error("script port socket(): %s",
		    win32_strerror(GetLastError()));
#endif /*]*/
	    Free(sa);
	    return;
	}
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
		    sizeof(on)) < 0) {
#if !defined(_WIN32) /*[*/
	    popup_an_errno(errno, "script port setsockopt(SO_REUSEADDR)");
#else /*][*/
	    popup_an_error("script port setsockopt(SO_REUSEADDR): %s",
		    win32_strerror(GetLastError()));
#endif /*]*/
	    Free(sa);
	    return;
	}
	if (bind(socketfd, sa, sa_len) < 0) {
#if !defined(_WIN32) /*[*/
	    popup_an_errno(errno, "script port socket bind");
#else /*][*/
	    popup_an_error("script port socket bind: %s",
		    win32_strerror(GetLastError()));
#endif /*]*/
	    SOCK_CLOSE(socketfd);
	    socketfd = -1;
	    Free(sa);
	    return;
	}
	if (listen(socketfd, 1) < 0) {
#if !defined(_WIN32) /*[*/
	    popup_an_errno(errno, "script port socket listen");
#else /*][*/
	    popup_an_error("script port socket listen: %s",
		    win32_strerror(GetLastError()));
#endif /*]*/
	    SOCK_CLOSE(socketfd);
	    socketfd = -1;
	    Free(sa);
	    return;
	}
#if defined(_WIN32) /*[*/
	socket_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (socket_event == NULL) {
	    popup_an_error("script port CreateEvent: %s",
		    win32_strerror(GetLastError()));
	    SOCK_CLOSE(socketfd);
	    socketfd = -1;
	    Free(sa);
	    return;
	}
	if (WSAEventSelect(socketfd, socket_event, FD_ACCEPT) != 0) {
	    popup_an_error("script port WSAEventSelect: %s",
		    win32_strerror(GetLastError()));
	    SOCK_CLOSE(socketfd);
	    socketfd = -1;
	    Free(sa);
	    return;
	}
	socket_id = AddInput(socket_event, socket_connection);
#else /*][*/
	socket_id = AddInput(socketfd, socket_connection);
#endif/*]*/
	register_schange(ST_EXITING, cleanup_socket);

	if (sa->sa_family == AF_INET) {
	    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

	    vtrace("Listening on script port %s (%s, port %u).\n",
		    appres.script_port,
		    inet_ntop(sa->sa_family, &sin->sin_addr, hostbuf,
			sizeof(hostbuf)),
		    ntohs(sin->sin_port));
	}
#if defined(X3270_IPV6) /*[*/
	else {
	    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

	    vtrace("Listening on script port %s (%s, port %u).\n",
		    appres.script_port,
		    inet_ntop(sa->sa_family, &sin6->sin6_addr, hostbuf,
			sizeof(hostbuf)),
		    ntohs(sin6->sin6_port));
	}
#endif /*]*/

	Free(sa);
	return;
    }
#if !defined(_WIN32) /*[*/
    if (appres.socket && !appres.script_port) {
	struct sockaddr_un ssun;

	/* -socket overrides -script */
	appres.scripted = false;

	/* Create the listening socket. */
	socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socketfd < 0) {
	    popup_an_errno(errno, "script Unix-domain socket");
	    return;
	}
	(void) memset(&ssun, '\0', sizeof(ssun));
	ssun.sun_family = AF_UNIX;
	(void) snprintf(ssun.sun_path, sizeof(ssun.sun_path),
		"/tmp/x3sck.%u", (unsigned)getpid());
	(void) unlink(ssun.sun_path);
	if (bind(socketfd, (struct sockaddr *)&ssun, sizeof(ssun)) < 0) {
	    popup_an_errno(errno, "script Unix-domain socket bind");
	    close(socketfd);
	    socketfd = -1;
	    return;
	}
	if (listen(socketfd, 1) < 0) {
	    popup_an_errno(errno, "script Unix-domain socket listen");
	    close(socketfd);
	    socketfd = -1;
	    (void) unlink(ssun.sun_path);
	    return;
	}
	socket_id = AddInput(socketfd, socket_connection);
	register_schange(ST_EXITING, cleanup_socket);
	return;
    }
#endif /*]*/

    if (appres.httpd_port) {
	appres.scripted = false;
    }

    if (!appres.scripted) {
	return;
    }

    if (sms == NULL) {
	/* No login script running, simply push a new sms. */
	(void) sms_push(ST_PEER);
	s = sms;
	on_top = true;
    } else {
	/* Login script already running, pretend we started it. */
	s = sms_enqueue(ST_PEER);
	s->state = SS_RUNNING;
	on_top = false;
    }

    s->infd = fileno(stdin);
#if defined(_WIN32) /*[*/
    peer_enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    peer_done_event = s->inhandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    peer_thread = CreateThread(NULL,
			       0,
			       peer_read,
			       NULL,
			       0,
			       NULL);
    if (peer_thread == NULL) {
	popup_an_error("Cannot create peer script thread: %s\n",
		win32_strerror(GetLastError()));
    }
    SetEvent(peer_enable_event);
#endif /*]*/
    s->outfile = stdout;
    (void) SETLINEBUF(s->outfile);	/* even if it's a pipe */

    if (on_top) {
	if (HALF_CONNECTED || (CONNECTED && (kybdlock & KL_AWAITING_FIRST))) {
	    s->state = SS_CONNECT_WAIT;
	} else {
	    script_enable();
	}
    }
}

/* Accept a new socket connection. */
static void
socket_connection(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    socket_t accept_fd;
    sms_t *s;

    /* Accept the connection. */
#if !defined(_WIN32) /*[*/
    if (appres.script_port)
#endif /*]*/
    {
	union {
	    struct sockaddr sa;
	    struct sockaddr_in sin;
#if defined(X3270_IPV6) /*[*/
	    struct sockaddr_in6 sin6;
#endif /*]*/
	} sa;
	socklen_t len = sizeof(sa);
	char hostbuf[128];

	accept_fd = accept(socketfd, &sa.sa, &len);
	if (accept_fd != INVALID_SOCKET) {
	    if (sa.sa.sa_family == AF_INET) {
		vtrace("New script socket connection from %s:%u\n",
			inet_ntop(AF_INET, &sa.sin.sin_addr, hostbuf,
			    sizeof(hostbuf)), ntohs(sa.sin.sin_port));
	    }
#if defined(X3270_IPV6) /*[*/
	    else if (sa.sa.sa_family == AF_INET6) {
		vtrace("New script socket connection from %s:%u\n",
			inet_ntop(AF_INET6, &sa.sin6.sin6_addr, hostbuf,
			    sizeof(hostbuf)), ntohs(sa.sin6.sin6_port));
	    }
#endif /*]*/
	    else {
		vtrace("New script socket connection from ???\n");
	    }
	}
    }
#if !defined(_WIN32) /*[*/
    else {
	struct sockaddr_un ssun;
	socklen_t len = sizeof(ssun);

	(void) memset(&ssun, '\0', sizeof(ssun));
	ssun.sun_family = AF_UNIX;
	accept_fd = accept(socketfd, (struct sockaddr *)&ssun, &len);
	if (accept_fd != INVALID_SOCKET) {
	    vtrace("New Unix-domain script socket connection\n");
	}
    }
#endif /*]*/

    if (accept_fd == INVALID_SOCKET) {
	popup_an_errno(errno, "script socket accept");
	return;
    }

    /* Push on a peer script. */
    (void) sms_push(ST_PEER);
    s = sms;
    s->is_transient = true;
    s->is_external = true;
    s->insocket = accept_fd;
#if !defined(_WIN32) /*[*/
    s->outfile = fdopen(dup(accept_fd), "w");
#endif /*]*/
#if defined(_WIN32) /*[*/
    s->inhandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (s->inhandle == NULL) {
	fprintf(stderr, "Can't create socket handle\n");
	exit(1);
    }
    if (WSAEventSelect(s->insocket, s->inhandle, FD_READ | FD_CLOSE) != 0) {
	fprintf(stderr, "Can't set socket handle events\n");
	exit(1);
    }
#endif /*]*/
    s->is_socket = true;
    script_enable();

    /* Don't accept any more connections. */
    RemoveInput(socket_id);
    socket_id = NULL_IOID;
}

# if defined(_WIN32) /*[*/
/* Accept a new socket connection from a child process. */
static void
child_socket_connection(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    socket_t accept_fd;
    sms_t *old_sms;
    sms_t *s;
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);

    /* Accept the connection. */
    (void) memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    accept_fd = accept(sms->insocket, (struct sockaddr *)&sin, &len);

    if (accept_fd == INVALID_SOCKET) {
	popup_an_error("script socket accept: %s",
		win32_strerror(GetLastError()));
	return;
    }
    vtrace("New child script socket connection\n");

    /* Push on a peer script. */
    old_sms = sms;
    (void) sms_push(ST_PEER);
    s = sms;
    s->is_transient = true;
    s->insocket = accept_fd;
    s->inhandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (s->inhandle == NULL) {
	fprintf(stderr, "Can't create socket handle\n");
	exit(1);
    }
    if (WSAEventSelect(s->insocket, s->inhandle, FD_READ | FD_CLOSE) != 0) {
	fprintf(stderr, "Can't set socket handle events\n");
	exit(1);
    }
    s->is_socket = true;
    script_enable();

    /* Don't accept any more connections on the global listen socket. */
    RemoveInput(old_sms->listen_id);
    old_sms->listen_id = NULL_IOID;
}
#endif /*]*/

/* Clean up the Unix-domain socket. */
static void
cleanup_socket(bool b _is_unused)
{
#if !defined(_WIN32) /*[*/
    (void) unlink(lazyaf("/tmp/x3sck.%u", getpid()));
#endif /*]*/
}

#if defined(_WIN32) /*[*/
/* Process an event on a child script handle (presumably a process exit). */
static void
child_exited(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    sms_t *s;
    DWORD status;

    for (s = sms; s != NULL; s = s->next) {
	if (s->type == ST_CHILD) {
	    status = 0;
	    if (GetExitCodeProcess(s->child_handle, &status) == 0) {
		popup_an_error("GetExitCodeProcess failed: %s",
			win32_strerror(GetLastError()));
	    } else if (status != STILL_ACTIVE) {
		vtrace("Child script exited with status 0x%x\n",
			(unsigned)status);
		CloseHandle(s->child_handle);
		s->child_handle = INVALID_HANDLE_VALUE;
		RemoveInput(s->exit_id);
		s->exit_id = NULL_IOID;
		if (s == sms) {
		    sms_pop(false);
		    sms_continue();
		}
		break;
	    }
	}
    }
}
#endif /*]*/

/*
 * Interpret and execute a script or macro command.
 */
enum em_stat { EM_CONTINUE, EM_PAUSE, EM_ERROR };
static enum em_stat
execute_command(enum iaction cause, char *s, char **np)
{
#   define MAX_ANAME	64
    enum {
	ME_GND,		/* before action name */
	ME_COMMENT,	/* within a comment */
	ME_FUNCTION,	/* within action name */
	ME_FUNCTIONx,	/* saw whitespace after action name */
	ME_LPAREN,	/* saw left paren */
	ME_P_PARM,	/* paren: within unquoted parameter */
	ME_P_QPARM,	/* paren: within quoted parameter */
	ME_P_BSL,	/* paren: after backslash in quoted parameter */
	ME_P_PARMx,	/* paren: saw whitespace after parameter */
	ME_S_PARM,	/* space: within unquoted parameter */
	ME_S_QPARM,	/* space: within quoted parameter */
	ME_S_BSL,	/* space: after backslash in quoted parameter */
	ME_S_PARMx	/* space: saw whitespace after parameter */
    } state = ME_GND;
    char c;
    char aname[MAX_ANAME+1];
    int nx = 0;
    unsigned param_count = 0;		/* parameter count */
    unsigned vbcount = 0;	/* allocated parameter count */
    varbuf_t *r = NULL;		/* accumulated parameters */
    int failreason = 0;
    action_elt_t *e;
    action_elt_t *any = NULL;
    action_elt_t *exact = NULL;
    unsigned i;
    enum em_stat rc = EM_ERROR;	/* failure return code */
    static const char *fail_text[] = {
	/*1*/ "Action name must begin with an alphanumeric character",
	/*2*/ "Syntax error in action name",
	/*3*/ "Syntax error: \")\" or \",\" expected",
	/*4*/ "Extra data after parameters",
	/*5*/ "Syntax error: \")\" expected"
    };
#define fail(n) { failreason = n; goto failure; }

    while ((c = *s++)) {

	if ((param_count + 1) > vbcount) {
	    /* Allocate a varbuf for the next parameter. */
	    r = (varbuf_t *)Realloc(r, (param_count + 1) * sizeof(varbuf_t));
	    vb_init(&r[param_count]);
	    vbcount = param_count + 1;
	}

	switch (state) {
	case ME_GND:
	    if (isspace((unsigned char)c)) {
		continue;
	    } else if (isalnum((unsigned char)c)) {
		state = ME_FUNCTION;
		nx = 0;
		aname[nx++] = c;
	    } else if (c == '!' || c == '#') {
		state = ME_COMMENT;
	    } else {
		fail(1);
	    }
	    break;
	case ME_COMMENT:
	    break;
	case ME_FUNCTION:	/* within function name */
	    if (c == '(' || isspace((unsigned char)c)) {
		aname[nx] = '\0';
		if (c == '(') {
		    nx = 0;
		    state = ME_LPAREN;
		} else {
		    state = ME_FUNCTIONx;
		}
	    } else if (isalnum((unsigned char)c) || c == '_' || c == '-') {
		if (nx < MAX_ANAME) {
		    aname[nx++] = c;
		}
	    } else {
		fail(2);
	    }
	    break;
	case ME_FUNCTIONx:	/* space after function name */
	    if (isspace((unsigned char)c)) {
		continue;
	    } else if (c == '(') {
		nx = 0;
		state = ME_LPAREN;
	    } else if (c == '"') {
		nx = 0;
		state = ME_S_QPARM;
	    } else {
		state = ME_S_PARM;
		nx = 0;
		vb_append(&r[param_count], &c, 1);
	    }
	    break;
	case ME_LPAREN:
	    if (isspace((unsigned char)c)) {
		continue;
	    } else if (c == '"') {
		state = ME_P_QPARM;
	    } else if (c == ',') {
		param_count++;
	    } else if (c == ')') {
		goto success;
	    } else {
		state = ME_P_PARM;
		vb_append(&r[param_count], &c, 1);
	    }
	    break;
	case ME_P_PARM:
	    if (isspace((unsigned char)c)) {
		param_count++;
		state = ME_P_PARMx;
	    } else if (c == ')') {
		param_count++;
		goto success;
	    } else if (c == ',') {
		param_count++;
		state = ME_LPAREN;
	    } else {
		vb_append(&r[param_count], &c, 1);
	    }
	    break;
	case ME_P_BSL:
	    if (c == 'n') {
		vb_append(&r[param_count], "\n", 1);
	    } else {
		if (c != '"') {
		    vb_append(&r[param_count], "\\", 1);
		}
		vb_append(&r[param_count], &c, 1);
	    }
	    state = ME_P_QPARM;
	    break;
	case ME_P_QPARM:
	    if (c == '"') {
		param_count++;
		state = ME_P_PARMx;
	    } else if (c == '\\') {
		state = ME_P_BSL;
	    } else {
		vb_append(&r[param_count], &c, 1);
	    }
	    break;
	case ME_P_PARMx:
	    if (isspace((unsigned char)c)) {
		continue;
	    } else if (c == ',') {
		state = ME_LPAREN;
	    } else if (c == ')') {
		goto success;
	    } else {
		fail(3);
	    }
	    break;
	case ME_S_PARM:
	    if (isspace((unsigned char)c)) {
		param_count++;
		state = ME_S_PARMx;
	    } else {
		vb_append(&r[param_count], &c, 1);
	    }
	    break;
	case ME_S_BSL:
	    if (c == 'n') {
		vb_append(&r[param_count], "\n", 1);
	    } else {
		if (c != '"') {
		    vb_append(&r[param_count], "\\", 1);
		}
		vb_append(&r[param_count], &c, 1);
	    }
	    state = ME_S_QPARM;
	    break;
	case ME_S_QPARM:
	    if (c == '"') {
		param_count++;
		state = ME_S_PARMx;
	    } else if (c == '\\') {
		state = ME_S_BSL;
	    } else {
		vb_append(&r[param_count], &c, 1);
	    }
	    break;
	case ME_S_PARMx:
	    if (isspace((unsigned char)c)) {
		continue;
	    } else if (c == '"') {
		state = ME_S_QPARM;
	    } else {
		vb_append(&r[param_count], &c, 1);
		state = ME_S_PARM;
	    }
	    break;
	}
    }

    /* Terminal state. */
    switch (state) {
    case ME_FUNCTION:	/* mid-function-name */
	aname[nx] = '\0';
	break;
    case ME_FUNCTIONx:	/* space after function */
	break;
    case ME_GND:	/* nothing */
    case ME_COMMENT:
	if (np) {
	    *np = s - 1;
	}
	rc = EM_CONTINUE;
	goto silent_failure;
    case ME_S_PARMx:	/* space after space-style parameter */
	break;
    case ME_S_PARM:	/* mid space-style parameter */
	param_count++;
	break;
    default:
	fail(5);
    }

success:
    if (c) {
	while (*s && isspace((unsigned char)*s)) {
	    s++;
	}
	if (*s) {
	    if (np) {
		*np = s;
	    } else {
		fail(4);
	    }
	} else if (np) {
	    *np = s;
	}
    } else if (np) {
	*np = s-1;
    }

    /*
     * There used to be logic to do variable substituion here under most
     * circumstances. That's just plain wrong.
     *
     * Substitutions should be handled for specific arguments to specific
     * actions. If substitutions are needed for special situations, they
     * should be added explicitly, or new actions or variants of actions
     * should be added that include the substitutions.
     */

    /* Search the action list. */
    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (!strcasecmp(aname, e->t.name)) {
	    exact = any = e;
	    break;
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    if (exact == NULL) {
	FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	    if (!strncasecmp(aname, e->t.name, strlen(aname))) {
		if (any != NULL) {
		    popup_an_error("Ambiguous action name: %s", aname);
		    goto silent_failure;
		}
		any = e;
	    }
	} FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    }

    if (any != NULL) {
	const char **params = NULL;

	sms->accumulated = false;
	sms->msec = 0L;

	if (param_count) {
	    /* Create the parameter array. */
	    params = (const char **)Malloc(param_count * sizeof(const char *));
	    for (i = 0; i < param_count; i++) {
		params[i] = vb_buf(&r[i]);
	    }
	}

	run_action_entry(any, cause, param_count, param_count? params: NULL);

	if (vbcount) {
	    /* Free the varbuf array. */
	    Free((char *)params);
	    for (i = 0; i < vbcount; i++) {
		vb_free(&r[i]);
	    }
	    Free(r);
	    r = NULL;
	}

	/* Refresh the screen, in case the action changed it. */
	screen_disp(false);
    } else {
	popup_an_error("Unknown action: %s", aname);
	goto silent_failure;
    }

    /* If it produced an error message, it failed. */
    if (!sms->success) {
	return EM_ERROR;
    }

    if (ft_state != FT_NONE) {
	sms->state = SS_FT_WAIT;
    }
    trace_rollover_check();
    if (CKBWAIT) {
	return EM_PAUSE;
    } else {
	return EM_CONTINUE;
    }

failure:
    popup_an_error("%s", fail_text[failreason-1]);
silent_failure:
    if (vbcount) {
	for (i = 0; i < vbcount; i++) {
	    vb_free(&r[i]);
	}
	Free(r);
    }
    return rc;

#undef fail
}

/* Run the string at the top of the stack. */
static void
run_string(void)
{
    size_t len;
    size_t len_left;

    vtrace("%s[%d] running\n", ST_NAME, sms_depth);

    sms->state = SS_RUNNING;
    len = strlen(sms->dptr);
    vtrace("%sString[%d]: '%s'\n", sms->is_hex ? "Hex" : "", sms_depth,
	    sms->dptr);

    if (sms->is_hex) {
	if (CKBWAIT) {
	    sms->state = SS_KBWAIT;
	    vtrace("%s[%d] paused %s\n", ST_NAME, sms_depth,
		    sms_state_name[sms->state]);
	} else {
	    hex_input(sms->dptr);
	    sms_pop(false);
	}
    } else {
	if ((len_left = emulate_input(sms->dptr, len, false))) {
	    sms->dptr += len - len_left;
	    if (CKBWAIT) {
		sms->state = SS_KBWAIT;
		vtrace("%s[%d] paused %s\n", ST_NAME, sms_depth,
			sms_state_name[sms->state]);
	    }
	} else {
	    sms_pop(false);
	}
    }
}

/* Run the macro at the top of the stack. */

static void
run_macro(void)
{
    char *a = sms->dptr;
    char *nextm;
    enum em_stat es;
    sms_t *s;

    vtrace("%s[%d] running\n", ST_NAME, sms_depth);

    /*
     * Keep executing commands off the line until one pauses or
     * we run out of commands.
     */
    while (*a) {
	enum iaction ia;

	/*
	 * Check for command failure.
	 */
	if (!sms->success) {
	    vtrace("%s[%d] failed\n", ST_NAME, sms_depth);

	    /* Propagate it. */
	    if (sms->next != NULL) {
		sms->next->success = false;
	    }
	    break;
	}

	sms->state = SS_RUNNING;
	vtrace("%s[%d]: '%s'\n", ST_NAME, sms_depth, a);
	s = sms;
	s->success = true;
	s->executing = true;

	if (s->type == ST_MACRO &&
		s->next != NULL &&
		s->next->type == ST_CB) {
	    ia = s->next->cbx.cb->ia;
	} else if (s->type == ST_MACRO &&
		s->next != NULL &&
		(s->next->type == ST_CHILD || s->next->type == ST_PEER)) {
	    ia = IA_SCRIPT;
	} else {
	    ia = st_cause[s->type];
	}

	es = execute_command(ia, a, &nextm);
	s->executing = false;
	s->dptr = nextm;

	/*
	 * If a new sms was started, we will be resumed
	 * when it completes.
	 */
	if (sms != s) {
	    return;
	}

	/* Macro could not execute.  Abort it. */
	if (es == EM_ERROR) {
	    vtrace("%s[%d] error\n", ST_NAME, sms_depth);

	    /* Propagate it. */
	    if (sms->next != NULL) {
		sms->next->success = false;
	    }

	    /* If it was an idle command, cancel it. */
	    cancel_if_idle_command();
	    break;
	}

	/* Macro paused, implicitly or explicitly.  Suspend it. */
	if (es == EM_PAUSE || (int)sms->state >= (int)SS_KBWAIT) {
	    if (sms->state == SS_RUNNING) {
		sms->state = SS_KBWAIT;
	    }
	    vtrace("%s[%d] paused %s\n", ST_NAME, sms_depth,
		    sms_state_name[sms->state]);
	    sms->dptr = nextm;
	    return;
	}

	/* Macro ran. */
	a = nextm;
    }

    /* Finished with this macro. */
    sms_pop(false);
}

/* Push a macro (macro, command or keymap action) on the stack. */
static void
push_xmacro(enum sms_type type, const char *s, size_t len, bool is_login)
{
    macro_output = false;
    if (!sms_push(type)) {
	return;
    }
    sms->msc = Malloc(len + 1);
    (void) memcpy(sms->msc, s, len + 1);
    sms->msc_len = strlen(sms->msc);
    sms->dptr = sms->msc;
    if (is_login) {
	sms->state = SS_WAIT_IFIELD;
	sms->is_login = true;
    } else {
	sms->state = SS_INCOMPLETE;
    }
    sms_continue();
}

/* Push a macro on the stack. */
void
push_macro(char *s, bool is_login)
{
    push_xmacro(ST_MACRO, s, strlen(s), is_login);
}

/* Push an interactive command on the stack. */
void
push_command(char *s)
{
    push_xmacro(ST_COMMAND, s, strlen(s), false);
}

/* Push a keymap action on the stack. */
void
push_keymap_action(char *s)
{
    push_xmacro(ST_KEYMAP, s, strlen(s), false);
}

/* Push an idle action on the stack. */
void
push_idle(char *s)
{
    push_xmacro(ST_IDLE, s, strlen(s), false);
}

/* Push a string on the stack. */
static void
push_string(char *s, bool is_login, bool is_hex)
{
    if (!sms_push(ST_STRING)) {
	return;
    }
    sms->msc = NewString(s);
    sms->msc_len = strlen(sms->msc);
    sms->dptr = sms->msc;
    if (is_login) {
	sms->state = SS_WAIT_IFIELD;
	sms->is_login = true;
    } else {
	sms->state = SS_INCOMPLETE;
    }
    sms->is_hex = is_hex;
    if (sms_depth == 1) {
	sms_continue();
    }
}

/* Push a Source'd file on the stack. */
static void
push_file(int fd)
{
    if (!sms_push(ST_FILE)) {
	return;
    }
    sms->infd = fd;
    read_from_file();
}

/* Push a callback on the stack. */
void
push_cb(const char *buf, size_t len, const sms_cb_t *cb, sms_cbh handle)
{
    /* Push the callback sms on the stack. */
    if (!sms_push(ST_CB)) {
	return;
    }
    sms->cbx.cb = cb;
    sms->cbx.handle = handle;
    sms->state = SS_RUNNING;
    sms->need_prompt = true;

    /* Push the command in as a macro on top of that. */
    push_xmacro(ST_MACRO, buf, len, false);
}

/* Set a pending string. */
void
ps_set(char *s, bool is_hex)
{
    push_string(s, false, is_hex);
}

/* Callback for macros menu. */
void
macro_command(struct macro_def *m)
{
    push_macro(m->action, false);
}

/*
 * If the string looks like an action, e.g., starts with "Xxx(", run a login
 * macro.  Otherwise, set a simple pending login string.
 */
void
login_macro(char *s)
{
    char *t = s;
    bool looks_right = false;

    while (isspace((unsigned char)*t)) {
	t++;
    }
    if (isalnum((unsigned char)*t)) {
	while (isalnum((unsigned char)*t)) {
	    t++;
	}
	while (isspace((unsigned char)*t)) {
	    t++;
	}
	if (*t == '(') {
	    looks_right = true;
	}
    }

    if (looks_right) {
	push_macro(s, true);
    } else {
	push_string(s, true, false);
    }
}

/* Run the first command in the msc[] buffer. */
static void
run_script(void)
{
    char *ptr;
    size_t cmd_len;
    char *cmd;

    vtrace("%s[%d] %s\n", ST_NAME, sms_depth,
	    sms->need_prompt? "continuing": "running");

    /* If a prompt is required, send one. */
    if (sms->need_prompt) {
	script_prompt(sms->success);
	sms->need_prompt = false;
    }

    /* If there isn't anything left, we're done. */
    if (!sms->msc_len) {
	script_enable();
	return;
    }

    /* Isolate the command. */
    ptr = memchr(sms->msc, '\n', sms->msc_len);
    if (!ptr) {
	/* No newline yet. */
	script_enable();
	return;
    }
    *ptr++ = '\0';
    cmd_len = ptr - sms->msc;
    cmd = NewString(sms->msc);

    /* Execute it. */
    sms->state = SS_RUNNING;
    sms->success = true;
    vtrace("%s[%d]: '%s'\n", ST_NAME, sms_depth, cmd);
    sms->need_prompt = true;

    /* Move the rest of the buffer over. */
    sms->msc_len -= cmd_len;
    if (sms->msc_len) {
	(void) memmove(sms->msc, ptr, sms->msc_len);
    }
    sms->msc[sms->msc_len] = '\0';

    /* Push a macro for this line of input. */
    sms->executing = true;
    push_macro(cmd, false);
    sms->executing = false;

    /* Copy the output wait state to the child. */
    sms->output_wait_needed = sms->next->output_wait_needed;

    Free(cmd);
}

/* Read the next command from a file. */
static void
read_from_file(void)
{
    varbuf_t r;
    char *buf;

    vb_init(&r);
    sms->msc_len = 0;

    while (true) {
	char c;
	int nr;

	nr = read(sms->infd, &c, 1);
	if (nr < 0) {
	    vtrace("%s[%d] read error\n", ST_NAME, sms_depth);
	    vb_free(&r);
	    sms_pop(false);
	    return;
	}
	if (nr == 0) {
	    if (sms->msc_len == 0) {
		vtrace("%s[%d] read EOF\n", ST_NAME, sms_depth);
		vb_free(&r);
		sms_pop(false);
		return;
	    } else {
		vtrace("%s[%d] read EOF without newline\n", ST_NAME,
			sms_depth);
		break;
	    }
	}
	if (c == '\r' || c == '\n') {
	    if (vb_len(&r)) {
		break;
	    } else {
		continue;
	    }
	}
	vb_append(&r, &c, 1);
	sms->msc_len++;
    }

    /* Run the command as a macro. */
    buf = vb_consume(&r);
    vtrace("%s[%d] read '%s'\n", ST_NAME, sms_depth, buf);
    sms->state = SS_INCOMPLETE;
    push_macro(buf, false);
    Free(buf);
}

/* Handle an error generated during the execution of a script or macro. */
void
sms_error(const char *msg)
{
    sms_t *s;
    bool is_script = false;

    /* Print the error message. */
    s = sms_redirect_to();
    is_script = (s != NULL);
    if (is_script) {
	size_t sl = strlen(msg);
	char *text = Malloc(strlen("data: ") + sl + 2);
	char *newline;
	char *last_space;

	/* Prepend 'data: ', unless doing a callback. */
	if (s->type == ST_CB) {
	    strcpy(text, msg);
	} else {
	    sprintf(text, "data: %s", msg);
	}

	/* Translate newlines to spaces. */
	newline = text;
	while ((newline = strchr(newline, '\n')) != NULL) {
	    *newline++ = ' ';
	}

	if (s->type == ST_CB) {
	    /* Remove trailing spaces. */
	    while (sl && text[sl - 1] == ' ') {
		sl--;
	    }
	    trace_script_output("%.*s\n", (int)sl, text);
	    (*s->cbx.cb->data)(s->cbx.handle, text, sl);
	} else {
	    /* End with one newline. */
	    last_space = strrchr(text, ' ');
	    if (last_space != NULL && last_space == text + strlen(text) - 1) {
		*last_space = '\n';
	    } else {
		strcat(text, "\n");
	    }
	    trace_script_output("%s", text);
	    if (s->is_socket) {
		send(s->insocket, text, (int)strlen(text), 0);
	    } else {
		fprintf(s->outfile, "%s", text);
	    }
	}

	Free(text);
    } else {
	(void) fprintf(stderr, "%s\n", msg);
	fflush(stderr);
    }

    /* Fail the current command. */
    sms->success = false;

    /* Cancel any login. */
    if (s != NULL && s->is_login) {
	host_disconnect(true);
    }
}

/*
 * Generate a response to a script command.
 * Makes sure that each line of output is prefixed with 'data:', if
 * appropriate, and makes sure that the output is newline terminated.
 *
 * If the parameter is an empty string, generates nothing, but if it is a
 * newline, generates an empty line.
 */
void
sms_info(const char *fmt, ...)
{
    char *nl;
    char *msgbuf;
    char *msg;
    va_list args;
    sms_t *s;

    va_start(args, fmt);
    msgbuf = xs_vbuffer(fmt, args);
    va_end(args);

    msg = msgbuf;
    do {
	size_t nc;

	nl = strchr(msg, '\n');
	if (nl != NULL) {
	    nc = nl - msg;
	} else {
	    nc = strlen(msg);
	}
	if (nc || (nl != NULL)) {
	    if ((s = sms_redirect_to()) != NULL) {
		if (s->type == ST_CB) {
		    (*s->cbx.cb->data)(s->cbx.handle, msg, nc);
		    trace_script_output("%.*s\n", nc, msg);
		} else {
		    char *text = Malloc(strlen("data: ") + nc + 2);

		    sprintf(text, "data: %.*s\n", (int)nc, msg);
		    if (s->is_socket) {
			send(s->insocket, text, (int)strlen(text), 0);
		    } else {
			(void) fprintf(s->outfile, "%s", text);
		    }
		    trace_script_output("%s", text);
		    Free(text);
		}
	    } else {
		(void) printf("%.*s\n", (int)nc, msg);
	    }
	}
	msg = nl + 1;
    } while (nl);

    Free(msgbuf);

    macro_output = true;
}

/* Process available input from a script. */
static void
script_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    char buf[8192];
    size_t n2r;
    ssize_t nr;
    char *ptr;
    char c;

    vtrace("Input for %s[%d] %s reading %s %d\n", ST_NAME, sms_depth,
	    sms_state_name[sms->state],
	    sms->is_socket? "socket": "fd",
	    sms->is_socket? (int)sms->insocket: sms->infd);

    /* Read in what you can. */
    n2r = sizeof(buf);
    if (sms->is_socket) {
	nr = recv(sms->insocket, buf, (int)n2r, 0);
    }
#if defined(_WIN32) /*[*/
    else if (sms->inhandle == peer_done_event) {
	nr = peer_nr;
	peer_nr = 0;
	if (nr < 0) {
	    errno = peer_errno;
	}
	SetEvent(peer_enable_event);
	memcpy(buf, peer_buf, nr);
    }
#endif /*]*/
    else {
	nr = read(sms->infd, buf, (int)n2r);
    }
    if (nr < 0) {
#if defined(_WIN32) /*[*/
	if (sms->is_socket) {
	    popup_an_error("%s[%d] recv: %s", ST_NAME, sms_depth,
		    win32_strerror(GetLastError()));
	} else
#endif
	{
	    popup_an_errno(errno, "%s[%d] read", ST_NAME, sms_depth);
	}
	sms_pop(true);
	sms_continue();
	return;
    }
    vtrace("Input for %s[%d] %s complete, nr=%d\n", ST_NAME, sms_depth,
	    sms_state_name[sms->state], (int)nr);
    if (nr == 0) {	/* end of file */
	vtrace("EOF %s[%d]\n", ST_NAME, sms_depth);
	if (sms->msc_len) {
	    popup_an_error("%s[%d]: missing newline", ST_NAME, sms_depth);
	}
	sms_pop(true);
	sms_continue();
	return;
    }

    /* Reallocate. */
    if (sms->msc == NULL) {
	sms->msc = Malloc(sizeof(buf));
    } else {
	sms->msc = Realloc(sms->msc, sms->msc_len + nr + 1);
    }

    /* Append to the pending command, stripping carriage returns. */
    ptr = buf;
    while (nr--) {
	if ((c = *ptr++) != '\r') {
	    sms->msc[sms->msc_len++] = c;
	}
    }
    sms->msc[sms->msc_len] = '\0';

    /* Run the command(s). */
    sms->state = SS_INCOMPLETE;
    sms_continue();
}

/* Resume a paused sms, if conditions are now ripe. */
void
sms_continue(void)
{
    static bool continuing = false;

    if (continuing) {
	return;
    }
    continuing = true;

    while (true) {
	if (sms == NULL) {
	    continuing = false;
	    return;
	}

	switch (sms->state) {

	case SS_IDLE:
	    continuing = false;
	    return;		/* nothing to do */

	case SS_INCOMPLETE:
	case SS_RUNNING:
	    break;		/* let it proceed */

	case SS_KBWAIT:
	    if (CKBWAIT) {
		continuing = false;
		return;
	    }
	    break;

	case SS_WAIT_NVT:
	    if (IN_NVT) {
		sms->state = SS_WAIT_IFIELD;
		continue;
	    }
	    continuing = false;
	    return;

	case SS_WAIT_3270:
	    if (IN_3270 | IN_SSCP) {
		sms->state = SS_WAIT_IFIELD;
		continue;
	    }
	    continuing = false;
	    return;

	case SS_WAIT_UNLOCK:
	    if (KBWAIT) {
		continuing = false;
		return;
	    }
	    break;

	case SS_WAIT_IFIELD:
	    if (!CAN_PROCEED) {
		continuing = false;
		return;
	    }
	    /* fall through... */
	case SS_CONNECT_WAIT:
	    if (HALF_CONNECTED ||
		(CONNECTED && (kybdlock & KL_AWAITING_FIRST))) {
		continuing = false;
		return;
	    }
	    break;

	case SS_FT_WAIT:
	    if (ft_state == FT_NONE) {
		break;
	    } else {
		continuing = false;
		return;
	    }

	case SS_TIME_WAIT:
	    continuing = false;
	    return;

	case SS_WAIT_OUTPUT:
	case SS_SWAIT_OUTPUT:
	    if (!CONNECTED) {
		popup_an_error("Host disconnected");
		break;
	    }
	    continuing = false;
	    return;

	case SS_WAIT_DISC:
	    if (!CONNECTED) {
		break;
	    } else {
		continuing = false;
		return;
	    }

	case SS_PAUSED:
	    continuing = false;
	    return;

	case SS_EXPECTING:
	    continuing = false;
	    return;

	case SS_CLOSING:
	    continuing = false;
	    return;	/* can't happen, I hope */

	}

	/* Restart the sms. */

	sms->state = SS_IDLE;

	if (sms->wait_id != NULL_IOID) {
	    RemoveTimeOut(sms->wait_id);
	    sms->wait_id = NULL_IOID;
	}

	switch (sms->type) {
	case ST_STRING:
	    run_string();
	    break;
	case ST_MACRO:
	case ST_COMMAND:
	case ST_KEYMAP:
	case ST_IDLE:
	    run_macro();
	    break;
	case ST_PEER:
	case ST_CHILD:
	    run_script();
	    break;
	case ST_FILE:
	    read_from_file();
	    break;
	case ST_CB:
	    script_prompt(sms->success);
	    break;
	}
    }

    continuing = false;
}

/*
 * Return true if there is a pending macro.
 */
bool
sms_in_macro(void)
{
    sms_t *s;

    for (s = sms; s != NULL; s = s->next) {
	if (s->type == ST_MACRO || s->type == ST_STRING) {
	    return true;
	}
    }
    return false;
}

/* Set and propagate the output_wait_needed flag. */
static void
set_output_needed(bool needed)
{
    sms_t *next = sms->next;

    /* Set the current sms' state. */
    sms->output_wait_needed = needed;

    /* If its parent is a script, set its parent's state, too. */
    if (sms->type == ST_MACRO &&
		next != NULL &&
	    (next->type == ST_PEER ||
	     next->type == ST_CHILD ||
	     next->type == ST_CB)) {
	next->output_wait_needed = needed;
    }
}

/*
 * Macro- and script-specific actions.
 */

static void
dump_range(int first, int len, bool in_ascii, struct ea *buf,
    int rel_rows _is_unused, int rel_cols)
{
    int i;
    bool any = false;
    bool is_zero = false;
    varbuf_t r;

    vb_init(&r);

    /*
     * If the client has looked at the live screen, then if they later
     * execute 'Wait(output)', they will need to wait for output from the
     * host.  output_wait_needed is cleared by sms_host_output,
     * which is called from the write logic in ctlr.c.
     *
     * Any of the following actions will enable Wait(Output):
     * - Ascii
     * - Ebcdic
     */     
    if (sms != NULL && buf == ea_buf) {
	set_output_needed(true);
    }

    is_zero = FA_IS_ZERO(get_field_attribute(first));

    for (i = 0; i < len; i++) {
	if (i && !((first + i) % rel_cols)) {
	    action_output("%s", vb_buf(&r));
	    vb_reset(&r);
	    any = false;
	}
	if (in_ascii) {
	    char mb[16];
	    ucs4_t uc;
	    size_t j;
	    size_t xlen;

	    if (buf[first + i].fa) {
		is_zero = FA_IS_ZERO(buf[first + i].fa);
		vb_appends(&r, " ");
	    } else if (is_zero) {
		vb_appends(&r, " ");
	    } else if (IS_LEFT(ctlr_dbcs_state(first + i))) {
		xlen = ebcdic_to_multibyte(
			(buf[first + i].cc << 8) | buf[first + i + 1].cc,
			mb, sizeof(mb));
		for (j = 0; j < xlen - 1; j++) {
		    vb_appendf(&r, "%c", mb[j]);
		}
	    } else if (IS_RIGHT(ctlr_dbcs_state(first + i))) {
		continue;
	    } else {
		xlen = ebcdic_to_multibyte_x(
			buf[first + i].cc,
			buf[first + i].cs,
			mb, sizeof(mb),
			EUO_BLANK_UNDEF,
			&uc);
		for (j = 0; j < xlen - 1; j++) {
		    vb_appendf(&r, "%c", mb[j]);
		}
	    }
	} else {
	    vb_appendf(&r, "%s%02x", any ? " " : "", buf[first + i].cc);
	}
	any = true;
    }
    if (any) {
	action_output("%s", vb_buf(&r));
    }
    vb_free(&r);
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

    if ((row < 0 || row > rel_rows || col < 0 || col > rel_cols || len < 0) ||
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
	int i;

	for (i = 0; i < rows; i++) {
	    dump_range(((row+i) * rel_cols) + col, cols, in_ascii, buf,
		    rel_rows, rel_cols);
	}
    }
    return true;
}

static bool
dump_field(unsigned count, const char *name, bool in_ascii)
{
    int faddr;
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
    faddr = find_field_attribute(cursor_addr);
    start = faddr;
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
Ascii_action(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    return dump_fixed(argv, argc, "Ascii", true, ea_buf, ROWS, COLS,
	    cursor_addr);
}

static bool
AsciiField_action(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    return dump_field(argc, "AsciiField", true);
}

static bool
Ebcdic_action(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    return dump_fixed(argv, argc, "Ebcdic", false, ea_buf, ROWS, COLS,
	    cursor_addr);
}

static bool
EbcdicField_action(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    return dump_field(argc, "EbcdicField", false);
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
do_read_buffer(const char **params, unsigned num_params, struct ea *buf,
	int fd)
{
    int	baddr;
    unsigned char current_fg = 0x00;
    unsigned char current_bg = 0x00;
    unsigned char current_gr = 0x00;
    unsigned char current_cs = 0x00;
    unsigned char current_ic = 0x00;
    bool in_ebcdic = false;
    varbuf_t r;

    if (num_params > 0) {
	if (num_params > 1) {
	    popup_an_error("ReadBuffer: extra agruments");
	    return false;
	}
	if (!strncasecmp(params[0], "Ascii", strlen(params[0]))) {
	    in_ebcdic = false;
	} else if (!strncasecmp(params[0], "Ebcdic", strlen(params[0]))) {
	    in_ebcdic = true;
	} else {
	    popup_an_error("ReadBuffer: first parameter must be Ascii or "
		    "Ebcdic");
	    return false;
	}
    }

    if (fd >= 0) {
	char *s;
	int nw;

	s = xs_buffer("rows %d cols %d cursor %d\n", ROWS, COLS, cursor_addr);
	nw = write(fd, s, (int)strlen(s));
	Free(s);
	if (nw < 0) {
		return false;
	}
    }

    /*
     * If the client has looked at the live screen, then if they later
     * execute 'Wait(output)', they will need to wait for output from the
     * host.  output_wait_needed is cleared by sms_host_output,
     * which is called from the write logic in ctlr.c.
     *
     * Any of the following actions will enable Wait(Output):
     * - Ascii
     * - Ebcdic
     * - ReadBuffer
     */     
    if (sms != NULL && buf == ea_buf) {
	set_output_needed(true);
    }

    vb_init(&r);
    baddr = 0;
    do {
	if (!(baddr % COLS)) {
	    if (baddr) {
		if (fd >= 0) {
		    if (write(fd, vb_buf(&r) + 1, (int)(vb_len(&r) - 1)) < 0) {
			goto done;
		    }
		    if (write(fd, "\n", 1) < 0) {
			goto done;
		    }
		} else {
		    action_output("%s", vb_buf(&r) + 1);
		}
	    }
	    vb_reset(&r);
	}
	if (buf[baddr].fa) {
	    vb_appendf(&r, " SF(%02x=%02x", XA_3270, buf[baddr].fa);
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
	} else {
	    bool any_sa = false;
#           define SA_SEP (any_sa? ",": " SA(")

	    if (buf[baddr].fg != current_fg) {
		vb_appendf(&r, "%s%02x=%02x", SA_SEP, XA_FOREGROUND,
			buf[baddr].fg);
		current_fg = buf[baddr].fg;
		any_sa = true;
	    }
	    if (buf[baddr].bg != current_bg) {
		vb_appendf(&r, "%s%02x=%02x", SA_SEP, XA_BACKGROUND,
			buf[baddr].fg);
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
		current_gr = buf[baddr].gr;
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
	    }
	    if (in_ebcdic) {
		if (buf[baddr].cs & CS_GE) {
		    vb_appendf(&r, " GE(%02x)", buf[baddr].cc);
		} else {
		    vb_appendf(&r, " %02x", buf[baddr].cc);
		}
	    } else {
		bool done = false;
		char mb[16];
		size_t j;
		ucs4_t uc;
		size_t len;

		if (IS_LEFT(ctlr_dbcs_state(baddr))) {
		    len = ebcdic_to_multibyte( (buf[baddr].cc << 8) |
			    buf[baddr + 1].cc, mb, sizeof(mb));
		    vb_appends(&r, " ");
		    for (j = 0; j < len-1; j++) {
			vb_appendf(&r, "%02x", mb[j] & 0xff);
		    }
		    done = true;
		} else if (IS_RIGHT(ctlr_dbcs_state(baddr))) {
		    vb_appends(&r, " -");
		    done = true;
		}

		switch (buf[baddr].cc) {
		case EBC_null:
		    mb[0] = '\0';
		    break;
		case EBC_so:
		    mb[0] = 0x0e;
		    mb[1] = '\0';
		    break;
		case EBC_si:
		    mb[0] = 0x0f;
		    mb[1] = '\0';
		    break;
		default:
		    (void) ebcdic_to_multibyte_x(buf[baddr].cc, buf[baddr].cs,
			    mb, sizeof(mb), EUO_NONE, &uc);
		    break;
		}

		if (!done) {
		    vb_appends(&r, " ");
		    if (mb[0] == '\0') {
			vb_appends(&r, "00");
		    } else {
			for (j = 0; mb[j]; j++) {
			    vb_appendf(&r, "%02x", mb[j] & 0xff);
			}
		    }
		}
	    }
	}
	INC_BA(baddr);
    } while (baddr != 0);
    if (fd >= 0) {
	if (write(fd, vb_buf(&r) + 1, (int)(vb_len(&r) - 1)) < 0) {
	    goto done;
	}
	if (write(fd, "\n", 1) < 0) {
	    goto done;
	}
    } else {
	action_output("%s", vb_buf(&r) + 1);
    }
done:
    vb_free(&r);
    return true;
}

/*
 * ReadBuffer action.
 */
static bool
ReadBuffer_action(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    return do_read_buffer(argv, argc, ea_buf, -1);
}

/*
 * The sms prompt is preceeded by a status line with 11 fields:
 *
 *  1 keyboard status
 *     U unlocked
 *     L locked, waiting for host response
 *     E locked, keying error
 *  2 formatting status of screen
 *     F formatted
 *     U unformatted
 *  3 protection status of current field
 *     U unprotected (modifiable)
 *     P protected
 *  4 connect status
 *     N not connected
 *     C(host) connected
 *  5 emulator mode
 *     N not connected
 *     C connected in NVT character mode
 *     L connected in NVT line mode
 *     P 3270 negotiation pending
 *     I connected in 3270 mode
 *  6 model number
 *  7 rows
 *  8 cols
 *  9 cursor row
 * 10 cursor col
 * 11 main window id
 */
static char *
status_string(void)
{
    char kb_stat;
    char fmt_stat;
    char prot_stat;
    char *connect_stat = NULL;
    char em_mode;
    char *r;

    if (!kybdlock) {
	kb_stat = 'U';
    } else {
	kb_stat = 'L';
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
	connect_stat = xs_buffer("C(%s)", current_host);
    } else {
	connect_stat = NewString("N");
    }

    if (CONNECTED) {
	if (IN_NVT) {
	    if (linemode) {
		em_mode = 'L';
	    } else {
		em_mode = 'C';
	    }
	} else if (IN_3270) {
	    em_mode = 'I';
	} else {
	    em_mode = 'P';
	}
    } else {
	em_mode = 'N';
    }

    r = xs_buffer("%c %c %c %s %c %d %d %d %d %d 0x%lx",
	    kb_stat,
	    fmt_stat,
	    prot_stat,
	    connect_stat,
	    em_mode,
	    model_num,
	    ROWS, COLS,
	    cursor_addr / COLS, cursor_addr % COLS,
	    screen_window_number());

    Free(connect_stat);
    return r;
}

static void
script_prompt(bool success)
{
    char *s;
    const char *timing;
    char *t;

    s = status_string();

    if (sms != NULL && sms->accumulated) {
	timing = lazyaf("%ld.%03ld", sms->msec / 1000L, sms->msec % 1000L);
	sms->accumulated = 0;
    } else {
	timing = "-";
    }

    if (sms->type == ST_CB) {
	t = lazyaf("%s %s", s, timing);
	trace_script_output("%s\n", t);
    } else {
	t = lazyaf("%s %s\n%s\n", s, timing, success ? "ok" : "error");
	trace_script_output("%s", t);
    }
    Free(s);

    if (sms->is_socket) {
	send(sms->insocket, t, (int)strlen(t), 0);
    } else if (sms->type == ST_CB) {
	struct sms_cbx cbx = sms->cbx;

	sms_pop(false);
	(*cbx.cb->done)(cbx.handle, success, t, strlen(t));
	sms_continue();
    } else {
	(void) fprintf(sms->outfile, "%s", t);
	(void) fflush(sms->outfile);
    }
}

/* Save the state of the screen for Snap queries. */
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
    set_output_needed(true);
    Replace(snap_status, status_string());

    Replace(snap_buf, (struct ea *)Malloc(ROWS*COLS*sizeof(struct ea)));
    (void) memcpy(snap_buf, ea_buf, ROWS*COLS*sizeof(struct ea));

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

/*
 * "Snap" action, maintains a snapshot for consistent multi-field comparisons:
 *
 *  Snap [Save]
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
 *  Snap ReadBuffer
 *	runs the named command
 *  Snap Wait [tmo] Output
 *      wait for the screen to change, then do a Snap Save
 */
static bool
Snap_action(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    if (sms == NULL || sms->state != SS_RUNNING) {
	popup_an_error("Snap can only be called from scripts or macros");
	return false;
    }

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
	 * If we don't, then Snap(Wait) is equivalent to Snap().
	 */
	if (!sms->output_wait_needed) {
	    snap_save();
	    return true;
	}

	/* Set the new state. */
	sms->state = SS_SWAIT_OUTPUT;

	/* Set up a timeout, if they want one. */
	if (tmo >= 0) {
	    sms->wait_id = AddTimeOut(tmo? (tmo * 1000): 1, wait_timed_out);
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
	action_output("%s", snap_status);
    } else if (!strcasecmp(argv[0], "Rows")) {
	if (argc != 1) {
	    popup_an_error("Extra argument(s)");
	    return false;
	}
	if (snap_status == NULL) {
	    popup_an_error("No saved state");
	    return false;
	}
	action_output("%d", snap_rows);
    } else if (!strcasecmp(argv[0], "Cols")) {
	if (argc != 1) {
	    popup_an_error("Extra argument(s)");
	    return false;
	}
	if (snap_status == NULL) {
	    popup_an_error("No saved state");
	    return false;
	}
	action_output("%d", snap_cols);
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
	return do_read_buffer(argv + 1, argc - 1, snap_buf, -1);
    } else {
	popup_an_error("Snap: Argument must be Save, Status, Rows, Cols, "
		"Wait, Ascii, Ebcdic, or ReadBuffer");
	return false;
    }
    return true;
}

/*
 * Wait for various conditions.
 */
static bool
Wait_action(ia_t ia _is_unused, unsigned argc, const char **argv)
{
    enum sms_state next_state = SS_WAIT_IFIELD;
    float tmo = -1.0;
    char *ptr;
    unsigned np;
    const char **pr;

    /* Pick off the timeout parameter first. */
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

    if (np > 1) {
	popup_an_error("Too many arguments to Wait or invalid timeout value");
	return false;
    }
    if (sms == NULL || sms->state != SS_RUNNING) {
	popup_an_error("Wait can only be called from scripts or macros");
	return false;
    }
    if (np == 1) {
	if (!strcasecmp(pr[0], "NVTMode") || !strcasecmp(pr[0], "ansi")) {
	    if (!IN_NVT) {
		next_state = SS_WAIT_NVT;
	    }
	} else if (!strcasecmp(pr[0], "3270Mode") ||
		   !strcasecmp(pr[0], "3270")) {
	    if (!IN_3270) {
		next_state = SS_WAIT_3270;
	    }
	} else if (!strcasecmp(pr[0], "Output")) {
	    if (sms->output_wait_needed) {
		next_state = SS_WAIT_OUTPUT;
	    } else {
		return true;
	    }
	} else if (!strcasecmp(pr[0], "Disconnect")) {
	    if (CONNECTED) {
		next_state = SS_WAIT_DISC;
	    } else {
		return true;
	    }
	} else if (!strcasecmp(pr[0], "Unlock")) {
	    if (KBWAIT) {
		next_state = SS_WAIT_UNLOCK;
	    } else {
		return true;
	    }
	} else if (tmo > 0.0 && !strcasecmp(pr[0], "Seconds")) {
	    next_state = SS_TIME_WAIT;
	} else if (strcasecmp(pr[0], "InputField")) {
	    popup_an_error("Wait argument must be InputField, "
		    "NVTmode, 3270Mode, Output, Seconds, Disconnect "
		    "or Unlock");
	    return false;
	}
    }
    if (!(CONNECTED || HALF_CONNECTED)) {
	popup_an_error("Wait: Not connected");
	return false;
    }

    /* Is it already okay? */
    if (next_state == SS_WAIT_IFIELD && CAN_PROCEED) {
	return true;
    }

    /* No, wait for it to happen. */
    sms->state = next_state;

    /* Set up a timeout, if they want one. */
    if (tmo >= 0.0) {
	unsigned long tmo_msec = (unsigned long)(tmo * 1000);

	if (tmo_msec == 0) {
	    tmo_msec = 1;
	}
	sms->wait_id = AddTimeOut(tmo_msec, wait_timed_out);
    }
    return true;
}

/*
 * Callback from Connect() and Reconnect() actions, to minimally pause a
 * running sms.
 */
void
sms_connect_wait(void)
{
    if (sms != NULL &&
	(int)sms->state >= (int)SS_RUNNING &&
	sms->state != SS_WAIT_IFIELD &&
	(HALF_CONNECTED || (CONNECTED && (kybdlock & KL_AWAITING_FIRST)))) {

	sms->state = SS_CONNECT_WAIT;
    }
}

/*
 * Callback from ctlr.c, to indicate that the host has changed the screen.
 */
void
sms_host_output(void)
{
    if (sms != NULL) {
	set_output_needed(false);

	switch (sms->state) {
	case SS_SWAIT_OUTPUT:
	    snap_save();
	    /* fall through... */
	case SS_WAIT_OUTPUT:
	    sms->state = SS_RUNNING;
	    sms_continue();
	    break;
	default:
	    break;
	}
    }
}

/* Return whether error pop-ups and action output should be short-circuited. */
static sms_t *
sms_redirect_to(void)
{
    sms_t *s;

    for (s = sms; s != NULL; s = s->next) {
	if ((s->type == ST_CHILD || s->type == ST_PEER || s->type == ST_CB) &&
	    (s->state == SS_RUNNING ||
	     s->state == SS_CONNECT_WAIT ||
	     s->state == SS_WAIT_OUTPUT ||
	     s->state == SS_SWAIT_OUTPUT ||
	     s->state == SS_FT_WAIT ||
	     s->wait_id != NULL_IOID)) {
	    return s;
	}
    }
    return NULL;
}

/* Return whether error pop-ups and acition output should be short-circuited. */
bool
sms_redirect(void)
{
    return sms_redirect_to() != NULL;
}

/* Return whether any scripts are active. */
bool
sms_active(void)
{
    return sms != NULL;
}

/* Translate an expect string (uses C escape syntax). */
static void
expand_expect(const char *s)
{
    char *t = Malloc(strlen(s) + 1);
    char c;
    enum { XS_BASE, XS_BS, XS_O, XS_X } state = XS_BASE;
    int n = 0;
    int nd = 0;
    static char hexes[] = "0123456789abcdef";

    expect_text = t;

    while ((c = *s++)) {
	switch (state) {
	case XS_BASE:
	    if (c == '\\') {
		state = XS_BS;
	    } else {
		*t++ = c;
	    }
	    break;
	case XS_BS:
	    switch (c) {
	    case 'x':
		nd = 0;
		n = 0;
		state = XS_X;
		break;
	    case 'r':
		*t++ = '\r';
		state = XS_BASE;
		break;
	    case 'n':
		*t++ = '\n';
		state = XS_BASE;
		break;
	    case 'b':
		*t++ = '\b';
		state = XS_BASE;
		break;
	    default:
		if (c >= '0' && c <= '7') {
		    nd = 1;
		    n = c - '0';
		    state = XS_O;
		} else {
		    *t++ = c;
		    state = XS_BASE;
		}
		break;
	    }
	    break;
	case XS_O:
	    if (nd < 3 && c >= '0' && c <= '7') {
		n = (n * 8) + (c - '0');
		nd++;
	    } else {
		*t++ = n;
		*t++ = c;
		state = XS_BASE;
	    }
	    break;
	case XS_X:
	    if (isxdigit((unsigned char)c)) {
		n = (n * 16) + (int)(strchr(hexes,
			    tolower((unsigned char)c)) - hexes);
		nd++;
	    } else {
		if (nd) {
		    *t++ = n;
		} else {
		    *t++ = 'x';
		}
		*t++ = c;
		state = XS_BASE;
	    }
	    break;
	}
    }
    expect_len = t - expect_text;
}

/* 'mem' version of strstr */
static char *
memstr(char *s1, char *s2, int n1, int n2)
{
    int i;

    for (i = 0; i <= n1 - n2; i++, s1++) {
	if (*s1 == *s2 && !memcmp(s1, s2, n2)) {
	    return s1;
	}
    }
    return NULL;
}

/* Check for a match against an expect string. */
static bool
expect_matches(void)
{
    size_t ix, i;
    unsigned char buf[NVT_SAVE_SIZE];
    char *t;

    ix = (nvt_save_ix + NVT_SAVE_SIZE - nvt_save_cnt) % NVT_SAVE_SIZE;
    for (i = 0; i < nvt_save_cnt; i++) {
	buf[i] = nvt_save_buf[(ix + i) % NVT_SAVE_SIZE];
    }
    t = memstr((char *)buf, expect_text, (int)nvt_save_cnt, (int)expect_len);
    if (t != NULL) {
	nvt_save_cnt -= ((unsigned char *)t - buf) + expect_len;
	Free(expect_text);
	expect_text = NULL;
	return true;
    } else {
	return false;
    }
}

/* Store an NVT character for use by the Ansi action. */
void
sms_store(unsigned char c)
{
    if (sms == NULL) {
	return;
    }

    /* Save the character in the buffer. */
    nvt_save_buf[nvt_save_ix++] = c;
    nvt_save_ix %= NVT_SAVE_SIZE;
    if (nvt_save_cnt < NVT_SAVE_SIZE) {
	nvt_save_cnt++;
    }

    /* If a script or macro is waiting to match a string, check now. */
    if (sms->state == SS_EXPECTING && expect_matches()) {
	RemoveTimeOut(sms->expect_id);
	sms->expect_id = NULL_IOID;
	sms->state = SS_INCOMPLETE;
	sms_continue();
    }
}

/* Dump whatever NVT data has been sent by the host since last called. */
static bool
AnsiText_action(ia_t ia, unsigned argc, const char **argv)
{
    size_t i;
    size_t ix;
    unsigned char c;
    varbuf_t r;

    action_debug("AnsiText", ia, argc, argv);
    if (check_argc("AnsiText", argc, 0, 0) < 0) {
	return false;
    }

    if (!nvt_save_cnt) {
	return true;
    }

    ix = (nvt_save_ix + NVT_SAVE_SIZE - nvt_save_cnt) % NVT_SAVE_SIZE;
    vb_init(&r);
    for (i = 0; i < nvt_save_cnt; i++) {
	c = nvt_save_buf[(ix + i) % NVT_SAVE_SIZE];
	if (!(c & ~0x1f)) switch (c) {
	    case '\n':
		vb_appends(&r, "\\n");
		break;
	    case '\r':
		vb_appends(&r, "\\r");
		break;
	    case '\b':
		vb_appends(&r, "\\b");
		break;
	    default:
		vb_appendf(&r, "\\%03o", c);
		break;
	} else if (c == '\\') {
	    vb_appends(&r, "\\\\");
	} else {
	    vb_append(&r, (char *)&c, 1);
	}
    }
    action_output("%s", vb_buf(&r));
    vb_free(&r);
    nvt_save_cnt = 0;
    nvt_save_ix = 0;
    return true;
}

/* Pause a script. */
static bool
PauseScript_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("PauseScript", ia, argc, argv);
    if (check_argc("PauseScript", argc, 0, 0) < 0) {
	return false;
    }
    if (sms == NULL || sms->type == ST_STRING || sms->type == ST_COMMAND ||
	    sms->type == ST_IDLE) {
	popup_an_error("PauseScript can only be called from a script");
	return false;
    }
    sms->state = SS_PAUSED;
    return true;
}

/* Continue a script. */
static bool
ContinueScript_action(ia_t ia, unsigned argc, const char **argv)
{
    sms_t *s;

    action_debug("ContinueScript", ia, argc, argv);
    if (check_argc("ContinueScript", argc, 0, 1) < 0) {
	return false;
    }

    /*
     * Skip past whatever scripts are RUNNING or INCOMPLETE at the top of the
     * stack, until we find one that is PAUSED.
     */
    for (s = sms; s != NULL; s = s->next) {
	if (s->state != SS_RUNNING && s->state != SS_INCOMPLETE) {
	    break;
	}
    }
    if (s == NULL || s->state != SS_PAUSED) {
	popup_an_error("ContinueScript: No script waiting");
	sms_continue();
	return false;
    }

    /* Pop the RUNNING and INCOMPLETE scripts. */
    while (sms != NULL && sms->state == SS_RUNNING) {
	sms_pop(false);
    }

    /* Continue the running script and output the token to it. */
    sms->state = SS_RUNNING;
    if (argc) {
	action_output("%s", argv[0]);
    }
    sms_continue();
    return true;
}

/* Stop listening to stdin. */
static bool
CloseScript_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("CloseScript", ia, argc, argv);
    if (check_argc("CloseScript", argc, 0, 1) < 0) {
	return false;
    }

    if (sms != NULL && (sms->type == ST_PEER || sms->type == ST_CHILD)) {

	/* Close this script. */
	sms->state = SS_CLOSING;
	script_prompt(true);

	/* If nonzero status passed, fail the calling script. */
	if (argc > 0 &&
	    atoi(argv[0]) != 0 &&
	    sms->next != NULL) {
	    sms->next->success = false;
	    if (sms->is_login) {
		host_disconnect(true);
	    }
	}
    } else {
	popup_an_error("CloseScript can only be called from a script");
	return false;
    }
    return true;
}

/* Execute an arbitrary shell command. */
static bool
Execute_action(ia_t ia, unsigned argc, const char **argv)
{
    int status;
    bool rv = true;

    action_debug("Execute", ia, argc, argv);
    if (check_argc("Execute", argc, 1, 1) < 0) {
	return false;
    }

    status = system(argv[0]);
    if (status < 0) {
	popup_an_errno(errno, "system(\"%s\") failed", argv[0]);
	rv = false;
    } else if (status != 0) {
#if defined(_WIN32) /*[*/
	popup_an_error("system(\"%s\") exited with status %d\n", argv[0],
		status);
#else /*][*/
	if (WIFEXITED(status)) {
	    popup_an_error("system(\"%s\") exited with status %d\n", argv[0],
		    WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
	    popup_an_error("system(\"%s\") killed by signal %d\n", argv[0],
		    WTERMSIG(status));
	} else if (WIFSTOPPED(status)) {
	    popup_an_error("system(\"%s\") stopped by signal %d\n", argv[0],
		    WSTOPSIG(status));
	}
#endif /*]*/
	rv = false;
    }

    /* Get back mouse events; system() cancels them. */
    screen_system_fixup();

    return rv;
}

/* Timeout for Expect action. */
static void
expect_timed_out(ioid_t id _is_unused)
{
    if (sms == NULL || sms->state != SS_EXPECTING) {
	return;
    }

    Free(expect_text);
    expect_text = NULL;
    popup_an_error("Expect: Timed out");
    sms->expect_id = NULL_IOID;
    sms->state = SS_INCOMPLETE;
    sms->success = false;
    if (sms->is_login) {
	host_disconnect(true);
    }
    sms_continue();
}

/* Timeout for Wait action. */
static void
wait_timed_out(ioid_t id _is_unused)
{
    /* If they just wanted a delay, succeed. */
    if (sms->state == SS_TIME_WAIT) {
	sms->success = true;
	sms->state = SS_INCOMPLETE;
	sms->wait_id = NULL_IOID;
	sms_continue();
	return;
    }

    /* Pop up the error message. */
    popup_an_error("Wait: Timed out");

    /* Forget the ID. */
    sms->wait_id = NULL_IOID;

    /* If this is a login macro, it has failed. */
    if (sms->is_login) {
	host_disconnect(true);
    }

    sms->success = false;
    sms->state = SS_INCOMPLETE;

    /* Let the script proceed. */
    sms_continue();
}

/* Wait for a string from the host (NVT mode only). */
static bool
Expect_action(ia_t ia, unsigned argc, const char **argv)
{
    int tmo;

    action_debug("Expect", ia, argc, argv);
    if (check_argc("Expect", argc, 1, 2) < 0) {
	return false;
    }

    /* Verify the environment and parameters. */
    if (sms == NULL || sms->state != SS_RUNNING) {
	popup_an_error("Expect can only be called from a script or macro");
	return false;
    }
    if (!IN_NVT) {
	popup_an_error("Expect is valid only when connected in NVT mode");
	return false;
    }
    if (argc == 2) {
	tmo = atoi(argv[1]);
	if (tmo < 1 || tmo > 600) {
	    popup_an_error("Expect: Invalid timeout: %s", argv[1]);
	    return false;
	}
    } else {
	tmo = 30;
    }

    /* See if the text is there already; if not, wait for it. */
    expand_expect(argv[0]);
    if (!expect_matches()) {
	sms->expect_id = AddTimeOut(tmo * 1000, expect_timed_out);
	sms->state = SS_EXPECTING;
    }
    /* else allow sms to proceed */
    return true;
}

#if defined(_WIN32) /*[*/
/* Let the system pick a TCP port to bind to, and listen on it. */
static unsigned short
pick_port(socket_t *sp)
{
    	socket_t s;
    	struct sockaddr_in sin;
	socklen_t len;

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
	    	popup_an_error("socket: %s\n", win32_strerror(GetLastError()));
		return 0;
	}
	(void) memset(&sin, '\0', sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	    	popup_an_error("bind: %s\n", win32_strerror(GetLastError()));
		SOCK_CLOSE(s);
		return 0;
	}
	len = sizeof(sin);
	if (getsockname(s, (struct sockaddr *)&sin, &len) < 0) {
	    	popup_an_error("getsockaddr: %s\n",
			win32_strerror(GetLastError()));
		SOCK_CLOSE(s);
		return 0;
	}
	if (listen(s, 10) < 0) {
	    	popup_an_error("listen: %s\n", win32_strerror(GetLastError()));
		SOCK_CLOSE(s);
		return 0;
	}
	*sp = s;
	return ntohs(sin.sin_port);
}
#endif /*]*/

/* "Script" action, runs a script as a child process. */
#if !defined(_WIN32) /*[*/
static bool
Script_action(ia_t ia, unsigned argc, const char **argv)
{
    int inpipe[2];
    int outpipe[2];

    if (argc < 1) {
	popup_an_error("Script requires at least one argument");
	return false;
    }

    /* Create a new script description. */
    if (!sms_push(ST_CHILD)) {
	return false;
    }

    /*
     * Create pipes and stdout stream for the script process.
     *  inpipe[] is read by x3270, written by the script
     *  outpipe[] is written by x3270, read by the script
     */
    if (pipe(inpipe) < 0) {
	sms_pop(false);
	popup_an_error("pipe() failed");
	return false;
    }
    if (pipe(outpipe) < 0) {
	(void) close(inpipe[0]);
	(void) close(inpipe[1]);
	sms_pop(false);
	popup_an_error("pipe() failed");
	return false;
    }
    if ((sms->outfile = fdopen(outpipe[1], "w")) == NULL) {
	(void) close(inpipe[0]);
	(void) close(inpipe[1]);
	(void) close(outpipe[0]);
	(void) close(outpipe[1]);
	sms_pop(false);
	popup_an_error("fdopen() failed");
	return false;
    }
    (void) SETLINEBUF(sms->outfile);

    /* Fork and exec the script process. */
    if ((sms->pid = fork_child()) < 0) {
	(void) close(inpipe[0]);
	(void) close(inpipe[1]);
	(void) close(outpipe[0]);
	sms_pop(false);
	popup_an_error("fork() failed");
	return false;
    }

    /* Child processing. */
    if (sms->pid == 0) {
	char **child_argv;
	unsigned i;

	/* Clean up the pipes. */
	(void) close(outpipe[1]);
	(void) close(inpipe[0]);

	/* Export the names of the pipes into the environment. */
	(void) putenv(xs_buffer("X3270OUTPUT=%d", outpipe[0]));
	(void) putenv(xs_buffer("X3270INPUT=%d", inpipe[1]));

	/* Set up arguments. */
	child_argv = (char **)Malloc((argc + 1) * sizeof(char *));
	for (i = 0; i < argc; i++) {
	    child_argv[i] = (char *)argv[i];
	}
	child_argv[i] = NULL;

	/* Exec. */
	(void) execvp(argv[0], child_argv);
	(void) fprintf(stderr, "exec(%s) failed\n", argv[0]);
	(void) _exit(1);
    }

    /* Clean up our ends of the pipes. */
    sms->infd = inpipe[0];
    (void) close(inpipe[1]);
    (void) close(outpipe[0]);

    /* Enable input. */
    script_enable();

    /* Set up to reap the child's exit status. */
    ++children;

    return true;
}
#endif /*]*/

#if defined(_WIN32) /*[*/
/* "Script" action, runs a script as a child process. */
static bool
Script_action(ia_t ia, unsigned argc, const char **argv)
{
    socket_t s = INVALID_SOCKET;
    unsigned short port = 0;
    HANDLE hevent;
    char *pe;
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION process_information;
    char *args;
    unsigned i;

    action_debug("Script", ia, argc, argv);

    if (argc < 1) {
	popup_an_error("Script requires at least one argument");
	return false;
    }

    /* Set up X3270PORT for the child process. */
    port = pick_port(&s);
    if (port == 0) {
	return false;
    }
    hevent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hevent == NULL) {
	popup_an_error("CreateEvent: %s", win32_strerror(GetLastError()));
	closesocket(s);
	return false;
    }
    if (WSAEventSelect(s, hevent, FD_ACCEPT) != 0) {
	popup_an_error("WSAEventSelect: %s", win32_strerror(GetLastError()));
	closesocket(s);
	return false;
    }

    pe = xs_buffer("X3270PORT=%d", port);
    putenv(pe);
    Free(pe);

    /* Start the child process. */
    (void) memset(&startupinfo, '\0', sizeof(STARTUPINFO));
    startupinfo.cb = sizeof(STARTUPINFO);
    (void) memset(&process_information, '\0', sizeof(PROCESS_INFORMATION));
    args = NewString(argv[0]);
    for (i = 1; i < argc; i++) {
	char *t;

	if (strchr(argv[i], ' ') != NULL &&
	    argv[i][0] != '"' &&
	    argv[i][strlen(argv[i]) - 1] != '"') {
	    t = xs_buffer("%s \"%s\"", args, argv[i]);
	} else {
	    t = xs_buffer("%s %s", args, argv[i]);
	}
	Free(args);
	args = t;
    }
    if (CreateProcess(
		NULL,
		args,
		NULL,
		NULL,
		FALSE,
		DETACHED_PROCESS,
		NULL,
		NULL,
		&startupinfo,
		&process_information) == 0) {
	popup_an_error("CreateProcess(%s) failed: %s", argv[0],
		win32_strerror(GetLastError()));
	Free(args);
	return false;
    } else {
	Free(args);
	CloseHandle(process_information.hThread);
    }

    /* Create a new script description. */
    if (!sms_push(ST_CHILD)) {
	return false;
    }
    sms->child_handle = process_information.hProcess;
    sms->inhandle = hevent;
    sms->insocket = s;
    sms->is_socket = true;

    /*
     * Wait for the child process to exit.
     * Note that this is an asynchronous event -- exits for multiple
     * children can happen in any order.
     */
    sms->exit_id = AddInput(process_information.hProcess, child_exited);

    /* Allow the child script to connect back to us. */
    sms->listen_id = AddInput(hevent, child_socket_connection);

    /* Enable input. */
    script_enable();

    return true;
}
#endif /*]*/

/* "Macro" action, explicitly invokes a named macro. */
static bool
Macro_action(ia_t ia, unsigned argc, const char **argv)
{
	struct macro_def *m;

    action_debug("Macro", ia, argc, argv);
    if (check_argc("Macro", argc, 1, 1) < 0) {
	return false;
    }
    for (m = macro_defs; m != NULL; m = m->next) {
	if (!strcmp(m->name, argv[0])) {
	    push_macro(m->action, false);
	    return true;
	}
    }
    popup_an_error("no such macro: '%s'", argv[0]);
    return false;
}

/*
 * Idle cancellation: cancels the idle command if the current sms or any sms
 * that called it caused an error.
 */
void
cancel_if_idle_command(void)
{
    sms_t *s;

    for (s = sms; s != NULL; s = s->next) {
	if (s->type == ST_IDLE) {
	    cancel_idle_timer();
	    s->idle_error = true;
	    vtrace("Cancelling idle command");
	    break;
	}
    }
}

/* "Printer" action, starts or stops a printer session. */
static bool
Printer_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Printer", ia, argc, argv);
    if (check_argc("Printer", argc, 1, 2) < 0) {
	return false;
    }
    if (!strcasecmp(argv[0], "Start")) {
	pr3287_session_start((argc > 1)? argv[1] : NULL);
    } else if (!strcasecmp(argv[0], "Stop")) {
	if (argc != 1) {
	    popup_an_error("Printer: Extra argument(s)");
	    return false;
	}
	pr3287_session_stop();
    } else {
	popup_an_error("Printer: Argument must be Start or Stop");
	return false;
    }
    return true;
}

/* Abort all running scripts. */
void
abort_script(void)
{
	while (sms != NULL) {
#if !defined(_WIN32) /*[*/
		if (sms->type == ST_CHILD && sms->pid > 0)
			(void) kill(sms->pid, SIGTERM);
#endif /*]*/
		sms_pop(true);
	}
}

/* "Abort" action, stops pending scripts. */
static bool
Abort_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Abort", ia, argc, argv);
    if (check_argc("Abort", argc, 0, 0) < 0) {
	return false;
    }
    if (ia == IA_SCRIPT) {
	return false;
    }
#if !defined(_WIN32) /*[*/
    child_ignore_output();
#endif /*]*/
    abort_script();
    return true;
}

/* Accumulate command execution time. */
void
sms_accumulate_time(struct timeval *t0, struct timeval *t1)
{
    sms_t *s;
    unsigned long msec;

    msec = (t1->tv_sec - t0->tv_sec) * 1000 +
	   (t1->tv_usec - t0->tv_usec + 500) / 1000;

    if (sms != NULL) {
	sms->accumulated = true;
	sms->msec += msec;
#if defined(DEBUG_ACCUMULATE) /*[*/
	printf("%s: accumulated %lu msec\n", ST_NAME, sms->msec);
#endif /*]*/
    }

    s = sms_redirect_to();
    if (s != NULL) {
	s->accumulated = true;
	s->msec += msec;
    }
}

static bool
Query_action(ia_t ia, unsigned argc, const char **argv)
{
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

    switch (argc) {
    case 0:
	for (i = 0; queries[i].name != NULL; i++) {
	    action_output("%s: %s", queries[i].name,
		    queries[i].fn? (*queries[i].fn)(): queries[i].string);
	}
	break;
    case 1:
	for (i = 0; queries[i].name != NULL; i++) {
	    if (!strcasecmp(argv[0], queries[i].name)) {
		const char *s;

		if (queries[i].fn) {
		    s = (*queries[i].fn)();
		} else {
		    s = queries[i].string;
		}
		action_output("%s\n", *s? s: " ");
		return true;
	    }
	}
	popup_an_error("Query: Unknown parameter");
	break;
    default:
	popup_an_error("Query: Requires 0 or 1 arguments");
	break;
    }
    return true;
}

/*
 * Bell action, used by scripts to ring the console bell and enter a comment
 * into the trace log.
 */
static bool
Bell_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Bell", ia, argc, argv);
    if (check_argc("Bell", argc, 0, 0) < 0) {
	return false;
    }
    if (product_has_display()) {
	ring_bell();
    } else {
	action_output("(ding)");
    }

    return true;
}

static bool
Source_action(ia_t ia, unsigned argc, const char **argv)
{
    int fd;
    char *expanded_filename;

    action_debug("Source", ia, argc, argv);
    if (check_argc("Source", argc, 1, 1) < 0) {
	return false;
    }
    expanded_filename = do_subst(argv[0], DS_VARS | DS_TILDE);
    fd = open(expanded_filename, O_RDONLY);
    if (fd < 0) {
	Free(expanded_filename);
	popup_an_errno(errno, "%s", argv[0]);
	return false;
    }
    Free(expanded_filename);
    push_file(fd);
    return true;
}
