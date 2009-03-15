/*
 * Copyright (c) 1993-2009, Paul Mattes.
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
#include <stdarg.h>
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
#include "popupsc.h"
#include "printerc.h"
#include "screenc.h"
#include "statusc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
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
#include <windows.h>
#include "winversc.h"
#include "windirsc.h"
#endif /*]*/

#if defined(_WIN32) /*[*/
# define PROGRAM_NAME	"wc3270"
#else /*][*/
# define PROGRAM_NAME	"c3270"
#endif /*]*/

static void interact(void);
static void stop_pager(void);

#if defined(HAVE_LIBREADLINE) /*[*/
static CPPFunction attempted_completion;
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
/* Base keymap. */
static char *base_keymap1 =
"Ctrl<Key>]: Escape\n"
"Ctrl<Key>a Ctrl<Key>a: Key(0x01)\n"
"Ctrl<Key>a Ctrl<Key>]: Key(0x1d)\n"
"Ctrl<Key>a <Key>c: Clear\n"
"Ctrl<Key>a <Key>e: Escape\n"
"Ctrl<Key>a <Key>i: Insert\n"
"Ctrl<Key>a <Key>r: Reset\n"
"Ctrl<Key>a <Key>l: Redraw\n"
"Ctrl<Key>a <Key>m: Compose\n"
"Ctrl<Key>a <Key>^: Key(notsign)\n"
"<Key>DC: Delete\n"
"<Key>UP: Up\n"
"<Key>DOWN: Down\n"
"<Key>LEFT: Left\n"
"<Key>RIGHT: Right\n"
"<Key>HOME: Home\n"
"Ctrl<Key>a <Key>1: PA(1)\n"
"Ctrl<Key>a <Key>2: PA(2)\n"
"Ctrl<Key>a <Key>3: PA(3)\n";
static char *base_keymap2 =
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
"Ctrl<Key>a <Key>F12: PF(24)\n";

/* Base keymap, 3270 mode. */
static char *base_3270_keymap =
"Ctrl<Key>a <Key>a: Attn\n"
"Ctrl<Key>c: Clear\n"
"Ctrl<Key>d: Dup\n"
"Ctrl<Key>f: FieldMark\n"
"Ctrl<Key>h: Erase\n"
"Ctrl<Key>i: Tab\n"
"Ctrl<Key>j: Newline\n"
"Ctrl<Key>l: Redraw\n"
"Ctrl<Key>m: Enter\n"
"Ctrl<Key>r: Reset\n"
"Ctrl<Key>u: DeleteField\n"
"<Key>IC: ToggleInsert\n"
"<Key>DC: Delete\n"
"<Key>BACKSPACE: Erase\n"
"<Key>HOME: Home\n"
"<Key>END: FieldEnd\n";

#else /*][*/

/* Base keymap. */
static char *base_keymap =
       "Alt <Key>1:      PA(1)\n"
       "Alt <Key>2:      PA(2)\n"
       "Alt <Key>3:      PA(3)\n"
       "Alt <Key>^:      Key(notsign)\n"
       "Alt <Key>c:      Clear\n"
       "Alt <Key>C:      Clear\n"
       "Alt <Key>l:      Redraw\n"
       "Alt <Key>L:      Redraw\n"
       "Alt <Key>m:      Compose\n"
       "Alt <Key>M:      Compose\n"
       "Alt <Key>p:      PrintText\n"
       "Alt <Key>P:      PrintText\n"
      "Ctrl <Key>]:      Escape\n"
     "Shift <Key>F1:     PF(13)\n"
     "Shift <Key>F2:     PF(14)\n"
     "Shift <Key>F3:     PF(15)\n"
     "Shift <Key>F4:     PF(16)\n"
     "Shift <Key>F5:     PF(17)\n"
     "Shift <Key>F6:     PF(18)\n"
     "Shift <Key>F7:     PF(19)\n"
     "Shift <Key>F8:     PF(20)\n"
     "Shift <Key>F9:     PF(21)\n"
     "Shift <Key>F10:    PF(22)\n"
     "Shift <Key>F11:    PF(23)\n"
     "Shift <Key>F12:    PF(24)\n"
     "Shift <Key>ESCAPE: Key(0x1d)\n";

