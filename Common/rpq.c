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
} rpq_id_t;

#define RPQ_ADDRESS_NAME	"ADDRESS"
#define RPQ_TIMESTAMP_NAME	"TIMESTAMP"
#define RPQ_TIMEZONE_NAME	"TIMEZONE"
#define RPQ_USER_NAME		"USER"
#define RPQ_VERSION_NAME	"VERSION"

/* Error code return by individual term get functions. */
typedef enum {
    TR_SUCCESS,		/* successful generation */
    TR_OMIT,		/* term intentionally omitted */
    TR_NOSPACE,		/* insufficient space to store term */
    TR_ERROR,		/* other error generating term */
} term_result_t;
typedef term_result_t get_term_fn(unsigned char *buf, const size_t buflen, size_t *len);

/* Statics */
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
    bool omit;		/* set from X3270RPQ="kw1:kw2..." environment var */
    size_t oride;	/* displacement */
    const bool allow_oride;
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

static char *x3270rpq;

/*
 * RPQNAMES query reply.
 */
void
do_qr_rpqnames(void)
{
#   define TERM_PREFIX_SIZE 2	/* Each term has 1 byte length and 1 byte id */
    ssize_t nw;
    enum me_fail error;
    bool truncated = false;
    unsigned char *rpql;
    unsigned i;
    size_t remaining = 254;	/* maximum data area for rpqname reply */
    static const char x3270name[] = "x3270";
#   define X3270_NAMESIZE (sizeof(x3270name) - 1)

    trace_ds("> QueryReply(RPQNames)\n");

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

    for (i = 0; i < NS_RPQ; i++) {
	bool omit_due_space_limit = false;

	if (rpq_keywords[i].omit) {
	    continue;
	}

	omit_due_space_limit = remaining < TERM_PREFIX_SIZE;
	if (!omit_due_space_limit) {
	    size_t term_len = 0;
	    term_result_t term_result;

	    term_result = rpq_keywords[i].get(obptr + TERM_PREFIX_SIZE, remaining - TERM_PREFIX_SIZE, &term_len);
	    if (term_result == TR_SUCCESS) {
		*obptr++ = TERM_PREFIX_SIZE + term_len;		/* length of term */
		*obptr++ = rpq_keywords[i].id;			/* term ID */
		obptr += term_len;				/* jump over term contents */
		remaining -= TERM_PREFIX_SIZE + term_len;	/* account for space taken */
	    } else {
		/* Failed, check for overflow, which will cause error output. */
		omit_due_space_limit = (term_result == TR_NOSPACE);
	    }
	}

	if (omit_due_space_limit) {
	    rpq_warning("RPQ %s term omitted due to insufficient space", rpq_keywords[i].text);
	}
    }

    /* Fill in overall length of RPQNAME info */
    *rpql = (unsigned char)(obptr - rpql);

    rpq_dump_warnings();
}

