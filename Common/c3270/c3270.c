/*
 * Copyright (c) 1993-2018 Paul Mattes.
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
#include "charset.h"
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
#include "lazya.h"
#include "linemode.h"
#include "nvt.h"
#include "opts.h"
#include "popups.h"
#include "pr3287_session.h"
#include "print_screen.h"
#include "product.h"
#include "screen.h"
#include "selectc.h"
#include "sio_glue.h"
#include "split_host.h"
#include "status.h"
#include "task.h"
#include "telnet.h"
#include "telnet_gui.h"
#include "toggles.h"
#include "trace.h"
#include "utf8.h"
#include "utils.h"
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

#define INPUT	"[input] "

#if defined(_WIN32) /*[*/
# define DELENV		"WC3DEL"
#endif /*]*/

#if defined(_WIN32) /*[*/
# define PR3287_NAME "wpr3287"
#else /*][*/
# define PR3287_NAME "pr3287"
#endif /*]*/

static void c3270_push_command(char *s);
static void interact(void);
static void stop_pager(void);
static void display_prompt(void);

#if !defined(_WIN32) /*[*/
static bool merge_profile(void);
#endif /*]*/

#if defined(HAVE_LIBREADLINE) /*[*/
static char **attempted_completion();
static char *completion_entry(const char *, int);
static char *readline_command;
static bool readline_done = false;
#endif /*]*/

/* Pager state. */
#if !defined(_WIN32) /*[*/
static FILE *pager = NULL;
static pid_t pager_pid = 0;
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
#endif /*]*/
static bool any_error_output;
static bool command_running = false;
static bool command_complete = false;
static bool command_output = false;

#if !defined(_WIN32) /*[*/
static bool stop_pending = false;
static int signalpipe[2];
static void synchronous_signal(iosrc_t fd, ioid_t id);
static void common_handler(int signum);
#endif /*]*/
static char *prompt_string = NULL;
static char *real_prompt_string = NULL;
static char *escape_action = NULL;

static bool aux_input = false;

static ioid_t c3270_input_id = NULL_IOID;
static task_cb_ir_state_t command_ir_state;

#if defined(_WIN32) /*[*/
char *instdir = NULL;
char *mydesktop = NULL;
char *mydocs3270 = NULL;
char *commondocs3270 = NULL;
unsigned windirs_flags;
static void start_auto_shortcut(void);

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
#endif /*]*/

static void c3270_register(void);

void
usage(const char *msg)
{
    if (msg != NULL) {
	fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, "Usage: %s [options] [ps:][LUname@]hostname[:port]\n",
	    programname);
    fprintf(stderr, "Options:\n");
    cmdline_help(false);
    exit(1);
}

