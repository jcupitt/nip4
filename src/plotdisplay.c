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

/* Everything we need to be able to plot a graph.
 */
typedef struct _Drawstate {
	Kplot *kplot;
	Kplotcfg kcfg_thumbnail;
	Kplotcfg kcfg_window;
	Kplotccfg *kccfg;
	Kplotctx kctx;
} Drawstate;

struct _Plotdisplay {
	GtkDrawingArea parent_instance;

	// a weakref to the model we display
	Plot *plot;

	/* Draw in thumbnail (low detail) mode.
	 */
	gboolean thumbnail;

	Drawstate state;

	guint plot_changed_sid;
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
drawstate_free(Drawstate *state)
{
	VIPS_FREE(state->kccfg);
	VIPS_FREEF(kplot_free, state->kplot);
}

static gboolean
drawstate_build_kplot(Drawstate *state, Plot *plot, gboolean thumbnail)
{
	Kplotcfg *kcfg = thumbnail ? &state->kcfg_thumbnail : &state->kcfg_window;

	Kdatacfg *kdatacfg = thumbnail ?
		&plotdisplay_series_datacfg_thumbnail :
		&plotdisplay_series_datacfg_window;

	// use our scaling ... this is in config, unfortunately
	kcfg->extrema = EXTREMA_XMIN | EXTREMA_XMAX | EXTREMA_YMIN | EXTREMA_YMAX;
	kcfg->extrema_xmin = plot->xmin;
	kcfg->extrema_xmax = plot->xmax;
	kcfg->extrema_ymin = plot->ymin;
	kcfg->extrema_ymax = plot->ymax;
	kcfg->xaxislabel = thumbnail && plot->xcaption ? NULL : plot->xcaption;
	kcfg->yaxislabel = thumbnail && plot->ycaption ? NULL : plot->ycaption;

	g_autoptr(Kplot) kplot = kplot_alloc(kcfg);

	for (int i = 0; i < plot->columns; i++) {
		g_autoptr(Kdata) kdata = NULL;

		if (!(kdata = kdata_array_alloc(NULL, plot->rows))) {
			return FALSE;
		}

		for (int j = 0; j < plot->rows; j++)
			kdata_set(kdata, j, plot->xcolumn[i][j], plot->ycolumn[i][j]);

		kplot_attach_data(kplot, kdata,
			plot->style == PLOT_STYLE_POINT ? KPLOT_POINTS : KPLOT_LINES,
			kdatacfg);
	}

	VIPS_FREEF(kplot_free, state->kplot);
	state->kplot = g_steal_pointer(&kplot);

	return TRUE;
}

static double
drawstate_pick_interval(double min, double max, int width)
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

static void
drawstate_draw(Drawstate *state,
	cairo_t *cr, int width, int height, gboolean thumbnail, Plot *plot)
{
	// white background
	GdkRGBA white;
	gdk_rgba_parse(&white, "white");
	gdk_cairo_set_source_rgba(cr, &white);
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_fill(cr);

	if (thumbnail) {
		state->kplot->cfg.xinterval = 0.0;
		state->kplot->cfg.yinterval = 0.0;
	}
	else {
		state->kplot->cfg.xinterval =
			drawstate_pick_interval(plot->xmin, plot->xmax, width);
		state->kplot->cfg.yinterval =
			drawstate_pick_interval(plot->ymin, plot->ymax, height);
	}

	// and plot
	kplot_draw_ctx(&state->kctx, state->kplot, width, height, cr);
}

static void
drawstate_init(Drawstate *state)
{
	// minimal config for thumbnail draw
	kplotcfg_defaults(&state->kcfg_thumbnail);
	state->kcfg_thumbnail.xinterval = 0;
	state->kcfg_thumbnail.yinterval = 0;
	state->kcfg_thumbnail.ticlabel = 0;
	state->kcfg_thumbnail.marginsz = 3;
	state->kcfg_thumbnail.ticline.len = 0;
	state->kcfg_thumbnail.grid = 0;
	state->kcfg_thumbnail.clrsz = VIPS_NUMBER(plotdisplay_series_rgba);
	state->kcfg_thumbnail.clrs = plotdisplay_series_ccfg;

	// bigger for window
	kplotcfg_defaults(&state->kcfg_window);
	state->kcfg_window.xinterval = 0;
	state->kcfg_window.yinterval = 0;
	state->kcfg_window.clrsz = VIPS_NUMBER(plotdisplay_series_rgba);
	state->kcfg_window.clrs = plotdisplay_series_ccfg;
	state->kcfg_window.ticlabelfont = plotdisplay_tic_font;
	state->kcfg_window.axislabelfont = plotdisplay_axis_font;

	state->kplot = NULL;
}

static void
plotdisplay_dispose(GObject *object)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) object;

