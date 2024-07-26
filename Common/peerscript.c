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
 *      peerscript.c
 *              Read script actions from a TCP socket.
 */

#include "globals.h"


#if !defined(_WIN32) /*[*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /*]*/

#include "wincmn.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include "actions.h"
#include "json.h"
#include "json_run.h"
#include "kybd.h"
#include "names.h"
#include "peerscript.h"
#include "popups.h"
#include "s3270_proto.h"
#include "s3common.h"
#include "source.h"
#include "task.h"
#include "telnet_core.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "varbuf.h"
#include "w3misc.h"
#include "xio.h"

static void peer_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool peer_done(task_cbh handle, bool success, bool abort);
static void peer_closescript(task_cbh handle);
static void peer_setflags(task_cbh handle, unsigned flags);
static unsigned peer_getflags(task_cbh handle);
static void peer_setxflags(task_cbh handle, unsigned flags);
static unsigned peer_getxflags(task_cbh handle);

static void peer_setir(task_cbh handle, void *irhandle);
static void *peer_getir(task_cbh handle);
static void peer_setir_state(task_cbh handle, const char *name, void *state,
	ir_state_abort_cb abort_cb);
static void *peer_getir_state(task_cbh handle, const char *name);
static void peer_reqinput(task_cbh handle, const char *buf, size_t len,
	bool echo);

static irv_t peer_irv = {
    peer_setir,
    peer_getir,
    peer_setir_state,
    peer_getir_state
};

/* Callback block for peer. */
static tcb_t peer_cb = {
    "s3sock",
    IA_SCRIPT,
    CB_NEW_TASKQ | CB_PEER | CB_NEEDCOOKIE,
    peer_data,
    peer_done,
    NULL,
    peer_closescript,
    peer_setflags,
    peer_getflags,
    &peer_irv,
    NULL,
    peer_reqinput,
    peer_setxflags,
    peer_getxflags,
};

/* Callback block for an interactive peer. */
static tcb_t interactive_cb = {
    "s3sock",
    IA_COMMAND,
    CB_NEW_TASKQ | CB_PEER | CB_NEEDCOOKIE,
    peer_data,
    peer_done,
    NULL,
    peer_closescript,
    peer_setflags,
    peer_getflags,
    &peer_irv,
    NULL,
    peer_reqinput,
    peer_setxflags,
    peer_getxflags,
};

/* Peer script context. */
typedef struct {
    llist_t llist;	/* list linkage */
    socket_t socket;	/* socket */
#if defined(_WIN32) /*[*/
    HANDLE event;	/* event */
#endif /*]*/
    peer_listen_t listener;
    ioid_t id;		/* I/O identifier */
    char *buf;		/* pending command */
    size_t buf_len;	/* length of pending command */
    size_t pj_offset;	/* partial JSON offset */
    bool enabled;	/* is this peer enabled? */
    char *name;		/* task name */
    unsigned capabilities; /* self-reported capabilities */
    unsigned xflags;	/* extended flags */
    void *irhandle;	/* input request handle */
    task_cb_ir_state_t ir_state; /* named input request state */
    json_t *json_result; /* pending JSON result */
} peer_t;
static llist_t peer_scripts = LLIST_INIT(peer_scripts);

/* Listening context. */
struct _peer_listen {
    llist_t llist;	/* list linkage */
    socket_t socket;	/* socket */
#if defined(_WIN32) /*[*/
    HANDLE event;	/* event */
#endif /*]*/
    ioid_t id;		/* I/O identifier */
    peer_listen_mode mode; /* listen mode */
    char *desc;		/* listener description */
};
static llist_t peer_listeners = LLIST_INIT(peer_listeners);

/**
 * Tear down a peer connection.
 *
 * @param[in,out] p	Peer
 */
