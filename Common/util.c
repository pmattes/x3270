/*
 * Copyright (c) 1993-2015 Paul Mattes.
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
#include "resources.h"
#include "charset.h"
#include "lazya.h"
#include "product.h"
#include "varbuf.h"

#include "utils.h"

#define my_isspace(c)	isspace((unsigned char)c)

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
    r = xs_vbuffer(fmt, args);
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
    r = xs_vbuffer(fmt, args);
    va_end(args);
    Error(r);
    Free(r);
}

/* Prettyprinter for strings with unprintable data. */
void
fcatv(FILE *f, char *s)
{
    char c;

    while ((c = *s++)) {
	switch (c) {
	case '\n':
	    (void) fprintf(f, "\\n");
	    break;
	case '\t':
	    (void) fprintf(f, "\\t");
	    break;
	case '\b':
	    (void) fprintf(f, "\\b");
	    break;
	default:
	    if ((c & 0x7f) < ' ') {
		(void) fprintf(f, "\\%03o", c & 0xff);
	    } else {
		fputc(c, f);
	    }
	    break;
	}
    }
}

/* String version of fcatv. */
char *
scatv(const char *s, char *buf, size_t len)
{
    char c;
    varbuf_t r;

    vb_init(&r);
    while ((c = *s++)) {

	/* Expand this character. */
	switch (c) {
	case '\n':
	    vb_appends(&r, "\\n");
	    break;
	case '\t':
	    vb_appends(&r, "\\t");
	    break;
	case '\b':
	    vb_appends(&r, "\\b");
	    break;
	default:
	    if ((c & 0x7f) < ' ') {
		vb_appendf(&r, "\\%03o", c & 0xff);
	    } else {
		vb_append(&r, &c, 1);
	    }
	    break;
	}
    }

    /* Copy what fits. */
    (void) snprintf(buf, len, "%s", vb_buf(&r));
    vb_free(&r);

    return buf;
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

    if ((r = get_resource(lazyaf("%s.%s", ResMessage, key))) != NULL) {
	return r;
    } else {
	return lazyaf("[missing \"%s\" message]", key);
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
	return xs_buffer("%04u%02u%02u%02u%02u%02u%06u",
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
	    return xs_buffer("%u", (unsigned)getpid());
	} else {
	    return xs_buffer("%u-%u", (unsigned)getpid(), *up);
	}
    } else {
	return getenv(name);
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
			(void) strncpy(o, vn_start, vn_len);
			o += vn_len;
			state = VS_BASE;
			continue;	/* rescan */
		    }
		    vn = Malloc(vn_len + 1);
		    (void) strncpy(vn, vn_start, vn_len);
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
			(void) strcpy(o, vv);
			o += strlen(vv);
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
	(void) strncpy(mname, s, len);
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
	(void) strcpy(r, p->pw_dir);
	(void) strcat(r, rest);
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
	return xs_buffer("%s%s", t, s + 1);
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

/* A version of get_resource that accepts sprintf arguments. */
char *
get_fresource(const char *fmt, ...)
{
    va_list args;
    char *name;
    char *r;

    va_start(args, fmt);
    name = xs_vbuffer(fmt, args);
    va_end(args);
    r = get_resource(name);
    Free(name);
    return r;
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
split_hier(char *label, char **base, char ***parents)
{
    int n_parents = 0;
    char *gt;
    char *lp;

    label = NewString(label);
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
	    (*parents)[n_parents++] = lp;
	    *gt = '\0';
	}
	*base = lp;
    } else {
	(*parents) = NULL;
	(*base) = label;
    }
    return true;
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

    return lazyaf("%s%s%s",
	    "Build options:"
#if defined(X3270_DBCS) /*[*/
	    " --enable-dbcs"
#else /*][*/
	    " --disable-dbcs"
#endif /*]*/
#if defined(X3270_LOCAL_PROCESS) /*[*/
	    " --enable-local-process"
#else /*][*/
	    " --disable-local-process"
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
	    " --with-ssl"
#else /*][*/
	    " --without-ssl"
#endif /*]*/
	    , p,

#if defined(USE_ICONV) /*[*/
	    " --with-iconv"
#endif /*]*/
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
    printf("%s\n%s\n", build, build_options());
    charset_list();
    printf("\n"
"Copyright 1989-%s, Paul Mattes, GTRC and others.\n"
"See the source code or documentation for licensing details.\n"
"Distributed WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n", cyear);
    exit(0);
}

/* Scale a number for display. */
const char *
display_scale(double d)
{
    if (d >= 1000000.0) {
	return lazyaf("%.3g M", d / 1000000.0);
    } else if (d >= 1000.0) {
	return lazyaf("%.3g K", d / 1000.0);
    } else {
	return lazyaf("%.3g ", d);
    }
}
