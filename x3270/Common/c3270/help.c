/*
 * Copyright (c) 2000-2009, Paul Mattes.
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
 *	Help.c
 *		Help information for c3270.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include "actionsc.h"
#include "gluec.h"
#include "popupsc.h"
#include "screenc.h"

#define P_3270		0x0001	/* 3270 actions */
#define P_SCRIPTING	0x0002	/* scripting actions */
#define P_INTERACTIVE	0x0004	/* interactive (command-prompt) actions */
#define P_OPTIONS	0x0008	/* command-line options */
#define P_TRANSFER	0x0010	/* file transfer options */

#if defined(WC3270) /*[*/
#define PROGRAM	"wc3270"
#else /*][*/
#define PROGRAM	"c3270"
#endif /*]*/

static struct {
	const char *name;
	const char *args;
	int purpose;
	const char *help;
} cmd_help[] = {
#if defined(X3270_SCRIPT) /*[*/
	{ "Abort",	CN, P_SCRIPTING, "Abort pending scripts and macros" },
	{ "AnsiText",	CN, P_SCRIPTING, "Dump pending NVT text" },
#endif /*]*/
	{ "Ascii",	CN, P_SCRIPTING, "Screen contents in ASCII" },
	{ "Ascii",	"<n>", P_SCRIPTING, "<n> bytes of screen contents from cursor, in ASCII" },
	{ "Ascii",	"<row> <col> <n>", P_SCRIPTING, "<n> bytes of screen contents from <row>,<col>, in ASCII" },
	{ "Ascii",	"<row> <col> <rows> <cols>", P_SCRIPTING, "<rows>x<cols> of screen contents from <row>,<col>, in ASCII" },
	{ "AsciiField", CN, P_SCRIPTING, "Contents of current field, in ASCII" },
	{ "Attn", CN, P_3270, "Send 3270 ATTN sequence (TELNET IP)" },
	{ "BackSpace", CN, P_3270, "Move cursor left" },
	{ "BackTab", CN, P_3270, "Move to previous field" },
#if defined(X3270_SCRIPT) /*[*/
	{ "Bell", CN, P_SCRIPTING, "Ring the terminal bell" },
#endif /*]*/
	{ "CircumNot", CN, P_3270, "Send ~ in NVT mode, \254 in 3270 mode" },
	{ "Clear", CN, P_3270, "Send CLEAR AID (clear screen)" },
	{ "Close", CN, P_INTERACTIVE, "Alias for 'Disconnect'" },
#if defined(X3270_SCRIPT) /*[*/
	{ "CloseScript", CN, P_SCRIPTING, "Exit peer script" },
#endif /*]*/
	{ "Compose", CN, P_INTERACTIVE, "Interpret next two keystrokes according to the compose map" },
	{ "Connect", "[<lu>@]<host>[:<port>]", P_INTERACTIVE, "Open connection to <host>" },
#if defined(LOCAL_PROCESS) /*[*/
	{ "Connect", "-e [<command> [<arg>...]]", P_INTERACTIVE, "Open connection to a local shell or command" },
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
	{ "ContinueScript", CN, P_SCRIPTING, "Resume paused script" },
#endif /*]*/
	{ "CursorSelect", CN, P_3270, "Light pen select at cursor location" },
	{ "Delete", CN, P_3270, "Delete character at cursor" },
	{ "DeleteField", CN, P_3270, "Erase field at cursor location (^U)" },
	{ "DeleteWord", CN, P_3270, "Erase word before cursor location (^W)" },
	{ "Disconnect", CN, P_INTERACTIVE, "Close connection to host" },
	{ "Down", CN, P_3270, "Move cursor down" },
	{ "Dup", CN, P_3270, "3270 DUP key (X'1C')" },
	{ "Ebcdic",	CN, P_SCRIPTING, "Screen contents in EBCDIC" },
	{ "Ebcdic",	"<n>", P_SCRIPTING, "<n> bytes of screen contents from cursor, in EBCDIC" },
	{ "Ebcdic",	"<row> <col> <n>", P_SCRIPTING, "<n> bytes of screen contents from <row>,<col>, in EBCDIC" },
	{ "Ebcdic",	"<row> <col> <rows> <cols>", P_SCRIPTING, "<rows>x<cols> of screen contents from <row>,<col>, in EBCDIC" },
	{ "EbcdicField", CN, P_SCRIPTING, "Contents of current field, in EBCDIC" },
	{ "Enter", CN, P_3270, "Send ENTER AID" },
	{ "Erase", CN, P_3270, "Destructive backspace" },
	{ "EraseEOF", CN, P_3270, "Erase from cursor to end of field" },
	{ "EraseInput", CN, P_3270, "Erase all input fields" },
	{ "Escape", CN, P_INTERACTIVE, "Escape to "
#if defined(WC3270) /*[*/
	                                           "'wc3270>'"
#else /*][*/
	                                           "'c3270>'"
#endif /*]*/
						               " prompt" },
#if defined(X3270_SCRIPT) /*[*/
	{ "Execute", "<command>", P_SCRIPTING, "Execute a shell command" },
#endif /*]*/
	{ "Exit", CN, P_INTERACTIVE, "Exit "
#if defined(WC3270) /*[*/
	                                    "wc3270"
#else /*][*/
	                                    "c3270"
#endif /*]*/
					    	    },
#if defined(X3270_SCRIPT) /*[*/
	{ "Expect", "<pattern>", P_SCRIPTING, "Wait for NVT output" },
#endif /*]*/
	{ "FieldEnd", CN, P_3270, "Move to end of field" },
	{ "FieldMark", CN, P_3270, "3270 FIELD MARK key (X'1E')" },
	{ "Flip", CN, P_3270, "Flip display left-to-right" },
	{ "Help", "all|interactive|3270|scripting|transfer|<cmd>", P_INTERACTIVE, "Get help" },
	{ "HexString", "<digits>", P_3270|P_SCRIPTING, "Input field data in hex" },
	{ "Home", CN, P_3270, "Move cursor to first field" },
	{ "ignore", CN, P_3270, "Do nothing" },
	{ "Info", "<text>", P_SCRIPTING|P_INTERACTIVE, "Display text in OIA" },
	{ "Insert", CN, P_3270, "Set 3270 insert mode" },
	{ "Interrupt", CN, P_3270, "In NVT mode, send IAC IP" },
	{ "Key", "<symbol>|0x<nn>", P_3270, "Input one character" },
	{ "Left", CN, P_3270, "Move cursr left" },
	{ "Left2", CN, P_3270, "Move cursor left 2 columns" },
#if defined(X3270_SCRIPT) /*[*/
	{ "Macro", "<name>", P_SCRIPTING, "Execute a predefined macro" },
#endif /*]*/
	{ "MonoCase", CN, P_3270, "Toggle monocase mode" },
	{ "MoveCursor", "<row> <col>", P_3270|P_SCRIPTING, "Move cursor to specific location" },
	{ "Newline", CN, P_3270, "Move cursor to first field in next row" },
	{ "NextWord", CN, P_3270, "Move cursor to next word" },
	{ "Open", CN, P_INTERACTIVE, "Alias for 'Connect'" },
	{ "PA", "<n>", P_3270, "Send 3270 Program Attention" },
#if defined(WC3270) /*[*/
	{ "Paste", CN, P_3270, "Paste clipboard contents" },
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
	{ "PauseScript", CN, P_SCRIPTING, "Pause script until ResumeScript" },
#endif /*]*/
	{ "PF", "<n>", P_3270, "Send 3270 PF AID" },
	{ "PreviousWord", CN, P_3270, "Move cursor to previous word" },
	{ "Printer", "Start[,lu]|Stop", P_3270|P_SCRIPTING|P_INTERACTIVE,
#if defined(WC3270) /*[*/
	    "Start or stop wpr3287 printer session" },
#else /*][*/
	    "Start or stop pr3287 printer session" },
