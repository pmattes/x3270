/*
 * Copyright (c) 1993-2015, 2019-2020 Paul Mattes.
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
 *	trace_gui.c
 *		GUI for 3270 data stream tracing.
 *
 */

#include "globals.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Label.h>

#include "appres.h"
#include "resources.h"

#include "objects.h"
#include "popups.h"
#include "toggles.h"
#include "trace.h"
#include "trace_gui.h"
#include "xmenubar.h"
#include "xpopups.h"

/* Statics */
static Widget trace_shell = NULL;

/* Pop up an info about a bogus trace file maximum size. */
void
trace_gui_bad_size(const char *default_value)
{
    popup_an_info("Invalid %s '%s', assuming %s",
	    ResTraceFileSize, appres.trace_file_size,
	    default_value);
}

/* Callback for "Trace" button on trace popup. */
static void
tracefile_callback(Widget w, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *tfn = NULL;

    if (w) {
	tfn = XawDialogGetValueString((Widget)client_data);
    } else {
	tfn = (char *)client_data;
    }
    tracefile_ok(tfn);
    if (w) {
	XtPopdown(trace_shell);
    }
}

/*
 * Tracing has been started. Pop up the dialog, if appropriate.
 * Returns true for dialog up, false to go ahead and start tracing.
 */
bool
trace_gui_on(int reason, enum toggle_type tt, const char *tracefile)
{
    if (tt != TT_XMENU || tt == TT_ACTION) {
	/* Start tracing now. */
	return false;
    }

    if (trace_shell == NULL) {
	trace_shell = create_form_popup("trace",
		tracefile_callback,
		NULL,
		FORM_NO_WHITE);
	XtVaSetValues(XtNameToWidget(trace_shell, ObjDialog),
		XtNvalue, tracefile,
		NULL);
    }

    popup_popup(trace_shell, XtGrabExclusive);

    /* Pop-up is up and will start tracing when it completes. */
    return true;
}

/* Change the menu option for tracing when the toggle is changed. */
void
trace_gui_toggle(void)
{
    if (toggle_widget[SCREEN_TRACE].w[0] != NULL) {
	XtVaSetValues(toggle_widget[SCREEN_TRACE].w[0],
		XtNleftBitmap, toggled(SCREEN_TRACE)? dot: None,
		NULL);
    }
}
