/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes nor the names of their
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND JEFF SPARKES "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR JEFF SPARKES BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	util.c
 *		Utility functions for x3270/c3270/s3270/tcl3270
 */

#include "globals.h"
#if !defined(_WIN32) /*[*/
# include <pwd.h>
#endif /*]*/
#include <fcntl.h>
#include <errno.h>

#include "appres.h"
#include "boolstr.h"
#include "resources.h"
#include "codepage.h"
#include "fallbacks.h"
#include "names.h"
#include "popups.h"
#include "product.h"
#include "telnet.h"
#include "trace.h"
#include "txa.h"
#include "unicodec.h"
#include "varbuf.h"

#include "utils.h"

#define my_isspace(c)	isspace((unsigned char)c)

static struct dresource {
    struct dresource *next;
    const char *name;
    char *value;
} *drdb = NULL, **drdb_next = &drdb;

/**
 * printf-like interface to Warning().
 * Displays a warning message, given a printf format.
 *
 * @param[in] fmt	printf format
 */
void
xs_warning(const char *fmt, ...)
{
    va_list args;
    char *r;

    va_start(args, fmt);
    r = Vasprintf(fmt, args);
    va_end(args);
    Warning(r);
    Free(r);
}

/**
 * printf-like interface to Error().
 * Displays an error message, and exits, given a printf format.
 *
 * @param[in] fmt	printf format
 */
void
xs_error(const char *fmt, ...)
{
    va_list args;
    char *r;

    va_start(args, fmt);
    r = Vasprintf(fmt, args);
    va_end(args);
    Error(r);
    Free(r);
}

/**
 * Expand control characters in a string.
 *
 * @param[in] s		String to format
 * @param[in] quoted	If true, quote the string.
 *
 * @returns Expanded string
 */
static char *
catv_common(const char *s, bool quoted)
{
    char c;
    varbuf_t r;

    vb_init(&r);
    if (quoted) {
	vb_appends(&r, "\"");
    }
    while ((c = *s++)) {
	unsigned char uc = c;

	/* Expand this character. */
	switch (uc) {
	case '\n':
	    vb_appends(&r, "\\n");
	    break;
	case '\t':
	    vb_appends(&r, "\\t");
	    break;
	case '\b':
	    vb_appends(&r, "\\b");
	    break;
	case '"':
	    if (quoted) {
		vb_appends(&r, "\\\"");
		break;
	    }
	    /* else fall through */
	default:
	    if (uc < ' ' || uc == 0x7f) {
		vb_appendf(&r, "\\%03o", uc);
	    } else {
		vb_append(&r, &c, 1);
	    }
	    break;
	}
    }
    if (quoted) {
	vb_appends(&r, "\"");
    }
    return txdFree(vb_consume(&r));
}

/**
 * Expand control characters in a string.
 *
 * @param[in] s		String to format
 *
 * @returns Expanded string
 */
char *
scatv(const char *s)
{
    return catv_common(s, false);
}

/**
 * Expand control characters in a string, quoting the result.
 *
 * @param[in] s		String to format
 *
 * @returns Expanded string
 */
char *
qscatv(const char *s)
{
    return catv_common(s, true);
}

/**
 * Definition resource splitter, for resources of the repeating form:
 *	left: right\n
 * Can be called iteratively to parse a list. Set offset to 0 to begin.
 *
 * @param[in] st		String to parse
 * @param[in,out] offset	Offset within string
 * @param[out] left		Returned left side
 * @param[out] right		Returned right side
 *
 * @returns 1 for success, 0 for EOF, -1 for error.
 */
int
s_split_dresource(const char *st, size_t *offset, char **left, char **right)
{
    char *st_copy = NewString(st + *offset);
    char *st_copy0 = st_copy;
    char *l, *r;
    int ret = split_dresource(&st_copy, &l, &r);

    if (ret > 0) {
	*left = NewString(l);
	*right = NewString(r);
	*offset += st_copy - st_copy0;
    }
    Free(st_copy0);
    return ret;
}

