/* run the displays for regions on images
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

/* Verbose.
 */
#define DEBUG

/* Just trace create/destroy.
 */
#define DEBUG_MAKE

/* Trace grab/ungrab
#define DEBUG_GRAB
 */

/* Define this to trace event propogation
#define EVENT
 */

/* See paint events.
#define DEBUG_PAINT
 */

/* Define this to make region drags default to no-update during drag/resize.
#define NO_UPDATE
 */

#include "nip4.h"

G_DEFINE_TYPE(Regionview, regionview, VIEW_TYPE)

typedef void *(*regionview_rect_fn)(Regionview *, VipsRect *, void *);
typedef void (*regionview_paint_fn)(Regionview *);

/* Region border width, without shadows.
 */
static const int regionview_border_width = 2;

/* Space around text in label.
 */
static const int regionview_label_border = 5;

/* Length of crosshair bars.
 */
static const int regionview_crosshair_length = 5;

/* The center of the crosshair is also sensitive for arrows.
 */
static const int regionview_crosshair_centre = 8;

/* How close you need to get to switch the type.
 */
static const int regionview_morph_threshold = 20;

// from the imagedisplay snapshot method: draw any visible regions
static void
regionview_overlay_snapshot(Imagedisplay *imagedisplay,
	GtkSnapshot *snapshot, Regionview *regionview)
{
}

/* Stop tracking.
 */
static void
regionview_detach(Regionview *regionview)
{
	if (regionview->grabbed) {
		g_assert(regionview->imageui->grabbed == regionview);

#ifdef DEBUG_GRAB
		printf("regionview_detach: %p\n", regionview);
#endif /*DEBUG_GRAB*/

		regionview->state = REGIONVIEW_WAIT;
		regionview->grabbed = FALSE;
		regionview->imageui->grabbed = NULL;
	}
}

static void
regionview_dispose(GtkObject *object)
{
	Regionview *regionview;
	Imagedisplay *id;

#ifdef DEBUG_MAKE
	printf("regionview_dispose: %p\n", object);
#endif /*DEBUG_MAKE*/

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_REGIONVIEW(object));

	regionview = REGIONVIEW(object);

	regionview_detach(regionview);

	VIPS_FREEF(g_source_remove, regionview->dash_crawl);
	vips_buf_destroy(&regionview->caption);

	if (regionview->imageui) {
		if (regionview->imageui->regionview == regionview)
			regionview->imageui->regionview = NULL;

		regionview->imageui->regionviews =
			g_slist_remove(regionview->imageui->regionviews, regionview);

		regionview->imageui = NULL;
	}

	if (regionview->classmodel) {
		regionview->classmodel->views = g_slist_remove(
			regionview->classmodel->views, regionview);
		regionview->classmodel = NULL;
	}

	G_OBJECT_CLASS(regionview_parent_class)->dispose(object);
}

/* Compute the label geometry.
 */
static void
regionview_label_geo(Regionview *regionview)
{
	int n = regionview_label_border;
	const char *str = vips_buf_all(&regionview->caption);
	int width, height;
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout(
		GTK_WIDGET(regionview->ip->id), str);
	pango_layout_get_pixel_size(layout, &width, &height);
	g_object_unref(layout);

	regionview->label.width = width + 2 * n;
	regionview->label.height = height + 2 * n;
	regionview->ascent = 0;
}

static void
regionview_refresh_label(Regionview *regionview)
{
	if (regionview->classmodel) {
		Row *row = HEAPMODEL(regionview->classmodel)->row;

		vips_buf_rewind(&regionview->caption);
		row_qualified_name_relative(row->ws->sym, row,
			&regionview->caption);
		regionview_label_geo(regionview);
	}
}

/* Move label to try to keep it within the window, and away from the
 * selected pixels.
 */
static void
regionview_position_label(Regionview *regionview)
{
	Imagepresent *ip = regionview->ip;
	Conversion *conv = ip->id->conv;
	VipsRect *visible = &conv->visible;
	VipsRect *label = &regionview->label;
	const int b = regionview_border_width + 2;

	VipsRect dr;

	if (regionview->label_geo) {
		regionview_refresh_label(regionview);
		regionview->label_geo = FALSE;
	}

	conversion_im_to_disp_rect(conv, &regionview->area, &dr);

	switch (regionview->type) {
	case REGIONVIEW_REGION:
	case REGIONVIEW_AREA:
	case REGIONVIEW_BOX:
		if (dr.top > visible->top + label->height + b) {
			/* Space above region for label.
			 */
			label->left = dr.left - b;
			label->top = dr.top - label->height - b;
		}
		else if (dr.left > visible->left + label->width + b) {
			/* Space to left of region for label
			 */
			label->left = dr.left - label->width - b;
			label->top = dr.top - b;
		}
		else if (VIPS_RECT_RIGHT(&dr) <
			VIPS_RECT_RIGHT(visible) - label->width - b) {
			/* Space at right.
			 */
			label->left = VIPS_RECT_RIGHT(&dr) + b;
			label->top = dr.top - b;
		}
		else if (VIPS_RECT_BOTTOM(&dr) <
			VIPS_RECT_BOTTOM(visible) - label->height - b) {
			/* Space at bottom.
			 */
			label->left = dr.left - b;
			label->top = VIPS_RECT_BOTTOM(&dr) + b;
		}
		else {
			/* Inside top left.
			 */
			label->left = dr.left;
			label->top = dr.top;
		}
		break;

	case REGIONVIEW_HGUIDE:
	case REGIONVIEW_VGUIDE:
	case REGIONVIEW_MARK:
	case REGIONVIEW_ARROW:
	case REGIONVIEW_LINE:
		/* Space above?
		 */
		if (dr.top > visible->top + label->height + b / 2 + 2) {
			if (dr.left > VIPS_RECT_RIGHT(visible) -
					label->width - b / 2 - 2) {
				/* Above left.
				 */
				label->left = dr.left - b / 2 - 2 - label->width;
				label->top = dr.top - b / 2 - 2 - label->height;
			}
			else {
				/* Above right.
				 */
				label->left = dr.left + b / 2 + 2;
				label->top = dr.top - b / 2 - 2 - label->height;
			}
		}
		else if (dr.left > VIPS_RECT_RIGHT(visible) -
				label->width - b / 2 - 2) {
			/* Below left.
			 */
			label->left = dr.left - b / 2 - 2 - label->width;
			label->top = dr.top + b / 2 + 2;
		}
		else {
			/* Below right.
			 */
			label->left = dr.left + b / 2 + 2;
			label->top = dr.top + b / 2 + 2;
		}
		break;

	default:
		g_assert(FALSE);
	}
}

