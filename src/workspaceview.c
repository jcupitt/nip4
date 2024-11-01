/* a workspaceview button in a workspace
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
#define DEBUG_VERBOSE
#define DEBUG
 */

#include "nip4.h"

G_DEFINE_TYPE(Workspaceview, workspaceview, VIEW_TYPE)

/* Params for "Align Columns" function.
 */
static const int workspaceview_layout_snap = 100;
static const int workspaceview_layout_hspacing = 10;
static const int workspaceview_layout_vspacing = 10;
static const int workspaceview_layout_left = 5;
static const int workspaceview_layout_top = 5;
static const double workspaceview_animation_duration = 0.5;

/* From clutter-easing.c, based on Robert Penner's infamous easing equations,
 * MIT license.
 */
static double
ease_out_cubic(double t)
{
	double p = t - 1;

	return p * p * p + 1;
}

static void
workspaceview_stop_animation(Workspaceview *wview)
{
	if (wview->tick_handler) {
		gtk_widget_remove_tick_callback(GTK_WIDGET(wview), wview->tick_handler);
		wview->tick_handler = 0;
	}
}

static void
workspaceview_remove_previous_row_shadow(Workspaceview *wview)
{
	if (wview->previous_row_shadow) {
		Subcolumnview *sview = wview->previous_row_shadow_column->sview;

		gtk_grid_remove(GTK_GRID(sview->grid), wview->previous_row_shadow);
		wview->previous_row_shadow = NULL;
		wview->previous_row_shadow_column = NULL;
	}
}

static void
workspaceview_remove_row_shadow(Workspaceview *wview)
{
	if (wview->row_shadow) {
		Subcolumnview *sview = wview->row_shadow_column->sview;

		gtk_grid_remove(GTK_GRID(sview->grid), wview->row_shadow);
		wview->row_shadow = NULL;
		wview->row_shadow_column = NULL;
	}

	workspaceview_remove_previous_row_shadow(wview);
}

static void
workspaceview_set_row_shadow_height(Workspaceview *wview, int shadow_height)
{
	if (wview->row_shadow)
		gtk_widget_set_margin_top(wview->row_shadow, shadow_height);
	if (wview->previous_row_shadow)
		gtk_widget_set_margin_top(wview->previous_row_shadow,
			wview->max_row_shadow_height - shadow_height);
}

static gboolean
workspaceview_tick(GtkWidget *widget, GdkFrameClock *frame_clock,
	gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);

	gint64 frame_time = gdk_frame_clock_get_frame_time(frame_clock);
	double dt = wview->last_frame_time > 0 ? (double) (frame_time - wview->last_frame_time) / G_TIME_SPAN_SECOND : 1.0 / G_TIME_SPAN_SECOND;

#ifdef DEBUG_VERBOSE
	printf("workspaceview_tick: dt = %g\n", dt);
#endif /*DEBUG_VERBOSE*/

	gboolean finished = TRUE;

	/* Column drag animate.
	 */
	for (GSList *p = VIEW(wview)->children; p; p = p->next) {
		Columnview *cview = COLUMNVIEW(p->data);

		int x;
		int y;

		if (!cview->animating)
			continue;

		// elapsed time in seconds
		cview->elapsed += dt;

		// 0-1 progress in animation
		double duration = wview->should_animate ? workspaceview_animation_duration : cview->elapsed;
		double t = VIPS_CLIP(0, ease_out_cubic(cview->elapsed / duration), 1);

		if (cview->shadow ||
			t == 1.0) {
			/* If this column is being dragged by the user, don't animate, just
			 * set the final position immediately.
			 *
			 * Likewise, if time is up, make sure we set the exact position
			 * requested.
			 */
			x = cview->x;
			y = cview->y;
			cview->animating = FALSE;
		}
		else {
			finished = FALSE;
			x = cview->start_x + t * (cview->x - cview->start_x);
			y = cview->start_y + t * (cview->y - cview->start_y);
		}

		gtk_fixed_move(GTK_FIXED(wview->fixed), GTK_WIDGET(cview), x, y);
	}

	/* Row drag animate.
	 */
	if (wview->row_shadow ||
		wview->previous_row_shadow) {
		wview->row_shadow_elapsed += dt;

		// 0-1 progress in animation
		double duration = wview->should_animate ? workspaceview_animation_duration : wview->row_shadow_elapsed;
		double t = VIPS_CLIP(0, ease_out_cubic(wview->row_shadow_elapsed / duration), 1);

		workspaceview_set_row_shadow_height(wview,
			VIPS_RINT(t * wview->max_row_shadow_height));

		if (t != 1.0)
			finished = FALSE;
	}

	if (finished)
		workspaceview_stop_animation(wview);

	wview->last_frame_time = frame_time;

	return G_SOURCE_CONTINUE;
}

