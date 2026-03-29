/*
 * Copyright (c) 1999-2026 Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * 	sched.c
 * 		Task scheduler.
 */

#include "globals.h"
#if defined(WIN32_HEAP_CHECK) /*[*/
# include <malloc.h>
#endif /*]*/
#include "glue.h"
#include "appres.h"
#include "task.h"
#include "timeouts.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#if !defined(_WIN32) /*[*/
# include <sys/wait.h>
#endif /*]*/

#if defined(SEPARATE_SELECT_H) /*[*/
# include <sys/select.h>
#endif /*]*/
#if defined(HAVE_SYS_POLL_H) /*[*/
# include <sys/poll.h>
#endif /*]*/

enum condition {
    WantRead,
    WantExcept,
    WantWrite,
};

#define SF_CURRENT	0x1	/* part of the current iteration */
#define SF_RAN		0x2	/* ran this iteration */

/* Input events. */
typedef struct input {
    struct input *next;
    iosrc_t source;		/* source */
    enum condition condition;	/* condition desired */
    iofn_t proc;		/* callback */
    bool valid;			/* true if not deleted */
    unsigned sflags;		/* sched flags */
#if defined(_WIN32) /*[*/
    bool is_socket;		/* true if this is a Windows socket */
#endif /*]*/
    int wait_index;		/* linear index when searching for matches */
} input_t;
static input_t *inputs = NULL;
static bool inputs_changed = false;

#if defined(_WIN32) /*[*/
/* The Windows socket table. */
typedef struct wst {
    struct wst *next;		/* linkage */
    socket_t socket;		/* socket */
    HANDLE event;		/* WSAEventSelect event */
    long network_events;	/* mask of FD_XXX */
    WSANETWORKEVENTS events;	/* network events read */
    bool events_valid;		/* true if events is valid */
    int refcnt;			/* reference count */
} wst_t;
static wst_t *wst = NULL;
#define IP_EVENTS(ip)	(((ip)->condition == WantRead)? (FD_ACCEPT | FD_CONNECT | FD_READ | FD_CLOSE): FD_WRITE)

/* Expand a set of network events for display. */
const char *
sched_expand_network_events(long fds)
{
    return txAsprintf("[0x%lx%s%s%s%s%s]", fds,
	    (fds & FD_ACCEPT)? " ACCEPT": "",
	    (fds & FD_CONNECT)? " CONNECT": "",
	    (fds & FD_READ)? " READ": "",
	    (fds & FD_CLOSE)? " CLOSE": "",
	    (fds & FD_WRITE)? " WRITE": "");
}

/* Add an entry to the socket table. */
static iosrc_t
add_socket_table(socket_t socket, long network_events)
{
    wst_t *w;

    /* Search for an existing entry. */
    for (w = wst; w != NULL; w = w->next) {
	if (w->socket == socket) {
	    break;
	}
    }

    if (w != NULL) {
	/* Existing socket. */
	w->refcnt++;
	vcdtrace(TC_SCHED, "wst inc refcnt socket S0x%lx event 0x%lx events %s\n",
		(unsigned long)(size_t)socket,
		(unsigned long)(size_t)w->event,
		sched_expand_network_events(network_events));
    } else {
	/* New socket. */
	w = Malloc(sizeof(wst_t));
	w->socket = socket;
	w->event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (w->event == NULL) {
	    xs_error("add_socket_table: Cannot create socket handle: %s\n", win32_strerror(GetLastError()));
	}
	w->network_events = network_events;
	if (WSAEventSelect(socket, w->event, w->network_events) != 0) {
	    xs_error("add_socket_table: WSAEventSelect failed: %s\n", win32_strerror(GetLastError()));
	}
	w->refcnt = 1;
	memset(&w->events, 0, sizeof(WSANETWORKEVENTS));
	w->events_valid = false;
	w->next = wst;
	wst = w;
	vcdtrace(TC_SCHED, "wst created socket S0x%lx event 0x%lx events %s\n",
		(unsigned long)(size_t)socket,
		(unsigned long)(size_t)w->event,
		sched_expand_network_events(network_events));
    }
    return (iosrc_t)w->event;
}

