/*
 * Copyright 1999, 2000, 2001, 2005, 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
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
