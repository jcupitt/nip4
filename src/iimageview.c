/* run the display for an image in a workspace
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

G_DEFINE_TYPE(iImageview, iimageview, GRAPHICVIEW_TYPE)

static void
iimageview_dispose(GObject *object)
{
	iImageview *iimageview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_IIMAGEVIEW(object));

	iimageview = IIMAGEVIEW(object);

#ifdef DEBUG
	printf("iimageview_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(iimageview->tilesource);
	gtk_widget_dispose_template(GTK_WIDGET(iimageview), IIMAGEVIEW_TYPE);

	G_OBJECT_CLASS(iimageview_parent_class)->dispose(object);
}

static void
iimageview_realize(GtkWidget *widget)
{
	GTK_WIDGET_CLASS(iimageview_parent_class)->realize(widget);

	/* Mark us as a symbol drag-to widget.
	 */
	set_symbol_drag_type(widget);
}

/*
GtkWidget *
iimageview_drag_window_new(int width, int height)
{
	GtkWidget *window;

	window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);
	gtk_widget_set_size_request(window, width, height);
	gtk_widget_realize(window);
	gdk_window_set_opacity(window->window, 0.5);

	return window;
}

static void
iimageview_drag_begin(GtkWidget *widget, GdkDragContext *context)
{
	iImageview *iimageview = IIMAGEVIEW(widget);
	Conversion *conv = iimageview->conv;
	GtkWidget *window;
	Imagedisplay *id;

	window = iimageview_drag_window_new(
		conv->canvas.width, conv->canvas.height);
	gtk_object_set_data_full(GTK_OBJECT(widget),
		"nip2-drag-window", window,
		(GtkDestroyNotify) gtk_widget_destroy);
	id = imagedisplay_new(conv);
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(id));
	gtk_widget_show(GTK_WIDGET(id));
	gtk_drag_set_icon_widget(context, window, -2, -2);
}

static void
iimageview_drag_end(GtkWidget *widget, GdkDragContext *context)
{
	gtk_object_set_data(GTK_OBJECT(widget),
		"nip2-drag-window", NULL);
}

static void
iimageview_drag_data_get(GtkWidget *widget, GdkDragContext *context,
	GtkSelectionData *selection_data, guint info, guint time)
{
	if (info == TARGET_SYMBOL) {
		iImageview *iimageview = IIMAGEVIEW(widget);
		iImage *iimage = IIMAGE(VOBJECT(iimageview)->iobject);
		Row *row = HEAPMODEL(iimage)->row;
		char txt[256];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		row_qualified_name_relative(main_workspaceroot->sym,
			row, &buf);
		gtk_selection_data_set(selection_data,
			gdk_atom_intern("text/symbol", FALSE), 8,
			(guchar *) vips_buf_all(&buf),
			strlen(vips_buf_all(&buf)));
	}
}

static void
iimageview_drag_data_received(GtkWidget *widget, GdkDragContext *context,
	gint x, gint y, GtkSelectionData *selection_data,
	guint info, guint time)
{
	if (info == TARGET_SYMBOL && selection_data->length > 0 &&
		selection_data->format == 8) {
		const char *from_row_path = (const char *) selection_data->data;
		iImageview *iimageview = IIMAGEVIEW(widget);
		iImage *iimage = IIMAGE(VOBJECT(iimageview)->iobject);
		Row *row = HEAPMODEL(iimage)->row;
		Row *from_row;

		printf(" seen TARGET_SYMBOL \"%s\"\n", from_row_path);

		if ((from_row = row_parse_name(main_workspaceroot->sym,
				 from_row_path)) &&
			from_row != row) {
			iText *itext = ITEXT(HEAPMODEL(iimage)->rhs->itext);
			char txt[256];
			VipsBuf buf = VIPS_BUF_STATIC(txt);

			if (row->top_row->sym)
				row_qualified_name_relative(row->top_row->sym,
					from_row, &buf);

			if (itext_set_formula(itext, vips_buf_all(&buf))) {
				itext_set_edited(itext, TRUE);
				(void) expr_dirty(row->expr,
					link_serial_new());
				workspace_set_modified(row->ws, TRUE);
				symbol_recalculate_all();
			}

			row_select(row);
		}
	}
}
 */

