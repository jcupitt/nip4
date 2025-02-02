/* run the display for an arrow in a workspace
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

G_DEFINE_TYPE(Fontnameview, fontnameview, GRAPHICVIEW_TYPE)

static void
fontnameview_dispose(GObject *object)
{
	Fontnameview *fontnameview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FONTNAMEVIEW(object));

	fontnameview = FONTNAMEVIEW(object);

	VIPS_FREEF(gtk_widget_unparent, fontnameview->top);

	G_OBJECT_CLASS(fontnameview_parent_class)->dispose(object);
}

static void
fontnameview_link(View *view, Model *model, View *parent)
{
	Fontnameview *fontnameview = FONTNAMEVIEW(view);

	VIEW_CLASS(fontnameview_parent_class)->link(view, model, parent);

	if (GRAPHICVIEW(view)->sview)
		gtk_size_group_add_widget(GRAPHICVIEW(view)->sview->group,
			fontnameview->label);
}

static void
fontnameview_refresh(vObject *vobject)
{
	Fontnameview *fontnameview = FONTNAMEVIEW(vobject);
	Fontname *fontname = FONTNAME(VOBJECT(vobject)->iobject);

#ifdef DEBUG
	printf("fontnameview_refresh: ");
	row_name_print(HEAPMODEL(fontname)->row);
	printf("\n");
#endif /*DEBUG*/

	if (vobject->iobject->caption)
		set_glabel(fontnameview->label, _("%s:"), vobject->iobject->caption);

	if (fontname->value) {
		g_autoptr(PangoFontDescription) desc =
			pango_font_description_from_string(fontname->value);
		g_object_set(fontnameview->button, "font-desc", desc, NULL);
	}

	VOBJECT_CLASS(fontnameview_parent_class)->refresh(vobject);
}

static void
fontnameview_desc_notify(GtkWidget *widget,
	GParamSpec *pspec, Fontnameview *fontnameview)
{
	Fontname *fontname = FONTNAME(VOBJECT(fontnameview)->iobject);

	g_autoptr(PangoFontDescription) desc = NULL;
	g_object_get(fontnameview->button, "font-desc", &desc, NULL);
	if (desc) {
		g_autofree char *value = pango_font_description_to_string(desc);
		VIPS_SETSTR(fontname->value, value);
		classmodel_update_view(CLASSMODEL(fontname));
		symbol_recalculate_all();
	}
}

static void
fontnameview_class_init(FontnameviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("fontnameview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Fontnameview, top);
	BIND_VARIABLE(Fontnameview, label);
	BIND_VARIABLE(Fontnameview, button);

	BIND_CALLBACK(fontnameview_desc_notify);

	object_class->dispose = fontnameview_dispose;

	vobject_class->refresh = fontnameview_refresh;

	view_class->link = fontnameview_link;
}

static void
fontnameview_init(Fontnameview *fontnameview)
{
	gtk_widget_init_template(GTK_WIDGET(fontnameview));
}

View *
fontnameview_new(void)
{
	Fontnameview *fontnameview = g_object_new(FONTNAMEVIEW_TYPE, NULL);

	return VIEW(fontnameview);
}
