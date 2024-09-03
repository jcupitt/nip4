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
#define DEBUG
 */

/* Just trace create/destroy.
#define DEBUG_MAKE
 */

#include "nip4.h"

G_DEFINE_TYPE(Regionview, regionview, VIEW_TYPE)

GdkRGBA regionview_border = { 0.9, 1.0, 0.9, 1 };
GdkRGBA regionview_shadow = { 0.2, 0.2, 0.2, 0.5 };
int regionview_line_width = 5;
int regionview_corner_radius = 4;
int regionview_label_margin = 5;

static PangoContext *regionview_context = NULL;
static PangoFontMap *regionview_fontmap = NULL;

typedef void *(*regionview_rect_fn)(Regionview *, VipsRect *, void *);
typedef void (*regionview_paint_fn)(Regionview *);

static void
regionview_dispose(GObject *object)
{
	Regionview *regionview;

#ifdef DEBUG_MAKE
	printf("regionview_dispose: %p\n", object);
#endif /*DEBUG_MAKE*/

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_REGIONVIEW(object));

	regionview = REGIONVIEW(object);

	VIPS_UNREF(regionview->layout);
	vips_buf_destroy(&regionview->caption);

	if (regionview->classmodel) {
		g_assert(g_slist_find(regionview->classmodel->views, regionview));
		regionview->classmodel->views = g_slist_remove(
			regionview->classmodel->views, regionview);
		regionview->classmodel = NULL;
	}

	G_OBJECT_CLASS(regionview_parent_class)->dispose(object);
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

/* Type we display for each of the classes. Order is important!
 */
typedef struct {
	const char *name;
	RegionviewType type;
} RegionviewDisplay;

static RegionviewDisplay regionview_display_table[] = {
	{ CLASS_MARK, REGIONVIEW_MARK },
	{ CLASS_REGION, REGIONVIEW_REGION },
	{ CLASS_ARROW, REGIONVIEW_ARROW }
};

/* Look at the class we are drawing, set the display type.
 */
static void
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
				regionview->type = regionview_display_table[i].type;
				break;
			}
		}
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

	if (regionview->classmodel) {
		Row *row = HEAPMODEL(regionview->classmodel)->row;

		if (row->expr) {
			PElement *root = &row->expr->root;

			regionview_set_type(regionview, root);
			vips_buf_rewind(&regionview->caption);
			row_qualified_name_relative(row->ws->sym,
				row, &regionview->caption);
		}
	}

	VIPS_UNREF(regionview->layout);
	regionview->layout = pango_layout_new(regionview_context);
	g_autoptr(PangoFontDescription) font =
		pango_font_description_from_string("sans bold 12");
	pango_layout_set_font_description(regionview->layout, font);
	pango_layout_set_markup(regionview->layout,
		vips_buf_all(&regionview->caption), -1);

	PangoRectangle logical_rect;
	pango_layout_get_pixel_extents(regionview->layout,
		&regionview->ink_rect, &logical_rect);
}

/* Update the model from our_area.
 */
void
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

#ifdef DEBUG
	printf("regionview_model_update: set model to %dx%d size %dx%d\n",
		model_area->left, model_area->top,
		model_area->width, model_area->height);
#endif /*DEBUG*/
}

static void
regionview_draw_label_shadow(Regionview *regionview, GtkSnapshot *snapshot)
{
	if (regionview->classmodel) {
		GskRoundedRect label;
		gsk_rounded_rect_init_from_rect(&label,
			&GRAPHENE_RECT_INIT(
				regionview->label.left,
				regionview->label.top,
				regionview->label.width,
				regionview->label.height),
			regionview_corner_radius);

		// label frame shadow
		float shadow_offset = regionview_line_width / 2.0;
		gtk_snapshot_append_outset_shadow(snapshot, &label, &regionview_shadow,
			shadow_offset, shadow_offset,
			1, regionview_corner_radius);
	}
}

