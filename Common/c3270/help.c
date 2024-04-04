/*
 * Copyright (c) 2000-2024 Paul Mattes.
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
#include "c3270.h"
#include "glue.h"
#include "help.h"
#include "icmdc.h"
#include "names.h"
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
	{ AnAbort, NULL, P_SCRIPTING, "Abort pending scripts and macros" },
	{ AnAnsiText, NULL, P_SCRIPTING, "Dump pending NVT text" },
	{ AnAscii, NULL, P_SCRIPTING, "Screen contents in ASCII" },
	{ AnAscii, "<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from cursor, in ASCII" },
	{ AnAscii, "<row>,<col>,<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from <row>,<col> (0-origin), in ASCII" },
	{ AnAscii, "<row>,<col>,<rows>,<cols>", P_SCRIPTING,
	    "<rows>x<cols> of screen contents from <row>,<col> (0-origin), in ASCII" },
	{ AnAscii1, "<row>,<col>,<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from <row>,<col> (1-origin), in ASCII" },
	{ AnAscii1, "<row>,<col>,<rows>,<cols>", P_SCRIPTING,
	    "<rows>x<cols> of screen contents from <row>,<col> (1-origin), in ASCII" },
	{ AnAsciiField, NULL, P_SCRIPTING,
	    "Contents of current field, in ASCII" },
	{ AnAttn, NULL, P_3270, "Send 3270 ATTN sequence (TELNET IP)" },
	{ AnBackSpace, NULL, P_3270, "Move cursor left" },
	{ AnBackTab, NULL, P_3270, "Move to previous field" },
	{ AnBell, NULL, P_SCRIPTING, "Ring the terminal bell" },
	{ AnCircumNot, NULL, P_3270,
	    "Send ~ in NVT mode, notsign (X'5F', U+00AC) in 3270 mode" },
	{ AnClear, NULL, P_3270, "Send CLEAR AID (clear screen)" },
	{ AnClose, NULL, P_INTERACTIVE, "Alias for " AnDisconnect "()" },
	{ AnCloseScript, NULL, P_SCRIPTING, "Exit peer script" },
	{ AnCompose, NULL, P_INTERACTIVE,
	    "Interpret next two keystrokes according to the compose map" },
	{ AnConnect, "[L:][Y:][A:][<lu>@]<host>[:<port>][=<accept>]",
	    P_INTERACTIVE, "Open connection to <host>" },
#if defined(LOCAL_PROCESS) /*[*/
	{ AnConnect, "-e,[<command>[,<arg>...]]", P_INTERACTIVE,
	    "Open connection to a local shell or command" },
#endif /*]*/
#if defined(WC3270) /*[*/
	{ AnCopy, NULL, P_3270, "Copy selected text to Windows clipboard" },
#endif /*]*/
	{ AnCursorSelect, NULL, P_3270,
	    "Light pen select at cursor location" },
#if defined(WC3270) /*[*/
	{ AnCut, NULL, P_3270,
	    "Copy selected text to Windows clipboard, then erase" },
