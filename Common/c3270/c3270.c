/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	c3270.c
 *		A curses-based 3270 Terminal Emulator
 *		A Windows console 3270 Terminal Emulator
 *		Main proceudre.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#endif /*]*/
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actions.h"
#include "base64.h"
#include "bind-opt.h"
#include "boolstr.h"
#include "codepage.h"
#include "cookiefile.h"
#include "ckeypad.h"
#include "cscreen.h"
#include "cstatus.h"
#include "ctlrc.h"
#include "cmenubar.h"
#include "unicodec.h"
#include "ft.h"
#include "glue.h"
#include "glue_gui.h"
#include "help.h"
#include "host.h"
#include "httpd-core.h"
#include "httpd-nodes.h"
#include "httpd-io.h"
#include "icmdc.h"
#include "idle.h"
#include "keymap.h"
#include "kybd.h"
#include "login_macro.h"
#include "model.h"
#include "names.h"
#include "nvt.h"
#include "opts.h"
#include "popups.h"
#include "pr3287_session.h"
#include "prefer.h"
#include "print_screen.h"
#include "product.h"
#include "proxy_toggle.h"
#include "query.h"
#include "s3270_proto.h"
#include "save_restore.h"
#include "screen.h"
#include "selectc.h"
#include "sio_glue.h"
#include "split_host.h"
#include "status.h"
#include "status_dump.h"
#include "task.h"
#include "telnet.h"
#include "telnet_new_environ.h"
#include "telnet_gui.h"
#include "toggles.h"
#include "trace.h"
#include "screentrace.h" /* has to come after trace.h */
#include "txa.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"
#include "vstatus.h"
#include "xio.h"
#include "xpopen.h"
#include "xscroll.h"

#if defined(HAVE_LIBREADLINE) /*[*/
#include <readline/readline.h>
#if defined(HAVE_READLINE_HISTORY_H) /*[*/
#include <readline/history.h>
#endif /*]*/
#endif /*]*/

#if defined(_WIN32) /*[*/
#include "relinkc.h"
#include "w3misc.h"
#include "wc3270.h"
#include "windirs.h"
#include "winvers.h"
#endif /*]*/

#if defined(_WIN32) /*[*/
# define DELENV		"WC3DEL"
#endif /*]*/

#if !defined(_WIN32) /*[*/
# if defined(HAVE_LIBREADLINE) /*[*/
#  define PROMPT_PRE	Asprintf("%c%s%c", '\001', \
				screen_setaf(ACOLOR_BLUE), '\002')
#  define PROMPT_POST	Asprintf("%c%s%c", '\001', screen_op(), '\002')
# else /*][*/
#  define PROMPT_PRE	screen_setaf(ACOLOR_BLUE)
#  define PROMPT_POST	screen_op()
# endif /*]*/
#else /*][*/
# define PROMPT_PRE	""
# define PROMPT_POST	""
#endif /*]*/

static void c3270_push_command(const char *s);
static void interact(void);
static void stop_pager(void);
static void display_prompt(void);

#if !defined(_WIN32) /*[*/
static bool merge_profile(void);
#endif /*]*/

#if defined(HAVE_LIBREADLINE) /*[*/
static char **attempted_completion(const char *text, int start, int end);
static char *completion_entry(const char *, int);
static char *readline_command;
static bool readline_done = false;
#endif /*]*/

static bool color_prompt;

/* Pager state. */
#if !defined(_WIN32) /*[*/
struct {
    FILE *fp;		/* file pointer */
    pid_t pid;		/* process ID */
} pager = { NULL, 0 };
#else /*][*/
struct {
    int rows;		/* current number of rows */
    int cols;		/* current number of colunns */
    int rowcnt;		/* number of rows displayed so far */
    int nw;		/* number of bytes written in the prompt */
    char *residual;	/* residual output */
    bool flushing;	/* true if 'q' selected and output is being discarded */
    bool running;	/* true if pager is active */
} pager = { 25, 80, 0, 0, NULL, false, false };
static void pager_key_done(void);
static void pager_output(const char *s, bool success);
#endif /*]*/
static bool any_error_output;
static bool command_running = false;
static bool command_output = false;
static bool connect_once = false;

#if !defined(_WIN32) /*[*/
# define PAGER_RUNNING	(pager.pid != 0)
#else /*][*/
# define PAGER_RUNNING	(pager.residual != NULL)
#endif /*][*/

#if !defined(_WIN32) /*[*/
static bool sigtstp_pending = false;
static int signalpipe[2];
sigset_t pending_signals;
static void synchronous_signal(iosrc_t fd, ioid_t id);
static void common_handler(int signum);
#else /*][*/
static void windows_sigint(iosrc_t fd, ioid_t id);
static void windows_ctrlc(void);
static HANDLE sigint_event;
#endif /*]*/
static struct {
    char *string;		/* current prompt text */
    char *default_string;	/* default prompt text */
    bool displayed;
} prompt = { NULL, NULL, false };

static char *error_pending = NULL;
static bool exit_pending = false;

static bool aux_input = false;
static bool aux_pwinput = false;

static bool escape_single = false;

static void c3270_input(iosrc_t fd, ioid_t id);
static ioid_t c3270_input_id = NULL_IOID;
static task_cb_ir_state_t command_ir_state;

#if defined(_WIN32) /*[*/
char *instdir = NULL;
char *mydesktop = NULL;
char *mydocs3270 = NULL;
char *commondocs3270 = NULL;
unsigned windirs_flags;
static void start_auto_shortcut(int argc, char *argv[]);

static struct {
    HANDLE thread;		/* thread handle */
    HANDLE enable_event;	/* let the thread do a read */
    HANDLE done_event;		/* thread has read input */
    enum imode { LINE, KEY } mode; /* input mode */
    char buf[1024];		/* input buffer */
    int nr;			/* number of bytes read */
    int error;			/* error code */
} inthread;
static DWORD WINAPI inthread_read(LPVOID lpParameter);
# define PAGER_PROMPT "Press any key to continue . . . "
#endif /*]*/
static bool glue_gui_xoutput(const char *s, bool success);

static void command_setir(task_cbh handle, void *irhandle);
static void *command_getir(task_cbh handle);
static void command_setir_state(task_cbh handle, const char *name, void *state,
	ir_state_abort_cb abort_cb);
static void *command_getir_state(task_cbh handle, const char *name);
static void *command_irhandle;

static irv_t command_irv = {
    command_setir,
    command_getir,
    command_setir_state,
    command_getir_state
};

static void command_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool command_done(task_cbh handle, bool success, bool abort);
static unsigned command_getflags(task_cbh handle);
static void command_reqinput(task_cbh handle, const char *buf, size_t len,
	bool echo);

/* Callback block for actions. */
static tcb_t command_cb = {
    "command",
    IA_COMMAND,
    CB_NEW_TASKQ,
    command_data,
    command_done,
    NULL,
    NULL,
    NULL,
    command_getflags,
    &command_irv,
    NULL,
    command_reqinput
};

static void c3270_register(void);

void
usage(const char *msg)
{
    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, "Usage: %s [options] [prefix:][LUname@]hostname[:port]\n",
	    programname);
    fprintf(stderr, "       %s [options] [<session-file>.s3270]\n",
	    programname);
    fprintf(stderr, "Use " OptHelp1 " for the list of options\n");
    exit(1);
}

/**
 * Wrapper for screen_suspend() that fixes up signal handling.
 */
static bool
c3270_screen_suspend(void)
{
    bool needed = screen_suspend();
    if (needed) {
#if !defined(_WIN32) /*[*/
	/* screen_init/screen_resume reset these. */
	signal(SIGINT, common_handler);
	signal(SIGTSTP, common_handler);
#endif /*]*/
    }
    return needed;
}

/* Make sure error messages are seen. */
static void
pause_for_errors(void)
{
    char s[10];

    if (any_error_output) {
	c3270_screen_suspend();
	printf("[Press <Enter>] ");
	fflush(stdout);
	if (fgets(s, sizeof(s), stdin) == NULL) {
	    x3270_exit(1);
	}
	any_error_output = false;
    }
}

/* Callback for 3270-mode state changes. */
static void
c3270_3270_mode(bool ignored)
{
    if (x3270_exiting) {
	return;
    }

    if (CONNECTED || appres.disconnect_clear) {
#if defined(C3270_80_132) /*[*/
	if (appres.c3270.altscreen != NULL) {
	    ctlr_erase(false);
	} else
#endif /*]*/
	{
	    ctlr_erase(true);
	}
    }
} 

