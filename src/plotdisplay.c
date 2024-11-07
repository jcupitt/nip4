/* display an plot in a drawingarea
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
#define DEBUG_VERBOSE
#define DEBUG

struct _Plotdisplay {
	GtkDrawingArea parent_instance;

	/* The Plot model we draw. We hold a ref to this.
	 */
	Plot *plot;

	/* The rect of the widget.
	 */
	VipsRect widget_rect;

	/* The sub-area of widget_rect that we paint.
	 */
	VipsRect paint_rect;

	/* Draw in thumbnail (low detail) mode.
	 */
	gboolean thumbnail;

	// add the various kplot data structs
};

G_DEFINE_TYPE(Plotdisplay, plotdisplay, GTK_TYPE_DRAWING_AREA);

enum {
	/* Set the plot model we display.
	 */
	PROP_PLOT = 1,

	/* Draw in low-detail mode.
	 */
	PROP_THUMBNAIL,

};

static void
plotdisplay_dispose(GObject *object)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) object;

#ifdef DEBUG
	printf("plotdisplay_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(plotdisplay->plot);

	G_OBJECT_CLASS(plotdisplay_parent_class)->dispose(object);
}

static void
plotdisplay_layout(Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_layout:\n");
#endif /*DEBUG*/

	plotdisplay->widget_rect.width =
		gtk_widget_get_width(GTK_WIDGET(plotdisplay));
	plotdisplay->widget_rect.height =
		gtk_widget_get_height(GTK_WIDGET(plotdisplay));

	/* width and height will be 0 if _layout runs too early to be useful.
	 */
	if (!plotdisplay->widget_rect.width ||
		!plotdisplay->widget_rect.height)
		return;

	/* If there's no plot yet, we can't do anything.
	 */
	if (!plotdisplay->plot)
		return;

	// smaller margins in thumbnail mode
	plotdisplay->paint_rect = plotdisplay->widget_rect;
	vips_rect_marginadjust(&plotdisplay->paint_rect,
			plotdisplay->thumbnail ? -10 : -50);
}

static void
plotdisplay_plot_changed(Plot *plot, Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_tilecache_changed:\n");
#endif /*DEBUG*/

	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_set_plot(Plotdisplay *plotdisplay, Plot *plot)
{
	VIPS_UNREF(plotdisplay->plot);

	if (plot) {
		plotdisplay->plot = plot;
		g_object_ref(plotdisplay->plot);

		g_signal_connect_object(plot, "changed",
			G_CALLBACK(plotdisplay_plot_changed),
			plotdisplay, 0);

		/* Do initial change to init.
		 */
		plotdisplay_plot_changed(plot, plotdisplay);
	}
}

#ifdef DEBUG
static const char *
plotdisplay_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_PLOT:
		return "PLOT";
		break;

	case PROP_THUMBNAIL:
		return "THUMBNAIL";
		break;

	default:
		return "<unknown>";
	}
}
#endif /*DEBUG*/

static void
plotdisplay_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) object;

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("plotdisplay_set_property: %s = %s\n",
			plotdisplay_property_name(prop_id), str);
	}
#endif /*DEBUG*/

	switch (prop_id) {
	case PROP_PLOT:
		plotdisplay_set_plot(plotdisplay, g_value_get_object(value));
		break;

	case PROP_THUMBNAIL:
		plotdisplay->thumbnail = g_value_get_boolean(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
plotdisplay_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) object;

	switch (prop_id) {
	case PROP_PLOT:
		g_value_set_object(value, plotdisplay->plot);
		break;

	case PROP_THUMBNAIL:
		g_value_set_boolean(value, plotdisplay->thumbnail);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
plotdisplay_resize(GtkWidget *widget, int width, int height)
{
#ifdef DEBUG_VERBOSE
	printf("plotdisplay_resize: %d x %d\n", width, height);
#endif /*DEBUG_VERBOSE*/

	Plotdisplay *plotdisplay = (Plotdisplay *) widget;

	plotdisplay_layout(plotdisplay);

	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_draw_function(GtkDrawingArea *area,
	cairo_t *cr, int width, int height, gpointer user_data)
{
#ifdef DEBUG_VERBOSE
	printf("plotdisplay_draw_function:\n");
#endif /*DEBUG_VERBOSE*/

	//Plotdisplay *plotdisplay = (Plotdisplay *) user_data;

	GdkRGBA color;

	cairo_arc(cr,
		width / 2.0, height / 2.0, MIN (width, height) / 2.0, 0, 2 * G_PI);

	gtk_widget_get_color(GTK_WIDGET(area), &color);
	gdk_cairo_set_source_rgba(cr, &color);

	cairo_fill(cr);
}

static void
plotdisplay_init(Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_init:\n");
#endif /*DEBUG*/

	g_signal_connect(GTK_DRAWING_AREA(plotdisplay), "resize",
		G_CALLBACK(plotdisplay_resize), NULL);

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(plotdisplay),
		plotdisplay_draw_function, NULL, NULL);
}

static void
plotdisplay_class_init(PlotdisplayClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("plotdisplay_class_init:\n");
#endif /*DEBUG*/

	gobject_class->dispose = plotdisplay_dispose;
	gobject_class->set_property = plotdisplay_set_property;
	gobject_class->get_property = plotdisplay_get_property;

	g_object_class_install_property(gobject_class, PROP_PLOT,
		g_param_spec_object("plot",
			_("Plot"),
			_("Plot model to draw"),
			PLOT_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_THUMBNAIL,
		g_param_spec_boolean("thumbnail",
			_("Thumbnail"),
			_("Display a plot in thumbnail mode"),
			FALSE,
			G_PARAM_READWRITE));

}

Plotdisplay *
plotdisplay_new(Plot *plot)
{
	Plotdisplay *plotdisplay;

#ifdef DEBUG
	printf("plotdisplay_new:\n");
#endif /*DEBUG*/

	plotdisplay = g_object_new(plotdisplay_get_type(),
		"plot", plot,
		NULL);

	return plotdisplay;
}

/* plot	level0 plot coordinates ... this is the coordinate space we
 *		pass down to tilecache
 *
 * gtk		screen cods, so the coordinates we use to render tiles
 */

void
plotdisplay_plot_to_gtk(Plotdisplay *plotdisplay,
	double x_plot, double y_plot, double *x_gtk, double *y_gtk)
{
	*x_gtk = x_plot + plotdisplay->paint_rect.left;
	*y_gtk = y_plot + plotdisplay->paint_rect.top;
}

void
plotdisplay_gtk_to_plot(Plotdisplay *plotdisplay,
	double x_gtk, double y_gtk, double *x_plot, double *y_plot)
{
	*x_plot = x_gtk - plotdisplay->paint_rect.left;
	*y_plot = y_gtk - plotdisplay->paint_rect.top;
}