/*
 * Definition resource splitter, for resources of the repeating form:
 *	left: right\n
 *
 * Can be called iteratively to parse a list.
 * Returns 1 for success, 0 for EOF, -1 for error.
 *
 * Note: Modifies the input string.
 */
int
split_dresource(char **st, char **left, char **right)
{
    char *s = *st;
    char *t;
    bool quote;

    /* Skip leading white space. */
    while (my_isspace(*s)) {
	s++;
    }

    /* If nothing left, EOF. */
    if (!*s) {
	return 0;
    }

    /* There must be a left-hand side. */
    if (*s == ':') {
	return -1;
    }

    /* Scan until an unquoted colon is found. */
    *left = s;
    for (; *s && *s != ':' && *s != '\n'; s++) {
	if (*s == '\\' && *(s+1) == ':') {
	    s++;
	}
    }
    if (*s != ':') {
	return -1;
    }

    /* Stip white space before the colon. */
    for (t = s-1; my_isspace(*t); t--) {
	*t = '\0';
    }

    /* Terminate the left-hand side. */
    *(s++) = '\0';

    /* Skip white space after the colon. */
    while (*s != '\n' && my_isspace(*s)) {
	s++;
    }

    /* There must be a right-hand side. */
    if (!*s || *s == '\n') {
	return -1;
    }

    /* Scan until an unquoted newline is found. */
    *right = s;
    quote = false;
    for (; *s; s++) {
	if (*s == '\\' && *(s+1) == '"') {
	    s++;
	} else if (*s == '"') {
	    quote = !quote;
	} else if (!quote && *s == '\n') {
	    break;
	}
    }

    /* Strip white space before the newline. */
    if (*s) {
	t = s;
	*st = s+1;
    } else {
	t = s-1;
	*st = s;
    }
    while (my_isspace(*t)) {
	*t-- = '\0';
    }

    /* Done. */
    return 1;
}

/*
 * Split a DBCS resource into its parts.
 * Returns the number of parts found:
 *	-1 error (empty sub-field)
 *	 0 nothing found
 *	 1 one and just one thing found
 *	 2 two things found
 *	 3 more than two things found
 */
int
split_dbcs_resource(const char *value, char sep, char **part1, char **part2)
{
    int n_parts = 0;
    const char *s = value;
    const char *f_start = NULL;	/* start of sub-field */
    const char *f_end = NULL;	/* end of sub-field */
    char c;
    char **rp;

    *part1 = NULL;
    *part2 = NULL;

    for( ; ; ) {
	c = *s;
	if (c == sep || c == '\0') {
	    if (f_start == NULL) {
		return -1;
	    }
	    if (f_end == NULL) {
		f_end = s;
	    }
	    if (f_end == f_start) {
		if (c == sep) {
		    if (*part1) {
			Free(*part1);
			*part1 = NULL;
		    }
		    if (*part2) {
			Free(*part2);
			*part2 = NULL;
		    }
		    return -1;
		} else {
		    return n_parts;
		}
	    }
	    switch (n_parts) {
	    case 0:
		rp = part1;
		break;
	    case 1:
		rp = part2;
		break;
	    default:
		return 3;
	    }
	    *rp = Malloc(f_end - f_start + 1);
	    strncpy(*rp, f_start, f_end - f_start);
	    (*rp)[f_end - f_start] = '\0';
	    f_end = NULL;
	    f_start = NULL;
	    n_parts++;
	    if (c == '\0') {
		return n_parts;
	    }
	} else if (isspace((unsigned char)c)) {
	    if (f_end == NULL) {
		f_end = s;
	    }
	} else {
	    if (f_start == NULL) {
		f_start = s;
	    }
	    f_end = NULL;
	}
	s++;
    }
}

/*
 * List resource splitter, for lists of elements speparated by newlines.
 *
 * Can be called iteratively.
 * Returns 1 for success, 0 for EOF, -1 for error.
 */
