/* (from) $XConsortium: SmeBSB.c,v 1.16 91/03/15 15:59:41 gildea Exp $ */

/*
 * Copyright (c) 1995-2009, 2013-2014, 2019 Paul Mattes.
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
 *
 * Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * CmeBSB.c - Source code file for BSB Menu Entry object.
 * (from) SmeBSB.c - Source code file for BSB Menu Entry object.
 *
 * Date:    September 26, 1989
 *
 * By:      Chris D. Peterson
 *          MIT X Consortium 
 *          kit@expo.lcs.mit.edu
 */

#include "globals.h"

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>

#include <X11/Xmu/Drawing.h>

#include <X11/Xaw/XawInit.h>
#include "CmplxMenu.h"
#include "CmeBSBP.h"
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/MenuButton.h>

#include <stdio.h>

#define ONE_HUNDRED 100

#define offset(field) XtOffsetOf(CmeBSBRec, cme_bsb.field)

static XtResource resources[] = {
  {XtNlabel,  XtCLabel, XtRString, sizeof(String),
     offset(label), XtRString, NULL},
  {XtNvertSpace,  XtCVertSpace, XtRInt, sizeof(int),
     offset(vert_space), XtRImmediate, (XtPointer) 25},
  {XtNleftBitmap, XtCLeftBitmap, XtRBitmap, sizeof(Pixmap),
     offset(left_bitmap), XtRImmediate, (XtPointer)None},
  {XtNjustify, XtCJustify, XtRJustify, sizeof(XtJustify),
     offset(justify), XtRImmediate, (XtPointer) XtJustifyLeft},
  {XtNmenuName, XtCMenuName, XtRString, sizeof(String),
     offset(menu_name), XtRString, NULL},
  {XtNrightBitmap, XtCRightBitmap, XtRBitmap, sizeof(Pixmap),
     offset(right_bitmap), XtRImmediate, (XtPointer)None},
  {XtNleftMargin,  XtCHorizontalMargins, XtRDimension, sizeof(Dimension),
     offset(left_margin), XtRImmediate, (XtPointer) 4},
  {XtNrightMargin,  XtCHorizontalMargins, XtRDimension, sizeof(Dimension),
     offset(right_margin), XtRImmediate, (XtPointer) 4},
  {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
     offset(foreground), XtRString, XtDefaultForeground},
  {XtNfont,  XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     offset(font), XtRString, XtDefaultFont},
};   
#undef offset

/*
 * Semi Public function definitions. 
 */

static void FlipColors(Widget);
static void Initialize(Widget, Widget);
static void Destroy(Widget);
static void Redisplay(Widget, XEvent *, Region);
static void FlipOn(Widget);
static void FlipOff(Widget);
static void PopupMenu(Widget);
static void ClassInitialize(void);
static Boolean SetValues(Widget, Widget, Widget);
static XtGeometryResult QueryGeometry(Widget, XtWidgetGeometry *,
	XtWidgetGeometry *);

/* 
 * Private Function Definitions.
 */

static void GetDefaultSize(Widget, Dimension *, Dimension *);
static void DrawBitmaps(Widget, GC);
static void GetBitmapInfo(Widget, Boolean);
static void CreateGCs(Widget);
static void DestroyGCs(Widget);
    