#endif /*]*/
	{ AnDelete, NULL, P_3270, "Delete character at cursor" },
	{ AnDeleteField, NULL, P_3270, "Erase field at cursor location (^U)" },
	{ AnDeleteWord, NULL, P_3270,
	    "Erase word before cursor location (^W)" },
	{ AnDisconnect, NULL, P_INTERACTIVE, "Close connection to host" },
	{ AnDown, NULL, P_3270, "Move cursor down" },
	{ AnDup, NULL, P_3270, "3270 DUP key (X'1C')" },
	{ AnEbcdic,	NULL, P_SCRIPTING, "Screen contents in EBCDIC" },
	{ AnEbcdic,	"<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from cursor, in EBCDIC" },
	{ AnEbcdic,	"<row>,<col>,<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from <row>,<col> (0-origin), in EBCDIC" },
	{ AnEbcdic,	"<row>,<col>,<rows>,<cols>", P_SCRIPTING,
	    "<rows>x<cols> of screen contents from <row>,<col> (0-origin), in EBCDIC" },
	{ AnEbcdic1,	"<row>,<col>,<n>", P_SCRIPTING,
	    "<n> bytes of screen contents from <row>,<col> (1-origin), in EBCDIC" },
	{ AnEbcdic1,	"<row>,<col>,<rows>,<cols>", P_SCRIPTING,
	    "<rows>x<cols> of screen contents from <row>,<col> (1-origin), in EBCDIC" },
	{ AnEbcdicField, NULL, P_SCRIPTING,
	    "Contents of current field, in EBCDIC" },
	{ AnEcho, "<text>", P_SCRIPTING, "Return text as a string" },
	{ AnEnter, NULL, P_3270, "Send ENTER AID" },
	{ AnErase, NULL, P_3270, "Destructive backspace" },
	{ AnEraseEOF, NULL, P_3270, "Erase from cursor to end of field" },
	{ AnEraseInput, NULL, P_3270, "Erase all input fields" },
	{ AnEscape, NULL, P_INTERACTIVE,
	    "Escape to '" HELP_W "c3270>' prompt" },
	{ AnExecute, "<command>", P_SCRIPTING, "Execute a shell command" },
	{ "Exit", NULL, P_INTERACTIVE, "Exit " HELP_W "c3270" },
	{ AnExpect, "<pattern>", P_SCRIPTING, "Wait for NVT output" },
	{ AnFail, "<text>", P_SCRIPTING, "Fail and return text" },
	{ AnFieldEnd, NULL, P_3270, "Move to end of field" },
	{ AnFieldMark, NULL, P_3270, "3270 FIELD MARK key (X'1E')" },
	{ AnFlip, NULL, P_3270, "Flip display left-to-right" },
	{ AnHelp, "all|interactive|3270|scripting|transfer|<action>",
	    P_INTERACTIVE, "Get help" },
	{ AnHexString, "<digits>", P_3270|P_SCRIPTING,
	    "Input field data in hex" },
	{ AnHome, NULL, P_3270, "Move cursor to first field" },
	{ Anignore, NULL, P_3270, "Do nothing" },
	{ "Info", "<text>", P_SCRIPTING|P_INTERACTIVE, "Display text in OIA" },
	{ AnInsert, NULL, P_3270, "Set 3270 insert mode" },
	{ AnInterrupt, NULL, P_3270, "In NVT mode, send IAC IP" },
	{ AnKey, "<symbol>|0x<nn>", P_3270, "Input one character" },
	{ AnKeyboardDisable, "[" ResTrue "|" ResFalse "|" KwForceEnable "]",
	    P_SCRIPTING|P_INTERACTIVE,
	    "Modify automatic script keyboard locking" },
	{ AnKeymap, "[<keymap-name>]", P_SCRIPTING|P_INTERACTIVE,
	    "Push temporary keymap, or pop if none specified" },
	{ AnKeypad, NULL, P_INTERACTIVE, "Pop up the 3270 keypad" },
	{ AnLeft, NULL, P_3270, "Move cursr left" },
	{ AnLeft2, NULL, P_3270, "Move cursor left 2 columns" },
	{ AnMacro, "<name>", P_SCRIPTING, "Execute a predefined macro" },
	{ AnMenu, NULL, P_INTERACTIVE, "Pop up the command menu" },
	{ AnMoveCursor, "<row>,<col>", P_3270|P_SCRIPTING,
	    "Move cursor to specific location (0-origin, deprecated)" },
	{ AnMoveCursor, "<offset>", P_3270|P_SCRIPTING,
	    "Move cursor to a buffer offset (0-origin)" },
	{ AnMoveCursor1, "<row>,<col>", P_3270|P_SCRIPTING,
	    "Move cursor to specific location (1-origin)" },
	{ AnNewline, NULL, P_3270, "Move cursor to first field in next row" },
	{ AnNextWord, NULL, P_3270, "Move cursor to next word" },
	{ AnNvtText, NULL, P_SCRIPTING, "Dump pending NVT text" },
	{ AnOpen, NULL, P_INTERACTIVE, "Alias for " AnConnect "()" },
	{ AnPA, "<n>", P_3270, "Send 3270 Program Attention" },