static VipsRect *
regionview_get_model(Regionview *regionview)
{
	Classmodel *classmodel = regionview->classmodel;
	iRegionInstance *instance;
	VipsRect *model_area;

	/* If we have a class, update from the inside of that.
	 */
	if (classmodel &&
		(instance = classmodel_get_instance(classmodel)))
		model_area = &instance->area;
	else
		model_area = regionview->model_area;

	return model_area;
}

/* Update our_area from the model. Translate to our cods too: we always have
 * x/y in 0 to xsize/ysize.
 */
static void
regionview_update_from_model(Regionview *regionview)
{
	VipsRect *model_area = regionview_get_model(regionview);

#ifdef DEBUG
	printf("regionview_update_from_model: model is %dx%d size %dx%d\n",
		model_area->left, model_area->top,
		model_area->width, model_area->height);
#endif /*DEBUG*/

	regionview->our_area = *model_area;

#ifdef DEBUG
	printf("regionview_update_from_model: set regionview to %dx%d size %dx%d\n",
		regionview->our_area.left, regionview->our_area.top,
		regionview->our_area.width, regionview->our_area.height);
#endif /*DEBUG*/
}

/* Update the model from our_area.
 */
static void
regionview_model_update(Regionview *regionview)
{
	Classmodel *classmodel = regionview->classmodel;
	VipsRect *model_area = regionview_get_model(regionview);

#ifdef DEBUG
	printf("regionview_model_update: regionview is %dx%d size %dx%d\n",
		regionview->our_area.left, regionview->our_area.top,
		regionview->our_area.width, regionview->our_area.height);
#endif /*DEBUG*/

	*model_area = regionview->our_area;

	if (classmodel) {
		classmodel_update(classmodel);

		if (CALC_RECOMP_REGION)
			symbol_recalculate_all();
	}

	/* Refresh immediately .. gives faster feedback during drag.
	 */
	vobject_refresh(VOBJECT(regionview));

#ifdef DEBUG
	printf("regionview_model_update: set model to %dx%d size %dx%d\n",
		model_area->left, model_area->top,
		model_area->width, model_area->height);
#endif /*DEBUG*/
}

/* Our model has changed ... undraw in the old position, draw in the new
 * position.
 */
static void
regionview_refresh(vObject *vobject)
{
	Regionview *regionview = REGIONVIEW(vobject);

#ifdef DEBUG
	printf("regionview_refresh1: %dx%d size %dx%d\n",
		regionview->our_area.left, regionview->our_area.top,
		regionview->our_area.width, regionview->our_area.height);
#endif /*DEBUG*/

	/* Update our_area from model.
	 */
	regionview_update_from_model(regionview);

#ifdef DEBUG
	printf("regionview_refresh2: %dx%d size %dx%d\n",
		regionview->our_area.left, regionview->our_area.top,
		regionview->our_area.width, regionview->our_area.height);
#endif /*DEBUG*/

	if (!regionview->first)
		regionview_queue_draw(regionview);
	regionview->first = FALSE;

	/* Set new position.
	 */
	regionview->area = regionview->our_area;
	regionview->last_paint_state = regionview->paint_state;
	regionview->last_type = regionview->type;

	/* Choose a new label position.
	 */
	regionview_position_label(regionview);

	/* Draw in the new place, clip against imagedisplay draw area.
	 */
	regionview_queue_draw(regionview);

	VOBJECT_CLASS(regionview_parent_class)->refresh(vobject);
}

static void
regionview_edit_cb(GtkWidget *menu, Regionview *regionview, Imagepresent *ip)
{
	model_edit(GTK_WIDGET(ip), MODEL(regionview->classmodel));
}

static void
regionview_clone_cb(GtkWidget *menu, Regionview *regionview, Imagepresent *ip)
{
	Row *row = HEAPMODEL(regionview->classmodel)->row;
	Workspace *ws = row->top_col->ws;

	if (row->top_row != row) {
		error_top(_("Can't duplicate."));
		error_sub("%s",
			_("You can only duplicate top level regions."));
		iwindow_alert(GTK_WIDGET(regionview->ip), GTK_MESSAGE_INFO);
		return;
	}

	workspace_deselect_all(ws);
	row_select(row);
	if (!workspace_selected_duplicate(ws))
		iwindow_alert(GTK_WIDGET(regionview), GTK_MESSAGE_ERROR);
	workspace_deselect_all(ws);

	symbol_recalculate_all();
}

static void
regionview_clear_edited_cb(GtkWidget *menu,
	Regionview *regionview, Imagepresent *ip)
{
	(void) icontainer_map_all(ICONTAINER(regionview->classmodel),
		(icontainer_map_fn) model_clear_edited, NULL);
	symbol_recalculate_all();
}