/* Remove an entry from the socket table. */
static void
remove_socket_table(HANDLE event)
{
    wst_t *w, *prev = NULL;

    vcdtrace(TC_SCHED, "wst remove event 0x%lx\n", (unsigned long)(size_t)event);

    for (w = wst; w != NULL; w = w->next) {
	if (w->event == event) {
	    break;
	}
	prev = w;
    }
    if (w == NULL) {
	vctrace(TC_SCHED, " remove_socket_table: cannot find event\n");
	return;
    }
    if (!--w->refcnt) {
	vcdtrace(TC_SCHED, " refcnt == 0, wst deleting event 0x%lx\n", (unsigned long)(size_t)w->event);

	/* Free the entry. */
	CloseHandle(w->event);
	if (prev != NULL) {
	    prev->next = w->next;
	} else {
	    wst = w->next;
	}
	memset(w, 0, sizeof(wst_t));
	Free(w);
    } else {
	vcdtrace(TC_SCHED, " refcnt != 0, wst not deleting event 0x%lx\n", (unsigned long)(size_t)w->event);
    }
}
#endif /*]*/

/* Appends a new input event to the list of pending events. */
static void
append_input(input_t *ip)
{
    input_t *prev = NULL, *i;

    for (i = inputs; i != NULL; i = i->next) {
	prev = i;
    }
    if (prev != NULL) {
	prev->next = ip;
    } else {
	inputs = ip;
    }
    ip->next = NULL;
}

#if !defined(_WIN32) /*[*/
ioid_t
AddInput(iosrc_t source, iofn_t fn)
#else /*][*/
static ioid_t
AddInputCommon(iosrc_t source, iofn_t fn, bool is_socket)
#endif /*]*/
{
    input_t *ip;

    assert(source != INVALID_IOSRC);

    ip = (input_t *)Malloc(sizeof(input_t));
    ip->source = source;
    ip->condition = WantRead;
    ip->proc = fn;
    ip->valid = true;
    ip->sflags = 0;
#if defined(_WIN32) /*[*/
    ip->is_socket = is_socket;
#endif /*]*/
    ip->wait_index = -1;
    append_input(ip);
    inputs_changed = true;
    vcdtrace(TC_SCHED, "AddInput 0x%lx\n", (unsigned long)(size_t)source);
    return (ioid_t)ip;
}

#if defined(_WIN32) /*[*/
ioid_t
AddInput(iosrc_t source, iofn_t fn)
{
    return AddInputCommon(source, fn, false);
}

ioid_t
AddInputSocket(socket_t socket, long events, iofn_t fn)
{
    iosrc_t event;

    if (events & FD_READ) {
	/* If we're reading, we're probably going to want to write, too. */
	events |= FD_WRITE;
    }
    event = add_socket_table(socket, events);
    vcdtrace(TC_SCHED, "AddInputSocket S0x%lx 0x%lx %s\n", (unsigned long)(size_t)socket, (unsigned long)(size_t)event,
	    sched_expand_network_events(events));
    return AddInputCommon(event, fn, true);
}
#endif /*]*/

ioid_t
AddExcept(socket_t socket, iofn_t fn)
{
#if defined(_WIN32) /*[*/
    return NULL_IOID;
#else /*][*/
    input_t *ip;

    ip = (input_t *)Malloc(sizeof(input_t));
    ip->source = socket;
    ip->condition = WantExcept;
    ip->proc = fn;
    ip->valid = true;
    ip->sflags = 0;
    append_input(ip);
    inputs_changed = true;
    return (ioid_t)ip;
#endif /*]*/
}

ioid_t
AddOutput(socket_t socket, iofn_t fn)
{
    input_t *ip;

    ip = (input_t *)Malloc(sizeof(input_t));
#if !defined(_WIN32) /*[*/
    ip->source = socket;
#else /*][*/
    ip->source = add_socket_table(socket, FD_READ | FD_CLOSE | FD_WRITE);
#endif /*]*/
    ip->condition = WantWrite;
    ip->proc = fn;
    ip->valid = true;
    ip->sflags = 0;
#if defined(_WIN32) /*[*/
    ip->is_socket = true;
#endif /*]*/
    ip->wait_index = -1;
    append_input(ip);
    inputs_changed = true;
    vcdtrace(TC_SCHED, "AddOutput 0x%lx\n", (unsigned long)(size_t)ip->source);
    return (ioid_t)ip;
}

/* Common code for RemoveInput/RemoveOutput/RemoveExcept. */
static void
remove_common(ioid_t id, const char *name)
{
    input_t *ip;

    for (ip = inputs; ip != NULL; ip = ip->next) {
	if (ip->valid && ip == (input_t *)id) {
	    ip->valid = false;
	    vcdtrace(TC_SCHED, "%s 0x%lx\n", name, (unsigned long)(size_t)ip->source);
#if defined(_WIN32) /*[*/
	    if (ip->is_socket) {
		remove_socket_table(ip->source);
		ip->source = INVALID_IOSRC;
	    }
#endif /*]*/
	    inputs_changed = true;
	    return;
	}
    }
}

