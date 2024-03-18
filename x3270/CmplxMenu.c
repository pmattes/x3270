/* (from) $XConsortium: SimpleMenu.c,v 1.41 92/09/10 16:25:07 converse Exp $ */

/*
 * Copyright (c) 1995-2024 Paul Mattes.
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
 * ComplexMenu.c - Source code file for ComplexMenu widget.
 * (from) SimpleMenu.c - Source code file for SimpleMenu widget.
 *
 * Date:    April 3, 1989
 *
 * By:      Chris D. Peterson
 *          MIT X Consortium 
 *          kit@expo.lcs.mit.edu
 */

#include "globals.h"

#include <stdio.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/XawInit.h>
#include "CmplxMenuP.h"
#include "CmeBSB.h"
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/MenuButton.h>

#include <X11/Xmu/Initer.h>
#include <X11/Xmu/CharSet.h>

#define streq(a, b)        ( strcmp((a), (b)) == 0 )

#define offset(field) XtOffsetOf(ComplexMenuRec, complex_menu.field)

static XtResource resources[] = { 

/*
 * Label Resources.
 */

  {XtNlabel,  XtCLabel, XtRString, sizeof(String),
     offset(label_string), XtRString, NULL},
  {XtNlabelClass,  XtCLabelClass, XtRPointer, sizeof(WidgetClass),
     offset(label_class), XtRImmediate, (XtPointer) NULL},

/*
 * Layout Resources.
 */

  {XtNrowHeight,  XtCRowHeight, XtRDimension, sizeof(Dimension),
     offset(row_height), XtRImmediate, (XtPointer) 0},
  {XtNtopMargin,  XtCVerticalMargins, XtRDimension, sizeof(Dimension),
     offset(top_margin), XtRImmediate, (XtPointer) 0},
  {XtNbottomMargin,  XtCVerticalMargins, XtRDimension, sizeof(Dimension),
     offset(bottom_margin), XtRImmediate, (XtPointer) 0},

/*
 * Misc. Resources
 */

  { XtNallowShellResize, XtCAllowShellResize, XtRBoolean, sizeof(Boolean),
      XtOffsetOf(ComplexMenuRec, shell.allow_shell_resize),
      XtRImmediate, (XtPointer) TRUE },
  {XtNcursor, XtCCursor, XtRCursor, sizeof(Cursor),
      offset(cursor), XtRImmediate, (XtPointer) None},
  {XtNmenuOnScreen,  XtCMenuOnScreen, XtRBoolean, sizeof(Boolean),
      offset(menu_on_screen), XtRImmediate, (XtPointer) TRUE},
  {XtNpopupOnEntry,  XtCPopupOnEntry, XtRWidget, sizeof(Widget),
      offset(popup_entry), XtRWidget, NULL},
  {XtNbackingStore, XtCBackingStore, XtRBackingStore, sizeof (int),
      offset(backing_store), 
      XtRImmediate, (XtPointer) (Always + WhenMapped + NotUseful)},
  {XtNcMparent, XtCCMparent, XtRWidget, sizeof(Widget),
      offset(parent), XtRWidget, NULL},
  {XtNcMdefer, XtCCMdefer, XtRWidget, sizeof(Widget),
      offset(deferred_notify), XtRWidget, NULL},
};  
#undef offset

static char defaultTranslations[] =
    "<EnterWindow>:     highlight()             \n\
     <LeaveWindow>:     leftWindow()            \n\
     <BtnMotion>:       highlight()             \n\
     <BtnUp>:           saveUnhighlight() myMenuPopdown()"; 

/*
 * Semi Public function definitions. 
 */

static void Redisplay(Widget, XEvent *, Region);
static void Realize(Widget, XtValueMask *, XSetWindowAttributes *);
static void ChangeManaged(Widget);
static void Resize(Widget);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void ClassInitialize(void);
static void ClassPartInitialize(WidgetClass);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean SetValuesHook(Widget, ArgList, Cardinal *);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *,
	XtWidgetGeometry *);

/*
 * Action Routine Definitions
 */

static void PositionMenuAction(Widget, XEvent *, String *, Cardinal *);
static void SaveUnhighlight(Widget, XEvent *, String *, Cardinal *);
static void LeftWindow(Widget, XEvent *, String *, Cardinal *);
static void MyMenuPopdown(Widget, XEvent *, String *, Cardinal *);
static void Highlight(Widget, XEvent *, String *, Cardinal *);

/* 
 * Private Function Definitions.
 */

static void Unhighlight(Widget, XEvent *, String *, Cardinal *);
static void Notify(Widget, XEvent *, String *, Cardinal *);
static void MakeSetValuesRequest(Widget, Dimension, Dimension);
static void Layout(Widget, Dimension *, Dimension *);
static void CreateLabel(Widget);
static void AddPositionAction(XtAppContext, caddr_t);
static void ChangeCursorOnGrab(Widget, XtPointer junk, XtPointer);
static void PositionMenu(Widget, XPoint *);
static void ClearParent(Widget, XtPointer, XtPointer);
static Dimension GetMenuWidth(Widget, Widget);
static Dimension GetMenuHeight(Widget);
static Widget FindMenu(Widget, String);
static CmeObject GetEventEntry(Widget, XEvent *);
static CmeObject GetRightEntry(Widget, XEvent *);
static void MoveMenu(Widget, Position, Position);

static XtActionsRec actionsList[] =
{
  {"highlight",         Highlight},
  {"saveUnhighlight",   SaveUnhighlight},
  {"leftWindow",	LeftWindow},
  {"myMenuPopdown",	MyMenuPopdown},
};
 
