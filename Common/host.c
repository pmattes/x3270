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
 *	host.c
 *		This module handles the ibm_hosts file, connecting to and
 *		disconnecting from hosts, and state changes on the host
 *		connection.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include <assert.h>
#include <limits.h>
#include "actions.h"
#include "boolstr.h"
#include "glue_gui.h"
#include "host.h"
#include "host_gui.h"
#include "login_macro.h"
#include "names.h"
#include "popups.h"
#include "product.h"
#include "split_host.h"
#include "task.h"
#include "telnet.h"
#include "telnet_core.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
#include "uri.h"
#include "screentrace.h"
#include "utils.h"
#include "xio.h"

#include <errno.h>

#define RECONNECT_MS		2000	/* 2 sec before reconnecting to host */
#define RECONNECT_ERR_MS	5000	/* 5 sec before reconnecting to host */

#define MAX_RECENT		20	/* upper limit on appres.max_recent */

enum cstate	cstate = NOT_CONNECTED;
unsigned	host_flags = 0;
#define		LUNAME_SIZE	1024
char		luname[LUNAME_SIZE+1];
char		*connected_lu = NULL;
char		*connected_type = NULL;
bool		ever_3270 = false;

char           *current_host = NULL;
char           *full_current_host = NULL;
unsigned short  current_port;
char	       *reconnect_host = NULL;
char	       *qualified_host = NULL;
enum iaction	connect_ia = IA_NONE;
bool		host_retry_mode = false;
char 	       *host_user = NULL;

struct host *hosts = NULL;
static struct host *last_host = NULL;
static iosrc_t net_sock = INVALID_IOSRC;
static ioid_t reconnect_id = NULL_IOID;

static char *host_ps = NULL;

static void save_recent(const char *);

static void try_reconnect(ioid_t id);

static action_t Connect_action;
static action_t Disconnect_action;
static action_t Reconnect_action;

static char *
stoken(char **s)
{
    char *r;
    char *ss = *s;

    if (!*ss) {
	return NULL;
    }
    r = ss;
    while (*ss && *ss != ' ' && *ss != '\t') {
	ss++;
    }
    if (*ss) {
	*ss++ = '\0';
	while (*ss == ' ' || *ss == '\t') {
	    ss++;
	}
    }
    *s = ss;
    return r;
}

/*
 * Read the hosts file.
 */
static void
read_hosts_file(void)
{
    FILE *hf;
    char buf[1024];
    struct host *h;
    char *hostfile_name;

    /* This only applies to emulators with displays. */
    if (!product_has_display()) {
	return;
    }

    hostfile_name = appres.hostsfile;
    if (hostfile_name == NULL) {
	hostfile_name = Asprintf("%s/ibm_hosts", appres.conf_dir);
    } else {
	hostfile_name = do_subst(appres.hostsfile, DS_VARS | DS_TILDE);
    }
    hf = fopen(hostfile_name, "r");
    if (hf != NULL) {
	while (fgets(buf, sizeof(buf), hf)) {
	    char *s = buf;
	    char *name, *entry_type, *hostname;
	    char *slash;

	    if (strlen(buf) > (unsigned)1 && buf[strlen(buf) - 1] == '\n') {
		buf[strlen(buf) - 1] = '\0';
	    }
	    while (isspace((unsigned char)*s)) {
		s++;
	    }
	    if (!*s || *s == '#') {
		continue;
	    }
	    name = stoken(&s);
	    entry_type = stoken(&s);
	    hostname = stoken(&s);
	    if (!name || !entry_type || !hostname) {
		popup_an_error("Bad %s syntax, entry skipped", ResHostsFile);
		continue;
	    }
	    h = (struct host *)Malloc(sizeof(*h));
	    if (!split_hier(name, &h->name, &h->parents)) {
		Free(h);
		continue;
	    }
	    h->hostname = NewString(hostname);

	    /*
	     * Quick syntax extension to allow the hosts file to
	     * specify a port as host/port.
	     */
	    if ((slash = strchr(h->hostname, '/'))) {
		*slash = ':';
	    }

	    if (!strcmp(entry_type, "primary")) {
		h->entry_type = PRIMARY;
	    } else {
		h->entry_type = ALIAS;
	    }
	    if (*s) {
		h->loginstring = NewString(s);
	    } else {
		h->loginstring = NULL;
	    }
	    h->prev = last_host;
	    h->next = NULL;
	    if (last_host) {
		last_host->next = h;
	    } else {
		hosts = h;
	    }
	    last_host = h;
	}
	fclose(hf);
    } else if (appres.hostsfile != NULL) {
	popup_an_errno(errno, "Cannot open " ResHostsFile " '%s'",
		appres.hostsfile);
    }
    Free(hostfile_name);

    /*
     * Read the recent-connection file, and prepend it to the hosts list.
     */
    save_recent(NULL);
}

