/*
 * Copyright (c) 2001-2009, 2014 Paul Mattes.
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
 * Finds fonts that implement a specified character set.
 * Based (almost entirely) on properties, rather than the XLFD name fields.
 *
 * Options:
 *  -charset <registry>-<encoding>[,<registry>-<encoding>...][ ...]
 *    (required) The desired character set(s).
 *  -pattern <pattern>
 *    Font pattern to search (default of "*" is recommended).
 *  -verbose
 *    Explain what's going on.
 *  -byname
 *    Search by name, rather than by properties (not recommended).
 *  -list
 *    Output in x3270 resizeFontList format.
 *  -file
 *    Output to file(s) named by the <registry>-<encoding>.
 */

#include "X11/Intrinsic.h"
#include "X11/StringDefs.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Options. */
XrmOptionDescRec options[]= {
	{ "-charset",	".charset",	XrmoptionSepArg,	NULL },
	{ "-pattern",	".pattern",	XrmoptionSepArg,	NULL },
	{ "-verbose",	".verbose", 	XrmoptionNoArg,		"True" },
	{ "-byname",	".byName", 	XrmoptionNoArg,		"True" },
	{ "-list",	".list",	XrmoptionNoArg,		"True" },
	{ "-file",	".file",	XrmoptionNoArg,		"True" }
};
int num_options = XtNumber(options);

/* Application resources. */
typedef struct {
	char *charset;
	char *pattern;
	Boolean verbose;
	Boolean byname;
	Boolean list;
	Boolean file;
} appres_t, *appresp_t;
appres_t appres;
XtResource resources[] = {
	{ "charset", "Charset", XtRString, sizeof(String),
	  XtOffset(appresp_t, charset), XtRString, "unknown" },
	{ "pattern", "Pattern", XtRString, sizeof(String),
	  XtOffset(appresp_t, pattern), XtRString, "*" },
	{ "verbose", "Verbose", XtRBoolean, sizeof(Boolean),
	  XtOffset(appresp_t, verbose), XtRString, "False" },
	{ "byName", "ByName", XtRBoolean, sizeof(Boolean),
	  XtOffset(appresp_t, byname), XtRString, "False" },
	{ "list", "List", XtRBoolean, sizeof(Boolean),
	  XtOffset(appresp_t, list), XtRString, "False" },
	{ "file", "File", XtRBoolean, sizeof(Boolean),
	  XtOffset(appresp_t, file), XtRString, "False" }
};
Cardinal num_resources = XtNumber(resources);
String fallbacks[] = { NULL };
Display *display;
char *charset;
char *split[14];
struct xfs {
	struct xfs *next;
	char *name;
	XFontStruct *f;
} *xfs = NULL;

static Boolean search(char **matches, XFontStruct **f, int count);
static Boolean split14(char *s);
static void record(char *name, XFontStruct *f);
static void massage(void);
static Boolean dump(void);
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