#endif /*]*/
        { "PrintText",
#if defined(WC3270) /*[*/
	              "<printer>",
#else /*][*/
	              "<print-command>",
#endif /*]*/
		                         P_SCRIPTING|P_INTERACTIVE,
	    "Dump screen image to printer" },
#if defined(X3270_SCRIPT) /*[*/
        { "Query", "<keyword>", P_SCRIPTING|P_INTERACTIVE,
	    "Query operational parameters" },
#endif /*]*/
	{ "Quit", CN, P_INTERACTIVE, "Exit " PROGRAM },
#if defined(X3270_SCRIPT) /*[*/
        { "ReadBuffer", "ASCII|EBCDIC", P_SCRIPTING, "Dump display buffer" },
#endif /*]*/
	{ "Redraw", CN, P_INTERACTIVE|P_3270, "Redraw screen" },
	{ "Reset", CN, P_3270, "Clear keyboard lock" },
	{ "Right", CN, P_3270, "Move cursor right" },
	{ "Right2", CN, P_3270, "Move cursor right 2 columns" },
#if defined(X3270_TRACE) /*[*/
	{ "ScreenTrace", "on [<file>]|off", P_INTERACTIVE, "Configure screen tracing" },
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
	{ "Script", "<path> [<arg>...]", P_SCRIPTING, "Run a child script" },