void
RemoveInput(ioid_t id)
{
    remove_common(id, "RemoveInput");
}

void
RemoveOutput(ioid_t id)
{
    remove_common(id, "RemoveOutput");
}

void
RemoveExcept(ioid_t id)
{
    remove_common(id, "RemoveExcept");
}

#if !defined(_WIN32) /*[*/
/* Child exit events. */
typedef struct child_exit {
    struct child_exit *next;
    pid_t pid;
    childfn_t proc;
} child_exit_t;
static child_exit_t *child_exits = NULL;

ioid_t
AddChild(pid_t pid, childfn_t fn)
{
    child_exit_t *cx;

    assert(pid != 0 && pid != -1);

    cx = (child_exit_t *)Malloc(sizeof(child_exit_t));
    cx->pid = pid;
    cx->proc = fn;
    cx->next = child_exits;
    child_exits = cx;
    return (ioid_t)cx;
}

/**
 * Poll for an exited child processes.
 *
 * @return true if a waited-for child exited
 */
static bool
poll_children(void)
{
    pid_t pid;
    int status = 0;
    child_exit_t *c;
    child_exit_t *next = NULL;
    child_exit_t *prev = NULL;
    bool any = false;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	for (c = child_exits; c != NULL; c = next) {
	    next = c->next;
	    if (c->pid == pid) {
		(*c->proc)((ioid_t)c, status);
		if (prev) {
		    prev->next = next;
		} else {
		    child_exits = next;
		}
		Free(c);
		any = true;
	    } else {
		prev = c;
	    }
	}
    }
    return any;
}
#endif /*]*/

#if defined(HAVE_POLL) /*[*/
static struct pollfd *fds = NULL;
static int fds_allocated = 0;
# define FDS_ALLOCATE_INCREMENT	128

/* Reallocate the fds array. */
static void
realloc_fds(int count)
{
    if (fds_allocated < count) {
	fds_allocated += FDS_ALLOCATE_INCREMENT;
	fds = (struct pollfd *)Realloc(fds, fds_allocated * sizeof(struct pollfd));
    }
}

/* Find an entry in the fds array. */
static struct pollfd *
find_pollfd(struct pollfd *fds, nfds_t nfds, int fd, bool can_fail)
{
    static struct pollfd no_pollfd = { 0, 0, 0 };
    nfds_t i;

    for (i = 0; i < nfds; i++) {
	if (fds[i].fd == fd) {
	    return &fds[i];
	}
    }
    return can_fail? NULL: &no_pollfd;
}

/*
 * Adds a new event to poll for.
 * Returns the amount to increment nfds.
 */
static int
add_poll(input_t *ip, int events, nfds_t nfds)
{
    struct pollfd *p = find_pollfd(fds, nfds, ip->source, true);
    if (p != NULL) {
	p->events |= events;
	return 0;
    } else {
	realloc_fds(nfds + 1);
	fds[nfds].fd = ip->source;
	fds[nfds].events = events;
	fds[nfds].revents = 0;
	return 1;
    }
}
#endif /*]*/

#if defined(_WIN32) /*[*/
/*
 * This allows handling more than MAXIMUM_WAIT_OBJECTS handles with WaitForMultipleObjects.
 *
 * Below (MAXIMUM_WAIT_OBJECTS - 1) handles, the main thread simply calls WaitForMultipleObjects.
 * A wait thread is created for every set of (MAXIMUM_WAIT_OBJECTS - 1) beyond that. A common 'done'
 * event is used to indicate that one or more wait threads or the main thread has completed
 * WaitForMultipleObjects; it is the first handle in each group of handles that is waited for.
 *
 * A semaphore is used to coordinate the completion of a wait cycle by all of the threads. When a
 * wait thread is done with a cycle, it increments the semaphore; when the main thread completes
 * WaitForMultipleObjects, it decrements the semaphore for every active wait thread.
 *
 * Each wait thread also has a 'go' event, used to control when it should execute the next iteration
 * of waiting.
 *
 * The wait threads are never deallocated. If the number of handles drops below the need for a given
 * thread, it simply isn't used for that cycle (its 'go' event is never set and it is not waited for
 * through the semaphore).
 */

/*
 * Returns the maximum number of wait objects per thread.
 * Normally this is MAXIMUM_WAIT_OBJECTS - 1, but it can be made smaller for debug purposes
 * using an envronment variable.
 */
