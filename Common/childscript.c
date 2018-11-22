/*
 * Copyright (c) 1993-2016, 2018 Paul Mattes.
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
 *      childscript.c
 *              The Script() action.
 */

#include "globals.h"

#include <assert.h>
#include <fcntl.h>
#if !defined(_WIN32) /*[*/
# include <signal.h>
# include <sys/signal.h>
# include <sys/wait.h>
#endif /*]*/

#include "actions.h"
#include "child.h"
#include "popups.h"
#include "child_popups.h"
#include "childscript.h"
#include "lazya.h"
#include "peerscript.h"
#include "s3270_proto.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"
#include "w3misc.h"

#define CHILD_BUF 1024

static void child_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool child_done(task_cbh handle, bool success, bool abort);
static bool child_run(task_cbh handle, bool *success);
static void child_closescript(task_cbh handle);
static void child_setflags(task_cbh handle, unsigned flags);
static unsigned child_getflags(task_cbh handle);

static void child_setir(task_cbh handle, void *irhandle);
static void *child_getir(task_cbh handle);
static void child_setir_state(task_cbh handle, const char *name, void *state,
	ir_state_abort_cb abort_cb);
static void *child_getir_state(task_cbh handle, const char *name);
static const char *child_command(task_cbh handle);

static irv_t child_irv = {
    child_setir,
    child_getir,
    child_setir_state,
    child_getir_state
};

/* Callback block for parent script. */
static tcb_t script_cb = {
    "child",
    IA_MACRO,
    0,
    child_data,
    child_done,
    child_run,
    child_closescript,
    child_setflags,
    child_getflags,
    &child_irv,
    NULL,
    child_command
};

/* Asynchronous callback block for parent script. */
static tcb_t async_script_cb = {
    "child",
    IA_MACRO,
    CB_NEW_TASKQ,
    child_data,
    child_done,
    child_run,
    child_closescript,
    child_setflags,
    child_getflags,
    &child_irv,
    NULL,
    child_command
};

#if !defined(_WIN32) /*[*/
/* Callback block for child (creates new taskq). */
static tcb_t child_cb = {
    "child",
    IA_MACRO,
    CB_NEW_TASKQ,
    child_data,
    child_done,
    child_run,
    child_closescript,
    child_setflags,
    child_getflags,
    &child_irv,
    NULL,
    child_command
};
#endif /*]*/

#if defined(_WIN32) /*[*/
/* Stdout read context. */
typedef struct {
    HANDLE pipe_rd_handle;	/* read handle for pipe */
    HANDLE pipe_wr_handle;	/* write handle for pipe */
    HANDLE enable_event;	/* enable event (emulator to read thread) */
    HANDLE done_event;		/* done event (read thread to emulator) */
    ioid_t done_id;		/* I/O identifier for done event */
    HANDLE read_thread;		/* read thread */
    char buf[CHILD_BUF];	/* input  buffer */
    DWORD nr;			/* length of data in I/O buffer */
    int error;			/* error code for failed read */
    bool dead;			/* read thread has exited, set by read thread */
    bool collected_eof;		/* EOF collected by I/O function */
} cr_t;
#endif /*]*/

/* Child script context. */
typedef struct {
    llist_t llist;		/* linkage */
    char *parent_name;		/* cb name */
    char *command;		/* command text */
    bool done;			/* true if script is complete */
    bool success;		/* success or failure */
    ioid_t exit_id;		/* I/O identifier for child exit */
    int exit_status;		/* exit status */
    bool enabled;		/* enabled */
    char *output_buf;		/* output buffer */
    size_t output_buflen;	/* size of output buffer */
    bool keyboard_lock;		/* lock/unlock keyboard while running */
    unsigned capabilities;	/* self-reported capabilities */
    void *irhandle;		/* input request handle */
    task_cb_ir_state_t ir_state; /* named input request state */
#if defined(_WIN32) /*[*/
    DWORD pid;			/* process ID */
    HANDLE child_handle;	/* status collection handle */
    peer_listen_t listener;	/* listener for child commands */
    cr_t cr;			/* stdout read context */
#else /*][*/
    char *child_name;		/* cb name */
    pid_t pid;			/* process ID */
    int infd;			/* input (to emulator) file descriptor */
    int outfd;			/* output (to script) file descriptor */
    ioid_t id;			/* input I/O identifier */
    char *buf;			/* pending command */
    size_t buf_len;		/* length of pending command */
    int stdoutpipe;		/* stdout pipe */
    ioid_t stdout_id;		/* stdout I/O identifier */
#endif /*]*/
} child_t;
static llist_t child_scripts = LLIST_INIT(child_scripts);

