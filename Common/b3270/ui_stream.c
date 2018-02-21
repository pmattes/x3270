/*
 * Copyright (c) 2016-2017 Paul Mattes.
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
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ui_stream.c
 *		A GUI back-end for a 3270 Terminal Emulator
 *		GUI data stream generation.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <unistd.h>
# include <netinet/in.h>
# include <sys/select.h>
# include <arpa/inet.h>
#endif /*]*/
#include <errno.h>
#include <expat.h>

#include "actions.h"
#include "appres.h"
#include "3270ds.h"
#include "b3270proto.h"
#include "b_password.h"
#include "lazya.h"
#include "popups.h"
#include "resources.h"
#include "screen.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "xio.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

#include "ui_stream.h"

#define BOM_SIZE	3

#define INBUF_SIZE	8192

#if defined(_WIN32) /*[*/
static HANDLE peer_thread;
static HANDLE peer_enable_event, peer_done_event;
static char peer_buf[INBUF_SIZE];
int peer_nr;
int peer_errno;
#endif /*]*/

/* XML container stack. */
typedef struct _ui_container {
    struct _ui_container *next;
    char *name;
} ui_container_t;
static ui_container_t *ui_container;
static int ui_depth;

static XML_Parser parser;
int input_nest = 0;

/* Action state. */
typedef struct {
    char *tag;
    char *result;
} ui_action_t;

/* Callback blocks. */
static void ui_action_data(task_cbh handle, const char *buf, size_t len);
static bool ui_action_done(task_cbh handle, bool success, bool abort);
static tcb_t cb_keymap = {
    "ui",
    IA_KEYMAP,
    CB_UI | CB_NEW_TASKQ,
    ui_action_data,
    ui_action_done,
    NULL
};
static tcb_t cb_macro = {
    "ui",
    IA_MACRO,
    CB_UI | CB_NEW_TASKQ,
    ui_action_data,
    ui_action_done,
    NULL
};
static tcb_t cb_command = {
    "ui",
    IA_COMMAND,
    CB_UI | CB_NEW_TASKQ,
    ui_action_data,
    ui_action_done,
    NULL
};
static tcb_t cb_ui = {
    "ui",
    IA_UI,
    CB_UI | CB_NEW_TASKQ,
    ui_action_data,
    ui_action_done,
    NULL
};

/* Write to the UI socket. */
static void
uprintf(const char *fmt, ...)
{
    va_list ap;
    char *s;
    static bool eol = true;

    va_start(ap, fmt);
    s = xs_vbuffer(fmt, ap);
    va_end(ap);
    write(fileno(stdout), s, strlen(s));

    if (eol) {
	vtrace("ui> ");
	eol = false;
    }
    vtrace("%s", s);
    if (strlen(s) > 0 && s[strlen(s) - 1] == '\n') {
	eol = true;
    }
    Free(s);
}

/* Dump a string in HTML quoted format, if needed. */
static void
xml_safe(const char *value)
{
    char s;

    while ((s = *value++)) {
	unsigned char c = s;

	if ((c & 0x80) && (c < 0x86 || c > 0x9f)) {
	    /* UTF-8 upper. */
	    uprintf("%c", c);
	} else if (c >= ' ' && c != 0x7f) {
	    /* Printable, but might need quoting. */
	    switch (c) {
	    case '<':
		uprintf("&lt;");
		break;
	    case '>':
		uprintf("&gt;");
		break;
	    case '"':
		uprintf("&quot;");
		break;
	    case '&':
		uprintf("&amp;");
		break;
	    case '\'':
		uprintf("&apos;");
		break;
	    default:
		uprintf("%c", c);
		break;
	    }
	} else {
	    /*
	     * Not printable. XML 1.0 understands tabs, newlines and carriage
	     * returns, and that's about it.
	     */
	    switch (c) {
	    case 9:
	    case 10:
	    case 13:
		uprintf("&#%u;", c);
		break;
	    default:
		uprintf(" ");
		break;
	    }
	}
    }
}

