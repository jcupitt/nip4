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

	int page;
	GtkWidget *current_front;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(wsgview->notebook));
	current_front =
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

static void
workspacegroupview_class_init(WorkspacegroupviewClass *class)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("workspacegroupview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(Workspacegroupview, notebook);

	BIND_CALLBACK(workspacegroupview_new_tab_clicked);

	G_OBJECT_CLASS(class)->dispose = workspacegroupview_dispose;

	widget_class->realize = workspacegroupview_realize;

	view_class->child_add = workspacegroupview_child_add;
	view_class->child_remove = workspacegroupview_child_remove;
	view_class->child_position = workspacegroupview_child_position;
	view_class->child_front = workspacegroupview_child_front;
}

static Workspaceview *
notebookpage_get_workspaceview(GtkWidget *page)
{
	return WORKSPACEVIEW(page);
}

// Called for switching the current page, and for page drags between
// notebooks.
static void
workspacegroupview_switch_page_cb(GtkNotebook *notebook,
	GtkWidget *page, guint page_num, gpointer user_data)
{
	Workspaceview *wview = notebookpage_get_workspaceview(page);

	// we can come here during destruction ... make sure our model is still
	// around
	if (!VOBJECT(wview)->iobject)
		return;
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

	workspaceview_scroll_reset(wview);

	// we must layout in case this tab was previously laid out while hidden
	workspace_queue_layout(ws);

	if (ws->compat_major) {
		error_top(_("Compatibility mode"));
		error_sub(_("This workspace was created by version %d.%d. "
					"A set of compatibility menus have been loaded "
					"for this window."),
			ws->compat_major,
			ws->compat_minor);
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(wview))));
	}
}

static void
workspacegroupview_page_added_cb(GtkNotebook *notebook,
	GtkWidget *page, guint page_num, gpointer user_data)
{
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(user_data);
	Workspacegroup *wsg = WORKSPACEGROUP(VOBJECT(wsgview)->iobject);
	Mainwindow *main = MAINWINDOW(gtk_widget_get_root(GTK_WIDGET(notebook)));

	filemodel_set_window_hint(FILEMODEL(wsg), GTK_WINDOW(main));
}

static GtkNotebook *
workspacegroupview_create_window_cb(GtkNotebook *notebook,
	GtkWidget *page, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(page);
	Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);
	Workspacegroup *old_wsg = WORKSPACEGROUP(ICONTAINER(ws)->parent);
	Workspaceroot *wsr = old_wsg->wsr;
	Mainwindow *old_main = MAINWINDOW(view_get_window(VIEW(wview)));
	GtkApplication *app = gtk_window_get_application(GTK_WINDOW(old_main));
	Workspacegroup *new_wsg = workspacegroup_new(wsr);

#ifdef DEBUG
	printf("workspacegroupview_create_window_cb:\n");
	printf("\told_wsg %p (%s)\n", old_wsg, IOBJECT(old_wsg)->name);
	printf("\tws %p (%s)\n", ws, IOBJECT(ws)->name);
	printf("\tnew_wsg %p (%s)\n", new_wsg, IOBJECT(new_wsg)->name);
#endif /*DEBUG*/

	Mainwindow *new_main = mainwindow_new(app);
	mainwindow_set_wsg(new_main, new_wsg);
	// viewchild on the model will spot the _detach / _attach and also
	// reparent the view
	icontainer_reparent(ICONTAINER(new_wsg), ICONTAINER(ws), 0);

	gtk_window_present(GTK_WINDOW(new_main));

	return GTK_NOTEBOOK(mainwindow_get_workspacegroupview(new_main)->notebook);
}

static void
workspacegroupview_page_reordered_cb(GtkNotebook *notebook,
	GtkWidget *page, guint page_num, gpointer user_data)
{
	Workspaceview *wview = WORKSPACEVIEW(page);
	Workspacegroupview *wsgview = WORKSPACEGROUPVIEW(VIEW(wview)->parent);
	Workspacegroup *wsg = WORKSPACEGROUP(VOBJECT(wsgview)->iobject);

	int i;
	gboolean changed;

	changed = FALSE;

	for (i = 0; i < gtk_notebook_get_n_pages(notebook); i++) {
		GtkWidget *page_n = gtk_notebook_get_nth_page(notebook, i);
		Workspaceview *wview = WORKSPACEVIEW(page_n);
		Workspace *ws = WORKSPACE(VOBJECT(wview)->iobject);

		if (ICONTAINER(ws)->pos != i) {
			ICONTAINER(ws)->pos = i;
			changed = TRUE;
		}
	}

	if (changed) {
		icontainer_pos_sort(ICONTAINER(wsg));
		filemodel_set_modified(FILEMODEL(wsg), TRUE);
	}
}

static void
workspacegroupview_init(Workspacegroupview *wsgview)
{
#ifdef DEBUG
	printf("workspacegroupview_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(wsgview));

	g_signal_connect(wsgview->notebook, "switch_page",
		G_CALLBACK(workspacegroupview_switch_page_cb), wsgview);
	g_signal_connect(wsgview->notebook, "page_added",
		G_CALLBACK(workspacegroupview_page_added_cb), wsgview);
	g_signal_connect(wsgview->notebook, "page_reordered",
		G_CALLBACK(workspacegroupview_page_reordered_cb), wsgview);
	g_signal_connect(wsgview->notebook, "create_window",
		G_CALLBACK(workspacegroupview_create_window_cb), wsgview);
}

View *
workspacegroupview_new(void)
{
	Workspacegroupview *wsgview = g_object_new(WORKSPACEGROUPVIEW_TYPE, NULL);

	return VIEW(wsgview);
}
