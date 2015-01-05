/*
 * Copyright (c) 1993-2015, Paul Mattes.
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
#endif /*]*/
#include <signal.h>
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actionsc.h"
#include "bind-optc.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "unicodec.h"
#include "ftc.h"
#include "gluec.h"
#include "help.h"
#include "hostc.h"
#include "httpd-corec.h"
#include "httpd-nodesc.h"
#include "httpd-ioc.h"
#include "icmdc.h"
#include "idlec.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "linemodec.h"
#include "macrosc.h"
#include "menubarc.h"
#include "nvtc.h"
#include "popupsc.h"
#include "print_screen.h"
#include "pr3287_session.h"
#include "screenc.h"
#include "scrollc.h"
#include "selectc.h"
#include "statusc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace.h"
#include "utf8c.h"
#include "utilc.h"
#include "xioc.h"

#if defined(HAVE_LIBREADLINE) /*[*/
#include <readline/readline.h>
#if defined(HAVE_READLINE_HISTORY_H) /*[*/
#include <readline/history.h>
#endif /*]*/
#endif /*]*/

#if defined(_WIN32) /*[*/
#include "w3miscc.h"
#include "winversc.h"
#include "windirsc.h"
#include "relinkc.h"
#endif /*]*/

#if defined(_WIN32) /*[*/
# define PROGRAM_NAME	"wc3270"
#else /*][*/
# define PROGRAM_NAME	"c3270"
#endif /*]*/

#if defined(_WIN32) /*[*/
# define DELENV		"WC3DEL"
#endif /*]*/

static void interact(void);
static void stop_pager(void);

#if defined(HAVE_LIBREADLINE) /*[*/
static char **attempted_completion();
static char *completion_entry(const char *, int);
#endif /*]*/

/* Pager state. */
#if !defined(_WIN32) /*[*/
static FILE *pager = NULL;
#else /*][*/
static int pager_rowcnt = 0;
static Boolean pager_q = False;
#endif /*]*/

#if !defined(_WIN32) /*[*/
/* Base keymap for c3270. */
static char *base_keymap1 =
           "Ctrl<Key>]: Escape\n"
"Ctrl<Key>a Ctrl<Key>a: Key(0x01)\n"
"Ctrl<Key>a Ctrl<Key>]: Key(0x1d)\n"
    "Ctrl<Key>a <Key>c: Clear\n"
    "Ctrl<Key>a <Key>e: Escape\n"
    "Ctrl<Key>a <Key>i: Insert\n"
    "Ctrl<Key>a <Key>r: Reset\n"
    "Ctrl<Key>a <Key>k: Keypad\n"
    "Ctrl<Key>a <Key>l: Redraw\n"
    "Ctrl<Key>a <Key>m: Compose\n"
    "Ctrl<Key>a <Key>n: Menu\n"
    "Ctrl<Key>a <Key>p: PrintText\n"
    "Ctrl<Key>a <Key>^: Key(notsign)\n"
              "<Key>DC: Delete\n"
              "<Key>UP: Up\n"
            "<Key>DOWN: Down\n"
            "<Key>LEFT: Left\n"
           "<Key>RIGHT: Right\n"
            "<Key>HOME: Home\n"
    "Ctrl<Key>a <Key>1: PA(1)\n"
    "Ctrl<Key>a <Key>2: PA(2)\n";
static char *base_keymap2 =
    "Ctrl<Key>a <Key>3: PA(3)\n"
              "<Key>F1: PF(1)\n"
   "Ctrl<Key>a <Key>F1: PF(13)\n"
              "<Key>F2: PF(2)\n"
   "Ctrl<Key>a <Key>F2: PF(14)\n"
              "<Key>F3: PF(3)\n"
   "Ctrl<Key>a <Key>F3: PF(15)\n"
              "<Key>F4: PF(4)\n"
   "Ctrl<Key>a <Key>F4: PF(16)\n"
              "<Key>F5: PF(5)\n"
   "Ctrl<Key>a <Key>F5: PF(17)\n"
              "<Key>F6: PF(6)\n"
   "Ctrl<Key>a <Key>F6: PF(18)\n";
