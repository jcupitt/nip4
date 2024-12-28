/* main processing window
 */

/*

	Copyright (C) 1991-2003 The National Gallery
	Copyright (C) 2004-2023 libvips.org

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

#include "nip4.h"

/*
#define DEBUG
 */

G_DEFINE_TYPE(Workspacegroupview, workspacegroupview, VIEW_TYPE)

/* GTypes we handle in copy/paste and drag/drop paste ... needed for tab drop.
 *
 * Created in _class_init(), since some of these types are only defined at
 * runtime.
 */
static GType *workspacegroupview_supported_types;
static int workspacegroupview_n_supported_types;

static void
workspacegroupview_dispose(GObject *object)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(object);

#ifdef DEBUG
	printf("workspacegroupview_dispose:\n");
#endif /*DEBUG*/

	gtk_widget_dispose_template(GTK_WIDGET(wsgview), WORKSPACEGROUPVIEW_TYPE);

	G_OBJECT_CLASS(workspacegroupview_parent_class)->dispose(object);
}

static void
workspacegroupview_realize(GtkWidget *widget)
{
#ifdef DEBUG
	printf("workspacegroupview_realize\n");
#endif /*DEBUG*/

	GTK_WIDGET_CLASS(workspacegroupview_parent_class)->realize(widget);

	/* Mark us as a symbol drag-to widget.
	 */
	set_symbol_drag_type(widget);
}

static void
workspacegroupview_child_add(View *parent, View *child)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(parent);
	Workspaceview *wview = WORKSPACEVIEW(child);
	Workspace *ws = WORKSPACE(VOBJECT(child)->iobject);

	VIEW_CLASS(workspacegroupview_parent_class)->child_add(parent, child);

	gtk_notebook_insert_page(GTK_NOTEBOOK(wsgview->notebook),
		GTK_WIDGET(wview), GTK_WIDGET(wview->label), ICONTAINER(ws)->pos);

	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(wsgview->notebook),
		GTK_WIDGET(wview), TRUE);
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(wsgview->notebook),
		GTK_WIDGET(wview), TRUE);
}

static void
workspacegroupview_child_remove(View *parent, View *child)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(parent);
	Workspace *ws = WORKSPACE(VOBJECT(child)->iobject);

	gtk_notebook_remove_page(GTK_NOTEBOOK(wsgview->notebook),
		ICONTAINER(ws)->pos);

	VIEW_CLASS(workspacegroupview_parent_class)->child_remove(parent, child);
}

static void
workspacegroupview_child_position(View *parent, View *child)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(parent);
	Workspaceview *wview = WORKSPACEVIEW(child);

	gtk_notebook_reorder_child(GTK_NOTEBOOK(wsgview->notebook),
		GTK_WIDGET(wview), ICONTAINER(wview)->pos);

	VIEW_CLASS(workspacegroupview_parent_class)->child_position(parent, child);
}

static void
workspacegroupview_child_front(View *parent, View *child)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(parent);
	Workspaceview *wview = WORKSPACEVIEW(child);

	int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(wsgview->notebook));
	GtkWidget *current_front =
		gtk_notebook_get_nth_page(GTK_NOTEBOOK(wsgview->notebook), page);

	if (current_front != GTK_WIDGET(wview)) {
		page = gtk_notebook_page_num(GTK_NOTEBOOK(wsgview->notebook),
			GTK_WIDGET(wview));

		gtk_notebook_set_current_page(GTK_NOTEBOOK(wsgview->notebook), page);
	}
}

static void
workspacegroupview_new_tab_clicked(GtkButton *button, void *user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Workspacegroup *wsg = WORKSPACEGROUP(VOBJECT(wsgview)->iobject);

	if (!workspace_new_blank(wsg))
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(wsgview))));
}

