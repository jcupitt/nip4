/* A rowview in a workspace ... not a widget, part of column
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

G_DEFINE_TYPE(Rowview, rowview, VIEW_TYPE)

/*
enum {
  ROWVIEW_TARGET_STRING,
};

static GtkTargetEntry rowview_target_table[] = {
  { "STRING", 0, ROWVIEW_TARGET_STRING },
  { "text/plain", 0, ROWVIEW_TARGET_STRING }
};
 */

static void
rowview_dispose(GObject *object)
{
	Rowview *rview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_ROWVIEW(object));

	rview = ROWVIEW(object);

#ifdef DEBUG
	printf("rowview_dispose: ");
	row_name_print(ROW(VOBJECT(rview)->iobject));
	printf("\n");
#endif /*DEBUG*/

	VIPS_FREE(rview->last_tooltip);
	// we may run more than once ... only dispose the template the first time
	if (rview->spin)
		gtk_widget_dispose_template(GTK_WIDGET(rview), ROWVIEW_TYPE);

	G_OBJECT_CLASS(rowview_parent_class)->dispose(object);
}

static void
rowview_attach(Rowview *rview, GtkWidget *child, int x)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(VIEW(rview)->parent);

	g_object_ref(child);

	GtkWidget *parent = gtk_widget_get_parent(child);
	if (parent) {
		if (IS_ROWVIEW(parent))
			gtk_widget_unparent(child);
		else
			gtk_grid_remove(GTK_GRID(sview->grid), child);
	}

	gtk_grid_attach(GTK_GRID(sview->grid), child, x, rview->rnum, 1, 1);

	g_object_unref(child);
}

static const char *
rowview_css(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	if (row->err)
		return "error_widget";
	else if (row->selected)
		return "selected_widget";
	else if (row->show == ROW_SHOW_PARENT)
		return "parent_widget";
	else if (row->show == ROW_SHOW_CHILD)
		return "child_widget";
	else if (row->dirty)
		return "dirty_widget";
	else
		return "widget";
}

static void
rowview_update_widgets(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);
	// use every other row, so we have spaces for any shadow
	int pos = 2 * ICONTAINER(row)->pos + 1;
	gboolean editable = row->ws->mode != WORKSPACE_MODE_NOEDIT;

#ifdef DEBUG
	printf("rowview_update_widgets: %p ", rview);
	row_name_print(row);
	printf("\n");
	printf("\teditable == %d\n", editable);
#endif /*DEBUG*/

	/* Attach widgets to parent in new place.
	 */
	if (rview->rnum != pos) {
#ifdef DEBUG
		printf("rowview_update_widgets: move from row %d to row %d\n",
			rview->rnum, pos);
#endif /*DEBUG*/

		rview->rnum = pos;

		rowview_attach(rview, rview->spin, 0);
		rowview_attach(rview, rview->frame, 1);
		if (rview->rhsview)
			rowview_attach(rview, GTK_WIDGET(rview->rhsview), 2);
	}

	if (rview->css_class) {
		gtk_widget_remove_css_class(rview->frame, rview->css_class);
		rview->css_class = NULL;
	}
	rview->css_class = rowview_css(rview);
	if (rview->css_class)
		gtk_widget_add_css_class(rview->frame, rview->css_class);

	/* Update label.
	 */
	set_glabel(rview->label, row_name(row));
	gtk_widget_set_visible(rview->frame, rview->visible && editable);

	/* Spin visible only if this is a class.
	 */
	gtk_widget_set_visible(rview->spin,
		rview->visible && row->is_class && editable);

	if (rview->rhsview)
		gtk_widget_set_visible(GTK_WIDGET(rview->rhsview), rview->visible);
}

static void
rowview_reset(View *view)
{
	Rowview *rview = ROWVIEW(view);

	rowview_update_widgets(rview);

	VIEW_CLASS(rowview_parent_class)->reset(view);
}

static void
rowview_refresh(vObject *vobject)
{
	Rowview *rview = ROWVIEW(vobject);
	Row *row = ROW(VOBJECT(rview)->iobject);
	Workspace *ws = row->ws;

	rowview_update_widgets(rview);
	workspace_queue_layout(ws);

	VOBJECT_CLASS(rowview_parent_class)->refresh(vobject);
}

/* Save the current item.
 */
static void
rowview_save_cb(GtkWidget *menu, GtkWidget *button, Rowview *rview)
{
	/*
	GtkWindow *window = view_get_window(VIEW(rview));
	Row *row = ROW(VOBJECT(rview)->iobject);
	Model *graphic = row->child_rhs->graphic;

	if (graphic)
		classmodel_graphic_save(CLASSMODEL(graphic),
			GTK_WIDGET(iwnd));
	 */
}

/* Replace the current item.
 */
