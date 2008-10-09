/*
 * Copyright 2000-2008 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * c3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	Help.c
 *		Help information for c3270.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include "actionsc.h"
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
	{ "Abort",	CN, P_SCRIPTING, "Abort pending scripts and macros" },
	{ "AnsiText",	CN, P_SCRIPTING, "Dump pending NVT text" },
	{ "Ascii",	CN, P_SCRIPTING, "Screen contents in ASCII" },
	{ "Ascii",	"<n>", P_SCRIPTING, "<n> bytes of screen contents from cursor, in ASCII" },
	{ "Ascii",	"<row> <col> <n>", P_SCRIPTING, "<n> bytes of screen contents from <row>,<col>, in ASCII" },
	{ "Ascii",	"<row> <col> <rows> <cols>", P_SCRIPTING, "<rows>x<cols> of screen contents from <row>,<col>, in ASCII" },
	{ "AsciiField", CN, P_SCRIPTING, "Contents of current field, in ASCII" },
	{ "Attn", CN, P_3270, "Send 3270 ATTN sequence (TELNET IP)" },
	{ "BackSpace", CN, P_3270, "Move cursor left" },
	{ "BackTab", CN, P_3270, "Move to previous field" },
	{ "CircumNot", CN, P_3270, "Send ~ in NVT mode, \254 in 3270 mode" },
	{ "Clear", CN, P_3270, "Send CLEAR AID (clear screen)" },
	{ "Close", CN, P_INTERACTIVE, "Alias for 'Disconnect'" },
	{ "CloseScript", CN, P_SCRIPTING, "Exit peer script" },
	{ "Connect", "[<lu>@]<host>[:<port>]", P_INTERACTIVE, "Open connection to <host>" },
#if defined(LOCAL_PROCESS) /*[*/
	{ "Connect", "-e [<command> [<arg>...]]", P_INTERACTIVE, "Open connection to a local shell or command" },
#endif /*]*/
	{ "ContinueScript", CN, P_SCRIPTING, "Resume paused script" },
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
	{ "Execute", "<command>", P_SCRIPTING, "Execute a shell command" },
	{ "Expect", "<pattern>", P_SCRIPTING, "Wait for NVT output" },
	{ "FieldEnd", CN, P_3270, "Move to end of field" },
	{ "FieldMark", CN, P_3270, "3270 FIELD MARK key (X'1E')" },
#if 0
	{ "Flip", CN, P_3270, "Flip display left-to-right" },
#endif
	{ "Help", "all|interactive|3270|scripting|transfer|<cmd>", P_INTERACTIVE, "Get help" },
	{ "HexString", "<digits>", P_3270|P_SCRIPTING, "Input field data in hex" },
	{ "Home", CN, P_3270, "Move cursor to first field" },
	{ "ignore", CN, P_3270, "Do nothing" },
	{ "Insert", CN, P_3270, "Set 3270 insert mode" },
	{ "Key", "<symbol>|0x<nn>", P_3270, "Input one character" },
	{ "Left", CN, P_3270, "Move cursr left" },
	{ "Left2", CN, P_3270, "Move cursor left 2 columns" },
	{ "MonoCase", CN, P_3270, "Toggle monocase mode" },
	{ "MoveCursor", "<row> <col>", P_3270|P_SCRIPTING, "Move cursor to specific location" },
	{ "Newline", CN, P_3270, "Move cursor to first field in next row" },
	{ "NextWord", CN, P_3270, "Move cursor to next word" },
	{ "Open", CN, P_INTERACTIVE, "Alias for 'Connect'" },
	{ "PA", "<n>", P_3270, "Send 3270 Program Attention" },
#if defined(WC3270) /*[*/
	{ "Paste", CN, P_3270, "Paste clipboard contents" },
#endif /*]*/
	{ "PauseScript", CN, P_SCRIPTING, "Pause script until ResumeScript" },
	{ "PF", "<n>", P_3270, "Send 3270 PF AID" },
	{ "PreviousWord", CN, P_3270, "Move cursor to previous word" },
	{ "Printer", "Start[,lu]|Stop", P_3270|P_SCRIPTING|P_INTERACTIVE,
#if defined(WC3270) /*[*/
	    "Start or stop wpr3287 printer session" },
