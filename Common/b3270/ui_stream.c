/*
 * Copyright (c) 2016-2024 Paul Mattes.
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
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <expat.h>

#include "actions.h"
#include "appres.h"
#include "3270ds.h"
#include "b3270proto.h"
#include "bind-opt.h"
#include "b_password.h"
#include "json.h"
#include "json_run.h"
#include "popups.h"
#include "resources.h"
#include "screen.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "utf8.h"
#include "utils.h"
#include "xio.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

#include "ui_stream.h"

#define BOM_SIZE	3

#define INBUF_SIZE	8192

#define JW_OPTS		(appres.b3270.indent? 0: JW_ONE_LINE)
#define XINDENT		(appres.b3270.indent? uix.depth: 0)
#define MIN_D		(appres.b3270.wrapper_doc? 2: 1)
#define XNL		((appres.b3270.indent || uix.depth < MIN_D)? "\n": "")
#define XNLL(leaf)	((appres.b3270.indent || (uix.depth < MIN_D && leaf))? "\n": "")

#if defined(_WIN32) /*[*/
static HANDLE peer_thread;
static HANDLE peer_enable_event, peer_done_event;
static char peer_buf[INBUF_SIZE];
int peer_nr;
int peer_errno;
#endif /*]*/

/* XML container stack. */
typedef struct _uix_container {
    struct _uix_container *next;
    char *name;
} uix_container_t;

/* XML state. */
static struct {
    uix_container_t *container;
    int depth;
    bool isopen;
    XML_Parser parser;
    int input_nest;
    bool master_doc;
    bool need_reset;
} uix;

/* JSON container stack. */
typedef struct _uij_container {
    struct _uij_container *next;
    json_t *j;	/* object or array */
} uij_container_t;

/* JSON state. */
static struct {
    uij_container_t *container;
    char *pending_input;
    int line;
    int column;
} uij;

/* Action state. */
typedef struct {
    char *tag;
    char *xresult;		/* XML result */
    char *xresult_err;		/* XML result error indicators */
    json_t *jresult;		/* JSON result */
    json_t *jresult_err;	/* JSON result error indicators */
} ui_action_t;