/* Selects which terms will be returned in RPQNAMES. */
static void
select_rpq_terms(void)
{
    size_t i;
    unsigned j ,k;
    size_t len;
    char *uplist;
    char *p1, *p2;
    char *kw;
    bool is_no_form;

    /* Reinitialize. */
    for (j = 0; j < NS_RPQ; j++) {
	rpq_keywords[j].omit = true;
	rpq_keywords[j].oride = 0;
    }

    /* See if the user wants any rpqname self-defining terms returned. */
    if (appres.rpq != NULL) {
	x3270rpq = appres.rpq;
    } else if ((x3270rpq = getenv("X3270RPQ")) == NULL) {
	return;
    }
    for (p1 = x3270rpq; *p1 && isspace((unsigned char)*p1); p1++) {
    }
    if (!*p1) {
	x3270rpq = NULL;
	return;
    }

    /*
     * Make an uppercase copy of the user selections so I can match
     * keywords more easily.
     * If there are override values, I'll get those from the ORIGINAL
     * string so upper/lower case is preserved as necessary.
     */
    uplist = (char *)Malloc(strlen(x3270rpq) + 1);
    p1 = uplist;
    p2 = x3270rpq;
    while (*p2) {
	*p1++ = toupper((unsigned char)*p2++);
    }
    *p1 = '\0';

    for (i = 0; i < strlen(x3270rpq); ) {
	char *after_kw;
	size_t kw_len;

	kw = uplist + i;
	i++;
	if (isspace((unsigned char)*kw)) {
	    continue;	/* skip leading white space */
	}
	if (*kw == ':') {
	    continue;
	}

	/* : separates terms, but \: is literal : */
	p1 = kw;
	do {
	    p1 = strchr(p1 + 1,':');
	    if (p1 == NULL) {
		break;
	    }
	} while (*(p1 - 1) == '\\');
	/* p1 points to the : separating a term, or is NULL */
	if (p1 != NULL) {
	    *p1 = '\0';
	}
	/* kw is now a string of the entire, single term. */

	i = (kw - uplist) + strlen(kw) + 1;
	/* It might be a keyword=value item... */

	for (p1 = kw; *p1; p1++) {
	    if (!isupper((unsigned char)*p1)) {
		break;
	    }
	}
	len = p1 - kw; 
	is_no_form = len > 2 && !strncmp("NO", kw, 2);
	if (is_no_form) {
	    kw += 2;		/* skip "NO" prefix for matching keyword */
	    len -= 2;		/* adjust keyword length */
	}

	after_kw = kw;
	while (isupper((unsigned char)*after_kw)) {
	    after_kw++;
	}
	kw_len = (size_t)(after_kw - kw);

	for (j = 0; j < NS_RPQ; j++) {
	    if (kw_len == strlen(rpq_keywords[j].text) && !strncmp(kw, rpq_keywords[j].text, kw_len)) {
		rpq_keywords[j].omit = is_no_form;
		while (*p1 && isspace((unsigned char)*p1)) {
		    p1++;
		}
		if (*p1 == '=') {
		    if (rpq_keywords[j].allow_oride) {
			rpq_keywords[j].oride = p1 - uplist + 1;
		    } else {
			rpq_warning("RPQ %s term override ignored", rpq_keywords[j].text);
		    }
		}
		break;
	    }
	}

	if (j >= NS_RPQ) {
	    /* unrecognized keyword... */
	    if (!strcmp(kw, "ALL")) {
		for (k = 0; k < NS_RPQ; k++) {
		    rpq_keywords[k].omit = is_no_form;
		}
	    } else {
		rpq_warning("RPQ term \"%s\" is unrecognized", kw);
	    }
	}
    }

    Free(uplist);
}

