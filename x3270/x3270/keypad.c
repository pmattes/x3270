/*
 * Copyright (c) 1993-2009, Paul Mattes.
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
 *	keypad.c
 *		This module handles the keypad with buttons for each of the
 *		3270 function keys.
 */

#include "globals.h"

#if defined(X3270_KEYPAD) /*[*/

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include "appres.h"
#include "resources.h"
#include "screen.h"

#include "actionsc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "screenc.h"
#include "utilc.h"

#include "keypad.bm"

enum kp_placement kp_placement;

#define NUM_ROWS	4
#define NUM_VROWS	15
#define BORDER		1
#define TOP_MARGIN	6
#define BOTTOM_MARGIN	6

#define SPACING		2
#define FAT_SPACING	3
#define VGAP		4
#define HGAP		4
#define SIDE_MARGIN	4

#define HORIZ_WIDTH \
	(SIDE_MARGIN + \
	 12*(pf_width+2*BORDER) + \
	 11*SPACING + \
	 HGAP + \
	 3*(pa_width+2*BORDER) + \
	 2*SPACING + \
	 SIDE_MARGIN)

#define VERT_WIDTH \
	(SIDE_MARGIN + \
	 3*(pa_width+2*BORDER) + \
	 2*SPACING + \
	 SIDE_MARGIN)

/*
 * Table of 3278 key labels and actions
 */
struct button_list {
	const char *label;
	const char *name;
	const char *bits;
	int width;
	int height;
	XtActionProc action;
	const char *parm;
};

Boolean keypad_changed = False;

static char Lg[] = "large";
static char Bm[] = "bm";
static char Sm[] = "small";

static struct button_list pf_list[] = {
    { "PF13",           Lg, CN, 0, 0, PF_action,       "13" },
    { "PF14",           Lg, CN, 0, 0, PF_action,       "14" },
    { "PF15",           Lg, CN, 0, 0, PF_action,       "15" },
    { "PF16",           Lg, CN, 0, 0, PF_action,       "16" },
    { "PF17",           Lg, CN, 0, 0, PF_action,       "17" },
    { "PF18",           Lg, CN, 0, 0, PF_action,       "18" },
    { "PF19",           Lg, CN, 0, 0, PF_action,       "19" },
    { "PF20",           Lg, CN, 0, 0, PF_action,       "20" },
    { "PF21",           Lg, CN, 0, 0, PF_action,       "21" },
    { "PF22",           Lg, CN, 0, 0, PF_action,       "22" },
    { "PF23",           Lg, CN, 0, 0, PF_action,       "23" },
    { "PF24",           Lg, CN, 0, 0, PF_action,       "24" },
    { "PF1",            Lg, CN, 0, 0, PF_action,       "1" },
    { "PF2",            Lg, CN, 0, 0, PF_action,       "2" },
    { "PF3",            Lg, CN, 0, 0, PF_action,       "3" },
    { "PF4",            Lg, CN, 0, 0, PF_action,       "4" },
    { "PF5",            Lg, CN, 0, 0, PF_action,       "5" },
    { "PF6",            Lg, CN, 0, 0, PF_action,       "6" },
    { "PF7",            Lg, CN, 0, 0, PF_action,       "7" },
    { "PF8",            Lg, CN, 0, 0, PF_action,       "8" },
    { "PF9",            Lg, CN, 0, 0, PF_action,       "9" },
    { "PF10",           Lg, CN, 0, 0, PF_action,       "10" },
    { "PF11",           Lg, CN, 0, 0, PF_action,       "11" },
    { "PF12",           Lg, CN, 0, 0, PF_action,       "12" }
};
#define PF_SZ (sizeof(pf_list)/sizeof(pf_list[0]))

static struct button_list pad_list[] = {
    { "PA1",            Lg, CN, 0, 0, PA_action,       "1" },
    { "PA2",            Lg, CN, 0, 0, PA_action,       "2" },
    { "PA3",            Lg, CN, 0, 0, PA_action,       "3" },
    { 0, 0, 0, 0 },
    { "Up" ,            Bm, (char *)up_bits, up_width, up_height,
                                      Up_action,       CN },
    { 0, 0, 0, 0 },
    { "Left",           Bm, (char *)left_bits, left_width, left_height,
                                      Left_action,     CN },
    { "Home",           Bm, (char *)home_bits, home_width, home_height,
                                      Home_action,     CN },
    { "Right",          Bm, (char *)right_bits, right_width, right_height,
                                      Right_action,    CN },
    { 0, 0, 0, 0 },
    { "Down",           Bm, (char *)down_bits, down_width, down_height,
                                      Down_action,     CN },
    { 0, 0, 0, 0 },
};
#define PAD_SZ (sizeof(pad_list)/sizeof(pad_list[0]))