/* Callback for 3270-mode state changes. */
static void
c3270_3270_mode(bool ignored)
{
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

/* Callback for connection state changes. */
static void
c3270_connect(bool ignored)
{
    c3270_3270_mode(true);
    if (CONNECTED) {
	/* Clear 'Trying' text. */
	status_push(NULL);
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
    } else {
	if (screen_suspend()) {
	    screen_final();
	}
    }
} 

/* Make sure error messages are seen. */
static void
pause_for_errors(void)
{
    char s[10];

    if (any_error_output) {
	screen_suspend();
	printf("[Press <Enter>] ");
	fflush(stdout);
	if (fgets(s, sizeof(s), stdin) == NULL) {
	    x3270_exit(1);
	}
	any_error_output = false;
    }
}

#if !defined(_WIN32) /*[*/
/* Empty SIGCHLD handler, ensuring that we can collect child exit status. */
static void
sigchld_handler(int ignored)
{
#if !defined(_AIX) /*[*/
    (void) signal(SIGCHLD, sigchld_handler);
#endif /*]*/
}
#endif /*]*/

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

/**
 * Redirection for Warning.
 */
static void
c3270_Warning(const char *s)
{
    fprintf(stderr, "Warning: %s\n", s);
    fflush(stderr);
    any_error_output = true;
}

/* Pause before exiting. */
static void
exit_pause(bool mode _is_unused)
{
    if (x3270_exit_code) {
	char buf[2];

	printf("\n[Press <Enter>] ");
	fflush(stdout);
	(void) fgets(buf, sizeof(buf), stdin);
    }
}
#endif /*]*/

int
main(int argc, char *argv[])
{
    const char	*cl_hostname = NULL;
    bool	 once = false;
    bool	 cl_connect_done = false;
#if defined(_WIN32) /*[*/
    char	*delenv;
#endif /*]*/

#if defined(_WIN32) /*[*/
    /* Redirect Error() so we pause. */
    Error_redirect = c3270_Error;
    Warning_redirect = c3270_Warning;

    /* Register a final exit function, so we pause. */
    register_schange_ordered(ST_EXITING, exit_pause, ORDER_LAST);

    /* Get Windows version and directories. */
    (void) get_version_info();
    if (!get_dirs("wc3270", &instdir, &mydesktop, NULL, NULL, NULL, NULL, NULL,
		&mydocs3270, &commondocs3270, &windirs_flags)) {
	x3270_exit(1);
    }

    /* Start Winsock. */
    if (sockstart()) {
	x3270_exit(1);
    }
#endif /*]*/

    real_prompt_string = prompt_string = xs_buffer("%s> ", app);

#if !defined(_WIN32) && !defined(CURSES_WIDE) /*[*/
    /* Explicitly turn off DBCS if wide curses is not supported. */
    dbcs_allowed = false;
#endif /*]*/

    /*
     * Call the module registration functions, to build up the tables of
     * actions, options and callbacks.
     */
    c3270_register();
    charset_register();
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
    menubar_register();
    nvt_register();
    pr3287_session_register();
    print_screen_register();
#if defined(_WIN32) /*[*/
    select_register();
#endif /*]*/
    screen_register();
    scroll_register();
    toggles_register();
    trace_register();
    xio_register();
    sio_glue_register();
    hio_register();

#if !defined(_WIN32) /*[*/
    register_merge_profile(merge_profile);
#endif /*]*/
    argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

    printf("%s\n\n"
	    "Copyright 1989-%s by Paul Mattes, GTRC and others.\n"
	    "Type 'show copyright' for full copyright information.\n"
	    "Type 'help' for help information.\n\n",
	    build, cyear);

#if defined(_WIN32) /*[*/
    /* Delete the link file, if we've been told do. */
    delenv = getenv(DELENV);
    if (delenv != NULL) {
	unlink(delenv);
	putenv(DELENV "=");
    }

    /* Check for auto-shortcut mode. */
    if (appres.c3270.auto_shortcut) {
	start_auto_shortcut();
	exit(0);
    }
#endif /*]*/

    if (charset_init(appres.charset) != CS_OKAY) {
	xs_warning("Cannot find charset \"%s\"", appres.charset);
	(void) charset_init(NULL);
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

#if defined(_WIN32) /*[*/
    /* Create a thread to read data from stdin. */
    inthread.enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    inthread.done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    inthread.thread = CreateThread(NULL, 0, inthread_read, NULL, 0, NULL);
    if (inthread.thread == NULL) {
	win32_perror("CreateThread failed");
	exit(1);
    }
#endif /*]*/

#if !defined(_WIN32) /*[*/
    /* Set up the signal pipes. */
    if (pipe(signalpipe) < 0) {
	perror("pipe");
	exit(1);
    }
    AddInput(signalpipe[0], synchronous_signal);
#endif /*]*/

    /* Get the screen set up as early as possible. */
    screen_init();

    idle_init();
    keymap_init();
    hostfile_init();

    if (appres.httpd_port) {
	struct sockaddr *sa;
	socklen_t sa_len;

	if (!parse_bind_opt(appres.httpd_port, &sa, &sa_len)) {
	    xs_warning("Invalid -httpd port \"%s\"", appres.httpd_port);
	} else {
	    httpd_objects_init();
	    hio_init(sa, sa_len);
	}
    }

    ft_init();

#if !defined(_WIN32) /*[*/
    /* Make sure we don't fall over any SIGPIPEs. */
    (void) signal(SIGPIPE, SIG_IGN);

    /* Make sure we can collect child exit status. */
    (void) signal(SIGCHLD, sigchld_handler);

    /* Handle run-time signals. */
    (void) signal(SIGINT, common_handler);
    (void) signal(SIGTSTP, common_handler);
#endif /*]*/
    task_cb_init_ir_state(&command_ir_state);

    /* Handle initial toggle settings. */
    initialize_toggles();

    if (cl_hostname != NULL) {
	pause_for_errors();
	/* Connect to the host. */
	once = true;
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
    peer_script_init();

    /* Process events forever. */
    while (1) {
	bool was_connected = CONNECTED;
	bool was_escaped = escaped;

	/* Connect to the host. */
	if (cl_hostname != NULL && !cl_connect_done) {
	    if (!host_connect(cl_hostname, IA_UI)) {
		x3270_exit(1);
	    }
	    screen_resume();
	    cl_connect_done = true;
	}

	(void) process_events(true);

	if (!appres.secure &&
		was_connected &&
		!CONNECTED &&
		!appres.interactive.reconnect
		&& !escaped) {
	    screen_suspend();
	    (void) printf("Disconnected.\n");
	    if (once) {
		x3270_exit(0);
	    }
	    interact();
	} else if (!PCONNECTED &&
		!appres.interactive.reconnect &&
		cl_hostname != NULL) {
	    screen_suspend();
#if defined(_WIN32) /*[*/
	    pause_for_errors();
#endif /*]*/
	    if (was_connected) {
		(void) printf("Disconnected.\n");
	    }
	    x3270_exit(0);
	} else if (!was_escaped && escaped) {
	    interact();
	}

	if (PCONNECTED) {
	    screen_disp(false);
	}
    }
}

#if !defined(_WIN32) /*[*/
/* Synchronous signal handler. */
static void
synchronous_signal(iosrc_t fd, ioid_t id)
{
    unsigned char sig;
    int nr;

    /* Read the signal from the pipe. */
    nr = read(signalpipe[0], &sig, 1);
    if (nr < 0) {
	perror("signalpipe read");
	exit(1);
    }

    if (!escaped) {
	vtrace("Ingoring synchronous signal\n");
	return;
    }

    switch (sig) {
    case SIGINT:
	if (command_running) {
	    vtrace("SIGINT while running an action -- ignorning\n");
	} else if (!aux_input) {
	    vtrace("SIGINT at the normal prompt -- ignorning\n");
	} else {
	    vtrace("SIGINT with aux input -- aborting action\n");
#if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
#endif /*]*/
	    printf("\n");
	    aux_input = false;
	    c3270_push_command("ResumeInput(-Abort)");
	    Replace(prompt_string, real_prompt_string);
	    RemoveInput(c3270_input_id);
	    c3270_input_id = NULL_IOID;
	    /* And wait for it to complete before displaying a new prompt. */
	}
	break;
    case SIGTSTP:
	if (command_running) {
	    /* Defer handling until command completes. */
	    vtrace("SIGTSTP while running an action -- deferring\n");
	    stop_pending = true;
	} else {
	    vtrace("SIGTSTP at the %s\n", (pager_pid == 0)? "prompt": "pager");
#if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
#endif /*]*/
	    kill(getpid(), SIGSTOP);
	    if (pager_pid == 0) {
		display_prompt();
	    }
	}
	break;
    default:
	vtrace("Got unknown synchronous signal %u\n", sig);
	break;
    }
}

/* Common signal handler. Writes to the pipe. */
static void
common_handler(int signum)
{
    char sig = signum;

    signal(signum, common_handler);
    write(signalpipe[1], &sig, 1);
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
    if (ft_state != FT_NONE) {
	(void) printf("File transfer in progress.\n");
    }
    if (PCONNECTED && !aux_input) {
	(void) printf("Press <Enter> to resume session.\n");
    }

    stop_pager(); /* to ensure flushing is complete */

#if defined(HAVE_LIBREADLINE) /*[*/
    rl_callback_handler_install(prompt_string, &rl_handler);
#else /*][*/
    fputs(prompt_string, stdout);
    fflush(stdout);
#endif /*]*/
#if !defined(_WIN32) /*[*/
    signal(SIGTSTP, common_handler);
    signal(SIGINT, common_handler);
#endif /*]*/
}

#if defined(_WIN32) /*[*/
static void
enable_input(enum imode mode)
{
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

    /* Get the input. */
#if !defined(_WIN32) /*[*/
#if defined(HAVE_LIBREADLINE) /*[*/
    rl_callback_read_char();
    if (!readline_done) {
	/* More to collect. */
	return;
    }
    command = readline_command;
    readline_done = false;
#else /*][*/
    command = fgets(inbuf, sizeof(inbuf), stdin);
#endif /*]*/
#else /*][*/
    command = inthread.buf;
#endif /*]*/

    if (command == NULL) {
	/* EOF */
	printf("\n");
	fflush(stdout);
	exit(0);
    }

    s = command;
    if (!aux_input) {
	while (isspace((unsigned char)*s)) {
	    s++;
	}
	sl = strlen(s);
	while (sl && isspace((unsigned char)s[sl - 1])) {
	    s[--sl] = '\0';
	}
    } else {
	sl = strlen(s);
	if (sl && s[sl - 1] == '\n') {
	    s[sl - 1] = '\0';
	}
    }

    /* A null command means exit from the prompt. */
    if (!aux_input && !sl) {
	if (PCONNECTED) {
	    /* Stop interacting. */
	    RemoveInput(c3270_input_id);
	    c3270_input_id = NULL_IOID;
	    screen_resume();
#if defined(_WIN32) /*[*/
	    signal(SIGINT, SIG_DFL);
#endif /*]*/
	    goto done;
	} else {
	    /* Continue interacting. Display the prompt. */
	    display_prompt();

	    /* Wait for more input. */
#if defined(_WIN32) /*[*/
	    enable_input(LINE);
#endif /*]*/
	    goto done;
	}
    }

#if defined(HAVE_LIBREADLINE) /*[*/
    /* Save this command in the history buffer. */
    if (!aux_input) {
	add_history(s);
    }
#endif /*]*/

    /* "?" is an alias for "Help". */
    if (!aux_input && !strcmp(s, "?")) {
	s = "Help";
    }

    /* No more input. */
    RemoveInput(c3270_input_id);
    c3270_input_id = NULL_IOID;

    /* Run the command. */
#if defined(_WIN32) /*[*/
    get_console_size(&pager.rows, &pager.cols);
#endif /*]*/
    if (aux_input) {
	aux_input = false;
	c3270_push_command(lazyaf("ResumeInput(%s)",
		    *s? lazya(base64_encode(s)): "\"\""));
	Replace(prompt_string, real_prompt_string);
    } else {
	c3270_push_command(s);
    }

done:
#if defined(HAVE_LIBREADLINE) /*[*/
    Free(command);
#endif /*]*/
    s = s; /* to make the compiler happy */
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
		if (inthread.nr < 0) {
		    inthread.error = GetLastError();
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
    /* In case we got here because of a command output, stop the pager. */
#if 0
    stop_pager();
#endif

    /* In secure mode, we don't interact. */
    if (appres.secure) {
	char s[10];

	printf("[Press <Enter>] ");
	fflush(stdout);
	if (fgets(s, sizeof(s), stdin) == NULL) {
	    x3270_exit(1);
	}
	return;
    }

    /* Now we are interacting. */
    vtrace("Interacting.\n");

    if (escape_action != NULL) {
	c3270_push_command(escape_action);
	Replace(escape_action, NULL);
	return;
    }

#if !defined(_WIN32) /*[*/
    /* screen_init/screen_resume reset these. */
    (void) signal(SIGINT, common_handler);
    (void) signal(SIGTSTP, common_handler);
#endif /*]*/

    /* Display the prompt. */
    display_prompt();

    /* Wait for input. */
    assert(c3270_input_id == NULL_IOID);
#if !defined(_WIN32) /*[*/
    c3270_input_id = AddInput(0, c3270_input);
#else /*][*/
    c3270_input_id = AddInput(inthread.done_event, c3270_input);
    enable_input(LINE);
#endif /*]*/
    signal(SIGINT, SIG_IGN);
}

#if !defined(_WIN32) /*[*/
static void
pager_exit(ioid_t id, int status)
{
    vtrace("pager exited with status %d\n", status);

    pager_pid = 0;
    if (command_output || !CONNECTED) {
	/* Command produced output, or we are not connected any more. */

	/* Process a pending stop. */
	if (stop_pending) {
	    vtrace("Processing deferred SIGTSTP on pager exit\n");
	    stop_pending = false;
#if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
#endif /*]*/
	    kill(getpid(), SIGSTOP);
	}

	/* Display the prompt. */
	display_prompt();

	/* Wait for more input. */
	assert(c3270_input_id == NULL_IOID);
	c3270_input_id = AddInput(0, c3270_input);
    } else {
	/* Exit interactive mode. */
	screen_resume();
    }

    stop_pending = false;
}
#endif /*]*/

/* A command is about to produce output.  Start the pager. */
FILE *
start_pager(void)
{
#if !defined(_WIN32) /*[*/
    static char *lesspath = LESSPATH;
    static char *lesscmd = LESSPATH " -EX";
    static char *morepath = MOREPATH;
    static char *or_cat = " || cat";
    char *pager_env;
    char *pager_cmd = NULL;

    if (pager != NULL) {
	return pager;
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
	(void) sprintf(s, "%s%s", pager_cmd, or_cat);
	pager = xpopen(s, "w", &pager_pid);
	Free(s);
	if (pager == NULL) {
	    (void) perror(pager_cmd);
	} else {
	    AddChild(pager_pid, pager_exit);
	}
    }
    if (pager == NULL) {
	pager = stdout;
    }
    return pager;
#else /*][*/
    if (!pager.running) {
	pager.rowcnt = 0;
	Replace(pager.residual, NULL);
	pager.flushing = false;
	pager.running = true;
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
    if (pager != NULL) {
	if (pager != stdout) {
	    xpclose(pager, XPC_NOWAIT);
	}
	pager = NULL;
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
pager_key_done(iosrc_t fd, ioid_t id)
{
    char *p;

    pager.flushing = inthread.buf[0] == 'q';
    vtrace("Got pager key%s\n", pager.flushing? " (quit)": "");

    /* No more key-mode input. */
    RemoveInput(c3270_input_id);
    c3270_input_id = NULL_IOID;

    /* Overwrite the prompt and reset. */
    printf("\r%*s\r", (pager.nw > 0)? pager.nw: 79, "");
    fflush(stdout);
    pager.rowcnt = 0;
    get_console_size(&pager.rows, &pager.cols);

    if (pager.flushing && command_complete) {
	/* Pressed 'q' and the command is complete. */

	/* New prompt. */
	display_prompt();
	assert(c3270_input_id == NULL_IOID);
	c3270_input_id = AddInput(inthread.done_event, c3270_input);
	enable_input(LINE);
	return;
    }

    /* Dump what's remaining, which might leave more output pager.residual. */
    p = pager.residual;
    pager.residual = NULL;
    pager_output(p);
    Free(p);

    if (command_complete && pager.residual == NULL) {
	/* Command no longer running, and no more pending otuput. */
	display_prompt();
	assert(c3270_input_id == NULL_IOID);
	c3270_input_id = AddInput(inthread.done_event, c3270_input);
	enable_input(LINE);
    }
}

/* Write a line of output to the pager. */
void
pager_output(const char *s)
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
	    pager.nw = printf("Press any key to continue . . . ");
	    fflush(stdout);
	    assert(c3270_input_id == NULL_IOID);
	    c3270_input_id = AddInput(inthread.done_event, pager_key_done);
	    enable_input(KEY);
	    return;
	}

	/*
	 * Look for an embedded newline.  If one is found, just print
	 * up to it, so we can count the newline and possibly pause
	 * partway through the string.
	 */
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
    while (*s && isspace(*s)) {
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
	while (*t && !isspace(*t)) {
	    t++;
	}
	while (*t && isspace(*t)) {
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
	if (!((!strncasecmp(s, "Open", 4) && isspace(*(s + 4))) ||
	      (!strncasecmp(s, "Connect", 7) && isspace(*(s + 7))))) {
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
		(void) sprintf(matches[j], "\"%s\"", h->name);
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

/* Return a time difference in English */
static char *
hms(time_t ts)
{
    time_t t, td;
    long hr, mn, sc;

    (void) time(&t);

    td = t - ts;
    hr = (long)(td / 3600);
    mn = (td % 3600) / 60;
    sc = td % 60;

    if (hr > 0) {
	return lazyaf("%ld %s %ld %s %ld %s",
	    hr, (hr == 1) ?
		get_message("hour") : get_message("hours"),
	    mn, (mn == 1) ?
		get_message("minute") : get_message("minutes"),
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    } else if (mn > 0) {
	return lazyaf("%ld %s %ld %s",
	    mn, (mn == 1) ?
		get_message("minute") : get_message("minutes"),
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    } else {
	return lazyaf("%ld %s",
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    }
}

static void
indent_dump(const char *s)
{
    const char *newline;

    while ((newline = strchr(s, '\n')) != NULL) {
	action_output("    %.*s", (int)(newline - s), s);
	s = newline + 1;
    }
    action_output("    %s", s);
}

static void
status_dump(void)
{
    const char *emode, *ftype, *ts;
    const char *clu;
    const char *eopts;
    const char *bplu;
    const char *ptype;

    action_output("%s", build);
    action_output("%s %s: %d %s x %d %s, %s, %s",
	    get_message("model"), model_name,
	    maxCOLS, get_message("columns"),
	    maxROWS, get_message("rows"),
	    appres.m3279? get_message("fullColor"): get_message("mono"),
	    (appres.extended && !HOST_FLAG(STD_DS_HOST))?
		get_message("extendedDs"): get_message("standardDs"));
    action_output("%s %s", get_message("terminalName"), termtype);
    clu = net_query_lu_name();
    if (clu != NULL && clu[0]) {
	action_output("%s %s", get_message("luName"), clu);
    }
    bplu = net_query_bind_plu_name();
    if (bplu != NULL && bplu[0]) {
	action_output("%s %s", get_message("bindPluName"), bplu);
    }
    action_output("%s %s (%s)", get_message("characterSet"),
	    get_charset_name(), dbcs? "DBCS": "SBCS");
    action_output("%s %s",
	    get_message("hostCodePage"),
	    get_host_codepage());
    action_output("%s GCSGID %u, CPGID %u",
	    get_message("sbcsCgcsgid"),
	    (unsigned short)((cgcsgid >> 16) & 0xffff),
	    (unsigned short)(cgcsgid & 0xffff));
    if (dbcs) {
	action_output("%s GCSGID %u, CPGID %u",
		get_message("dbcsCgcsgid"),
		(unsigned short)((cgcsgid_dbcs >> 16) & 0xffff),
		(unsigned short)(cgcsgid_dbcs & 0xffff));
    }
#if !defined(_WIN32) /*[*/
    action_output("%s %s", get_message("localeCodeset"), locale_codeset);
    action_output("%s DBCS %s, wide curses %s",
	    get_message("buildOpts"),
# if defined(X3270_DBCS) /*[*/
	    get_message("buildEnabled"),
# else /*][*/
	    get_message("buildDisabled"),
# endif /*]*/
# if defined(CURSES_WIDE) /*[*/
	    get_message("buildEnabled")
# else /*][*/
	    get_message("buildDisabled")
# endif /*]*/
	    );
#else /*][*/
    action_output("%s OEM %d ANSI %d", get_message("windowsCodePage"),
	    windows_cp, GetACP());
#endif /*]*/
    if (appres.interactive.key_map) {
	action_output("%s %s", get_message("keyboardMap"),
		appres.interactive.key_map);
    }
    if (CONNECTED) {
	action_output("%s %s", get_message("connectedTo"),
#if defined(LOCAL_PROCESS) /*[*/
		(local_process && !strlen(current_host))? "(shell)":
#endif /*]*/
		current_host);
#if defined(LOCAL_PROCESS) /*[*/
	if (!local_process)
#endif /*]*/
	{
	    action_output("  %s %d", get_message("port"), current_port);
	}
	if (net_secure_connection()) {
	    const char *session, *cert;

	    action_output("  %s%s%s", get_message("secure"),
			net_secure_unverified()? ", ": "",
			net_secure_unverified()? get_message("unverified"): "");
	    action_output("  %s %s", get_message("provider"),
		    net_sio_provider());
	    if ((session = net_session_info()) != NULL) {
		action_output("  %s", get_message("sessionInfo"));
		indent_dump(session);
	    }
	    if ((cert = net_server_cert_info()) != NULL) {
		action_output("  %s", get_message("serverCert"));
		indent_dump(cert);
	    }
	}
	ptype = net_proxy_type();
	if (ptype) {
	    action_output("  %s %s  %s %s  %s %s",
		    get_message("proxyType"), ptype,
		    get_message("server"), net_proxy_host(),
		    get_message("port"), net_proxy_port());
	}
	ts = hms(ns_time);
	if (IN_E) {
	    emode = "TN3270E ";
	} else {
	    emode = "";
	}
	if (IN_NVT) {
	    if (linemode) {
		ftype = get_message("lineMode");
	    } else {
		ftype = get_message("charMode");
	    }
	    action_output("  %s%s, %s", emode, ftype, ts);
	} else if (IN_SSCP) {
	    action_output("  %s%s, %s", emode, get_message("sscpMode"), ts);
	} else if (IN_3270) {
	    action_output("  %s%s, %s", emode, get_message("dsMode"), ts);
	} else if (cstate == CONNECTED_UNBOUND) {
	    action_output("  %s%s, %s", emode, get_message("unboundMode"), ts);
	} else {
	    action_output("  %s, %s", get_message("unnegotiated"), ts);
	}

	eopts = tn3270e_current_opts();
	if (eopts != NULL) {
	    action_output("  %s %s", get_message("tn3270eOpts"), eopts);
	} else if (IN_E) {
	    action_output("  %s", get_message("tn3270eNoOpts"));
	}

	if (IN_3270) {
	    action_output("%s %d %s, %d %s\n%s %d %s, %d %s",
		    get_message("sent"),
		    ns_bsent, (ns_bsent == 1)?
			get_message("byte") : get_message("bytes"),
		    ns_rsent, (ns_rsent == 1)?
			get_message("record") : get_message("records"),
		    get_message("Received"), ns_brcvd,
			(ns_brcvd == 1)? get_message("byte"):
					 get_message("bytes"),
		    ns_rrcvd,
		    (ns_rrcvd == 1)? get_message("record"):
				     get_message("records"));
	} else {
	    action_output("%s %d %s, %s %d %s",
		    get_message("sent"), ns_bsent,
		    (ns_bsent == 1)? get_message("byte"):
				     get_message("bytes"),
		    get_message("received"), ns_brcvd,
		    (ns_brcvd == 1)? get_message("byte"):
				     get_message("bytes"));
	}

	if (IN_NVT) {
	    struct ctl_char *c = linemode_chars();
	    int i;
	    char buf[128];
	    char *s = buf;

	    action_output("%s", get_message("specialCharacters"));
	    for (i = 0; c[i].name; i++) {
		if (i && !(i % 4)) {
		    *s = '\0';
		    action_output("%s", buf);
		    s = buf;
		}
		s += sprintf(s, "  %s %s", c[i].name, c[i].value);
	    }
	    if (s != buf) {
		*s = '\0';
		action_output("%s", buf);
	    }
	}
    } else if (HALF_CONNECTED) {
	action_output("%s %s", get_message("connectionPending"),
	    current_host);
    } else {
	action_output("%s", get_message("notConnected"));
    }
}

static void
copyright_dump(void)
{
    action_output(" ");
    action_output("%s", build);
    action_output(" ");
    action_output("Copyright (c) 1993-%s, Paul Mattes.", cyear);
    action_output("Copyright (c) 1990, Jeff Sparkes.");
    action_output("Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA");
    action_output(" 30332.");
    action_output("All rights reserved.");
    action_output(" ");
    action_output("Redistribution and use in source and binary forms, with or without");
    action_output("modification, are permitted provided that the following conditions are met:");
    action_output("    * Redistributions of source code must retain the above copyright");
    action_output("      notice, this list of conditions and the following disclaimer.");
    action_output("    * Redistributions in binary form must reproduce the above copyright");
    action_output("      notice, this list of conditions and the following disclaimer in the");
    action_output("      documentation and/or other materials provided with the distribution.");
    action_output("    * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of");
    action_output("      their contributors may be used to endorse or promote products derived");
    action_output("      from this software without specific prior written permission.");
    action_output(" ");
    action_output("THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC \"AS IS\" AND");
    action_output("ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE");
    action_output("IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE");
    action_output("ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE");
    action_output("LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR");
    action_output("CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF");
    action_output("SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS");
    action_output("INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN");
    action_output("CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)");
    action_output("ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE");
    action_output("POSSIBILITY OF SUCH DAMAGE.");
    action_output(" ");
}

static bool
Show_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Show", ia, argc, argv);
    if (check_argc("Show", argc, 0, 1) < 0) {
	return false;
    }

    if (argc == 0) {
	action_output("  Show copyright   copyright information");
	action_output("  Show stats       connection statistics");
	action_output("  Show status      same as 'Show stats'");
	action_output("  Show keymap      current keymap");
	return true;
    }
    if (!strncasecmp(argv[0], "stats", strlen(argv[0])) ||
	!strncasecmp(argv[0], "status", strlen(argv[0]))) {
	status_dump();
    } else if (!strncasecmp(argv[0], "keymap", strlen(argv[0]))) {
	keymap_dump();
    } else if (!strncasecmp(argv[0], "copyright", strlen(argv[0]))) {
	copyright_dump();
    } else {
	popup_an_error("Unknown 'Show' keyword");
	return false;
    }
    return true;
}

/* Trace([data|keyboard][on [filename]|off]) */
static bool
Trace_action(ia_t ia, unsigned argc, const char **argv)
{
    bool on = false;
    unsigned arg0 = 0;

    action_debug("Trace", ia, argc, argv);

    if (argc == 0) {
	if (toggled(TRACING) && tracefile_name != NULL) {
	    action_output("Trace file is %s.", tracefile_name);
	} else {
	    action_output("Tracing is %sabled.",
		    toggled(TRACING)? "en": "dis");
	}
	return true;
    }

    if (!strcasecmp(argv[0], "Data") || !strcasecmp(argv[0], "Keyboard")) {
	/* Skip. */
	arg0++;
    }
    if (!strcasecmp(argv[arg0], "Off")) {
	on = false;
	arg0++;
	if (argc > arg0) {
	    popup_an_error("Trace: Too many arguments for 'Off'");
	    return false;
	}
	if (!toggled(TRACING)) {
	    return true;
	}
    } else if (!strcasecmp(argv[arg0], "On")) {
	on = true;
	arg0++;
	if (argc == arg0) {
	    /* Nothing else to do. */
	} else if (argc == arg0 + 1) {
	    if (toggled(TRACING)) {
		popup_an_error("Trace: filename argument ignored.");
	    } else {
		trace_set_trace_file(argv[arg0]);
	    }
	} else {
	    popup_an_error("Trace: Too many arguments for 'On'");
	    return false;
	}
    } else {
	popup_an_error("Trace: Parameter must be On or Off");
	return false;
    }

    if ((on && !toggled(TRACING)) || (!on && toggled(TRACING))) {
	do_toggle(TRACING);
	if (!on) {
	    action_output("Tracing stopped.");
	}
    }

    if (tracefile_name != NULL) {
	if (task_is_interactive()) {
	    action_output("Trace file is %s.", tracefile_name);
	} else {
	    popup_an_info("Trace file is %s.", tracefile_name);
	}
    }
    return true;
}

/* Break to the command prompt. */
static bool
Escape_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Escape", ia, argc, argv);
    if (check_argc("Escape", argc, 0, 1) < 0) {
	return false;
    }

    if (escaped && argc > 0) {
	popup_an_error("Cannot nest Escape()");
	return false;
    }

    if (!escaped && !appres.secure) {
	if (argc > 0) {
	    escape_action = NewString(argv[0]);
	}
	host_cancel_reconnect(); /* why? */
	screen_suspend();
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
    (void) vsprintf(vmsgbuf, fmt, args);
    va_end(args);

    /* Remove trailing newlines. */
    sl = strlen(vmsgbuf);
    while (sl && vmsgbuf[sl - 1] == '\n') {
	vmsgbuf[--sl] = '\0';
    }

    /* Push it out. */
    if (sl) {
	if (/*escaped*/false) {
	    printf("%s\n", vmsgbuf);
	    fflush(stdout);
	} else {
	    char *s;

	    while ((s = strchr(vmsgbuf, '\n')) != NULL) {
		*s = ' ';
	    }
	    status_push(vmsgbuf);
	}
    }
}

static bool
Info_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Info", ia, argc, argv);

    if (!argc) {
	return true;
    }

    popup_an_info("%s", argv[0]);
    return true;
}

static bool
ignore_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("ignore", ia, argc, argv);
    return true;
}

/* Command-prompt action support. */
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
    &command_irv
};

/**
 * Callback for data returned to action.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] data	True if data, false if error message
 */
static void
command_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    if (handle != (tcb_t *)&command_cb) {
	vtrace("command_data: no match\n");
	return;
    }

    if (!success && !strncmp(buf, INPUT, strlen(INPUT))) {
	prompt_string = base64_decode(buf + strlen(INPUT));
	aux_input = true;
	command_output = true; /* a while lie */
    } else {
	glue_gui_output(lazyaf("%.*s", (int)len, buf));
    }
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
    if (pager.residual != NULL) {
	/* Pager is paused, don't do anything yet. */
	command_complete = true;
	return true;
    }
#endif /*]*/

    /* Stop the pager. */
    stop_pager();

#if !defined(_WIN32) /*[*/
    if (pager_pid != 0) {
	/* Pager process is still running, don't do anything else yet. */
	command_complete = true;
	return true;
    }
#endif /*]*/

    /* The command from the prompt completed. */
    if (command_output || !PCONNECTED) {
	/* Command produced output, or we are not connected any more. */

#if !defined(_WIN32) /*[*/
	/* Process a pending stop. */
	if (stop_pending) {
	    vtrace("Processing deferred SIGTSTP on command completion\n");
	    stop_pending = false;
#if defined(HAVE_LIBREADLINE) /*[*/
	    rl_callback_handler_remove();
#endif /*]*/
	    kill(getpid(), SIGSTOP);
#if !defined(_WIN32) /*[*/
	    if (pager_pid != 0) {
		return true;
	    }
#endif /*]*/
	}
#endif /*]*/

	/* Display the prompt. */
	display_prompt();

	/* Wait for more input. */
	assert(c3270_input_id == NULL_IOID); // crashes after disc
#if !defined(_WIN32) /*[*/
	c3270_input_id = AddInput(0, c3270_input);
#else /*][*/
	c3270_input_id = AddInput(inthread.done_event, c3270_input);
	enable_input(LINE);
#endif /*]*/
    } else {
	/* Exit interactive mode. */
	screen_resume();
#if defined(_WIN32) /*[*/
	signal(SIGINT, SIG_DFL);
#endif /*]*/
    }

#if !defined(_WIN32) /*[*/
    stop_pending = false;
#endif /*]*/
    command_complete = true;
    return true;
}

static unsigned
command_getflags(task_cbh handle)
{
    /*
     * INTERACTIVE: We understand [input] responses.
     * CONNECT_NONBLOCK: We do not want Connect()/Open() to block.
     */
    return CBF_INTERACTIVE | CBF_CONNECT_NONBLOCK;
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
c3270_push_command(char *s)
{
    command_running = true;
    command_complete = false;
    command_output = false;

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
start_auto_shortcut(void)
{
    char *tempdir;
    FILE *f;
    session_t s;
    HRESULT hres;
    char exepath[MAX_PATH];
    char linkpath[MAX_PATH];
    char sesspath[MAX_PATH];
    char delenv[32 + MAX_PATH];
    char args[1024];
    HINSTANCE h;
    char *cwd;

    /* Make sure there is a session file. */
    if (profile_path == NULL) {
	fprintf(stderr, "Can't use auto-shortcut mode without a "
		    "session file\n");
	fflush(stderr);
	return;
    }

#if defined(AS_DEBUG) /*[*/
    printf("Running auto-shortcut\n");
    fflush(stdout);
#endif /*]*/

    /* Read the session file into 's'. */
    f = fopen(profile_path, "r");
    if (f == NULL) {
	fprintf(stderr, "%s: %s\n", profile_path, strerror(errno));
	x3270_exit(1);
    }
    memset(&s, '\0', sizeof(session_t));
    if (read_session(f, &s, NULL) == 0) {
	fprintf(stderr, "%s: invalid format\n", profile_path);
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
	x3270_exit(1);
    }
    sprintf(args, "+S \"%s\"", sesspath);
    cwd = getcwd(NULL, 0);
    hres = create_shortcut(&s,		/* session */
			   exepath,	/* .exe    */
			   linkpath,	/* .lnk    */
			   args,	/* args    */
			   cwd		/* cwd     */);
    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "Cannot create ShellLink '%s'\n", linkpath);
	x3270_exit(1);
    }
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
	x3270_exit(1);
    }

#if defined(AS_DEBUG) /*[*/
    printf("Started ShellLink\n");
    fflush(stdout);
#endif /*]*/

    exit(0);
}

/* Start a browser window to display wc3270 help. */
void
start_html_help(void)
{
    system(lazyaf("start \"wc3270 Help\" \"%shtml\\README.html\"", instdir));

    /* Get back mouse events */
    screen_system_fixup();
}

/* Start a copy of the Session Wizard. */
void
start_wizard(const char *session)
{
    char *cmd;

    if (session != NULL) {
	cmd = xs_buffer("start \"wc3270 Session Wizard\" \"%swc3270wiz.exe\" "
		"-e \"%s\"", instdir, session);
    } else {
	cmd = xs_buffer("start \"wc3270 Session Wizard\" \"%swc3270wiz.exe\"",
		instdir);
    }
    system(cmd);
    Free(cmd);

    /* Get back mouse events */
    screen_system_fixup();
}

#endif /*]*/

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
    appres.c3270.meta_escape = "auto";
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
bool
glue_gui_output(const char *s)
{
    screen_suspend();

#if !defined(_WIN32) /*[*/
    (void) fprintf(start_pager(), "%s\n", s);
#else /*][*/
    start_pager();
    pager_output(s);
#endif /*]*/
    command_output = true;
    /* any_error_output = true; */ /* XXX: Needed? */
    return true;
}

/**
 * GUI redirect function for popup_an_error.
 */
bool
glue_gui_error(const char *s)
{
    bool was_escaped = escaped;

    if (!was_escaped) {
	screen_suspend();
    } else {
#if defined(_WIN32) /*[*/
	if (pager.residual == NULL) {
	    /* Send yourself an ESC to flush current input. */
	    screen_send_esc();
	}
#endif /*]*/
    }

    ring_bell();
    fprintf(stderr, "\n%s\n", s);
    fflush(stderr);
    any_error_output = true;

    if (was_escaped) {
#if !defined(_WIN32) /*[*/
	/* Interrupted the prompt. */
# if defined(HAVE_LIBREADLINE) /*[*/
	/* Redisplay the prompt and any pending input. */
	rl_forced_update_display();
# else /*][*/
	/* Discard any pending input and redisplay the prompt. */
	tcflush(0, TCIFLUSH);
	display_prompt();
# endif /*]*/
#else /*][*/
	if (pager.residual == NULL) {
	    /* Redisplay the c3270 prompt. */
	    display_prompt();
	} else {
	    /* Redisplay the pager prompt. */
	    pager.nw = printf("Press any key to continue . . . ");
	    fflush(stdout);
	}
#endif /*]*/
    }

    return true;
}

/**
 * c3270 main module registration.
 */
static void
c3270_register(void)
{
    static action_table_t actions[] = {
	{ "Escape",		Escape_action,		ACTION_KE },
	{ "ignore",		ignore_action,		ACTION_KE },
	{ "Info",		Info_action,		ACTION_KE },
	{ "Show",		Show_action,		ACTION_KE },
	{ "Trace",		Trace_action,		ACTION_KE },
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
	{ OptPrinterLu,OPT_STRING,  false, ResPrinterLu,
	    aoffset(interactive.printer_lu),
	    "<luname>",
	    "Automatically start a "PR3287_NAME" printer session to <luname>" },
	{ OptReconnect,OPT_BOOLEAN, true,  ResReconnect,
	    aoffset(interactive.reconnect),
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
	{ ResAllBold,	aoffset(c3270.all_bold_on),	XRM_STRING },
	{ ResAsciiBoxDraw,aoffset(c3270.ascii_box_draw),XRM_BOOLEAN },
	{ ResIdleCommand,aoffset(idle_command),		XRM_STRING },
	{ ResIdleCommandEnabled,aoffset(idle_command_enabled),XRM_BOOLEAN },
	{ ResIdleTimeout,aoffset(idle_timeout),		XRM_STRING },
	{ ResKeymap,	aoffset(interactive.key_map),	XRM_STRING },
	{ ResMenuBar,	aoffset(interactive.menubar),	XRM_BOOLEAN },
	{ ResNoPrompt,	aoffset(secure),		XRM_BOOLEAN },
	{ ResPrinterLu,	aoffset(interactive.printer_lu),XRM_STRING },
	{ ResPrinterOptions,aoffset(interactive.printer_opts),XRM_STRING },
	{ ResReconnect,	aoffset(interactive.reconnect),XRM_BOOLEAN },
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
	{ ResAssocCommand,		V_FLAT },
	{ ResLuCommandLine,		V_FLAT },
	{ ResPrintTextScreensPerPage,	V_FLAT },
	{ ResMessage,			V_WILD },
#if defined(_WIN32) /*[*/
	{ ResPrinterCodepage,		V_FLAT },
	{ ResPrinterCommand,		V_FLAT },
	{ ResPrinterName, 		V_FLAT },
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
	{ ResPrinterCommand,		V_FLAT },
	{ ResPrintTextCommand,		V_FLAT },
	{ ResCursesColorForDefault,	V_FLAT },
	{ ResCursesColorForIntensified,	V_FLAT },
	{ ResCursesColorForProtected,	V_FLAT },
	{ ResCursesColorForProtectedIntensified,V_FLAT },
	{ ResCursesColorForHostColor,	V_COLOR },
#endif /*]*/
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
}
