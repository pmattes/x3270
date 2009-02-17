/*
 * Copyright (c) 1993-2009, Paul Mattes.
 * Copyright (c) 2004, Don Russell.
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
 *     * Neither the names of Paul Mattes, Don Russell nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DON RUSSELL "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DON RUSSELL
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	about.c
 *		Pop-up window with the current state of x3270.
 */

#include "globals.h"

#if defined(X3270_MENUS) /*[*/

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include "appres.h"
#include "objects.h"
#include "resources.h"

#include "aboutc.h"
#include "charsetc.h"
#include "keymapc.h"
#include "popupsc.h"
#include "screenc.h"
#include "telnetc.h"
#include "utf8c.h"
#include "utilc.h"

static Widget about_shell = NULL;
static Widget about_form;
extern time_t ns_time;
extern int ns_brcvd;
extern int ns_rrcvd;
extern int ns_bsent;
extern int ns_rsent;
extern int linemode;
extern Pixmap icon;

/* Called when OK is pressed on the about popup */
static void
saw_about(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
	XtPopdown(about_shell);
}

/* Called when the about popup is popped down */
static void
destroy_about(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
	XtDestroyWidget(about_shell);
	about_shell = NULL;
}

/* Return a time difference in English */
static char *
hms(time_t ts)
{
	time_t t, td;
	long hr, mn, sc;
	static char buf[128];

	(void) time(&t);

	td = t - ts;
	hr = td / 3600;
	mn = (td % 3600) / 60;
	sc = td % 60;

	if (hr > 0)
		(void) sprintf(buf, "%ld %s %ld %s %ld %s",
		    hr, (hr == 1) ?
			get_message("hour") : get_message("hours"),
		    mn, (mn == 1) ?
			get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));
	else if (mn > 0)
		(void) sprintf(buf, "%ld %s %ld %s",
		    mn, (mn == 1) ?
			get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));
	else
		(void) sprintf(buf, "%ld %s",
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));

	return buf;
}

#define MAKE_SMALL(label, n) { \
	w_prev = w; \
	w = XtVaCreateManagedWidget( \
	    ObjSmallLabel, labelWidgetClass, about_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w, \
	    XtNleft, XtChainLeft, \
	    XtNvertDistance, (n), \
	    NULL); \
	vd = n; \
	}

#define MAKE_LABEL(label, n) { \
	w_prev = w; \
	w = XtVaCreateManagedWidget( \
	    ObjNameLabel, labelWidgetClass, about_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w, \
	    XtNfromHoriz, left_anchor, \
	    XtNleft, XtChainLeft, \
	    XtNvertDistance, (n), \
	    NULL); \
	vd = n; \
	}

#define MAKE_VALUE(label) { \
	v = XtVaCreateManagedWidget( \
	    ObjDataLabel, labelWidgetClass, about_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w_prev, \
	    XtNfromHoriz, w, \
	    XtNhorizDistance, 0, \
	    XtNvertDistance, vd, \
	    XtNleft, XtChainLeft, \
	    NULL); \
	}

#define MAKE_LABEL2(label) { \
	w = XtVaCreateManagedWidget( \
	    ObjNameLabel, labelWidgetClass, about_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w_prev, \
	    XtNfromHoriz, v, \
	    XtNhorizDistance, 0, \
	    XtNvertDistance, vd, \
	    XtNleft, XtChainLeft, \
	    NULL); \
	}