#define superclass (&cmeClassRec)
CmeBSBClassRec cmeBSBClassRec = {
  {
    /* superclass         */    (WidgetClass) superclass,
    /* class_name         */    "CmeBSB",
    /* size               */    sizeof(CmeBSBRec),
    /* class_initializer  */	ClassInitialize,
    /* class_part_initialize*/	NULL,
    /* Class init'ed      */	FALSE,
    /* initialize         */    (XtInitProc)Initialize,
    /* initialize_hook    */	NULL,
    /* realize            */    NULL,
    /* actions            */    NULL,
    /* num_actions        */    ZERO,
    /* resources          */    resources,
    /* resource_count     */	XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    FALSE, 
    /* compress_exposure  */    FALSE,
    /* compress_enterleave*/ 	FALSE,
    /* visible_interest   */    FALSE,
    /* destroy            */    Destroy,
    /* resize             */    NULL,
    /* expose             */    Redisplay,
    /* set_values         */    (XtSetValuesFunc)SetValues,
    /* set_values_hook    */	NULL,
    /* set_values_almost  */	XtInheritSetValuesAlmost,  
    /* get_values_hook    */	NULL,			
    /* accept_focus       */    NULL,
    /* intrinsics version */	XtVersion,
    /* callback offsets   */    NULL,
    /* tm_table		  */    NULL,
    /* query_geometry	  */    QueryGeometry,
    /* display_accelerator*/    NULL,
    /* extension	  */    NULL
  },{
    /* Menu Entry Fields */
      
    /* highlight */             FlipOn,
    /* unhighlight */           FlipOff,
    /* notify */		XtInheritNotify,
    /* extension	  */    NULL
  }, {
    /* BSB Menu entry Fields */  

    /* extension	  */    NULL
  }
};

WidgetClass cmeBSBObjectClass = (WidgetClass) &cmeBSBClassRec;

/************************************************************
 *
 * Semi-Public Functions.
 *
 ************************************************************/

/*	Function Name: ClassInitialize
 *	Description: Initializes the CmeBSBObject. 
 *	Arguments: none.
 *	Returns: none.
 */

static void 
ClassInitialize(void)
{
    XawInitializeWidgetSet();
    XtAddConverter( XtRString, XtRJustify, XmuCvtStringToJustify, NULL, 0 );
}

/*      Function Name: Initialize
 *      Description: Initializes the complex menu widget
 *      Arguments: request - the widget requested by the argument list.
 *                 new     - the new widget with both resource and non
 *                           resource values.
 *      Returns: none.
 */

static void
Initialize(Widget request _is_unused, Widget new)
{
    CmeBSBObject entry = (CmeBSBObject) new;

    if (entry->cme_bsb.label == NULL) 
	entry->cme_bsb.label = XtName(new);
    else
	entry->cme_bsb.label = XtNewString( entry->cme_bsb.label );

    GetDefaultSize(new, &(entry->rectangle.width), &(entry->rectangle.height));
    CreateGCs(new);

    entry->cme_bsb.left_bitmap_width = entry->cme_bsb.left_bitmap_height = 0;
    entry->cme_bsb.right_bitmap_width = entry->cme_bsb.right_bitmap_height = 0;

    GetBitmapInfo(new, TRUE);	/* Left Bitmap Info */
    GetBitmapInfo(new, FALSE);	/* Right Bitmap Info */

    entry->cme_bsb.ticking = False;
}

/*      Function Name: Destroy
 *      Description: Called at destroy time, cleans up.
 *      Arguments: w - the complex menu widget.
 *      Returns: none.
 */

static void
Destroy(Widget w)
{
    CmeBSBObject entry = (CmeBSBObject) w;

    DestroyGCs(w);
    if (entry->cme_bsb.label != XtName(w))
	XtFree(entry->cme_bsb.label);
    if (entry->cme_bsb.ticking)
	XtRemoveTimeOut(entry->cme_bsb.id);
}

/*      Function Name: Redisplay
 *      Description: Redisplays the contents of the widget.
 *      Arguments: w - the complex menu widget.
 *                 event - the X event that caused this redisplay.
 *                 region - the region the needs to be repainted. 
 *      Returns: none.
 */

