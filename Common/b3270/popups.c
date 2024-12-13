/*
 * Copyright (c) 2016-2024 Paul Mattes.
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
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	popups.c
 *		A GUI back-end for a 3270 Terminal Emulator
 *		Error and info pop-ups.
 */

#include "globals.h"
#include "resources.h"

#include "appres.h"
#include "b3270proto.h"
#include "host.h"
#include "popups.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "ui_stream.h"
#include "utils.h"

#include "b3270_popups.h"

bool error_popup_visible = false;

typedef struct stored_popup {
    struct stored_popup *next;
    bool is_error;
    pae_t error_type;
    bool retrying;
    char *text;
} stored_popup_t;
static stored_popup_t *sp_first = NULL;
static stored_popup_t *sp_last = NULL;
static bool popups_ready = false;

static const char *error_types[] = {
    PtConnectionError,
    PtError
};

/* Store a pending pop-up. */
static void
popup_store(bool is_error, pae_t type, bool retrying, const char *text)
{
    stored_popup_t *sp =
	(stored_popup_t *)Malloc(sizeof(stored_popup_t) + strlen(text) + 1);
    sp->is_error = is_error;
    sp->error_type = type;
    sp->retrying = retrying;
    sp->text = (char *)(sp + 1);
    strcpy(sp->text, text);

    sp->next = NULL;
    if (sp_last != NULL) {
	sp_last->next = sp;
    } else {
	sp_first = sp;
    }
    sp_last = sp;
}

/* Pop up an error message, given a va_list. */
bool
glue_gui_error(pae_t type, const char *s)
{
    if (!popups_ready) {
	popup_store(true, type, host_retry_mode, s);
    } else {
	ui_leaf(IndPopup,
		AttrType, AT_STRING, error_types[type],
		AttrText, AT_STRING, s,
		AttrRetrying, AT_BOOLEAN, host_retry_mode,
		NULL);
    }
    return true;
}

/* Pop up an info message. */
void
popup_an_info(const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = txVasprintf(fmt, ap);
    va_end(ap);
    if (!popups_ready) {
	popup_store(false, ET_OTHER, false, s);
    } else {
	ui_leaf(IndPopup,
		AttrType, AT_STRING, PtInfo,
		AttrText, AT_STRING, s,
		NULL);
    }
}

/* Output from an action. */
bool
glue_gui_output(const char *s)
{
    ui_leaf(IndPopup,
	    AttrType, AT_STRING, PtResult,
	    AttrText, AT_STRING, s,
	    NULL);
    return true;
}

/* Output from the printer process. */
void
popup_printer_output(bool is_err, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = txVasprintf(fmt, ap);
    va_end(ap);
    ui_leaf(IndPopup,
	    AttrType, AT_STRING, PtPrinter,
	    AttrError, AT_BOOLEAN, is_err,
	    AttrText, AT_STRING, s,
	    NULL);
}

/* Output from a child process. */
void
popup_child_output(bool is_err, abort_callback_t *a _is_unused,
	const char *fmt, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fmt);
    s = txVasprintf(fmt, ap);
    va_end(ap);
    ui_leaf(IndPopup,
	    AttrType, AT_STRING, PtChild,
	    AttrError, AT_BOOLEAN, is_err,
	    AttrText, AT_STRING, s,
	    NULL);
}

void
child_popup_init(void)
{
}

/* Initialization is complete. */
void
popups_dump(void)
{
    stored_popup_t *sp;

    while ((sp = sp_first) != NULL) {
	ui_leaf(IndPopup,
		AttrType, AT_STRING,
		    sp->is_error? error_types[sp->error_type]: PtInfo,
		AttrText, AT_STRING, sp->text,
		sp->is_error? AttrRetrying: NULL, AT_BOOLEAN, sp->retrying,
		NULL);
	sp_first = sp->next;
	Free(sp);
    }
    sp_last = NULL;

    popups_ready = true;
}