#ifdef DEBUG
	printf("plotdisplay_dispose:\n");
#endif /*DEBUG*/

	FREESID(plotdisplay->plot_changed_sid, plotdisplay->plot);
	drawstate_free(&plotdisplay->state);

	G_OBJECT_CLASS(plotdisplay_parent_class)->dispose(object);
}

static void
plotdisplay_plot_changed(Plot *plot, Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_plot_changed:\n");
#endif /*DEBUG*/

	if (plotdisplay->plot)
		drawstate_build_kplot(&plotdisplay->state,
			plotdisplay->plot, plotdisplay->thumbnail);

	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_set_plot(Plotdisplay *plotdisplay, Plot *plot)
{
	if (plotdisplay->plot != plot) {
		FREESID(plotdisplay->plot_changed_sid, plotdisplay->plot);

		plotdisplay->plot = plot;

		if (plot)
			plotdisplay->plot_changed_sid = g_signal_connect(plot, "changed",
				G_CALLBACK(plotdisplay_plot_changed),
				plotdisplay);

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
plotdisplay_draw_function(GtkDrawingArea *area,
	cairo_t *cr, int width, int height, gpointer user_data)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) user_data;

#ifdef DEBUG_VERBOSE
	printf("plotdisplay_draw_function: %d x %d\n", width, height);
#endif /*DEBUG_VERBOSE*/

	if (plotdisplay->state.kplot)
		drawstate_draw(&plotdisplay->state,
			cr, width, height, plotdisplay->thumbnail, plotdisplay->plot);
}

static void
plotdisplay_init(Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_init:\n");
#endif /*DEBUG*/

	drawstate_init(&plotdisplay->state);
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
	return kplot_cairo_to_data(&plotdisplay->state.kctx,
		gtk_x, gtk_y, data_x, data_y);
}

static void
premultiplied_bgra2rgba(guint32 *restrict p, int n)
{
    int x;

    for (x = 0; x < n; x++) {
        guint32 bgra = GUINT32_FROM_BE(p[x]);
        guint8 a = bgra & 0xff;

        guint32 rgba;

        if (a == 0 ||
            a == 255)
            rgba =
                (bgra & 0x00ff00ff) |
                (bgra & 0x0000ff00) << 16 |
                (bgra & 0xff000000) >> 16;
        else
            /* Undo premultiplication.
             */
            rgba =
                ((255 * ((bgra >> 8) & 0xff) / a) << 24) |
                ((255 * ((bgra >> 16) & 0xff) / a) << 16) |
                ((255 * ((bgra >> 24) & 0xff) / a) << 8) |
                a;

        p[x] = GUINT32_TO_BE(rgba);
    }
}

Imageinfo *
plotdisplay_to_image(Plot *plot, Reduce *rc, int width, int height)
{
	g_autoptr(VipsImage) image = NULL;

	if (!(image = vips_image_new_memory())) {
		error_vips_all();
		return NULL;
	}
	vips_image_init_fields(image, width, height, 4,
		VIPS_FORMAT_UCHAR, VIPS_CODING_NONE,
		VIPS_INTERPRETATION_sRGB, 1.0, 1.0);
	if (vips_image_write_prepare(image)) {
		error_vips_all();
		return NULL;
	}

	g_autoptr(cairo_surface_t) surface = NULL;
	surface = cairo_image_surface_create_for_data(VIPS_IMAGE_ADDR(image, 0, 0),
        CAIRO_FORMAT_ARGB32, width, height, VIPS_IMAGE_SIZEOF_LINE(image));
	if (!surface) {
		error_vips_all();
		return NULL;
	}

	g_autoptr(cairo_t) cr = cairo_create(surface);
	if (!cr) {
		error_vips_all();
		return NULL;
	}

	Drawstate state;
	drawstate_init(&state);
	drawstate_build_kplot(&state, plot, FALSE);
	drawstate_draw(&state, cr, width, height, FALSE, plot);
	drawstate_free(&state);

	// cairo writes BGRA, we want RGBA
	for (int y = 0; y < image->Ysize; y++)
		premultiplied_bgra2rgba((guint32 *) VIPS_IMAGE_ADDR(image, 0, y),
			image->Xsize);

	Imageinfo *ii;
    if (!(ii = imageinfo_new(main_imageinfogroup, rc->heap, image, NULL)))
		return NULL;
	image = NULL;

    return ii;
}