static void
Redisplay(Widget w, XEvent *event _is_unused, Region region _is_unused)
{
    GC gc;
    CmeBSBObject entry = (CmeBSBObject) w;
    int	font_ascent, font_descent, y_loc;

    entry->cme_bsb.set_values_area_cleared = FALSE;    
    font_ascent = entry->cme_bsb.font->max_bounds.ascent;
    font_descent = entry->cme_bsb.font->max_bounds.descent;

    y_loc = entry->rectangle.y;
    
    if (XtIsSensitive(w) && XtIsSensitive( XtParent(w) ) ) {
	if ( w == XawComplexMenuGetActiveEntry(XtParent(w)) ) {
	    XFillRectangle(XtDisplayOfObject(w), XtWindowOfObject(w), 
			   entry->cme_bsb.norm_gc, 0, y_loc,
			   (unsigned int) entry->rectangle.width,
			   (unsigned int) entry->rectangle.height);
	    gc = entry->cme_bsb.rev_gc;
	}
	else
	    gc = entry->cme_bsb.norm_gc;
    }
    else
	gc = entry->cme_bsb.norm_gray_gc;
    
    if (entry->cme_bsb.label != NULL) {
	int x_loc = entry->cme_bsb.left_margin;
	int len = strlen(entry->cme_bsb.label);
	char * label = entry->cme_bsb.label;

	switch(entry->cme_bsb.justify) {
	    int width, t_width;

	case XtJustifyCenter:
	    t_width = XTextWidth(entry->cme_bsb.font, label, len);
	    width = entry->rectangle.width - (entry->cme_bsb.left_margin +
					      entry->cme_bsb.right_margin);
	    x_loc += (width - t_width)/2;
	    break;
	case XtJustifyRight:
	    t_width = XTextWidth(entry->cme_bsb.font, label, len);
	    x_loc = entry->rectangle.width - (entry->cme_bsb.right_margin +
					      t_width);
	    break;
	case XtJustifyLeft:
	default:
	    break;
	}

	y_loc += ((int)entry->rectangle.height - 
		  (font_ascent + font_descent)) / 2 + font_ascent;
	
	XDrawString(XtDisplayOfObject(w), XtWindowOfObject(w), gc,
		    x_loc, y_loc, label, len);
    }

    DrawBitmaps(w, gc);
}


/*      Function Name: SetValues
 *      Description: Relayout the menu when one of the resources is changed.
 *      Arguments: current - current state of the widget.
 *                 request - what was requested.
 *                 new - what the widget will become.
 *      Returns: none
 */

static Boolean
SetValues(Widget current, Widget request _is_unused, Widget new)
{
    CmeBSBObject entry = (CmeBSBObject) new;
    CmeBSBObject old_entry = (CmeBSBObject) current;
    Boolean ret_val = FALSE;

    if (old_entry->cme_bsb.label != entry->cme_bsb.label) {
        if (old_entry->cme_bsb.label != XtName( new ) )
	    XtFree( (char *) old_entry->cme_bsb.label );

	if (entry->cme_bsb.label != XtName(new) ) 
	    entry->cme_bsb.label = XtNewString( entry->cme_bsb.label );

	ret_val = True;
    }

    if (entry->rectangle.sensitive != old_entry->rectangle.sensitive )
	ret_val = TRUE;

    if (entry->cme_bsb.left_bitmap != old_entry->cme_bsb.left_bitmap) {
	GetBitmapInfo(new, TRUE);
	ret_val = TRUE;
    }

    if (entry->cme_bsb.right_bitmap != old_entry->cme_bsb.right_bitmap) {
	GetBitmapInfo(new, FALSE);
	ret_val = TRUE;
    }

    if ( (old_entry->cme_bsb.font != entry->cme_bsb.font) ||
	 (old_entry->cme_bsb.foreground != entry->cme_bsb.foreground) ) {
	DestroyGCs(current);
	CreateGCs(new);
	ret_val = TRUE;
    }

    if (ret_val) {
	GetDefaultSize(new, 
		       &(entry->rectangle.width), &(entry->rectangle.height));
	entry->cme_bsb.set_values_area_cleared = TRUE;
    }
    return(ret_val);
}

/*	Function Name: QueryGeometry.
 *	Description: Returns the preferred geometry for this widget.
 *	Arguments: w - the menu entry object.
 *                 itended, return_val - the intended and return geometry info.
 *	Returns: A Geometry Result.
 *
 * See the Intrinsics manual for details on what this function is for.
 * 
 * I just return the height and width of the label plus the margins.
 */