static void
regionview_remove_yes(iWindow *iwnd, void *client,
	iWindowNotifyFn nfn, void *sys)
{
	Regionview *regionview = REGIONVIEW(client);
	Row *row = HEAPMODEL(regionview->classmodel)->row;

	IDESTROY(row->sym);

	nfn(sys, IWINDOW_YES);
}

static void
regionview_remove_cb(GtkWidget *menu,
	Regionview *regionview, Imagepresent *ip)
{
	Row *row = HEAPMODEL(regionview->classmodel)->row;

	if (row->top_row != row) {
		error_top(_("Can't delete."));
		error_sub(_("You can only delete top level regions."));
		iwindow_alert(GTK_WIDGET(regionview->ip), GTK_MESSAGE_INFO);
		return;
	}

	box_yesno(GTK_WIDGET(ip),
		regionview_remove_yes, iwindow_true_cb, regionview,
		iwindow_notify_null, NULL,
		GTK_STOCK_DELETE,
		_("Delete Region?"),
		_("Are you sure you want to delete Region \"%s\"?"),
		vips_buf_all(&regionview->caption));
}

static void
regionview_class_init(RegionviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	GtkWidget *pane;

	object_class->dispose = regionview_dispose;

	/* Create signals.
	 */

	/* Init methods.
	 */
	vobject_class->refresh = regionview_refresh;

	/* Other init.
	pane = regionview_popup_menu = popup_build(_("Region menu"));
	popup_add_but(pane, _("_Edit"),
		POPUP_FUNC(regionview_edit_cb));
	popup_add_but(pane, STOCK_DUPLICATE,
		POPUP_FUNC(regionview_clone_cb));
	popup_add_but(pane, _("_Reset"),
		POPUP_FUNC(regionview_clear_edited_cb));
	menu_add_sep(pane);
	popup_add_but(pane, GTK_STOCK_DELETE,
		POPUP_FUNC(regionview_remove_cb));
	 */
}

static void
regionview_init(Regionview *regionview)
{
	static VipsRect empty_rect = { -1, -1, -1, -1 };

#ifdef DEBUG_MAKE
	printf("regionview_init\n");
#endif /*DEBUG_MAKE*/

	regionview->type = REGIONVIEW_MARK;
	regionview->frozen = TRUE;

	regionview->state = REGIONVIEW_WAIT;
	regionview->resize = REGIONVIEW_RESIZE_NONE;
	regionview->dx = -1;
	regionview->dy = -1;
	regionview->grabbed = FALSE;

	regionview->classmodel = NULL;
	regionview->ip = NULL;
	regionview->cntxt = NULL;
	regionview->expose_sid = 0;
	regionview->destroy_sid = 0;
	regionview->event_sid = 0;
	regionview->changed_sid = 0;
	regionview->conv_destroy_sid = 0;

	regionview->model_area = NULL;
	regionview->paint_state = GTK_STATE_NORMAL;

	regionview->area = empty_rect;
	regionview->label = empty_rect;
	regionview->ascent = 0;
	regionview->dash_offset = 0;
	regionview->dash_crawl = 0;
	regionview->last_paint_state = (GtkStateType) -1;
	regionview->last_type = (RegionviewType) -1;
	regionview->first = TRUE;
	regionview->label_geo = TRUE;

	vips_buf_init_dynamic(&regionview->caption, REGIONVIEW_LABEL_MAX);

	gtk_widget_set_name(GTK_WIDGET(regionview), "regionview_widget");
}

/* Test for rect touches rect (non-empty intersection).
 */
static void *
regionview_rect_touching(Regionview *regionview, VipsRect *a, VipsRect *b)
{
	VipsRect overlap;

	vips_rect_intersectrect(a, b, &overlap);

	if (!vips_rect_isempty(&overlap))
		return regionview;
	else
		return NULL;
}

/* Does expose rect touch the mark positioned at mark_x/mark_y. Include a big
 * grab handle in the centre of the crosshair.
 */
static gboolean
regionview_rect_touches_mark(Regionview *regionview,
	int mark_x, int mark_y, VipsRect *expose)
{
	Conversion *conv = regionview->ip->id->conv;

	VipsRect tiny;
	int x, y;

	conversion_im_to_disp(conv, mark_x, mark_y, &x, &y);
	if (regionview_crosshair_foreach(regionview, x, y,
			(regionview_rect_fn) regionview_rect_touching, expose))
		return TRUE;

	/* ... and the centre of the crosshairs.
	 */
	tiny.left = x;
	tiny.top = y;
	tiny.width = 1;
	tiny.height = 1;
	vips_rect_marginadjust(&tiny, regionview_crosshair_centre);
	if (regionview_rect_touching(regionview, &tiny, expose))
		return TRUE;

	return FALSE;
}

/* Test for rect intersects some part of region.
 */