/**
 * Free a child.
 *
 * @param[in,out] c	Child.
 */
static void
free_child(child_t *c)
{
    llist_unlink(&c->llist);
    Replace(c->parent_name, NULL);
    Replace(c->command, NULL);
#if !defined(_WIN32) /*[*/
    Replace(c->child_name, NULL);
#endif /*]*/
    Replace(c->output_buf, NULL);
    Free(c);
}

#if !defined(_WIN32) /*[*/
/**
 * Run the next command in the child buffer.
 *
 * @param[in,out] c	Child
 *
 * @return true if command was run. Command is deleted from the buffer.
 */
static bool
run_next(child_t *c)
{
    size_t cmdlen;
    char *name;

    /* Find a newline in the buffer. */
    for (cmdlen = 0; cmdlen < c->buf_len; cmdlen++) {
	if (c->buf[cmdlen] == '\n') {
	    break;
	}
    }
    if (cmdlen >= c->buf_len) {
	return false;
    }

    /*
     * Run the first command.
     * cmdlen is the number of characters in the command, not including the
     * newline.
     */
    name = push_cb(c->buf, cmdlen, &child_cb, (task_cbh)c);
    Replace(c->child_name, NewString(name));

    /* If there is more, shift it over. */
    cmdlen++; /* count the newline */
    if (c->buf_len > cmdlen) {
	memmove(c->buf, c->buf + cmdlen, c->buf_len - cmdlen);
	c->buf_len = c->buf_len - cmdlen;
    } else {
	Replace(c->buf, NULL);
	c->buf_len = 0;
    }
    return true;
}

/**
 * Tear down a child.
 *
 * @param[in,out] c	Child.
 */
static void
close_child(child_t *c)
{
    if (c->infd != -1) {
	close(c->infd);
	c->infd = -1;
    }
    if (c->outfd != -1) {
	close(c->outfd);
	c->outfd = -1;
    }
    if (c->id != NULL_IOID) {
	RemoveInput(c->id);
	c->id = NULL_IOID;
    }
    Replace(c->buf, NULL);
    if (c->stdout_id != NULL_IOID) {
	RemoveInput(c->stdout_id);
	c->stdout_id = NULL_IOID;
    }
    if (c->stdoutpipe != -1) {
	close(c->stdoutpipe);
	c->stdoutpipe = -1;
    }
    if (c->irhandle != NULL) {
	task_abort_input_request_irhandle(c->irhandle);
	c->irhandle = NULL;
    }
    task_cb_abort_ir_state(&c->ir_state);
}

/**
 * Read the next command from a child pipe.
 * @param[in] fd	File descriptor
 * @param[in] id	I/O identifier
 */
static void
child_input(iosrc_t fd _is_unused, ioid_t id)
{
    child_t *c;
    bool found_child = false;
    char buf[8192];
    size_t n2r;
    size_t nr;
    size_t i;

    /* Find the child. */
    FOREACH_LLIST(&child_scripts, c, child_t *) {
	if (c->id == id) {
	    found_child = true;
	    break;
	}
    } FOREACH_LLIST_END(&child_scripts, c, child_t *);
    assert(found_child);

    /* Read input. */
    n2r = sizeof(buf);
    nr = read(c->infd, buf, (int)n2r);
    assert(nr >= 0);
    vtrace("%s input complete, nr=%d\n", c->parent_name, (int)nr);
    if (nr == 0) {
	vtrace("%s script EOF\n", c->parent_name);
	close_child(c);
	if (c->exit_id == NULL_IOID) {
	    c->done = true;
	    task_activate((task_cbh *)c);
	}
	RemoveInput(c->id);
	c->id = NULL_IOID;
	close(c->infd);
	c->infd = -1;
	return;
    }

    /* Append, filtering out CRs. */
    c->buf = Realloc(c->buf, c->buf_len + nr + 1);
    for (i = 0; i < nr; i++) {
	char ch = buf[i];

	if (ch != '\r') {
	    c->buf[c->buf_len++] = ch;
	}
    }

    /* Disable further input. */
    if (c->id != NULL_IOID) {
	RemoveInput(c->id);
	c->id = NULL_IOID;
    }

    /* Run the next command, if we have it all. */
    if (!run_next(c) && c->id == NULL_IOID) {
	/* Get more input. */
	c->id = AddInput(c->infd, child_input);
    }
}

/**
 * Read output from a child script.
 * @param[in] fd	File descriptor
 * @param[in] id	I/O identifier
 */