/* Called when the "About x3270->Copyright" button is pressed */
void
popup_about_copyright(void)
{
	Widget w = NULL, w_prev = NULL;
	Widget left_anchor = NULL;
	int vd = 4;
	static Boolean catted = False;
	static char *s1 = NULL;
	static char *s2 = NULL;
	static char *s1a =
"* Redistributions of source code must retain the above copyright\n\
notice, this list of conditions and the following disclaimer.\n\
* Redistributions in binary form must reproduce the above copyright\n\
notice, this list of conditions and the following disclaimer in the\n";
	static char *s1b =
"documentation and/or other materials provided with the distribution.\n\
* Neither the names of Paul Mattes, Don Russell, Dick Altenbern,\n\
Jeff Sparkes, GTRC nor their contributors may be used to endorse or\n\
promote products derived from this software without specific prior\n\
written permission.";
	static char *s2a =
"THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, DICK ALTENBERN,\n\
JEFF SPARKES AND GTRC \"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES,\n\
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY\n\
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL\n\
MATTES, DON RUSSELL, DICK ALTENBERN, JEFF SPARKES OR GTRC BE LIABLE FOR ANY\n\
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n";
	static char *s2b =
"(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n\
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER\n\
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n\
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n\
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH\n\
DAMAGE.";

	if (!catted) {
	    /* Make up for the ANSI C listerl string length limit. */
	    s1 = Malloc(strlen(s1a) + strlen(s1b) + 1);
	    strcpy(s1, s1a);
	    strcat(s1, s1b);
	    s2 = Malloc(strlen(s2a) + strlen(s2b) + 1);
	    strcpy(s2, s2a);
	    strcat(s2, s2b);
	    catted = True;
	}

	/* Create the popup */

	about_shell = XtVaCreatePopupShell(
	    "aboutCopyrightPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(about_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(about_shell, XtNpopdownCallback, destroy_about,
	    NULL);

	/* Create a form in the popup */

	about_form = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, about_shell,
	    NULL);

	/* Pretty picture */

	left_anchor = XtVaCreateManagedWidget(
	    "icon", labelWidgetClass, about_form,
	    XtNborderWidth, 0,
	    XtNbitmap, icon,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    NULL);

	/* Miscellany */

	MAKE_LABEL(build, 4);

	/* Everything else at the left margin under the bitmap */
	w = left_anchor;
	left_anchor = NULL;

	MAKE_SMALL(
"Copyright \251 1993-2009, Paul Mattes.\n\
Copyright \251 2004-2005, Don Russell.\n\
Copyright \251 1995, Dick Altenbern.\n\
Copyright \251 1990, Jeff Sparkes.\n\
Copyright \251 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA 30332.\n\
All rights reserved.", 4);
	MAKE_SMALL(
"Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions\n\
are met:", 4);
	MAKE_SMALL(s1, 4);
	MAKE_SMALL(s2, 4);

	/* Add "OK" button at the lower left */

	w = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, about_form,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    XtNbottom, XtChainBottom,
	    NULL);
	XtAddCallback(w, XtNcallback, saw_about, 0);

	/* Pop it up */

	popup_popup(about_shell, XtGrabExclusive);
}

/* Called when the "About x3270->Configuration" button is pressed */
void
popup_about_config(void)
{
	Widget w = NULL, w_prev = NULL;
	Widget v = NULL;
	Widget left_anchor = NULL;
	int vd = 4;
	char fbuf[1024];
	const char *ftype;
	char *xbuf;

	/* Create the popup */

	about_shell = XtVaCreatePopupShell(
	    "aboutConfigPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(about_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(about_shell, XtNpopdownCallback, destroy_about,
	    NULL);

	/* Create a form in the popup */

	about_form = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, about_shell,
	    NULL);

	/* Pretty picture */

	left_anchor = XtVaCreateManagedWidget(
	    "icon", labelWidgetClass, about_form,
	    XtNborderWidth, 0,
	    XtNbitmap, icon,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    NULL);

	/* Miscellany */

	MAKE_LABEL(build, 4);
	MAKE_LABEL(get_message("processId"), 4);
	(void) sprintf(fbuf, "%d", getpid());
	MAKE_VALUE(fbuf);
	MAKE_LABEL2(get_message("windowId"));
	(void) sprintf(fbuf, "0x%lx", XtWindow(toplevel));
	MAKE_VALUE(fbuf);

	/* Everything else at the left margin under the bitmap */
	w = left_anchor;
	left_anchor = NULL;

	(void) sprintf(fbuf, "%s %s: %d %s x %d %s, %s, %s",
	    get_message("model"), model_name,
	    maxCOLS, get_message("columns"),
	    maxROWS, get_message("rows"),
	    appres.mono ? get_message("mono") :
		(appres.m3279 ? get_message("fullColor") :
		    get_message("pseudoColor")),
	    (appres.extended && !std_ds_host) ? get_message("extendedDs") :
		get_message("standardDs"));
	MAKE_LABEL(fbuf, 4);

	MAKE_LABEL(get_message("terminalName"), 4);
	MAKE_VALUE(termtype);

	MAKE_LABEL(get_message("emulatorFont"), 4);
	MAKE_VALUE(full_efontname);
	if (*standard_font) {
		ftype = get_message("xFont");
	} else {
		ftype = get_message("cgFont");
	}
	xbuf = xs_buffer("  %s", ftype);
	MAKE_LABEL(xbuf, 0);
	XtFree(xbuf);