static void
workspaceview_start_animation(Workspaceview *wview)
{
	if (!wview->tick_handler) {
		wview->last_frame_time = -1;
		wview->tick_handler = gtk_widget_add_tick_callback(GTK_WIDGET(wview),
			workspaceview_tick, wview, NULL);
	}
}

// make a new full-size row shadow
static void
workspaceview_add_row_shadow(Workspaceview *wview, int max_row_shadow_height,
	Columnview *row_shadow_column, int row_shadow_position)
{
	workspaceview_remove_row_shadow(wview);

	if (row_shadow_position != -1) {
		wview->max_row_shadow_height = max_row_shadow_height;

		// an empty bopx is a convenient zero width/height widget
		// for padding
		wview->row_shadow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_margin_top(wview->row_shadow,
			wview->max_row_shadow_height);
		wview->row_shadow_column = row_shadow_column;
		wview->row_shadow_position = row_shadow_position;

		Subcolumnview *sview = row_shadow_column->sview;
		gtk_grid_attach(GTK_GRID(sview->grid), wview->row_shadow,
			1, row_shadow_position, 1, 1);
	}
}

// animate the row shadow to a new position
static void
workspaceview_move_row_shadow(Workspaceview *wview,
	Columnview *row_shadow_column, int row_shadow_position)
{
	if (wview->row_shadow_position != row_shadow_position ||
		wview->row_shadow_column != row_shadow_column) {
		// any previous shadow vanishes
		workspaceview_remove_previous_row_shadow(wview);

		// any current shadow becomes the previous shadow
		if (wview->row_shadow) {
			wview->previous_row_shadow = wview->row_shadow;
			wview->previous_row_shadow_column = wview->row_shadow_column;
			wview->previous_row_shadow_position = wview->row_shadow_position;

			wview->row_shadow = NULL;
			wview->row_shadow_position = -1;
			wview->row_shadow_column = NULL;
		}

		// start the shadow growing from the new row
		if (row_shadow_position != -1) {
			wview->row_shadow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_widget_set_margin_top(wview->row_shadow, 0);
			wview->row_shadow_column = row_shadow_column;
			wview->row_shadow_position = row_shadow_position;

			Subcolumnview *sview = row_shadow_column->sview;
			gtk_grid_attach(GTK_GRID(sview->grid), wview->row_shadow,
				1, row_shadow_position, 1, 1);
		}

		wview->row_shadow_elapsed = 0.0;
		workspaceview_start_animation(wview);
	}
}

static void
workspaceview_scroll_to(Workspaceview *wview, int x, int y)
{
	int nx, ny;

	// printf("workspaceview_scroll_to: x=%d y=%d\n", x, y);

	nx = VIPS_CLIP(0, x, wview->width - wview->vp.width);
	ny = VIPS_CLIP(0, y, wview->height - wview->vp.height);

	gtk_adjustment_set_value(wview->hadj, nx);
	gtk_adjustment_set_value(wview->vadj, ny);
}

/* Scroll to make an xywh area visible. If the area is larger than the
 * viewport, position the view at the bottom left if the xywh area ...
 * this is usually right for workspaces.
 */
void
workspaceview_scroll(Workspaceview *wview, int x, int y, int w, int h)
{
	VipsRect *vp = &wview->vp;

	int nx, ny;

#ifdef DEBUG_VERBOSE
	printf("workspaceview_scroll: x=%d, y=%d, w=%d, h=%d\n", x, y, w, h);
#endif /*DEBUG_VERBOSE*/

	nx = gtk_adjustment_get_value(wview->hadj);
	ny = gtk_adjustment_get_value(wview->vadj);

	if (x + w > VIPS_RECT_RIGHT(vp))
		nx = VIPS_MAX(0, (x + w) - vp->width);
	if (x < nx)
		nx = x;

	if (y + h > VIPS_RECT_BOTTOM(vp))
		ny = VIPS_MAX(0, (y + h) - vp->height);
	if (y < ny)
		ny = y;

	workspaceview_scroll_to(wview, nx, ny);
}

/* Update our geometry from the fixed widget.
 */
static void
workspaceview_scroll_update(Workspaceview *wview)
{
	wview->vp.left = gtk_adjustment_get_value(wview->hadj);
	wview->vp.top = gtk_adjustment_get_value(wview->vadj);
	wview->vp.width = gtk_adjustment_get_page_size(wview->hadj);
	wview->vp.height = gtk_adjustment_get_page_size(wview->vadj);

	wview->width = gtk_adjustment_get_upper(wview->hadj);
	wview->height = gtk_adjustment_get_upper(wview->vadj);

	/* Update vp hint in model too.
	 */
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	if (ws)
		ws->vp = wview->vp;

#ifdef DEBUG_VERBOSE
	printf("workspaceview_scroll_update: %s\n", IOBJECT(ws)->name);
	printf("  wview->vp: l=%d, t=%d, w=%d, h=%d; fixed w=%d; h=%d\n",
		wview->vp.left, wview->vp.top,
		wview->vp.width, wview->vp.height,
		wview->width, wview->height);
#endif /*DEBUG_VERBOSE*/
}

