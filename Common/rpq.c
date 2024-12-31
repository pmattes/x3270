/*
 * Copyright (c) 2005-2024 Paul Mattes.
 * Copyright (c) 2004-2005, Don Russell.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC nor
 *       the names of their contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DON RUSSELL "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DON RUSSELL BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	rpq.c
 *		RPQNAMES structured field support.
 *
 */

#include "globals.h"
#include <errno.h>
#if !defined(_WIN32) /*[*/
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#endif /*]*/
#include <assert.h>
#include "3270ds.h"

#include "appres.h"
#include "ctlrc.h"
#include "popups.h"
#include "resources.h"
#include "sf.h"	 /* has to come before rpq.h */
#include "rpq.h"
#include "telnet.h"
#include "telnet_core.h"
#include "trace.h"
#include "unicodec.h"
#include "toggles.h"
#include "utils.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

/*
 * Define symbolic names for RPQ self-defining terms.
 * (Numbering is arbitrary, but must be 0-255 inclusive.
 * Do not renumber existing items because these identify the
 * self-defining term to the mainframe software. Changing pre-existing
 * values will possibly impact host based software.
 */
typedef enum {
    RPQ_ADDRESS = 0,
    RPQ_TIMESTAMP = 1,
    RPQ_TIMEZONE = 2,
    RPQ_USER = 3,
    RPQ_VERSION = 4,
    RPQ_NUM_TERMS = 5,
} rpq_id_t;

#define RPQ_ADDRESS_NAME	"ADDRESS"
#define RPQ_TIMESTAMP_NAME	"TIMESTAMP"
#define RPQ_TIMEZONE_NAME	"TIMEZONE"
#define RPQ_USER_NAME		"USER"
#define RPQ_VERSION_NAME	"VERSION"

#define RPQ_ALL			"ALL"
#define RPQ_NO			"NO"

/* Error code return by individual term get functions. */
typedef enum {
    TR_SUCCESS,		/* successful generation */
    TR_OMIT,		/* term intentionally omitted */
    TR_NOSPACE,		/* insufficient space to store term */
    TR_ERROR,		/* other error generating term */
} term_result_t;
typedef term_result_t get_term_fn(unsigned char *buf, const size_t buflen, size_t *len);

/* Statics */
static struct rpq_keyword *find_kw(rpq_id_t id);
static void select_rpq_terms(void);
static get_term_fn get_rpq_address, get_rpq_timestamp, get_rpq_timezone, get_rpq_user, get_rpq_version;
static void rpq_warning(const char *fmt, ...);
static void rpq_init_warnings(void);
static void rpq_dump_warnings(void);
static char *rpq_warnbuf = NULL;
static char *rpq_warnbuf_prev = NULL;

/*
 * Define a table of RPQ self-defing terms. 
 * NOTE: Synonyms could be specified by coding different text items but using
 * the same "id" value.
 * Items should be listed in alphabetical order by "text" name so if the user
 * specifies abbreviations, they work in a predictable manner.  E.g., "TIME"
 * should match TIMESTAMP instead of TIMEZONE.
 */
static struct rpq_keyword {
    bool omit;
    size_t override_offset;
    const bool allow_override;
    const rpq_id_t id;
    const char *text;
    get_term_fn *get;
} rpq_keywords[] = {
    { true, 0, 	true,	RPQ_ADDRESS,	RPQ_ADDRESS_NAME,	get_rpq_address },
    { true, 0, 	false,	RPQ_TIMESTAMP,	RPQ_TIMESTAMP_NAME,	get_rpq_timestamp },
    { true, 0, 	true,	RPQ_TIMEZONE,	RPQ_TIMEZONE_NAME,	get_rpq_timezone },
    { true, 0, 	true,	RPQ_USER,	RPQ_USER_NAME,		get_rpq_user },
    { true, 0, 	false,	RPQ_VERSION,	RPQ_VERSION_NAME,	get_rpq_version },
};
#define NS_RPQ (sizeof(rpq_keywords)/sizeof(rpq_keywords[0]))

static char *rpq_spec;

/*
 * RPQNAMES query reply.
 */
