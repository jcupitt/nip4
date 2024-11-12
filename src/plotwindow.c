/* an plot display window
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

struct _Plotwindow {
	GtkApplicationWindow parent;

	Plot *plot;

	/* Widgets.
	 */
	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *plotdisplay;

};

G_DEFINE_TYPE(Plotwindow, plotwindow, GTK_TYPE_APPLICATION_WINDOW);

enum {
	/* Set the plot model we display.
	 */
	PROP_PLOT = 1,

};

static void
plotwindow_dispose(GObject *object)
{
	Plotwindow *plotwindow = PLOTWINDOW(object);

#ifdef DEBUG
	printf("plotwindow_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(plotwindow->plot);

	G_OBJECT_CLASS(plotwindow_parent_class)->dispose(object);
}

static void
plotwindow_set_plot(Plotwindow *plotwindow, Plot *plot)
{
	VIPS_UNREF(plotwindow->plot);

	plotwindow->plot = plot;
	g_object_ref(plot);

	g_object_set(plotwindow->plotdisplay, "plot", plot, NULL);

	gtk_label_set_text(GTK_LABEL(plotwindow->title), "");
	gtk_label_set_text(GTK_LABEL(plotwindow->subtitle), "");

	if (plot) {
		Row *row = HEAPMODEL(plot)->row;

		if (row) {
			char txt[100];
			VipsBuf buf = VIPS_BUF_STATIC(txt);

			row_qualified_name(row, &buf);
			gtk_label_set_text(GTK_LABEL(plotwindow->title),
				vips_buf_all(&buf));
		}

		gtk_label_set_text(GTK_LABEL(plotwindow->subtitle),
			vips_buf_all(&plot->caption_buffer));
	}
}

static void
plotwindow_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Plotwindow *plotwindow = (Plotwindow *) object;

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("plotwindow_set_property: %s = %s\n",
			plotwindow_property_name(prop_id), str);
	}
#endif /*DEBUG*/

	switch (prop_id) {
	case PROP_PLOT:
		plotwindow_set_plot(plotwindow, g_value_get_object(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
plotwindow_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Plotwindow *plotwindow = (Plotwindow *) object;

	switch (prop_id) {
	case PROP_PLOT:
		g_value_set_object(value, plotwindow->plot);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
plotwindow_init(Plotwindow *plotwindow)
{
#ifdef DEBUG
	puts("plotwindow_init");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(plotwindow));
}

static void
plotwindow_class_init(PlotwindowClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	BIND_RESOURCE("plotwindow.ui");

	BIND_VARIABLE(Plotwindow, title);
	BIND_VARIABLE(Plotwindow, subtitle);
	BIND_VARIABLE(Plotwindow, plotdisplay);

	gobject_class->dispose = plotwindow_dispose;
	gobject_class->set_property = plotwindow_set_property;
	gobject_class->get_property = plotwindow_get_property;

	g_object_class_install_property(gobject_class, PROP_PLOT,
		g_param_spec_object("plot",
			_("Plot"),
			_("Plot model to draw"),
			PLOT_TYPE,
			G_PARAM_READWRITE));

}

Plotwindow *
plotwindow_new(App *app)
{
	return g_object_new(PLOTWINDOW_TYPE, "application", app, NULL);
}