static void
child_stdout(iosrc_t fd _is_unused, ioid_t id)
{
    child_t *c;
    bool found_child = false;
    char buf[8192];
    size_t n2r;
    size_t nr;
    size_t new_buflen;

    /* Find the child. */
    FOREACH_LLIST(&child_scripts, c, child_t *) {
	if (c->stdout_id == id) {
	    found_child = true;
	    break;
	}
    } FOREACH_LLIST_END(&child_scripts, c, child_t *);
    assert(found_child);

    /* Read input. */
    n2r = sizeof(buf);
    nr = read(fd, buf, (int)n2r);
    assert(nr >= 0);
    vtrace("%s stdout read complete, nr=%d\n", c->parent_name, (int)nr);
    if (nr == 0) {
	vtrace("%s script stdout EOF\n", c->parent_name);
	RemoveInput(c->stdout_id);
	c->stdout_id = NULL_IOID;
	close(c->stdoutpipe);
	c->stdoutpipe = -1;
	return;
    }

    /* Save it. */
    new_buflen = c->output_buflen + nr;
    c->output_buf = Realloc(c->output_buf, new_buflen + 1);
    memcpy(c->output_buf + c->output_buflen, buf, nr);
    c->output_buflen = new_buflen;
    c->output_buf[new_buflen] = '\0';
}
#endif /*]*/

/**
 * Callback for data returned to child script command.
 *
 * @param[in] handle    Callback handle
 * @param[in] buf       Buffer
 * @param[in] len       Buffer length
 * @param[in] success   True if data, false if error message
 */
static void
child_data(task_cbh handle, const char *buf, size_t len, bool success)
{
#if !defined(_WIN32) /*[*/
    child_t *c = (child_t *)handle;
    char *s = lazyaf(DATA_PREFIX "%.*s\n", (int)len, buf);

    (void) write(c->outfd, s, strlen(s));
#endif /*]*/
}

/**
 * Callback for completion of one command executed from the child script.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True of script has terminated
 */
static bool
child_done(task_cbh handle, bool success, bool abort)
{
    child_t *c = (child_t *)handle;
#if !defined(_WIN32) /*[*/
    bool new_child = false;
    char *prompt = task_cb_prompt(handle);
    char *s = lazyaf("%s\n%s\n", prompt, success? "ok": "error");

    /* Print the prompt. */
    vtrace("Output for %s: %s/%s\n", c->child_name, prompt,
	success? "ok": "error");
    (void) write(c->outfd, s, strlen(s));

    if (abort || !c->enabled) {
	close(c->outfd);
	c->outfd = -1;
	if (abort) {
	    vtrace("%s killing process %d\n", c->child_name, (int)c->pid);
	    killpg(c->pid, SIGKILL);
	    if (c->keyboard_lock) {
		disable_keyboard(ENABLE, IMPLICIT, "Script() abort");
	    }
	}
	return true;
    }

    /* Run any pending command that we already read in. */
    new_child = run_next(c);
    if (!new_child && c->id == NULL_IOID && c->infd != -1) {
	/* Allow more input. */
	c->id = AddInput(c->infd, child_input);
    }

    /*
     * If there was a new child, we're still active. Otherwise, let our sms
     * be popped.
     */
    return !new_child;

#else /*][*/

    if (abort) {
	peer_shutdown(c->listener);
	c->listener = NULL;
	vtrace("%s terminating script process\n", c->parent_name);
	TerminateProcess(c->child_handle, 1);
	if (c->keyboard_lock) {
	    disable_keyboard(ENABLE, IMPLICIT, "Script() abort");
	}
    }
    return true;

#endif /*]*/
}

#if defined(_WIN32) /*[*/
static void
cr_teardown(cr_t *cr)
{
    if (cr->pipe_rd_handle != INVALID_HANDLE_VALUE) {
	CloseHandle(cr->pipe_rd_handle);
	cr->pipe_rd_handle = INVALID_HANDLE_VALUE;
    }
    if (cr->pipe_wr_handle != INVALID_HANDLE_VALUE) {
	CloseHandle(cr->pipe_wr_handle);
	cr->pipe_wr_handle = INVALID_HANDLE_VALUE;
    }
    if (cr->enable_event != INVALID_HANDLE_VALUE) {
	CloseHandle(cr->enable_event);
	cr->enable_event = INVALID_HANDLE_VALUE;
    }
    if (cr->done_event != INVALID_HANDLE_VALUE) {
	CloseHandle(cr->done_event);
	cr->done_event = INVALID_HANDLE_VALUE;
    }
    if (cr->done_id != NULL_IOID) {
	RemoveInput(cr->done_id);
	cr->done_id = NULL_IOID;
    }
    if (cr->read_thread != INVALID_HANDLE_VALUE) {
	CloseHandle(cr->read_thread);
	cr->read_thread = INVALID_HANDLE_VALUE;
    }
}