#if defined(X3270_DBCS) /*[*/
	if (dbcs) {
		MAKE_LABEL(get_message("emulatorFontDbcs"), 4);
		MAKE_VALUE(full_efontname_dbcs);
	}
#endif /*]*/

	MAKE_LABEL(get_message("displayCharacterSet"), 4);
	if (!efont_matches) {
		xbuf = xs_buffer("ascii-7 (%s %s, %s %s)",
		    get_message("require"), display_charset(),
		    get_message("have"), efont_charset);
		MAKE_VALUE(xbuf);
		XtFree(xbuf);
	} else {
		MAKE_VALUE(efont_charset);
	}
#if defined(X3270_DBCS) /*[*/
	if (dbcs) {
		MAKE_LABEL(get_message("displayCharacterSetDbcs"), 4);
		MAKE_VALUE(efont_charset_dbcs);
	}
#endif /*]*/

	MAKE_LABEL(get_message("charset"), 4);
	xbuf = xs_buffer("%s (code page %s)", get_charset_name(),
	     get_host_codepage());
	MAKE_VALUE(xbuf);
	XtFree(xbuf);

	MAKE_LABEL(get_message("sbcsCgcsgid"), 4);
	xbuf = xs_buffer("GCSGID %u, CPGID %u",
	    (unsigned short)((cgcsgid >> 16) & 0xffff),
	    (unsigned short)(cgcsgid & 0xffff));
	MAKE_VALUE(xbuf);
	XtFree(xbuf);
#if defined(X3270_DBCS) /*[*/
	if (dbcs) {
		MAKE_LABEL(get_message("dbcsCgcsgid"), 4);
		xbuf = xs_buffer("GCSGID %u, CPGID %u",
		    (unsigned short)((cgcsgid_dbcs >> 16) & 0xffff),
		    (unsigned short)(cgcsgid_dbcs & 0xffff));
		MAKE_VALUE(xbuf);
		XtFree(xbuf);
		MAKE_LABEL(get_message("inputMethod"), 4);
		if (appres.input_method) {
			MAKE_VALUE(appres.input_method);
		} else if (getenv("XMODIFIERS") != CN) {
			MAKE_VALUE("(via environment)");
		} else {
			MAKE_VALUE("(unspecified)");
		}
		MAKE_LABEL2(get_message("ximState"));
		if (xim_error)
			ftype = get_message("ximDisabled");
		else if (im == NULL)
			ftype = get_message("ximNotFound");
		else
			ftype = get_message("ximActive");
		MAKE_VALUE(ftype);
		MAKE_LABEL2(get_message("ximLocale"));
		if (locale_name != CN) {
			MAKE_VALUE(locale_name);
		} else {
			MAKE_VALUE("(error)");
		}
	}