static void
regionview_draw_label(Regionview *regionview, GtkSnapshot *snapshot)
{
	if (regionview->classmodel) {
		GskRoundedRect label;
		gsk_rounded_rect_init_from_rect(&label,
			&GRAPHENE_RECT_INIT(
				regionview->label.left,
				regionview->label.top,
				regionview->label.width,
				regionview->label.height),
			regionview_corner_radius);

		float label_line = label.bounds.size.height / 2.0;
		gtk_snapshot_append_border(snapshot,
			&label,
			((float[4]){ label_line, label_line, label_line, label_line }),
			((GdkRGBA[4]){ regionview_border, regionview_border,
				regionview_border, regionview_border }));

		gtk_snapshot_save(snapshot);

		graphene_point_t p = GRAPHENE_POINT_INIT(
			label.bounds.origin.x + regionview_label_margin,
			label.bounds.origin.y - regionview_line_width);
		gtk_snapshot_translate(snapshot, &p);

		gtk_snapshot_append_layout(snapshot, regionview->layout,
			&((GdkRGBA){ 0, 0, 0, 1 }));

		gtk_snapshot_restore(snapshot);
	}
}

static void
regionview_draw_init_region_label(Regionview *regionview)
{
	regionview->label.left = regionview->frame.left - regionview_line_width;
	regionview->label.top = regionview->frame.top -
		2 * regionview_label_margin - regionview->ink_rect.height,
	regionview->label.width = regionview->ink_rect.width +
		2 * regionview_label_margin;
	regionview->label.height = regionview->ink_rect.height +
		2 * regionview_label_margin;
}

static void
regionview_draw_region(Regionview *regionview, GtkSnapshot *snapshot)
{
	Imageui *imageui = regionview->imageui;
	double zoom = imageui_get_zoom(imageui);

	// main frame
	double left;
	double top;
	imageui_image_to_gtk(imageui,
		regionview->draw_area.left, regionview->draw_area.top, &left, &top);
	regionview->frame.left = left;
	regionview->frame.top = top;
	regionview->frame.width = regionview->draw_area.width * zoom;
	regionview->frame.height = regionview->draw_area.height * zoom;

	GskRoundedRect frame;
	gsk_rounded_rect_init_from_rect(&frame,
		&GRAPHENE_RECT_INIT(
			regionview->frame.left,
			regionview->frame.top,
			regionview->frame.width,
			regionview->frame.height),
		0);

	regionview_draw_init_region_label(regionview);

	// bottom and right frame shadown
	float shadow_offset = regionview_line_width / 2.0;
	gtk_snapshot_append_outset_shadow(snapshot, &frame, &regionview_shadow,
		2 * shadow_offset, 2 * shadow_offset,
		3, regionview_corner_radius);

	// top and left frame shadown
	gtk_snapshot_append_inset_shadow(snapshot, &frame, &regionview_shadow,
		shadow_offset, shadow_offset,
		1, regionview_corner_radius);

	regionview_draw_label_shadow(regionview, snapshot);

	// border append draws *within* the rect, so we must step out
	gsk_rounded_rect_shrink(&frame,
		-regionview_line_width, -regionview_line_width,
		-regionview_line_width, -regionview_line_width);
	gtk_snapshot_append_border(snapshot,
		&frame,
		((float[4]){ regionview_line_width, regionview_line_width,
			regionview_line_width, regionview_line_width }),
		((GdkRGBA[4]){ regionview_border, regionview_border,
			regionview_border, regionview_border }));

	regionview_draw_label(regionview, snapshot);
}

static void
regionview_draw_init_mark_label(Regionview *regionview)
{
	regionview->label.width = regionview->ink_rect.width +
		2 * regionview_label_margin;
	regionview->label.height = regionview->ink_rect.height +
		2 * regionview_label_margin;
	regionview->label.left = regionview->frame.left -
		regionview->label.width - 1.5 * regionview_line_width;
	regionview->label.top = regionview->frame.top -
		regionview->label.height - 1.5 * regionview_line_width;
}

