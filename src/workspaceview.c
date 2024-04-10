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
 */
#define DEBUG

/* Define to trace button press events.
#define EVENT
 */

#include "nip4.h"

G_DEFINE_TYPE(Workspaceview, workspaceview, VIEW_TYPE)

/* Params for "Align Columns" function.
 */
static const int workspaceview_layout_snap_threshold = 30;
static const int workspaceview_layout_hspacing = 10;
static const int workspaceview_layout_vspacing = 10;
static const int workspaceview_layout_left = WORKSPACEVIEW_MARGIN_LEFT;
static const int workspaceview_layout_top = WORKSPACEVIEW_MARGIN_TOP;

static void
workspaceview_scroll_to(Workspaceview *wview, int x, int y)
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(wview->window));
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(wview->window));
	int nx, ny;

	nx = VIPS_CLIP(0, x, wview->width - wview->vp.width);
	ny = VIPS_CLIP(0, y, wview->height - wview->vp.height);

	gtk_adjustment_set_value(hadj, nx);
	gtk_adjustment_set_value(vadj, ny);
}

/* Scroll by an amount horizontally and vertically.
 */
static void
workspaceview_displace(Workspaceview *wview, int u, int v)
{
	workspaceview_scroll_to(wview, wview->vp.left + u, wview->vp.top + v);
}

/* Scroll to make an xywh area visible. If the area is larger than the
 * viewport, position the view at the bottom left if the xywh area ...
 * this is usually right for workspaces.
 */
void
workspaceview_scroll(Workspaceview *wview, int x, int y, int w, int h)
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(wview->window));
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(wview->window));
	VipsRect *vp = &wview->vp;
	int nx, ny;

	nx = gtk_adjustment_get_value(hadj);
	if (x + w > VIPS_RECT_RIGHT(vp))
		nx = VIPS_MAX(0, (x + w) - vp->width);
	if (x < nx)
		nx = x;

	ny = gtk_adjustment_get_value(vadj);
	if (y + h > VIPS_RECT_BOTTOM(vp))
		ny = VIPS_MAX(0, (y + h) - vp->height);
	if (y < ny)
		ny = y;

#ifdef DEBUG
	printf("workspaceview_scroll: x=%d, y=%d, w=%d, h=%d, "
		   "nx = %d, ny = %d\n",
		x, y, w, h, nx, ny);
#endif /*DEBUG*/

	gtk_adjustment_set_value(hadj, nx);
	gtk_adjustment_set_value(vadj, ny);
}

/* Update our geometry from the fixed widget.
 */
static void
workspaceview_scroll_update(Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(wview->window));
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(wview->window));

	wview->vp.left = gtk_adjustment_get_value(hadj);
	wview->vp.top = gtk_adjustment_get_value(vadj);
	wview->vp.width = gtk_adjustment_get_page_size(hadj);
	wview->vp.height = gtk_adjustment_get_page_size(vadj);

	wview->width = gtk_adjustment_get_upper(hadj);
	wview->height = gtk_adjustment_get_upper(vadj);

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

typedef struct _WorkspaceviewFindColumnview {
	Workspaceview *wview;
	int x;
	int y;
} WorkspaceviewFindColumnview;

static void *
workspaceview_find_columnview_sub(View *view,
	WorkspaceviewFindColumnview *args)
{
	Columnview *cview = COLUMNVIEW(view);
	VipsRect col;
	int x, y, w, h;

	columnview_get_position(cview, &x, &y, &w, &h);
	col.left = x;
	col.top = y;
	col.width = w;
	col.height = h;

	if (vips_rect_includespoint(&col, args->x, args->y))
		return cview;

	return NULL;
}

/* Test for a point is workspaceview background ... ie. is not enclosed by one
 * of our columns.
 */
static Columnview *
workspaceview_find_columnview(Workspaceview *wview, int x, int y)
{
	WorkspaceviewFindColumnview args;
	void *res;

	args.wview = wview;
	args.x = x;
	args.y = y;

	res = view_map(VIEW(wview),
		(view_map_fn) workspaceview_find_columnview_sub, &args, NULL);

	if (res)
		return COLUMNVIEW(res);
	else
		return NULL;
}

static void
workspaceview_scroll_adjustment_cb(GtkAdjustment *adj, Workspaceview *wview)
{
	workspaceview_scroll_update(wview);
}

/* Timer callback for background scroll.
 */
static gboolean
workspaceview_scroll_time_cb(Workspaceview *wview)
{
	/* Perform scroll.
	 */
	workspaceview_scroll_update(wview);
	if (wview->u != 0 || wview->v != 0)
		workspaceview_displace(wview, wview->u, wview->v);

	/* Start timer again.
	 */
	return TRUE;
}

/* Stop the tally_scroll timer.
 */