/**
 * Tear down a child.
 *
 * @param[in,out] c	Child.
 */
static void
close_child(child_t *c)
{
    if (c->child_handle != INVALID_HANDLE_VALUE) {
	CloseHandle(c->child_handle);
	c->child_handle = INVALID_HANDLE_VALUE;
    }
    if (c->listener != NULL) {
	peer_shutdown(c->listener);
	c->listener = NULL;
    }
    cr_teardown(&c->cr);
    if (c->irhandle != NULL) {
	task_abort_input_request_irhandle(c->irhandle);
	c->irhandle = NULL;
    }
    task_cb_abort_ir_state(&c->ir_state);
}

/*
 * Collect output from the read thread.
 * Returns true if more input may be available.
 */
static bool
cr_collect(child_t *c)
{
    cr_t *cr = &c->cr;
    if (cr->nr != 0) {
	vtrace("Got %d bytes of script stdout/stderr\n", (int)cr->nr);
	if (cr->nr == 2 && !strncmp(cr->buf, "^C", 2)) {
	    /* Hack, hack, hack. */
	    vtrace("Suppressing '^C' output from child\n");
	} else {
	    c->output_buf = Realloc(c->output_buf,
		    c->output_buflen + cr->nr + 1);
	    memcpy(c->output_buf + c->output_buflen, cr->buf, cr->nr);
	    c->output_buflen += cr->nr;
	    c->output_buf[c->output_buflen] = '\0';
	}

	/* Ready for more. */
	cr->nr = 0;
    }
    if (cr->dead) {
	if (cr->error != 0) {
	    vtrace("Script stdout/stderr read failed: %s\n",
		    win32_strerror(cr->error));
	}
	cr->collected_eof = true;
	return false;
    }
    SetEvent(cr->enable_event);
    return true;
}
#endif /*]*/

/**
 * Run vector for child scripts.
 *
 * @param[in] handle	Context.
 * @param[out] success	Returned true if script succeeded.
 *
 * @return True if script is complete.
 */
static bool
child_run(task_cbh handle, bool *success)
{
    child_t *c = (child_t *)handle;

    if (c->done) {
#if defined(_WIN32) /*[*/
	/* Collect remaining output and let the read thread exit. */
	cr_t *cr = &c->cr;

	if (!cr->collected_eof) {
	    do {
		WaitForSingleObject(cr->done_event, INFINITE);
	    } while (cr_collect(c));
	}
#endif /*]*/
	if (c->output_buflen) {
	    /* Strip out CRs. */
	    char *tmp = Malloc(strlen(c->output_buf) + 1);
	    char *s = c->output_buf;
	    char *t = tmp;
	    char c;

	    while ((c = *s++) != '\0') {
		if (c != '\r') {
		    *t++ = c;
		}
	    }
	    *t = '\0';
	    action_output("%s", tmp);
	    Free(tmp);
	}
#if defined(_WIN32) /*[*/
	close_child(c);
#endif /*]*/
	if (!c->success) {
#if !defined(_WIN32) /*[*/
	    if (WIFEXITED(c->exit_status)) {
		popup_an_error("Script exited with status %d",
			WEXITSTATUS(c->exit_status));
	    } else if (WIFSIGNALED(c->exit_status)) {
		popup_an_error("Script killed by signal %d",
			WTERMSIG(c->exit_status));
	    } else {
		popup_an_error("Script stopped by unknown status %d",
			c->exit_status);
	    }
#else /*][*/
	    popup_an_error("Script exited with status %d",
		    c->exit_status);
#endif /*]*/
	}
	*success = c->success;
	if (c->keyboard_lock) {
	    disable_keyboard(ENABLE, IMPLICIT, "Script() completion");
	}
	free_child(c);
	return true;
    }

    return false;
}

/**
 * Close a running child script.
 *
 * @param[in] handle	Child context
 */
static void
child_closescript(task_cbh handle)
{
    child_t *c = (child_t *)handle;

    c->enabled = false;
}

/**
 * Set capabilities flags.
 *
 * @param[in] handle	Child context
 * @param[in] flags	Flags
 */
static void
child_setflags(task_cbh handle, unsigned flags)
{
    child_t *c = (child_t *)handle;

    c->capabilities = flags;
}

/**
 * Get capabilities flags.
 *
 * @param[in] handle	Child context
 * @returns flags
 */
static unsigned
child_getflags(task_cbh handle)
{
    child_t *c = (child_t *)handle;

    return c->capabilities;
}

/**
 * Set the pending input request.
 *
 * @param[in] handle	Child context
 * @param[in] irhandle	Input request handle
 */
