/* a view of a text thingy
 */

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define DEBUG
 */

#include "nip4.h"

G_DEFINE_TYPE(Numberview, numberview, EDITVIEW_TYPE)

/* Re-read the text in a tally entry.
 */
static void *
numberview_scan(View *view)
{
	Numberview *numberview = NUMBERVIEW(view);
	Number *number = NUMBER(VOBJECT(numberview)->iobject);
	Expr *expr = HEAPMODEL(number)->row->expr;

#ifdef DEBUG
	printf("numberview_scan: %s\n", row_name(HEAPMODEL(number)->row));
#endif /*DEBUG*/

	expr_error_clear(expr);

	double value;
	if (!ientry_get_double(IENTRY(EDITVIEW(numberview)->ientry), &value)) {
		error_top(_("Bad value"));
		error_sub(_("not a number"));
		expr_error_set(expr);
		return view;
	}

	if (number->value != value) {
		number->value = value;
		classmodel_update_view(CLASSMODEL(number));
	}

	return VIEW_CLASS(numberview_parent_class)->scan(view);
}

static void
numberview_refresh(vObject *vobject)
{
	Numberview *numberview = NUMBERVIEW(vobject);
	Number *number = NUMBER(VOBJECT(numberview)->iobject);

#ifdef DEBUG
	printf("numberview_scan: %s\n", row_name(HEAPMODEL(number)->row));
#endif /*DEBUG*/

	editview_set_entry(EDITVIEW(numberview), "%g", number->value);

	VOBJECT_CLASS(numberview_parent_class)->refresh(vobject);
}

static void
numberview_class_init(NumberviewClass *class)
{
	ViewClass *view_class = (ViewClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	/* Create signals.
	 */

	/* Init methods.
	 */
	vobject_class->refresh = numberview_refresh;

	view_class->scan = numberview_scan;
}

static void
numberview_init(Numberview *numberview)
{
}

View *
numberview_new(void)
{
	Numberview *numberview = g_object_new(NUMBERVIEW_TYPE, NULL);

	return VIEW(numberview);
}
