/* view of a column
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

#define COLUMNVIEW_TYPE (columnview_get_type())
#define COLUMNVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), COLUMNVIEW_TYPE, Columnview))
#define COLUMNVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), COLUMNVIEW_TYPE, ColumnviewClass))
#define IS_COLUMNVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), COLUMNVIEW_TYPE))
#define IS_COLUMNVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), COLUMNVIEW_TYPE))
#define COLUMNVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), COLUMNVIEW_TYPE, ColumnviewClass))

/* State ... for mouse titlebar interactions.
 */
typedef enum {
	COL_WAIT,	/* Rest state */
	COL_SELECT, /* Select start, but no drag yet */
	COL_DRAG,	/* Drag state */
	COL_EDIT	/* Editing caption */
} ColumnviewState;

struct _Columnview {
	View view;

	/* Our enclosing workspaceview.
	 */
	Workspaceview *wview;

	/* Display parts.
	 */
	GtkWidget *top;		/* Enclosing widget for the whole cview */
	GtkWidget *title;	/* Columnview titlebar */
	GtkWidget *label;	/* Columnview name label */
	GtkWidget *head;	/* Columnview caption */
	GtkWidget *entry;	/* Text entry at bottom */
	GtkWidget *capedit; /* Shadow text for editing caption */

	/* A shadow for this cview, used during drag to show where this column
	 * will end up.
	 *
	 * And if we are a shadow, the master cview we are the shadow for.
	 */
	Columnview *shadow;
	Columnview *master;

	/* Appearance state info.
	 */
	ColumnviewState state; /* Waiting or dragging */
	int start_x;		   /* Drag start */
	int start_y;

	int lx, ly; /* last pos we set */
	int sx, sy; /* Drag start point */
	int rx, ry; /* Drag offset */
	int tx, ty; /* Tally window pos in root cods */

	gboolean selected; /* Last drawn in selected state? */

	/* We watch resize events and trigger a workspace relayout with these.
	 */
	int old_width;
	int old_height;
};

typedef struct _ColumnviewClass {
	ViewClass parent_class;

	/* My methods.
	 */
} ColumnviewClass;

void columnview_get_position(Columnview *cview,
	int *x, int *y, int *w, int *h);
void columnview_add_shadow(Columnview *old_cview);

GType columnview_get_type(void);
View *columnview_new(void);