static char *base_keymap3 =
              "<Key>F7: PF(7)\n"
   "Ctrl<Key>a <Key>F7: PF(19)\n"
              "<Key>F8: PF(8)\n"
   "Ctrl<Key>a <Key>F8: PF(20)\n"
              "<Key>F9: PF(9)\n"
   "Ctrl<Key>a <Key>F9: PF(21)\n"
             "<Key>F10: PF(10)\n"
  "Ctrl<Key>a <Key>F10: PF(22)\n"
             "<Key>F11: PF(11)\n"
  "Ctrl<Key>a <Key>F11: PF(23)\n"
             "<Key>F12: PF(12)\n"
  "Ctrl<Key>a <Key>F12: PF(24)\n"
           "<Key>PPAGE: Scroll(Backward)\n"
           "<Key>NPAGE: Scroll(Forward)\n";

/* Base keymap for c3270, 3270 mode. */
static char *base_3270_keymap =
    "Ctrl<Key>a <Key>a: Attn\n"
           "Ctrl<Key>c: Clear\n"
           "Ctrl<Key>d: Dup\n"
           "Ctrl<Key>f: FieldMark\n"
           "Ctrl<Key>h: Erase\n"
           "Ctrl<Key>i: Tab\n"
           "Ctrl<Key>j: Newline\n"
           "Ctrl<Key>k: Keypad\n"
           "Ctrl<Key>l: Redraw\n"
           "Ctrl<Key>m: Enter\n"
           "Ctrl<Key>n: Menu\n"
           "Ctrl<Key>r: Reset\n"
           "Ctrl<Key>u: DeleteField\n"
    "Ctrl<Key>a <Key>v: ToggleReverse\n"
    "Ctrl<Key>a <Key>f: Flip\n"
              "<Key>IC: ToggleInsert\n"
              "<Key>DC: Delete\n"
       "<Key>BACKSPACE: Erase\n"
            "<Key>HOME: Home\n"
             "<Key>END: FieldEnd\n";
  
#else /*][*/

/* Base keymap for wc3270. */
static char *base_keymap =
           "Alt <Key>1: PA(1)\n"
           "Alt <Key>2: PA(2)\n"
           "Alt <Key>3: PA(3)\n"
           "Alt <Key>^: Key(notsign)\n"
          "Ctrl <Key>c: Copy\n"
           "Alt <Key>k: Keypad\n"
           "Alt <Key>K: Keypad\n"
           "Alt <Key>l: Redraw\n"
           "Alt <Key>L: Redraw\n"
           "Alt <Key>m: Compose\n"
           "Alt <Key>M: Compose\n"
           "Alt <Key>n: Menu\n"
           "Alt <Key>N: Menu\n"
           "Alt <Key>p: PrintText\n"
           "Alt <Key>P: PrintText\n"
          "Ctrl <Key>v: Paste\n"
          "Ctrl <Key>x: Cut\n"
          "Ctrl <Key>]: Escape\n"
        "Shift <Key>F1: PF(13)\n"
        "Shift <Key>F2: PF(14)\n"
        "Shift <Key>F3: PF(15)\n"
        "Shift <Key>F4: PF(16)\n"
        "Shift <Key>F5: PF(17)\n"
        "Shift <Key>F6: PF(18)\n"
        "Shift <Key>F7: PF(19)\n"
        "Shift <Key>F8: PF(20)\n"
       " Shift <Key>F9: PF(21)\n"
       "Shift <Key>F10: PF(22)\n"
       "Shift <Key>F11: PF(23)\n"
       "Shift <Key>F12: PF(24)\n"
    "Shift <Key>ESCAPE: Key(0x1d)\n"
           "<Key>PRIOR: Scroll(Backward)\n"
	    "<Key>NEXT: Scroll(Forward)";