/* Locates a keyword table entry by ID. */
static struct rpq_keyword *
find_kw(rpq_id_t id)
{
    unsigned j;

    for (j = 0; j < NS_RPQ; j++) {
	if (rpq_keywords[j].id == id) {
	    return &rpq_keywords[j];
	}
    }
    assert(j < NS_RPQ);
    return NULL;
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
    time_t here;
    struct tm here_tm;
    struct tm *utc_tm;
    double delta;
    char *p1, *p2;
    unsigned char *ptr;
    struct rpq_keyword *kw = find_kw(RPQ_TIMEZONE);

    *lenp = 0;

    if (buflen < 2) {
	return TR_NOSPACE;
    }

    /* Is there a user override? */
    if ((kw->allow_oride) && (kw->oride > 0)) {
	ldiv_t hhmm;
	long x;

	p1 = x3270rpq + kw->oride;

	errno = 0;
	x = strtol(p1, &p2, 10);
	if (errno != 0 || ((*p2 != '\0') && (*p2 != ':') && (!isspace((unsigned char)*p2)))) {
	    rpq_warning("RPQ " RPQ_TIMEZONE_NAME " term is invalid - use +/-hhmm");
	    return TR_ERROR;
	}

	hhmm = ldiv(x, 100L);

	if (hhmm.rem > 59L) {
	    rpq_warning("RPQ " RPQ_TIMEZONE_NAME " term is invalid (minutes > 59)");
	    return TR_ERROR;
	}

	delta = (labs(hhmm.quot) * 60L) + hhmm.rem;
	if (hhmm.quot < 0L) {
	    delta = -delta;
	}
    } else {
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
    const char *rpqtext = NULL;
    size_t x = 0;
    struct rpq_keyword *kw = find_kw(RPQ_USER);
    char *sbuf, *sbuf0;
    const char *s;
    enum me_fail error;
    bool truncated = false;
    ssize_t xlen;
    term_result_t ret = TR_SUCCESS;

    *lenp = 0;

    if ((!kw->allow_oride) || (kw->oride <= 0)) {
	return TR_OMIT;
    }

    rpqtext = x3270rpq + kw->oride;

    if ((*rpqtext == '0') && (toupper((unsigned char)*(rpqtext + 1)) == 'X')) {
	/* Text has 0x prefix... interpret as hex, no translation */
	char *hexstr = Malloc(strlen(rpqtext));
	char *p_h;
	char c;
	bool is_first_hex_digit;

	p_h = hexstr;
	/*
	 * Copy the hex digits from X3270RPQ, removing white
	 * space, and using all upper case for the hex digits a-f.
	 */
	rpqtext += 2;	/* skip 0x prefix */
	for (*p_h = '\0'; *rpqtext; rpqtext++) {
	    c  = toupper((unsigned char)*rpqtext);
	    if ((c==':') || (c=='\0')) {
		break;
	    }
	    if (isspace((unsigned char)c)) {
		continue;	 /* skip white space */
	    }
	    if (!isxdigit((unsigned char)c)) {
		rpq_warning("RPQ " RPQ_USER_NAME " term has non-hex character");
		break;
	    }
	    x = (p_h - hexstr) / 2;
	    if (x >= buflen) {
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
    sbuf = sbuf0 = Malloc(strlen(rpqtext) + 1);
    for (s = rpqtext; *s && (*s != ':'); s++) {
	if (*s == '\\' && *(s + 1)) {
	    *sbuf++ = *++s;
	} else {
	    *sbuf++ = *s;
	}
    }
    *sbuf = '\0';

    /* Translate multibyte to EBCDIC in the target buffer. */
    xlen = multibyte_to_ebcdic_string(sbuf0, strlen(sbuf0), buf, buflen, &error, &truncated);
    if (xlen < 0) {
	rpq_warning("RPQ " RPQ_USER_NAME " term translation error");
	ret = TR_ERROR;
    } else {
	if (truncated) {
	    ret = TR_NOSPACE;
	}
	x = xlen;
    }
    Free(sbuf0);

    if (ret == TR_SUCCESS) {
	*lenp = x;
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
    if ((kw->allow_oride) && (kw->oride > 0)) {
	char *p1, *p2, *rpqtext;
	struct addrinfo *res;
	int ga_err;

	p1 = x3270rpq + kw->oride;
	rpqtext = (char *)Malloc(strlen(p1) + 1);
	for (p2 = rpqtext; *p1; p2++) {
	    if (*p1 == ':') {
		break;
	    }
	    if ((*p1 == '\\') && (*(p1 + 1) == ':')) {
		p1++;
	    }
	    *p2 = *p1;
	    p1++;
	}
	*p2 = '\0';

	ga_err = getaddrinfo(rpqtext, NULL, NULL, &res);
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
	    rpq_warning("RPQ: can't resolve '%s': %s", rpqtext,
# if defined(_WIN32) /*[*/
		    to_localcp(gai_strerror(ga_err))
# else /*][*/
		    gai_strerror(ga_err)
# endif /*]*/
		    );
	}
	Free(rpqtext);
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
	    /* XXX: display error message */
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
    int nw;
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
