/*
 * Copyright (c) 2001-2009, Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * x3270 font finder.
 */

#include "globals.h"
#include "resources.h"

#include "find_fontsc.h"
#include "utilc.h"

static int search(char *charset, char **matches, XFontStruct **f, int count,
    FILE *outfile);
static Boolean split14(char *s);
static void record(char *name, XFontStruct *f);
static void massage(char *charset);
static int dump(char *charset, FILE *);
static void delete_all();

/* XLFD fields */
#define FOUNDRY			0
#define FAMILY_NAME		1
#define WEIGHT_NAME		2
#define SLANT			3
#define SETWIDTH_NAME		4
#define ADD_STYLE_NAME		5
#define PIXEL_SIZE		6
#define POINT_SIZE		7
#define RESOLUTION_X		8
#define RESOLUTION_Y		9
#define SPACING			10
#define AVERAGE_WIDTH		11
#define CHARSET_REGISTRY	12
#define CHARSET_ENCODING	13
#define NUM_ATOMS		14
struct {
	char *name;
	Atom atom;
	Boolean is_string;
} atoms[] = {
	{ "FOUNDRY", 0L, True },
	{ "FAMILY_NAME", 0L, True },
	{ "WEIGHT_NAME", 0L, True },
	{ "SLANT", 0L, True },
	{ "SETWIDTH_NAME", 0L, True },
	{ "ADD_STYLE_NAME", 0L, True },
	{ "PIXEL_SIZE", 0L, False },
	{ "POINT_SIZE", 0L, False },
	{ "RESOLUTION_X", 0L, False },
	{ "RESOLUTION_Y", 0L, False },
	{ "SPACING", 0L, True },
	{ "AVERAGE_WIDTH", 0L, False },
	{ "CHARSET_REGISTRY", 0L, True },
	{ "CHARSET_ENCODING", 0L, True }
};

static char *split[14];
struct xfs {
	struct xfs *next;
	char *name;
	XFontStruct *f;
} *xfs = NULL;
Boolean verbose = False;

static void
ff_init(void)
{
	static Boolean initted = False;
	int i;

	if (!initted) {
		initted = True;

		/* Get the atoms for the font properties. */
		for (i = 0; i < NUM_ATOMS; i++) {
			atoms[i].atom = XInternAtom(display, atoms[i].name,
			    False);
		}
	}
}

#define NUM_INFOS	10

static struct {
	int base;		/* current index into f */
	int count;		/* number of elements in f */
	char *charset;		/* character set to match */
	char **matches;		/* font names */
	XFontStruct **f;	/* font properties */
	FILE *outfile;		/* output file */
	void (*progress)(int);	/* progress callback */
	void (*done)(int);	/* result function */
} work;

static Boolean
ff_work_proc(XtPointer closure _is_unused)
{
	int i;
	Boolean n_found;

	/* Read NUM_INFOS more fonts. */
	for (i = 0; i < NUM_INFOS && i + work.base < work.count; i++) {
		XFontStruct *f1;
		int cnt;

		if (verbose)
			printf("getting properties for %s\n",
			    work.matches[i+work.base]);
		if (XListFontsWithInfo(display, work.matches[i+work.base], 1,
			    &cnt, &f1) == NULL) {
			XtWarning(xs_buffer("Can't find info for font '%s'",
			    work.matches[i+work.base]));
			 work.f[work.base + i] = NULL;
		} else {
			work.f[work.base + i] = f1;
		}
	}

	/* Remember what we read. */
	work.base += i;

	/* If we're not done gathering info, return. */
	if (work.base < work.count) {
		(*work.progress)((work.base * 100) / work.count);
		return False;	/* more work to do */
	}

	/* We're done.  Look for fonts. */
	n_found = search(work.charset, work.matches, work.f, work.count,
		work.outfile);

	/* Clean up. */
	Free(work.charset);
	XFreeFontNames(work.matches);
	for (i = 0; i < work.count; i++) {
		if (work.f[i] != NULL)
			XFreeFontInfo(NULL, work.f[i], 1);
	}

	/* Report back. */
	(*work.done)(n_found);

	return True;	/* no more work to do */
}

Boolean
find_fonts(char *charset, FILE *outfile, void (*progress)(int),
    void (*done)(int))
{
	/* Initialize. */
	ff_init();

	/* Let the server display a message or two. */
	if (verbose)
		printf("Scanning:\n");

	/* Get the list of fonts from the server. */
	work.matches = XListFonts(display, "*", 32767, &work.count);
	if (work.matches == (char **)NULL) {
		if (verbose)
			printf("XListFonts returned nothing\n");
		return False;
	}

	/* Get the font info. */
	work.f = (XFontStruct **)Malloc(work.count * sizeof(XFontStruct *));

	/* Set up for background processing. */
	work.base = 0;
	work.charset = NewString(charset);
	work.outfile = outfile;
	work.progress = progress;
	work.done = done;
	XtAppAddWorkProc(appcontext, ff_work_proc, NULL);
	return True;
}

