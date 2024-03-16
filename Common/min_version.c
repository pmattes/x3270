/*
 * Copyright (c) 2015-2024 Paul Mattes.
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
 *	min_version.c
 *		Minimum version checker.
 */

#include "globals.h"

#include "min_version.h"
#include "resources.h"

int our_major, our_minor, our_iteration;

/**
 * Parse a version number.
 * @verbatim
 * Version numbers are of the form: <major>.<minor>text<iteration>, such as
 *  3.4ga10 (3, 4, 10)
 *  3.5apha3 (3, 5, 3)
 * The version can be under-specified, e.g.:
 *  3.4 (3, 4, 0)
 *  3 (3, 0, 0)
 * Numbers are limited to 0..999.
 * @endverbatim
 * @param[in] text		String to decode.
 * @param[out] major		Major number.
 * @param[out] minor		Minor number.
 * @param[out] iteration	Iteration.
 *
 * @return true if parse successful.
 */
#define MAX_VERSION 999
static bool
parse_version(const char *text, int *major, int *minor, int *iteration)
{
    const char *t = text;
    unsigned long n;
    char *ptr;

    *major = 0;
    *minor = 0;
    *iteration = 0;

    /* Parse the major number. */
    n = strtoul(t, &ptr, 10);
    if (ptr == t || (*ptr != '.' && *ptr != '\0') || n > MAX_VERSION) {
	return false;
    }
    *major = (int)n;

    if (*ptr == '\0') {
	/* Just a major number. */
	return true;
    }

    /* Parse the minor number. */
    t = ptr + 1;
    n = strtoul(t, &ptr, 10);
    if (ptr == text || n > MAX_VERSION) {
	return false;
    }
    *minor = (int)n;

    if (*ptr == '\0') {
	/* Just a major and minor number. */
	return true;
    }

    /* Parse the iteration. */
    t = ptr;
    while (!isdigit((unsigned char)*t) && *t != '\0')
    {
	t++;
    }
    if (*t == '\0') {
	return false;
    }

    n = strtoul(t, &ptr, 10);
    if (ptr == t || *ptr != '\0' || n > MAX_VERSION) {
	return false;
    }
    *iteration = (int)n;

    return true;
}

/**
 * Check the requested version against the actual version.
 * @param[in] min_version	Desired minimum version
 */
void
check_min_version(const char *min_version)
{
    int min_major, min_minor, min_iteration;

    /* Parse our version. */
    if (!parse_version(build_rpq_version, &our_major, &our_minor,
		&our_iteration)) {
	fprintf(stderr, "Internal error: Can't parse version: %s\n",
		build_rpq_version);
	exit(1);
    }

    if (min_version == NULL) {
	return;
    }

    /* Parse the desired version. */
    if (!parse_version(min_version, &min_major, &min_minor, &min_iteration)) {
	fprintf(stderr, "Invalid %s: %s\n", ResMinVersion, min_version);
	exit(1);
    }

    /* Compare. */
    if (our_major < min_major ||
	    (our_major == min_major && our_minor < min_minor) ||
	    (our_major == min_major && our_minor == min_minor && our_iteration < min_iteration)) {
	fprintf(stderr, "Version %s < requested %s, aborting\n",
		build_rpq_version, min_version);
	exit(1);
    }
}
