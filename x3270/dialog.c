/*
 * Copyright (c) 1996-2012, 2014-2016 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
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
 *     * Neither the names of Paul Mattes, Dick Altenbern nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	dialog.c
 *		Common code for nontrival dialog boxes.
 */

#include "globals.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Shell.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/TextSrc.h>
#include <X11/Xaw/TextSink.h>
#include <X11/Xaw/AsciiSrc.h>
#include <X11/Xaw/AsciiSink.h>
#include <errno.h>

#include "appres.h"
#include "dialog.h"
#include "ft_cut.h"
#include "ft_dft.h"
#include "ft.h"
#include "host.h"
#include "kybd.h"
#include "macros.h"
#include "objects.h"
#include "popups.h"
#include "telnet.h"
#include "utils.h"
#include "xmenubar.h"

/* Globals. */
text_t t_numeric = T_NUMERIC;
text_t t_hostfile = T_HOSTFILE;
text_t t_unixfile = T_UNIXFILE;
text_t t_command = T_COMMAND;

bool s_true = true;
bool s_false = false;

/* Statics. */
static sr_t **srp = NULL;
static sr_t *sr_last = NULL;
static Widget focus_widget = NULL;
static void focus_next(sr_t *s);

/* Support functions for dialogs. */

/* Set one dialog (hack). */
void
dialog_set(sr_t **srs, Widget f)
{
	sr_t *s;

	srp = srs;
	for (s = *srp; s != NULL; s = s->next) {
		sr_last = s;
	}

	focus_widget = f;
}

/* Match one dimension of two widgets. */
void
dialog_match_dimension(Widget w1, Widget w2, const char *n)
{
	Dimension h1, h2;
	Dimension b1, b2;

	XtVaGetValues(w1, n, &h1, XtNborderWidth, &b1, NULL);
	XtVaGetValues(w2, n, &h2, XtNborderWidth, &b2, NULL);
	h1 += 2 * b1;
	h2 += 2 * b2;
	if (h1 > h2)
		XtVaSetValues(w2, n, h1 - (2 * b2), NULL);
	else if (h2 > h1)
		XtVaSetValues(w1, n, h2 - (2 * b1), NULL);
}

/* Apply a bitmap to a widget. */
void
dialog_apply_bitmap(Widget w, Pixmap p)
{
	Dimension d1;

	XtVaGetValues(w, XtNheight, &d1, NULL);
	if (d1 < 10)
		XtVaSetValues(w, XtNheight, 10, NULL);
	XtVaSetValues(w, XtNleftBitmap, p, NULL);
}

/* Flip a multi-valued toggle. */
void
dialog_flip_toggles(struct toggle_list *toggle_list, Widget w)
{
	int i;

	/* Flip the widget w to on, and the rest to off. */
	for (i = 0; toggle_list->widgets[i] != NULL; i++) {
		/* Process each widget in the list */
		dialog_mark_toggle(*(toggle_list->widgets+i),
		    (*(toggle_list->widgets+i) == w) ? diamond : no_diamond);
	}
}

/*
 * Callback for text source changes.  Edits the text to ensure it meets the
 * specified criteria.
 */
void
dialog_text_callback(Widget w, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	XawTextBlock b;		/* firstPos, length, ptr, format */
	static XawTextBlock nullb = { 0, 0, NULL };
	XawTextPosition pos = 0;
	int i;
	text_t t = *(text_t *)client_data;
	static bool called_back = false;

	if (called_back)
		return;
	else
		called_back = true;

	while (1) {
		bool replaced = false;

		XawTextSourceRead(w, pos, &b, 1024);
		if (b.length <= 0)
			break;
		nullb.format = b.format;
		for (i = 0; i < b.length; i++) {
			bool bad = false;
			char c = *(b.ptr + i);

			switch (t) {
			    case T_NUMERIC:
				/* Only numbers. */
				bad = !isdigit((unsigned char)c);
				break;
			    case T_HOSTFILE:
				/*
				 * Only printing characters and spaces; no
				 * leading or trailing blanks.
				 */
				bad = !isprint((unsigned char)c) || (!pos && !i && c == ' ');
				break;
			    case T_UNIXFILE:
				/* Only printing characters. */
				bad = !isprint((unsigned char)c);
				break;
			    case T_COMMAND:
				/* Only printing characters. */
				bad = !isprint((unsigned char)c);
				break;
			    default:
				/* Only printing characters, no spaces. */
				bad = !isgraph((unsigned char)c);
				break;
			}
			if (bad) {
				XawTextSourceReplace(w, pos + i, pos + i + 1,
				    &nullb);
				pos = 0;
				replaced = true;
				break;
			}
		}
		if (replaced)
			continue; /* rescan the same block */
		pos += b.length;
		if (b.length < 1024)
			break;
	}
	called_back = false;
}

