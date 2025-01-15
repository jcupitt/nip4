/* a view of a column
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

G_DEFINE_TYPE(Columnview, columnview, VIEW_TYPE)

static void
columnview_edit(Columnview *cview)
{
	if (cview->state != COL_EDIT) {
		Column *col = COLUMN(VOBJECT(cview)->iobject);

		cview->state = COL_EDIT;
		g_object_set(cview->caption_edit,
			"text", IOBJECT(col)->caption,
			NULL);

		vobject_refresh_queue(VOBJECT(cview));

		ientry_grab_focus(IENTRY(cview->caption_edit));
	}
}

static void
columnview_caption_edit_activate(GtkEntry *self, gpointer user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	g_autofree char *text = NULL;
	g_object_get(self, "text", &text, NULL);
	if (text) {
		VIPS_SETSTR(IOBJECT(col)->caption, text);
		workspace_set_modified(ws, TRUE);
	}

	cview->state = COL_WAIT;
	vobject_refresh_queue(VOBJECT(cview));
}

static void
columnview_caption_edit_cancel(GtkEntry *self, gpointer user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);

	cview->state = COL_WAIT;
	vobject_refresh_queue(VOBJECT(cview));
}

static void
columnview_merge_sub(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(cview)));
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		mainwindow_set_load_folder(main, file);

		g_autofree char *filename = g_file_get_path(file);

		if (!workspacegroup_merge_rows(wsg, filename))
			workspace_set_show_error(ws, TRUE);
		symbol_recalculate_all();
	}
}

static void
columnview_merge(Columnview *cview)
{
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(cview)));
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	// workspacegroup_merge_rows() merges into the current column
	workspace_column_select(ws, col);

	GtkFileDialog *dialog;

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Merge into column");
	gtk_file_dialog_set_accept_label(dialog, "Merge");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GFile *load_folder = mainwindow_get_load_folder(main);
	if (load_folder)
		gtk_file_dialog_set_initial_folder(dialog, load_folder);

	gtk_file_dialog_open(dialog, GTK_WINDOW(main), NULL,
		&columnview_merge_sub, cview);
}

static void
columnview_saveas_sub(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(cview)));
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		mainwindow_set_save_folder(main, file);

		g_autofree char *filename = g_file_get_path(file);

		workspace_deselect_all(ws);
		column_select_symbols(col);
		if (!workspacegroup_save_selected(wsg, filename))
			workspace_set_show_error(ws, TRUE);
	}
}

static void
columnview_saveas(Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	Mainwindow *main = MAINWINDOW(view_get_window(VIEW(cview)));

	GtkFileDialog *dialog;

	// we can only save the current tab
	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save column as");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GFile *save_folder = mainwindow_get_save_folder(main);
	if (save_folder)
		gtk_file_dialog_set_initial_folder(dialog, save_folder);

	gtk_file_dialog_save(dialog, GTK_WINDOW(main), NULL,
		&columnview_saveas_sub, cview);
}

/* Find the position and size of a columnview.
 */
void
columnview_get_position(Columnview *cview, int *x, int *y, int *w, int *h)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(cview));

	graphene_rect_t bounds;

	*x = 0;
	*y = 0;
	*w = 0;
	*h = 0;

	if (gtk_widget_compute_bounds(GTK_WIDGET(cview), parent, &bounds)) {
		*x = bounds.origin.x;
		*y = bounds.origin.y;
		*w = bounds.size.width;
		*h = bounds.size.height;
	}

	if (*w == 0 || *h == 0) {
		// we're probably not realised yet ... use the saved position
		*x = col ? col->x : 0;
		*y = col ? col->y : 0;
		*w = 200;
		*h = 50;
	}

#ifdef DEBUG
	printf("columnview_get_position: %s, x = %d, y = %d, w = %d, h = %d\n",
		col ? IOBJECT(col)->name : "null", *x, *y, *w, *h);
#endif /*DEBUG*/
}

Workspaceview *
columnview_get_wview(Columnview *cview)
{
	return WORKSPACEVIEW(VIEW(cview)->parent);
}

