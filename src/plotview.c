/* run the display for a plotview in a workspace
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
#define DEBUG_GEO
#define DEBUG
 */

#include "nip4.h"

G_DEFINE_TYPE(Plotview, plotview, GRAPHICVIEW_TYPE)

static void
plotview_dispose(GObject *object)
{
	Plotview *plotview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_PLOTVIEW(object));

#ifdef DEBUG
	printf("plotview_disposed\n");
#endif /*DEBUG*/

	plotview = PLOTVIEW(object);

	gtk_widget_dispose_template(GTK_WIDGET(plotview), PLOTVIEW_TYPE);

	G_OBJECT_CLASS(plotview_parent_class)->dispose(object);
}

static void
plotview_refresh_tooltip(Plotview *plotview)
{
	Plot *plot = PLOT(VOBJECT(plotview)->iobject);

	char txt[1024];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	vips_buf_appends(&buf, vips_buf_all(&plot->caption_buffer));

	vips_buf_appendf(&buf, ", %s, %s",
		plot_f2c(plot->format), plot_s2c(plot->style));

	VipsImage *im;
	if ((im = plot->value.ii->image)) {
		vips_buf_appends(&buf, ", ");
		vips_buf_appendi(&buf, im);
	}

	gtk_widget_set_tooltip_text(plotview->top, vips_buf_all(&buf));
}

static void
plotview_refresh(vObject *vobject)
{
	Plotview *plotview = PLOTVIEW(vobject);
	Plot *plot = PLOT(VOBJECT(plotview)->iobject);

#ifdef DEBUG
	printf("plotview_refresh\n");
#endif /*DEBUG*/

	/* Can't refresh before model build.
	 */
	if (plot->rows == 0 ||
		plot->columns == 0)
		return;

	if (plotview->label)
		set_glabel(plotview->label, "%s", IOBJECT(plot)->caption);

	plotview_refresh_tooltip(plotview);

	g_object_set(plotview->plotdisplay, "plot", plot, NULL);

	VOBJECT_CLASS(plotview_parent_class)->refresh(vobject);
}

static void
plotview_class_init(PlotviewClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	BIND_RESOURCE("plotview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Plotview, top);
	BIND_VARIABLE(Plotview, plotdisplay);
	BIND_VARIABLE(Plotview, label);

	BIND_CALLBACK(graphicview_click);

	gobject_class->dispose = plotview_dispose;

	vobject_class->refresh = plotview_refresh;
}

static void
plotview_init(Plotview *plotview)
{
#ifdef DEBUG
	printf("plotview_init\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(plotview));
}

View *
plotview_new(void)
{
	Plotview *plotview = g_object_new(PLOTVIEW_TYPE, NULL);

	return VIEW(plotview);
}

