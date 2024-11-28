/*
 * Copyright (c) 2024 Paul Mattes.
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
 *	uri.c
 *		URI parsing.
 */

#include "globals.h"

#include "httpd-core.h"
#include "split_host.h"
#include "uri.h"
#include "utils.h"

#define SCHEME_TELNET	"telnet"	/* RFC 4248 */
#define SCHEME_TELNETS	"telnets"	/* Extension to RFC 4248 */
#define SCHEME_TN3270	"tn3270"	/* RFC 6270 */
#define SCHEME_TN3270S	"tn3270s"	/* Extension to RFC 6270 */

static struct {
    const char *name;
    int port;
    unsigned prefixes;

} schemes[] = {
    { SCHEME_TELNET, 23, 1 << ANSI_HOST },
    { SCHEME_TELNETS, 992, 1 << ANSI_HOST | 1 << TLS_HOST },
    { SCHEME_TN3270, 23, 0 },
    { SCHEME_TN3270S, 992, 1 << TLS_HOST },
    { NULL, 0 }
};

#if defined(_WIN32) /*[*/
static char *
strcasestr(const char *haystack, const char *needle)
{
    const char *s = haystack;

    if (!*haystack) {
	return (*needle)? NULL: (char *)haystack;
    }

    while (*s) {
	if (!strncasecmp(needle, s, strlen(needle))) {
	    return (char *)s;
	}
	s++;
    }
    return NULL;
}
#endif /*]*/

/**
 * Check a query for a match.
 *
 * @param[in] query	Query string.
 * @param[in] keyword	Keyword to look for.
 * @param[out] value	Returned Malloc'd value, or NULL.
 */
static void
check_query(const char *query, const char *keyword, char **value)
{
    char *eq = Asprintf("%s=", keyword);
    char *match = strcasestr(query, eq);
    char *start;
    char *question;

    *value = NULL;
    if (match == NULL) {
	goto done;
    }
    start = match + strlen(eq);
    question = strchr(start, '?');
    if (question != NULL) {
	*value = Asprintf("%.*s", (int)(question - start), start);
    } else {
	*value = NewString(start);
    }

done:
    Free(eq);
}

/**
 * Check a query for all supported matches.
 * @param[in] query		Query string.
 * @param[out] lu		Returned LU name.
 * @param[out] accept		Returned accept hostname.
 * @param[in,out] prefixes	Modified prefix flags.
 */
static void
check_queries(const char *query, char **lu, char **accept, unsigned *prefixes)
{
    char *wait;
    char *verify;

    check_query(query, "lu", lu);
    check_query(query, "accepthostname", accept);
    check_query(query, "waitoutput", &wait);
    if (wait != NULL) {
	if (!strcasecmp(wait, "false")) {
	    *prefixes |= 1 << NO_LOGIN_HOST;
	}
	Free(wait);
    }
    check_query(query, "verifyhostcert", &verify);
    if (verify != NULL) {
	if (!strcasecmp(verify, "false")) {
	    *prefixes |= 1 << NO_VERIFY_CERT_HOST;
	}
	Free(verify);
    }
}

/**
 * Authority parser.
 *
 * @param[in] authority	Authority to parse
 * @param[out] host	Returned Malloc'd hostname, isolated from other parts
 * @param[out] port	Returned Malloc'd port, or NULL
 * @param[out] username	Returned Malloc'd username, or NULL
 * @param[out] password	Returned Malloc'd password, or NULL
 * @param[out] error	Returned error text for failure, or NULL
 *
 * @return true for success, false for no match.
 */