/* Base keymap, 3270 mode. */
static char *base_3270_keymap =
      "Ctrl <Key>a:      Attn\n"
       "Alt <Key>a:      Attn\n"
       "Alt <Key>A:      Attn\n"
      "Ctrl <Key>c:      Clear\n"
      "Ctrl <Key>d:      Dup\n"
       "Alt <Key>d:      Dup\n"
       "Alt <Key>D:      Dup\n"
      "Ctrl <Key>f:      FieldMark\n"
       "Alt <Key>f:      FieldMark\n"
       "Alt <Key>F:      FieldMark\n"
      "Ctrl <Key>h:      Erase\n"
       "Alt <Key>i:      Insert\n"
       "Alt <Key>I:      Insert\n"
      "Ctrl <Key>i:      Tab\n"
      "Ctrl <Key>j:      Newline\n"
      "Ctrl <Key>l:      Redraw\n"
      "Ctrl <Key>m:      Enter\n"
      "Ctrl <Key>r:      Reset\n"
       "Alt <Key>r:      Reset\n"
       "Alt <Key>R:      Reset\n"
      "Ctrl <Key>u:      DeleteField\n"
      "Ctrl <Key>v:      Paste\n"
           "<Key>INSERT: ToggleInsert\n"
     "Shift <Key>TAB:    BackTab\n"
           "<Key>BACK:   Erase\n"
     "Shift <Key>END:    EraseEOF\n"
           "<Key>END:    FieldEnd\n"
     "Shift <Key>LEFT:   PreviousWord\n"
     "Shift <Key>RIGHT:  NextWord\n";
#endif /*]*/

Boolean any_error_output = False;
Boolean escape_pending = False;
Boolean stop_pending = False;
Boolean dont_return = False;

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
	if (get_dirs(NULL, myappdata, "wc3270") < 0)
	    	x3270_exit(1);

	/*
	 * Create the application data directory, in case we are being run
	 * by someone other than the user that installed us.
	 */
	(void) _mkdir(myappdata);
}
#endif /*]*/

