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
#define DEBUG_VERBOSE
#define DEBUG
 */

// these must be typedefs for autoptr
typedef struct kdata Kdata;
typedef struct kplot Kplot;
typedef struct kplotcfg Kplotcfg;
typedef struct kplotccfg Kplotccfg;
typedef struct kplotline Kplotline;
typedef struct kplotpoint Kplotpoint;
typedef struct kdatacfg Kdatacfg;
typedef struct kplotfont Kplotfont;
typedef struct kplotctx Kplotctx;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Kdata, kdata_destroy)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(Kplot, kplot_free)

/* Our series colours ... RGB for the first three, a bit random after that.
 */
static const char *plotdisplay_series_rgba[] = {
	"red",
	"green",
	"blue",
	"cyan",
	"magenta",
	"yellow",
	"orange",
	"purple",
	"lime",
	"black",
	"red",
	"green",
	"blue",
	"cyan",
	"magenta",
	"yellow",
	"orange",
	"purple",
	"lime",
	"black",
	"red",
	"green",
	"blue",
	"cyan",
	"magenta",
	"yellow",
	"orange",
	"purple",
	"lime",
	"black",
};

// colours as an array of Kplotccfg
static Kplotccfg *plotdisplay_series_ccfg = NULL;

// line styles
static Kdatacfg plotdisplay_series_datacfg_thumbnail = {
	.line = {
		.sz = 0.5,
		.dashesz = 0,
		.join = CAIRO_LINE_JOIN_ROUND,
		.clr = {
			.type = KPLOTCTYPE_DEFAULT,
		},
	},
	.point = {
		.sz = 0.5,
		.radius = 1,
		.dashesz = 0,
		.clr = {
			.type = KPLOTCTYPE_DEFAULT,
		},
	},
};

static Kdatacfg plotdisplay_series_datacfg_window = {
	.line = {
		.sz = 3,
		.dashesz = 0,
		.join = CAIRO_LINE_JOIN_ROUND,
		.clr = {
			.type = KPLOTCTYPE_DEFAULT,
		},
	},
	.point = {
		.sz = 3,
		.radius = 5,
		.dashesz = 0,
		.clr = {
			.type = KPLOTCTYPE_DEFAULT,
		},
	},
};

static Kplotfont plotdisplay_tic_font = {
	.family = "sans",
	.sz = 12,
    .slant = CAIRO_FONT_SLANT_NORMAL,
    .weight = CAIRO_FONT_WEIGHT_NORMAL,
};

static Kplotfont plotdisplay_axis_font = {
	.family = "sans",
	.sz = 15,
    .slant = CAIRO_FONT_SLANT_NORMAL,
    .weight = CAIRO_FONT_WEIGHT_NORMAL,
};

struct _Plotdisplay {
	GtkDrawingArea parent_instance;

	/* The Plot model we draw. We hold a ref to this.
	 */
	Plot *plot;

	/* Draw in thumbnail (low detail) mode.
	 */
	gboolean thumbnail;

