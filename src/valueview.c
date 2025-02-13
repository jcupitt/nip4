/* display a minimal graphic for an object (just the caption)
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

G_DEFINE_TYPE(Valueview, valueview, GRAPHICVIEW_TYPE)

static void
valueview_dispose(GObject *object)
{
	Valueview *valueview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_VALUEVIEW(object));

	valueview = VALUEVIEW(object);

#ifdef DEBUG
	printf("valueview_dispose:\n");
#endif /*DEBUG*/

	gtk_widget_dispose_template(GTK_WIDGET(valueview), VALUEVIEW_TYPE);

	G_OBJECT_CLASS(valueview_parent_class)->dispose(object);
}

static void
valueview_refresh(vObject *vobject)
{
	Valueview *valueview = VALUEVIEW(vobject);
	Model *model = MODEL(vobject->iobject);

#ifdef DEBUG
	printf("valueview_refresh: ");
	row_name_print(HEAPMODEL(model)->row);
	printf("\n");
#endif /*DEBUG*/

	set_glabel(valueview->label, "%s", IOBJECT(model)->caption);

	VOBJECT_CLASS(valueview_parent_class)->refresh(vobject);
}

static void
valueview_class_init(ValueviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	BIND_RESOURCE("valueview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Valueview, label);

	BIND_CALLBACK(graphicview_pressed);

	/* Create signals.
	 */

	/* Init methods.
	 */
	object_class->dispose = valueview_dispose;

	vobject_class->refresh = valueview_refresh;
}

static void
valueview_init(Valueview *valueview)
{
	gtk_widget_init_template(GTK_WIDGET(valueview));
}

View *
valueview_new(void)
{
	Valueview *valueview = g_object_new(VALUEVIEW_TYPE, NULL);

	return VIEW(valueview);
}
