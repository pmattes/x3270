/*
 * Copyright (c) 2026 Paul Mattes.
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
 *      oq.c
 *              Output queueing.
 */

#include "globals.h"

#include <assert.h>
#include <errno.h>
#if !defined(_WIN32) /*[*/
# include <fcntl.h>
#endif /*]*/
#include "names.h"
#include "query.h"
#include "telnet.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#include "varbuf.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

#include "oq.h"

#define OQ_MAX_DEFAULT	(size_t)(10 * 1024 * 1024)	/* 10 MiB maximum */

#define GiB	(size_t)(1024LL * 1024LL * 1024LL)
#define MiB	(size_t)(1024LL * 1024LL)
#define KiB	(size_t)(1024LL)

#define GB	(size_t)(1000LL * 1000LL * 1000LL)
#define MB	(size_t)(1000LL * 1000LL)
#define KB	(size_t)(1000LL)

/* Pending data chunk. */
typedef struct {
    llist_t list;	/* linkage */
    oq_t oq;		/* associated queue */
    char *data;		/* data to write */
    size_t len;		/* remaining length */
} oq_data_t;

/* Output queue instance. */
struct oq {
    llist_t oq_list;	/* linkage for global set of oq's */
    llist_t data_list;	/* list of data chunks to write */
    char *name;		/* queue name, for tracing */
    tc_t tc;		/* trace class */
    bool is_sock;	/* true if socket */
    union {
	socket_t socket;
	iosrc_t fd;
    } u;
    ioid_t id;		/* I/O identifier for output space available */
    bool errored;	/* true if errored */
    char *errmsg;	/* saved error message */
    size_t cur_bytes;	/* current bytes count */
    size_t max_bytes;	/* maximum number of bytes queued */
    size_t total_bytes; /* total number of bytes written */
};

/* The set of active output queues. */
static llist_t oqs = LLIST_INIT(oqs);

/* The maximum number of bytes queued in any single queue. */
static size_t max_bytes;

/* The total number of bytes written by all queues. */
static size_t total_bytes;

/*
 * The maximum number of bytes to queue.
 * If < 0, do infinite queueing.
 * If == 0, do blocking output if possible.
 * If > 0, queue at most that many bytes.
 */
static size_t oq_max = OQ_MAX_DEFAULT;
static bool oq_enabled = true;
#define OQ_FINITE	(oq_max > 0)

#if defined(_WIN32) /*[*/
static HANDLE stdout_semaphore;
static HANDLE stdout_mutex;
static HANDLE stdout_thread;
static int stdout_errno;
static oq_t stdout_oq;

static void oq_init_stdout(oq_t oq);
static bool oq_enq_stdout(oq_t oq, const char *buf, size_t len, const char **errmsg);
static bool oq_write_stdout(oq_t oq, const char *buf, size_t len, const char **errmsg);
#endif /*]*/

/* Look for a keyword match. */
static bool
kw_match(const char *text, const char *keywords[])
{
    int i;

    for (i = 0; keywords[i] != NULL; i++) {
	if (!strcasecmp(text, keywords[i])) {
	    return true;
	}
    }
    return false;
}

/**
 * Initialize the output queue.
 */
void
oq_init(const char *spec)
{
    static bool initted = false;

    if (initted) {
	return;
    }
    initted = true;

    if (spec == NULL) {
	return;
    }

    static const char *enable_kw[] = { "enable", "enabled", "true", NULL };
    if (kw_match(spec, enable_kw)) {
	return;
    }
    static const char *infinite_kw[] = { "infinite", "infinity", "unlimited", NULL };
    if (kw_match(spec, infinite_kw)) {
	oq_enabled = true;
	oq_max = 0;
	return;
    }
    static const char *disable_kw[] = { "block", "blocking", "disabled", "disable", "none", "false", NULL };
    if (kw_match(spec, disable_kw)) {
	oq_enabled = false;
	oq_max = 0;
	return;
    }

    /*
     * Parse a number followed by an optional [KMG], i and B.
     * B redundantly specifies bytes; it echoes the display format.
     * K/M/G without i are SI units (K == 1000).
     * K/M/G with i are computer units (K == 1024).
     * So 2KB (or 2K) is 2000, and 2KiB (or 2Ki) is 2048.
     */
    char *sc = NewString(spec);
    size_t sl = strlen(sc);
    bool si = false;
    size_t mult = 1;

    if (sl > 0 && sc[sl - 1] == 'B') {
	sc[--sl] = '\0';
    }
    if (sl > 0 && sc[sl - 1] == 'i') {
	si = true;
	sc[--sl] = '\0';
    }
    if (sl > 0 && toupper((unsigned char)sc[sl - 1]) == 'K') {
	mult = si? KiB: KB;
	sc[--sl] = '\0';
    } else if (sl > 0 && toupper((unsigned char)sc[sl - 1]) == 'M') {
	mult = si? MiB: MB;
	sc[--sl] = '\0';
    } else if (sl > 0 && toupper((unsigned char)sc[sl - 1]) == 'G') {
	mult = si? GiB: GB;
	sc[--sl] = '\0';
    }

    char *endptr = NULL;
    long long l = strtoull(sc, &endptr, 10);

    if (*endptr == '\0') {
	oq_enabled = true;
	oq_max = (size_t)l * mult;
    } else {
	xs_warning("Cannot parse queueing spec '%s'", scatv(spec));
    }
    Free(sc);
}