/* Base keymap for wc3270, 3270 mode. */
static char *base_3270_keymap =
          "Ctrl <Key>a: Attn\n"
           "Alt <Key>a: Attn\n"
           "Alt <Key>A: Attn\n"
           "Alt <Key>c: Clear\n"
           "Alt <Key>C: Clear\n"
          "Ctrl <Key>d: Dup\n"
           "Alt <Key>d: Dup\n"
           "Alt <Key>D: Dup\n"
          "Ctrl <Key>f: FieldMark\n"
           "Alt <Key>f: FieldMark\n"
           "Alt <Key>F: FieldMark\n"
          "Ctrl <Key>h: Erase\n"
           "Alt <Key>i: Insert\n"
           "Alt <Key>I: Insert\n"
          "Ctrl <Key>i: Tab\n"
          "Ctrl <Key>j: Newline\n"
          "Ctrl <Key>l: Redraw\n"
          "Ctrl <Key>m: Enter\n"
          "Ctrl <Key>r: Reset\n"
           "Alt <Key>r: Reset\n"
           "Alt <Key>R: Reset\n"
          "Ctrl <Key>u: DeleteField\n"
           "Alt <Key>v: ToggleReverse\n"
           "Alt <Key>x: Flip\n"
          "<Key>INSERT: ToggleInsert\n"
       "Shift <Key>TAB: BackTab\n"
            "<Key>BACK: Erase\n"
       "Shift <Key>END: EraseEOF\n"
             "<Key>END: FieldEnd\n"
      "Shift <Key>LEFT: PreviousWord\n"
     "Shift <Key>RIGHT: NextWord\n";
#endif /*]*/

Boolean any_error_output = False;
Boolean escape_pending = False;
Boolean stop_pending = False;
Boolean dont_return = False;

#if defined(_WIN32) /*[*/
char *instdir = NULL;
char *myappdata = NULL;
char *commonappdata = NULL;
char *mydesktop = NULL;
int is_installed;
static void start_auto_shortcut(void);
#endif /*]*/

static action_t Escape_action;
static action_t ignore_action;
static action_t Info_action;
static action_t ScreenTrace_action;
static action_t Show_action;
static action_t Trace_action;

static action_table_t main_actions[] = {
    { "Escape",		Escape_action,		ACTION_KE },
    { "ignore",		ignore_action,		ACTION_KE },
    { "Info",		Info_action,		ACTION_KE },
    { "ScreenTrace",	ScreenTrace_action,	ACTION_KE },
    { "Show",		Show_action,		ACTION_KE },
    { "Trace",		Trace_action,		ACTION_KE },
};

void
usage(const char *msg)
{
	if (msg != NULL)
		fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "Usage: %s [options] [ps:][LUname@]hostname[:port]\n",
		programname);
	fprintf(stderr, "Options:\n");
	cmdline_help(False);
	exit(1);
}

/* Callback for connection state changes. */
static void
main_connect(Boolean ignored)
{
	if (CONNECTED || appres.disconnect_clear) {
#if defined(C3270_80_132) /*[*/
		if (appres.altscreen != NULL)
			ctlr_erase(False);
		else
#endif /*]*/
			ctlr_erase(True);
	}
} 

