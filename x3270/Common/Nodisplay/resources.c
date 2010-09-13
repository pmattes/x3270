/*
 * Copyright (c) 1999-2009, Paul Mattes.
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

#include <stdio.h>
#include <string.h>
#include "localdefs.h"

extern String fallbacks[];

/* s3270 substitute Xt resource database. */

#if defined(C3270) /*[*/
/*
 * These should be properly #ifdef'd in X3270.xad, but it would turn it into
 * spaghetti.
 */
static struct {
        char *name;
        char *value;
} rdb[] = {
	{ "message.hour",       "hour" },
	{ "message.hours",      "hours" },
	{ "message.minute",     "minute" },
	{ "message.bindPluName", "BIND PLU name:" },
	{ "message.buildDisabled",	"disabled" },
	{ "message.buildEnabled",	"enabled" },
	{ "message.buildOpts",	"Build options:" },
	{ "message.byte",       "byte" },
	{ "message.bytes",      "bytes" },
	{ "message.characterSet",       "EBCDIC character set:" },
	{ "message.charMode",   "NVT character mode" },
	{ "message.columns",    "columns" },
	{ "message.connectedTo",        "Connected to:" },
	{ "message.connectionPending",  "Connection pending to:" },
	{ "message.dbcsCgcsgid",	"Host DBCS CGCSGID:" },
	{ "message.defaultCharacterSet",        "Default (us) EBCDIC character set" },
	{ "message.dsMode",     "3270 mode" },
	{ "message.extendedDs", "extended data stream" },
	{ "message.fullColor",  "color" },
	{ "message.hostCodePage", "Host code page:" },
	{ "message.keyboardMap",        "Keyboard map:" },
	{ "message.lineMode",   "NVT line mode" },
	{ "message.localeCodeset",	"Locale codeset:" },
	{ "message.luName",     "LU name:" },
	{ "message.minute",     "minute" },
	{ "message.minutes",    "minutes" },
	{ "message.model",      "Model" },
	{ "message.mono",       "monochrome" },
	{ "message.notConnected",       "Not connected" },
	{ "message.port",       "Port:" },
	{ "message.proxyType",  "Proxy type:" },
	{ "message.Received",   "Received" },
	{ "message.received",   "received" },
	{ "message.record",     "record" },
	{ "message.records",    "records" },
	{ "message.rows",       "rows" },
	{ "message.sbcsCgcsgid",	"Host SBCS CGCSGID:" },
	{ "message.second",     "second" },
	{ "message.seconds",    "seconds" },
	{ "message.secure",     "via TLS/SSL" },
	{ "message.sent",       "Sent" },
	{ "message.server",     "Server:" },
	{ "message.specialCharacters",  "Special characters:" },
	{ "message.sscpMode",   "SSCP-LU mode" },
	{ "message.standardDs", "standard data stream" },
	{ "message.terminalName",       "Terminal name:" },
	{ "message.tn3270eNoOpts",      "No TN3270E options" },
	{ "message.tn3270eOpts",        "TN3270E options:" },
#if defined(_WIN32) /*[*/
	{ "message.windowsCodePage",	"Windows code page:" },
#endif /*][*/
	{ NULL, NULL }
};
#endif /*]*/

static struct dresource {
	struct dresource *next;
	const char *name;
	char *value;
} *drdb = NULL, **drdb_next = &drdb;

void
add_resource(const char *name, char *value)
{
	struct dresource *d;

	for (d = drdb; d != NULL; d = d->next) {
		if (!strcmp(d->name, name)) {
			d->value = value;
			return;
		}
	}
	d = Malloc(sizeof(struct dresource));
	d->next = NULL;
	d->name = name;
	d->value = value;
	*drdb_next = d;
	drdb_next = &d->next;
}

char *
get_resource(const char *name)
{
	struct dresource *d;
	int i;

	for (d = drdb; d != NULL; d = d->next) {
		if (!strcmp(d->name, name)) {
			return d->value;
		}
	}

	for (i = 0; fallbacks[i] != NULL; i++) {
		if (!strncmp(fallbacks[i], name, strlen(name)) &&
		    *(fallbacks[i] + strlen(name)) == ':') {
			return fallbacks[i] + strlen(name) + 2;
		}
	}
#if defined(C3270) /*[*/
	for (i = 0; rdb[i].name != (char *)NULL; i++) {
		if (!strcmp(rdb[i].name, name)) {
			return rdb[i].value;
		}
	}
#endif /*]*/
	return NULL;
}
