/*
 * Copyright (c) 1993-2009, Paul Mattes.
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
#include <pwd.h>
#endif /*]*/
#include <stdarg.h>
#include "resources.h"
#include "charsetc.h"

#include "utilc.h"

#define my_isspace(c)	isspace((unsigned char)c)

/*
 * Cheesy internal version of sprintf that allocates its own memory.
 */
static char *
xs_vsprintf(const char *fmt, va_list args)
{
	char *r = CN;
#if defined(HAVE_VASPRINTF) /*[*/
	int nw;

	nw = vasprintf(&r, fmt, args);
	if (nw < 0 || r == CN)
		Error("Out of memory");
	return r;
#else /*][*/
	char buf[16384];
	int nc;

	nc = vsprintf(buf, fmt, args);
	if (nc > sizeof(buf))
		Error("Internal buffer overflow");
	r = Malloc(nc + 1);
	return strcpy(r, buf);
#endif /*]*/
}

/*
 * Common helper functions to insert strings, through a template, into a new
 * buffer.
 * 'format' is assumed to be a printf format string with '%s's in it.
 */
char *
xs_buffer(const char *fmt, ...)
{
	va_list args;
	char *r;

	va_start(args, fmt);
	r = xs_vsprintf(fmt, args);
	va_end(args);
	return r;
}

/* Common uses of xs_buffer. */
void
xs_warning(const char *fmt, ...)
{
	va_list args;
	char *r;

	va_start(args, fmt);
	r = xs_vsprintf(fmt, args);
	va_end(args);
	Warning(r);
	Free(r);
}

