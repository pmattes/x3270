/*
 * Copyright (c) 1994-2015 Paul Mattes.
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
 *	fprint_screen.c
 *		Screen printing functions.
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"

#include "resources.h"

#include "fprint_screen.h"
#if defined(_WIN32) /*[*/
# include "gdi_print.h"
#endif /*]*/
#include "trace.h"
#include "unicodec.h"
#include "utf8.h"
#include "utils.h"
#include "varbuf.h"

/* Typedefs */
typedef struct {
	ptype_t ptype;		/* Type P_XXX (text, html, rtf) */
	unsigned opts;		/* FPS_XXX options */
	bool need_separator;	/* Pending page indicator */
	bool broken;		/* If set, output has failed already. */
	int spp;		/* Screens per page. */
	int screens;		/* Screen count this page. */
	FILE *file;		/* Stream to write to */
	char *caption;		/* Caption with %T% expanded */
	char *printer_name;	/* Printer name (used by GDI) */
} real_fps_t;

/* Globals */

/* Statics */

/*
 * Map default 3279 colors.  This code is duplicated three times. ;-(
 */
static int
color_from_fa(unsigned char fa)
{
	static int field_colors[4] = {
		HOST_COLOR_GREEN,        /* default */
		HOST_COLOR_RED,          /* intensified */
		HOST_COLOR_BLUE,         /* protected */
		HOST_COLOR_WHITE         /* protected, intensified */
#       define DEFCOLOR_MAP(f) \
		((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))
	};

	if (appres.m3279)
		return field_colors[DEFCOLOR_MAP(fa)];
	else
		return HOST_COLOR_GREEN;
}

/*
 * Map 3279 colors onto HTML colors.
 */
static char *
html_color(int color)
{
	static char *html_color_map[] = {
		"black",
		"deepSkyBlue",
		"red",
		"pink",
		"green",
		"turquoise",
		"yellow",
		"white",
		"black",
		"blue3",
		"orange",
		"purple",
		"paleGreen",
		"paleTurquoise2",
		"grey",
		"white"
	};
	if (color >= HOST_COLOR_NEUTRAL_BLACK && color <= HOST_COLOR_WHITE)
		return html_color_map[color];
	else
		return "black";
}

/* Convert a caption string to UTF-8 RTF. */
static char *
rtf_caption(const char *caption)
{
    ucs4_t u;
    int consumed;
    enum me_fail error;
    char mb[16];
    varbuf_t r;

    vb_init(&r);

    while (*caption) {
	u = multibyte_to_unicode(caption, strlen(caption), &consumed, &error);
	if (u == 0) {
	    break;
	}
	if (u & ~0x7f) {
	    vb_appendf(&r, "\\u%u?", u);
	} else {
	    (void) unicode_to_multibyte(u, mb, sizeof(mb));
	    if (mb[0] == '\\' || mb[0] == '{' || mb[0] == '}') {
		vb_appendf(&r, "\\%c", mb[0]);
	    } else if (mb[0] == '-') {
		vb_appends(&r, "\\_");
	    } else if (mb[0] == ' ') {
		vb_appends(&r, "\\~");
	    } else {
		vb_append(&r, &mb[0], 1);
	    }
	}

	caption += consumed;
    }
    return vb_consume(&r);
}

/* Convert a caption string to UTF-8 HTML. */
static char *
html_caption(const char *caption)
{
    ucs4_t u;
    int consumed;
    enum me_fail error;
    char u8buf[16];
    int nu8;
    varbuf_t r;

    vb_init(&r);

    while (*caption) {
	u = multibyte_to_unicode(caption, strlen(caption), &consumed, &error);
	if (u == 0) {
	    break;
	}
	switch (u) {
	case '<':
	    vb_appends(&r, "&lt;");
	    break;
	case '>':
	    vb_appends(&r, "&gt;");
	    break;
	case '&':
	    vb_appends(&r, "&amp;");
	    break;
	default:
	    nu8 = unicode_to_utf8(u, u8buf);
	    vb_append(&r, u8buf, nu8);
	    break;
	}
	caption += consumed;
    }
    return vb_consume(&r);
}

/*
 * Write a screen trace header to a stream.
 * Returns the context to use with subsequent calls.
 */