static XtGeometryResult
QueryGeometry(Widget w, XtWidgetGeometry *intended,
	XtWidgetGeometry *return_val)
{
    CmeBSBObject entry = (CmeBSBObject) w;
    Dimension width, height;
    XtGeometryResult ret_val = XtGeometryYes;
    XtGeometryMask mode = intended->request_mode;

    GetDefaultSize(w, &width, &height );    

    if ( ((mode & CWWidth) && (intended->width != width)) ||
	 !(mode & CWWidth) ) {
	return_val->request_mode |= CWWidth;
	return_val->width = width;
	ret_val = XtGeometryAlmost;
    }

    if ( ((mode & CWHeight) && (intended->height != height)) ||
	 !(mode & CWHeight) ) {
	return_val->request_mode |= CWHeight;
	return_val->height = height;
	ret_val = XtGeometryAlmost;
    }

    if (ret_val == XtGeometryAlmost) {
	mode = return_val->request_mode;
	
	if ( ((mode & CWWidth) && (width == entry->rectangle.width)) &&
	     ((mode & CWHeight) && (height == entry->rectangle.height)) )
	    return(XtGeometryNo);
    }

    return(ret_val);
}

/*      Function Name: OnCallback
 *      Description: Timeout callback for submenu pop-up.
 *      Arguments: closure - the bsb menu entry widget.
 *      Returns: none.
 */

static void
OnCallback(XtPointer closure, XtIntervalId *id _is_unused)
{
    Widget w = (Widget) closure;
    CmeBSBObject entry = (CmeBSBObject) w;

    if (entry->cme_bsb.ticking &&
	    XawComplexMenuGetActiveEntry(XtParent(w)) == w)
	PopupMenu(w);
    entry->cme_bsb.ticking = False;
}

/*      Function Name: FlipOn
 *      Description: Invert the colors of the current entry.
 *      Arguments: w - the bsb menu entry widget.
 *      Returns: none.
 */

static void 
FlipOn(Widget w)
{
    CmeBSBObject entry = (CmeBSBObject) w;

    FlipColors(w);
    if (entry->cme_bsb.menu_name == NULL)
	return;
    if (entry->cme_bsb.ticking)
	XtRemoveTimeOut(entry->cme_bsb.id);
    entry->cme_bsb.ticking = True;
    entry->cme_bsb.id = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
	    200L, OnCallback, (XtPointer)w);
}

/*      Function Name: FlipOff
 *      Description: Invert the colors of the current entry.
 *      Arguments: w - the bsb menu entry widget.
 *      Returns: none.
 */

static void 
FlipOff(Widget w)
{
    CmeBSBObject entry = (CmeBSBObject) w;
    Widget menu = NULL, temp;
#define NUM_MENUS	16
    Widget menus[NUM_MENUS];
    int num_menus = 0;

    FlipColors(w);
    if (entry->cme_bsb.menu_name == NULL)
	return;
    if (entry->cme_bsb.ticking) {
	XtRemoveTimeOut(entry->cme_bsb.id);
	entry->cme_bsb.ticking = False;
	return;
    }

    temp = w;
    while(temp != NULL) {
	menu = XtNameToWidget(temp, entry->cme_bsb.menu_name);
	if (menu == NULL) 
	    temp = XtParent(temp);
	else {
#if defined(CmeDebug)
	    printf("FlipOff(BSB %lx) parent '%s': menu is %lx '%s'\n",
			    (unsigned long)w,
			    XtParent(w)->core.name,
			    (unsigned long)menu,
			    entry->cme_bsb.menu_name);
#endif
	    break;
	}
    }

    if (menu == NULL) {
	char error_buf[BUFSIZ];
	snprintf(error_buf, sizeof(error_buf), "CmeBSB: %s %s.",
	    "Could not find menu widget named", entry->cme_bsb.menu_name);
	XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
	return;
    }

    /* Pop down the last menu in the chain, not the first. */
    menus[num_menus++] = menu;
    while ((w = XawComplexMenuGetActiveEntry(menu)) != NULL) {
#if defined(CmeDebug)
	printf("FlipOff: menu has an active entry\n");
#endif
	entry = (CmeBSBObject) w;
	temp = w;
	while (temp != NULL) {
	    menu = XtNameToWidget(temp, entry->cme_bsb.menu_name);
	    if (menu == NULL) 
		temp = XtParent(temp);
	    else
		break;
	}
	if (menu == NULL)
	    break;
    	menus[num_menus++] = menu;
    }
    while (num_menus)
	XtPopdown(menus[--num_menus]);
}
    
