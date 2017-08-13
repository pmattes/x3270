/*
 * Copyright (c) 1993-2017 Paul Mattes.
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

#include "actions.h"
#include "host.h"
#include "host_gui.h"
#include "macros.h"
#include "popups.h"
#include "product.h"
#include "split_host.h"
#include "telnet.h"
#include "telnet_core.h"
#include "trace.h"
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

struct host *hosts = NULL;
static struct host *last_host = NULL;
static bool auto_reconnect_inprogress = false;
static iosrc_t net_sock = INVALID_IOSRC;
static ioid_t reconnect_id = NULL_IOID;

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
	hostfile_name = xs_buffer("%s/ibm_hosts", appres.conf_dir);
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
	    if (!split_hier(NewString(name), &h->name, &h->parents)) {
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
	(void) fclose(hf);
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
 * Hosts module registration.
 */
void
host_register(void)
{
    static action_table_t host_actions[] = {
	{ "Close",	Disconnect_action,	ACTION_KE },
	{ "Connect",	Connect_action,		ACTION_KE },
	{ "Disconnect",	Disconnect_action,	ACTION_KE },
	{ "Open",	Connect_action,		ACTION_KE },
	{ "Reconnect",	Reconnect_action,	ACTION_KE }
    };

    /* Register for events. */
    register_schange(ST_EXITING, host_exiting);

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
	if (!strcmp(name, h->name)) {
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

    /* Call the sane, new version. */
    if (!new_split_host(s, &lu, &host, port, accept, flags, &error)) {
	popup_an_error("%s", error);
	Free(error);
	return NULL;
    }

    if (lu) {
	strncpy(xluname, lu, LUNAME_SIZE);
	xluname[LUNAME_SIZE] = '\0';
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
host_connect(const char *n)
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

    if (CONNECTED || auto_reconnect_inprogress) {
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
    Replace(reconnect_host, NewString(nb));

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

	/* Look up the name in the hosts file. */
	if (!needed && hostfile_lookup(s, &target_name, &ps)) {
	    /*
	     * Rescan for qualifiers.
	     * Qualifiers, LU names, ports and accept names  are all
	     * overridden by the hosts file.
	     */
	    Free(s);
	    if (!(s = split_host(target_name, &host_flags, luname, &port,
			    &accept, &needed))) {
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
    if (n != full_current_host) {
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
    Replace(qualified_host, xs_buffer("%s%s%s%s:%s%s%s",
		HOST_FLAG(SSL_HOST)? "L:": "",
		has_colons? "[": "",
		chost,
		has_colons? "]": "",
		port,
		(accept != NULL)? "=": "",
		(accept != NULL)? accept: ""));

    /* Attempt contact. */
    ever_3270 = false;
    nc = net_connect(chost, port, accept, localprocess_cmd != NULL, &net_sock);
    if (nc == NC_FAILED) {
	if (!host_gui_connect()) {
	    if (appres.interactive.reconnect) {
		auto_reconnect_inprogress = true;
		reconnect_id = AddTimeOut(RECONNECT_ERR_MS, try_reconnect);
	    }
	}
	/* Redundantly signal a disconnect. */
	st_changed(ST_CONNECT, false);
	goto failure;
    }

    /* Still thinking about it? */
    if (nc == NC_RESOLVING) {
	cstate = RESOLVING;
	st_changed(ST_RESOLVING, true);
	goto success;
    }
    if (nc == NC_SSL_PASS) {
	cstate = SSL_PASS;
	goto success;
    }

    /* Success. */

    /* Set pending string. */
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
	cstate = PENDING;
	st_changed(ST_HALF_CONNECT, true);
    } else {
	/* cstate == NC_CONNECTED */
	if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	    cstate = CONNECTED_NVT;
	} else {
	    cstate = CONNECTED_INITIAL;
	}
	st_changed(ST_CONNECT, true);
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

/* Process a new connection, when it happens after SSL validation. */
void
host_new_connection(bool pending)
{
    /* Set state and tell the world. */
    if (pending) {
	cstate = PENDING;
	st_changed(ST_HALF_CONNECT, true);
    } else {
	if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	    cstate = CONNECTED_NVT;
	} else {
	    cstate = CONNECTED_INITIAL;
	}
	st_changed(ST_CONNECT, true);
	host_gui_connect_initial();
    }
}

/*
 * Reconnect to the last host.
 */
static void
host_reconnect(void)
{
    if (auto_reconnect_inprogress || current_host == NULL || CONNECTED ||
	    HALF_CONNECTED) {
	return;
    }
    if (host_connect(reconnect_host)) {
	auto_reconnect_inprogress = false;
    }
}

/*
 * Called from timer to attempt an automatic reconnection.
 */
static void
try_reconnect(ioid_t id _is_unused)
{
    auto_reconnect_inprogress = false;
    host_reconnect();
}

/*
 * Cancel any pending reconnect attempt.
 */
void
host_cancel_reconnect(void)
{
    if (auto_reconnect_inprogress) {
	RemoveTimeOut(reconnect_id);
	auto_reconnect_inprogress = false;
    }
}

void
host_disconnect(bool failed)
{
    if (!PCONNECTED) {
	return;
    }

    x_remove_input();
    net_disconnect(true);
    net_sock = INVALID_IOSRC;
    if (!host_gui_disconnect()) {
	if (appres.interactive.reconnect && !auto_reconnect_inprogress) {
	    /* Schedule an automatic reconnection. */
	    auto_reconnect_inprogress = true;
	    reconnect_id = AddTimeOut(failed? RECONNECT_ERR_MS:
					      RECONNECT_MS,
		  try_reconnect);
	}
    }

    /*
     * Remember a disconnect from NVT mode, to keep screen tracing
     * in sync.
     */
    if (IN_NVT && toggled(SCREEN_TRACE)) {
	trace_nvt_disc();
    }

    cstate = NOT_CONNECTED;

    /* Propagate the news to everyone else. */
    st_changed(ST_CONNECT, false);
}

/* The host has entered 3270 or NVT mode, or switched between them. */
void
host_in3270(enum cstate new_cstate)
{
    bool now3270 = (new_cstate == CONNECTED_3270 ||
		    new_cstate == CONNECTED_SSCP ||
		    new_cstate == CONNECTED_TN3270E);
    bool was3270 = (cstate == CONNECTED_3270 ||
	    	    cstate == CONNECTED_SSCP ||
		    cstate == CONNECTED_TN3270E);
    bool now_nvt = (new_cstate == CONNECTED_NVT ||
		    new_cstate == CONNECTED_E_NVT);
    bool was_nvt = (cstate == CONNECTED_NVT ||
		    cstate == CONNECTED_E_NVT);

    cstate = new_cstate;
    ever_3270 = now3270;
    if (now3270 != was3270 || now_nvt != was_nvt) {
	st_changed(ST_3270_MODE, now3270);
    }
}

void
host_connected(void)
{
    cstate = CONNECTED_INITIAL;
    st_changed(ST_CONNECT, true);
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
		    (void) fprintf(lcf, "%lu %s\n",
			    (unsigned long)h->connect_time, h->name);
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
    action_debug("Connect", ia, argc, argv);
    if (check_argc("Connect", argc, 1, 1) < 0) {
	return false;
    }
    if (PCONNECTED) {
	popup_an_error("Already connected");
	return false;
    }
    (void) host_connect(argv[0]);

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
    if (ia != IA_KEYMAP) {
	sms_connect_wait();
    }
    return true;
}

static bool
Reconnect_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Reconnect", ia, argc, argv);
    if (check_argc("Reconnect", argc, 0, 0) < 0) {
	return false;
    }
    if (PCONNECTED) {
	popup_an_error("Already connected");
	return false;
    }
    if (current_host == NULL) {
	popup_an_error("No previous host to connect to");
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
    if (ia != IA_KEYMAP) {
	sms_connect_wait();
    }
    return true;
}

static bool
Disconnect_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Disconnect", ia, argc, argv);
    if (check_argc("Disconnect", argc, 0, 0) < 0) {
	return false;
    }
    host_disconnect(false);
    return true;
}
