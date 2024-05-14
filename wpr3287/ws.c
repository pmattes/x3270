/*
 * Copyright (c) 2007-2024 Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ws.c
 *		Interactions with the Win32 print spooler (winspool).
 */

#include "globals.h"

#include <winspool.h>
#include <sys/stat.h>

#include "utils.h"
#include "wsc.h"

#define PRINTER_BUFSIZE	16384

static enum {
    PRINTER_IDLE,	/* not doing anything */
    PRINTER_OPEN,	/* open, but no pending print job */
    PRINTER_JOB		/* print job pending */
} printer_state = PRINTER_IDLE;

static HANDLE printer_handle = INVALID_HANDLE_VALUE;
static char *printer_dir = NULL;
static FILE *printer_file = NULL;

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
static int
ws_start_printer(const char *printer_name)
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
    memset(&defaults, '\0', sizeof(defaults));
    defaults.pDatatype = "RAW";
    defaults.pDevMode = NULL;
    defaults.DesiredAccess = PRINTER_ACCESS_USE;

    if (OpenPrinter((char *)printer_name, &printer_handle, &defaults) == 0) {

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
static int
ws_flush_printer(void)
{
    DWORD wrote;
    int rv = 0;

    switch (printer_state) {
    case PRINTER_IDLE:
	errmsg("ws_flush: printer not open");
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
static int
ws_putc_printer(int c)
{
    DOC_INFO_1 doc_info;

    switch (printer_state) {

    case PRINTER_IDLE:
	errmsg("ws_putc: printer not open");
	return -1;

    case PRINTER_OPEN:
	/* Start a new document. */
	doc_info.pDocName = "pr3287 print job";
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
    if ((pbcnt >= PRINTER_BUFSIZE) && (ws_flush_printer() < 0))
	return -1;

    /* Buffer this character. */
    printer_buf[pbcnt++] = (char)c;
    return 0;
}

/*
 * Write multiple bytes to the current print job.
 */
static int
ws_write_printer(char *s, int len)
{
    while (len--) {
	if (ws_putc_printer(*s++) < 0)
	    return -1;
    }
    return 0;
}

/*
 * Complete the current print job.
 * Leaves the connection open for the next job, which is implicitly started
 * by the next call to ws_putc() or ws_write().
 */
static int
ws_endjob_printer(void)
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
    if (ws_flush_printer() < 0)
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

/* Print-to-file versions of the functions. */

static int
ws_start_file(const char *printer_name)
{
    printer_dir = strdup(printer_name);
    return 0;
}

static int
ws_endjob_file(void)
{
    int rc;

    if (printer_file == NULL) {
	return 0;
    }
    rc = fclose(printer_file);
    printer_file = NULL;
    return rc;
}

static int
ws_flush_file(void)
{
    return fflush(printer_file);
}

/* Open a print file. */
static FILE *
ws_open_file(void)
{
    int iter = 0;
    char *path = NULL;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    FILE *f;

    /* Parts of the printer file name. */
#   define PATH_PFX	"%s\\print-%04d%02d%02d-%02d%02d%02d"
#   define PATH_ITER	".%d"
#   define PATH_SFX	".txt"

    while (true) {
	path = Asprintf(iter? PATH_PFX PATH_ITER PATH_SFX: PATH_PFX PATH_SFX,
	    printer_dir,
	    tm->tm_year + 1900,
	    tm->tm_mon + 1,
	    tm->tm_mday,
	    tm->tm_hour,
	    tm->tm_min,
	    tm->tm_sec,
	    iter);
	if (access(path, F_OK) == 0) {
	    iter++;
	    free(path);
	    continue;
	} else {
	    break;
	}
    }
    f = fopen(path, "w");
    if (f == NULL) {
	errmsg("ws_putc: fopen(%s) failed: %s", path, strerror(errno));
    }
    free(path);
    return f;
}

static int
ws_putc_file(int c)
{
    int rc;

    if (printer_file == NULL && (printer_file = ws_open_file()) == NULL) {
	return -1;
    }

    rc = fputc(c, printer_file);
    return (rc == EOF)? -1: 0;
}

/*
 * Generic versions of the functions, call the appropriate file- or
 * printer-based version.
 */

int
ws_write_file(char *s, int len)
{
    int i = len;

    while (i-- > 0) {
	if (ws_putc_file(*s) < 0) {
	    return -1;
	}
    }
    return 0;
}

int
ws_start(const char *printer_name)
{
    struct stat buf;

    if (stat(printer_name, &buf) == 0 && (buf.st_mode & S_IFMT) == S_IFDIR) {
	return ws_start_file(printer_name);
    }
    return ws_start_printer(printer_name);
}

int
ws_endjob(void)
{
    return (printer_dir != NULL)? ws_endjob_file(): ws_endjob_printer();
}

int
ws_flush(void)
{
    return (printer_dir != NULL)? ws_flush_file(): ws_flush_printer();
}

int
ws_putc(int c)
{
    return (printer_dir != NULL)? ws_putc_file(c): ws_putc_printer(c);
}

int
ws_write(char *s, int len)
{
    return (printer_dir != NULL)?
	ws_write_file(s, len): ws_write_printer(s, len);
}