static gboolean
regionview_rect_touches_region(Regionview *regionview, VipsRect *expose)
{
	Conversion *conv = regionview->ip->id->conv;

	VipsRect canvas_area;

	if (regionview->classmodel && regionview_rect_touching(regionview, &regionview->label, expose))
		return TRUE;

	switch (regionview->type) {
	case REGIONVIEW_REGION:
	case REGIONVIEW_AREA:
	case REGIONVIEW_BOX:
	case REGIONVIEW_LINE:
		conversion_im_to_disp_rect(conv,
			&regionview->area, &canvas_area);
		vips_rect_normalise(&canvas_area);
		if (regionview_border_foreach(regionview, &canvas_area,
				(regionview_rect_fn) regionview_rect_touching,
				expose))
			return TRUE;
		break;

	case REGIONVIEW_MARK:
		if (regionview_rect_touches_mark(regionview,
				regionview->area.left, regionview->area.top,
				expose))
			return TRUE;

		break;

	case REGIONVIEW_ARROW:
		/* Test two marks first.
		 */
		if (regionview_rect_touches_mark(regionview,
				regionview->area.left, regionview->area.top,
				expose))
			return TRUE;
		if (regionview_rect_touches_mark(regionview,
				VIPS_RECT_RIGHT(&regionview->area),
				VIPS_RECT_BOTTOM(&regionview->area),
				expose))
			return TRUE;

		/* Spot in main area too ... for the dotted line. Also avoid
		 * zero-width/height areas for h and v lines.
		 */
		conversion_im_to_disp_rect(conv,
			&regionview->area, &canvas_area);
		vips_rect_normalise(&canvas_area);
		vips_rect_marginadjust(&canvas_area, 1);
		if (regionview_rect_touching(regionview,
				&canvas_area, expose))
			return TRUE;

		break;

	case REGIONVIEW_HGUIDE:
	case REGIONVIEW_VGUIDE:
		conversion_im_to_disp_rect(conv,
			&regionview->area, &canvas_area);
		vips_rect_marginadjust(&canvas_area, 5);

		if (regionview_rect_touching(regionview,
				&canvas_area, expose))
			return TRUE;

		break;

	default:
		g_assert(FALSE);
	}

	return FALSE;
}

/* From the expose event.
 */
static void
regionview_expose(Regionview *regionview, VipsRect *expose)
{
#ifdef DEBUG_PAINT
	printf("regionview_expose: at %dx%d size %dx%d\n",
		expose->left, expose->top, expose->width, expose->height);
#endif /*DEBUG_PAINT*/

	g_assert(expose->width >= 0 && expose->height >= 0);

	/* If we've not finished init, don't paint.
	 */
	if (regionview->first)
		return;

	/* If the expose doesn't touch the region, don't bother painting.
	 */
	if (!regionview_rect_touches_region(regionview, expose))
		return;

	regionview_paint(regionview);
}

static void
regionview_model_changed_cb(Classmodel *classmodel, Regionview *regionview)
{
	vobject_refresh_queue(VOBJECT(regionview));
}

static gboolean
regionview_expose_cb(Imagedisplay *id, GdkEventExpose *event,
	Regionview *regionview)
{
	VipsRect expose;

	expose.left = event->area.x;
	expose.top = event->area.y;
	expose.width = event->area.width;
	expose.height = event->area.height;

	regionview_expose(regionview, &expose);

	return FALSE;
}

/* Test for point is in the grab area of a region border or label.
 */
static gboolean
regionview_point_in_region(Regionview *regionview, int x, int y)
{
	VipsRect r;

	r.left = x;
	r.top = y;
	r.width = 1;
	r.height = 1;
	return regionview_rect_touches_region(regionview, &r);
}

/* Given a position, find the sort of resize we should allow.
 */
static RegionviewResize
regionview_find_resize(Regionview *regionview, int x, int y)
{
	Conversion *conv = regionview->ip->id->conv;

	VipsRect canvas_area, tiny;
	int dx, dy;

	if (vips_rect_includespoint(&regionview->label, x, y))
		return REGIONVIEW_RESIZE_EDIT;

	conversion_im_to_disp_rect(conv, &regionview->area, &canvas_area);
	dx = x - canvas_area.left;
	dy = y - canvas_area.top;

	switch (regionview->type) {
	case REGIONVIEW_REGION:
		if (dx > canvas_area.width - 10) {
			if (dy > canvas_area.height - 10)
				return REGIONVIEW_RESIZE_BOTTOMRIGHT;
			else if (dy < 10)
				return REGIONVIEW_RESIZE_TOPRIGHT;
			else
				return REGIONVIEW_RESIZE_RIGHT;
		}
		else if (dx < 10) {
			if (dy > canvas_area.height - 10)
				return REGIONVIEW_RESIZE_BOTTOMLEFT;
			else if (dy < 10)
				return REGIONVIEW_RESIZE_TOPLEFT;
			else
				return REGIONVIEW_RESIZE_LEFT;
		}
		else {
			if (dy < canvas_area.height / 2)
				return REGIONVIEW_RESIZE_TOP;
			else
				return REGIONVIEW_RESIZE_BOTTOM;
		}
		break;

	case REGIONVIEW_MARK:
	case REGIONVIEW_AREA:
		return REGIONVIEW_RESIZE_MOVE;

	case REGIONVIEW_ARROW:
		tiny.left = x;
		tiny.top = y;
		tiny.width = 1;
		tiny.height = 1;
		if (regionview_crosshair_foreach(regionview,
				canvas_area.left, canvas_area.top,
				(regionview_rect_fn) regionview_rect_touching, &tiny))
			return REGIONVIEW_RESIZE_TOPLEFT;
		if (regionview_crosshair_foreach(regionview,
				VIPS_RECT_RIGHT(&canvas_area),
				VIPS_RECT_BOTTOM(&canvas_area),
				(regionview_rect_fn) regionview_rect_touching, &tiny))
			return REGIONVIEW_RESIZE_BOTTOMRIGHT;

		/* Extra tests ... allow grabs in the centre of the crosshairs
		 * too.
		 */
		tiny.left = VIPS_RECT_RIGHT(&canvas_area);
		tiny.top = VIPS_RECT_BOTTOM(&canvas_area);
		tiny.width = 1;
		tiny.height = 1;
		vips_rect_marginadjust(&tiny, regionview_crosshair_centre);
		if (vips_rect_includespoint(&tiny, x, y))
			return REGIONVIEW_RESIZE_BOTTOMRIGHT;

		tiny.left = canvas_area.left;
		tiny.top = canvas_area.top;
		tiny.width = 1;
		tiny.height = 1;
		vips_rect_marginadjust(&tiny, regionview_crosshair_centre);
		if (vips_rect_includespoint(&tiny, x, y))
			return REGIONVIEW_RESIZE_TOPLEFT;

		break;

	case REGIONVIEW_VGUIDE:
	case REGIONVIEW_HGUIDE:
		vips_rect_marginadjust(&canvas_area, 5);
		if (vips_rect_includespoint(&canvas_area, x, y))
			return REGIONVIEW_RESIZE_MOVE;

		break;

	case REGIONVIEW_BOX:
	case REGIONVIEW_LINE:
		break;

	default:
		g_assert(FALSE);
	}

	return REGIONVIEW_RESIZE_NONE;
}

