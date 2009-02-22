/*
 * Copyright (c) 1994-2009, Paul Mattes.
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
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#endif /*]*/

#include "objects.h"
#include "resources.h"

#include "actionsc.h"
#include "charsetc.h"
#include "popupsc.h"
#include "printc.h"
#include "unicodec.h"
#include "utf8c.h"
#include "utilc.h"

#if defined(_WIN32) /*[*/
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif /*]*/

#if defined(_MSC_VER) /*[*/
#include "Msc/deprecated.h"
#endif /*]*/

/* Globals */
#if defined(X3270_DISPLAY) /*[*/
char *print_text_command = NULL;
Boolean ptc_changed = FALSE;
#endif /*]*/

/* Statics */

#if defined(X3270_DISPLAY) /*[*/
static Widget print_text_shell = (Widget)NULL;
static Widget save_text_shell = (Widget)NULL;
static Widget print_window_shell = (Widget)NULL;
char *print_window_command = CN;
#endif /*]*/


/* Print Text popup */

/*
 * Map default 3279 colors.  This code is duplicated three times. ;-(
 */
static int
color_from_fa(unsigned char fa)
{
	static int field_colors[4] = {
		COLOR_GREEN,        /* default */
		COLOR_RED,          /* intensified */
		COLOR_BLUE,         /* protected */
		COLOR_WHITE         /* protected, intensified */
#       define DEFCOLOR_MAP(f) \
		((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))
	};

	if (appres.m3279)
		return field_colors[DEFCOLOR_MAP(fa)];
	else
		return COLOR_GREEN;
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
	if (color >= COLOR_NEUTRAL_BLACK && color <= COLOR_WHITE)
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
	char *result = Malloc(1);
	int rlen = 1;
	char uubuf[64];
	char mb[16];
	int nmb;

	result[0] = '\0';

	while (*caption) {
		u = multibyte_to_unicode(caption, strlen(caption), &consumed,
			&error);
		if (u == 0)
		    	break;
		if (u & ~0x7f) {
			sprintf(uubuf, "\\u%u?", u);
		} else {
			nmb = unicode_to_multibyte(u, mb, sizeof(mb));
			if (mb[0] == '\\' ||
			    mb[0] == '{' ||
			    mb[0] == '}')
				sprintf(uubuf, "\\%c", mb[0]);
			else if (mb[0] == '-')
				sprintf(uubuf, "\\_");
			else if (mb[0] == ' ')
				sprintf(uubuf, "\\~");
			else {
			    	uubuf[0] = mb[0];
				uubuf[1] = '\0';
			}
		}
		result = Realloc(result, rlen + strlen(uubuf));
		strcat(result, uubuf);
		rlen += strlen(uubuf);

		caption += consumed;
	}
	return result;
}

/* Convert a caption string to UTF-8 HTML. */
static char *
html_caption(const char *caption)
{
    	ucs4_t u;
	int consumed;
	enum me_fail error;
	char *result = Malloc(1);
	int rlen = 1;
	char u8buf[16];
	int nu8;

	result[0] = '\0';

	while (*caption) {
		u = multibyte_to_unicode(caption, strlen(caption), &consumed,
			&error);
		if (u == 0)
		    	break;
		switch (u) {
		case '<':
		    	result = Realloc(result, rlen + 4);
			strcat(result, "&lt;");
			rlen += 4;
			break;
		case '>':
		    	result = Realloc(result, rlen + 4);
			strcat(result, "&gt;");
			rlen += 4;
			break;
		case '&':
		    	result = Realloc(result, rlen + 5);
			strcat(result, "&amp;");
			rlen += 5;
			break;
		default:
			nu8 = unicode_to_utf8(u, u8buf);
			result = Realloc(result, rlen + nu8);
			memcpy(result + rlen - 1, u8buf, nu8);
			rlen += nu8;
			result[rlen - 1] = '\0';
			break;
		}
		caption += consumed;
	}
	return result;
}

/*
 * Print the ASCIIfied contents of the screen onto a stream.
 * Returns True if anything printed, False otherwise.
 * 
 * 'ptype' can specify:
 *  P_TEXT: Ordinary text
 *  P_HTML: HTML
 *  P_RTF: Windows rich text
 *
 * 'opts' is an OR of:
 *  FPS_EVEN_IF_EMPTY	Create a file even if the screen is clear
 *  FPS_MODIFIED_ITALIC	Print modified fields in italic
 *    font-style:normal|italic
 */