static Workspaceview *
workspacegroupview_get_workspaceview(Workspacegroupview *wsgview)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(wsgview->notebook);
	int page_number = gtk_notebook_get_current_page(notebook);

	return WORKSPACEVIEW(gtk_notebook_get_nth_page(notebook, page_number));
}

// translate (x,y) to workspace coordinates for the current page
static gboolean
workspacegroupview_to_fixed(Workspacegroupview *wsgview,
	double *x, double *y)
{
	Workspaceview *wview = workspacegroupview_get_workspaceview(wsgview);

	graphene_point_t wsgview_point = GRAPHENE_POINT_INIT(*x, *y);
	graphene_point_t wview_point;
	if (!gtk_widget_compute_point(GTK_WIDGET(wsgview), GTK_WIDGET(wview->fixed),
			&wsgview_point, &wview_point))
		return FALSE;

	*x = wview_point.x;
	*y = wview_point.y;

	return TRUE;
}

static Columnview *
workspacegroupview_pick_columnview(Workspacegroupview *wsgview,
	double x, double y)
{
	Workspaceview *wview = workspacegroupview_get_workspaceview(wsgview);

	if (!workspacegroupview_to_fixed(wsgview, &x, &y))
		return NULL;

	return workspaceview_find_columnview(wview, x, y);
}

static Columnview *
workspacegroupview_pick_columnview_title(Workspacegroupview *wsgview,
	double x, double y)
{
	Workspaceview *wview = workspacegroupview_get_workspaceview(wsgview);

	if (!workspacegroupview_to_fixed(wsgview, &x, &y))
		return NULL;

	return workspaceview_find_columnview_title(wview, x, y);
}

static Rowview *
workspacegroupview_pick_rowview(Workspacegroupview *wsgview,
	double x, double y)
{
	Columnview *cview = workspacegroupview_pick_columnview(wsgview, x, y);
	if (!cview)
		return NULL;

	if (!workspacegroupview_to_fixed(wsgview, &x, &y))
		return NULL;

	return columnview_find_rowview(cview, x, y);
}

static void
workspacegroupview_background_menu(GtkGestureClick *gesture,
	guint n_press, double x, double y, void *user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Workspaceview *wview = workspacegroupview_get_workspaceview(wsgview);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

	// disable on locked workspaces
	if (ws->locked)
		return;

	GtkWidget *menu = NULL;

	Columnview *cview;
	if ((cview = workspacegroupview_pick_columnview_title(wsgview, x, y))) {
		mainwindow_set_action_view(VIEW(cview));
		menu = wsgview->column_menu;
	}
	else if ((cview = workspacegroupview_pick_columnview(wsgview, x, y))) {
		Rowview *rview;

		if ((rview = workspacegroupview_pick_rowview(wsgview, x, y))) {
			mainwindow_set_action_view(VIEW(rview));
			menu = wsgview->row_menu;
		}
		else {
			mainwindow_set_action_view(VIEW(wview));
			menu = wsgview->workspace_menu;
		}
	}

	if (menu) {
		gtk_popover_set_pointing_to(GTK_POPOVER(menu),
			&(const GdkRectangle){ x, y, 1, 1 });

		gtk_popover_popup(GTK_POPOVER(menu));
	}
}

static void
workspacegroupview_class_init(WorkspacegroupviewClass *class)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("workspacegroupview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Workspacegroupview, notebook);
	BIND_VARIABLE(Workspacegroupview, workspace_menu);
	BIND_VARIABLE(Workspacegroupview, column_menu);
	BIND_VARIABLE(Workspacegroupview, row_menu);

	BIND_CALLBACK(workspacegroupview_new_tab_clicked);
	BIND_CALLBACK(workspacegroupview_background_menu);

	G_OBJECT_CLASS(class)->dispose = workspacegroupview_dispose;

	widget_class->realize = workspacegroupview_realize;

	view_class->child_add = workspacegroupview_child_add;
	view_class->child_remove = workspacegroupview_child_remove;
	view_class->child_position = workspacegroupview_child_position;
	view_class->child_front = workspacegroupview_child_front;

	GType supported_types[] = {
		GDK_TYPE_FILE_LIST,
		G_TYPE_FILE,
		GDK_TYPE_TEXTURE,
		G_TYPE_STRING,
	};

	workspacegroupview_n_supported_types = VIPS_NUMBER(supported_types);
	workspacegroupview_supported_types =
		VIPS_ARRAY(NULL, workspacegroupview_n_supported_types + 1, GType);
	for (int i = 0; i < workspacegroupview_n_supported_types; i++)
		workspacegroupview_supported_types[i] = supported_types[i];
}