static void
close_peer(peer_t *p)
{
    llist_unlink(&p->llist);
    if (p->socket != INVALID_SOCKET) {
	SOCK_CLOSE(p->socket);
	p->socket = INVALID_SOCKET;
    }
#if defined(_WIN32) /*[*/
    if (p->event != INVALID_HANDLE_VALUE) {
	CloseHandle(p->event);
	p->event = INVALID_HANDLE_VALUE;
    }
#endif /*]*/
    if (p->id != NULL_IOID) {
	RemoveInput(p->id);
	p->id = NULL_IOID;
    }
    Replace(p->buf, NULL);
    Replace(p->name, NULL);

    if (p->listener == NULL || p->listener->mode == PLM_ONCE) {
	vtrace("once-only socket closed, exiting\n");
	x3270_exit(0);
    }
    task_cb_abort_ir_state(&p->ir_state);

    json_free(p->json_result);

    Free(p);
}

/**
 * Pushes a command, with possible JSON parsing.
 *
 * @param[in] p		Peer state
 * @param[in] buf	Command buffer
 * @param[in] len	Buffer length
 *
 * @return true if command complete, false if partial JSON found
 */
static bool
do_push(peer_t *p, const char *buf, size_t len)
{
    const char *s = buf;
    char *name;
    tcb_t *tcb = (p->capabilities & CBF_INTERACTIVE)?
	&interactive_cb : &peer_cb;

    while (len && isspace((int)*s)) {
	s++;
	len--;
    }

    /* Try JSON parsing. */
    if (!(p->capabilities & CBF_INTERACTIVE) && (*s == '{' || *s == '[' || *s == '"')) {
	cmd_t **cmds;
	char *single;
	char *errmsg;
	hjparse_ret_t ret;

	ret = hjson_parse(s, len, &cmds, &single, &errmsg);
	if (ret == HJ_OK) {
	    /* Good JSON. */
	    p->json_result = s3json_init();
	    if (cmds != NULL) {
		name = push_cb_split(cmds, tcb, (task_cbh)p);
	    } else {
		name = push_cb(single, strlen(single), tcb, (task_cbh)p);
		Free(single);
	    }
	} else if (ret == HJ_INCOMPLETE) {
	    Free(errmsg);
	    return false;
	} else {
	    /* Bad JSON. */
	    char *fail = Asprintf(AnFail "(\"%s\")", errmsg);

	    /* Answer in JSON only if successfully parsed. */
	    if (ret != HJ_BAD_SYNTAX) {
		p->json_result = s3json_init();
	    }
	    Free(errmsg);
	    name = push_cb(fail, strlen(fail), tcb, (task_cbh)p);
	    Free(fail);
	}
    } else {
	json_free(p->json_result);
	name = push_cb(s, len, tcb, (task_cbh)p);
    }
    Replace(p->name, NewString(name));
    return true;
}

/**
 * Run the next command in the peer buffer.
 *
 * @param[in,out] p	Peer
 *
 * @return true if command was run and command deleted from the buffer.
 */
static bool
run_next(peer_t *p)
{
    size_t cmdlen;

    while (true) {
	/* Find the first newline in the buffer. */
	for (cmdlen = p->pj_offset; cmdlen < p->buf_len; cmdlen++) {
	    if (p->buf[cmdlen] == '\n') {
		break;
	    }
	}
	if (cmdlen >= p->buf_len) {
	    /* No newline. */
	    return false;
	}

	/*
	 * Run the first command.
	 * cmdlen is the number of characters in the command, not including the
	 * newline.
	 */
	if (do_push(p, p->buf, cmdlen)) {
	    break;
	}

	/* Partial JSON. */
	p->pj_offset = cmdlen + 1;
    }
    p->pj_offset = 0;

    /* If there is more, shift it over. */
    cmdlen++; /* count the newline */
    if (p->buf_len > cmdlen) {
	memmove(p->buf, p->buf + cmdlen, p->buf_len - cmdlen);
	p->buf_len = p->buf_len - cmdlen;
    } else {
	Replace(p->buf, NULL);
	p->buf_len = 0;
    }
    return true;
}

/**
 * Read the next command from a peer socket.
 * @param[in] fd	File descriptor
 * @param[in] id	I/O identifier
 */