static CompositeClassExtensionRec extension_rec = {
    /* next_extension */  NULL,
    /* record_type */     NULLQUARK,
    /* version */         XtCompositeExtensionVersion,
    /* record_size */     sizeof(CompositeClassExtensionRec),
    /* accepts_objects */ TRUE,
};

#define superclass (&overrideShellClassRec)
    
ComplexMenuClassRec complexMenuClassRec = {
  {
    /* superclass         */    (WidgetClass) superclass,
    /* class_name         */    "ComplexMenu",
    /* size               */    sizeof(ComplexMenuRec),
    /* class_initialize   */	ClassInitialize,
    /* class_part_initialize*/	ClassPartInitialize,
    /* Class init'ed      */	FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */	NULL,
    /* realize            */    Realize,
    /* actions            */    actionsList,
    /* num_actions        */    XtNumber(actionsList),
    /* resources          */    resources,
    /* resource_count     */	XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    TRUE, 
    /* compress_exposure  */    TRUE,
    /* compress_enterleave*/ 	TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    Resize,
    /* expose             */    Redisplay,
    /* set_values         */    SetValues,
    /* set_values_hook    */	SetValuesHook,
    /* set_values_almost  */	XtInheritSetValuesAlmost,  
    /* get_values_hook    */	NULL,			
    /* accept_focus       */    NULL,
    /* intrinsics version */	XtVersion,
    /* callback offsets   */    NULL,
    /* tm_table		  */    defaultTranslations,
    /* query_geometry	  */    NULL,
    /* display_accelerator*/    NULL,
    /* extension	  */    NULL
  },{
    /* geometry_manager   */    GeometryManager,
    /* change_managed     */    ChangeManaged,
    /* insert_child	  */	XtInheritInsertChild,
    /* delete_child	  */	XtInheritDeleteChild,
    /* extension	  */    NULL
  },{
    /* Shell extension	  */    NULL
  },{
    /* Override extension */    NULL
  },{
    /* Complex Menu extension*/  NULL
  }
};

WidgetClass complexMenuWidgetClass = (WidgetClass)&complexMenuClassRec;

/************************************************************
 *
 * Semi-Public Functions.
 *
 ************************************************************/

/*      Function Name: ClassInitialize
 *      Description: Class Initialize routine, called only once.
 *      Arguments: none.
 *      Returns: none.
 */

static void
ClassInitialize(void)
{
  XawInitializeWidgetSet();
  XtAddConverter( XtRString, XtRBackingStore, XmuCvtStringToBackingStore,
		 NULL, 0 );
  XmuAddInitializer( AddPositionAction, NULL);
}

/*      Function Name: ClassInitialize
 *      Description: Class Part Initialize routine, called for every
 *                   subclass.  Makes sure that the subclasses pick up 
 *                   the extension record.
 *      Arguments: wc - the widget class of the subclass.
 *      Returns: none.
 */

static void
ClassPartInitialize(WidgetClass wc)
{
    ComplexMenuWidgetClass cmwc = (ComplexMenuWidgetClass) wc;

/*
 * Make sure that our subclass gets the extension rec too.
 */

    extension_rec.next_extension = cmwc->composite_class.extension;
    cmwc->composite_class.extension = (XtPointer) &extension_rec;
}

/*      Function Name: Initialize
 *      Description: Initializes the complex menu widget
 *      Arguments: request - the widget requested by the argument list.
 *                 new     - the new widget with both resource and non
 *                           resource values.
 *      Returns: none.
 */

static void
Initialize(Widget request _is_unused, Widget new, ArgList args _is_unused,
	Cardinal *num_args _is_unused)
{
  ComplexMenuWidget cmw = (ComplexMenuWidget) new;

  XmuCallInitializers(XtWidgetToApplicationContext(new));

  if (cmw->complex_menu.label_class == NULL) 
      cmw->complex_menu.label_class = cmeBSBObjectClass;

  cmw->complex_menu.label = NULL;
  cmw->complex_menu.entry_set = NULL;
  cmw->complex_menu.prev_entry = NULL;
  cmw->complex_menu.recursive_set_values = FALSE;

  if (cmw->complex_menu.label_string != NULL)
      CreateLabel(new);

  cmw->complex_menu.menu_width = TRUE;

  if (cmw->core.width == 0) {
      cmw->complex_menu.menu_width = FALSE;
      cmw->core.width = GetMenuWidth(new, NULL);
  }

  cmw->complex_menu.menu_height = TRUE;

  if (cmw->core.height == 0) {
      cmw->complex_menu.menu_height = FALSE;
      cmw->core.height = GetMenuHeight(new);
  }

/*
 * Add a popup_callback routine for changing the cursor.
 */
  
  XtAddCallback(new, XtNpopupCallback, ChangeCursorOnGrab, NULL);

/*
 * Add a popdown_callback routine for clearing the parent field.
 */
  
  XtAddCallback(new, XtNpopdownCallback, ClearParent, NULL);
}

/*      Function Name: Redisplay
 *      Description: Redisplays the contents of the widget.
 *      Arguments: w - the complex menu widget.
 *                 event - the X event that caused this redisplay.
 *                 region - the region the needs to be repainted. 
 *      Returns: none.
 */

