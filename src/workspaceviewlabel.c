/* the label on a workspace tab
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

/*
 */
#define DEBUG

struct _Workspaceviewlabel {
	GtkWidget parent_instance;

	/* The workspaceview we are a label for.
	 */
	Workspaceview *wview;

	GtkWidget *top;
	GtkWidget *label;
	GtkWidget *lock;
	GtkWidget *error;
	GtkWidget *right_click_menu;
};

G_DEFINE_TYPE(Workspaceviewlabel, workspaceviewlabel, GTK_TYPE_WIDGET);

enum {
	PROP_WORKSPACEVIEW = 1,

	SIG_LAST
};

static void
workspaceviewlabel_dispose(GObject *object)
{
	Workspaceviewlabel *wviewlabel = (Workspaceviewlabel *) object;

#ifdef DEBUG
	printf("workspaceviewlabel_dispose:\n");
#endif /*DEBUG*/

	VIPS_FREEF(gtk_widget_unparent, wviewlabel->top);
	UNPARENT(wviewlabel->right_click_menu);

	G_OBJECT_CLASS(workspaceviewlabel_parent_class)->dispose(object);
}

static void
workspaceviewlabel_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Workspaceviewlabel *wviewlabel = (Workspaceviewlabel *) object;

	switch (prop_id) {
	case PROP_WORKSPACEVIEW:
		wviewlabel->wview = g_value_get_object(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
workspaceviewlabel_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Workspaceviewlabel *wviewlabel = (Workspaceviewlabel *) object;

	switch (prop_id) {
	case PROP_WORKSPACEVIEW:
		g_value_set_object(value, wviewlabel->wview);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
workspaceviewlabel_init(Workspaceviewlabel *wviewlabel)
{
#ifdef DEBUG
	printf("workspaceviewlabel_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(wviewlabel));
}

static void
workspaceviewlabel_menu(GtkGestureClick *gesture,
	guint n_press, double x, double y, Workspaceviewlabel *wviewlabel)
{
	gtk_popover_set_pointing_to(GTK_POPOVER(wviewlabel->right_click_menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	gtk_popover_popup(GTK_POPOVER(wviewlabel->right_click_menu));
}

#define BIND(field) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		Workspaceviewlabel, field);

static void
workspaceviewlabel_class_init(WorkspaceviewlabelClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

#ifdef DEBUG
	printf("workspaceviewlabel_class_init:\n");
#endif /*DEBUG*/

	G_OBJECT_CLASS(class)->dispose = workspaceviewlabel_dispose;

	gtk_widget_class_set_layout_manager_type(widget_class,
		GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
		APP_PATH "/workspaceviewlabel.ui");

	BIND(top);
	BIND(label);
	BIND(lock);
	BIND(error);
	BIND(right_click_menu);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		workspaceviewlabel_menu);

	gobject_class->dispose = workspaceviewlabel_dispose;
	gobject_class->set_property = workspaceviewlabel_set_property;
	gobject_class->get_property = workspaceviewlabel_get_property;

	g_object_class_install_property(gobject_class, PROP_WORKSPACEVIEW,
		g_param_spec_object("workspaceview",
			_("Workspaceview"),
			_("The workspaceview we are for"),
			WORKSPACEVIEW_TYPE,
			G_PARAM_READWRITE));
}

Workspaceviewlabel *
workspaceviewlabel_new(Workspaceview *wview)
{
	Workspaceviewlabel *wviewlabel;

#ifdef DEBUG
	printf("workspaceviewlabel_new:\n");
#endif /*DEBUG*/

	wviewlabel = g_object_new(workspaceviewlabel_get_type(),
		"workspaceview", wview,
		NULL);

	return wviewlabel;
}
