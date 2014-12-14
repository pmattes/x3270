/*
 * Copyright (c) 1994-2014, Paul Mattes.
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
 *	print.c
 *		Screen printing functions.
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"
#include "tablesc.h"

#include <errno.h>

#if defined(X3270_DISPLAY) /*[*/
# include <X11/StringDefs.h>
# include <X11/Xaw/Dialog.h>
#endif /*]*/

#include "objects.h"
#include "resources.h"

#include "actionsc.h"
#include "charsetc.h"
#include "fprint_screenc.h"
#include "popupsc.h"
#include "printc.h"
#if defined(X3270_DISPLAY) /*[*/
# include "stmenuc.h"
#endif /*]*/
#include "trace_dsc.h"
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"

#if defined(_WIN32) /*[*/
# include <fcntl.h>
# include <sys/stat.h>
# include "w3miscc.h"
# include "winprintc.h"
#  if defined(WC3270) /*[*/
#   include <screenc.h>
#  endif /*]*/
#endif /*]*/

/* Typedefs */

/* Globals */

/* Statics */
#if defined(X3270_DISPLAY) /*[*/
static Widget print_window_shell = (Widget)NULL;
char *print_window_command = CN;
#endif /*]*/

/* Print Text popup */

#if !defined(_WIN32) /*[*/
/* Termination code for print text process. */
static void
print_text_done(FILE *f)
{
	int status;

	status = pclose(f);
	if (status) {
		popup_an_error("Print program exited with status %d.",
		    (status & 0xff00) > 8);
	} else {
# if defined(X3270_INTERACTIVE) /*[*/
		if (appres.do_confirms)
			popup_an_info("Screen image printed.");
# endif /*]*/
	}

}
#endif /*]*/

/**
 * Default caption.
 *
 * @return caption text
 */
