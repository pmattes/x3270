/*
 * Copyright (c) 1994-2020 Paul Mattes.
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
 *	print_screen.c
 *		Screen printing functions.
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"

#include <errno.h>
#include <assert.h>

#include "resources.h"

#include "actions.h"
#include "child.h"
#include "codepage.h"
#include "fprint_screen.h"
#include "lazya.h"
#include "popups.h"
#include "print_screen.h"
#include "print_gui.h"
#include "product.h"
#include "task.h"
#include "toggles.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"

#if defined(_WIN32) /*[*/
# include <fcntl.h>
# include <sys/stat.h>
# include "w3misc.h"
# include "winprint.h"
#endif /*]*/

/* Typedefs */

/* Saved context for a suspended PrintText(). */
typedef struct {
    FILE *f;		/* temporary file */
    ptype_t ptype;	/* print type */
    unsigned opts;	/* options */
    const char *caption; /* caption text */
    const char *name;	/* printer name */
    char *temp_name;	/* temporary file name */
} printtext_t;

/* Globals */

/* Statics */

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
    char ComputerName[MAX_COMPUTERNAME_LENGTH + 1];

    username = getenv("USERNAME");
    if (username == NULL) {
	username = "(unknown)";
    }
    computername = getenv("COMPUTERNAME");
    if (computername == NULL) {
	DWORD size = MAX_COMPUTERNAME_LENGTH + 1;

	if (GetComputerName(ComputerName, &size)) {
	    computername = ComputerName;
	} else {
	    computername = "(unknown)";
	}
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

/* Extended-wait continue function for PrintText(). */
static void
printtext_continue(void *context, bool cancel)
{
    printtext_t *pt = (printtext_t *)context;
    fps_status_t status;

    if (cancel) {
	vtrace("PrintText canceled\n");
	fclose(pt->f);
	if (pt->temp_name != NULL) {
	    unlink(pt->temp_name);
	    Free(pt->temp_name);
	}
	Free(pt);
	return;
    }

    status = fprint_screen(pt->f, pt->ptype, pt->opts | FPS_DIALOG_COMPLETE,
	    pt->caption, pt->name, NULL);
    switch (status) {
    case FPS_STATUS_SUCCESS:
    case FPS_STATUS_SUCCESS_WRITTEN:
	vtrace("PrintText: printing succeeded.\n");
	break;
    case FPS_STATUS_ERROR:
	popup_an_error("Screen print failed");
	/* fall through */
    case FPS_STATUS_CANCEL:
	if (status == FPS_STATUS_CANCEL) {
	    vtrace("PrintText: printing canceled.\n");
	}
	break;
    case FPS_STATUS_WAIT:
	/* Can't happen. */
	assert(status != FPS_STATUS_WAIT);
	break;
    }

    fclose(pt->f);
    if (pt->temp_name != NULL) {
	unlink(pt->temp_name);
	Free(pt->temp_name);
    }
    Free(pt);
}

/* Print or save the contents of the screen as text. */
bool
PrintText_action(ia_t ia, unsigned argc, const char **argv)
{
    unsigned i;
    const char *name = NULL;
    bool secure = appres.secure;
    ptype_t ptype = P_NONE;
    bool use_file = false;
    bool use_string = false;
    bool replace = false;
    char *temp_name = NULL;
    unsigned opts = FPS_EVEN_IF_EMPTY;
    const char *caption = NULL;
    FILE *f;
    int fd = -1;
    fps_status_t status;
    printtext_t *pt;

    if (!appres.interactive.print_dialog) {
	opts |= FPS_NO_DIALOG;
    }

    action_debug("PrintText", ia, argc, argv);

    /*
     * Pick off optional arguments:
     *  file     directs the output to a file instead of a command;
     *  	      must be the last keyword
     *  html     generates HTML output instead of ASCII text (and implies
     *            'file')
     *  rtf      generates RTF output instead of ASCII text (and implies
     *            'file')
     *  gdi      prints to a GDI printer (Windows only)
     *  nodialog skip print dialog (Windows only)
     *            this is the default for ws3270
     *  dialog	 use print dialog (Windows only)
     *            this is the default for wc3270
     *  replace  replace the file
     *  append   append to the file, if it exists (default)
     *  modi     print modified fields in italics
     *  caption "text"
     *           Adds caption text above the screen
     *           %T% is replaced by a timestamp
     *  secure   disables the pop-up dialog, if this action is invoked from
     *            a keymap (x3270 only)
     *  command  directs the output to a command (this is the default, but
     *            allows the command to be one of the other keywords);
     *  	      must be the last keyword
     *  string   returns the data as a string, allowed only from scripts
     */
    for (i = 0; i < argc; i++) {
	if (!strcasecmp(argv[i], "file")) {
	    use_file = true;
	    i++;
	    break;
	} else if (!strcasecmp(argv[i], "html")) {
	    ptype = P_HTML;
	    use_file = true;
	} else if (!strcasecmp(argv[i], "rtf")) {
	    ptype = P_RTF;
	    use_file = true;
	} else if (!strcasecmp(argv[i], "replace")) {
	    replace = true;
	} else if (!strcasecmp(argv[i], "append")) {
	    replace = false;
	}
#if defined(_WIN32) /*[*/
	else if (!strcasecmp(argv[i], "gdi")) {
	    ptype = P_GDI;
	} else if (!strcasecmp(argv[i], "nodialog")) {
	    opts |= FPS_NO_DIALOG;
	} else if (!strcasecmp(argv[i], "dialog")) {
	    opts &= ~FPS_NO_DIALOG;
	}
#endif /*]*/
	else if (!strcasecmp(argv[i], "secure")) {
	    secure = true;
	} else if (!strcasecmp(argv[i], "command")) {
	    if ((ptype != P_NONE) || use_file) {
		popup_an_error("PrintText: contradictory options");
		return false;
	    }
	    ptype = P_TEXT;
	    i++;
	    break;
	} else if (!strcasecmp(argv[i], "string")) {
	    use_string = true;
	    use_file = true;
	} else if (!strcasecmp(argv[i], "modi")) {
	    opts |= FPS_MODIFIED_ITALIC;
	} else if (!strcasecmp(argv[i], "caption")) {
	    if (i == argc - 1) {
		popup_an_error("PrintText: mising caption parameter");
		return false;
	    }
	    caption = argv[++i];
	} else {
	    break;
	}
    }

    switch (argc - i) {
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
	    popup_an_error("PrintText: extra arguments or invalid option(s)");
	    return false;
	}
	name = argv[i];
	break;
    default:
	popup_an_error("PrintText: extra arguments or invalid option(s)");
	return false;
    }

    if (!use_string && !use_file) {
	if (ptype != P_NONE) {
	    popup_an_error("PrintText: cannot specify printer and type");
	    return false;
	}
#if !defined(_WIN32) /*[*/
	ptype = P_TEXT;
#else /*]*/
	ptype = P_GDI;
#endif /*]*/
    }

    if (ptype == P_NONE && use_file && name != NULL) {
	size_t sl = strlen(name);

	if ((sl > 5 && !strcasecmp(name + sl - 5, ".html")) ||
	    (sl > 4 && !strcasecmp(name + sl - 4, ".htm"))) {
	    ptype = P_HTML;
	} else if (sl > 4 && !strcasecmp(name + sl - 4, ".rtf")) {
	    ptype = P_RTF;
	}
    }

    if (ptype == P_NONE) {
	ptype = P_TEXT;
    }

    if (name != NULL && name[0] == '@') {
	/*
	 * Starting the PrintTextCommand resource value with '@'
	 * suppresses the pop-up dialog, as does setting the 'secure'
	 * resource.
	 */
	secure = true;
	name++;
    }
    if (!use_file && (name == NULL || !*name)) {
#if !defined(_WIN32) /*[*/
	name = "lpr";
#else /*][*/
	name = NULL;
#endif /*]*/
    }

    /* See if the GUI wants to handle it. */
    if (!secure && print_text_gui(use_file)) {
	return true;
    }

    /* Do the real work. */
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
		return false;
	    }
	    f = fdopen(fd, "w+");
	    vtrace("PrintText: using '%s'\n", temp_name);
	} else {
	    if (name == NULL || !*name) {
		popup_an_error("PrintText: missing filename");
		return false;
	    }
	    f = fopen(name, replace? "w": "a");
	}
    } else {
#if !defined(_WIN32) /*[*/
	const char *expanded_name;
	char *pct_e;

	if ((pct_e = strstr(name, "%E%")) != NULL) {
	    expanded_name = lazyaf("%.*s%s%s",
		    (int)(pct_e - name), name,
		    programname,
		    pct_e + 3);
	} else {
	    expanded_name = name;
	}
	f = printer_open(expanded_name, NULL);
#else /*][*/
	fd = win_mkstemp(&temp_name, ptype);
	if (fd < 0) {
	    popup_an_errno(errno, "mkstemp");
	    return false;
	}
	if (ptype == P_GDI) {
	    f = fdopen(fd, "wb+");
	} else {
	    f = fdopen(fd, "w+");
	}
	vtrace("PrintText: using '%s'\n", temp_name);
#endif /*]*/
    }
    if (f == NULL) {
	popup_an_errno(errno, "PrintText: %s", name);
	if (fd >= 0) {
	    close(fd);
	}
	if (temp_name) {
	    unlink(temp_name);
	    Free(temp_name);
	}
	return false;
    }

    /* Captions look nice on GDI, so create a default one. */
    if (ptype == P_GDI && caption == NULL) {
	caption = default_caption();
    }

    pt = (printtext_t *)Calloc(1, sizeof(printtext_t));
    status = fprint_screen(f, ptype, opts, caption, name, pt);
    switch (status) {
    case FPS_STATUS_SUCCESS:
    case FPS_STATUS_SUCCESS_WRITTEN:
	vtrace("PrintText: printing succeeded.\n");
	Free(pt);
	break;
    case FPS_STATUS_ERROR:
	popup_an_error("Screen print failed.");
	/* fall through */
    case FPS_STATUS_CANCEL:
	if (status == FPS_STATUS_CANCEL) {
	    vtrace("PrintText: printing canceled.\n");
	}
	Free(pt);
	fclose(f);
	if (temp_name) {
	    unlink(temp_name);
	    Free(temp_name);
	}
	return false;
    case FPS_STATUS_WAIT:
	/* Waiting for asynchronous activity. */
	assert(ptype == P_GDI);
	pt->f = f;
	pt->ptype = ptype;
	pt->opts = opts;
	pt->caption = caption;
	pt->name = name;
	pt->temp_name = temp_name;
	task_xwait(pt, printtext_continue, "printing");
	return true;
    }

    if (use_string) {
	char buf[8192];

	/* Print to string. */
	fflush(f);
	rewind(f);
	while (fgets(buf, sizeof(buf), f) != NULL) {
	    action_output("%s", buf);
	}
	fclose(f);
	unlink(temp_name);
	Free(temp_name);
	return true;
    }

    if (use_file) {
	/* Print to specified file. */
	fclose(f);
	return true;
    }

    /* Print to printer. */
#if defined(_WIN32) /*[*/
    fclose(f);
    unlink(temp_name);
    if (appres.interactive.do_confirms) {
	popup_an_info("Screen image printing.\n");
    }
#endif /*]*/
    Free(temp_name);
    return true;
}

/**
 * Print screen module registration.
 */
void
print_screen_register(void)
{
    static action_table_t print_text_actions[] = {
	{ "PrintText",		PrintText_action,	ACTION_KE },
    };

    /* Register the actions. */
    register_actions(print_text_actions, array_count(print_text_actions));
}