/* Not the same as model->edit :-( if this is a region, don't pop the region
 * edit box, pop a viewer on the image.
 */
static void
iimageview_edit(GtkWidget *parent, iImageview *iimageview)
{
	iImage *iimage = IIMAGE(VOBJECT(iimageview)->iobject);

	if (IS_IREGION(iimage) && iimage->value.ii) {
		printf("iimageview_edit: FIXME region edit\n");
		// imageview_new( iimage, parent );
	}
	else
		model_edit(parent, MODEL(iimage));
}

static void
iimageview_link(View *view, Model *model, View *parent)
{
	iImageview *iimageview = IIMAGEVIEW(view);

	Rowview *rview;

	VIEW_CLASS(iimageview_parent_class)->link(view, model, parent);
}

static void
iimageview_refresh(vObject *vobject)
{
	iImageview *iimageview = IIMAGEVIEW(vobject);
	iImage *iimage = IIMAGE(vobject->iobject);
	Row *row = HEAPMODEL(iimage)->row;
	Imageinfo *ii = iimage->value.ii;

	int w, h;
	gboolean enabled;
	double scale, offset;
	gboolean falsecolour, type;

#ifdef DEBUG
#endif /*DEBUG*/
	printf("iimageview_refresh: FIXME\n");

	if (iimageview->imagedisplay) {
		if (!iimageview->tilesource ||
			!tilesource_has_imageinfo(iimageview->tilesource, ii)) {
			VIPS_UNREF(iimageview->tilesource);
			if (ii)
				iimageview->tilesource = tilesource_new_from_imageinfo(ii);

			if (iimageview->tilesource) {
				g_object_set(iimageview->imagedisplay,
					"bestfit", TRUE,
					"tilesource", iimageview->tilesource,
					NULL);

				// image tilesources are always loaded and can start painting
				// right away
				g_object_set(iimageview->tilesource,
					"loaded", TRUE,
					"visible", TRUE,
					NULL);

				// we will need to disable visible for thumbnails that are
				// off-screen, in a closed column, or in a workspace that's
				// not at the front of the stack or performance will be
				// horrible
				printf("iimageview_refresh: FIXME ... don't set visible\n");
			}
		}
	}

	if (iimageview->label)
		set_glabel(iimageview->label, "%s", IOBJECT(iimage)->caption);

	/* Set scale/offset for the thumbnail. Use the prefs setting, or if
	 * there's a setting for this image, override with that.
	enabled = DISPLAY_CONVERSION;
	scale = row->ws->scale;
	offset = row->ws->offset;
	falsecolour = FALSE;
	type = TRUE;
	 */

	/* If the image_width has been set, a viewer must have popped down and
	 * set it, so the recorded settings must be valid.
	if (MODEL(iimage)->window_width != -1) {
		enabled = iimage->show_convert;
		scale = iimage->scale;
		offset = iimage->offset;
		falsecolour = iimage->falsecolour;
		type = iimage->type;
	}

	conversion_set_params(iimageview->conv,
		enabled, scale, offset, falsecolour, type);
	 */

	VOBJECT_CLASS(iimageview_parent_class)->refresh(vobject);
}

static Workspaceview *
iimageview_workspaceview(iImageview *iimageview)
{
	View *p;

	for (p = VIEW(iimageview); !IS_WORKSPACEVIEW(p); p = p->parent)
		;

	return WORKSPACEVIEW(p);
}

static Rhsview *
iimageview_rhsview(iImageview *iimageview)
{
	View *p;

	for (p = VIEW(iimageview); !IS_RHSVIEW(p); p = p->parent)
		;

	return RHSVIEW(p);
}