static int
maximum_wait_objects(void)
{
    static bool done = false;
    static int override = MAXIMUM_WAIT_OBJECTS - 1;

    if (done) {
	return override;
    }

    if (appres.ut_env) {
	char *mwo_override = getenv("MWO");

	if (mwo_override != NULL) {
	    int mwo_value = atoi(mwo_override);

	    if (mwo_value > 0 && mwo_value < MAXIMUM_WAIT_OBJECTS - 1) {
		override = mwo_value;
	    }
	}
    }

    done = true;
    return override;
}

/* A waitgroup: A set of wait handles and a 'go' event used by one event wait thread. */
typedef struct {
    HANDLE ha[MAXIMUM_WAIT_OBJECTS];	/* wait handles, first is 'done' event */
    int nha;				/* number of handles */
    DWORD ret;				/* return value */
    HANDLE go_event;			/* go event */
} waitgroup_t;
static waitgroup_t *waitgroups;
static int num_wait_groups;		/* The number of wait groups allocated. */
static HANDLE done_event;		/* Event that indicates that this cycle of events is complete. */
static HANDLE event_semaphore;		/* Semaphore used to tell the main thread that the event wait threads
					   have completed this cycle. */

/* Event wait thread. */
static DWORD WINAPI
wait_thread(LPVOID parameter)
{
    int index = (int)(size_t)parameter;

    while (true) {
	/* Wait for the 'go' event. */
	if (WaitForSingleObject(waitgroups[index].go_event, INFINITE) == WAIT_FAILED) {
	    xs_warning("sched: wait_thread(%d): WaitForSingleObject(go_event) failed: %s", index, win32_strerror(GetLastError()));
	    break;
	}

	/* Wait. */
	waitgroups[index].ret = WaitForMultipleObjects(waitgroups[index].nha, waitgroups[index].ha, FALSE, INFINITE);
	if (waitgroups[index].ret == WAIT_FAILED) {
	    xs_warning("sched: wait_thread(%d): WaitForMultipleObjects(%d) failed: %s", index, waitgroups[index].nha,
		    win32_strerror(GetLastError()));
	    break;
	}

	/* Done. Signal the done event and release the semaphore. */
	if (waitgroups[index].ret != WAIT_OBJECT_0) {
	    if (!SetEvent(done_event)) {
		xs_warning("sched: wait_thread(%d): SetEvent failed: %s", index, win32_strerror(GetLastError()));
		break;
	    }
	}
	if (!ReleaseSemaphore(event_semaphore, 1L, NULL)) {
	    xs_warning("sched: wait_thread(%d): ReleaseSemaphore failed: %s", index, win32_strerror(GetLastError()));
	    break;
	}
    }
    xs_error("sched: wait_thread(%d) failure", index);
    return 0;
}

/* Allocate a new wait group. */
static void
allocate_wait_group(void)
{
    vctrace(TC_SCHED, "Allocating wait group %d\n", num_wait_groups);
    waitgroups = (waitgroup_t *)Realloc(waitgroups, (num_wait_groups + 1) * sizeof(waitgroup_t));
    waitgroups[num_wait_groups].ha[0] = done_event;
    waitgroups[num_wait_groups].nha = 1;

    /* Wait group 0 is used by the main thread. Subsequent groups get an event wait thread allocated. */
    if (num_wait_groups > 0) {
	HANDLE h;

	waitgroups[num_wait_groups].go_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (waitgroups[num_wait_groups].go_event == NULL) {
	    xs_error("sched: Cannot create go_event(%d): %s", num_wait_groups, win32_strerror(GetLastError()));
	}
	h = CreateThread(NULL, 0, wait_thread, (LPVOID)(size_t)num_wait_groups, 0, NULL);
	if (h == NULL) {
	    xs_error("sched: Cannot create wait_thread %d\n", num_wait_groups);
	}
    }
    ++num_wait_groups;
}
#endif /*]*/

#if defined(HAVE_POLL) /*[*/
/* Counts the bits in an integer. */
static unsigned
bit_count(unsigned value)
{
    unsigned count = 0;

    while (value > 0) {
	if (value & 1) {
            count++;
	}
        value >>= 1;
    }
    return count;
}

/* Counts the number or events we ask for in poll(). */
# if 0
static int
count_events(struct pollfd *fds, nfds_t nfds)
{
    nfds_t *i;
    int ret = 0;

    for (i = 0; i < nfds; i++) {
	ret += count_bits(fds[i].revents & 0xffff);
    }
}
# endif

/* Counts the number or events we got back from poll(). */
static int
count_revents(struct pollfd *fds, nfds_t nfds)
{
    nfds_t i;
    int ret = 0;

    for (i = 0; i < nfds; i++) {
	ret += bit_count(fds[i].revents & 0xffff);
    }
    return ret;
}
#endif /*]*/