/* Common create function. */
oq_t
oq_create_common(const char *name, tc_t tc, socket_t socket, iosrc_t fd)
{
    size_t sl1 = strlen(name) + 1;
    oq_t oq = (oq_t)Malloc(sizeof(struct oq) + sl1);

    memset(oq, 0, sizeof(*oq));
    llist_init(&oq->oq_list);
    llist_init(&oq->data_list);
    oq->name = (char *)(oq + 1);
    oq->tc = tc;
    strncpy(oq->name, name, sl1);
    oq->is_sock = socket != INVALID_SOCKET;
    if (oq->is_sock) {
	oq->u.socket = socket;
    } else {
	oq->u.fd = fd;
    }
    oq->id = NULL_IOID;
    oq->errored = false;
    oq->cur_bytes = 0;
    oq->max_bytes = 0;
    oq->total_bytes = 0;

    LLIST_APPEND(&oq->oq_list, oqs);

    return oq;
}

/**
 * Create an output queue for a socket.
 * @param[in] name	Name of queue
 * @param[in] tc	Trace class
 * @param[in] socket	Socket to write to
 * @returns handle
 */
oq_t
oq_create_socket(const char *name, tc_t tc, socket_t socket)
{
#if !defined(_WIN32) /*[*/
    const char *errmsg;

    if (!net_nonblocking(socket, &errmsg)) {
	vctrace(tc, "oq_create_socket: net_blocking: %s\n", errmsg);
    }
#endif

    return oq_create_common(name, tc, socket, INVALID_IOSRC);
}

/**
 * Create an output queue for stdout.
 * @param[in] name	Name of queue
 * @param[in] tc	Trace class
 * @returns handle
 */
oq_t
oq_create_stdout(const char *name, tc_t tc)
{
    oq_t oq;

#if !defined(_WIN32) /*[*/
    int f;

    if (oq_enabled) {
	if ((f = fcntl(fileno(stdout), F_GETFL, 0)) < 0) {
	    vctrace(tc, "%s fcntl(F_GETFL): %s\n", name, strerror(errno));
	} else {
	    if (fcntl(fileno(stdout), F_SETFL, f | O_NDELAY) < 0) {
		vctrace(tc, "%s fcntl(F_SETFL): %s\n", name, strerror(errno));
	    }
	}
    }

    oq = oq_create_common(name, tc, INVALID_SOCKET, fileno(stdout));
#else /*][*/
    oq = oq_create_common(name, tc, INVALID_SOCKET, INVALID_IOSRC);
    if (oq_enabled) {
	oq_init_stdout(oq);
    }
#endif /*]*/
    return oq;
}

/* Flush pending data. */
static void
oq_flush(oq_t oq)
{
    while (!llist_isempty(&oq->data_list)) {
	oq_data_t *d = (oq_data_t *)oq->data_list.next;

	llist_unlink(&d->list);
	Free(d);
    }
    if (oq->id != NULL_IOID) {
	RemoveOutput(oq->id);
	oq->id = NULL_IOID;
    }
    oq->cur_bytes = 0;
}

