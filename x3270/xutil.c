/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
AddExcept(iosrc_t sock, iofn_t fn)
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
AddOutput(iosrc_t sock, iofn_t fn)
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

/*
 * Timer callbacks.
 */

typedef struct torec {
    tofn_t	  fn;
    XtIntervalId  id;
    struct torec *next;
} torec_t;

static torec_t *torecs = NULL;

static void
to_fn(XtPointer closure, XtIntervalId *id)
{
    torec_t *torec;
    torec_t *prev = NULL;
    tofn_t fn = NULL;

    for (torec = torecs; torec != NULL; torec = torec->next) {
	if (torec->id == *id) {
		break;
	}
	prev = torec;
    }

    if (torec != NULL) {

	/* Remember the record. */
	fn = torec->fn;

	/* Free the record. */
	if (prev != NULL) {
	    prev->next = torec->next;
	} else {
	    torecs = torec->next;
	}
	XtFree((XtPointer)torec);

	/* Call the function. */
	(*fn)((ioid_t)*id);
    }
}

ioid_t
AddTimeOut(unsigned long msec, tofn_t fn)
{
    torec_t *torec;

    torec = (torec_t *)XtMalloc(sizeof(torec_t));
    torec->fn = fn;
    torec->id = XtAppAddTimeOut(appcontext, msec, to_fn, NULL);
    torec->next = torecs;
    torecs = torec;
    return torec->id;
}

void
RemoveTimeOut(ioid_t cookie)
{
    torec_t *torec;
    torec_t *prev = NULL;

    for (torec = torecs; torec != NULL; torec = torec->next) {
	if (torec->id == cookie) {
		break;
	}
	prev = torec;
    }

    if (torec != NULL) {
	XtRemoveTimeOut(cookie);
	if (prev != NULL) {
	    prev->next = torec->next;
	} else {
	    torecs = torec->next;
	}
	XtFree((XtPointer)torec);
    } else {
	Error("RemoveTimeOut: Can't find");
    }
}

ks_t
string_to_key(char *s)
{
    return (ks_t)XStringToKeysym(s);
}