/*      Function Name: FlipColors
 *      Description: Invert the colors of the current entry.
 *      Arguments: w - the bsb menu entry widget.
 *      Returns: none.
 */

static void 
FlipColors(Widget w)
{
    CmeBSBObject entry = (CmeBSBObject) w;

    if (entry->cme_bsb.set_values_area_cleared) return;

    XFillRectangle(XtDisplayOfObject(w), XtWindowOfObject(w),
		   entry->cme_bsb.invert_gc, 0, (int) entry->rectangle.y,
		   (unsigned int) entry->rectangle.width, 
		   (unsigned int) entry->rectangle.height);
}

/************************************************************
 *
 * Private Functions.
 *
 ************************************************************/

/*	Function Name: GetDefaultSize
 *	Description: Calculates the Default (preferred) size of
 *                   this menu entry.
 *	Arguments: w - the menu entry widget.
 *                 width, height - default sizes (RETURNED).
 *	Returns: none.
 */

static void
GetDefaultSize(Widget w, Dimension *width, Dimension *height)
{
    CmeBSBObject entry = (CmeBSBObject) w;

    if (entry->cme_bsb.label == NULL) 
	*width = 0;
    else
	*width = XTextWidth(entry->cme_bsb.font, entry->cme_bsb.label,
			    strlen(entry->cme_bsb.label));

    *width += entry->cme_bsb.left_margin + entry->cme_bsb.right_margin;
    
    *height = (entry->cme_bsb.font->max_bounds.ascent +
	       entry->cme_bsb.font->max_bounds.descent);

    *height = ((int)*height * ( ONE_HUNDRED + 
			        entry->cme_bsb.vert_space )) / ONE_HUNDRED;
}

/*      Function Name: DrawBitmaps
 *      Description: Draws left and right bitmaps.
 *      Arguments: w - the complex menu widget.
 *                 gc - graphics context to use for drawing.
 *      Returns: none
 */

static void
DrawBitmaps(Widget w, GC gc)
{
    int x_loc, y_loc;
    CmeBSBObject entry = (CmeBSBObject) w;
    
    if ( (entry->cme_bsb.left_bitmap == None) && 
	 (entry->cme_bsb.right_bitmap == None) ) return;

/*
 * Draw Left Bitmap.
 */

  if (entry->cme_bsb.left_bitmap != None) {
    x_loc = (int)(entry->cme_bsb.left_margin -
	          entry->cme_bsb.left_bitmap_width) / 2;

    y_loc = entry->rectangle.y + (int)(entry->rectangle.height -
				       entry->cme_bsb.left_bitmap_height) / 2;

    XCopyPlane(XtDisplayOfObject(w), entry->cme_bsb.left_bitmap,
	       XtWindowOfObject(w), gc, 0, 0, 
	       entry->cme_bsb.left_bitmap_width,
	       entry->cme_bsb.left_bitmap_height, x_loc, y_loc, 1);
  }

/*
 * Draw Right Bitmap.
 */


  if (entry->cme_bsb.right_bitmap != None) {
    x_loc = entry->rectangle.width -
	      (int)(entry->cme_bsb.right_margin +
		    entry->cme_bsb.right_bitmap_width) / 2;

    y_loc = entry->rectangle.y + (int)(entry->rectangle.height -
				       entry->cme_bsb.right_bitmap_height) / 2;

    XCopyPlane(XtDisplayOfObject(w), entry->cme_bsb.right_bitmap,
	       XtWindowOfObject(w), gc, 0, 0, 
	       entry->cme_bsb.right_bitmap_width,
	       entry->cme_bsb.right_bitmap_height, x_loc, y_loc, 1);
  }
}