/* Callback blocks. */
static void ui_action_data(task_cbh handle, const char *buf, size_t len,
	bool success);
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
static tcb_t cb_keypad = {
    "ui",
    IA_KEYPAD,
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

static socket_t ui_socket = INVALID_SOCKET;

static void xml_start(void *userData, const XML_Char *name,
	const XML_Char **atts);
static void xml_end(void *userData, const XML_Char *name);
static void xml_data(void *userData, const XML_Char *s, int len);
static void uij_add_to_parent(const char *name, json_t *j);

/* Write to the UI socket. */
static void
uprintf(const char *fmt, ...)
{
    va_list ap;
    char *s;
    static char *pending_trace = NULL;
    ssize_t nw;

    va_start(ap, fmt);
    s = Vasprintf(fmt, ap);
    va_end(ap);
    if (ui_socket != INVALID_SOCKET) {
	nw = send(ui_socket, s, (int)strlen(s), 0);
    } else {
	nw = write(fileno(stdout), s, (int)strlen(s));
    }

    if (pending_trace != NULL) {
	char *pt = Asprintf("%s%s", pending_trace, s);

	Replace(pending_trace, pt);
	Free(s);
    } else {
	pending_trace = s;
    }
    if (strlen(pending_trace) > 0 &&
	    pending_trace[strlen(pending_trace) - 1] == '\n') {
	char *t = pending_trace;

	while (*t) {
	    char *newline = strchr(t, '\n');

	    vtrace("ui> %.*s", (int)(newline - t) + 1, t);
	    t = newline + 1;
	}
	Replace(pending_trace, NULL);
    }

    if (nw < 0) {
	    vtrace("UI write failure: %s\n",
#if defined(_WIN32) /*[*/
		    (ui_socket != INVALID_SOCKET)?
			win32_strerror(GetLastError()):
#endif /*]*/
		    strerror(errno));
    }
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
uix_object(bool leaf, const char *name, const char *args[])
{
    const char *tag;
    int i = 0;

    uprintf("%*s<%s", XINDENT, "", name);

    while ((tag = args[i++]) != NULL) {
	const char *value = args[i++];

	uprintf(" %s=\"", tag);
	xml_safe(value);
	uprintf("\"");
    }

    uprintf("%s>%s", leaf? "/": "", XNLL(leaf));
}

/*
 * Generate a GUI object, either leaf or container.
 * The name is followed by a NULL-terminated list of tags, types and values.
 */
static void
uix_vobject3(bool leaf, const char *name, va_list ap)
{
    const char *tag;

    uprintf("%*s<%s", XINDENT, "", name);

    while ((tag = va_arg(ap, const char *)) != NULL) {
	ui_attr_t type = va_arg(ap, ui_attr_t);
	const char *value;

	switch (type) {
	case AT_STRING:
	    value = va_arg(ap, const char *);
	    if (value == NULL) {
		break;
	    }
	    uprintf(" %s=\"", tag);
	    xml_safe(value);
	    uprintf("\"");
	    break;
	case AT_INT:
	    uprintf(" %s=\"%"PRId64"\"", tag, va_arg(ap, int64_t));
	    break;
	case AT_SKIP_INT:
	    (void) va_arg(ap, int64_t);
	    break;
	case AT_DOUBLE:
	    uprintf(" %s=\"%g\"", tag, va_arg(ap, double));
	    break;
	case AT_BOOLEAN:
	    uprintf(" %s=\"%s\"", tag, ValTrueFalse(va_arg(ap, int)));
	    break;
	case AT_SKIP_BOOLEAN:
	    (void) va_arg(ap, int);
	    break;
	case AT_NODE:
	    assert(false);
	    break;
	}
    }

    uprintf("%s>%s", leaf? "/": "", XNLL(leaf));
}

/*
 * Generate a GUI leaf object.
 * The name is followed by a NULL-terminated list of tags, types and values.
 */
void
ui_leaf(const char *name, ...)
{
    va_list ap;

    va_start(ap, name);
    if (XML_MODE) {
	uix_vobject3(true, name, ap);
    } else {
	const char *tag;
	bool toplevel = false;

	if (uij.container == NULL || json_is_array(uij.container->j)) {
	    uij_open_object(NULL);
	    toplevel = true;
	}
	uij_open_object(name);
	while ((tag = va_arg(ap, const char *)) != NULL) {
	    ui_attr_t type = va_arg(ap, ui_attr_t);

	    switch (type) {
	    case AT_STRING:
		ui_add_element(tag, type, va_arg(ap, const char *));
		break;
	    case AT_INT:
		ui_add_element(tag, type, va_arg(ap, int64_t));
		break;
	    case AT_SKIP_INT:
		(void) va_arg(ap, int64_t);
		break;
	    case AT_DOUBLE:
		ui_add_element(tag, type, va_arg(ap, double));
		break;
	    case AT_BOOLEAN:
		ui_add_element(tag, type, va_arg(ap, int));
		break;
	    case AT_SKIP_BOOLEAN:
		(void) va_arg(ap, int);
		break;
	    case AT_NODE:
		ui_add_element(tag, type, va_arg(ap, json_t *));
		break;
	    }
	}
	uij_close_object();
	if (toplevel) {
	    uij_close_object();
	}
    }
    va_end(ap);
}

/* Remember a container name. */
static void
push_name(const char *name, bool is_array)
{
    uix_container_t *g;

    g = Malloc(sizeof(uix_container_t) + strlen(name) + 1);
    g->name = (char *)(g + 1);
    strcpy(g->name, name);
    g->next = uix.container;
    uix.container = g;
    uix.depth++;
}

/*
 * Start a container object.
 * The name is followed by a NULL-terminated list of tags, types and values.
 */
void
uix_push(const char *name, ...)
{
    va_list ap;

    /* Output the start of the object. */
    va_start(ap, name);
    uix_vobject3(false, name, ap);
    va_end(ap);

    /* Remember the name. */
    push_name(name, false);
}

/*
 * End a container object.
 */
void
uix_pop(void)
{
    uix_container_t *g = uix.container;

    uix.depth--;
    uprintf("%*s</%s>%s", XINDENT, "", g->name, XNL);
    uix.container = g->next;
    Free(g);
}

/* Open an XML leaf. */
void
uix_open_leaf(const char *name)
{
    assert(!uix.isopen);
    uprintf("%*s<%s", XINDENT, "", name);
    uix.isopen = true;
}

/* Close an XML leaf. */
void
uix_close_leaf(void)
{
    assert(uix.isopen);
    uprintf("/>%s", XNL);
    uix.isopen = false;
}

/* Add an element to the current container. */
void
ui_add_element(const char *tag, ui_attr_t attr, ...)
{
    va_list ap;
    const char *value;

    if (XML_MODE) {
	assert(uix.isopen);

	va_start(ap, attr);
	switch (attr) {
	case AT_STRING:
	    value = va_arg(ap, const char *);
	    if (value == NULL) {
		break;
	    }
	    uprintf(" %s=\"", tag);
	    xml_safe(value);
	    uprintf("\"");
	    break;
	case AT_INT:
	    uprintf(" %s=\"%"PRId64"\"", tag, va_arg(ap, int64_t));
	    break;
	case AT_SKIP_INT:
	    (void) va_arg(ap, int64_t);
	    break;
	case AT_DOUBLE:
	    uprintf(" %s=\"%g\"", tag, va_arg(ap, double));
	    break;
	case AT_BOOLEAN:
	    uprintf(" %s=\"%s\"", tag, ValTrueFalse(va_arg(ap, int)));
	    break;
	case AT_SKIP_BOOLEAN:
	    (void) va_arg(ap, int);
	    break;
	case AT_NODE:
	    assert(false);
	    break;
	}
	va_end(ap);
    } else {
	json_t *j = NULL;
	assert(uij.container != NULL);
	bool add = true;

	va_start(ap, attr);
	switch (attr) {
	case AT_STRING:
	    value = va_arg(ap, const char *);
	    if (value == NULL) {
		va_end(ap);
		return;
	    }
	    j = json_string(value, NT);
	    break;
	case AT_INT:
	    j = json_integer(va_arg(ap, int64_t));
	    break;
	case AT_SKIP_INT:
	    (void) va_arg(ap, int64_t);
	    add = false;
	    break;
	case AT_DOUBLE:
	    j = json_double(va_arg(ap, double));
	    break;
	case AT_BOOLEAN:
	    j = json_boolean(va_arg(ap, int));
	    break;
	case AT_SKIP_BOOLEAN:
	    (void) va_arg(ap, int);
	    add = false;
	    break;
	case AT_NODE:
	    j = va_arg(ap, json_t *);
	    if (j == NULL) {
		va_end(ap);
		return;
	    }
	}

	va_end(ap);
	if (add) {
	    uij_add_to_parent(tag, j);
	}
    }
}

/* JSON-specific functions. */

/* Add to a parent. */
static void
uij_add_to_parent(const char *name, json_t *j)
{
    json_t *parent = uij.container->j;

    /* Add to the parent container. */
    if (json_is_object(parent)) {
	assert(name != NULL);
	json_object_set(parent, name, NT, j);
    } else {
	assert(name == NULL);
	json_array_append(parent, j);
    }
}

/*
 * Add an empty container, and leave it open.
 */
static void
uij_open(const char *name, json_t *j)
{
    uij_container_t *jc;

    if (uij.container != NULL) {
	uij_add_to_parent(name, j);
    }

    /* Push this on the stack. */
    jc = Malloc(sizeof(uij_container_t));
    jc->j = j;
    jc->next = uij.container;
    uij.container = jc;
}

/*
 * Add an empty object, and leave it open.
 */
void
uij_open_object(const char *name)
{
    uij_open(name, json_object());
}

/*
 * Add an empty array, and leave it open.
 */
void
uij_open_array(const char *name)
{
    uij_open(name, json_array());
}

/* Close an open container. */
static void
uij_close(void)
{
    uij_container_t *jc = uij.container;

    if (jc->next == NULL) {
	char *s = json_write_o(jc->j, JW_OPTS);

	uprintf("%s\n", s);
	Free(s);
	json_free(jc->j);
    }
    uij.container = jc->next;
    Free(jc);
}

/* Close an open object. */
void
uij_close_object(void)
{
    assert(uij.container != NULL);
    assert(json_is_object(uij.container->j));
    uij_close();
}

/* Close an open array. */
void
uij_close_array(void)
{
    assert(uij.container != NULL);
    assert(json_is_array(uij.container->j));
    uij_close();
}

/* Action execution support. */

/* Data callback. */
static void
ui_action_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    ui_action_t *uia = (ui_action_t *)handle;

    if (XML_MODE) {
	size_t i;
	int nlines = 1;
	int j;
	char *re = success? ResFalse: ResTrue;

	/* Count the lines. */
	for (i = 0; i < len; i++) {
	    if (buf[i] == '\n') {
		nlines++;
	    }
	}

	if (uia->xresult) {
	    /* Extend the result. */
	    uia->xresult = Realloc(uia->xresult, strlen(uia->xresult) + 1 + len + 1);
	    sprintf(strchr(uia->xresult, '\0'), "\n%.*s", (int)len, buf);
	} else {
	    /* Create the result. */
	    uia->xresult = Asprintf("%.*s", (int)len, buf);
	    uia->xresult_err = Asprintf("%s", re);
	    nlines--; /* already added one in the line above */
	}

	/* Add needed error status. */
	for (j = 0; j < nlines; j++) {
	    uia->xresult_err = Realloc(uia->xresult_err, strlen(uia->xresult_err) + 1 + strlen(re) + 1);
	    sprintf(strchr(uia->xresult_err, '\0'), ",%s", re);
	}
    } else {
	const char *cur = buf;
	const char *start = buf;
	size_t len_left = len;

	if (uia->jresult == NULL) {
	    uia->jresult = json_array();
	    uia->jresult_err = json_array();
	}

	/*
	 * When completing a pass-through action, we went to the trouble of
	 * joining a returned array with newlines. Now undo that.
	 * There should really be a different internal API for this.
	 */
	while (len_left--) {
	    if (*cur++ == '\n') {
		json_array_append(uia->jresult, json_string(start, cur - start - 1));
		json_array_append(uia->jresult_err, json_boolean(!success));
		start = cur;
	    }
	}
	json_array_append(uia->jresult, json_string(start, cur - start));
	json_array_append(uia->jresult_err, json_boolean(!success));
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

    ui_leaf(IndRunResult,
	    AttrRTag, AT_STRING, uia->tag,
	    AttrSuccess, AT_BOOLEAN, success,
	    AttrText,
		XML_MODE? AT_STRING: AT_NODE,
		XML_MODE? uia->xresult: (char *)uia->jresult,
	    AttrTextErr,
		XML_MODE? AT_STRING: AT_NODE,
		XML_MODE? uia->xresult_err: (char *)uia->jresult_err,
	    AttrAbort, abort? AT_BOOLEAN: AT_SKIP_BOOLEAN, abort,
	    AttrTime, AT_DOUBLE, (double)msec / 1000.0,
	    NULL);
    if (XML_MODE) {
	Replace(uia->xresult, NULL);
	Replace(uia->xresult_err, NULL);
    }

    Free(uia);
    return true;
}

/* Emit a warning about an unknown attribute/member. */
static void
ui_unknown_attribute(const char *element, const char *attribute)
{
    ui_leaf(IndUiError,
	    AttrFatal, AT_BOOLEAN, false,
	    AttrText, AT_STRING, XML_MODE? "unknown attribute":
		"unknown member",
	    XML_MODE? AttrElement: AttrOperation, AT_STRING, element,
	    XML_MODE? AttrAttribute: AttrMember, AT_STRING, attribute,
	    AttrLine,
		XML_MODE? AT_INT: AT_SKIP_INT,
		XML_MODE? (int64_t)XML_GetCurrentLineNumber(uix.parser): 0,
	    AttrColumn,
		XML_MODE? AT_INT: AT_SKIP_INT,
		XML_MODE? (int64_t)XML_GetCurrentColumnNumber(uix.parser): 0,
	    NULL);
}

/* Emit a warning about a missing attribute/member. */
static void
ui_missing_attribute(const char *element, const char *attribute)
{
    ui_leaf(IndUiError,
	    AttrFatal, AT_BOOLEAN, false,
	    AttrText, AT_STRING, XML_MODE? "missing attribute":
		"missing member",
	    XML_MODE? AttrElement: AttrOperation, AT_STRING, element,
	    XML_MODE? AttrAttribute: AttrMember, AT_STRING, attribute,
	    AttrLine,
		XML_MODE? AT_INT: AT_SKIP_INT,
		XML_MODE? (int64_t)XML_GetCurrentLineNumber(uix.parser): 0,
	    AttrColumn,
		XML_MODE? AT_INT: AT_SKIP_INT,
		XML_MODE? (int64_t)XML_GetCurrentColumnNumber(uix.parser): 0,
	    NULL);
}

/* Emit a warning about a non-string attribute. */
static void
uij_non_string_attribute(const char *element, const char *attribute)
{
    ui_leaf(IndUiError,
	    AttrFatal, AT_BOOLEAN, false,
	    AttrText, AT_STRING, "member must be a string",
	    AttrOperation, AT_STRING, element,
	    AttrMember, AT_STRING, attribute,
	    NULL);
}

/* Get a string attribute. */
static char *
get_jstring(const json_t *j, const char *element, const char *attribute)
{
    const char *svalue;
    size_t slen;

    if (!json_is_string(j)) {
	uij_non_string_attribute(element, attribute);
	return NULL;
    }
    svalue = json_string_value(j, &slen);
    return txAsprintf("%.*s", (int)slen, svalue);
}

/* Run the command. */
static void
run_command(const char *tag, const char *type, const char *actions,
	cmd_t **cmds)
{
    ui_action_t *uia;
    tcb_t *tcb;

    uia = (ui_action_t *)Calloc(1, sizeof(ui_action_t) +
	    (tag? (strlen(tag) + 1): 0));
    uia->tag = tag? (char *)(uia + 1): NULL;
    if (tag) {
	strcpy(uia->tag, tag);
    }
    uia->xresult = NULL;
    uia->xresult_err = NULL;
    if (type != NULL && !strcasecmp(type, "keymap")) {
	tcb = &cb_keymap;
    } else if (type != NULL && !strcasecmp(type, "command")) {
	tcb = &cb_command;
    } else if (type != NULL && !strcasecmp(type, "macro")) {
	tcb = &cb_macro;
    } else if (type != NULL && !strcasecmp(type, "keypad")) {
	tcb = &cb_keypad;
    } else {
	tcb = &cb_ui;
    }
    if (actions != NULL) {
	push_cb(actions, strlen(actions), tcb, (task_cbh)uia);
    } else {
	push_cb_split(cmds, tcb, (task_cbh)uia);
    }
}

/* Execute the 'run' command, XML version. */
static void
do_run(const char *cmd, const char **attrs)
{
    const char *type = NULL;
    const char *tag = NULL;
    const char *command = NULL;
    int i;

    for (i = 0; attrs[i] != NULL; i += 2) {
	if (!strcasecmp(attrs[i], AttrType)) {
	    type = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrRTag)) {
	    tag = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrActions)) {
	    command = attrs[i + 1];
	} else {
	    ui_unknown_attribute(OperRun, attrs[i]);
	}
    }

    if (command == NULL) {
	ui_missing_attribute(OperRun, AttrActions);
	return;
    }

    /* Run the command. */
    run_command(tag, type, command, NULL);
}

