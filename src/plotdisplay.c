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

// these must be typedefs for autoptr
typedef struct kdata Kdata;
typedef struct kplot Kplot;
typedef struct kplotcfg Kplotcfg;
typedef struct kplotccfg Kplotccfg;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Kdata, kdata_destroy)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(Kplot, kplot_free)

/* Our series colours ... RGB for the first three, a bit random after that.
 */
static unsigned int plotdisplay_series_rgba[] = {
	0xff0000ff,
	0x00ff00ff,
	0x0000ffff,
	0xff00ffff,
	0xffff00ff,
	0x00ffffff,
	0xffffffff,
	0x000000ff,
};

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

	// the kplot ready to draw
	Kplot *kplot;
	Kplotcfg kcfg;
	Kplotccfg *kccfg;
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
	VIPS_FREE(plotdisplay->kccfg);

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

static gboolean
plotdisplay_build_kplot(Plotdisplay *plotdisplay)
{
	Plot *plot = plotdisplay->plot;

	printf("plotdisplay_build_kplot:\n");

	// use our scaling ... this is in config, unfortunately
	plotdisplay->kcfg.extrema = 1;
	plotdisplay->kcfg.extrema_xmin = plot->xmin;
	plotdisplay->kcfg.extrema_xmax = plot->xmax;
	plotdisplay->kcfg.extrema_ymin = plot->ymin;
	plotdisplay->kcfg.extrema_ymax = plot->ymax;

	g_autoptr(Kplot) kplot = kplot_alloc(&plotdisplay->kcfg);

	for (int i = 0; i < plot->columns; i++) {
		g_autoptr(Kdata) kdata = NULL;

		if (!(kdata = kdata_array_alloc(NULL, plot->rows)))
			return FALSE;

		for (int j = 0; j < plot->rows; j++)
			kdata_set(kdata, j, plot->xcolumn[i][j], plot->ycolumn[i][j]);

		kplot_attach_data(kplot, kdata,
			plot->style == PLOT_STYLE_POINT ? KPLOT_POINTS : KPLOT_LINES, NULL);
	}

	VIPS_FREEF(kplot_free, plotdisplay->kplot);
	plotdisplay->kplot = g_steal_pointer(&kplot);

	printf("\tsuccess\n");

	return TRUE;
}

static void
plotdisplay_plot_changed(Plot *plot, Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_tilecache_changed:\n");
#endif /*DEBUG*/

	if (plotdisplay->plot)
		plotdisplay_build_kplot(plotdisplay);

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
	Plotdisplay *plotdisplay = (Plotdisplay *) user_data;

#ifdef DEBUG_VERBOSE
	printf("plotdisplay_draw_function:\n");
#endif /*DEBUG_VERBOSE*/

	if (plotdisplay->kplot) {
		// white background
		GdkRGBA white;
		gdk_rgba_parse(&white, "white");
		gdk_cairo_set_source_rgba(cr, &white);
		cairo_rectangle(cr, 0.0, 0.0, width, height);
		cairo_fill(cr);

		// and plot
		kplot_draw(plotdisplay->kplot, width, height, cr);
	}
}

static void
plotdisplay_init(Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_init:\n");
#endif /*DEBUG*/

	// minimal config for thumbnail draw
	kplotcfg_defaults(&plotdisplay->kcfg);
	plotdisplay->kcfg.ticlabel = 0;
	plotdisplay->kcfg.marginsz = 3;
	plotdisplay->kcfg.ticline.len = 0;
	plotdisplay->kcfg.grid = 0;

	// set colours ... there seems to be no useful API for this
	plotdisplay->kcfg.clrsz = VIPS_NUMBER(plotdisplay_series_rgba);
	plotdisplay->kcfg.clrs =
		VIPS_ARRAY(NULL, plotdisplay->kcfg.clrsz, Kplotccfg);
	for (int i = 0; i < plotdisplay->kcfg.clrsz; i++) {
		plotdisplay->kcfg.clrs[i].type = KPLOTCTYPE_RGBA;

		for (int j = 0; j < 4; j++)
			plotdisplay->kcfg.clrs[i].rgba[3 - j] =
				((plotdisplay_series_rgba[i] >> (8 * j)) & 0xff) / 255.0;
	}

	g_signal_connect(GTK_DRAWING_AREA(plotdisplay), "resize",
		G_CALLBACK(plotdisplay_resize), NULL);

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(plotdisplay),
		plotdisplay_draw_function, plotdisplay, NULL);
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