#if defined(WC3270) /*[*/
	{ "Paste", NULL, P_3270, "Paste clipboard contents" },
#endif /*]*/
	{ AnPasteString, "hex-string...", P_SCRIPTING, "Enter input as if pasted" },
	{ AnPause, NULL, P_SCRIPTING, "Wait for 350ms" },
	{ AnPF, "<n>", P_3270, "Send 3270 PF AID" },
	{ AnPreviousWord, NULL, P_3270, "Move cursor to previous word" },
	{ AnPrinter, KwStart "[,lu]|" KwStop, P_3270|P_SCRIPTING|P_INTERACTIVE,
	    "Start or stop " HELP_W "pr3287 printer session" },
        { AnPrintText,
	    "[" KwHtml "|" KwRtf ",][" KwModi ",][" KwCaption ",<caption>,][" KwReplace "|" KwAppend ",]" KwFile ",<filename>",
	    P_INTERACTIVE|P_SCRIPTING,
	    "Save screen image in a file" },
        { AnPrintText,
	    "[" KwModi ",][" KwCaption ",<caption>],"
#if defined(WC3270) /*[*/
	    "[" KwDialog "|" KwNoDialog ",][<printer-name>]",
#else /*][*/
	    "[<print-command>]",
#endif /*]*/
	    P_INTERACTIVE|P_SCRIPTING,
	    "Print screen image" },
	{ AnPrompt, "[app-name]", P_SCRIPTING|P_INTERACTIVE,
	    "Start an external prompt" },
        { AnQuery, "<keyword>", P_SCRIPTING|P_INTERACTIVE,
	    "Query operational parameters" },
	{ AnQuit, NULL, P_INTERACTIVE, "Exit " HELP_W "3270" },
        { AnReadBuffer, "[" KwAscii "|" KwEbcdic "|" KwUnicode "]", P_SCRIPTING,
	    "Dump display buffer" },
        { AnReadBuffer, "[" KwAscii "|" KwEbcdic "|" KwUnicode ",]" KwField, P_SCRIPTING,
	    "Dump display buffer for current field" },
	{ AnReconnect, NULL, P_INTERACTIVE, "Reconnect to previous host" },
	{ AnRedraw, NULL, P_INTERACTIVE|P_3270, "Redraw screen" },
	{ AnReset, NULL, P_3270, "Clear keyboard lock" },
	{ AnRestoreInput, "[<set>]", P_INTERACTIVE, "Restore screen input fields" },
	{ AnRight, NULL, P_3270, "Move cursor right" },
	{ AnRight2, NULL, P_3270, "Move cursor right 2 columns" },
	{ AnSaveInput, "[<set>]", P_INTERACTIVE, "Save screen input fields" },
	{ AnScreenTrace, KwOn "[[," KwFile "],<filename>]",  P_INTERACTIVE,
	    "Save screen images to file" },
	{ AnScreenTrace,
# if defined(_WIN32) /*[*/
	    KwOn "," KwPrinter "[,<printer-name>]",
# else /*][*/
	    KwOn "," KwPrinter "[,<print-command>]",
# endif /*]*/
	    P_INTERACTIVE, "Save screen images to printer" },
	{ AnScreenTrace, KwOff, P_INTERACTIVE, "Stop saving screen images" },
	{ AnScript, "[" KwDashAsync ",][" KwDashNoLock ",][" KwDashSingle ",]"
#if defined(_WIN32) /*[*/
	    "[" KwDashShareConsole ",]"
#endif /*]*/
	    "<path>[,<arg>...]",
	    P_SCRIPTING, "Run a child script" },
	{ AnScroll, KwForward "|" KwBackward, P_INTERACTIVE, "Scroll screen" },
	{ AnSet, "[<setting-name>,value]", P_INTERACTIVE|P_SCRIPTING,
	    "Change a setting or display all settings" },
	{ AnShow, KwCopyright "|" KwStatus "|" KwKeymap, P_INTERACTIVE,
	    "Display status and settings" },
	{ AnSnap, "<args>", P_SCRIPTING, "Screen snapshot manipulation" },
        { AnSource, "<file>", P_SCRIPTING|P_INTERACTIVE, "Read actions from file" },
	{ AnString, "<text>", P_3270|P_SCRIPTING, "Input a string" },
	{ AnSysReq, NULL, P_3270,
	    "Send 3270 Attention (TELNET ABORT or SYSREQ AID)" },
	{ AnTab, NULL, P_3270, "Move cursor to next field" },
	{ AnTemporaryComposeMap, "[<compose-map-name>]",
	    P_SCRIPTING|P_INTERACTIVE, "Set or clear temporary compose map" },
	{ AnTemporaryKeymap, "[<keymap-name>]", P_SCRIPTING|P_INTERACTIVE,
	    "Alias for " AnKeymap "()" },