/* Execute the 'run' command, JSON version. */
static void
do_jrun(json_t *j)
{
    const char *type = NULL;
    const char *tag = NULL;
    char *actions = NULL;
    const char *key;
    size_t key_length;
    const json_t *member;
    const char *svalue;
    size_t slen;
    cmd_t **cmds = NULL;

    if (!json_is_object(j)) {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING, OperRun " parameter must be an object",
		NULL);
	return;
    }

    BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, member) {
	if (json_key_matches(key, key_length, AttrType)) {
	    if ((type = get_jstring(member, OperRun, AttrType)) == NULL) {
		free_cmds(cmds);
		return;
	    }
	} else if (json_key_matches(key, key_length, AttrRTag)) {
	    if ((tag = get_jstring(member, OperRun, AttrRTag)) == NULL) {
		free_cmds(cmds);
		return;
	    }
	    svalue = json_string_value(member, &slen);
	    tag = txAsprintf("%.*s", (int)slen, svalue);
	} else if (json_key_matches(key, key_length, AttrActions)) {
	    char *errmsg;

	    if (!hjson_split(member, &cmds, &actions, &errmsg)) {
		ui_leaf(IndUiError,
			AttrFatal, AT_BOOLEAN, false,
			AttrText, AT_STRING, errmsg,
			AttrOperation, AT_STRING, OperRun,
			AttrMember, AT_STRING, AttrActions,
			NULL);
		Free(errmsg);
		return;
	    }
	} else {
	    ui_unknown_attribute(OperRun,
		    txAsprintf("%.*s", (int)key_length, key));
	}
    } END_JSON_OBJECT_FOREACH(j, key, key_length, member);

    if (actions == NULL && cmds == NULL) {
	ui_missing_attribute(OperRun, AttrActions);
	return;
    }

    /* Run the command. */
    run_command(tag, type, actions, cmds);
    Free(actions);
}

