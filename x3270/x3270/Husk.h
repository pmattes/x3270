/*
 * Copyright 1996, 1999, 2000 by Paul Mattes.
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
 * Husk.h
 *	Husk Widget (subclass of CompositeClass)
 */

/* Parameters:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		Pixel		XtDefaultBackground
 border		     BorderColor	Pixel		XtDefaultForeground
 borderWidth	     BorderWidth	Dimension	1
 destroyCallback     Callback		Pointer		NULL
 height		     Height		Dimension	0
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 width		     Width		Dimension	0
 x		     Position		Position	0
 y		     Position		Position	0

*/

/* Class record constants */

extern WidgetClass huskWidgetClass;

typedef struct _HuskClassRec *HuskWidgetClass;
typedef struct _HuskRec      *HuskWidget;
