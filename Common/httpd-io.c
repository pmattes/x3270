/*
 * Copyright (c) 2014-2015 Paul Mattes.
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
 *      httpd-io.c
 *              x3270 webserver, I/O module
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <unistd.h>
# include <netinet/in.h>
# include <sys/select.h>
# include <arpa/inet.h>
#endif /*]*/
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "appres.h"
#include "lazya.h"
#include "macros.h"
#include "popups.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"

#include "httpd-core.h"
#include "httpd-io.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "winprint.h"
#endif /*]*/

#define IDLE_MAX	15

#define N_SESSIONS	32
typedef struct {
    llist_t link;	/* list linkage */
    socket_t s;		/* socket */
#if defined(_WIN32) /*[*/
    HANDLE event;
#endif /*]*/
    void *dhandle;	/* httpd protocol handle */
    int idle;
    ioid_t ioid;	/* AddInput ID */
    ioid_t toid;	/* AddTimeOut ID */

    struct {		/* pending command state: */
	sendto_callback_t *callback; /* callback function */
	content_t content_type; /* content type */
	varbuf_t result; /* accumulated result data */
	bool done;	/* is the command done? */
    } pending;
} session_t;
llist_t sessions = LLIST_INIT(sessions);
static int n_sessions;
static socket_t listen_s;
#if defined(_WIN32) /*[*/
static HANDLE listen_event;
#endif /*]*/

/**
 * Return the text for the most recent socket error.
 *
 * @return Error text
 */
static const char *
socket_errtext(void)
{
#if !defined(_WIN32) /*[*/
    return strerror(errno);
#else /*][*/
    return win32_strerror(GetLastError());
#endif /*]*/
}

/**
 * Close the session associated with a particular socket.
 * Called from the HTTPD logic when a fatal error or EOF occurs.
 *
 * @param[in] session	Session
 */
static void
hio_socket_close(session_t *session)
{
    SOCK_CLOSE(session->s);
    if (session->ioid != NULL_IOID) {
	RemoveInput(session->ioid);
    }
    if (session->toid != NULL_IOID) {
	RemoveTimeOut(session->toid);
    }
#if defined(_WIN32) /*[*/
    CloseHandle(session->event);
#endif /*]*/
    vb_free(&session->pending.result);
    llist_unlink(&session->link);
    Free(session);
    n_sessions--;
}

/**
 * httpd timeout.
 *
 * @param[in] id	timeout ID
 */
static void
hio_timeout(ioid_t id)
{
    session_t *session;

    session = NULL;
    FOREACH_LLIST(&sessions, session, session_t *) {
	if (session->toid == id) {
	    break;
	}
    } FOREACH_LLIST_END(&sessions, session, session_t *);
    if (session == NULL) {
	vtrace("httpd mystery timeout\n");
	return;
    }

    session->toid = NULL_IOID;
    httpd_close(session->dhandle, "timeout");
    hio_socket_close(session);
}

/**
 * New inbound data for an httpd connection.
 *
 * @param[in] fd	socket file descriptor
 * @param[in] id	I/O ID
 */
void
hio_socket_input(iosrc_t fd, ioid_t id)
{
    session_t *session;
    char buf[1024];
    ssize_t nr;

    session = NULL;
    FOREACH_LLIST(&sessions, session, session_t *) {
	if (session->ioid == id) {
	    break;
	}
    } FOREACH_LLIST_END(&sessions, session, session_t *);
    if (session == NULL) {
	vtrace("httpd mystery input\n");
	return;
    }

    /* Move this session to the front of the list. */
    llist_unlink(&session->link);
    llist_insert_before(&session->link, sessions.next);

    session->idle = 0;

    if (session->toid != NULL_IOID) {
	RemoveTimeOut(session->toid);
	session->toid = NULL_IOID;
    }

    nr = recv(session->s, buf, sizeof(buf), 0);
    if (nr <= 0) {
	const char *ebuf;
	bool harmless = false;

	if (nr < 0) {
	    if (socket_errno() == SE_EWOULDBLOCK) {
		harmless = true;
	    }
	    ebuf = lazyaf("recv error: %s", socket_errtext());
	    vtrace("httpd %s%s\n", ebuf, harmless? " (harmless)": "");
	} else {
	    ebuf = "session EOF";
	}
	if (!harmless) {
	    httpd_close(session->dhandle, ebuf);
	    hio_socket_close(session);
	}
    } else {
	httpd_status_t rv;

	rv = httpd_input(session->dhandle, buf, nr);
	if (rv < 0) {
	    httpd_close(session->dhandle, "protocol error");
	    hio_socket_close(session);
	} else if (rv == HS_PENDING) {
	    /* Stop input on this socket. */
	    RemoveInput(session->ioid);
	    session->ioid = NULL_IOID;
	} else if (session->toid == NULL_IOID) {
	    /* Leave input enabled and start the timeout. */
	    session->toid = AddTimeOut(IDLE_MAX * 1000, hio_timeout);
	}
    }
}