static void
rowview_replace_cb(GtkWidget *menu, GtkWidget *button, Rowview *rview)
{
	/*
	GtkWindow *window = view_get_window(VIEW(rview));
	Row *row = ROW(VOBJECT(rview)->iobject);
	Model *graphic = row->child_rhs->graphic;

	if (graphic)
		classmodel_graphic_replace(CLASSMODEL(graphic),
			GTK_WIDGET(iwnd));
	 */
}

/* Scroll to make tally entry visible.
 */
static void
rowview_scrollto(View *view, ModelScrollPosition position)
{
	Rowview *rview = ROWVIEW(view);
	Columnview *cview = view_get_columnview(VIEW(rview));
	Workspaceview *wview = columnview_get_wview(cview);

	int x, y, w, h;

	/* Extract position of tally row in RC widget.
	 */
	rowview_get_position(rview, &x, &y, &w, &h);
	workspaceview_scroll(wview, x, y, w, h);
}

static void
rowview_child_add(View *parent, View *child)
{
	Rowview *rview = ROWVIEW(parent);

	g_assert(IS_RHSVIEW(child));
	g_assert(!rview->rhsview);

	// we don't hold a ref to rhsview, the grid in the enclosing subcolumnview
	// does
	rview->rhsview = RHSVIEW(child);

	VIEW_CLASS(rowview_parent_class)->child_add(parent, child);
}

static void
rowview_child_remove(View *parent, View *child)
{
	Rowview *rview = ROWVIEW(parent);

	g_assert(IS_RHSVIEW(child));
	g_assert(rview->rhsview);

	rview->rhsview = NULL;

	VIEW_CLASS(rowview_parent_class)->child_remove(parent, child);
}

/* Edit our object.
 */
static gboolean
rowview_edit(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);
	Model *graphic = row->child_rhs->graphic;
	Subcolumnview *sview = SUBCOLUMNVIEW(VIEW(rview)->parent);

	if (graphic)
		model_edit(GTK_WIDGET(sview), graphic);

	return TRUE;
}

static void
rowview_click(GtkGestureClick *gesture,
	guint n_press, double x, double y, Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	if (n_press == 1) {
		if (row->err &&
			row->sym &&
			!symbol_recalculate_check(row->sym))
			// click on a row with an error displays the error
			mainwindow_error(MAINWINDOW(view_get_window(VIEW(rview))));
		else {
			// select
			guint state = get_modifiers(GTK_EVENT_CONTROLLER(gesture));

			row_select_modifier(row, state);
		}
	}
	else
		rowview_edit(rview);
}

static Workspaceview *
rowview_workspaceview(Rowview *rview)
{
	View *p;

	for (p = VIEW(rview); !IS_WORKSPACEVIEW(p); p = p->parent)
		;

	return WORKSPACEVIEW(p);
}

static void
rowview_up_click(GtkGestureClick *gesture, Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);
	Rhs *rhs = row->child_rhs;
	Workspace *ws = row->ws;

	rhs_vislevel_less(rhs);
	workspace_queue_layout(ws);
	workspace_set_modified(row->ws, TRUE);
}

static void
rowview_down_click(GtkGestureClick *gesture, Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);
	Rhs *rhs = row->child_rhs;
	Workspace *ws = row->ws;

	rhs_vislevel_more(rhs);
	workspace_queue_layout(ws);
	workspace_set_modified(row->ws, TRUE);
}

/*
static void
rowview_enter_cb(GtkWidget *widget, Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	row_show_dependents(row);
}

static void
rowview_leave_cb(GtkWidget *widget, Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	row_hide_dependents(row);
}

static gboolean
rowview_focus_cb(GtkWidget *widget, GtkDirectionType dir, Rowview *rview)
{
	view_scrollto(VIEW(rview), MODEL_SCROLL_TOP);

	return FALSE;
}

static void
rowview_tooltip_generate(GtkWidget *widget, VipsBuf *buf, Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	iobject_info(IOBJECT(row), buf);
	vips_buf_removec(buf, '\n');
}
 */

/* Clone the current item.
 */
static void
rowview_duplicate(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);
	Workspace *ws = row->ws;

	/* Only allow clone of top level rows.
	 */
	if (row->top_row != row) {
		error_top("%s", _("Can't duplicate"));
		error_sub("%s", _("you can only duplicate top level rows"));
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(rview))));
		return;
	}

	if (!workspace_selected_any(ws))
		row_select(row);
	if (!workspace_selected_duplicate(ws))
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(rview))));
	workspace_deselect_all(ws);

	symbol_recalculate_all();
}

static void
rowview_group(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	if (!workspace_selected_any(row->ws))
		row_select(row);
	if (!workspace_selected_group(row->ws))
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(rview))));
	workspace_deselect_all(row->ws);

	symbol_recalculate_all();
}