int
split_lresource(char **st, char **value)
{
    char *s = *st;
    char *t;
    bool quote;

    /* Skip leading white space. */
    while (my_isspace(*s)) {
	s++;
    }

    /* If nothing left, EOF. */
    if (!*s) {
	return 0;
    }

    /* Save starting point. */
    *value = s;

    /* Scan until an unquoted newline is found. */
    quote = false;
    for (; *s; s++) {
	if (*s == '\\' && *(s+1) == '"') {
	    s++;
	} else if (*s == '"') {
	    quote = !quote;
	} else if (!quote && *s == '\n') {
	    break;
	}
    }

    /* Strip white space before the newline. */
    if (*s) {
	t = s;
	*st = s+1;
    } else {
	t = s-1;
	*st = s;
    }
    while (my_isspace(*t)) {
	*t-- = '\0';
    }

    /* Done. */
    return 1;
}

const char *
get_message(const char *key)
{
    char *r;

    if ((r = get_resource(txAsprintf("%s.%s", ResMessage, key))) != NULL) {
	return r;
    } else {
	return txAsprintf("[missing \"%s\" message]", key);
    }
}

static char *
ex_getenv(const char *name, unsigned long flags, int *up)
{
    if (!strcasecmp(name, "TIMESTAMP")) {
	/* YYYYMMDDHHMMSSUUUUUU */
	struct timeval tv;
	time_t t; /* on Windows, timeval.tv_sec is a long */
	struct tm *tm;

	if (gettimeofday(&tv, NULL) < 0) {
	    return NewString("?");
	}
	t = tv.tv_sec;
	tm = localtime(&t);
	return Asprintf("%04u%02u%02u%02u%02u%02u%06u",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec,
		(unsigned)tv.tv_usec);
    } else if (!strcasecmp(name, "UNIQUE")) {
	++*up;
	if (*up == 0) {
	    return Asprintf("%u", (unsigned)getpid());
	} else {
	    return Asprintf("%u-%u", (unsigned)getpid(), *up);
	}
    } else {
	return NewString(getenv(name));
    }
}

/* Variable and tilde substitution functions. */
static char *
var_subst(const char *s, unsigned long flags)
{
    const char *t;
    enum { VS_BASE, VS_QUOTE, VS_DOLLAR, VS_BRACE, VS_VN, VS_VNB, VS_EOF }
	state;
    char c;
    size_t o_len;
    char *ob;
    char *o;
    const char *vn_start;
    int u = -1;
#   define LBR	'{'
#   define RBR	'}'

    if (strchr(s, '$') == NULL) {
	return NewString(s);
    }

    for (;;) {
	t = s;
	state = VS_BASE;
	vn_start = NULL;
	o_len = strlen(t) + 1;
	ob = Malloc(o_len);
	o = ob;

	while (state != VS_EOF) {
	    c = *t;
	    switch (state) {
	    case VS_BASE:
		if (c == '\\') {
		    state = VS_QUOTE;
		} else if (c == '$') {
		    state = VS_DOLLAR;
		} else {
		    *o++ = c;
		}
		break;
	    case VS_QUOTE:
		if (c == '$') {
		    *o++ = c;
		    o_len--;
		} else {
		    *o++ = '\\';
		    *o++ = c;
		}
		state = VS_BASE;
		break;
	    case VS_DOLLAR:
		if (c == LBR) {
		    state = VS_BRACE;
		} else if (isalpha((unsigned char)c) || c == '_') {
		    vn_start = t;
		    state = VS_VN;
		} else {
		    *o++ = '$';
		    *o++ = c;
		    state = VS_BASE;
		}
		break;
	    case VS_BRACE:
		if (isalpha((unsigned char)c) || c == '_') {
		    vn_start = t;
		    state = VS_VNB;
		} else {
		    *o++ = '$';
		    *o++ = LBR;
		    *o++ = c;
		    state = VS_BASE;
		}
		break;
	    case VS_VN:
	    case VS_VNB:
		if (!(isalnum((unsigned char)c) || c == '_')) {
		    size_t vn_len;
		    char *vn;
		    char *vv;

		    vn_len = t - vn_start;
		    if (state == VS_VNB && c != RBR) {
			*o++ = '$';
			*o++ = LBR;
			strncpy(o, vn_start, vn_len);
			o += vn_len;
			state = VS_BASE;
			continue;	/* rescan */
		    }
		    vn = Malloc(vn_len + 1);
		    strncpy(vn, vn_start, vn_len);
		    vn[vn_len] = '\0';
		    if ((vv = ex_getenv(vn, flags, &u))) {
			*o = '\0';
			o_len = o_len
			    - 1			/* '$' */
			    - (state == VS_VNB)	/* { */
			    - vn_len		/* name */
			    - (state == VS_VNB)	/* } */
			    + strlen(vv);
			ob = Realloc(ob, o_len);
			o = strchr(ob, '\0');
			strcpy(o, vv);
			o += strlen(vv);
			Free(vv);
		    }
		    Free(vn);
		    if (state == VS_VNB) {
			state = VS_BASE;
			break;
		    } else {
			/* Rescan this character */
			state = VS_BASE;
			continue;
		    }
		}
		break;
	    case VS_EOF:
		break;
	    }
	    t++;
	    if (c == '\0') {
		state = VS_EOF;
	    }
	}

	/*
	 * Check for $UNIQUE.
	 *
	 * vr_subst() will increment u if $UNIQUE was used. If it has
	 * been incremented, then try creating the resulting file. If
	 * the open() call fails with EEXIST, then re-run this function
	 * with the new value of u, and try this again with the next
	 * name.
	 *
	 * Keep trying until open() succeeds, or fails with something
	 * other than EEXIST.
	 */
	if (u != -1) {
	    int fd;

	    fd = open(ob, O_WRONLY | O_EXCL | O_CREAT, 0600);
	    if (fd < 0) {
		if (errno == EEXIST) {
		    /* Try again. */
		    Free(ob);
		    continue;
		}
	    } else {
		close(fd);
	    }
	    break;
	} else {
	    break;
	}
    }

    return ob;
}