static Boolean
charset_matches(const char *charset, const char *font_registry,
    const char *font_encoding, const char *both)
{
	char *fcs;
	const char *font_charset;
	char *s = NewString(charset);
	char *ptr;
	char *cs;
	Boolean matches = False;
	char *cntxt;

	if (both == NULL) {
		fcs = xs_buffer("%s-%s", font_registry, font_encoding);
		font_charset = fcs;
	} else
		font_charset = both;
	ptr = s;
	while (!matches && (cs = strtok_r(ptr, ",", &cntxt)) != NULL) {
		ptr = NULL;
		matches = !strcasecmp(cs, font_charset);
	}
	if (both == NULL)
		Free(fcs);
	Free(s);
	return matches;
}

/*
 * Search the list of fonts for one character set.
 * Returns True if any matches were found.
 */
static int
search(char *charset, char **matches, XFontStruct **f, int count, FILE *outfile)
{
	int i;
	int n_found;

	if (verbose)
		printf("Searching for %s:\n", charset);

	for (i = 0; i < count; i++) {
		Atom a_charset_registry, a_charset_encoding;
		char *font_registry = NULL;
		char *font_encoding = NULL;
		Atom a_font_spacing;
		char *font_spacing = NULL;
		Atom a_slant;
		char *slant;
		Atom a_add_style;
		char *add_style;
		const char *mapped_charset = NULL;
		Boolean unknown = True;

		/* We might not have been able to get its properties. */
		if (f[i] == NULL)
			continue;

		/* Check the registry/encoding. */
		if (XGetFontProperty(f[i], atoms[CHARSET_REGISTRY].atom,
					&a_charset_registry))
			font_registry = XGetAtomName(display,
			    a_charset_registry);
		if (font_registry == NULL || !*font_registry)
			font_registry = "unknown";
		else
			unknown = False;
		if (XGetFontProperty(f[i], atoms[CHARSET_ENCODING].atom,
					&a_charset_encoding))
			font_encoding = XGetAtomName(display,
			    a_charset_encoding);
		if (font_encoding == NULL || !*font_encoding)
			font_encoding = "unknown";
		else
			unknown = False;
		if (unknown || (!strcmp(font_registry, "IBM 3270") &&
				!strcmp(font_encoding, "3270")))
			mapped_charset = name2cs_3270(matches[i]);

		if (!charset_matches(charset, font_registry, font_encoding,
					mapped_charset)) {
			if (verbose) {
				if (mapped_charset != CN)
					printf("%s", mapped_charset);
				else
					printf("%s-%s", font_registry,
					    font_encoding);
				printf(" (%s) doesn't match %s\n",
				    matches[i], charset);
			}
			continue;
		}

		/*
		 * Check the (named) PIXEL_SIZE, POINT_SIZE, and AVERAGE_WIDTH.
		 * These are 7th, 8th, and 12th fields (from 0).
		 * If they are 0, this is a scalable font, and we don't want
		 * to use it.
		 */
		if (split14(matches[i]) &&
		    !strcmp(split[PIXEL_SIZE], "0") &&
		    !strcmp(split[POINT_SIZE], "0") &&
		    !strcmp(split[AVERAGE_WIDTH], "0")) {
			if (verbose)
				printf("rejecting %s: scalable\n", matches[i]);
			continue;
		}

		/*
		 * Check the spacing property.
		 * It must be "c" or "m".
		 */
		if (XGetFontProperty(f[i], atoms[SPACING].atom,
					&a_font_spacing))
			font_spacing = XGetAtomName(display, a_font_spacing);
		if (!font_spacing) {
			if (verbose)
				printf("rejecting %s: spacing missing\n",
				    matches[i]);
			continue;
		}
		if (strcasecmp(font_spacing, "c") &&
		    strcasecmp(font_spacing, "m")) {
			if (verbose)
				printf("rejecting %s: spacing %s\n", matches[i],
				    font_spacing);
			continue;
		}

		/*
		 * Check the slant property.  It must be "r".
		 */
		if (XGetFontProperty(f[i], atoms[SLANT].atom, &a_slant))
			slant = XGetAtomName(display, a_slant);
		if (!slant) {
			if (verbose)
				printf("rejecting %s: slant missing\n",
				    matches[i]);
			continue;
		}
		if (strcasecmp(slant, "r")) {
			if (verbose)
				printf("rejecting %s: slant %s\n", matches[i],
				    slant);
			continue;
		}

		/*
		 * Check the add_style property.  It cannot be "Debug".
		 */
		if (XGetFontProperty(f[i], atoms[ADD_STYLE_NAME].atom,
			    &a_add_style))
			add_style = XGetAtomName(display, a_add_style);
		if (add_style && !strcasecmp(add_style, "Debug")) {
			if (verbose)
				printf("rejecting %s: add_style %s\n",
				    matches[i], add_style);
			continue;
		}

		/* Record it. */
		record(matches[i], f[i]);
	}

	/* Dump 'em. */
	if (verbose)
		printf("\nFilterting:\n");
	massage(charset);
	if (verbose)
		printf("\nFinal list:\n");
	n_found = dump(charset, outfile);

	/* Start over. */
	delete_all();

	return n_found;
}