/* Right button press event.
 */
static gint
regionview_right_press(Regionview *regionview, GdkEvent *ev, int x, int y)
{
	if (vips_rect_includespoint(&regionview->label, x, y)) {
		popup_show(GTK_WIDGET(regionview), ev);

		return TRUE;
	}

	return FALSE;
}

/* Get ready to track this region. See imagepresent.c.
 */
void
regionview_attach(Regionview *regionview, int x, int y)
{
	Imagepresent *ip = regionview->ip;
	Conversion *conv = ip->id->conv;
	int dx, dy;

	g_assert(!regionview->grabbed);
	g_assert(!regionview->ip->grabbed);

#ifdef DEBUG_GRAB
	printf("regionview_attach: %p\n", regionview);
#endif /*DEBUG_GRAB*/

	switch (regionview->resize) {
	case REGIONVIEW_RESIZE_NONE:
		regionview->resize = REGIONVIEW_RESIZE_BOTTOMRIGHT;
		regionview->state = REGIONVIEW_RESIZE;
		break;

	case REGIONVIEW_RESIZE_MOVE:
	case REGIONVIEW_RESIZE_EDIT:
		regionview->state = REGIONVIEW_MOVE;
		break;

	default:
		regionview->state = REGIONVIEW_RESIZE;
		break;
	}

	regionview->paint_state = GTK_STATE_ACTIVE;

	iwindow_cursor_context_set_cursor(regionview->cntxt,
		regionview_cursors[regionview->resize]);

	regionview->grabbed = TRUE;
	regionview->ip->grabbed = regionview;

	conversion_im_to_disp(conv,
		regionview->our_area.left, regionview->our_area.top, &dx, &dy);
	regionview->dx = dx - x;
	regionview->dy = dy - y;
}

/* Left button press event.
 */
static gint
regionview_left_press(Regionview *regionview, GdkEvent *ev, int x, int y)
{
	gboolean handled = FALSE;

	if (!regionview_point_in_region(regionview, x, y))
		return FALSE;

	switch (regionview->state) {
	case REGIONVIEW_WAIT:
		regionview->resize = regionview_find_resize(regionview, x, y);

		if (regionview->resize != REGIONVIEW_RESIZE_NONE) {
			regionview_attach(regionview, x, y);
			handled = TRUE;
		}

		break;

	case REGIONVIEW_MOVE:
	case REGIONVIEW_RESIZE:
		break;

	default:
		g_assert(FALSE);
	}

	return handled;
}

/* Left button release event.
 */
static gint
regionview_left_release(Regionview *regionview, GdkEvent *ev)
{
	switch (regionview->state) {
	case REGIONVIEW_WAIT:
		break;

	case REGIONVIEW_MOVE:
	case REGIONVIEW_RESIZE:
		regionview_detach(regionview);

		if (!CALC_RECOMP_REGION)
			symbol_recalculate_all();

		break;
	}

	return FALSE;
}

