/*
 * Copyright (c) 1994-2024 Paul Mattes.
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
 *	winprint.c
 *		Windows screen printing functions.
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"

#include <errno.h>

#include "popups.h"
#include "resources.h"
#include "print_screen.h"
#include "trace.h"
#include "utils.h"

#include "winprint.h"

#include <fcntl.h>
#include <sys/stat.h>
#include "w3misc.h"

/* Typedefs */

/* Globals */

/* Statics */

/*
 * A Windows version of something like mkstemp().  Creates a temporary
 * file in $TEMP, returning its path and an open file descriptor.
 */
int
win_mkstemp(char **path, ptype_t ptype)
{
    char *s;
    int fd;
    unsigned gen = 0;
    char *suffix;
    int xflags = O_TEXT;

    switch (ptype) {
    case P_GDI:
	suffix = "gdi";
	xflags = O_BINARY;
	break;
    case P_RTF:
	suffix = "rtf";
	break;
    default:
	suffix = "txt";
	break;
    }

    while (gen < 1000) {
	s = getenv("TEMP");
	if (s == NULL) {
	    s = getenv("TMP");
	}
	if (s == NULL) {
	    s = "C:";
	}
	if (gen) {
	    *path = Asprintf("%s\\x3h-%u-%u.%s", s, getpid(), gen, suffix);
	} else {
	    *path = Asprintf("%s\\x3h-%u.%s", s, getpid(), suffix);
	}
	fd = open(*path, O_CREAT | O_EXCL | O_RDWR | xflags,
		S_IREAD | S_IWRITE);
	if (fd >= 0) {
	    break;
	}

	/* Try again. */
	Free(*path);
	*path = NULL;
	if (errno != EEXIST)
	    break;
	gen++;
    }
    return fd;
}
