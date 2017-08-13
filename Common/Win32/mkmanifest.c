/*
 * Copyright (c) 2017 Paul Mattes.
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
 * Construct a manifest file from a template.
 * Roughly Equivalent to the mkmanifest.sh used on Unix.
 *
 * mkmanifest
 *   -a 32|64
 *   -d description
 *   -e app-name
 *   -m manifest-template
 *   -v version-file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wincmn.h"

typedef char bool;
#define true 1
#define false 0

/* Keyword substitutions. */
#define SUBST(s)	{ s, "%" #s "%" }
enum subst {
    NAME,
    VERSION,
    ARCHITECTURE,
    DESCRIPTION,
    NUM_SUBST
};
static struct {
    enum subst subst;
    const char *keyword;
    char *value;
} substs[] = {
    SUBST(NAME),
    SUBST(VERSION),
    SUBST(ARCHITECTURE),
    SUBST(DESCRIPTION)
};

/* Allocate memory. */
void *
Malloc(size_t len)
{
    void *r;

    r = malloc(len);
    if (r == NULL) {
	fprintf(stderr, "out of memory\n");
	exit(1);
    }
    return r;
}

/* Free memory. */
void
Free(void *s)
{
    free(s);
}

/* Allocate a string. */
static char *
NewString(char *s)
{
    return strcpy(Malloc(strlen(s) + 1), s);
}

/* Display usage and exit. */
void
Usage(void)
{
    fprintf(stderr, "Usage: mkmanifest -a 32|64 -d description -e app-name -m manifest-template -v version-file\n");
    exit(1);
}

/* Parse the version string (3.2ga7) into Windows format (3.2.7.0). */
char *
parse_version(const char *version_string)
{
    enum fsm {
	BASE,
	DIG_A,
	DIG_A_DOT,
	DIG_B,
	KW,
	DIG_C
    } state = BASE;
    unsigned char c;
    char out[256];
    char *outp = out;

#   define STORE(c) { \
	if (outp - out >= sizeof(out)) { \
	    return NULL; \
	} \
	*outp++ = c; \
    }

    while ((c = *version_string++)) {
	switch (state) {
	case BASE:
	    if (isdigit(c)) {
		STORE(c);
		state = DIG_A;
	    } else {
		return NULL;
	    }
	    break;
	case DIG_A:
	    if (isdigit(c)) {
		STORE(c);
	    } else if (c == '.') {
		STORE(c);
		state = DIG_A_DOT;
	    } else {
		return NULL;
	    }
	    break;
	case DIG_A_DOT:
	    if (isdigit(c)) {
		STORE(c);
		state = DIG_B;
	    } else {
		return NULL;
	    }
	    break;
	case DIG_B:
	    if (isdigit(c)) {
		STORE(c);
	    } else {
		state = KW;
	    }
	    break;
	case KW:
	    if (isdigit(c)) {
		STORE('.');
		STORE(c);
		state = DIG_C;
	    }
	    break;
	case DIG_C:
	    if (isdigit(c)) {
		STORE(c);
	    } else {
		return NULL;
	    }
	    break;
	}
    }

    if (state != DIG_C) {
	return NULL;
    }

    STORE('.');
    STORE('0');
    STORE('\0');
    return NewString(out);
}

int
main(int argc, char *argv[])
{
    bool ia64 = false;
    FILE *f;
    char buf[1024];
    char *version = NULL;
    char *manifest = NULL;
    char *arch = NULL;
    char *version_string = NULL;
    char *manifest_version = NULL;
    char *appname = NULL;
    char *description = NULL;
    int i, j, k;

    /* Check the command line. */
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-e")) {
	    if (i + 1 >= argc) {
		Usage();
	    }
	    appname = argv[++i];
	} else if (!strcmp(argv[i], "-v")) {
	    if (i + 1 >= argc) {
		Usage();
	    }
	    version = argv[++i];
	} else if (!strcmp(argv[i], "-d")) {
	    if (i + 1 >= argc) {
		Usage();
	    }
	    description = argv[++i];
	} else if (!strcmp(argv[i], "-a")) {
	    if (i + 1 >= argc) {
		Usage();
	    }
	    arch = argv[++i];
	    if (!strcmp(arch, "32") || !strcmp(arch, "Win32")) {
		ia64 = false;
	    } else if (!strcmp(arch, "64") || !strcmp(arch, "x64")) {
		ia64 = true;
	    } else {
		Usage();
	    }
	} else if (!strcmp(argv[i], "-m")) {
	    if (i + 1 >= argc) {
		Usage();
	    }
	    manifest = argv[++i];
	} else {
	    Usage();
	}
    }
    if (appname == NULL || description == NULL || manifest == NULL
	    || arch == NULL || version == NULL) {
	Usage();
    }

    /* Read up version.txt. */
    f = fopen(version, "r");
    if (f == NULL) {
	perror(version);
	return 1;
    }
    while (fgets(buf, sizeof(buf), f) != NULL) {
	if (!strncmp(buf, "version=\"", 9)) {
	    char *q;

	    version_string = NewString(buf + 9);
	    q = strchr(version_string, '"');
	    if (q == NULL) {
		fprintf(stderr, "syntax error in %s\n", version);
		return 1;
	    }
	    *q = '\0';
	}
    }
    fclose(f);
    if (version_string == NULL) {
	fprintf(stderr, "missing version= in %s\n", version);
	return 1;
    }

    /* Translate the version. */
    manifest_version = parse_version(version_string);
    if (manifest_version == NULL) {
	fprintf(stderr, "Syntax error in version '%s'\n", version_string);
	return 1;
    }

    /* Populate the subsitutions. */
    substs[NAME].value = appname;
    substs[VERSION].value = manifest_version;
    substs[ARCHITECTURE].value = ia64? "ia64": "x86";
    substs[DESCRIPTION].value = description;

    /* Check the substitutions. */
    for (j = 0; j < NUM_SUBST; j++) {
	for (k = 0; k < NUM_SUBST; k++) {
	    if (strstr(substs[k].value, substs[j].keyword) != NULL) {
		fprintf(stderr, "Substitution '%s' contains keyword '%s'\n",
			substs[k].value, substs[j].keyword);
		return 1;
	    }
	}
    }

    /* Parse and substitute. */
    f = fopen(manifest, "r");
    if (f == NULL) {
	perror(manifest);
	return 1;
    }
    while (fgets(buf, sizeof(buf), f) != NULL) {
	int i;
	char *xbuf = NewString(buf);

	for (i = 0; i < NUM_SUBST; i++) {
	    char *s;
	    while ((s = strstr(xbuf, substs[i].keyword)) != NULL) {
		size_t left_len = s - xbuf;
		char *middle_string = substs[i].value;
		size_t middle_len = strlen(middle_string);
		char *right_string = s + strlen(substs[i].keyword);
		size_t right_len = strlen(right_string);
		size_t bufsize = left_len + middle_len + right_len + 1;
		char *ybuf = Malloc(bufsize);

		sprintf(ybuf, "%.*s%s%s",
		    (int)left_len, xbuf,
		    middle_string,
		    right_string);
		Free(xbuf);
		xbuf = ybuf;
	    }
	}
	write(1, xbuf, (int)strlen(xbuf));
	Free(xbuf);
    }

    fclose(f);
    return 0;
}