#if defined(_WIN32) /*[*/
/*
 * Sets up the wait group state for an event.
 * Returns true if one was added (not a duplicate).
 */
static bool
set_wait_group(input_t *ip, DWORD ha_total, int *wait_index)
{
    int wait_thread_index = ha_total / maximum_wait_objects();
    int i;

    /* Check for a duplicate. */
    for (i = 0; i < (int)ha_total; i++) {
	if (waitgroups[i / maximum_wait_objects()].ha[1 + (i % maximum_wait_objects())] == ip->source) {
	    vcdtrace(TC_SCHED, "set_wait_group: dup\n");
	    *wait_index = i;
	    return false; /* dup */
	}
    }

    if (wait_thread_index > num_wait_groups - 1) {
	allocate_wait_group();
    }
    waitgroups[wait_thread_index].ha[waitgroups[wait_thread_index].nha] = ip->source;
    waitgroups[wait_thread_index].nha++;
    *wait_index = ha_total;
    return true;
}

/* Signals the wait threads to proceed. */
static void
wait_threads_go(void)
{
    int i;

    for (i = 1; i < num_wait_groups; i++) {
	if (waitgroups[i].nha > 1 && !SetEvent(waitgroups[i].go_event)) {
	    xs_error("sched: Cannot set go_event(%d): %s", i, win32_strerror(GetLastError()));
	}
    }
}

/* Synchronizes the wait threads after the main thread has finished WaitForMultipleObjects(). */
static void
sync_wait_threads(DWORD ret)
{
    int i;

    /* Signal the other threads to stop, and wait for them to do it. */
    if (ret != WAIT_OBJECT_0) {
	if (!SetEvent(done_event)) {
	    xs_error("sched: Cannot set done_event: %s", win32_strerror(GetLastError()));
	}
    }
    for (i = 1; i < num_wait_groups; i++) {
	if (waitgroups[i].nha > 1 && WaitForSingleObject(event_semaphore, INFINITE) == WAIT_FAILED) {
	    xs_error("sched: WaitForSingleObject(semaphore %d) failed: %s", i, win32_strerror(GetLastError()));
	}
    }
    for (i = 1; i < num_wait_groups; i++) {
	if (waitgroups[i].nha > 1 && waitgroups[i].ret != WAIT_OBJECT_0) {
	    vtrace("sched: Got sub-event 0x%lx from %d\n", waitgroups[i].ret, i);
	}
    }

    /* Get ready for the next iteration. */
    if (!ResetEvent(done_event)) {
	xs_error("sched: Cannot reset done_event: %s", win32_strerror(GetLastError()));
    }
}
#endif /*]*/

/*
 * Purge the deleted inputs, and move anything that ran to the back.
 *
 * The first allows callbacks to delete entries as a side-effect of processing events without having to
 * restart the whole scheduling process, as earlier versions of this code did.
 *
 * The second is an attempt to avoid starvation, on Windows in particular, which only tells us about the
 * first completion in each block of handles.
 */
static void
purge_inputs(void)
{
    input_t *ip, *prev = NULL, *next = NULL, *hold_first = NULL, *hold_last = NULL;

    /* Delete anything that isn't valid any more, and move everything that ran to a hold queue. */
    for (ip = inputs; ip != NULL; ip = next) {
	next = ip->next;
	if (!ip->valid || (ip->sflags & SF_RAN)) {
	    /* Remove from the queue. */
	    if (prev != NULL) {
		prev->next = next;
	    } else {
		inputs = next;
	    }
	    if (!ip->valid) {
		Free(ip);
	    } else {
		/* Move to the tail of the hold queue. */
		ip->sflags &= ~SF_RAN;
		if (hold_last != NULL) {
		    hold_last->next = ip;
		} else {
		    hold_first = ip;
		}
		hold_last = ip;
		ip->next = NULL;
	    }
	} else {
	    prev = ip;
	}
    }

    /* Move the hold queue to the end of the global queue. */
    if (hold_first != NULL) {
#if defined(HAVE_POLL) /*[*/
	if (appres.ut_env && getenv("ORDER") != NULL) {
	    vctrace(TC_SCHED, "Moved to rear:");
	    for (ip = hold_first; ip != NULL; ip = ip->next) {
		vtrace(" %d", ip->source);
	    }
	    vtrace("\n");
	}
#endif /*]*/
	if (prev != NULL) {
	    prev->next = hold_first;
	} else {
	    inputs = hold_first;
	}
    }
}