/*
 * Generate a GUI object, either leaf or container.
 * The name is followed by a NULL-terminated list of tags and values.
 */
static void
ui_object(bool leaf, const char *name, const char *args[])
{
    const char *tag;
    int i = 0;

    uprintf("%*s<%s", ui_depth, "", name);
    while ((tag = args[i++]) != NULL) {
	const char *value = args[i++];

	uprintf(" %s=\"", tag);
	xml_safe(value);
	uprintf("\"");
    }
    uprintf("%s>\n", leaf? "/": "");
}

/*
 * Generate a GUI object, either leaf or container.
 * The name is followed by a NULL-terminated list of tags and values.
 */
static void
ui_vobject(bool leaf, const char *name, va_list ap)
{
    const char *tag;

    uprintf("%*s<%s", ui_depth, "", name);
    while ((tag = va_arg(ap, const char *)) != NULL) {
	const char *value = va_arg(ap, const char *);

	if (value) {
	    uprintf(" %s=\"", tag);
	    xml_safe(value);
	    uprintf("\"");
	}
    }
    uprintf("%s>\n", leaf? "/": "");
}

/*
 * Generate a GUI leaf object.
 */
void
ui_leaf(const char *name, const char *args[])
{
    ui_object(true, name, args);
}

/*
 * Generate a GUI leaf object.
 * The name is followed by a NULL-terminated list of tags and values.
 */
void
ui_vleaf(const char *name, ...)
{
    va_list ap;

    va_start(ap, name);
    ui_vobject(true, name, ap);
    va_end(ap);
}

/* Remember a container name. */
static void
push_name(const char *name)
{
    ui_container_t *g;

    g = Malloc(sizeof(ui_container_t) + strlen(name) + 1);
    g->name = (char *)(g + 1);
    strcpy(g->name, name);
    g->next = ui_container;
    ui_container = g;
    ui_depth++;
}

/*
 * Start a container object.
 */
void
ui_push(const char *name, const char *args[])
{
    /* Output the start of the object. */
    ui_object(false, name, args);

    /* Remember the name. */
    push_name(name);
}

/*
 * Start a container object.
 * The name is followed by a NULL-terminated list of tags and values.
 */
void
ui_vpush(const char *name, ...)
{
    va_list ap;

    /* Output the start of the object. */
    va_start(ap, name);
    ui_vobject(false, name, ap);
    va_end(ap);

    /* Remember the name. */
    push_name(name);
}

/*
 * End a container object.
 */
void
ui_pop(void)
{
    ui_container_t *g = ui_container;

    ui_depth--;
    uprintf("%*s</%s>\n", ui_depth, "", g->name);
    ui_container = g->next;
    Free(g);
}

/* Data callback. */
static void
ui_action_data(task_cbh handle, const char *buf, size_t len)
{
    ui_action_t *uia = (ui_action_t *)handle;

    if (uia->result) {
	uia->result = Realloc(uia->result, strlen(uia->result) + 1 + len + 1);
	sprintf(strchr(uia->result, '\0'), "\n%.*s", (int)len, buf);
    } else {
	uia->result = xs_buffer("%.*s", (int)len, buf);
    }
}

/* Completion callback. */
static bool
ui_action_done(task_cbh handle, bool success, bool abort)
{
    ui_action_t *uia = (ui_action_t *)handle;
    unsigned long msec = task_cb_msec(handle);

    /*
     * Repaint the screen, so the effect of the action can be seen before
     * we indicate that the action is complete.
     */
    screen_disp(false);

    ui_vleaf("run-result",
	    "r-tag", uia->tag,
	    "success", success? "true": "false",
	    "text", uia->result,
	    "abort", abort? "true": NULL,
	    "time", lazyaf("%ld.%03ld", msec / 1000L, msec % 1000L),
	    NULL);

    Replace(uia->result, NULL);
    Free(uia);
    return true;
}

/* Emit a warning about an unknown attribute. */
static void
ui_unknown_attribute(const char *element, const char *attribute)
{
    ui_vleaf("ui-error",
	    "fatal", "false",
	    "text", "unknown attribute",
	    "element", element,
	    "attribute", attribute,
	    "line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
	    "column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
	    NULL);
}