void
do_qr_rpqnames(void)
{
#   define TERM_PREFIX_SIZE 2	/* Each term has 1 byte length and 1 byte id */
    rpq_id_t id;
    ssize_t nw;
    enum me_fail error;
    bool truncated = false;
    unsigned char *rpql;
    size_t remaining = 254;	/* maximum data area for rpqname reply */
    static const char x3270name[] = "x3270";
#   define X3270_NAMESIZE (sizeof(x3270name) - 1)

    trace_ds("> QueryReply(RPQNames");

    /* Start with a fresh warning buffer. */
    rpq_init_warnings();

    /*
     * Allocate enough space for the maximum allowed item.
     * By pre-allocating the space I don't have to worry about the
     * possibility of addresses changing.
     */
    space3270out(4 + 4 + 1 + remaining);/* Maximum space for an RPQNAME item */
    SET32(obptr, 0);			/* Device number, 0 = All */
    SET32(obptr, 0);			/* Model number, 0 = All */
    rpql = obptr++;			/* Save address to place data length. */

    /*
     * Create fixed length portion - program id: x3270
     */
    nw = multibyte_to_ebcdic_string(x3270name, X3270_NAMESIZE, obptr, X3270_NAMESIZE, &error, &truncated);
    assert(nw == X3270_NAMESIZE);
    obptr += nw;
    remaining -= nw;

    /* Create user selected variable-length self-defining terms. */
    select_rpq_terms();
    trace_ds(" '%s' -> 0 0 %s", rpq_spec? rpq_spec: "", x3270name);

    /*
     * Note: We emit terms in identifier order, so the output is deterministic (including terms omitted
     * due to space constraints) even when new term types are added later.
     *
     * The keyword table is sorted alphabetically to preserve abbreviation semantics, so when a new term
     * type is added later, the table will no longer be sorted by identifier. So we need to walk by identifier
     * and search for the right slot at each iteration.
     */
    char *sep = " ";
    for (id = 0; id < RPQ_NUM_TERMS; id++) {
	struct rpq_keyword *kw = find_kw(id);
	bool omit_due_space_limit = false;

	if (kw->omit) {
	    continue;
	}

	omit_due_space_limit = remaining < TERM_PREFIX_SIZE;
	if (!omit_due_space_limit) {
	    size_t term_len = 0;
	    term_result_t term_result;

	    term_result = kw->get(obptr + TERM_PREFIX_SIZE, remaining - TERM_PREFIX_SIZE, &term_len);
	    if (term_result == TR_SUCCESS) {
		trace_ds("%s%s%s", sep, kw->text, kw->override_offset? "=": "");
		sep = ",";
		*obptr++ = (unsigned char)(TERM_PREFIX_SIZE + term_len); /* length of term */
		*obptr++ = kw->id;				/* term ID */
		obptr += term_len;				/* jump over term contents */
		remaining -= TERM_PREFIX_SIZE + term_len;	/* account for space taken */
	    } else {
		/* Failed, check for overflow, which will cause error output. */
		omit_due_space_limit = (term_result == TR_NOSPACE);
	    }
	}

	if (omit_due_space_limit) {
	    rpq_warning("RPQ %s term omitted due to insufficient space", kw->text);
	}
    }

    /* Fill in overall length of RPQNAME info */
    *rpql = (unsigned char)(obptr - rpql);

    trace_ds(")\n");
    rpq_dump_warnings();
}

