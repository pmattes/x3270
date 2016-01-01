/*
 * Copyright (c) 1993-2016 Paul Mattes.
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

static char *pfxstr = "AaCcLlNnPpSsBb";

/**
 * Hostname parser.
 *  [prefix:...][lu@]hostname[:port]
 * Backslashes to quote anything (including backslashes).
 * [ ] quotes : and @, e.g., [1:2::3] to quote an IPv6 numeric hostname.
 *
 * @param[in] raw	Raw hostname, with possible decorations
 * @param[out] lu	Returned Malloc'd LU name, or NULL
 * @param[out] host	Returned Malloc'd hostname, isolated from other parts
 * @param[out] port	Returned Malloc'd port, or NULL
 * @param[out] prefixes	Returned bitmap of prefixes, indexed by ACLNPSB (bit 0
 * 			 is A, bit 1 is C, bit 2 is L, etc.)
 * @param[out] error	Returned error text for failure, or NULL
 *
 * @return true for success, false for syntax error.
 */
static bool
new_split_host(char *raw, char **lu, char **host, char **port,
	unsigned *prefixes, char **error)
{
    char   *start     = raw;
    size_t  sl        = strlen(raw);
    char   *s;
    char   *uq        = NULL;
    int     uq_len    = 0;
    char   *qmap      = NULL;
    char   *rqmap;
    char   *errmsg    = "nonspecific";
    bool rc        = false;
    bool quoted    = false;
    int     bracketed = 0;
    int     n_ch      = 0;
    int     n_at      = 0;
    int     n_colon   = 0;
    char   *part[3]   = { NULL, NULL, NULL };
    int     part_ix   = 0;
    char   *pfx;

    *lu       = NULL;
    *host     = NULL;
    *port     = NULL;
    *prefixes = 0;
    *error    = NULL;

    /* Trim leading and trailing blanks. */
    while (sl && isspace((unsigned char)*start)) {
	start++;
	sl--;
    }
    while (sl && isspace((unsigned char)start[sl - 1])) {
	sl--;
    }
    if (!sl) {
	errmsg = "empty string";
	goto done;
    }

    /*
     * 'start' now points to the start of the string, and sl is its length.
     */

    /*
     * Create a bit-map of quoted characters.
     * This includes and character preceded by \, and any : or @ inside
     *  unquoted [ and ].
     * This can fail if an unquoted [ is found inside a [ ], or if an
     *  unquoted [ is not terminated, or if whitespace is found.
     * Backslashes and unquoted square brackets are deleted at this point.
     * Leaves a filtered copy of the string in uq[].
     */
    uq = Malloc(sl + 1);
    qmap = Malloc(sl + 1);
    memset(qmap, ' ', sl);
    qmap[sl] = '\0';
    rqmap = qmap;
    for (s = start; (size_t)(s - start) < sl; s++) {
	if (isspace((unsigned char)*s)) {
	    errmsg = "contains whitespace";
	    goto done;
	}
	if (quoted) {
	    qmap[uq_len] = '+';
	    quoted = false;
	    uq[uq_len++] = *s;
	    continue;
	} else if (*s == '\\') {
	    quoted = true;
	    continue;
	}
	if (bracketed) {
	    if (*s == ':' || *s == '@') {
		qmap[uq_len] = '+';
		/* add the character below */
	    } else if (*s == '[') {
		errmsg = "nested '['";
		goto done;
	    } else if (*s == ']') {
		/*
		 * What follows has to be the end of the
		 * string, or an unquoted ':' or a '@'.
		 */
		if ((size_t)(s - start) == sl - 1 ||
			*(s + 1) == '@' ||
			*(s + 1) == ':') {
			bracketed = 0;
		} else {
		    errmsg = "text following ']'";
		    goto done;
		}
		continue;
	    }
	} else if (*s == '[') {
	    /*
	     * Make sure that what came before is the beginning of
	     * the string or an unquoted : or @.
	     */
	    if (uq_len == 0 ||
		    (qmap[uq_len - 1] == ' ' &&
		     (uq[uq_len - 1] == ':' ||
		      uq[uq_len - 1] == '@'))) {
		bracketed = 1;
	    } else {
		errmsg = "text preceding '['";
		goto done;
	    }
	    continue;
	}
	uq[uq_len++] = *s;
    }
    if (quoted) {
	errmsg = "dangling '\\'";
	goto done;
    }
    if (bracketed) {
	errmsg = "missing ']'";
	goto done;
    }
    if (!uq_len) {
	errmsg = "empty hostname";
	goto done;
    }
    uq[uq_len] = '\0';

    /* Trim off prefixes. */
    s = uq;
    while ((pfx = strchr(pfxstr, *s)) != NULL &&
	    qmap[(s + 1) - uq] == ' ' &&
	    *(s + 1) == ':') {

	*prefixes |= 1 << ((pfx - pfxstr) / 2);
	s += 2;
	rqmap += 2;
    }
    start = s;

    /*
     * Now check for syntax: [LUname@]hostname[:port]
     * So more than one @, more than one :, : before @, or no text before @
     * or :, or no text after : are all syntax errors.
     * This also lets us figure out which elements are there.
     */
    while (*s) {
	if (rqmap[s - start] == ' ') {
	    if (*s == '@') {
		if (n_ch == 0) {
		    errmsg = "empty LU name";
		    goto done;
		}
		if (n_colon > 0) {
		    errmsg = "'@' after ':'";
		    goto done;
		}
		if (n_at > 0) {
		    errmsg = "double '@'";
		    goto done;
		}
		n_at++;
		n_ch = 0;
	    } else if (*s == ':') {
		if (n_ch == 0) {
		    errmsg = "empty hostname";
		    goto done;
		}
		if (n_colon > 0) {
		    errmsg = "double ':'";
		    goto done;
		}
		n_colon++;
		n_ch = 0;
	    } else {
		n_ch++;
	    }
	} else {
	    n_ch++;
	}
    s++;
    }
    if (!n_ch) {
	if (n_colon) {
	    errmsg = "empty port";
	} else {
	    errmsg = "empty hostname";
	}
	goto done;
    }

    /*
     * The syntax is clean, and we know what parts there are.
     * Split them out.
     */
    if (n_at) {
	*lu = Malloc(uq_len + 1);
	part[0] = *lu;
    }
    *host = Malloc(uq_len + 1);
    part[1] = *host;
    if (n_colon) {
	*port = Malloc(uq_len + 1);
	part[2] = *port;
    }
    s = start;
    n_ch = 0;
    while (*s) {
	if (rqmap[s - start] == ' ' && (*s == '@' || *s == ':')) {
	    part[part_ix][n_ch] = '\0';
	    part_ix++;
	    n_ch = 0;
	} else {
	    while (part[part_ix] == NULL) {
		part_ix++;
	    }
	    part[part_ix][n_ch++] = *s;
	}
	s++;
    }
    part[part_ix][n_ch] = '\0';

    /* Success! */
    rc = true;

done:
    if (uq != NULL) {
	Free(uq);
    }
    if (qmap != NULL) {
	Free(qmap);
    }
    if (!rc) {
	*error = xs_buffer("Hostname syntax error in '%s': %s", raw, errmsg);
    }
    return rc;
}

