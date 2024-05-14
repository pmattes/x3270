/*
 * Copyright (c) 2017-2024 Paul Mattes.
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
 *	sioc.c
 *		Common back-end logic used by secure I/O providers,
 *		plus common internal logic that depends on that.
 */

#include "globals.h"

#include <errno.h>
#include <stdarg.h>
#include <assert.h>

#include "utils.h"
#include "tls_config.h"

#include "sio.h"
#include "varbuf.h"	/* must precede sioc.h */
#include "sioc.h"

#define READ_BUF	1024

#define STRING_PASSWD	"string:"
#define FILE_PASSWD	"file:"

/* Typedefs */

/* Statics */
static struct {
    const char *name1;
    const char *name2;
    const char *name3;
    int protocol;
} protos[] = {
    { "SSL2", "SSL2.0", "SSL2_0", SIP_SSL2 },
    { "SSL3", "SSL3.0", "SSL3_0", SIP_SSL3 },
    { "TLS1", "TLS1.0", "TLS1_0", SIP_TLS1 },
    { "TLS1.1", "TLS1_1", NULL, SIP_TLS1_1 },
    { "TLS1.2", "TLS1_2", NULL, SIP_TLS1_2 },
    { "TLS1.3", "TLS1_3", NULL, SIP_TLS1_3 },
};
static int max_proto = (int)array_count(protos) - 1;

/* Globals */
static char *sioc_last_error;

/* Record an error. */
void
sioc_set_error(const char *fmt, ...)
{
    va_list args;
    char *t, *u;

    va_start(args, fmt);
    t = Vasprintf(fmt, args);
    va_end(args);

    u = Asprintf("TLS: %s", t);
    Free(t);
    Replace(sioc_last_error, u);
}

/* Clear the last error. */
void
sioc_error_reset(void)
{
    Replace(sioc_last_error, NULL);
}

/*
 * Returns the last error as text.
 */
const char *
sio_last_error(void)
{
    return ((sioc_last_error != NULL)? sioc_last_error: "TLS: No error");
}

/* Expand the contents of a file into a string. */
char *
sioc_string_from_file(const char *path, size_t *len_ret)
{
    FILE *f;
    char *accum = NULL;
    size_t n_accum = 0;

    f = fopen(path, "r");
    if (f == NULL) {
	sioc_set_error("%s: %s", path, strerror(errno));
	*len_ret = 0;
	return NULL;
    }
    while (true) {
	size_t nr;

	accum = Realloc(accum, n_accum + READ_BUF);
	nr = fread(accum + n_accum, 1, READ_BUF, f);
	n_accum += nr;
	if (nr < READ_BUF) {
	    break;
	}
    }

    /* Null-terminate the string, but don't count that in the length. */
    accum = Realloc(accum, n_accum + 1);
    accum[n_accum] = '\0';

    fclose(f);
    *len_ret = n_accum;
    return accum;
}

/* Parse a password spec. */
char *
sioc_parse_password_spec(const char *spec)
{
    if (!strncasecmp(spec, STRING_PASSWD, strlen(STRING_PASSWD))) {
	/* string:xxx */
	return NewString(spec + strlen(STRING_PASSWD));
    }
    if (!strncasecmp(spec, FILE_PASSWD, strlen(FILE_PASSWD))) {
	size_t len;
	char *password;

	/* file:xxx */
	password = sioc_string_from_file(spec + strlen(FILE_PASSWD), &len);
	if (password == NULL) {
	    return NULL;
	}
	if (len > 0 && password[len - 1] == '\n') {
	    password[--len] = '\0';
	}
	if (len > 0 && password[len - 1] == '\r') {
	    password[--len] = '\0';
	}
	if (len == 0) {
	    sioc_set_error("Empty password file");
	    Free(password);
	    return NULL;
	}
	return password;
    }

    /* No qualifier, assume direct value */
    return (NewString(spec));
}

/*
 * Parse a protocol number.
 */
static int
parse_protocol(const char *protocol)
{
    int i;

    for (i = 0; i <= max_proto; i++) {
	if (!strcasecmp(protocol, protos[i].name1) ||
		(protos[i].name2 != NULL && !strcasecmp(protocol, protos[i].name2)) ||
		(protos[i].name3 != NULL && !strcasecmp(protocol, protos[i].name3))) {
	    return protos[i].protocol;
	}
    }
    return -1;
}