/* The (dummy) action for pass-through actions. */
static bool
Passthru_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned in_ix = 0;
    const char *passthru_tag;
    task_cbh *ret_cbh = NULL;

    /* Mark this action as waiting for a pass-through response. */
    passthru_tag = task_set_passthru(&ret_cbh);

    /* Tell the UI we are waiting. */
    if (XML_MODE) {
	const char **args =
	    (const char **)Malloc((4 + 2 + (argc * 2) + 1) * sizeof(char *));
	int out_ix = 0;

	args[out_ix++] = AttrAction;
	args[out_ix++] = current_action_name;
	args[out_ix++] = AttrPTag;
	args[out_ix++] = passthru_tag;
	if (ret_cbh != NULL) {
	    ui_action_t *uia = (ui_action_t *)ret_cbh;

	    if (uia->tag) {
		args[out_ix++] = AttrParentRTag;
		args[out_ix++] = uia->tag;
	    }
	}
	for (in_ix = 0; in_ix < argc; in_ix++) {
	    args[out_ix++] = txAsprintf(AttrArg "%d", in_ix + 1);
	    args[out_ix++] = argv[in_ix];
	}
	args[out_ix] = NULL;
	uix_object(true, IndPassthru, args);
	Free((void *)args);
    } else {
	uij_open_object(NULL);
	uij_open_object(IndPassthru);
	ui_add_element(AttrAction, AT_STRING, current_action_name);
	ui_add_element(AttrPTag, AT_STRING, passthru_tag);
	if (ret_cbh != NULL) {
	    ui_action_t *uia = (ui_action_t *)ret_cbh;

	    if (uia->tag) {
		ui_add_element(AttrParentRTag, AT_STRING, uia->tag);
	    }
	}
	if (argc > 0) {
	    uij_open_array(AttrArgs);
	    for (in_ix = 0; in_ix < argc; in_ix++) {
		ui_add_element(NULL, AT_STRING, argv[in_ix]);
	    }
	    uij_close_array();
	}
	uij_close_object();
	uij_close_object();
    }

    return true;
}