static void *
workspaceview_columnview_hit(View *view, void *a, void *b)
{
	Columnview *cview = COLUMNVIEW(view);
	graphene_point_t *point = (graphene_point_t *) a;
	Workspaceview *wview = WORKSPACEVIEW(b);

	graphene_rect_t bounds;
	if (gtk_widget_compute_bounds(GTK_WIDGET(cview), wview->fixed, &bounds) &&
		graphene_rect_contains_point(&bounds, point))
		return cview;

	return NULL;
}

/* Find the columnview for a point.
 */
Columnview *
workspaceview_find_columnview(Workspaceview *wview, int x, int y)
{
	graphene_point_t point = GRAPHENE_POINT_INIT(x, y);

	return view_map(VIEW(wview),
		workspaceview_columnview_hit, &point, wview);
}

static void *
workspaceview_columnview_title_hit(View *view, void *a, void *b)
{
	Columnview *cview = COLUMNVIEW(view);
	graphene_point_t *point = (graphene_point_t *) a;
	Workspaceview *wview = WORKSPACEVIEW(b);

	graphene_rect_t bounds;

	if (gtk_widget_compute_bounds(cview->title, wview->fixed, &bounds) &&
		graphene_rect_contains_point(&bounds, point))
		return cview;

	return NULL;
}

/* Find the columnview title bar for a point.
 */
Columnview *
workspaceview_find_columnview_title(Workspaceview *wview, int x, int y)
{
	graphene_point_t point = GRAPHENE_POINT_INIT(x, y);

	return view_map(VIEW(wview),
		workspaceview_columnview_title_hit, &point, wview);
}

static void
workspaceview_unfloat(Workspaceview *wview)
{
	if (wview->floating) {
		view_child_remove(VIEW(wview->floating));
		wview->floating = NULL;
	}
}

static void
workspaceview_dispose(GObject *object)
{
	Workspaceview *wview;

#ifdef DEBUG
	printf("workspaceview_dispose: %p\n", object);
#endif /*DEBUG*/

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_WORKSPACEVIEW(object));

	wview = WORKSPACEVIEW(object);

	workspaceview_stop_animation(wview);
	workspaceview_remove_row_shadow(wview);
	workspaceview_unfloat(wview);
	FREESID(wview->watch_changed_sid, main_watchgroup);
	gtk_widget_dispose_template(GTK_WIDGET(wview), WORKSPACEVIEW_TYPE);

	G_OBJECT_CLASS(workspaceview_parent_class)->dispose(object);
}

static void
workspaceview_realize(GtkWidget *widget)
{
#ifdef DEBUG
	{
		Workspaceview *wview = WORKSPACEVIEW(widget);
		Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

		printf("workspaceview_realize: %s\n", IOBJECT(ws)->name);
	}
#endif /*DEBUG*/

	GTK_WIDGET_CLASS(workspaceview_parent_class)->realize(widget);

	/* Mark us as a symbol drag-to widget.
	 */
	set_symbol_drag_type(widget);
}

static void
workspaceview_link(View *view, Model *model, View *parent)
{
	VIEW_CLASS(workspaceview_parent_class)->link(view, model, parent);

	printf("workspaceview_link: FIXME toolkitbrowser, panes, etc.\n");
	// Workspace *ws = WORKSPACE(model);
	// Workspaceview *wview = WORKSPACEVIEW(view);
	// vobject_link(VOBJECT(wview->toolkitbrowser), IOBJECT(ws->kitg));
	// vobject_link(VOBJECT(wview->workspacedefs), IOBJECT(ws));
	// toolkitbrowser_set_workspace(wview->toolkitbrowser, ws);
	//   pane_set_state(wview->rpane, ws->rpane_open, ws->rpane_position);
	//   pane_set_state(wview->lpane, ws->lpane_open, ws->lpane_position);
}

static void
workspaceview_child_add(View *parent, View *child)
{
	Columnview *cview = COLUMNVIEW(child);
	Workspaceview *wview = WORKSPACEVIEW(parent);

	VIEW_CLASS(workspaceview_parent_class)->child_add(parent, child);

	gtk_fixed_put(GTK_FIXED(wview->fixed), GTK_WIDGET(cview),
		cview->x, cview->y);
}

