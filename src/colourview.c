/* run the display for an image in a workspace
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

G_DEFINE_TYPE(Colourview, colourview, GRAPHICVIEW_TYPE)

static void
colourview_dispose(GObject *object)
{
	Colourview *colourview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_COLOURVIEW(object));

	colourview = COLOURVIEW(object);

#ifdef DEBUG
	printf("colourview_dispose:\n");
#endif /*DEBUG*/

	gtk_widget_dispose_template(GTK_WIDGET(colourview), COLOURVIEW_TYPE);

	G_OBJECT_CLASS(colourview_parent_class)->dispose(object);
}

static void
colourview_realize(GtkWidget *widget)
{
	GTK_WIDGET_CLASS(colourview_parent_class)->realize(widget);

	/* Mark us as a symbol drag-to widget.
	 */
	set_symbol_drag_type(widget);
}

static Rowview *
colourview_rowview(Colourview *colourview)
{
	View *p;

	for (p = VIEW(colourview); !IS_ROWVIEW(p); p = p->parent)
		;

	return ROWVIEW(p);
}

static void
colourview_click(GtkGestureClick *gesture,
	guint n_press, double x, double y, Colourview *colourview)
{
	if (n_press == 1) {
		Rowview *rowview = colourview_rowview(colourview);
		Row *row = ROW(VOBJECT(rowview)->iobject);
		guint state = get_modifiers(GTK_EVENT_CONTROLLER(gesture));

		row_select_modifier(row, state);
	}
	else {
		Colour *colour = COLOUR(VOBJECT(colourview)->iobject);

		model_edit(GTK_WIDGET(colourview), MODEL(colour));
	}
}

static void
colourview_refresh(vObject *vobject)
{
	Colourview *colourview = COLOURVIEW(vobject);
	Colour *colour = COLOUR(vobject->iobject);

#ifdef DEBUG
	printf("colourview_refresh\n");
#endif /*DEBUG*/

	g_object_set(colourview->imagedisplay,
		"bestfit", TRUE,
		"tilesource", tilesource_new_from_imageinfo(colour_ii_new(colour)),
		NULL);

	set_glabel(colourview->label, "%s", vips_buf_all(&colour->caption));

	VOBJECT_CLASS(colourview_parent_class)->refresh(vobject);
}

static void
colourview_class_init(ColourviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	BIND_RESOURCE("colourview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(Colourview, top);
	BIND_VARIABLE(Colourview, imagedisplay);
	BIND_VARIABLE(Colourview, label);

	BIND_CALLBACK(colourview_click);

	object_class->dispose = colourview_dispose;

	widget_class->realize = colourview_realize;

	vobject_class->refresh = colourview_refresh;
}

static void
colourview_changed(Imagedisplay *imagedisplay, Colourview *colourview)
{
	Colour *colour = COLOUR(VOBJECT(colourview)->iobject);

	double rgb[3];
	Tilesource *tilesource;

	printf("colourview_changed:\n");

	//imageinfo_get_rgb(id->tilesource->ii, rgb);
	//colour_set_rgb(colour, rgb);
}

static void
colourview_init(Colourview *colourview)
{
#ifdef DEBUG
	printf("colourview_init\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(colourview));

	g_signal_connect(colourview->imagedisplay, "changed",
		G_CALLBACK(colourview_changed), colourview);
}

View *
colourview_new(void)
{
	Colourview *colourview = g_object_new(COLOURVIEW_TYPE, NULL);

	return VIEW(colourview);
}