static struct button_list lower_list[] = {
    { "Clear",          Sm, CN, 0, 0, Clear_action,    CN },
    { "Reset",          Sm, CN, 0, 0, Reset_action,    CN },
    { "Ins",            Bm, (char *)ins_bits, ins_width, ins_height,
                                      Insert_action,   CN },
    { "Del",            Bm, (char *)del_bits, del_width, del_height,
                                      Delete_action,   CN },
    { "Erase\nEOF",     Sm, CN, 0, 0, EraseEOF_action, CN },
    { "Erase\nInput",   Sm, CN, 0, 0, EraseInput_action,CN },
    { "Dup",            Sm, CN, 0, 0, Dup_action,      CN },
    { "Field\nMark",    Sm, CN, 0, 0, FieldMark_action,CN },
    { "Sys\nReq",       Sm, CN, 0, 0, SysReq_action,   CN },
    { "Cursor\nSelect", Sm, CN, 0, 0, CursorSelect_action,CN },
    { "Attn",           Sm, CN, 0, 0, Attn_action,     CN },
    { "Compose",        Sm, CN, 0, 0, Compose_action,  CN },
    { "Btab",           Bm, (char *)btab_bits, btab_width, btab_height,
                                      BackTab_action,  CN },
    { "Tab",            Bm, (char *)tab_bits, tab_width, tab_height,
                                      Tab_action,      CN },
    { "Newline",	Bm, (char *)newline_bits, newline_width, newline_height,
				      Newline_action,  CN },
    { "Enter",          Sm, CN, 0, 0, Enter_action,    CN }
};
#define LOWER_SZ (sizeof(lower_list)/sizeof(lower_list[0]))

static struct button_list vpf_list[] = {
    { "PF1",            Lg, CN, 0, 0, PF_action,       "1" },
    { "PF2",            Lg, CN, 0, 0, PF_action,       "2" },
    { "PF3",            Lg, CN, 0, 0, PF_action,       "3" },
    { "PF4",            Lg, CN, 0, 0, PF_action,       "4" },
    { "PF5",            Lg, CN, 0, 0, PF_action,       "5" },
    { "PF6",            Lg, CN, 0, 0, PF_action,       "6" },
    { "PF7",            Lg, CN, 0, 0, PF_action,       "7" },
    { "PF8",            Lg, CN, 0, 0, PF_action,       "8" },
    { "PF9",            Lg, CN, 0, 0, PF_action,       "9" },
    { "PF10",           Lg, CN, 0, 0, PF_action,       "10" },
    { "PF11",           Lg, CN, 0, 0, PF_action,       "11" },
    { "PF12",           Lg, CN, 0, 0, PF_action,       "12" },
};
#define VPF_SZ (sizeof(vpf_list)/sizeof(vpf_list[0]))

static struct button_list vspf_list[] = {
    { "PF13",           Lg, CN, 0, 0, PF_action,       "13" },
    { "PF14",           Lg, CN, 0, 0, PF_action,       "14" },
    { "PF15",           Lg, CN, 0, 0, PF_action,       "15" },
    { "PF16",           Lg, CN, 0, 0, PF_action,       "16" },
    { "PF17",           Lg, CN, 0, 0, PF_action,       "17" },
    { "PF18",           Lg, CN, 0, 0, PF_action,       "18" },
    { "PF19",           Lg, CN, 0, 0, PF_action,       "19" },
    { "PF20",           Lg, CN, 0, 0, PF_action,       "20" },
    { "PF21",           Lg, CN, 0, 0, PF_action,       "21" },
    { "PF22",           Lg, CN, 0, 0, PF_action,       "22" },
    { "PF23",           Lg, CN, 0, 0, PF_action,       "23" },
    { "PF24",           Lg, CN, 0, 0, PF_action,       "24" },
};
static Widget vpf_w[2][VPF_SZ];