Boolean
fprint_screen(FILE *f, ptype_t ptype, unsigned opts, char *caption)
{
	register int i;
	unsigned long uc;
	int ns = 0;
	int nr = 0;
	Boolean any = False;
	int fa_addr = find_field_attribute(0);
	unsigned char fa = ea_buf[fa_addr].fa;
	int fa_fg, current_fg;
	int fa_bg, current_bg;
	Bool fa_high, current_high;
	Bool fa_ital, current_ital;
	Bool mi = ((opts & FPS_MODIFIED_ITALIC)) != 0;
	char *xcaption = NULL;

	if (caption != NULL) {
	    	char *ts = strstr(caption, "%T%");

		if (ts != NULL) {
		    	time_t t = time(NULL);
			struct tm *tm = localtime(&t);

		    	xcaption = Malloc(strlen(caption) + 1 - 3 + 19);
			strncpy(xcaption, caption, ts - caption);
			sprintf(xcaption + (ts - caption),
				"%04d-%02d-%02d %02d:%02d:%02d",
				tm->tm_year + 1900,
				tm->tm_mon + 1,
				tm->tm_mday,
				tm->tm_hour,
				tm->tm_min,
				tm->tm_sec);
			strcat(xcaption, ts + 3);
		} else {
		    	xcaption = NewString(caption);
		}
	}

	if (ptype != P_TEXT) {
		opts |= FPS_EVEN_IF_EMPTY;
	}

	if (ea_buf[fa_addr].fg)
		fa_fg = ea_buf[fa_addr].fg & 0x0f;
	else
		fa_fg = color_from_fa(fa);
	current_fg = fa_fg;

	if (ea_buf[fa_addr].bg)
		fa_bg = ea_buf[fa_addr].bg & 0x0f;
	else
		fa_bg = COLOR_BLACK;
	current_bg = fa_bg;

	if (ea_buf[fa_addr].gr & GR_INTENSIFY)
		fa_high = True;
	else
		fa_high = FA_IS_HIGH(fa);
	current_high = fa_high;
	fa_ital = mi && FA_IS_MODIFIED(fa);
	current_ital = fa_ital;

	if (ptype == P_RTF) {
		char *pt_font = get_resource(ResPrintTextFont);
		char *pt_size = get_resource(ResPrintTextSize);
		int pt_nsize;

		if (pt_font == CN)
		    	pt_font = "Courier New";
		if (pt_size == CN)
		    	pt_size = "8";
		pt_nsize = atoi(pt_size);
		if (pt_nsize <= 0)
		    	pt_nsize = 8;

		fprintf(f, "{\\rtf1\\ansi\\ansicpg%u\\deff0\\deflang1033{\\fonttbl{\\f0\\fmodern\\fprq1\\fcharset0 %s;}}\n"
			    "\\viewkind4\\uc1\\pard\\f0\\fs%d ",
#if defined(_WIN32) /*[*/
			    GetACP(),
#else /*][*/
			    1252, /* the number doesn't matter */
#endif /*]*/
			    pt_font, pt_nsize * 2);
		if (xcaption != NULL) {
		    	char *hcaption = rtf_caption(xcaption);

			fprintf(f, "%s\\par\\par\n", hcaption);
			Free(hcaption);
		}
		if (current_high)
		    	fprintf(f, "\\b ");
	}

	if (ptype == P_HTML) {
	    	char *hcaption = NULL;

	    	/* Make the caption HTML-safe. */
	    	if (xcaption != NULL)
			hcaption = html_caption(xcaption);

	    	/* Print the preamble. */
		fprintf(f, "<html>\n"
			   "<head>\n"
			   " <meta http-equiv=\"Content-Type\" "
			     "content=\"text/html; charset=UTF-8\">\n"
			   "</head>\n"
			   " <body>\n");
		if (hcaption)
		    	fprintf(f, "<p>%s</p>\n", hcaption);
		fprintf(f, "  <table border=0>"
			   "<tr bgcolor=black><td>"
			   "<pre><span style=\"color:%s;"
			                       "background:%s;"
					       "font-weight:%s;"
					       "font-style:%s\">",
			   html_color(current_fg),
			   html_color(current_bg),
			   current_high? "bold": "normal",
			   current_ital? "italic": "normal");
		if (hcaption != NULL)
			Free(hcaption);
	}

	if (ptype == P_TEXT) {
	    	if (xcaption != NULL)
		    	fprintf(f, "%s\n\n", xcaption);
	}

	for (i = 0; i < ROWS*COLS; i++) {
		char mb[16];
		int nmb;

		uc = 0;

		if (i && !(i % COLS)) {
		    	if (ptype == P_HTML)
			    	(void) fputc('\n', f);
			else
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
				fa_bg = COLOR_BLACK;
			if (ea_buf[i].gr & GR_INTENSIFY)
				fa_high = True;
			else
				fa_high = FA_IS_HIGH(fa);
			fa_ital = mi && FA_IS_MODIFIED(fa);
		}
		if (FA_IS_ZERO(fa)) {
#if defined(X3270_DBCS) /*[*/
			if (ctlr_dbcs_state(i) == DBCS_LEFT)
			    	uc = 0x3000;
			else
#endif /*]*/
				uc = ' ';
		} else {
		    	/* Convert EBCDIC to Unicode. */
#if defined(X3270_DBCS) /*[*/
			switch (ctlr_dbcs_state(i)) {
			case DBCS_NONE:
			case DBCS_SB:
			    	uc = ebcdic_to_unicode(ea_buf[i].cc,
					ea_buf[i].cs, False);
				if (uc == 0)
				    	uc = ' ';
				break;
			case DBCS_LEFT:
				uc = ebcdic_to_unicode(
					(ea_buf[i].cc << 8) |
					 ea_buf[i + 1].cc,
					CS_BASE, False);
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
			uc = ebcdic_to_unicode(ea_buf[i].cc, ea_buf[i].cs,
				False);
			if (uc == 0)
				uc = ' ';
#endif /*]*/
		}

		/* Translate to a type-specific format and write it out. */
		if (uc == ' ' && ptype != P_HTML)
			ns++;
#if defined(X3270_DBCS) /*[*/
		else if (uc == 0x3000) {
		    	if (ptype == P_HTML)
			    	fprintf(f, "  ");
			else
				ns += 2;
		}
#endif /*]*/
		else {
			while (nr) {
			    	if (ptype == P_RTF)
				    	fprintf(f, "\\par");
				(void) fputc('\n', f);
				nr--;
			}
			while (ns) {
			    	if (ptype == P_RTF)
				    	fprintf(f, "\\~");
				else
					(void) fputc(' ', f);
				ns--;
			}
			if (ptype == P_RTF) {
				Bool high;

				if (ea_buf[i].gr & GR_INTENSIFY)
					high = True;
				else
					high = fa_high;
				if (high != current_high) {
					if (high)
						fprintf(f, "\\b ");
					else
						fprintf(f, "\\b0 ");
					current_high = high;
				}
			}
			if (ptype == P_HTML) {
				int fg_color, bg_color;
				Bool high;

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
				    	fg_color = (bg_color == COLOR_RED)?
							COLOR_BLACK: bg_color;
					bg_color = COLOR_RED;
				}
				if (ea_buf[i].gr & GR_INTENSIFY)
					high = True;
				else
					high = fa_high;

				if (fg_color != current_fg ||
				    bg_color != current_bg ||
				    high != current_high ||
				    fa_ital != current_ital) {
					fprintf(f,
						"</span><span "
						"style=\"color:%s;"
						"background:%s;"
						"font-weight:%s;"
						"font-style:%s\">",
						html_color(fg_color),
						html_color(bg_color),
						high? "bold": "normal",
						fa_ital? "italic": "normal");
					current_fg = fg_color;
					current_bg = bg_color;
					current_high = high;
					current_ital = fa_ital;
				}
			}
			any = True;
			if (ptype == P_RTF) {
				if (uc & ~0x7f) {
					fprintf(f, "\\u%ld?", uc);
				} else {
					nmb = unicode_to_multibyte(uc,
						mb, sizeof(mb));
					if (mb[0] == '\\' ||
						mb[0] == '{' ||
						mb[0] == '}')
						fprintf(f, "\\%c",
							mb[0]);
					else if (mb[0] == '-')
						fprintf(f, "\\_");
					else if (mb[0] == ' ')
						fprintf(f, "\\~");
					else
						fputc(mb[0], f);
				}
			} else if (ptype == P_HTML) {
				if (uc == '<')
					fprintf(f, "&lt;");
				else if (uc == '&')
				    	fprintf(f, "&amp;");
				else if (uc == '>')
				    	fprintf(f, "&gt;");
				else {
					nmb = unicode_to_utf8(uc, mb);
					{
					    int k;

					    for (k = 0; k < nmb; k++) {
						fputc(mb[k], f);
					    }
					}
				}
			} else {
				nmb = unicode_to_multibyte(uc,
					mb, sizeof(mb));
				(void) fputs(mb, f);
			}
		}
	}

	if (xcaption != NULL)
	    	Free(xcaption);

	if (ptype == P_HTML)
	    	(void) fputc('\n', f);
	else
		nr++;
	if (!any && !(opts & FPS_EVEN_IF_EMPTY) && ptype == P_TEXT)
		return False;
	while (nr) {
	    	if (ptype == P_RTF)
		    	fprintf(f, "\\par");
		if (ptype == P_TEXT)
			(void) fputc('\n', f);
		nr--;
	}
	if (ptype == P_RTF) {
	    	fprintf(f, "\n}\n%c", 0);
	}
	if (ptype == P_HTML) {
		fprintf(f, "%s</span></pre></td></tr>\n"
		           "  </table>\n"
			   " </body>\n"
			   "</html>\n",
			   current_high? "</b>": "");
	}
	return True;
}