#if defined(_WIN32) /*[*/
/* Check an event for completion. */
static bool
is_ready(input_t *ip, enum condition condition)
{
    wst_t *w;
    bool ret;

    if (!ip->is_socket) {
	/* Not a socket, only valid for Read events. */
	return condition == WantRead;
    }

    /* Find the socket table entry. */
    vctrace(TC_SCHED, "is_ready: looking for 0x%lx %s ", (unsigned long)(size_t)ip->source, (condition == WantRead)? "Read": "Write");
    for (w = wst; w != NULL; w = w->next) {
	if (w->event == ip->source) {
	    break;
	}
    }
    assert(w != NULL);

    if (!w->events_valid) {
	/* Fetch the completed network events. */
        if (WSAEnumNetworkEvents(w->socket, w->event, &w->events) != 0) {
	    xs_error("sched: WSAEnumNetworkEvents failed: %s\n", win32_strerror(WSAGetLastError()));
	}
	w->events_valid = true;
	vctrace(TC_SCHED, "new ");
    }
    vctrace(TC_SCHED, "events %s ", sched_expand_network_events(w->events.lNetworkEvents));

    ret = (w->events.lNetworkEvents & IP_EVENTS(ip)) != 0;
    vctrace(TC_SCHED, "%s\n", ret? "success": "failure");
    return ret;
}

/* Clear the network events for all sockets. */
static void
clear_wst_events(void)
{
    wst_t *w;

    for (w = wst; w != NULL; w = w->next) {
	memset(&w->events, 0, sizeof(WSANETWORKEVENTS));
	w->events_valid = false;
    }
}
#endif /*]*/

/*
 * Inner event dispatcher.
 * Processes one or more pending I/O and timeout events.
 * Waits for the first event if block is true.
 * Returns in *processed_any if any events were processed.
 *
 * Returns true if all pending events have been processed.
 * Returns false if the set of events changed while events were being processed
 *  and new ones may be ready; this function should be called again (with block
 *  set to false) to try to process them.
 *
 * Please forgive the heavily-interleaved #ifdef style here.
 * I think it is easier to read and maintain as a single function with three variants of each
 * step, rather than three different platform-specific functions.
 */
#if defined(__clang__) /*[*/
/* This function is so much uglier without the common code that uselessly sets 'i'. */
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif /*]*/
static bool
process_some_events(bool block, bool *processed_any)
{
#if defined(_WIN32) /*[*/
    static bool initted = false;
    DWORD ha_total = 0;
    DWORD ret;
#else /*][*/
    int ne = 0;
    int ns;
# if defined(HAVE_POLL) /*[*/
    nfds_t nfds = 0;
    int events;
# else /*][*/
    fd_set rfds, wfds, xfds;
    struct timeval twait;
# endif /*]*/
#endif /*]*/
    TIMEOUT_T tmo;
    input_t *ip;
    bool any_events_pending;
    int i;
    const char *tmo_str;
    char tmo_buf[256];

#if defined(_WIN32) /*[*/
# define READ_READY(i, ip)      ((waitgroups[i / maximum_wait_objects()].ret == WAIT_OBJECT_0 + 1 + (i % maximum_wait_objects())) \
				 && is_ready(ip, WantRead))
# define WRITE_READY(i, ip)     ((waitgroups[i / maximum_wait_objects()].ret == WAIT_OBJECT_0 + 1 + (i % maximum_wait_objects())) \
				 && is_ready(ip, WantWrite))
# define EXCEPT_READY(i, ip)    false
# define WAIT_BAD        	(ret == WAIT_FAILED)
#else /*][*/
# if defined(HAVE_POLL) /*[*/
#  define READ_READY(i, ip)     (find_pollfd(fds, nfds, ip->source, false)->revents & (POLLIN | POLLHUP))
#  define WRITE_READY(i, ip)    (find_pollfd(fds, nfds, ip->source, false)->revents & (POLLOUT | POLLERR))
#  define EXCEPT_READY(i, ip)   (find_pollfd(fds, nfds, ip->source, false)->revents & POLLPRI)
# else /*][*/
#  define READ_READY(i, ip)     FD_ISSET(ip->source, &rfds)
#  define WRITE_READY(i,  ip)	FD_ISSET(ip->source, &wfds)
#  define EXCEPT_READY(i, ip)   FD_ISSET(ip->source, &xfds)
# endif /*]*/
# define WAIT_BAD        	(ns < 0)
#endif /*]*/

#if defined(_WIN32) /*[*/
    if (!initted) {
	initted = true;
	done_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (done_event == NULL) {
	    xs_error("sched: Cannot create done_event: %s", win32_strerror(GetLastError()));
	}
	event_semaphore = CreateSemaphore(NULL, 0, 99999, NULL);
	if (event_semaphore == NULL) {
	    xs_error("sched: Cannot create event_semaphore: %s", win32_strerror(GetLastError()));
	}
	allocate_wait_group();
    }

    clear_wst_events();