static void
peer_input(iosrc_t fd _is_unused, ioid_t id)
{
    peer_t *p;
    bool found_peer = false;
    char buf[8192];
    size_t n2r;
    ssize_t nr;
    ssize_t i;

    /* Find the peer. */
    FOREACH_LLIST(&peer_scripts, p, peer_t *) {
	if (p->id == id) {
	    found_peer = true;
	    break;
	}
    } FOREACH_LLIST_END(&peer_scripts, p, peer_t *);
    assert(found_peer);

    /* Read input. */
    n2r = sizeof(buf);
    nr = recv(p->socket, buf, (int)n2r, 0);
    if (nr < 0) {
#if defined(_WIN32) /*[*/
	if (GetLastError() != WSAECONNRESET) {
	    /* Windows does this habitually. */
	    vtrace("s3sock recv: %s\n", win32_strerror(GetLastError()));
	}
#else /*][*/
	vtrace("s3sock recv: %s\n", strerror(errno));
#endif /*]*/
	close_peer(p);
	return;
    }
    vtrace("Input for s3sock complete, nr=%d\n", (int)nr);
    if (nr == 0) {
	vtrace("s3sock EOF\n");
	close_peer(p);
	return;
    }

    /* Append, filtering out CRs. */
    p->buf = Realloc(p->buf, p->buf_len + nr + 1);
    for (i = 0; i < nr; i++) {
	char c = buf[i];

	if (c != '\r') {
	    p->buf[p->buf_len++] = c;
	}
    }

    /* Disable further input. */
    if (p->id != NULL_IOID) {
	RemoveInput(p->id);
	p->id = NULL_IOID;
    }

    /* Run the next command, if we have it all. */
    if (!run_next(p) && p->id == NULL_IOID) {
	/* Get more input. */
#if defined(_WIN32) /*[*/
	p->id = AddInput(p->event, peer_input);
#else /*][*/
	p->id = AddInput(p->socket, peer_input);
#endif /*]*/
    }
}

/**
 * Send data on a socket and check the result.
 *
 * @param[in] s		Socket
 * @param[in] data	Data to send
 * @param[in] len	Length
 * @param[in] sender	Sending function
 */
static void
check_send(socket_t s, const char *data, size_t len, const char *sender)
{
    ssize_t ns = send(s, data, (int)len, 0);
    if (ns != (ssize_t)len) {
	if (ns < 0) {
#if !defined(_WIN32) /*[*/
	    vtrace("%s send: %s\n", sender, strerror(errno));
#else /*][*/
	    vtrace("%s send: %s\n", sender, win32_strerror(GetLastError()));
#endif/*]*/
	} else {
	    vtrace("%s: short send\n", sender);
	}
    }
}

/**
 * Callback for data returned to peer socket command.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
peer_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    peer_t *p = (peer_t *)handle;
    char *cooked;
    static bool recursing = false;

    if (recursing) {
	return;
    }
    recursing = true;

    s3data(buf, len, success, p->capabilities, p->json_result, NULL, &cooked);
    if (cooked != NULL) {
	check_send(p->socket, cooked, strlen(cooked), "peer_data");
	Free(cooked);
    }

    recursing = false;
}

/**
 * Callback for input request.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] echo	True to echo input
 */
static void
peer_reqinput(task_cbh handle, const char *buf, size_t len, bool echo)
{
    peer_t *p = (peer_t *)handle;
    char *s;
    static bool recursing = false;

    if (recursing) {
	return;
    }
    recursing = true;

    s = Asprintf("%s%.*s\n", echo? INPUT_PREFIX: PWINPUT_PREFIX, (int)len, buf);
    check_send(p->socket, s, strlen(s), "peer_reqinput");
    Free(s);
    recursing = false;
}

/**
 * Callback for completion of one command executed from the peer socket.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if script has terminated
 */
static bool
peer_done(task_cbh handle, bool success, bool abort)
{
    peer_t *p = (peer_t *)handle;
    char *out;
    bool new_child = false;

    s3done(handle, success, &p->json_result, &out);
    check_send(p->socket, out, strlen(out), "peer_done");
    Free(out);

    if (abort || !p->enabled) {
	close_peer(p);
	return true;
    }

    /* Run any pending command that we already read in. */
    new_child = run_next(p);
    if (!new_child && p->id == NULL_IOID) {
	/* Allow more input. */
#if defined(_WIN32) /*[*/
	p->id = AddInput(p->event, peer_input);
#else /*][*/
	p->id = AddInput(p->socket, peer_input);
#endif /*]*/
    }

    /*
     * If there was a new child, we're still active. Otherwise, let our sms
     * be popped.
     */
    return !new_child;
}