static void
Redisplay(Widget w, XEvent *event _is_unused, Region region)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject * entry;
    CmeObjectClass class;

    if (region == NULL)
	XClearWindow(XtDisplay(w), XtWindow(w));

    /*
     * Check and Paint each of the entries - including the label.
     */

    ForAllChildren(cmw, entry) {
	if (!XtIsManaged ( (Widget) *entry)) continue;

	if (region != NULL) 
	    switch(XRectInRegion(region, (int) (*entry)->rectangle.x,
				 (int) (*entry)->rectangle.y,
				 (unsigned int) (*entry)->rectangle.width,
				 (unsigned int) (*entry)->rectangle.height)) {
	    case RectangleIn:
	    case RectanglePart:
		break;
	    default:
		continue;
	    }
	class = (CmeObjectClass) (*entry)->object.widget_class;

	if (class->rect_class.expose != NULL)
	    (class->rect_class.expose)( (Widget) *entry, NULL, NULL);
    }
}

/*      Function Name: Realize
 *      Description: Realizes the widget.
 *      Arguments: w - the complex menu widget.
 *                 mask - value mask for the window to create.
 *                 attrs - attributes for the window to create.
 *      Returns: none
 */

static void
Realize(Widget w, XtValueMask *mask, XSetWindowAttributes *attrs)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;

    attrs->cursor = cmw->complex_menu.cursor;
    *mask |= CWCursor;
    if ((cmw->complex_menu.backing_store == Always) ||
	(cmw->complex_menu.backing_store == NotUseful) ||
	(cmw->complex_menu.backing_store == WhenMapped) ) {
	*mask |= CWBackingStore;
	attrs->backing_store = cmw->complex_menu.backing_store;
    }
    else
	*mask &= ~CWBackingStore;

    (*superclass->core_class.realize) (w, mask, attrs);
}

/*      Function Name: Resize
 *      Description: Handle the menu being resized bigger.
 *      Arguments: w - the complex menu widget.
 *      Returns: none.
 */

static void
Resize(Widget w)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject * entry;

    if ( !XtIsRealized(w) ) return;

    ForAllChildren(cmw, entry) 	/* reset width of all entries. */
	if (XtIsManaged( (Widget) *entry))
	    (*entry)->rectangle.width = cmw->core.width;
    
    Redisplay(w, (XEvent *) NULL, (Region) NULL);
}

/*      Function Name: SetValues
 *      Description: Relayout the menu when one of the resources is changed.
 *      Arguments: current - current state of the widget.
 *                 request - what was requested.
 *                 new - what the widget will become.
 *      Returns: none
 */

static Boolean
SetValues(Widget current, Widget request _is_unused, Widget new,
	ArgList args _is_unused, Cardinal *num_args _is_unused)
{
    ComplexMenuWidget cmw_old = (ComplexMenuWidget) current;
    ComplexMenuWidget cmw_new = (ComplexMenuWidget) new;
    Boolean ret_val = FALSE, layout = FALSE;
    
    if (!XtIsRealized(current)) return(FALSE);
    
    if (!cmw_new->complex_menu.recursive_set_values) {
	if (cmw_new->core.width != cmw_old->core.width) {
	    cmw_new->complex_menu.menu_width = (cmw_new->core.width != 0);
	    layout = TRUE;
	}
	if (cmw_new->core.height != cmw_old->core.height) {
	    cmw_new->complex_menu.menu_height = (cmw_new->core.height != 0);
	    layout = TRUE;
	}
    }

    if (cmw_old->complex_menu.cursor != cmw_new->complex_menu.cursor)
	XDefineCursor(XtDisplay(new),
		      XtWindow(new), cmw_new->complex_menu.cursor);
    
    if (cmw_old->complex_menu.label_string !=cmw_new->complex_menu.label_string) { 
	if (cmw_new->complex_menu.label_string == NULL)         /* Destroy. */
	    XtDestroyWidget((Widget) cmw_old->complex_menu.label);
	else if (cmw_old->complex_menu.label_string == NULL)    /* Create. */
	    CreateLabel(new);
	else {                                                 /* Change. */
	    Arg arglist[1];
	    
	    XtSetArg(arglist[0], XtNlabel, cmw_new->complex_menu.label_string);
	    XtSetValues((Widget) cmw_new->complex_menu.label, arglist, ONE);
	}
    }
    
    if (cmw_old->complex_menu.label_class != cmw_new->complex_menu.label_class)
	XtAppWarning(XtWidgetToApplicationContext(new),
		     "No Dynamic class change of the ComplexMenu Label.");
    
    if ((cmw_old->complex_menu.top_margin != cmw_new->complex_menu.top_margin) ||
	(cmw_old->complex_menu.bottom_margin != 
	 cmw_new->complex_menu.bottom_margin) /* filler.................  */ ) {
	layout = TRUE;
	ret_val = TRUE;
    }

    if (layout)
	Layout(new, NULL, NULL);

    return(ret_val);
}

/*      Function Name: SetValuesHook
 *      Description: To handle a special case, this is passed the
 *                   actual arguments.
 *      Arguments: w - the menu widget.
 *                 arglist - the argument list passed to XtSetValues.
 *                 num_args - the number of args.
 *      Returns: none
 */

/* 
 * If the user actually passed a width and height to the widget
 * then this MUST be used, rather than our newly calculated width and
 * height.
 */

static Boolean
SetValuesHook(Widget w, ArgList arglist, Cardinal *num_args)
{
    register Cardinal i;
    Dimension width, height;
    
    width = w->core.width;
    height = w->core.height;
    
    for ( i = 0 ; i < *num_args ; i++) {
	if ( streq(arglist[i].name, XtNwidth) )
	    width = (Dimension) arglist[i].value;
	if ( streq(arglist[i].name, XtNheight) )
	    height = (Dimension) arglist[i].value;
    }

    if ((width != w->core.width) || (height != w->core.height))
	MakeSetValuesRequest(w, width, height);
    return(FALSE);
}