static void
workspaceview_scroll_stop(Workspaceview *wview)
{
	VIPS_FREEF(g_source_remove, wview->timer);
}

/* Start the tally_scroll timer.
 */
static void
workspaceview_scroll_start(Workspaceview *wview)
{
	workspaceview_scroll_stop(wview);
	wview->timer = g_timeout_add(30,
		(GSourceFunc) workspaceview_scroll_time_cb, wview);
}

/* Set a background scroll. Pass both zero to stop scroll.
 */
void
workspaceview_scroll_background(Workspaceview *wview, int u, int v)
{
	wview->u = u;
	wview->v = v;

	if (u == 0 && v == 0)
		workspaceview_scroll_stop(wview);
	else
		workspaceview_scroll_start(wview);
}

static void
workspaceview_dispose(GObject *object)
{
	Workspaceview *wview;

#ifdef DEBUG
	printf("workspaceview_destroy: %p\n", object);
#endif /*DEBUG*/

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_WORKSPACEVIEW(object));

	wview = WORKSPACEVIEW(object);

	/* Instance destroy.
	 */
	workspaceview_scroll_stop(wview);
	FREESID(wview->watch_changed_sid, main_watchgroup);
	VIPS_UNREF(wview->popup);
	UNPARENT(wview->tab_menu);
	UNPARENT(wview->right_click_menu);

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

static void *
workspaceview_child_size_sub(Columnview *cview, VipsRect *area)
{
	int x, y, w, h;
	VipsRect col;

	columnview_get_position(cview, &x, &y, &w, &h);

	col.left = x;
	col.top = y;
	col.width = w;
	col.height = h;

	vips_rect_unionrect(area, &col, area);

	return NULL;
}

static void
workspaceview_child_size_cb(Columnview *cview,
	GtkAllocation *allocation, Workspaceview *wview)
{
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	int right, bottom;

	g_assert(IS_WORKSPACEVIEW(wview));

	/* Compute a new bounding box for our children.
	 */
	wview->bounding.left = 0;
	wview->bounding.top = 0;
	wview->bounding.width = 0;
	wview->bounding.height = 0;

	(void) view_map(VIEW(wview),
		(view_map_fn) workspaceview_child_size_sub,
		&wview->bounding, NULL);

	wview->bounding.width += 1000;
	wview->bounding.height += 1000;

#ifdef DEBUG
	{
		Column *col = COLUMN(VOBJECT(cview)->iobject);

		printf("workspaceview_child_size_cb: cview %s "
			   "bb left=%d, top=%d, width=%d, height=%d\n",
			IOBJECT(col)->name,
			wview->bounding.left,
			wview->bounding.top,
			wview->bounding.width,
			wview->bounding.height);
	}
#endif /*DEBUG*/

	/* Resize our fixed if necessary.
	 */
	right = VIPS_RECT_RIGHT(&wview->bounding);
	bottom = VIPS_RECT_BOTTOM(&wview->bounding);
	if (right != wview->width || bottom != wview->height) {
		gtk_widget_set_size_request(GTK_WIDGET(wview->fixed),
			right, bottom);

		/* Update the model hints ... it uses bounding to position
		 * loads and saves.
		 */
		ws->area = wview->bounding;
		filemodel_set_offset(FILEMODEL(wsg),
			ws->area.left, ws->area.top);
	}
}

/* Pick an xy position for the next column.
 */
static void
workspaceview_pick_xy(Workspaceview *wview, int *x, int *y)
{
	/* Position already set? No change.
	 */
	if (*x >= 0)
		return;

	/* Set this position.
	 */
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

	printf("workspaceview_child_add: watch resize of columns\n");
	// g_signal_connect(child, "size_allocate",
	// G_CALLBACK(workspaceview_child_size_cb), parent);

	VIEW_CLASS(workspaceview_parent_class)->child_add(parent, child);

	/* Pick start xy pos.
	 */
	workspaceview_pick_xy(wview, &column->x, &column->y);
	gtk_fixed_put(GTK_FIXED(wview->fixed),
		GTK_WIDGET(cview), column->x, column->y);
	cview->lx = column->x;
	cview->ly = column->y;
}

static void
workspaceview_child_position(View *parent, View *child)
{
	Workspaceview *wview = WORKSPACEVIEW(parent);
	Columnview *cview = COLUMNVIEW(child);

	gtk_fixed_move(GTK_FIXED(wview->fixed),
		GTK_WIDGET(cview), cview->lx, cview->ly);

	VIEW_CLASS(workspaceview_parent_class)->child_position(parent, child);
}