static struct button_list vpad_list[] = {
    { 0, 0, 0 },
    { "Up" ,            Bm, (char *)up_bits, up_width, up_height,
                                      Up_action,       CN },
    { 0, 0, 0 },
    { "Left" ,          Bm, (char *)left_bits, left_width, left_height,
                                      Left_action,     CN },
    { "Home",           Bm, (char *)home_bits, home_width, home_height,
                                      Home_action,     CN },
    { "Right" ,         Bm, (char *)right_bits, right_width, right_height,
                                      Right_action,    CN },
    { "Ins",            Bm, (char *)ins_bits, ins_width, ins_height,
                                      Insert_action,   CN },
    { "Down" ,          Bm, (char *)down_bits, down_width, down_height,
                                      Down_action,     CN },
    { "Del",            Bm, (char *)del_bits, del_width, del_height,
                                      Delete_action,   CN },
    { "PA1",            Lg, CN, 0, 0, PA_action,       "1" },
    { "PA2",            Lg, CN, 0, 0, PA_action,       "2" },
    { "PA3",            Lg, CN, 0, 0, PA_action,       "3" },
};
#define VPAD_SZ (sizeof(vpad_list)/sizeof(vpad_list[0]))

static struct button_list vfn_list[] = {
    { "Btab",           Bm, (char *)btab_bits, btab_width, btab_height,
                                      BackTab_action,  CN },
    { "Tab",            Bm, (char *)tab_bits, tab_width, tab_height,       
                                      Tab_action,      CN },
    { "Clear",          Sm, CN, 0, 0, Clear_action,    CN },
    { "Reset",          Sm, CN, 0, 0, Reset_action,    CN },
    { "Erase\nEOF",     Sm, CN, 0, 0, EraseEOF_action, CN },
    { "Erase\nInput",   Sm, CN, 0, 0, EraseInput_action,CN },
    { "Dup",            Sm, CN, 0, 0, Dup_action,      CN },
    { "Field\nMark",    Sm, CN, 0, 0, FieldMark_action,CN },
    { "Sys\nReq",       Sm, CN, 0, 0, SysReq_action,   CN },
    { "Cursor\nSelect", Sm, CN, 0, 0, CursorSelect_action,CN },
    { "Attn",           Sm, CN, 0, 0, Attn_action,     CN },
    { "Compose",        Sm, CN, 0, 0, Compose_action,  CN },
    { "Newline",	Bm, (char *)newline_bits, newline_width, newline_height,
				      Newline_action,  CN },
    { "Enter",          Sm, CN, 0, 0, Enter_action,    CN },
};
#define VFN_SZ (sizeof(vfn_list)/sizeof(vfn_list[0]))

static Dimension pf_width;
static Dimension key_height;
static Dimension pa_width;
static Dimension key_width;
static Dimension large_key_width;

static Widget keypad_container = (Widget) NULL;
static Widget key_pad = (Widget)NULL;
static XtTranslations keypad_t00 = (XtTranslations) NULL;
static XtTranslations keypad_t0 = (XtTranslations) NULL;
static XtTranslations saved_xt = (XtTranslations) NULL;


/* Initialize the keypad placement from the keypad resource. */
void
keypad_placement_init(void)
{
	if (!strcmp(appres.keypad, KpLeft))
		kp_placement = kp_left;
	else if (!strcmp(appres.keypad, KpRight))
		kp_placement = kp_right;
	else if (!strcmp(appres.keypad, KpBottom))
		kp_placement = kp_bottom;
	else if (!strcmp(appres.keypad, KpIntegral))
		kp_placement = kp_integral;
	else if (!strcmp(appres.keypad, KpInsideRight))
		kp_placement = kp_inside_right;
	else
		xs_error("Unknown value for %s", ResKeypad);

	if (kp_placement == kp_integral && fixed_width) {
		popup_an_error("Cannot have integral keypad and fixed size");
		kp_placement = kp_right;
	}
}

/*
 * Callback for keypad buttons.  Simply calls the function pointed to by the
 * client data.
 */