static void
iimageview_menu(GtkGestureClick *gesture,
	guint n_press, double x, double y, iImageview *iimageview)
{
	Workspaceview *wsview = iimageview_workspaceview(iimageview);
	Rhsview *rhsview = iimageview_rhsview(iimageview);

	mainwindow_set_action_view(rhsview);

	graphene_point_t iimageview_point = GRAPHENE_POINT_INIT(x, y);
	graphene_point_t wsview_point;
	if (gtk_widget_compute_point(iimageview->top, wsview->fixed,
			&iimageview_point, &wsview_point)) {
		gtk_popover_set_pointing_to(GTK_POPOVER(wsview->rowview_menu),
			&(const GdkRectangle){ wsview_point.x, wsview_point.y, 1, 1 });

		gtk_popover_popup(GTK_POPOVER(wsview->rowview_menu));
	}
}

static Rowview *
iimageview_rowview(iImageview *iimageview)
{
	View *p;

	for (p = VIEW(iimageview); !IS_ROWVIEW(p); p = p->parent)
		;

	return ROWVIEW(p);
}

static void
iimageview_click(GtkGestureClick *gesture,
	guint n_press, double x, double y, iImageview *iimageview)
{
	if (n_press == 1) {
		Rowview *rowview = iimageview_rowview(iimageview);
		Row *row = ROW(VOBJECT(rowview)->iobject);
		guint state = get_modifiers(GTK_EVENT_CONTROLLER(gesture));

		row_select_modifier(row, state);
	}
	else {
		iImage *iimage = IIMAGE(VOBJECT(iimageview)->iobject);

		model_edit(GTK_WIDGET(iimageview), MODEL(iimage));
	}
}

static void
iimageview_class_init(iImageviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("iimageview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(iImageview, top);
	BIND_VARIABLE(iImageview, imagedisplay);
	BIND_VARIABLE(iImageview, label);

	BIND_CALLBACK(iimageview_menu);
	BIND_CALLBACK(iimageview_click);

	object_class->dispose = iimageview_dispose;

	widget_class->realize = iimageview_realize;
	printf("iimageview_class_init: FIXME ... implement drag-drop\n");
	/*
	widget_class->drag_begin = iimageview_drag_begin;
	widget_class->drag_end = iimageview_drag_end;
	widget_class->drag_data_get = iimageview_drag_data_get;
	widget_class->drag_data_received = iimageview_drag_data_received;
	 */

	vobject_class->refresh = iimageview_refresh;

	view_class->link = iimageview_link;
}

/*
static void
iimageview_doubleclick_one_cb(GtkWidget *widget, GdkEvent *event,
	iImageview *iimageview)
{
	Heapmodel *heapmodel = HEAPMODEL(VOBJECT(iimageview)->iobject);
	Row *row = heapmodel->row;

	row_select_modifier(row, event->button.state);
}

static void
iimageview_doubleclick_two_cb(GtkWidget *widget, GdkEvent *event,
	iImageview *iimageview)
{
	iimageview_edit(widget, iimageview);
}

static gboolean
iimageview_filedrop(iImageview *iimageview, const char *file)
{
	iImage *iimage = IIMAGE(VOBJECT(iimageview)->iobject);
	gboolean result;

	if ((result = iimage_replace(iimage, file)))
		symbol_recalculate_all();

	return result;
}

static void
iimageview_tooltip_generate(GtkWidget *widget,
	VipsBuf *buf, iImageview *iimageview)
{
	iImage *iimage = IIMAGE(VOBJECT(iimageview)->iobject);
	Imageinfo *ii = iimage->value.ii;
	IMAGE *im = imageinfo_get(FALSE, ii);

	vips_buf_rewind(buf);
	vips_buf_appends(buf, vips_buf_all(&iimage->caption_buffer));
	if (im) {
		double size = (double) im->Ysize * VIPS_IMAGE_SIZEOF_LINE(im);

		vips_buf_appends(buf, ", ");
		vips_buf_append_size(buf, size);
		vips_buf_appendf(buf, ", %.3gx%.3g p/mm", im->Xres, im->Yres);
	}
}
 */

static void
iimageview_init(iImageview *iimageview)
{
	gtk_widget_init_template(GTK_WIDGET(iimageview));
}

View *
iimageview_new(void)
{
	iImageview *iimageview = g_object_new(IIMAGEVIEW_TYPE, NULL);

	return VIEW(iimageview);
}