/**
 * GUI redirect function for popup_an_error.
 * This handles asynchronous errors, such as file transfers that do not start
 * or abort.
 *
 * @param[in] s			Text to display
 * @param[in] set_any_error	true to set any_error_output
 * @returns true
 */
static bool
glue_gui_error_cond(const char *s, bool set_any_error)
{
    bool was_escaped = escaped;

    if (command_running || PAGER_RUNNING) {
	/* You can't interrupt a running command. */
	if (error_pending != NULL) {
	    char *xerror = Asprintf("%s\n%s", error_pending, s);

	    Replace(error_pending, xerror);
	} else {
	    Replace(error_pending, NewString(s));
	}
	return true;
    }

    if (!was_escaped) {
	c3270_screen_suspend();
    } else {
#if defined(_WIN32) /*[*/
	/*
	 * Send yourself an ESC to flush current input.
	 * This needs to happen before displaying the text.
	 */
	screen_send_esc();
#endif /*]*/
    }

    if (prompt.displayed) {
	printf("\n");
	fflush(stdout);
    }
#if !defined(_WIN32) /*[*/
    printf("%s%s%s\n",
	    color_prompt? screen_setaf(ACOLOR_RED): "",
	    s,
	    color_prompt? screen_op(): "");
#else /*][*/
    screen_color(PC_ERROR);
    printf("%s", s);
    screen_color(PC_DEFAULT);
    printf("\n");
#endif /*]*/
    fflush(stdout);
    if (set_any_error) {
	any_error_output = true;
    }

    if (was_escaped) {
	/* Interrupted the prompt. */
#if defined(HAVE_LIBREADLINE) /*[*/
	/* Redisplay the prompt and any pending input. */
	rl_forced_update_display();
#elif !defined(_WIN32) /*[*/
	/* Discard any pending input. */
	tcflush(0, TCIFLUSH);
	prompt.displayed = false;
#else /*][*/
	prompt.displayed = false;
#endif /*]*/
    }

    return true;
}

/* Callback for connection state changes. */
static void
c3270_connect(bool ignored)
{
    if (x3270_exiting) {
	return;
    }

    if (PCONNECTED) {
	macros_init();
    }

    c3270_3270_mode(true);

    if (CONNECTED) {
	/* Clear the last 'Trying' text. */
	status_push(NULL);
	return;
    }

    if (cstate == RESOLVING || cstate == TCP_PENDING) {
	return;
    }

    /* Not connected. */
    if (!appres.secure &&
	    !PCONNECTED &&
	    !host_retry_mode) {
	glue_gui_error_cond("Disconnected.", false);
    }
    if (connect_once && !host_retry_mode) {
	/* Exit after the connection is broken. */
	if (command_running || PAGER_RUNNING) {
	    /* Exit when the command and pager are complete. */
	    exit_pending = true;
	} else {
	    /* Exit right now. */
	    screen_suspend();
#if !defined(_WIN32) /*[*/
	    if (prompt.displayed) {
		printf("\n");
		fflush(stdout);
	    }
#else /*][*/
	    pause_for_errors();
#endif /*]*/
	    x3270_exit(0);
	}
    }
} 

/* Callback for application exit. */
static void
main_exiting(bool ignored)
{       
    if (escaped) {
	stop_pager();
#if defined(HAVE_LIBREADLINE) /*[*/
	rl_callback_handler_remove();
#endif /*]*/
	printf("\n");
    } else {
	if (c3270_screen_suspend()) {
	    screen_final();
	}
    }
} 

#if !defined(_WIN32) /*[*/
/* Empty SIGCHLD handler, ensuring that we can collect child exit status. */
static void
sigchld_handler(int ignored)
{
#if !defined(_AIX) /*[*/
    signal(SIGCHLD, sigchld_handler);
#endif /*]*/
}
#endif /*]*/

/**
 * Redirection for Warning.
 */
static void
c3270_Warning(const char *s)
{
    if (!escaped) {
	if (error_pending != NULL) {
	    char *xerror = Asprintf("%s\n%s", error_pending, s);

	    Replace(error_pending, xerror);
	} else {
	    Replace(error_pending, NewString(s));
	}
    } else {
	fprintf(stderr, "Warning: %s\n", s);
	fflush(stderr);
	any_error_output = true;
    }
}

#if defined(_WIN32) /*[*/
/*
 * wc3270 version of Error, that makes sure the user has a chance to see the
 * error message before we close the window.
 */
static void
c3270_Error(const char *s)
{
    /* Dump the error on the console. */
    fprintf(stderr, "Error: %s\n", s);
    fflush(stderr);

    /* Wait for the <Return> key, and then exit. */
    x3270_exit(1);
}

/* Pause before exiting. */
static void
exit_pause(bool mode _is_unused)
{
    if (x3270_exit_code) {
	char buf[2];

	printf("\n[Press <Enter>] ");
	fflush(stdout);
	fgets(buf, sizeof(buf), stdin);
    }
}
#endif /*]*/

/* Initialize the prompt. */
static void
prompt_init(void)
{
    prompt.default_string = appres.secure? "[Press <Enter>] ":
	(color_prompt?
	 Asprintf("%s%s> %s", PROMPT_PRE, app, PROMPT_POST):
	 Asprintf("%s> ", app));
    prompt.string = prompt.default_string;
}

