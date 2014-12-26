/*
 * Copyright (c) 1993-2014, Paul Mattes.
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
#if !defined(PR3287) /*[*/
# include "resources.h"
#endif /*]*/
#if defined(WC3270) /*[*/
# include "appres.h"
# include "screenc.h"
#endif /*]*/
#include "asprintfc.h"
#include "charsetc.h"

#include "utilc.h"

#define my_isspace(c)	isspace((unsigned char)c)

/**
 * Local variation of vasprintf(). Returns the buffer instead of a count, and
 * crashes if it runs out of memory.
 *
 * @param[in] fmt	printf format string
 * @param[in] args	argument list
 *
 * @return malloc'd buffer, guaranteed not to be NULL. Must free() when done.
 */
char *
xs_vbuffer(const char *fmt, va_list args)
{
    char *r = NULL;
    int nw;

    nw = vasprintf(&r, fmt, args);
    if (nw < 0) {
	Error("xs_vbuffer: vasprintf failure");
    }
    if (r == NULL) {
	Error("Out of memory");
    }
    return r;
}

/**
 * Local variation of vsprintf(). Returns the buffer instead of a count, and
 * crashes if it runs out of memory.
 *
 * @param[in] fmt	printf format string
 *
 * @return malloc'd buffer, guaranteed not to be NULL. Must free() when done.
 */
char *
xs_buffer(const char *fmt, ...)
{
    va_list args;
    char *r;

    va_start(args, fmt);
    r = xs_vbuffer(fmt, args);
    va_end(args);
    return r;
}

#if !defined(PR3287) /*[*/

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
			if ((c & 0x7f) < ' ')
				(void) fprintf(f, "\\%03o", c & 0xff);
			else
				fputc(c, f);
			break;
		}
	}
}

/* String version of fcatv. */
char *
scatv(const char *s, char *buf, size_t len)
{
	char c;
	char *dst = buf;

	while ((c = *s++) && len > 0) {
		char cbuf[5];
		char *t = cbuf;

		/* Expand this character. */
		switch (c) {
		    case '\n':
			(void) strcpy(cbuf, "\\n");
			break;
		    case '\t':
			(void) strcpy(cbuf, "\\t");
			break;
		    case '\b':
			(void) strcpy(cbuf, "\\b");
			break;
		    default:
			if ((c & 0x7f) < ' ')
				(void) snprintf(cbuf, sizeof(cbuf), "\\%03o",
					c & 0xff);
			else {
				cbuf[0] = c;
				cbuf[1] = '\0';
			}
			break;
		}
		/* Copy as much as will fit. */
		while ((c = *t++) && len > 0) {
			*dst++ = c;
			len--;
		}
	}
	if (len > 0)
		*dst = '\0';

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
	Boolean quote;

	/* Skip leading white space. */
	while (my_isspace(*s))
		s++;

	/* If nothing left, EOF. */
	if (!*s)
		return 0;

	/* There must be a left-hand side. */
	if (*s == ':')
		return -1;

	/* Scan until an unquoted colon is found. */
	*left = s;
	for (; *s && *s != ':' && *s != '\n'; s++)
		if (*s == '\\' && *(s+1) == ':')
			s++;
	if (*s != ':')
		return -1;

	/* Stip white space before the colon. */
	for (t = s-1; my_isspace(*t); t--)
		*t = '\0';

	/* Terminate the left-hand side. */
	*(s++) = '\0';

	/* Skip white space after the colon. */
	while (*s != '\n' && my_isspace(*s))
		s++;

	/* There must be a right-hand side. */
	if (!*s || *s == '\n')
		return -1;

	/* Scan until an unquoted newline is found. */
	*right = s;
	quote = False;
	for (; *s; s++) {
		if (*s == '\\' && *(s+1) == '"')
			s++;
		else if (*s == '"')
			quote = !quote;
		else if (!quote && *s == '\n')
			break;
	}

	/* Strip white space before the newline. */
	if (*s) {
		t = s;
		*st = s+1;
	} else {
		t = s-1;
		*st = s;
	}
	while (my_isspace(*t))
		*t-- = '\0';

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
	const char *f_end = NULL;		/* end of sub-field */
	char c;
	char **rp;

	*part1 = NULL;
	*part2 = NULL;

	for( ; ; ) {
		c = *s;
		if (c == sep || c == '\0') {
			if (f_start == NULL)
				return -1;
			if (f_end == NULL)
				f_end = s;
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
				} else
					return n_parts;
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
			if (c == '\0')
				return n_parts;
		} else if (isspace(c)) {
			if (f_end == NULL)
				f_end = s;
		} else {
			if (f_start == NULL)
				f_start = s;
			f_end = NULL;
		}
		s++;
	}
}