/* Called for switching the current page, and for page drags between
 * notebooks.
 */
static void
workspacegroupview_switch_page(GtkNotebook *notebook,
	GtkWidget *page, guint page_num, gpointer user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Workspacegroup *wsg = WORKSPACEGROUP(VOBJECT(wsgview)->iobject);
	Workspaceview *wview = WORKSPACEVIEW(page);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Workspacegroup *old_wsg = WORKSPACEGROUP(ICONTAINER(ws)->parent);

#ifdef DEBUG
	printf("workspacegroupview_switch_page: page_num = %d\n", page_num);
	printf("\tws = %p (%s)\n", ws, IOBJECT(ws)->name);
	printf("\tmain = %p\n", main);
#endif /*DEBUG*/

	if (ICONTAINER(ws)->parent != ICONTAINER(wsg)) {
		icontainer_reparent(ICONTAINER(wsg), ICONTAINER(ws), -1);

		// we want the from and to wsgs to be modified on tab drag
		filemodel_set_modified(FILEMODEL(wsg), TRUE);
		filemodel_set_modified(FILEMODEL(old_wsg), TRUE);

		// we may have left an empty mainwindow
		mainwindow_cull();
	}

	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));

	// we must relayout in case this tab was previously laid out while hidden
	workspace_queue_layout(ws);

	if (ws->compat_major) {
		error_top(_("Compatibility mode"));
		error_sub(_("this workspace was created by version %d.%d -- "
					"a set of compatibility menus have been loaded "
					"for this window"),
			ws->compat_major,
			ws->compat_minor);
		workspace_set_show_error(ws, TRUE);
	}
}

/* We come here for tab drag, so we reparent if the enclosing notebook is part
 * of a different mainwindow.
 */
static void
workspacegroupview_page_added(GtkNotebook *notebook,
	GtkWidget *page, guint page_num, gpointer user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Mainwindow *main = MAINWINDOW(gtk_widget_get_root(GTK_WIDGET(wsgview)));
	Workspacegroup *wsg = WORKSPACEGROUP(VOBJECT(wsgview)->iobject);

	filemodel_set_window_hint(FILEMODEL(wsg), GTK_WINDOW(main));
}

static GtkNotebook *
workspacegroupview_create_window(GtkNotebook *notebook,
	GtkWidget *page, gpointer user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Mainwindow *old_main = MAINWINDOW(gtk_widget_get_root(GTK_WIDGET(wsgview)));
	Workspacegroup *old_wsg = WORKSPACEGROUP(VOBJECT(wsgview)->iobject);

#ifdef DEBUG
	printf("workspacegroupview_create_window:\n");
	printf("\told_wsg = %p\n", old_wsg);
#endif /*DEBUG*/

	App *app = APP(gtk_window_get_application(GTK_WINDOW(old_main)));
	Workspaceroot *wsr = old_wsg->wsr;
	Workspacegroup *new_wsg = workspacegroup_new(wsr);

#ifdef DEBUG
	printf("\told_wsg = %p (%s)\n", old_wsg, IOBJECT(old_wsg)->name);
	printf("\tnew_wsg = %p (%s)\n", new_wsg, IOBJECT(new_wsg)->name);
#endif /*DEBUG*/

	/* Don't reparent here ... detect reparent in page_added and do it there.
	 *
	 * We have two cases: drag tab to background (this cb makes the new
	 * window), and drag tab to existing window (this cb won't be called).
	 * page_added is the common path.
	 */

	Mainwindow *new_main = mainwindow_new(app, new_wsg);
	gtk_window_present(GTK_WINDOW(new_main));

#ifdef DEBUG
	printf("\told_main = %p\n", old_main);
	printf("\tnew_main = %p\n", new_main);
#endif /*DEBUG*/

	return GTK_NOTEBOOK(mainwindow_get_workspacegroupview(new_main)->notebook);
}