/* Emit a warning about a missing attribute. */
static void
ui_missing_attribute(const char *element, const char *attribute)
{
    ui_vleaf("ui-error",
	    "fatal", "false",
	    "text", "missing attribute",
	    "element", element,
	    "attribute", attribute,
	    "line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
	    "column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
	    NULL);
}

/* Execute the 'run' command. */
static void
do_run(const char *cmd, const char **attrs)
{
    const char *type = NULL;
    const char *tag = NULL;
    const char *command = NULL;
    int i;
    ui_action_t *uia;
    tcb_t *tcb;

    for (i = 0; attrs[i] != NULL; i += 2) {
	if (!strcasecmp(attrs[i], "type")) {
	    type = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], "r-tag")) {
	    tag = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], "actions")) {
	    command = attrs[i + 1];
	} else {
	    ui_unknown_attribute("run", attrs[i]);
	}
    }

    if (command == NULL) {
	ui_missing_attribute("run", "actions");
	return;
    }

    /* Run the command. */
    uia = (ui_action_t *)Malloc(sizeof(ui_action_t) +
	    (tag? (strlen(tag) + 1): 0));
    uia->tag = tag? (char *)(uia + 1): NULL;
    if (tag) {
	strcpy(uia->tag, tag);
    }
    uia->result = NULL;
    if (type != NULL && !strcasecmp(type, "keymap")) {
	tcb = &cb_keymap;
    } else if (type != NULL && !strcasecmp(type, "command")) {
	tcb = &cb_command;
    } else if (type != NULL && !strcasecmp(type, "macro")) {
	tcb = &cb_macro;
    } else {
	tcb = &cb_ui;
    }
    push_cb(command, strlen(command), tcb, (task_cbh)uia);
}

/* The (dummy) action for pass-through actions. */
static bool
Passthru_action(ia_t ia, unsigned argc, const char **argv)
{
    const char **args =
	(const char **)Malloc((5 + (argc * 2) + 1) * sizeof(char *));
    unsigned in_ix = 0;
    int out_ix = 0;
    const char *passthru_tag;
    task_cbh *ret_cbh = NULL;

    /* Mark this action as waiting for a pass-through response. */
    passthru_tag = task_set_passthru(&ret_cbh);

    /* Tell the UI we are waiting. */
    args[out_ix++] = "action";
    args[out_ix++] = current_action_name;
    args[out_ix++] = "p-tag";
    args[out_ix++] = passthru_tag;
    if (ret_cbh != NULL) {
	ui_action_t *uia = (ui_action_t *)ret_cbh;

	if (uia->tag) {
	    args[out_ix++] = "parent-r-tag";
	    args[out_ix++] = uia->tag;
	}
    }
    for (in_ix = 0; in_ix < argc; in_ix++) {
	args[out_ix++] = lazyaf("arg%d", in_ix + 1);
	args[out_ix++] = argv[in_ix];
    }
    args[out_ix] = NULL;
    ui_object(true, "passthru", args);

    return true;
}

/* Register a command. */
static void
do_register(const char *cmd, const char **attrs)
{
    const char *name = NULL;
    const char *help_text = NULL;
    const char *help_parms = NULL;
    int i;
    size_t j;
    action_table_t *a;

    for (i = 0; attrs[i] != NULL; i += 2) {
	if (!strcasecmp(attrs[i], "name")) {
	    name = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], "help-text")) {
	    help_text = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], "help-parms")) {
	    help_parms = attrs[i + 1];
	} else {
	    ui_unknown_attribute("register", attrs[i]);
	}
    }

    if (name == NULL) {
	ui_missing_attribute("register", "name");
	return;
    }
    for (j = 0; name[j]; j++) {
	if (!isprint((unsigned char)name[j])) {
	    ui_vleaf("ui-error",
		    "fatal", "false",
		    "text", "invalid name",
		    "element", "register",
		    "line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
		    "column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
		    NULL);
	    return;
	}
    }

    a = (action_table_t *)Malloc(sizeof(action_table_t));
    memset(a, 0, sizeof(action_table_t));
    a[0].name = NewString(name);
    a[0].action = Passthru_action;
    a[0].flags = ACTION_KE;
    a[0].help_flags = 0;
    a[0].help_parms = help_parms? NewString(help_parms): NULL;
    a[0].help_text = help_text? NewString(help_text): NULL;
    a[0].ia_restrict = password_ia_restrict(name);

    register_actions(a, 1);
}