/*
 * Split a font name into its XLFD fields.
 * Returns True if the name conforms (broadly) to XLFD.
 */
static Boolean
split14(char *s)
{
	int i;
	char *dash;
	static char *ret = NULL;

	if (s[0] != '-')
		return False;

	Free(ret);
	s = ret = NewString(s+1);

	/* Seek. */
	for (i = 0; i < 13; i++) {
		dash = strchr(s, '-');
		if (dash == NULL)
			return False;
		split[i] = s;
		*dash = '\0';
		s = dash + 1;
	}
	split[13] = s;
	if (strchr(s, '-') != NULL)
		return False;
	return True;
}

/*
 * Compare two fonts for equal properties.
 * If 'except' is >= 0, it is the XLFD index of one property to skip.
 * Returns True if they are equal.
 */
static Boolean
equal_properties(XFontStruct *x, XFontStruct *y, int except)
{
	int i;

	for (i = 0; i < NUM_ATOMS; i++) {
		Atom ax = 0L, ay = 0L;

		if (i == except)
			continue;
		(void) XGetFontProperty(x, atoms[i].atom, &ax);
		(void) XGetFontProperty(y, atoms[i].atom, &ay);
		if (strcasecmp(ax? XGetAtomName(display, ax): "",
			       ay? XGetAtomName(display, ay): ""))
			return False;
	}
	return True;
}

/*
 * Store a font in the list.
 * Rejects the font if its name is a duplicate.
 */
static void
record(char *name, XFontStruct *f)
{
	struct xfs *x;

	/*
	 * Reject duplicate names; they're inaccessable, regardless of what
	 * xlsfonts says.
	 */
	for (x = xfs; x != NULL; x = x->next) {
		if (!strcasecmp(x->name, name)) {
			if (verbose)
				printf("rejecting %s: duplicate name\n", name);
			return;
		}
	}

	if (verbose)
		printf("recording %s\n", name);
	x = (struct xfs *)Malloc(sizeof(struct xfs));
	x->name = name;
	x->f = f;
	x->next = xfs;
	xfs = x;
}

/*
 * Dump the list of fonts.
 * Based on options, will also dump the font properties, dump in x3270
 * fontList resource format, and put in a file named by the registry
 * and encoding.
 * Returns True if at least one font was listed.
 */
static int
dump(char *charset, FILE *outfile)
{
	struct xfs *x;
	int i;
	Boolean first = True;
	int n_found = 0;

	/* Check for nothing to do. */
	if (xfs == NULL)
		return 0;

	/* Create the resource definition header. */
	fprintf(outfile, "x3270." ResFontList ".%s: \\\n", charset);

	/* Walk the list. */
	for (x = xfs; x != NULL; x = x->next) {
		/* Display the font name. */
		fprintf(outfile, " ");
		if (first)
			first = False;
		else
			fprintf(outfile, "\\n\\\n ");
		fprintf(outfile, "%s", x->name);
		n_found++;

		/* Dump the properties. */
		if (!verbose)
			continue;
		printf(" (");
		for (i = 0; i < NUM_ATOMS; i++) {
			Atom a;
			char *n;

			if (XGetFontProperty(x->f, atoms[i].atom, &a)) {
				if (atoms[i].is_string) {
			    		if ((n = XGetAtomName(display, a))
					    != NULL)
						printf("-%s", n);
					else
						printf("-?");
				} else
					printf("-%ld", a);
			} else if (atoms[i].is_string)
				printf("-");
			else
				printf("-0");
		}
		printf(")\n");
	}

	fprintf(outfile, "\n");
	return n_found;
}

/*
 * Delete a font from the list.
 * This is done quite inefficiently.
 */
static void
delete_font(struct xfs *x)
{
	struct xfs *p, *prev = NULL;

	for (p = xfs; p != NULL; p = p->next) {
		if (p == x)
			break;
		prev = p;
	}
	if (p != NULL) {
		if (verbose)
			printf("deleting %s\n", x->name);
		if (prev != NULL)
			prev->next = p->next;
		else
			xfs = p->next;
		/* Try to catch stray pointers. */
		(void) memset(p, '\0', sizeof(struct xfs));
		Free(p);
	}
}

