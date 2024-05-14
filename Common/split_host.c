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
 *	split_host.c
 *		Host name parsing.
 */

#include "globals.h"

#include "split_host.h"
#include "utils.h"

static const char *pfxstr = "AaCcLlNnPpSsBbYyTt";

/**
 * Return the set of host prefixes.
 *
 * @returns Set of host prefixes.
 */
const char *
host_prefixes(void)
{
    static char *ret = NULL;
    size_t sl;
    char *r;
    size_t i;

    if (ret != NULL) {
	return ret;
    }

    sl = strlen(pfxstr);
    r = ret = Malloc((sl / 2) + 1);
    for (i = 0; i < sl; i += 2) {
	*r++ = pfxstr[i];
    }
    *r = '\0';
    return ret;
}

/**
 * Hostname parser.
 *  [prefix:...][lu@]hostname[:port][=accept]
 * Backslashes to quote anything (including backslashes).
 * [ ] quotes : and @, e.g., [1:2::3] to quote an IPv6 numeric hostname.
 *
 * @param[in] raw	Raw hostname, with possible decorations
 * @param[out] lu	Returned Malloc'd LU name, or NULL
 * @param[out] host	Returned Malloc'd hostname, isolated from other parts
 * @param[out] port	Returned Malloc'd port, or NULL
 * @param[out] accept	Returned Malloc'd accept hostname, or NULL
 * @param[out] prefixes	Returned bitmap of prefixes, indexed by ACLNPSBYT (bit
 * 			 0 is A, bit 1 is C, bit 2 is L, etc.)
 * @param[out] error	Returned error text for failure, or NULL
 *
 * @return true for success, false for syntax error.
 */
bool
new_split_host(char *raw, char **lu, char **host, char **port, char **accept,
	unsigned *prefixes, char **error)
{
    char   *start     = raw;
    size_t  sl        = strlen(raw);
    char   *s;
    char   *uq        = NULL;
    int     uq_len    = 0;
    char   *qmap      = NULL;
    char   *rqmap;
    char   *skip_map  = NULL;
    char   *errmsg    = "nonspecific";
    bool rc        = false;
    bool quoted    = false;
    int     bracketed = 0;
    int     n_ch      = 0;
    int     n_at      = 0;
    int     n_colon   = 0;
    int     n_equal   = 0;
    char   *part[4]   = { NULL, NULL, NULL, NULL };
    int     part_ix   = 0;
    char   *pfx;

    *lu       = NULL;
    *host     = NULL;
    *port     = NULL;
    *accept   = NULL;
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
    skip_map = Malloc(sl + 1);
    memset(skip_map, ' ', sl);
    skip_map[sl] = '\0';
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
     * Now check for syntax: [LUname@]hostname[:port][=accept]
     * So more than one @, more than one :, : before @, or no text before @
     * or :, or no text after : are all syntax errors.
     * So is more than one = and no text after =.
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
		if (n_equal > 0) {
		    errmsg = "'@' after '='";
		    goto done;
		}
		if (n_at > 0) {
		    errmsg = "double '@'";
		    goto done;
		}
		n_at++;
		n_ch = 0;

		/* Trim off prefixes. */
		while (rqmap[s + 1 - start] == ' ' &&
		    rqmap[s + 2 - start] == ' ' &&
		    (pfx = strchr(pfxstr, *(s + 1))) != NULL &&
		    *(s + 2) == ':') {

		    *prefixes |= 1 << ((pfx - pfxstr) / 2);
		    skip_map[s + 1 - start] = '+';
		    skip_map[s + 2 - start] = '+';
		    s += 2;
		}

	    } else if (*s == ':') {
		if (n_colon > 0) {
		    errmsg = "double ':'";
		    goto done;
		}
		if (n_ch == 0) {
		    errmsg = "empty hostname";
		    goto done;
		}
		if (n_equal > 0) {
		    errmsg = "':' after '='";
		    goto done;
		}
		n_colon++;
		n_ch = 0;
	    } else if (*s == '=') {
		if (n_equal > 0) {
		    errmsg = "double '='";
		    goto done;
		}
		if (n_ch == 0) {
		    errmsg = "empty accept name";
		    goto done;
		}
		n_equal++;
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
	if (n_equal) {
	    errmsg = "empty accept name";
	} else if (n_colon) {
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
    if (n_equal) {
	*accept = Malloc(uq_len + 1);
	part[3] = *accept;
    }
    s = start;
    n_ch = 0;
    while (*s) {
	if (skip_map[s - start] == ' ') {
	    if (rqmap[s - start] == ' ' &&
		    (*s == '@' || *s == ':' || *s == '=')) {
		part[part_ix][n_ch] = '\0';
		part_ix++;
		n_ch = 0;
	    } else {
		while (part[part_ix] == NULL) {
		    part_ix++;
		}
		part[part_ix][n_ch++] = *s;
	    }
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
    if (skip_map != NULL) {
	Free(skip_map);
    }
    if (!rc) {
	*error = Asprintf("Hostname syntax error in '%s': %s", raw, errmsg);
    }
    return rc;
}