#if !defined(_WIN32) /*[*/
/* Termination code for print text process. */
static void
print_text_done(FILE *f, Boolean do_popdown
#if defined(X3270_DISPLAY) /*[*/
					    _is_unused
#endif /*]*/
						  )
{
	int status;

	status = pclose(f);
	if (status) {
		popup_an_error("Print program exited with status %d.",
		    (status & 0xff00) > 8);
	} else {
#if defined(X3270_DISPLAY) /*[*/
		if (do_popdown)
			XtPopdown(print_text_shell);
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
		if (appres.do_confirms)
			popup_an_info("Screen image printed.");
#endif /*]*/
	}

}
#endif /*]*/

#if defined(X3270_DISPLAY) /*[*/
/* Callback for "OK" button on the print text popup. */
static void
print_text_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *filter;
	FILE *f;

	filter = XawDialogGetValueString((Widget)client_data);
	if (!filter) {
		XtPopdown(print_text_shell);
		return;
	}
	if (!(f = popen(filter, "w"))) {
		popup_an_errno(errno, "popen(%s)", filter);
		return;
	}
	if (print_text_command == NULL ||
		strcmp(print_text_command, filter)) {
	    Replace(print_text_command, filter);
	    ptc_changed = True;
	}
	(void) fprint_screen(f, P_TEXT, FPS_EVEN_IF_EMPTY, NULL);
	print_text_done(f, True);
}