static void
workspaceview_child_remove(View *parent, View *child)
{
	Workspaceview *wview = WORKSPACEVIEW(parent);

	if (wview->drag_cview == COLUMNVIEW(child))
		wview->drag_cview = NULL;

	gtk_fixed_remove(GTK_FIXED(wview->fixed), GTK_WIDGET(child));

	VIEW_CLASS(workspaceview_parent_class)->child_remove(parent, child);
}

static void
workspaceview_child_position(View *parent, View *child)
{
	Workspaceview *wview = WORKSPACEVIEW(parent);

	workspaceview_start_animation(wview);

	VIEW_CLASS(workspaceview_parent_class)->child_position(parent, child);
}

static void
workspaceview_child_front(View *parent, View *child)
{
	Workspaceview *wview = WORKSPACEVIEW(parent);
	Columnview *cview = COLUMNVIEW(child);

	g_object_ref(cview);

	gtk_fixed_remove(GTK_FIXED(wview->fixed), GTK_WIDGET(cview));
	gtk_fixed_put(GTK_FIXED(wview->fixed), GTK_WIDGET(cview),
		cview->x, cview->y);

	g_object_unref(cview);
}

static void
workspaceview_refresh(vObject *vobject)
{
	Workspaceview *wview = WORKSPACEVIEW(vobject);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

#ifdef DEBUG
	printf("workspaceview_refresh: %p %s\n", ws, IOBJECT(ws)->name);
#endif /*DEBUG*/

	gtk_widget_set_sensitive(GTK_WIDGET(wview), !ws->locked);

	if (ws->rpane_open)
		pane_animate_open(wview->rpane);
	if (!ws->rpane_open)
		pane_animate_closed(wview->rpane);

	if (ws->lpane_open)
		pane_animate_open(wview->lpane);
	if (!ws->lpane_open)
		pane_animate_closed(wview->lpane);

	if (wview->label)
		workspaceviewlabel_refresh(wview->label);

	// show or hide the error bar
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(wview->error_bar),
		ws->show_error);
	if (ws->show_error) {
		set_glabel(wview->error_top, ws->error_top);
		set_glabel(wview->error_sub, ws->error_sub);
	}

	// columnviews change appearance based on workspace settings like locked,
	// so they must refresh too
	icontainer_map(ICONTAINER(ws),
		(icontainer_map_fn) iobject_changed, NULL, NULL);

	// and of course that can cause a relayout
	workspace_queue_layout(ws);

	VOBJECT_CLASS(workspaceview_parent_class)->refresh(vobject);
}

/* What we track during a layout.
 */
typedef struct _WorkspaceLayout {
	/* Context.
	 */
	Workspaceview *wview;

	/* Set of columnviews still to be laid out.
	 */
	GSList *undone_columns;

	/* Track the current set of columns here.
	 */
	GSList *current_columns;

	/* Current position for write.
	 */
	int out_x, out_y;

	/* Accumulate the size of the current set of columns here.
	 */
	VipsRect area;

	/* Track the current columnview here.
	 */
	Columnview *cview;
} WorkspaceLayout;

static void *
workspaceview_layout_add(View *view, WorkspaceLayout *layout)
{
	layout->undone_columns = g_slist_prepend(layout->undone_columns, view);

	return NULL;
}

static void *
workspaceview_layout_find_leftmost(Columnview *cview, WorkspaceLayout *layout)
{
	if (cview->x < layout->area.left) {
		layout->area.left = cview->x;
		layout->cview = cview;
	}

	return NULL;
}

static void *
workspaceview_layout_find_similar_x(Columnview *cview,
	WorkspaceLayout *layout)
{
	gboolean snap;

	/* Special case: a colum at zero makes a new column on the far left.
	 */
	snap = FALSE;
	if (layout->area.left == 0 &&
		cview->x == 0)
		snap = TRUE;
	if (layout->area.left > 0 &&
		ABS(cview->x - layout->area.left) < workspaceview_layout_snap)
		snap = TRUE;

	if (snap)
		layout->current_columns =
			g_slist_prepend(layout->current_columns, cview);

	return NULL;
}

/* Compare func for row recomp sort.
 */
static int
workspaceview_layout_sort_y(Columnview *a, Columnview *b)
{
	return a->y - b->y;
}

static void *
workspaceview_layout_set_pos(Columnview *cview, WorkspaceLayout *layout)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	/* columnview_refresh() will move the views and the shadow.
	 */
	col->x = layout->out_x;
	col->y = layout->out_y;
	iobject_changed(IOBJECT(col));

	int x, y, w, h;
	columnview_get_position(cview, &x, &y, &w, &h);
	layout->out_y += h + workspaceview_layout_vspacing;

	// find the widest column
	layout->area.width = VIPS_MAX(layout->area.width, w);

	return NULL;
}

static void *
workspaceview_layout_strike(Columnview *cview, WorkspaceLayout *layout)
{
	layout->undone_columns = g_slist_remove(layout->undone_columns, cview);

	return NULL;
}

