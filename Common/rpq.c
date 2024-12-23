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

#include "ctlrc.h"
#include "popups.h"
#include "sf.h"	 /* has to come before rpq.h */
#include "rpq.h"
#include "tables.h"
#include "telnet.h"
#include "telnet_core.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

/* Statics */
static bool select_rpq_terms(void);
static int get_rpq_timezone(int *offsetp);
static size_t get_rpq_user(unsigned char buf[], const size_t buflen);
static size_t get_rpq_address(unsigned char buf[], const size_t buflen);
static void rpq_warning(const char *fmt, ...);
static void rpq_dump_warnings(void);
static bool rpq_complained = false;
static char *rpq_warnbuf = NULL;
static bool omit_due_space_limit = false;

/*
 * Define symbolic names for RPQ self-defining terms.
 * (Numbering is arbitrary, but must be 0-255 inclusive.
 * Do not renumber existing items because these identify the
 * self-defining term to the mainframe software. Changing pre-existing
 * values will possibly impact host based software.
 */
#define	RPQ_ADDRESS	0
#define	RPQ_TIMESTAMP	1
#define	RPQ_TIMEZONE	2
#define	RPQ_USER	3
#define	RPQ_VERSION	4

/*
 * Define a table of RPQ self-defing terms. 
 * NOTE: Synonyms could be specified by coding different text items but using
 * the same "id" value.
 * Items should be listed in alphabetical order by "text" name so if the user
 * specifies abbreviations, they work in a predictable manner.  E.g., "TIME"
 * should match TIMESTAMP instead of TIMEZONE.
 */