/* There is output space available. */
static void
write_more(iosrc_t fd _is_unused, ioid_t id)
{
    bool found = false;
    oq_t oq;
    ssize_t nw;

    /* Find the correct output queue. */
    FOREACH_LLIST(&oqs, oq, oq_t) {
	if (oq->id == id) {
	    found = true;
	    break;
	}
    } FOREACH_LLIST_END(&oqs, oq, oq_t);
    if (!found) {
	vctrace(TC_INFRA, "oq write_more: cannot find matching active oq\n");
	return;
    }

    while (!llist_isempty(&oq->data_list)) {
	oq_data_t *d = (oq_data_t *)oq->data_list.next;

	if (oq->is_sock) {
	    nw = send(oq->u.socket, d->data, (int)d->len, 0);
	} else {
#if !defined(_WIN32) /*[*/
	    nw = write(oq->u.fd, d->data, (int)d->len);
#else /*][*/
	    assert(false);
#endif /*]*/
	}
	if (nw < 0) {
	    const char *errmsg = socket_strerror(socket_errno());

	    vctrace(oq->tc, "oq %s write_more: write failure: %s\n", oq->name, errmsg);
	    if (IS_EWOULDBLOCK(socket_errno())) {
		nw = 0;
	    } else {
		oq_flush(oq);
		oq->errmsg = NewString(errmsg);
		oq->errored = true;
		return;
	    }
	}

	vctrace(oq->tc, "oq %s bg wrote %zd/%zu -> %zu\n", oq->name, nw, d->len, oq->cur_bytes - nw);
	oq->cur_bytes -= nw;
	if ((size_t)nw < d->len) {
	    /* Short write, including 0 bytes (EWOULDBLOCK). */
	    d->data += nw;
	    d->len -= nw;
	    return;
	}

	/* This chunk is complete. */
	llist_unlink(&d->list);
	Free(d);
    }

    vctrace(oq->tc, "oq %s output queue empty\n", oq->name);
    RemoveOutput(oq->id);
    oq->id = NULL_IOID;
}

/* Set up a data segment. */
static oq_data_t *
oq_enq_segment(oq_t oq, const char *data, size_t len)
{
    oq_data_t *d = Malloc(sizeof(oq_data_t) + len);

    llist_init(&d->list);
    d->oq = oq;
    d->data = (char *)(d + 1);
    memcpy(d->data, data, len);
    d->len = len;
    return d;
}

/* Increment the queue length. */
static size_t
oq_incr_len(oq_t oq, size_t len)
{
    oq->cur_bytes += len;
    if (oq->cur_bytes > oq->max_bytes) {
	oq->max_bytes = oq->cur_bytes;
	if (oq->max_bytes > max_bytes) {
	    max_bytes = oq->max_bytes;
	}
    }
    return oq->cur_bytes;
}

/* Enqueue pending data. */
static void
oq_enq(oq_t oq, const char *data, size_t len)
{
    oq_data_t *d = oq_enq_segment(oq, data, len);

    LLIST_APPEND(&d->list, oq->data_list);
    if (oq->id == NULL_IOID) {
#if !defined(_WIN32) /*[*/
	oq->id = AddOutput(oq->is_sock? oq->u.socket: oq->u.fd, write_more);
#else /*][*/
	oq->id = AddOutput(oq->u.socket, write_more);
#endif /*]*/
    }
    (void) oq_incr_len(oq, len);
}

/* Humanize the queueing limit. */
static const char *
humanize(size_t max)
{
    if (max >= GiB) {
	return txAsprintf("%zdGiB", max / GiB);
    } else if (max >= MiB) {
	return txAsprintf("%zdMiB", max / MiB);
    } else if (max >= KiB) {
	return txAsprintf("%zdKiB", max / KiB);
    } else {
	return txAsprintf("%zdB", max);
    }
}

/**
 * Write data to an output queue.
 * @param[in] oq	Handle
 * @param[in] data	Data buffer
 * @param[in] len	Length of data
 * @param[out] errmsg	Error message, returned on failure
 * @returns true for success, false for failure
 */