#else /*][*/
	    "Start or stop pr3287 printer session" },
#endif /*]*/
	{ "Quit", CN, P_INTERACTIVE, "Exit " PROGRAM },
	{ "Redraw", CN, P_INTERACTIVE|P_3270, "Redraw screen" },
	{ "Reset", CN, P_3270, "Clear keyboard lock" },
	{ "Right", CN, P_3270, "Move cursor right" },
	{ "Right2", CN, P_3270, "Move cursor right 2 columns" },
	{ "Script", "<path> [<arg>...]", P_SCRIPTING, "Run a child script" },
	{ "Show", CN, P_INTERACTIVE, "Display status and settings" },
	{ "Snap", "<args>", P_SCRIPTING, "Screen snapshot manipulation" },
	{ "String", "<text>", P_3270|P_SCRIPTING, "Input a string" },
	{ "SysReq", CN, P_3270, "Send 3270 Attention (TELNET ABORT or SYSREQ AID)" },
	{ "Tab", CN, P_3270, "Move cursor to next field" },
	{ "Toggle", "<toggle-name> [set|clear]", P_INTERACTIVE|P_SCRIPTING,
	    "Change a toggle" },
	{ "ToggleInsert", CN, P_3270, "Set or clear 3270 insert mode" },
#if 0
	{ "ToggleReverse", CN, P_3270, "Set or clear reverse-input mode" },
#endif
	{ "Trace", "[data|keyboard]on|off [<file>]", P_INTERACTIVE, "Configure tracing" },
	{ "Transfer", "<args>", P_INTERACTIVE, "IND$FILE file transfer" },
	{ "Up", CN, P_3270, "Move cursor up" },
	{ "Wait", "<args>", P_SCRIPTING, "Wait for host events" },
	{ CN,  CN, 0, CN }
};

static const char *options_help[] = {
	"Command-line options:",
	"  " OptCharset " <name>",
	"    Use EBCDIC character set <name>",
	"  " OptClear " <toggle>",
	"    Turn off the specified <toggle> option",
	"  " OptDsTrace "",
	"    Turn on data stream tracing",
	"  " OptHostsFile " <file>",
	"    Use <file> as the hosts file",
	"  " OptKeymap " <file>",
	"    Use the keymap in <file>",
	"  " OptModel " <n>",
	"    Emulate a 327x model <n>",
	"  " OptMono "",
	"    Emulate a monochrome 3278, even if the terminal can display color",
	"  " OptOversize " <cols>x<rows>",
	"    Make the screen oversized to <cols>x<rows>",
	"  " OptSet " <toggle>",
	"    Turn on the specified <toggle> option",
	"  " OptTermName " <name>",
	"    Send <name> as the TELNET terminal name",
	"  -xrm \"" PROGRAM ".<resourcename>: <value>\"",
	"    Set a resource value",
	NULL
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
} help_subcommand[] = {
	{ "all",		
#if defined(X3270_SCRIPT) /*[*/
	    			-1,
#else /*][*/
				~P_SCRIPTING,
#endif /*]*/
						CN, NULL },
	{ "3270",		P_3270,		CN, NULL },
	{ "interactive",	P_INTERACTIVE,	CN, NULL },
	{ "options",		P_OPTIONS,	CN, options_help },
#if defined(X3270_SCRIPT) /*[*/
	{ "scripting",		P_SCRIPTING,	CN, NULL },
#endif /*]*/
#if defined(X3270_FT) /*[*/
	{ "file-transfer",	P_TRANSFER,	CN, ft_help },
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
			action_output(help_subcommand[overall].text);
			return;
		}
		if (help_subcommand[overall].block != NULL) {
			int j;

			for (j = 0;
			     help_subcommand[overall].block[j] != CN;
			     j++) {
				action_output(help_subcommand[overall].block[j]);
			}
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