static void
child_setir(task_cbh handle, void *irhandle)
{
    child_t *c = (child_t *)handle;

    c->irhandle = irhandle;
}

/**
 * Get the pending input request.
 *
 * @param[in] handle	Child context
 *
 * @returns input request handle
 */
static void *
child_getir(task_cbh handle)
{
    child_t *c = (child_t *)handle;

    return c->irhandle;
}

/**
 * Set input request state.
 *
 * @param[in] handle    CB handle
 * @param[in] name      Input request type name
 * @param[in] state     State to store
 * @param[in] abort     Abort callback
 */
static void
child_setir_state(task_cbh handle, const char *name, void *state,
	ir_state_abort_cb abort)
{
    child_t *c = (child_t *)handle;

    task_cb_set_ir_state(&c->ir_state, name, state, abort);
}

/**
 * Get input request state.
 *
 * @param[in] handle    CB handle
 * @param[in] name      Input request type name
 *
 * @returns input request state
 */
static void *
child_getir_state(task_cbh handle, const char *name)
{
    child_t *c = (child_t *)handle;

    return task_cb_get_ir_state(&c->ir_state, name);
}

/**
 * Get the command text.
 *
 * @param[in] handle	CB handle
 *
 * @returns command text, or NULL
 */
static const char *
child_command(task_cbh handle)
{
    child_t *c = (child_t *)handle;

    return c->command;
}

#if !defined(_WIN32) /*[*/
static void
child_exited(ioid_t id, int status)
{
    child_t *c;
    bool found_child = false;

    FOREACH_LLIST(&child_scripts, c, child_t *) {
	if (c->exit_id == id) {
	    found_child = true;
	    break;
	}
    } FOREACH_LLIST_END(&child_scripts, c, child_t *);

    if (!found_child) {
	vtrace("child_exited: no match\n");
	return;
    }

    vtrace("%s script %d exited with status %d\n", c->child_name, (int)c->pid,
	    status);

    c->exit_status = status;
    if (status != 0) {
	c->success = false;
    }
    c->exit_id = NULL_IOID;
    if (c->id == NULL_IOID) {
	/* Tell sms that this should be run. */
	c->done = true;
	task_activate((task_cbh *)c);
    }
}
#endif /*]*/

#if defined(_WIN32) /*[*/
/* Process an event on a child script handle (a process exit). */
static void
child_exited(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    child_t *c;
    bool found_child = false;
    DWORD status;

    FOREACH_LLIST(&child_scripts, c, child_t *) {
	if (c->exit_id == id) {
	    found_child = true;
	    break;
	}
    } FOREACH_LLIST_END(&child_scripts, c, child_t *);

    if (!found_child) {
	vtrace("child_exited: no match\n");
	return;
    }

    status = 0;
    if (GetExitCodeProcess(c->child_handle, &status) == 0) {
	popup_an_error("GetExitCodeProcess failed: %s",
	win32_strerror(GetLastError()));
    } else if (status != STILL_ACTIVE) {
	vtrace("%s script exited with status %d\n", c->parent_name,
		(unsigned)status);
	c->exit_status = status;
	if (status != 0) {
	    c->success = false;
	}
	CloseHandle(c->child_handle);
	c->child_handle = INVALID_HANDLE_VALUE;
	RemoveInput(c->exit_id);
	c->exit_id = NULL_IOID;

	/* Tell sms that this should be run. */
	c->done = true;
	task_activate((task_cbh *)c);
    }
}

/* Let the system pick a TCP port to bind to. */
static unsigned short
pick_port(socket_t *sp)
{
    socket_t s;
    struct sockaddr_in sin;
    socklen_t len;
    int on = 1;

    s = socket(PF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
	popup_an_error("socket: %s\n", win32_strerror(GetLastError()));
	return 0;
    }
    (void) memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	popup_an_error("bind: %s\n", win32_strerror(GetLastError()));
	SOCK_CLOSE(s);
	return 0;
    }
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
	popup_an_error("setsockopt: %s\n", win32_strerror(GetLastError()));
	SOCK_CLOSE(s);
	return 0;
    }
    len = sizeof(sin);
    if (getsockname(s, (struct sockaddr *)&sin, &len) < 0) {
	popup_an_error("getsockaddr: %s\n", win32_strerror(GetLastError()));
	SOCK_CLOSE(s);
	return 0;
    }
    *sp = s;
    return ntohs(sin.sin_port);
}

