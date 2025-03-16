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

G_DEFINE_TYPE(iImageview, iimageview, GRAPHICVIEW_TYPE)

static void
iimageview_dispose(GObject *object)
{
	iImageview *iimageview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_IIMAGEVIEW(object));

	iimageview = IIMAGEVIEW(object);

#ifdef DEBUG
	printf("iimageview_dispose:\n");
#endif /*DEBUG*/

	if (iimageview->wview)
		workspaceview_remove_iimageview(iimageview->wview, iimageview);

	gtk_widget_dispose_template(GTK_WIDGET(iimageview), IIMAGEVIEW_TYPE);

	G_OBJECT_CLASS(iimageview_parent_class)->dispose(object);
}

void
iimageview_compute_visibility(iImageview *iimageview)
{
	gboolean enable;

	enable = FALSE;

	if (iimageview->imagedisplay &&
		iimageview->wview &&
		gtk_widget_get_mapped(GTK_WIDGET(iimageview->imagedisplay))) {
		Workspaceview *wview = iimageview->wview;

		graphene_rect_t bounds;
		if (gtk_widget_compute_bounds(GTK_WIDGET(iimageview->imagedisplay),
			wview->fixed, &bounds)) {
			VipsRect widget = {
				.left = bounds.origin.x,
				.top = bounds.origin.y,
				.width = bounds.size.width,
				.height = bounds.size.height
			};

			// widgets are often size 0 on first refresh, so we can only test
			// x, y
			if (vips_rect_isempty(&widget))
				enable = vips_rect_includespoint(&wview->vp,
					widget.left, widget.top);
			else {
				vips_rect_intersectrect(&wview->vp, &widget, &widget);
				enable = !vips_rect_isempty(&widget);
			}
		}
	}

	if (iimageview->enable != enable) {
#ifdef DEBUG
		printf("iimageview_compute_visibility: %d\n", enable);
#endif /*DEBUG*/

		iimageview->enable = enable;

		if (enable)
			// this will (eventually) restart animated GIF display etc.
			iobject_changed(VOBJECT(iimageview)->iobject);
		else
			g_object_set(iimageview->imagedisplay,
				"tilesource", NULL,
				NULL);

		// don't make tilesource if enable is TRUE, we can wait for
		// _refresh to do this
	}
}

static Workspaceview *
iimageview_get_wview(iImageview *iimageview)
{
	GtkWidget *p;

	for (p = GTK_WIDGET(iimageview);
		p && !IS_WORKSPACEVIEW(p);
		p = gtk_widget_get_parent(p))
		;

	return WORKSPACEVIEW(p);
}

static void
iimageview_refresh(vObject *vobject)
{
	iImageview *iimageview = IIMAGEVIEW(vobject);
	iImage *iimage = IIMAGE(vobject->iobject);

#ifdef DEBUG
	printf("iimageview_refresh:\n");
#endif /*DEBUG*/

	// on the first refresh, register with the enclosing workspaceview
	if (iimageview->first) {
		iimageview->first = FALSE;

		iimageview->wview = iimageview_get_wview(iimageview);
		workspaceview_add_iimageview(iimageview->wview, iimageview);
	}

	iimageview_compute_visibility(iimageview);

	if (iimageview->enable) {
		if (!iimage->value.ii)
			g_object_set(iimageview->imagedisplay,
				"tilesource", NULL,
				NULL);
		else {
			Tilesource *current_tilesource;
			g_object_get(iimageview->imagedisplay,
				"tilesource", &current_tilesource,
				NULL);

			if (current_tilesource &&
				current_tilesource->image != iimage->value.ii->image) {
				Tilesource *new_tilesource =
					tilesource_new_from_iimage(iimage, -1000);

				g_object_set(iimageview->imagedisplay,
					"bestfit", TRUE,
					"tilesource", new_tilesource,
					NULL);

				// set the image loading, if necessary
				tilesource_background_load(new_tilesource);

				VIPS_UNREF(new_tilesource);
			}

			VIPS_UNREF(current_tilesource);
		}
	}

	if (iimageview->label)
		set_glabel(iimageview->label, "%s", IOBJECT(iimage)->caption);

	VOBJECT_CLASS(iimageview_parent_class)->refresh(vobject);
}

static void
iimageview_class_init(iImageviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	BIND_RESOURCE("iimageview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(iImageview, top);
	BIND_VARIABLE(iImageview, imagedisplay);
	BIND_VARIABLE(iImageview, label);

	BIND_CALLBACK(graphicview_pressed);

	object_class->dispose = iimageview_dispose;

	vobject_class->refresh = iimageview_refresh;
}

static void
iimageview_init(iImageview *iimageview)
{
	gtk_widget_init_template(GTK_WIDGET(iimageview));
	iimageview->first = TRUE;
}

View *
iimageview_new(void)
{
	iImageview *iimageview = g_object_new(IIMAGEVIEW_TYPE, NULL);

	return VIEW(iimageview);
}
