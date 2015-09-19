/*
 * Copyright (c) 2000-2009, 2013-2015 Paul Mattes.
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

#include "actions.h"
#include "glue.h"
#include "help.h"
#include "icmdc.h"
#include "popups.h"
#include "screen.h"
#include "utils.h"

#if defined(_WIN32) /*[*/
# include "wc3270.h"
#endif /*]*/

#define P_3270		0x0001	/* 3270 actions */
#define P_SCRIPTING	0x0002	/* scripting actions */
#define P_INTERACTIVE	0x0004	/* interactive (command-prompt) actions */
#define P_OPTIONS	0x0008	/* command-line options */
#define P_TRANSFER	0x0010	/* file transfer options */
#define P_HTML		0x0020	/* HTML help */

#if defined(WC3270) /*[*/
#define HELP_W	"w"
#else /*][*/
#define HELP_W	""
#endif /*]*/

static struct {
	const char *name;
	const char *args;
	int purpose;
	const char *help;
} cmd_help[] = {
	{ "Abort",	NULL, P_SCRIPTING, "Abort pending scripts and macros" },
	{ "AnsiText",	NULL, P_SCRIPTING, "Dump pending NVT text" },
	{ "Ascii",	NULL, P_SCRIPTING, "Screen contents in ASCII" },
	{ "Ascii",	"<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from cursor, in ASCII" },
	{ "Ascii",	"<row> <col> <n>", P_SCRIPTING,
	    "<n> bytes of screen contents from <row>,<col>, in ASCII" },
	{ "Ascii",	"<row> <col> <rows> <cols>", P_SCRIPTING,
	    "<rows>x<cols> of screen contents from <row>,<col>, in ASCII" },
	{ "AsciiField", NULL, P_SCRIPTING,
	    "Contents of current field, in ASCII" },
	{ "Attn", NULL, P_3270, "Send 3270 ATTN sequence (TELNET IP)" },
	{ "BackSpace", NULL, P_3270, "Move cursor left" },
	{ "BackTab", NULL, P_3270, "Move to previous field" },
	{ "Bell", NULL, P_SCRIPTING, "Ring the terminal bell" },
	{ "CircumNot", NULL, P_3270,
	    "Send ~ in NVT mode, notsign (X'5F', U+00AC) in 3270 mode" },
	{ "Clear", NULL, P_3270, "Send CLEAR AID (clear screen)" },
	{ "Close", NULL, P_INTERACTIVE, "Alias for 'Disconnect'" },
	{ "CloseScript", NULL, P_SCRIPTING, "Exit peer script" },
	{ "Compose", NULL, P_INTERACTIVE,
	    "Interpret next two keystrokes according to the compose map" },
	{ "Connect", "[<lu>@]<host>[:<port>]", P_INTERACTIVE,
	    "Open connection to <host>" },
#if defined(LOCAL_PROCESS) /*[*/
	{ "Connect", "-e [<command> [<arg>...]]", P_INTERACTIVE,
	    "Open connection to a local shell or command" },
#endif /*]*/
	{ "ContinueScript", "<result>", P_SCRIPTING, "Resume paused script" },
#if defined(WC3270) /*[*/
	{ "Copy", NULL, P_3270, "Copy selected text to Windows clipboard" },
#endif /*]*/
	{ "CursorSelect", NULL, P_3270,
	    "Light pen select at cursor location" },
#if defined(WC3270) /*[*/
	{ "Cut", NULL, P_3270,
	    "Copy selected text to Windows clipboard, then erase" },
#endif /*]*/
	{ "Delete", NULL, P_3270, "Delete character at cursor" },
	{ "DeleteField", NULL, P_3270, "Erase field at cursor location (^U)" },
	{ "DeleteWord", NULL, P_3270,
	    "Erase word before cursor location (^W)" },
	{ "Disconnect", NULL, P_INTERACTIVE, "Close connection to host" },
	{ "Down", NULL, P_3270, "Move cursor down" },
	{ "Dup", NULL, P_3270, "3270 DUP key (X'1C')" },
	{ "Ebcdic",	NULL, P_SCRIPTING, "Screen contents in EBCDIC" },
	{ "Ebcdic",	"<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from cursor, in EBCDIC" },
	{ "Ebcdic",	"<row> <col> <n>", P_SCRIPTING,
	    "<n> bytes of screen contents from <row>,<col>, in EBCDIC" },
	{ "Ebcdic",	"<row> <col> <rows> <cols>", P_SCRIPTING,
	    "<rows>x<cols> of screen contents from <row>,<col>, in EBCDIC" },
	{ "EbcdicField", NULL, P_SCRIPTING,
	    "Contents of current field, in EBCDIC" },
	{ "Enter", NULL, P_3270, "Send ENTER AID" },
	{ "Erase", NULL, P_3270, "Destructive backspace" },
	{ "EraseEOF", NULL, P_3270, "Erase from cursor to end of field" },
	{ "EraseInput", NULL, P_3270, "Erase all input fields" },
	{ "Escape", NULL, P_INTERACTIVE,
	    "Escape to '" HELP_W "c3270>' prompt" },
	{ "Execute", "<command>", P_SCRIPTING, "Execute a shell command" },
	{ "Exit", NULL, P_INTERACTIVE, "Exit " HELP_W "c3270" },
	{ "Expect", "<pattern>", P_SCRIPTING, "Wait for NVT output" },
	{ "FieldEnd", NULL, P_3270, "Move to end of field" },
	{ "FieldMark", NULL, P_3270, "3270 FIELD MARK key (X'1E')" },
	{ "Flip", NULL, P_3270, "Flip display left-to-right" },
	{ "Help", "all|interactive|3270|scripting|transfer|<cmd>",
	    P_INTERACTIVE, "Get help" },
	{ "HexString", "<digits>", P_3270|P_SCRIPTING,
	    "Input field data in hex" },
	{ "Home", NULL, P_3270, "Move cursor to first field" },
	{ "ignore", NULL, P_3270, "Do nothing" },
	{ "Info", "<text>", P_SCRIPTING|P_INTERACTIVE, "Display text in OIA" },
	{ "Insert", NULL, P_3270, "Set 3270 insert mode" },
	{ "Interrupt", NULL, P_3270, "In NVT mode, send IAC IP" },
	{ "Key", "<symbol>|0x<nn>", P_3270, "Input one character" },
	{ "Keymap", "[<keymap-name>]", P_SCRIPTING|P_INTERACTIVE,
	    "Push temporary keymap, or pop if none specified" },
	{ "Keypad", NULL, P_INTERACTIVE, "Pop up the 3270 keypad" },
	{ "Left", NULL, P_3270, "Move cursr left" },
	{ "Left2", NULL, P_3270, "Move cursor left 2 columns" },
	{ "Macro", "<name>", P_SCRIPTING, "Execute a predefined macro" },
	{ "Menu", NULL, P_INTERACTIVE, "Pop up the command menu" },
	{ "MonoCase", NULL, P_3270, "Toggle monocase mode" },
	{ "MoveCursor", "<row> <col>", P_3270|P_SCRIPTING,
	    "Move cursor to specific location" },
	{ "Newline", NULL, P_3270, "Move cursor to first field in next row" },
	{ "NextWord", NULL, P_3270, "Move cursor to next word" },
	{ "Open", NULL, P_INTERACTIVE, "Alias for 'Connect'" },
	{ "PA", "<n>", P_3270, "Send 3270 Program Attention" },
#if defined(WC3270) /*[*/
	{ "Paste", NULL, P_3270, "Paste clipboard contents" },
#endif /*]*/
	{ "PauseScript", NULL, P_SCRIPTING, "Pause script until ContinueScript" },
	{ "PF", "<n>", P_3270, "Send 3270 PF AID" },
	{ "PreviousWord", NULL, P_3270, "Move cursor to previous word" },
	{ "Printer", "Start[,lu]|Stop", P_3270|P_SCRIPTING|P_INTERACTIVE,
	    "Start or stop " HELP_W "pr3287 printer session" },
        { "PrintText",
	    "[Html] [Rtf] [Modi] [Caption <caption>] [Replace|Append] File <filename>",
	    P_INTERACTIVE|P_SCRIPTING,
	    "Save screen image in a file" },
        { "PrintText",
	    "[Modi] [Caption <caption>] "
#if defined(WC3270) /*[*/
	    "[Gdi|Wordpad] [NoDialog] [<printer-name>]",
#else /*][*/
	    "[<print-command>]",
#endif /*]*/
	    P_INTERACTIVE|P_SCRIPTING,
	    "Print screen image" },
        { "Query", "<keyword>", P_SCRIPTING|P_INTERACTIVE,
	    "Query operational parameters" },
	{ "Quit", NULL, P_INTERACTIVE, "Exit " HELP_W "3270" },
        { "ReadBuffer", "Ascii|Ebcdic", P_SCRIPTING, "Dump display buffer" },
	{ "Reconnect", NULL, P_INTERACTIVE, "Reconnect to previous host" },
	{ "Redraw", NULL, P_INTERACTIVE|P_3270, "Redraw screen" },
	{ "Reset", NULL, P_3270, "Clear keyboard lock" },
	{ "Right", NULL, P_3270, "Move cursor right" },
	{ "Right2", NULL, P_3270, "Move cursor right 2 columns" },
	{ "ScreenTrace", "On [[File] <filename>]",  P_INTERACTIVE,
	    "Save screen images to file" },
	{ "ScreenTrace",
# if defined(_WIN32) /*[*/
	    "On Printer [Gdi|WordPad] [<printer-name>]",
# else /*][*/
	    "On Printer [<print-command>]",
# endif /*]*/
	    P_INTERACTIVE, "Save screen images to printer" },
	{ "ScreenTrace", "Off", P_INTERACTIVE, "Stop saving screen images" },
	{ "Script", "<path> [<arg>...]", P_SCRIPTING, "Run a child script" },
	{ "Scroll", "Forward|Backward", P_INTERACTIVE, "Scroll screen" },
	{ "Show", "Copyright|Stats|Keymap", P_INTERACTIVE,
	    "Display status and settings" },
	{ "Snap", "<args>", P_SCRIPTING, "Screen snapshot manipulation" },
        { "Source", "<file>", P_SCRIPTING|P_INTERACTIVE, "Read actions from file" },
	{ "String", "<text>", P_3270|P_SCRIPTING, "Input a string" },
	{ "SysReq", NULL, P_3270,
	    "Send 3270 Attention (TELNET ABORT or SYSREQ AID)" },
	{ "Tab", NULL, P_3270, "Move cursor to next field" },
	{ "TemporaryKeymap", "[<keymap-name>]", P_SCRIPTING|P_INTERACTIVE,
	    "Alias for Keymap" },
#if defined(WC3270) /*[*/
	{ "Title", "<text>", P_SCRIPTING|P_INTERACTIVE, "Change window title" },
#endif /*]*/
	{ "Toggle", "<toggle-name> [Set|Clear]", P_INTERACTIVE|P_SCRIPTING,
	    "Change a toggle" },
	{ "ToggleInsert", NULL, P_3270, "Set or clear 3270 insert mode" },
	{ "ToggleReverse", NULL, P_3270, "Set or clear reverse-input mode" },
	{ "Trace", "On [<file>]|Off", P_INTERACTIVE, "Configure tracing" },
	{ "Transfer", "[<args>]", P_INTERACTIVE,
	    "IND$FILE file transfer (see 'help file-transfer')" },
	{ "Up", NULL, P_3270, "Move cursor up" },
	{ "Wait", "<args>", P_SCRIPTING, "Wait for host events" },
	{ NULL,  NULL, 0, NULL }
};

