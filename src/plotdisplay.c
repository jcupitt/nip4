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

// the focus colour we paint
// FIXME ... we should somehow get this from the theme, I'm not sure how
#define BORDER ((GdkRGBA){ 0.4, 0.4, 0.6, 1 })

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

	plotdisplay->paint_rect.width = VIPS_MIN(
		plotdisplay->widget_rect.width,
		plotdisplay->plot_rect.width * plotdisplay->scale);
	plotdisplay->paint_rect.height = VIPS_MIN(
		plotdisplay->widget_rect.height,
		plotdisplay->plot_rect.height * plotdisplay->scale);

	/* If we've zoomed right out, centre the plot in the window.
	 */
	plotdisplay->paint_rect.left = VIPS_MAX(0,
		(plotdisplay->widget_rect.width -
			plotdisplay->paint_rect.width) /
			2);
	plotdisplay->paint_rect.top = VIPS_MAX(0,
		(plotdisplay->widget_rect.height -
			plotdisplay->paint_rect.height) /
			2);

	plotdisplay_set_hadjustment_values(plotdisplay);
	plotdisplay_set_vadjustment_values(plotdisplay);
}

static void
plotdisplay_plot_changed(Tilecache *tilecache,
	Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_tilecache_changed:\n");
#endif /*DEBUG*/

	plotdisplay->plot_rect.width = tilecache->tilesource->display_width;
	plotdisplay->plot_rect.height = tilecache->tilesource->display_height;
	plotdisplay_layout(plotdisplay);

	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

/* Tiles have changed, but not plot geometry. Perhaps falsecolour.
 */
static void
plotdisplay_tilecache_tiles_changed(Tilecache *tilecache,
	Plotdisplay *plotdisplay)
{
#ifdef DEBUG
	printf("plotdisplay_tilecache_tiles_changed:\n");
#endif /*DEBUG*/

	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_tilecache_area_changed(Tilecache *tilecache,
	VipsRect *dirty, int z, Plotdisplay *plotdisplay)
{
#ifdef DEBUG_VERBOSE
	printf("plotdisplay_tilecache_area_changed: "
		   "at %d x %d, size %d x %d, z = %d\n",
		dirty->left, dirty->top,
		dirty->width, dirty->height,
		z);
#endif /*DEBUG_VERBOSE*/

	/* Sadly, gtk4 only has this and we can't redraw areas. Perhaps we
	 * could just regenerate part of the snapshot?
	 */
	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_set_tilesource(Plotdisplay *plotdisplay, Tilesource *tilesource)
{
	VIPS_UNREF(plotdisplay->tilecache);
	VIPS_UNREF(plotdisplay->tilesource);

	if (tilesource) {
		plotdisplay->tilesource = tilesource;
		g_object_ref(plotdisplay->tilesource);

		plotdisplay->tilecache = tilecache_new(plotdisplay->tilesource);

		g_signal_connect_object(plotdisplay->tilecache, "changed",
			G_CALLBACK(plotdisplay_tilecache_changed),
			plotdisplay, 0);
		g_signal_connect_object(plotdisplay->tilecache, "tiles-changed",
			G_CALLBACK(plotdisplay_tilecache_tiles_changed),
			plotdisplay, 0);
		g_signal_connect_object(plotdisplay->tilecache, "area-changed",
			G_CALLBACK(plotdisplay_tilecache_area_changed),
			plotdisplay, 0);

		/* Do initial change to init.
		 */
		plotdisplay_tilecache_changed(plotdisplay->tilecache, plotdisplay);
	}
}

#ifdef DEBUG
static const char *
plotdisplay_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_TILESOURCE:
		return "TILESOURCE";
		break;

	case PROP_HADJUSTMENT:
		return "HADJUSTMENT";
		break;

	case PROP_HSCROLL_POLICY:
		return "HSCROLL_POLICY";
		break;

	case PROP_VADJUSTMENT:
		return "VADJUSTMENT";
		break;

	case PROP_VSCROLL_POLICY:
		return "VSCROLL_POLICY";
		break;

	case PROP_BESTFIT:
		return "BESTFIT";
		break;

	case PROP_BACKGROUND:
		return "BACKGROUND";
		break;

	case PROP_ZOOM:
		return "ZOOM";
		break;

	case PROP_X:
		return "X";
		break;

	case PROP_Y:
		return "Y";
		break;

	case PROP_DEBUG:
		return "DEBUG";
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
	case PROP_HADJUSTMENT:
		if (plotdisplay_set_adjustment(plotdisplay,
				&plotdisplay->hadj,
				g_value_get_object(value))) {
			plotdisplay_set_hadjustment_values(plotdisplay);
			g_object_notify(G_OBJECT(plotdisplay),
				"hadjustment");
		}
		break;

	case PROP_VADJUSTMENT:
		if (plotdisplay_set_adjustment(plotdisplay,
				&plotdisplay->vadj,
				g_value_get_object(value))) {
			plotdisplay_set_vadjustment_values(plotdisplay);
			g_object_notify(G_OBJECT(plotdisplay),
				"vadjustment");
		}
		break;

	case PROP_HSCROLL_POLICY:
		if (plotdisplay->hscroll_policy !=
			g_value_get_enum(value)) {
			plotdisplay->hscroll_policy =
				g_value_get_enum(value);
			gtk_widget_queue_resize(GTK_WIDGET(plotdisplay));
			g_object_notify_by_pspec(object, pspec);
		}
		break;

	case PROP_VSCROLL_POLICY:
		if (plotdisplay->vscroll_policy !=
			g_value_get_enum(value)) {
			plotdisplay->vscroll_policy =
				g_value_get_enum(value);
			gtk_widget_queue_resize(GTK_WIDGET(plotdisplay));
			g_object_notify_by_pspec(object, pspec);
		}
		break;

	case PROP_TILESOURCE:
		plotdisplay_set_tilesource(plotdisplay,
			g_value_get_object(value));
		break;

	case PROP_BESTFIT:
		plotdisplay->bestfit = g_value_get_boolean(value);
		break;

	case PROP_BACKGROUND:
		if (plotdisplay->tilecache)
			g_object_set(plotdisplay->tilecache,
				"background", g_value_get_int(value),
				NULL);
		break;

	case PROP_ZOOM:
		g_object_set(plotdisplay, "bestfit", FALSE, NULL);
		plotdisplay_set_transform(plotdisplay,
			g_value_get_double(value),
			plotdisplay->x,
			plotdisplay->y);
		plotdisplay_layout(plotdisplay);
		gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
		break;

	case PROP_X:
		g_object_set(plotdisplay, "bestfit", FALSE, NULL);
		plotdisplay_set_transform(plotdisplay,
			plotdisplay->scale,
			g_value_get_double(value),
			plotdisplay->y);
		gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
		break;

	case PROP_Y:
		g_object_set(plotdisplay, "bestfit", FALSE, NULL);
		plotdisplay_set_transform(plotdisplay,
			plotdisplay->scale,
			plotdisplay->x,
			g_value_get_double(value));
		gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
		break;

	case PROP_DEBUG:
		plotdisplay->debug = g_value_get_boolean(value);
		gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
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
	case PROP_HADJUSTMENT:
		g_value_set_object(value, plotdisplay->hadj);
		break;

	case PROP_VADJUSTMENT:
		g_value_set_object(value, plotdisplay->vadj);
		break;

	case PROP_HSCROLL_POLICY:
		g_value_set_enum(value, plotdisplay->hscroll_policy);
		break;

	case PROP_VSCROLL_POLICY:
		g_value_set_enum(value, plotdisplay->vscroll_policy);
		break;

	case PROP_TILESOURCE:
		g_value_set_object(value, plotdisplay->tilesource);
		break;

	case PROP_BESTFIT:
		g_value_set_boolean(value, plotdisplay->bestfit);
		break;

	case PROP_BACKGROUND:
		if (plotdisplay->tilecache)
			g_object_get_property(G_OBJECT(plotdisplay->tilecache),
				"background", value);
		break;

	case PROP_ZOOM:
		g_value_set_double(value, plotdisplay->scale);
		break;

	case PROP_X:
		g_value_set_double(value, plotdisplay->x);
		break;

	case PROP_Y:
		g_value_set_double(value, plotdisplay->y);
		break;

	case PROP_DEBUG:
		g_value_set_boolean(value, plotdisplay->debug);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
plotdisplay_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
	Plotdisplay *plotdisplay = PLOTDISPLAY(widget);

#ifdef DEBUG
	printf("plotdisplay_snapshot:\n");
#endif /*DEBUG*/

	/* Clip to the widget area, or we may paint over the display control
	 * bar.
	 */
	gtk_snapshot_push_clip(snapshot,
		&GRAPHENE_RECT_INIT(0, 0,
			gtk_widget_get_width(widget), gtk_widget_get_height(widget)));

	if (plotdisplay->tilecache &&
		plotdisplay->tilecache->tiles)
		tilecache_snapshot(plotdisplay->tilecache, snapshot,
			plotdisplay->scale, plotdisplay->x, plotdisplay->y,
			&plotdisplay->paint_rect,
			plotdisplay->debug);

	// draw any overlays
	plotdisplay_overlay_snapshot(plotdisplay, snapshot);

	gtk_snapshot_pop(snapshot);

	/* It's unclear how to do this :( maybe we're supposed to get the base
	 * widget class to do it? Draw it ourselves for now.
	 */
	if (gtk_widget_has_focus(widget)) {
		GskRoundedRect outline;

		gsk_rounded_rect_init_from_rect(&outline,
			&GRAPHENE_RECT_INIT(
				3,
				3,
				gtk_widget_get_width(widget) - 6,
				gtk_widget_get_height(widget) - 6),
			5);

		gtk_snapshot_append_border(snapshot,
			&outline,
			(float[4]){ 2, 2, 2, 2 },
			(GdkRGBA[4]){ BORDER, BORDER, BORDER, BORDER });
	}
}

static void
plotdisplay_resize(GtkWidget *widget, int width, int height)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) widget;

#ifdef DEBUG
	printf("plotdisplay_resize: %d x %d\n", width, height);
#endif /*DEBUG*/

	plotdisplay_layout(plotdisplay);

	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_focus_notify(GtkEventControllerFocus *focus,
	GParamSpec *pspec, gpointer user_data)
{
	Plotdisplay *plotdisplay = (Plotdisplay *) user_data;

#ifdef DEBUG
	printf("plotdisplay_focus_notify: %s\n", pspec->name);
#endif /*DEBUG*/

	gtk_widget_queue_draw(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_click(GtkEventController *controller,
	int n_press, double x, double y, Plotdisplay *plotdisplay)
{
	gtk_widget_grab_focus(GTK_WIDGET(plotdisplay));
}

static void
plotdisplay_init(Plotdisplay *plotdisplay)
{
	GtkEventController *controller;

#ifdef DEBUG
	printf("plotdisplay_init:\n");
#endif /*DEBUG*/

	plotdisplay->scale = 1;

	g_signal_connect(GTK_DRAWING_AREA(plotdisplay), "resize",
		G_CALLBACK(plotdisplay_resize), NULL);

	gtk_widget_set_focusable(GTK_WIDGET(plotdisplay), TRUE);
	controller = gtk_event_controller_focus_new();
	gtk_widget_add_controller(GTK_WIDGET(plotdisplay), controller);
	g_signal_connect(controller, "notify::is-focus",
		G_CALLBACK(plotdisplay_focus_notify), plotdisplay);

	controller = GTK_EVENT_CONTROLLER(gtk_gesture_click_new());
	g_signal_connect(controller, "pressed",
		G_CALLBACK(plotdisplay_click), plotdisplay);
	gtk_widget_add_controller(GTK_WIDGET(plotdisplay), controller);

	plotdisplay->bestfit = TRUE;
}

static void
plotdisplay_class_init(PlotdisplayClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

#ifdef DEBUG
	printf("plotdisplay_class_init:\n");
#endif /*DEBUG*/

	gobject_class->dispose = plotdisplay_dispose;
	gobject_class->set_property = plotdisplay_set_property;
	gobject_class->get_property = plotdisplay_get_property;

	widget_class->snapshot = plotdisplay_snapshot;

	g_object_class_install_property(gobject_class, PROP_TILESOURCE,
		g_param_spec_object("tilesource",
			_("Tile source"),
			_("The tile source to be displayed"),
			TILESOURCE_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BESTFIT,
		g_param_spec_boolean("bestfit",
			_("Bestfit"),
			_("Shrink on first display"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BACKGROUND,
		g_param_spec_int("background",
			_("Background"),
			_("Background mode"),
			0, TILECACHE_BACKGROUND_LAST - 1,
			TILECACHE_BACKGROUND_CHECKERBOARD,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ZOOM,
		g_param_spec_double("zoom",
			_("Zoom"),
			_("Zoom of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_X,
		g_param_spec_double("x",
			_("x"),
			_("Horizontal position of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_Y,
		g_param_spec_double("y",
			_("y"),
			_("Vertical position of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_DEBUG,
		g_param_spec_boolean("debug",
			_("Debug"),
			_("Render snapshot in debug mode"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_override_property(gobject_class,
		PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property(gobject_class,
		PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property(gobject_class,
		PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property(gobject_class,
		PROP_VSCROLL_POLICY, "vscroll-policy");

	plotdisplay_signals[SIG_CHANGED] = g_signal_new("changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	plotdisplay_signals[SIG_SNAPSHOT] = g_signal_new("snapshot",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		GTK_TYPE_SNAPSHOT);
}

Plotdisplay *
plotdisplay_new(Tilesource *tilesource)
{
	Plotdisplay *plotdisplay;

#ifdef DEBUG
	printf("plotdisplay_new:\n");
#endif /*DEBUG*/

	plotdisplay = g_object_new(plotdisplay_get_type(),
		"tilesource", tilesource,
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
	*x_gtk = x_plot * plotdisplay->scale -
		plotdisplay->x + plotdisplay->paint_rect.left;
	*y_gtk = y_plot * plotdisplay->scale -
		plotdisplay->y + plotdisplay->paint_rect.top;
}

void
plotdisplay_gtk_to_plot(Plotdisplay *plotdisplay,
	double x_gtk, double y_gtk, double *x_plot, double *y_plot)
{
	*x_plot = (plotdisplay->x +
				   x_gtk - plotdisplay->paint_rect.left) /
		plotdisplay->scale;
	*y_plot = (plotdisplay->y +
				   y_gtk - plotdisplay->paint_rect.top) /
		plotdisplay->scale;

	*x_plot = VIPS_CLIP(0, *x_plot, plotdisplay->plot_rect.width - 1);
	*y_plot = VIPS_CLIP(0, *y_plot, plotdisplay->plot_rect.height - 1);
}
