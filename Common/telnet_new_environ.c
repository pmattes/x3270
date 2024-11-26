/*
 * Copyright (c) 2017-2024 Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used to
 *       endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE US* OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	telnet_new_environ.c
 *		The TELNET NEW-ENVIRON option (RFC 1572).
 */

#include "globals.h"

#include "arpa_telnet.h"
#include "tn3270e.h"
#include "3270ds.h"

#include "appres.h"

#include "devname.h"
#include "resources.h"
#include "sio.h"
#include "telnet.h"
#include "telnet_core.h"
#include "telnet_private.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "varbuf.h"

#include "telnet_new_environ.h"

#define ESCAPED(c)	\
    (c == TELOBJ_VAR || c == TELOBJ_USERVAR || c == TELOBJ_ESC || \
     c == TELOBJ_VALUE)
#define USER_VARNAME		"USER"
#define DEVNAME_USERVARNAME	"DEVNAME"
#define IBMELF_VARNAME		"IBMELF"
#define IBMAPPLID_VARNAME	"IBMAPPLID"
#define IBMELF_YES		"YES"
#define IBMAPPLID_NONE		"None"

/* Globals */

/* Statics */
static const char *telobjs[4] = { "VAR", "VALUE", "ESC", "USERVAR" };

/* An environment variable. */
typedef struct {
    llist_t list;
    char *name;
    size_t name_len;
    char *value;
    size_t value_len;
    devname_t *devname;
} environ_t;

static llist_t vars = LLIST_INIT(vars);
static llist_t uservars = LLIST_INIT(uservars);

/* A request. */
typedef struct {
    llist_t list;
    int group;		/* TELOBJ_VAR or TELOBJ_USERVAR */
    char *name;		/* Variable name (including escapes), or NULL for all */
    size_t name_len;	/* Name length */
} ereq_t;

/* Compute the length of a quoted environment name or value. */
size_t
escaped_len(const char *s, size_t len)
{
    size_t ret = 0;
    while (len--) {
	char c = *s++;

	ret += 1 + ESCAPED(c);
    }
    return ret;
}

/* Copy and quote an environment name or value. */
void
escaped_copy(char *to, const char *from, size_t len)
{
    while (len--) {
	char c = *from++;

	if (ESCAPED(c)) {
	    *to++ = TELOBJ_ESC;
	}
	*to++ = c;
    }
}

/* Add a value to an environment list. */
static environ_t *
add_environ(llist_t *list, const char *name, const char *value)
{
    size_t name_len = strlen(name);
    size_t name_xlen = escaped_len(name, name_len);
    size_t value_len = strlen(value);
    size_t value_xlen = escaped_len(value, value_len);

    /* Add it to the list. */
    environ_t *e = (environ_t *)Calloc(sizeof(environ_t) + name_xlen + value_xlen, 1);

    e->name = (char *)(e + 1);
    escaped_copy(e->name, name, name_len);
    e->name_len = name_xlen;
    e->value = e->name + name_xlen;
    escaped_copy(e->value, value, value_len);
    e->value_len = value_xlen;

    llist_init(&e->list);
    llist_insert_before(&e->list, list);
    return e;
}

/*
 * Find a value on an environment list.
 * Returns it in escaped format.
 */
static environ_t *
find_environ(llist_t *list, const char *name, size_t namelen)
{
    environ_t *e;

    /* We have no variables with embedded nulls in their names. */
    if (memchr(name, 0, namelen) != NULL) {
	return NULL;
    }

    /* Search for a match. */
    FOREACH_LLIST(list, e, environ_t *) {
	if (!memcmp(name, e->name, namelen)) {
	    return e;
	}
    } FOREACH_LLIST_END(list, e, environ_t *);
    return NULL;
}

/* Reset an environment variable list. */
static void
environ_clear(llist_t *list)
{
    while (!llist_isempty(list)) {
	llist_t *l = list->next;
	environ_t *e;

	llist_unlink(l);
	e = (environ_t *)l;
	if (e->devname != NULL) {
	    devname_free(e->devname);
	}
	Free(e);
    }
}

