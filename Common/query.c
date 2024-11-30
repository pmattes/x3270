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
 *      query.c
 *              The Query() action.
 */

#include "globals.h"

#include "appres.h"

#include "actions.h"
#include "codepage.h"
#include "copyright.h"
#include "ctlrc.h"
#include "host.h"
#include "model.h"
#include "names.h"
#include "popups.h"
#include "query.h"
#include "split_host.h"
#include "telnet.h"
#include "task.h"
#include "trace.h"
#include "screentrace.h" /* has to come after trace.h */
#include "txa.h"
#include "unicodec.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"

/* Globals */

/* Statics */

/* The current set of queries. */
static query_t *queries;
static size_t num_queries;

static const char *
query_terminal_name(void)
{
    return termtype;
}

static const char *
query_build(void)
{
    return build;
}

static const char *
get_connect_time(void)
{
    time_t t, td;
    long dy, hr, mn, sc;

    if (!CONNECTED) {
	return NULL;
    }

    time(&t);

    td = t - ns_time;
    dy = (long)(td / (3600 * 24));
    hr = (dy % (3600 * 24)) / 3600;
    mn = (td % 3600) / 60;
    sc = td % 60;

    return (dy > 0)?
	txAsprintf("%ud%02u:%02u:%02u", (u_int)dy, (u_int)hr, (u_int)mn,
		(u_int)sc):
	txAsprintf("%02u:%02u:%02u", (u_int)hr, (u_int)mn, (u_int)sc);
}

static const char *
get_codepage(void)
{
    char *sbcs = txAsprintf("%s sbcs gcsgid %u cpgid %u",
	    get_canonical_codepage(),
	    (unsigned short)((cgcsgid >> 16) & 0xffff),
	    (unsigned short)(cgcsgid & 0xffff));
    return dbcs? txAsprintf("%s dbcs gcsgid %u cpgid %u",
	    sbcs,
	    (unsigned short)((cgcsgid_dbcs >> 16) & 0xffff),
	    (unsigned short)(cgcsgid_dbcs & 0xffff)):
	sbcs;
}

static const char *
get_codepages(void)
{
    cpname_t *c = get_cpnames();
    varbuf_t r;
    int i, j;
    char *sep = "";

    vb_init(&r);
    for (i = 0; c[i].name != NULL; i++) {
	vb_appendf(&r, "%s%s %cbcs", sep, c[i].name, c[i].dbcs? 'd': 's');
	sep = "\n";
	for (j = 0; j < c[i].num_aliases; j++) {
	    vb_appendf(&r, " %s", c[i].aliases[j]);
	}
    }

    free_cpnames(c);
    return txdFree(vb_consume(&r));
}

static const char *
get_proxy(void)
{
    const char *ptype = net_proxy_type();
    const char *user = net_proxy_user();

    return (ptype != NULL)?
	txAsprintf("%s %s %s%s%s",
		ptype,
		net_proxy_host(),
		net_proxy_port(),
		(user != NULL)? " ": "",
		(user != NULL)? user: ""):
	NULL;
}

static const char *
get_rx(void)
{
    if (!CONNECTED) {
	return NULL;
    }

    return IN_3270?
	txAsprintf("records %u bytes %u", ns_rrcvd, ns_brcvd):
	txAsprintf("bytes %u", ns_brcvd);
}

static const char *
get_screentracefile(void)
{
    if (!toggled(SCREEN_TRACE)) {
	return NULL;
    }
    return trace_get_screentrace_name();
}

static const char *
get_tasks(void)
{
    char *r = task_get_tasks();
    size_t sl = strlen(r);

    if (sl > 0 && r[sl - 1] == '\n') {
	r[sl - 1] = '\0';
    }
    return txdFree(r);
}

static const char *
get_tracefile(void)
{
    if (!toggled(TRACING)) {
	return NULL;
    }
    return tracefile_name;
}

static const char *
get_tx(void)
{
    if (!CONNECTED) {
	return NULL;
    }

    return IN_3270?
	txAsprintf("records %u bytes %u", ns_rsent, ns_bsent):
	txAsprintf("bytes %u", ns_bsent);
}

const char *
get_about(void)
{
    return txAsprintf("%s\nCopyright 1989-%s by Paul Mattes, GTRC and others.",
	    build, cyear);
}