#endif /*]*/
	MAKE_LABEL(get_message("localeCodeset"), 4);
	MAKE_VALUE(locale_codeset);

	if (trans_list != (struct trans_list *)NULL ||
	    temp_keymaps != (struct trans_list *)NULL) {
		struct trans_list *t;

		fbuf[0] = '\0';
		for (t = trans_list; t; t = t->next) {
			if (fbuf[0])
				(void) strcat(fbuf, ",");
			(void) strcat(fbuf, t->name);
		}
		for (t = temp_keymaps; t; t = t->next) {
			if (fbuf[0])
				(void) strcat(fbuf, " ");
			(void) strcat(fbuf, "+");
			(void) strcat(fbuf, t->name);
		}
		MAKE_LABEL(get_message("keyboardMap"), 4)
		MAKE_VALUE(fbuf);
	} else
		MAKE_LABEL(get_message("defaultKeyboardMap"), 4);
	if (appres.compose_map) {
		MAKE_LABEL(get_message("composeMap"), 4);
		MAKE_VALUE(appres.compose_map);
	} else {
		MAKE_LABEL(get_message("noComposeMap"), 4);
	}

	if (appres.active_icon) {
		MAKE_LABEL(get_message("activeIcon"), 4);
		xbuf = xs_buffer("  %s", get_message("iconFont"));
		MAKE_LABEL(xbuf, 0);
		XtFree(xbuf);
		MAKE_VALUE(appres.icon_font);
		if (appres.label_icon) {
			xbuf = xs_buffer("  %s", get_message("iconLabelFont"));
			MAKE_LABEL(xbuf, 0);
			XtFree(xbuf);
			MAKE_VALUE(appres.icon_label_font);
		}
	} else
		MAKE_LABEL(get_message("staticIcon"), 4);

	/* Add "OK" button at the lower left */

	w = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, about_form,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    XtNbottom, XtChainBottom,
	    NULL);
	XtAddCallback(w, XtNcallback, saw_about, 0);

	/* Pop it up */

	popup_popup(about_shell, XtGrabExclusive);
}