/* Callback for "Plain Text" button on save text popup. */
static void
save_text_plain_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *filename;
	FILE *f;

	filename = XawDialogGetValueString((Widget)client_data);
	if (!filename) {
		XtPopdown(save_text_shell);
		return;
	}
	if (!(f = fopen(filename, "a"))) {
		popup_an_errno(errno, "%s", filename);
		return;
	}
	(void) fprint_screen(f, P_TEXT, FPS_EVEN_IF_EMPTY, NULL);
	fclose(f);
	XtPopdown(save_text_shell);
	if (appres.do_confirms)
		popup_an_info("Screen image saved.");
}

/* Callback for "HTML" button on save text popup. */
static void
save_text_html_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	char *filename;
	FILE *f;

	filename = XawDialogGetValueString((Widget)client_data);
	if (!filename) {
		XtPopdown(save_text_shell);
		return;
	}
	if (!(f = fopen(filename, "a"))) {
		popup_an_errno(errno, "%s", filename);
		return;
	}
	(void) fprint_screen(f, P_HTML, FPS_EVEN_IF_EMPTY, NULL);
	fclose(f);
	XtPopdown(save_text_shell);
	if (appres.do_confirms)
		popup_an_info("Screen image saved.");
}

/* Pop up the Print Text dialog, given a filter. */
static void
popup_print_text(char *filter)
{
	if (print_text_shell == NULL) {
		print_text_shell = create_form_popup("PrintText",
		    print_text_callback, (XtCallbackProc)NULL,
		    FORM_AS_IS);
		XtVaSetValues(XtNameToWidget(print_text_shell, ObjDialog),
		    XtNvalue, filter,
		    NULL);
	}
	popup_popup(print_text_shell, XtGrabExclusive);
}