/*      Function Name: GetBitmapInfo
 *      Description: Gets the bitmap information from either of the bitmaps.
 *      Arguments: w - the bsb menu entry widget.
 *                 is_left - TRUE if we are testing left bitmap,
 *                           FALSE if we are testing the right bitmap.
 *      Returns: none
 */

static void
GetBitmapInfo(Widget w, Boolean is_left)
{
    CmeBSBObject entry = (CmeBSBObject) w;    
    unsigned int depth, bw;
    Window root;
    int x, y;
    unsigned int width, height;
    char buf[BUFSIZ];
    
    if (is_left) {
	if (entry->cme_bsb.left_bitmap != None) {
	    if (!XGetGeometry(XtDisplayOfObject(w), 
			      entry->cme_bsb.left_bitmap, &root, 
			      &x, &y, &width, &height, &bw, &depth)) {
		snprintf(buf, sizeof(buf),
			"CmeBSB Object: %s %s \"%s\".",
			"Could not",
			"get Left Bitmap geometry information for menu entry ",
			XtName(w));
		XtAppError(XtWidgetToApplicationContext(w), buf);
	    }
	    if (depth != 1) {
		snprintf(buf, sizeof(buf),
			"CmeBSB Object: %s \"%s\"%s.", 
			"Left Bitmap of entry ", 
			XtName(w), " is not one bit deep.");
		XtAppError(XtWidgetToApplicationContext(w), buf);
	    }
	    entry->cme_bsb.left_bitmap_width = (Dimension) width; 
	    entry->cme_bsb.left_bitmap_height = (Dimension) height;
	}
    }
    else if (entry->cme_bsb.right_bitmap != None) {
	if (!XGetGeometry(XtDisplayOfObject(w),
			  entry->cme_bsb.right_bitmap, &root,
			  &x, &y, &width, &height, &bw, &depth)) {
	    snprintf(buf, sizeof(buf), "CmeBSB Object: %s %s \"%s\".",
		    "Could not",
		    "get Right Bitmap geometry information for menu entry ",
		    XtName(w));
	    XtAppError(XtWidgetToApplicationContext(w), buf);
	}
	if (depth != 1) {
	    snprintf(buf, sizeof(buf), "CmeBSB Object: %s \"%s\"%s.", 
		    "Right Bitmap of entry ", XtName(w),
		    " is not one bit deep.");
	    XtAppError(XtWidgetToApplicationContext(w), buf);
	}
	entry->cme_bsb.right_bitmap_width = (Dimension) width; 
	entry->cme_bsb.right_bitmap_height = (Dimension) height;
    }
}      

/*      Function Name: CreateGCs
 *      Description: Creates all gc's for the complex menu widget.
 *      Arguments: w - the complex menu widget.
 *      Returns: none.
 */

static void
CreateGCs(Widget w)
{
    CmeBSBObject entry = (CmeBSBObject) w;    
    XGCValues values;
    XtGCMask mask;
    
    values.foreground = XtParent(w)->core.background_pixel;
    values.background = entry->cme_bsb.foreground;
    values.font = entry->cme_bsb.font->fid;
    values.graphics_exposures = FALSE;
    mask        = GCForeground | GCBackground | GCFont | GCGraphicsExposures;
    entry->cme_bsb.rev_gc = XtGetGC(w, mask, &values);
    
    values.foreground = entry->cme_bsb.foreground;
    values.background = XtParent(w)->core.background_pixel;
    entry->cme_bsb.norm_gc = XtGetGC(w, mask, &values);
    
    values.fill_style = FillTiled;
    values.tile   = XmuCreateStippledPixmap(XtScreenOfObject(w), 
					    entry->cme_bsb.foreground,
					    XtParent(w)->core.background_pixel,
					    XtParent(w)->core.depth);
    values.graphics_exposures = FALSE;
    mask |= GCTile | GCFillStyle;
    entry->cme_bsb.norm_gray_gc = XtGetGC(w, mask, &values);
    
    values.foreground ^= values.background;
    values.background = 0;
    values.function = GXxor;
    mask = GCForeground | GCBackground | GCGraphicsExposures | GCFunction;
    entry->cme_bsb.invert_gc = XtGetGC(w, mask, &values);
}

