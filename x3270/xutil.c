/*
 * Copyright (c) 1993-2026 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes nor the names of their
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND JEFF SPARKES "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR JEFF SPARKES BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	xutil.c
 *		Utility functions for x3270
 */

#include "globals.h"
#include "xglobals.h"
#include <assert.h>
#include <fcntl.h>
#include <time.h>

#include "utils.h"

/* Glue between x3270 and the X libraries. */

char *fallbacks[] = { NULL };

/*
 * Get an arbitrarily-named resource.
 */
char *
get_underlying_resource(const char *resource)
{
    char *tlname;	/* top-level name */
    char *fq_resource;	/* fully-qualified resource name */
    char *lcomp;	/* last component in resource name */
    char *fq_class;	/* fully-qualified class name */
    char *type;		/* resource type */
    XrmValue value;	/* resource value */
    char *r = NULL;	/* returned value */

    if (toplevel == NULL) {
	return NULL;
    }

    /* Find the toplevel name. */
    tlname = XtName(toplevel);

    /* Figure out the full resource name. */
    fq_resource = Asprintf("%s.%s", tlname, resource);

    /* Figure out the full class name. */
    fq_class = XtNewString(fq_resource);
    lcomp = strrchr(fq_class, '.') + 1;
    *lcomp = toupper((unsigned char)*lcomp);

    /* Look up the resource. */
    if (XrmGetResource(rdb, fq_resource, fq_class, &type, &value) == True &&
	    *value.addr) {
        r = value.addr;
    }

    /* Clean up and return the value. */
    XtFree(fq_class);
    XtFree(fq_resource);
    return r;
}

/*
 * Input callbacks.
 */
typedef void voidfn(void);

typedef struct iorec {
    iofn_t 	  fn;
    XtInputId	  id;
    struct iorec *next;
} iorec_t;

static iorec_t *iorecs = NULL;

static void
io_fn(XtPointer closure, int *source, XtInputId *id)
{
    iorec_t *iorec;

    for (iorec = iorecs; iorec != NULL; iorec = iorec->next) {
	if (iorec->id == *id) {
	    (*iorec->fn)(*source, *id);
	    break;
	}
    }
}

ioid_t
AddInput(iosrc_t sock, iofn_t fn)
{
    iorec_t *iorec;

    iorec = (iorec_t *)XtMalloc(sizeof(iorec_t));
    iorec->fn = fn;
    iorec->id = XtAppAddInput(appcontext, sock, (XtPointer)XtInputReadMask,
	    io_fn, NULL);

    iorec->next = iorecs;
    iorecs = iorec;

    return iorec->id;
}

ioid_t
AddExcept(socket_t sock, iofn_t fn)
{
    iorec_t *iorec;

    iorec = (iorec_t *)XtMalloc(sizeof(iorec_t));
    iorec->fn = fn;
    iorec->id = XtAppAddInput(appcontext, sock, (XtPointer)XtInputExceptMask,
	    io_fn, NULL);
    iorec->next = iorecs;
    iorecs = iorec;

    return iorec->id;
}

ioid_t
AddOutput(socket_t sock, iofn_t fn)
{
    iorec_t *iorec;

    iorec = (iorec_t *)XtMalloc(sizeof(iorec_t));
    iorec->fn = fn;
    iorec->id = XtAppAddInput(appcontext, sock, (XtPointer)XtInputWriteMask,
	    io_fn, NULL);
    iorec->next = iorecs;
    iorecs = iorec;

    return iorec->id;
}

void
RemoveInput(ioid_t cookie)
{
    iorec_t *iorec;
    iorec_t *prev = NULL;

    for (iorec = iorecs; iorec != NULL; iorec = iorec->next) {
	if (iorec->id == cookie) {
	    break;
	}
	prev = iorec;
    }

    if (iorec != NULL) {
	XtRemoveInput(cookie);
	if (prev != NULL) {
	    prev->next = iorec->next;
	} else {
	    iorecs = iorec->next;
	}
	XtFree((XtPointer)iorec);
    }
}

void
RemoveOutput(ioid_t cookie)
{
    RemoveInput(cookie);
}