/**
 * New inbound connection for httpd.
 *
 * @param[in] fd	socket file descriptor
 * @param[in] id	I/O ID
 */
void
hio_connection(iosrc_t fd, ioid_t id)
{
    socket_t t;
    union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#if defined(X3270_IPV6) /*[*/
	struct sockaddr_in6 sin6;
#endif /*]*/
    } sa;
    socklen_t len;
    char hostbuf[128];
    session_t *session;

    len = sizeof(sa);
    t = accept(listen_s, &sa.sa, &len);
    if (t == INVALID_SOCKET) {
	vtrace("httpd accept error: %s%s\n", socket_errtext(),
		(socket_errno() == SE_EWOULDBLOCK)? " (harmless)": "");
	return;
    }
    if (n_sessions >= N_SESSIONS) {
	vtrace("Too many connections.\n");
	SOCK_CLOSE(t);
	return;
    }

    session = Malloc(sizeof(session_t));
    memset(session, 0, sizeof(session_t));
    vb_init(&session->pending.result);
    session->s = t;
#if defined(_WIN32) /*[*/
    session->event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (session->event == NULL) {
	vtrace("httpd: can't create socket handle\n");
	SOCK_CLOSE(t);
	Free(session);
	return;
    }
    if (WSAEventSelect(session->s, session->event, FD_READ | FD_CLOSE) != 0) {
	vtrace("httpd: Can't set socket handle events\n");
	CloseHandle(session->event);
	SOCK_CLOSE(t);
	Free(session);
	return;
    }
#endif /*]*/
    if (sa.sa.sa_family == AF_INET) {
	session->dhandle = httpd_new(session,
		lazyaf("%s:%u",
		    inet_ntop(AF_INET, &sa.sin.sin_addr, hostbuf,
			sizeof(hostbuf)),
		    ntohs(sa.sin.sin_port)));
    }
#if defined(X3270_IPV6) /*[*/
    else if (sa.sa.sa_family == AF_INET6) {
	session->dhandle = httpd_new(session,
		lazyaf("%s:%u",
		    inet_ntop(AF_INET6, &sa.sin6.sin6_addr, hostbuf,
			sizeof(hostbuf)),
		    ntohs(sa.sin6.sin6_port)));
    }
#endif /*]*/
    else {
	session->dhandle = httpd_new(session, "???");
    }
#if !defined(_WIN32) /*[*/
    session->ioid = AddInput(t, hio_socket_input);
#else /*][*/
    session->ioid = AddInput(session->event, hio_socket_input);
#endif /*]*/

    /* Set the timeout for the first line of input. */
    session->toid = AddTimeOut(IDLE_MAX * 1000, hio_timeout);

    llist_insert_before(&session->link, sessions.next);
    n_sessions++;
}

/**
 * Initialize the httpd socket.
 *
 * @param[in] sa	address and port to listen on
 * @param[in] sa_len	length of sa
 */
void
hio_init(struct sockaddr *sa, socklen_t sa_len)
{
    int on = 1;

    listen_s = socket(sa->sa_family, SOCK_STREAM, 0);
    if (listen_s == INVALID_SOCKET) {
	popup_an_error("httpd socket: %s", socket_errtext());
	return;
    }
    if (setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
		sizeof(on)) < 0) {
	popup_an_error("httpd setsockopt: %s", socket_errtext());
	SOCK_CLOSE(listen_s);
	listen_s = INVALID_SOCKET;
	return;
    }
    if (bind(listen_s, sa, sa_len) < 0) {
	popup_an_error("httpd bind: %s", socket_errtext());
	SOCK_CLOSE(listen_s);
	listen_s = INVALID_SOCKET;
	return;
    }
    if (listen(listen_s, 10) < 0) {
	popup_an_error("httpd listen: %s", socket_errtext());
	SOCK_CLOSE(listen_s);
	listen_s = INVALID_SOCKET;
	return;
    }
#if defined(_WIN32) /*[*/
    listen_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (listen_event == NULL) {
	popup_an_error("httpd: cannot create listen handle");
	SOCK_CLOSE(listen_s);
	listen_s = INVALID_SOCKET;
	return;
    }
    if (WSAEventSelect(listen_s, listen_event, FD_ACCEPT) != 0) {
	popup_an_error("httpd: WSAEventSelect failed: %s",
		socket_errtext());
	CloseHandle(listen_event);
	listen_event = INVALID_HANDLE_VALUE;
	SOCK_CLOSE(listen_s);
	listen_s = INVALID_SOCKET;
    }
    (void) AddInput(listen_event, hio_connection);
#else /*][*/
    (void) AddInput(listen_s, hio_connection);
