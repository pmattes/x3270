/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *      run_action.c
 *              Floating action invocation.
 */

#include "globals.h"

#include "wincmn.h"
#include <errno.h>
#include <fcntl.h>

#include "actions.h"
#include "idle.h"
#include "kybd.h"
#include "popups.h"
#include "source.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "varbuf.h"
#include "w3misc.h"
#include "xio.h"

static void action_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool action_done(task_cbh handle, bool success, bool abort);

/* Action context. */
typedef struct {
    llist_t llist;	/* list linkage */
    char *result;	/* accumulated result */
    size_t result_len;
    enum iaction ia;	/* cause */
} action_context_t;
static llist_t action_contexts = LLIST_INIT(action_contexts);

/* Action callback collection. */
typedef struct {
    llist_t llist;	/* linkage */
    tcb_t *cb;	/* callback block */
} action_cb_t;
static llist_t action_cbs = LLIST_INIT(action_cbs);

/**
 * Callback for data returned to keymap.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
action_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    action_context_t *k;
    bool found = false;

    FOREACH_LLIST(&action_contexts, k, action_context_t *) {
	if (k == (action_context_t *)handle) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&action_contexts, k, action_context_t *);

    if (!found) {
	vtrace("action_data: no match\n");
	return;
    }

    k->result = Realloc(k->result, k->result_len + 1 + len + 1);
    if (k->result_len) {
	k->result[k->result_len++] = '\n';
    }
    strncpy(k->result + k->result_len, buf, len);
    k->result[k->result_len + len] = '\0';
    k->result_len += len;
}

/**
 * Callback for completion of one command executed from a keymap.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if context is complete
 */
static bool
action_done(task_cbh handle, bool success, bool abort)
{
    action_context_t *k;
    bool found = false;

    FOREACH_LLIST(&action_contexts, k, action_context_t *) {
	if (k == (action_context_t *)handle) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&action_contexts, k, action_context_t *);

    if (!found) {
	vtrace("action_data: no match\n");
	return true;
    }

    if (success) {
	if (k->result) {
	    popup_an_info("%s", k->result);
	}
    } else {
	if (k->result) {
	    popup_an_error("%s", k->result);
	} else {
	    popup_an_error("%s failed", ia_name[k->ia]);
	}
    }

    if (k->result) {
	Free(k->result);
    }
    llist_unlink(&k->llist);
    Free(k);

    /* Yes, done. */
    return true;
}

/**
 * Push an action.
 *
 * @param[in] ia	Cause.
 * @param[in] s		Text of action.
 */
static void
push_action(enum iaction ia, char *s)
{
    action_cb_t *acb;
    tcb_t *cb;
    bool found = false;
    action_context_t *k;

    /* Find a callback block. */
    FOREACH_LLIST(&action_cbs, acb, action_cb_t *) {
	if (acb->cb->ia == ia) {
	    cb = acb->cb;
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&action_cbs, acb, action_cb_t *);

    if (!found) {
	acb = (action_cb_t *)Calloc(sizeof(action_cb_t) + sizeof(tcb_t), 1);
	llist_init(&acb->llist);
	acb->cb = cb = (tcb_t *)(acb + 1);
	LLIST_APPEND(&acb->llist, action_cbs);
	cb->shortname = ia_name[ia];
	cb->ia = ia;
	cb->flags = CB_NEW_TASKQ | CB_UI;
	cb->data = action_data;
	cb->done = action_done;
	cb->run = NULL;
    }

    /* Set up a context. */
    k = (action_context_t *)Malloc(sizeof(action_context_t));
    llist_init(&k->llist);
    k->result = NULL;
    k->result_len = 0;
    k->ia = ia;
    LLIST_APPEND(&k->llist, action_contexts);

    /* Push a callback with a macro. */
    push_cb(s, strlen(s), cb, (task_cbh)k);
}

/**
 * Push a floating keymap action.
 *
 * @param[in] s		Text of action.
 */
void
push_keymap_action(char *s)
{
    push_action(IA_KEYMAP, s);
}

/**
 * Push a floating macro.
 *
 * @param[in] s		Text of action.
 */
void
push_macro(char *s)
{
    push_action(IA_MACRO, s);
}

/**
 * Push a floating keypad action.
 *
 * @param[in] s		Text of action.
 */
void
push_keypad_action(char *s)
{
    push_action(IA_KEYPAD, s);
}

/**
 * Run an action.
 *
 * @param[in] name	Action name
 * @param[in] cause	Cause
 * @param[in] parm1	First parameter (optional)
 * @param[in] parm2	Second parameter (optional)
 *
 * @return true
 */
bool
run_action(const char *name, enum iaction cause, const char *parm1,
	const char *parm2)
{
    if (!parm1) {
	push_action(cause, txAsprintf("%s()", name));
    } else if (!parm2) {
	push_action(cause, txAsprintf("%s(%s)", name, parm1));
    } else {
	push_action(cause, txAsprintf("%s(%s,%s)", name, parm1, parm2));
    }
    return true;
}

/**
 * Format a parameter for safe consumption as a parameter.
 *
 * @param[in] s		Parameter
 *
 * @return Possibly-quoted copy
 */
char *
safe_param(const char *s)
{
    varbuf_t r;
    char c;
    char *ret;
    bool quoted = false;

    if (strcspn(s, " ,()\\\b\f\r\n\t\v\"") == strlen(s)) {
	/* Safe already. */
	return (char *)s;
    }

    /* Quote it. */
    vb_init(&r);
    vb_appends(&r, "\"");
    while ((c = *s++)) {
	if (quoted) {
	    /* Pass the backslash and whatever follows. */
	    vb_appends(&r, "\\");
	    vb_append(&r, &c, 1);
	    quoted = false;
	} else {
	    if (c == '\\') {
		/* Remember a backslash. */
		quoted = true;
	    } else if (c == '"') {
		/* Double quotes need to be escaped. */
		vb_appends(&r, "\\\"");
	    } else {
		/* Pass through anything else. */
		vb_append(&r, &c, 1);
	    }
	}
    }

    if (quoted) {
	/* Trailing backslash must be quoted. */
	vb_appends(&r, "\\\\");
    }

    vb_appends(&r, "\"");
    ret = vb_consume(&r);
    txdFree(ret);
    return ret;
}

/**
 * Run an action, given an array of parameters.
 *
 * @param[in] name	Action name
 * @param[in] cause	Cause
 * @param[in] count	Parameter count
 * @param[in] parms	Parameters
 *
 * @return true
 */
bool
run_action_a(const char *name, enum iaction cause, unsigned count,
	const char **parms)
{
    varbuf_t r;
    unsigned i;
    char *s;

    vb_init(&r);
    vb_appendf(&r, "%s(", name);
    for (i = 0; i < count; i++) {
	vb_appendf(&r, "%s%s", (i > 0)? ",": "", safe_param(parms[i]));
    }
    vb_appends(&r, ")");
    s = vb_consume(&r);
    push_action(cause, s);
    Free(s);

    return true;
}