/* Common code for Query() and Show() actions. */
bool
query_common(const char *name, ia_t ia, unsigned argc, const char **argv)
{
    size_t i;
    size_t sl;

    action_debug(name, ia, argc, argv);
    if (check_argc(name, argc, 0, 1) < 0) {
	return false;
    }

    switch (argc) {
    case 0:
	for (i = 0; i < num_queries; i++) {
	    if (!queries[i].hidden) {
		const char *s = (queries[i].fn? (*queries[i].fn)():
			queries[i].string);

		if (s == NULL) {
		    s = "";
		}
		if (queries[i].specific && strcmp(s, "")) {
		    s = "...";
		}
		action_output("%s:%s%s", queries[i].name, *s? " ": "", s);
	    }
	}
	break;
    case 1:
	sl = strlen(argv[0]);
	for (i = 0; i < num_queries; i++) {
	    if (!strncasecmp(argv[0], queries[i].name, sl)) {
		const char *s;

		if (strlen(queries[i].name) > sl &&
			queries[i + 1].name != NULL &&
			!strncasecmp(argv[0], queries[i + 1].name, sl)) {
		    popup_an_error("%s: Ambiguous parameter", name);
		    return false;
		}

		if (queries[i].fn) {
		    s = (*queries[i].fn)();
		} else {
		    s = queries[i].string;
		}
		if (s == NULL) {
		    s = "";
		}
		action_output("%s\n", s);
		return true;
	    }
	}
	popup_an_error("%s: Unknown parameter", name);
	return false;
    }
    return true;
}

/* Compare two queries by name. */
static int
query_compare(const void *a, const void *b)
{
    const query_t *qa = (query_t *)a;
    const query_t *qb = (query_t *)b;

    return strcmp(qa->name, qb->name);
}

bool
Query_action(ia_t ia, unsigned argc, const char **argv)
{
    return query_common(AnQuery, ia, argc, argv);
}

bool
Show_action(ia_t ia, unsigned argc, const char **argv)
{
    return query_common(AnShow, ia, argc, argv);
}

/**
 * Register a set of queries.
 */
void
register_queries(query_t new_queries[], size_t count)
{
    query_t *q = Malloc(sizeof(query_t) * (num_queries + count));

    memcpy(q, queries, sizeof(query_t) * num_queries);
    memcpy(q + num_queries, new_queries, sizeof(query_t) * count);
    qsort(q, num_queries + count, sizeof(query_t), query_compare);
    Free(queries);
    queries = q;
    num_queries += count;
}

/**
 * Query module registration.
 */
void
query_register(void)
{
    static action_table_t actions[] = {
	{ AnQuery,		Query_action, 0 },
	{ AnShow,		Show_action, 0 }
    };
    static query_t base_queries[] = {
	{ KwAbout, get_about, NULL, false, true },
	{ KwActions, all_actions, NULL, false, true },
	{ KwBindPluName, net_query_bind_plu_name, NULL, false, false },
	{ KwBuildOptions, build_options, NULL, false, false },
	{ KwConnectionState, net_query_connection_state, NULL, false, false },
	{ KwConnectTime, get_connect_time, NULL, false, false },
	{ KwCodePage, get_codepage, NULL, false, false },
	{ KwCodePages, get_codepages, NULL, false, true },
	{ KwCopyright, show_copyright, NULL, false, true },
	{ KwCursor, ctlr_query_cursor, NULL, true, false },
	{ KwCursor1, ctlr_query_cursor1, NULL, false, false },
	{ KwFormatted, ctlr_query_formatted, NULL, false, false },
	{ KwHost, net_query_host, NULL, false, false },
	{ KwLocalEncoding, get_codeset, NULL, false, false },
	{ KwLuName, net_query_lu_name, NULL, false, false },
	{ KwModel, get_full_model, NULL, true, false },
	{ KwPrefixes, host_prefixes, NULL, false, false },
	{ KwProxy, get_proxy, NULL, false, false },
	{ KwScreenCurSize, ctlr_query_cur_size_old, NULL, true, false },
	{ KwScreenMaxSize, ctlr_query_max_size_old, NULL, true, false },
	{ KwScreenSizeCurrent, ctlr_query_cur_size, NULL, false, false },
	{ KwScreenSizeMax, ctlr_query_max_size, NULL, false, false },
	{ KwScreenTraceFile, get_screentracefile, NULL, false, false },
	{ KwSsl, net_query_tls, NULL, true, false },
	{ KwStatsRx, get_rx, NULL, false, false },
	{ KwStatsTx, get_tx, NULL, false, false },
	{ KwTasks, get_tasks, NULL, false, true },
	{ KwTelnetMyOptions, net_myopts, NULL, false, false },
	{ KwTelnetHostOptions, net_hisopts, NULL, false, false },
	{ KwTerminalName, query_terminal_name, NULL, false, false },
	{ KwTraceFile, get_tracefile, NULL, false, false },
	{ KwTls, net_query_tls, NULL, false, false },
	{ KwTlsCertInfo, net_server_cert_info, NULL, false, true },
	{ KwTlsSubjectNames, net_server_subject_names, NULL, false, true },
	{ KwTlsProvider, net_sio_provider, NULL, false, false },
	{ KwTlsSessionInfo, net_session_info, NULL, false, true },
	{ KwTn3270eOptions, tn3270e_current_opts, NULL, false, false },
	{ KwVersion, query_build, NULL, false, false }
    };

    /* Register actions.*/
    register_actions(actions, array_count(actions));

    /* Register queries. */
    register_queries(base_queries, array_count(base_queries));
}