#if !defined(_WIN32) /*[*/
/*
 * Do tilde (home directory) substitution on a string.  Returns a malloc'd
 * result.
 */
static char *
tilde_subst(const char *s)
{
    char *slash;
    const char *name;
    const char *rest;
    struct passwd *p;
    char *r;
    char *mname = NULL;

    /* Does it start with a "~"? */
    if (*s != '~') {
	return NewString(s);
    }

    /* Terminate with "/". */
    slash = strchr(s, '/');
    if (slash) {
	int len = slash - s;

	mname = Malloc(len + 1);
	strncpy(mname, s, len);
	mname[len] = '\0';
	name = mname;
	rest = slash;
    } else {
	name = s;
	rest = strchr(name, '\0');
    }

    /* Look it up. */
    if (!strcmp(name, "~")) {	/* this user */
	p = getpwuid(getuid());
    } else {			/* somebody else */
	p = getpwnam(name + 1);
    }

    /* Free any temporary copy. */
    Free(mname);

    /* Substitute and return. */
    if (p == NULL) {
	r = NewString(s);
    } else {
	r = Malloc(strlen(p->pw_dir) + strlen(rest) + 1);
	strcpy(r, p->pw_dir);
	strcat(r, rest);
    }
    return r;
}
#else /*][*/
static char *
tilde_subst(const char *s)
{
    char *t;

    if (*s != '~' || (t = getenv("HOMEPATH")) == NULL) {
	return NewString(s);
    }

    switch (*(s + 1)) {
    case '\0':
	return NewString(t);
    case '/':
    case '\\':
	return Asprintf("%s%s", t, s + 1);
    default:
	return NewString(s);
    }
}
#endif /*]*/

char *
do_subst(const char *s, unsigned flags)
{
    if (flags == DS_NONE) {
	return NewString(s);
    }

    if (flags & DS_VARS) {
	char *t;

	t = var_subst(s, flags);
	if (flags & DS_TILDE) {
	    char *u;

	    u = tilde_subst(t);
	    Free(t);
	    return u;
	}
	return t;
    }

    return tilde_subst(s);
}

/*
 * ctl_see
 *	Expands a character in the manner of "cat -v".
 */