int
main(int argc, char *argv[])
{
    const char	*cl_hostname = NULL;
#if defined(_WIN32) /*[*/
    char	*delenv;
    int		save_argc;
    char	**save_argv;
    int		i;
#endif /*]*/

    Warning_redirect = c3270_Warning;
#if defined(_WIN32) /*[*/
    /* Redirect Error() so we pause. */
    Error_redirect = c3270_Error;

    /* Register a final exit function, so we pause. */
    register_schange_ordered(ST_EXITING, exit_pause, ORDER_LAST);

    /* Get Windows version and directories. */
    get_version_info();
    if (!get_dirs("wc3270", &instdir, &mydesktop, NULL, NULL, NULL, NULL, NULL,
		&mydocs3270, &commondocs3270, &windirs_flags)) {
	x3270_exit(1);
    }

    /* Start Winsock. */
    if (sockstart()) {
	x3270_exit(1);
    }
#endif /*]*/

#if !defined(_WIN32) && !defined(CURSES_WIDE) /*[*/
    /* Explicitly turn off DBCS if wide curses is not supported. */
    dbcs_allowed = false;
#endif /*]*/

    /*
     * Call the module registration functions, to build up the tables of
     * actions, options and callbacks.
     */
    c3270_register();
    codepage_register();
    ctlr_register();
    ft_register();
    help_register();
    host_register();
    icmd_register();
    idle_register();
    keymap_register();
    keypad_register();
    kybd_register();
    task_register();
    query_register();
    menubar_register();
    nvt_register();
    pr3287_session_register();
    print_screen_register();
    save_restore_register();
#if defined(_WIN32) /*[*/
    select_register();
#endif /*]*/
    screen_register();
    scroll_register();
    toggles_register();
    trace_register();
    screentrace_register();
    xio_register();
    sio_glue_register();
    hio_register();
    proxy_register();
    model_register();
    net_register();
    login_macro_register();
    vstatus_register();
    prefer_register();
    telnet_new_environ_register();

#if !defined(_WIN32) /*[*/
    register_merge_profile(merge_profile);
#endif

#if defined(_WIN32) /*[*/
    /* Save the command-line arguments for auto-shortcut mode. */
    save_argc = argc;
    save_argv = (char **)Malloc((argc + 1) * sizeof(char *));
    for (i = 0; i < argc; i++) {
	save_argv[i] = NewString(argv[i]);
    }
    save_argv[i] = NULL;
#endif /*]*/

    argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

    printf("%s\n\nType 'show copyright' for full copyright information.\n\
Type 'help' for help information.\n\n",
	    get_about());

#if defined(_WIN32) /*[*/
    /* Delete the link file, if we've been told do. */
    delenv = getenv(DELENV);
    if (delenv != NULL) {
	unlink(delenv);
	putenv(DELENV "=");
    }

    /* Check for auto-shortcut mode. */
    if (appres.c3270.auto_shortcut) {
	start_auto_shortcut(save_argc, save_argv);
	exit(0);
    }
#endif /*]*/

    if (codepage_init(appres.codepage) != CS_OKAY) {
	xs_warning("Cannot find code page \"%s\"", appres.codepage);
	codepage_init(NULL);
    }
    model_init();

#if defined(HAVE_LIBREADLINE) /*[*/
    /* Set up readline. */
    rl_readline_name = "c3270";
    rl_initialize();
    rl_attempted_completion_function = attempted_completion;
#if defined(RL_READLINE_VERSION) && (RL_READLINE_VERSION > 0x0402) /*[*/
    rl_completion_entry_function = completion_entry;
#else /*][*/
    rl_completion_entry_function = (Function *)completion_entry;
#endif /*]*/
#endif /*]*/

#if !defined(_WIN32) /*[*/
    /* Set up the signal pipes. */
    sigemptyset(&pending_signals);
    if (pipe(signalpipe) < 0) {
	perror("pipe");
	exit(1);
    }
    fcntl(signalpipe[0], F_SETFD, 1);
    fcntl(signalpipe[1], F_SETFD, 1);
    AddInput(signalpipe[0], synchronous_signal);
#else /*][*/
    sigint_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    AddInput(sigint_event, windows_sigint);
    screen_set_ctrlc_fn(windows_ctrlc);
#endif /*]*/

    /* Get the screen set up as early as possible. */
    screen_init();
#if !defined(_WIN32) /*[*/
    color_prompt = screen_has_ansi_color();
#endif /*]*/

    prompt_init();

#if defined(_WIN32) /*[*/
    /* Create a thread to read data from stdin. */
    inthread.enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    inthread.done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    inthread.thread = CreateThread(NULL, 0, inthread_read, NULL, 0, NULL);
    if (inthread.thread == NULL) {
	win32_perror("CreateThread failed");
	exit(1);
    }
    c3270_input_id = AddInput(inthread.done_event, c3270_input);
#endif /*]*/

    idle_init();
    keymap_init();
    hostfile_init();
    if (!cookiefile_init()) {
        exit(1);
    }
    httpd_objects_init();

    if (appres.httpd_port) {
	struct sockaddr *sa;
	socklen_t sa_len;

	if (!parse_bind_opt(appres.httpd_port, &sa, &sa_len)) {
	    xs_warning("Invalid -httpd port \"%s\"", appres.httpd_port);
	} else {
	    hio_init(sa, sa_len);
	}
    }

    ft_init();

#if !defined(_WIN32) /*[*/
    /* Make sure we don't fall over any SIGPIPEs. */
    signal(SIGPIPE, SIG_IGN);

    /* Make sure we can collect child exit status. */
    signal(SIGCHLD, sigchld_handler);

    /* Handle run-time signals. */
    signal(SIGINT, common_handler);
    signal(SIGTSTP, common_handler);
#endif /*]*/
    task_cb_init_ir_state(&command_ir_state);

    /* Handle initial toggle settings. */
    initialize_toggles();

    /* Set up the peer script. */
    peer_script_init();

    /* Set up macros. */
    macros_init();

    if (cl_hostname != NULL) {
	pause_for_errors();
	/* Connect to the host. */
	if (!appres.reconnect) {
	    connect_once = true;
	}
	c3270_push_command(txAsprintf(AnConnect "(\"%s\")", cl_hostname));
	screen_resume();
    } else {
	/* Drop to the prompt. */
	if (!appres.secure) {
	    interact();
	} else {
	    /* Blank screen. */
	    pause_for_errors();
	    screen_resume();
	}
    }

    /* Process events forever. */
    while (1) {
	/* Process some events. */
	process_events(true);

	/* Update the screen. */
	if (!escaped) {
	    screen_disp(false);
	}

	/*
	 * If an error popped up while initializing the screen, force an
	 * escape.
	 */
	if (error_pending != NULL) {
	    c3270_screen_suspend();
	}

	/* Display the prompt. */
	if (escaped &&
		!prompt.displayed &&
		!command_running &&
		!PAGER_RUNNING) {
	    if (escape_single && !aux_input) {
		escape_single = false;
		screen_resume();
	    } else {
		interact();
	    }
	}
    }
}

static void
echo_mode(bool echo)
{
#if !defined(_WIN32) /*[*/
    struct termios t;

    tcgetattr(0, &t);
    if (echo) {
	t.c_lflag |= ECHO;
    } else {
	t.c_lflag &= ~ECHO;
    }
    tcsetattr(0, TCSANOW, &t);
#else /*][*/
    screen_echo_mode(echo);
#endif /*]*/
}

/* Reset the prompt. */
static void
reset_prompt(void)
{
    if (prompt.string != prompt.default_string) {
	Free(prompt.string);
    }
    prompt.string = prompt.default_string;
}

#if !defined(_WIN32) /*[*/
/* Synchronous signal handler. */
static void
synchronous_signal(iosrc_t fd, ioid_t id)
{
    unsigned char dummy;
    int nr;
    sigset_t temp_sigset, old_sigset;
    bool got_sigint, got_sigtstp;

    /* Read the signal from the pipe. */
    nr = read(signalpipe[0], &dummy, 1);
    if (nr < 0) {
	perror("signalpipe read");
	exit(1);
    }

    if (!escaped) {
	vtrace("Ignoring synchronous signals\n");
	return;
    }

    /* Collect pending signals. */
    sigemptyset(&temp_sigset);
    sigaddset(&temp_sigset, SIGTSTP);
    sigaddset(&temp_sigset, SIGINT);
    sigemptyset(&old_sigset);
    sigprocmask(SIG_BLOCK, &temp_sigset, &old_sigset);
    got_sigint = sigismember(&pending_signals, SIGINT);
    sigdelset(&pending_signals, SIGINT);
    got_sigtstp = sigismember(&pending_signals, SIGTSTP);
    sigdelset(&pending_signals, SIGTSTP);
    sigprocmask(SIG_SETMASK, &old_sigset, NULL);

    /* Handle SIGINT first. */
    if (got_sigint) {
	if (command_running) {
	    vtrace("SIGINT while running an action\n");
	    abort_script_by_cb(command_cb.shortname);
	} else if (!aux_input) {
	    vtrace("SIGINT at the normal prompt -- ignoring\n");
	} else {
	    vtrace("SIGINT with aux input -- aborting action\n");
#if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
#endif /*]*/
	    printf("\n");
	    fflush(stdout);
	    aux_input = false;
	    aux_pwinput = false;
	    echo_mode(true);
	    c3270_push_command(RESUME_INPUT "(" RESUME_INPUT_ABORT ")");
	    reset_prompt();
	    RemoveInput(c3270_input_id);
	    c3270_input_id = NULL_IOID;
	    prompt.displayed = false;
	    /* And wait for it to complete before displaying a new prompt. */
	}
    }

    /* Then handle SIGTSTP. */
    if (got_sigtstp) {
	if (command_running) {
	    /* Defer handling until command completes. */
	    vtrace("SIGTSTP while running an action -- deferring\n");
	    sigtstp_pending = true;
	} else {
	    vtrace("SIGTSTP at the %s\n", (pager.pid == 0)? "prompt": "pager");
#if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
#endif /*]*/
	    kill(getpid(), SIGSTOP);
	    /* Process stops here. The following is run when it resumes. */
	    if (!PAGER_RUNNING) {
		display_prompt();
	    }
	}
    }
}

/* Common signal handler. Writes to the pipe. */
static void
common_handler(int signum)
{
    char dummy = '\0';
    sigset_t temp_sigset, old_sigset;
    ssize_t nw;

    /* Make sure this signal handler continues to be in effect. */
    signal(signum, common_handler);

    /* Set this signal as pending, while holding off other signals. */
    sigemptyset(&temp_sigset);
    sigaddset(&temp_sigset, SIGTSTP);
    sigaddset(&temp_sigset, SIGINT);
    sigemptyset(&old_sigset);
    sigprocmask(SIG_BLOCK, &temp_sigset, &old_sigset);
    sigaddset(&pending_signals, signum);
    sigprocmask(SIG_SETMASK, &old_sigset, NULL);

    /* Write to the pipe, so we process this synchronously. */
    nw = write(signalpipe[1], &dummy, 1);
    assert(nw == 1);
}
#else /*][*/
/* Control-C handler. */
static void
windows_ctrlc(void)
{
    /* Signal the event. */
    SetEvent(sigint_event);
}