/**
 * State change callback for emulator exit.
 *
 * @param[in] mode	Unused.
 */
static void
host_exiting(bool mode _is_unused)
{
    /* Disconnect from the host gracefully. */
    host_disconnect(false);
}

/**
 * Set a host flag.
 * 
 * @param[in] flag	Flag to set.
 */
void
host_set_flag(int flag)
{
    host_flags |= 1 << flag;
}

/*
 * Cancel any pending reconnect attempt.
 */
static void
host_cancel_reconnect(void)
{
    if (reconnect_id != NULL_IOID) {
	RemoveTimeOut(reconnect_id);
	reconnect_id = NULL_IOID;

	assert(cstate == RECONNECTING);
	change_cstate(NOT_CONNECTED, "host_cancel_reconnect");
    }
}

/**
 * Common logic for when the reconnect or retry options change.
 */
static void
reconnect_retry_touched(void)
{
    /*
     * Turning off reconnect/retry is the way to stop a reconnect in progress.
     */
    if (!appres.reconnect && !appres.retry) {
	host_cancel_reconnect();
    }

    /* They have changed one of the flags. Reset host_retry_mode. */
    host_retry_mode = appres.reconnect || appres.retry;
}

/**
 * Canonicalize the configuration directory.
 *
 * @param[in] dir	Configuration directory.
 *
 * @return Canonicalized form
 */
static const char *
canon_conf_dir(const char *dir)
{
#if !defined(_WIN32) /*[*/
    static char resolved_path[PATH_MAX];
    char *result = realpath(dir, resolved_path);

    return result? result: dir;
#else /*][*/
    static char resolved_path[MAX_PATH];
    DWORD len = GetFullPathName(dir, MAX_PATH, resolved_path, NULL);

    return len? resolved_path: dir;
#endif /*]*/
}

/**
 * Toggle the configuration directory.
 *
 * @param[in] name	Toggle name.
 * @param[in] value	New value.
 * @param[in] flags	Set() flags.
 * @param[in] ia	Cause.
 *
 * @return toggle_upcall_ret_t
 */
static toggle_upcall_ret_t
set_conf_dir(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    popup_an_error("Cannot set " ResConfDir);
    return TU_FAILURE;
}

/**
 * Toggle the reconnect flag.
 *
 * @param[in] name	Toggle name.
 * @param[in] value	New value.
 * @param[in] flags	Set() flags.
 * @param[in] ia	Cause.
 *
 * @return toggle_upcall_ret_t
 */
static toggle_upcall_ret_t
set_reconnect(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    bool previous = appres.reconnect;
    const char *errmsg;

    if ((errmsg = boolstr(value, &appres.reconnect)) != NULL) {
	popup_an_error("%s", errmsg);
	return TU_FAILURE;
    }

    if (appres.reconnect != previous) {
	reconnect_retry_touched();
    }
    return TU_SUCCESS;
}

/**
 * Toggle the retry flag.
 *
 * @param[in] name	Toggle name.
 * @param[in] value	New value.
 * @param[in] flags	Set() flags.
 * @param[in] ia	Cause.
 *
 * @return toggle_upcall_ret_t
 */
static toggle_upcall_ret_t
set_retry(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    bool previous = appres.retry;
    const char *errmsg;

    if ((errmsg = boolstr(value, &appres.retry)) != NULL) {
	popup_an_error("%s", errmsg);
	return TU_FAILURE;
    }

    if (appres.reconnect != previous) {
	reconnect_retry_touched();
    }
    return TU_SUCCESS;
}

/**
 * Hosts module registration.
 */