static void
workspacegroupview_page_reordered(GtkNotebook *notebook,
	GtkWidget *page, guint page_num, gpointer user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Workspacegroup *wsg = WORKSPACEGROUP(VOBJECT(wsgview)->iobject);

#ifdef DEBUG
	printf("workspacegroupview_page_reordered: page_num = %d\n", page_num);
#endif /*DEBUG*/

	gboolean changed;
	changed = FALSE;
	for (int i = 0; i < gtk_notebook_get_n_pages(notebook); i++) {
		GtkWidget *page_n = gtk_notebook_get_nth_page(notebook, i);
		Workspaceview *wview = WORKSPACEVIEW(page_n);
		Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

		if (ICONTAINER(ws)->pos != i) {
			ICONTAINER(ws)->pos = i;
			changed = TRUE;
		}
	}

	if (changed)
		icontainer_pos_sort(ICONTAINER(wsg));
}

static gboolean
workspacegroupview_dnd_drop(GtkDropTarget *target,
	const GValue *value, double x, double y, gpointer user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Mainwindow *main = MAINWINDOW(gtk_widget_get_root(GTK_WIDGET(wsgview)));

	printf("workspacegroupview_dnd_drop: %g x %g\n", x, y);

	gboolean handled = FALSE;

	Columnview *cview;
	Rowview *rview;
	if ((cview = workspacegroupview_pick_columnview(wsgview, x, y))) {
		handled = TRUE;

		if ((rview = workspacegroupview_pick_rowview(wsgview, x, y)))
			printf("\tdrop in row\n");
		else
			printf("\tdrop in column\n");
	}
	else {
		printf("\tdrop on background\n");
		handled = TRUE;

		if (!mainwindow_paste_value(main, value))
			mainwindow_error(main);
		else
			symbol_recalculate_all();
	}

	return handled;
}

static void
workspacegroupview_init(Workspacegroupview *wsgview)
{
#ifdef DEBUG
	printf("workspacegroupview_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(wsgview));

	g_signal_connect_object(wsgview->notebook, "switch-page",
		G_CALLBACK(workspacegroupview_switch_page), wsgview, 0);
	g_signal_connect_object(wsgview->notebook, "page-added",
		G_CALLBACK(workspacegroupview_page_added), wsgview, 0);
	g_signal_connect_object(wsgview->notebook, "create-window",
		G_CALLBACK(workspacegroupview_create_window), wsgview, 0);
	g_signal_connect_object(wsgview->notebook, "page-reordered",
		G_CALLBACK(workspacegroupview_page_reordered), wsgview, 0);

	/* We are a drop target for tabs.
	 */
	GtkDropTarget *target =
		gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY);
	gtk_drop_target_set_gtypes(target,
		workspacegroupview_supported_types,
		workspacegroupview_n_supported_types);
	g_signal_connect(target, "drop",
		G_CALLBACK(workspacegroupview_dnd_drop), wsgview);
	gtk_widget_add_controller(wsgview->notebook, GTK_EVENT_CONTROLLER(target));

}

View *
workspacegroupview_new(void)
{
	Workspacegroupview *wsgview = g_object_new(WORKSPACEGROUPVIEW_TYPE, NULL);

	return VIEW(wsgview);
}