static void
regionview_resize_area(Regionview *regionview, int ix, int iy)
{
	Imagepresent *ip = regionview->ip;
	Conversion *conv = ip->id->conv;
	IMAGE *im = imageinfo_get(FALSE, conv->ii);
	VipsRect *our_area = &regionview->our_area;
	int th = regionview_morph_threshold / conversion_dmag(conv->mag);

	int bot = our_area->top + our_area->height;
	int ri = our_area->left + our_area->width;
	int rx = ix - our_area->left;
	int ry = iy - our_area->top;

	/* If we're not frozen, do an unconstrained resize.
	 */
	if (!regionview->frozen) {
		switch (regionview->resize) {
		case REGIONVIEW_RESIZE_RIGHT:
			our_area->width = VIPS_CLIP(-our_area->left,
				rx, im->Xsize - our_area->left);
			break;

		case REGIONVIEW_RESIZE_BOTTOM:
			our_area->height = VIPS_CLIP(-our_area->top,
				ry, im->Ysize - our_area->top);
			break;

		case REGIONVIEW_RESIZE_MOVE:
			/* Get this for POINT on create ... treat as
			 * BOTTOMRIGHT.
			 */
		case REGIONVIEW_RESIZE_BOTTOMRIGHT:
			our_area->width = VIPS_CLIP(-our_area->left,
				rx, im->Xsize - our_area->left);
			our_area->height = VIPS_CLIP(-our_area->top,
				ry, im->Ysize - our_area->top);
			break;

		case REGIONVIEW_RESIZE_LEFT:
			our_area->left = VIPS_CLIP(0, ix, im->Xsize - 1);
			our_area->width = ri - our_area->left;
			break;

		case REGIONVIEW_RESIZE_TOP:
			our_area->top = VIPS_CLIP(0, iy, im->Ysize - 1);
			our_area->height = bot - our_area->top;
			break;

		case REGIONVIEW_RESIZE_TOPLEFT:
			our_area->top = VIPS_CLIP(0, iy, im->Ysize - 1);
			our_area->left = VIPS_CLIP(0, ix, im->Xsize - 1);
			our_area->width = ri - our_area->left;
			our_area->height = bot - our_area->top;
			break;

		case REGIONVIEW_RESIZE_TOPRIGHT:
			our_area->top = VIPS_CLIP(0, iy, im->Ysize - 1);
			our_area->height = bot - our_area->top;
			our_area->width = VIPS_CLIP(-our_area->left,
				rx, im->Xsize - our_area->left);
			break;

		case REGIONVIEW_RESIZE_BOTTOMLEFT:
			our_area->left = VIPS_CLIP(0, ix, im->Xsize - 1);
			our_area->width = ri - our_area->left;
			our_area->height = VIPS_CLIP(-our_area->top,
				ry, im->Ysize - our_area->top);
			break;

		default:
			g_assert(FALSE);
		}

		if (abs(our_area->width) < th &&
			abs(our_area->height - im->Ysize) < th)
			regionview->type = REGIONVIEW_VGUIDE;
		else if (abs(our_area->height) < th &&
			abs(our_area->width - im->Xsize) < th)
			regionview->type = REGIONVIEW_HGUIDE;
		else if (abs(our_area->width) < th &&
			abs(our_area->height) < th)
			regionview->type = REGIONVIEW_MARK;
		else if (our_area->width > 0 &&
			our_area->height > 0)
			regionview->type = REGIONVIEW_REGION;
		else
			regionview->type = REGIONVIEW_ARROW;
	}
	else {
		/* We're frozen ... resize should be tightly constrained.
		 */
		switch (regionview->type) {
		case REGIONVIEW_REGION:
			switch (regionview->resize) {
			case REGIONVIEW_RESIZE_RIGHT:
				our_area->width = VIPS_CLIP(1, rx,
					im->Xsize - our_area->left);
				break;

			case REGIONVIEW_RESIZE_BOTTOM:
				our_area->height = VIPS_CLIP(1, ry,
					im->Ysize - our_area->top);
				break;

			case REGIONVIEW_RESIZE_BOTTOMRIGHT:
				our_area->width = VIPS_CLIP(1, rx,
					im->Xsize - our_area->left);
				our_area->height = VIPS_CLIP(1, ry,
					im->Ysize - our_area->top);
				break;

			case REGIONVIEW_RESIZE_TOP:
				our_area->top = VIPS_CLIP(0, iy, bot - 1);
				our_area->height = bot - our_area->top;
				break;

			case REGIONVIEW_RESIZE_LEFT:
				our_area->left = VIPS_CLIP(0, ix, ri - 1);
				our_area->width = ri - our_area->left;
				break;

			case REGIONVIEW_RESIZE_TOPLEFT:
				our_area->left = VIPS_CLIP(0, ix, ri - 1);
				our_area->width = ri - our_area->left;
				our_area->top = VIPS_CLIP(0, iy, bot - 1);
				our_area->height = bot - our_area->top;
				break;

			case REGIONVIEW_RESIZE_TOPRIGHT:
				our_area->top = VIPS_CLIP(0, iy, bot - 1);
				our_area->height = bot - our_area->top;
				our_area->width = VIPS_CLIP(1, rx,
					im->Xsize - our_area->left);
				break;

			case REGIONVIEW_RESIZE_BOTTOMLEFT:
				our_area->left = VIPS_CLIP(0, ix, ri - 1);
				our_area->width = ri - our_area->left;
				our_area->height = VIPS_CLIP(1, ry,
					im->Ysize - our_area->top);
				break;

			default:
				g_assert(FALSE);
			}
			break;

		case REGIONVIEW_ARROW:
		case REGIONVIEW_LINE:
		case REGIONVIEW_BOX:
			switch (regionview->resize) {
			case REGIONVIEW_RESIZE_TOPLEFT:
				our_area->left = VIPS_CLIP(0, ix, im->Xsize);
				our_area->width = ri - our_area->left;
				our_area->top = VIPS_CLIP(0, iy, im->Ysize);
				our_area->height = bot - our_area->top;
				break;

			case REGIONVIEW_RESIZE_BOTTOMRIGHT:
				our_area->width =
					VIPS_CLIP(-our_area->left, rx,
						im->Xsize - our_area->left);
				our_area->height =
					VIPS_CLIP(-our_area->top, ry,
						im->Ysize - our_area->top);
				break;

			default:
				g_assert(FALSE);
			}
			break;

		case REGIONVIEW_MARK:
			our_area->left = VIPS_CLIP(0, ix, im->Xsize - 1);
			our_area->top = VIPS_CLIP(0, iy, im->Ysize - 1);
			our_area->width = 0;
			our_area->height = 0;
			break;

		case REGIONVIEW_HGUIDE:
			our_area->top = VIPS_CLIP(0, iy, im->Ysize - 1);
			break;

		case REGIONVIEW_VGUIDE:
			our_area->left = VIPS_CLIP(0, ix, im->Xsize - 1);
			break;

		default:
			g_assert(FALSE);
		}
	}
}

/* Change the state.
 */
static void
regionview_set_paint_state(Regionview *regionview, GtkStateType paint_state)
{
	if (regionview->paint_state != paint_state) {
		regionview->paint_state = paint_state;
		vobject_refresh_queue(VOBJECT(regionview));
	}
}

/* A motion event while we're grabbed.
 */