/************************************************************
 *
 * Geometry Management routines.
 *
 ************************************************************/

/*	Function Name: GeometryManager
 *	Description: This is the ComplexMenu Widget's Geometry Manager.
 *	Arguments: w - the Menu Entry making the request.
 *                 request - requested new geometry.
 *                 reply - the allowed geometry.
 *	Returns: XtGeometry{Yes, No, Almost}.
 */

static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) XtParent(w);
    CmeObject entry = (CmeObject) w;
    XtGeometryMask mode = request->request_mode;
    XtGeometryResult answer;
    Dimension old_height, old_width;

    if ( !(mode & CWWidth) && !(mode & CWHeight) )
	return(XtGeometryNo);

    reply->width = request->width;
    reply->height = request->height;

    old_width = entry->rectangle.width;
    old_height = entry->rectangle.height;

    Layout(w, &(reply->width), &(reply->height) );

/*
 * Since we are an override shell and have no parent there is no one to
 * ask to see if this geom change is okay, so I am just going to assume
 * we can do whatever we want.  If you subclass be very careful with this
 * assumption, it could bite you.
 *
 * Chris D. Peterson - Sept. 1989.
 */

    if ( (reply->width == request->width) &&
	 (reply->height == request->height) ) {

	if ( mode & XtCWQueryOnly ) { /* Actually perform the layout. */
	    entry->rectangle.width = old_width;
	    entry->rectangle.height = old_height;	
	}
	else {
	    Layout(( Widget) cmw, NULL, NULL);
	}
	answer = XtGeometryDone;
    }
    else {
	entry->rectangle.width = old_width;
	entry->rectangle.height = old_height;	

	if ( ((reply->width == request->width) && !(mode & CWHeight)) ||
	      ((reply->height == request->height) && !(mode & CWWidth)) ||
	      ((reply->width == request->width) && 
	       (reply->height == request->height)) )
	    answer = XtGeometryNo;
	else {
	    answer = XtGeometryAlmost;
	    reply->request_mode = 0;
	    if (reply->width != request->width)
		reply->request_mode |= CWWidth;
	    if (reply->height != request->height)
		reply->request_mode |= CWHeight;
	}
    }
    return(answer);
}

/*	Function Name: ChangeManaged
 *	Description: called whenever a new child is managed.
 *	Arguments: w - the complex menu widget.
 *	Returns: none.
 */

static void
ChangeManaged(Widget w)
{
    Layout(w, NULL, NULL);
}

/************************************************************
 *
 * Global Action Routines.
 * 
 * These actions routines will be added to the application's
 * global action list. 
 * 
 ************************************************************/

/*      Function Name: PositionMenuAction
 *      Description: Positions the complex menu widget.
 *      Arguments: w - a widget (no the complex menu widget.)
 *                 event - the event that caused this action.
 *                 params, num_params - parameters passed to the routine.
 *                                      we expect the name of the menu here.
 *      Returns: none
 */

static void
PositionMenuAction(Widget w, XEvent *event, String *params,
	Cardinal *num_params)
{ 
  Widget menu;
  XPoint loc;

  if (*num_params != 1) {
    char error_buf[BUFSIZ];
    snprintf(error_buf, sizeof(error_buf), "%s %s",
	    "Xaw - ComplexMenuWidget: position menu action expects only one",
	    "parameter which is the name of the menu.");
    XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
    return;
  }

  if ( (menu = FindMenu(w, params[0])) == NULL) {
    char error_buf[BUFSIZ];
    snprintf(error_buf, sizeof(error_buf), "%s '%s'",
	    "Xaw - ComplexMenuWidget: could not find menu named: ", params[0]);
    XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
    return;
  }
  
  switch (event->type) {
  case ButtonPress:
  case ButtonRelease:
    loc.x = event->xbutton.x_root;
    loc.y = event->xbutton.y_root;
    PositionMenu(menu, &loc);
    break;
  case EnterNotify:
  case LeaveNotify:
    loc.x = event->xcrossing.x_root;
    loc.y = event->xcrossing.y_root;
    PositionMenu(menu, &loc);
    break;
  case MotionNotify:
    loc.x = event->xmotion.x_root;
    loc.y = event->xmotion.y_root;
    PositionMenu(menu, &loc);
    break;
  default:
    PositionMenu(menu, NULL);
    break;
  }
}  

/************************************************************
 *
 * Widget Action Routines.
 * 
 ************************************************************/

/*      Function Name: Unhighlight
 *      Description: Unhighlights current entry.
 *      Arguments: w - the complex menu widget.
 *                 event - the event that caused this action.
 *                 params, num_params - ** NOT USED **
 *      Returns: none
 */

static void
Unhighlight(Widget w, XEvent *event _is_unused, String *params _is_unused,
	Cardinal *num_params _is_unused)
{ 
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject entry = cmw->complex_menu.entry_set;
    CmeObjectClass class;
 
    if ( entry == NULL) return;

#if defined(CmeDebug)
    printf("Unhighlight(%lx) '%s': zapping %lx\n",
		    (unsigned long)w, w->core.name, (unsigned long)entry);
#endif
    cmw->complex_menu.entry_set = NULL;
    class = (CmeObjectClass) entry->object.widget_class;
    (class->cme_class.unhighlight) ( (Widget) entry);
}

static void
SaveUnhighlight(Widget w, XEvent *event _is_unused, String *params _is_unused,
	Cardinal *num_params _is_unused)
{ 
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject entry = cmw->complex_menu.entry_set;
    CmeObjectClass class;
 
#if defined(CmeDebug)
    printf("SaveUnhighlight(%lx) '%s', BtnUp\n",
		    (unsigned long)w, cmw->core.name);
#endif
    if ( entry == NULL) return;

    class = (CmeObjectClass) entry->object.widget_class;
    (class->cme_class.unhighlight) ( (Widget) entry);
}