char *
default_caption(void)
{
    static char *r = NULL;

#if !defined(_WIN32) /*[*/
    /* Unix version: username@host %T% */
    char hostname[132];
    char *user;

    gethostname(hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = '\0';
    user = getenv("USER");
    if (user == NULL) {
	user = "(unknown)";
    }

    if (r != NULL) {
	Free(r);
    }
    r = xs_buffer("%s @ %s %%T%%", user, hostname);

#else /*][*/

    char *username;
    char *computername;
    char *userdomain;

    username = getenv("USERNAME");
    if (username == NULL) {
	username = "(unknown)";
    }
    computername = getenv("COMPUTERNAME");
    if (computername == NULL) {
	computername = "(unknown)";
    }
    userdomain = getenv("USERDOMAIN");
    if (userdomain == NULL) {
	userdomain = "(unknown)";
    }

    if (r != NULL) {
	Free(r);
    }
    if (strcasecmp(userdomain, computername)) {
	r = xs_buffer("%s\\%s @ %s %%T%%", userdomain, username, computername);
    } else {
	r = xs_buffer("%s @ %s %%T%%", username, computername);
    }
#endif

    return r;
}

/* Print or save the contents of the screen as text. */
void
PrintText_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	Cardinal i;
	char *name = CN;
#if defined(X3270_DISPLAY) /*[*/
	Boolean secure = appres.secure;
#endif /*]*/
	ptype_t ptype = P_TEXT;
	Boolean use_file = False;
	Boolean use_string = False;
	char *temp_name = NULL;
	unsigned opts = FPS_EVEN_IF_EMPTY;
	char *caption = NULL;

	action_debug(PrintText_action, event, params, num_params);

	/*
	 * Pick off optional arguments:
	 *  file     directs the output to a file instead of a command;
	 *  	      must be the last keyword
	 *  html     generates HTML output instead of ASCII text (and implies
	 *            'file')
	 *  rtf      generates RTF output instead of ASCII text (and implies
	 *            'file')
	 *  gdi      prints to a GDI printer (wc3270 only)
	 *  wordpad  prints via WordPad (wc3270 only)
	 *  modi     print modified fields in italics
	 *  caption "text"
	 *           Adds caption text above the screen
	 *           %T% is replaced by a timestamp
	 *  secure   disables the pop-up dialog, if this action is invoked from
	 *            a keymap
	 *  command  directs the output to a command (this is the default, but
	 *            allows the command to be one of the other keywords);
	 *  	      must be the last keyword
	 *  string   returns the data as a string, allowed only from scripts
	 */
	for (i = 0; i < *num_params; i++) {
		if (!strcasecmp(params[i], "file")) {
			use_file = True;
			i++;
			break;
		} else if (!strcasecmp(params[i], "html")) {
			ptype = P_HTML;
			use_file = True;
		} else if (!strcasecmp(params[i], "rtf")) {
			ptype = P_RTF;
			use_file = True;
		}
#if defined(WC3270) /*[*/
		else if (!strcasecmp(params[i], "gdi")) {
			ptype = P_GDI;
		} else if (!strcasecmp(params[i], "wordpad")) {
			ptype = P_RTF;
		}
#endif /*]*/
		else if (!strcasecmp(params[i], "secure")) {
#if defined(X3270_DISPLAY) /*[*/
			secure = True;
#endif /*]*/
		} else if (!strcasecmp(params[i], "command")) {
			if ((ptype != P_TEXT) || use_file) {
				popup_an_error("%s: contradictory options",
				    action_name(PrintText_action));
				return;
			}
			i++;
			break;
		} else if (!strcasecmp(params[i], "string")) {
			if (ia_cause != IA_SCRIPT) {
				popup_an_error("%s(string) can only be used "
						"from a script",
				    action_name(PrintText_action));
				return;
			}
			use_string = True;
			use_file = True;
		} else if (!strcasecmp(params[i], "modi")) {
		    	opts |= FPS_MODIFIED_ITALIC;
		} else if (!strcasecmp(params[i], "caption")) {
		    	if (i == *num_params - 1) {
			    	popup_an_error("%s: mising caption parameter",
					action_name(PrintText_action));
				return;
			}
			caption = params[++i];
		} else {
			break;
		}
	}
	switch (*num_params - i) {
	case 0:
		/* Use the default. */
		if (!use_file) {
#if !defined(_WIN32) /*[*/
			name = get_resource(ResPrintTextCommand);
#else /*][*/
			name = get_resource(ResPrinterName); /* XXX */
#endif /*]*/
		}
		break;
	case 1:
		if (use_string) {
			popup_an_error("%s: extra arguments or invalid option(s)",
			    action_name(PrintText_action));
			return;
		}
		name = params[i];
		break;
	default:
		popup_an_error("%s: extra arguments or invalid option(s)",
		    action_name(PrintText_action));
		return;
	}

#if defined(_WIN32) /*[*/
	/* On Windows, use GDI as the default. */
	if (!use_string && !use_file && ptype == P_TEXT) {
		ptype = P_GDI;
	}
#endif /*]*/

	if (name != CN && name[0] == '@') {
		/*
		 * Starting the PrintTextCommand resource value with '@'
		 * suppresses the pop-up dialog, as does setting the 'secure'
		 * resource.
		 */
#if defined(X3270_DISPLAY) /*[*/
		secure = True;
#endif /*]*/
		name++;
	}
	if (!use_file && (name == CN || !*name)) {
#if !defined(_WIN32) /*[*/
		name = "lpr";
#else /*][*/
		name = CN;
#endif /*]*/
	}

#if defined(X3270_DISPLAY) /*[*/
	if (secure ||
		ia_cause == IA_COMMAND ||
		ia_cause == IA_MACRO ||
		ia_cause == IA_SCRIPT)
#endif /*]*/
	{
		FILE *f;
		int fd = -1;

		/* Invoked from somewhere other than a keymap. */
		if (use_file) {
			if (use_string) {
#if defined(_WIN32) /*[*/
				fd = win_mkstemp(&temp_name, ptype);
#else /*][*/
				temp_name = NewString("/tmp/x3hXXXXXX");
				fd = mkstemp(temp_name);
#endif /*]*/
				if (fd < 0) {
					popup_an_errno(errno, "mkstemp");
					return;
				}
				f = fdopen(fd, "w+");
			} else {
				if (name == CN || !*name) {
					popup_an_error("%s: missing filename",
						action_name(PrintText_action));
					return;
				}
				f = fopen(name, "a");
			}
		} else {
#if !defined(_WIN32) /*[*/
			f = popen(name, "w");
#else /*][*/
			fd = win_mkstemp(&temp_name, ptype);
			if (fd < 0) {
				popup_an_errno(errno, "mkstemp");
				return;
			}
			if (ptype == P_GDI) {
				f = fdopen(fd, "wb+");
			} else {
				f = fdopen(fd, "w+");
			}
#endif /*]*/
		}
		if (f == NULL) {
			popup_an_errno(errno, "%s: %s",
					action_name(PrintText_action),
					name);
			if (fd >= 0) {
				(void) close(fd);
			}
			if (temp_name) {
				unlink(temp_name);
				Free(temp_name);
			}
			return;
		}
		if (caption == NULL) {
		    caption = default_caption();
		}
		switch (fprint_screen(f, ptype, opts, caption, name)) {
		case FPS_STATUS_SUCCESS:
		case FPS_STATUS_SUCCESS_WRITTEN:
			break;
		case FPS_STATUS_ERROR:
			popup_an_error("Screen print failed.");
			/* fall through */
		case FPS_STATUS_CANCEL:
			fclose(f);
			if (temp_name) {
				unlink(temp_name);
				Free(temp_name);
			}
			return;
		}
		if (use_string) {
			char buf[8192];

			rewind(f);
			while (fgets(buf, sizeof(buf), f) != NULL)
				action_output("%s", buf);
		}
		if (use_file) {
			fclose(f);
		} else {
#if !defined(_WIN32) /*[*/
			print_text_done(f);
#else /*][*/
			fclose(f);
			if (ptype == P_RTF) {
# if defined(S3270) /*[*/
				/*
				 * Run WordPad to print the file, synchronusly.
				 */
				start_wordpad_sync("PrintText", temp_name,
					name);
# else /*][*/
				/*
				 * Run WordPad to print the file,
				 * asynchronusly.
				 */
				start_wordpad_async("PrintText", temp_name,
					name);
# endif /*]*/
			} else if (ptype == P_GDI) {
				/* All done with the temp file. */
				unlink(temp_name);
			}
# if !defined(S3270) /*[*/
			if (appres.do_confirms)
				popup_an_info("Screen image printing.\n");
# endif /*]*/
#endif /*]*/
		}
#if !defined(WC3270) /*[*/
		if (temp_name) {
		    	unlink(temp_name);
			Free(temp_name);
		}
#endif /*]*/
		return;
	}

#if defined(X3270_DISPLAY) /*[*/
	/* Invoked from a keymap -- pop up the confirmation dialog. */
	if (use_file) {
		stmenu_popup(STMP_TEXT);
	} else {
		stmenu_popup(STMP_PRINTER);
	}
#endif /*]*/
}


