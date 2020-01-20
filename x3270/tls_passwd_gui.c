/*
 * Copyright (c) 1993-2017, 2020 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC
 *       nor their contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, JEFF SPARKES AND
 * GTRC "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES,
 * DON RUSSELL, JEFF SPARKES OR GTRC BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	tls_passwd_gui.c
 *		TLS certificate password dialog for x3270.
 */

#include "globals.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>

#include "appres.h"
#include "host.h"
#include "objects.h"
#include "popups.h"
#include "sio.h"
#include "telnet.h"
#include "telnet_private.h"
#include "tls_passwd_gui.h"
#include "xglobals.h"
#include "xpopups.h"

/* Statics. */
static char *tls_password;
static Widget password_shell = NULL;

/* Callback for "OK" button on the password popup. */
static void
password_callback(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *password;

    password = XawDialogGetValueString((Widget)client_data);
    tls_password = NewString(password);
    XtPopdown(password_shell);

    net_password_continue(tls_password);
}

/* The password dialog was popped down. */
static void
password_popdown(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* If there's no password (they cancelled), don't pop up again. */
    if (tls_password == NULL) {
	/* We might want to do something more sophisticated here. */
	host_disconnect(true);
    }
}

/* Pop up the password dialog. */
static void
popup_password(void)
{
    if (password_shell == NULL) {
	password_shell = create_form_popup("Password", password_callback, NULL,
		FORM_AS_IS);
	XtAddCallback(password_shell, XtNpopdownCallback, password_popdown,
		NULL);
	}
    XtVaSetValues(XtNameToWidget(password_shell, ObjDialog),
	    XtNvalue, "",
	    NULL);
    Replace(tls_password, NULL);

    popup_popup(password_shell, XtGrabExclusive);
}

/*
 * Password callback.
 */
tls_passwd_ret_t
tls_passwd_gui_callback(char *buf, int size, bool again)
{
    /* Pop up the dialog. */
    popup_password();
    if (again) {
	popup_an_error("Password is incorrect.");
    }
    return SP_PENDING;
}
