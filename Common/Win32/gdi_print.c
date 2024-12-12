/*
 * Copyright (c) 1994-2024 Paul Mattes.
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
 *	gdi_print.c
 *		GDI screen printing functions.
 */

#include "globals.h"

#include <windows.h>
#include <commdlg.h>
#include <winspool.h>
#include <assert.h>

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"

#include "resources.h"

#include "fprint_screen.h"
#include "gdi_print.h"
#include "names.h"
#include "nvt.h"
#include "popups.h"
#include "task.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"
#include "w3misc.h"

/* Defines */
#define PPI			72	/* points per inch */

/* Typedefs */

/* Globals */

/* Statics */
typedef struct {		/* user parameters: */
    int orientation;		/*  orientation */
    double hmargin;		/*  horizontal margin in inches */
    double vmargin;		/*  vertical margin in inches */
    const char *font_name;	/*  font name */
    int font_size;		/*  font size in points */
    int spp;			/*  screens per page */
    bool done;		/* done fetching values */
} uparm_t;
static uparm_t uparm;
static struct {			/* printer characteristics: */
    int ppiX, ppiY;		/*  points per inch */
    int poffX, poffY;		/*  left, top physical offsets */
    int horzres, vertres;	/*  resolution (usable area) */
    int pwidth, pheight;	/*  physical width, height */
} pchar;
static struct {			/* printer state */
    bool active;		/*  is GDI printing active? */
    char *caption;		/*  caption */
    int out_row;		/*  next row to print to */
    int screens;		/*  number of screens on current page */
    PRINTDLG dlg;		/*  Windows print dialog */
    FLOAT xptscale, yptscale;	/*  x, y point-to-LU scaling factors */
    int hmargin_pixels, vmargin_pixels; /*  margins, in pixels */
    int usable_xpixels, usable_ypixels;/*  usable area (pixels) */
    int usable_cols, usable_rows;/*  usable area (chars) */
    HFONT font, bold_font, underscore_font, bold_underscore_font;
    HFONT caption_font;
			        /*  fonts */
    SIZE space_size;		/*  size of a space character */
    INT *dx;			/*  spacing array */

    HANDLE thread;		/* thread to run the print dialog */
    HANDLE done_event;		/* event to signal dialog is done */
    bool cancel;		/* true if dialog canceled */
    void *wait_context;		/* task wait context */
} pstate;
static bool pstate_initted = false;

/* Forward declarations. */
static void gdi_get_params(uparm_t *up);
static gdi_status_t gdi_init(const char *printer_name, unsigned opts,
	const char **fail, void *wait_context);
static int gdi_screenful(struct ea *ea, unsigned short rows,
	unsigned short cols, const char **fail);
static int gdi_done(const char **fail);
static void gdi_abort(void);
static BOOL get_printer_device(const char *printer_name, HGLOBAL *pdevnames,
	HGLOBAL *pdevmode);

/*
 * Initialize printing to a GDI printer.
 */
gdi_status_t
gdi_print_start(const char *printer_name, unsigned opts, void *wait_context)
{
    const char *fail = "";

    if (!uparm.done) {
	/* Set the defaults. */
	uparm.orientation = 0;
	uparm.hmargin = 0.5;
	uparm.vmargin = 0.5;
	uparm.font_name = NULL;
	uparm.font_size = 0; /* auto */
	uparm.spp = 1;

	/* Gather up the parameters. */
	gdi_get_params(&uparm);

	/* Don't do this again. */
	uparm.done = true;
    }

    /* Initialize the printer and pop up the dialog. */
    switch (gdi_init(printer_name, opts, &fail, wait_context)) {
    case GDI_STATUS_SUCCESS:
	vtrace("[gdi] initialized\n");
	break;
    case GDI_STATUS_ERROR:
	popup_an_error("Printer initialization error: %s", fail);
	return GDI_STATUS_ERROR;
    case GDI_STATUS_CANCEL:
	vtrace("[gdi] canceled\n");
	return GDI_STATUS_CANCEL;
    case GDI_STATUS_WAIT:
	vtrace("[gdi] waiting\n");
	return GDI_STATUS_WAIT;
    }

    return GDI_STATUS_SUCCESS;
}

/* Finish printing to a GDI printer. */
gdi_status_t
gdi_print_finish(FILE *f, const char *caption)
{
    size_t nr;
    struct ea *ea_tmp;
    gdi_header_t h;
    const char *fail = "";

    /* Save the caption. */
    if (caption != NULL) {
	Replace(pstate.caption, NewString(caption));
    } else {
	Replace(pstate.caption, NULL);
    }

    /* Allocate the buffer. */
    ea_tmp = Malloc((((maxROWS * maxCOLS) + 1) * sizeof(struct ea)));

    /* Set up the fake fa in location -1. */
    memset(&ea_tmp[0], '\0', sizeof(struct ea));
    ea_tmp[0].fa = FA_PRINTABLE | FA_MODIFY;

    /* Rewind the file. */
    fflush(f);
    rewind(f);

    /* Read it back. */
    while ((nr = fread(&h, sizeof(gdi_header_t), 1, f)) == 1) {
	/* Check the signature. */
	if (h.signature != GDI_SIGNATURE) {
	    popup_an_error("Corrupt temporary file (signature)");
	    goto abort;
	}

	/* Check the screen dimensions. */
	if (h.rows > maxROWS || h.cols > maxCOLS) {
	    popup_an_error("Corrupt temporary file (screen size)");
	    goto abort;
	}

	/* Read the screen image in. */
	if (fread(ea_tmp + 1, sizeof(struct ea), h.rows * h.cols, f) !=
		h.rows * h.cols) {
	    popup_an_error("Truncated temporary file");
	    goto abort;
	}

	/* Process it. */
	if (gdi_screenful(ea_tmp + 1, h.rows, h.cols, &fail) < 0) {
		popup_an_error("Printing error: %s", fail);
		goto abort;
	}
    }
    if (gdi_done(&fail) < 0) {
	popup_an_error("Final printing error: %s", fail);
	goto abort;
    }
    Free(ea_tmp);

    pstate.active = false;
    return GDI_STATUS_SUCCESS;

abort:
    Free(ea_tmp);
    gdi_abort();
    return GDI_STATUS_ERROR;
}