/* Initialize the NEW-ENVIRON variables. */
void
environ_init(void)
{
    char *user;
    char *ibmapplid;

    /* Clean up from last time. */
    environ_clear(&vars);
    environ_clear(&uservars);

    user = host_user? host_user: (appres.user? appres.user: getenv("USER"));
    if (user == NULL) {
	user = getenv("USERNAME");
    }
    if (user == NULL) {
	user = "UNKNOWN";
    }
    add_environ(&vars, USER_VARNAME, user);
    if (appres.devname != NULL) {
	environ_t *e = add_environ(&uservars, DEVNAME_USERVARNAME, appres.devname);
	e->devname = devname_init(appres.devname);
    }
    add_environ(&uservars, IBMELF_VARNAME, IBMELF_YES);
    ibmapplid = getenv(IBMAPPLID_VARNAME);
    if (ibmapplid == NULL) {
	ibmapplid = IBMAPPLID_NONE;
    }
    add_environ(&uservars, IBMAPPLID_VARNAME, ibmapplid);
}

/* Expand a name into a readable string. */
static char *
expand_name(const char *s, size_t len)
{
    varbuf_t v;
    unsigned char c;

    vb_init(&v);
    while (len--) {
	c = (unsigned char)*s++;
	if (c == TELOBJ_ESC) {
	    if (len == 1) {
		break;
	    }
	    c = (unsigned char)*s++;
	    len--;
	}
	if (c == '\\') {
	    vb_appends(&v, "\\\\");
	} else if (c < ' ' || c >= 0x7f) {
	    vb_appendf(&v, "\\u%04x", c);
	} else {
	    vb_append(&v, (char *)&c, 1);
	}
    }
    return txdFree(vb_consume(&v));
}

/* Expand IACs in a reply buffer. */
static void
expand_iac(const unsigned char *raw, size_t raw_len, unsigned char **result,
	size_t *result_len)
{
    int iacs = 0;
    const unsigned char *remain = raw;
    size_t remain_len = raw_len;
    void *iac;
    unsigned char *out;

    while ((iac = memchr(remain, IAC, remain_len)) != NULL) {
	iacs++;
	remain = (unsigned char *)iac + 1;
	if (remain >= raw + raw_len) {
	    break;
	}
	remain_len = (raw + raw_len) - remain;
    }

    *result_len = raw_len + iacs;
    *result = Malloc(*result_len);
    out = *result;
    while (raw_len--) {
	unsigned char c = *raw++;

	if (c == IAC) {
	    *out++ = IAC;
	}
	*out++ = c;
    }
}

/*
 * Parse the TELNET NEW-ENVIRON option.
 *
 * @param[in] request_buf	TELNET IAC SB buffer. Leading IAC SB
 * 				NEW-ENVIRON SEND and trailing IAC SE have been
 * 				 removed. Embedded IACs have been removed.
 * @param[in] request_buflen	Length of the request buffer.
 * @param[out] fake_input	Returned true if input was faked (empty
 * 				 request).
 * @returns List of requests, or null if there was an error.
 */
