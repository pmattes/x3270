/*
 * Copyright 2008 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * wpr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 * This program is a C-language encapsulation of all but the last line of:
 *
 *	#! /bin/sh
 *	# Create version.o from version.txt
 *
 *	# Ensure that 'date' emits 7-bit U.S. ASCII.
 *	LANG=C
 *	LC_ALL=C
 *	export LANG LC_ALL
 *
 *	set -e
 *
 *	. ./version.txt
 *	builddate=`date`
 *	sccsdate=`date +%Y/%m/%d`
 *	user=${LOGNAME-$USER}
 *
 *	# Create an all numeric timestamp for rpqnames.
 *	# rpq.c will return this string of numbers in bcd format
 *	# It is OK to change the length (+ or -), but use
 *	# decimal (0-9) digits only. Length must be even number of digits.
 *	rpq_timestamp=`date +%Y%m%d%H%M%S`
 *
 *	trap 'rm -f version.c' 0 1 2 15
 *
 *	cat <<EOF >version.c
 *	char *build = "${2-x3270} v$version $builddate $user";
 *	char *app_defaults_version = "$adversion";
 *	static char sccsid[] = "@(#)${2-x3270} v$version $sccsdate $user";
 *
 *	const char *build_rpq_timestamp = "$rpq_timestamp";
 *	const char *build_rpq_version = "$version";
 *	EOF
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *
NewString(char *s)
{
	char *t = malloc(strlen(s) + 1);

	if (t == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	return strcpy(t, s);
}

int
main(int argc, char *argv[])
{
	FILE *f;
	char buf[1024];
	char *version = NULL;
	char *adversion = NULL;
	char *user;
	__time64_t t;
	char *builddate;
	struct tm *tm;
	char sccsdate[128];
	char rpqtime[128];
	int is_w = 0;
	char *ofile = "version.c";
	char *progname = "wc3270";

	if (argc > 1 && !strcmp(argv[1], "-w")) {
		is_w = 1;
		ofile = "wversion.c";
		argv++;
		argc--;
	}
	if (argc > 1)
		progname = argv[1];

	/* Read up version.txt. */
	f = fopen("version.txt", "r");
	if (f == NULL) {
		perror("version.txt");
		return 1;
	}
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (!strncmp(buf, "version=\"", 9)) {
			char *q;

			version = NewString(buf + 9);
			q = strchr(version, '"');
			if (q == NULL) {
				fprintf(stderr,
					"syntax error in version.txt\n");
				return 1;
			}
			*q = '\0';
		} else if (!strncmp(buf, "adversion=\"", 11)) {
			char *q;

			adversion = NewString(buf + 11);
			q = strchr(adversion, '"');
			if (q == NULL) {
				fprintf(stderr,
					"syntax error in version.txt\n");
				return 1;
			}
			*q = '\0';
		}
	}
	fclose(f);
	if (version == NULL || adversion == NULL) {
		fprintf(stderr,
			"missing version= or adversion= in version.txt\n");
		return 1;
	}

	/* Grab the username. */
	user = getenv("USERNAME");
	if (user == NULL) {
		fprintf(stderr, "No %USERNAME%?\n");
		return 1;
	}

	/* Format the dates. */
	_time64(&t);
	builddate = NewString(_ctime64(&t));
	builddate[strlen(builddate) - 1] = '\0';
	tm = _localtime64(&t);
	sprintf(sccsdate, "%d/%02d/%02d",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday);
	sprintf(rpqtime, "%02d%02d%02d%02d%02d",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec);

	/* Create the code. */
	f = fopen(ofile, "w");
	if (f == NULL) {
		perror(ofile);
		return 1;
	}
	if (is_w) {
		fprintf(f, "char *wversion = \"%s\";\n", version);
	} else {
		fprintf(f, "char *build = \"%s v%s %s %s\";\n",
			progname, version, builddate, user);
		fprintf(f, "char *app_defaults_version = \"%s\";\n",
			adversion);
		fprintf(f, "static char sccsid[] = \"@(#)%s v%s %s %s\";\n",
			progname, version, sccsdate, user);
		fprintf(f, "const char *build_rpq_timestamp = \"%s\";\n",
			rpqtime);
		fprintf(f, "const char *build_rpq_version = \"%s\";\n",
			version);
	}
	fclose(f);

	return 0;
}
