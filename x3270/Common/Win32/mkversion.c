/*
 * Copyright (c) 2008-2009, Paul Mattes.
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