/* Selects which terms will be returned in RPQNAMES. */
static void
select_rpq_terms(void)
{
    size_t i;
    unsigned j, k;
    char *spec_copy;
    char *s;
    char *kw;
    bool is_no_form;

    /* Reinitialize. */
    for (j = 0; j < NS_RPQ; j++) {
	rpq_keywords[j].omit = true;
	rpq_keywords[j].override_offset = 0;
    }

    /* See if the user wants any rpqname self-defining terms returned. */
    if (appres.rpq != NULL) {
	rpq_spec = appres.rpq;
    } else if ((rpq_spec = getenv("X3270RPQ")) == NULL) {
	return;
    }
    for (s = rpq_spec; *s && isspace((unsigned char)*s); s++) {
    }
    if (!*s) {
	rpq_spec = NULL;
	return;
    }

    /* Make a copy of the user selections so the fields can be NUL terminated. */
    spec_copy = NewString(rpq_spec);

    for (i = 0; i < strlen(rpq_spec); ) {
	size_t len;

	kw = spec_copy + i;
	i++;
	if (isspace((unsigned char)*kw)) {
	    continue;	/* skip leading white space */
	}
	if (*kw == ':') {
	    continue;
	}

	/* : separates terms, but \: is literal : */
	s = kw;
	do {
	    s = strchr(s + 1,':');
	    if (s == NULL) {
		break;
	    }
	} while (*(s - 1) == '\\');
	/* s points to the : separating a term, or is NULL */
	if (s != NULL) {
	    *s = '\0';
	}
	/* kw is now a string of the entire, single term. */

	i = (kw - spec_copy) + strlen(kw) + 1;
	/* It might be a keyword=value item... */

	for (s = kw; *s; s++) {
	    if (!isalpha((unsigned char)*s)) {
		break;
	    }
	}
	len = s - kw; 
	is_no_form = len > 2 && !strncasecmp(RPQ_NO, kw, strlen(RPQ_NO));
	if (is_no_form) {
	    kw += strlen(RPQ_NO);	/* skip "NO" prefix for matching keyword */
	    len -= strlen(RPQ_NO);	/* adjust keyword length */
	}

	for (j = 0; j < NS_RPQ; j++) {
	    if (len > 0 && !strncasecmp(kw, rpq_keywords[j].text, len)) {
		while (*s && isspace((unsigned char)*s)) {
		    s++;
		}
		if (*s == '=') {
		    if (rpq_keywords[j].allow_override && !is_no_form) {
			rpq_keywords[j].override_offset = s - spec_copy + 1;
		    } else {
			rpq_warning("RPQ %s term override ignored", rpq_keywords[j].text);
		    }
		} else if (*s != '\0' && *s != ':') {
		    rpq_warning("RPQ syntax error after \"%.*s\"", (int)len, kw);
		    break;
		}
		rpq_keywords[j].omit = is_no_form;
		break;
	    }
	}

	if (j >= NS_RPQ) {
	    /* unrecognized keyword... */
	    if (!strcasecmp(kw, RPQ_ALL)) {
		for (k = 0; k < NS_RPQ; k++) {
		    rpq_keywords[k].omit = is_no_form;
		}
	    } else if (len == 0) {
		rpq_warning("RPQ syntax error, term expected");
	    } else {
		rpq_warning("RPQ term \"%.*s\" is unrecognized", (int)len, kw);
	    }
	}
    }

    Free(spec_copy);
}

/* Locates a keyword table entry by ID. */
static struct rpq_keyword *
find_kw(rpq_id_t id)
{
    unsigned i;

    for (i = 0; i < NS_RPQ; i++) {
	if (rpq_keywords[i].id == id) {
	    return &rpq_keywords[i];
	}
    }
    assert(i < NS_RPQ);
    return NULL;
}

/* Checks that s points to whitespace, ':', or nothing. */
static bool
empty_after(const char *s)
{
    unsigned char c;

    while ((c = *s++) && c != ':') {
	if (!isspace(c)) {
	    return false;
	}
    }
    return true;
}