/*
 * Validate and scale a margin value.
 */
static double
parse_margin(char *s, const char *what)
{
    double d;
    char *nextp;

    d = strtod(s, &nextp);
    if (d > 0.0) {
	while (*nextp == ' ') {
	    nextp++;
	}
	if (*nextp == '\0' || *nextp == '"' ||
		!strcasecmp(nextp, "in") ||
		!strcasecmp(nextp, "inch") ||
		!strcasecmp(nextp, "inches")) {
	    /* Do nothing. */
	} else if (!strcasecmp(nextp, "mm")) {
	    d /= 25.4;
	} else if (!strcasecmp(nextp, "cm")) {
	    d /= 2.54;
	} else {
	    vtrace("gdi: unknown %s unit '%s'\n",
		    what, nextp);
	}
    } else {
	vtrace("gdi: invalid %s '%s'\n", what, s);
	return 0;
    }
    return d;
}

/*
 * Gather the user parameters from resources.
 */
static void
gdi_get_params(uparm_t *up)
{
    char *s;
    double d;
    unsigned long l;
    char *nextp;

    /* Orientation. */
    if ((s = get_resource(ResPrintTextOrientation)) != NULL) {
	if (!strcasecmp(s, "portrait")) {
	    up->orientation = DMORIENT_PORTRAIT;
	} else if (!strcasecmp(s, "landscape")) {
	    up->orientation = DMORIENT_LANDSCAPE;
	} else {
	    vtrace("gdi: unknown orientation '%s'\n", s);
	}
    }

    /* Horizontal margin. */
    if ((s = get_resource(ResPrintTextHorizontalMargin)) != NULL) {
	d = parse_margin(s, ResPrintTextHorizontalMargin);
	if (d > 0) {
	    up->hmargin = d;
	}
    }

    /* Vertical margin. */
    if ((s = get_resource(ResPrintTextVerticalMargin)) != NULL) {
	d = parse_margin(s, ResPrintTextVerticalMargin);
	if (d > 0) {
	    up->vmargin = d;
	}
    }

    /* Font name. */
    if ((s = get_resource(ResPrintTextFont)) != NULL) {
	up->font_name = s;
    }

    /* Font size. */
    if ((s = get_resource(ResPrintTextSize)) != NULL) {
	if (strcasecmp(s, KwAuto)) {
	    l = strtoul(s, &nextp, 0);
	    if (l > 0) {
		up->font_size = (int)l;
	    } else {
		vtrace("gdi: invalid %s '%s'\n", ResPrintTextSize, s);
	    }
	}
    }

    /* Screens per page. */
    if ((s = get_resource(ResPrintTextScreensPerPage)) != NULL) {
	l = strtoul(s, &nextp, 0);
	if (l > 0) {
	    up->spp = (int)l;
	} else {
	    vtrace("gdi: invalid %s '%s'\n", ResPrintTextScreensPerPage, s);
	}
    }
}

/*
 * Clean up fonts.
 */
static void
cleanup_fonts(void)
{
    if (pstate.font) {
	DeleteObject(pstate.font);
	pstate.font = NULL;
    }
    if (pstate.bold_font) {
	DeleteObject(pstate.bold_font);
	pstate.bold_font = NULL;
    }
    if (pstate.underscore_font) {
	DeleteObject(pstate.underscore_font);
	pstate.underscore_font = NULL;
    }
    if (pstate.caption_font) {
	DeleteObject(pstate.caption_font);
	pstate.caption_font = NULL;
    }

    pstate.active = false;
}

/*
 * Create a Roman font.
 * Returns 0 for success, -1 for failure.
 */
static int
create_roman_font(HDC dc, int fheight, int fwidth, const char **fail)
{
    char *w, *h;

    w = fwidth? Asprintf("%d", fwidth): NewString("(auto)");
    h = fheight? Asprintf("%d", fheight): NewString("(auto)");
    vtrace("[gdi] requesting a font %sx%s logical units\n", w, h);
    Free(w);
    Free(h);

    pstate.font = CreateFont(
	    fheight,		/* height */
	    fwidth,		/* width */
	    0,			/* escapement */
	    0,			/* orientation */
	    FW_NORMAL,		/* weight */
	    FALSE,		/* italic */
	    FALSE,		/* underline */
	    FALSE,		/* strikeout */
	    DEFAULT_CHARSET,	/* character set */
	    OUT_OUTLINE_PRECIS,	/* output precision */
	    CLIP_DEFAULT_PRECIS,/* clip precision */
	    DEFAULT_QUALITY,	/* quality */
	    FIXED_PITCH|FF_DONTCARE,/* pitch and family */
	    uparm.font_name);	/* face */
    if (pstate.font == NULL) {
	*fail = "CreateFont failed";
	return -1;
    }

    /* Measure a space to find out the size we got. */
    SelectObject(dc, pstate.font);
    if (!GetTextExtentPoint32(dc, " ", 1, &pstate.space_size)) {
	*fail = "GetTextExtentPoint32 failed";
	return -1;
    }
    vtrace("[gdi] space character is %dx%d logical units\n",
	    (int)pstate.space_size.cx, (int)pstate.space_size.cy);
    pstate.usable_cols = pstate.usable_xpixels / pstate.space_size.cx;
    pstate.usable_rows = pstate.usable_ypixels / pstate.space_size.cy;
    vtrace("[gdi] usable area is %dx%d characters\n",
	    pstate.usable_cols, pstate.usable_rows);
    return 0;
}