/* Read from the child's stdout or stderr. */
static DWORD WINAPI
child_read_thread(LPVOID parameter)
{
    child_t *child = (child_t *)parameter;
    cr_t *cr = &child->cr;
    DWORD success;
    bool done = false;

    while (!done) {
	switch (WaitForSingleObject(cr->enable_event, INFINITE)) {
	case WAIT_OBJECT_0:
	    success = ReadFile(cr->pipe_rd_handle, cr->buf, CHILD_BUF, &cr->nr,
		    NULL);
	    if (!success) {
		/* Canceled or pipe broken. */
		cr->error = GetLastError();
		done = true;
		break;
	    }
	    SetEvent(cr->done_event);
	    break;
	default:
	    cr->error = GetLastError();
	    done = true;
	    break;
	}
    }

    /* All done, I hope. */
    cr->nr = 0;
    cr->dead = true;
    SetEvent(cr->done_event);
    return 0;
}

/* The child stdout/stderr thread produced output. */
static void
cr_output(iosrc_t fd, ioid_t id)
{
    child_t *c;
    bool found_child = false;

    /* Find the descriptor. */
    FOREACH_LLIST(&child_scripts, c, child_t *) {
	if (c->cr.done_id == id) {
	    found_child = true;
	    break;
	}
    } FOREACH_LLIST_END(&child_scripts, c, child_t *);
    assert(found_child);

    /* Collect the output. */
    cr_collect(c);
}