// position the four marks we draw
static void
regionview_draw_init_mark_ticks(Regionview *regionview, VipsRect mark[4],
	int x, int y)
{
	// north
	mark[0] = (VipsRect){
		x - 0.5 * regionview_line_width, y - 3.5 * regionview_line_width,
		regionview_line_width, 2 * regionview_line_width
	};

	// south
	mark[1] = (VipsRect){
		x - 0.5 * regionview_line_width, y + 1.5 * regionview_line_width,
		regionview_line_width, 2 * regionview_line_width
	};

	// east
	mark[2] = (VipsRect){
		x + 1.5 * regionview_line_width, y - 0.5 * regionview_line_width,
		2 * regionview_line_width, regionview_line_width
	};

	// west
	mark[3] = (VipsRect){
		x - 3.5 * regionview_line_width, y - 0.5 * regionview_line_width,
		2 * regionview_line_width, regionview_line_width
	};
}

static void
regionview_draw_crosshair(Regionview *regionview, GtkSnapshot *snapshot,
	int x, int y)
{
	VipsRect mark[4];
	regionview_draw_init_mark_ticks(regionview, mark, x, y);

	GskRoundedRect rrmark[4];
	for (int i = 0; i < 4; i++)
		gsk_rounded_rect_init_from_rect(&rrmark[i],
			&GRAPHENE_RECT_INIT(mark[i].left, mark[i].top,
				mark[i].width, mark[i].height),
			0);

	// mark shadows
	float mark_width = regionview_line_width / 2.0;
	for (int i = 0; i < 4; i++)
		gtk_snapshot_append_outset_shadow(snapshot, &rrmark[i],
			&regionview_shadow,
			mark_width, mark_width,
			1, regionview_corner_radius);

	// draw marks
	for (int i = 0; i < 4; i++)
		gtk_snapshot_append_border(snapshot, &rrmark[i],
			((float[4]){ mark_width, mark_width,
				mark_width, mark_width }),
			((GdkRGBA[4]){ regionview_border, regionview_border,
				regionview_border, regionview_border }));
}

static void
regionview_draw_mark(Regionview *regionview, GtkSnapshot *snapshot)
{
	Imageui *imageui = regionview->imageui;

	// point we mark
	double left;
	double top;
	imageui_image_to_gtk(imageui,
		regionview->draw_area.left, regionview->draw_area.top, &left, &top);
	regionview->frame.left = left;
	regionview->frame.top = top;
	regionview->frame.width = 0;
	regionview->frame.height = 0;

	regionview_draw_init_mark_label(regionview);

	regionview_draw_crosshair(regionview, snapshot,
		regionview->frame.left, regionview->frame.top);

	regionview_draw_label_shadow(regionview, snapshot);

	regionview_draw_label(regionview, snapshot);
}

static void
regionview_draw_arrow(Regionview *regionview, GtkSnapshot *snapshot)
{
	Imageui *imageui = regionview->imageui;
	double zoom = imageui_get_zoom(imageui);

	// two points to mark
	double left;
	double top;
	imageui_image_to_gtk(imageui,
		regionview->draw_area.left, regionview->draw_area.top, &left, &top);
	regionview->frame.left = left;
	regionview->frame.top = top;
	regionview->frame.width = regionview->draw_area.width * zoom;
	regionview->frame.height = regionview->draw_area.height * zoom;

	regionview_draw_init_mark_label(regionview);

	regionview_draw_crosshair(regionview, snapshot,
		regionview->frame.left, regionview->frame.top);
	regionview_draw_crosshair(regionview, snapshot,
		VIPS_RECT_RIGHT(&regionview->frame),
		VIPS_RECT_BOTTOM(&regionview->frame));

	regionview_draw_label_shadow(regionview, snapshot);

	regionview_draw_label(regionview, snapshot);

	/* Stroke a connecting line.
	 */
	GskPathBuilder *builder;
	builder = gsk_path_builder_new();
	gsk_path_builder_move_to(builder,
		regionview->frame.left, regionview->frame.top);
	gsk_path_builder_line_to(builder,
		VIPS_RECT_RIGHT(&regionview->frame),
		VIPS_RECT_BOTTOM(&regionview->frame));
	g_autoptr(GskPath) path = gsk_path_builder_free_to_path(builder);

	GskStroke *stroke = gsk_stroke_new(3);
	gsk_stroke_set_dash(stroke, (float[1]){ 10 }, 1);
	gtk_snapshot_append_stroke(snapshot, path, stroke, &regionview_border);
	gsk_stroke_free(stroke);

	stroke = gsk_stroke_new(3);
	gsk_stroke_set_dash(stroke, (float[1]){ 10 }, 1);
	gsk_stroke_set_dash_offset(stroke, 10);
	gtk_snapshot_append_stroke(snapshot, path, stroke, &regionview_shadow);
	gsk_stroke_free(stroke);
}

