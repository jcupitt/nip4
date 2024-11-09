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

static void
plotwindow_dispose(GObject *object)
{
	Plotwindow *win = PLOTWINDOW(object);

#ifdef DEBUG
	printf("plotwindow_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(win->plot);

	G_OBJECT_CLASS(plotwindow_parent_class)->dispose(object);
}

static void
plotwindow_init(Plotwindow *win)
{
#ifdef DEBUG
	puts("plotwindow_init");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(win));
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

}

Plotwindow *
plotwindow_new(App *app)
{
	return g_object_new(PLOTWINDOW_TYPE, "application", app, NULL);
}

void
plotwindow_set_plot(Plotwindow *win, Plot *plot)
{
	VIPS_UNREF(win->plot);

	win->plot = plot;
	g_object_ref(plot);

	g_object_set(win->plotdisplay, "plot", plot, NULL);
}