/* Set up the stdout reader context. */
static bool
setup_cr(child_t *c)
{
    cr_t *cr = &c->cr;
    SECURITY_ATTRIBUTES sa;
    DWORD mode;

    /* Create the pipe. */
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&cr->pipe_rd_handle, &cr->pipe_wr_handle, &sa, 0)) {
	popup_an_error("CreatePipe() failed: %s",
		win32_strerror(GetLastError()));
	return false;
    }
    if (!SetHandleInformation(cr->pipe_rd_handle, HANDLE_FLAG_INHERIT, 0)) {
	popup_an_error("SetHandleInformation() failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(cr->pipe_rd_handle);
	CloseHandle(cr->pipe_wr_handle);
	return false;
    }
    mode = PIPE_READMODE_BYTE;
    if (!SetNamedPipeHandleState(cr->pipe_rd_handle, &mode, NULL, NULL)) {
	popup_an_error("SetNamedPipeHandleState(stdout) failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(cr->pipe_rd_handle);
	CloseHandle(cr->pipe_wr_handle);
	return false;
    }

    /* Express interest in their output. */
    cr->enable_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    cr->done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    cr->read_thread = CreateThread(NULL, 0, child_read_thread, c, 0, NULL);
    cr->done_id = AddInput(cr->done_event, cr_output);

    return true;
}
#endif /*]*/


/* "Script" action, runs a script as a child process. */
bool
Script_action(ia_t ia, unsigned argc, const char **argv)
{
    child_t *c;
    char *name;
    bool async = false;
    bool keyboard_lock = true;
    bool stdout_redirect = true;
    varbuf_t r;
    unsigned i;
#if !defined(_WIN32) /*[*/
    pid_t pid;
    int inpipe[2];
    int outpipe[2];
    int stdoutpipe[2];
#else /*][*/
    peer_listen_mode mode = PLM_MULTI;
    unsigned short port;
    socket_t s;
    struct sockaddr_in *sin;
    peer_listen_t listener;
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION process_information;
    char *args;
    cr_t *cr;
#endif /*]*/

    action_debug("Script", ia, argc, argv);

    for (;;) {
	if (argc < 1) {
	    popup_an_error("Script requires at least one argument");
	    return false;
	}
	if (!strcasecmp(argv[0], "-Async")) {
	    async = true;
	    keyboard_lock = false;
	    argc--;
	    argv++;
	} else if (!strcasecmp(argv[0], "-NoLock")) {
	    keyboard_lock = false;
	    argc--;
	    argv++;
	} else if (!strcasecmp(argv[0], "-Single")) {
#if defined(_WIN32) /*[*/
	    mode = PLM_SINGLE;
#endif /*]*/
	    argc--;
	    argv++;
	} else if (!strcasecmp(argv[0], "-NoStdoutRedirect")) {
	    stdout_redirect = false;
	    argc--;
	    argv++;
	} else {
	    break;
	}
    }

#if !defined(_WIN32) /*[*/
    /*
     * Create pipes and stdout stream for the script process.
     *  inpipe[] is read by x3270, written by the script
     *  outpipe[] is written by x3270, read by the script
     */
    if (pipe(inpipe) < 0) {
	popup_an_error("pipe() failed");
	return false;
    }
    if (pipe(outpipe) < 0) {
	(void) close(inpipe[0]);
	(void) close(inpipe[1]);
	popup_an_error("pipe() failed");
	return false;
    }

    /* Create a pipe to capture child stdout. */
    if (pipe(stdoutpipe) < 0) {
	(void) close(outpipe[0]);
	(void) close(outpipe[1]);
	(void) close(inpipe[0]);
	(void) close(inpipe[1]);
	popup_an_error("pipe() failed");
    }

    /* Fork and exec the script process. */
    if ((pid = fork()) < 0) {
	(void) close(inpipe[0]);
	(void) close(inpipe[1]);
	(void) close(outpipe[0]);
	(void) close(outpipe[1]);
	(void) close(stdoutpipe[0]);
	(void) close(stdoutpipe[1]);
	popup_an_error("fork() failed");
	return false;
    }

    /* Child processing. */
    if (pid == 0) {
	char **child_argv;
	unsigned i;

	/* Become a process group. */
	setsid();

	/* Clean up the pipes. */
	(void) close(outpipe[1]);
	(void) close(inpipe[0]);
	(void) close(stdoutpipe[0]);

	/* Redirect output. */
	if (stdout_redirect) {
	    (void) dup2(stdoutpipe[1], 1);
	}
	(void) dup2(stdoutpipe[1], 2);

	/* Export the names of the pipes into the environment. */
	(void) putenv(xs_buffer(OUTPUT_ENV "=%d", outpipe[0]));
	(void) putenv(xs_buffer(INPUT_ENV "=%d", inpipe[1]));

	/* Set up arguments. */
	child_argv = (char **)Malloc((argc + 1) * sizeof(char *));
	for (i = 0; i < argc; i++) {
	    child_argv[i] = (char *)argv[i];
	}
	child_argv[i] = NULL;

	/* Exec. */
	(void) execvp(argv[0], child_argv);
	(void) fprintf(stderr, "exec(%s) failed\n", argv[0]);
	(void) _exit(1);
    }

    c = (child_t *)Calloc(1, sizeof(child_t));
    llist_init(&c->llist);
    LLIST_APPEND(&c->llist, child_scripts);
    c->success = true;
    c->done = false;
    c->buf = NULL;
    c->buf_len = 0;
    c->pid = pid;
    c->exit_id = AddChild(pid, child_exited);
    c->enabled = true;
    c->stdoutpipe = stdoutpipe[0];
    task_cb_init_ir_state(&c->ir_state);

    /* Clean up our ends of the pipes. */
    c->infd = inpipe[0];
    (void) close(inpipe[1]);
    c->outfd = outpipe[1];
    (void) close(outpipe[0]);
    (void) close(stdoutpipe[1]);

    /* Allow child input. */
    c->id = AddInput(c->infd, child_input);

    /* Capture child output. */
    c->stdout_id = AddInput(c->stdoutpipe, child_stdout);

#else /*]*/

    /* Set up X3270PORT for the child process. */
    port = pick_port(&s);
    if (port == 0) {
	return false;
    }
    sin = (struct sockaddr_in *)Calloc(1, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin->sin_port = htons(port);
    listener = peer_init((struct sockaddr *)sin, sizeof(*sin), mode);
    SOCK_CLOSE(s);
    if (listener == NULL) {
	return false;
    }
    putenv(lazyaf(PORT_ENV "=%d", port));

    /* Set up the stdout/stderr output pipes. */
    c = (child_t *)Calloc(1, sizeof(child_t));
    if (!setup_cr(c)) {
	Free(c);
	return false;
    }
    task_cb_init_ir_state(&c->ir_state);
    cr = &c->cr;

    /* Start the child process. */
    (void) memset(&startupinfo, '\0', sizeof(STARTUPINFO));
    startupinfo.cb = sizeof(STARTUPINFO);
    if (stdout_redirect) {
	startupinfo.hStdOutput = cr->pipe_wr_handle;
    }
    startupinfo.hStdError = cr->pipe_wr_handle;
    startupinfo.dwFlags |= STARTF_USESTDHANDLES;
    (void) memset(&process_information, '\0', sizeof(PROCESS_INFORMATION));
    args = NewString(argv[0]);
    for (i = 1; i < argc; i++) {
	char *t;

	if (strchr(argv[i], ' ') != NULL &&
	    argv[i][0] != '"' &&
	    argv[i][strlen(argv[i]) - 1] != '"') {
	    t = xs_buffer("%s \"%s\"", args, argv[i]);
	} else {
	    t = xs_buffer("%s %s", args, argv[i]);
	}
	Free(args);
	args = t;
    }
    if (CreateProcess(NULL, args, NULL, NULL, TRUE,
		stdout_redirect? DETACHED_PROCESS: 0,
		NULL, NULL, &startupinfo, &process_information) == 0) {
	popup_an_error("CreateProcess(%s) failed: %s", argv[0],
		win32_strerror(GetLastError()));
	peer_shutdown(listener);

	/* Let the read thread complete. */
	CloseHandle(cr->pipe_wr_handle);
	cr->pipe_wr_handle = INVALID_HANDLE_VALUE;
	SetEvent(cr->enable_event);
	WaitForSingleObject(cr->done_event, INFINITE);

	cr_teardown(cr);
	Free(c);
	Free(args);
	return false;
    }

    Free(args);
    CloseHandle(process_information.hThread);
    CloseHandle(cr->pipe_wr_handle);
    cr->pipe_wr_handle = INVALID_HANDLE_VALUE;
    SetEvent(cr->enable_event);

    /* Create a new script description. */
    llist_init(&c->llist);
    LLIST_APPEND(&c->llist, child_scripts);
    c->success = true;
    c->done = false;
    c->child_handle = process_information.hProcess;
    c->pid = (int)process_information.dwProcessId;
    c->listener = listener;
    c->enabled = true;

    /*
     * Wait for the child process to exit.
     * Note that this is an asynchronous event -- exits for multiple
     * children can happen in any order.
     */
    c->exit_id = AddInput(process_information.hProcess, child_exited);

#endif /*]*/

    /* Save the arguments. */
    vb_init(&r);
    for (i = 0; i < argc; i++) {
	if (i > 0) {
	    vb_appends(&r, ",");
	}
	vb_appends(&r, argv[i]);
    }
    c->command = vb_consume(&r);

    /* Create the context. It will be idle. */
    c->keyboard_lock = keyboard_lock;
    name = push_cb(NULL, 0, async? &async_script_cb: &script_cb, (task_cbh)c);
    Replace(c->parent_name, NewString(name));
    vtrace("%s script process is %d\n", c->parent_name, (int)c->pid);

    if (keyboard_lock) {
	disable_keyboard(DISABLE, IMPLICIT, "Script() start");
    }

    return true;
}

/* Add an element to a dynamically-allocated array. */
void
array_add(const char ***s, int ix, const char *v)
{
    *s = Realloc((void *)*s, (ix + 1) * sizeof(const char *));
    (*s)[ix] = v;
}

/*
 * Start an x3270if-based interactive console, optionally overriding the app
 * name used as the prompt.
 *
 * Prompt([prompt[,help-action]])
 */
bool
Prompt_action(ia_t ia, unsigned argc, const char **argv)
{
    const char *params[2] = { programname, NULL };
    unsigned i;
    const char **nargv = NULL;
    int nargc = 0;

    action_debug("Prompt", ia, argc, argv);
    if (check_argc("Prompt", argc, 0, 2) < 0) {
	return false;
    }

    for (i = 0; i < argc; i++) {
	const char *in = argv[i];
	char *new_param = lazya(NewString(argv[i]));
	char *out = new_param;
	char c;

	while ((c = *in++)) {
	    if (c != '\'' && c != '"' && !isspace((int)c)) {
		*out++ = c;
	    }
	}
	*out = '\0';
	if (strlen(new_param) > 0) {
	    params[i] = new_param;
	}
    }

    array_add(&nargv, nargc++, "-Async");
#if !defined(_WIN32) /*[*/
    array_add(&nargv, nargc++, "xterm");
    array_add(&nargv, nargc++, "-title");
    array_add(&nargv, nargc++, lazyaf("%s>", params[0]));
    array_add(&nargv, nargc++, "-e");
    array_add(&nargv, nargc++, "/bin/sh");
    array_add(&nargv, nargc++, "-c");
    array_add(&nargv, nargc++, lazyaf("x3270if -I '%s'%s || (echo 'Press <Enter>'; read x)",
	    params[0],
	    (params[1] != NULL)? lazyaf(" -H '%s'", params[1]): ""));
#else /*][*/
    array_add(&nargv, nargc++, "-Single");
    array_add(&nargv, nargc++, "cmd.exe");
    array_add(&nargv, nargc++, "/c");
    array_add(&nargv, nargc++, "start");
    array_add(&nargv, nargc++, lazyaf("\"%s\"", params[0]));
    array_add(&nargv, nargc++, "/wait");
    array_add(&nargv, nargc++, "x3270if.exe");
    array_add(&nargv, nargc++, "-I");
    array_add(&nargv, nargc++, params[0]);
    if (params[1] != NULL) {
	array_add(&nargv, nargc++, "-H");
	array_add(&nargv, nargc++, params[1]);
    }
#endif /*]*/
    array_add(&nargv, nargc++, NULL);
    lazya((void *)nargv);

    return Script_action(ia, nargc - 1, nargv);
}