/*      Function Name: LeftWindow
 *      Description: Mouse has left window, usually this means unhighlight
 *      Arguments: w - the complex menu widget.
 *                 event - the event that caused this action.
 *                 params, num_params - ** NOT USED **
 *      Returns: none
 */

static void
LeftWindow(Widget w, XEvent *event, String *params _is_unused,
	Cardinal *num_params _is_unused)
{ 
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject entry = cmw->complex_menu.entry_set;
    CmeObjectClass class;
    String mn;

    if ( entry == NULL) return;

    mn = NULL;
    XtVaGetValues((Widget) entry, XtNmenuName, &mn, NULL);
    if (mn != NULL && GetRightEntry(w, event) == entry)
	return;

    cmw->complex_menu.prev_entry = NULL;
    cmw->complex_menu.entry_set = NULL;
    class = (CmeObjectClass) entry->object.widget_class;
    (class->cme_class.unhighlight) ( (Widget) entry);
}

/*      Function Name: MyMenuPopdown
 *      Description: BtnUp, time to pop this menu down (is that all?)
 *      Arguments: w - the complex menu widget.
 *                 event - the event that caused this action.
 *                 params, num_params - ** NOT USED **
 *      Returns: none
 */

static void
MyMenuPopdown(Widget w, XEvent *event, String *params _is_unused,
	Cardinal *num_params _is_unused)
{ 
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;

#if defined(CmeDebug1)
    printf("MyMenuPopdown(%lx) '%s'\n", (unsigned long)w, w->core.name);
#endif

    if (((ShellWidget)w)->shell.popped_up) {
#if defined(CmeDebug1)
	printf("MyMenuPopdown: popping down myself\n");
#endif
	XtPopdown(w);
    }

    /* Cascade up. */
    while (cmw->complex_menu.parent != NULL) {
#if defined(CmeDebug1)
	printf("MyMenuPopdown [cascade up]: parent %lx '%s'\n",
			(unsigned long)cmw->complex_menu.parent,
			cmw->complex_menu.parent->core.name);
#endif
	XtPopdown(cmw->complex_menu.parent);
	cmw = (ComplexMenuWidget) cmw->complex_menu.parent;
    }

}

/*      Function Name: Highlight
 *      Description: Highlights current entry.
 *      Arguments: w - the complex menu widget.
 *                 event - the event that caused this action.
 *                 params, num_params - ** NOT USED **
 *      Returns: none
 */

static void
Highlight(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject entry;
    CmeObjectClass class;
    ShellWidget shell_widget = (ShellWidget)w;

    
#if defined(CmeDebug)
    printf("Highlight(%lx) '%s' ", (unsigned long)w, cmw->core.name);
#endif

    if (shell_widget->shell.popped_up != TRUE) {
#if defined(CmeDebug)
	printf("not popped up -- bogus\n");
#endif
	return;
    }

    if ( !XtIsSensitive(w) ) {
#if defined(CmeDebug)
	printf("not sensitive, nop\n");
#endif
        return;
    }
    
    entry = GetEventEntry(w, event);

    if (entry == cmw->complex_menu.entry_set) {
#if defined(CmeDebug)
	printf("already set, nop\n");
#endif
	return;
    }

#if defined(CmeDebug)
    printf("unhighlighting, ");
#endif
    Unhighlight(w, event, params, num_params);  

    if (entry == NULL) {
#if defined(CmeDebug)
	printf("no new entry, done\n");
#endif
        return;
    }

    if ( !XtIsSensitive( (Widget) entry)) {
#if defined(CmeDebug)
	printf("new entry isn't sensitive, done\n");
#endif
	cmw->complex_menu.entry_set = NULL;
	return;
    }

    cmw->complex_menu.entry_set = entry;
    class = (CmeObjectClass) entry->object.widget_class;

#if defined(CmeDebug)
    printf("highlighting %lx '%s'\n", (unsigned long)entry,
		    XtParent(((Widget)entry))->core.name);
#endif
    (class->cme_class.highlight) ( (Widget) entry);
}

static void
NotifyCallback(XtPointer closure, XtIntervalId *id _is_unused)
{
    CmeObject entry = closure;
    CmeObjectClass class;

#if defined(CmeDebug)
    printf("NotifyCallback %lx\n", (unsigned long)entry);
#endif
    class = (CmeObjectClass) entry->object.widget_class;
    (class->cme_class.notify)( (Widget) entry );
}

/*      Function Name: Notify
 *      Description: Notify user of current entry.
 *      Arguments: w - the complex menu widget.
 *                 event - the event that caused this action.
 *                 params, num_params - ** NOT USED **
 *      Returns: none
 */

static void
Notify(Widget w, XEvent *event _is_unused, String *params _is_unused,
	Cardinal *num_params _is_unused)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject entry = cmw->complex_menu.entry_set;

#if defined(CmeDebug)
    printf("Notify(%lx) '%s': ", (unsigned long)w, cmw->core.name);
#endif

    if (entry != NULL && XtIsSensitive((Widget) entry)) {
#if defined(CmeDebug) /*[*/
        printf("just notifying 0x%lx\n", (unsigned long)entry);
#endif /*]*/
	XtAppAddTimeOut(XtWidgetToApplicationContext(w),
		    1L, NotifyCallback, (XtPointer)entry);
    }
#if defined(CmeDebug) /*[*/
    else
        printf("no entry\n");