void
host_register(void)
{
    static action_table_t host_actions[] = {
	{ AnClose,	Disconnect_action,	ACTION_KE },
	{ AnConnect,	Connect_action,		ACTION_KE },
	{ AnDisconnect,	Disconnect_action,	ACTION_KE },
	{ AnOpen,	Connect_action,		ACTION_KE },
	{ AnReconnect,	Reconnect_action,	ACTION_KE },
    };

    /* Register for events. */
    register_schange(ST_EXITING, host_exiting);

    /* Register our toggles. */
    register_extended_toggle(ResConfDir, set_conf_dir, NULL, canon_conf_dir,
	    (void **)&appres.conf_dir, XRM_STRING);
    register_extended_toggle(ResReconnect, set_reconnect, NULL, NULL,
	    (void **)&appres.reconnect, XRM_BOOLEAN);
    register_extended_toggle(ResRetry, set_retry, NULL, NULL,
	    (void **)&appres.retry, XRM_BOOLEAN);

    /* Register our actions. */
    register_actions(host_actions, array_count(host_actions));
}

/**
 * Read in the hosts file.
 */
void
hostfile_init(void)
{
    static bool hostfile_initted = false;

    if (hostfile_initted) {
	return;
    }

    read_hosts_file();

    host_retry_mode = appres.reconnect || appres.retry;

    hostfile_initted = true;
}

/*
 * Look up a host in the list.  Turns aliases into real hostnames, and
 * finds loginstrings.
 */
static int
hostfile_lookup(const char *name, char **hostname, char **loginstring)
{
    struct host *h;

    hostfile_init();
    for (h = hosts; h != NULL; h = h->next) {
	if (h->entry_type == RECENT) {
	    continue;
	}
	if (!strcasecmp(name, h->name)) {
	    *hostname = h->hostname;
	    if (h->loginstring != NULL) {
		*loginstring = h->loginstring;
	    } else {
		*loginstring = appres.login_macro;
	    }
	    return 1;
	}
    }
    return 0;
}

#if defined(LOCAL_PROCESS) /*[*/
/* Recognize and translate "-e" options. */
static const char *
parse_localprocess(const char *s)
{
    int sl = strlen(OptLocalProcess);

    if (!strncmp(s, OptLocalProcess, sl)) {
	if (s[sl] == ' ') {
	    return(s + sl + 1);
	} else if (s[sl] == '\0') {
	    char *r;

	    r = getenv("SHELL");
	    if (r != NULL) {
		return r;
	    } else {
		return "/bin/sh";
	    }
	}
    }
    return NULL;
}
#endif /*]*/

/*
 * Strip qualifiers from a hostname.
 * Returns the hostname part in a newly-malloc'd string.
 * 'needed' is returned true if anything was actually stripped.
 * Returns NULL if there is a syntax error.
 */
static char *
split_host(char *s, unsigned *flags, char *xluname, char **port, char **accept,
	bool *needed)
{
    char *lu;
    char *host;
    char *error;

    *flags = 0;
    *needed = false;

    if (is_x3270_uri(s)) {
	char *password;
	const char *err;

	if (!parse_x3270_uri(s, &host, port, flags, &host_user, &password, &lu, accept, &err)) {
	    popup_an_error("URI error in '%s': %s", s, err);
	    return NULL;
	}
	goto done;
    }

    /* Call the sane, new version. */
    if (!new_split_host(s, &lu, &host, port, accept, flags, &error)) {
	popup_an_error("%s", error);
	Free(error);
	return NULL;
    }

done:
    if (lu != NULL) {
	strncpy(xluname, lu, LUNAME_SIZE);
	xluname[LUNAME_SIZE] = '\0';
	Free(lu);
    } else {
	*xluname = '\0';
    }
    *needed = (strcmp(s, host) != 0);
    return host;
}


/*
 * Network connect/disconnect operations, combined with X input operations.
 *
 * Returns true for success, false for error.
 * Sets 'reconnect_host', 'current_host' and 'full_current_host' as
 * side-effects.
 */