/* Complete a pass-through command. */
void
do_passthru_complete(bool success, const char *cmd, const char **attrs)
{
    const char *tag = NULL;
    const char *text = NULL;
    int i;

    for (i = 0; attrs[i] != NULL; i += 2) {
	if (!strcasecmp("p-tag", attrs[i])) {
	    tag = attrs[i + 1];
	} else if (!strcasecmp("text", attrs[i])) {
	    text = attrs[i + 1];
	} else {
	    ui_unknown_attribute(cmd, attrs[i]);
	}
    }

    if (tag == NULL) {
	ui_missing_attribute(cmd, "p-tag");
	return;
    }

    if (!success && text == NULL) {
	ui_missing_attribute(cmd, "text");
	return;
    }


    /* Succeed. */
    task_passthru_done(tag, success, text);
}

/* UI input processor. */
static void
process_input(const char *buf, ssize_t nr)
{
    if (XML_Parse(parser, buf, nr, 0) == 0) {
	ui_vleaf("ui-error",
		"fatal", "true",
		"text", xs_buffer("XML parsing error: %s",
		    XML_ErrorString(XML_GetErrorCode(parser))),
		"line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
		"column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
		NULL);
	fprintf(stderr, "Fatal XML parsing error: %s\n",
		XML_ErrorString(XML_GetErrorCode(parser)));
	x3270_exit(1);
    }
}

/* UI input-ready function. */
static void
ui_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    ssize_t nr;
    char buf[INBUF_SIZE];
    ssize_t nc;
    static ssize_t bom_count = 0;
    static char bom_read[BOM_SIZE];
    static unsigned char bom_value[BOM_SIZE] = { 0xef, 0xbb, 0xbf };

    /* Read the data. */
#if !defined(_WIN32) /*[*/
    nr = read(fileno(stdin), buf, INBUF_SIZE);
#else /*][*/
    nr = peer_nr;
    peer_nr = 0;
    if (nr < 0) {
	errno = peer_errno;
    } else {
	memcpy(buf, peer_buf, nr);
	SetEvent(peer_enable_event);
    }
#endif /*]*/

    if (nr < 0) {
	popup_an_errno(errno, "UI input");
	fprintf(stderr, "UI read error\n");
	x3270_exit(1);
    }
    if (nr == 0) {
	vtrace("UI input EOF, exiting\n");
	if (input_nest) {
	    ui_vleaf("ui-error",
		    "fatal", "false",
		    "text", "unclosed elements",
		    "count", lazyaf("%d", input_nest),
		    "line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
		    "column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
		    NULL);
	}
	x3270_exit(0);
    }

    /* Trace it, skipping any initial newline. */
    {
	int nrd = (int)nr;
	char *bufd = buf;

	if (bufd[0] == '\r') {
	    nrd--;
	    bufd++;
	}
	if (nrd > 0 && bufd[0] == '\n') {
	    nrd--;
	    bufd++;
	}

	vtrace("ui< %.*s", nrd, bufd);
	if (nrd == 0 || bufd[nrd - 1] != '\n') {
	    vtrace("\n");
	}
    }

    /* If we're past the BOM, process directly. */
    if (bom_count >= BOM_SIZE) {
	process_input(buf, nr);
	return;
    }

    /* Copy into the bom_read buffer. */
    nc = BOM_SIZE - bom_count;
    if (nc > nr) {
	nc = nr;
    }
    memcpy(bom_read + bom_count, buf, nc);
    bom_count += nc;

    /*
     * Check for a match. If not, process the (wrong) BOM, then whatever
     * else we read.
     */
    if (memcmp(bom_read, bom_value, bom_count)) {

	/* No match. Process the mistaken BOM as regular input. */
	process_input(bom_read, bom_count);

	/* No more BOM processing. */
	bom_count = BOM_SIZE;
    } else {

	/* It matched so far. */
	if (bom_count < BOM_SIZE) {

	    /* But we're not done. */
	    return;
	}
    }

    /* Process what we read past the BOM. */
    if (nr > nc) {
	process_input(buf + nc, nr - nc);
    }
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