/**
 * Stop the current script.
 */
static void
peer_closescript(task_cbh handle)
{
    peer_t *p = (peer_t *)handle;

    p->enabled = false;
}

/**
 * Set capabilities flags.
 *
 * @param[in] handle	Peer context
 * @param[in] flags	Flags
 */
static void
peer_setflags(task_cbh handle, unsigned flags)
{
    peer_t *p = (peer_t *)handle;

    p->capabilities = flags;
}

/**
 * Get capabilities flags.
 *
 * @param[in] handle	Peer context
 * @returns flags
 */
static unsigned
peer_getflags(task_cbh handle)
{
    peer_t *p = (peer_t *)handle;

    return p->capabilities;
}

/**
 * Set the pending input request.
 *
 * @param[in] handle	Peer context
 * @param[in] irhandle	Input request handle
 */
static void
peer_setir(task_cbh handle, void *irhandle)
{
    peer_t *p = (peer_t *)handle;

    p->irhandle = irhandle;
}

/**
 * Get extended flags.
 *
 * @param[in] handle	Peer context
 * @returns flags
 */
static unsigned
peer_getxflags(task_cbh handle)
{
    peer_t *p = (peer_t *)handle;

    return p->xflags;
}

/**
 * Set extended flags.
 *
 * @param[in] handle	Peer context
 * @param[in] flags	Flags
 */
static void
peer_setxflags(task_cbh handle, unsigned flags)
{
    peer_t *p = (peer_t *)handle;

    p->xflags = flags;
}

/**
 * Get the pending input request.
 *
 * @param[in] handle	Peer context
 *
 * @returns input request handle
 */