int
main(int argc, char *argv[])
{
	XtAppContext appcontext;
	Widget toplevel;
	char buf[1024];
	char *pattern;
	char *ptr;
	char *cntxt;

	char **matches;
	int count;
	int i;
	XFontStruct **f;

	Boolean any = False;

	/* Connect to server, process command-line options, get resources. */
	toplevel = XtVaAppInitialize(
	    &appcontext,
	    "X3ff",
	    options, num_options,
	    &argc, argv,
	    fallbacks,
	    NULL);
	if (argc > 1) {
		fprintf(stderr, "Unknown or incomplete option: '%s'\n",
		    argv[1]);
		exit(1);
	}
	display = XtDisplay(toplevel);
	XtGetApplicationResources(toplevel, (XtPointer)&appres, resources,
	    num_resources, 0, 0);

	/* Get the atoms for the font properties. */
	for (i = 0; i < NUM_ATOMS; i++) {
		atoms[i].atom = XInternAtom(display, atoms[i].name, False);
	}

	/* Make sure they reqested a character set. */
	if (!strcmp(appres.charset, "unknown")) {
		fprintf(stderr, "Must specify -charset.\n");
		exit(1);
	}

	/* Set up the search pattern. */
	if (appres.byname) {
		(void) sprintf(buf, "*-*-*-*-r-*-*-*-*-*-*-*-*-%s",
		    appres.charset);
		pattern = buf;
	} else
		pattern = appres.pattern;

	/* Let the server display a message or two. */
	if (appres.verbose)
		printf("Scanning:\n");

	/* Get the list of fonts from the server. */
	matches = XListFonts(display, pattern, 32767, &count);
	if (matches == NULL) {
		fprintf(stderr, "No fonts match pattern '%s'.\n",
		    appres.pattern);
		exit(1);
	}

	/* Get the font info. */
	f = (XFontStruct **)XtMalloc(count * sizeof(XFontStruct *));
	for (i = 0; i < count; i++) {
		XFontStruct *f1;
		int cnt;

		if (XListFontsWithInfo(display, matches[i], 1, &cnt, &f1) ==
		    NULL) {
			XtError("Can't find info for font?");
		}
		f[i] = f1;
	}

	/* Process the list for each requested group of character sets. */
	ptr = XtNewString(appres.charset);
	while ((charset = strtok_r(ptr, " ", &cntxt)) != NULL) {
		ptr = NULL;
		any |= search(matches, f, count);
	}

	XFreeFontNames(matches);
	for (i = 0; i < count; i++) {
		XFreeFontInfo(NULL, f[i], 1);
	}

	exit(any? 0: 1);
}

static Boolean
charset_matches(char *font_registry, char *font_encoding)
{
	char *font_charset = XtMalloc(strlen(font_registry) + 1 +
				      strlen(font_encoding) + 1);
	char *s = XtNewString(charset);
	char *ptr;
	char *cs;
	Boolean matches = False;
	char *cntxt;

	(void) sprintf(font_charset, "%s-%s", font_registry, font_encoding);
	ptr = s;
	while (!matches && (cs = strtok_r(ptr, ",", &cntxt)) != NULL) {
		ptr = NULL;
		matches = !strcasecmp(cs, font_charset);
	}
	XtFree(font_charset);
	XtFree(s);
	return matches;
}

/*
 * Search the list of fonts for one character set.
 * Returns True if any matches were found.
 */
static Boolean
search(char **matches, XFontStruct **f, int count)
{
	int i;
	Boolean any;

	if (appres.verbose)
		printf("Searching for %s:\n", charset);

	for (i = 0; i < count; i++) {
		Atom a_charset_registry, a_charset_encoding;
		char *font_registry = NULL;
		char *font_encoding = NULL;
		Atom a_font_spacing;
		char *font_spacing = NULL;
		Atom a_slant;
		char *slant;

		/* Check the registry/encoding. */
		if (XGetFontProperty(f[i], atoms[CHARSET_REGISTRY].atom,
					&a_charset_registry))
			font_registry = XGetAtomName(display,
			    a_charset_registry);
		if (font_registry == NULL)
			font_registry = "unknown";
		if (XGetFontProperty(f[i], atoms[CHARSET_ENCODING].atom,
					&a_charset_encoding))
			font_encoding = XGetAtomName(display,
			    a_charset_encoding);
		if (font_encoding == NULL)
			font_encoding = "unknown";
		if (!charset_matches(font_registry, font_encoding))
			continue;

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
			if (appres.verbose)
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
			if (appres.verbose)
				printf("rejecting %s: spacing missing\n",
				    matches[i]);
			continue;
		}
		if (strcasecmp(font_spacing, "c") &&
		    strcasecmp(font_spacing, "m")) {
			if (appres.verbose)
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
			if (appres.verbose)
				printf("rejecting %s: slant missing\n",
				    matches[i]);
			continue;
		}
		if (strcasecmp(slant, "r")) {
			if (appres.verbose)
				printf("rejecting %s: slant %s\n", matches[i],
				    slant);
			continue;
		}

		/* Record it. */
		record(matches[i], f[i]);
	}

	/* Dump 'em. */
	if (appres.verbose)
		printf("\nFilterting:\n");
	massage();
	if (appres.verbose)
		printf("\nFinal list:\n");
	any = dump();

	/* Start over. */
	delete_all();

	return any;
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

	if (ret != NULL)
		XtFree(ret);
	s = ret = XtNewString(s+1);

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
			if (appres.verbose)
				printf("rejecting %s: duplicate name\n", name);
			return;
		}
	}

	if (appres.verbose)
		printf("recording %s\n", name);
	x = (struct xfs *)XtMalloc(sizeof(struct xfs));
	x->name = name;
	x->f = f;
	x->next = xfs;
	xfs = x;
}