bool
host_connect(const char *n, enum iaction ia)
{
    char *nb;		/* name buffer */
    char *s;		/* temporary */
    const char *chost;	/* to whom we will connect */
    char *target_name;
    char *ps = NULL;
    char *port = NULL;
    char *accept = NULL;
    const char *localprocess_cmd = NULL;
    bool has_colons = false;
    net_connect_t nc;

    if (!glue_gui_open_safe()) {
	popup_an_error("User interface in the wrong state");
	return false;
    }

    if (cstate == RECONNECTING) {
	popup_an_error("Reconnect in progress");
	return false;
    }
    if (PCONNECTED) {
	popup_an_error("Already connected");
	return true;
    }

    /* Skip leading blanks. */
    while (*n == ' ') {
	n++;
    }
    if (!*n) {
	popup_an_error("Invalid (empty) hostname");
	return false;
    }

    /* Save in a modifiable buffer. */
    nb = NewString(n);

    /* Strip trailing blanks. */
    s = nb + strlen(nb) - 1;
    while (*s == ' ') {
	*s-- = '\0';
    }

    /* Remember this hostname, as the last hostname we connected to. */
    if (reconnect_host == NULL || strcmp(reconnect_host, nb)) {
	Replace(reconnect_host, NewString(nb));
    }

    /* Remember this hostname in the recent connection list and file. */
    save_recent(nb);

#if defined(LOCAL_PROCESS) /*[*/
    if ((localprocess_cmd = parse_localprocess(nb)) != NULL) {
	chost = localprocess_cmd;
	port = appres.port;
    } else
#endif /*]*/
    {
	bool needed;

	/* Strip off and remember leading qualifiers. */
	if ((s = split_host(nb, &host_flags, luname, &port, &accept,
			&needed)) == NULL) {
	    goto failure;
	}

	/* If the hostname is naked, look it up in the hosts file. */
	if (!needed && hostfile_lookup(s, &target_name, &ps)) {
	    /*
	     * Split out all of the other decorations from the entry in the
	     * hosts file.
	     */
	    Free(s);
	    s = split_host(target_name, &host_flags, luname, &port, &accept,
		    &needed);
	    if (s == NULL) {
		goto failure;
	    }
	}
	chost = s;

	/* Default the port. */
	if (port == NULL) {
	    port = appres.port;
	}
    }

    /*
     * Store the original name in globals, even if we fail the connect later:
     *  current_host is the hostname part, stripped of qualifiers, luname
     *   and port number
     *  full_current_host is the entire string, for use in reconnecting
     */
    if (full_current_host == NULL || strcmp(full_current_host, n)) {
	Replace(full_current_host, NewString(n));
    }
    Replace(current_host, NULL);
    if (localprocess_cmd != NULL) {
	if (full_current_host[strlen(OptLocalProcess)] != '\0') {
	    current_host = NewString(full_current_host +
		    strlen(OptLocalProcess) + 1);
	} else {
	    current_host = NewString("default shell");
	}
    } else {
	current_host = s;
    }

    has_colons = (strchr(chost, ':') != NULL);
    Replace(qualified_host, Asprintf("%s%s%s%s%s:%s%s%s",
		HOST_FLAG(TLS_HOST)? "L:": "",
		HOST_FLAG(NO_VERIFY_CERT_HOST)? "Y:": "",
		has_colons? "[": "",
		chost,
		has_colons? "]": "",
		port,
		(accept != NULL)? "=": "",
		(accept != NULL)? accept: ""));

    /* Attempt contact. */
    host_retry_mode = appres.reconnect || appres.retry;
    ever_3270 = false;
    nc = net_connect(chost, port, accept, localprocess_cmd != NULL, &net_sock);
    if (port != appres.port) {
	Replace(port, NULL);
    }
    Replace(accept, NULL);
    if (nc == NC_FAILED) {
	if (!host_gui_connect()) {
	    if (host_retry_mode) {
		reconnect_id = AddTimeOut(RECONNECT_ERR_MS, try_reconnect);
		change_cstate(RECONNECTING, "host_connect");
	    }
	}
	/* Redundantly signal a disconnect. */
	change_cstate(NOT_CONNECTED, "host_connect");
	if (reconnect_id != NULL_IOID) {
	    /* Change it back so we are in the right state when we retry. */
	    change_cstate(RECONNECTING, "host_connect");
	}
	goto failure;
    }

    /* Save the pending string. */
    if (ps == NULL) {
	ps = appres.login_macro;
    }
    host_ps = NewString(ps);

    /* Still thinking about it? */
    connect_ia = ia;
    if (nc == NC_RESOLVING) {
	change_cstate(RESOLVING, "host_connect");
	goto success;
    }
    if (nc == NC_TLS_PASS) {
	change_cstate(TLS_PASS, "host_connect");
	goto success;
    }

    /* Success. */

    /* Set pending string. */
    Replace(host_ps, NULL);
    if (ps == NULL) {
	ps = appres.login_macro;
    }
    if (ps != NULL) {
	login_macro(ps);
    }

    /* Prepare Xt for I/O. */
    if (net_sock != INVALID_IOSRC) {
	x_add_input(net_sock);
    }

    /* Set state and tell the world. */
    if (nc == NC_CONNECT_PENDING) {
	change_cstate(TCP_PENDING, "host_connect");
    } else if (cstate != TLS_PENDING) {
	/* nc == NC_CONNECTED and TLS not pending */
	if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	    change_cstate(CONNECTED_NVT, "host_connect");
	} else {
	    change_cstate(TELNET_PENDING, "host_connect");
	}
	host_gui_connect_initial();
    }

