/*
 * Copyright (c) 2015-2019 Paul Mattes.
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

#include "b3270proto.h"
#include "ctlr.h"
#include "kybd.h"
#include "lazya.h"
#include "screen.h"
#include "status.h"
#include "ui_stream.h"
#include "utils.h"

#define FLASH_MSEC	1000

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

static bool scrolled = false;
static int scroll_n = -1;
static char *saved_lock;
static bool flashing = false;
static ioid_t flashing_id = NULL_IOID;

bool
screen_suspend(void)
{
    return false;
}

void
status_compose(bool on, ucs4_t ucs4, enum keytype keytype)
{
    static bool is_on = false;

    if (on == is_on) {
	return;
    }
    is_on = on;

    ui_vleaf(IndOia,
	    AttrField, OiaCompose,
	    AttrValue, ValTrueFalse(on),
	    AttrChar, on? lazyaf("U+%04%x", ucs4): NULL,
	    AttrType, on? ((keytype == KT_STD)? "std": "ge"): NULL,
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

    ui_vleaf(IndOia,
	    AttrField, OiaNotUndera,
	    AttrValue, ValFalse,
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

    ui_vleaf(IndOia,
	    AttrField, OiaInsert,
	    AttrValue, ValTrueFalse(on),
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

    ui_vleaf(IndOia,
	    AttrField, OiaLu,
	    AttrValue, s,
	    NULL);
}

/* Display or buffer a new lock state. */
static void
status_lock(char *msg)
{
    Replace(saved_lock, msg);
    if (!scrolled && !flashing) {
	ui_vleaf(IndOia,
		AttrField, OiaLock,
		AttrValue, saved_lock,
		NULL);
    }
}

void
status_minus(void)
{
    if (oia_kybdlock == K_MINUS) {
	return;
    }
    oia_kybdlock = K_MINUS;
    status_lock(NewString(OiaLockMinus));
}

void
status_oerr(int error_type)
{
    static const char *oerr_names[] = {
	OiaOerrProtected,
	OiaOerrNumeric,
	OiaOerrOverflow,
	OiaOerrDbcs
    };
    char *name;

    oia_kybdlock = K_OERR;

    if (error_type >= 1 && error_type <= 4) {
	name = xs_buffer(OiaLockOerr " %s", oerr_names[error_type - 1]);
    } else {
	name = xs_buffer(OiaLockOerr " %d", error_type);
    }
    status_lock(name);
}

void
status_reset(void)
{
    if (!CONNECTED) {
	if (oia_kybdlock == K_NOT_CONNECTED) {
	    return;
	}
	oia_kybdlock = K_NOT_CONNECTED;
	status_lock(NewString(OiaLockNotConnected));
    } else if (kybdlock & KL_ENTER_INHIBIT) {
	if (oia_kybdlock == K_INHIBIT) {
	    return;
	}
	oia_kybdlock = K_INHIBIT;
	status_lock(NewString(OiaLockInhibit));
    } else if (kybdlock & KL_DEFERRED_UNLOCK) {
	if (oia_kybdlock == K_DEFERRED) {
	    return;
	}
	oia_kybdlock = K_DEFERRED;
	status_lock(NewString(OiaLockDeferred));
    } else {
	if (oia_kybdlock == K_NONE) {
	    return;
	}
	oia_kybdlock = K_NONE;
	status_lock(NULL);
    }
}

void
status_reverse_mode(bool on)
{
    static bool is_on = false;

    if (on == is_on) {
	return;
    }
    is_on = on;

    ui_vleaf(IndOia,
	    AttrField, OiaReverseInput,
	    AttrValue, ValTrueFalse(on),
	    NULL);
}

void
status_screentrace(int n)
{
    ui_vleaf(IndOia,
	    AttrField, OiaScreentrace,
	    AttrValue, (n >= 0)? lazyaf("%d", n): NULL,
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

    ui_vleaf(IndOia,
	    AttrField, OiaScript,
	    AttrValue, ValTrueFalse(on),
	    NULL);
}

void
status_scrolled(int n)
{
    if (n != 0) {
	if (scrolled && scroll_n == n) {
	    return;
	}
	scrolled = true;
	scroll_n = n;
	if (!flashing) {
	    ui_vleaf(IndOia,
		    AttrField, OiaLock,
		    AttrValue, lazyaf(OiaLockScrolled " %d", n),
		    NULL);
	}
    } else {
	if (!scrolled) {
	    return;
	}
	scrolled = false;
	scroll_n = -1;
	if (!flashing) {
	    ui_vleaf(IndOia,
		    AttrField, OiaLock,
		    AttrValue, saved_lock,
		    NULL);
	}
    }
}

/* A keyboard disable flash is done. */
static void
flash_done(ioid_t id _is_unused)
{
    flashing = false;
    flashing_id = NULL_IOID;
    if (scrolled) {
	int n = scroll_n;

	/* Restore the scroll message. */
	scroll_n = -1;
	status_scrolled(n);
    } else {
	/* Restore the lock message. */
	ui_vleaf(IndOia,
		AttrField, OiaLock,
		AttrValue, saved_lock,
		NULL);
    }
}

void
status_keyboard_disable_flash(void)
{
    if (!flashing) {
	ui_vleaf(IndOia,
		AttrField, OiaLock,
		AttrValue, OiaLockDisabled,
		NULL);
    }
    flashing = true;
    if (flashing_id != NULL_IOID) {
	RemoveTimeOut(flashing_id);
    }
    flashing_id = AddTimeOut(FLASH_MSEC, flash_done);
}

void
status_syswait(void)
{
    if (oia_kybdlock == K_SYSWAIT) {
	return;
    }
    oia_kybdlock = K_SYSWAIT;
    status_lock(NewString(OiaLockSyswait));
}

static bool is_timed = false;

void
status_timing(struct timeval *t0, struct timeval *t1)
{
    unsigned long cs;

    is_timed = true;
    cs = (t1->tv_sec - t0->tv_sec) * 10 +
	 (t1->tv_usec - t0->tv_usec + 50000) / 100000;
    ui_vleaf(IndOia,
	    AttrField, OiaTiming,
	    AttrValue, lazyaf("%lu.%lu", cs / 10, cs % 10),
	    NULL);
}

void
status_untiming(void)
{
    if (!is_timed) {
	return;
    }
    is_timed = false;

    ui_vleaf(IndOia,
	    AttrField, OiaTiming,
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

    ui_vleaf(IndOia,
	    AttrField, OiaNotUndera,
	    AttrValue, ValTrue,
	    NULL);

    status_lock(NewString(OiaLockTwait));
}

void
status_typeahead(bool on _is_unused)
{
    static bool is_on = false;

    if (on == is_on) {
	return;
    }
    is_on = on;

    ui_vleaf(IndOia,
	    AttrField, OiaTypeahead,
	    AttrValue, ValTrueFalse(on),
	    NULL);
}