fps_status_t
fprint_screen_start(FILE *f, ptype_t ptype, unsigned opts, const char *caption,
	const char *printer_name, fps_t *fps_ret)
{
    real_fps_t *fps;
    int rv = FPS_STATUS_SUCCESS;
    char *pt_spp;

    /* Non-text types can always generate blank output. */
    if (ptype != P_TEXT) {
	opts |= FPS_EVEN_IF_EMPTY;
    }

    /* Reset and save the state. */
    fps = (real_fps_t *)Malloc(sizeof(real_fps_t));
    fps->ptype = ptype;
    fps->opts = opts;
    fps->need_separator = false;
    fps->broken = false;
    fps->spp = 1;
    fps->screens = 0;
    fps->file = f;

    if (caption != NULL) {
	char *xcaption;
	char *ts = strstr(caption, "%T%");

	if (ts != NULL) {
	    time_t t = time(NULL);
	    struct tm *tm = localtime(&t);

	    xcaption = xs_buffer("%.*s" "%04d-%02d-%02d %02d:%02d:%02d" "%s",
		    (int)(ts - caption), caption,
		    tm->tm_year + 1900,
		    tm->tm_mon + 1,
		    tm->tm_mday,
		    tm->tm_hour,
		    tm->tm_min,
		    tm->tm_sec,
		    ts + 3);
	} else {
	    xcaption = NewString(caption);
	}
	fps->caption = xcaption;
    } else {
	fps->caption = NULL;
    }

    if (printer_name != NULL && printer_name[0]) {
	fps->printer_name = NewString(printer_name);
    } else {
	fps->printer_name = NULL;
    }

    switch (ptype) {
    case P_RTF: {
	char *pt_font = get_resource(ResPrintTextFont);
	char *pt_size = get_resource(ResPrintTextSize);
	int pt_nsize;

	if (pt_font == NULL) {
	    pt_font = "Courier New";
	}
	if (pt_size == NULL) {
	    pt_size = "8";
	}
	pt_nsize = atoi(pt_size);
	if (pt_nsize <= 0) {
	    pt_nsize = 8;
	}

	if (fprintf(f, "{\\rtf1\\ansi\\ansicpg%u\\deff0\\deflang1033{"
		    "\\fonttbl{\\f0\\fmodern\\fprq1\\fcharset0 %s;}}\n"
		    "\\viewkind4\\uc1\\pard\\f0\\fs%d ",
#if defined(_WIN32) /*[*/
		    GetACP(),
#else /*][*/
		    1252, /* the number doesn't matter */
#endif /*]*/
		    pt_font, pt_nsize * 2) < 0) {
	    rv = FPS_STATUS_ERROR;
	}
	if (rv == FPS_STATUS_SUCCESS && fps->caption != NULL) {
	    char *hcaption = rtf_caption(fps->caption);

	    if (fprintf(f, "%s\\par\\par\n", hcaption) < 0) {
		rv = FPS_STATUS_ERROR;
	    }
	    Free(hcaption);
	}
	break;
    }
    case P_HTML: {
	char *hcaption = NULL;

	/* Make the caption HTML-safe. */
	if (fps->caption != NULL) {
	    hcaption = html_caption(fps->caption);
	}

	/* Print the preamble. */
	if (!(opts & FPS_NO_HEADER) &&
		fprintf(f, "<html>\n"
		   "<head>\n"
		   " <meta http-equiv=\"Content-Type\" "
		     "content=\"text/html; charset=UTF-8\">\n"
		   "</head>\n"
		   " <body>\n") < 0) {
	    rv = FPS_STATUS_ERROR;
	}
	if (rv == FPS_STATUS_SUCCESS && hcaption) {
	    if (fprintf(f, "<p>%s</p>\n", hcaption) < 0) {
		rv = FPS_STATUS_ERROR;
	    }
	    Free(hcaption);
	}
	break;
    }
    case P_TEXT:
	if (fps->caption != NULL) {
	    if (fprintf(f, "%s\n\n", fps->caption) < 0) {
		rv = FPS_STATUS_ERROR;
	    }
	}
	break;
    case P_GDI:
#if defined(_WIN32) /*[*/
	switch (gdi_print_start(printer_name, opts)) {
	case GDI_STATUS_SUCCESS:
	    break;
	case GDI_STATUS_ERROR:
	    rv = FPS_STATUS_ERROR;
	    break;
	case GDI_STATUS_CANCEL:
	    rv = FPS_STATUS_CANCEL;
	    break;
	}
#endif /*]*/
	break;
    }

    /* Set up screens-per-page. */
    pt_spp = get_resource(ResPrintTextScreensPerPage);
    if (pt_spp != NULL) {
	fps->spp = atoi(pt_spp);
	if (fps->spp < 1 || fps->spp > 5) {
	    fps->spp = 1;
	}
    }

    if (rv != FPS_STATUS_SUCCESS) {
	/* We've failed; there's no point in returning the context. */
	Free(fps->caption);
	Free(fps->printer_name);
	Free(fps);
	*fps_ret = NULL;
    } else {
	*fps_ret = (fps_t)(void *)fps;
    }

    return rv;
}