bool
oq_write(oq_t oq, const char *data, size_t len, const char **errmsg)
{
    ssize_t nw;

    *errmsg = NULL;

    oq->total_bytes += len;
    total_bytes += len;

    if (oq->errored) {
	*errmsg = oq->errmsg;
	return false;
    }

    /* Check for output overflow. */
    if (OQ_FINITE && oq->cur_bytes > oq_max) {
	oq->errmsg = Asprintf("Unread output exceeded %s bytes, data lost", humanize(oq_max));
	vctrace(oq->tc, "oq %s %s\n", oq->name, oq->errmsg);
	*errmsg = oq->errmsg;
	oq->errored = true;
	return false;
    }

#if defined(_WIN32) /*[*/
    if (!oq->is_sock) {
	return oq_enabled? oq_enq_stdout(oq, data, len, errmsg): oq_write_stdout(oq, data, len, errmsg);
    }
#endif /*]*/

    if (oq->id != NULL_IOID) {
	vctrace(oq->tc, "oq %s pending +%zd -> %zd\n", oq->name, len, oq->cur_bytes + len);
	oq_enq(oq, data, len);
	return true;
    }

    if (oq->is_sock) {
	nw = send(oq->u.socket, data, (int)len, 0);
    } else {
#if !defined(_WIN32) /*[*/
	nw = write(oq->u.fd, data, len);
#else /*][*/
	assert(false);
#endif /*]*/
    }
    if (nw < 0) {
	if (IS_EWOULDBLOCK(socket_errno())) {
	    nw = 0;
	} else {
	    *errmsg = socket_strerror(socket_errno());
	    oq->errmsg = NewString(*errmsg);
	    oq_flush(oq);
	    oq->errored = true;
	    return false;
	}
    }
    if ((size_t)nw < len) {
	vctrace(oq->tc, "oq %s pending %zd\n", oq->name, len - nw);
	oq_enq(oq, data + nw, len - nw);
    }
    return true;
}

/**
 * Check an output queue for a fatal error.
 *
 * @param[out] errmsg	Error message.
 *
 * @return true for a fatal error.
 */
bool
oq_errored(oq_t oq, const char **errmsg)
{
    *errmsg = oq->errored? oq->errmsg: NULL;
    return oq->errored;
}

/**
 * Free an output queue, discarding any pending data.
 * @param[in,out] oq	Handle
 */
void
oq_free(oq_t *oq)
{
    oq_flush(*oq);
    llist_unlink(&(*oq)->oq_list);
    Replace((*oq)->errmsg, NULL);
    memset(*oq, 0, sizeof(struct oq));
    *oq = NULL;
}

#if defined(_WIN32) /*[*/
/* Output queueing for standard output on Windows. */

/* Standard output writer thread. */
static DWORD WINAPI
oq_stdout_writer(LPVOID lp_parameter)
{
    while (true) {
	while (WaitForSingleObject(stdout_semaphore, INFINITE) == WAIT_OBJECT_0) {
	    oq_data_t *d;

	    /* The semaphore should guarantee this. */
	    assert(!llist_isempty(&stdout_oq->data_list));

	    /* Dequeue the data element under the mutex. */
	    if (WaitForSingleObject(stdout_mutex, INFINITE) != WAIT_OBJECT_0) {
		xs_error("oq: WaitForSingleObject failed: %s", win32_strerror(GetLastError()));
	    }
	    d = (oq_data_t *)stdout_oq->data_list.next;
	    llist_unlink(&d->list);
	    stdout_oq->cur_bytes -= d->len;
	    if (ReleaseMutex(stdout_mutex) == 0) {
		xs_error("oq: ReleaseMutex failed: %s", win32_strerror(GetLastError()));
	    }

	    /* N.B.: We can't trace the queue length changes here, because the tracing functions are not thread-safe. */

	    /* Write it out and free it. */
	    if (write(fileno(stdout), d->data, (int)d->len) < 0) {
		stdout_errno = errno? errno: EINVAL;
	    }
	    Free(d);
	}
    }

    return 0;
}

/* Exiting handler. */
static void
oq_exiting(bool ignored)
{
    DWORD t0;

    /* Wait up to 1 second (1000ms) for the queue to flush. */
    t0 = GetTickCount();
    while (!llist_isempty(&stdout_oq->data_list)) {
	DWORD t1 = GetTickCount();
	DWORD diff;

	if (t1 > t0) {
	    diff = t1 - t0;
	} else {
	    /* Wrapped. */
	    diff = (DWORD)((t1 + 0x100000000ULL) - t0);
	}
	if (diff > 1000) {
	    /* Too long. */
	    return;
	}
	Sleep(100);
    }
}

/**
 * Standard output writer initialization.
 */
static void
oq_init_stdout(oq_t oq)
{
    stdout_oq = oq;
    stdout_semaphore = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
    if (stdout_semaphore == INVALID_HANDLE_VALUE) {
	xs_error("oq_init_stdout: CreateSemaphore failed: %s", win32_strerror(GetLastError()));
    }
    stdout_mutex = CreateMutex(NULL, FALSE, NULL);
    if (stdout_mutex == NULL) {
	xs_error("oq_init_stdout: CreateMutex failed: %s", win32_strerror(GetLastError()));
    }
    stdout_thread = CreateThread(NULL, 0, oq_stdout_writer, NULL, 0, NULL);
    if (stdout_thread == NULL) {
	xs_error("oq_init_stdout: CreateThread failed: %s", win32_strerror(GetLastError()));
    }

    /*
     * Set up a state-change handler for exiting, second-last. Last is the 'any key to continue' prompt.
     * This is needed because we enqueue all output to the output thread, unlike on POSIX where we
     * enqueue only when we're backed up. So there might be pending output that could be displayed
     * synchronously, but the output thread simply hasn't seen it yet.
     */
    register_schange_ordered(ST_EXITING, oq_exiting, ORDER_LAST - 1);
}