void
xs_error(const char *fmt, ...)
{
	va_list args;
	char *r;

	va_start(args, fmt);
	r = xs_vsprintf(fmt, args);
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
				(void) sprintf(cbuf, "\\%03o", c & 0xff);
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
	const char *f_start = CN;	/* start of sub-field */
	const char *f_end = CN;		/* end of sub-field */
	char c;
	char **rp;

	*part1 = CN;
	*part2 = CN;

	for( ; ; ) {
		c = *s;
		if (c == sep || c == '\0') {
			if (f_start == CN)
				return -1;
			if (f_end == CN)
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
			f_end = CN;
			f_start = CN;
			n_parts++;
			if (c == '\0')
				return n_parts;
		} else if (isspace(c)) {
			if (f_end == CN)
				f_end = s;
		} else {
			if (f_start == CN)
				f_start = s;
			f_end = CN;
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

	(void) sprintf(namebuf, "%s.%s", ResMessage, key);
	if ((r = get_resource(namebuf)) != CN)
		return r;
	else {
		(void) sprintf(namebuf, "[missing \"%s\" message]", key);
		return namebuf;
	}
}

#define ex_getenv getenv

/* Variable and tilde substitution functions. */
static char *
var_subst(const char *s)
{
	enum { VS_BASE, VS_QUOTE, VS_DOLLAR, VS_BRACE, VS_VN, VS_VNB, VS_EOF }
	    state = VS_BASE;
	char c;
	int o_len = strlen(s) + 1;
	char *ob;
	char *o;
	const char *vn_start = CN;

	if (strchr(s, '$') == CN)
		return NewString(s);

	o_len = strlen(s) + 1;
	ob = Malloc(o_len);
	o = ob;
#	define LBR	'{'
#	define RBR	'}'

	while (state != VS_EOF) {
		c = *s;
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
				vn_start = s;
				state = VS_VN;
			} else {
				*o++ = '$';
				*o++ = c;
				state = VS_BASE;
			}
			break;
		    case VS_BRACE:
			if (isalpha(c) || c == '_') {
				vn_start = s;
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

				vn_len = s - vn_start;
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
				if ((vv = ex_getenv(vn))) {
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
		s++;
		if (c == '\0')
			state = VS_EOF;
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
	char *mname = CN;

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
	if (p == (struct passwd *)NULL)
		r = NewString(s);
	else {
		r = Malloc(strlen(p->pw_dir) + strlen(rest) + 1);
		(void) strcpy(r, p->pw_dir);
		(void) strcat(r, rest);
	}
	return r;
}
#endif /*]*/

char *
do_subst(const char *s, Boolean do_vars, Boolean do_tilde)
{
	if (!do_vars && !do_tilde)
		return NewString(s);

	if (do_vars) {
		char *t;

		t = var_subst(s);
#if !defined(_WIN32) /*[*/
		if (do_tilde) {
			char *u;

			u = tilde_subst(t);
			Free(t);
			return u;
		}
#endif /*]*/
		return t;
	}

#if !defined(_WIN32) /*[*/
	return tilde_subst(s);
#else /*][*/
	return NewString(s);
#endif /*]*/
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
	name = xs_vsprintf(fmt, args);
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
	for (lp = label; (gt = strchr(lp, '>')) != CN; lp = gt + 1) {
		if (gt == lp)
			return False;
		n_parents++;
	}
	if (!*lp)
		return False;

	if (n_parents) {
		*parents = (char **)Calloc(n_parents + 1, sizeof(char *));
		for (n_parents = 0, lp = label;
		     (gt = strchr(lp, '>')) != CN;
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

/*
 * Incremental, reallocing version of snprintf.
 */
#define RPF_BLKSIZE	4096
#define SP_TMP_LEN	16384

/* Initialize an RPF structure. */
void
rpf_init(rpf_t *r)
{
	r->buf = NULL;
	r->alloc_len = 0;
	r->cur_len = 0;
}

/* Reset an initialized RPF structure (re-use with length 0). */
void
rpf_reset(rpf_t *r)
{
	r->cur_len = 0;
}

/* Append a string to a dynamically-allocated buffer. */
void
rpf(rpf_t *r, char *fmt, ...)
{
	va_list a;
	Boolean need_realloc = False;
	int ns;
	char tbuf[SP_TMP_LEN];

	/* Figure out how much space would be needed. */
	va_start(a, fmt);
	ns = vsprintf(tbuf, fmt, a); /* XXX: dangerous, but so is vsnprintf */
	va_end(a);
	if (ns >= SP_TMP_LEN)
	    Error("rpf overrun");

	/* Make sure we have that. */
	while (r->alloc_len - r->cur_len < ns + 1) {
		r->alloc_len += RPF_BLKSIZE;
		need_realloc = True;
	}
	if (need_realloc) {
		r->buf = Realloc(r->buf, r->alloc_len);
	}

	/* Scribble onto the end of that. */
	(void) strcpy(r->buf + r->cur_len, tbuf);
	r->cur_len += ns;
}

/* Free resources associated with an RPF. */
void
rpf_free(rpf_t *r)
{
	Free(r->buf);
	r->buf = NULL;
	r->alloc_len = 0;
	r->cur_len = 0;
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
	char *r = CN;

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
	voidfn		*fn;
	XtInputId	 id;
	struct iorec	*next;
} iorec_t;

static iorec_t *iorecs = NULL;

static void
io_fn(XtPointer closure, int *source _is_unused, XtInputId *id)
{
	iorec_t *iorec;

	for (iorec = iorecs; iorec != NULL; iorec = iorec->next) {
	    if (iorec->id == *id) {
		(*iorec->fn)();
		break;
	    }
	}
}

unsigned long
AddInput(int sock, voidfn *fn)
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

unsigned long
AddExcept(int sock, voidfn *fn)
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

unsigned long
AddOutput(int sock, voidfn *fn)
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
RemoveInput(unsigned long cookie)
{
	iorec_t *iorec;
	iorec_t *prev = NULL;

	for (iorec = iorecs; iorec != NULL; iorec = iorec->next) {
	    if (iorec->id == (XtInputId)cookie) {
		break;
	    }
	    prev = iorec;
	}

	if (iorec != NULL) {
		XtRemoveInput((XtInputId)cookie);
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
	voidfn		*fn;
	XtIntervalId	 id;
	struct torec	*next;
} torec_t;

static torec_t *torecs = NULL;

static void
to_fn(XtPointer closure, XtIntervalId *id)
{
	torec_t *torec;
	torec_t *prev = NULL;
	voidfn *fn = NULL;

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
		(*fn)();
	}
}

unsigned long
AddTimeOut(unsigned long msec, voidfn *fn)
{
	torec_t *torec;

	torec = (torec_t *)XtMalloc(sizeof(torec_t));
	torec->fn = fn;
	torec->id = XtAppAddTimeOut(appcontext, msec, to_fn, NULL);
	torec->next = torecs;
	torecs = torec;
	return (unsigned long)torec->id;
}

void
RemoveTimeOut(unsigned long cookie)
{
	torec_t *torec;
	torec_t *prev = NULL;

	for (torec = torecs; torec != NULL; torec = torec->next) {
		if (torec->id == (XtIntervalId)cookie) {
			break;
		}
		prev = torec;
	}

	if (torec != NULL) {
		XtRemoveTimeOut((XtIntervalId)cookie);
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

/* Return configuration options. */
const char *
build_options(void)
{
    	return "Build options:"
#if defined(X3270_ANSI) /*[*/
		" --enable-ansi"
#else /*][*/
		" --disable-ansi"
#endif /*]*/
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
#if defined(X3270_FT) /*[*/
		" --enable-ft"
#else /*][*/
		" --disable-ft"
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
# if defined(X3270_KEYPAD) /*[*/
		" --enable-keypad"
# else /*][*/
		" --disable-keypad"
# endif /*]*/
#endif /*]*/
#if defined(X3270_LOCAL_PROCESS) /*[*/
		" --enable-local-process"
#else /*][*/
		" --disable-local-process"
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
# if defined(X3270_MENUS) /*[*/
		" --enable-menus"
# else /*][*/
		" --disable-menus"
# endif /*]*/
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
# if defined(X3270_PRINTER) /*[*/
		" --enable-printer"
# else /*][*/
		" --disable-printer"
# endif /*]*/
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
# if defined(X3270_SCRIPT) /*[*/
		" --enable-script"
# else /*][*/
		" --disable-script"
# endif /*]*/
#endif /*]*/
#if defined(X3270_TN3270E) /*[*/
		" --enable-tn3270e"
#else /*][*/
		" --disable-tn3270e"
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
		" --enable-trace"
#else /*][*/
		" --disable-trace"
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
		;
}

void
dump_version(void)
{
	printf("%s\n%s\n", build, build_options());
	charset_list();
	printf("\n"
"Copyright 1989-2009, Paul Mattes, GTRC and others.\n"
"See the source code or documentation for licensing details.\n"
"Distributed WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	exit(0);
}