#endif /*]*/

    *processed_any = false;
    any_events_pending = false;

    /* Prepare the data structures for the wait. */
#if defined(_WIN32) /*[*/
    /* Account for the 'done' event that everyone waits for. */
    for (i = 0; i < num_wait_groups; i++) {
	waitgroups[i].nha = 1;
    }
#elif !defined(HAVE_POLL) /*][*/
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&xfds);
#endif /*]*/

    i = 0;
    for (ip = inputs; ip != NULL; ip = ip->next) {

	if (!ip->valid) {
	    /* We will clean these up at the bottom of this function. */
	    continue;
	}

	ip->sflags = 0;
	ip->wait_index = -1;

	/* Set pending input event. */
	if (ip->condition == WantRead) {
#if defined(_WIN32) /*[*/
	    if (set_wait_group(ip, ha_total, &ip->wait_index)) {
		++ha_total;
	    }
#else /*][*/
	    ip->wait_index = i;
# if defined(HAVE_POLL) /*[*/
	    nfds += add_poll(ip, POLLIN, nfds);
# else /*][*/
	    assert(ip->source <= FD_SETSIZE);
	    FD_SET(ip->source, &rfds);
# endif /*]*/
	    ne++;
#endif /*]*/
	    ip->sflags |= SF_CURRENT;
	    any_events_pending = true;
	}

	/* Set pending output event. */
	if (ip->condition == WantWrite) {
#if defined(_WIN32) /*[*/
	    if (set_wait_group(ip, ha_total, &ip->wait_index)) {
		++ha_total;
	    }
#else /*][*/
	    ip->wait_index = i;
# if defined(HAVE_POLL) /*[*/
	    nfds += add_poll(ip, POLLOUT, nfds);
# else /*][*/
	    assert(ip->source <= FD_SETSIZE);
	    FD_SET(ip->source, &wfds);
# endif /*]*/
	    ne++;
#endif /*]*/
	    ip->sflags |= SF_CURRENT;
	    any_events_pending = true;
	}

#if !defined(_WIN32) /*[*/
	/* Set pending exception event. */
	if (ip->condition == WantExcept) {
	    ip->wait_index = i;
#if defined(HAVE_POLL) /*[*/
	    nfds += add_poll(ip, POLLPRI, nfds);
#else /*][*/
	    assert(ip->source <= FD_SETSIZE);
	    FD_SET(ip->source, &xfds);
#endif /*]*/
	    ne++;
	    ip->sflags |= SF_CURRENT;
	    any_events_pending = true;
	}
#endif /*]*/
	i++;
    }

    /* Compute the next timeout. */
    any_events_pending |= compute_timeout(&tmo, block);

#if !defined(_WIN32) /*[*/
    /* Poll for exited children. */
    if (poll_children()) {
	return false;
    }
#endif /*]*/

    /* If there's nothing to do now, we're done. */
    if (!any_events_pending) {
	return true;
    }

    /* Trace what we're about to do. */
    vctrace(TC_SCHED, "Waiting for ");
#if defined(_WIN32) /*[*/
    vtrace("%d handle%s", (int)ha_total, (ha_total == 1)? "": "s");
    {
	int i;

	for (i = 1; i < waitgroups[0].nha; i++) {
	    vcdtrace(TC_SCHED, " 0x%lx", (unsigned long)(size_t)waitgroups[0].ha[i]);
	}

    }
#else /*][*/
    vtrace("%d event%s", ne, (ne == 1)? "": "s");
#endif /*]*/
    tmo_str = trace_tmo(tmo, tmo_buf, sizeof(tmo_buf));
    vtrace("%s%s\n", tmo_str? " or ": "", tmo_str? tmo_str: "");
#if defined(HAVE_POLL) /*[*/
    if (appres.ut_env && getenv("ORDER") != NULL) {
	nfds_t n;

	vctrace(TC_SCHED, "Order:");
	for (n = 0; n < nfds; n++) {
	    vtrace(" %d", fds[n].fd);
	}
	vtrace("\n");
    }
#endif /*]*/

    /* Wait for events. */
#if defined(_WIN32) /*[*/
    wait_threads_go();
    ret = WaitForMultipleObjects(waitgroups[0].nha, waitgroups[0].ha, FALSE, tmo);
    waitgroups[0].ret = ret;
#else /*][*/
# if defined(HAVE_POLL) /*[*/
    ns = poll(fds, nfds, tmo);
