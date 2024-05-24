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
#define DEBUG
 */

#include "nip4.h"

G_DEFINE_TYPE(Workspaceview, workspaceview, VIEW_TYPE)

/* Params for "Align Columns" function.
 */
static const int workspaceview_layout_snap_threshold = 40;
static const int workspaceview_layout_hspacing = 10;
static const int workspaceview_layout_vspacing = 10;
static const int workspaceview_layout_left = WORKSPACEVIEW_MARGIN_LEFT;
static const int workspaceview_layout_top = WORKSPACEVIEW_MARGIN_TOP;

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

static gboolean
workspaceview_tick(GtkWidget *widget, GdkFrameClock *frame_clock,
	gpointer user_data)
{
	const double animation_duration = 0.5;

    Workspaceview *wview = WORKSPACEVIEW(user_data);

    gint64 frame_time = gdk_frame_clock_get_frame_time(frame_clock);
    double dt = wview->last_frame_time > 0 ?
        (double) (frame_time - wview->last_frame_time) / G_TIME_SPAN_SECOND :
        1.0 / G_TIME_SPAN_SECOND;

#ifdef DEBUG
    printf("workspaceview_tick: dt = %g\n", dt);
#endif /*DEBUG*/

	gboolean finished = TRUE;

	for (GSList *p = VIEW(wview)->children; p; p = p->next) {
		Columnview *cview = COLUMNVIEW(p->data);
		Column *col = COLUMN(VOBJECT(cview)->iobject);

		int x;
		int y;

		if (!cview->animating)
			continue;

		// elapsed time in seconds
		cview->elapsed += dt;

		// 0-1 progress in animation
		double duration = wview->should_animate ?
			animation_duration : cview->elapsed;
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

static void
workspaceview_scroll_to(Workspaceview *wview, int x, int y)
{
	int nx, ny;

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

#ifdef DEBUG
	printf("workspaceview_scroll: x=%d, y=%d, w=%d, h=%d\n", x, y, w, h);
#endif /*DEBUG*/

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

	gtk_adjustment_set_value(wview->hadj, nx);
	gtk_adjustment_set_value(wview->vadj, ny);
}

/* Update our geometry from the fixed widget.
 */
static void
workspaceview_scroll_update(Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

	wview->vp.left = gtk_adjustment_get_value(wview->hadj);
	wview->vp.top = gtk_adjustment_get_value(wview->vadj);
	wview->vp.width = gtk_adjustment_get_page_size(wview->hadj);
	wview->vp.height = gtk_adjustment_get_page_size(wview->vadj);

	wview->width = gtk_adjustment_get_upper(wview->hadj);
	wview->height = gtk_adjustment_get_upper(wview->vadj);

	/* Update vp hint in model too.
	 */
	ws->vp = wview->vp;

#ifdef DEBUG
	printf("workspaceview_scroll_update: %s\n", IOBJECT(ws)->name);
	printf("  wview->vp: l=%d, t=%d, w=%d, h=%d; fixed w=%d; h=%d\n",
		wview->vp.left, wview->vp.top,
		wview->vp.width, wview->vp.height,
		wview->width, wview->height);
#endif /*DEBUG*/
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
static Columnview *
workspaceview_find_columnview(Workspaceview *wview,
	int start_x, int start_y)
{
	graphene_point_t point = GRAPHENE_POINT_INIT(start_x, start_y);

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
static Columnview *
workspaceview_find_columnview_title(Workspaceview *wview,
	int start_x, int start_y)
{
	graphene_point_t point = GRAPHENE_POINT_INIT(start_x, start_y);

	return view_map(VIEW(wview),
		workspaceview_columnview_title_hit, &point, wview);
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

/* Pick an xy position for the next column.
 */
static void
workspaceview_pick_xy(Workspaceview *wview, int *x, int *y)
{
	/* Only if there's no position set yet.
	 */
	if (*x < 0) {
		*x = wview->next_x + wview->vp.left;
		*y = wview->next_y + wview->vp.top;

		/* And move on.
		 */
		wview->next_x += 30;
		wview->next_y += 30;
		if (wview->next_x > 300)
			wview->next_x = 3;
		if (wview->next_y > 200)
			wview->next_y = 3;
	}
}

static void
workspaceview_link(View *view, Model *model, View *parent)
{
	Workspaceview *wview = WORKSPACEVIEW(view);
	Workspace *ws = WORKSPACE(model);

	VIEW_CLASS(workspaceview_parent_class)->link(view, model, parent);

	printf("workspaceview_link: FIXME toolkitbrowser, panes, etc.\n");
	// vobject_link(VOBJECT(wview->toolkitbrowser), IOBJECT(ws->kitg));
	// vobject_link(VOBJECT(wview->workspacedefs), IOBJECT(ws));
	// toolkitbrowser_set_workspace(wview->toolkitbrowser, ws);
	//  pane_set_state(wview->rpane, ws->rpane_open, ws->rpane_position);
	//  pane_set_state(wview->lpane, ws->lpane_open, ws->lpane_position);
}

static void
workspaceview_child_add(View *parent, View *child)
{
	Columnview *cview = COLUMNVIEW(child);
	Column *column = COLUMN(VOBJECT(cview)->iobject);
	Workspaceview *wview = WORKSPACEVIEW(parent);

	VIEW_CLASS(workspaceview_parent_class)->child_add(parent, child);

	/* Pick start xy pos.
	 */
	workspaceview_pick_xy(wview, &column->x, &column->y);
	gtk_fixed_put(GTK_FIXED(wview->fixed), GTK_WIDGET(cview),
		column->x, column->y);
	columnview_animate_to(cview, column->x, column->y);
}

static void
workspaceview_child_remove(View *parent, View *child)
{
	Workspaceview *wview = WORKSPACEVIEW(parent);

	VIEW_CLASS(workspaceview_parent_class)->child_remove(parent, child);

	// must be at the end since this will unref the child
	gtk_fixed_remove(GTK_FIXED(wview->fixed), GTK_WIDGET(child));
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
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	g_object_ref(cview);

	gtk_fixed_remove(GTK_FIXED(wview->fixed), GTK_WIDGET(cview));
	gtk_fixed_put(GTK_FIXED(wview->fixed), GTK_WIDGET(cview), col->x, col->y);

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

	workspace_jump_update(ws, wview->popup_jump);

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
	int x, y, w, h;

	columnview_get_position(cview, &x, &y, &w, &h);
	if (x < layout->area.left) {
		layout->area.left = x;
		layout->cview = cview;
	}

	return NULL;
}

static void *
workspaceview_layout_find_similar_x(Columnview *cview,
	WorkspaceLayout *layout)
{
	int x, y, w, h;
	gboolean snap;

	/* Special case: a colum at zero makes a new column on the far left.
	 */
	columnview_get_position(cview, &x, &y, &w, &h);
	snap = FALSE;
	if (layout->area.left == 0 &&
		x == 0)
		snap = TRUE;

	if (layout->area.left > 0 &&
		ABS(x - layout->area.left) < workspaceview_layout_snap_threshold)
		snap = TRUE;

	if (snap) {
		layout->current_columns =
			g_slist_prepend(layout->current_columns, cview);
		layout->area.width = VIPS_MAX(layout->area.width, w);
	}

	return NULL;
}

/* Compare func for row recomp sort.
 */
static int
workspaceview_layout_sort_y(Columnview *a, Columnview *b)
{
	int ax, ay, aw, ah;
	int bx, by, bw, bh;

	columnview_get_position(a, &ax, &ay, &aw, &ah);
	columnview_get_position(b, &bx, &by, &bw, &bh);

	return ay - by;
}

static void *
workspaceview_layout_set_pos(Columnview *cview, WorkspaceLayout *layout)
{
	Column *column = COLUMN(VOBJECT(cview)->iobject);

	/* columnview_refresh() will move the views and the shadow.
	 */
	column->x = layout->out_x;
	column->y = layout->out_y;
	iobject_changed(IOBJECT(column));

	int x, y, w, h;
	columnview_get_position(cview, &x, &y, &w, &h);
	layout->out_y += h + workspaceview_layout_vspacing;

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
	int x, y, w, h;

	layout->cview = NULL;
	layout->area.left = INT_MAX;
	slist_map(layout->undone_columns,
		(SListMapFn) workspaceview_layout_find_leftmost, layout);

	layout->current_columns = NULL;
	columnview_get_position(layout->cview, &x, &y, &w, &h);
	layout->area.width = w;
	slist_map(layout->undone_columns,
		(SListMapFn) workspaceview_layout_find_similar_x, layout);

	layout->current_columns = g_slist_sort(layout->current_columns,
		(GCompareFunc) workspaceview_layout_sort_y);

	layout->out_y = workspaceview_layout_top;
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
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

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
workspaceview_action(GSimpleAction *action, GVariant *parameter, View *view)
{
	Workspaceview *wview = WORKSPACEVIEW(view);
	const char *name = g_action_get_name(G_ACTION(action));

	printf("workspaceview_action: %s\n", name);

	if (g_str_equal(name, "column-new")) {
		Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

		workspace_column_new(ws);
	}
}

static void
workspaceview_click(GtkGestureClick *gesture,
	guint n_press, double x, double y, Workspaceview *wview)
{
	Columnview *cview = workspaceview_find_columnview(wview, x, y);

	printf("workspaceview_click:\n");

	if (!cview) {
		Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

		workspace_deselect_all(ws);
	}

	// we detect single click in drag_update
}

static void
workspaceview_background_menu(GtkGestureClick *gesture,
	guint n_press, double x, double y, Workspaceview *wview)
{
	// gets picked up by menu actions
	mainwindow_set_action_view(VIEW(wview));

	gtk_popover_set_pointing_to(GTK_POPOVER(wview->right_click_menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	gtk_popover_popup(GTK_POPOVER(wview->right_click_menu));
}

static void
workspaceview_drag_begin(GtkEventControllerMotion *self,
	gdouble start_x, gdouble start_y, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(user_data);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

#ifdef DEBUG
	printf("workspaceview_drag_begin: %g x %g\n", start_x, start_y);
#endif /*DEBUG*/

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
			Column *col = COLUMN(VOBJECT(title)->iobject);

			wview->drag_cview = title;
			wview->state = WVIEW_SELECT;
			wview->obj_x = cview->x;
			wview->obj_y = cview->y;
		}
		else if (!cview) {
			// drag on background ... note the scroll position of the fixed
			// at the start of drag
			wview->drag_cview = NULL;
			wview->state = WVIEW_SELECT;
			wview->obj_x = wview->vp.left;
			wview->obj_y = wview->vp.top;
		}
		break;

	case WVIEW_SELECT:
	case WVIEW_DRAG:
	case WVIEW_EDIT:
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

#ifdef DEBUG
	printf("workspaceview_drag_update: %g x %g\n", offset_x, offset_y);
#endif /*DEBUG*/

	switch (wview->state) {
	case WVIEW_EDIT:
	case WVIEW_WAIT:
		break;

	case WVIEW_SELECT:
		if (abs(offset_x) > 5 || abs(offset_x) > 5) {
			wview->state = WVIEW_DRAG;

			if (wview->drag_cview)
				columnview_add_shadow(wview->drag_cview);
		}

		break;

	case WVIEW_DRAG:
		if (wview->drag_cview) {
			Column *col = COLUMN(VOBJECT(wview->drag_cview)->iobject);

			int x, y, w, h;
			columnview_get_position(wview->drag_cview, &x, &y, &w, &h);

			// don't let x/y go -ve (layout hates it)
			columnview_animate_to(wview->drag_cview,
				VIPS_CLIP(0,
					VIPS_RINT(wview->obj_x + offset_x), wview->width - w),
				VIPS_CLIP(0,
					VIPS_RINT(wview->obj_y + offset_y), wview->height - h));

			// move other columns about (won't touch drag_cview)
			model_layout(MODEL(ws));

			// top, since we want the titlebar to be visible
			view_scrollto(wview->drag_cview, MODEL_SCROLL_TOP);
		}
		else {
			// we don't want to drag the window with its own coordinate
			// system, we'll get feedback
			graphene_point_t fixed = GRAPHENE_POINT_INIT(offset_x, offset_y);
			graphene_point_t screen;
			if (!gtk_widget_compute_point(wview->fixed, GTK_WIDGET(wview),
					&fixed, &screen))
				return;

			// drag on background
			int new_x = VIPS_MAX(0, VIPS_RINT(wview->obj_x - screen.x));
			int new_y = VIPS_MAX(0, VIPS_RINT(wview->obj_y - screen.y));
			workspaceview_scroll_to(wview, new_x, new_y);
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

#ifdef DEBUG
	printf("workspaceview_drag_end: %g x %g\n", offset_x, offset_y);
#endif /*DEBUG*/

	/* Back to wait.
	 */
	switch (wview->state) {
	case WVIEW_SELECT:
		wview->state = WVIEW_WAIT;
		if (wview->drag_cview) {
			Column *col = COLUMN(VOBJECT(wview->drag_cview)->iobject);

			workspace_column_select(ws, col);
			column_scrollto(col, MODEL_SCROLL_TOP);
			workspace_queue_layout(ws);
		}

		break;

	case WVIEW_DRAG:
		wview->state = WVIEW_WAIT;
		if (wview->drag_cview)
			columnview_remove_shadow(wview->drag_cview);
		model_layout(MODEL(ws));
		workspace_set_modified(ws, TRUE);

		break;

	case WVIEW_EDIT:
	case WVIEW_WAIT:
		break;

	default:
		g_assert(FALSE);
	}

	wview->drag_cview = NULL;
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

	BIND_VARIABLE(Workspaceview, scrolled_window);
	BIND_VARIABLE(Workspaceview, fixed);
	BIND_VARIABLE(Workspaceview, right_click_menu);
	BIND_VARIABLE(Workspaceview, rowview_menu);

	BIND_CALLBACK(workspaceview_click);
	BIND_CALLBACK(workspaceview_background_menu);
	BIND_CALLBACK(workspaceview_drag_begin);
	BIND_CALLBACK(workspaceview_drag_update);
	BIND_CALLBACK(workspaceview_drag_end);

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

/* Can't use main_load(), we want to select wses after load.
 */
static gboolean
workspaceview_load(Workspace *ws, const char *filename)
{
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	Workspaceroot *wsr = wsg->wsr;

	Workspacegroup *new_wsg;

	printf("workspaceview_load: FIXME ... need to get app somehow?\n");
	// or maybe don't make mainw here, but one step up?
	if ((new_wsg = workspaceroot_open_workspace(wsr, NULL, filename)))
		return TRUE;

	error_clear();

	/* workspace_load_file() needs to recalc to work, try to avoid that by
	 * doing .defs first.
	 */
	printf("workspaceview_load: def filetype test FIXME\n");
	if (vips_iscasepostfix(filename, ".def")) {
		if (toolkit_new_from_file(main_toolkitgroup, filename))
			return TRUE;

		error_clear();
	}

	/* Try as matrix or image. Have to do these via definitions.
	 */
	if (workspace_load_file(ws, filename))
		return TRUE;

	error_clear();

	error_top(_("Unknown file type"));
	error_sub(_("Unable to load \"%s\"."), filename);

	return FALSE;
}

static void
workspaceview_lpane_changed_cb(Pane *pane, Workspaceview *wview)
{
	Workspace *ws;

	if ((ws = WORKSPACE(VOBJECT(wview)->iobject)))
		if (ws->lpane_open != pane_get_open(pane) ||
			ws->lpane_position != pane_get_position(pane)) {
			ws->lpane_open = pane_get_open(pane);
			ws->lpane_position = pane_get_position(pane);

			prefs_set("WORKSPACE_LPANE_OPEN", "%d", ws->lpane_open);
			prefs_set("WORKSPACE_LPANE_POSITION", "%d", ws->lpane_position);

			iobject_changed(IOBJECT(ws));
		}
}

static void
workspaceview_rpane_changed_cb(Pane *pane, Workspaceview *wview)
{
	Workspace *ws;

	if ((ws = WORKSPACE(VOBJECT(wview)->iobject)))
		if (ws->rpane_open != pane_get_open(pane) ||
			ws->rpane_position != pane_get_position(pane)) {
			ws->rpane_open = pane_get_open(pane);
			ws->rpane_position = pane_get_position(pane);

			prefs_set("WORKSPACE_RPANE_OPEN", "%d", ws->rpane_open);
			prefs_set("WORKSPACE_RPANE_POSITION", "%d", ws->rpane_position);

			iobject_changed(IOBJECT(ws));
		}
}

static gboolean
workspaceview_filedrop(Workspaceview *wview, const char *filename)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

	gboolean result;

	result = workspaceview_load(ws, filename);
	if (result)
		symbol_recalculate_all();

	return result;
}

static void
workspaceview_column_new_action_cb2(GtkWidget *wid, GtkWidget *host,
	Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

	if (!workspace_column_new(ws))
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(wview))));
}

static void
workspaceview_group_action_cb2(GtkWidget *wid, GtkWidget *host,
	Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

	workspace_selected_group(ws);
}

static void
workspaceview_next_error_action_cb2(GtkWidget *wid, GtkWidget *host,
	Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

	if (!workspace_next_error(ws)) {
		error_top(_("No errors in tab"));
		error_sub("%s", _("There are no errors (that I can see) in this tab."));
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(wview))));
	}
}

static void
workspaceview_scroll_adjustment_cb(GtkAdjustment *adj, Workspaceview *wview)
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
	g_signal_connect(G_OBJECT(wview->hadj), "value_changed",
		G_CALLBACK(workspaceview_scroll_adjustment_cb), wview);
	g_signal_connect(G_OBJECT(wview->hadj), "changed",
		G_CALLBACK(workspaceview_scroll_adjustment_cb), wview);
	g_signal_connect(G_OBJECT(wview->vadj), "value_changed",
		G_CALLBACK(workspaceview_scroll_adjustment_cb), wview);
	g_signal_connect(G_OBJECT(wview->vadj), "changed",
		G_CALLBACK(workspaceview_scroll_adjustment_cb), wview);

	wview->should_animate = TRUE;

	// a lot of stuff to go in here
	printf("workspaceview_init: FIXME we must do stuff\n");
}

View *
workspaceview_new(void)
{
	Workspaceview *wview = g_object_new(WORKSPACEVIEW_TYPE, NULL);

	return VIEW(wview);
}

GtkWidget *
workspaceview_build_label(Workspaceview *wview)
{
}