/* Callback for application exit. */
static void
main_exiting(Boolean ignored)
{       
	if (escaped)
		stop_pager();
	else
		if (screen_suspend())
		    	screen_final();
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
		if (fgets(s, sizeof(s), stdin) == NULL)
			x3270_exit(1);
		any_error_output = False;
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

int
main(int argc, char *argv[])
{
	const char	*cl_hostname = NULL;
#if !defined(_WIN32) /*[*/
	pid_t		 pid;
	int		 status;
#else /*][*/
	char		*delenv;
#endif /*]*/

#if defined(_WIN32) /*[*/
	(void) get_version_info();
	if (get_dirs(argv[0], "wc3270", &instdir, &mydesktop, &myappdata, NULL,
		    &commonappdata, &is_installed) < 0)
	    	x3270_exit(1);
	if (sockstart())
	    	x3270_exit(1);
#endif /*]*/

	add_resource("keymap.base",
#if defined(_WIN32) /*[*/
	    base_keymap
#else /*][*/
	    xs_buffer("%s%s%s", base_keymap1, base_keymap2, base_keymap3)
#endif /*]*/
	    );
	add_resource("keymap.base.3270", NewString(base_3270_keymap));

	argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

	printf("%s\n\n"
		"Copyright 1989-2014 by Paul Mattes, GTRC and others.\n"
		"Type 'show copyright' for full copyright information.\n"
		"Type 'help' for help information.\n\n",
		build);

#if defined(_WIN32) /*[*/
	/* Delete the link file, if we've been told do. */
	delenv = getenv(DELENV);
	if (delenv != NULL) {
		unlink(delenv);
		putenv(DELENV "=");
	}

	/* Check for auto-shortcut mode. */
	if (appres.auto_shortcut) {
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

	/* Get the screen set up as early as possible. */
	screen_init();

	kybd_init();
	idle_init();
	keymap_init();
	hostfile_init();
	nvt_init();

	sms_init();

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

	register_schange(ST_CONNECT, main_connect);
	register_schange(ST_3270_MODE, main_connect);
        register_schange(ST_EXITING, main_exiting);
	register_actions(main_actions, array_count(main_actions));
	ft_init();
	pr3287_session_init();
	xio_init();
	print_screen_init();
	keypad_init();
	toggles_init();
	menubar_init();
	scroll_init();
	help_init();

#if !defined(_WIN32) /*[*/
	/* Make sure we don't fall over any SIGPIPEs. */
	(void) signal(SIGPIPE, SIG_IGN);

	/* Make sure we can collect child exit status. */
	(void) signal(SIGCHLD, sigchld_handler);
#endif /*]*/

	/* Handle initial toggle settings. */
	initialize_toggles();
	icmd_init();

#if defined(HAVE_LIBSSL) /*[*/
	/* Initialize SSL and ask for the password, if needed. */
	ssl_base_init(NULL, NULL);
#endif /*]*/

	if (cl_hostname != NULL) {
		pause_for_errors();
		/* Connect to the host. */
		appres.once = True;
		if (host_connect(cl_hostname) < 0)
			x3270_exit(1);
		/* Wait for negotiations to complete or fail. */
		while (!IN_NVT && !IN_3270) {
			(void) process_events(True);
			if (!PCONNECTED)
				x3270_exit(1);
			if (escaped) {
			    	printf("Connection aborted.\n");
				x3270_exit(1);
			}
		}
		pause_for_errors();
		screen_disp(False);
	} else {
	    	/* Drop to the prompt. */
		appres.once = False;
		if (!appres.secure) {
			interact();
			screen_disp(False);
		} else {
			pause_for_errors();
			screen_resume();
		}
	}
	peer_script_init();

	/* Process events forever. */
	while (1) {
		if (!escaped || ft_state != FT_NONE) {
			(void) process_events(True);
		}
		if (appres.cbreak_mode && escape_pending) {
			escape_pending = False;
			screen_suspend();
		}
		if (!appres.secure && !CONNECTED && !appres.reconnect) {
			screen_suspend();
			(void) printf("Disconnected.\n");
			if (appres.once)
				x3270_exit(0);
			interact();
			screen_resume();
		} else if (escaped && ft_state == FT_NONE) {
			interact();
			vtrace("Done interacting.\n");
			screen_resume();
		} else if (!CONNECTED &&
			   !appres.reconnect &&
			   cl_hostname != NULL) {
			screen_suspend();
			x3270_exit(0);
		}

#if !defined(_WIN32) /*[*/
		if (children && (pid = waitpid(-1, &status, WNOHANG)) > 0) {
			pr3287_session_check(pid, status);
			--children;
		}
#else /*][*/
		pr3287_session_check();
#endif /*]*/
		screen_disp(False);
	}
}

#if !defined(_WIN32) /*[*/
/*
 * SIGTSTP handler for use while a command is running.  Sets a flag so that
 * c3270 will stop before the next prompt is printed.
 */
static void
running_sigtstp_handler(int ignored _is_unused)
{
	signal(SIGTSTP, SIG_IGN);
	stop_pending = True;
}

/*
 * SIGTSTP haandler for use while the prompt is being displayed.
 * Acts immediately by setting SIGTSTP to the default and sending it to
 * ourselves, but also sets a flag so that the user gets one free empty line
 * of input before resuming the connection.
 */
static void
prompt_sigtstp_handler(int ignored _is_unused)
{
	if (CONNECTED)
		dont_return = True;
	signal(SIGTSTP, SIG_DFL);
	kill(getpid(), SIGTSTP);
}
#endif /*]*/

/*static*/ void
interact(void)
{
	/* In case we got here because a command output, stop the pager. */
	stop_pager();

	vtrace("Interacting.\n");
	if (appres.secure) {
		char s[10];

		printf("[Press <Enter>] ");
		fflush(stdout);
		if (fgets(s, sizeof(s), stdin) == NULL)
		    	x3270_exit(1);
		return;
	}

#if !defined(_WIN32) /*[*/
	/* Handle SIGTSTP differently at the prompt. */
	signal(SIGTSTP, SIG_DFL);
#endif /*]*/

	/*
	 * Ignore SIGINT at the prompt.
	 * I'm sure there's more we could do.
	 */
	signal(SIGINT, SIG_IGN);

	for (;;) {
		int sl;
		char *s;
#if defined(HAVE_LIBREADLINE) /*[*/
		char *rl_s;
#else /*][*/
		char buf[1024];
#endif /*]*/

		dont_return = False;

		/* Process a pending stop now. */
		if (stop_pending) {
			stop_pending = False;
#if !defined(_WIN32) /*[*/
			signal(SIGTSTP, SIG_DFL);
			kill(getpid(), SIGTSTP);
#endif /*]*/
			continue;
		}

#if !defined(_WIN32) /*[*/
		/* Process SIGTSTPs immediately. */
		signal(SIGTSTP, prompt_sigtstp_handler);
#endif /*]*/
		/* Display the prompt. */
		if (CONNECTED)
		    	(void) printf("Press <Enter> to resume session.\n");
#if defined(HAVE_LIBREADLINE) /*[*/
		s = rl_s = readline("c3270> ");
		if (s == NULL) {
			printf("\n");
			exit(0);
		}
#else /*][*/
		(void) printf(PROGRAM_NAME "> ");
		(void) fflush(stdout);

		/* Get the command, and trim white space. */
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			printf("\n");
#if defined(_WIN32) /*[*/
		    	continue;
#else /*][*/
			x3270_exit(0);
#endif /*]*/
		}
		s = buf;
#endif /*]*/
#if !defined(_WIN32) /*[*/
		/* Defer SIGTSTP until the next prompt display. */
		signal(SIGTSTP, running_sigtstp_handler);
#endif /*]*/

		while (isspace(*s))
			s++;
		sl = strlen(s);
		while (sl && isspace(s[sl-1]))
			s[--sl] = '\0';

		/* A null command means go back. */
		if (!sl) {
			if (CONNECTED && !dont_return)
				break;
			else
				continue;
		}

#if defined(HAVE_LIBREADLINE) /*[*/
		/* Save this command in the history buffer. */
		add_history(s);
#endif /*]*/

		/* "?" is an alias for "Help". */
		if (!strcmp(s, "?"))
			s = "Help";

		/*
		 * Process the command like a macro, and spin until it
		 * completes.
		 */
		push_command(s);
		while (sms_active()) {
			(void) process_events(True);
		}

		/* Close the pager. */
		stop_pager();

#if defined(HAVE_LIBREADLINE) /*[*/
		/* Give back readline's buffer. */
		free(rl_s);
#endif /*]*/

		/* If it succeeded, return to the session. */
		if (!macro_output && CONNECTED)
			break;
	}

	/* Ignore SIGTSTP again. */
	stop_pending = False;
#if !defined(_WIN32) /*[*/
	signal(SIGTSTP, SIG_IGN);
#endif /*]*/
#if defined(_WIN32) /*[*/
	signal(SIGINT, SIG_DFL);
#endif /*]*/
}

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

	if (pager != NULL)
		return pager;

	if ((pager_env = getenv("PAGER")) != NULL)
		pager_cmd = pager_env;
	else if (strlen(lesspath))
		pager_cmd = lesscmd;
	else if (strlen(morepath))
		pager_cmd = morepath;
	if (pager_cmd != NULL) {
		char *s;

		s = Malloc(strlen(pager_cmd) + strlen(or_cat) + 1);
		(void) sprintf(s, "%s%s", pager_cmd, or_cat);
		pager = popen(s, "w");
		Free(s);
		if (pager == NULL)
			(void) perror(pager_cmd);
	}
	if (pager == NULL)
		pager = stdout;
	return pager;
#else /*][*/
	return stdout;
#endif /*]*/
}