char *
ctl_see(int c)
{
    static char buf[64];
    char *p = buf;

    c &= 0xff;
    if ((c & 0x80) && (c <= 0xa0)) {
	*p++ = 'M';
	*p++ = '-';
	c &= 0x7f;
    }
    if (c >= ' ' && c != 0x7f) {
	*p++ = c;
    } else {
	*p++ = '^';
	if (c == 0x7f) {
	    *p++ = '?';
	} else {
	    *p++ = c + '@';
	}
    }
    *p = '\0';
    return buf;
}

/**
 * Add a resource value.
 *
 * @param[in] name	Resource name.
 * @param[in] value	Resource value.
 */
void
add_resource(const char *name, const char *value)
{
    struct dresource *d;

    for (d = drdb; d != NULL; d = d->next) {
	if (!strcmp(d->name, name)) {
	    Replace(d->value, NewString(value));
	    return;
	}
    }
    d = Malloc(sizeof(struct dresource));
    d->next = NULL;
    d->name = NewString(name);
    d->value = NewString(value);
    *drdb_next = d;
    drdb_next = &d->next;
}

/**
 * Get a string-valued resource.
 *
 * @param[in] name	Resource name.
 *
 * @returns Resource value.
 */
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

    return get_underlying_resource(name);
}

/* A version of get_resource that accepts sprintf arguments. */
char *
get_fresource(const char *fmt, ...)
{
    va_list args;
    char *name;
    char *r;

    va_start(args, fmt);
    name = Vasprintf(fmt, args);
    va_end(args);
    r = get_resource(name);
    Free(name);
    return r;
}

/**
 * Get an integer-valued resource.
 *
 * @param[in] name	Resource name
 *
 * @returns Resource value
 */
int
get_resource_int(const char *name)
{
    char *s = get_resource(name);

    return s? atoi(s): 0;
}

/**
 * Get a Boolean-valued resource.
 *
 * @param[in] name	Resource name
 *
 * @returns Resource value
 */
bool
get_resource_bool(const char *name)
{
    char *s = get_resource(name);
    bool b;

    if (s == NULL) {
	return false;
    }

    if (boolstr(s, &b) != NULL) {
	return false;
    }
    return b;
}

/*
 * Whitespace stripper.
 */
char *
strip_whitespace(const char *s)
{
    char *t = NewString(s);

    while (*t && my_isspace(*t)) {
	t++;
    }
    if (*t) {
	char *u = t + strlen(t) - 1;

	while (my_isspace(*u)) {
	    *u-- = '\0';
	}
    }
    return t;
}

/*
 * Hierarchy (a>b>c) splitter.
 */
bool
split_hier(const char *label, char **base, char ***parents)
{
    int n_parents = 0;
    char *gt;
    const char *lp;

    for (lp = label; (gt = strchr(lp, '>')) != NULL; lp = gt + 1) {
	if (gt == lp) {
	    return false;
	}
	n_parents++;
    }
    if (!*lp) {
	return false;
    }

    if (n_parents) {
	*parents = (char **)Calloc(n_parents + 1, sizeof(char *));
	for (n_parents = 0, lp = label;
	     (gt = strchr(lp, '>')) != NULL;
	     lp = gt + 1) {
	    (*parents)[n_parents++] = Asprintf("%.*s", (int)(gt - lp), lp);
	}
	*base = NewString(lp);
    } else {
	(*parents) = NULL;
	(*base) = NewString(label);
    }
    return true;
}

/* Free a parent list. */
void
free_parents(char **parents)
{
    int i;

    for (i = 0; parents[i] != NULL; i++) {
	Free(parents[i]);
    }
    Free(parents);
}

#if defined(_MSC_VER) /*[*/
#define xstr(s)	str(s)
#define str(s)	#s
#endif /*]*/

/* Return configuration options. */
const char *
build_options(void)
{
    const char *p = product_specific_build_options();

    if (p == NULL) {
	p = "";
    }

    return txAsprintf("%s%s%s%s",
#if defined(X3270_LOCAL_PROCESS) /*[*/
	    "--enable-local-process"
#else /*][*/
	    "--disable-local-process"
#endif /*]*/
	    , p, using_iconv()? " --with-iconv": "",
#if defined(_MSC_VER) /*[*/
	    " via MSVC " xstr(_MSC_VER)
#endif /*]*/
#if defined(__GNUC__) /*[*/
	    " via gcc " __VERSION__
#endif /*]*/
#if defined(__LP64__) || defined(__LLP64__) /*[*/
	    " 64-bit"
#else /*][*/
	    " 32-bit"
#endif /*]*/
	    );
}