void
RemoveExcept(ioid_t cookie)
{
    RemoveInput(cookie);
}

/*
 * Timer callbacks.
 */
static int to_pipe[2] = { -1, -1 };
static unsigned long to_next_seq = 1;

/* Timeout record. */
typedef struct torec {
    llist_t		llist;		/* linkage */
    struct timespec	expires;	/* monotonic expiration time */
    tofn_t	  	fn;		/* callback function */
    XtIntervalId	id;		/* Xt identifier */
    unsigned long	seq;		/* sequence number */
} torec_t;

llist_t torecs = LLIST_INIT(torecs);

/**
 * Timeout callback function.
 * @param[in] closure	Context
 * @param[in] id	Identifier
 */
static void
to_fn(XtPointer closure, XtIntervalId *id)
{
    torec_t *torec;
    bool found = false;
    FOREACH_LLIST(&torecs, torec, torec_t *) {
	if (torec->id == *id) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&torecs, torec, torec_t *);

    /* Write the sequence number to the pipe. */
    if (found) {
	ssize_t nw = write(to_pipe[1], &torec->seq, sizeof(torec->seq));
	assert(nw == sizeof(unsigned long));
    }
}

/**
 * Timeout input function (input from the pipe).
 * @param[in] closure	Context (unused)
 * @param[in] source	Input source (unused)
 * @param[in] id	Idenfier (unused)
 */
static void
to_input(XtPointer closure, int *source, XtInputId *id)
{
    /* Read in the sequence number. */
    unsigned long seq;
    ssize_t nr = read(to_pipe[0], &seq, sizeof(seq));
    assert(nr == sizeof(seq));

    torec_t *torec;
    bool found = false;
    FOREACH_LLIST(&torecs, torec, torec_t *) {
	if (torec->seq == seq) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&torecs, torec, torec_t *);

    if (found) {
	llist_unlink(&torec->llist);
	(torec->fn)((ioid_t)torec->seq);
	Free(torec);
    }
}

/**
 * Add a timeout.
 * @param[in] msec	Interval, in milliseconds
 * @param[in] fn	Callback function
 * @returns I/O identifier
 */
ioid_t
AddTimeOut(unsigned long msec, tofn_t fn)
{
    if (to_pipe[0] < 0) {
	if (pipe(to_pipe) < 0) {
	    XtError("AddTimeOut: pipe() failed");
	}
	if (fcntl(to_pipe[0], F_SETFD, 1) < 0) {
	    XtError("AddTimeOut: Can't set close-on-exec on timeout pipe input");
	}
	if (fcntl(to_pipe[1], F_SETFD, 1) < 0) {
	    XtError("AddTimeOut: Can't set close-on-exec on timeout pipe output");
	}
	XtAppAddInput(appcontext, to_pipe[0], (XtPointer)XtInputReadMask, to_input, NULL);
    }

    if (msec > 0) {
	/* Allocate a record for it and add an Xt timeout. */
	torec_t *torec = (torec_t *)XtMalloc(sizeof(torec_t));
	llist_init(&torec->llist);
	torec->fn = fn;
	torec->id = XtAppAddTimeOut(appcontext, msec, to_fn, NULL);
	torec->seq = to_next_seq++;
	llist_insert_before(&torec->llist, &torecs);
	return (ioid_t)torec->seq;
    } else {
	/* Add a deferred event instead. */
	return AddDefer(fn);
    }
}

/**
 * Remove a timeout.
 * @param[in] cookie	I/O identifier
 */
void
RemoveTimeOut(ioid_t cookie)
{
    if (RemoveDefer(cookie)) {
	return;
    }

    torec_t *torec;
    bool found = false;
    FOREACH_LLIST(&torecs, torec, torec_t *) {
	if (torec->seq == (unsigned long)cookie) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&torecs, torec, torec_t *);

    /* Remove the Xt timeout and free the record. */
    if (found) {
	XtRemoveTimeOut(torec->id);
	llist_unlink(&torec->llist);
	Free(torec);
    }
}

ks_t
string_to_key(char *s)
{
    return (ks_t)XStringToKeysym(s);
}