/* Stop the pager. */
static void
stop_pager(void)
{
#if !defined(_WIN32) /*[*/
	if (pager != NULL) {
		if (pager != stdout)
			pclose(pager);
		pager = NULL;
	}
#else /*][*/
	pager_rowcnt = 0;
	pager_q = False;
#endif /*]*/
}

#if defined(_WIN32) /*[*/
void
pager_output(const char *s)
{
    	if (pager_q)
	    	return;

	do {
		char *nl;
	    	int sl;

		/* Pause for a screenful. */
		if (pager_rowcnt >= maxROWS) {
			printf("Press any key to continue . . . ");
			fflush(stdout);
			pager_q = screen_wait_for_key(NULL);
			printf("\r                                \r");
			pager_rowcnt = 0;
			if (pager_q)
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
			printf("%.*s\n", sl, s);
			s = nl + 1;
		} else {
			printf("%s\n", s);
			sl = strlen(s);
			s = NULL;
		}

		/* Account for the newline. */
		pager_rowcnt++;

		/* Account (conservatively) for any line wrap. */
		pager_rowcnt += sl / maxCOLS;

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
	static char buf[128];

	(void) time(&t);

	td = t - ts;
	hr = (long)(td / 3600);
	mn = (td % 3600) / 60;
	sc = td % 60;

	if (hr > 0)
		(void) sprintf(buf, "%ld %s %ld %s %ld %s",
		    hr, (hr == 1) ?
			get_message("hour") : get_message("hours"),
		    mn, (mn == 1) ?
			get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));
	else if (mn > 0)
		(void) sprintf(buf, "%ld %s %ld %s",
		    mn, (mn == 1) ?
			get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));
	else
		(void) sprintf(buf, "%ld %s",
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));

	return buf;
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
	    (appres.extended && !std_ds_host)? get_message("extendedDs"):
		get_message("standardDs"));
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
    if (appres.key_map) {
	action_output("%s %s", get_message("keyboardMap"), appres.key_map);
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
#if defined(LOCAL_PROCESS) /*[*/
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
	if (secure_connection) {
	    action_output("  %s%s%s", get_message("secure"),
			secure_unverified? ", ": "",
			secure_unverified? get_message("unverified"): "");
	    if (secure_unverified) {
		int i;

		for (i = 0; unverified_reasons[i] != NULL; i++) {
		    action_output("   %s", unverified_reasons[i]);
		}
	    }
	}
#endif /*]*/
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
	action_output("Copyright (c) 1993-2014, Paul Mattes.");
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

static Boolean
Show_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Show", ia, argc, argv);
    if (check_argc("Show", argc, 0, 1) < 0) {
	return False;
    }

    if (argc == 0) {
	action_output("  Show copyright   copyright information");
	action_output("  Show stats       connection statistics");
	action_output("  Show status      same as 'Show stats'");
	action_output("  Show keymap      current keymap");
	return True;
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
	return False;
    }
    return True;
}