static void
rowview_ungroup(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	if (!workspace_selected_any(row->ws))
		row_select(row);
	if (!workspace_selected_ungroup(row->ws))
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(rview))));
	workspace_deselect_all(row->ws);

	symbol_recalculate_all();
}

static void
rowview_recalc(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	/* Mark dirty from this sym on, and force a recalc even if recalc is
	 * off.
	 */
	if (!workspace_selected_any(row->ws))
		row_select(row);
	if (!workspace_selected_recalc(row->ws))
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(rview))));
	workspace_deselect_all(row->ws);

	if (row->sym &&
		!symbol_recalculate_check(row->sym))
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(rview))));
}

static void
rowview_reset_menu(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	(void) icontainer_map_all(ICONTAINER(row),
		(icontainer_map_fn) model_clear_edited, NULL);
	symbol_recalculate_all();
}

static void
rowview_action(GSimpleAction *action, GVariant *parameter, View *view)
{
	Rowview *rview = ROWVIEW(view);
	View *graphic = rview->rhsview->graphic;
	Row *row = ROW(VOBJECT(rview)->iobject);
	Rhs *rhs = row->child_rhs;
	Workspace *ws = row->ws;
	const char *name = g_action_get_name(G_ACTION(action));

	printf("rowview_action: %s\n", name);

	if (graphic &&
		g_str_equal(name, "row-edit"))
		model_edit(GTK_WIDGET(rview), rhs);
	else if (g_str_equal(name, "row-saveas") &&
		rhs->graphic)
		classmodel_graphic_save(CLASSMODEL(rhs->graphic), GTK_WIDGET(rview));
	else if (g_str_equal(name, "row-delete")) {
		if (!workspace_selected_any(ws))
			row_select(row);
		workspace_selected_remove_yesno(ws, GTK_WIDGET(rview));
	}
	else if (g_str_equal(name, "row-replace") &&
		rhs->graphic)
		classmodel_graphic_replace(CLASSMODEL(rhs->graphic), GTK_WIDGET(rview));
	else if (g_str_equal(name, "row-duplicate"))
		rowview_duplicate(rview);
	else if (g_str_equal(name, "row-group"))
		rowview_group(rview);
	else if (g_str_equal(name, "row-ungroup"))
		rowview_ungroup(rview);
	else if (g_str_equal(name, "row-recalc"))
		rowview_recalc(rview);
	else if (g_str_equal(name, "row-reset"))
		rowview_reset_menu(rview);
}

static void
rowview_class_init(RowviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("rowview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_CALLBACK(rowview_click);
	BIND_CALLBACK(rowview_up_click);
	BIND_CALLBACK(rowview_down_click);

	printf("rowview_class_init: need button enter, leave, focus, tooltip\n");

	BIND_VARIABLE(Rowview, spin);
	BIND_VARIABLE(Rowview, frame);
	BIND_VARIABLE(Rowview, label);

	/* Create signals.
	 */

	/* Init methods.
	 */
	object_class->dispose = rowview_dispose;

	vobject_class->refresh = rowview_refresh;

	view_class->child_add = rowview_child_add;
	view_class->child_remove = rowview_child_remove;
	view_class->reset = rowview_reset;
	view_class->scrollto = rowview_scrollto;
	view_class->action = rowview_action;
}

static void
rowview_init(Rowview *rview)
{
	gtk_widget_init_template(GTK_WIDGET(rview));

	// ensure that we always attach children
	rview->rnum = -1;
}

View *
rowview_new(void)
{
	Rowview *rview = g_object_new(ROWVIEW_TYPE, NULL);

	return VIEW(rview);
}

/* Find the position and size of a row in the enclosing GtkFixed.
 */
void
rowview_get_position(Rowview *rview, int *x, int *y, int *w, int *h)
{
	Columnview *cview = view_get_columnview(VIEW(rview));
	Workspaceview *wview = columnview_get_wview(cview);
	GtkWidget *fixed = wview->fixed;

	graphene_rect_t bounds;

	if (gtk_widget_compute_bounds(GTK_WIDGET(rview), fixed, &bounds)) {
		*x = bounds.origin.x;
		*y = bounds.origin.y;
		*w = bounds.size.width;
		*h = bounds.size.height;
	}
	else {
		/* Nothing there yet ... guess.
		 */
		*x = 10;
		*y = 10;
		*w = 200;
		*h = 50;
	}
}

/* Hide/show a row.
 */
void
rowview_set_visible(Rowview *rview, gboolean visible)
{
	if (rview->visible != visible) {
		rview->visible = visible;
		rowview_update_widgets(rview);
	}
}

gboolean
rowview_get_visible(Rowview *rview)
{
	return rview->visible;
}
