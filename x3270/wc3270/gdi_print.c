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
#include "tablesc.h"

#include "objects.h"
#include "resources.h"

#include "fprint_screenc.h"
#include "gdi_printc.h"
#include "popupsc.h"
#include "trace_dsc.h"
#include "unicodec.h"
#include "utilc.h"
#include "w3miscc.h"

/* Defines */
#define PPI			72	/* points per inch */
#define DEFAULT_FONTSIZE	8	/* default size is 8pt type */

/* Typedefs */

/* Globals */

/* Statics */
typedef struct {			/* user parameters: */
	int orientation;		/*  orientation */
	double hmargin;			/*  horizontal margin in inches */
	double vmargin;			/*  vertical margin in inches */
	const char *font_name;		/*  font name */
	int font_size;			/*  font size in points */
	int spp;			/*  screens per page */
	Boolean done;			/* done fetching values */
} uparm_t;
static uparm_t uparm;
static struct {				/* printer characteristics: */
	int ppiX, ppiY;			/*  points per inch */
	int poffX, poffY;		/*  left, top physical offsets */
	int horzres, vertres;		/*  resolution (usable area) */
	int pwidth, pheight;		/*  physical width, height */
} pchar;
static struct {				/* printer state */
	char *caption;			/*  caption */
	int out_row;			/*  next row to print to */
	int screens;			/*  number of screens on current page */
	PRINTDLG dlg;			/*  Windows print dialog */
	FLOAT xptscale, yptscale;	/*  x, y point-to-LU scaling factors */
	int hmargin_pixels, vmargin_pixels; /*  margins, in pixels */
	int usable_xpixels, usable_ypixels;/*  usable area (pixels) */
	int usable_cols, usable_rows;	/*  usable area (chars) */
	HFONT font, bold_font, underscore_font, bold_underscore_font;
					/*  fonts */
	SIZE space_size;		/*  size of a space character */
	INT *dx;			/*  spacing array */
} pstate;

/* Forward declarations. */
static void gdi_get_params(uparm_t *up);
static gdi_status_t gdi_init(const char *printer_name, const char **fail);
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
gdi_print_start(const char *printer_name)
{
	const char *fail = "";

	if (!uparm.done) {
		/* Set the defaults. */
		uparm.orientation = 0;
		uparm.hmargin = 0.5;
		uparm.vmargin = 0.5;
		uparm.font_name = NULL;
		uparm.font_size = DEFAULT_FONTSIZE;
		uparm.spp = 1;

		/* Gather up the parameters. */
		gdi_get_params(&uparm);

		/* Don't do this again. */
		uparm.done = True;
	}

	/* Initialize the printer and pop up the dialog. */
	switch (gdi_init(printer_name, &fail)) {
	case GDI_STATUS_SUCCESS:
		trace_event("[gdi] initialized\n");
		break;
	case GDI_STATUS_ERROR:
		popup_an_error("Printer initialization error: %s", fail);
		return GDI_STATUS_ERROR;
	case GDI_STATUS_CANCEL:
		trace_event("[gdi] canceled\n");
		return GDI_STATUS_CANCEL;
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
			trace_event("gdi: unknown %s unit '%s'\n",
				what, nextp);
		}
	} else {
	    trace_event("gdi: invalid %s '%s'\n", what, s);
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
			trace_event("gdi: unknown orientation '%s'\n", s);
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
		l = strtoul(s, &nextp, 0);
		if (l > 0) {
			up->font_size = (int)l;
		} else {
			trace_event("gdi: invalid %s '%s'\n",
				ResPrintTextSize, s);
		}
	}

	/* Screens per page. */
	if ((s = get_resource(ResPrintTextScreensPerPage)) != NULL) {
		l = strtoul(s, &nextp, 0);
		if (l > 0) {
			up->spp = (int)l;
		} else {
			trace_event("gdi: invalid %s '%s'\n",
				ResPrintTextScreensPerPage, s);
		}
	}
}

/*
 * Initalize the named GDI printer. If the name is NULL, use the default
 * printer.
 */