/* Trace([data|keyboard][on [filename]|off]) */
static Boolean
Trace_action(ia_t ia, unsigned argc, const char **argv)
{
    Boolean on = False;
    unsigned arg0 = 0;

    action_debug("Trace", ia, argc, argv);

    if (argc == 0) {
	if (toggled(TRACING) && tracefile_name != NULL) {
	    action_output("Trace file is %s.", tracefile_name);
	} else {
	    action_output("Tracing is %sabled.",
		    toggled(TRACING)? "en": "dis");
	}
	return True;
    }

    if (!strcasecmp(argv[0], "Data") || !strcasecmp(argv[0], "Keyboard")) {
	/* Skip. */
	arg0++;
    }
    if (!strcasecmp(argv[arg0], "Off")) {
	on = False;
	arg0++;
	if (argc > arg0) {
	    popup_an_error("Trace: Too many arguments for 'Off'");
	    return False;
	}
	if (!toggled(TRACING)) {
	    return True;
	}
    } else if (!strcasecmp(argv[arg0], "On")) {
	on = True;
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
	    return False;
	}
    } else {
	popup_an_error("Trace: Parameter must be On or Off");
	return False;
    }

    if ((on && !toggled(TRACING)) || (!on && toggled(TRACING))) {
	do_toggle(TRACING);
	if (!on) {
	    action_output("Tracing stopped.");
	}
    }

    if (tracefile_name != NULL) {
	if (ia_cause == IA_COMMAND) {
	    action_output("Trace file is %s.", tracefile_name);
	} else {
	    popup_an_info("Trace file is %s.", tracefile_name);
	}
    }
    return True;
}