/* Register a command. */
static void
complete_register(const char *name, const char *help_text,
	const char *help_parms)
{
    size_t j;
    action_table_t *a;

    if (name == NULL) {
	ui_missing_attribute(OperRegister, AttrName);
	return;
    }
    for (j = 0; name[j]; j++) {
	if (!isprint((unsigned char)name[j])) {
	    ui_leaf(IndUiError,
		    AttrFatal, AT_BOOLEAN, false,
		    AttrText, AT_STRING, "invalid name",
		    XML_MODE? AttrElement: AttrOperation, AT_STRING,
			OperRegister,
		    AttrLine,
			XML_MODE? AT_INT: AT_SKIP_INT,
			XML_MODE?
			    (int64_t)XML_GetCurrentLineNumber(uix.parser): 0,
		    AttrColumn,
			XML_MODE? AT_INT: AT_SKIP_INT,
			XML_MODE?
			    (int64_t)XML_GetCurrentColumnNumber(uix.parser): 0,
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
    Free(a);
}

/* Register a command, XML version. */
static void
do_register(const char *cmd, const char **attrs)
{
    const char *name = NULL;
    const char *help_text = NULL;
    const char *help_parms = NULL;
    int i;

    for (i = 0; attrs[i] != NULL; i += 2) {
	if (!strcasecmp(attrs[i], AttrName)) {
	    name = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrHelpText)) {
	    help_text = attrs[i + 1];
	} else if (!strcasecmp(attrs[i], AttrHelpParms)) {
	    help_parms = attrs[i + 1];
	} else {
	    ui_unknown_attribute(OperRegister, attrs[i]);
	}
    }

    complete_register(name, help_text, help_parms);
}

/* Register a command, JSON version. */
static void
do_jregister(json_t *j)
{
    const char *name = NULL;
    const char *help_text = NULL;
    const char *help_parms = NULL;
    const char *key;
    size_t key_length;
    const json_t *member;

    if (!json_is_object(j)) {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING, OperRegister
		    " parameter must be an object",
		NULL);
	return;
    }

    BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, member) {
	if (json_key_matches(key, key_length, AttrName)) {
	    if ((name = get_jstring(member, OperRegister, AttrName)) == NULL) {
		return;
	    }
	} else if (json_key_matches(key, key_length, AttrHelpText)) {
	    if ((help_text = get_jstring(member, OperRegister, AttrHelpText))
		    == NULL) {
		return;
	    }
	} else if (json_key_matches(key, key_length, AttrHelpParms)) {
	    if ((help_parms = get_jstring(member, OperRegister,
			    AttrHelpParms)) == NULL) {
		return;
	    }
	} else {
	    ui_unknown_attribute(OperRegister,
		    txAsprintf("%.*s", (int)key_length, key));
	}
    } END_JSON_OBJECT_FOREACH(j, key, key_length, member);

    complete_register(name, help_text, help_parms);
}