static void
workspaceview_child_front(View *parent, View *child)
{
	Workspaceview *wview = WORKSPACEVIEW(parent);
	Columnview *cview = COLUMNVIEW(child);

	g_object_ref(cview);

	gtk_fixed_remove(GTK_FIXED(wview->fixed), GTK_WIDGET(cview));
	gtk_fixed_put(GTK_FIXED(wview->fixed),
		GTK_WIDGET(cview), cview->lx, cview->ly);

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

	if (wview->label) {
		gtk_label_set_text(GTK_LABEL(wview->label),
			IOBJECT(ws)->name);

		if (IOBJECT(ws)->caption)
			set_tooltip(wview->label, "%s", IOBJECT(ws)->caption);

		printf("workspaceview_refresh: FIXME update padlock and error icons\n");
		/*
		if (ws->locked)
			gtk_image_set_from_icon_name(GTK_IMAGE(wview->padlock), "locked");
		else
			gtk_image_clear(GTK_IMAGE(wview->padlock));

		if (ws->errors)
			gtk_image_set_from_icon_name(GTK_IMAGE(wview->alert), "alert");
		else
			gtk_image_clear(GTK_IMAGE(wview->alert));
		 */
	}

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
	layout->undone_columns =
		g_slist_prepend(layout->undone_columns, view);

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
		layout->current_columns = g_slist_prepend(
			layout->current_columns, cview);
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

	int x, y, w, h;
	gboolean changed;

	/* If this column is being dragged, put the xy we allocate into the
	 * shadow instead.
	 */
	changed = FALSE;
	if (cview->shadow) {
		if (cview->shadow->lx != layout->out_x ||
			cview->shadow->ly != layout->out_y) {
			cview->shadow->lx = layout->out_x;
			cview->shadow->ly = layout->out_y;
			changed = TRUE;
		}
	}
	else {
		if (column->x != layout->out_x ||
			column->y != layout->out_y) {
			column->x = layout->out_x;
			column->y = layout->out_y;
			changed = TRUE;
		}
	}

	columnview_get_position(cview, &x, &y, &w, &h);
	layout->out_y += h + workspaceview_layout_vspacing;

	if (changed)
		iobject_changed(IOBJECT(column));

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

	search for left-most column

	search for all columns with a 'small' overlap

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
workspaceview_tab_menu(GtkGestureClick *gesture,
	guint n_press, double x, double y, Workspaceview *wview)
{
	gtk_popover_set_pointing_to(GTK_POPOVER(wview->tab_menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	printf("workspaceview_tab_menu:\n");

	gtk_popover_popup(GTK_POPOVER(wview->tab_menu));
}

static void
workspaceview_tab_pressed(GtkGestureClick *gesture,
	guint n_press, double x, double y, Workspaceview *wview)
{
	if (n_press == 2) {
		// rename tab
		printf("workspaceview_tab_pressed: doubleclick\n");
		// workspaceview_rename_cb(wid, NULL, wview);
	}
}

static void
workspaceview_background_menu(GtkGestureClick *gesture,
	guint n_press, double x, double y, Workspaceview *wview)
{
	gtk_popover_set_pointing_to(GTK_POPOVER(wview->right_click_menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	gtk_popover_popup(GTK_POPOVER(wview->right_click_menu));
}

#define BIND(field) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		Workspaceview, field);

static void
workspaceview_class_init(WorkspaceviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	gtk_widget_class_set_layout_manager_type(widget_class,
		GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
		APP_PATH "/workspaceview.ui");

	BIND(fixed);
	BIND(label);
	BIND(tab_menu);
	BIND(right_click_menu);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		workspaceview_tab_menu);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		workspaceview_tab_pressed);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		workspaceview_background_menu);

	object_class->dispose = workspaceview_dispose;

	widget_class->realize = workspaceview_realize;

	vobject_class->refresh = workspaceview_refresh;

	view_class->link = workspaceview_link;
	view_class->child_add = workspaceview_child_add;
	view_class->child_position = workspaceview_child_position;
	view_class->child_front = workspaceview_child_front;
	view_class->layout = workspaceview_layout;
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

	error_top(_("Unknown file type."));
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
		error_alert(GTK_WIDGET(wview));
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
		error_top(_("No errors in tab."));
		error_sub("%s", _("There are no errors (that I can see) in this tab."));
		error_alert(GTK_WIDGET(wview));
	}
}

static void
workspaceview_init(Workspaceview *wview)
{
	gtk_widget_init_template(GTK_WIDGET(wview));

	// a lot of stuff to go in here
	printf("workspaceview_init: FIXME we must do stuff\n");
}

View *
workspaceview_new(void)
{
	Workspaceview *wview = g_object_new(WORKSPACEVIEW_TYPE, NULL);

	return VIEW(wview);
}

void
workspaceview_set_label(Workspaceview *wview, GtkWidget *label)
{
	g_assert(!wview->label);

	wview->label = label;
}
