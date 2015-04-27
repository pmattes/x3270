/*
 * Copyright (c) 2000-2010, 2013-2014 Paul Mattes.
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
 *	printer_gui.c
 *		GUI for 3287 printer session support
 */

#include "globals.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>

#include "popups.h"
#include "pr3287_session.h"
#include "printer_gui.h"
#include "xpopups.h"

static Widget lu_shell = NULL;

/* Callback for "OK" button on printer specific-LU popup */
static void
lu_callback(Widget w, XtPointer client_data, XtPointer call_data _is_unused)
{
    char *lu;

    if (w) {
	lu = XawDialogGetValueString((Widget)client_data);
	if (lu == NULL || *lu == '\0') {
	    popup_an_error("Must supply an LU");
	    return;
	} else {
	    XtPopdown(lu_shell);
	}
    } else {
	lu = (char *)client_data;
    }
    pr3287_session_start(lu);
}

/* Pop up the LU dialog box. */
void
printer_lu_dialog(void)
{
    if (lu_shell == NULL) {
	lu_shell = create_form_popup("printerLu", lu_callback, NULL,
		FORM_NO_WHITE);
    }
    popup_popup(lu_shell, XtGrabExclusive);
}