static void
callfn(Widget w _is_unused, XtPointer client_data, XtPointer call_data _is_unused)
{
	struct button_list *keyd = (struct button_list *) client_data;

	action_internal(keyd->action, IA_KEYPAD, keyd->parm, CN);
}

/*
 * Create a button.
 */
static Widget
make_a_button(Widget container, Position x, Position y, Dimension w,
    Dimension h, struct button_list *keyd)
{
	Widget command;
	Pixmap pixmap;

	if (!keyd->label)
		return (Widget) 0;

	command = XtVaCreateManagedWidget(
	    keyd->name, commandWidgetClass, container,
	    XtNx, x,
	    XtNy, y,
	    XtNwidth, w,
	    XtNheight, h,
	    XtNresize, False,
	    NULL);
	XtAddCallback(command, XtNcallback, callfn, (XtPointer)keyd);
	if (keyd->bits) {
		pixmap = XCreateBitmapFromData(display, root_window,
		    keyd->bits, keyd->width, keyd->height);
		XtVaSetValues(command, XtNbitmap, pixmap, NULL);
	} else
		XtVaSetValues(command, XtNlabel, keyd->label, NULL);
	return command;
}

/*
 * Create the keys for a horizontal keypad
 */
static void
keypad_keys_horiz(Widget container)
{
	unsigned i;
	Position row, col;
	Position x0, y0;

	/* PF Keys */
	row = col = 0;
	x0 = SIDE_MARGIN;
	y0 = TOP_MARGIN;
	for (i = 0; i < PF_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(pf_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pf_width,
		    key_height,
		    &pf_list[i]);
		if (++col >= 12) {
			col = 0;
			row++;
		}
	}

	/* Keypad */
	row = col = 0;
	x0 = SIDE_MARGIN + 12*(pf_width+2*BORDER+SPACING) + HGAP;
	y0 = TOP_MARGIN;
	for (i = 0; i < PAD_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &pad_list[i]);
		if (++col >= 3) {
			col = 0;
			if (++row == 1)
				y0 += VGAP;
		}
	}

	/* Bottom */
	row = col = 0;
	x0 = SIDE_MARGIN;
	y0 = TOP_MARGIN + 2*(key_height+2*BORDER+SPACING) + VGAP;

	for (i = 0; i < LOWER_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(key_width+2*BORDER+FAT_SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    key_width,
		    key_height,
		    &lower_list[i]);
		if (++row >= 2) {
			++col;
			row = 0;
		}
	}
}

static Boolean vert_keypad = False;
static Widget spf_container;

/*
 * Create the keys for a vertical keypad.
 */
static void
keypad_keys_vert(Widget container)
{
	unsigned i;
	Position row, col;
	Position x0, y0;
	Widget c1, c2;

	vert_keypad = True;

	/* Container for shifted PF keys */
	spf_container = XtVaCreateManagedWidget(
	    "shift", compositeWidgetClass, container,
	    XtNmappedWhenManaged, False,
	    XtNborderWidth, 0,
	    XtNwidth, VERT_WIDTH,
	    XtNheight, TOP_MARGIN+4*(key_height+2*BORDER)+3*SPACING,
	    NULL);
	if (appres.mono)
		XtVaSetValues(spf_container, XtNbackgroundPixmap, gray, NULL);
	else
		XtVaSetValues(spf_container, XtNbackground, keypadbg_pixel,
		    NULL);

	/* PF keys */
	if (appres.invert_kpshift) {
		c1 = spf_container;
		c2 = container;
	} else {
		c1 = container;
		c2 = spf_container;
	}
	row = col = 0;
	x0 = SIDE_MARGIN;
	y0 = TOP_MARGIN;
	for (i = 0; i < VPF_SZ; i++) {
		vpf_w[0][i] = make_a_button(c1,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &vpf_list[i]);
		vpf_w[1][i] = make_a_button(c2,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &vspf_list[i]);
		if (++col >= 3) {
			col = 0;
			row++;
		}
	}

	/* Cursor and PA keys */
	for (i = 0; i < VPAD_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &vpad_list[i]);
		if (++col >= 3) {
			col = 0;
			row++;
		}
	}

	/* Other keys */
	for (i = 0; i < VFN_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(large_key_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    large_key_width,
		    key_height,
		    &vfn_list[i]);
		if (++col >= 2) {
			col = 0;
			row++;
		}
	}
}

