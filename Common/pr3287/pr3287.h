/*
 * Copyright (c) 2013-2024 Paul Mattes.
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
 * pr3287: A 3270 printer emulator for TELNET sessions.
 *
 * 	Global declarations.
 */

/* Nested include for tls_config_t. */
#include "tls_config.h"

/* Options. */
typedef struct {
	const char *assoc;	/* TN3270 session to associate with (-assoc) */
#if !defined(_WIN32) /*[*/
	enum { NOT_DAEMON, WILL_DAEMON, AM_DAEMON } bdaemon; /* daemon mode */
#endif /*]*/
	int blanklines;		/* -blanklines */
	const char *codepage;	/* code page (-codepage) */
#if !defined(_WIN32) /*[*/
	const char *command;	/* command to run for printing */
#endif /*]*/
	int crlf;		/* -crlf */
	int crthru;		/* -crtrhru */
	int emflush;		/* -emfush */
	unsigned long eoj_timeout; /* -eojtimeout */
	int ffeoj;		/* -ffeoj */
	int ffthru;		/* -ffthru */
	int ffskip;		/* -ffskip */
	int ignoreeoj;		/* -ignoreeoj */
#if defined(_WIN32) /*[*/
	const char *printer;	/* printer to use (-printer) */
	int printercp;		/* -printercp */
#endif /*]*/
	const char *proxy_spec;	/* proxy specification */
	int reconnect;		/* -reconnect */
	int skipcc;		/* -skipcc */
	int mpp;		/* -mpp */
	bool tls_host;		/* L: */
	tls_config_t tls;	/* TLS options */
	int syncport;		/* -syncport */
	const char *tracedir;	/* where we are tracing (-tracedir) */
	const char *tracefile;	/* where we are tracing (-tracefile) */
	int tracing;		/* are we tracing? (-trace) */
	const char *trnpre;	/* -trnpre */
	const char *trnpost;	/* -trnpost */
	bool prefer_ipv4;	/* -4 */
	bool prefer_ipv6;	/* -6 */
	int verbose;		/* -V */
	bool ut_env;		/* -utenv */
} options_t;
extern options_t options;

#if defined(_WIN32) /*[*/
extern char *instdir;
extern char *common_appdata;
#endif /*]*/
extern socket_t syncsock;

extern void pr3287_exit(int exit_code);

#define MIN_UNF_MPP	40	/* minimum value for unformatted MPP */
#define MAX_UNF_MPP	256	/* maximum value for unformatted MPP */
#define DEFAULT_UNF_MPP	132	/* default value for unformatted MPP */