void
dump_version(void)
{
    fprintf(stderr, "%s\nBuild options: %s\n", build, build_options());
    fprintf(stderr, "TLS provider: %s\n", net_sio_provider());
    codepage_list();
    fprintf(stderr, "\n"
"Copyright 1989-%s, Paul Mattes, GTRC and others.\n"
"See the source code or documentation for licensing details.\n"
"Distributed WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n", cyear);
    fflush(stderr);
    exit(0);
}

/* Scale a number for display. */
const char *
display_scale(double d)
{
    if (d >= 1000000.0) {
	return txAsprintf("%.3g M", d / 1000000.0);
    } else if (d >= 1000.0) {
	return txAsprintf("%.3g K", d / 1000.0);
    } else {
	return txAsprintf("%.3g ", d);
    }
}

/* Add an element to a dynamically-allocated array. */
void
array_add(const char ***s, int ix, const char *v)
{
    *s = Realloc((void *)*s, (ix + 1) * sizeof(const char *));
    (*s)[ix] = v;
}

/* Clean the terminal name. */
char *
clean_termname(const char *tn)
{
    const char *s = tn;
    size_t sl;
    char *ret;

    if (tn == NULL) {
	return NULL;
    }

    while (*s && isspace((unsigned char)*s)) {
	s++;
    }
    if (!*s) {
	return NULL;
    }
    sl = strlen(s);
    ret = NewString(s);
    while (sl && isspace((unsigned char)ret[sl - 1])) {
	ret[--sl] = 0;
    }

    return ret;
}

#if defined(HAVE_START) /*[*/
void
start_help(void)
{
    /* Figure out the version. */
    const char *s = build_rpq_version;
    char *url;
    char *command = NULL;
    size_t pnl;

    pnl = strlen(programname);
#if defined(_WIN32) /*[*/
    if (pnl > 4 && !strcasecmp(programname + pnl - 4, ".exe")) {
	pnl -= 4;
    }
#endif /*]*/

    while (*s != '\0' && (*s == '.' || isdigit((unsigned char)*s))) {
	s++;
    }
    url = Asprintf("http://x3270.bgp.nu/%.*s-help/%.*s/",
	    (int)pnl, programname,
	    (int)(s - build_rpq_version), build_rpq_version);

    /* Get appropriate help. */
#if defined(_WIN32) /*[*/
    command = Asprintf("start \"%.*s help\" \"%s\"", (int)pnl, programname,
	url);
#elif defined(linux) || defined(__linux__) /*[*/
    command = Asprintf("xdg-open %s", url);
#elif defined(__APPLE__) /*][*/
    command = Asprintf("open %s", url);
#elif defined(__CYGWIN__) /*][*/
    command = Asprintf("cygstart -o %s", url);
#endif /*]*/
    if (command != NULL) {
	int rc;

	vtrace("Starting help command: %s\n", command);
	rc = system(command);
	if (rc != 0) {
	    popup_an_error("Help failed, return code %d", rc);
	}
	Free(command);
    }
    Free(url);
}
#endif /*]*/

/* Get a unit-testing-specific environment variable. */
const char *
ut_getenv(const char *name)
{
    return appres.ut_env? getenv(name): NULL;
}

/*
 * Parse a tri-state resource value.
 * Returns true for success, false for failure.
 */
bool
ts_value(const char *s, enum ts *tsp)
{
    *tsp = TS_AUTO;

    if (s != NULL && s[0]) {
	int sl = strlen(s);

	if (!strncasecmp(s, ResTrue, sl)) {
	    *tsp = TS_ON;
	} else if (!strncasecmp(s, ResFalse, sl)) {
	    *tsp = TS_OFF;
	} else if (strncasecmp(s, KwAuto, sl)) {
	    return false;
	}
    }
    return true;
}