bool
parse_authority(const char *authority, char **host, char **port, char **username,
	char **password, const char **error)
{
    const char *at;
    char *user_pass = NULL;
    char *xhost = NULL;

    /* @ wins over everything. */
    at = strchr(authority, '@');
    if (at != NULL) {
	user_pass = Asprintf("%.*s", (int)(at - authority), authority);
	xhost = NewString(at + 1);
    } else {
	user_pass = NULL;
	xhost = NewString(authority);
    }

    /* Now look for a password. */
    if (user_pass != NULL) {
	char *colon = strchr(user_pass, ':');

	if (colon != NULL) {
	    *username = percent_decode(user_pass, colon - user_pass, false);
	    *password = percent_decode(colon + 1, strlen(colon + 1), false);
	} else {
	    *username = percent_decode(user_pass, strlen(user_pass), false);
	}
    }

    /* Pick apart the host. */
    if (xhost[0] == '[') {
	/* IPv6. */
	char *right_bracket = strchr(xhost, ']');

	if (right_bracket == NULL) {
	    *error = "Missing IPv6 ']'";
	    goto fail;
	}
	*host = percent_decode(xhost + 1, right_bracket - xhost - 1, false);
	if (!*host[0] || strspn(*host, ":0123456789abcdefABCDEF") != strlen(*host)) {
	    *error = "Invalid IPv6 address";
	    goto fail;
	}
	switch (*(right_bracket + 1)) {
	case ':':
	    /* Port follows. */
	    *port = percent_decode(right_bracket + 2, strlen(right_bracket + 2), false);
	    break;
	case '\0':
	    break;
	default:
	    *error = "Invalid syntax after ']'";
	    goto fail;
	}
    } else {
	/* Normal. */
	char *colon = strchr(xhost, ':');

	if (colon != NULL) {
	    *host = percent_decode(xhost, colon - xhost, false);
	    *port = percent_decode(colon + 1, strlen(colon + 1), false);
	} else {
	    *host = percent_decode(xhost, strlen(xhost), false);
	}
    }

    /* Check the port. */
    if (*port != NULL &&
	    (!*port[0] ||
	     strspn(*port, "0123456789") != strlen(*port) ||
	     (strtoul(*port, NULL, 10) & ~0xffffUL) != 0)) {
	*error = "Invalid port";
	goto fail;
    }

    Replace(xhost, NULL);
    Replace(user_pass, NULL);
    return true;

fail:
    Replace(xhost, NULL);
    Replace(user_pass, NULL);
    Replace(*host, NULL);
    Replace(*port, NULL);
    Replace(*username, NULL);
    Replace(*password, NULL);
    return false;
}

/**
 * URI parser.
 *
 * @param[in] uri	URI to parse
 * @param[out] scheme	Returned Malloc'd scheme
 * @param[out] username	Returned Malloc'd username, or NULL
 * @param[out] password	Returned Malloc'd password, or NULL
 * @param[out] host	Returned Malloc'd hostname
 * @param[out] port	Returned Malloc'd port, or NULL
 * @param[out] path	Returned Malloc'd path, or NULL
 * @param[out] query	Returned Malloc'd query, or NULL
 * @param[out] fragment	Returned Malloc'd fragment, or NULL
 * @param[out] error	Returned error text for failure, or NULL
 *
 * @return true for success, false for no match.
 */
bool
parse_uri(const char *uri, char **scheme, char **username, char **password, char **host,
	char **port, char **path, char **query, char **fragment, const char **error)
{
    char *percent;
    char *authority_start;
    char *slash;
    char *question;
    char *hash;
    char *authority = NULL;
    char *end;

    *scheme = NULL;
    *username = NULL;
    *password = NULL;
    *host = NULL;
    *port = NULL;
    *path = NULL;
    *query = NULL;
    *fragment = NULL;
    *error = NULL;

    /* Check for percent decode errors once, so we don't need to check again. */
    percent = percent_decode(uri, strlen(uri), false);
    if (percent == NULL) {
	*error = "Percent error";
	return false;
    }
    Free(percent);

    /* Find the scheme. */
    end = strstr(uri, "://");
    if (end == NULL) {
	*error = "Missing scheme";
	goto fail;
    }
    *scheme = percent_decode(uri, end - uri, false);

    /* Find / ? # in order. */
    authority_start = end + strlen("://");
    slash = strchr(authority_start, '/');
    if (slash != NULL) {
	question = strchr(slash + 1, '?');
    } else {
	question = strchr(authority_start, '?');
    }
    if (question != NULL) {
	hash = strchr(question + 1, '#');
    } else if (slash != NULL) {
	hash = strchr(slash + 1, '#');
    } else {
	hash = strchr(authority_start, '#');
    }

    /* Isolate the authority. */
    if (slash != NULL) {
	end = slash;
    } else if (question != NULL) {
	end = question;
    } else if (hash != NULL) {
	end = hash;
    } else {
	end = strchr(authority_start, '\0');
    }
    if (end == authority_start) {
	*error = "Missing authority";
	goto fail;
    }
    authority = Asprintf("%.*s", (int)(end - authority_start), authority_start);

    /* Parse the authority. */
    if (!parse_authority(authority, host, port, username, password, error)) {
	goto fail;
    }
    Replace(authority, NULL);

    /* Isolate the path. */
    if (slash != NULL) {
	if (question != NULL) {
	    end = question;
	} else if (hash != NULL) {
	    end = hash;
	} else {
	    end = strchr(slash, '\0');
	}
	*path = percent_decode(slash, end - slash, false);
    }

    /* Isolate the query. */
    if (question != NULL) {
	if (hash != NULL) {
	    end = hash;
	} else {
	    end = strchr(question, '\0');
	}
	*query = percent_decode(question + 1, end - (question + 1), false);
    }

    /* Finally the fragement. */
    if (hash != NULL) {
	*fragment = percent_decode(hash + 1, strlen(hash) - 1, false);
    }

    return true;

fail:
    Replace(*scheme, NULL);
    Replace(*username, NULL);
    Replace(*password, NULL);
    Replace(*host, NULL);
    Replace(*port, NULL);
    Replace(*path, NULL);
    Replace(*query, NULL);
    Replace(*fragment, NULL);
    Replace(authority, NULL);
    return false;
}