#if defined(X3270_DISPLAY) /*[*/
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
	Boolean quote;

	/* Skip leading white space. */
	while (my_isspace(*s))
		s++;

	/* If nothing left, EOF. */
	if (!*s)
		return 0;

	/* Save starting point. */
	*value = s;

	/* Scan until an unquoted newline is found. */
	quote = False;
	for (; *s; s++) {
		if (*s == '\\' && *(s+1) == '"')
			s++;
		else if (*s == '"')
			quote = !quote;
		else if (!quote && *s == '\n')
			break;
	}

	/* Strip white space before the newline. */
	if (*s) {
		t = s;
		*st = s+1;
	} else {
		t = s-1;
		*st = s;
	}
	while (my_isspace(*t))
		*t-- = '\0';

	/* Done. */
	return 1;
}
#endif /*]*/

const char *
get_message(const char *key)
{
	static char namebuf[128];
	char *r;

	(void) snprintf(namebuf, sizeof(namebuf), "%s.%s", ResMessage, key);
	if ((r = get_resource(namebuf)) != NULL)
		return r;
	else {
		(void) snprintf(namebuf, sizeof(namebuf),
			"[missing \"%s\" message]", key);
		return namebuf;
	}
}

static char *
ex_getenv(const char *name, unsigned long flags, int *up)
{
	if (!strcasecmp(name, "TIMESTAMP")) {
		/* YYYYMMDDHHMMSSUUUUUU */
		static char ts[21];
		struct timeval tv;
		time_t t; /* on Windows, timeval.tv_sec is a long */
		struct tm *tm;

		if (gettimeofday(&tv, NULL) < 0)
			return NewString("?");
		t = tv.tv_sec;
		tm = localtime(&t);
		(void) snprintf(ts, sizeof(ts),
			"%04u%02u%02u%02u%02u%02u%06u",
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec,
			(unsigned)tv.tv_usec);
		return NewString(ts);
	} else if (!strcasecmp(name, "UNIQUE")) {
		char buf[64];

		++*up;
		if (*up == 0)
			(void) snprintf(buf, sizeof(buf), "%u",
				(unsigned)getpid());
		else
			(void) snprintf(buf, sizeof(buf), "%u-%u",
				(unsigned)getpid(), *up);
		return NewString(buf);
	} else {
		return getenv(name);
	}
}

