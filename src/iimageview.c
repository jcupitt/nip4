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
iimageview_dispose(GObject *object)
{
	iImageview *iimageview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_IIMAGEVIEW(object));

	iimageview = IIMAGEVIEW(object);

#ifdef DEBUG
	printf("iimageview_dispose:\n");
#endif /*DEBUG*/

	gtk_widget_dispose_template(GTK_WIDGET(iimageview), IIMAGEVIEW_TYPE);
	Workspaceview *wview = iimageview_get_wview(iimageview);
	if (wview)
		workspaceview_remove_iimageview(wview, iimageview);

	G_OBJECT_CLASS(iimageview_parent_class)->dispose(object);
}

static void
iimageview_refresh(vObject *vobject)
{
	iImageview *iimageview = IIMAGEVIEW(vobject);
	iImage *iimage = IIMAGE(vobject->iobject);

#ifdef DEBUG
	printf("iimageview_refresh: FIXME\n");
#endif /*DEBUG*/

	iimage_tilesource_update(iimage);

	Tilesource *current_tilesource;
	g_object_get(iimageview->imagedisplay,
		"tilesource", &current_tilesource,
		NULL);
	if (current_tilesource != iimage->tilesource) {
		g_object_set(iimageview->imagedisplay,
			"bestfit", TRUE,
			"tilesource", iimage->tilesource,
			NULL);

		// set the image loading, if necessary
		if (iimage->tilesource)
			tilesource_background_load(iimage->tilesource);
	}
	VIPS_UNREF(current_tilesource);

	// on the first refresh, register with the encosing workspaceview
	if (iimageview->first) {
		Workspaceview *wview = iimageview_get_wview(iimageview);

		if (wview)
			workspaceview_add_iimageview(wview, iimageview);
		iimageview->first = FALSE;
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

	BIND_CALLBACK(graphicview_click);

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
