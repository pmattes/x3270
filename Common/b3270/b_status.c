/*
 * Copyright (c) 2015-2016 Paul Mattes.
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
 *	b_status.c
 *		b3270 status line.
 */

#include "globals.h"

#include "ctlr.h"
#include "kybd.h"
#include "lazya.h"
#include "screen.h"
#include "status.h"
#include "ui_stream.h"

typedef enum {
    K_NONE,
    K_MINUS,
    K_OERR,
    K_SYSWAIT,
    K_NOT_CONNECTED,
    K_INHIBIT,
    K_DEFERRED,
    K_TWAIT
} oia_kybdlock_t;
oia_kybdlock_t oia_kybdlock = K_NONE;

bool
screen_suspend(void)
{
    return false;
}

void
status_compose(bool on, unsigned char c, enum keytype keytype)
{
    static bool is_on = false;

    if (on == is_on) {
	return;
    }
    is_on = on;

    ui_vleaf("oia",
	    "field", "compose",
	    "value", on? "true": "false",
	    "char", on? lazyaf("U+%04%x", c): NULL,
	    "type", on? ((keytype == KT_STD)? "std": "ge"): NULL,
	    NULL);
}

static bool oia_undera = true;

void
status_ctlr_done(void)
{
    if (oia_undera) {
	return;
    }
    oia_undera = true;

    ui_vleaf("oia",
	    "field", "not-undera",
	    "value", "false",
	    NULL);
}

void
status_insert_mode(bool on)
{
    static bool is_on = false;

    if (on == is_on) {
	return;
    }
    is_on = on;

    ui_vleaf("oia",
	    "field", "insert",
	    "value", on? "true": "false",
	    NULL);
}

void
status_lu(const char *s)
{
    static char *saved_lu = NULL;

    if (saved_lu == NULL && s == NULL) {
	return;
    }
    if (saved_lu != NULL && s != NULL && !strcmp(saved_lu, s)) {
	return;
    }
    if (s == NULL) {
	Replace(saved_lu, NULL);
    } else {
	Replace(saved_lu, NewString(s));
    }

    ui_vleaf("oia",
	    "field", "lu",
	    "value", s,
	    NULL);
}

void
status_minus(void)
{
    if (oia_kybdlock == K_MINUS) {
	return;
    }
    oia_kybdlock = K_MINUS;

    ui_vleaf("oia",
	    "field", "lock",
	    "value", "minus",
	    NULL);
}

void
status_oerr(int error_type)
{
    static const char *oerr_names[] = {
	"protected",
	"numeric",
	"overflow",
	"dbcs"
    };
    char *name;

    oia_kybdlock = K_OERR;

    if (error_type >= 1 && error_type <= 4) {
	name = lazyaf("oerr %s", oerr_names[error_type - 1]);
    } else {
	name = lazyaf("oerr %d", error_type);
    }
    ui_vleaf("oia",
	    "field", "lock",
	    "value", name,
	    NULL);
}

void
status_reset(void)
{
    if (!CONNECTED) {
	if (oia_kybdlock == K_NOT_CONNECTED) {
	    return;
	}
	oia_kybdlock = K_NOT_CONNECTED;
	ui_vleaf("oia",
		"field", "lock",
		"value", "not-connected",
		NULL);
    } else if (kybdlock & KL_ENTER_INHIBIT) {
	if (oia_kybdlock == K_INHIBIT) {
	    return;
	}
	oia_kybdlock = K_INHIBIT;
	ui_vleaf("oia",
		"field", "lock",
		"value", "inhibit",
		NULL);
    } else if (kybdlock & KL_DEFERRED_UNLOCK) {
	if (oia_kybdlock == K_DEFERRED) {
	    return;
	}
	oia_kybdlock = K_DEFERRED;
	ui_vleaf("oia",
		"field", "lock",
		"value", "deferred",
		NULL);
    } else {
	if (oia_kybdlock == K_NONE) {
	    return;
	}
	oia_kybdlock = K_NONE;
	ui_vleaf("oia",
		"field", "lock",
		NULL);
    }
}

void
status_reverse_mode(bool on)
{
    return;
}

void
status_screentrace(int n)
{
    ui_vleaf("oia",
	    "field", "screentrace",
	    "value", (n >= 0)? lazyaf("%d", n): NULL,
	    NULL);
}

void
status_script(bool on)
{
    static bool is_on = false;

    if (on == is_on) {
	return;
    }
    is_on = on;

    ui_vleaf("oia",
	    "field", "script",
	    "value", on? "true": "false",
	    NULL);
}

void
status_scrolled(int n)
{
    ui_vleaf("oia",
	    "field", "lock",
	    "value", lazyaf("scrolled %d", n),
	    NULL);
}

void
status_syswait(void)
{
    if (oia_kybdlock == K_SYSWAIT) {
	return;
    }
    oia_kybdlock = K_SYSWAIT;

    ui_vleaf("oia",
	    "field", "lock",
	    "value", "syswait",
	    NULL);
}

static bool is_timed = false;

void
status_timing(struct timeval *t0 _is_unused, struct timeval *t1 _is_unused)
{
    unsigned long cs;

    is_timed = true;
    cs = (t1->tv_sec - t0->tv_sec) * 10 +
	 (t1->tv_usec - t0->tv_usec + 50000) / 100000;
    ui_vleaf("oia",
	    "field", "timing",
	    "value", lazyaf("%lu.%lu", cs / 10, cs % 10),
	    NULL);
}

void
status_untiming(void)
{
    if (!is_timed) {
	return;
    }
    is_timed = false;

    ui_vleaf("oia",
	    "field", "timing",
	    NULL);
}

void
status_twait(void)
{
    if (oia_kybdlock == K_TWAIT) {
	return;
    }
    oia_kybdlock = K_TWAIT;

    oia_undera = false;

    ui_vleaf("oia",
	    "field", "not-undera",
	    "value", "true",
	    NULL);

    ui_vleaf("oia",
	    "field", "lock",
	    "value", "twait",
	    NULL);
}

void
status_typeahead(bool on _is_unused)
{
    static bool is_on = false;

    if (on == is_on) {
	return;
    }
    is_on = on;

    ui_vleaf("oia",
	    "field", "typeahead",
	    "value", on? "true": "false",
	    NULL);
}