/* Fetches the TIMEZONE term. */
static term_result_t
get_rpq_timezone(unsigned char *buf, const size_t buflen, size_t *lenp)
{
    /*
     * Return the signed number of minutes we're offset from UTC.
     * Example: North America Pacific Standard Time = UTC - 8 Hours, so we
     * return (-8) * 60 = -480.
     */
    double delta;
    unsigned char *ptr;
    struct rpq_keyword *kw = find_kw(RPQ_TIMEZONE);

    *lenp = 0;

    if (buflen < 2) {
	return TR_NOSPACE;
    }

    /* Is there a user override? */
    if (kw->override_offset > 0) {
	char *override, *endptr;
	ldiv_t hhmm;
	long l;

	override = rpq_spec + kw->override_offset;

	errno = 0;
	l = strtol(override, &endptr, 10);
	if (endptr == override || errno != 0 || !empty_after(endptr)) {
	    rpq_warning("RPQ " RPQ_TIMEZONE_NAME " term is invalid - use +/-hhmm");
	    return TR_ERROR;
	}

	hhmm = ldiv(l, 100L);

	if (hhmm.rem > 59L) {
	    rpq_warning("RPQ " RPQ_TIMEZONE_NAME " term is invalid (minutes > 59)");
	    return TR_ERROR;
	}

	delta = (labs(hhmm.quot) * 60L) + hhmm.rem;
	if (hhmm.quot < 0L) {
	    delta = -delta;
	}
    } else {
	time_t here;
	struct tm here_tm;
	struct tm *utc_tm;

	/*
	 * No override specified, try to get information from the system.
	 */
	if ((here = time(NULL)) == (time_t)(-1)) {
	    rpq_warning("RPQ: Unable to determine workstation local time");
	    return TR_ERROR;
	}
	memcpy(&here_tm, localtime(&here), sizeof(struct tm));
	if ((utc_tm = gmtime(&here)) == NULL) {
	    rpq_warning("RPQ: Unable to determine workstation UTC time");
	    return TR_ERROR;
	}

	/*
	 * Do not take Daylight Saving Time into account.
	 * We just want the "raw" time difference.
	 */
	here_tm.tm_isdst = 0;
	utc_tm->tm_isdst = 0;
	delta = difftime(mktime(&here_tm), mktime(utc_tm)) / 60L;
    }

    /* sanity check: difference cannot exceed +/- 12 hours */
    if (labs((long)delta) > 720L) {
	rpq_warning("RPQ " RPQ_TIMEZONE_NAME " exceeds 12 hour UTC offset");
	return TR_ERROR;
    }

    ptr = buf;
    SET16(ptr, (int)delta);
    *lenp = 2;
    return TR_SUCCESS;
}

/* Fetches the USER term. */
static term_result_t
get_rpq_user(unsigned char *buf, const size_t buflen, size_t *lenp) 
{
    /*
     * Text may be specified in one of two ways, but not both.
     * An environment variable provides the user interface:
     *    - X3270RPQ: Keyword USER=
     *
     *    NOTE: If the string begins with 0x then no ASCII/EBCDIC
     *    translation is done.  The hex characters will be sent as true hex
     *    data.  E.g., X3270RPQ="user=0x ab 12 EF" will result in 3 bytes
     *    sent as 0xAB12EF.  White space is optional in hex data format.
     *    When hex format is required, the 0x prefix must be the first two
     *    characters of the string.  E.g., X3270RPQ="user= 0X AB" will
     *    result in 6 bytes sent as 0x40F0E740C1C2 because the text is
     *    accepted "as is" then translated from ASCII to EBCDIC.
     */
    const char *override = NULL;
    size_t len = 0;
    struct rpq_keyword *kw = find_kw(RPQ_USER);
    char *sbuf, *sbuf0;
    const char *s;
    enum me_fail error;
    bool truncated = false;
    ssize_t xlate_len;
    term_result_t ret = TR_SUCCESS;

    *lenp = 0;

    if (kw->override_offset == 0) {
	return TR_OMIT;
    }

    override = rpq_spec + kw->override_offset;

    if (*override == '0' && toupper((unsigned char)*(override + 1)) == 'X') {
	/* Text has 0x prefix... interpret as hex, no translation */
	char *hexstr = Malloc(strlen(override));
	char *p_h;
	char c;
	bool is_first_hex_digit;

	p_h = hexstr;
	/*
	 * Copy the hex digits from X3270RPQ, removing white
	 * space, and using all upper case for the hex digits a-f.
	 */
	override += 2;	/* skip 0x prefix */
	for (*p_h = '\0'; *override; override++) {
	    c  = toupper((unsigned char)*override);
	    if (c == ':' || c == '\0') {
		break;
	    }
	    if (isspace((unsigned char)c)) {
		continue;	 /* skip white space */
	    }
	    if (!isxdigit((unsigned char)c)) {
		rpq_warning("RPQ " RPQ_USER_NAME " term has non-hex character");
		break;
	    }
	    len = (p_h - hexstr) / 2;
	    if (len >= buflen) {
		/* Too long. */
		Free(hexstr);
		return TR_NOSPACE;
	    }

	    *p_h++ = c;		/* copy (upper case) character */
	    *p_h = '\0';	/* keep string properly terminated */
	}
	/*
	 * 'hexstr' is now a character string of 0-9, A-F only, (a-f were converted to upper case).
	 * There may be an odd number of characters, implying a leading 0.
	 * The string is also known to fit in the area specified.
	 */

	/*
	 * Hex digits are handled in pairs, set a flag so we keep track
	 * of which hex digit we're currently working with.
	 */
	is_first_hex_digit = ((strlen(hexstr) % 2) == 0);
	if (!is_first_hex_digit) {
	    rpq_warning("RPQ " RPQ_USER_NAME " term has odd number of hex digits");
	}
	*buf = 0;	/* initialize first byte for possible implied leading zero */
	for (p_h = &hexstr[0]; *p_h; p_h++) {
	    /* convert the hex character to a value 0-15 */
	    int n = isdigit((unsigned char)*p_h) ? *p_h - '0' : *p_h - 'A' + 10;

	    if (is_first_hex_digit) {
		*buf = n << 4;
	    } else {
		*buf++ |= n;
	    }
	    is_first_hex_digit = !is_first_hex_digit;
	}
	*lenp = (strlen(hexstr) + 1) / 2;
	Free(hexstr);
	return TR_SUCCESS;
    }

    /* plain text - subject to ascii/ebcdic translation */

    /*
     * Copy the source string to a temporary buffer, terminating on 
     * ':', unless preceded by '\'.
     */
    sbuf = sbuf0 = Malloc(strlen(override) + 1);
    for (s = override; *s && (*s != ':'); s++) {
	if (*s == '\\' && *(s + 1)) {
	    *sbuf++ = *++s;
	} else {
	    *sbuf++ = *s;
	}
    }
    *sbuf = '\0';

    /* Translate multibyte to EBCDIC in the target buffer. */
    xlate_len = multibyte_to_ebcdic_string(sbuf0, strlen(sbuf0), buf, buflen, &error, &truncated);
    if (xlate_len < 0) {
	rpq_warning("RPQ " RPQ_USER_NAME " term translation error");
	ret = TR_ERROR;
    } else {
	if (truncated) {
	    ret = TR_NOSPACE;
	}
	len = xlate_len;
    }
    Free(sbuf0);

    if (ret == TR_SUCCESS) {
	*lenp = len;
    }
    return ret;
}