#define FAIL do { \
    rv = -1; \
    goto done; \
} while(false)

/*
 * Add a screen image to a stream.
 *
 * Returns 0 for no screen written, 1 for screen written, -1 for error.
 */
fps_status_t
fprint_screen_body(fps_t ofps)
{
	real_fps_t *fps = (real_fps_t *)(void *)ofps;
	register int i;
	unsigned long uc;
	int ns = 0;
	int nr = 0;
	bool any = false;
	int fa_addr = find_field_attribute(0);
	unsigned char fa = ea_buf[fa_addr].fa;
	int fa_fg, current_fg;
	int fa_bg, current_bg;
	bool fa_high, current_high;
	bool fa_ital, current_ital;
	bool mi;
#if defined(_WIN32) /*[*/
	gdi_header_t h;
#endif /*]*/
	fps_status_t rv = FPS_STATUS_SUCCESS;

	/* Quick short-circuit. */
	if (fps == NULL || fps->broken) {
		return FPS_STATUS_ERROR;
	}

	mi = ((fps->opts & FPS_MODIFIED_ITALIC)) != 0;
	if (ea_buf[fa_addr].fg)
		fa_fg = ea_buf[fa_addr].fg & 0x0f;
	else
		fa_fg = color_from_fa(fa);
	current_fg = fa_fg;

	if (ea_buf[fa_addr].bg)
		fa_bg = ea_buf[fa_addr].bg & 0x0f;
	else
		fa_bg = HOST_COLOR_BLACK;
	current_bg = fa_bg;

	if (ea_buf[fa_addr].gr & GR_INTENSIFY)
		fa_high = true;
	else
		fa_high = FA_IS_HIGH(fa);
	current_high = fa_high;
	fa_ital = mi && FA_IS_MODIFIED(fa);
	current_ital = fa_ital;

	switch (fps->ptype) {
	case P_RTF:
		if (fps->need_separator) {
		    	if (fps->screens < fps->spp) {
				if (fprintf(fps->file, "\\par\n") < 0) {
				    	FAIL;
				}
			} else {
				if (fprintf(fps->file, "\n\\page\n") < 0) {
					FAIL;
				}
				fps->screens = 0;
			}
		}
		if (current_high) {
			if (fprintf(fps->file, "\\b ") < 0) {
				FAIL;
			}
		}
		break;
	case P_HTML:
		if (fprintf(fps->file, "  <table border=0>"
			   "<tr bgcolor=black><td>"
			   "<pre><span style=\"color:%s;"
			                       "background:%s;"
					       "font-weight:%s;"
					       "font-style:%s\">",
			   html_color(current_fg),
			   html_color(current_bg),
			   current_high? "bold": "normal",
			   current_ital? "italic": "normal") < 0)
			FAIL;
		break;
	case P_TEXT:
		if (fps->need_separator) {
			if ((fps->opts & FPS_FF_SEP) &&
				fps->screens >= fps->spp) {

				if (fputc('\f', fps->file) < 0)
					FAIL;
				fps->screens = 0;
			} else {
			    	for (i = 0; i < COLS; i++) {
					if (fputc('=', fps->file) < 0)
						FAIL;
				}
				if (fputc('\n', fps->file) < 0)
					FAIL;
			}
		}
		break;
#if defined(_WIN32) /*[*/
	case P_GDI:
		/*
		 * Write the current screen buffer to the file.
		 * We will read it back and print it when we are done.
		 */
		h.signature = GDI_SIGNATURE;
		h.rows = ROWS;
		h.cols = COLS;
		if (fwrite(&h, sizeof(h), 1, fps->file) != 1) {
			FAIL;
		}
		if (fwrite(ea_buf, sizeof(struct ea), ROWS * COLS, fps->file)
			    != ROWS * COLS) {
			FAIL;
		}
		fflush(fps->file);
		goto done;
#endif /*]*/
	default:
		break;
	}

	fps->need_separator = false;

	for (i = 0; i < ROWS*COLS; i++) {
		char mb[16];
		int nmb;

		uc = 0;

		if (i && !(i % COLS)) {
		    	if (fps->ptype == P_HTML) {
			    	if (fputc('\n', fps->file) < 0)
					FAIL;
			} else
				nr++;
			ns = 0;
		}
		if (ea_buf[i].fa) {
			uc = ' ';
			fa = ea_buf[i].fa;
			if (ea_buf[i].fg)
				fa_fg = ea_buf[i].fg & 0x0f;
			else
				fa_fg = color_from_fa(fa);
			if (ea_buf[i].bg)
				fa_bg = ea_buf[i].bg & 0x0f;
			else
				fa_bg = HOST_COLOR_BLACK;
			if (ea_buf[i].gr & GR_INTENSIFY)
				fa_high = true;
			else
				fa_high = FA_IS_HIGH(fa);
			fa_ital = mi && FA_IS_MODIFIED(fa);
		}
		if (FA_IS_ZERO(fa)) {
			if (ctlr_dbcs_state(i) == DBCS_LEFT)
			    	uc = 0x3000;
			else
				uc = ' ';
		} else {
		    	/* Convert EBCDIC to Unicode. */
			switch (ctlr_dbcs_state(i)) {
			case DBCS_NONE:
			case DBCS_SB:
			    	uc = ebcdic_to_unicode(ea_buf[i].cc,
					ea_buf[i].cs, EUO_NONE);
				if (uc == 0)
				    	uc = ' ';
				break;
			case DBCS_LEFT:
				uc = ebcdic_to_unicode(
					(ea_buf[i].cc << 8) |
					 ea_buf[i + 1].cc,
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
		}

		/* Translate to a type-specific format and write it out. */
		if (uc == ' ' && fps->ptype != P_HTML) {
			ns++;
		} else if (uc == 0x3000) {
		    	if (fps->ptype == P_HTML) {
			    	if (fprintf(fps->file, "  ") < 0)
					FAIL;
			} else
				ns += 2;
		} else {
			while (nr) {
			    	if (fps->ptype == P_RTF)
				    	if (fprintf(fps->file, "\\par") < 0)
						FAIL;
				if (fputc('\n', fps->file) < 0)
					FAIL;
				nr--;
			}
			while (ns) {
			    	if (fps->ptype == P_RTF) {
				    	if (fprintf(fps->file, "\\~") < 0)
						FAIL;
				} else
					if (fputc(' ', fps->file) < 0)
						FAIL;
				ns--;
			}
			if (fps->ptype == P_RTF) {
				bool high;

				if (ea_buf[i].gr & GR_INTENSIFY)
					high = true;
				else
					high = fa_high;
				if (high != current_high) {
					if (high) {
						if (fprintf(fps->file, "\\b ")
							    < 0)
							FAIL;
					} else
						if (fprintf(fps->file, "\\b0 ")
							    < 0)
							FAIL;
					current_high = high;
				}
			}
			if (fps->ptype == P_HTML) {
				int fg_color, bg_color;
				bool high;

				if (ea_buf[i].fg)
					fg_color = ea_buf[i].fg & 0x0f;
				else
					fg_color = fa_fg;
				if (ea_buf[i].bg)
					bg_color = ea_buf[i].bg & 0x0f;
				else
					bg_color = fa_bg;
				if (ea_buf[i].gr & GR_REVERSE) {
				    	int tmp;

					tmp = fg_color;
					fg_color = bg_color;
					bg_color = tmp;
				}

				if (i == cursor_addr) {
				    	fg_color = (bg_color == HOST_COLOR_RED)?
							HOST_COLOR_BLACK: bg_color;
					bg_color = HOST_COLOR_RED;
				}
				if (ea_buf[i].gr & GR_INTENSIFY)
					high = true;
				else
					high = fa_high;

				if (fg_color != current_fg ||
				    bg_color != current_bg ||
				    high != current_high ||
				    fa_ital != current_ital) {
					if (fprintf(fps->file,
						"</span><span "
						"style=\"color:%s;"
						"background:%s;"
						"font-weight:%s;"
						"font-style:%s\">",
						html_color(fg_color),
						html_color(bg_color),
						high? "bold": "normal",
						fa_ital? "italic": "normal")
						    < 0)
					    FAIL;
					current_fg = fg_color;
					current_bg = bg_color;
					current_high = high;
					current_ital = fa_ital;
				}
			}
			any = true;
			if (fps->ptype == P_RTF) {
				if (uc & ~0x7f) {
					if (fprintf(fps->file, "\\u%ld?", uc)
						    < 0)
						FAIL;
				} else {
					nmb = unicode_to_multibyte(uc,
						mb, sizeof(mb));
					if (mb[0] == '\\' ||
						mb[0] == '{' ||
						mb[0] == '}') {
						if (fprintf(fps->file, "\\%c",
							    mb[0]) < 0)
							FAIL;
					} else if (mb[0] == '-') {
						if (fprintf(fps->file, "\\_")
							    < 0)
							FAIL;
					} else if (mb[0] == ' ') {
						if (fprintf(fps->file, "\\~")
							    < 0)
							FAIL;
					} else {
						if (fputc(mb[0], fps->file) < 0)
							FAIL;
					}
				}
			} else if (fps->ptype == P_HTML) {
				if (uc == '<') {
					if (fprintf(fps->file, "&lt;") < 0)
						FAIL;
				} else if (uc == '&') {
				    	if (fprintf(fps->file, "&amp;") < 0)
						FAIL;
				} else if (uc == '>') {
				    	if (fprintf(fps->file, "&gt;") < 0)
						FAIL;
				} else {
					nmb = unicode_to_utf8(uc, mb);
					{
					    int k;

					    for (k = 0; k < nmb; k++) {
						if (fputc(mb[k], fps->file) < 0)
							FAIL;
					    }
					}
				}
			} else {
				nmb = unicode_to_multibyte(uc,
					mb, sizeof(mb));
				if (fputs(mb, fps->file) < 0)
					FAIL;
			}
		}
	}

	if (fps->ptype == P_HTML) {
	    	if (fputc('\n', fps->file) < 0)
			FAIL;
	} else
		nr++;
	if (!any && !(fps->opts & FPS_EVEN_IF_EMPTY) && fps->ptype == P_TEXT) {
		return FPS_STATUS_SUCCESS;
	}
	while (nr) {
	    	if (fps->ptype == P_RTF)
		    	if (fprintf(fps->file, "\\par") < 0)
				FAIL;
		if (fps->ptype == P_TEXT)
			if (fputc('\n', fps->file) < 0)
				FAIL;
		nr--;
	}
	if (fps->ptype == P_HTML)
		if (fprintf(fps->file, "%s</span></pre></td></tr>\n"
		           "  </table>\n",
			   current_high? "</b>": "") < 0)
			FAIL;
	fps->need_separator = true;
	fps->screens++;
	rv = FPS_STATUS_SUCCESS_WRITTEN; /* wrote a screen */

    done:
	if (FPS_IS_ERROR(rv)) {
		fps->broken = true;
	}
	return rv;
}

#undef FAIL

/*
 * Finish writing a multi-screen image.
 * Returns 0 success, -1 for error. In either case, the context is freed.
 */
fps_status_t
fprint_screen_done(fps_t *ofps)
{
	real_fps_t *fps = (real_fps_t *)*(void **)ofps;
	int rv = FPS_STATUS_SUCCESS;

	if (fps == NULL) {
		return FPS_STATUS_ERROR;
	}

	if (!fps->broken) {
		switch (fps->ptype) {
		case P_RTF:
			if (fprintf(fps->file, "\n}\n%c", 0) < 0) {
				rv = FPS_STATUS_ERROR;
			}
			break;
		case P_HTML:
			if (!(fps->opts & FPS_NO_HEADER) &&
				fprintf(fps->file, " </body>\n</html>\n") < 0) {
				rv = FPS_STATUS_ERROR;
			}
			break;
#if defined(_WIN32) /*[*/
		case P_GDI:
			vtrace("Printing to GDI printer\n");
			if (gdi_print_finish(fps->file, fps->caption) < 0) {
				rv = FPS_STATUS_ERROR;
			}
			break;
#endif /*]*/
		default:
			break;
		}
	}

	/* Done with the context. */
	Free(fps->caption);
	Free(fps->printer_name);
	memset(fps, '\0', sizeof(*fps));
	Free(*(void **)ofps);
	*(void **)ofps = NULL;

	return rv;
}

/*
 * Write a header, screen image, and trailer to a file.
 */
fps_status_t
fprint_screen(FILE *f, ptype_t ptype, unsigned opts, const char *caption,
	const char *printer_name)
{
	fps_t fps;
	fps_status_t srv;
	fps_status_t srv_body;

	srv = fprint_screen_start(f, ptype, opts, caption, printer_name, &fps);
	if (FPS_IS_ERROR(srv)) {
		return srv;
	}
	srv_body = fprint_screen_body(fps);
	if (FPS_IS_ERROR(srv_body)) {
		(void) fprint_screen_done(&fps);
		return srv_body;
	}
	srv = fprint_screen_done(&fps);
	if (FPS_IS_ERROR(srv)) {
		return srv;
	}
	return srv_body;
}