/* Complete a pass-through command, XML version. */
void
do_passthru_complete(bool success, const char *cmd, const char **attrs)
{
    const char *tag = NULL;
    const char *text = NULL;
    int i;

    for (i = 0; attrs[i] != NULL; i += 2) {
	if (!strcasecmp(AttrPTag, attrs[i])) {
	    tag = attrs[i + 1];
	} else if (!strcasecmp(AttrText, attrs[i])) {
	    text = attrs[i + 1];
	} else {
	    ui_unknown_attribute(cmd, attrs[i]);
	}
    }

    if (tag == NULL) {
	ui_missing_attribute(cmd, AttrPTag);
	return;
    }

    if (!success && text == NULL) {
	ui_missing_attribute(cmd, AttrText);
	return;
    }


    /* Succeed. */
    task_passthru_done(tag, success, text);
}

/*
 * Ensure a JSON object is a string or an array of strings and return it as
 * a string or the array joined by newlines.
 */
static char *
jtext_array(const json_t *j, const char *element, const char *attribute)
{
    unsigned int i;
    char *ret = NULL;
    size_t len;

    if (json_is_string(j)) {
	return NewString(json_string_value(j, &len));
    }
    if (!json_is_array(j)) {
	goto fail;
    }
    for (i = 0; i < json_array_length(j); i++) {
	json_t *e = json_array_element(j, i);

	if (!json_is_string(e)) {
	    Free(ret);
	    goto fail;
	}
	if (ret != NULL) {
	    char *t = ret;

	    ret = Asprintf("%s\n%s", ret, json_string_value(e, &len));
	    Free(t);
	} else {
	    ret = NewString(json_string_value(e, &len));
	}
    }
    if (ret == NULL) {
	ret = NewString("");
    }
    return ret;

fail:
    ui_leaf(IndUiError,
	    AttrFatal, AT_BOOLEAN, false,
	    AttrText, AT_STRING,
		"member must be a string or array of strings",
	    AttrOperation, AT_STRING, element,
	    AttrMember, AT_STRING, attribute,
	    NULL);
    return NULL;
}

/* Complete a pass-through command, JSON version. */
static void
do_jpassthru_complete(const json_t *j, bool success)
{
    char *tag = NULL;
    char *text = NULL;
    const char *key;
    size_t key_length;
    const json_t *element;
    const char *cmd = success? OperSucceed: OperFail;

    if (!json_is_object(j)) {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING,
		    txAsprintf("%s parameter must be an object", cmd),
		NULL);
	return;
    }

    BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, element) {
	if (json_key_matches(key, key_length, AttrPTag)) {
	    if ((tag = get_jstring(element, cmd, AttrPTag)) == NULL) {
		Free(text);
		return;
	    }
	} else if (json_key_matches(key, key_length, AttrText)) {
	    if ((text = jtext_array(element, cmd, AttrText)) == NULL) {
		return;
	    }
	} else {
	    ui_unknown_attribute(cmd, txAsprintf("%.*s", (int)key_length, key));
	}
    } END_JSON_OBJECT_FOREACH(j, key, key_length, element);

    if (tag == NULL) {
	ui_missing_attribute(cmd, AttrPTag);
	Free(text);
	return;
    }

    if (!success && text == NULL) {
	ui_missing_attribute(cmd, AttrText);
	return;
    }

    /* Succeed. */
    task_passthru_done(tag, success, text);
    Free(text);
}

/*
 * Handle JSON input.
 * Returns a JSON parse error code.
 */
static json_errcode_t
handle_json_input(char *buf, size_t nr, size_t *offset)
{
    json_errcode_t errcode;
    json_t *result;
    json_parse_error_t *error;
    json_t *element;

    *offset = 0;

    /* Try parsing it as JSON. */
    errcode = json_parse(buf, nr, &result, &error);
    if (errcode != JE_OK) {
	*offset = error->offset;
	if (errcode == JE_INCOMPLETE) {
	    _json_free_error(error);
	    return errcode;
	}
	if (errcode != JE_EXTRA) {
	    int line = uij.line + error->line;
	    int column = uij.column + error->column;
	    ui_leaf(IndUiError,
		    AttrFatal, AT_BOOLEAN, true,
		    AttrText, AT_STRING, error->errmsg,
		    AttrLine, AT_INT, (int64_t)line,
		    AttrColumn, AT_INT, (uint64_t)column,
		    NULL);
	    fprintf(stderr, "Fatal JSON parsing error at input:%d:%d: %s\n",
		    uij.line + error->line,
		    uij.column + error->column,
		    error->errmsg);
	    x3270_exit(1);
	}
	_json_free_error(error);
    }

    /* Pick it apart. */
    if (json_is_string(result)) {
	/* Quick command syntax: string == run. */
	ui_action_t *uia = (ui_action_t *)Calloc(1, sizeof(ui_action_t));
	const char *command;
	size_t len;

	command = json_string_value(result, &len);

	push_cb(command, len, &cb_ui, (task_cbh)uia);
	json_free(result);
	return errcode;
    }

    if (!json_is_object(result) || json_object_length(result) != 1) {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING,
		    "Operation must be an object with one member",
		NULL);
	json_free(result);
	return errcode;
    }
    if (json_object_member(result, OperRun, NT, &element)) {
	do_jrun(element);
    } else if (json_object_member(result, OperRegister, NT, &element)) {
	do_jregister(element);
    } else if (json_object_member(result, OperSucceed, NT, &element)) {
	do_jpassthru_complete(element, true);
    } else if (json_object_member(result, OperFail, NT, &element)) {
	do_jpassthru_complete(element, false);
    } else {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING, "Unknown operation",
		NULL);
    }
    json_free(result);
    return errcode;
}