static void
workspaceview_layout_loop(WorkspaceLayout *layout)
{
	layout->cview = NULL;
	layout->area.left = INT_MAX;
	slist_map(layout->undone_columns,
		(SListMapFn) workspaceview_layout_find_leftmost, layout);

	layout->current_columns = NULL;
	slist_map(layout->undone_columns,
		(SListMapFn) workspaceview_layout_find_similar_x, layout);

	layout->current_columns = g_slist_sort(layout->current_columns,
		(GCompareFunc) workspaceview_layout_sort_y);

	layout->out_y = workspaceview_layout_top;
	layout->area.width = 0;
	slist_map(layout->current_columns,
		(SListMapFn) workspaceview_layout_set_pos, layout);

	layout->out_x += layout->area.width + workspaceview_layout_hspacing;

	slist_map(layout->current_columns,
		(SListMapFn) workspaceview_layout_strike, layout);

	VIPS_FREEF(g_slist_free, layout->current_columns);
}

/* Autolayout ... try to rearrange columns so they don't overlap.

	Strategy:

	select the left-most column

	search for all columns with a 'small' overlap with the selected column

	lay those columns out vertically with some space between them ... keep
	the vertical ordering we had before

	find the width of the widest, move output over that much

	strike that set of columns from the list of columns to be laid out
 */
static void
workspaceview_layout(View *view)
{
	Workspaceview *wview = WORKSPACEVIEW(view);

	WorkspaceLayout layout;

	layout.wview = wview;
	layout.undone_columns = NULL;
	layout.current_columns = NULL;
	layout.out_x = workspaceview_layout_left;

	view_map(VIEW(wview),
		(view_map_fn) workspaceview_layout_add, &layout, NULL);

	while (layout.undone_columns)
		workspaceview_layout_loop(&layout);
}

static void
workspaceview_saveas_sub(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(wview)));
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		mainwindow_set_save_folder(main, file);

		g_autofree char *filename = g_file_get_path(file);

		icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));
		if (!workspacegroup_save_current(wsg, filename))
			workspace_set_show_error(ws, TRUE);
	}
}

static void
workspaceview_saveas(Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(wview)));

	GtkFileDialog *dialog;

	// we can only save the current tab
	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save tab as");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GFile *save_folder = mainwindow_get_save_folder(main);
	if (save_folder)
		gtk_file_dialog_set_initial_folder(dialog, save_folder);

	gtk_file_dialog_save(dialog, GTK_WINDOW(main), NULL,
		&workspaceview_saveas_sub, wview);
}

static void
workspaceview_merge_sub(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(wview)));
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		mainwindow_set_save_folder(main, file);

		g_autofree char *filename = g_file_get_path(file);

		if (!workspace_merge_file(ws, filename))
			workspace_set_show_error(ws, TRUE);
	}
}

static void
workspaceview_merge(Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(wview)));

	// we can only save the current tab
	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Merge into tab");
	gtk_file_dialog_set_accept_label(dialog, "Merge");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GFile *load_folder = mainwindow_get_load_folder(main);
	if (load_folder)
		gtk_file_dialog_set_initial_folder(dialog, load_folder);

	gtk_file_dialog_open(dialog, GTK_WINDOW(main), NULL,
		&workspaceview_merge_sub, wview);
}

static void
workspaceview_action(GSimpleAction *action, GVariant *parameter, View *view)
{
	Workspaceview *wview = WORKSPACEVIEW(view);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	const char *name = g_action_get_name(G_ACTION(action));

	printf("workspaceview_action: %s\n", name);

	if (g_str_equal(name, "column-new"))
		workspace_column_new(ws);
	else if (g_str_equal(name, "next-error"))
		workspace_set_show_error(ws, workspace_next_error(ws));
	else if (g_str_equal(name, "tab-rename"))
		g_object_set(wview->label,
			"edit", TRUE,
			NULL);
	else if (g_str_equal(name, "tab-select-all")) {
		if (!ws->locked)
			workspace_select_all(ws);
	}
	else if (g_str_equal(name, "tab-duplicate"))
		workspace_duplicate(ws);
	else if (g_str_equal(name, "tab-merge"))
		workspaceview_merge(wview);
	else if (g_str_equal(name, "tab-saveas"))
		workspaceview_saveas(wview);
	else if (g_str_equal(name, "tab-lock")) {
		workspace_set_locked(ws, !ws->locked);

		GVariant *locked = g_variant_new_boolean(ws->locked);
		g_simple_action_set_state(action, locked);
	}
	else if (g_str_equal(name, "tab-delete")) {
		if (!ws->locked)
			model_check_destroy(view_get_window(VIEW(wview)), MODEL(ws));
	}
}