/**
 * Clean up the UI socket when exiting.
 *
 * @param[in] ignored
 */
static void
ui_exiting(bool ignored)
{
    while (ui_container) {
	ui_pop();
    }
}

/**
 * XML start handler.
 */
static void
xml_start(void *userData _is_unused, const XML_Char *name,
	const XML_Char **atts)
{
    input_nest++;
    if (input_nest > 2) {
	ui_vleaf("ui-error",
		"fatal", "false",
		"text", "invalid nested element",
		"element", name,
		"line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
		"column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
		NULL);
	return;
    }

    if (input_nest == 1) {
	int i;

	if (strcasecmp(name, DocIn)) {
	    ui_vleaf("ui-error",
		    "fatal", "true",
		    "text", "unexpected document element (want " DocIn ")",
		    "element", name,
		    "line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
		    "column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
		    NULL);
	    fprintf(stderr, "UI document element error\n");
	    x3270_exit(1);
	}
	for (i = 0; atts[i] != NULL; i += 2) {
	    ui_unknown_attribute(DocIn, atts[i]);
	}
	return;
    }

    if (!strcasecmp(name, "run")) {
	do_run(name, atts);
    } else if (!strcasecmp(name, "register")) {
	do_register(name, atts);
    } else if (!strcasecmp(name, "succeed")) {
	do_passthru_complete(true, name, atts);
    } else if (!strcasecmp(name, "fail")) {
	do_passthru_complete(false, name, atts);
    } else {
	ui_vleaf("ui-error",
		"fatal", "false",
		"text", "unrecognized element",
		"element", name,
		"line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
		"column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
		NULL);
    }
}

/**
 * XML end handler.
 */
static void
xml_end(void *userData _is_unused, const XML_Char *name)
{
    input_nest--;
}

/**
 * XML plain data handler.
 */
static void
xml_data(void *userData _is_unused, const XML_Char *s, int len)
{
    bool nonwhite = false;
    int i;

    for (i = 0; i < len; i++) {
	if (!isspace((unsigned char)s[i])) {
	    nonwhite = true;
	    break;
	}
    }

    if (!nonwhite) {
	return;
    }

    ui_vleaf("ui-error",
	    "fatal", "false",
	    "text", "ignoring plain text",
	    "line", lazyaf("%d", XML_GetCurrentLineNumber(parser)),
	    "column", lazyaf("%d", XML_GetCurrentColumnNumber(parser)),
	    "count", lazyaf("%d", len),
	    NULL);
}

/**
 * Initialize the UI socket.
 *
 * @param[in] sa	address and port to listen on
 * @param[in] sa_len	length of sa
 */
void
ui_io_init()
{
    /* Create the XML parser. */
    parser = XML_ParserCreate("UTF-8");
    if (parser == NULL) {
	Error("Cannot create XML parser");
    }
    XML_SetElementHandler(parser, xml_start, xml_end);
    XML_SetCharacterDataHandler(parser, xml_data);

#if !defined(_WIN32) /*[*/
    AddInput(fileno(stdin), ui_input);
#else /*][*/
    /* Set up the peer thread. */
    peer_enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    peer_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
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
    AddInput(peer_done_event, ui_input);
#endif /*]*/

    /* Start the XML stream. */
    uprintf("%c%c%c<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
	    0xef, 0xbb, 0xbf);
    ui_vpush(DocOut, NULL);

    /* Set up a handler for exit. */
    register_schange_ordered(ST_EXITING, ui_exiting, ORDER_LAST);
}