/* Count the newlines in a buffer. */
static int
count_newlines(const char *s, size_t len, int *column)
{
    int nnl = 0;

    while (len) {
	ucs4_t ucs4;
	int nr = utf8_to_unicode(s, len, &ucs4);

	assert(nr > 0);
	if (ucs4 == '\n') {
	    nnl++;
	    if (column != NULL) {
		*column = 0;
	    }
	} else if (column != NULL) {
	    (*column)++;
	}
	s += nr;
	len -= nr;
    }

    return nnl;
}

/* UI input processor. */
static void
process_input(const char *buf, ssize_t nr)
{
    if (XML_MODE) {

	/* Process the input in newline-delimited chunks. */
	while (true) {
	    ssize_t i = 0;

	    while (i < nr) {
		if (buf[i++] == '\n') {
		    break;
		}
	    }

	    if (uix.need_reset) {
		uix.need_reset = false;
		XML_ParserReset(uix.parser, "UTF-8");
		XML_SetElementHandler(uix.parser, xml_start, xml_end);
		XML_SetCharacterDataHandler(uix.parser, xml_data);
	    }

	    if (XML_Parse(uix.parser, buf, (int)i, 0) == 0) {
		ui_leaf(IndUiError,
			AttrFatal, AT_BOOLEAN, true,
			AttrText, AT_STRING, txAsprintf("XML parsing error: %s",
			    XML_ErrorString(XML_GetErrorCode(uix.parser))),
			AttrLine, AT_INT,
			    (int64_t)XML_GetCurrentLineNumber(uix.parser),
			AttrColumn, AT_INT,
			    (int64_t)XML_GetCurrentColumnNumber(uix.parser),
			NULL);
		fprintf(stderr, "Fatal XML parsing error: %s\n",
			XML_ErrorString(XML_GetErrorCode(uix.parser)));
		x3270_exit(1);
	    }

	    if (i >= nr) {
		break;
	    }

	    buf += i;
	    nr -= i;
	}

    } else {
	char *rs;	/* run start */
	char *ss;	/* scan start, may skip newlines */
	bool exhausted = false;

	if (uij.pending_input != NULL) {
	    char *p = uij.pending_input;

	    /* Handle multi-line input. */
	    uij.pending_input = Asprintf("%s%.*s", p, (int)nr, buf);
	    Free(p);
	} else {
	    uij.pending_input = Asprintf("%.*s", (int)nr, buf);
	}

	/*
	 * Input may arrive in awkward chunks: with a partial line or with
	 * multiple lines at once (embedded newlines).
	 */
	ss = rs = uij.pending_input;
	while (true) {
	    size_t len;
	    char *newline = strrchr(ss, '\n');
	    json_errcode_t errcode;
	    size_t offset;

	    if (newline == NULL) {
		/* No newline. Don't even try to parse it. */
		break;
	    }

	    /* Get ready to scan past this newline. */
	    len = newline - rs;
	    ss = newline + 1;

	    /* Parse and run. */
	    errcode = handle_json_input(rs, len, &offset);
	    if (errcode == JE_OK) {
		/* Complete, nothing extra. */
		uij.line += count_newlines(rs, len + 1, NULL);
		uij.column = 0;
		if (!*ss) {
		    /* Buffer is completely digested. */
		    exhausted = true;
		    break;
		}
		/* Buffer is partially digested. */
		rs = ss;
	    } else if (errcode == JE_EXTRA) {
		/* Successfully parsed, with extra data. */
		uij.line += count_newlines(rs, offset, &uij.column);
		memmove(uij.pending_input, uij.pending_input + offset,
			strlen(uij.pending_input + offset) + 1);
		rs = ss = uij.pending_input;
	    } else {
		/* Incomplete. */
		assert(errcode == JE_INCOMPLETE);
	    }
	}
	if (exhausted) {
	    Replace(uij.pending_input, NULL);
	}
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
    if (ui_socket != INVALID_SOCKET) {
	nr = recv(ui_socket, buf, INBUF_SIZE, 0);
    } else {
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
    }

    if (nr < 0) {
#if !defined(_WIN32) /*[*/
	popup_an_errno(errno, "UI input");
#else /*][*/
	popup_an_error("UI input: %s\n", win32_strerror(GetLastError()));
#endif /*]*/
	fprintf(stderr, "UI read error\n");
	x3270_exit(1);
    }
    if (nr == 0) {
	vtrace("UI input EOF, exiting\n");
	if (uix.input_nest) {
	    ui_leaf(IndUiError,
		    AttrFatal, AT_BOOLEAN, false,
		    AttrText, AT_STRING, "unclosed elements",
		    AttrCount, AT_INT, (int64_t)uix.input_nest,
		    AttrLine, AT_INT,
			(int64_t)XML_GetCurrentLineNumber(uix.parser),
		    AttrColumn, AT_INT,
			(int64_t)XML_GetCurrentColumnNumber(uix.parser),
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
    if (XML_MODE) {
	while (uix.container != NULL) {
	    uix_pop();
	}
    }
}

/**
 * XML start handler.
 */
static void
xml_start(void *userData _is_unused, const XML_Char *name,
	const XML_Char **atts)
{
    int i;

    uix.input_nest++;
    if (uix.input_nest - uix.master_doc > 1) {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING, "invalid nested element",
		AttrElement, AT_STRING, name,
		AttrLine, AT_INT, (int64_t)XML_GetCurrentLineNumber(uix.parser),
		AttrColumn, AT_INT,
		    (int64_t)XML_GetCurrentColumnNumber(uix.parser),
		NULL);
	return;
    }

    if (!strcasecmp(name, DocIn)) {
	for (i = 0; atts[i] != NULL; i += 2) {
	    ui_unknown_attribute(DocIn, atts[i]);
	}
	uix.master_doc = true;
    } else if (!strcasecmp(name, OperRun)) {
	do_run(name, atts);
    } else if (!strcasecmp(name, OperRegister)) {
	do_register(name, atts);
    } else if (!strcasecmp(name, OperSucceed)) {
	do_passthru_complete(true, name, atts);
    } else if (!strcasecmp(name, OperFail)) {
	do_passthru_complete(false, name, atts);
    } else {
	ui_leaf(IndUiError,
		AttrFatal, AT_BOOLEAN, false,
		AttrText, AT_STRING, "unrecognized element",
		AttrElement, AT_STRING, name,
		AttrLine, AT_INT, (int64_t)XML_GetCurrentLineNumber(uix.parser),
		AttrColumn, AT_INT,
		    (int64_t)XML_GetCurrentColumnNumber(uix.parser),
		NULL);
    }
}

/**
 * XML end handler.
 */
static void
xml_end(void *userData _is_unused, const XML_Char *name)
{
    if (!--uix.input_nest) {
	if (uix.master_doc) {
	    x3270_exit(0);
	}
	uix.need_reset = true;
    }
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

    ui_leaf(IndUiError,
	    AttrFatal, AT_BOOLEAN, false,
	    AttrText, AT_STRING, "ignoring plain text",
	    AttrLine, AT_INT, (int64_t)XML_GetCurrentLineNumber(uix.parser),
	    AttrColumn, AT_INT, (int64_t)XML_GetCurrentColumnNumber(uix.parser),
	    AttrCount, AT_INT, (int64_t)len,
	    NULL);
}

/**
 * Initialize the UI socket.
 */
void
ui_io_init(void)
{
    /* See if we need to call out or use stdin/stdout. */
    if (appres.scripting.callback != NULL) {
	struct sockaddr *sa;
	socklen_t sa_len;

	if (!parse_bind_opt(appres.scripting.callback, &sa, &sa_len)) {
	    Error("Cannot parse " ResCallback);
	}
	if ((ui_socket = socket(sa->sa_family, SOCK_STREAM, 0))
		== INVALID_SOCKET) {
#if !defined(_WIN32) /*[*/
	    perror("socket");
#else /*][*/
	    fprintf(stderr, "socket: %s\n", win32_strerror(WSAGetLastError()));
	    fflush(stdout);
#endif /*]*/
	    exit(1);
	}
	if (connect(ui_socket, sa, sa_len) < 0) {
#if !defined(_WIN32) /*[*/
	    perror("connect");
#else /*][*/
	    fprintf(stderr, "connect: %s\n", win32_strerror(WSAGetLastError()));
	    fflush(stdout);
#endif /*]*/
	    exit(1);
	}
	Free(sa);

	vtrace("Callback: connected to %s\n", appres.scripting.callback);
    }

    if (XML_MODE) {
	/* Create the XML parser. */
	uix.parser = XML_ParserCreate("UTF-8");
	if (uix.parser == NULL) {
	    Error("Cannot create XML parser");
	}
	XML_SetElementHandler(uix.parser, xml_start, xml_end);
	XML_SetCharacterDataHandler(uix.parser, xml_data);
    }

#if !defined(_WIN32) /*[*/
    AddInput((ui_socket != INVALID_SOCKET)? ui_socket: fileno(stdin), ui_input);
#else /*][*/
    /* Set up the peer thread. */
    if (ui_socket != INVALID_SOCKET) {
	HANDLE ui_socket_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	WSAEventSelect(ui_socket, ui_socket_event, FD_READ | FD_CLOSE);
	AddInput(ui_socket_event, ui_input);
    } else {
	peer_enable_event = CreateEvent(NULL, FALSE, TRUE, NULL);
	assert(peer_enable_event != INVALID_HANDLE_VALUE);
	peer_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(peer_done_event != INVALID_HANDLE_VALUE);
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
	AddInput(peer_done_event, ui_input);
    }
#endif /*]*/

    if (XML_MODE && appres.b3270.wrapper_doc) {
	/* Start the XML stream. */
	uprintf("%c%c%c<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
		0xef, 0xbb, 0xbf);
	uix_push(DocOut, NULL);
	if (!appres.b3270.indent) {
	    uprintf("\n");
	}
    }

    /* Set up a handler for exit. */
    register_schange_ordered(ST_EXITING, ui_exiting, ORDER_LAST);
}
