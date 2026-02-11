/*
 * Copyright (c) 1993-2026 Paul Mattes.
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

#include "3270ds.h"
#include "actions.h"
#include "codepage.h"
#include "copyright.h"
#include "ctlrc.h"
#include "host.h"
#include "linemode.h"
#include "model.h"
#include "names.h"
#include "nvt_gui.h"
#include "popups.h"
#include "product.h"
#include "query.h"
#include "see.h"
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

#if defined(_WIN32) /*[*/
# include "main_window.h"
#endif /*]*/

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
    const char *sbcs = txAsprintf("%s sbcs gcsgid %u cpgid %u",
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
get_reply_mode(void)
{
    varbuf_t r;
    int i;

    switch (reply_mode) {
    case SF_SRM_FIELD:
	return "field";
    case SF_SRM_XFIELD:
	return "extended-field";
    case SF_SRM_CHAR:
	vb_init(&r);
	vb_appends(&r, "character");
	for (i = 0; i < crm_nattr; i++) {
	    vb_appendf(&r, " +%s", see_efa_only(crm_attr[i]));
	}
	return txdFree(vb_consume(&r));
    default:
	return txAsprintf("0x%02x", reply_mode);
    }
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

#if defined(_WIN32) /*[*/
static const char *
get_windowid(void)
{
    return get_main_window_str();
}
#endif /*]*/

static const char *
format_pixels(unsigned height, unsigned width)
{
    return txAsprintf("height %u width %d", height, width);
}

static const char *
query_character_pixels(void)
{
    unsigned height = 0, width = 0;

    get_character_pixels(&height, &width);
    return format_pixels(height, width);
}

static const char *
query_display_pixels(void)
{
    unsigned height = 0, width = 0;

    get_screen_pixels(&height, &width);
    return format_pixels(height, width);
}

static const char *
query_window_pixels(void)
{
    unsigned height = 0, width = 0;

    get_window_pixels(&height, &width);
    return format_pixels(height, width);
}

static const char *
query_window_location(void)
{
    int x = 0, y = 0;

    get_window_location(&x, &y);
    return txAsprintf("x %d y %d", x, y);
}

static const char *
query_window_state(void)
{
    switch (get_window_state()) {
	case WS_NORMAL:
	    return "normal";
	case WS_ICONIFIED:
	    return "iconified";
       case WS_MAXIMIZED:
	    return "maximized";
       case WS_FULLSCREEN:
	    return "full-screen";
       case WS_NONE:
       default:
	    return "none";
    }
}

/* Query everything, hidden or not, specific or not. */
static const char *
query_all(void)
{
    size_t i;
    varbuf_t r;
    const char *bk = "";

    vb_init(&r);
    for (i = 0; i < num_queries; i++) {
	const char *s;

	if (queries[i].fn == query_all || (queries[i].flags & (QF_ALIAS | QF_DEPRECATED))) {
	    continue;
	}

	s = (queries[i].fn? (*queries[i].fn)(): queries[i].string);
	if (s == NULL) {
	    s = "";
	}
	if (queries[i].flags & QF_SPECIFIC) {
	    const char *rest = s;
	    const char *nl;

	    if (!*s) {
		vb_appendf(&r, "%s%s:", bk, queries[i].name);
		bk = "\n";
		continue;
	    }
	    while ((nl = strchr(rest, '\n')) != NULL) {
		vb_appendf(&r, "%s%s: %.*s", bk, queries[i].name, (int)(nl - rest), rest);
		rest = nl + 1;
		bk = "\n";
	    }
	    if (*rest) {
		vb_appendf(&r, "%s%s: %s", bk, queries[i].name, rest);
		bk = "\n";
	    }
	} else {
	    vb_appendf(&r, "%s%s:%s%s",
		    bk,
		    queries[i].name,
		    *s? " ": "",
		    s);
	    bk = "\n";
	}
    }
    return txdFree(vb_consume(&r));
}

/* Common code for Query() and Show() actions. */
bool
query_common(const char *name, ia_t ia, unsigned argc, const char **argv)
{
    size_t i;
    ssize_t exact = -1;
    ssize_t any = -1;
    int matches = 0;

    action_debug(name, ia, argc, argv);
    if (check_argc(name, argc, 0, 1) < 0) {
	return false;
    }

    switch (argc) {
    case 0:
	for (i = 0; i < num_queries; i++) {
	    if (!(queries[i].flags & (QF_HIDDEN | QF_ALIAS | QF_DEPRECATED))) {
		const char *s = (queries[i].fn? (*queries[i].fn)():
			queries[i].string);

		if (s == NULL) {
		    s = "";
		}
		if ((queries[i].flags & QF_SPECIFIC) && strcmp(s, "")) {
		    s = "...";
		}
		action_output("%s:%s%s", queries[i].name, *s? " ": "", s);
	    }
	}
	break;
    case 1:
	/* Look for an exact match. */
	for (i = 0; i < num_queries; i++) {
	    if (!strcasecmp(argv[0], queries[i].name)) {
		exact = i;
		break;
	    }
	}

	if (exact < 0) {
	    varbuf_t r;
	    size_t sl = strlen(argv[0]);

	    vb_init(&r);

	    /* Look for an inexact match. */
	    for (i = 0; i < num_queries; i++) {
		if (!strncasecmp(argv[0], queries[i].name, sl)) {
		    any = i;
		    vb_appendf(&r, "%s%s", matches? ", ": "", queries[i].name);
		    matches++;
		}
	    }

	    if (matches > 1) {
		popup_an_error("%s(): Ambiguous parameter '%s': %s", name, argv[0], txdFree(vb_consume(&r)));
		return false;
	    }

	    vb_free(&r);
	}

	if (exact >= 0 || any >= 0) {
	    const char *s;

	    i = (exact >= 0)? exact: any;
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
	popup_an_error("%s(): Unknown parameter '%s'", name, argv[0]);
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

/* Get the special line-mode characters. */
static const char *
get_special_characters(void)
{
    varbuf_t r;
    struct ctl_char *c = linemode_chars();
    int i;

    vb_init(&r);
    for (i = 0; c[i].name; i++) {
	if (i && !(i % 4)) {
	    vb_appends(&r, "\n");
	}
	vb_appendf(&r, "%s%s %s", (i % 4)? " ": "", c[i].name, c[i].value);
    }
    return txdFree(vb_consume(&r));
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
	{ KwAbout, get_about, NULL, QF_SPECIFIC },
	{ KwActions, all_actions, NULL, QF_SPECIFIC },
	{ KwAll, query_all, NULL, QF_SPECIFIC },
	{ KwBindPluName, net_query_bind_plu_name, NULL, 0 },
	{ KwBuildOptions, build_options, NULL, 0 },
	{ KwConnectionState, net_query_connection_state, NULL, 0 },
	{ KwConnectTime, get_connect_time, NULL, 0 },
	{ KwCodePage, get_codepage, NULL, 0 },
	{ KwCodePages, get_codepages, NULL, QF_SPECIFIC },
	{ KwCopyright, show_copyright, NULL, QF_SPECIFIC },
	{ KwCursor, ctlr_query_cursor, NULL, QF_DEPRECATED },
	{ KwCursor1, ctlr_query_cursor1, NULL, 0 },
	{ KwFormatted, ctlr_query_formatted, NULL, 0 },
	{ KwHost, net_query_host, NULL, 0 },
	{ KwLocalEncoding, get_codeset, NULL, 0 },
	{ KwLuName, net_query_lu_name, NULL, 0 },
	{ KwModel, get_full_model, NULL, QF_DEPRECATED },
	{ KwPrefixes, host_prefixes, NULL, 0 },
	{ KwProxy, get_proxy, NULL, 0 },
	{ KwReplyMode, get_reply_mode, NULL, 0 },
	{ KwScreenCurSize, ctlr_query_cur_size_old, NULL, QF_DEPRECATED },
	{ KwScreenMaxSize, ctlr_query_max_size_old, NULL, QF_DEPRECATED },
	{ KwScreenSizeCurrent, ctlr_query_cur_size, NULL, 0 },
	{ KwScreenSizeMax, ctlr_query_max_size, NULL, 0 },
	{ KwScreenTraceFile, get_screentracefile, NULL, 0 },
	{ KwSpecialCharacters, get_special_characters, NULL, QF_SPECIFIC },
	{ KwSsl, net_query_tls, NULL, QF_ALIAS },
	{ KwStatsRx, get_rx, NULL, 0 },
	{ KwStatsTx, get_tx, NULL, 0 },
	{ KwTasks, get_tasks, NULL, QF_SPECIFIC },
	{ KwTelnetMyOptions, net_myopts, NULL, 0 },
	{ KwTelnetHostOptions, net_hisopts, NULL, 0 },
	{ KwTerminalName, query_terminal_name, NULL, 0 },
	{ KwTraceFile, get_tracefile, NULL, 0 },
	{ KwTls, net_query_tls, NULL, 0 },
	{ KwTlsCertInfo, net_server_cert_info, NULL, QF_SPECIFIC },
	{ KwTlsSubjectNames, net_server_subject_names, NULL, QF_SPECIFIC },
	{ KwTlsProvider, net_sio_provider, NULL, 0 },
	{ KwTlsSessionInfo, net_session_info, NULL, QF_SPECIFIC },
	{ KwTn3270eOptions, tn3270e_current_opts, NULL, 0 },
	{ KwVersion, query_build, NULL, 0 },
    };
    static query_t hidden_window_queries[] = {
	{ KwCharacterPixels, query_character_pixels, NULL, QF_HIDDEN },
	{ KwDisplayPixels, query_display_pixels, NULL, QF_HIDDEN },
	{ KwWindowPixels, query_window_pixels, NULL, QF_HIDDEN },
	{ KwWindowLocation, query_window_location, NULL, QF_HIDDEN },
	{ KwWindowState, query_window_state, NULL, QF_HIDDEN },
    };
    static query_t visible_window_queries[] = {
	{ KwCharacterPixels, query_character_pixels, NULL, 0 },
	{ KwDisplayPixels, query_display_pixels, NULL, 0 },
	{ KwWindowPixels, query_window_pixels, NULL, 0 },
	{ KwWindowLocation, query_window_location, NULL, 0 },
	{ KwWindowState, query_window_state, NULL, 0 },
    };
#if defined(_WIN32) /*[*/
    static query_t visible_window_id_queries[] = {
	{ KwWindowId, get_windowid, NULL, 0 },
    };
    static query_t hidden_window_id_queries[] = {
	{ KwWindowId, get_windowid, NULL, QF_HIDDEN },
    };
#endif /*]*/

    /* Register actions.*/
    register_actions(actions, array_count(actions));

    /* Register base queries. */
    register_queries(base_queries, array_count(base_queries));

    /* Register possibly-hidden queries. */
    if (get_window_state() != WS_NONE) {
	register_queries(visible_window_queries, array_count(visible_window_queries));
    } else {
	register_queries(hidden_window_queries, array_count(hidden_window_queries));
    }
#if defined(_WIN32) /*[*/
    if (product_has_window_id()) {
	register_queries(visible_window_id_queries, array_count(visible_window_id_queries));
    } else {
	register_queries(hidden_window_id_queries, array_count(hidden_window_id_queries));
    }
#endif /*]*/
}