/*      Function Name: DestroyGCs
 *      Description: Removes all gc's for the complex menu widget.
 *      Arguments: w - the complex menu widget.
 *      Returns: none.
 */

static void
DestroyGCs(Widget w)
{
    CmeBSBObject entry = (CmeBSBObject) w;    

    XtReleaseGC(w, entry->cme_bsb.norm_gc);
    XtReleaseGC(w, entry->cme_bsb.norm_gray_gc);
    XtReleaseGC(w, entry->cme_bsb.rev_gc);
    XtReleaseGC(w, entry->cme_bsb.invert_gc);
}

/*      Function Name: PopupMenu
 *      Description: Pops up the pullright menu associated with this widget.
 *      Arguments: w - the complex menu widget.
 *      Returns: none.
 */

static void
PopupMenu(Widget w)
{
    CmeBSBObject entry = (CmeBSBObject) w;
    Widget menu = NULL, temp;
    Arg arglist[3];
    Cardinal num_args;
    int menu_x, menu_y, menu_width, menu_height, button_width;
    Position button_x, button_y;

    temp = w;
    while(temp != NULL) {
	menu = XtNameToWidget(temp, entry->cme_bsb.menu_name);
	if (menu == NULL) 
	    temp = XtParent(temp);
	else
	    break;
    }

    if (menu == NULL) {
	char error_buf[BUFSIZ];
	snprintf(error_buf, sizeof(error_buf), "CmeBSB: %s %s.",
	    "Could not find menu widget named", entry->cme_bsb.menu_name);
	XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
	return;
    }
#ifdef CmeDebug
    printf("PopupMenu(%lx) '%s'\n", (unsigned long)menu,
		    entry->cme_bsb.menu_name);
#endif
    if (!XtIsRealized(menu))
	XtRealizeWidget(menu);

    menu_width = menu->core.width + 2 * menu->core.border_width;
    button_width = w->core.width + 2 * w->core.border_width;
    menu_height = menu->core.height + 2 * menu->core.border_width;

    XtTranslateCoords(w, 0, 0, &button_x, &button_y);
    menu_x = button_x + button_width + menu->core.border_width - 10; /* XXX */
    menu_y = button_y + 1;

    if (menu_x >= 0) {
	int scr_width = WidthOfScreen(XtScreen(menu));
	if (menu_x + menu_width > scr_width)
	    menu_x = scr_width - menu_width;
    }
    if (menu_x < 0) 
	menu_x = 0;

    if (menu_y >= 0) {
	int scr_height = HeightOfScreen(XtScreen(menu));
	if (menu_y + menu_height > scr_height)
	    menu_y = scr_height - menu_height;
    }
    if (menu_y < 0)
	menu_y = 0;

    num_args = 0;
    XtSetArg(arglist[num_args], XtNx, menu_x); num_args++;
    XtSetArg(arglist[num_args], XtNy, menu_y); num_args++;
    XtSetArg(arglist[num_args], XtNcMparent, XtParent(w)); num_args++;
    XtSetValues(menu, arglist, num_args);

    XtPopup(menu, XtGrabNonexclusive);
}

#ifdef apollo

/*
 * The apollo compiler that we have optimizes out my code for
 * FlipColors() since it is static. and no one executes it in this
 * file.  I am setting the function pointer into the class structure so
 * that it can be called by my parent who will tell me to when to
 * highlight and unhighlight.
 */

void
_XawCmeBSBApolloHack(void)
{
    FlipColors();
}
#endif /* apollo */