/* Synchronous SIGINT handler. */
static void
windows_sigint(iosrc_t fd, ioid_t id)
{
    if (command_running) {
	vtrace("SIGINT while running an action\n");
	abort_script_by_cb(command_cb.shortname);
    } else if (!aux_input) {
	vtrace("SIGINT at the normal prompt -- ignoring\n");
    } else {
	vtrace("SIGINT with aux input -- handled when 0-length read arrives\n");
    }
}
#endif /*]*/

#if defined(HAVE_LIBREADLINE) /*[*/
/* Readline completion handler -- a line of input has been collected. */
static void
rl_handler(char *command)
{
    readline_done = true;
    readline_command = command;
    rl_callback_handler_remove();
}

#endif /*]*/

/* Display the prompt. */
static void
display_prompt(void)
{
    if (error_pending != NULL) {
#if !defined(_WIN32) /*[*/
	if (color_prompt) {
	    printf("%s", screen_setaf(ACOLOR_RED));
	}
#else /*][*/
	screen_color(PC_ERROR);
#endif /*]*/
	printf("%s", error_pending);
#if !defined(_WIN32) /*[*/
	if (color_prompt) {
	    printf("%s", screen_op());
	}
#else /*][*/
	screen_color(PC_DEFAULT);
#endif /*]*/
	printf("\n");
	fflush(stdout);
	Replace(error_pending, NULL);
    }
    if (exit_pending) {
#if defined(_WIN32) /*[*/
	pause_for_errors();
#endif /*]*/
	x3270_exit(0);
    }
    if (!appres.secure && !aux_input) {
	if (ft_state != FT_NONE) {
#if !defined(_WIN32) /*[*/
	    if (color_prompt) {
		printf("%s", screen_setaf(ACOLOR_YELLOW));
	    }
#else /*][*/
	    screen_color(PC_PROMPT);
#endif /*]*/
	    printf("File transfer in progress. Use Transfer(Cancel) to "
		    "cancel.");
#if !defined(_WIN32) /*[*/
	    if (color_prompt) {
		printf("%s", screen_op());
	    }
#else /*][*/
	    screen_color(PC_DEFAULT);
#endif /*]*/
	    printf("\n");
	    fflush(stdout);
	}
	if (PCONNECTED) {
#if !defined(_WIN32) /*[*/
	    if (color_prompt) {
		printf("%s", screen_setaf(ACOLOR_BLUE));
	    }
#else /*][*/
	    screen_color(PC_PROMPT);
#endif /*]*/
	    printf("Press <Enter> to resume session.");
#if !defined(_WIN32) /*[*/
	    if (color_prompt) {
		printf("%s", screen_op());
	    }
#else /*][*/
	    screen_color(PC_DEFAULT);
#endif /*]*/
	    printf("\n");
	    fflush(stdout);
	}
    }

#if defined(HAVE_LIBREADLINE) /*[*/
    rl_callback_handler_install(prompt.string, &rl_handler);
#else /*][*/
# if defined(_WIN32) /*[*/
    screen_color(PC_PROMPT);
# endif
    fputs(prompt.string, stdout);
    fflush(stdout);
# if defined(_WIN32) /*[*/
    screen_color(PC_DEFAULT);
# endif
#endif /*]*/
#if !defined(_WIN32) /*[*/
    signal(SIGTSTP, common_handler);
    signal(SIGINT, common_handler);
#endif /*]*/
    prompt.displayed = true;
}

#if defined(_WIN32) /*[*/
static void
enable_input(enum imode mode)
{
    vtrace("enable_input(%s)\n", (mode == LINE)? "LINE": "KEY");
    inthread.mode = mode;
    SetEvent(inthread.enable_event);
}
#endif /*]*/

/* We've got console input. */
static void
c3270_input(iosrc_t fd, ioid_t id)
{
#if !defined(HAVE_LIBREADLINE) && !defined(_WIN32) /*[*/
    char inbuf[1024];
#endif /*]*/
    char *command;
    char *s;
    size_t sl;

#if defined(_WIN32) /*[*/
    if (pager.residual != NULL) {
	pager_key_done();
	return;
    }
#endif /*]*/

    /* Get the input. */
#if !defined(_WIN32) /*[*/
# if defined(HAVE_LIBREADLINE) /*[*/
    rl_callback_read_char();
    if (!readline_done) {
	/* More to collect. */
	return;
    }
    command = readline_command;
    readline_done = false;
# else /*][*/
    command = fgets(inbuf, sizeof(inbuf), stdin);
# endif /*]*/
    RemoveInput(c3270_input_id);
    c3270_input_id = NULL_IOID;
#else /*][*/
    if (inthread.nr < 0) {
	vtrace("c3270_input: input failed\n");
	enable_input(LINE);
    }
    vtrace("c3270_input: got %d bytes\n", inthread.nr);
    command = inthread.buf;
#endif /*]*/

    prompt.displayed = false;
    if (command == NULL) {
	/* EOF */
	echo_mode(true);
	printf("\n");
	fflush(stdout);
	exit(0);
    }

#if defined(_WIN32) /*[*/
    if (!command[0]) {
	/*
	 * Windows returns 0 bytes to read(0) in two cases: ^Z+<Return> and
	 * ^C. Though the documentation says otherwise, there is no way to
	 * distinguish them. So we simply re-post the read.
	 */
	if (aux_input) {
	    /* Abort the input. */
	    vtrace("Aborting auxiliary input\n");
	    aux_input = false;
	    aux_pwinput = false;
	    echo_mode(true);
	    c3270_push_command(RESUME_INPUT "(" RESUME_INPUT_ABORT ")");
	    reset_prompt();
	} else {
	    /* Get more input. */
	    printf("\n");
	    fflush(stdout);
	}
	goto done;
    }
#endif /*]*/

    s = command;
    if (!aux_input) {
	/* Strip all leading and trailing white space. */
	while (isspace((unsigned char)*s)) {
	    s++;
	}
	sl = strlen(s);
	while (sl && isspace((unsigned char)s[sl - 1])) {
	    s[--sl] = '\0';
	}
    } else {
	/* Strip any trailing newline. */
	sl = strlen(s);
	if (sl && s[sl - 1] == '\n') {
	    s[sl - 1] = '\0';
	}
    }

    /* A null command while connected means resume the session. */
    if (!aux_input && (!sl || appres.secure)) {
	if (PCONNECTED || appres.secure) {
	    /* Stop interacting. */
	    screen_resume();
	}
	goto done;
    }

#if defined(HAVE_LIBREADLINE) /*[*/
    /* Save this command in the history buffer. */
    if (!aux_input) {
	add_history(s);
    }
#endif /*]*/

    /* "?" is an alias for "Help". */
    if (!aux_input && !strcmp(s, "?")) {
	s = "Help()";
    }

    /* Run the command. */
    if (aux_input) {
	if (aux_pwinput) {
	    printf("\n");
	}
	fflush(stdout);
	aux_input = false;
	aux_pwinput = false;
	echo_mode(true);
	c3270_push_command(txAsprintf(RESUME_INPUT "(%s)",
		    *s? txdFree(base64_encode(s)): "\"\""));
	reset_prompt();
    } else {
	c3270_push_command(s);
    }

done:
#if defined(HAVE_LIBREADLINE) /*[*/
    Free(command);
#endif /*]*/
    return;
}

#if defined(_WIN32) /*[*/
/*
 * stdin input thread
 *
 * Endlessly:
 * - waits for inthread.enable_event
 * - reads from stdin
 * - leaves the input in inthread.buf and the length in inthread.nr
 * - sets inthread.done_event
 *
 * If inthread.mode is KEY, waits for a 'q' or other key instead of a
 * line of input, and leaves the result in inthread.buf.
 * If there is a read error, leaves -1 in inthread.nr and a Windows error code
 * in inthread.error.
 */
static DWORD WINAPI
inthread_read(LPVOID lpParameter)
{
    for (;;) {
	DWORD rv;

	rv = WaitForSingleObject(inthread.enable_event, INFINITE);
	switch (rv) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
	case WAIT_FAILED:
	    inthread.nr = -1;
	    inthread.error = GetLastError();
	    SetEvent(inthread.done_event);
	    break;
	case WAIT_OBJECT_0:
	    if (inthread.mode == KEY) {
		if (screen_wait_for_key(NULL)) {
		    inthread.buf[0] = 'q';
		    inthread.nr = 1;
		} else {
		    inthread.nr = 0;
		}
		inthread.buf[inthread.nr] = '\0';
	    } else {
		inthread.nr = read(0, inthread.buf, sizeof(inthread.buf) - 1);
		if (inthread.nr <= 0) {
		    inthread.error = GetLastError();
		    inthread.buf[0] = '\0';
		} else {
		    inthread.buf[inthread.nr] = '\0';
		}
	    }
	    SetEvent(inthread.done_event);
	    break;
	}
    }
    return 0;
}
#endif /*]*/

