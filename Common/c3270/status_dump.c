/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
#include "linemode.h"
#include "model.h"
#include "popups.h"
#include "query.h"
#include "resources.h"
#include "split_host.h"
#include "status_dump.h"
#include "telnet.h"
#include "txa.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"

/* Return a time difference in English */
static char *
hms(time_t ts)
{
    time_t t, td;
    long hr, mn, sc;

    time(&t);

    td = t - ts;
    hr = (long)(td / 3600);
    mn = (td % 3600) / 60;
    sc = td % 60;

    if (hr > 0) {
	return txAsprintf("%ld %s %ld %s %ld %s",
	    hr, (hr == 1) ?
		get_message("hour") : get_message("hours"),
	    mn, (mn == 1) ?
		get_message("minute") : get_message("minutes"),
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    } else if (mn > 0) {
	return txAsprintf("%ld %s %ld %s",
	    mn, (mn == 1) ?
		get_message("minute") : get_message("minutes"),
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    } else {
	return txAsprintf("%ld %s",
	    sc, (sc == 1) ?
		get_message("second") : get_message("seconds"));
    }
}

static void
indent_dump(varbuf_t *r, const char *s)
{
    const char *newline;

    while ((newline = strchr(s, '\n')) != NULL) {
	vb_appendf(r, "    %.*s\n", (int)(newline - s), s);
	s = newline + 1;
    }
    vb_appendf(r, "    %s\n", s);
}

const char *
status_dump(void)
{
    varbuf_t r;
    const char *emode, *ftype, *ts;
    const char *clu;
    const char *eopts;
    const char *bplu;
    const char *ptype;
    char *s;
    size_t sl;

    vb_init(&r);

    vb_appendf(&r, "%s\n", build);
    vb_appendf(&r, "%s %s: %d %s x %d %s, %s, %s\n",
	    get_message("model"), get_model(),
	    maxCOLS, get_message("columns"),
	    maxROWS, get_message("rows"),
	    mode3279? get_message("fullColor"): get_message("mono"),
	    (appres.extended_data_stream && !HOST_FLAG(STD_DS_HOST))?
		get_message("extendedDs"): get_message("standardDs"));
    vb_appendf(&r, "%s %s\n", get_message("terminalName"), termtype);
    clu = net_query_lu_name();
    if (clu != NULL && clu[0]) {
	vb_appendf(&r, "%s %s\n", get_message("luName"), clu);
    }
    bplu = net_query_bind_plu_name();
    if (bplu != NULL && bplu[0]) {
	vb_appendf(&r, "%s %s\n", get_message("bindPluName"), bplu);
    }
    vb_appendf(&r, "%s %s (%s) %s\n", get_message("hostCodePage"),
	    get_codepage_name(), dbcs? "DBCS": "SBCS",
	    get_codepage_number());
    vb_appendf(&r, "%s GCSGID %u, CPGID %u\n",
	    get_message("sbcsCgcsgid"),
	    (unsigned short)((cgcsgid >> 16) & 0xffff),
	    (unsigned short)(cgcsgid & 0xffff));
    if (dbcs) {
	vb_appendf(&r, "%s GCSGID %u, CPGID %u\n",
		get_message("dbcsCgcsgid"),
		(unsigned short)((cgcsgid_dbcs >> 16) & 0xffff),
		(unsigned short)(cgcsgid_dbcs & 0xffff));
    }
#if !defined(_WIN32) /*[*/
    vb_appendf(&r, "%s %s\n", get_message("localeCodeset"), locale_codeset);
    vb_appendf(&r, "%s, wide curses %s\n",
	    get_message("buildOpts"),
# if defined(CURSES_WIDE) /*[*/
	    get_message("buildEnabled")
# else /*][*/
	    get_message("buildDisabled")
# endif /*]*/
	    );
#else /*][*/
    vb_appendf(&r, "%s OEM %d ANSI %d\n", get_message("windowsCodePage"),
	    windows_cp, GetACP());
#endif /*]*/
    if (appres.interactive.key_map) {
	vb_appendf(&r, "%s %s\n", get_message("keyboardMap"),
		appres.interactive.key_map);
    }
    if (CONNECTED) {
	vb_appendf(&r, "%s %s\n", get_message("connectedTo"),
#if defined(LOCAL_PROCESS) /*[*/
		(local_process && !strlen(current_host))? "(shell)":
#endif /*]*/
		current_host);
#if defined(LOCAL_PROCESS) /*[*/
	if (!local_process)
#endif /*]*/
	{
	    vb_appendf(&r, "  %s %d\n", get_message("port"), current_port);
	}
	if (net_secure_connection()) {
	    const char *session, *cert;

	    vb_appendf(&r, "  %s%s%s\n", get_message("secure"),
			net_secure_unverified()? ", ": "",
			net_secure_unverified()? get_message("unverified"): "");
	    vb_appendf(&r, "  %s %s\n", get_message("provider"),
		    net_sio_provider());
	    if ((session = net_session_info()) != NULL) {
		vb_appendf(&r, "  %s\n", get_message("sessionInfo"));
		indent_dump(&r, session);
	    }
	    if ((cert = net_server_cert_info()) != NULL) {
		vb_appendf(&r, "  %s\n", get_message("serverCert"));
		indent_dump(&r, cert);
	    }
	}
	ptype = net_proxy_type();
	if (ptype != NULL) {
	    vb_appendf(&r, "  %s %s  %s %s  %s %s",
		    get_message("proxyType"), ptype,
		    get_message("server"), net_proxy_host(),
		    get_message("port"), net_proxy_port());
	    if (net_proxy_user() != NULL) {
		vb_appendf(&r, "  %s %s", get_message("user"),
			net_proxy_user());
	    }
	    vb_appendf(&r, "\n");
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
	    vb_appendf(&r, "  %s%s, %s\n", emode, ftype, ts);
	} else if (IN_SSCP) {
	    vb_appendf(&r, "  %s%s, %s\n", emode, get_message("sscpMode"), ts);
	} else if (IN_3270) {
	    vb_appendf(&r, "  %s%s, %s\n", emode, get_message("dsMode"), ts);
	} else if (cstate == CONNECTED_UNBOUND) {
	    vb_appendf(&r, "  %s%s, %s\n", emode, get_message("unboundMode"), ts);
	} else {
	    vb_appendf(&r, "  %s, %s\n", get_message("unnegotiated"), ts);
	}

	eopts = tn3270e_current_opts();
	if (eopts != NULL) {
	    vb_appendf(&r, "  %s %s\n", get_message("tn3270eOpts"), eopts);
	} else if (IN_E) {
	    vb_appendf(&r, "  %s\n", get_message("tn3270eNoOpts"));
	}

	if (IN_3270) {
	    vb_appendf(&r, "%s %d %s, %d %s\n%s %d %s, %d %s\n",
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
	    vb_appendf(&r, "%s %d %s, %s %d %s\n",
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

	    vb_appendf(&r, "%s\n", get_message("specialCharacters"));
	    for (i = 0; c[i].name; i++) {
		if (i && !(i % 4)) {
		    *s = '\0';
		    vb_appendf(&r, "%s\n", buf);
		    s = buf;
		}
		s += sprintf(s, "  %s %s", c[i].name, c[i].value);
	    }
	    if (s != buf) {
		*s = '\0';
		vb_appendf(&r, "%s\n", buf);
	    }
	}
    } else if (HALF_CONNECTED) {
	vb_appendf(&r, "%s %s\n", get_message("connectionPending"),
	    current_host);
    } else if (host_reconnecting()) {
	vb_appendf(&r, "%s\n", get_message("reconnecting"));
    } else {
	vb_appendf(&r, "%s\n", get_message("notConnected"));
    }

    s = vb_consume(&r);
    sl = strlen(s);
    if (sl > 0 && s[sl - 1] == '\n') {
	s[sl - 1] = '\0';
    }
    return txdFree(s);
}