#endif /*]*/
	{ "Show", CN, P_INTERACTIVE, "Display status and settings" },
#if defined(X3270_SCRIPT) /*[*/
	{ "Snap", "<args>", P_SCRIPTING, "Screen snapshot manipulation" },
#endif /*]*/
        { "Source", "<file>", P_SCRIPTING|P_INTERACTIVE, "Read actions from file" },
	{ "String", "<text>", P_3270|P_SCRIPTING, "Input a string" },
	{ "SysReq", CN, P_3270, "Send 3270 Attention (TELNET ABORT or SYSREQ AID)" },
	{ "Tab", CN, P_3270, "Move cursor to next field" },
#if defined(WC3270) /*[*/
	{ "Title", "<text>", P_SCRIPTING|P_INTERACTIVE, "Change window title" },
#endif /*]*/
	{ "Toggle", "<toggle-name> [set|clear]", P_INTERACTIVE|P_SCRIPTING,
	    "Change a toggle" },
	{ "ToggleInsert", CN, P_3270, "Set or clear 3270 insert mode" },
	{ "ToggleReverse", CN, P_3270, "Set or clear reverse-input mode" },
#if defined(X3270_TRACE) /*[*/
	{ "Trace", "[data|keyboard] on [<file>]|off", P_INTERACTIVE, "Configure tracing" },
#endif /*]*/
	{ "Transfer", "[<args>]", P_INTERACTIVE, "IND$FILE file transfer (see 'help file-transfer')" },
	{ "Up", CN, P_3270, "Move cursor up" },
#if defined(X3270_SCRIPT) /*[*/
	{ "Wait", "<args>", P_SCRIPTING, "Wait for host events" },
#endif /*]*/
	{ CN,  CN, 0, CN }
};

static const char *ft_help[] = {
	"Syntax:",
	"  To be prompted interactively for parameters:",
	"    Transfer",
	"  To specify parameters on the command line:",
	"    Transfer <keyword>=<value>...",
	"Keywords:",
	"  Direction=send|receive               default 'receive'",
	"  HostFile=<path>                      required",
	"  LocalFile=<path>                     required",
	"  Host=tso|vm                          default 'tso'",
	"  Mode=ascii|binary                    default 'ascii'",
	"  Cr=remove|add|keep                   default 'remove'",
	"  Remap=yes|no                         default 'yes'",
	"  Exist=keep|replace|append            default 'keep'",
	"  Recfm=fixed|variable|undefined       for Direction=send",
	"  Lrecl=<n>                            for Direction=send",
	"  Blksize=<n>                          for Direction=send Host=tso",
	"  Allocation=tracks|cylinders|avblock  for Direction=send Host=tso",
	"  PrimarySpace=<n>                     for Direction=send Host=tso",
	"  SecondarySpace=<n>                   for Direction=send Host=tso",
	"Note that to embed a space in a value, you must quote the keyword, e.g.:",
	"  Transfer Direction=send LocalFile=/tmp/foo \"HostFile=foo text a\" Host=vm",
	NULL
};

static struct {
	const char *name;
	int flag;
	const char *text;
	const char **block;
	void (*fn)(Boolean);
} help_subcommand[] = {
	{ "all",		
#if defined(X3270_SCRIPT) /*[*/
	    			-1,
#else /*][*/
				~P_SCRIPTING,
#endif /*]*/
						CN, NULL, NULL },
	{ "3270",		P_3270,		CN, NULL, NULL },
	{ "interactive",	P_INTERACTIVE,	CN, NULL, NULL },
	{ "options",		P_OPTIONS,	CN, NULL, &cmdline_help },
#if defined(X3270_SCRIPT) /*[*/
	{ "scripting",		P_SCRIPTING,	CN, NULL, NULL },
#endif /*]*/
#if defined(X3270_FT) /*[*/
	{ "file-transfer",	P_TRANSFER,	CN, ft_help, NULL },
#endif /*]*/
	{ CN, 0, CN }
};