/* Pop up the Save Text dialog. */
static void
popup_save_text(char *filename)
{
	if (save_text_shell == NULL) {
		save_text_shell = create_form_popup("SaveText",
		    save_text_plain_callback,
		    save_text_html_callback,
		    FORM_AS_IS);
	}
	if (filename != CN)
		XtVaSetValues(XtNameToWidget(save_text_shell, ObjDialog),
		    XtNvalue, filename,
		    NULL);
	popup_popup(save_text_shell, XtGrabExclusive);
}
#endif /*]*/

#if defined(_WIN32) /*[*/
/*
 * A Windows version of something like mkstemp().  Creates a temporary
 * file in $TEMP, returning its path and an open file descriptor.
 */
int
win_mkstemp(char **path, ptype_t ptype)
{
	char *s;
	int fd;

	s = getenv("TEMP");
	if (s == NULL)
		s = getenv("TMP");
	if (s == NULL)
		s = "C:";
	*path = xs_buffer("%s\\x3h%u.%s", s, getpid(),
			    (ptype == P_RTF)? "rtf": "txt");
	fd = open(*path, O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
	if (fd < 0) {
	    Free(*path);
	    *path = NULL;
	}
	return fd;
}

/*
 * Find WORDPAD.EXE.
 */
#define PROGRAMFILES "%ProgramFiles%"
char *
find_wordpad(void)
{
	char data[1024];
	DWORD dlen;
	char *slash;
	static char *wp = NULL;
	HKEY key;

	if (wp != NULL)
	    return wp;

	/* Get the shell print command for RTF files. */
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		    "Software\\Classes\\rtffile\\shell\\print\\command",
		    0,
		    KEY_READ,
		    &key) != ERROR_SUCCESS) {
	    	return NULL;
	}
	dlen = sizeof(data);
    	if (RegQueryValueEx(key,
		    NULL,
		    NULL,
		    NULL,
		    (LPVOID)data,
		    &dlen) != ERROR_SUCCESS) {
		RegCloseKey(key);
	    	return NULL;
	}
	RegCloseKey(key);

	if (data[0] == '"') {
	    char data2[1024];
	    char *q2;

	    /* The first token is quoted; that's the path. */
	    strcpy(data2, data + 1);
	    q2 = strchr(data2, '"');
	    if (q2 == NULL) {
		return NULL;
	    }
	    *q2 = '\0';
	    strcpy(data, data2);
	} else if ((slash = strchr(data, '/')) != NULL) {
	    /* Find the "/p". */
	    *slash = '\0';
	    if (*(slash - 1) == ' ')
		*(slash - 1) = '\0';
	}

	if (!strncasecmp(data, PROGRAMFILES, strlen(PROGRAMFILES))) {
	    char *pf = getenv("PROGRAMFILES");

	    /* Substitute %ProgramFiles%. */
	    if (pf == NULL) {
		return NULL;
	    }
	    wp = xs_buffer("%s\\%s", pf, data + strlen(PROGRAMFILES));
	} else {
	    wp = NewString(data);
	}
	if (GetShortPathName(wp, data, sizeof(data)) != 0) {
	    Free(wp);
	    wp = NewString(data);
	}
	return wp;
}
#endif /*]*/

/* Print or save the contents of the screen as text. */
void
PrintText_action(Widget w _is_unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	Cardinal i;
	char *filter = CN;
	Boolean secure = appres.secure;
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
		} else if (!strcasecmp(params[i], "secure")) {
			secure = True;
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
			filter = get_resource(ResPrintTextCommand);
#else /*][*/
			filter = get_resource(ResPrinterName); /* XXX */
#endif /*]*/
		}
		break;
	case 1:
		if (use_string) {
			popup_an_error("%s: extra arguments or invalid option(s)",
			    action_name(PrintText_action));
			return;
		}
		filter = params[i];
		break;
	default:
		popup_an_error("%s: extra arguments or invalid option(s)",
		    action_name(PrintText_action));
		return;
	}

#if defined(_WIN32) /*[*/
	/* On Windows, use rich text. */
	if (!use_string && !use_file && ptype != P_HTML)
	    ptype = P_RTF;
#endif /*]*/

	if (filter != CN && filter[0] == '@') {
		/*
		 * Starting the PrintTextCommand resource value with '@'
		 * suppresses the pop-up dialog, as does setting the 'secure'
		 * resource.
		 */
		secure = True;
		filter++;
	}
	if (!use_file && (filter == CN || !*filter))
#if !defined(_WIN32) /*[*/
		filter = "lpr";