/*
 * Strip qualifiers from a hostname.
 * Returns the hostname part in a newly-malloc'd string.
 * 'needed' is returned true if anything was actually stripped.
 * Returns NULL if there is a syntax error.
 */
static char *
split_host(char *s, unsigned *flags, char *xluname, char **port, bool *needed)
{
    char *lu;
    char *host;
    char *error;

    *flags = 0;
    *needed = false;

    /* Call the sane, new version. */
    if (!new_split_host(s, &lu, &host, port, flags, &error)) {
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
    bool resolving;
    bool pending;
    const char *localprocess_cmd = NULL;
    bool has_colons = false;

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
	if ((s = split_host(nb, &host_flags, luname, &port,
			&needed)) == NULL) {
	    goto failure;
	}

	/* Look up the name in the hosts file. */
	if (!needed && hostfile_lookup(s, &target_name, &ps)) {
	    /*
	     * Rescan for qualifiers.
	     * Qualifiers, LU names, and ports are all overridden by the hosts
	     * file.
	     */
	    Free(s);
	    if (!(s = split_host(target_name, &host_flags, luname, &port,
			    &needed))) {
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
    Replace(qualified_host, xs_buffer("%s%s%s%s:%s",
		HOST_FLAG(SSL_HOST)? "L:": "",
		has_colons? "[": "",
		chost,
		has_colons? "]": "",
		port));

    /* Attempt contact. */
    ever_3270 = false;
    net_sock = net_connect(chost, port, localprocess_cmd != NULL, &resolving,
	    &pending);
    if (net_sock == INVALID_IOSRC && !resolving) {
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
    if (resolving) {
	cstate = RESOLVING;
	st_changed(ST_RESOLVING, true);
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
    x_add_input(net_sock);

    /* Set state and tell the world. */
    if (pending) {
	cstate = PENDING;
	st_changed(ST_HALF_CONNECT, true);
    } else {
	if (appres.nvt_mode) {
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
    net_disconnect();
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
    if (CONNECTED || HALF_CONNECTED) {
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
    if (CONNECTED || HALF_CONNECTED) {
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
