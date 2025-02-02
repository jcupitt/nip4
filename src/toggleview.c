/* the display part of a toggle button
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

#include "nip4.h"

G_DEFINE_TYPE(Toggleview, toggleview, GRAPHICVIEW_TYPE)

static void
toggleview_dispose(GObject *object)
{
	Toggleview *togview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_TOGGLEVIEW(object));

	togview = TOGGLEVIEW(object);

	VIPS_FREEF(gtk_widget_unparent, togview->toggle);

	G_OBJECT_CLASS(toggleview_parent_class)->dispose(object);
}

static void
toggleview_toggled(GtkWidget *widget, Toggleview *togview)
{
	Toggle *tog = TOGGLE(VOBJECT(togview)->iobject);
	Classmodel *classmodel = CLASSMODEL(tog);
	gboolean active =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(togview->toggle));

	if (tog->value != active) {
		tog->value = active;
		classmodel_update_view(classmodel);
		symbol_recalculate_all();
	}
}

static void
toggleview_refresh(vObject *vobject)
{
	Toggleview *togview = TOGGLEVIEW(vobject);
	Toggle *tog = TOGGLE(VOBJECT(togview)->iobject);
	GtkCheckButton *button = GTK_CHECK_BUTTON(togview->toggle);

	gtk_check_button_set_active(button, tog->value);
	gtk_check_button_set_label(button, IOBJECT(tog)->caption);

	VOBJECT_CLASS(toggleview_parent_class)->refresh(vobject);
}

static void
toggleview_class_init(ToggleviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	BIND_RESOURCE("toggleview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Toggleview, toggle);
	BIND_CALLBACK(toggleview_toggled);

	object_class->dispose = toggleview_dispose;

	vobject_class->refresh = toggleview_refresh;
}

static void
toggleview_init(Toggleview *togview)
{
	gtk_widget_init_template(GTK_WIDGET(togview));
}

View *
toggleview_new(void)
{
	Toggleview *togview = g_object_new(TOGGLEVIEW_TYPE, NULL);

	return VIEW(togview);
}