static void
interact(void)
{
    if (!prompt.displayed) {
	/* Display the prompt. */
	display_prompt();

	/* Wait for input. */
#if !defined(_WIN32) /*[*/
	assert(c3270_input_id == NULL_IOID);
	c3270_input_id = AddInput(0, c3270_input);
#else /*][*/
	enable_input(LINE);
#endif /*]*/
    }
}

#if !defined(_WIN32) /*[*/
static void
pager_exit(ioid_t id, int status)
{
    vtrace("pager exited with status %d\n", status);

    if (pager.pid == 0) {
	sigtstp_pending = false;
	return;
    }

    pager.pid = 0;
    if (command_output || !CONNECTED) {
	/* Command produced output, or we are not connected any more. */

	/* Process a pending stop. */
	if (sigtstp_pending) {
	    vtrace("Processing deferred SIGTSTP on pager exit\n");
	    sigtstp_pending = false;
#if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
#endif /*]*/
	    kill(getpid(), SIGSTOP);
	}
    } else {
	/* Exit interactive mode. */
	screen_resume();
    }

    sigtstp_pending = false;
}
#endif /*]*/

/* A command is about to produce output.  Start the pager. */
FILE *
start_pager(void)
{
#if !defined(_WIN32) /*[*/
    static char *lesspath = LESSPATH;
    static char *lesscmd = LESSPATH " -EXR";
    static char *morepath = MOREPATH;
    static char *or_cat = " || cat";
    char *pager_env;
    char *pager_cmd = NULL;

    if (pager.fp != NULL) {
	return pager.fp;
    }

    if ((pager_env = getenv("PAGER")) != NULL) {
	pager_cmd = pager_env;
    } else if (strlen(lesspath)) {
	pager_cmd = lesscmd;
    } else if (strlen(morepath)) {
	pager_cmd = morepath;
    }
    if (pager_cmd != NULL && strcmp(pager_cmd, "none")) {
	char *s;

	s = Malloc(strlen(pager_cmd) + strlen(or_cat) + 1);
	sprintf(s, "%s%s", pager_cmd, or_cat);
	if (appres.secure) {
	    putenv("LESSSECURE=1");
	}
	pager.fp = xpopen(s, "w", &pager.pid);
	Free(s);
	if (pager.fp == NULL) {
	    perror(pager_cmd);
	} else {
	    AddChild(pager.pid, pager_exit);
	}
    }
    if (pager.fp == NULL) {
	pager.fp = stdout;
    }
    return pager.fp;
#else /*][*/
    if (!pager.running) {
	pager.rowcnt = 0;
	Replace(pager.residual, NULL);
	pager.flushing = false;
	pager.running = true;
	get_console_size(&pager.rows, &pager.cols);
    }
    return stdout;
#endif /*]*/
}

/* Stop the pager. */
static void
stop_pager(void)
{
    vtrace("stop pager\n");
#if !defined(_WIN32) /*[*/
    if (pager.fp != NULL) {
	if (pager.fp != stdout) {
	    xpclose(pager.fp, XPC_NOWAIT);
	}
	pager.fp = NULL;
    }
#else /*][*/
    pager.rowcnt = 0;
    Replace(pager.residual, NULL);
    pager.flushing = false;
    pager.running = false;
#endif /*]*/
}

#if defined(_WIN32) /*[*/
/* A key was pressed while the pager prompt was up. */
static void
pager_key_done(void)
{
    char *p;

    pager.flushing = inthread.buf[0] == 'q';
    vtrace("Got pager key%s\n", pager.flushing? " (quit)": "");

    /* Overwrite the prompt and reset. */
    printf("\r%*s\r", (pager.nw > 0)? pager.nw: 79, "");
    fflush(stdout);
    pager.rowcnt = 0;
    get_console_size(&pager.rows, &pager.cols);

    if (pager.flushing && !command_running) {
	/* Pressed 'q' and the command is complete. */
	stop_pager();
	return;
    }

    /* Dump what's remaining, which might leave more output pager.residual. */
    p = pager.residual;
    pager.residual = NULL;
    pager_output(p, true);
    Free(p);
}

/* Write a line of output to the pager. */
void
pager_output(const char *s, bool success)
{
    if (pager.flushing) {
	/* They don't want to see any more. */
	return;
    }

    if (pager.residual != NULL) {
	/* Output is pending already. */
	vtrace("pager accumulate\n");
	pager.residual = Realloc(pager.residual,
		strlen(pager.residual) + strlen(s) + 2);
	strcat(strcat(pager.residual, "\n"), s);
	return;
    }

    do {
	char *nl;
	size_t sl;

	/* Pause for a screenful. */
	if (pager.rowcnt >= (pager.rows - 1)) {
	    vtrace("pager pausing\n");
	    Replace(pager.residual, NewString(s));
	    screen_color(PC_PROMPT);
	    pager.nw = printf(PAGER_PROMPT);
	    fflush(stdout);
	    screen_color(PC_DEFAULT);
	    enable_input(KEY);
	    return;
	}

	/*
	 * Look for an embedded newline.  If one is found, just print
	 * up to it, so we can count the newline and possibly pause
	 * partway through the string.
	 */
	screen_color(success? PC_NORMAL: PC_ERROR);
	nl = strchr(s, '\n');
	if (nl != NULL) {
	    sl = nl - s;
	    printf("%.*s\n", (int)sl, s);
	    s = nl + 1;
	} else {
	    printf("%s\n", s);
	    sl = strlen(s);
	    s = NULL;
	}
	fflush(stdout);
	screen_color(PC_DEFAULT);

	/* Account for the newline. */
	pager.rowcnt++;

	/* Account (conservatively) for any line wrap. */
	pager.rowcnt += (int)(sl / pager.cols);

    } while (s != NULL);
}
#endif /*]*/

#if defined(HAVE_LIBREADLINE) /*[*/

static char **matches = NULL;
static char **next_match;