static llist_t *
parse_new_environ(unsigned char *request_buf, size_t request_buflen,
	bool *fake_input)
{
    size_t i;
    enum {
	EE_BASE,		/* base state */
	EE_VAR,			/* VAR or USERVAR seen */
	EE_NAME,		/* name character seen */
	EE_NAME_ESC		/* ESC seen in name */
    } state = EE_BASE;
    static llist_t ereqs;	/* returned parsed request */
    ereq_t *ereq = NULL;	/* current request (group or variable) */

    *fake_input = false;
    llist_init(&ereqs);

    /* Parse the input into a series of requests. */
    for (i = 0; i < request_buflen; i++) {

	unsigned char c = request_buf[i];

	switch (state) {

	case EE_BASE:
	    switch (c) {
	    case TELOBJ_VAR:
	    case TELOBJ_USERVAR:
		/* New request is pending. */
		ereq = (ereq_t *)Malloc(sizeof(ereq_t));
		memset(ereq, 0, sizeof(ereq_t));
		llist_init(&ereq->list);
		ereq->group = c;
		state = EE_VAR;
		break;
	    default:
		/* Only those two are allowed. */
		return NULL;
		break;
	    }
	    break;

	case EE_VAR:
	    switch (c) {
	    case TELOBJ_VAR:
	    case TELOBJ_USERVAR:
		/* The previous request is done. */
		llist_insert_before(&ereq->list, &ereqs);
		/* Start a new one. */
		ereq = (ereq_t *)Malloc(sizeof(ereq_t));
		memset(ereq, 0, sizeof(ereq_t));
		llist_init(&ereq->list);
		ereq->group = c;
		break;
	    default:
		ereq->name = Malloc(1);
		ereq->name[0] = c;
		ereq->name_len = 1;
		state = (c == TELOBJ_ESC)? EE_NAME_ESC: EE_NAME;
		break;
	    }
	    break;

	case EE_NAME:
	    switch (c) {
	    case TELOBJ_VAR:
	    case TELOBJ_USERVAR:
		/* The previous request is done. */
		llist_insert_before(&ereq->list, &ereqs);
		/* Start a new one. */
		ereq = (ereq_t *)Malloc(sizeof(ereq_t));
		memset(ereq, 0, sizeof(ereq_t));
		llist_init(&ereq->list);
		ereq->group = c;
		state = EE_VAR;
		break;
	    case TELOBJ_ESC:
		state = EE_NAME_ESC;
		/* fall through... */
	    default:
		ereq->name_len++;
		ereq->name = Realloc(ereq->name, ereq->name_len);
		ereq->name[ereq->name_len - 1] = c;
		break;
	    }
	    break;

	case EE_NAME_ESC:
	    ereq->name_len++;
	    ereq->name = Realloc(ereq->name, ereq->name_len);
	    ereq->name[ereq->name_len - 1] = c;
	    break;
	}
    }

    if (state == EE_BASE) {
	/* No input. Fake TELOBJ_VER and TELOBJ_USERVAR. */
	ereq = (ereq_t *)Malloc(sizeof(ereq_t));
	memset(ereq, 0, sizeof(ereq_t));
	llist_init(&ereq->list);
	ereq->group = TELOBJ_VAR;
	llist_insert_before(&ereq->list, &ereqs);

	ereq = (ereq_t *)Malloc(sizeof(ereq_t));
	memset(ereq, 0, sizeof(ereq_t));
	llist_init(&ereq->list);
	ereq->group = TELOBJ_USERVAR;
	llist_insert_before(&ereq->list, &ereqs);

	*fake_input = true;
    } else {
	/* Something is pending. */
	llist_insert_before(&ereq->list, &ereqs);
    }

    return &ereqs;
}

/*
 * Parse the TELNET NEW-ENVIRON option and form the response.
 *
 * @param[in] request_buf	TELNET IAC SB buffer. Leading IAC SB
 * 				NEW-ENVIRON SEND and trailing IAC SE have been
 * 				 removed. Embedded IACs have been removed.
 * @param[in] request_buflen	Length of the buffer.
 * @param[out] reply_buf	Returned malloc'd transmit buffer, starting
 * 				 with IAC SB, ending with IAC SE.
 * @param[out] reply_buflen	Returned buffer length.
 * @param[out] trace_inp	Returned malloc'd input trace message.
 * @param[out] trace_outp	Returned malloc'd output trace message.
 * @returns true for success
 */