/* Variable and tilde substitution functions. */
/*static*/ char *
var_subst(const char *s, unsigned long flags)
{
	const char *t;
	enum { VS_BASE, VS_QUOTE, VS_DOLLAR, VS_BRACE, VS_VN, VS_VNB, VS_EOF }
	    state;
	char c;
	int o_len;
	char *ob;
	char *o;
	const char *vn_start;
	int u = -1;
#	define LBR	'{'
#	define RBR	'}'

	if (strchr(s, '$') == NULL)
		return NewString(s);

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
				if (c == '\\')
				    state = VS_QUOTE;
				else if (c == '$')
				    state = VS_DOLLAR;
				else
				    *o++ = c;
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
				if (c == LBR)
					state = VS_BRACE;
				else if (isalpha(c) || c == '_') {
					vn_start = t;
					state = VS_VN;
				} else {
					*o++ = '$';
					*o++ = c;
					state = VS_BASE;
				}
				break;
			    case VS_BRACE:
				if (isalpha(c) || c == '_') {
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
				if (!(isalnum(c) || c == '_')) {
					int vn_len;
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
			if (c == '\0')
				state = VS_EOF;
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
		} else
		    	break;
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
	if (*s != '~')
		return NewString(s);

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
	if (!strcmp(name, "~"))	/* this user */
		p = getpwuid(getuid());
	else			/* somebody else */
		p = getpwnam(name + 1);

	/* Free any temporary copy. */
	Free(mname);

	/* Substitute and return. */
	if (p == NULL)
		r = NewString(s);
	else {
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

	if (*s != '~' || (t = getenv("HOMEPATH")) == NULL)
		return NewString(s);

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
	if (flags == DS_NONE)
		return NewString(s);

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
	static char	buf[64];
	char	*p = buf;

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

	while (*t && my_isspace(*t))
		t++;
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
Boolean
split_hier(char *label, char **base, char ***parents)
{
	int n_parents = 0;
	char *gt;
	char *lp;

	label = NewString(label);
	for (lp = label; (gt = strchr(lp, '>')) != NULL; lp = gt + 1) {
		if (gt == lp)
			return False;
		n_parents++;
	}
	if (!*lp)
		return False;

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
	return True;
}

#if defined(X3270_DISPLAY) /*[*/

/* Glue between x3270 and the X libraries. */

/*
 * A way to work around problems with Xt resources.  It seems to be impossible
 * to get arbitrarily named resources.  Someday this should be hacked to
 * add classes too.
 */
char *
get_resource(const char *name)
{
	XrmValue value;
	char *type;
	char *str;
	char *r = NULL;

	str = xs_buffer("%s.%s", XtName(toplevel), name);
	if ((XrmGetResource(rdb, str, 0, &type, &value) == True) && *value.addr)
		r = value.addr;
	XtFree(str);
	return r;
}

/*
 * Input callbacks.
 */
typedef void voidfn(void);

typedef struct iorec {
	iofn_t 		 fn;
	XtInputId	 id;
	struct iorec	*next;
} iorec_t;

static iorec_t *iorecs = NULL;

static void
io_fn(XtPointer closure, int *source, XtInputId *id)
{
	iorec_t *iorec;

	for (iorec = iorecs; iorec != NULL; iorec = iorec->next) {
		if (iorec->id == *id) {
			(*iorec->fn)(*source, *id);
			break;
		}
	}
}

ioid_t
AddInput(unsigned long sock, iofn_t fn)
{
	iorec_t *iorec;

	iorec = (iorec_t *)XtMalloc(sizeof(iorec_t));
	iorec->fn = fn;
	iorec->id = XtAppAddInput(appcontext, sock,
		(XtPointer) XtInputReadMask, io_fn, NULL);

	iorec->next = iorecs;
	iorecs = iorec;

	return iorec->id;
}

ioid_t
AddExcept(unsigned long sock, iofn_t fn)
{
	iorec_t *iorec;

	iorec = (iorec_t *)XtMalloc(sizeof(iorec_t));
	iorec->fn = fn;
	iorec->id = XtAppAddInput(appcontext, sock,
		(XtPointer) XtInputExceptMask, io_fn, NULL);
	iorec->next = iorecs;
	iorecs = iorec;

	return iorec->id;
}

ioid_t
AddOutput(unsigned long sock, iofn_t fn)
{
	iorec_t *iorec;

	iorec = (iorec_t *)XtMalloc(sizeof(iorec_t));
	iorec->fn = fn;
	iorec->id = XtAppAddInput(appcontext, sock,
		(XtPointer) XtInputWriteMask, io_fn, NULL);
	iorec->next = iorecs;
	iorecs = iorec;

	return iorec->id;
}

void
RemoveInput(ioid_t cookie)
{
	iorec_t *iorec;
	iorec_t *prev = NULL;

	for (iorec = iorecs; iorec != NULL; iorec = iorec->next) {
	    if (iorec->id == cookie) {
		break;
	    }
	    prev = iorec;
	}

	if (iorec != NULL) {
		XtRemoveInput(cookie);
		if (prev != NULL)
			prev->next = iorec->next;
		else
			iorecs = iorec->next;
		XtFree((XtPointer)iorec);
	}
}

/*
 * Timer callbacks.
 */

typedef struct torec {
	tofn_t		 fn;
	XtIntervalId	 id;
	struct torec	*next;
} torec_t;

static torec_t *torecs = NULL;

static void
to_fn(XtPointer closure, XtIntervalId *id)
{
	torec_t *torec;
	torec_t *prev = NULL;
	tofn_t fn = NULL;

	for (torec = torecs; torec != NULL; torec = torec->next) {
		if (torec->id == *id) {
			break;
		}
		prev = torec;
	}

	if (torec != NULL) {

	    	/* Remember the record. */
		fn = torec->fn;

		/* Free the record. */
		if (prev != NULL)
			prev->next = torec->next;
		else
			torecs = torec->next;
		XtFree((XtPointer)torec);

		/* Call the function. */
		(*fn)((ioid_t)id);
	}
}

ioid_t
AddTimeOut(unsigned long msec, tofn_t fn)
{
	torec_t *torec;

	torec = (torec_t *)XtMalloc(sizeof(torec_t));
	torec->fn = fn;
	torec->id = XtAppAddTimeOut(appcontext, msec, to_fn, NULL);
	torec->next = torecs;
	torecs = torec;
	return torec->id;
}

void
RemoveTimeOut(ioid_t cookie)
{
	torec_t *torec;
	torec_t *prev = NULL;

	for (torec = torecs; torec != NULL; torec = torec->next) {
		if (torec->id == cookie) {
			break;
		}
		prev = torec;
	}

	if (torec != NULL) {
		XtRemoveTimeOut(cookie);
		if (prev != NULL)
			prev->next = torec->next;
		else
			torecs = torec->next;
		XtFree((XtPointer)torec);
	} else {
		Error("RemoveTimeOut: Can't find");
	}
}

KeySym
StringToKeysym(char *s)
{
	return XStringToKeysym(s);
}
#endif /*]*/

#if defined(_MSC_VER) /*[*/
#define xstr(s)	str(s)
#define str(s)	#s
#endif /*]*/

/* Return configuration options. */
const char *
build_options(void)
{
    	return "Build options:"
#if defined(X3270_APL) /*[*/
		" --enable-apl"
#else /*][*/
		" --disable-apl"
#endif /*]*/
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
#if defined(C3270) /*[*/
# if defined(HAVE_LIBREADLINE) /*[*/
		" --with-readline"
# else /*][*/
		" --without-readline"
# endif /*]*/
# if !defined(_WIN32) /*[*/
#  if defined(CURSES_WIDE) /*[*/
		" --with-curses-wide"
#  else /*][*/
		" --without-curses-wide"
#  endif /*]*/
# endif /*]*/
#endif /*]*/
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
		;
}

void
dump_version(void)
{
	printf("%s\n%s\n", build, build_options());
	charset_list();
	printf("\n"
"Copyright 1989-2014, Paul Mattes, GTRC and others.\n"
"See the source code or documentation for licensing details.\n"
"Distributed WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	exit(0);
}

/* Scale a number for display. */
const char *
display_scale(double d, char *buf, size_t buflen)
{
	if (d >= 1000000.0)
		snprintf(buf, buflen, "%.3g M", d / 1000000.0);
	else if (d >= 1000.0)
		snprintf(buf, buflen, "%.3g K", d / 1000.0);
	else
		snprintf(buf, buflen, "%.3g ", d);

	/* Don't trust snprintf. */
	buf[buflen - 1] = '\0';

	return buf;
}

#if defined(WC3270) /*[*/
void
start_html_help(void)
{
	char *cmd;

	cmd = xs_buffer("start \"wc3270 Help\" \"%shtml\\README.html\"",
		instdir);
	system(cmd);
	Free(cmd);
	screen_fixup(); /* get back mouse events */
}
#endif /*]*/

/*
 * Doubly-linked lists.
 */
Boolean
llist_isempty(llist_t *l)
{
    return l->next == l && l->prev == l;
}

void
llist_init(llist_t *l)
{
    l->next = l->prev = l;
}

void
llist_insert_before(llist_t *element, llist_t *before)
{
    element->next = before;
    element->prev = before->prev;
    before->prev->next = element;
    before->prev = element;
}

void
llist_unlink(llist_t *element)
{
    element->next->prev = element->prev;
    element->prev->next = element->next;
}

#endif /*]*/