#if defined(WC3270) /*[*/
	{ AnTitle, "<text>", P_SCRIPTING|P_INTERACTIVE, "Change window title" },
#endif /*]*/
	{ AnToggle, "[<toggle-name>[,value]]", P_INTERACTIVE|P_SCRIPTING,
	    "Change a toggle" },
	{ AnToggleInsert, NULL, P_3270, "Set or clear 3270 insert mode" },
	{ AnToggleReverse, NULL, P_3270, "Set or clear reverse-input mode" },
	{ AnTrace, KwOn "[,<file>]|" KwOff, P_INTERACTIVE, "Configure tracing" },
	{ AnTransfer, "[<args>]", P_INTERACTIVE,
	    "IND$FILE file transfer (see 'help file-transfer')" },
	{ AnUp, NULL, P_3270, "Move cursor up" },
	{ AnWait, "<args>", P_SCRIPTING, "Wait for host events" },
	{ NULL,  NULL, 0, NULL }
};

#if defined(HAVE_START) || defined(WC3270) /*[*/
static void html_help(bool);
#endif /*]*/

static struct {
	const char *name;
	int flag;
	const char *text;
	const char **block;
	void (*fn)(bool);
} help_subcommand[] = {
#if defined(HAVE_START) /*[*/
	{ "online",		P_HTML,		NULL, NULL, html_help },
#endif /*]*/
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

    action_debug(AnHelp, ia, argc, argv);
    if (check_argc(AnHelp, argc, 0, 1) < 0) {
	return false;
    }

    if (argc != 1) {
	action_output(
#if defined(HAVE_START) /*[*/
"  help online        launch online help\n"
#endif /*]*/
"  help all           all actions\n"
"  help 3270          3270 actions\n"
"  help interactive   interactive (command-prompt) actions\n"
"  help <action>      help for one <action>\n"
"  help options       command-line options\n"
"  help scripting     scripting actions\n"
"  help file-transfer file transfer options\n"
#if defined(WC3270) /*[*/
"  help html          alias for 'help online'\n"
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
	    if (!found && !(e->t.flags & ACTION_HIDDEN)) {
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
	    action_output("  %s(%s)\n    %s",
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
	    action_output("  %s(%s)\n    %s",
		    cmd_help[i].name,
		    cmd_help[i].args? cmd_help[i].args: "",
		    cmd_help[i].help? cmd_help[i].help: "");
	}
    }

    return true;
}

#if defined(HAVE_START) || defined(WC3270) /*[*/
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
	{ AnHelp,	Help_action,	ACTION_KE }
    };

    /* Register the actions. */
    register_actions(help_actions, array_count(help_actions));
}