/*
 * ScreenTrace(On)
 * ScreenTrace(On,filename)			 backwards-compatible
 * ScreenTrace(On,File,filename)		 preferred
 * ScreenTrace(On,Printer)
 * ScreenTrace(On,Printer,"print command")	 Unix
 * ScreenTrace(On,Printer[,Gdi|WordPad],printername) Windows
 * ScreenTrace(Off)
 */
static Boolean
ScreenTrace_action(ia_t ia, unsigned argc, const char **argv)
{
    Boolean on = False;
#if defined(_WIN32) /*[*/
    Boolean is_file = False;
#endif /*]*/
    tss_t how = TSS_FILE;
    ptype_t ptype = P_TEXT;
    const char *name = NULL;
    unsigned px;

    action_debug("ScreenTrace", ia, argc, argv);

    if (argc == 0) {
	how = trace_get_screentrace_how();
	if (toggled(SCREEN_TRACE)) {
	    action_output("Screen tracing is enabled, %s \"%s\".",
		    (how == TSS_FILE)? "file":
#if !defined(_WIN32) /*[*/
		    "with print command",
#else /*]*/
		    "to printer",
#endif /*]*/
		    trace_get_screentrace_name());
	} else {
	    action_output("Screen tracing is disabled.");
	}
	return True;
    }

    if (!strcasecmp(argv[0], "Off")) {
	if (!toggled(SCREEN_TRACE)) {
	    popup_an_error("Screen tracing is already disabled.");
	    return False;
	}
	on = False;
	if (argc > 1) {
	    popup_an_error("ScreenTrace(): Too many arguments for 'Off'");
	    return False;
	}
	goto toggle_it;
    }
    if (strcasecmp(argv[0], "On")) {
	popup_an_error("ScreenTrace(): Must be 'On' or 'Off'");
	return False;
    }

    /* Process 'On'. */
    if (toggled(SCREEN_TRACE)) {
	popup_an_error("Screen tracing is already enabled.");
	return True;
    }

    on = True;
    px = 1;

    if (px >= argc) {
	/*
	 * No more parameters. Trace to a file, and generate the name.
	 */
	goto toggle_it;
    }
    if (!strcasecmp(argv[px], "File")) {
	px++;
#if defined(_WIN32) /*[*/
	is_file = True;
#endif /*]*/
    } else if (!strcasecmp(argv[px], "Printer")) {
	px++;
	how = TSS_PRINTER;
#if defined(WIN32) /*[*/
	ptype = P_GDI;
#endif /*]*/
    }
#if defined(_WIN32) /*[*/
    if (px < argc && !strcasecmp(argv[px], "Gdi")) {
	if (is_file) {
	    popup_an_error("ScreenTrace(): Cannot specify 'File' and 'Gdi'.");
	    return False;
	}
	px++;
	how = TSS_PRINTER;
	ptype = P_GDI;
    } else if (px < argc && !strcasecmp(argv[px], "WordPad")) {
	if (is_file) {
	    popup_an_error("ScreenTrace(): Cannot specify 'File' and "
		    "'WordPad'.");
	    return False;
	}
	px++;
	how = TSS_PRINTER;
	ptype = P_RTF;
    }
#endif /*]*/
    if (px < argc) {
	name = argv[px];
	px++;
    }
    if (px < argc) {
	popup_an_error("ScreenTrace(): Too many arguments.");
	return False;
    }
    if (how == TSS_PRINTER && name == NULL) {
#if !defined(_WIN32) /*[*/
	name = get_resource(ResPrintTextCommand);
#else /*][*/
	name = get_resource(ResPrinterName);
#endif /*]*/
    }

toggle_it:
    if ((on && !toggled(SCREEN_TRACE)) || (!on && toggled(SCREEN_TRACE))) {
	if (on) {
	    trace_set_screentrace_file(how, ptype, name);
	}
	do_toggle(SCREEN_TRACE);
    }
    if (on && !toggled(SCREEN_TRACE)) {
	return True;
    }

    name = trace_get_screentrace_name();
    if (name != NULL) {
	if (on) {
	    if (how == TSS_FILE) {
		if (ia_cause == IA_COMMAND) {
		    action_output("Trace file is %s.", name);
		} else {
		    popup_an_info("Trace file is %s.", name);
		}
	    } else {
		if (ia_cause == IA_COMMAND) {
		    action_output("Tracing to printer.");
		} else {
		    popup_an_info("Tracing to printer.");
		}
	    }
	} else {
	    if (trace_get_screentrace_last_how() == TSS_FILE) {
		if (ia_cause == IA_COMMAND) {
		    action_output("Tracing complete. Trace file is %s.", name);
		} else {
		    popup_an_info("Tracing complete. Trace file is %s.", name);
		}
	    } else {
		if (ia_cause == IA_COMMAND) {
		    action_output("Tracing to printer complete.");
		} else {
		    popup_an_info("Tracing to printer complete.");
		}
	    }
	}
    }
    return True;
}