/* Generate a match list. */
static char **
attempted_completion(const char *text, int start, int end)
{
    char *s;
    unsigned i, j;
    int match_count;
    action_elt_t *e;

    /* If this is not the first word, fail. */
    s = rl_line_buffer;
    while (*s && isspace((int)*s)) {
	s++;
    }
    if (s - rl_line_buffer < start) {
	char *t = s;
	struct host *h;

	/*
	 * If the first word is 'Connect' or 'Open', and the
	 * completion is on the second word, expand from the
	 * hostname list.
	 */

	/* See if we're in the second word. */
	while (*t && !isspace((int)*t)) {
	    t++;
	}
	while (*t && isspace((int)*t)) {
	    t++;
	}
	if (t - rl_line_buffer < start) {
	    return NULL;
	}

	/*
	 * See if the first word is 'Open' or 'Connect'.  In future,
	 * we might do other expansions, and this code would need to
	 * be generalized.
	 */
	if (!((!strncasecmp(s, AnOpen, 4) && isspace((int)*(s + 4))) ||
	      (!strncasecmp(s, AnConnect, 7) && isspace((int)*(s + 7))))) {
	    return NULL;
	}

	/* Look for any matches.  Note that these are case-sensitive. */
	for (h = hosts, match_count = 0; h; h = h->next) {
	    if (!strncmp(h->name, t, strlen(t))) {
		match_count++;
	    }
	}
	if (!match_count) {
	    return NULL;
	}

	/* Allocate the return array. */
	next_match = matches = Malloc((match_count + 1) * sizeof(char **));

	/* Scan again for matches to fill in the array. */
	for (h = hosts, j = 0; h; h = h->next) {
	    int skip = 0;

	    if (strncmp(h->name, t, strlen(t))) {
		continue;
	    }

	    /*
	     * Skip hostsfile entries that are duplicates of
	     * RECENT entries we've already recorded.
	     */
	    if (h->entry_type != RECENT) {
		for (i = 0; i < j; i++) {
		    if (!strcmp(matches[i], h->name)) {
			skip = 1;
			break;
		    }
		}
	    }
	    if (skip) {
		continue;
	    }

	    /*
	     * If the string contains spaces, put it in double
	     * quotes.  Otherwise, just copy it.  (Yes, this code
	     * is fairly stupid, and can be fooled by other
	     * whitespace and embedded double quotes.)
	     */
	    if (strchr(h->name, ' ') != NULL) {
		matches[j] = Malloc(strlen(h->name) + 3);
		sprintf(matches[j], "\"%s\"", h->name);
		j++;
	    } else {
		matches[j++] = NewString(h->name);
	    }
	}
	matches[j] = NULL;
	return NULL;
    }

    /* Search for matches. */
    match_count = 0;
    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (e->t.flags & ACTION_HIDDEN) {
	    continue;
	}
	if (!strncasecmp(e->t.name, s, strlen(s))) {
	    match_count++;
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    if (!match_count) {
	return NULL;
    }

    /* Return what we got. */
    next_match = matches = Malloc((match_count + 1) * sizeof(char **));
    j = 0;
    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	if (e->t.flags & ACTION_HIDDEN) {
	    continue;
	}
	if (!strncasecmp(e->t.name, s, strlen(s))) {
	    matches[j++] = NewString(e->t.name);
	}
    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
    matches[j] = NULL;
    return NULL;
}

/* Return the match list. */
static char *
completion_entry(const char *text, int state)
{
    char *r;

    if (next_match == NULL) {
	return NULL;
    }

    if ((r = *next_match++) == NULL) {
	Free(matches);
	next_match = matches = NULL;
	return NULL;
    } else {
	return r;
    }
}

#endif /*]*/

/* c3270-specific actions. */

/* Break to the command prompt. */
static bool
Escape_action(ia_t ia, unsigned argc, const char **argv)
{
    bool no_prompt_after = false;

    action_debug(AnEscape, ia, argc, argv);
    if (check_argc(AnEscape, argc, 0, 2) < 0) {
	return false;
    }

    if (argc > 0 && !strcasecmp(argv[0], KwDashNoPromptAfter)) {
	no_prompt_after = true;
	argc--;
	argv++;
	if (argc == 0) {
	    popup_an_error(AnEscape "(): Must specify an action with "
		    KwDashNoPromptAfter);
	    return false;
	}
    }
    if (argc > 1) {
	popup_an_error(AnEscape "(): Too many arguments, or unrecognized "
		"option");
	return false;
    }

    if (escaped && argc > 0) {
	popup_an_error("Cannot nest " AnEscape "()");
	return false;
    }

    if (!escaped) {
	if (appres.secure && argc == 0) {
	    /* Plain Escape() does nothing when secure. */
	    return true;
	}
	c3270_screen_suspend();
	if (argc > 0) {
	    escape_single = no_prompt_after;
	    c3270_push_command(argv[0]);
	}
    }
    return true;
}

/* Popup an informational message. */
void
popup_an_info(const char *fmt, ...)
{
    va_list args;
    static char vmsgbuf[4096];
    size_t sl;

    /* Expand it. */
    va_start(args, fmt);
    vsprintf(vmsgbuf, fmt, args);
    va_end(args);

    /* Remove trailing newlines. */
    sl = strlen(vmsgbuf);
    while (sl && vmsgbuf[sl - 1] == '\n') {
	vmsgbuf[--sl] = '\0';
    }

    /* Push it out. */
    if (sl) {
	char *s;

	while ((s = strchr(vmsgbuf, '\n')) != NULL) {
	    *s = ' ';
	}
	status_push(vmsgbuf);
    }
}

/**
 * Callback for data returned to action.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
command_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    if (handle != (tcb_t *)&command_cb) {
	vtrace("command_data: no match\n");
	return;
    }

    glue_gui_xoutput(txAsprintf("%.*s", (int)len, buf), success);
}

/**
 * Callback for input request.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] echo	True if input should be echoed
 */
static void
command_reqinput(task_cbh handle, const char *buf, size_t len, bool echo)
{
    char *p;

    if (handle != (tcb_t *)&command_cb) {
	vtrace("command_reqinput: no match\n");
	return;
    }

    p = txdFree(base64_decode(buf));

    if (prompt.string != prompt.default_string) {
	Free(prompt.string);
    }
    prompt.string =
	color_prompt?
	    Asprintf("%s%s%s", PROMPT_PRE, p, PROMPT_POST):
	    NewString(p);
    aux_input = true;
    aux_pwinput = !echo;
    command_output = true; /* a white lie */
    echo_mode(echo);
}

/**
 * Callback for completion of one command executed from the prompt.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if context is complete
 */
static bool
command_done(task_cbh handle, bool success, bool abort)
{
    if (handle != (tcb_t *)&command_cb) {
	vtrace("command_data: no match\n");
	return true;
    }

    vtrace("command complete\n");

    command_running = false;

#if defined(_WIN32) /*[*/
    if (PAGER_RUNNING) {
	/* Pager is paused, don't do anything yet. */
	return true;
    }
#endif /*]*/

    /* Send EOF to the pager. */
    stop_pager();

#if !defined(_WIN32) /*[*/
    if (PAGER_RUNNING) {
	/* Pager process is still running, don't do anything else yet. */
	return true;
    }
#endif /*]*/

    /* The command from the prompt completed. */
    if (command_output || !PCONNECTED) {
	/* Command produced output, or we are not connected any more. */

#if !defined(_WIN32) /*[*/
	/* Process a pending stop. */
	if (sigtstp_pending) {
	    vtrace("Processing deferred SIGTSTP on command completion\n");
	    sigtstp_pending = false;
# if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
# endif /*]*/
	    kill(getpid(), SIGSTOP);
# if !defined(_WIN32) /*[*/
	    if (pager.pid != 0) {
		return true;
	    }
# endif /*]*/
	}
#endif /*]*/
    } else {
	/* Exit interactive mode. */
	screen_resume();
#if defined(_WIN32) /*[*/
	signal(SIGINT, SIG_DFL);
#endif /*]*/
    }

#if !defined(_WIN32) /*[*/
    sigtstp_pending = false;
#endif /*]*/
    return true;
}

static unsigned
command_getflags(task_cbh handle)
{
    /*
     * INTERACTIVE: We understand [input] responses.
     * CONNECT_NONBLOCK: We do not want Connect()/Open() to block.
     * PWINPUT: We understand [pwinput] responses.
     */
    return CBF_INTERACTIVE | CBF_CONNECT_FT_NONBLOCK | CBF_PWINPUT;
}

/**
 * Set the pending input request.
 *
 * @param[in] handle	Context
 * @param[in] irhandle	Input request handle
 */
static void
command_setir(task_cbh handle, void *irhandle)
{
    command_irhandle = irhandle;
}

/**
 * Get the pending input request.
 *
 * @param[in] handle	 Context
 *
 * @returns input request handle
 */
static void *
command_getir(task_cbh handle)
{
    return command_irhandle;
}

/**
 * Set input request state.
 *
 * @param[in] handle	CB handle
 * @param[in] name	Input request type name
 * @param[in] state	State to store
 * @param[in] abort	Abort callback
 */
static void
command_setir_state(task_cbh handle, const char *name, void *state,
	ir_state_abort_cb abort)
{
    task_cb_set_ir_state(&command_ir_state, name, state, abort);
}

/**
 * Get input request state.
 *
 * @param[in] handle    CB handle
 * @param[in] name      Input request type name
 */
static void *
command_getir_state(task_cbh handle, const char *name)
{
    return task_cb_get_ir_state(&command_ir_state, name);
}

/**
 * Push a command.
 *
 * @param[in] s		Text of action.
 */
static void
c3270_push_command(const char *s)
{
    command_running = true;
    command_output = false;

#if defined(_WIN32) /*[*/
    stop_pager();
    get_console_size(&pager.rows, &pager.cols);
#endif /*]*/

    /* Push a callback with a macro. */
    push_cb(s, strlen(s), &command_cb, (task_cbh)&command_cb);
}

#if !defined(_WIN32) /*[*/

/* Support for c3270 profiles. */

#define PROFILE_ENV	"C3270PRO"
#define NO_PROFILE_ENV	"NOC3270PRO"
#define DEFAULT_PROFILE	"~/.c3270pro"

/* Read in the .c3270pro file. */
static bool
merge_profile(void)
{
    const char *fname;
    char *profile_name;
    bool did_read = false;

    /* Check for the no-profile environment variable. */
    if (getenv(NO_PROFILE_ENV) != NULL) {
	return did_read;
    }

    /* Read the file. */
    fname = getenv(PROFILE_ENV);
    if (fname == NULL || *fname == '\0') {
	fname = DEFAULT_PROFILE;
    }
    profile_name = do_subst(fname, DS_VARS | DS_TILDE);
    did_read = read_resource_file(profile_name, false);
    Free(profile_name);
    return did_read;
}

#endif /*]*/

#if defined(_WIN32) /*[*/
/* Start a auto-shortcut-mode copy of wc3270.exe. */
static void
start_auto_shortcut(int argc, char *argv[])
{
    char *tempdir;
    FILE *f;
    session_t s;
    HRESULT hres;
    char exepath[MAX_PATH];
    char linkpath[MAX_PATH];
    char sesspath[MAX_PATH];
    char delenv[32 + MAX_PATH];
    HINSTANCE h;
    char *cwd;
    varbuf_t r;
    int i;

    /* Make sure there is a session file. */
    if (profile_path == NULL) {
	fprintf(stderr, "Can't use auto-shortcut mode without a "
		    "session file\n");
	fflush(stderr);
	return;
    }

#if defined(AS_DEBUG) /*[*/
    printf("Running auto-shortcut, profile path is %s\n", profile_path);
    fflush(stdout);
#endif /*]*/

    /* Read the session file into 's'. */
    f = fopen(profile_path, "r");
    if (f == NULL) {
	fprintf(stderr, "%s: %s\n", profile_path, strerror(errno));
	fflush(stderr);
	x3270_exit(1);
    }
    memset(&s, '\0', sizeof(session_t));
    if (read_session(f, &s, NULL) == 0) {
	fprintf(stderr, "%s: invalid format\n", profile_path);
	fflush(stderr);
	x3270_exit(1);
    }
#if defined(AS_DEBUG) /*[*/
    printf("Reading session file '%s'\n", profile_path);
    fflush(stdout);
#endif /*]*/

    /* Create the shortcut. */
    tempdir = getenv("TEMP");
    if (tempdir == NULL) {
	fprintf(stderr, "No %%TEMP%%?\n");
	fflush(stderr);
	x3270_exit(1);
    }
    sprintf(linkpath, "%s\\wcsa%u.lnk", tempdir, getpid());
    sprintf(exepath, "%s%s", instdir, "wc3270.exe");
#if defined(AS_DEBUG) /*[*/
    printf("Executable path is '%s'\n", exepath);
    fflush(stdout);
#endif /*]*/
    if (GetFullPathName(profile_path, MAX_PATH, sesspath, NULL) == 0) {
	fprintf(stderr, "%s: Error %ld\n", profile_path, GetLastError());
	fflush(stderr);
	x3270_exit(1);
    }

    /*
     * Copy the command-line arguments. Surround each argument with double
     * quotes, and hope that is sufficient.
     */
    vb_init(&r);
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], OptAutoShortcut) && strcmp(argv[i], profile_path)) {
	    vb_appendf(&r, "\"%s\" ", argv[i]);
	}
    }
    vb_appendf(&r, OptNoAutoShortcut " \"%s\"", sesspath);