#else /*][*/
		filter = CN;
#endif /*]*/

#if defined(X3270_DISPLAY) /*[*/
	if (secure ||
		ia_cause == IA_COMMAND ||
		ia_cause == IA_MACRO ||
		ia_cause == IA_SCRIPT)
#endif /*]*/
	{
		FILE *f;
		int fd = -1;

		/* Invoked non-interactively. */
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
				if (filter == CN || !*filter) {
					popup_an_error("%s: missing filename",
						action_name(PrintText_action));
					return;
				}
				f = fopen(filter, "a");
			}
		} else {
#if !defined(_WIN32) /*[*/
			f = popen(filter, "w");
#else /*][*/
			fd = win_mkstemp(&temp_name, ptype);
			if (fd < 0) {
				popup_an_errno(errno, "mkstemp");
				return;
			}
			f = fdopen(fd, "w+");
#endif /*]*/
		}
		if (f == NULL) {
			popup_an_errno(errno, "%s: %s",
					action_name(PrintText_action),
					filter);
			if (fd >= 0) {
				(void) close(fd);
			}
			if (temp_name) {
				unlink(temp_name);
				Free(temp_name);
			}
			return;
		}
		(void) fprint_screen(f, ptype, opts, caption);
		if (use_string) {
			char buf[8192];

			rewind(f);
			while (fgets(buf, sizeof(buf), f) != NULL)
				action_output("%s", buf);
		}
		if (use_file)
			fclose(f);
		else {
#if !defined(_WIN32) /*[*/
			print_text_done(f, False);
#else /*][*/
			char *wp;

			fclose(f);
			wp = find_wordpad();
			if (wp == NULL) {
				popup_an_error("%s: Can't find WORDPAD.EXE",
					action_name(PrintText_action));
			} else {
				char *cmd;

				if (filter != CN)
				    cmd = xs_buffer("start /wait %s /pt \"%s\" \"%s\"", wp,
					    temp_name, filter);
				else
				    cmd = xs_buffer("start /wait %s /p \"%s\"", wp,
					    temp_name);
				system(cmd);
				Free(cmd);
			}
#if !defined(S3270) /*[*/
			if (appres.do_confirms)
				popup_an_info("Screen image printed.\n");
#endif /*]*/
#endif /*]*/
		}
		if (temp_name) {
		    	unlink(temp_name);
			Free(temp_name);
		}
		return;
	}

#if defined(X3270_DISPLAY) /*[*/
	/* Invoked interactively -- pop up the confirmation dialog. */
	if (use_file) {
		popup_save_text(filter);
	} else {
		popup_print_text(filter);
	}
#endif /*]*/
}

#if defined(X3270_DISPLAY) /*[*/
#if defined(X3270_MENUS) /*[*/


/* Callback for Print Text menu option. */
void
print_text_option(Widget w, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	char *filter = get_resource(ResPrintTextCommand);
	Boolean secure = appres.secure;
	ptype_t ptype = P_TEXT;

	if (print_text_command != NULL)
	    	filter = print_text_command;
	else {
	    	filter = get_resource(ResPrintTextCommand);
		if (filter == NULL || !*filter)
		    	filter = "lpr";
		print_text_command = XtNewString(filter);
	}

	/* Decode the filter. */
	if (filter != CN && *filter == '@') {
		secure = True;
		filter++;
	}
	if (filter == CN || !*filter)
		filter = "lpr";

	if (secure) {
		FILE *f;

		/* Print the screen without confirming. */
		if (!(f = popen(filter, "w"))) {
			popup_an_errno(errno, "popen(%s)", filter);
			return;
		}
		(void) fprint_screen(f, ptype, FPS_EVEN_IF_EMPTY, NULL);
		print_text_done(f, False);
	} else {
		/* Pop up a dialog to confirm or modify their choice. */
		popup_print_text(filter);
	}
}

/* Callback for Save Text menu option. */
void
save_text_option(Widget w, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	/* Pop up a dialog to confirm or modify their choice. */
	popup_save_text(CN);
}
#endif /*]*/


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

#if defined(X3270_MENUS) /*[*/
/* Callback for menu Print Window option. */
void
print_window_option(Widget w, XtPointer client_data _is_unused,
    XtPointer call_data _is_unused)
{
	Cardinal zero = 0;

	PrintWindow_action(w, (XEvent *)NULL, (String *)NULL, &zero);
}
#endif /*]*/

#endif /*]*/

