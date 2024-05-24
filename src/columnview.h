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
	COL_WAIT,						/* Rest state */
	COL_SELECT,						/* Select start, but no drag yet */
	COL_DRAG,						/* Drag state */
	COL_EDIT						/* Editing caption */
} ColumnviewState;

struct _Columnview {
	View view;

	/* Display parts.
	 */
	GtkWidget *top;					/* Enclosing widget for the whole cview */
	GtkWidget *title;				/* Columnview titlebar */
	GtkWidget *expand_button;		/* Expander button */
	GtkWidget *name;				/* Columnview name label */
	GtkWidget *caption_edit_stack;  /* Caption mode switcher */
	GtkWidget *caption;				/* Caption display */
	GtkWidget *caption_edit;		/* Caption edit */
	GtkWidget *entry;				/* Text entry at bottom */
	GtkWidget *revealer;			/* Animate visibility for body */
	GtkWidget *body;				/* The body of the columnview */

	/* Context menu for titlebar.
	 */
	GtkWidget *right_click_menu;

	/* A shadow for this cview, used during drag to show where this column
	 * will end up.
	 *
	 * And if we are a shadow, the master cview we are the shadow for.
	 */
	Columnview *shadow;
	Columnview *master;

	/* Appearance state info.
	 */
	ColumnviewState state;			/* Waiting or dragging */

	/* Current position. Though it will be drawn somewhere between x/y and
	 * start_x/start_y, depending on elapsed time.
	 *
	 * We can't use col->x/y, since every columnview needs a separate
	 * position, including the shadow. Copy to/from col->x/y on update.
	 */
	int x;
	int y;

	/* Start position for animation towards x/y
	 */
	int start_x;
	int start_y;

	/* Time in seconds since the start of the animation.
	 */
	double elapsed;

	/* TRUE if an animation is in progress.
	 */
	gboolean animating;

	gboolean selected;				/* Last drawn in selected state? */

	const char *css_class;
};

typedef struct _ColumnviewClass {
	ViewClass parent_class;

	/* My methods.
	 */
} ColumnviewClass;

void columnview_get_position(Columnview *cview, int *x, int *y, int *w, int *h);
Workspaceview *columnview_get_wview(Columnview *cview);
void columnview_add_shadow(Columnview *old_cview);
void columnview_remove_shadow(Columnview *cview);
void columnview_animate_to(Columnview *cview, int x, int y);

GType columnview_get_type(void);
View *columnview_new(void);
