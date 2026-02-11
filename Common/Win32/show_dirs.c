/*
 * Copyright (c) 2026 Paul Mattes.
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
 *	show_dirs.c
 *		Windows directory display.
 */

#include "globals.h"

#include "names.h"
#include "query.h"
#include "txa.h"
#include "varbuf.h"
#include "windirs.h"

#include "show_dirs.h"


/* Decode the flags returned by get_dirs(). */
static const char *
decode_gdflags(unsigned flags)
{
    static struct {
	unsigned flag;
	const char *name;
    } flagname[] = {
	{ GD_CATF, "catf" },
	{ GD_INSTALLED, "installed" },
	{ 0, NULL }
    };
    int i;
    varbuf_t r;
    const char *space = "";

    if (flags == 0) {
	return "";
    }
    vb_init(&r);
    for (i = 0; flagname[i].name != NULL; i++) {
	if (flags & flagname[i].flag) {
	    vb_appendf(&r, "%s%s", space, flagname[i].name);
	    space = " ";
	    flags &= ~flagname[i].flag;
	}
    }
    if (flags != 0) {
	vb_appendf(&r, "%s0x%x", space, flags);
    }
    return txdFree(vb_consume(&r));
}

/* Show/Query for Windows directories. */
static const char *
dirs_dump(void)
{
    char *instdir;
    char *desktop;
    char *appdata;
    char *common_desktop;
    char *common_appdata;
    char *documents;
    char *common_documents;
    char *docs3270;
    char *common_docs3270;
    unsigned flags;
#   define OR_NONE(s)	(((s) != NULL)? (s): "(none)")

    if (!get_dirs(app, &instdir, &desktop, &appdata, &common_desktop, &common_appdata,
		&documents, &common_documents, &docs3270, &common_docs3270, &flags)) {
	return "failed";
    }

    return txAsprintf(
"Install: %s\n\
Desktop: %s\n\
Appdata: %s\n\
Documents: %s\n\
3270 documents: %s\n\
Common desktop: %s\n\
Common appdata: %s\n\
Common documents: %s\n\
Common 3270 documents: %s\n\
Flags: %s",
	OR_NONE(instdir),
	OR_NONE(desktop),
	OR_NONE(appdata),
	OR_NONE(documents),
	OR_NONE(docs3270),
	OR_NONE(common_desktop),
	OR_NONE(common_appdata),
	OR_NONE(common_documents),
	OR_NONE(common_docs3270),
	decode_gdflags(flags));
}

/* Module registration. */
void
show_dirs_register(void)
{
    static query_t queries[] = {
	{ KwDirs, dirs_dump, NULL, true, true },
    };

    /* Register our queries. */
    register_queries(queries, array_count(queries));
}
