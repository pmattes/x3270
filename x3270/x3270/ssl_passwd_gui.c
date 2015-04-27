/*
 * Copyright (c) 1993-2015 Paul Mattes.
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
 *	ssl_passwd_gui.c
 *		SSL certificate password dialog for x3270.
 */

#include "globals.h"

#if defined(HAVE_LIBSSL) /*[*/

#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>

#include <openssl/ssl.h>

#include "host.h"
#include "objects.h"
#include "popups.h"
#include "ssl_passwd_gui.h"
#include "telnet.h"
#include "telnet_private.h"
#include "xpopups.h"

/* Statics. */
static bool ssl_password_prompted;
static char *ssl_password;
static Widget password_shell = NULL;

/* Callback for "OK" button on the password popup. */
static void
password_callback(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    char *password;

    password = XawDialogGetValueString((Widget)client_data);
    ssl_password = NewString(password);
    XtPopdown(password_shell);

    /* Try init again, with the right password. */
    ssl_base_init(NULL, NULL);

    /*
     * Now try connecting to the command-line hostname, if SSL init
     *  succeeded and there is one.
     * If SSL init failed because of a password problem, the password
     *  dialog will be popped back up.
     */
    if (ssl_ctx != NULL && ssl_cl_hostname) {
	(void) host_connect(ssl_cl_hostname);
	Replace(ssl_cl_hostname, NULL);
    }
}

/* The password dialog was popped down. */
static void
password_popdown(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* If there's no password (they cancelled), don't pop up again. */
    if (ssl_password == NULL) {
	/* Don't pop up again. */
	add_error_popdown_callback(NULL);

	/* Try connecting to the command-line host. */
	if (ssl_cl_hostname != NULL) {
	    (void) host_connect(ssl_cl_hostname);
	    Replace(ssl_cl_hostname, NULL);
	}
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
    Replace(ssl_password, NULL);

    popup_popup(password_shell, XtGrabExclusive);
}

/*
 * Password callback.
 * Returns the length of the password (including 0 for no password), or -1
 * to indicate an error.
 */
int
ssl_passwd_gui_callback(char *buf, int size)
{
    if (ssl_pending != NULL) {
	/* Delay the host connection until the dialog is complete. */
	*ssl_pending = true;

	/* Pop up the dialog. */
	popup_password();
	ssl_password_prompted = true;
	return 0;
    } else if (ssl_password != NULL) {
	/* Dialog is complete. */
	snprintf(buf, size, "%s", ssl_password);
	memset(ssl_password, 0, strlen(ssl_password));
	Replace(ssl_password, NULL);
	return strlen(buf);
    } else {
	return -1;
    }
}

/* Password GUI reset. */
void
ssl_passwd_gui_reset(void)
{
    ssl_password_prompted = false;
}

/*
 * Password GUI retry.
 * Returns true if we should try prompting for the password again.
 */
bool
ssl_passwd_gui_retry(void)
{
    /* Pop up the password dialog again when the error pop-up pops down. */
    if (ssl_password_prompted) {
	add_error_popdown_callback(popup_password);
    }
    return false;
}
#endif /*]*/