/**
 * x3270 URI parser.
 *
 * @param[in] uri	URI to parse
 * @param[out] host	Returned Malloc'd hostname, isolated from other parts
 * @param[out] port	Returned Malloc'd port, or NULL
 * @param[out] prefixes	Returned bitmap of prefixes, indexed by ACLNPSBYT (bit
 * 			 0 is A, bit 1 is C, bit 2 is L, etc.)
 * @param[out] username	Returned Malloc'd username, or NULL
 * @param[out] password	Returned Malloc'd password, or NULL
 * @param[out] lu	Returned Malloc'd LU name list, or NULL
 * @param[out] accept	Returned accept host name, or NULL
 * @param[out] error	Returned error text for failure, or NULL
 *
 * @return true for success, false for no match.
 *
 * @note The protocol is returned indiectly through the prefixes. E.g., secure connection is
 *	returned via L: and TELNET (not TN3270E) is returned via A:.
 */
bool
parse_x3270_uri(const char *uri, char **host, char **port, unsigned *prefixes, char **username,
	char **password, char **lu, char **accept, const char **error)
{
    char *scheme = NULL;
    char *path = NULL;
    char *query = NULL;
    char *fragment = NULL;
    int i;

    *host = NULL;
    *port = NULL;
    *prefixes = 0;
    *username = NULL;
    *password = NULL;
    *lu = NULL;
    *accept = NULL;
    *error = NULL;

    if (!parse_uri(uri, &scheme, username, password, host, port, &path, &query, &fragment, error)) {
	return false;
    }

    /* Validate there is no path and no fragment. */
    if (path != NULL && strlen(path) > 1) {
	*error = "Invalid path";
	goto fail;
    }
    if (fragment != NULL && strlen(fragment) > 0) {
	*error = "Invalid fragment";
	goto fail;
    }

    /* Translate the scheme. */
    for (i = 0; schemes[i].name != NULL; i++) {
	if (!strcasecmp(scheme, schemes[i].name)) {
	    break;
	}
    }
    if (schemes[i].name == NULL) {
	*error = "Unsupported URI scheme";
	goto fail;
    }
    *prefixes = schemes[i].prefixes;

    /* Parse the query. */
    if (query != NULL) {
	check_queries(query, lu, accept, prefixes);
    }

    if (*port == NULL && schemes[i].port != 0) {
	*port = Asprintf("%d", schemes[i].port);
    }

    Replace(scheme, NULL);
    Replace(path, NULL);
    Replace(query, NULL);
    Replace(fragment, NULL);

    return true;

fail:
    Replace(scheme, NULL);
    Replace(path, NULL);
    Replace(query, NULL);
    Replace(fragment, NULL);

    Replace(*host, NULL);
    Replace(*port, NULL);
    *prefixes = 0;
    Replace(*username, NULL);
    Replace(*password, NULL);
    Replace(*lu, NULL);
    Replace(*accept, NULL);

    return false;
}

/**
 * Test a candidate URI for being an x3270 URI.
 * @param[in] uri	Candidate URI
 *
 * @returns true if a likely x3270 URI.
 */
bool
is_x3270_uri(const char *uri)
{
    int i;

    for (i = 0; schemes[i].name != NULL; i++) {
	if (!strncmp(uri, schemes[i].name, strlen(schemes[i].name)) &&
		!strncmp(uri + strlen(schemes[i].name), "://", 3)) {
	    return true;
	}
    }
    return false;
}