static void
regionview_motion_grab(Regionview *regionview, int x, int y)
{
	Imagepresent *ip = regionview->ip;
	Imagemodel *imagemodel = ip->imagemodel;
	Conversion *conv = imagemodel->conv;
	VipsRect *visible = &imagemodel->visible;
	VipsRect *our_area = &regionview->our_area;
	VipsRect snap;

	IMAGE *im;
	int ix, iy;

#ifdef DEBUG
	printf("regionview_motion_grab:\n");
	printf("cods: %dx%d size %dx%d\n",
		our_area->left, our_area->top,
		our_area->width, our_area->height);
#endif /*DEBUG*/

	switch (regionview->state) {
	case REGIONVIEW_MOVE:
		conversion_disp_to_im(conv,
			x + regionview->dx, y + regionview->dy, &ix, &iy);
		im = imageinfo_get(FALSE, conv->ii);

		switch (regionview->type) {
		case REGIONVIEW_REGION:
		case REGIONVIEW_AREA:
			our_area->left = VIPS_CLIP(0, ix,
				im->Xsize - our_area->width);
			our_area->top = VIPS_CLIP(0, iy,
				im->Ysize - our_area->height);
			break;

		case REGIONVIEW_ARROW:
			our_area->left = VIPS_CLIP(
				VIPS_MAX(0, -our_area->width),
				ix,
				VIPS_MIN(im->Xsize - 1,
					im->Xsize - our_area->width));
			our_area->top = VIPS_CLIP(
				VIPS_MAX(0, -our_area->height),
				iy,
				VIPS_MIN(im->Ysize - 1,
					im->Ysize - our_area->height));
			break;

		case REGIONVIEW_MARK:
		case REGIONVIEW_HGUIDE:
		case REGIONVIEW_VGUIDE:
			our_area->left = VIPS_CLIP(0, ix,
				im->Xsize - our_area->width - 1);
			our_area->top = VIPS_CLIP(0, iy,
				im->Ysize - our_area->height - 1);
			break;

		case REGIONVIEW_LINE:
		case REGIONVIEW_BOX:
			our_area->left = ix;
			our_area->top = iy;
			break;

		default:
			g_assert(FALSE);
		}

		snap = *our_area;
		conversion_im_to_disp_rect(conv, &snap, &snap);
		if (imagepresent_snap_rect(ip, &snap, &snap)) {
			conversion_disp_to_im_rect(conv, &snap, &snap);
			our_area->left = snap.left;
			our_area->top = snap.top;
		}

		regionview_model_update(regionview);

		break;

	case REGIONVIEW_RESIZE:
		imagepresent_snap_point(ip, x, y, &x, &y);
		conversion_disp_to_im(conv, x, y, &ix, &iy);
		regionview_resize_area(regionview, ix, iy);

		regionview_model_update(regionview);

		break;

	default:
		break;
	}

	if (!vips_rect_includespoint(visible, x, y)) {
		int u, v;

		if (x < visible->left)
			u = -8;
		else if (x > VIPS_RECT_RIGHT(visible))
			u = 8;
		else
			u = 0;
		if (y < visible->top)
			v = -8;
		else if (y > VIPS_RECT_BOTTOM(visible))
			v = 8;
		else
			v = 0;

		imagepresent_scroll_start(regionview->ip, u, v);
	}
	else
		imagepresent_scroll_stop(regionview->ip);
}

#ifdef EVENT
static char *
resize_to_str(RegionviewResize resize)
{
	switch (resize) {
	case REGIONVIEW_RESIZE_NONE:
		return "REGIONVIEW_RESIZE_NONE";
	case REGIONVIEW_RESIZE_MOVE:
		return "REGIONVIEW_RESIZE_MOVE";
	case REGIONVIEW_RESIZE_EDIT:
		return "REGIONVIEW_RESIZE_EDIT";
	case REGIONVIEW_RESIZE_TOPLEFT:
		return "REGIONVIEW_RESIZE_TOPLEFT";
	case REGIONVIEW_RESIZE_TOP:
		return "REGIONVIEW_RESIZE_TOP";
	case REGIONVIEW_RESIZE_TOPRIGHT:
		return "REGIONVIEW_RESIZE_TOPRIGHT";
	case REGIONVIEW_RESIZE_RIGHT:
		return "REGIONVIEW_RESIZE_RIGHT";
	case REGIONVIEW_RESIZE_BOTTOMRIGHT:
		return "REGIONVIEW_RESIZE_BOTTOMRIGHT";
	case REGIONVIEW_RESIZE_BOTTOM:
		return "REGIONVIEW_RESIZE_BOTTOM";
	case REGIONVIEW_RESIZE_BOTTOMLEFT:
		return "REGIONVIEW_RESIZE_BOTTOMLEFT";
	case REGIONVIEW_RESIZE_LEFT:
		return "REGIONVIEW_RESIZE_LEFT";
	case REGIONVIEW_RESIZE_LAST:
		return "REGIONVIEW_RESIZE_LAST";

	default:
		g_assert(0);
	}
}
#endif /*EVENT*/

/* Motion event.
 */
static gint
regionview_motion(Regionview *regionview, GdkEvent *ev, int x, int y)
{
	GdkWindow *win = GTK_WIDGET(regionview->ip->id)->window;
	RegionviewResize resize;

#ifdef EVENT
	printf("regionview_motion: %p, %d x %d\n", regionview, x, y);
#endif /*EVENT*/

	/* We've got hints turned on, so we have to read the pointer.
	 */
	gdk_window_get_pointer(win, &x, &y, NULL);

	switch (regionview->state) {
	case REGIONVIEW_WAIT:
		if (regionview_point_in_region(regionview, x, y)) {
			resize = regionview_find_resize(regionview, x, y);

			iwindow_cursor_context_set_cursor(regionview->cntxt,
				regionview_cursors[resize]);
			regionview_set_paint_state(regionview,
				GTK_STATE_PRELIGHT);
		}
		else {
			iwindow_cursor_context_set_cursor(regionview->cntxt,
				IWINDOW_SHAPE_NONE);
			regionview_set_paint_state(regionview,
				GTK_STATE_NORMAL);
		}

		break;

	case REGIONVIEW_MOVE:
	case REGIONVIEW_RESIZE:
		if (regionview->grabbed)
			regionview_motion_grab(regionview, x, y);

		break;

	default:
		g_assert(FALSE);
	}

	return FALSE;
}

