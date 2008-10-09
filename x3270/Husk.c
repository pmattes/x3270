/*
 * Copyright 1996-2008 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 * Husk.c - Husk composite widget
 *	A "Husk" (a nearly useless shell) is a trivial container widget, a
 *	subclass of the Athena Composite widget with a no-op geometry manager
 *	that allows children to move and resize themselves at will.
 */

#include "globals.h"
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Misc.h>
#include <X11/Xaw/XawInit.h>
#include "HuskP.h"

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(register Widget, Mask *, XSetWindowAttributes *);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *,
	XtWidgetGeometry *);
static void ChangeManaged(Widget);
static XtGeometryResult QueryGeometry(Widget, XtWidgetGeometry *,
	XtWidgetGeometry *);

HuskClassRec huskClassRec = {
    { /* core_class fields      */
	/* superclass         */ (WidgetClass) & compositeClassRec,
	/* class_name         */ "Husk",
	/* widget_size        */ sizeof(HuskRec),
	/* class_initialize   */ ClassInitialize,
	/* class_part_init    */ NULL,
	/* class_inited       */ FALSE,
	/* initialize         */ Initialize,
	/* initialize_hook    */ NULL,
	/* realize            */ Realize,
	/* actions            */ NULL,
	/* num_actions	      */ 0,
	/* resources          */ NULL,
	/* num_resources      */ 0,
	/* xrm_class          */ NULLQUARK,
	/* compress_motion    */ TRUE,
	/* compress_exposure  */ TRUE,
	/* compress_enterleave */ TRUE,
	/* visible_interest   */ FALSE,
	/* destroy            */ NULL,
	/* resize             */ NULL,
	/* expose             */ NULL,
	/* set_values         */ SetValues,
	/* set_values_hook    */ NULL,
	/* set_values_almost  */ XtInheritSetValuesAlmost,
	/* get_values_hook    */ NULL,
	/* accept_focus       */ NULL,
	/* version            */ XtVersion,
	/* callback_private   */ NULL,
	/* tm_table           */ NULL,
	/* query_geometry     */ QueryGeometry,
	/* display_accelerator */ XtInheritDisplayAccelerator,
	/* extension          */ NULL
    }, {
	/* composite_class fields */
	/* geometry_manager   */ GeometryManager,
	/* change_managed     */ ChangeManaged,
	/* insert_child	      */ XtInheritInsertChild,
	/* delete_child	      */ XtInheritDeleteChild,
	/* extension          */ NULL
    }, {
	/* Husk class fields */
	/* empty	      */ 0,
    }
};

WidgetClass huskWidgetClass = (WidgetClass)&huskClassRec;

static XtGeometryResult 
QueryGeometry(Widget widget _is_unused, XtWidgetGeometry *constraint _is_unused,
	XtWidgetGeometry *preferred _is_unused)
{
	return XtGeometryYes;
}

static XtGeometryResult 
GeometryManager(Widget w, XtWidgetGeometry *request,
    XtWidgetGeometry *reply _is_unused)
{
	/* Always succeed. */
	if (!(request->request_mode & XtCWQueryOnly)) {
		if (request->request_mode & CWX)
			w->core.x = request->x;
		if (request->request_mode & CWY)
			w->core.y = request->y;
		if (request->request_mode & CWWidth)
			w->core.width = request->width;
		if (request->request_mode & CWHeight)
			w->core.height = request->height;
		if (request->request_mode & CWBorderWidth)
			w->core.border_width = request->border_width;
	}
	return XtGeometryYes;
}

static void 
ChangeManaged(Widget w _is_unused)
{
}

static void 
ClassInitialize(void)
{
	XawInitializeWidgetSet();
}

static void 
Initialize(Widget request _is_unused, Widget new _is_unused, ArgList args _is_unused,
	Cardinal *num_args _is_unused)
{
}

static void 
Realize(register Widget w, Mask *valueMask, XSetWindowAttributes *attributes)
{
	attributes->bit_gravity = NorthWestGravity;
	*valueMask |= CWBitGravity;

	XtCreateWindow(w, (unsigned)InputOutput, (Visual *)CopyFromParent,
	       *valueMask, attributes);
}

static Boolean 
SetValues(Widget current _is_unused, Widget request _is_unused, Widget new _is_unused,
	ArgList args _is_unused, Cardinal *num_args _is_unused)
{
	return False;
}