/**
 * Enqueue data to write to stdout.
 *
 * @param[in] oq	Output queue.
 * @param[in] buf	Buffer to write.
 * @param[in] len	Length of buffer.
 * @param[out] errmsg	Error message.
 *
 * @returns true for success, false for failure.
 */
static bool
oq_enq_stdout(oq_t oq, const char *buf, size_t len, const char **errmsg)
{
    oq_data_t *d = oq_enq_segment(oq, buf, len);
    size_t len_before, len_after;

    *errmsg = NULL;

    if (stdout_errno != 0) {
	oq->errmsg = NewString(strerror(stdout_errno));
	*errmsg = oq->errmsg;
	oq->errored = true;
	return false;
    }

    /* Enqueue under the mutex. */
    if (WaitForSingleObject(stdout_mutex, INFINITE) != WAIT_OBJECT_0) {
	xs_error("oq_enq_stdout: WaitForSingleObject failed: %s", win32_strerror(GetLastError()));
    }
    LLIST_APPEND(&d->list, oq->data_list);
    len_before = oq->cur_bytes;
    len_after = oq_incr_len(oq, len);
    if (ReleaseMutex(stdout_mutex) == 0) {
	xs_error("oq: ReleaseMutex failed: %s", win32_strerror(GetLastError()));
    }

    /* Let the thread go. */
    if (ReleaseSemaphore(stdout_semaphore, 1, NULL) == 0) {
	xs_error("oq_enq_stdout: ReleaseSemaphore failed: %s", win32_strerror(GetLastError()));
    }

    /*
     * Trace increases in the queue length if the length was nonzero before.
     * This gives us some useful information about falling behind without tracing every single
     * output operation.
     */
    if (len_before > 0) {
	vctrace(oq->tc, "oq %s pending +%zd -> %zd\n", oq->name, len, len_after);
    }

    return true;
}

/**
 * Write data directly to stdout (Windows)
 *
 * @param[in] oq	Output queue.
 * @param[in] buf	Buffer to write.
 * @param[in] len	Length of buffer.
 * @param[out] errmsg	Returned error message.
 *
 * @returns true for success.
 */
static bool
oq_write_stdout(oq_t oq, const char *buf, size_t len, const char **errmsg)
{
    *errmsg = NULL;
    if (write(fileno(stdout), buf, (unsigned int)len) < 0) {
	oq->errmsg = NewString(strerror(errno));
	*errmsg = oq->errmsg;
	oq->errored = true;
	return false;
    }

    return true;
}
#endif /*]*/

/*
 * Dump output queue stats.
 */
static const char *
oq_dump(void)
{
    varbuf_t r;
    oq_t oq;
    size_t total_pending = 0;
    bool any = false;

    vb_init(&r);

    if (oq_enabled) {
	if (OQ_FINITE) {
	    vb_appendf(&r, "queueing enabled limit %s", humanize(oq_max));
	} else {
	    vb_appendf(&r, "queueing enabled limit infinite");
	}
	any = true;
	FOREACH_LLIST(&oqs, oq, oq_t) {
	    vb_appendf(&r, "%s%s queued %zd maximum-queued %zd written %zd",
		    any? "\n": "", oq->name, oq->cur_bytes, oq->max_bytes, oq->total_bytes);
	    total_pending += oq->cur_bytes;
	    any = true;
	} FOREACH_LLIST_END(&oqs, oq, oq_t);
	vb_appendf(&r, "%stotal queued %zd maximum-queued %zd written %zd",
		any? "\n": "", total_pending, max_bytes, total_bytes);

	return txdFree(vb_consume(&r));
    } else {
	return "queueing disabled";
    }
}

/**
 * Register our queries.
 */
void
oq_register(void)
{
    static query_t queries[] = {
        { KwOutputQueues, oq_dump, NULL, QF_HIDDEN | QF_TRACEHDR | QF_MULTILINE },
    };

    /* Register our queries. */
    register_queries(queries, array_count(queries));
}