/* c3270-specific actions. */
void
Help_action(Widget w _is_unused, XEvent *event _is_unused, String *params,
    Cardinal *num_params)
{
	int i;
	int overall = -1;
	int match = 0;

	action_debug(Help_action, event, params, num_params);

	if (*num_params != 1) {
		action_output(
"  help all           all commands\n"
"  help 3270          3270 commands\n"
"  help interactive   interactive (command-prompt) commands\n"
"  help <command>     help for one <command>\n"
"  help options       command-line options\n"
#if defined(X3270_SCRIPT) /*[*/
"  help scripting     scripting commands\n"
#endif /*]*/
#if defined(X3270_FT) /*[*/
"  help file-transfer file transfer options\n"
		);
#endif /*]*/
		return;
	}

	if (!strcmp(params[0], "verify")) {
		int j;
		Boolean any = False;

		for (i = 0; cmd_help[i].name; i++) {
		    	Boolean found = False;

			for (j = 0; j < actioncount; j++) {
				if (!strcasecmp(cmd_help[i].name,
					        actions[j].string)) {
					found = True;
					break;
				}
			}
			if (!found) {
			    	action_output("Help for nonexistent action: %s",
					cmd_help[i].name);
				any = True;
			}
		}
		if (!any)
		    	action_output("No orphaned help messages.");
		any = False;
		for (j = 0; j < actioncount; j++) {
		    	Boolean found = False;

			for (i = 0; cmd_help[i].name; i++) {

				if (!strcasecmp(cmd_help[i].name,
					        actions[j].string)) {
					found = True;
					break;
				}
			}
			if (!found) {
			    	action_output("No Help for %s",
					actions[j].string);
				any = True;
			}
		}
		if (!any)
		    	printf("No orphaned actions.\n");
		return;
	}

	for (i = 0; help_subcommand[i].name != CN; i++) {
		if (!strncasecmp(help_subcommand[i].name, params[0],
		    strlen(params[0]))) {
			match = help_subcommand[i].flag;
			overall = i;
			break;
		}
	}
	if (match) {
		for (i = 0; cmd_help[i].name != CN; i++) {
			if (!strncasecmp(cmd_help[i].name, params[0],
			    strlen(params[0]))) {
				action_output("Ambiguous: matches '%s' and "
				    "one or more commands",
				    help_subcommand[overall].name);
				return;
			}
		}
		if (help_subcommand[overall].text != CN) {
			action_output("%s", help_subcommand[overall].text);
			return;
		}
		if (help_subcommand[overall].block != NULL) {
			int j;

			for (j = 0;
			     help_subcommand[overall].block[j] != CN;
			     j++) {
				action_output("%s",
					help_subcommand[overall].block[j]);
			}
			return;
		}
		if (help_subcommand[overall].fn != NULL) {
		    	(*help_subcommand[overall].fn)(True);
			return;
		}
		for (i = 0; cmd_help[i].name != CN; i++) {
			if (cmd_help[i].purpose & match) {
				action_output("  %s %s\n    %s",
				    cmd_help[i].name,
				    cmd_help[i].args? cmd_help[i].args: "",
				    cmd_help[i].help? cmd_help[i].help: "");
			}
		}
	} else {
		Boolean any = False;

		for (i = 0; cmd_help[i].name != CN; i++) {
#if !defined(X3270_SCRIPT) /*[*/
			if (cmd_help[i].purpose == P_SCRIPTING)
				continue;
#endif /*]*/
			if (!strncasecmp(cmd_help[i].name, params[0],
			    strlen(params[0]))) {
				action_output("  %s %s\n    %s",
				    cmd_help[i].name,
				    cmd_help[i].args? cmd_help[i].args: "",
				    cmd_help[i].help? cmd_help[i].help: "");
				any = True;
			}
		}
		if (!any)
			action_output("No such command: %s", params[0]);
	}
}