static struct rpq_keyword {
    bool omit;	/* set from X3270RPQ="kw1:kw2..." environment var */
    size_t oride;	/* displacement */
    const bool allow_oride;
    const unsigned char id;
    const char *text;
} rpq_keywords[] = {
    { true, 0, 	true,	RPQ_ADDRESS,	"ADDRESS" },
    { true, 0, 	false,	RPQ_TIMESTAMP,	"TIMESTAMP" },
    { true, 0, 	true,	RPQ_TIMEZONE,	"TIMEZONE" },
    { true, 0, 	true,	RPQ_USER,	"USER" },
    { true, 0, 	false,	RPQ_VERSION,	"VERSION" },
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

    unsigned char *rpql, *p_term;
    unsigned j;
    int term_id;
    size_t i, x;
    size_t remaining = 254;	/* maximum data area for rpqname reply */
    bool omit_due_space_limit;

    trace_ds("> QueryReply(RPQNames)\n");

    /*
     * Allocate enough space for the maximum allowed item.
     * By pre-allocating the space I don't have to worry about the
     * possibility of addresses changing.
     */
    space3270out(4+4+1+remaining);	/* Maximum space for an RPQNAME item */

    SET32(obptr, 0);			/* Device number, 0 = All */
    SET32(obptr, 0);			/* Model number, 0 = All */

    rpql = obptr++;			/* Save address to place data length. */

    /*
     * Create fixed length portion - program id: x3270
     * This is known 8-bit text so we can use asc2ebc0 to translate it.
     */
    for (j = 0; j < 5; j++) {
	*obptr++ = asc2ebc0[(int)"x3270"[j]];
	remaining--;
    }

    /* Create user selected variable-length self-defining terms. */
    select_rpq_terms();

    for (j = 0; j < NS_RPQ; j++) {
	if (rpq_keywords[j].omit) {
	    continue;
	}

	if (remaining < TERM_PREFIX_SIZE) {
	    rpq_warning("RPQ %s term omitted due to insufficient space", rpq_keywords[j].text);
	    continue;
	}

	omit_due_space_limit = false;

	term_id = rpq_keywords[j].id;

	p_term = obptr;		/* save starting address (to insert length later) */
	obptr++;		/* skip length of term, fill in later */
	*obptr++ = term_id;	/* identify this term */

	/*
	 * Adjust remaining space by the term prefix size so each case
	 * can use the "remaining" space without concern for the
	 * prefix.  This subtraction is accounted for after the item
	 * is built and the updated remaining space is determined.
	 */
	remaining -= TERM_PREFIX_SIZE;

	switch (term_id) {	/* build the term based on id */
	case RPQ_USER:		/* User text from env. vars */
	    obptr += get_rpq_user(obptr, remaining);
	    break;

	case RPQ_TIMEZONE:	/* UTC time offset */
	    omit_due_space_limit = (remaining < 2);
	    if (!omit_due_space_limit) {
		int offset = 0;
		int err = get_rpq_timezone(&offset);

		if (!err) {
		    SET16(obptr, offset);
		}
	    }
	    break;

	case RPQ_ADDRESS:	/* Workstation address */
	    obptr += get_rpq_address(obptr, remaining);
	    break;

	case RPQ_VERSION:	/* program version */
	    /*
	     * Note: It is legal to use asc2ebc0 to translate the
	     * build string from ASCII to EBCDIC because the build
	     * string is always generated in the "C" locale.
	     */
	    x = strlen(build_rpq_version);
	    omit_due_space_limit = (x > remaining);
	    if (!omit_due_space_limit) {
		for (i = 0; i < x; i++) {
		    *obptr++ = asc2ebc0[(int)(*(build_rpq_version+i) & 0xff)];
		}
	    }
	    break;

	case RPQ_TIMESTAMP:	/* program build time (yyyymmddhhmmss bcd) */
	    x = strlen(build_rpq_timestamp);
	    omit_due_space_limit = ((x + 1) / 2 > remaining);
	    if (!omit_due_space_limit) {
		for (i = 0; i < x; i += 2) {
		    *obptr++ = ((*(build_rpq_timestamp+i) - '0') << 4)
			+ (*(build_rpq_timestamp+i+1) - '0');
		}
	    }
	    break;

	default:		/* unsupported ID, (can't happen) */
	    Error("Unsupported RPQ term");
	    break;		
	}

	if (omit_due_space_limit) {
	    rpq_warning("RPQ %s term omitted due to insufficient space", rpq_keywords[j].text);
	}

	/*
	 * The item is built, insert item length as needed and
	 * adjust space remaining.
	 * obptr now points at "next available byte".
	 */
	x = obptr-p_term;
	if (x > TERM_PREFIX_SIZE) {
	    *p_term = (unsigned char)x;
	    remaining -= x;	/* This includes length and id fields,
				   correction below */
	} else {
	    /* We didn't add an item after all, reset pointer. */
	    obptr = p_term;
	}
	/*
	 * When we calculated the length of the term, a few lines
	 * above, that length included the term length and term id
	 * prefix too. (TERM_PREFIX_SIZE)
	 * But just prior to the switch statement, we decremented the 
	 * remaining space by that amount so subsequent routines would
	 * be told how much space they have for their data, without
	 * each routine having to account for that prefix.
	 * That means the remaining space is actually more than we
	 * think right now, by the length of the prefix.... add that
	 * back so the remaining space is accurate.
	 *
	 * And... if there was no item added, we still have to make the
	 * same correction to "claim back" the term prefix area so it
	 * may be used by the next possible term.
	 */
	remaining += TERM_PREFIX_SIZE;
    }

    /* Fill in overall length of RPQNAME info */
    *rpql = (unsigned char)(obptr - rpql);

    rpq_dump_warnings();
}