#endif /*]*/
    cmw->complex_menu.entry_set = NULL;
    return;
}

/************************************************************
 *
 * Public Functions.
 *
 ************************************************************/
 
/*	Function Name: XawComplexMenuAddGlobalActions
 *	Description: adds the global actions to the complex menu widget.
 *	Arguments: app_con - the appcontext.
 *	Returns: none.
 */

void
XawComplexMenuAddGlobalActions(XtAppContext app_con)
{
    XtInitializeWidgetClass(complexMenuWidgetClass);
    XmuCallInitializers( app_con );
} 

 
/*	Function Name: XawComplexMenuGetActiveEntry
 *	Description: Gets the currently active (set) entry.
 *	Arguments: w - the cmw widget.
 *	Returns: the currently set entry or NULL if none is set.
 */

Widget
XawComplexMenuGetActiveEntry(Widget w)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;

    return( (Widget) cmw->complex_menu.entry_set);
} 

/*	Function Name: XawComplexMenuClearActiveEntry
 *	Description: Unsets the currently active (set) entry.
 *	Arguments: w - the cmw widget.
 *	Returns: none.
 */

void
XawComplexMenuClearActiveEntry(Widget w)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;

    cmw->complex_menu.entry_set = NULL;
    cmw->complex_menu.prev_entry = NULL;
} 

/************************************************************
 *
 * Private Functions.
 *
 ************************************************************/

/*	Function Name: CreateLabel
 *	Description: Creates a the menu label.
 *	Arguments: w - the cmw widget.
 *	Returns: none.
 * 
 * Creates the label object and makes sure it is the first child in
 * in the list.
 */

static void
CreateLabel(Widget w)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    register Widget * child, * next_child;
    register int i;
    Arg args[2];

    if ( (cmw->complex_menu.label_string == NULL) ||
	 (cmw->complex_menu.label != NULL) ) {
	char error_buf[BUFSIZ];

	snprintf(error_buf, sizeof(error_buf),
		"Xaw Complex Menu Widget: %s or %s, %s",
		"label string is NULL", "label already exists", 
		"no label is being created.");
	XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
	return;
    }

    XtSetArg(args[0], XtNlabel, cmw->complex_menu.label_string);
    XtSetArg(args[1], XtNjustify, XtJustifyCenter);
    cmw->complex_menu.label = (CmeObject) 
	                      XtCreateManagedWidget("menuLabel", 
					    cmw->complex_menu.label_class, w,
					    args, TWO);

    next_child = NULL;
    for (child = cmw->composite.children + cmw->composite.num_children,
	 i = cmw->composite.num_children ; i > 0 ; i--, child--) {
	if (next_child != NULL)
	    *next_child = *child;
	next_child = child;
    }
    *child = (Widget) cmw->complex_menu.label;
}

/*	Function Name: Layout
 *	Description: lays the menu entries out all nice and neat.
 *	Arguments: w - See below (+++)
 *                 width_ret, height_ret - The returned width and 
 *                                         height values.
 *	Returns: none.
 *
 * if width == NULL || height == NULL then it assumes the you do not care
 * about the return values, and just want a relayout.
 *
 * if this is not the case then it will set width_ret and height_ret
 * to be width and height that the child would get if it were layed out
 * at this time.
 *
 * +++ "w" can be the complex menu widget or any of its object children.
 */

static void
Layout(Widget w, Dimension *width_ret, Dimension *height_ret)
{
    CmeObject current_entry, *entry;
    ComplexMenuWidget cmw;
    Dimension width, height;
    Boolean do_layout = ((height_ret == NULL) || (width_ret == NULL));
    Boolean allow_change_size;
    height = 0;

    if ( XtIsSubclass(w, complexMenuWidgetClass) ) {
	cmw = (ComplexMenuWidget) w;
	current_entry = NULL;
    }
    else {
	cmw = (ComplexMenuWidget) XtParent(w);
	current_entry = (CmeObject) w;
    }

    allow_change_size = (!XtIsRealized((Widget)cmw) ||
			 (cmw->shell.allow_shell_resize));

    if ( cmw->complex_menu.menu_height )
	height = cmw->core.height;
    else
	if (do_layout) {
	    height = cmw->complex_menu.top_margin;
	    ForAllChildren(cmw, entry) {
		if (!XtIsManaged( (Widget) *entry)) continue;

		if ( (cmw->complex_menu.row_height != 0) && 
		    (*entry != cmw->complex_menu.label) ) 
		    (*entry)->rectangle.height = cmw->complex_menu.row_height;
		
		(*entry)->rectangle.y = height;
		(*entry)->rectangle.x = 0;
		height += (*entry)->rectangle.height;
	    }
	    height += cmw->complex_menu.bottom_margin;
	}
	else {
	    if ((cmw->complex_menu.row_height != 0) && 
		(current_entry != cmw->complex_menu.label) )
		height = cmw->complex_menu.row_height;
	}
    
    if (cmw->complex_menu.menu_width)
	width = cmw->core.width;
    else if ( allow_change_size )
	width = GetMenuWidth((Widget) cmw, (Widget) current_entry);
    else
	width = cmw->core.width;

    if (do_layout) {
	ForAllChildren(cmw, entry)
	    if (XtIsManaged( (Widget) *entry)) 
		(*entry)->rectangle.width = width;

	if (allow_change_size)
	    MakeSetValuesRequest((Widget) cmw, width, height);
    }
    else {
	*width_ret = width;
	if (height != 0)
	    *height_ret = height;
    }
}
    