static gdi_status_t
gdi_init(const char *printer_name, const char **fail)
{
	LPDEVMODE devmode;
	HDC dc;
	DOCINFO docinfo;
	DEVNAMES *devnames;
	int rmargin, bmargin; /* right margin, bottom margin */
	int maxphmargin, maxpvmargin;
	int i;
	static char get_fail[1024];

	memset(&pstate.dlg, '\0', sizeof(pstate.dlg));
	pstate.dlg.lStructSize = sizeof(pstate.dlg);
	pstate.dlg.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_HIDEPRINTTOFILE |
	    PD_NOSELECTION;

	if (printer_name != NULL && *printer_name) {
		if (!get_printer_device(printer_name, &pstate.dlg.hDevNames,
			    &pstate.dlg.hDevMode)) {
			snprintf(get_fail, sizeof(get_fail),
				"GetPrinter(%s) failed: %s",
				printer_name? printer_name: "system default",
				win32_strerror(GetLastError()));
			*fail = get_fail;
			goto failed;
		}
		if (uparm.orientation) {
		    devmode = (LPDEVMODE)GlobalLock(pstate.dlg.hDevMode);
		    devmode->dmFields |= DM_ORIENTATION;
		    devmode->dmOrientation = uparm.orientation;
		    GlobalUnlock(devmode);
		}
	}

	if (!PrintDlg(&pstate.dlg)) {
	    	return GDI_STATUS_CANCEL;
	}
	dc = pstate.dlg.hDC;

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
	trace_event("[gdi] Printer '%s' capabilities:\n",
		(char *)devnames + devnames->wDeviceOffset);
	GlobalUnlock(devnames);
	trace_event("[gdi]  LOGPIXELSX %d LOGPIXELSY %d\n",
		pchar.ppiX, pchar.ppiY);
	trace_event("[gdi]  PHYSICALOFFSETX %d PHYSICALOFFSETY %d\n",
		pchar.poffX, pchar.poffY);
	trace_event("[gdi]  HORZRES %d VERTRES %d\n",
		pchar.horzres, pchar.vertres);
	trace_event("[gdi]  PHYSICALWIDTH %d PHYSICALHEIGHT %d\n",
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
	trace_event("[gdi] maxphmargin is %d, maxpvmargin is %d pixels\n",
		maxphmargin, maxpvmargin);

	/* Compute the margins in pixels. */
	pstate.hmargin_pixels = (int)(uparm.hmargin * pchar.ppiX);
	pstate.vmargin_pixels = (int)(uparm.vmargin * pchar.ppiY);

	/* See if the margins are too small. */
	if (pstate.hmargin_pixels < maxphmargin) {
		pstate.hmargin_pixels = maxphmargin;
		trace_event("[gdi] hmargin is too small, setting to %g\"\n",
			(float)pstate.hmargin_pixels / pchar.ppiX);
	}
	if (pstate.vmargin_pixels < maxpvmargin) {
		pstate.vmargin_pixels = maxpvmargin;
		trace_event("[gdi] vmargin is too small, setting to %g\"\n",
			(float)pstate.vmargin_pixels / pchar.ppiX);
	}

	/* See if the margins are too big. */
	if (pstate.hmargin_pixels * 2 >= pchar.horzres) {
		pstate.hmargin_pixels = pchar.ppiX;
		trace_event("[gdi] hmargin is too big, setting to 1\"\n");
	}
	if (pstate.vmargin_pixels * 2 >= pchar.vertres) {
		pstate.vmargin_pixels = pchar.ppiY;
		trace_event("[gdi] vmargin is too big, setting to 1\"\n");
	}

	/*
	 * Compute the usable area in pixels. That's the physical page size
	 * less the margins, now that we know that the margins are reasonable.
	 */
	pstate.usable_xpixels = pchar.pwidth - (2 * pstate.hmargin_pixels);
	pstate.usable_ypixels = pchar.pheight - (2 * pstate.vmargin_pixels);
	trace_event("[gdi] usable area is %dx%d pixels\n",
		pstate.usable_xpixels, pstate.usable_ypixels);

	/* Create the Roman font. */
	pstate.font = CreateFont(
		(int)(uparm.font_size * pstate.yptscale), /* height */
		0,			/* width */
		0,			/* escapement */
		0,			/* orientation */
		FW_NORMAL,		/* weight */
		FALSE,			/* italic */
		FALSE,			/* underline */
		FALSE,			/* strikeout */
		DEFAULT_CHARSET,	/* character set */
		OUT_OUTLINE_PRECIS,	/* output precision */
		CLIP_DEFAULT_PRECIS,	/* clip precision */
		DEFAULT_QUALITY,	/* quality */
		FIXED_PITCH|FF_DONTCARE,/* pitch and family */
		uparm.font_name);	/* face */
	if (pstate.font == NULL) {
		*fail = "CreateFont failed";
		goto failed;
	}

	/* Measure a space to find out the size we got. */
	SelectObject(dc, pstate.font);
	if (!GetTextExtentPoint32(dc, " ", 1, &pstate.space_size)) {
		*fail = "GetTextExtentSize failed";
		goto failed;
	}
	trace_event("[gdi] space character is %dx%d logical units\n",
		(int)pstate.space_size.cx, (int)pstate.space_size.cy);
	pstate.usable_cols = pstate.usable_xpixels / pstate.space_size.cx;
	pstate.usable_rows = pstate.usable_ypixels / pstate.space_size.cy;
	trace_event("[gdi] usable area is %dx%d characters\n",
		pstate.usable_cols, pstate.usable_rows);

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

	return GDI_STATUS_SUCCESS;

failed:
	/* Clean up what we can and return failure. */
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
	Bool fa_high, high;
	Bool fa_underline, underline;
	Bool fa_reverse, reverse;
	unsigned long uc;
	Bool is_dbcs;
	char c;

	devmode = (LPDEVMODE)GlobalLock(pstate.dlg.hDevMode);

	/*
	 * If there is a caption, center it on the first line and skip a line
	 * after that.
	 */
	if (pstate.out_row == 0 && pstate.caption != NULL) {
		int center;

		if ((int)strlen(pstate.caption) < pstate.usable_cols) {
			center = (pstate.usable_xpixels -
			       (strlen(pstate.caption) * pstate.space_size.cx))
			      / 2;
		} else {
		    	center = 0;
		}
		SelectObject(dc, pstate.bold_font);
		status = ExtTextOut(dc,
			pstate.hmargin_pixels - center - pchar.poffX,
			pstate.vmargin_pixels + pstate.space_size.cy -
			    pchar.poffY,
			0, NULL,
			pstate.caption, strlen(pstate.caption), pstate.dx);
		if (status <= 0) {
			*fail = "ExtTextOut failed";
			rc = -1;
			goto done;
		}
		pstate.out_row = 2;
	}

	/* Now dump out a screen's worth. */
	if (ea[fa_addr].gr & GR_INTENSIFY) {
		fa_high = True;
	} else {
		fa_high = FA_IS_HIGH(fa);
	}
	fa_reverse = ((ea[fa_addr].gr & GR_REVERSE) != 0);
	fa_underline = ((ea[fa_addr].gr & GR_UNDERLINE) != 0);

	for (baddr = 0, row = 0; row < ROWS; row++) {
		if (pstate.out_row + row >= pstate.usable_rows) {
		    	break;
		}
		for (col = 0; col < COLS; col++, baddr++) {
			if (ea[baddr].fa) {
				fa = ea[baddr].fa;
				if (ea[baddr].gr & GR_INTENSIFY) {
					fa_high = True;
				} else {
					fa_high = FA_IS_HIGH(fa);
				}
				fa_reverse =
				    ((ea[fa_addr].gr & GR_REVERSE) != 0);
				fa_underline =
				    ((ea[fa_addr].gr & GR_UNDERLINE) != 0);

				/* Just skip it. */
				continue;
			}
			if (col >= pstate.usable_cols) {
				continue;
			}
			is_dbcs = FALSE;
			if (FA_IS_ZERO(fa)) {
#if defined(X3270_DBCS) /*[*/
				if (ctlr_dbcs_state_ea(baddr, ea) == DBCS_LEFT)
					uc = 0x3000;
				else
#endif /*]*/
					uc = ' ';
			} else {
				/* Convert EBCDIC to Unicode. */
#if defined(X3270_DBCS) /*[*/
				switch (ctlr_dbcs_state(baddr)) {
				case DBCS_NONE:
				case DBCS_SB:
					uc = ebcdic_to_unicode(ea[baddr].cc,
						ea[baddr].cs, EUO_NONE);
					if (uc == 0)
						uc = ' ';
					break;
				case DBCS_LEFT:
					is_dbcs = TRUE;
					uc = ebcdic_to_unicode(
						(ea[baddr].cc << 8) |
						 ea[baddr + 1].cc,
						CS_BASE, EUO_NONE);
					if (uc == 0)
						uc = 0x3000;
					break;
				case DBCS_RIGHT:
					/* skip altogether, we took care of it above */
					continue;
				default:
					uc = ' ';
					break;
				}
#else /*][*/
				uc = ebcdic_to_unicode(ea[baddr].cc,
					ea[baddr].cs, EUO_NONE);
				if (uc == 0)
					uc = ' ';
#endif /*]*/
			}

			/*
			 * Figure out the attributes of the current buffer
			 * position.
			 */
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

			/*
			 * Set the bg/fg color and font. Obviously this could
			 * be optimized quite a bit.
			 */
			if (reverse) {
				SetTextColor(dc, 0xffffff);
				SetBkColor(dc, 0);
				SetBkMode(dc, OPAQUE);
			} else {
				SetTextColor(dc, 0);
				SetBkColor(dc, 0xffffff);
				SetBkMode(dc, TRANSPARENT);
			}
			if (!high && !underline) {
				SelectObject(dc, pstate.font);
			} else if (high && !underline) {
				SelectObject(dc, pstate.bold_font);
			} else if (!high && underline) {
				SelectObject(dc, pstate.underscore_font);
			} else {
				SelectObject(dc, pstate.bold_underscore_font);
			}

			/*
			 * Handle spaces and DBCS spaces (U+3000).
			 * If not reverse or underline, just skip over them.
			 * Otherwise, print a space or two spaces, using the
			 * right font and modes.
			 */
			if (uc == ' ' || uc == 0x3000) {
				if (reverse || underline) {
					status = ExtTextOut(dc,
						pstate.hmargin_pixels +
						  (col * pstate.space_size.cx) -
						  pchar.poffX,
						pstate.vmargin_pixels +
						  ((pstate.out_row + row + 1) * pstate.space_size.cy) -
						  pchar.poffY,
						0, NULL,
						"  ",
						(uc == 0x3000)? 2: 1,
						pstate.dx);
					if (status <= 0) {
						*fail = "ExtTextOut failed";
						rc = -1;
						goto done;
					}
				}
				continue;
			}
#if defined(X3270_DBCS) /*[*/
			if (is_dbcs) {
				wchar_t w;
				INT wdx;

				w = (wchar_t)uc;
				wdx = pstate.space_size.cx;

				status = ExtTextOutW(dc,
					pstate.hmargin_pixels + (col * pstate.space_size.cx) - pchar.poffX,
					pstate.vmargin_pixels + ((pstate.out_row + row + 1) * pstate.space_size.cy) - pchar.poffY,
					0, NULL,
					&w, 1, &wdx);
				if (status <= 0) {
					*fail = "ExtTextOutW failed";
					rc = -1;
					goto done;
				}
				continue;
			}
#endif
			c = (char)uc;
			status = ExtTextOut(dc,
				pstate.hmargin_pixels +
				    (col * pstate.space_size.cx) -
				    pchar.poffX,
				pstate.vmargin_pixels +
				    ((pstate.out_row + row + 1) * pstate.space_size.cy) -
				    pchar.poffY,
				0, NULL,
				&c, 1, pstate.dx);
			if (status <= 0) {
				*fail = "ExtTextOut failed";
				rc = -1;
				goto done;
			}
		}
	}

	/* Tally the current screen and see if we need to go to a new page. */
	pstate.out_row += (row + 1); /* current screen plus a gap */
	pstate.screens++;
	if (pstate.out_row >= pstate.usable_rows ||
		    pstate.screens >= uparm.spp) {
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

	return rc;
}

/*
 * Clean up the GDI data structures without attempting any more printing.
 */
static void
gdi_abort(void)
{
	if (pstate.out_row) {
		(void) EndPage(pstate.dlg.hDC);
		pstate.out_row = 0;
	}
	(void) EndDoc(pstate.dlg.hDC);
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
    (void) GetPrinter(h, 2, NULL, 0, &len);
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
    dn->wDriverOffset = offset;
    memcpy((char *)dn + offset, pi->pDriverName, ldn);
    offset += ldn;
    dn->wDeviceOffset = offset;
    memcpy((char *)dn + offset, pi->pPrinterName, lpn);
    offset += lpn;
    dn->wOutputOffset = offset;
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