	// the kplot ready to draw
	Kplot *kplot;
	Kplotcfg kcfg_thumbnail;
	Kplotcfg kcfg_window;
	Kplotccfg *kccfg;
	Kplotctx kctx;
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

static gboolean
plotdisplay_build_kplot(Plotdisplay *plotdisplay)
{
	Plot *plot = plotdisplay->plot;

	Kplotcfg *kcfg = plotdisplay->thumbnail ?
		&plotdisplay->kcfg_thumbnail :
		&plotdisplay->kcfg_window;

	Kdatacfg *kdatacfg = plotdisplay->thumbnail ?
		&plotdisplay_series_datacfg_thumbnail :
		&plotdisplay_series_datacfg_window;

	printf("plotdisplay_build_kplot:\n");

	// use our scaling ... this is in config, unfortunately
	kcfg->extrema = EXTREMA_XMIN | EXTREMA_XMAX | EXTREMA_YMIN | EXTREMA_YMAX;
	kcfg->extrema_xmin = plot->xmin;
	kcfg->extrema_xmax = plot->xmax;
	kcfg->extrema_ymin = plot->ymin;
	kcfg->extrema_ymax = plot->ymax;
	kcfg->xaxislabel = plotdisplay->thumbnail && plot->xcaption ?
		NULL : plot->xcaption;
	kcfg->yaxislabel = plotdisplay->thumbnail && plot->ycaption ?
		NULL : plot->ycaption;

	g_autoptr(Kplot) kplot = kplot_alloc(kcfg);

	for (int i = 0; i < plot->columns; i++) {
		g_autoptr(Kdata) kdata = NULL;

		if (!(kdata = kdata_array_alloc(NULL, plot->rows)))
			return FALSE;

		for (int j = 0; j < plot->rows; j++)
			kdata_set(kdata, j, plot->xcolumn[i][j], plot->ycolumn[i][j]);

		kplot_attach_data(kplot, kdata,
			plot->style == PLOT_STYLE_POINT ? KPLOT_POINTS : KPLOT_LINES,
			kdatacfg);
	}

	VIPS_FREEF(kplot_free, plotdisplay->kplot);
	plotdisplay->kplot = g_steal_pointer(&kplot);

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
	if (plotdisplay->plot != plot) {
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

static double
plotdisplay_pick_xinterval(double min, double max, int width)
{
	double range = max - min;
	double magnitude = pow(10.0, ceil(log10(range)));

	if (width < 200)
		return magnitude / 2.0;
	else if (width < 400)
		return magnitude / 5.0;
	else if (width < 600)
		return magnitude / 10.0;
	else if (width < 1400)
		return magnitude / 20.0;
	else if (width < 2000)
		return magnitude / 50.0;
	else
		return magnitude / 100.0;
}

// vertical labels can be quite a bit closer
static double
plotdisplay_pick_yinterval(double min, double max, int height)
{
	double range = max - min;
	double magnitude = pow(10.0, ceil(log10(range)));

	if (height < 100)
		return magnitude / 2.0;
	else if (height < 150)
		return magnitude / 5.0;
	else if (height < 250)
		return magnitude / 10.0;
	else if (height < 450)
		return magnitude / 20.0;
	else if (height < 700)
		return magnitude / 50.0;
	else if (height < 1300)
		return magnitude / 100.0;
	else
		return magnitude / 200.0;
}

static void
plotdisplay_draw_function(GtkDrawingArea *area,
	cairo_t *cr, int width, int height, gpointer user_data)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) user_data;

#ifdef DEBUG_VERBOSE
	printf("plotdisplay_draw_function: %d x %d\n", width, height);
#endif /*DEBUG_VERBOSE*/

	if (plotdisplay->kplot) {
		// white background
		GdkRGBA white;
		gdk_rgba_parse(&white, "white");
		gdk_cairo_set_source_rgba(cr, &white);
		cairo_rectangle(cr, 0.0, 0.0, width, height);
		cairo_fill(cr);

		if (!plotdisplay->thumbnail) {
			plotdisplay->kplot->cfg.xinterval =
				plotdisplay_pick_xinterval(plotdisplay->plot->xmin,
						plotdisplay->plot->xmax, width);
			plotdisplay->kplot->cfg.yinterval =
				plotdisplay_pick_yinterval(plotdisplay->plot->ymin,
						plotdisplay->plot->ymax, height);
		}
		else {
			plotdisplay->kplot->cfg.xinterval = 0.0;
			plotdisplay->kplot->cfg.yinterval = 0.0;
		}

		// and plot
		kplot_draw_ctx(&plotdisplay->kctx,
			plotdisplay->kplot, width, height, cr);
	}
}

static void
plotdisplay_init(Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_init:\n");
#endif /*DEBUG*/

	// minimal config for thumbnail draw
	kplotcfg_defaults(&plotdisplay->kcfg_thumbnail);
	plotdisplay->kcfg_thumbnail.xinterval = 0;
	plotdisplay->kcfg_thumbnail.yinterval = 0;
	plotdisplay->kcfg_thumbnail.ticlabel = 0;
	plotdisplay->kcfg_thumbnail.marginsz = 3;
	plotdisplay->kcfg_thumbnail.ticline.len = 0;
	plotdisplay->kcfg_thumbnail.grid = 0;
	plotdisplay->kcfg_thumbnail.clrsz = VIPS_NUMBER(plotdisplay_series_rgba);
	plotdisplay->kcfg_thumbnail.clrs = plotdisplay_series_ccfg;

	// bigger for window
	kplotcfg_defaults(&plotdisplay->kcfg_window);
	plotdisplay->kcfg_window.xinterval = 0;
	plotdisplay->kcfg_window.yinterval = 0;
	plotdisplay->kcfg_window.clrsz = VIPS_NUMBER(plotdisplay_series_rgba);
	plotdisplay->kcfg_window.clrs = plotdisplay_series_ccfg;
	plotdisplay->kcfg_window.ticlabelfont = plotdisplay_tic_font;
	plotdisplay->kcfg_window.axislabelfont = plotdisplay_axis_font;

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

	int n_ccfg = VIPS_NUMBER(plotdisplay_series_rgba);
	plotdisplay_series_ccfg = VIPS_ARRAY(NULL, n_ccfg, Kplotccfg);
	for (int i = 0; i < n_ccfg; i++) {
		plotdisplay_series_ccfg[i].type = KPLOTCTYPE_RGBA;

		GdkRGBA rgba;
		gdk_rgba_parse(&rgba, plotdisplay_series_rgba[i]);
		plotdisplay_series_ccfg[i].rgba[0] = rgba.red;
		plotdisplay_series_ccfg[i].rgba[1] = rgba.green;
		plotdisplay_series_ccfg[i].rgba[2] = rgba.blue;
		plotdisplay_series_ccfg[i].rgba[3] = rgba.alpha;
	}

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

gboolean
plotdisplay_gtk_to_data(Plotdisplay *plotdisplay,
	double gtk_x, double gtk_y,
	double *data_x, double *data_y)
{
	return kplot_cairo_to_data(&plotdisplay->kctx,
		gtk_x, gtk_y, data_x, data_y);
}