void
columnview_add_shadow(Columnview *cview)
{
	if (!cview->shadow) {
		Column *col = COLUMN(VOBJECT(cview)->iobject);
		Workspaceview *wview = columnview_get_wview(cview);

		Columnview *shadow;

		/* We can't use model_view_new() etc as we don't want the shadow to be
		 * part of the viewchild system, or to auto update when col updates.
		 *
		 * So we have to replicate all that code here :( This is very
		 * fragile.
		 */
		shadow = COLUMNVIEW(columnview_new());
		VIEW(shadow)->parent = VIEW(cview)->parent;
		VOBJECT(shadow)->iobject = IOBJECT(col);
		cview->shadow = shadow;
		shadow->master = cview;

		/* Position it in the right place immediatly.
		 */
		shadow->x = cview->x;
		shadow->y = cview->y;

		/* Match the size of the original as well.
		 */
		int x, y, w, h;
		columnview_get_position(cview, &x, &y, &w, &h);
		gtk_widget_set_size_request(GTK_WIDGET(shadow), w, h);

		// needs to be on the view children list so it gets animated
		VIEW(wview)->children = g_slist_prepend(VIEW(wview)->children, shadow);
		g_object_ref(shadow);

		gtk_fixed_put(GTK_FIXED(wview->fixed),
			GTK_WIDGET(shadow), shadow->x, shadow->y);

		/* The shadow will be on top of the real column and hide it.
		 * Put the real column to the front.
		 */
		view_child_front(VIEW(cview));
	}
}

void
columnview_remove_shadow(Columnview *cview)
{
	if (cview->shadow) {
		cview->shadow->master = NULL;
		view_child_remove(VIEW(cview->shadow));
		cview->shadow = NULL;
	}
}

static void
columnview_dispose(GObject *object)
{
	Columnview *cview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_COLUMNVIEW(object));

	cview = COLUMNVIEW(object);

#ifdef DEBUG
	printf("columnview_dispose: cview=%p\n", cview);
#endif /*DEBUG*/

	if (cview->shadow)
		view_child_remove(VIEW(cview->shadow));

	gtk_widget_dispose_template(GTK_WIDGET(cview), COLUMNVIEW_TYPE);

	G_OBJECT_CLASS(columnview_parent_class)->dispose(object);
}

static const char *
columnview_css(Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	if (ws->locked)
		return "widget";
	else if (cview->master)
		return "shadow_widget";
	else if (col->selected)
		return "selected_widget";
	else
		return "widget";
}

void
columnview_animate_to(Columnview *cview, int x, int y)
{
	if (cview->x != x ||
		cview->y != y) {
		// the current position is the new start, the next position is the new
		// target
		cview->start_x = cview->x;
		cview->start_y = cview->y;
		cview->x = x;
		cview->y = y;
		cview->elapsed = 0.0;
		cview->animating = TRUE;

		// start animation
		view_child_position(VIEW(cview));
	}
}

static void
columnview_refresh(vObject *vobject)
{
	Columnview *cview = COLUMNVIEW(vobject);
	Columnview *shadow = cview->shadow;
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

#ifdef DEBUG
	printf("columnview_refresh: %p\n", cview);
#endif /*DEBUG*/

	gboolean editable = col->ws->mode != WORKSPACE_MODE_NOEDIT;

#ifdef DEBUG
	printf("\tmodel %s\n", IOBJECT(col)->name);
#endif /*DEBUG*/

	/* If this column has a shadow, workspaceview will have put a layout
	 * position into it. See workspaceview_layout_set_pos().
	 */
	if (shadow)
		columnview_animate_to(shadow, col->x, col->y);
	else
		columnview_animate_to(cview, col->x, col->y);

	/* Titlebar off in no-edit mode.
	 */
	gtk_widget_set_visible(cview->title, editable);

	/* Update name and caption.
	 */
	set_glabel(cview->name, "%s", IOBJECT(col)->name);
	if (IOBJECT(col)->caption)
		set_glabel(cview->caption, "%s", IOBJECT(col)->caption);
	else
		set_glabel(cview->caption, "double-click to set caption");
	gtk_stack_set_visible_child(GTK_STACK(cview->caption_edit_stack),
		cview->state == COL_EDIT ? cview->caption_edit : cview->caption);

	/* Set open/closed.
	 */
	gtk_button_set_icon_name(GTK_BUTTON(cview->expand_button),
		col->open ? "pan-down-symbolic" : "pan-end-symbolic");
	gtk_revealer_set_reveal_child(GTK_REVEALER(cview->revealer), col->open);

	/* Closed columns are hidden in NOEDIT mode.
	 */
	gtk_widget_set_visible(GTK_WIDGET(cview), editable || col->open);

	/* Set bottom entry visibility.
	 */
	gtk_widget_set_visible(cview->entry,
		col->selected &&
			col->open &&
			editable &&
			!cview->master &&
			!ws->locked);

	/* Set select state.
	 */
	if (col->selected && !cview->selected) {
		cview->selected = TRUE;
		gtk_widget_grab_focus(cview->entry);
	}
	else if (!col->selected)
		cview->selected = FALSE;

	if (cview->css_class) {
		gtk_widget_remove_css_class(cview->title, cview->css_class);
		cview->css_class = NULL;
	}
	cview->css_class = columnview_css(cview);
	if (cview->css_class)
		gtk_widget_add_css_class(cview->title, cview->css_class);

	VOBJECT_CLASS(columnview_parent_class)->refresh(vobject);
}