success:
    if (nb != NULL) {
	Free(nb);
    }
    return true;

failure:
    if (nb != NULL) {
	Free(nb);
    }
    return false;
}

/* Process a new connection, when it happens after TLS validation. */
void
host_new_connection(bool pending)
{
    /* Set state and tell the world. */
    if (pending) {
	change_cstate(TCP_PENDING, "host_new_connection");
    } else {
	if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	    change_cstate(CONNECTED_NVT, "host_new_connection");
	} else {
	    change_cstate(TELNET_PENDING, "host_new_connection");
	}
	host_gui_connect_initial();
    }
}

/* Continue a connection after hostname resolution completes. */
void
host_continue_connect(iosrc_t iosrc, net_connect_t nc)
{
    char *ps = host_ps;

    /* Set pending string. */
    if (ps == NULL) {
	ps = appres.login_macro;
    }
    if (ps != NULL) {
	login_macro(ps);
    }

    /* Prepare Xt for I/O. */
    net_sock = iosrc;
    if (net_sock != INVALID_IOSRC) {
	x_add_input(net_sock);
    }

    /* Set state and tell the world. */
    if (nc == NC_CONNECT_PENDING) {
	change_cstate(TCP_PENDING, "host_continue_connect");
    } else {
	/* cstate == NC_CONNECTED */
	if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	    change_cstate(CONNECTED_NVT, "host_continue_connect");
	} else {
	    change_cstate(TELNET_PENDING, "host_continue_connect");
	}
	host_gui_connect_initial();
    }
}

/*
 * Reconnect to the last host.
 * Returns true if connection initiated, false otherwise.
 */
static bool
host_reconnect(void)
{
    if (current_host == NULL) {
	return false;
    }
    return host_connect(reconnect_host, connect_ia);
}

/*
 * Called from timer to attempt an automatic reconnection.
 */
static void
try_reconnect(ioid_t id _is_unused)
{
    reconnect_id = NULL_IOID;
    assert(cstate == RECONNECTING);
    change_cstate(NOT_CONNECTED, "try_reconnect");

    host_reconnect();
}

void
host_disconnect(bool failed)
{
    if (cstate <= RECONNECTING) {
	return;
    }

    x_remove_input();
    net_disconnect(true);
    net_sock = INVALID_IOSRC;
    if (!host_gui_disconnect()) {
	if (host_retry_mode && reconnect_id == NULL_IOID) {
	    /* Schedule an automatic reconnection. */
	    reconnect_id = AddTimeOut(failed? RECONNECT_ERR_MS:
					      RECONNECT_MS,
		  try_reconnect);
	    change_cstate(RECONNECTING, "host_disconnect");
	}
    }

    /*
     * Remember a disconnect from NVT mode, to keep screen tracing
     * in sync.
     */
    if (IN_NVT && toggled(SCREEN_TRACE)) {
	trace_nvt_disc();
    }

    if (cstate != RECONNECTING) {
	change_cstate(NOT_CONNECTED, "host_disconnect");

	/* Forget pending string. */
	Replace(host_ps, NULL);
	Replace(host_user, NULL);
    }

    /* No more host, no more host flags. */
    host_flags = 0;
    net_set_default_termtype();
}

/* The host has entered 3270 or NVT mode, or switched between them. */
void
host_in3270(enum cstate new_cstate)
{
    ever_3270 = cIN_3270(new_cstate);
    change_cstate(new_cstate, "host_in3270");
}

void
host_connected(void)
{
    change_cstate(TELNET_PENDING, "host_connected");
    host_retry_mode = appres.reconnect;
    host_gui_connected();
}

