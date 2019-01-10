/*
 * Copyright (c) 1993-2019 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	show_action.c
 *		A curses-based 3270 Terminal Emulator
 *		'Show()' action.
 */

#include "globals.h"

#include "3270ds.h"
#include "actions.h"
#include "appres.h"
#include "codepage.h"
#include "copyright.h"
#include "cscreen.h"
#include "host.h"
#include "keymap.h"
#include "lazya.h"
#include "linemode.h"
#include "popups.h"
#include "resources.h"
#include "show_action.h"
#include "split_host.h"
#include "telnet.h"
#include "utf8.h"
#include "utils.h"

/* Return a time difference in English */
static char *
hms(time_t ts)
{
    time_t t, td;
    long hr, mn, sc;

    (void) time(&t);

    td = t - ts;
    hr = (long)(td / 3600);
    mn = (td % 3600) / 60;
    sc = td % 60;

    if (hr > 0) {
	return lazyaf("%ld %s %ld %s %ld %s",
	    hr, (hr == 1) ?
		get_message("hour") : get_message("hours"),
	    mn, (mn == 1) ?
		get_message("minute") : get_message("minutes"),
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    } else if (mn > 0) {
	return lazyaf("%ld %s %ld %s",
	    mn, (mn == 1) ?
		get_message("minute") : get_message("minutes"),
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    } else {
	return lazyaf("%ld %s",
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    }
}

static void
indent_dump(const char *s)
{
    const char *newline;

    while ((newline = strchr(s, '\n')) != NULL) {
	action_output("    %.*s", (int)(newline - s), s);
	s = newline + 1;
    }
    action_output("    %s", s);
}

static void
status_dump(void)
{
    const char *emode, *ftype, *ts;
    const char *clu;
    const char *eopts;
    const char *bplu;
    const char *ptype;

    action_output("%s", build);
    action_output("%s %s: %d %s x %d %s, %s, %s",
	    get_message("model"), model_name,
	    maxCOLS, get_message("columns"),
	    maxROWS, get_message("rows"),
	    appres.m3279? get_message("fullColor"): get_message("mono"),
	    (appres.extended && !HOST_FLAG(STD_DS_HOST))?
		get_message("extendedDs"): get_message("standardDs"));
    action_output("%s %s", get_message("terminalName"), termtype);
    clu = net_query_lu_name();
    if (clu != NULL && clu[0]) {
	action_output("%s %s", get_message("luName"), clu);
    }
    bplu = net_query_bind_plu_name();
    if (bplu != NULL && bplu[0]) {
	action_output("%s %s", get_message("bindPluName"), bplu);
    }
    action_output("%s %s (%s) %s", get_message("hostCodePage"),
	    get_codepage_name(), dbcs? "DBCS": "SBCS",
	    get_codepage_number());
    action_output("%s GCSGID %u, CPGID %u",
	    get_message("sbcsCgcsgid"),
	    (unsigned short)((cgcsgid >> 16) & 0xffff),
	    (unsigned short)(cgcsgid & 0xffff));
    if (dbcs) {
	action_output("%s GCSGID %u, CPGID %u",
		get_message("dbcsCgcsgid"),
		(unsigned short)((cgcsgid_dbcs >> 16) & 0xffff),
		(unsigned short)(cgcsgid_dbcs & 0xffff));
    }
#if !defined(_WIN32) /*[*/
    action_output("%s %s", get_message("localeCodeset"), locale_codeset);
    action_output("%s, wide curses %s",
	    get_message("buildOpts"),
# if defined(CURSES_WIDE) /*[*/
	    get_message("buildEnabled")
# else /*][*/
	    get_message("buildDisabled")
# endif /*]*/
	    );
#else /*][*/
    action_output("%s OEM %d ANSI %d", get_message("windowsCodePage"),
	    windows_cp, GetACP());
#endif /*]*/
    if (appres.interactive.key_map) {
	action_output("%s %s", get_message("keyboardMap"),
		appres.interactive.key_map);
    }
    if (CONNECTED) {
	action_output("%s %s", get_message("connectedTo"),
#if defined(LOCAL_PROCESS) /*[*/
		(local_process && !strlen(current_host))? "(shell)":
#endif /*]*/
		current_host);
#if defined(LOCAL_PROCESS) /*[*/
	if (!local_process)
#endif /*]*/
	{
	    action_output("  %s %d", get_message("port"), current_port);
	}
	if (net_secure_connection()) {
	    const char *session, *cert;

	    action_output("  %s%s%s", get_message("secure"),
			net_secure_unverified()? ", ": "",
			net_secure_unverified()? get_message("unverified"): "");
	    action_output("  %s %s", get_message("provider"),
		    net_sio_provider());
	    if ((session = net_session_info()) != NULL) {
		action_output("  %s", get_message("sessionInfo"));
		indent_dump(session);
	    }
	    if ((cert = net_server_cert_info()) != NULL) {
		action_output("  %s", get_message("serverCert"));
		indent_dump(cert);
	    }
	}
	ptype = net_proxy_type();
	if (ptype) {
	    action_output("  %s %s  %s %s  %s %s",
		    get_message("proxyType"), ptype,
		    get_message("server"), net_proxy_host(),
		    get_message("port"), net_proxy_port());
	}
	ts = hms(ns_time);
	if (IN_E) {
	    emode = "TN3270E ";
	} else {
	    emode = "";
	}
	if (IN_NVT) {
	    if (linemode) {
		ftype = get_message("lineMode");
	    } else {
		ftype = get_message("charMode");
	    }
	    action_output("  %s%s, %s", emode, ftype, ts);
	} else if (IN_SSCP) {
	    action_output("  %s%s, %s", emode, get_message("sscpMode"), ts);
	} else if (IN_3270) {
	    action_output("  %s%s, %s", emode, get_message("dsMode"), ts);
	} else if (cstate == CONNECTED_UNBOUND) {
	    action_output("  %s%s, %s", emode, get_message("unboundMode"), ts);
	} else {
	    action_output("  %s, %s", get_message("unnegotiated"), ts);
	}

	eopts = tn3270e_current_opts();
	if (eopts != NULL) {
	    action_output("  %s %s", get_message("tn3270eOpts"), eopts);
	} else if (IN_E) {
	    action_output("  %s", get_message("tn3270eNoOpts"));
	}

	if (IN_3270) {
	    action_output("%s %d %s, %d %s\n%s %d %s, %d %s",
		    get_message("sent"),
		    ns_bsent, (ns_bsent == 1)?
			get_message("byte") : get_message("bytes"),
		    ns_rsent, (ns_rsent == 1)?
			get_message("record") : get_message("records"),
		    get_message("Received"), ns_brcvd,
			(ns_brcvd == 1)? get_message("byte"):
					 get_message("bytes"),
		    ns_rrcvd,
		    (ns_rrcvd == 1)? get_message("record"):
				     get_message("records"));
	} else {
	    action_output("%s %d %s, %s %d %s",
		    get_message("sent"), ns_bsent,
		    (ns_bsent == 1)? get_message("byte"):
				     get_message("bytes"),
		    get_message("received"), ns_brcvd,
		    (ns_brcvd == 1)? get_message("byte"):
				     get_message("bytes"));
	}

	if (IN_NVT) {
	    struct ctl_char *c = linemode_chars();
	    int i;
	    char buf[128];
	    char *s = buf;

	    action_output("%s", get_message("specialCharacters"));
	    for (i = 0; c[i].name; i++) {
		if (i && !(i % 4)) {
		    *s = '\0';
		    action_output("%s", buf);
		    s = buf;
		}
		s += sprintf(s, "  %s %s", c[i].name, c[i].value);
	    }
	    if (s != buf) {
		*s = '\0';
		action_output("%s", buf);
	    }
	}
    } else if (HALF_CONNECTED) {
	action_output("%s %s", get_message("connectionPending"),
	    current_host);
    } else if (host_reconnecting()) {
	action_output("%s", get_message("reconnecting"));
    } else {
	action_output("%s", get_message("notConnected"));
    }
}

static void
copyright_dump(void)
{
    action_output(" ");
    action_output("%s", show_copyright());
    action_output(" ");
}

bool
Show_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Show", ia, argc, argv);
    if (check_argc("Show", argc, 0, 1) < 0) {
	return false;
    }

    if (argc == 0) {
	action_output("  Show copyright   copyright information");
	action_output("  Show stats       connection statistics");
	action_output("  Show status      same as 'Show stats'");
	action_output("  Show keymap      current keymap");
	return true;
    }
    if (!strncasecmp(argv[0], "stats", strlen(argv[0])) ||
	!strncasecmp(argv[0], "status", strlen(argv[0]))) {
	status_dump();
    } else if (!strncasecmp(argv[0], "keymap", strlen(argv[0]))) {
	keymap_dump();
    } else if (!strncasecmp(argv[0], "copyright", strlen(argv[0]))) {
	copyright_dump();
    } else {
	popup_an_error("Unknown 'Show' keyword");
	return false;
    }
    return true;
}