#if defined(AS_DEBUG) /*[*/
    printf("args are '%s'\n", vb_buf(&r));
    fflush(stdout);
#endif /*]*/

    cwd = getcwd(NULL, 0);
    hres = create_shortcut(&s,		/* session */
			   exepath,	/* .exe    */
			   linkpath,	/* .lnk    */
			   (char *)vb_buf(&r),	/* args    */
			   cwd		/* cwd     */);
    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "Cannot create ShellLink '%s'\n", linkpath);
	fflush(stderr);
	x3270_exit(1);
    }
    vb_free(&r);
#if defined(AS_DEBUG) /*[*/
    printf("Created ShellLink '%s'\n", linkpath);
    fflush(stdout);
#endif /*]*/

    /* Execute it. */
    sprintf(delenv, "%s=%s", DELENV, linkpath);
    putenv(delenv);
    h = ShellExecute(NULL, "open", linkpath, "", tempdir, SW_SHOW);
    if ((uintptr_t)h <= 32) {
	fprintf(stderr, "ShellExecute failed, error %d\n", (int)(uintptr_t)h);
	fflush(stderr);
	x3270_exit(1);
    }

#if defined(AS_DEBUG) /*[*/
    printf("Started ShellLink\n");
    fflush(stdout);
#endif /*]*/

    exit(0);
}

/* Start a copy of the Session Wizard. */
void
start_wizard(const char *session)
{
    char *cmd;

    if (session != NULL) {
	cmd = Asprintf("start \"wc3270 Session Wizard\" \"%swc3270wiz.exe\" "
		"-e \"%s\"", instdir, session);
    } else {
	cmd = Asprintf("start \"wc3270 Session Wizard\" \"%swc3270wiz.exe\"",
		instdir);
    }
    system(cmd);
    Free(cmd);

    /* Get back mouse events */
    screen_system_fixup();
}

#endif /*]*/

/* Start a browser window to display c3270/wc3270 help. */
void
start_html_help(void)
{
#if defined(HAVE_START) /*[*/
    start_help();
#endif /*]*/

#if defined(_WIN32) /*[*/
    /* Get back mouse events */
    screen_system_fixup();
#endif /*]*/
}

/*
 * Product information functions.
 */
bool
product_has_display(void)
{   
    return true;
}

/**
 * Build options.
 *
 * @return Product-specific build options string, beginning with a space.
 */
const char *
product_specific_build_options(void)
{
    return
#if defined(HAVE_LIBREADLINE) /*[*/
	    " --with-readline"
#else /*][*/
	    " --without-readline"
#endif /*]*/
#if !defined(_WIN32) /*[*/
# if defined(CURSES_WIDE) /*[*/
	    " --with-curses-wide"
# else /*][*/
	    " --without-curses-wide"
# endif /*]*/
#endif /*]*/
	    ;
}

bool
product_auto_oversize(void)
{
    return true;
}

/**
 * Set appres defaults that are specific to this product.
 */
void
product_set_appres_defaults(void)
{
    appres.oerr_lock = true;
    appres.interactive.compose_map = "latin1";
    appres.interactive.do_confirms = true;
    appres.interactive.menubar = true;
    appres.interactive.save_lines = 4096;
#if defined(_WIN32) /*[*/
    appres.trace_monitor = true;
    set_toggle(UNDERSCORE, true);
#else /*][*/
    appres.c3270.meta_escape = NewString(KwAuto);
    appres.c3270.curses_keypad = true;
    appres.c3270.mouse = true;
#endif /*]*/

#if !defined(_WIN32) /*[*/
# if defined(CURSES_WIDE) /*[*/
    appres.c3270.acs = true;
# else /*][*/
    appres.c3270.ascii_box_draw = true;
# endif /*]*/
#endif /*]*/

    set_toggle(SELECT_URL, true);
}

/*
 * Telnet GUI.
 */
void
telnet_gui_connecting(const char *hostname, const char *portname)
{
    popup_an_info("Trying %s, port %s...", hostname, portname);
}

/**
 * GUI redirect function for action_output.
 */
static bool
glue_gui_xoutput(const char *s, bool success)
{
    c3270_screen_suspend();

    if (*s) {
#if !defined(_WIN32) /*[*/
	if (color_prompt) {
	    fprintf(start_pager(), "%s%s%s\n",
		    success? "": screen_setaf(ACOLOR_RED),
		    s,
		    success? "": screen_op());
	} else {
	    fprintf(start_pager(), "%s\n", s);
	}
	fflush(start_pager());
#else /*][*/
	start_pager();
	pager_output(s, success);
#endif /*]*/
    }
    command_output = true;
    /* any_error_output = true; */ /* XXX: Needed? */
    return true;
}

/**
 * GUI redirect function for action_output.
 */
bool
glue_gui_output(const char *s)
{
    return glue_gui_xoutput(s, true);
}

/**
 * GUI redirect function for popup_an_error.
 * This handles asynchronous errors, such as file transfers that do not start
 * or abort.
 *
 * @param[in] type	Error type
 * @param[in] s		Text to display
 * @returns true (the error has been handled)
 */
bool
glue_gui_error(pae_t type, const char *s)
{
    if (type == ET_CONNECT && host_retry_mode) {
	return true;
    }
    return glue_gui_error_cond(s, true);
}

/**
 * Determine if it is safe to process an Open()/Connect().
 *
 * @returns true if safe.
 */