/* Swap out net_sock. */
void
host_newfd(iosrc_t s)
{
    /* Shut off the old. */
    x_remove_input();

    /* Turn on the new. */
    net_sock = s;
    x_add_input(net_sock);
}

/* Comparison function for the qsort. */
static int
host_compare(const void *e1, const void *e2)
{
    const struct host *h1 = *(const struct host **)e1;
    const struct host *h2 = *(const struct host **)e2;
    int r;

    if (h1->connect_time > h2->connect_time) {
	r = -1;
    } else if (h1->connect_time < h2->connect_time) {
	r = 1;
    } else {
	r = 0;
    }
#if defined(CFDEBUG) /*[*/
    printf("%s %ld %d %s %ld\n",
	    h1->name, h1->connect_time,
	    r,
	    h2->name, h2->connect_time);
#endif /*]*/
    return r;
}

#if defined(CFDEBUG) /*[*/
static void
dump_array(const char *when, struct host **array, int nh)
{
    int i;

    printf("%s\n", when);
    for (i = 0; i < nh; i++) {
	printf(" %15s %ld\n", array[i]->name, array[i]->connect_time);
    }
}
#endif /*]*/

/* Save the most recent host in the recent host list. */
static void
save_recent(const char *hn)
{
    struct host *h;
    int nih = 0;
    struct host *r_start = NULL;
    char *lcf_name = NULL;
    FILE *lcf = NULL;
    struct host **h_array = NULL;
    int nh = 0;
    int i;
    time_t t = time(NULL);
    int n_recent;

    /* Don't let the user go overboard on the recent hosts list. */
    if (appres.max_recent > MAX_RECENT) {
	appres.max_recent = MAX_RECENT;
    }

    /*
     * Copy the ibm_hosts into the array, and point r_start at the first
     * recent-host entry.
     */
    for (h = hosts; h != NULL; h = h->next) {
	if (h->entry_type == RECENT) {
	    r_start = h;
	    break;
	}

	h_array = (struct host **)
	    Realloc(h_array, (nh + 1) * sizeof(struct host *));
	h_array[nh++] = h;
	nih++;
    }

    /*
     * Allocate a new entry and add it to the array, just under the
     * ibm_hosts and before the first recent entry.
     */
    if (hn != NULL) {
	h = (struct host *)Malloc(sizeof(*h));
	h->name = NewString(hn);
	h->parents = NULL;
	h->hostname = NewString(hn);
	h->entry_type = RECENT;
	h->loginstring = NULL;
	h->connect_time = t;
	h_array = (struct host **)
	    Realloc(h_array, (nh + 1) * sizeof(struct host *));
	h_array[nh++] = h;
    }

    /* Append the existing recent entries to the array. */
    for (h = r_start; h != NULL; h = h->next) {
	h_array = (struct host **)
	    Realloc(h_array, (nh + 1) * sizeof(struct host *));
	h_array[nh++] = h;
    }

    /*
     * Read the last-connection file, to capture the any changes made by
     * other instances of x3270.  
     */
    if (appres.connectfile_name != NULL &&
	    strcasecmp(appres.connectfile_name, "none")) {
	lcf_name = do_subst(appres.connectfile_name, DS_VARS | DS_TILDE);
	lcf = fopen(lcf_name, "r");
    }
    if (lcf != NULL) {
	char buf[1024];

	while (fgets(buf, sizeof(buf), lcf) != NULL) {
	    size_t sl;
	    time_t connect_time;
	    char *ptr;

	    /* Pick apart the entry. */
	    sl = strlen(buf);
	    if (buf[sl - 1] == '\n') {
		buf[sl-- - 1] = '\0';
	    }
	    if (!sl || buf[0] == '#' ||
		    (connect_time = strtoul(buf, &ptr, 10)) == 0L ||
		    ptr == buf || *ptr != ' ' || !*(ptr + 1)) {
		continue;
	    }

	    h = (struct host *)Malloc(sizeof(*h));
	    h->name = NewString(ptr + 1);
	    h->parents = NULL;
	    h->hostname = NewString(ptr + 1);
	    h->entry_type = RECENT;
	    h->loginstring = NULL;
	    h->connect_time = connect_time;
	    h_array = (struct host **)
		Realloc(h_array, (nh + 1) * sizeof(struct host *));
	    h_array[nh++] = h;
	}
	fclose(lcf);
    }

    /*
     * Sort the recent hosts, in reverse order by connect time.
     */
#if defined(CFDEBUG) /*[*/
    dump_array("before", h_array, nh);
#endif /*]*/
    qsort(h_array + nih, nh - nih, sizeof(struct host *), host_compare);
#if defined(CFDEBUG) /*[*/
    dump_array("after", h_array, nh);
#endif /*]*/

    /*
     * Filter out duplicate names in the recent host list.
     * At the same time, limit the size of the recent list to MAX_RECENT.
     */
    n_recent = 0;
    for (i = nih; i < nh; i++) {
	bool delete = false;

	if (n_recent >= appres.max_recent) {
	    delete = true;
	} else {
	    int j;

	    for (j = nih; j < i; j++) {
		if (h_array[j] != NULL &&
			!strcmp(h_array[i]->name, h_array[j]->name)) {
		    delete = true;
		    break;
		}
	    }
	}
	if (delete) {
	    Free(h_array[i]->name);
	    Free(h_array[i]->hostname);
	    Free(h_array[i]);
	    h_array[i] = NULL;
	} else {
		n_recent++;
	}
    }

    /* Create a new host list from what's left. */
    hosts = NULL;
    last_host = NULL;
    for (i = 0; i < nh; i++) {
	if ((h = h_array[i]) != NULL) {
	    h->next = NULL;
	    if (last_host != NULL) {
		last_host->next = h;
	    }
	    h->prev = last_host;
	    last_host = h;
	    if (hosts == NULL) {
		hosts = h;
	    }
	}
    }

    /* No need for the array any more. */
    Free(h_array);
    h_array = NULL;

    /* Rewrite the file. */
    if (lcf_name != NULL) {
	lcf = fopen(lcf_name, "w");
	if (lcf != NULL) {
	    fprintf(lcf,
"# Automatically generated %s# by %s\n\
# Do not edit!\n",
		    ctime(&t), build);
	    for (h = hosts; h != NULL; h = h->next) {
		if (h->entry_type == RECENT) {
		    fprintf(lcf, "%lu %s\n", (unsigned long)h->connect_time,
			    h->name);
		}
	    }
	    fclose(lcf);
	}
    }
    if (lcf_name != NULL) {
	Free(lcf_name);
    }
}