/*	Function Name: AddPositionAction
 *	Description: Adds the XawPositionComplexMenu action to the global
 *                   action list for this appcon.
 *	Arguments: app_con - the application context for this app.
 *                 data - NOT USED.
 *	Returns: none.
 */

static void
AddPositionAction(XtAppContext app_con, caddr_t data _is_unused)
{
    static XtActionsRec pos_action[] = {
        { "XawPositionComplexMenu", PositionMenuAction },
    };

    XtAppAddActions(app_con, pos_action, XtNumber(pos_action));
}

/*	Function Name: FindMenu
 *	Description: Find the menu give a name and reference widget.
 *	Arguments: widget - reference widget.
 *                 name   - the menu widget's name.
 *	Returns: the menu widget or NULL.
 */

static Widget 
FindMenu(Widget widget, String name)
{
    register Widget w, menu;
    
    for ( w = widget ; w != NULL ; w = XtParent(w) )
	if ( (menu = XtNameToWidget(w, name)) != NULL )
	    return(menu);
    return(NULL);
}

/*	Function Name: PositionMenu
 *	Description: Places the menu
 *	Arguments: w - the complex menu widget.
 *                 location - a pointer the the position or NULL.
 *	Returns: none.
 */

static void
PositionMenu(Widget w, XPoint *location)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject entry;
    XPoint t_point;
    
    if (location == NULL) {
	Window junk1, junk2;
	int root_x, root_y, junkX, junkY;
	unsigned int junkM;
	
	location = &t_point;
	if (XQueryPointer(XtDisplay(w), XtWindow(w), &junk1, &junk2, 
			  &root_x, &root_y, &junkX, &junkY, &junkM) == FALSE) {
	    char error_buf[BUFSIZ];
	    snprintf(error_buf, sizeof(error_buf),
		    "%s %s", "Xaw - ComplexMenuWidget:",
		    "Could not find location of mouse pointer");
	    XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
	    return;
	}
	location->x = (short) root_x;
	location->y = (short) root_y;
    }
    
    /*
     * The width will not be correct unless it is realized.
     */
    
    XtRealizeWidget(w);
    
    location->x -= (Position) w->core.width/2;
    
    if (cmw->complex_menu.popup_entry == NULL)
	entry = cmw->complex_menu.label;
    else
	entry = cmw->complex_menu.popup_entry;

    if (entry != NULL)
	location->y -= entry->rectangle.y + entry->rectangle.height/2;

    MoveMenu(w, (Position) location->x, (Position) location->y);
}

/*	Function Name: MoveMenu
 *	Description: Actually moves the menu, may force it to
 *                   to be fully visable if menu_on_screen is TRUE.
 *	Arguments: w - the complex menu widget.
 *                 x, y - the current location of the widget.
 *	Returns: none 
 */

static void
MoveMenu(Widget w, Position x, Position y)
{
    Arg arglist[2];
    Cardinal num_args = 0;
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    
    if (cmw->complex_menu.menu_on_screen) {
	int width = w->core.width + 2 * w->core.border_width;
	int height = w->core.height + 2 * w->core.border_width;
	
	if (x >= 0) {
	    int scr_width = WidthOfScreen(XtScreen(w));
	    if (x + width > scr_width)
		x = scr_width - width;
	}
	if (x < 0) 
	    x = 0;
	
	if (y >= 0) {
	    int scr_height = HeightOfScreen(XtScreen(w));
	    if (y + height > scr_height)
		y = scr_height - height;
	}
	if (y < 0)
	    y = 0;
    }
    
    XtSetArg(arglist[num_args], XtNx, x); num_args++;
    XtSetArg(arglist[num_args], XtNy, y); num_args++;
    XtSetValues(w, arglist, num_args);
}

/*	Function Name: ChangeCursorOnGrab
 *	Description: Changes the cursor on the active grab to the one
 *                   specified in out resource list.
 *	Arguments: w - the widget.
 *                 junk, garbage - ** NOT USED **.
 *	Returns: None.
 */

static void
ChangeCursorOnGrab(Widget w, XtPointer junk _is_unused, XtPointer garbage _is_unused)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    
#if defined(CmeDebug)
    printf("ChangeCursorOnGrab(%lx) '%s': parent=%lx '%s'\n", (unsigned long)w,
		    cmw->core.name,
		    (unsigned long)cmw->complex_menu.parent,
		    cmw->complex_menu.parent?
			    cmw->complex_menu.parent->core.name: "(null)");
#endif
    cmw->complex_menu.deferred_notify = NULL;
    cmw->complex_menu.prev_entry = NULL;
	
    /*
     * The event mask here is what is currently in the MIT implementation.
     * There really needs to be a way to get the value of the mask out
     * of the toolkit (CDP 5/26/89).
     */
    
    XChangeActivePointerGrab(XtDisplay(w), ButtonPressMask|ButtonReleaseMask,
			     cmw->complex_menu.cursor, 
			     XtLastTimestampProcessed(XtDisplay(w)));
}

static void
ClearParent(Widget w, XtPointer junk _is_unused, XtPointer garbage _is_unused)
{
#if defined(CmeDebug)
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
#endif

#if defined(CmeDebug)
    printf("ClearParent(%lx) '%s': parent=%lx '%s', popped down, notifying\n",
		    (unsigned long)w,
		    cmw->core.name,
		    (unsigned long)cmw->complex_menu.parent,
		    cmw->complex_menu.parent?
			    cmw->complex_menu.parent->core.name: "(null)");
#endif
    Notify(w, NULL, NULL, NULL);
}

