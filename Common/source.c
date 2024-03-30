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
 *      source.c
 *              The Source action.
 */

#include "globals.h"

#include "wincmn.h"
#include <errno.h>
#include <fcntl.h>

#include "actions.h"
#include "names.h"
#include "popups.h"
#include "source.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"

static void source_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool source_done(task_cbh handle, bool success, bool abort);
static bool source_run(task_cbh handle, bool *success);

/* Callback block for Source. */
static tcb_t source_cb = {
    "Source",
    IA_SCRIPT,
    CB_NEEDS_RUN,
    source_data,
    source_done,
    source_run
};

/* State for one instance of Source. */
typedef struct {
    int fd;		/* file descriptor */
    char *path;		/* pathname */
    char *name;		/* cb name */
    char *result; 	/* one line of error result */
} source_t;

/**
 * Callback for data returned to the Source action (which is ignored unless
 * a command it executes fails).
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
source_data(task_cbh handle _is_unused, const char *buf _is_unused,
	size_t len _is_unused, bool success)
{
    source_t *s = (source_t *)handle;
    char *b = Malloc(len + 1);
    
    strncpy(b, buf, len);
    b[len] = '\0';
    Replace(s->result, b);
}

/**
 * Free a source conext.
 *
 * @param[in] s		Context.
 */
static void
free_source(source_t *s)
{
    Replace(s->name, NULL);
    Replace(s->result, NULL);
    Free(s);
    disable_keyboard(ENABLE, IMPLICIT, AnSource "() completion");
}

/**
 * Callback for completion of one command executed by the Source action.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if Source is complete
 */
static bool
source_done(task_cbh handle, bool success, bool abort)
{
    source_t *s = (source_t *)handle;

    if (!success || abort) {
	vtrace("%s %s terminated due to error\n", s->name, s->path);
	close(s->fd);
	s->fd = -1;
	free_source(s);
	return true;
    }

    return false;
}

/**
 * Callback to run the Source action.
 *
 * @param[in] handle	Callback handle.
 * @param[out] success	If complete, returned success indication
 *
 * @return true if action is complete
 */
static bool
source_run(task_cbh handle, bool *success)
{
    source_t *s = (source_t *)handle;
    varbuf_t r;
    char *buf;

    /* Check for failure. */
    if (s->fd == -1) {
	/* Aborted. */
	popup_an_error(AnSource "(): %s", s->result? s->result: "failed");
	free_source(s);
	*success = false;
	return true;
    }

    vb_init(&r);

    /* Read the next command from the file. */
    while (true) {
	char c;
	int nr;

	nr = read(s->fd, &c, 1);
	if (nr < 0) {
	    popup_an_error(AnSource "(%s) read error\n", s->path);
	    vb_free(&r);
	    close(s->fd);
	    free_source(s);
	    *success = false;
	    return true;
	}
	if (nr == 0) {
	    if (vb_len(&r) == 0) {
		vtrace("%s %s EOF\n", s->name, s->path);
		vb_free(&r);
		close(s->fd);
		free_source(s);
		*success = true;
		return true;
	    } else {
		vtrace("%s %s EOF without newline\n", s->name, s->path);
		break;
	    }
	}
	if (c == '\r' || c == '\n') {
	    if (vb_len(&r)) {
		break;
	    } else {
		continue;
	    }
	}
	vb_append(&r, &c, 1);
    }

    /* Run the command as a macro. */
    buf = vb_consume(&r);
    vtrace("%s %s read '%s'\n", s->name, s->path, buf);
    push_stack_macro(buf);
    Free(buf);

    /* Not done yet. */
    return false;
}

/**
 * The Source() action.
 *
 * @param[in] ia	Trigger for this action
 * @param[in] argc	Argument count
 * @param[in] argv	Arguments
 */
bool
Source_action(ia_t ia, unsigned argc, const char **argv)
{
    int fd;
    char *expanded_filename;
    source_t *s;
    char *name;

    action_debug(AnSource, ia, argc, argv);
    if (check_argc(AnSource, argc, 1, 1) < 0) {
	return false;
    }
    expanded_filename = do_subst(argv[0], DS_VARS | DS_TILDE);
    fd = open(expanded_filename, O_RDONLY);
    if (fd < 0) {
	Free(expanded_filename);
	popup_an_errno(errno, "%s", argv[0]);
	return false;
    }
#if !defined(_WIN32) /*[*/
    fcntl(fd, F_SETFD, 1);
#endif /*]*/
    Free(expanded_filename);

    /* Start reading from the file. */
    s = (source_t *)Malloc(sizeof(source_t) + strlen(argv[0]) + 1);
    s->fd = fd;
    s->path = (char *)(s + 1);
    strcpy(s->path, argv[0]);
    s->result = NULL;
    name = push_cb(NULL, 0, &source_cb, (task_cbh)s);
    s->name = NewString(name);
    disable_keyboard(DISABLE, IMPLICIT, AnSource "() start");
    return true;
}