/*
 * Return the default printer name.
 */
static char *
get_default_printer_name(char *errbuf, size_t errbuf_size)
{
    DWORD size;
    char *buf;

    /* Figure out how much memory to allocate. */
    size = 0;
    GetDefaultPrinter(NULL, &size);
    buf = Malloc(size);
    if (GetDefaultPrinter(buf, &size) == 0) {
	snprintf(errbuf, errbuf_size, "Cannot determine default printer");
	return NULL;
    }
    return buf;
}

/* Thread to post the print dialog. */
static DWORD WINAPI
post_print_dialog(LPVOID lpParameter _is_unused)
{
    if (!PrintDlg(&pstate.dlg)) {
	pstate.cancel = true;
    }
    SetEvent(pstate.done_event);
    return 0;
}

/* The print dialog is complete. */
static void
print_dialog_complete(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    vtrace("Printer dialog complete (%s)\n",
	    pstate.cancel? "cancel": "continue");
    pstate.thread = INVALID_HANDLE_VALUE;
    task_resume_xwait(pstate.wait_context, pstate.cancel,
	    "print dialog complete");
}

/*
 * Hook procedure for the print dialog.
 */
static UINT_PTR CALLBACK
print_dialog_hook(HWND hdlg, UINT ui_msg, WPARAM wparam, LPARAM lparam)
{
    /*
     * Set the window to be topmost. The only thing that seems to work consistently is to do
     * this for every message.
     */
    SetWindowPos(hdlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    return ui_msg == WM_INITDIALOG;
}

/*
 * Initalize the named GDI printer. If the name is NULL, use the default
 * printer.
 */
static gdi_status_t
gdi_init(const char *printer_name, unsigned opts, const char **fail,
	void *wait_context)
{
    char *default_printer_name = NULL;
    LPDEVMODE devmode;
    HDC dc;
    DOCINFO docinfo;
    DEVNAMES *devnames;
    int rmargin, bmargin; /* right margin, bottom margin */
    int maxphmargin, maxpvmargin;
    int i;
    static char get_fail[1024];
    int fheight, fwidth;

    if (!pstate_initted) {
	pstate.thread = INVALID_HANDLE_VALUE;
	pstate.done_event = INVALID_HANDLE_VALUE;
	pstate_initted = true;
    }

    if (pstate.active) {
	*fail = "Only one GDI document at a time";
	goto failed;
    }

    if (pstate.thread != INVALID_HANDLE_VALUE) {
	*fail = "Print dialog already pending";
	goto failed;
    }

    if (!(opts & FPS_DIALOG_COMPLETE)) {
	memset(&pstate.dlg, '\0', sizeof(pstate.dlg));
	pstate.dlg.lStructSize = sizeof(pstate.dlg);
	pstate.dlg.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_HIDEPRINTTOFILE |
	    PD_NOSELECTION | PD_ENABLEPRINTHOOK;
	pstate.dlg.lpfnPrintHook = print_dialog_hook;
    }

    if (printer_name == NULL || !*printer_name) {
	default_printer_name = get_default_printer_name(get_fail,
		sizeof(get_fail));
	if (default_printer_name == NULL) {
	    *fail = get_fail;
	    goto failed;
	}
	printer_name = default_printer_name;
    }
    if (!get_printer_device(printer_name, &pstate.dlg.hDevNames,
		&pstate.dlg.hDevMode)) {
	snprintf(get_fail, sizeof(get_fail),
		"GetPrinter(%s) failed: %s",
		printer_name, win32_strerror(GetLastError()));
	*fail = get_fail;
	goto failed;
    }
    if (uparm.orientation) {
	devmode = (LPDEVMODE)GlobalLock(pstate.dlg.hDevMode);
	devmode->dmFields |= DM_ORIENTATION;
	devmode->dmOrientation = uparm.orientation;
	GlobalUnlock(devmode);
    }

    if (opts & FPS_NO_DIALOG) {
	/* They don't want the print dialog. Allocate a DC for it. */
	devmode = (LPDEVMODE)GlobalLock(pstate.dlg.hDevMode);
	pstate.dlg.hDC = CreateDC("WINSPOOL", printer_name, NULL, devmode);
	GlobalUnlock(devmode);
	if (pstate.dlg.hDC == NULL) {
	    snprintf(get_fail, sizeof(get_fail), "Cannot create DC for "
		    "printer '%s'", printer_name);
	    *fail = get_fail;
	    goto failed;
	}
    } else if (!(opts & FPS_DIALOG_COMPLETE)) {
	if (default_printer_name != NULL) {
	    Free(default_printer_name);
	    default_printer_name = NULL;
	}

	/* Pop up the dialog to get the printer characteristics. */
	pstate.cancel = false;
	pstate.wait_context = wait_context;
	if (pstate.done_event == INVALID_HANDLE_VALUE) {
	    pstate.done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	    AddInput(pstate.done_event, print_dialog_complete);
	} else {
	    ResetEvent(pstate.done_event); /* just in case */
	}
	pstate.cancel = false;
	pstate.thread = CreateThread(NULL, 0, post_print_dialog, NULL, 0, NULL);
	return GDI_STATUS_WAIT;
    }
    dc = pstate.dlg.hDC;

    if (default_printer_name != NULL) {
	Free(default_printer_name);
	default_printer_name = NULL;
    }

    /* Find out the printer characteristics. */

    /* LOGPIXELSX and LOGPIXELSY are the pixels-per-inch for the printer. */
    pchar.ppiX = GetDeviceCaps(dc, LOGPIXELSX);
    if (pchar.ppiX <= 0) {
	*fail = "Can't get LOGPIXELSX";
	goto failed;
    }
    pchar.ppiY = GetDeviceCaps(dc, LOGPIXELSY);
    if (pchar.ppiY <= 0) {
	*fail = "Can't get LOGPIXELSY";
	goto failed;
    }

    /*
     * PHYSICALOFFSETX and PHYSICALOFFSETY are the fixed top and left-hand
     * margins, in pixels. Whatever you print is offset by these amounts, so
     * you have to subtract them from your coordinates. You cannot print in
     * these areas.
     */
    pchar.poffX = GetDeviceCaps(dc, PHYSICALOFFSETX);
    if (pchar.poffX < 0) {
	*fail = "Can't get PHYSICALOFFSETX";
	goto failed;
    }
    pchar.poffY = GetDeviceCaps(dc, PHYSICALOFFSETY);
    if (pchar.poffY < 0) {
	*fail = "Can't get PHYSICALOFFSETY";
	goto failed;
    }

    /*
     * HORZRES and VERTRES are the size of the usable area of the page, in
     * pixels. They implicitly give you the size of the right-hand and
     * bottom physical offsets.
     */
    pchar.horzres = GetDeviceCaps(dc, HORZRES);
    if (pchar.horzres <= 0) {
	*fail = "Can't get HORZRES";
	goto failed;
    }
    pchar.vertres = GetDeviceCaps(dc, VERTRES);
    if (pchar.vertres <= 0) {
	*fail = "Can't get VERTRES";
	goto failed;
    }

    /*
     * PHYSICALWIDTH and PHYSICALHEIGHT are the size of the entire area of
     * the page, in pixels.
     */
    pchar.pwidth = GetDeviceCaps(dc, PHYSICALWIDTH);
    if (pchar.pwidth <= 0) {
	*fail = "Can't get PHYSICALWIDTH";
	goto failed;
    }
    pchar.pheight = GetDeviceCaps(dc, PHYSICALHEIGHT);
    if (pchar.pheight <= 0) {
	*fail = "Can't get PHYSICALHEIGHT";
	goto failed;
    }

    /* Trace the device characteristics. */
    devnames = (DEVNAMES *)GlobalLock(pstate.dlg.hDevNames);
    vtrace("[gdi] Printer '%s' capabilities:\n",
	    (char *)devnames + devnames->wDeviceOffset);
    GlobalUnlock(devnames);
    vtrace("[gdi]  LOGPIXELSX %d LOGPIXELSY %d\n",
	    pchar.ppiX, pchar.ppiY);
    vtrace("[gdi]  PHYSICALOFFSETX %d PHYSICALOFFSETY %d\n",
	    pchar.poffX, pchar.poffY);
    vtrace("[gdi]  HORZRES %d VERTRES %d\n",
	    pchar.horzres, pchar.vertres);
    vtrace("[gdi]  PHYSICALWIDTH %d PHYSICALHEIGHT %d\n",
	    pchar.pwidth, pchar.pheight);

    /* Compute the scale factors (points to pixels). */
    pstate.xptscale = (FLOAT)pchar.ppiX / (FLOAT)PPI;
    pstate.yptscale = (FLOAT)pchar.ppiY / (FLOAT)PPI;

    /* Compute the implied right and bottom margins. */
    rmargin = pchar.pwidth - pchar.horzres - pchar.poffX;
    bmargin = pchar.pheight - pchar.vertres - pchar.poffY;
    if (rmargin > pchar.poffX) {
	maxphmargin = rmargin;
    } else {
	maxphmargin = pchar.poffX;
    }
    if (bmargin > pchar.poffY) {
	maxpvmargin = bmargin;
    } else {
	maxpvmargin = pchar.poffY;
    }
    vtrace("[gdi] maxphmargin is %d, maxpvmargin is %d pixels\n",
	    maxphmargin, maxpvmargin);

    /* Compute the margins in pixels. */
    pstate.hmargin_pixels = (int)(uparm.hmargin * pchar.ppiX);
    pstate.vmargin_pixels = (int)(uparm.vmargin * pchar.ppiY);

    /* See if the margins are too small. */
    if (pstate.hmargin_pixels < maxphmargin) {
	pstate.hmargin_pixels = maxphmargin;
	vtrace("[gdi] hmargin is too small, setting to %g\"\n",
		(float)pstate.hmargin_pixels / pchar.ppiX);
    }
    if (pstate.vmargin_pixels < maxpvmargin) {
	pstate.vmargin_pixels = maxpvmargin;
	vtrace("[gdi] vmargin is too small, setting to %g\"\n",
		(float)pstate.vmargin_pixels / pchar.ppiX);
    }

    /* See if the margins are too big. */
    if (pstate.hmargin_pixels * 2 >= pchar.horzres) {
	pstate.hmargin_pixels = pchar.ppiX;
	vtrace("[gdi] hmargin is too big, setting to 1\"\n");
    }
    if (pstate.vmargin_pixels * 2 >= pchar.vertres) {
	pstate.vmargin_pixels = pchar.ppiY;
	vtrace("[gdi] vmargin is too big, setting to 1\"\n");
    }

    /*
     * Compute the usable area in pixels. That's the physical page size
     * less the margins, now that we know that the margins are reasonable.
     */
    pstate.usable_xpixels = pchar.pwidth - (2 * pstate.hmargin_pixels);
    pstate.usable_ypixels = pchar.pheight - (2 * pstate.vmargin_pixels);
    vtrace("[gdi] usable area is %dx%d pixels\n",
	    pstate.usable_xpixels, pstate.usable_ypixels);

    /*
     * Create the Roman font.
     *
     * If they specified a particular font size, use that as the height,
     * and let the system pick the width.
     *
     * If they did not specify a font size, or chose "auto", then let the
     * "screens per page" drive what to do. If "screens per page" is set,
     * then divide the page Y pixels by the screens-per-page times the
     * display height to get the font height, and let the system pick the
     * width.
     *
     * Otherwise, divide the page X pixels by COLS to get the font width,
     * and let the system pick the height.
     */
    if (uparm.font_size) {
	/* User-specified fixed font size. */
	fheight = (int)(uparm.font_size * pstate.yptscale);
	fwidth = 0;
    } else {
	if (uparm.spp > 1) {
	    /*
	     * Scale the height so the specified number of screens will
	     * fit.
	     */
	    fheight = pstate.usable_ypixels /
		(uparm.spp * maxROWS /* spp screens */
		 + (uparm.spp - 1) /* spaces between screens */
		 + 2 /* space and caption*/ );
	    fwidth = 0;
	} else {
	    /*
	     * Scale the width so a screen will fit the page horizonally.
	     */
	    fheight = 0;
	    fwidth = pstate.usable_xpixels / maxCOLS;
	}
    }
    if (create_roman_font(dc, fheight, fwidth, fail) < 0) {
	goto failed;
    }

    /*
     * If we computed the font size, see if the other dimension is too
     * big. If it is, scale using the other dimension, which is guaranteed to
     * make the original computed dimension no bigger.
     *
     * XXX: This needs more testing.
     */
    if (!uparm.font_size) {
	if (fwidth == 0) {
	    /*
	     * We computed the height because spp > 1. See if the width
	     * overflows.
	     */
	    if (pstate.space_size.cx * maxCOLS > pstate.usable_xpixels) {
		vtrace("[gdi] font too wide, retrying\n");
		DeleteObject(pstate.font);
		pstate.font = NULL;

		fheight = 0;
		fwidth = pstate.usable_xpixels / maxCOLS;
		if (create_roman_font(dc, fheight, fwidth, fail) < 0) {
		    goto failed;
		}
	    }
	} else if (fheight == 0) {
	    /*
	     * We computed the width (spp <= 1). See if the height
	     * overflows.
	     */
	    if (pstate.space_size.cy * (maxROWS + 2) >
		    pstate.usable_xpixels) {
		vtrace("[gdi] font too high, retrying\n");
		DeleteObject(pstate.font);
		pstate.font = NULL;

		fheight = pstate.usable_xpixels / (maxROWS + 2);
		fwidth = 0;
		if (create_roman_font(dc, fheight, fwidth, fail) < 0) {
		    goto failed;
		}
	    }
	}
    }

    /* Create a bold font that is the same size, if possible. */
    pstate.bold_font = CreateFont(
	    pstate.space_size.cy,	/* height */
	    pstate.space_size.cx,	/* width */
	    0,			/* escapement */
	    0,			/* orientation */
	    FW_BOLD,		/* weight */
	    FALSE,			/* italic */
	    FALSE,			/* underline */
	    FALSE,			/* strikeout */
	    ANSI_CHARSET,		/* character set */
	    OUT_OUTLINE_PRECIS,	/* output precision */
	    CLIP_DEFAULT_PRECIS,	/* clip precision */
	    DEFAULT_QUALITY,	/* quality */
	    FIXED_PITCH|FF_DONTCARE,/* pitch and family */
	    uparm.font_name);	/* face */
    if (pstate.bold_font == NULL) {
	*fail = "CreateFont (bold) failed";
	goto failed;
    }

    /* Create an underscore font that is the same size, if possible. */
    pstate.underscore_font = CreateFont(
	    pstate.space_size.cy,	/* height */
	    pstate.space_size.cx,	/* width */
	    0,			/* escapement */
	    0,			/* orientation */
	    FW_NORMAL,		/* weight */
	    FALSE,			/* italic */
	    TRUE,			/* underline */
	    FALSE,			/* strikeout */
	    ANSI_CHARSET,		/* character set */
	    OUT_OUTLINE_PRECIS,	/* output precision */
	    CLIP_DEFAULT_PRECIS,	/* clip precision */
	    DEFAULT_QUALITY,	/* quality */
	    FIXED_PITCH|FF_DONTCARE,/* pitch and family */
	    uparm.font_name);	/* face */
    if (pstate.underscore_font == NULL) {
	*fail = "CreateFont (underscore) failed";
	goto failed;
    }

    /* Create a bold, underscore font that is the same size, if possible. */
    pstate.bold_underscore_font = CreateFont(
	    pstate.space_size.cy,	/* height */
	    pstate.space_size.cx,	/* width */
	    0,			/* escapement */
	    0,			/* orientation */
	    FW_BOLD,		/* weight */
	    FALSE,			/* italic */
	    TRUE,			/* underline */
	    FALSE,			/* strikeout */
	    ANSI_CHARSET,		/* character set */
	    OUT_OUTLINE_PRECIS,	/* output precision */
	    CLIP_DEFAULT_PRECIS,	/* clip precision */
	    DEFAULT_QUALITY,	/* quality */
	    FIXED_PITCH|FF_DONTCARE,/* pitch and family */
	    uparm.font_name);	/* face */
    if (pstate.bold_underscore_font == NULL) {
	*fail = "CreateFont (bold underscore) failed";
	goto failed;
    }

    /* Create a caption font. */
    pstate.caption_font = CreateFont(
	    pstate.space_size.cy,	/* height */
	    0,			/* width */
	    0,			/* escapement */
	    0,			/* orientation */
	    FW_NORMAL,		/* weight */
	    TRUE,			/* italic */
	    FALSE,			/* underline */
	    FALSE,			/* strikeout */
	    ANSI_CHARSET,		/* character set */
	    OUT_OUTLINE_PRECIS,	/* output precision */
	    CLIP_DEFAULT_PRECIS,	/* clip precision */
	    DEFAULT_QUALITY,	/* quality */
	    VARIABLE_PITCH|FF_DONTCARE,/* pitch and family */
	    "Times New Roman");	/* face */
    if (pstate.bold_underscore_font == NULL) {
	*fail = "CreateFont (bold underscore) failed";
	goto failed;
    }

    /* Set up the manual spacing array. */
    pstate.dx = Malloc(sizeof(INT) * maxCOLS);
    for (i = 0; i < maxCOLS; i++) {
	pstate.dx[i] = pstate.space_size.cx;
    }

    /* Fill in the document info. */
    memset(&docinfo, '\0', sizeof(docinfo));
    docinfo.cbSize = sizeof(docinfo);
    docinfo.lpszDocName = "wc3270 screen";

    /* Start the document. */
    if (StartDoc(dc, &docinfo) <= 0) {
	*fail = "StartDoc failed";
	goto failed;
    }

    pstate.active = true;
    return GDI_STATUS_SUCCESS;

failed:
    /* Clean up what we can and return failure. */
    if (default_printer_name != NULL) {
	Free(default_printer_name);
    }
    cleanup_fonts();
    return GDI_STATUS_ERROR;
}

/*
 * Print one screeful to the GDI printer.
 */
static int
gdi_screenful(struct ea *ea, unsigned short rows, unsigned short cols,
	const char **fail)
{
    HDC dc = pstate.dlg.hDC;
    LPDEVMODE devmode;
    int row, col, baddr;
    int rc = 0;
    int status;
    int fa_addr = find_field_attribute_ea(0, ea);
    unsigned char fa = ea[fa_addr].fa;
    bool fa_high, high;
    bool fa_underline, underline;
    bool fa_reverse, reverse;
    ucs4_t uc;
    int usable_rows;
    HFONT got_font = NULL, want_font;
#if defined(GDI_DEBUG) /*[*/
    const char *want_font_name;
#endif /*]*/
    enum { COLOR_NONE, COLOR_NORMAL, COLOR_REVERSE } got_color = COLOR_NONE,
	want_color;

    devmode = (LPDEVMODE)GlobalLock(pstate.dlg.hDevMode);

    /* Compute the usable rows, including the caption. */
    usable_rows = pstate.usable_rows;
    if (pstate.caption) {
	usable_rows -= 2;
    }

    /*
     * Does this screen fit?
     * (Note that the first test, "pstate.out_row", is there so that if the
     * font is so big the image won't fit at all, we still print as much
     * of it as we can.)
     */
    if (pstate.out_row && pstate.out_row + ROWS > usable_rows) {
	if (EndPage(dc) <= 0) {
	    *fail = "EndPage failed";
	    rc = -1;
	    goto done;
	}
	pstate.out_row = 0;
	pstate.screens = 0;
    }

    /* If there is a caption, put it on the last line. */
    if (pstate.out_row == 0 && pstate.caption != NULL) {
	SelectObject(dc, pstate.caption_font);
	status = ExtTextOut(dc,
		pstate.hmargin_pixels - pchar.poffX,
		pstate.vmargin_pixels +
		    ((pstate.usable_rows - 1) * pstate.space_size.cy) -
		    pchar.poffY,
		0, NULL,
		pstate.caption, (UINT)strlen(pstate.caption), NULL);
	if (status <= 0) {
	    *fail = "ExtTextOut(caption) failed";
	    rc = -1;
	    goto done;
	}
    }

    /* Draw a line separating the screens. */
    if (pstate.out_row) {
	HPEN pen;

	pen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
	SelectObject(dc, pen);
	status = MoveToEx(dc, 
		pstate.hmargin_pixels - pchar.poffX,
		pstate.vmargin_pixels +
		    (pstate.out_row * pstate.space_size.cy) +
		    (pstate.space_size.cy / 2) - pchar.poffY,
		    NULL);
	if (status == 0) {
	    *fail = "MoveToEx failed";
	    rc = -1;
	    goto done;
	}
	status = LineTo(dc,
		pstate.hmargin_pixels - pchar.poffX + pstate.usable_xpixels,
		pstate.vmargin_pixels +
		    (pstate.out_row * pstate.space_size.cy) +
		    (pstate.space_size.cy / 2) - pchar.poffY);
	if (status == 0) {
	    *fail = "LineTo failed";
	    rc = -1;
	    goto done;
	}
	DeleteObject(pen);
    }

    /* Now dump out a screen's worth. */
    if (ea[fa_addr].gr & GR_INTENSIFY) {
	fa_high = true;
    } else {
	fa_high = FA_IS_HIGH(fa);
    }
    fa_reverse = ((ea[fa_addr].gr & GR_REVERSE) != 0);
    fa_underline = ((ea[fa_addr].gr & GR_UNDERLINE) != 0);

    for (baddr = 0, row = 0; row < ROWS; row++) {
	if (pstate.out_row + row >= usable_rows) {
	    break;
	}
	for (col = 0; col < COLS; col++, baddr++) {
	    wchar_t w;
	    INT wdx;

	    if (ea[baddr].fa) {
		fa = ea[baddr].fa;
		if (ea[baddr].gr & GR_INTENSIFY) {
		    fa_high = true;
		} else {
		    fa_high = FA_IS_HIGH(fa);
		}
		fa_reverse = ((ea[fa_addr].gr & GR_REVERSE) != 0);
		fa_underline = ((ea[fa_addr].gr & GR_UNDERLINE) != 0);

		/* Just skip it. */
		continue;
	    }
	    if (col >= pstate.usable_cols) {
		continue;
	    }
	    if (FA_IS_ZERO(fa)) {
		if (ctlr_dbcs_state_ea(baddr, ea) == DBCS_LEFT) {
		    uc = 0x3000;
		} else {
		    uc = ' ';
		}
	    } else if (is_nvt(&ea[baddr], false, &uc)) {
		switch (ctlr_dbcs_state(baddr)) {
		case DBCS_NONE:
		case DBCS_SB:
		case DBCS_LEFT:
		    break;
		case DBCS_RIGHT:
		    /* skip altogether, we took care of it above */
		    continue;
		default:
		    uc = ' ';
		    break;
		}
	    } else {
		/* Convert EBCDIC to Unicode. */
		switch (ctlr_dbcs_state(baddr)) {
		case DBCS_NONE:
		case DBCS_SB:
		    uc = ebcdic_to_unicode(ea[baddr].ec, ea[baddr].cs,
			    EUO_NONE);
		    if (uc == 0) {
			uc = ' ';
		    }
		    break;
		case DBCS_LEFT:
		    uc = ebcdic_to_unicode((ea[baddr].ec << 8) |
				ea[baddr + 1].ec,
			    CS_BASE, EUO_NONE);
		    if (uc == 0) {
			uc = 0x3000;
		    }
		    break;
		case DBCS_RIGHT:
		    /* skip altogether, we took care of it above */
		    continue;
		default:
		    uc = ' ';
		    break;
		}
	    }

	    /* Figure out the attributes of the current buffer position. */
	    high = ((ea[baddr].gr & GR_INTENSIFY) != 0);
	    if (!high) {
		high = fa_high;
	    }
	    reverse = ((ea[fa_addr].gr & GR_REVERSE) != 0);
	    if (!reverse) {
		reverse = fa_reverse;
	    }
	    underline = ((ea[fa_addr].gr & GR_UNDERLINE) != 0);
	    if (!underline) {
		underline = fa_underline;
	    }

	    /* Set the bg/fg color and font. */
	    if (reverse) {
		want_color = COLOR_REVERSE;
	    } else {
		want_color = COLOR_NORMAL;
	    }
	    if (want_color != got_color) {
		switch (want_color) {
		case COLOR_REVERSE:
		    SetTextColor(dc, 0xffffff);
		    SetBkColor(dc, 0);
		    SetBkMode(dc, OPAQUE);
		    break;
		case COLOR_NORMAL:
		    SetTextColor(dc, 0);
		    SetBkColor(dc, 0xffffff);
		    SetBkMode(dc, TRANSPARENT);
		    break;
		default:
		    break;
		}
		got_color = want_color;
	    }
	    if (!high && !underline) {
		want_font = pstate.font;
#if defined(GDI_DEBUG) /*[*/
		want_font_name = "Roman";
#endif /*]*/
	    } else if (high && !underline) {
		want_font = pstate.bold_font;
#if defined(GDI_DEBUG) /*[*/
		want_font_name = "Bold";
#endif /*]*/
	    } else if (!high && underline) {
		want_font = pstate.underscore_font;
#if defined(GDI_DEBUG) /*[*/
		want_font_name = "Underscore";
#endif /*]*/
	    } else {
		want_font = pstate.bold_underscore_font;
#if defined(GDI_DEBUG) /*[*/
		want_font_name = "Underscore";
#endif /*]*/
	    }
	    if (want_font != got_font) {
		SelectObject(dc, want_font);
		got_font = want_font;
#if defined(GDI_DEBUG) /*[*/
		vtrace("[gdi] selecting %s\n", want_font_name);
#endif /*]*/
	    }

	    /*
	     * Handle spaces and DBCS spaces (U+3000).
	     * If not reverse or underline, just skip over them.
	     * Otherwise, print a space or two spaces, using the
	     * right font and modes.
	     */
	    if (uc == ' ' || uc == 0x3000) {
		if (reverse || underline) {
		    status = ExtTextOut(dc, pstate.hmargin_pixels +
				(col * pstate.space_size.cx) -
				pchar.poffX,
			    pstate.vmargin_pixels +
				((pstate.out_row + row + 1) *
				 pstate.space_size.cy) -
				pchar.poffY,
			    0, NULL,
			    "  ",
			    (uc == 0x3000)? 2: 1,
			    pstate.dx);
		    if (status <= 0) {
			*fail = "ExtTextOut(space) failed";
			rc = -1;
			goto done;
		    }
		}
		continue;
	    }

	    /*
	     * Emit one character at a time. This should be optimized to print
	     * strings of characters with the same attributes.
	     */
#if defined(GDI_DEBUG) /*[*/
	    if (uc != ' ') {
		vtrace("[gdi] row %d col %d x=%ld y=%ld uc=%lx\n",
			row, col,
			pstate.hmargin_pixels + (col * pstate.space_size.cx) -
			    pchar.poffX,
			pstate.vmargin_pixels +
			  ((pstate.out_row + row + 1) * pstate.space_size.cy) -
			    pchar.poffY,
			uc);
	    }
#endif /*]*/
	    w = (wchar_t)uc;
	    wdx = pstate.space_size.cx;
	    status = ExtTextOutW(dc,
		    pstate.hmargin_pixels + (col * pstate.space_size.cx) -
			pchar.poffX,
		    pstate.vmargin_pixels +
			((pstate.out_row + row + 1) *
			 pstate.space_size.cy) -
			pchar.poffY,
		    0, NULL,
		    &w, 1, &wdx);
	    if (status <= 0) {
		*fail = "ExtTextOutW(image) failed";
		rc = -1;
		goto done;
	    }
	}
    }

    /* Tally the current screen and see if we need to go to a new page. */
    pstate.out_row += (row + 1); /* current screen plus a gap */
    pstate.screens++;
    if (pstate.out_row >= usable_rows || pstate.screens >= uparm.spp) {
	if (EndPage(dc) <= 0) {
	    *fail = "EndPage failed";
	    rc = -1;
	    goto done;
	}
	pstate.out_row = 0;
	pstate.screens = 0;
    }

done:
    GlobalUnlock(devmode);
    return rc;
}

/*
 * Finish the GDI print-out and clean up the data structures.
 */
static int
gdi_done(const char **fail)
{
    int rc = 0;

    if (pstate.out_row) {
	if (EndPage(pstate.dlg.hDC) <= 0) {
	    *fail = "EndPage failed";
	    rc = -1;
	}
	pstate.out_row = 0;
    }
    if (EndDoc(pstate.dlg.hDC) <= 0) {
	*fail = "EndDoc failed";
	rc = -1;
    }

    cleanup_fonts();

    return rc;
}

/*
 * Clean up the GDI data structures without attempting any more printing.
 */
static void
gdi_abort(void)
{
    if (pstate.out_row) {
	EndPage(pstate.dlg.hDC);
	pstate.out_row = 0;
    }
    EndDoc(pstate.dlg.hDC);

    cleanup_fonts();
}

/*
 * Get a DEVMODE and DEVNAMES from a printer name.
 *
 * Returns TRUE for success, FALSE for failure.
 */
static BOOL
get_printer_device(const char *printer_name, HGLOBAL *pdevnames,
	HGLOBAL *pdevmode)
{
    HANDLE h;
    DWORD len, len2;
    PRINTER_INFO_2 *pi;
    size_t dmsize;
    HGLOBAL gdm;
    char *dm;
    size_t ldn;
    size_t lpn;
    size_t ltn;
    HGLOBAL gdn;
    DEVNAMES *dn;
    size_t offset;

    /* Gotta have something to return the values in. */
    if (pdevmode == NULL || pdevnames == NULL) {
	return FALSE;
    }

    /* Open the printer. */
    h = NULL;
    if (!OpenPrinter((char *)printer_name, &h, NULL)) {
	return FALSE;
    }

    /* Get a PRINTER_INFO_2 structure for the printer. */
    GetPrinter(h, 2, NULL, 0, &len);
    pi = (PRINTER_INFO_2 *)malloc(len);
    if (!GetPrinter(h, 2, (LPBYTE)pi, len, &len2)) {
	free(pi);
	ClosePrinter(h);
	return FALSE;
    }
    ClosePrinter(h);
    h = NULL;

    /* Copy the DEVMODE from the PRINTER_INFO_2 into a global handle. */
    dmsize = sizeof(*pi->pDevMode) + pi->pDevMode->dmDriverExtra;
    gdm = GlobalAlloc(GHND, dmsize);
    assert(gdm);
    dm = (char *)GlobalLock(gdm);
    assert(dm);
    memcpy(dm, pi->pDevMode, dmsize);
    GlobalUnlock(gdm);

    /*
     * Compute the size of the DEVNAMES structure from the fields in the
     * PRINTER_INFO_2.
     */
    ldn = strlen(pi->pDriverName)  + 1;
    lpn = strlen(pi->pPrinterName) + 1;
    ltn = strlen(pi->pPortName)    + 1;

    /*
     * Construct a DEVNAMES from the PRINTER_INFO_2, allocated as a global
     * handle.
     */
    gdn = GlobalAlloc(GHND, sizeof(DEVNAMES) + ldn + lpn + ltn);
    assert(gdn);
    dn = (DEVNAMES *)GlobalLock(gdn);
    assert(dn);
    memset(dn, '\0', sizeof(DEVNAMES));
    offset = sizeof(DEVNAMES);
    dn->wDriverOffset = (WORD)offset;
    memcpy((char *)dn + offset, pi->pDriverName, ldn);
    offset += ldn;
    dn->wDeviceOffset = (WORD)offset;
    memcpy((char *)dn + offset, pi->pPrinterName, lpn);
    offset += lpn;
    dn->wOutputOffset = (WORD)offset;
    memcpy((char *)dn + offset, pi->pPortName, ltn);
    dn->wDefault = 0;

    /* Done filling in dn. */
    GlobalUnlock(gdn);

    /* Done with the PRINTER_INFO_2. */
    free(pi);
    pi = NULL;

    /* Return the devmode and devnames. */
    *pdevmode = gdm;
    *pdevnames = gdn;

    /* Success. */
    return TRUE;
}