/*      Function Name: MakeSetValuesRequest
 *      Description: Makes a (possibly recursive) call to SetValues,
 *                   I take great pains to not go into an infinite loop.
 *      Arguments: w - the complex menu widget.
 *                 width, height - the size of the ask for.
 *      Returns: none
 */

static void
MakeSetValuesRequest(Widget w, Dimension width, Dimension height)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    Arg arglist[2];
    Cardinal num_args = (Cardinal) 0;
    
    if ( !cmw->complex_menu.recursive_set_values ) {
	if ( (cmw->core.width != width) || (cmw->core.height != height) ) {
	    cmw->complex_menu.recursive_set_values = TRUE;
	    XtSetArg(arglist[num_args], XtNwidth, width);   num_args++;
	    XtSetArg(arglist[num_args], XtNheight, height); num_args++;
	    XtSetValues(w, arglist, num_args);
	}
	else if (XtIsRealized( (Widget) cmw))
	    Redisplay((Widget) cmw, (XEvent *) NULL, (Region) NULL);
    }
    cmw->complex_menu.recursive_set_values = FALSE;
}

/*      Function Name: GetMenuWidth
 *      Description: Sets the length of the widest entry in pixels.
 *      Arguments: w - the complex menu widget.
 *      Returns: width of menu.
 */

static Dimension
GetMenuWidth(Widget w, Widget w_ent)
{
    CmeObject cur_entry = (CmeObject) w_ent;
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    Dimension width, widest = (Dimension) 0;
    CmeObject * entry;
    
    if ( cmw->complex_menu.menu_width ) 
	return(cmw->core.width);

    ForAllChildren(cmw, entry) {
	XtWidgetGeometry preferred;

	if (!XtIsManaged( (Widget) *entry)) continue;
	
	if (*entry != cur_entry) {
	    XtQueryGeometry((Widget) *entry, NULL, &preferred);
	    
	    if (preferred.request_mode & CWWidth)
		width = preferred.width;
	    else
		width = (*entry)->rectangle.width;
	}
	else
	    width = (*entry)->rectangle.width;
	
	if ( width > widest )
	    widest = width;
    }
    
    return(widest);
}

/*      Function Name: GetMenuHeight
 *      Description: Sets the length of the widest entry in pixels.
 *      Arguments: w - the complex menu widget.
 *      Returns: width of menu.
 */

static Dimension
GetMenuHeight(Widget w)
{
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject * entry;
    Dimension height;
    
    if (cmw->complex_menu.menu_height)
	return(cmw->core.height);

    height = cmw->complex_menu.top_margin + cmw->complex_menu.bottom_margin;
    
    if (cmw->complex_menu.row_height == 0) {
	ForAllChildren(cmw, entry) 
	    if (XtIsManaged ((Widget) *entry)) 
		height += (*entry)->rectangle.height;
    } else 
	height += cmw->complex_menu.row_height * cmw->composite.num_children;
	
    return(height);
}

/*      Function Name: GetEventEntry
 *      Description: Gets an entry given an event that has X and Y coords.
 *      Arguments: w - the complex menu widget.
 *                 event - the event.
 *      Returns: the entry that this point is in.
 */

static CmeObject
GetEventEntry(Widget w, XEvent *event)
{
    Position x_loc = 0, y_loc = 0;
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject * entry;
    
    switch (event->type) {
    case MotionNotify:
	x_loc = event->xmotion.x;
	y_loc = event->xmotion.y;
	break;
    case EnterNotify:
    case LeaveNotify:
	x_loc = event->xcrossing.x;
	y_loc = event->xcrossing.y;
	break;
    case ButtonPress:
    case ButtonRelease:
	x_loc = event->xbutton.x;
	y_loc = event->xbutton.y;
	break;
    default:
	XtAppError(XtWidgetToApplicationContext(w),
		   "Unknown event type in GetEventEntry().");
	break;
    }
    
    if ( (x_loc < 0) || (x_loc >= (int)cmw->core.width) || (y_loc < 0) ||
	(y_loc >= (int)cmw->core.height) )
	return(NULL);
    
    ForAllChildren(cmw, entry) {
	if (!XtIsManaged ((Widget) *entry)) continue;

	if ( ((*entry)->rectangle.y < y_loc) &&
            ((*entry)->rectangle.y + (int) (*entry)->rectangle.height > y_loc) ) {
	    if ( *entry == cmw->complex_menu.label )
		return(NULL);	/* cannot select the label. */
	    else
		return(*entry);
        }
    }
    
    return(NULL);
}

/*      Function Name: GetRightEntry
 *      Description: Gets an entry given a crossing event that has X and Y
 *                   coords, unless it exited to the right.
 *      Arguments: w - the complex menu widget.
 *                 event - the event.
 *      Returns: the entry that this point is in.
 */

static CmeObject
GetRightEntry(Widget w, XEvent *event)
{
    Position x_loc, y_loc;
    ComplexMenuWidget cmw = (ComplexMenuWidget) w;
    CmeObject * entry;

    x_loc = event->xcrossing.x;
    y_loc = event->xcrossing.y;

    if ( (x_loc < 0) || /*(x_loc < (int)cmw->core.width) ||*/ (y_loc < 0) ||
	(y_loc >= (int)cmw->core.height) )
	return(NULL);

    ForAllChildren(cmw, entry) {
	if (!XtIsManaged ((Widget) *entry)) continue;

	if ( ((*entry)->rectangle.y < y_loc) &&
            ((*entry)->rectangle.y + (int) (*entry)->rectangle.height > y_loc) ) {
	    if ( *entry == cmw->complex_menu.label )
		return(NULL);	/* cannot select the label. */
	    else
		return(*entry);
        }
    }

    return(NULL);
}