/* Fetches the ADDRESS term. */
static term_result_t
get_rpq_address(unsigned char *buf, const size_t maxlen, size_t *lenp)
{
    struct rpq_keyword *kw = find_kw(RPQ_ADDRESS);
    size_t x = 0;

    *lenp = 0;

    if (maxlen < 2) {
	return TR_NOSPACE;
    }

    /* Is there a user override? */
    if (kw->override_offset > 0) {
	char *override;
	char *override_copy, *to;
	size_t sl;
	struct addrinfo *res;
	int ga_err;

	override = rpq_spec + kw->override_offset;

	/* Skip leading white space. */
	while (isspace((unsigned char)*override)) {
	    override++;
	}

	/* Isolate the override into its own buffer. */
	override_copy = (char *)Malloc(strlen(override) + 1);
	for (to = override_copy; *override; to++) {
	    if (*override == ':') {
		break;
	    }
	    if (*override == '\\' && *(override + 1) == ':') {
		override++;
	    }
	    *to = *override;
	    override++;
	}
	*to = '\0';

	/* Remove trailing white space. */
	sl = strlen(override_copy);
	while (sl > 0) {
	    if (isspace((unsigned char)override_copy[sl - 1])) {
		override_copy[--sl] = '\0';
	    } else {
		break;
	    }
	}
	if (!sl) {
	    rpq_warning("RPQ " RPQ_ADDRESS_NAME " term is invalid - empty");
	    return TR_ERROR;
	}

	ga_err = getaddrinfo(override_copy, NULL, NULL, &res);
	if (ga_err == 0) {
	    void *src = NULL;
	    int len = 0;

	    SET16(buf, res->ai_family);
	    x += 2;

	    switch (res->ai_family) {
	    case AF_INET:
		src = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
		len = sizeof(struct in_addr);
		break;
	    case AF_INET6:
		src = &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
		len = sizeof(struct in6_addr);
		break;
	    default:
		rpq_warning("RPQ " RPQ_ADDRESS_NAME " term has unrecognized family %u", res->ai_family);
		return TR_ERROR;
	    }

	    if (x + len <= maxlen) {
		x += len;
		memcpy(buf, src, len);
	    } else {
		return TR_NOSPACE;
	    }
	    /* Give back storage obtained by getaddrinfo */
	    freeaddrinfo(res);
	} else {
	    rpq_warning("RPQ: can't resolve '%s': %s", override_copy,
# if defined(_WIN32) /*[*/
		    to_localcp(gai_strerror(ga_err))
# else /*][*/
		    gai_strerror(ga_err)
# endif /*]*/
		    );
	}
	Free(override_copy);
    } else {
	/* No override... get our address from the actual socket */
	union {
	    struct sockaddr sa;
	    struct sockaddr_in sa4;
	    struct sockaddr_in6 sa6;
	} u;
	int addrlen = sizeof(u);
	void *src = NULL;
	int len = 0;

	if (net_getsockname(&u, &addrlen) < 0) {
	    rpq_warning("RPQ: can't get local address");
	    return TR_ERROR;
	}
	SET16(buf, u.sa.sa_family);
	x += 2;
	switch (u.sa.sa_family) {
	case AF_INET:
	    src = &u.sa4.sin_addr;
	    len = sizeof(struct in_addr);
	    break;
	case AF_INET6:
	    src = &u.sa6.sin6_addr;
	    len = sizeof(struct in6_addr);
	    break;
	default:
	    rpq_warning("RPQ " RPQ_ADDRESS_NAME " term has unrecognized family %u", u.sa.sa_family);
	    return TR_ERROR;
	}
	if (x + len <= maxlen) {
	    memcpy(buf, src, len);
	    x += len;
	} else {
	    return TR_NOSPACE;
	}
    }
    *lenp = x;
    return TR_SUCCESS;
}