bool
telnet_new_environ(unsigned char *request_buf, size_t request_buflen,
	unsigned char **reply_buf, size_t *reply_buflen,
	char **trace_inp, char **trace_outp)
{
    varbuf_t trace_in;	/* input trace */
    varbuf_t reply;	/* reply */
    varbuf_t trace_out;	/* output trace */
    llist_t *ereqs;	/* parsed request */
    ereq_t *ereq;	/* request element */
    environ_t *value;	/* found value */
    bool fake_input = false;
    unsigned char *reply_body;
    size_t reply_body_len;

    /* Parse the request. */
    ereqs = parse_new_environ(request_buf, request_buflen, &fake_input);
    if (ereqs == NULL) {
	/* Parse error. */
	return false;
    }

    /* Build up the return values. */
    vb_init(&trace_in);
    vb_appendf(&trace_in, "%s %s", opt(TELOPT_NEW_ENVIRON),
	    telquals[TELQUAL_SEND]);

    vb_init(&reply);
    vb_appendf(&reply, "%c%c", TELOPT_NEW_ENVIRON, TELQUAL_IS);

    vb_init(&trace_out);
    vb_appendf(&trace_out, "%s %s %s", cmd(SB), opt(TELOPT_NEW_ENVIRON),
	    telquals[TELQUAL_IS]);

    FOREACH_LLIST(ereqs, ereq, ereq_t *) {
	if (ereq->name_len == 0) {
	    llist_t *l;

	    /* No variable name. Dump the whole group. */
	    if (!fake_input) {
		/* Trace the request. */
		vb_appendf(&trace_in, " %s", telobjs[ereq->group]);
	    }

	    l = (ereq->group == TELOBJ_VAR)? &vars: &uservars;
	    FOREACH_LLIST(l, value, environ_t *) {
		/* Add this value to the reply. */
		vb_appendf(&reply, "%c", ereq->group);
		vb_append(&reply, value->name, value->name_len);
		vb_appendf(&reply, "%c", TELOBJ_VALUE);
		vb_append(&reply, value->value, value->value_len);

		/* Trace the reply. */
		vb_appendf(&trace_out, " %s \"%s\" %s \"%s\"",
			telobjs[ereq->group],
			expand_name(value->name, value->name_len),
			telobjs[TELOBJ_VALUE],
			expand_name(value->value, value->value_len));
	    } FOREACH_LLIST_END(l, ereq, ereq_t *)
	} else {
	    environ_t *value;
	    const char *dnext = NULL;

	    /* Trace thr request. */
	    vb_appendf(&trace_in, " %s \"%s\"", telobjs[ereq->group],
		    expand_name(ereq->name, ereq->name_len));

	    /* Dump one entry. */
	    value = find_environ(
		    (ereq->group == TELOBJ_VAR)? &vars : &uservars, ereq->name,
		    ereq->name_len);

	    vb_appendf(&reply, "%c", ereq->group);
	    vb_append(&reply, ereq->name, ereq->name_len);
	    if (value != NULL) {
		vb_appendf(&reply, "%c", TELOBJ_VALUE);
		if (value->devname != NULL) {
		    dnext = devname_next(value->devname);

		    vb_append(&reply, dnext, strlen(dnext));
		} else {
		    vb_append(&reply, value->value, value->value_len);
		}
	    }

	    /* Trace the reply, */
	    vb_appendf(&trace_out, " %s \"%s\"",
		    telobjs[ereq->group],
		    expand_name(ereq->name, ereq->name_len));
	    if (value != NULL) {
		vb_appendf(&trace_out, " %s \"%s\"",
		    telobjs[TELOBJ_VALUE],
		    (dnext != NULL)?
			expand_name(dnext, strlen(dnext)):
			expand_name(value->value, value->value_len));
	    }
	}
    } FOREACH_LLIST_END(ereqs, ereq, ereq_t);

    /* Trace SE in and out. */
    vb_appendf(&trace_in, " %s", cmd(SE));
    vb_appendf(&trace_out, " %s", cmd(SE));

    /* Free the parsed request. */
    while (!llist_isempty(ereqs)) {
	ereq = (ereq_t *)ereqs->next;
	llist_unlink(&ereq->list);
	if (ereq->name != NULL) {
	    Free(ereq->name);
	}
	Free(ereq);
    }

    /* Expand IACs, which may be hiding in names or values. */
    expand_iac((const unsigned char *)vb_buf(&reply), vb_len(&reply),
	    &reply_body, &reply_body_len);
    vb_free(&reply);

    /* Form the final reply message: IAC SB, reply body, IAC SE. */
    *reply_buflen = 2 + reply_body_len + 2;
    *reply_buf = Malloc(2 + reply_body_len + 2);
    **reply_buf = IAC;
    *(*reply_buf + 1) = SB;
    memcpy(*reply_buf + 2, reply_body, reply_body_len);
    *(*reply_buf + 2 + reply_body_len) = IAC;
    *(*reply_buf + 2 + reply_body_len + 1) = SE;
    Free(reply_body);

    /* Return the traces. */
    *trace_inp = vb_consume(&trace_in);
    *trace_outp = vb_consume(&trace_out);
    return true;
}

/**
 * Toggle a simple string.
 * @param[in] name	Toggle name.
 * @param[in] value	Toggle value.
 * @param[in] flags	Flags.
 * @param[in] ia	Source of the operation.
 * @returns success/failure/deferred
 */
static toggle_upcall_ret_t
toggle_string(const char *name, const char *value, unsigned flags, ia_t ia)
{
    char **target;

    if (!strcasecmp(name, ResUser)) {
	target = &appres.user;
    } else if (!strcasecmp(name, ResDevName)) {
	target = &appres.devname;
    } else {
	return TU_FAILURE;
    }

    if (value == NULL || !value[0]) {
	Replace(*target, NULL);
    } else {
	Replace(*target, NewString(value));
    }

    return TU_SUCCESS;
}

/**
 * New-environment module registration.
 */
void
telnet_new_environ_register(void)
{
    /* Register the toggles. */
    register_extended_toggle(ResUser, toggle_string, NULL, NULL, (void **)&appres.user, XRM_STRING);
    register_extended_toggle(ResDevName, toggle_string, NULL, NULL, (void **)&appres.devname, XRM_STRING);
}