bool
glue_gui_open_safe(void)
{
    return screen_initted || task_running_cb_contains(&command_cb);
}

/**
 * Determine if it is valid to run an interactive Script() action.
 *
 * @returns true.
 */
bool
glue_gui_script_interactive(void)
{
    return true;
}

/**
 * c3270 main module registration.
 */
static void
c3270_register(void)
{
    static action_table_t actions[] = {
	{ AnEscape,		Escape_action,		ACTION_KE }
    };
    static opt_t c3270_opts[] = {
	{ OptAllBold,  OPT_BOOLEAN, true,  ResAllBold,
	    aoffset(c3270.all_bold_on),
	    NULL, "Display all text in bold" },
	{ OptKeymap,   OPT_STRING,  false, ResKeymap,
	    aoffset(interactive.key_map),
	    "<name>[,<name>...]", "Keyboard map name(s)" },
	{ OptNoPrompt, OPT_BOOLEAN, true,  ResNoPrompt,
	    aoffset(secure),
	    NULL, "Alias for -secure" },
	{ OptReconnect,OPT_BOOLEAN, true,  ResReconnect,
	    aoffset(reconnect),
	    NULL, "Reconnect to host as soon as it disconnects" },
	{ OptSaveLines, OPT_INT,    false, ResSaveLines,
	    aoffset(interactive.save_lines),
	    "<lines>", "Number of lines to save for scrolling" },
	{ OptSecure,   OPT_BOOLEAN, true,  ResSecure,
	    aoffset(secure),
	    NULL, "Restrict potentially-destructive user actions" },
	{ OptUtf8,     OPT_BOOLEAN, true,  ResUtf8,      aoffset(utf8),
	    NULL, "Force local codeset to be UTF-8" },
#if defined(C3270_80_132) /*[*/
	{ OptAltScreen,OPT_STRING,  false, ResAltScreen,
	    aoffset(c3270.altscreen),
	    "<string>",
	    "String to switch terminal from 80-column mode to 132-column mode"
	},
	{ OptDefScreen,OPT_STRING,  false, ResDefScreen,
	    aoffset(c3270.defscreen),
	    "<string>",
	    "String to switch terminal from 132-column mode to 80-column mode"
	},
#endif /*]*/
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
	{ OptDefaultFgBg,OPT_BOOLEAN,true, ResDefaultFgBg,
	    aoffset(c3270.default_fgbg),
	    NULL,
	    "Use terminal's default foreground and background colors"
	},
#endif /*]*/
#if !defined(_WIN32) /*[*/
	{ OptCbreak,   OPT_BOOLEAN, true,  ResCbreak,
	    aoffset(c3270.cbreak_mode),
	    NULL, "Force terminal CBREAK mode" },
	{ OptMono,     OPT_BOOLEAN, true,  ResMono,
	    aoffset(interactive.mono),
	    NULL, "Do not use terminal color capabilities" },
	{ OptReverseVideo,OPT_BOOLEAN,true,ResReverseVideo,
	    aoffset(c3270.reverse_video),
	    NULL, "Switch to black-on-white mode" },
#endif /*]*/
#if defined(_WIN32) /*[*/
	{ OptAutoShortcut,OPT_BOOLEAN, true, ResAutoShortcut,
	    aoffset(c3270.auto_shortcut),
	    NULL, "Run in auto-shortcut mode" },
	{ OptNoAutoShortcut,OPT_BOOLEAN,false,ResAutoShortcut,
	    aoffset(c3270.auto_shortcut),
	    NULL, "Do not run in auto-shortcut mode" },
	{ OptTitle,    OPT_STRING,  false, ResTitle,
	    aoffset(c3270.title),
	    "<string>", "Set window title to <string>" },
#endif /*]*/
    };
    static res_t c3270_resources[] = {
	{ ResAllBold,	aoffset(c3270.all_bold),	XRM_STRING },
	{ ResAsciiBoxDraw,aoffset(c3270.ascii_box_draw),XRM_BOOLEAN },
	{ ResIdleCommand,aoffset(idle_command),		XRM_STRING },
	{ ResIdleCommandEnabled,aoffset(idle_command_enabled),XRM_BOOLEAN },
	{ ResIdleTimeout,aoffset(idle_timeout),		XRM_STRING },
	{ ResKeymap,	aoffset(interactive.key_map),	XRM_STRING },
	{ ResMenuBar,	aoffset(interactive.menubar),	XRM_BOOLEAN },
	{ ResNoPrompt,	aoffset(secure),		XRM_BOOLEAN },
	{ ResSaveLines,	aoffset(interactive.save_lines),XRM_INT },
#if !defined(_WIN32) /*[*/
	{ ResCbreak,	aoffset(c3270.cbreak_mode),	XRM_BOOLEAN },
	{ ResCursesKeypad,aoffset(c3270.curses_keypad),	XRM_BOOLEAN },
	{ ResMetaEscape,aoffset(c3270.meta_escape),	XRM_STRING },
	{ ResMono,	aoffset(interactive.mono),	XRM_BOOLEAN },
	{ ResMouse,	aoffset(c3270.mouse),		XRM_BOOLEAN },
	{ ResReverseVideo,aoffset(c3270.reverse_video),XRM_BOOLEAN },
#endif /*]*/
#if defined(C3270_80_132) /*[*/
	{ ResAltScreen,	aoffset(c3270.altscreen),	XRM_STRING },
	{ ResDefScreen,	aoffset(c3270.defscreen),	XRM_STRING },
#endif /*]*/
#if defined(CURSES_WIDE) /*[*/
	{ ResAcs,	aoffset(c3270.acs),		XRM_BOOLEAN },
#endif /*]*/
#if defined(HAVE_USE_DEFAULT_COLORS) /*[*/
	{ ResDefaultFgBg,aoffset(c3270.default_fgbg),	XRM_BOOLEAN },
# endif /*]*/
#if defined(_WIN32) /*[*/
	{ ResAutoShortcut,aoffset(c3270.auto_shortcut),	XRM_BOOLEAN },
	{ ResBellMode,	aoffset(c3270.bell_mode),	XRM_STRING },
	{ ResLightPenPrimary,aoffset(c3270.lightpen_primary),XRM_BOOLEAN },
	{ ResTitle,	aoffset(c3270.title),		XRM_STRING },
	{ ResVisualBell,aoffset(interactive.visual_bell),XRM_BOOLEAN },
#endif /*]*/
    };
    static xres_t c3270_xresources[] = {
	{ ResKeymap,			V_WILD },
	{ ResPrintTextScreensPerPage,	V_FLAT },
	{ ResMessage,			V_WILD },
#if defined(_WIN32) /*[*/
	{ ResPrintTextFont, 		V_FLAT },
	{ ResPrintTextHorizontalMargin,	V_FLAT },
	{ ResPrintTextOrientation,	V_FLAT },
	{ ResPrintTextSize, 		V_FLAT },
	{ ResPrintTextVerticalMargin,	V_FLAT },
	{ ResHostColorForDefault,	V_FLAT },
	{ ResHostColorForIntensified,	V_FLAT },
	{ ResHostColorForProtected,	V_FLAT },
	{ ResHostColorForProtectedIntensified,V_FLAT },
	{ ResConsoleColorForHostColor,	V_COLOR },
#else /*][*/
	{ ResPrintTextCommand,		V_FLAT },
	{ ResCursesColorForDefault,	V_FLAT },
	{ ResCursesColorForIntensified,	V_FLAT },
	{ ResCursesColorForProtected,	V_FLAT },
	{ ResCursesColorForProtectedIntensified,V_FLAT },
	{ ResCursesColorForHostColor,	V_COLOR },
#endif /*]*/
    };
    static query_t queries[] = {
	{ KwKeymap, keymap_dump, NULL, false, true },
	{ KwStatus, status_dump, NULL, false, true },
	{ KwStats, status_dump, NULL, true, true }
    };

    /* Register for state changes. */
    register_schange(ST_CONNECT, c3270_connect);
    register_schange(ST_3270_MODE, c3270_3270_mode);
    register_schange(ST_EXITING, main_exiting);

    /* Register our actions. */
    register_actions(actions, array_count(actions));

    /* Register our options. */
    register_opts(c3270_opts, array_count(c3270_opts));

    /* Register our resources. */
    register_resources(c3270_resources, array_count(c3270_resources));
    register_xresources(c3270_xresources, array_count(c3270_xresources));

    /* Register our queries. */
    register_queries(queries, array_count(queries));
}