// called from imageui for "snapshot" on imagedisplay
void
regionview_draw(Regionview *regionview, GtkSnapshot *snapshot)
{
#ifdef DEBUG_MAKE
	printf("regionview_draw:\n");
#endif /*DEBUG_MAKE*/

	switch (regionview->draw_type) {
	case REGIONVIEW_REGION:
	case REGIONVIEW_AREA:
		regionview_draw_region(regionview, snapshot);
		break;

	case REGIONVIEW_MARK:
		regionview_draw_mark(regionview, snapshot);
		break;

	case REGIONVIEW_ARROW:
		regionview_draw_arrow(regionview, snapshot);
		break;

		/*
	REGIONVIEW_HGUIDE,			// width == image width, height == 0
	REGIONVIEW_VGUIDE,			// width == 0, height == image height
	REGIONVIEW_LINE,			// floating dashed line for paintbox
	REGIONVIEW_BOX				// floating dashed box for paintbox
		 */

	default:
		g_assert_not_reached();
	}
}

// (x, y) in gtk cods
RegionviewResize
regionview_hit(Regionview *regionview, int x, int y)
{
	int margin = 3 * regionview_line_width;

	printf("regionview_hit: %d, %d\n", x, y);
	printf("\tleft = %d, top = %d, width = %d, height = %d\n",
		regionview->label.left, regionview->label.top,
		regionview->label.width, regionview->label.height);

	/* In the label? We take the bottom chunk off the label so that top-left
	 * resize works.
	 */
	VipsRect smaller = regionview->label;
	smaller.height -= 2 * regionview_line_width;
	if (vips_rect_includespoint(&smaller, x, y))
		return REGIONVIEW_RESIZE_MOVE;

	/* Grab on corners first, since they must override edges.
	 */
	VipsRect corner;
	switch (regionview->type) {
	case REGIONVIEW_REGION:
	case REGIONVIEW_AREA:
		// bottom-right first, since that's the most useful for small regions
		corner.left = VIPS_RECT_RIGHT(&regionview->frame);
		corner.top = VIPS_RECT_BOTTOM(&regionview->frame);
		corner.width = 0;
		corner.height = 0;
		vips_rect_marginadjust(&corner, margin);
		if (vips_rect_includespoint(&corner, x, y))
			return REGIONVIEW_RESIZE_BOTTOMRIGHT;

		corner.left = VIPS_RECT_RIGHT(&regionview->frame);
		corner.top = regionview->frame.top;
		corner.width = 0;
		corner.height = 0;
		vips_rect_marginadjust(&corner, margin);
		if (vips_rect_includespoint(&corner, x, y))
			return REGIONVIEW_RESIZE_TOPRIGHT;

		corner.left = regionview->frame.left;
		corner.top = VIPS_RECT_BOTTOM(&regionview->frame);
		corner.width = 0;
		corner.height = 0;
		vips_rect_marginadjust(&corner, margin);
		if (vips_rect_includespoint(&corner, x, y))
			return REGIONVIEW_RESIZE_BOTTOMLEFT;

		corner.left = regionview->frame.left;
		corner.top = regionview->frame.top;
		corner.width = 0;
		corner.height = 0;
		vips_rect_marginadjust(&corner, margin);
		if (vips_rect_includespoint(&corner, x, y))
			return REGIONVIEW_RESIZE_TOPLEFT;

		break;

	case REGIONVIEW_MARK:
		corner.left = regionview->frame.left;
		corner.top = regionview->frame.top;
		corner.width = 0;
		corner.height = 0;
		vips_rect_marginadjust(&corner, margin);
		if (vips_rect_includespoint(&corner, x, y))
			return REGIONVIEW_RESIZE_TOPLEFT;

		break;

	case REGIONVIEW_ARROW:
		// bottom-right first, since that's the most useful for small arrows
		corner.left = VIPS_RECT_RIGHT(&regionview->frame);
		corner.top = VIPS_RECT_BOTTOM(&regionview->frame);
		corner.width = 0;
		corner.height = 0;
		vips_rect_marginadjust(&corner, margin);
		if (vips_rect_includespoint(&corner, x, y))
			return REGIONVIEW_RESIZE_BOTTOMRIGHT;

		corner.left = regionview->frame.left;
		corner.top = regionview->frame.top;
		corner.width = 0;
		corner.height = 0;
		vips_rect_marginadjust(&corner, margin);
		if (vips_rect_includespoint(&corner, x, y))
			return REGIONVIEW_RESIZE_TOPLEFT;

		break;

	default:
		g_assert_not_reached();
	}

	/* Edge resize.
	 */
	switch (regionview->type) {
	case REGIONVIEW_REGION:
	case REGIONVIEW_AREA:
		VipsRect inner = regionview->frame;
		VipsRect outer = inner;
		vips_rect_marginadjust(&outer, margin);
		vips_rect_marginadjust(&inner, -margin);

		if (vips_rect_includespoint(&outer, x, y) &&
			!vips_rect_includespoint(&inner, x, y)) {
			if (x < regionview->frame.left)
				return REGIONVIEW_RESIZE_LEFT;
			if (x > VIPS_RECT_RIGHT(&regionview->frame))
				return REGIONVIEW_RESIZE_RIGHT;
			if (y < regionview->frame.top)
				return REGIONVIEW_RESIZE_TOP;
			if (y > VIPS_RECT_BOTTOM(&regionview->frame))
				return REGIONVIEW_RESIZE_BOTTOM;
		}

		break;

	case REGIONVIEW_MARK:
	case REGIONVIEW_ARROW:
		break;

	default:
		g_assert_not_reached();
	}

	return REGIONVIEW_RESIZE_NONE;
}