/* Callback for connection state changes. */
static void
main_connect(Boolean ignored)
{       
	if (CONNECTED || appres.disconnect_clear) {
#if defined(C3270_80_132) /*[*/
		if (appres.altscreen != CN)
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
		screen_suspend();
} 

/* Make sure error messages are seen. */
static void
pause_for_errors(void)
{
	char s[10];

	if (any_error_output) {
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
	const char	*cl_hostname = CN;

#if defined(_WIN32) /*[*/
	(void) get_version_info();
	(void) save_dirs(argv[0]);
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
		"Copyright 1989-2009 by Paul Mattes, GTRC and others.\n"
		"Type 'show copyright' for full copyright information.\n"
		"Type 'help' for help information.\n\n",
		build);

	if (charset_init(appres.charset) != CS_OKAY) {
		xs_warning("Cannot find charset \"%s\"", appres.charset);
		(void) charset_init(CN);
	}
	action_init();

#if defined(HAVE_LIBREADLINE) /*[*/
	/* Set up readline. */
	rl_readline_name = "c3270";
	rl_initialize();
	rl_attempted_completion_function = attempted_completion;
#if defined(RL_READLINE_VERSION) /*[*/
	rl_completion_entry_function = completion_entry;
#else /*][*/
	rl_completion_entry_function = (Function *)completion_entry;
#endif /*]*/
#endif /*]*/

	/*
	 * If any error messages have been displayed, make sure the
	 * user sees them -- before screen_init() clears the screen.
	 */
	pause_for_errors();
	screen_init();

	kybd_init();
	idle_init();
	keymap_init();
	hostfile_init();
	hostfile_init();
	ansi_init();

	sms_init();

	register_schange(ST_CONNECT, main_connect);
        register_schange(ST_3270_MODE, main_connect);
        register_schange(ST_EXITING, main_exiting);
#if defined(X3270_FT) /*[*/
	ft_init();
#endif /*]*/
#if defined(X3270_PRINTER) /*[*/
	printer_init();
#endif /*]*/

#if !defined(_WIN32) /*[*/
	/* Make sure we don't fall over any SIGPIPEs. */
	(void) signal(SIGPIPE, SIG_IGN);

	/* Make sure we can collect child exit status. */
	(void) signal(SIGCHLD, sigchld_handler);
#endif /*]*/

	/* Handle initial toggle settings. */
#if defined(X3270_TRACE) /*[*/
	if (!appres.debug_tracing) {
		appres.toggle[DS_TRACE].value = False;
		appres.toggle[EVENT_TRACE].value = False;
	}
#endif /*]*/
	initialize_toggles();

#if 0
	/* Can't have a prompt and aut-reconnect. */
	if (appres.reconnect && !appres.no_prompt) {
	    	Warning("Must have no-prompt mode for reconnect");
		appres.reconnect = False;
	}
#endif

	/* Connect to the host. */
	screen_suspend();
	if (cl_hostname != CN) {
		appres.once = True;
		if (host_connect(cl_hostname) < 0)
			x3270_exit(1);
		/* Wait for negotiations to complete or fail. */
		while (!IN_ANSI && !IN_3270) {
			(void) process_events(True);
			if (!PCONNECTED)
				x3270_exit(1);
		}
		pause_for_errors();
	} else {
		if (appres.secure) {
			Error("Must specify hostname with secure option");
		}
		appres.once = False;
		if (!appres.no_prompt)
			interact();
	}
	screen_resume();
	screen_disp(False);
	peer_script_init();

	/* Process events forever. */
	while (1) {
		if (!escaped
#if defined(X3270_FT) /*[*/
			|| ft_state != FT_NONE
#endif /*]*/
			)
			(void) process_events(True);
		if (appres.cbreak_mode && escape_pending) {
			escape_pending = False;
			screen_suspend();
		}
		if (!appres.no_prompt) {
			if (!CONNECTED && !appres.reconnect) {
				screen_suspend();
				(void) printf("Disconnected.\n");
				if (appres.once)
					x3270_exit(0);
				interact();
				screen_resume();
			} else if (escaped
#if defined(X3270_FT) /*[*/
				    && ft_state == FT_NONE
#endif /*]*/
				    ) {
				interact();
				trace_event("Done interacting.\n");
				screen_resume();
			}
		} else if (!CONNECTED &&
			   !appres.reconnect &&
			   !appres.no_prompt) {
			screen_suspend();
			x3270_exit(0);
		}

#if !defined(_WIN32) /*[*/
		if (children && waitpid(0, (int *)0, WNOHANG) > 0)
			--children;
#else /*][*/
		printer_check();
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

	trace_event("Interacting.\n");
	if (appres.secure || appres.no_prompt) {
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
		if (s == CN) {
			printf("\n");
			exit(0);
		}
#else /*][*/
		(void) printf(PROGRAM_NAME "> ");
		(void) fflush(stdout);

		/* Get the command, and trim white space. */
		if (fgets(buf, sizeof(buf), stdin) == CN) {
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
	char *pager_cmd = CN;

	if (pager != NULL)
		return pager;

	if ((pager_env = getenv("PAGER")) != CN)
		pager_cmd = pager_env;
	else if (strlen(lesspath))
		pager_cmd = lesscmd;
	else if (strlen(morepath))
		pager_cmd = morepath;
	if (pager_cmd != CN) {
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
			pager_q = screen_wait_for_key();
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
		if (nl != CN) {
		    	sl = nl - s;
			printf("%.*s\n", sl, s);
			s = nl + 1;
		} else {
			printf("%s\n", s);
			sl = strlen(s);
			s = CN;
		}

		/* Account for the newline. */
		pager_rowcnt++;

		/* Account (conservatively) for any line wrap. */
		pager_rowcnt += sl / maxCOLS;

	} while (s != CN);
}
#endif /*]*/

#if defined(HAVE_LIBREADLINE) /*[*/

static char **matches = (char **)NULL;
static char **next_match;

/* Generate a match list. */
static char **
attempted_completion(const char *text, int start, int end)
{
	char *s;
	int i, j;
	int match_count;

	/* If this is not the first word, fail. */
	s = rl_line_buffer;
	while (*s && isspace(*s))
		s++;
	if (s - rl_line_buffer < start) {
		char *t = s;
		struct host *h;

		/*
		 * If the first word is 'Connect' or 'Open', and the
		 * completion is on the second word, expand from the
		 * hostname list.
		 */

		/* See if we're in the second word. */
		while (*t && !isspace(*t))
			t++;
		while (*t && isspace(*t))
			t++;
		if (t - rl_line_buffer < start)
			return NULL;

		/*
		 * See if the first word is 'Open' or 'Connect'.  In future,
		 * we might do other expansions, and this code would need to
		 * be generalized.
		 */
		if (!((!strncasecmp(s, "Open", 4) && isspace(*(s + 4))) ||
		      (!strncasecmp(s, "Connect", 7) && isspace(*(s + 7)))))
			return NULL;

		/* Look for any matches.  Note that these are case-sensitive. */
		for (h = hosts, match_count = 0; h; h = h->next) {
			if (!strncmp(h->name, t, strlen(t)))
				match_count++;
		}
		if (!match_count)
			return NULL;

		/* Allocate the return array. */
		next_match = matches =
		    Malloc((match_count + 1) * sizeof(char **));

		/* Scan again for matches to fill in the array. */
		for (h = hosts, j = 0; h; h = h->next) {
			int skip = 0;

			if (strncmp(h->name, t, strlen(t)))
				continue;

			/*
			 * Skip hostsfile entries that are duplicates of
			 * RECENT entries we've already recorded.
			 */
			if (h->entry_type != RECENT) {
				for (i = 0; i < j; i++) {
					if (!strcmp(matches[i],
					    h->name)) {
						skip = 1;
						break;
					}
				}
			}
			if (skip)
				continue;

			/*
			 * If the string contains spaces, put it in double
			 * quotes.  Otherwise, just copy it.  (Yes, this code
			 * is fairly stupid, and can be fooled by other
			 * whitespace and embedded double quotes.)
			 */
			if (strchr(h->name, ' ') != CN) {
				matches[j] = Malloc(strlen(h->name) + 3);
				(void) sprintf(matches[j], "\"%s\"", h->name);
				j++;
			} else {
				matches[j++] = NewString(h->name);
			}
		}
		matches[j] = CN;
		return NULL;
	}

	/* Search for matches. */
	for (i = 0, match_count = 0; i < actioncount; i++) {
		if (!strncasecmp(actions[i].string, s, strlen(s)))
			match_count++;
	}
	if (!match_count)
		return NULL;

	/* Return what we got. */
	next_match = matches = Malloc((match_count + 1) * sizeof(char **));
	for (i = 0, j = 0; i < actioncount; i++) {
		if (!strncasecmp(actions[i].string, s, strlen(s))) {
			matches[j++] = NewString(actions[i].string);
		}
	}
	matches[j] = CN;
	return NULL;
}

/* Return the match list. */
static char *
completion_entry(const char *text, int state)
{
	char *r;

	if (next_match == NULL)
		return CN;

	if ((r = *next_match++) == CN) {
		Free(matches);
		next_match = matches = NULL;
		return CN;
	} else
		return r;
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
	hr = td / 3600;
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
#if defined(X3270_TN3270E) /*[*/
	const char *eopts;
	const char *bplu;
#endif /*]*/
	const char *ptype;
	extern int linemode; /* XXX */
	extern time_t ns_time;
	extern int ns_bsent, ns_rsent, ns_brcvd, ns_rrcvd;

	action_output("%s", build);
	action_output("%s %s: %d %s x %d %s, %s, %s",
	    get_message("model"), model_name,
	    maxCOLS, get_message("columns"),
	    maxROWS, get_message("rows"),
	    appres.m3279? get_message("fullColor"): get_message("mono"),
	    (appres.extended && !std_ds_host) ? get_message("extendedDs") :
		get_message("standardDs"));
	action_output("%s %s", get_message("terminalName"), termtype);
	if (connected_lu != CN && connected_lu[0])
		action_output("%s %s", get_message("luName"), connected_lu);
#if defined(X3270_TN3270E) /*[*/
	bplu = net_query_bind_plu_name();
	if (bplu != CN && bplu[0])
	    	action_output("%s %s", get_message("bindPluName"), bplu);
#endif /*]*/
	action_output("%s %s (%s)", get_message("characterSet"),
	    get_charset_name(),
#if defined(X3270_DBCS) /*[*/
	    dbcs? "DBCS": "SBCS"
#else /*][*/
	    "SBCS"
#endif /*]*/
	    );
	action_output("%s %s",
		get_message("hostCodePage"),
		get_host_codepage());
	action_output("%s GCSGID %u, CPGID %u",
		get_message("sbcsCgcsgid"),
		(unsigned short)((cgcsgid >> 16) & 0xffff),
		(unsigned short)(cgcsgid & 0xffff));
#if defined(X3270_DBCS) /*[*/
	if (dbcs)
		action_output("%s GCSGID %u, CPGID %u",
			get_message("dbcsCgcsgid"),
			(unsigned short)((cgcsgid_dbcs >> 16) & 0xffff),
			(unsigned short)(cgcsgid_dbcs & 0xffff));
#endif /*]*/
#if !defined(_WIN32) /*[*/
	action_output("%s %s", get_message("localeCodeset"), locale_codeset);
	action_output("%s DBCS %s, wide curses %s",
		get_message("buildOpts"),
#if defined(X3270_DBCS) /*[*/
		get_message("buildEnabled"),
#else /*][*/
		get_message("buildDisabled"),
#endif /*]*/
#if defined(CURSES_WIDE) /*[*/
		get_message("buildEnabled")
#else /*][*/
		get_message("buildDisabled")
#endif /*]*/
		);
#else /*][*/
	action_output("%s OEM %d ANSI %d", get_message("windowsCodePage"),
		windows_cp, GetACP());
#endif /*]*/
	if (appres.key_map) {
		action_output("%s %s", get_message("keyboardMap"),
		    appres.key_map);
	}
	if (CONNECTED) {
		action_output("%s %s",
		    get_message("connectedTo"),
#if defined(LOCAL_PROCESS) /*[*/
		    (local_process && !strlen(current_host))? "(shell)":
#endif /*]*/
			current_host);
#if defined(LOCAL_PROCESS) /*[*/
		if (!local_process) {
#endif /*]*/
			action_output("  %s %d", get_message("port"),
			    current_port);
#if defined(LOCAL_PROCESS) /*[*/
		}
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
		if (secure_connection)
			action_output("  %s", get_message("secure"));
#endif /*]*/
		ptype = net_proxy_type();
		if (ptype) {
		    	action_output("  %s %s  %s %s  %s %s",
				get_message("proxyType"), ptype,
				get_message("server"), net_proxy_host(),
				get_message("port"), net_proxy_port());
		}
		ts = hms(ns_time);
		if (IN_E)
			emode = "TN3270E ";
		else
			emode = "";
		if (IN_ANSI) {
			if (linemode)
				ftype = get_message("lineMode");
			else
				ftype = get_message("charMode");
			action_output("  %s%s, %s", emode, ftype, ts);
		} else if (IN_SSCP) {
			action_output("  %s%s, %s", emode,
			    get_message("sscpMode"), ts);
		} else if (IN_3270) {
			action_output("  %s%s, %s", emode,
			    get_message("dsMode"), ts);
		} else
			action_output("  %s", ts);

#if defined(X3270_TN3270E) /*[*/
		eopts = tn3270e_current_opts();
		if (eopts != CN) {
			action_output("  %s %s", get_message("tn3270eOpts"),
			    eopts);
		} else if (IN_E) {
			action_output("  %s", get_message("tn3270eNoOpts"));
		}
#endif /*]*/

		if (IN_3270)
			action_output("%s %d %s, %d %s\n%s %d %s, %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ?
				get_message("byte") : get_message("bytes"),
			    ns_rsent, (ns_rsent == 1) ?
				get_message("record") : get_message("records"),
			    get_message("Received"),
			    ns_brcvd, (ns_brcvd == 1) ?
				get_message("byte") : get_message("bytes"),
			    ns_rrcvd, (ns_rrcvd == 1) ?
				get_message("record") : get_message("records"));
		else
			action_output("%s %d %s, %s %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ?
				get_message("byte") : get_message("bytes"),
			    get_message("received"),
			    ns_brcvd, (ns_brcvd == 1) ?
				get_message("byte") : get_message("bytes"));

#if defined(X3270_ANSI) /*[*/
		if (IN_ANSI) {
			struct ctl_char *c = net_linemode_chars();
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
				s += sprintf(s, "  %s %s", c[i].name,
				    c[i].value);
			}
			if (s != buf) {
				*s = '\0';
				action_output("%s", buf);
			}
		}
#endif /*]*/
	} else if (HALF_CONNECTED) {
		action_output("%s %s", get_message("connectionPending"),
		    current_host);
	} else
		action_output("%s", get_message("notConnected"));
}

static void
copyright_dump(void)
{
	action_output(" ");
	action_output("Copyright (c) 1993-2009, Paul Mattes.");
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

void
Show_action(Widget w _is_unused, XEvent *event _is_unused, String *params,
    Cardinal *num_params)
{
	action_debug(Show_action, event, params, num_params);
	if (*num_params == 0) {
		action_output("  Show copyright   copyright information");
		action_output("  Show stats       connection statistics");
		action_output("  Show status      same as 'Show stats'");
		action_output("  Show keymap      current keymap");
		return;
	}
	if (!strncasecmp(params[0], "stats", strlen(params[0])) ||
	    !strncasecmp(params[0], "status", strlen(params[0]))) {
		status_dump();
	} else if (!strncasecmp(params[0], "keymap", strlen(params[0]))) {
		keymap_dump();
	} else if (!strncasecmp(params[0], "copyright", strlen(params[0]))) {
		copyright_dump();
	} else
		popup_an_error("Unknown 'Show' keyword");
}

#if defined(X3270_TRACE) /*[*/
/* Trace([data|keyboard][on[filename]|off]) */
void
Trace_action(Widget w _is_unused, XEvent *event _is_unused, String *params,
    Cardinal *num_params)
{
	int tg = 0;
	Boolean both = False;
	Boolean on = False;

	action_debug(Trace_action, event, params, num_params);
	if (*num_params == 0) {
		action_output("Data tracing is %sabled.",
		    toggled(DS_TRACE)? "en": "dis");
		action_output("Keyboard tracing is %sabled.",
		    toggled(EVENT_TRACE)? "en": "dis");
		return;
	}
	if (!strcasecmp(params[0], "Data"))
		tg = DS_TRACE;
	else if (!strcasecmp(params[0], "Keyboard"))
		tg = EVENT_TRACE;
	else if (!strcasecmp(params[0], "Off")) {
		both = True;
		on = False;
		if (*num_params > 1) {
			popup_an_error("Trace(): Too many arguments for 'Off'");
			return;
		}
	} else if (!strcasecmp(params[0], "On")) {
		both = True;
		on = True;
	} else {
		popup_an_error("Trace(): Unknown trace type -- "
		    "must be Data or Keyboard");
		return;
	}
	if (!both) {
		if (*num_params == 1 || !strcasecmp(params[1], "On"))
			on = True;
		else if (!strcasecmp(params[1], "Off")) {
			on = False;
			if (*num_params > 2) {
				popup_an_error("Trace(): Too many arguments "
				    "for 'Off'");
				return;
			}
		} else {
			popup_an_error("Trace(): Must be 'On' or 'Off'");
			return;
		}
	}

	if (both) {
		if (on && *num_params > 1)
			appres.trace_file = NewString(params[1]);
		if ((on && !toggled(DS_TRACE)) || (!on && toggled(DS_TRACE)))
			do_toggle(DS_TRACE);
		if ((on && !toggled(EVENT_TRACE)) ||
		    (!on && toggled(EVENT_TRACE)))
			do_toggle(EVENT_TRACE);
	} else if ((on && !toggled(tg)) || (!on && toggled(tg))) {
		if (on && *num_params > 2)
			appres.trace_file = NewString(params[2]);
		do_toggle(tg);
	}
	if (tracefile_name != NULL)
		action_output("Trace file is %s", tracefile_name);
}
#endif /*]*/

/* Break to the command prompt. */
void
Escape_action(Widget w _is_unused, XEvent *event _is_unused, String *params _is_unused,
    Cardinal *num_params _is_unused)
{
	action_debug(Escape_action, event, params, num_params);
	if (!appres.secure && !appres.no_prompt) {
	    	host_cancel_reconnect();
		screen_suspend();
	}
}

/* Popup an informational message. */
void
popup_an_info(const char *fmt, ...)
{
    	va_list args;
	static char vmsgbuf[4096];
	char *s, *t;
	Boolean quoted = False;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);

	/* Filter out the junk. */
	for (s = t = vmsgbuf; *s; s++) {
	    if (*s == '\n') {
		*t = '\0';
		break;
	    } else if (!quoted && *s == '\\') {
		quoted = True;
	    } else if (*s >= ' ' && *s <= '~') {
		*t++ = *s;
		quoted = False;
	    }
	}
	*t = '\0';

	if (strlen(vmsgbuf))
		status_push(vmsgbuf);
}

void
Info_action(Widget w _is_unused, XEvent *event _is_unused, String *params,
	Cardinal *num_params)
{
	action_debug(Info_action, event, params, num_params);

    	if (!*num_params)
	    	return;

	popup_an_info("%s", params[0]);
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
	if (getenv(NO_PROFILE_ENV) != CN)
		return did_read;

	/* Read the file. */
	fname = getenv(PROFILE_ENV);
	if (fname == CN || *fname == '\0')
		fname = DEFAULT_PROFILE;
	profile_name = do_subst(fname, True, True);
	did_read = (read_resource_file(profile_name, False) >= 0);
	Free(profile_name);
	return did_read;
}

#endif /*]*/