# else /*][*/
    ns = select(FD_SETSIZE, &rfds, &wfds, &xfds, tmo);
# endif /*]*/
#endif /*]*/

    /* Handle failures. */
    if (WAIT_BAD) {
#if !defined(_WIN32) /*[*/
	if (errno != EINTR) {
	    xs_error("sched: select() failed: %s", strerror(errno));
	}
#else /*][*/
	xs_error("sched: WaitForMultipleObjects(%d) failed: %s", waitgroups[0].nha, win32_strerror(GetLastError()));
#endif /*]*/
	return true;
    }

    /* Trace what we got. */
#if defined(_WIN32) /*[*/
    if (ret != WAIT_OBJECT_0) {
	vctrace(TC_SCHED, "Got event 0x%lx\n", ret);
    }
#elif defined(HAVE_POLL) /*[*/
    events = count_revents(fds, nfds);
    vctrace(TC_SCHED, "Got %u fd%s, %d event%s\n",
	    ns, (ns == 1)? "": "s",
	    events, (events == 1)? "": "s");
#else /*][*/
    vctrace(TC_SCHED, "Got %u event%s\n", ns, (ns == 1)? "": "s");
#endif /*]*/

#if defined(_WIN32) /*[*/
    /* Get the event threads ready for the next iteration. */
    sync_wait_threads(ret);
#endif /*]*/

    /* Process the events that completed. */
    inputs_changed = false;
    for (ip = inputs; ip != NULL; ip = ip->next) {
	if (ip->valid) {
	    if (!(ip->sflags & SF_CURRENT)) {
		/* From here on are entries that were added by callbacks. */
		vcdtrace(TC_SCHED, "0x%lx is not current\n", (unsigned long)(size_t)ip->source);
		break;
	    }

	    /* Check for completion. */
	    i = ip->wait_index;
	    vcdtrace(TC_SCHED, "Checking i=%d 0x%lx %s\n",
		    i, (unsigned long)(size_t)ip->source, (ip->condition == WantRead)? "Read": "Write");
	    if ((ip->condition == WantRead   && READ_READY(i, ip)) ||
		(ip->condition == WantWrite  && WRITE_READY(i, ip)) ||
		(ip->condition == WantExcept && EXCEPT_READY(i, ip))) {
		vcdtrace(TC_SCHED, "Running 0x%lx\n", (unsigned long)(size_t)ip->source);
		(*ip->proc)(ip->source, (ioid_t)ip);
#if defined(WIN32_HEAP_CHECK) /*[*/
		assert(_heapchk() == _HEAPOK);
#endif /*]*/
		ip->sflags |= SF_RAN;
		*processed_any = true;
	    }
	}
    }

    /* See what's expired. */
    *processed_any |= process_timeouts();

    /* Purge the deleted inputs, and move anything that ran to the back. */
    purge_inputs();

    /* If inputs have changed, retry. */
    return !inputs_changed;
}
#if defined(__clang__) /*[*/
# pragma clang diagnostic pop
#endif /*]*/

#if defined(_WIN32) /*[*/
/* Get the most recent network events for a socket. */
void
sched_get_network_events(socket_t socket, WSANETWORKEVENTS *events)
{
    wst_t *w;

    for (w = wst; w != NULL; w = w->next) {
	if (w->socket == socket) {
	    memcpy(events, &w->events, sizeof(WSANETWORKEVENTS));
	    return;
	}
    }

    vctrace(TC_SCHED, "sched_get_network_events can't find socket S0x%lx\n", (unsigned long)(size_t)socket);
    memset(events, 0, sizeof(WSANETWORKEVENTS));
}
#endif /*]*/

/*
 * Event dispatcher.
 * Processes all pending I/O and timeout events.
 * Waits for the first event if block is true.
 * Returns true if events were proccessed, false otherwise.
 */
bool
process_events(bool block)
{
    bool processed_any = false;
    bool any_this_time = false;
    bool done = false;

    /* Process events until no more are ready. */
    while (!done) {
	if (run_tasks()) {
	    return true;
	}

	/*
	 * Run deferred events.
	 * We always do this *after* running tasks, so the tasks will see
	 * the state changed by the deferred events.
	 * If any deferred events were run, run tasks again before blocking.
	 */
	bool any_deferred_ran = run_deferred();
	processed_any |= any_deferred_ran;
	if (any_deferred_ran && run_tasks()) {
	    return true;
	}

	/* Process some events. */
	done = process_some_events(block && !any_deferred(), &any_this_time);
	processed_any |= any_this_time;

	/* Free transaction memory. */
	txflush();

	/* Don't block a second time. */
	block = false;
    }

    return processed_any;
}