static void
workspaceview_click(GtkGestureClick *gesture,
	guint n_press, double x, double y, Workspaceview *wview)
{
	Columnview *cview = workspaceview_find_columnview(wview, x, y);

	if (!cview) {
		Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

		workspace_deselect_all(ws);
	}

	// we detect single click in drag_update
}

static void
workspaceview_float_rowview(Workspaceview *wview, Rowview *rview)
{
	workspaceview_unfloat(wview);

	// the sview we are removing the rowview from
	Subcolumnview *old_sview = SUBCOLUMNVIEW(VIEW(rview)->parent);

	// the column we drag from
	Columnview *old_cview = COLUMNVIEW(VIEW(old_sview)->parent);

	// note the source of the drag, so we can undo
	wview->old_sview = old_sview;

	// position of the label in workspace cods
	graphene_rect_t bounds;
	if (!gtk_widget_compute_bounds(rview->frame, wview->fixed, &bounds))
		return;

	// therefore object position at start of drag
	wview->floating = COLUMNVIEW(columnview_new());
	wview->floating->x = bounds.origin.x;
	wview->floating->y = bounds.origin.y;
	view_child_add(VIEW(wview), VIEW(wview->floating));

	Subcolumnview *sview = SUBCOLUMNVIEW(subcolumnview_new());
	view_child_add(VIEW(wview->floating), VIEW(sview));

	// put a shadow in the subcolumn where this row was
	Row *row = ROW(VOBJECT(rview)->iobject);
	workspaceview_add_row_shadow(wview, bounds.size.height,
		old_cview, 2 * ICONTAINER(row)->pos + 1);

	// reparent the rowview to the floating columnview
	g_object_ref(rview);

	view_child_remove(VIEW(rview));
	// the row number has to be wrong or reattach is skipped
	rview->rnum = -1;
	view_child_add(VIEW(sview), VIEW(rview));

	g_object_unref(rview);

	// force a refresh to get everything in the right place
	vobject_refresh(VOBJECT(wview->drag_rview));
	vobject_refresh(VOBJECT(wview->floating));
}

static void
workspaceview_drop_rowview(Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	// the row and floating column we are dragging
	Rowview *rview = wview->drag_rview;
	Row *row = ROW(VOBJECT(rview)->iobject);
	Column *row_col = COLUMN(ICONTAINER(row)->parent->parent);
	int old_pos = ICONTAINER(row)->pos;

	// the column we are dragging to --- if we are dropping on the background,
	// we need to make a new column for the destination
	Column *col;
	int new_pos;

	if (wview->row_shadow_column) {
		// drop on an existing column
		Columnview *cview = wview->row_shadow_column;

		col = COLUMN(VOBJECT(cview)->iobject);
		new_pos = wview->row_shadow_position / 2;
	}
	else {
		char new_name[MAX_STRSIZE];
		workspace_column_name_new(ws, new_name);
		int x = wview->floating ? wview->floating->x : -1;
		int y = wview->floating ? wview->floating->y : -1;

		col = column_new(ws, new_name, x, y);
		new_pos = -1;
	}

	// reparent the rowview back to the original column ... this is where
	// icontainer_reparent() expects to find it
	g_object_ref(rview);

	g_assert(IS_SUBCOLUMNVIEW(wview->old_sview));
	g_assert(IS_ROWVIEW(rview));

	view_child_remove(VIEW(rview));
	// the row number has to be wrong or reattach is skipped
	rview->rnum = -1;
	view_child_add(VIEW(wview->old_sview), VIEW(rview));

	g_object_unref(rview);

	// update the model
	if (col == row_col) {
		// move within one column

		/* new_pos is in rnum numbering, ie. the numbering BEFORE we
		 * removed this row and started dragging it.
		 *
		 * The pos argument for icontainer_child_move() is interpreted
		 * AFTER removing the old child. So to allow for that, we must
		 * subtract 1 if new_pos > old_pos.
		 */
		if (new_pos > old_pos)
			new_pos -= 1;

		icontainer_child_move(ICONTAINER(row), new_pos);
	}
	else
		// different column ... we must reparent the row model
		icontainer_reparent(ICONTAINER(col->scol),
			ICONTAINER(row), new_pos);

	workspaceview_remove_row_shadow(wview);
	workspace_queue_layout(ws);
	filemodel_set_modified(FILEMODEL(wsg), TRUE);
}

static void
workspaceview_drag_begin(GtkEventControllerMotion *self,
	gdouble start_x, gdouble start_y, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);

#ifdef DEBUG_VERBOSE
	printf("workspaceview_drag_begin: %g x %g\n", start_x, start_y);
