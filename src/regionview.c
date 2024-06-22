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

static void
regionview_dispose(GObject *object)
{
	Regionview *regionview;
	Imagedisplay *id;

#ifdef DEBUG_MAKE
	printf("regionview_dispose: %p\n", object);
#endif /*DEBUG_MAKE*/

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_REGIONVIEW(object));

	regionview = REGIONVIEW(object);

	VIPS_UNREF(regionview->layout);
	VIPS_FREEF(g_source_remove, regionview->dash_crawl);
	vips_buf_destroy(&regionview->caption);

	if (regionview->classmodel) {
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

		vips_buf_rewind(&regionview->caption);
        row_qualified_name_relative(row->ws->sym, row, &regionview->caption);
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

	/* Refresh immediately .. gives faster feedback during drag.
	 */
	vobject_refresh(VOBJECT(regionview));

#ifdef DEBUG
	printf("regionview_model_update: set model to %dx%d size %dx%d\n",
		model_area->left, model_area->top,
		model_area->width, model_area->height);
#endif /*DEBUG*/
}

/* Queue draws for all the pixels a region might touch.
 */
static void
regionview_queue_draw(Regionview *regionview)
{
	printf("regionview_queue_draw: FIXME\n");
}

void
regionview_draw(Regionview *regionview, Imageui *imageui, GtkSnapshot *snapshot)
{
	double zoom = imageui_get_zoom(imageui);

	// main frame
	double left;
	double top;
	imageui_image_to_gtk(imageui,
		regionview->our_area.left, regionview->our_area.top, &left, &top);
	regionview->frame.left = left;
	regionview->frame.top = top;
	regionview->frame.width = regionview->our_area.width * zoom;
	regionview->frame.height = regionview->our_area.height * zoom;

	GskRoundedRect frame;
	gsk_rounded_rect_init_from_rect(&frame,
		&GRAPHENE_RECT_INIT(
			regionview->frame.left,
			regionview->frame.top,
			regionview->frame.width,
			regionview->frame.height),
		0);

	// label
	regionview->label.left = frame.bounds.origin.x - regionview_line_width;
	regionview->label.top = frame.bounds.origin.y -
        2 * regionview_label_margin - regionview->ink_rect.height,
	regionview->label.width = regionview->ink_rect.width +
		2 * regionview_label_margin;
	regionview->label.height = regionview->ink_rect.height +
		2 * regionview_label_margin;

	GskRoundedRect label;
	gsk_rounded_rect_init_from_rect(&label,
		&GRAPHENE_RECT_INIT(
			regionview->label.left,
			regionview->label.top,
			regionview->label.width,
			regionview->label.height),
		regionview_corner_radius);

	gtk_snapshot_append_outset_shadow(snapshot, &frame, &regionview_shadow,
		regionview_line_width, regionview_line_width,
		3, regionview_corner_radius);

	gtk_snapshot_append_outset_shadow(snapshot, &label, &regionview_shadow,
		regionview_line_width, regionview_line_width,
		3, regionview_corner_radius);

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

	float label_line = label.bounds.size.height / 2;
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

// (x, y) in gtk cods
RegionviewResize
regionview_hit(Regionview *regionview, int x, int y)
{
	RegionviewResize resize;

	printf("regionview_hit: %d, %d\n", x, y);
	printf("\tleft = %d, top = %d, width = %d, height = %d\n",
		regionview->label.left, regionview->label.top,
		regionview->label.width, regionview->label.height);

	if (vips_rect_includespoint(&regionview->label, x, y))
		return REGIONVIEW_RESIZE_MOVE;

	/*
	REGIONVIEW_RESIZE_TOPLEFT,
	REGIONVIEW_RESIZE_TOP,
	REGIONVIEW_RESIZE_TOPRIGHT,
	REGIONVIEW_RESIZE_RIGHT,
	REGIONVIEW_RESIZE_BOTTOMRIGHT,
	REGIONVIEW_RESIZE_BOTTOM,
	REGIONVIEW_RESIZE_BOTTOMLEFT,
	REGIONVIEW_RESIZE_LEFT,
	 */

	return REGIONVIEW_RESIZE_NONE;
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
	regionview->ascent = 0;
	regionview->dash_offset = 0;
	regionview->dash_crawl = 0;
	regionview->last_type = (RegionviewType) -1;
	regionview->first = TRUE;
	regionview->label_geo = TRUE;

	vips_buf_init_dynamic(&regionview->caption, REGIONVIEW_LABEL_MAX);

	gtk_widget_set_name(GTK_WIDGET(regionview), "regionview_widget");
}

static void
regionview_model_changed_cb(Classmodel *classmodel, Regionview *regionview)
{
	vobject_refresh_queue(VOBJECT(regionview));
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

	regionview->classmodel = classmodel;
	regionview_update_from_model(regionview);

	if (classmodel)
		g_signal_connect_object(G_OBJECT(classmodel), "changed",
			G_CALLBACK(regionview_model_changed_cb), regionview, 0);

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
