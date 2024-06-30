/* draw a view of a region in an imageview
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

#define REGIONVIEW_TYPE (regionview_get_type())
#define REGIONVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), REGIONVIEW_TYPE, Regionview))
#define REGIONVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), REGIONVIEW_TYPE, RegionviewClass))
#define IS_REGIONVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), REGIONVIEW_TYPE))
#define IS_REGIONVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), REGIONVIEW_TYPE))
#define REGIONVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), REGIONVIEW_TYPE, RegionviewClass))

#define REGIONVIEW_LABEL_MAX (256)

/* Draw types.
 */
typedef enum {
	REGIONVIEW_REGION,			/* width & height > 0 */
	REGIONVIEW_AREA,			/* width & height > 0 and locked */
	REGIONVIEW_MARK,			/* width & height == 0 */
	REGIONVIEW_ARROW,			/* width & height unconstrained */
	REGIONVIEW_HGUIDE,			/* width == image width, height == 0 */
	REGIONVIEW_VGUIDE,			/* width == 0, height == image height */
	REGIONVIEW_LINE,			/* floating dashed line for paintbox */
	REGIONVIEW_BOX				/* floating dashed box for paintbox */
} RegionviewType;

/* Resize types.
 */
typedef enum {
	REGIONVIEW_RESIZE_NONE,
	REGIONVIEW_RESIZE_MOVE,
	REGIONVIEW_RESIZE_EDIT,
	REGIONVIEW_RESIZE_TOPLEFT,
	REGIONVIEW_RESIZE_TOP,
	REGIONVIEW_RESIZE_TOPRIGHT,
	REGIONVIEW_RESIZE_RIGHT,
	REGIONVIEW_RESIZE_BOTTOMRIGHT,
	REGIONVIEW_RESIZE_BOTTOM,
	REGIONVIEW_RESIZE_BOTTOMLEFT,
	REGIONVIEW_RESIZE_LEFT,
	REGIONVIEW_RESIZE_LAST
} RegionviewResize;

struct _Regionview {
	View view;

	RegionviewType type;
	gboolean frozen;			/* type is frozen ... not rethought on resize */

	/* The model we show.
	 */
	Classmodel *classmodel;
	VipsRect *model_area;		/* What we read/write to talk to the model */
	VipsRect our_area;			/* Same, but our copy ... origin top left */

	/* The window we draw on.
	 */
	Imageui *imageui;

	/* What's on the screen.
	 */
	PangoLayout *layout;		/* What we draw the label with */
	PangoRectangle ink_rect;	/* Label ink */
	VipsRect frame;				/* Area of region ... screen coordinates */
	VipsRect label;				/* Area covered by label ... screen cods */





	int ascent;					/* Height of ascenders for text */
	int dash_offset;
	guint dash_crawl;			/* Timer for dash crawl animation */
	RegionviewType last_type;
	gboolean first;				/* Initial draw (no old pos to remove) */
	gboolean label_geo;			/* Redo the label geo on refresh, please */


	/* Text of label we display
	 */
	VipsBuf caption;
};

typedef struct _RegionviewClass {
	ViewClass parent_class;

	/* My methods.
	 */
} RegionviewClass;

void regionview_model_update(Regionview *regionview);
RegionviewResize regionview_hit(Regionview *regionview, int x, int y);
void regionview_attach(Regionview *regionview, int x, int y);
void regionview_draw(Regionview *regionview, GtkSnapshot *snapshot);

GType regionview_get_type(void);
Regionview *regionview_new(Classmodel *classmodel);

void regionview_set_type(Regionview *regionview, PElement *root);