#endif /*DEBUG_VERBOSE*/

	switch (wview->state) {
	case WVIEW_WAIT:
		/* Search for a column titlebar we could be hitting.
		 */
		Columnview *title = workspaceview_find_columnview_title(wview,
			start_x, start_y);

		/* Search for a click on any part of a columnview.
		 */
		Columnview *cview = workspaceview_find_columnview(wview,
			start_x, start_y);

		if (title) {
			wview->drag_cview = title;
			wview->state = WVIEW_SELECT;
			wview->obj_x = cview->x;
			wview->obj_y = cview->y;
		}
		else if (cview) {
			Rowview *rview = columnview_find_rowview(cview, start_x, start_y);

			if (rview) {
				graphene_rect_t bounds;
				if (!gtk_widget_compute_bounds(GTK_WIDGET(rview->frame),
						wview->fixed, &bounds))
					return;

				wview->drag_rview = rview;
				wview->state = WVIEW_SELECT;
				wview->obj_x = bounds.origin.x;
				wview->obj_y = bounds.origin.y;
			}
		}
		else {
			// drag on background
			wview->drag_cview = NULL;
			wview->state = WVIEW_SELECT;
		}

		wview->start_x = start_x;
		wview->start_y = start_y;

		break;

	case WVIEW_SELECT:
	case WVIEW_DRAG:
		break;

	default:
		g_assert(FALSE);
	}
}

static void
workspaceview_drag_update(GtkEventControllerMotion *self,
	gdouble offset_x, gdouble offset_y, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

#ifdef DEBUG_VERBOSE
	printf("workspaceview_drag_update: %g x %g\n", offset_x, offset_y);
#endif /*DEBUG_VERBOSE*/

	switch (wview->state) {
	case WVIEW_WAIT:
		break;

	case WVIEW_SELECT:
		if (abs(offset_x) > 5 || abs(offset_x) > 5) {
			wview->state = WVIEW_DRAG;

			if (wview->drag_cview)
				columnview_add_shadow(wview->drag_cview);
			else if (wview->drag_rview) {
				workspaceview_float_rowview(wview, wview->drag_rview);
				wview->drag_cview = wview->floating;
			}
		}

		break;

	case WVIEW_DRAG:
		if (wview->drag_cview) {
			int x, y, w, h;
			columnview_get_position(wview->drag_cview, &x, &y, &w, &h);

			int obj_x = wview->obj_x + offset_x;
			int obj_y = wview->obj_y + offset_y;

			// don't let x/y go -ve (layout hates it)
			columnview_animate_to(wview->drag_cview,
				VIPS_CLIP(0, VIPS_RINT(obj_x), wview->width - w),
				VIPS_CLIP(0, VIPS_RINT(obj_y), wview->height - h));

			// top, since we want the titlebar to be visible
			view_scrollto(VIEW(wview->drag_cview), MODEL_SCROLL_TOP);

			if (wview->drag_rview) {
				// use the mouse position to pick the drop point
				int mouse_x = wview->start_x + offset_x;
				int mouse_y = wview->start_y + offset_y;

				// column we are currently over
				Columnview *cview = workspaceview_find_columnview(wview,
					mouse_x, mouse_y);

				if (cview) {
					// row we are over
					Rowview *rview = columnview_find_rowview(cview,
						mouse_x, mouse_y);

					if (rview) {
						graphene_rect_t bounds;
						if (!gtk_widget_compute_bounds(GTK_WIDGET(rview->frame),
								wview->fixed, &bounds))
							return;

						Row *row = ROW(VOBJECT(rview)->iobject);
						int pos = 2 * ICONTAINER(row)->pos + 1;
						if (mouse_y > bounds.origin.y + bounds.size.height / 2)
							workspaceview_move_row_shadow(wview,
								cview, pos + 1);
						else
							workspaceview_move_row_shadow(wview,
								cview, pos - 1);
					}
				}
				else
					workspaceview_move_row_shadow(wview, NULL, -1);
			}

			// move other columns about (won't touch drag_cview)
			model_layout(MODEL(ws));
		}
		else {
			// we don't want to drag the window with its own coordinate
			// system, we'll get feedback
			graphene_point_t fixed = GRAPHENE_POINT_INIT(offset_x, offset_y);
			graphene_point_t screen;
			if (!gtk_widget_compute_point(wview->fixed, wview->scrolled_window,
					&fixed, &screen))
				return;

			// drag on background
			workspaceview_scroll_to(wview, -screen.x, -screen.y);
		}

		break;

	default:
		g_assert(FALSE);
	}
}

