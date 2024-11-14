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
#define DEBUG_VERBOSE
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
	GtkWidget *x;
	GtkWidget *y;
	GtkWidget *values;

	/* The set of labels we make for the info bar value display.
	 */
	GSList *value_widgets;
};

G_DEFINE_TYPE(Plotwindow, plotwindow, GTK_TYPE_APPLICATION_WINDOW);

enum {
	/* Set the plot model we display.
	 */
	PROP_PLOT = 1,

};

static void
plotwindow_info_refresh(Plotwindow *plotwindow)
{
	Plot *plot = plotwindow->plot;

	/* Remove all existing children.
	 */
	GSList *p;
	for (p = plotwindow->value_widgets; p; p = p->next) {
		GtkWidget *label = GTK_WIDGET(p->data);

		gtk_box_remove(GTK_BOX(plotwindow->values), label);
	}
	VIPS_FREEF(g_slist_free, plotwindow->value_widgets);

	// limit the number of widgets ... columns can potentially get very large
	int n_children = VIPS_MIN(plot->columns, 20);

	for (int i = 0; i < n_children; i++) {
		GtkWidget *label;

		label = gtk_label_new("123");
		gtk_label_set_width_chars(GTK_LABEL(label), 10);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);
		gtk_box_append(GTK_BOX(plotwindow->values), label);
		plotwindow->value_widgets =
			g_slist_append(plotwindow->value_widgets, label);
	}
}

/* Find nearest x, display that y.
 */
static void
plotwindow_series_update(GtkWidget *label,
	Plot *plot, int column, double x, double y)
{
    double *xcolumn = plot->xcolumn[column];
    double *ycolumn = plot->ycolumn[column];

    int best_row = 0;
    double best_score = VIPS_ABS(x - xcolumn[0]);
    for (int r = 1; r < plot->rows; r++) {
        double score = VIPS_ABS(x - xcolumn[r]);

        if (score < best_score) {
            best_score = score;
            best_row = r;
        }
    }

    set_glabel(label, "%g", ycolumn[best_row]);
}

static void
plotwindow_motion(GtkEventControllerMotion *self,
	gdouble x, gdouble y, gpointer user_data)
{
	Plotwindow *plotwindow = PLOTWINDOW(user_data);
	Plotdisplay *plotdisplay = PLOTDISPLAY(plotwindow->plotdisplay);

#ifdef DEBUG_VERBOSE
	printf("plotwindow_motion: x = %g, y = %g\n", x, y);
#endif /*DEBUG_VERBOSE*/

	double data_x, data_y;
	if (plotdisplay_gtk_to_data(plotdisplay, x, y, &data_x, &data_y)) {
		Plot *plot = plotwindow->plot;
		int index = VIPS_RINT(data_x);

		set_glabel(plotwindow->x, "%9.1f", data_x);
		set_glabel(plotwindow->y, "%9.1f", data_y);

		GSList *p;
		int c;
		for (c = 0, p = plotwindow->value_widgets; p; p = p->next, c++) {
			GtkWidget *label = GTK_WIDGET(p->data);

			if (c < plot->columns &&
				index >= 0 &&
				index < plot->rows)
				plotwindow_series_update(label, plot, c, data_x, data_y);
		}
	}
}

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

		plotwindow_info_refresh(plotwindow);
	}
}

static void
plotwindow_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Plotwindow *plotwindow = (Plotwindow *) object;

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
plotwindow_export_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	//Plotwindow *plotwindow = PLOTWINDOW(user_data);

	printf("FIXME ... export plot as SVG or raster\n");
}

static void
plotwindow_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Plotwindow *plotwindow = PLOTWINDOW(user_data);

	gtk_window_destroy(GTK_WINDOW(plotwindow));
}

static GActionEntry plotwindow_entries[] = {
	{ "export", plotwindow_export_action },
	{ "close", plotwindow_close_action },
};

static void
plotwindow_init(Plotwindow *plotwindow)
{
#ifdef DEBUG
	puts("plotwindow_init");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(plotwindow));

	g_action_map_add_action_entries(G_ACTION_MAP(plotwindow),
		plotwindow_entries, G_N_ELEMENTS(plotwindow_entries),
		plotwindow);
}

static void
plotwindow_class_init(PlotwindowClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	BIND_RESOURCE("plotwindow.ui");

	BIND_VARIABLE(Plotwindow, title);
	BIND_VARIABLE(Plotwindow, subtitle);
	BIND_VARIABLE(Plotwindow, plotdisplay);
	BIND_VARIABLE(Plotwindow, x);
	BIND_VARIABLE(Plotwindow, y);
	BIND_VARIABLE(Plotwindow, values);

	BIND_CALLBACK(plotwindow_motion);

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