#if defined(X3270_DISPLAY) /*[*/
/* Print Window popup */

/*
 * Printing the window bitmap is a rather convoluted process:
 *    The PrintWindow action calls PrintWindow_action(), or a menu option calls
 *	print_window_option().
 *    print_window_option() pops up the dialog.
 *    The OK button on the dialog triggers print_window_callback.
 *    print_window_callback pops down the dialog, then schedules a timeout
 *     1 second away.
 *    When the timeout expires, it triggers snap_it(), which finally calls
 *     xwd.
 * The timeout indirection is necessary because xwd prints the actual contents
 * of the window, including any pop-up dialog in front of it.  We pop down the
 * dialog, but then it is up to the server and Xt to send us the appropriate
 * expose events to repaint our window.  Hopefully, one second is enough to do
 * that.
 */

/* Termination procedure for window print. */
static void
print_window_done(int status)
{
	if (status)
		popup_an_error("Print program exited with status %d.",
		    (status & 0xff00) >> 8);
	else if (appres.do_confirms)
		popup_an_info("Bitmap printed.");
}

/* Timeout callback for window print. */
static void
snap_it(XtPointer closure _is_unused, XtIntervalId *id _is_unused)
{
	if (!print_window_command)
		return;
	XSync(display, 0);
	print_window_done(system(print_window_command));
}

/* Callback for "OK" button on print window popup. */
static void
print_window_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	print_window_command = XawDialogGetValueString((Widget)client_data);
	XtPopdown(print_window_shell);
	if (print_window_command)
		(void) XtAppAddTimeOut(appcontext, 1000, snap_it, 0);
}

/* Print the contents of the screen as a bitmap. */
void
PrintWindow_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	char *filter = get_resource(ResPrintWindowCommand);
	char *fb = XtMalloc(strlen(filter) + 16);
	char *xfb = fb;
	Boolean secure = appres.secure;

	action_debug(PrintWindow_action, event, params, num_params);
	if (*num_params > 0)
		filter = params[0];
	if (*num_params > 1)
		popup_an_error("%s: extra arguments ignored",
		    action_name(PrintWindow_action));
	if (filter == CN) {
		popup_an_error("%s: no %s defined",
		    action_name(PrintWindow_action), ResPrintWindowCommand);
		return;
	}
	(void) sprintf(fb, filter, XtWindow(toplevel));
	if (fb[0] == '@') {
		secure = True;
		xfb = fb + 1;
	}
	if (secure) {
		print_window_done(system(xfb));
		Free(fb);
		return;
	}
	if (print_window_shell == NULL)
		print_window_shell = create_form_popup("printWindow",
		    print_window_callback, (XtCallbackProc)NULL, FORM_AS_IS);
	XtVaSetValues(XtNameToWidget(print_window_shell, ObjDialog),
	    XtNvalue, fb,
	    NULL);
	popup_popup(print_window_shell, XtGrabExclusive);
}

/* Callback for menu Print Window option. */
void
print_window_option(Widget w, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	Cardinal zero = 0;

	PrintWindow_action(w, (XEvent *)NULL, (String *)NULL, &zero);
}
#endif /*]*/