static void *
peer_getir(task_cbh handle)
{
    peer_t *p = (peer_t *)handle;

    return p->irhandle;
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
peer_setir_state(task_cbh handle, const char *name, void *state,
	ir_state_abort_cb abort)
{
    peer_t *p = (peer_t *)handle;

    task_cb_set_ir_state(&p->ir_state, name, state, abort);
}

/**
 * Get input request state.
 *
 * @param[in] handle    CB handle
 * @param[in] name      Input request type name
 */
static void *
peer_getir_state(task_cbh handle, const char *name)
{
    peer_t *p = (peer_t *)handle;

    return task_cb_get_ir_state(&p->ir_state, name);
}

/**
 * Accept a new peer socket connection.
 *
 * @param[in] fd	File descriptor
 * @param[in] id	I/O identifier
 */
static void
peer_connection(iosrc_t fd _is_unused, ioid_t id)
{
    socket_t accept_fd;
    union {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
    } sa;
    socklen_t len = sizeof(sa);
    char hostbuf[128];
    peer_listen_t listener;
    bool found = false;

    FOREACH_LLIST(&peer_listeners, listener, peer_listen_t) {
	if (listener->id == id) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&peer_listeners, listener, peer_listen_t);
    assert(found);

    accept_fd = accept(listener->socket, &sa.sa, &len);
    if (accept_fd != INVALID_SOCKET) {
	if (sa.sa.sa_family == AF_INET) {
	    vtrace("New script socket connection from %s:%u\n",
		    inet_ntop(AF_INET, &sa.sin.sin_addr, hostbuf,
			sizeof(hostbuf)), ntohs(sa.sin.sin_port));
	} else if (sa.sa.sa_family == AF_INET6) {
	    vtrace("New script socket connection from %s:%u\n",
		    inet_ntop(AF_INET6, &sa.sin6.sin6_addr, hostbuf,
			sizeof(hostbuf)), ntohs(sa.sin6.sin6_port));
	}
#if !defined(_WIN32) /*[*/
	else if (sa.sa.sa_family == AF_UNIX) {
	    vtrace("New Unix-domain script socket connection");
	}
#endif /*]*/
	else {
	    vtrace("New script socket connection from ???\n");
	}
    }

    if (accept_fd == INVALID_SOCKET) {
#if !defined(_WIN32) /*[*/
	vtrace("s3sock accept: %s\n", strerror(errno));
#else /*][*/
	vtrace("s3sock accept: %s\n", win32_strerror(GetLastError()));
#endif /*]*/
	return;
    }

    if (listener->mode == PLM_SINGLE || listener->mode == PLM_ONCE) {
	/* Close the listener. */
	vtrace("Closing listener %s (single mode)\n", listener->desc);
	if (listener->socket != INVALID_SOCKET) {
	    SOCK_CLOSE(listener->socket);
	    listener->socket = INVALID_SOCKET;
	}
#if defined(_WIN32) /*[*/
	if (listener->event != INVALID_HANDLE_VALUE) {
	    CloseHandle(listener->event);
	    listener->event = INVALID_HANDLE_VALUE;
	}
#endif /*]*/
	if (listener->id != NULL_IOID) {
	    RemoveInput(listener->id);
	    listener->id = NULL_IOID;
	}
    } else {
	vtrace("Not closing listener %s (multi mode)\n", listener->desc);
    }

    /* Allocate the peer state and remember it. */
    peer_accepted(accept_fd, listener);
}

/**
 * Set up for I/O on an accepted peer.
 *
 * @param[in] s		Socket
 * @param[in] listener	Listener, or null
 */
void
peer_accepted(socket_t s, void *listener)
{
    peer_t *p = (peer_t *)Calloc(1, sizeof(peer_t));
#if defined(_WIN32) /*[*/
    HANDLE event;
#endif /*]*/

#if !defined(_WIN32) /*[*/
    fcntl(s, F_SETFD, 1);
#else /*][*/
    event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (event == NULL) {
	fprintf(stderr, "Can't create socket handle\n");
	exit(1);
    }
    if (WSAEventSelect(s, event, FD_READ | FD_CLOSE) != 0) {
	fprintf(stderr, "Can't set socket handle events\n");
	exit(1);
    }
#endif /*]*/

    llist_init(&p->llist);
    p->listener = listener;
    p->socket = s;
#if defined(_WIN32) /*[*/
    p->event = event;
    p->id = AddInput(p->event, peer_input);
#else /*][*/
    p->id = AddInput(p->socket, peer_input);
#endif /*]*/
    p->buf = NULL;
    p->buf_len = 0;
    p->enabled = true;
    task_cb_init_ir_state(&p->ir_state);
    LLIST_APPEND(&p->llist, peer_scripts);
}

/**
 * Initialize accepting script connections on a specific TCP port.
 *
 * @param[in] sa	Socket address to listen on
 * @param[in] sa_len	Socket address length
 * @param[in] mode	Connection mode
 *
 * @return peer listen context
 */
peer_listen_t
peer_init(struct sockaddr *sa, socklen_t sa_len, peer_listen_mode mode)
{
    peer_listen_t listener;
    char hostbuf[128];
    int on = 1;

    /* Create the listening socket. */
    listener = (peer_listen_t)Calloc(sizeof(struct _peer_listen), 1);
    listener->socket = INVALID_SOCKET;
#if defined(_WIN32) /*[*/
    listener->event = INVALID_HANDLE_VALUE;
#endif /*]*/
    listener->id = NULL_IOID;

    listener->mode = mode;
#if !defined(_WIN32) /*[*/
    listener->socket = socket(sa->sa_family, SOCK_STREAM, 0);
#else /*][*/
    listener->socket = WSASocket(sa->sa_family, SOCK_STREAM, 0, NULL, 0,
	    WSA_FLAG_NO_HANDLE_INHERIT);
#endif /*]*/
    if (listener->socket == INVALID_SOCKET) {
#if !defined(_WIN32) /*[*/
	popup_an_errno(errno, "script socket()");
#else /*][*/
	popup_an_error("script socket(): %s",
		win32_strerror(GetLastError()));
#endif /*]*/
	goto fail;
    }

    if (setsockopt(listener->socket, SOL_SOCKET, SO_REUSEADDR,
		(char *)&on, sizeof(on)) < 0) {
#if !defined(_WIN32) /*[*/
	popup_an_errno(errno, "script setsockopt(SO_REUSEADDR)");
#else /*][*/
	popup_an_error("script setsockopt(SO_REUSEADDR): %s",
		win32_strerror(GetLastError()));
#endif /*]*/
	goto fail;
    }

    if (bind(listener->socket, sa, sa_len) < 0) {
#if !defined(_WIN32) /*[*/
	popup_an_errno(errno, "script socket bind");
#else /*][*/
	popup_an_error("script socket bind: %s",
		win32_strerror(GetLastError()));
#endif /*]*/
	goto fail;
    }

    if (getsockname(listener->socket, sa, &sa_len) < 0) {
#if !defined(_WIN32) /*[*/
	popup_an_errno(errno, "script socket getsockname");
#else /*][*/
	popup_an_error("script socket getsockname: %s",
		win32_strerror(GetLastError()));
#endif /*]*/
	goto fail;
    }

    if (listen(listener->socket, 1) < 0) {
#if !defined(_WIN32) /*[*/
	popup_an_errno(errno, "script socket listen");
#else /*][*/
	popup_an_error("script socket listen: %s",
		win32_strerror(GetLastError()));
#endif /*]*/
	goto fail;
    }

#if !defined(_WIN32) /*[*/
    fcntl(listener->socket, F_SETFD, 1);
#endif /*]*/

#if defined(_WIN32) /*[*/
    listener->event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (listener->event == NULL) {
	popup_an_error("script CreateEvent: %s",
		win32_strerror(GetLastError()));
	goto fail;
    }
    if (WSAEventSelect(listener->socket, listener->event, FD_ACCEPT) != 0) {
	popup_an_error("script WSAEventSelect: %s",
		win32_strerror(GetLastError()));
	goto fail;
    }
    listener->id = AddInput(listener->event, peer_connection);
#else /*][*/
    listener->id = AddInput(listener->socket, peer_connection);
#endif/*]*/

    if (sa->sa_family == AF_INET) {
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;

	listener->desc = Asprintf("%s:%u", inet_ntop(sa->sa_family,
		    &sin->sin_addr, hostbuf, sizeof(hostbuf)),
		ntohs(sin->sin_port));
	vtrace("Listening for s3sock scripts on %s\n", listener->desc);
    } else if (sa->sa_family == AF_INET6) {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

	listener->desc = Asprintf("[%s]:%u", inet_ntop(sa->sa_family,
		    &sin6->sin6_addr, hostbuf, sizeof(hostbuf)),
		ntohs(sin6->sin6_port));
	vtrace("Listening for s3sock scripts on %s\n", listener->desc);
    }
#if !defined(_WIN32) /*[*/
    else if (sa->sa_family == AF_UNIX) {
	struct sockaddr_un *ssun = (struct sockaddr_un *)sa;

	listener->desc = NewString(ssun->sun_path);
	vtrace("Listening for s3sock scripts on %s\n", listener->desc);
    }
#endif /*]*/

    /* Remember it. */
    llist_init(&listener->llist);
    LLIST_APPEND(&listener->llist, peer_listeners);

    /* Done. */
    goto done;

fail:
#if defined(_WIN32) /*[*/
    if (listener->event != INVALID_HANDLE_VALUE) {
	CloseHandle(listener->event);
	listener->event = INVALID_HANDLE_VALUE;
    }
#endif /*]*/
    if (listener->socket != INVALID_SOCKET) {
	SOCK_CLOSE(listener->socket);
	listener->socket = INVALID_SOCKET;
    }
    Replace(listener->desc, NULL);
    Free(listener);
    listener = NULL;

done:
    return listener;
}

/**
 * Stop listening.
 *
 * @param[in] listener	Listening context
 */
void
peer_shutdown(peer_listen_t listener)
{
    if (listener->socket != INVALID_SOCKET) {
	vtrace("Stopped listening for s3sock scripts on %s\n",
		listener->desc);
	SOCK_CLOSE(listener->socket);
	listener->socket = INVALID_SOCKET;
    }
#if defined(_WIN32) /*[*/
    if (listener->event != INVALID_HANDLE_VALUE) {
	CloseHandle(listener->event);
	listener->event = INVALID_HANDLE_VALUE;
    }
#endif /*]*/
    if (listener->id != NULL_IOID) {
	RemoveInput(listener->id);
	listener->id = NULL_IOID;
    }
    llist_unlink(&listener->llist);
    Replace(listener->desc, NULL);
    Free(listener);
}
