/*
 * Copyright 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * wpr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	ws.c
 *		Interactions with the Win32 print spooler (winspool).
 */

#include <windows.h>
#include <winspool.h>
#include "localdefs.h"
#include "wsc.h"

#define PRINTER_BUFSIZE	16384

static enum {
    PRINTER_IDLE,	/* not doing anything */
    PRINTER_OPEN,	/* open, but no pending print job */
    PRINTER_JOB		/* print job pending */
} printer_state = PRINTER_IDLE;

static HANDLE printer_handle;

static char printer_buf[PRINTER_BUFSIZE];
static int pbcnt = 0;

/*
 * This is not means a general-purpose interface to the Win32 Print Spooler,
 * but rather the minimum subset needed by wpr3287.
 *
 * The functions generally return 0 for success, and -1 for failure.
 * If a failure occurs, they issue an error message via the 'errmsg' call.
 */

/*
 * Start talking to the named printer.
 * If printer_name is NULL, uses the default printer.
 * This call should should only be made once.
 */
int
ws_start(char *printer_name)
{
    PRINTER_DEFAULTS defaults;

    /* If they didn't specify a printer, grab the default. */
    if (printer_name == NULL) {
	printer_name = ws_default_printer();
	if (printer_name == NULL) {
	    errmsg("ws_start: No default printer");
	    return -1;
	}
    }

    /* Talk to the printer. */
    (void) memset(&defaults, '\0', sizeof(defaults));
    defaults.pDatatype = "RAW";
    defaults.pDevMode = NULL;
    defaults.DesiredAccess = PRINTER_ACCESS_USE;

    if (OpenPrinter(printer_name, &printer_handle, &defaults) == 0) {

	errmsg("ws_start: OpenPrinter failed, "
		"Win32 error %d", GetLastError());
	return -1;
    }

    printer_state = PRINTER_OPEN;
    return 0;
}

/*
 * flush the print buffer.
 */
int
ws_flush(void)
{
    DWORD wrote;
    int rv = 0;

    switch (printer_state) {
	case PRINTER_IDLE:
	    errmsg("ws_endjob: printer not open");
	    return -1;
	case PRINTER_OPEN:
	    return 0;
	case PRINTER_JOB:
	    break;
    }

    if (pbcnt != 0) {

	if (WritePrinter(printer_handle, printer_buf, pbcnt, &wrote) == 0) {
	    errmsg("ws_flush: WritePrinter failed, "
		    "Win32 error %d", GetLastError());
	    rv = -1;
	}

	pbcnt = 0;
    }

    return rv;
}

/*
 * Write a byte to the current print job.
 */
int
ws_putc(char c)
{
    DOC_INFO_1 doc_info;

    switch (printer_state) {

	case PRINTER_IDLE:
	    errmsg("ws_putc: printer not open");
	    return -1;

	case PRINTER_OPEN:
	    /* Start a new document. */
	    doc_info.pDocName = "wpr3287 print job";
	    doc_info.pOutputFile = NULL;
	    doc_info.pDatatype = "RAW";
	    if (StartDocPrinter(printer_handle, 1, (LPBYTE)&doc_info) == 0) {
		errmsg("ws_putc: StartDocPrinter failed, "
			"Win32 error %d", GetLastError());
		return -1;
	    }
	    printer_state = PRINTER_JOB;
	    pbcnt = 0;
	    break;

	case PRINTER_JOB:
	    break;
    }

    /* Flush if needed. */
    if ((pbcnt >= PRINTER_BUFSIZE) && (ws_flush() < 0))
	return -1;

    /* Buffer this character. */
    printer_buf[pbcnt++] = c;
    return 0;
}

/*
 * Write multiple bytes to the current print job.
 */
int
ws_write(char *s, int len)
{
    while (len--) {
	if (ws_putc(*s++) < 0)
	    return -1;
    }
    return 0;
}

/*
 * Complete the current print job.
 * Leaves the connection open for the next job, which is implicitly started
 * by the next call to ws_putc() or ws_write().
 */
int
ws_endjob(void)
{
    int rv = 0;

    switch (printer_state) {
	case PRINTER_IDLE:
	    errmsg("ws_endjob: printer not open");
	    return -1;
	case PRINTER_OPEN:
	    return 0;
	case PRINTER_JOB:
	    break;
    }

    /* Flush whatever's pending. */
    if (ws_flush() < 0)
	rv = 1;

    /* Close out the job. */
    if (EndDocPrinter(printer_handle) == 0) {
	errmsg("ws_endjob: EndDocPrinter failed, "
		"Win32 error %d", GetLastError());
	rv = -1;
    }

    /* Done. */
    printer_state = PRINTER_OPEN;
    return rv;
}

/*
 * Antique method for figuring out the default printer.
 * Needed for compatibility with pre-Win2K systems.
 *
 * For Win2K and later, we could just call GetDefaultPrinter(), but that would
 * require delay-loading winspool.dll, which appears to be beyond MinGW and
 * GNU ld's capabilities at the moment.
 */
char *
ws_default_printer(void)
{
    static char pstring[1024];
    char *comma;

    /* Get the default printer, driver and port "from the .ini file". */
    pstring[0] = '\0';
    if (GetProfileString("windows", "device", "", pstring, sizeof(pstring))
	    == 0) {
	return NULL;
    }

    /*
     * Separate the printer name.  Note that commas are illegal in printer
     * names, so this method is safe.
     */
    comma = strchr(pstring, ',');
    if (comma != NULL)
	*comma = '\0';

    /*
     * If there is no default printer, I don't know if GetProfileString()
     * will fail, or if it will return nothing.  Perpare for the latter.
     */
    if (!*pstring)
	return NULL;

    return pstring;
}