/* Free the entire font list. */
static void
delete_all(void)
{
	while (xfs != NULL) {
		struct xfs *next = xfs->next;

		Free(xfs);
		xfs = next;
	}
}

/* Prune the font list. */
static void
massage(char *charset)
{
	struct xfs *x, *y, *xn, *yn;
	struct xfs *best = NULL;
	unsigned long best_pixel_size;

	/*
	 * Get rid of outright duplicates, where the comparison is based
	 * solely on properties (not on names).  We prefer fonts with
	 * spelled-out names to those with symbolic names.
	 */
	for (x = xfs; x != NULL; x = xn) {
		xn = x->next;
		for (y = x->next; y != NULL; y = yn) {
			yn = y->next;
			if (equal_properties(x->f, y->f, -1)) {
				Boolean xs = split14(x->name);
				Boolean ys = split14(y->name);

				if (verbose)
					printf("%s and %s have the same "
					    "properties\n",
					    x->name, y->name);
				if (xs && ys) {
					/* both are XLFD */
					if (charset_matches(charset,
						    split[CHARSET_REGISTRY],
						    split[CHARSET_ENCODING],
						    NULL)) {
						/* y names the charset */
						delete_font(x);
						break;
					} else {
						/* x might; it doesn't matter */
						if (y == xn)
							xn = xn->next;
						delete_font(y);
					}
				} else if (xs && !ys) {
					/* x is XLFD, y isn't */
					if (y == xn)
						xn = xn->next;
					delete_font(y);
				} else if (ys && !xs) {
					/* y is XLFD, x isn't */
					delete_font(x);
					break;
				} else if (!xs && !ys) {
					/* neither is XLFD, pick one */
					delete_font(x);
					break;
				}
			}
		}
	}

	/*
	 * If we have both "medium" and some other variant of an otherwise
	 * identical font, get rid of everything but the "medium".
	 */
	for (x = xfs; x != NULL; x = x->next) {
		Atom a;
		char *n;

		if (XGetFontProperty(x->f, atoms[WEIGHT_NAME].atom, &a) &&
			    (n = XGetAtomName(display, a)) != NULL &&
			    !strcasecmp(n, "medium")) {
			for (y = xfs; y != NULL; y = yn) {
				yn = y->next;

				if (y == x)
					continue;
				if (equal_properties(x->f, y->f, WEIGHT_NAME)) {
					if (verbose)
						printf("%s is a variant of "
						    "%s\n", y->name, x->name);
					delete_font(y);
				}
			}
		}
	}

	/*
	 * Find the one closest to a 14-point font and put that first.
	 */
	for (x = xfs; x != NULL; x = x->next) {
		Atom a;
		long d;

		if (XGetFontProperty(x->f, atoms[PIXEL_SIZE].atom, &a)) {
			if (a == 14L) {
				best = x;
				best_pixel_size = a;
				if (verbose)
					printf("perfect size: 14\n");
				break;
			}
			if (best == NULL ||
			    (d = (labs(a - 14L) - labs(best_pixel_size - 14L))) < 0 ||
			    (d == 0L && a != best_pixel_size && a < best_pixel_size)) {
				best = x;
				best_pixel_size = a;
				if (verbose)
					printf("best size so far is %ld\n",
					    best_pixel_size);
			}
		}
	}
	if (best != NULL && xfs != best) {
		struct xfs xx = *best;

		if (verbose)
			printf("best size overall is %ld\n", best_pixel_size);
		delete_font(best);
		record(xx.name, xx.f);
	}

}

/* Charset mapping for older 3270 fonts. */
static struct {
	const char *name;
	const char *cg;
} name2cs[] = {
	{ "3270",		"3270cg-1a" },
	{ "3270-12",		"3270cg-1" },
	{ "3270-12bold",	"3270cg-1" },
	{ "3270-20",		"3270cg-1" },
	{ "3270-20bold",	"3270cg-1" },
	{ "3270bold",		"3270cg-1a" },
	{ "3270d",		"3270cg-1a" },
	{ "3270gr",		"3270cg-7" },
	{ "3270gt12",		"3270cg-1" },
	{ "3270gt12bold",	"3270cg-1" },
	{ "3270gt16",		"3270cg-1" },
	{ "3270gt16bold",	"3270cg-1" },
	{ "3270gt24",		"3270cg-1" },
	{ "3270gt24bold",	"3270cg-1" },
	{ "3270gt32",		"3270cg-1" },
	{ "3270gt32bold",	"3270cg-1" },
	{ "3270gt8",		"3270cg-1" },
	{ "3270h",		"3270cg-8" },
	{ NULL,			NULL }

};

const char *
name2cs_3270(const char *name)
{
	int i;

	for (i = 0; name2cs[i].name != NULL; i++) {
		if (!strcasecmp(name, name2cs[i].name))
			return name2cs[i].cg;
	}
	return NULL;
}