/* Utility function used by the RPQNAMES query reply. */
static bool
select_rpq_terms(void)
{
    size_t i;
    unsigned j,k;
    size_t len;
    char *uplist;
    char *p1, *p2;
    char *kw;
    bool is_no_form;

    /* See if the user wants any rpqname self-defining terms returned */
    if ((x3270rpq = getenv("X3270RPQ")) == NULL) {
	return false;
    }

    /*
     * Make an uppercase copy of the user selections so I can match
     * keywords more easily.
     * If there are override values, I'll get those from the ORIGINAL
     * string so upper/lower case is preserved as necessary.
     */
    uplist = (char *)Malloc(strlen(x3270rpq)+1);
    assert(uplist != NULL);
    p1 = uplist;
    p2 = x3270rpq;
    do {
	*p1++ = toupper((unsigned char)*p2++);
    } while (*p2);
    *p1 = '\0';

    for (i = 0; i < strlen(x3270rpq); ) {
	kw = uplist+i;
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
	    p1 = strchr(p1+1,':');
	    if (p1 == NULL) {
		break;
	    }
	} while (*(p1-1) == '\\');
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
	len = p1-kw; 

	is_no_form = ((len > 2) && (strncmp("NO", kw, 2) == 0));
	if (is_no_form) {
	    kw += 2;		/* skip "NO" prefix for matching keyword */
	    len -= 2;		/* adjust keyword length */
	}
	for (j = 0; j < NS_RPQ; j++) {
	    if (strncmp(kw, rpq_keywords[j].text, len) == 0) {
		rpq_keywords[j].omit = is_no_form;
		while (*p1 && isspace((unsigned char)*p1)) {
		    p1++;
		}
		if (*p1 == '=') {
		    if (rpq_keywords[j].allow_oride) {
			rpq_keywords[j].oride = p1 - uplist + 1;
		    } else {
			rpq_warning("RPQ %s term override ignored", p1);
		    }
		}
		break;
	    }
	}
	if (j >= NS_RPQ) {
	    /* unrecognized keyword... */
	    if (strcmp(kw,"ALL") == 0) {
		for (k = 0; k < NS_RPQ; k++) {
		    rpq_keywords[k].omit = is_no_form;
		}
	    } else {
		rpq_warning("RPQ term \"%s\" is unrecognized", kw);
	    }
	}
    }

    Free(uplist);

    /*
     * Return to caller with indication (T/F) of any items 
     * to be selected (T) or are all terms suppressed? (F)
     */
    for (i = 0; i < NS_RPQ; i++) {
	if (!rpq_keywords[i].omit) {
	    return true;
	}
    }

    return false;
}

/* Locate a keyword table entry. */
static struct rpq_keyword *
find_kw(int id)
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

/*
 * Utility function used by the RPQNAMES query reply.
 * Returns 0 or an error code:
 * 1 - Cannot determine local calendar time
 * 2 - Cannot determine UTC
 * 3 - Difference exceeds 12 hours
 * 4 - User override is invalid
*/
static int
get_rpq_timezone(int *offsetp)
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
    struct rpq_keyword *kw = find_kw(RPQ_TIMEZONE);

    *offsetp = 0;

    /* Is there a user override? */
    if ((kw->allow_oride) && (kw->oride > 0)) {
	ldiv_t hhmm;
	long x;

	p1 = x3270rpq + kw->oride;
	
	errno = 0;
	x = strtol(p1, &p2, 10);
	if (errno != 0 || ((*p2 != '\0') && (*p2 != ':') && (!isspace((unsigned char)*p2)))) {
	    rpq_warning("RPQ TIMEZONE term is invalid - use +/-hhmm");
	    return 4;
	}

	hhmm = ldiv(x, 100L);

	if (hhmm.rem > 59L) {
	    rpq_warning("RPQ TIMEZONE term is invalid (minutes > 59)");
	    return 4;
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
	    return 1;
	}
	memcpy(&here_tm, localtime(&here), sizeof(struct tm));
	if ((utc_tm = gmtime(&here)) == NULL) {
	    rpq_warning("RPQ: Unable to determine workstation UTC time");
	    return 2;
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
	rpq_warning("RPQ TIMEZONE exceeds 12 hour UTC offset");
	return 3;
    }

    *offsetp = (int)delta;
    return 0;
}