/* Register widget sensitivity, based on zero to three bools. */
void
dialog_register_sensitivity(Widget w, bool *bvar1, bool bval1,
    bool *bvar2, bool bval2, bool *bvar3, bool bval3)
{
	sr_t *s;
	bool f;

	/* Allocate a structure. */
	s = (sr_t *)XtMalloc(sizeof(sr_t));
	s->w = w;
	s->bvar1 = bvar1;
	s->bval1 = bval1;
	s->bvar2 = bvar2;
	s->bval2 = bval2;
	s->bvar3 = bvar3;
	s->bval3 = bval3;
	s->is_value = !strcmp(XtName(w), "value");
	s->has_focus = false;

	/* Link it onto the chain. */
	s->next = NULL;
	if (sr_last != NULL)
		sr_last->next = s;
	else
		*srp = s;
	sr_last = s;

	/* Set up the initial widget sensitivity. */
	if (bvar1 == NULL)
		f = true;
	else {
		f = (*bvar1 == bval1);
		if (bvar2 != NULL)
			f &= (*bvar2 == bval2);
		if (bvar3 != NULL)
			f &= (*bvar3 == bval3);
	}
	XtVaSetValues(w, XtNsensitive, f, NULL);
}

/* Scan the list of registered widgets for a sensitivity change. */
void
dialog_check_sensitivity(bool *bvar)
{
	sr_t *s;

	for (s = *srp; s != NULL; s = s->next) {
		if (s->bvar1 == bvar || s->bvar2 == bvar || s->bvar3 == bvar) {
			bool f;

			f = (s->bvar1 != NULL && (*s->bvar1 == s->bval1));
			if (s->bvar2 != NULL)
				f &= (*s->bvar2 == s->bval2);
			if (s->bvar3 != NULL)
				f &= (*s->bvar3 == s->bval3);
			XtVaSetValues(s->w, XtNsensitive, f, NULL);

			/* If it is now insensitive, move the focus. */
			if (!f && s->is_value && s->has_focus)
				focus_next(s);
		}
	}
}

/* Move the input focus to the next sensitive value field. */
static void
focus_next(sr_t *s)
{
	sr_t *t;
	bool sen;

	/* Defocus this widget. */
	s->has_focus = false;
	XawTextDisplayCaret(s->w, False);

	/* Search after. */
	for (t = s->next; t != NULL; t = t->next) {
		if (t->is_value) {
			XtVaGetValues(t->w, XtNsensitive, &sen, NULL);
			if (sen)
				break;
		}
	}

	/* Wrap and search before. */
	if (t == NULL)
		for (t = *srp; t != s && t != NULL; t = t->next) {
			if (t->is_value) {
				XtVaGetValues(t->w, XtNsensitive, &sen, NULL);
				if (sen)
					break;
			}
		}

	/* Move the focus. */
	if (t != NULL && t != s) {
		t->has_focus = true;
		XawTextDisplayCaret(t->w, true);
		if (focus_widget)
			XtSetKeyboardFocus(focus_widget, t->w);
	}
}

/* Mark a toggle. */
void
dialog_mark_toggle(Widget w, Pixmap p)
{
        XtVaSetValues(w, XtNleftBitmap, p, NULL);
}

/* Dialog action procedures. */

/* Proceed to the next input field. */
void
PA_dialog_next_xaction(Widget w, XEvent *event _is_unused,
	String *parms _is_unused, Cardinal *num_parms _is_unused)
{
	sr_t *s;

	for (s = *srp; s != NULL; s = s->next) {
		if (s->w == w) {
			focus_next(s);
			return;
		}
	}
}

/* Set keyboard focus to an input field. */
void
PA_dialog_focus_xaction(Widget w, XEvent *event _is_unused,
	String *parms _is_unused, Cardinal *num_parms _is_unused)
{
	sr_t *s;

	/* Remove the focus from the widget that has it now. */
	for (s = *srp; s != NULL; s = s->next) {
		if (s->has_focus) {
			if (s->w == w)
				return;
			s->has_focus = false;
			XawTextDisplayCaret(s->w, False);
			break;
		}
	}

	/* Find this object. */
	for (s = *srp; s != NULL; s = s->next) {
		if (s->w == w)
			break;
	}
	if (s == NULL)
		return;

	/* Give it the focus. */
	s->has_focus = true;
	XawTextDisplayCaret(w, True);
	if (focus_widget)
		XtSetKeyboardFocus(focus_widget, w);
}