/* Main event loop.
 */
static gint
regionview_event_cb(GtkWidget *widget, GdkEvent *ev, Regionview *regionview)
{
	Imagepresent *ip = regionview->ip;
	Imagemodel *imagemodel = ip->imagemodel;
	gboolean handled = FALSE;

#ifdef EVENT
	if (ev->type == GDK_BUTTON_PRESS)
		printf("regionview_event: GDK_BUTTON_PRESS\n");
	if (ev->type == GDK_MOTION_NOTIFY)
		printf("regionview_event: GDK_MOTION_NOTIFY\n");
#endif /*EVENT*/

	/* Only manipulate regions if we're in SELECT mode ... don't want to
	 * drag regions while we're panning, for example. Exception ... we can
	 * drag/resize floating regions any time.
	 */
	if (imagemodel->state != IMAGEMODEL_SELECT && regionview->classmodel)
		return FALSE;

	/* If there's a regionview grabbed, only that regionview responds to
	 * events.
	 */
	if (regionview->ip->grabbed && regionview->ip->grabbed != regionview)
		return FALSE;

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		switch (ev->button.button) {
		case 1:
			handled = regionview_left_press(regionview,
				ev, ev->button.x, ev->button.y);
			break;

		case 3:
			handled = regionview_right_press(regionview,
				ev, ev->button.x, ev->button.y);
			break;

		default:
			break;
		}

		break;

	case GDK_2BUTTON_PRESS:
		switch (ev->button.button) {
		case 1:
			if (regionview->state == REGIONVIEW_MOVE &&
				regionview->resize == REGIONVIEW_RESIZE_EDIT &&
				regionview->classmodel) {
				model_edit(GTK_WIDGET(ip),
					MODEL(regionview->classmodel));
				handled = TRUE;
			}
			break;

		default:
			break;
		}

		break;

	case GDK_BUTTON_RELEASE:
		switch (ev->button.button) {
		case 1:
			handled = regionview_left_release(regionview, ev);
			break;

		default:
			break;
		}

		break;

	case GDK_MOTION_NOTIFY:
		handled = regionview_motion(regionview, ev, ev->button.x, ev->button.y);
		break;

	default:
		break;
	}

	return handled;
}

/* The conversion on our image has changed ... eg. on zoom in/out we need to
 * rethink the label position.
 */
static void
regionview_changed_cb(Model *model, Regionview *regionview)
{
#ifdef DEBUG
	printf("regionview_changed\n");
#endif /*DEBUG*/

	vobject_refresh_queue(VOBJECT(regionview));
}

/* The conversion on our image has been destroyed ... make sure we won't try
 * to disconnect when we go too.
 */
static void
regionview_conv_destroy_cb(Model *model, Regionview *regionview)
{
	regionview->changed_sid = 0;
	regionview->conv_destroy_sid = 0;
}

static void
regionview_setup(Regionview *regionview,
	Classmodel *classmodel, VipsRect *model_area, Imageui *imageui)
{
	iWindow *iwnd;

	regionview->classmodel = classmodel;
	regionview->imageui = imageui;
	regionview->model_area = model_area;
	regionview->our_area = *model_area;

	if (classmodel)
		g_signal_connect_object(G_OBJECT(classmodel), "changed",
			G_CALLBACK(regionview_model_changed_cb), regionview, 0);

	g_signal_connect_object(GTK_OBJECT(imageui->id), "snapshot",
		G_CALLBACK(regionview_overlay_snapshot), regionview);
	g_signal_connect_object(GTK_OBJECT(imageui->id), "event",
		G_CALLBACK(regionview_event_cb), regionview);

	g_signal_connect(G_OBJECT(imageui->tilesource), "changed",
		G_CALLBACK(regionview_changed_cb), regionview);
	g_signal_connect(G_OBJECT(imageui->tilesource), "destroy",
		G_CALLBACK(regionview_conv_destroy_cb), regionview);

	regionview->dash_crawl = g_timeout_add(200,
		(GSourceFunc) regionview_dash_crawl_cb, regionview);
}

Regionview *
regionview_new(Classmodel *classmodel, VipsRect *model_area, Imageui *imageui)
{
	Regionview *regionview = g_object_new(REGIONVIEW_TYPE, 0);

	regionview_setup(regionview, classmodel, model_area, imageui);

#ifdef DEBUG_MAKE
	printf("regionview_new: %dx%d size %dx%d\n",
		model_area->left, model_area->top,
		model_area->width, model_area->height);
#endif /*DEBUG_MAKE*/

	return regionview;
}

/* Type we display for each of the classes. Order is important!
 */
typedef struct {
	const char *name;
	RegionviewType type;
} RegionviewDisplay;

static RegionviewDisplay regionview_display_table[] = {
	{ CLASS_HGUIDE, REGIONVIEW_HGUIDE },
	{ CLASS_VGUIDE, REGIONVIEW_VGUIDE },
	{ CLASS_MARK, REGIONVIEW_MARK },
	{ CLASS_AREA, REGIONVIEW_AREA },
	{ CLASS_REGION, REGIONVIEW_REGION },
	{ CLASS_ARROW, REGIONVIEW_ARROW }
};

/* Look at the class we are drawing, set the display type.
 */
void
regionview_set_type(Regionview *regionview, PElement *root)
{
	gboolean result;
	int i;

	if (heap_is_class(root, &result) && result)
		for (i = 0; i < VIPS_NUMBER(regionview_display_table); i++) {
			const char *name = regionview_display_table[i].name;

			if (!heap_is_instanceof(name, root, &result))
				continue;
			if (result) {
				regionview->type =
					regionview_display_table[i].type;
				vobject_refresh_queue(VOBJECT(regionview));
				break;
			}
		}
}