/*
 * Produce an error string for an invalid protocol version.
 */
static char *
invalid_version_string(const char *str, const char *which, int rmin, int rmax)
{
    varbuf_t r;
    int i;

    vb_init(&r);
    vb_appendf(&r, "Invalid %s protocol '%s'\nValid protocols are", which, str);
    for (i = ((rmin >= 0)? rmin: 0); i <= ((rmax >= 0)? rmax: max_proto); i++) {
	vb_appendf(&r, " %s", protos[i].name1);
    }
    return vb_consume(&r);
}

/*
 * Parse a set of min/max TLS protocol versions.
 *
 * @param[in] minstr	Specified minimum protocol string, or NULL
 * @param[in] maxstr	Specified minimum protocol string, or NULL
 * @param[in] rmin	Implementation-defined minimum version or -1
 * @param[in] rmax	Implementation-defined maximum version or -1
 * @param[out] minp	Returned numeric minimum protocol, or -1
 * @param[out] maxp	Returned numeric maximum protocol, or -1
 *
 * @returns error string, or NULL if sucesful
 */
char *
sioc_parse_protocol_min_max(const char *minstr, const char *maxstr, int rmin, int rmax, int *minp, int *maxp)
{
    int min = -1, max = -1;

    if (rmin >= 0) {
	assert(rmin <= max_proto);
    }
    if (rmax >= 0) {
	assert(rmax <= max_proto);
    }
    if (rmin >=0 && rmax >= 0) {
	assert(rmin <= rmax);
    }

    if (minstr != NULL) {
	min = parse_protocol(minstr);
	if (min < 0 || (rmin >= 0 && min < rmin) || (rmax >= 0 && min > rmax)) {
	    return invalid_version_string(minstr, "minimum", rmin, rmax);
	}
    }
    if (maxstr != NULL) {
	max = parse_protocol(maxstr);
	if (max < 0 || (rmin >= 0 && max < rmin) || (rmax >= 0 && max > rmax)) {
	    return invalid_version_string(maxstr, "maximum", rmin, rmax);
	}
    }
    if (max >= 0 && min >= 0 && min > max) {
	return NewString("Minimum protocol > maximum protocol");
    }
    *minp = min;
    *maxp = max;
    return NULL;
}

/*
 * Report all supported TLS-related options.
 */
unsigned
sio_all_options_supported(void)
{
    if (sio_supported()) {
	return TLS_REQUIRED_OPTS | sio_options_supported();
    } else {
	return 0;
    }
}

/*
 * Add a string to a list of subjects.
 */
void
sioc_subject_add(char ***subjects, char *s, ssize_t len)
{
    int count = 0;

    /* Patch up the length. */
    if (len == (ssize_t)-1) {
	len = strlen(s);
    }

    /* See if we really need to add anything. */
    if (*subjects != (char **)NULL) {
	int i;

	for (i = 0; (*subjects)[i] != NULL; i++) {
	    if (!strncmp((*subjects)[i], s, len) &&
			(*subjects)[i][len] == '\0') {
		return;
	    }
	}
    }

    if (*subjects != (char **)NULL) {
	while ((*subjects)[count] != NULL) {
	    count++;
	}
    }
    *subjects = (char **)Realloc(*subjects, (count + 2) * sizeof(char *));
    (*subjects)[count] = Malloc(len + 1);
    strncpy((*subjects)[count], s, len);
    (*subjects)[count][len] = '\0';
    (*subjects)[count + 1] = NULL;
}

/*
 * Dump a list of subjects into a varbuf and free the list.
 */
void
sioc_subject_print(varbuf_t *v, char ***subjects)
{
    int i;

    if (*subjects == (char **)NULL) {
	return;
    }

    for (i = 0; (*subjects)[i] != NULL; i++) {
	vb_appendf(v, "%s\n", (*subjects)[i]);
	Free((*subjects)[i]);
    }
    Free(*subjects);
    *subjects = NULL;
}