/* Fetches the VERSION term. */
static term_result_t
get_rpq_version(unsigned char *buf, const size_t buflen, size_t *lenp)
{
    ssize_t nw;
    enum me_fail error;
    bool truncated;

    *lenp = 0;

    nw = multibyte_to_ebcdic_string(build_rpq_version, strlen(build_rpq_version), buf, buflen, &error, &truncated);
    if (truncated) {
	return TR_NOSPACE;
    }
    *lenp = nw;
    return TR_SUCCESS;
}

/* Fetches the TIMESTAMP term. */
static term_result_t
get_rpq_timestamp(unsigned char *buf, const size_t buflen, size_t *lenp)
{
    size_t x = strlen(build_rpq_timestamp);
    unsigned i;
    unsigned char *bufp = buf;

    *lenp = 0;
    if ((x + 1) / 2 > buflen) {
	return TR_NOSPACE;
    }
    for (i = 0; i < x; i += 2) {
	*bufp++ = ((*(build_rpq_timestamp + i) - '0') << 4) + (*(build_rpq_timestamp + i + 1) - '0');
    }

    *lenp = bufp - buf;
    return TR_SUCCESS;
}

/* Initializes a new cycle of warning messages. */
static void
rpq_init_warnings(void)
{
    Replace(rpq_warnbuf_prev, rpq_warnbuf);
    rpq_warnbuf = NULL;
}

/* Stores a warning message. */
static void
rpq_warning(const char *fmt, ...)
{
    va_list a;
    char *msg;

    va_start(a, fmt);
    msg = Vasprintf(fmt, a);
    va_end(a);
    if (rpq_warnbuf == NULL) {
	rpq_warnbuf = msg;
    } else {
	char *old = rpq_warnbuf;

	rpq_warnbuf = Asprintf("%s\n%s", old, msg);
	Free(old);
    }
}

/* Dumps warnings. */
static void
rpq_dump_warnings(void)
{
    /* Only complain if different from what was complained about last time. */
    if (rpq_warnbuf != NULL && (rpq_warnbuf_prev == NULL || strcmp(rpq_warnbuf, rpq_warnbuf_prev))) {
	popup_an_error("%s", rpq_warnbuf);
    }
}


/* Toggle the value of rpq. */
static toggle_upcall_ret_t
toggle_rpq(const char *name, const char *value, unsigned flags, ia_t ia)
{
    Replace(appres.rpq, NewString(value));
    return TU_SUCCESS;
}

/* Module registration. */
void
rpq_register(void)
{
    register_extended_toggle(ResRpq, toggle_rpq, NULL, NULL, (void **)&appres.rpq, XRM_STRING);
}
