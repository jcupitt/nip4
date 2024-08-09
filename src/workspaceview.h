/* a view of a workspace
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

#define WORKSPACEVIEW_TYPE (workspaceview_get_type())
#define WORKSPACEVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), WORKSPACEVIEW_TYPE, Workspaceview))
#define WORKSPACEVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), WORKSPACEVIEW_TYPE, WorkspaceviewClass))
#define IS_WORKSPACEVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), WORKSPACEVIEW_TYPE))
#define IS_WORKSPACEVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), WORKSPACEVIEW_TYPE))
#define WORKSPACEVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), WORKSPACEVIEW_TYPE, WorkspaceviewClass))

/* State ... for mouse titlebar interactions.
 */
typedef enum {
	WVIEW_WAIT,	  /* Rest state */
	WVIEW_SELECT, /* Select start, but no drag yet */
	WVIEW_DRAG,	  /* Drag state */
} WorkspaceviewState;

struct _Workspaceview {
	View view;

	Workspaceviewlabel *label;	/* For the notebook tab */
	GtkWidget *fixed;			/* GtkFixed for tally */
	GtkWidget *scrolled_window; /* ScrolledWindow holding fixed */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	Toolkitbrowser *toolkitbrowser;
	Workspacedefs *workspacedefs;

	/* Left and right panes ... program window and toolkit browser.
	 */
	Pane *lpane;
	Pane *rpane;

	GtkWidget *popup;

	/* Our state machine for interactions.
	 */
	WorkspaceviewState state; /* Waiting or dragging */

	/* Object drag.
	 */
	Columnview *drag_cview;		/* Column we are dragging (if any) */
	Rowview *drag_rview;		/* Row we are dragging (if any) */
	int obj_x;					/* Object position at start of drag */
	int obj_y;
	int start_x;				/* Mouse at start of drag */
	int start_y;

	/* Row shadow during row drag.
	 *
	 * The column containing the shadow, the column containing the previous
	 * shadow (can be the same), the max row shadow height during this row
	 * drag. We have to keep the shadow widgets here too.
	 *
	 * Shadow positions are -1 if no shadow row. Real rows are at 1, 3, 5
	 * etc. (2 * pos + 1), shadows at 0, 2, 4 etc.
	 */
	GtkWidget *row_shadow;
	Columnview *row_shadow_column;
	int row_shadow_position;

	GtkWidget *previous_row_shadow;
	Columnview *previous_row_shadow_column;
	int previous_row_shadow_position;

	int max_row_shadow_height;
	double row_shadow_elapsed;

	/* The source of the row drag for undo.
	 */
	Subcolumnview *old_sview;

	/* Geometry.
	 */
	VipsRect vp; /* Viewport pos and size */
	int width;	 /* Size of fixed area */
	int height;

	/* Placement hints for new columns.
	 */
	int next_x;
	int next_y;

	/* Follow prefs changes.
	 */
	guint watch_changed_sid;

	/* Only show the compat warning once.
	 */
	gboolean popped_compat;

	/* For layout animation.
	 */
	guint tick_handler;
	gint64 last_frame_time;
	gboolean should_animate;

	/* A temp floating columnview we just make for row drag.
	 */
	Columnview *floating;
};

typedef struct _WorkspaceviewClass {
	ViewClass parent_class;

	/* My methods.
	 */
} WorkspaceviewClass;

void workspaceview_scroll(Workspaceview *wview, int x, int y, int w, int h);

Columnview *workspaceview_find_columnview(Workspaceview *wview, int x, int y);
Columnview *workspaceview_find_columnview_title(Workspaceview *wview, int x, int y);

GType workspaceview_get_type(void);
View *workspaceview_new(void);

void workspaceview_set_label(Workspaceview *wview, GtkWidget *label);