static void
columnview_link(View *view, Model *model, View *parent)
{
	Column *col = COLUMN(model);
	Columnview *cview = COLUMNVIEW(view);

	cview->x = col->x;
	cview->y = col->y;

	VIEW_CLASS(columnview_parent_class)->link(view, model, parent);
}

static void
columnview_child_add(View *parent, View *child)
{
	Columnview *cview = COLUMNVIEW(parent);
	Subcolumnview *sview = SUBCOLUMNVIEW(child);

#ifdef DEBUG
	printf("columnview_child_add: cview=%p sview=%p\n", cview, sview);
#endif /*DEBUG*/

	cview->sview = sview;
	gtk_box_prepend(GTK_BOX(cview->body), GTK_WIDGET(sview));

	VIEW_CLASS(columnview_parent_class)->child_add(parent, child);
}

static void
columnview_child_remove(View *parent, View *child)
{
	Columnview *cview = COLUMNVIEW(parent);
	Subcolumnview *sview = SUBCOLUMNVIEW(child);

#ifdef DEBUG
	printf("columnview_child_remove: cview=%p sview=%p\n", cview, sview);
#endif /*DEBUG*/

	if (cview->body)
		gtk_box_remove(GTK_BOX(cview->body), GTK_WIDGET(sview));
	cview->sview = NULL;

	VIEW_CLASS(columnview_parent_class)->child_remove(parent, child);
}

/* Scroll to keep the text entry at the bottom of the columnview on screen.
 * We can't use the position/size of the text widget for positioning, since it
 * may not be properly realized yet ... make the bottom of the column visible
 * instead.
 */
static void
columnview_scrollto(View *view, ModelScrollPosition position)
{
	Columnview *cview = COLUMNVIEW(view);
	Workspaceview *wview = columnview_get_wview(cview);

	int x, y, w, h;
	columnview_get_position(cview, &x, &y, &w, &h);

	switch (position) {
	case MODEL_SCROLL_BOTTOM:
		y += h - 50;
		h = 50;
		break;

	case MODEL_SCROLL_TOP:
		h = 50;
		break;

	default:
		g_assert_not_reached();
		break;
	}

	workspaceview_scroll(wview, x, y, w, h);
}

static void
columnview_pressed(GtkGestureClick *gesture,
	guint n_press, double x, double y, Columnview *cview)
{
	if (n_press == 2)
		columnview_edit(cview);
}

static void
columnview_expand_clicked(GtkGestureClick *gesture,
	guint n_press, double x, double y, Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	column_set_open(col, !col->open);
}

static void
columnview_activate(GtkEntry *self, gpointer user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	GtkEntryBuffer *buffer = gtk_entry_get_buffer(self);
	const char *text = gtk_entry_buffer_get_text(buffer);

	Symbol *sym;

	if (!text || strspn(text, WHITESPACE) == strlen(text))
		return;

	if (!(sym = workspace_add_def_recalc(ws, text))) {
		workspace_set_show_error(ws, TRUE);
		return;
	}

	symbol_recalculate_all();

	set_gentry(GTK_WIDGET(self), NULL);
}

