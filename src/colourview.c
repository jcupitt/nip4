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
colourview_refresh(vObject *vobject)
{
	Colourview *colourview = COLOURVIEW(vobject);
	Colour *colour = COLOUR(vobject->iobject);

#ifdef DEBUG
	printf("colourview_refresh\n");
#endif /*DEBUG*/

	Imageinfo *imageinfo = colour_ii_new(colour);
	g_autoptr(Tilesource) tilesource = tilesource_new_from_imageinfo(imageinfo);
	if (tilesource) {
		g_object_set(colourview->imagedisplay,
			"tilesource", tilesource,
			// zoom zoom zoom!
			"zoom", 1024.0,
			"bestfit", FALSE,
			NULL);

		tilesource_background_load(tilesource);
	}

	set_glabel(colourview->label, "%s", vips_buf_all(&colour->caption));

	VOBJECT_CLASS(colourview_parent_class)->refresh(vobject);
}

static void
colourview_class_init(ColourviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	BIND_RESOURCE("colourview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Colourview, top);
	BIND_VARIABLE(Colourview, imagedisplay);
	BIND_VARIABLE(Colourview, label);

	BIND_CALLBACK(graphicview_pressed);

	object_class->dispose = colourview_dispose;

	vobject_class->refresh = colourview_refresh;
}

static void
colourview_init(Colourview *colourview)
{
#ifdef DEBUG
	printf("colourview_init\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(colourview));
}

View *
colourview_new(void)
{
	Colourview *colourview = g_object_new(COLOURVIEW_TYPE, NULL);

	return VIEW(colourview);
}