#if defined(WC3270) /*[*/
static void html_help(bool);
#endif /*]*/

static struct {
	const char *name;
	int flag;
	const char *text;
	const char **block;
	void (*fn)(bool);
} help_subcommand[] = {
	{ "all",		-1,		NULL, NULL, NULL },
	{ "3270",		P_3270,		NULL, NULL, NULL },
	{ "interactive",	P_INTERACTIVE,	NULL, NULL, NULL },
	{ "options",		P_OPTIONS,	NULL, NULL, &cmdline_help },
	{ "scripting",		P_SCRIPTING,	NULL, NULL, NULL },
	{ "file-transfer",	P_TRANSFER,	NULL, NULL, ft_help },
#if defined(WC3270) /*[*/
	{ "html",		P_HTML,		NULL, NULL, html_help },
#endif /*]*/
	{ NULL, 0, NULL }
};

/* c3270-specific actions. */
static bool
Help_action(ia_t ia, unsigned argc, const char **argv)
{
    int i;
    int overall = -1;
    int match = 0;
    bool any = false;

    action_debug("Help", ia, argc, argv);
    if (check_argc("Help", argc, 0, 1) < 0) {
	return false;
    }

    if (argc != 1) {
	action_output(
"  help all           all commands\n"
"  help 3270          3270 commands\n"
"  help interactive   interactive (command-prompt) commands\n"
"  help <command>     help for one <command>\n"
"  help options       command-line options\n"
"  help scripting     scripting commands\n"
"  help file-transfer file transfer options\n"
#if defined(WC3270) /*[*/
"  help html          display HTML help file\n"
#endif /*]*/
	);
	return true;
    }

    /* The (hidden) verify option verifies the integrity of the help list. */
    if (!strcmp(argv[0], "verify")) {
	action_elt_t *e;
	bool any = false;

	for (i = 0; cmd_help[i].name; i++) {
	    bool found = false;

	    FOREACH_LLIST(&actions_list, e, action_elt_t *) {
		if (!strcasecmp(cmd_help[i].name, e->t.name)) {
		    found = true;
		    break;
		}
	    } FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
	    if (!found) {
		action_output("Help for nonexistent action: %s",
			cmd_help[i].name);
		any = true;
	    }
	}
	if (!any) {
	    action_output("No orphaned help messages.");
	}
	any = false;
	FOREACH_LLIST(&actions_list, e, action_elt_t *) {
	    bool found = false;

	    for (i = 0; cmd_help[i].name; i++) {

		if (!strcasecmp(cmd_help[i].name, e->t.name)) {
		    found = true;
		    break;
		}
	    }
	    if (!found) {
		action_output("No Help for %s", e->t.name);
		any = true;
	    }
	} FOREACH_LLIST_END(&actions_list, e, action_elt_t *);
	if (!any) {
	    printf("No orphaned actions.\n");
	}
	return true;
    }

    /* Do a substring match on all of the actions. */
    for (i = 0; cmd_help[i].name != NULL; i++) {
	if (!strncasecmp(cmd_help[i].name, argv[0], strlen(argv[0]))) {
	    action_output("  %s %s\n    %s",
		    cmd_help[i].name,
		    cmd_help[i].args? cmd_help[i].args: "",
		    cmd_help[i].help? cmd_help[i].help: "");
		    any = true;
	}
    }
    if (any) {
	return true;
    }

    /* Check for an exact match on one of the topics. */
    for (i = 0; help_subcommand[i].name != NULL; i++) {
	if (!strncasecmp(help_subcommand[i].name, argv[0], strlen(argv[0]))) {
	    match = help_subcommand[i].flag;
	    overall = i;
	    break;
	}
    }

    if (!match) {
	action_output("No such command: %s", argv[0]);
	return false;
    }

    /* Matched on a topic. */
    if (help_subcommand[overall].text != NULL) {
	/* One-line topic. */
	action_output("%s", help_subcommand[overall].text);
	return true;
    }
    if (help_subcommand[overall].block != NULL) {
	int j;

	/* Multi-line topic. */
	for (j = 0; help_subcommand[overall].block[j] != NULL; j++) {
	    action_output("%s", help_subcommand[overall].block[j]);
	}
	return true;
    }
    if (help_subcommand[overall].fn != NULL) {
	/* Indirect output for topic. */
	(*help_subcommand[overall].fn)(true);
	return true;
    }

    /* Category. */
    for (i = 0; cmd_help[i].name != NULL; i++) {
	if (cmd_help[i].purpose & match) {
	    action_output("  %s %s\n    %s",
		    cmd_help[i].name,
		    cmd_help[i].args? cmd_help[i].args: "",
		    cmd_help[i].help? cmd_help[i].help: "");
	}
    }

    return true;
}

#if defined(WC3270) /*[*/
static void
html_help(bool ignored _is_unused)
{
	start_html_help();
}
#endif /*]*/

/**
 * Help module registration.
 */
void
help_register(void)
{
    static action_table_t help_actions[] = {
	{ "Help",	Help_action,	ACTION_KE }
    };

    /* Register the actions. */
    register_actions(help_actions, array_count(help_actions));
}
