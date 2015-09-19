/*
 * Copyright (c) 1993-2015 Paul Mattes.
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
 *      xaa.c
 *              The Execute an Action menu item.
 */

#include "globals.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>

#include "macros.h"
#include "popups.h"
#include "xaa.h"
#include "xpopups.h"

/* Macros */

/* Globals */

/* Statics */
static Widget execute_action_shell = NULL;

/* Callback for "OK" button on execute action popup */
static void
execute_action_callback(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *text;

    text = XawDialogGetValueString((Widget)client_data);
    XtPopdown(execute_action_shell);
    if (!text) {
	return;
    }
    push_macro(text, false);
}

void
execute_action_option(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (execute_action_shell == NULL) {
	execute_action_shell = create_form_popup("ExecuteAction",
		execute_action_callback, NULL, FORM_NO_CC);
    }

    popup_popup(execute_action_shell, XtGrabExclusive);
}