/*
 * Dump the list of fonts.
 * Based on options, will also dump the font properties, dump in x3270
 * resizeFontList resource format, and put in a file named by the registry
 * and encoding.
 * Returns True if at least one font was listed.
 */
static Boolean
dump(void)
{
	struct xfs *x;
	int i;
	Boolean first = True;
	FILE *f;

	/* Check for nothing to do. */
	if (xfs == NULL) {
		fprintf(stderr, "No %s fonts found.\n", charset);
		return False;
	}

	/* Create the file, if needed. */
	if (appres.file) {
		char filename[1024];

		(void) sprintf(filename, "%s", charset);
		f = fopen(filename, "w");
		if (f == NULL) {
			perror(filename);
			return False;
		}
	} else
		f = stdout;

	/* Create the resource definition header. */
	if (appres.list)
		fprintf(f, "x3270.resizeFontList.%s: \\\n", charset);

	/* Walk the list. */
	for (x = xfs; x != NULL; x = x->next) {
		/* Display the font name. */
		if (appres.list)
			fprintf(f, " ");
		if (first)
			first = False;
		else {
			if (appres.list)
				fprintf(f, "\\n\\");
			fprintf(f, "\n");
			if (appres.list)
				fprintf(f, " ");
		}
		fprintf(f, "%s", x->name);

		/* Dump the properties. */
		if (!appres.verbose)
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
		printf(")");
	}

	fprintf(f, "\n");
	if (appres.file)
		(void) fclose(f);
	return True;
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
		if (appres.verbose)
			printf("deleting %s\n", x->name);
		if (prev != NULL)
			prev->next = p->next;
		else
			xfs = p->next;
		XtFree((XtPointer)p);
	}
}

/* Free the entire font list. */
static void
delete_all(void)
{
	while (xfs != NULL) {
		struct xfs *next = xfs->next;

		XtFree((XtPointer)xfs);
		xfs = next;
	}
}

/* Prune the font list. */
static void
massage(void)
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

				if (appres.verbose)
					printf("%s and %s have the same "
					    "properties\n",
					    x->name, y->name);
				if (xs && ys) {
					/* both are XLFD */
					if (charset_matches(
						    split[CHARSET_REGISTRY],
						    split[CHARSET_ENCODING])) {
						/* y names the charset */
						delete_font(x);
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
					if (appres.verbose)
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
				if (appres.verbose)
					printf("perfect size: 14\n");
				break;
			}
			if (best == NULL ||
			    (d = (labs(a - 14L) - labs(best_pixel_size - 14L))) < 0 ||
			    (d == 0L && a != best_pixel_size && a < best_pixel_size)) {
				best = x;
				best_pixel_size = a;
				if (appres.verbose)
					printf("best size so far is %ld\n",
					    best_pixel_size);
			}
		}
	}
	if (best != NULL && xfs != best) {
		struct xfs xx = *best;

		if (appres.verbose)
			printf("best size overall is %ld\n", best_pixel_size);
		delete_font(best);
		record(xx.name, xx.f);
	}

}