/* Explicit connect/disconnect actions. */

static bool
Connect_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnConnect, ia, argc, argv);
    if (check_argc(AnConnect, argc, 1, 1) < 0) {
	return false;
    }

    if (!host_connect(argv[0], ia)) {
	return false;
    }

    /*
     * If not called from a keymap and the connection was successful (or
     * half-successful), pause the script until we are connected and
     * we have identified the host type.
     *
     * The reason for the check against keymaps is so the GUI doesn't stall
     * if someone puts a Connect() in a keymap. This is an imperfect check,
     * since someone could put a Source() in a keymap for a file that includes
     * a Connect(), and it would still stall here.
     */
    if (!task_nonblocking_connect() && !IA_IS_KEY(ia)) {
	task_connect_wait();
    }
    return true;
}

static bool
Reconnect_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnReconnect, ia, argc, argv);
    if (check_argc(AnReconnect, argc, 0, 0) < 0) {
	return false;
    }
    if (PCONNECTED) {
	popup_an_error(AnReconnect "(): Already connected");
	return false;
    }
    if (current_host == NULL) {
	popup_an_error(AnReconnect "(): No previous host to connect to");
	return false;
    }
    host_reconnect();

    /*
     * If called from a script and the connection was successful (or
     * half-successful), pause the script until we are connected and
     * we have identified the host type.
     *
     * The reason for the check against keymaps is so the GUI doesn't stall
     * if someone puts a Reconnect() in a keymap. This is an imperfect check,
     * since someone could put a Source() in a keymap for a file that includes
     * a Reconnect(), and it would still stall here.
     */
    if (!IA_IS_KEY(ia)) {
	task_connect_wait();
    }

    return cstate != NOT_CONNECTED;
}

static bool
Disconnect_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnDisconnect, ia, argc, argv);
    if (check_argc(AnDisconnect, argc, 0, 0) < 0) {
	return false;
    }
    host_disconnect(false);
    return true;
}

bool
host_reconnecting(void)
{
    return cstate == RECONNECTING;
}