/* Called when the "About x3270->Connection Status" button is pressed */
void
popup_about_status(void)
{
	Widget w = NULL, w_prev = NULL;
	Widget v = NULL;
	Widget left_anchor = NULL;
	int vd = 4;
	char fbuf[1024];
	const char *ftype;
	const char *emode;
#if defined(X3270_TN3270E) /*[*/
	const char *eopts;
#endif /*]*/
	const char *ptype;
	const char *bplu;

	/* Create the popup */

	about_shell = XtVaCreatePopupShell(
	    "aboutStatusPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(about_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(about_shell, XtNpopdownCallback, destroy_about,
	    NULL);

	/* Create a form in the popup */

	about_form = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, about_shell,
	    NULL);

	/* Pretty picture */

	left_anchor = XtVaCreateManagedWidget(
	    "icon", labelWidgetClass, about_form,
	    XtNborderWidth, 0,
	    XtNbitmap, icon,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    NULL);

	/* Miscellany */

	MAKE_LABEL(build, 4);

	/* Everything else at the left margin under the bitmap */
	w = left_anchor;
	left_anchor = NULL;

	if (CONNECTED) {
		MAKE_LABEL(get_message("connectedTo"), 4);
#if defined(LOCAL_PROCESS) /*[*/
		if (local_process && !strlen(current_host)) {
			MAKE_VALUE("(shell)");
		} else
#endif /*]*/
		{
			if (!appres.suppress_host) {
				MAKE_VALUE(current_host);
			}
		}
#if defined(LOCAL_PROCESS) /*[*/
		if (!local_process) {
#endif /*]*/
			(void) sprintf(fbuf, "  %s", get_message("port"));
			MAKE_LABEL2(fbuf);
			(void) sprintf(fbuf, "%d", current_port);
			MAKE_VALUE(fbuf);
#if defined(LOCAL_PROCESS) /*[*/
		}
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
		if (secure_connection) {
			(void) sprintf(fbuf, "%s", get_message("secure"));
			MAKE_LABEL2(fbuf);
		}
#endif /*]*/
		ptype = net_proxy_type();
		if (ptype) {
		    	MAKE_LABEL(get_message("proxyType"), 4);
			MAKE_VALUE(ptype);
			(void) sprintf(fbuf, "  %s", get_message("server"));
			MAKE_LABEL2(fbuf);
			MAKE_VALUE(net_proxy_host());
			(void) sprintf(fbuf, "  %s", get_message("port"));
			MAKE_LABEL2(fbuf);
			MAKE_VALUE(net_proxy_port());
		}

		if (IN_E)
			emode = "TN3270E ";
		else
			emode = "";
		if (IN_ANSI) {
			if (linemode)
				ftype = get_message("lineMode");
			else
				ftype = get_message("charMode");
			(void) sprintf(fbuf, "  %s%s, ", emode, ftype);
		} else if (IN_SSCP) {
			(void) sprintf(fbuf, "  %s%s, ", emode,
			    get_message("sscpMode"));
		} else if (IN_3270) {
			(void) sprintf(fbuf, "  %s%s, ", emode,
			    get_message("dsMode"));
		} else
			(void) strcpy(fbuf, "  ");
		(void) strcat(fbuf, hms(ns_time));

		MAKE_LABEL(fbuf, 0);

		if (connected_lu != CN && connected_lu[0]) {
		    	sprintf(fbuf, "  %s", get_message("luName"));
			MAKE_LABEL(fbuf, 0);
			MAKE_VALUE(connected_lu);
		}
		bplu = net_query_bind_plu_name();
		if (bplu != CN && bplu[0]) {
		    	sprintf(fbuf, "  %s", get_message("bindPluName"));
			MAKE_LABEL(fbuf, 0);
			MAKE_VALUE(bplu);
		}

#if defined(X3270_TN3270E) /*[*/
		eopts = tn3270e_current_opts();
		if (eopts != CN) {
			(void) sprintf(fbuf, "  %s",
	 			get_message("tn3270eOpts"));
			MAKE_LABEL(fbuf, 0);
			MAKE_VALUE(eopts);
		} else if (IN_E) {
			(void) sprintf(fbuf, "  %s",
				get_message("tn3270eNoOpts"));
			MAKE_LABEL(fbuf, 0);
		}
#endif /*]*/

		if (IN_3270)
			(void) sprintf(fbuf, "%s %d %s, %d %s\n%s %d %s, %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ?
				get_message("byte") : get_message("bytes"),
			    ns_rsent, (ns_rsent == 1) ?
				get_message("record") : get_message("records"),
			    get_message("Received"),
			    ns_brcvd, (ns_brcvd == 1) ?
				get_message("byte") : get_message("bytes"),
			    ns_rrcvd, (ns_rrcvd == 1) ?
				get_message("record") : get_message("records"));
		else
			(void) sprintf(fbuf, "%s %d %s, %s %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ?
				get_message("byte") : get_message("bytes"),
			    get_message("received"),
			    ns_brcvd, (ns_brcvd == 1) ?
				get_message("byte") : get_message("bytes"));
		MAKE_LABEL(fbuf, 4);

#if defined(X3270_ANSI) /*[*/
		if (IN_ANSI) {
			struct ctl_char *c = net_linemode_chars();
			int i;

			MAKE_LABEL(get_message("specialCharacters"), 4);
			for (i = 0; c[i].name; i++) {
				if (!i || !(i % 4)) {
					(void) sprintf(fbuf, "  %s", c[i].name);
					MAKE_LABEL(fbuf, 0);
				} else {
					MAKE_LABEL2(c[i].name);
				}
				MAKE_VALUE(c[i].value);
			}
		}
#endif /*]*/
	} else if (HALF_CONNECTED) {
		MAKE_LABEL(get_message("connectionPending"), 4);
		MAKE_VALUE(current_host);
	} else
		MAKE_LABEL(get_message("notConnected"), 4);

	/* Add "OK" button at the lower left */

	w = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, about_form,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    XtNbottom, XtChainBottom,
	    NULL);
	XtAddCallback(w, XtNcallback, saw_about, 0);

	/* Pop it up */

	popup_popup(about_shell, XtGrabExclusive);
}

#undef MAKE_LABEL
#undef MAKE_VALUE

#endif /*]*/