static void
columnview_action(GSimpleAction *action, GVariant *parameter, View *view)
{
	Columnview *cview = COLUMNVIEW(view);
	const char *name = g_action_get_name(G_ACTION(action));
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	printf("columnview_action: %s\n", name);

	if (g_str_equal(name, "column-edit-caption"))
		columnview_edit(cview);
	else if (g_str_equal(name, "column-select-all")) {
		workspace_deselect_all(ws);
		column_select_symbols(col);
	}
	else if (g_str_equal(name, "column-duplicate")) {
		char new_name[MAX_STRSIZE];
		Column *new_col;

		workspace_column_name_new(ws, new_name);
		new_col = column_new(ws, new_name, col->x + 100, col->y);
		iobject_set(IOBJECT(new_col), NULL, IOBJECT(col)->caption);

		workspace_deselect_all(ws);
		column_select_symbols(col);
		workspace_column_select(ws, new_col);
		if (!workspace_selected_duplicate(ws))
			workspace_set_show_error(ws, TRUE);

		filemodel_set_modified(FILEMODEL(wsg), TRUE);
		workspace_deselect_all(ws);
		workspace_queue_layout(ws);
		symbol_recalculate_all();
	}
	else if (g_str_equal(name, "column-merge"))
		columnview_merge(cview);
	else if (g_str_equal(name, "column-saveas"))
		columnview_saveas(cview);
	else if (g_str_equal(name, "column-delete"))
		model_check_destroy(view_get_window(VIEW(cview)),
			MODEL(VOBJECT(cview)->iobject));
}

static void
columnview_close_clicked(GtkButton *button, void *user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);

	model_check_destroy(view_get_window(VIEW(cview)),
		MODEL(VOBJECT(cview)->iobject));
}

static void
columnview_class_init(ColumnviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("columnview.ui");
	BIND_LAYOUT();

	BIND_CALLBACK(columnview_pressed);
	BIND_CALLBACK(columnview_expand_clicked);
	BIND_CALLBACK(columnview_activate);
	BIND_CALLBACK(columnview_close_clicked);
	BIND_CALLBACK(columnview_caption_edit_activate);
	BIND_CALLBACK(columnview_caption_edit_cancel);

	BIND_VARIABLE(Columnview, top);
	BIND_VARIABLE(Columnview, title);
	BIND_VARIABLE(Columnview, expand_button);
	BIND_VARIABLE(Columnview, name);
	BIND_VARIABLE(Columnview, caption_edit_stack);
	BIND_VARIABLE(Columnview, caption);
	BIND_VARIABLE(Columnview, caption_edit);
	BIND_VARIABLE(Columnview, revealer);
	BIND_VARIABLE(Columnview, body);
	BIND_VARIABLE(Columnview, entry);

	/* Init methods.
	 */
	object_class->dispose = columnview_dispose;

	vobject_class->refresh = columnview_refresh;

	view_class->link = columnview_link;
	view_class->child_add = columnview_child_add;
	view_class->child_remove = columnview_child_remove;
	view_class->scrollto = columnview_scrollto;
	view_class->action = columnview_action;
}

static void
columnview_init(Columnview *cview)
{
	gtk_widget_init_template(GTK_WIDGET(cview));

	// the default state is used for floating cviews for row drag
	gtk_widget_set_visible(cview->title, FALSE);
	gtk_revealer_set_reveal_child(GTK_REVEALER(cview->revealer), TRUE);
	gtk_widget_set_visible(cview->entry, FALSE);
}

View *
columnview_new(void)
{
	Columnview *cview = g_object_new(COLUMNVIEW_TYPE, NULL);

	return VIEW(cview);
}

static void *
columnview_rowview_hit(View *view, void *a, void *b)
{
	Rowview *rview = ROWVIEW(view);
	graphene_point_t *sview_point = (graphene_point_t *) a;
	Subcolumnview *sview = SUBCOLUMNVIEW(b);

	graphene_rect_t bounds;
	if (!gtk_widget_compute_bounds(rview->frame, GTK_WIDGET(sview), &bounds))
		return NULL;

	if (sview_point->y > bounds.origin.y &&
		sview_point->y < bounds.origin.y + bounds.size.height)
		return rview;

	return NULL;
}

/* Find the columnview for a point.
 */
Rowview *
columnview_find_rowview(Columnview *cview, int x, int y)
{
	Workspaceview *wview = columnview_get_wview(cview);

	graphene_point_t fixed_point = GRAPHENE_POINT_INIT(x, y);
	graphene_point_t sview_point;
	if (gtk_widget_compute_point(GTK_WIDGET(wview->fixed),
			GTK_WIDGET(cview->sview), &fixed_point, &sview_point))
		return view_map(VIEW(cview->sview),
			columnview_rowview_hit, &sview_point, cview->sview);

	return NULL;
}