/* Utility function used by the RPQNAMES query reply. */
static size_t
get_rpq_user(unsigned char buf[], const size_t buflen) 
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
    int xlen;

    if ((!kw->allow_oride) || (kw->oride <= 0)) {
	return 0;
    }

    rpqtext = x3270rpq + kw->oride;

    if ((*rpqtext == '0') && (toupper((unsigned char)*(rpqtext+1)) == 'X')) {
	/* Text has 0x prefix... interpret as hex, no translation */
	char hexstr[512];	/* more than enough room to copy */
	char *p_h;
	char c;
	bool is_first_hex_digit;

	p_h = &hexstr[0];
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
		rpq_warning("RPQ USER term has non-hex character");
		break;
	    }
	    x = (p_h - hexstr) / 2;
	    if (x >= buflen) {
		x = buflen;
		rpq_warning("RPQ USER term truncated after %d bytes", x);
		break; /* too long, truncate */
	    }

	    *p_h++ = c;		/* copy (upper case) character */
	    *p_h = '\0';	/* keep string properly terminated */
	}
	/*
	 * 'hexstr' is now a character string of 0-9, A-F only,
	 * (a-f were converted to upper case).
	 * There may be an odd number of characters, implying a leading
	 * 0.  The string is also known to fit in the area specified.
	 */

	/*
	 * Hex digits are handled in pairs, set a flag so we keep track
	 * of which hex digit we're currently working with.
	 */
	is_first_hex_digit = ((strlen(hexstr) % 2) == 0);
	if (!is_first_hex_digit) {
	    rpq_warning("RPQ USER term has odd number of hex digits");
	}
	*buf = 0;	/* initialize first byte for possible implied
			   leading zero */
	for (p_h = &hexstr[0]; *p_h; p_h++) {
	    int n;

	    /* convert the hex character to a value 0-15 */
	    n = isdigit((unsigned char)*p_h) ? *p_h - '0' : *p_h - 'A' + 10;
	    if (is_first_hex_digit) {
		*buf = n << 4;
	    } else {
		*buf++ |= n;
	    }
	    is_first_hex_digit = !is_first_hex_digit;
	}
	return (strlen(hexstr) + 1) / 2;
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
    xlen = multibyte_to_ebcdic_string(sbuf0, strlen(sbuf0), buf, buflen,
	    &error, &truncated);
    if (xlen < 0) {
	rpq_warning("RPQ USER term translation error");
	if (buflen) {
	    *buf = asc2ebc0['?'];
	    x = 1;
	}
    } else {
	if (truncated) {
	    rpq_warning("RPQ USER term truncated");
	}
	x = xlen;
    }
    Free(sbuf0);

    return x;
}

static size_t
get_rpq_address(unsigned char *buf, const size_t maxlen) 
{
    struct rpq_keyword *kw = find_kw(RPQ_ADDRESS);
    size_t x = 0;

    if (maxlen < 2) {
	omit_due_space_limit = true;
	return 0;
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
		rpq_warning("RPQ ADDRESS term has unrecognized family %u",
			res->ai_family);
		break;
	    }

	    if (x + len <= maxlen) {
		x += len;
		memcpy(buf, src, len);
	    } else {
		rpq_warning("RPQ ADDRESS term incomplete due to space limit");
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
	    return 0;
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
	    rpq_warning("RPQ ADDRESS term has unrecognized family %u",
		    u.sa.sa_family);
	    break;
	}
	if (x + len <= maxlen) {
	    memcpy(buf, src, len);
	    x += len;
	} else {
	    rpq_warning("RPQ ADDRESS term incomplete due to space limit");
	}
    }
    return x;
}

static void
rpq_warning(const char *fmt, ...)
{
    va_list a;
    char *msg;

    /* Only accumulate RPQ warnings if they have not been displayed already. */
    if (rpq_complained) {
	return;
    }

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

static void
rpq_dump_warnings(void)
{
    /* If there's something to complain about, only complain once. */
    if (rpq_warnbuf != NULL) {
	popup_an_error("%s", rpq_warnbuf);
	rpq_complained = true;
	Replace(rpq_warnbuf, NULL);
    }
}