static Dimension
get_keypad_dimension(const char *name)
{
	char *d;
	long v;

	if ((d = get_fresource("%s.%s", ResKeypad, name)) == CN)
		xs_error("Cannot find %s resource", ResKeypad);
	if ((v = strtol(d, (char **)0, 0)) <= 0)
		xs_error("Illegal %s resource", ResKeypad);
	return (Dimension)v;
}

static void
init_keypad_dimensions(void)
{
	static Boolean done = False;

	if (done)
		return;
	key_height = get_keypad_dimension(ResKeyHeight);
	key_width = get_keypad_dimension(ResKeyWidth);
	pf_width = get_keypad_dimension(ResPfWidth);
	pa_width = get_keypad_dimension(ResPaWidth);
	large_key_width = get_keypad_dimension(ResLargeKeyWidth);
	done = True;
}

Dimension
min_keypad_width(void)
{
	init_keypad_dimensions();
	return HORIZ_WIDTH;
}

Dimension
keypad_qheight(void)
{
	init_keypad_dimensions();
	return TOP_MARGIN +
	    (NUM_ROWS*(key_height+2*BORDER)) + (NUM_ROWS-1)*SPACING + VGAP +
	    BOTTOM_MARGIN;
}

/*
 * Create a keypad.
 */
Widget
keypad_init(Widget container, Dimension voffset, Dimension screen_width, Boolean floating, Boolean vert)
{
	Dimension height;
	Dimension width = screen_width;
	Dimension hoffset;

	init_keypad_dimensions();

	/* Figure out what dimensions to use */
	if (vert)
		width = VERT_WIDTH;
	else
		width = HORIZ_WIDTH;

	if (vert)
		height = TOP_MARGIN +
		   (NUM_VROWS*(key_height+2*BORDER)) + (NUM_VROWS-1)*SPACING +
		   BOTTOM_MARGIN;
	else
		height = keypad_qheight();

	/* Create a container */
	if (screen_width > width)
		hoffset = ((screen_width - width) / (unsigned) 2) & ~1;
	else
		hoffset = 0;
	if (voffset & 1)
		voffset++;
	if (key_pad == (Widget)NULL) {
		key_pad = XtVaCreateManagedWidget(
		    "keyPad", compositeWidgetClass, container,
		    XtNx, hoffset,
		    XtNy, voffset,
		    XtNborderWidth, (Dimension) (floating ? 1 : 0),
		    XtNwidth, width,
		    XtNheight, height,
		    NULL);
		if (appres.mono)
			XtVaSetValues(key_pad, XtNbackgroundPixmap, gray,
			    NULL);
		else
			XtVaSetValues(key_pad, XtNbackground, keypadbg_pixel,
			    NULL);

		/* Create the keys */
		if (vert)
			keypad_keys_vert(key_pad);
		else
			keypad_keys_horiz(key_pad);
	} else {
		XtVaSetValues(key_pad,
		    XtNx, hoffset,
		    XtNy, voffset,
		    NULL);
	}

	return key_pad;
}

/*
 * Swap PF1-12 and PF13-24 on the vertical popup keypad, by mapping or
 * unmapping the window containing the shifted keys.
 */
void
keypad_shift(void)
{
	if (!vert_keypad ||
		(spf_container == NULL) ||
		!XtIsRealized(spf_container))
		return;

	if (shifted)
		XtMapWidget(spf_container);
	else
		XtUnmapWidget(spf_container);
}


/*
 * Keypad popup
 */
Widget keypad_shell = NULL;
Boolean keypad_popped = False;

static Boolean TrueD = True;
static Boolean *TrueP = &TrueD;
static Boolean FalseD = False;
static Boolean *FalseP = &FalseD;
static enum placement *pp;

/*
 * Called when the main screen is first exposed, to pop up the keypad the
 * first time
 */
void
keypad_first_up(void)
{
	if (!appres.keypad_on || kp_placement == kp_integral)
		return;
	keypad_popup_init();
	popup_popup(keypad_shell, XtGrabNone);
}