static void
workspaceview_drag_end(GtkEventControllerMotion *self,
	gdouble offset_x, gdouble offset_y, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

#ifdef DEBUG_VERBOSE
	printf("workspaceview_drag_end: %g x %g\n", offset_x, offset_y);
#endif /*DEBUG_VERBOSE*/

	/* Back to wait.
	 */
	switch (wview->state) {
	case WVIEW_SELECT:
		if (wview->drag_cview) {
			Column *col = COLUMN(VOBJECT(wview->drag_cview)->iobject);

			workspace_column_select(ws, col);
			column_scrollto(col, MODEL_SCROLL_TOP);
		}

		wview->state = WVIEW_WAIT;

		break;

	case WVIEW_DRAG:
		wview->state = WVIEW_WAIT;

		if (wview->drag_rview) {
			// we were dragging a row
			workspaceview_drop_rowview(wview);

			Subcolumn *old_scol = SUBCOLUMN(VOBJECT(wview->old_sview)->iobject);
			Column *old_col = COLUMN(ICONTAINER(old_scol)->parent);
			if (icontainer_get_n_children(ICONTAINER(old_scol)) == 0) {
				printf("workspaceview_drag_end: destroying column %s\n",
					IOBJECT(old_col)->name);
				iobject_destroy(IOBJECT(old_col));
			}

			workspaceview_unfloat(wview);
			workspace_queue_layout(ws);
			workspace_set_modified(ws, TRUE);
		}
		else if (wview->drag_cview) {
			// we were dragging a column
			columnview_remove_shadow(wview->drag_cview);
			workspace_queue_layout(ws);
			workspace_set_modified(ws, TRUE);
		}

		break;

	case WVIEW_WAIT:
		break;

	default:
		g_assert(FALSE);
	}

	wview->drag_cview = NULL;
	wview->drag_rview = NULL;
	wview->old_sview = NULL;
}

static void
workspaceview_error_close_clicked(GtkButton *button, void *user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);

	gtk_action_bar_set_revealed(GTK_ACTION_BAR(wview->error_bar), FALSE);
}

static void
workspaceview_class_init(WorkspaceviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("workspaceview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(Workspaceview, top);
	BIND_VARIABLE(Workspaceview, error_bar);
	BIND_VARIABLE(Workspaceview, error_top);
	BIND_VARIABLE(Workspaceview, error_sub);
	BIND_VARIABLE(Workspaceview, scrolled_window);
	BIND_VARIABLE(Workspaceview, fixed);

	BIND_CALLBACK(workspaceview_click);
	BIND_CALLBACK(workspaceview_drag_begin);
	BIND_CALLBACK(workspaceview_drag_update);
	BIND_CALLBACK(workspaceview_drag_end);
	BIND_CALLBACK(workspaceview_error_close_clicked);

	object_class->dispose = workspaceview_dispose;

	widget_class->realize = workspaceview_realize;

	vobject_class->refresh = workspaceview_refresh;

	view_class->link = workspaceview_link;
	view_class->child_add = workspaceview_child_add;
	view_class->child_remove = workspaceview_child_remove;
	view_class->child_position = workspaceview_child_position;
	view_class->child_front = workspaceview_child_front;
	view_class->layout = workspaceview_layout;
	view_class->action = workspaceview_action;
}

static void
workspaceview_scroll_changed(GtkAdjustment *adj, Workspaceview *wview)
{
	workspaceview_scroll_update(wview);
}

static void
workspaceview_init(Workspaceview *wview)
{
	gtk_widget_init_template(GTK_WIDGET(wview));

	wview->label = workspaceviewlabel_new(wview);

	GtkScrolledWindow *scrolled_window =
		GTK_SCROLLED_WINDOW(wview->scrolled_window);
	wview->hadj = gtk_scrolled_window_get_hadjustment(scrolled_window);
	wview->vadj = gtk_scrolled_window_get_vadjustment(scrolled_window);
	g_signal_connect_object(G_OBJECT(wview->hadj), "value_changed",
		G_CALLBACK(workspaceview_scroll_changed), wview, 0);
	g_signal_connect_object(G_OBJECT(wview->hadj), "changed",
		G_CALLBACK(workspaceview_scroll_changed), wview, 0);
	g_signal_connect_object(G_OBJECT(wview->vadj), "value_changed",
		G_CALLBACK(workspaceview_scroll_changed), wview, 0);
	g_signal_connect_object(G_OBJECT(wview->vadj), "changed",
		G_CALLBACK(workspaceview_scroll_changed), wview, 0);

	wview->should_animate = TRUE;

	// otherwise we get a lot of annoying scrolling on notebook tab change
	GtkWidget *child = gtk_scrolled_window_get_child(scrolled_window);
	if (GTK_IS_VIEWPORT(child))
		gtk_viewport_set_scroll_to_focus(GTK_VIEWPORT(child), FALSE);

	// a lot of stuff to go in here
	printf("workspaceview_init: FIXME we must do stuff\n");
}

View *
workspaceview_new(void)
{
	Workspaceview *wview = g_object_new(WORKSPACEVIEW_TYPE, NULL);

	return VIEW(wview);
}