static void
regionview_pick_type(Regionview *regionview)
{
	if (abs(regionview->our_area.width) < 10)
		regionview->type = REGIONVIEW_MARK;
	else if (regionview->our_area.width > 0 &&
		regionview->our_area.height > 0)
		regionview->type = REGIONVIEW_REGION;
	else
		regionview->type = REGIONVIEW_ARROW;
}

void
regionview_resize(Regionview *regionview, guint modifiers,
	int width, int height, int x, int y)
{
	VipsRect *our_area = &regionview->our_area;
	VipsRect *start_area = &regionview->start_area;

	switch (regionview->resize) {
	case REGIONVIEW_RESIZE_MOVE:
		our_area->left = x + start_area->left;
		our_area->top = y + start_area->top;
		break;

	case REGIONVIEW_RESIZE_RIGHT:
		our_area->width = x + VIPS_RECT_RIGHT(start_area) - start_area->left;
		break;

	case REGIONVIEW_RESIZE_BOTTOM:
		our_area->height = y + VIPS_RECT_BOTTOM(start_area) - start_area->top;
		break;

	case REGIONVIEW_RESIZE_BOTTOMRIGHT:
		our_area->width = x + VIPS_RECT_RIGHT(start_area) - start_area->left;
		our_area->height = y + VIPS_RECT_BOTTOM(start_area) - start_area->top;
		break;

	case REGIONVIEW_RESIZE_LEFT:
		our_area->left = x + start_area->left;
		break;

	case REGIONVIEW_RESIZE_TOP:
		our_area->top = y + start_area->top;
		break;

	case REGIONVIEW_RESIZE_TOPLEFT:
		our_area->left = x + start_area->left;
		our_area->top = y + start_area->top;
		break;

	default:
		break;
	}

	if (!regionview->frozen)
		regionview_pick_type(regionview);

	switch (regionview->type) {
	case REGIONVIEW_REGION:
	case REGIONVIEW_MARK:
		switch (regionview->resize) {
		case REGIONVIEW_RESIZE_MOVE:
			our_area->left =
				VIPS_CLIP(0, our_area->left, width - start_area->width);
			our_area->top =
				VIPS_CLIP(0, our_area->top, height - start_area->height);
			break;

		case REGIONVIEW_RESIZE_RIGHT:
		case REGIONVIEW_RESIZE_BOTTOM:
		case REGIONVIEW_RESIZE_BOTTOMRIGHT:
			our_area->width =
				VIPS_CLIP(1, our_area->width, width - start_area->left);
			our_area->height =
				VIPS_CLIP(1, our_area->height, height - start_area->top);
			break;

		case REGIONVIEW_RESIZE_LEFT:
		case REGIONVIEW_RESIZE_TOP:
		case REGIONVIEW_RESIZE_TOPLEFT:
			our_area->left =
				VIPS_CLIP(0, our_area->left, VIPS_RECT_RIGHT(start_area) - 1);
			our_area->top =
				VIPS_CLIP(0, our_area->top, VIPS_RECT_BOTTOM(start_area) - 1);
			our_area->width = VIPS_RECT_RIGHT(start_area) - our_area->left;
			our_area->height = VIPS_RECT_BOTTOM(start_area) - our_area->top;
			break;

		default:
			break;
		}
		break;

	case REGIONVIEW_ARROW:
		if (modifiers & GDK_SHIFT_MASK) {
			if (our_area->height != 0 &&
				abs(our_area->width / our_area->height) > 10)
				our_area->height = 0;
			if (our_area->width != 0 &&
				abs(our_area->height / our_area->width) > 10)
				our_area->width = 0;
		}

		switch (regionview->resize) {
		case REGIONVIEW_RESIZE_TOPLEFT:
			// left/top have changed, so we must modify width and height
			// to keep the other point still
			our_area->width = VIPS_RECT_RIGHT(start_area) - our_area->left;
			our_area->height = VIPS_RECT_BOTTOM(start_area) - our_area->top;
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
}

static void
regionview_class_init(RegionviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;

	object_class->dispose = regionview_dispose;

	regionview_fontmap = pango_cairo_font_map_new();
	pango_cairo_font_map_set_resolution(
		PANGO_CAIRO_FONT_MAP(regionview_fontmap), 150);
	regionview_context = pango_font_map_create_context(regionview_fontmap);
}

static void
regionview_init(Regionview *regionview)
{
	static VipsRect empty_rect = { -1, -1, -1, -1 };

#ifdef DEBUG_MAKE
	printf("regionview_init\n");
#endif /*DEBUG_MAKE*/

	regionview->classmodel = NULL;

	regionview->model_area = NULL;

	regionview->frame = empty_rect;
	regionview->label = empty_rect;
	regionview->frozen = TRUE;

	vips_buf_init_dynamic(&regionview->caption, REGIONVIEW_LABEL_MAX);

	gtk_widget_set_name(GTK_WIDGET(regionview), "regionview_widget");
}

static void
regionview_model_changed(Classmodel *classmodel, Regionview *regionview)
{
#ifdef DEBUG
	printf("regionview_model_changed: classmodel has changed\n");
#endif /*DEBUG*/

	regionview_update_from_model(regionview);
}

static void
regionview_model_redraw(Classmodel *classmodel, Regionview *regionview)
{
#ifdef DEBUG
	printf("regionview_model_redraw:\n");
#endif /*DEBUG*/

	// save the state so it's not distrubed before the draw acyually occurs
	regionview->draw_area = regionview->our_area;
	regionview->draw_type = regionview->type;

	// queue a redraw on our imageui to get us repainted
	if (regionview->imageui)
		imageui_queue_draw(regionview->imageui);
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

Regionview *
regionview_new(Classmodel *classmodel)
{
	Regionview *regionview = g_object_new(REGIONVIEW_TYPE, 0);

	if (classmodel) {
		regionview->classmodel = classmodel;

		g_assert(!g_slist_find(classmodel->views, regionview));
		classmodel->views = g_slist_prepend(classmodel->views, regionview);

		regionview_update_from_model(regionview);

		if (classmodel) {
			g_signal_connect_object(G_OBJECT(classmodel), "changed",
				G_CALLBACK(regionview_model_changed), regionview, 0);

			/* "changed" is emitted all the time, "redraw" only at the end of
			 * update.
			 */
			g_signal_connect_object(G_OBJECT(classmodel), "redraw",
				G_CALLBACK(regionview_model_redraw), regionview, 0);
		}

		regionview_model_redraw(classmodel, regionview);
	}

	return regionview;
}