#endif /*]*/
}

/**
 * Send output on an http session.
 *
 * @param[in] mhandle	our handle
 * @param[in] buf	buffer to transmit
 * @param[in] len	length of buffer
 */
void
hio_send(void *mhandle, const char *buf, size_t len)
{
    session_t *s = mhandle;
    ssize_t nw;

    nw = send(s->s, buf, (int)len, 0);
    if (nw < 0) {
	vtrace("http send error: %s\n", socket_errtext());
    }
}

/**
 * Incremental data callback from x3270 back to httpd.
 *
 * @param[in] handle	handle
 * @param[in] buf	buffer
 * @param[in] len	size of buffer
 */
static void
hio_data(sms_cbh handle, const char *buf, size_t len)
{
    session_t *s = handle;

    if (s->pending.content_type == CT_HTML) {
	size_t i;
	char c;

	/* Quote HTML in the response. */
	for (i = 0; i < len; i++) {
	    c = buf[i];
	    switch (c) {
	    case '&':
		vb_appends(&s->pending.result, "&amp;");
		break;
	    case '<':
		vb_appends(&s->pending.result, "&lt;");
		break;
	    case '>':
		vb_appends(&s->pending.result, "&gt;");
		break;
	    case '"':
		vb_appends(&s->pending.result, "&quot;");
		break;
	    default:
		vb_append(&s->pending.result, &c, 1);
		break;
	    }
	}
    } else {
	vb_append(&s->pending.result, buf, len);
    }
    vb_appends(&s->pending.result, "\n");
}

/**
 * Completion callback from x3270 back to httpd.
 *
 * @param[in] handle	handle
 * @param[in] success	true if command succeeded
 * @param[in] status_buf status line buffer
 * @param[in] status_len size of status line buffer
 */
static void
hio_complete(sms_cbh handle, bool success, const char *status_buf,
	size_t status_len)
{
    session_t *s = handle;

    /* We're done. */
    s->pending.done = true;

    /* Pass the result up to the node. */
    s->pending.callback(s->dhandle, success? SC_SUCCESS: SC_USER_ERROR,
	    vb_buf(&s->pending.result), vb_len(&s->pending.result), status_buf,
	    status_len);

    /* Get ready for the next command. */
    vb_reset(&s->pending.result);
}

/**
 * Send a command to x3270.
 *
 * @param[in] cmd       command to send, not including the newline and
 * @param[in] callback  callback function for completion
 * @param[in] handle    handle to pass to completion callback function
 * @param[in] content_type How to handle content
 *
 * @return sendto_t
 */
sendto_t
hio_to3270(const char *cmd, sendto_callback_t *callback, void *dhandle,
	content_t content_type)
{
    static sms_cb_t httpd_cb = { "HTTPD", IA_SCRIPT, hio_data, hio_complete };
    size_t sl;
    session_t *s = httpd_mhandle(dhandle);

    sl = strlen(cmd);
    if (sl == 0) {
	/* No empty commands, please. */
	return SENDTO_INVALID;
    }

    /* Remove any trailing NL or CR/LF. */
    if (cmd[sl - 1] == '\n') {
	sl--;
    }
    if (sl && cmd[sl - 1] == '\r') {
	sl--;
    }
    if (!sl || strchr(cmd, '\r') != NULL || strchr(cmd, '\n') != NULL) {
	/* No empty commands, and no embedded CRs or LFs. */
	return SENDTO_INVALID;
    }

    /* Enqueue the command. */
    s->pending.callback = callback;
    s->pending.content_type = content_type;
    s->pending.done = false;
    push_cb(cmd, sl, &httpd_cb, s);

    /*
     * It's possible for the command to have completed already.
     * If so, return SENDTO_COMPLETE.
     * Otherwise, it's just queued; return SENDTO_PENDING.
     */
    return s->pending.done? SENDTO_COMPLETE: SENDTO_PENDING;
}

/**
 * Asynchronous completion.
 *
 * @param[in] dhandle   State
 * @param[in] rv        Completion status
 */
void
hio_async_done(void *dhandle, httpd_status_t rv)
{
    session_t *session = httpd_mhandle(dhandle);

    if (rv < 0) {
	hio_socket_close(session);
	return;
    }

    /* Allow more input. */
    if (session->ioid == NULL_IOID) {
#if !defined(_WIN32) /*[*/
	session->ioid = AddInput(session->s, hio_socket_input);
#else /*][*/
	session->ioid = AddInput(session->event, hio_socket_input);
#endif /*]*/
    }

    /*
     * Set a timeout for that input to arrive. We didn't set this timeout
     * as soon as the last input arrived, because it might have taken us a
     * long time to proces the last request.
     */
    if (session->toid == NULL_IOID) {
	session->toid = AddTimeOut(IDLE_MAX * 1000, hio_timeout);
    }
}