/* Break to the command prompt. */
static Boolean
Escape_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Escape", ia, argc, argv);
    if (check_argc("Escape", argc, 0, 0) < 0) {
	return False;
    }

    if (!appres.secure) {
	host_cancel_reconnect();
	screen_suspend();
#if 0 /* this fix is in there for something, but I don't know what */
	abort_script();
#endif
    }
    return True;
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
	while (sl && vmsgbuf[sl - 1] == '\n')
		vmsgbuf[--sl] = '\0';

	/* Push it out. */
	if (sl) {
		if (escaped) {
			printf("%s\n", vmsgbuf);
			fflush(stdout);
		} else {
			char *s;

			while ((s = strchr(vmsgbuf, '\n')) != NULL)
				*s = ' ';
			status_push(vmsgbuf);
		}
	}
}

static Boolean
Info_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Info", ia, argc, argv);

    if (!argc) {
	return True;
    }

    popup_an_info("%s", argv[0]);
    return True;
}

static Boolean
ignore_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("ignore", ia, argc, argv);
    return True;
}

#if !defined(_WIN32) /*[*/

/* Support for c3270 profiles. */

#define PROFILE_ENV	"C3270PRO"
#define NO_PROFILE_ENV	"NOC3270PRO"
#define DEFAULT_PROFILE	"~/.c3270pro"

/* Read in the .c3270pro file. */
Boolean
merge_profile(void)
{
    const char *fname;
    char *profile_name;
    Boolean did_read = False;

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
    did_read = read_resource_file(profile_name, False);
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
	    	fprintf(stderr, "%s: Error %ld\n", profile_path,
			GetLastError());
		x3270_exit(1);
	}
	sprintf(args, "+S \"%s\"", sesspath);
	cwd = getcwd(NULL, 0);
	hres = create_shortcut(&s,		/* session */
			       exepath,		/* .exe    */
			       linkpath,	/* .lnk    */
			       args,		/* args    */
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
	if ((int)h <= 32) {
	    fprintf(stderr, "ShellExecute failed, error %d\n", (int)h);
	    x3270_exit(1);
	}

#if defined(AS_DEBUG) /*[*/
	printf("Started ShellLink\n");
	fflush(stdout);
#endif /*]*/

	exit(0);
}
#endif /*]*/