/* Called when the keypad popup pops up or down */
static void
keypad_updown(Widget w _is_unused, XtPointer client_data, XtPointer call_data)
{
	appres.keypad_on = keypad_popped = *(Boolean *)client_data;
	if (!keypad_popped) {
	    	XtDestroyWidget(keypad_shell);
		keypad_shell = NULL;
		keypad_container = NULL;
		key_pad = NULL;
		spf_container = NULL;
	}

	if (appres.keypad_on)
		place_popup(w, (XtPointer)pp, call_data);
	menubar_keypad_changed();
}

/* Create the pop-up keypad */
void
keypad_popup_init(void)
{
	Widget w;
	Dimension height, width, border;
	Boolean vert = False;

	if (keypad_shell != NULL)
		return;

	switch (kp_placement) {
	    case kp_left:
		vert = True;
		pp = LeftP;
		break;
	    case kp_right:
		vert = True;
		pp = RightP;
		break;
	    case kp_bottom:
		vert = False;
		pp = BottomP;
		break;
	    case kp_integral:	/* can't happen */
		return;
	    case kp_inside_right:
		vert = True;
		pp = InsideRightP;
		break;
	}

	/* Create a popup shell */

	keypad_shell = XtVaCreatePopupShell(
	    "keypadPopup", transientShellWidgetClass, toplevel,
	    NULL);
	/*XtAddCallback(keypad_shell, XtNpopupCallback, place_popup,
	    (XtPointer)pp);*/
	XtAddCallback(keypad_shell, XtNpopupCallback, keypad_updown,
	    (XtPointer) TrueP);
	XtAddCallback(keypad_shell, XtNpopdownCallback, keypad_updown,
	    (XtPointer) FalseP);

	/* Create a keypad in the popup */

	keypad_container = XtVaCreateManagedWidget(
	    "container", compositeWidgetClass, keypad_shell,
	    XtNborderWidth, 0,
	    XtNheight, 10,
	    XtNwidth, 10,
	    NULL);
	w = keypad_init(keypad_container, 0, 0, True, vert);

	/* Fix the window size */

	XtVaGetValues(w,
	    XtNheight, &height,
	    XtNwidth, &width,
	    XtNborderWidth, &border,
	    NULL);
	height += 2*border;
	width += 2*border;
	XtVaSetValues(keypad_container,
	    XtNheight, height,
	    XtNwidth, width,
	    NULL);
	XtVaSetValues(keypad_shell,
	    XtNheight, height,
	    XtNwidth, width,
	    XtNbaseHeight, height,
	    XtNbaseWidth, width,
	    XtNminHeight, height,
	    XtNminWidth, width,
	    XtNmaxHeight, height,
	    XtNmaxWidth, width,
	    NULL);

	/* Make keystrokes in the popup apply to the main window */

	save_00translations(keypad_container, &keypad_t00);
	set_translations(keypad_container, (XtTranslations *)NULL, &keypad_t0);
	if (saved_xt != (XtTranslations) NULL) {
		XtOverrideTranslations(keypad_container, saved_xt);
		saved_xt = (XtTranslations) NULL;
	}
}

/* Set a temporary keymap. */
void
keypad_set_temp_keymap(XtTranslations trans)
{
	if (keypad_container != (Widget) NULL) {
		if (trans == (XtTranslations)NULL) {
			trans = keypad_t0;
			XtUninstallTranslations(keypad_container);
		}
		XtOverrideTranslations(keypad_container, trans);
		saved_xt = (XtTranslations) NULL;
	} else
		saved_xt = trans;
}

/* Change the baseleve keymap. */
void
keypad_set_keymap(void)
{
	if (keypad_container == (Widget)NULL)
		return;
	XtUninstallTranslations(keypad_container);
	set_translations(keypad_container, &keypad_t00, &keypad_t0);
}

/* Move the keypad. */
void
keypad_move(void)
{
	if (!keypad_popped)
		return;

	move_popup(keypad_shell, pp, NULL);
}

void
keypad_popdown(Boolean *was_up)
{
    	if (keypad_popped) {
	    	*was_up = True;
		XtPopdown(keypad_shell);
	} else
	    	*was_up = False;
}

void
keypad_popup(void)
{
#if 0
    	if (keypad_shell != NULL)
	    	XtPopup(keypad_shell, XtGrabNone);
#endif
	appres.keypad_on = True;
	keypad_first_up();
}

#endif /*]*/
