/*
 * Copyright (c) 1997-2009, 2014 Paul Mattes.
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
 * Quick C preprocessor substitute, for converting X3270.ad to X3270.ad.
 *
 * Understands a limited subset of #ifdef/#ifndef/#else/#endif syntax, and
 * understands -Dsym or -Usym, but not -Dsym=xxx.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_NEST	50

char *me;
int color = 0;

typedef struct sym {
	struct sym *next;
	char *name;
	int sl;
} sym_t;

sym_t *syms = NULL;

static int
is_sym(char *name)
{
	sym_t *s;
	int sl;

	sl = strlen(name);
	if (sl > 0 && name[sl - 1] == '\n')
		sl--;

	for (s = syms; s != NULL; s = s->next) {
		if (s->sl == sl && !strncmp(name, s->name, sl))
			return 1;
	}
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-v] [-Dname]... [-Uname]... "
			"[infile [outfile]]\n", me);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	char buf[1024];
	FILE *f, *t, *o;
	int nest = 0;
	int ln = 0;
	int pass[MAX_NEST];
	int elsed[MAX_NEST];
	sym_t *s, *prev;
	int verbose = 0;

	if ((me = strrchr(argv[0], '/')) != NULL)
		me++;
	else
		me = argv[0];

	while ((c = getopt(argc, argv, "D:U:v")) != -1) {
		switch (c) {
		    case 'D':
		        if (!is_sym(optarg)) {
				s = malloc(sizeof(sym_t) + strlen(optarg) + 1);
				if (s == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(1);
				}
				s->name = (char *)(s + 1);
				(void) strcpy(s->name, optarg);
				s->sl = strlen(s->name);
				s->next = syms;
				syms = s;
				if (verbose)
					printf("defined %s\n", optarg);
			}
			break;
		    case 'U':
			prev = NULL;

			for (s = syms; s != NULL; s = s->next) {
				if (!strcmp(s->name, optarg)) {
					if (prev != NULL)
						prev->next = s->next;
					else
						syms = s->next;
					free(s);
					break;
				}
				prev = s;
			}
			break;
		    case 'v':
			verbose = 1;
			break;
		    default:
			usage();
			break;
		}
	}
	switch (argc - optind) {
	    case 0:
		f = stdin;
		break;
	    case 1:
	    case 2:
		if (strcmp(argv[optind], "-")) {
			f = fopen(argv[optind], "r");
			if (f == NULL) {
				perror(argv[optind]);
				exit(1);
			}
		} else
			f = stdin;
		break;
	    default:
		usage();
		break;
	}

	t = tmpfile();
	if (t == NULL) {
		perror("tmpfile");
		exit(1);
	}

	pass[nest] = 1;

	while (fgets(buf, sizeof(buf), f) != NULL) {
		ln++;
		if (buf[0] != '#') {
			if (pass[nest])
				fprintf(t, "%s", buf);
			continue;
		}
		if (!strncmp(buf, "#ifdef ", 7)) {
			if (verbose)
				printf("%d: #ifdef %s -> %d\n", ln, buf + 7,
						is_sym(buf + 7));
			pass[nest+1] = pass[nest] && is_sym(buf + 7);
			nest++;
			elsed[nest] = 0;
		} else if (!strncmp(buf, "#ifndef ", 8)) {
			if (verbose)
				printf("%d: #ifndef %s -> %d\n", ln, buf + 8,
						!is_sym(buf + 8));
			pass[nest+1] = pass[nest] && !is_sym(buf + 8);
			nest++;
			elsed[nest] = 0;
		} else if (!strcmp(buf, "#else\n")) {
			if (!nest) {
				fprintf(stderr, "line %d: #else without #if\n",
				    ln);
				exit(1);
			}
			if (elsed[nest]) {
				fprintf(stderr, "line %d: duplicate #else\n",
				    ln);
				exit(1);
			}
			if (pass[nest])
				pass[nest] = 0;
			else if (pass[nest-1])
				pass[nest] = 1;
			elsed[nest] = 1;
		} else if (!strcmp(buf, "#endif\n")) {
			if (!nest) {
				fprintf(stderr, "line %d: #endif without #if\n",
				    ln);
				exit(1);
			}
			--nest;
		} else {
			fprintf(stderr, "line %d: unknown directive\n", ln);
			exit(1);
		}
#if 0
		fprintf(t, "! line %d nest %d pass[nest] %d\n",
		    ln, nest, pass[nest]);
#endif
	}
	if (nest > 0) {
		fprintf(stderr, "missing #endif\n");
		exit(1);
	}

	/* Close the input file, if there was one. */
	if (f != stdin)
		fclose(f);

	/* Open the output file, if there is one. */
	if (argc - optind == 2) {
		o = fopen(argv[optind + 1], "w");
		if (o == NULL) {
			perror(argv[optind + 1]);
			exit(1);
		}
	} else {
		o = stdout;
	}

	/* Copy the temp file to the output file. */
	rewind(t);
	while (fgets(buf, sizeof(buf), t) != NULL) {
		fprintf(o, "%s", buf);
	}
	fclose(t);
	if (o != stdout)
		fclose(o);

	return 0;
}
