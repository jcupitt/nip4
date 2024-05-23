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

		if (IOBJECT(col)->caption) {
			GtkEntryBuffer *buffer =
				gtk_entry_buffer_new(IOBJECT(col)->caption, -1);
			gtk_entry_set_buffer(GTK_ENTRY(cview->caption_edit), buffer);
		}

		vobject_refresh_queue(VOBJECT(cview));
		gtk_widget_grab_focus(cview->caption_edit);
	}
}

static void
columnview_caption_edit_activate(GtkEntry *self, gpointer user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	GtkEntryBuffer *buffer = gtk_entry_get_buffer(self);
	const char *text = gtk_entry_buffer_get_text(buffer);

	if (text && strspn(text, WHITESPACE) != strlen(text)) {
		VIPS_SETSTR(IOBJECT(col)->caption, text);
		workspace_set_modified(ws, TRUE);
	}

	cview->state = COL_WAIT;
	vobject_refresh_queue(VOBJECT(cview));
}

static gboolean
columnview_caption_key_pressed(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);

	gboolean handled;

	handled = FALSE;

	if (keyval == GDK_KEY_Escape) {
	    cview->state = COL_WAIT;
	    vobject_refresh_queue(VOBJECT(cview));
		handled = TRUE;
	}

	return handled;
}

/*
static void
columnview_merge_sub(iWindow *iwnd,
	void *client, iWindowNotifyFn nfn, void *sys)
{
	Filesel *filesel = FILESEL(iwnd);
	Column *col = COLUMN(client);
	Workspace *ws = col->ws;
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	char *filename;
	iWindowResult result;

	result = IWINDOW_YES;
	progress_begin();

	if ((filename = filesel_get_filename(filesel))) {
		if (!workspacegroup_merge_rows(wsg, filename))
			result = IWINDOW_ERROR;

		g_free(filename);
	}

	symbol_recalculate_all();
	progress_end();

	nfn(sys, result);
}
 */

static void
columnview_merge_cb(GtkWidget *wid, GtkWidget *host, Columnview *cview)
{
	/*
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	GtkWindow *window = view_get_window(VIEW(cview));
	GtkWidget *filesel = filesel_new();

	iwindow_set_title(IWINDOW(filesel),
		_("Merge Into Column \"%s\""), IOBJECT(col)->name);
	filesel_set_flags(FILESEL(filesel), FALSE, FALSE);
	filesel_set_filetype(FILESEL(filesel), filesel_type_workspace, 0);
	iwindow_set_parent(IWINDOW(filesel), GTK_WIDGET(iwnd));
	idialog_set_iobject(IDIALOG(filesel), IOBJECT(col));
	filesel_set_done(FILESEL(filesel), columnview_merge_sub, col);
	iwindow_build(IWINDOW(filesel));

	gtk_widget_show(GTK_WIDGET(filesel));
	 */

	printf("columnview_merge_cb: FIXME\n");
}

/* Callback from save browser.
static void
columnview_save_as_sub(iWindow *iwnd,
	void *client, iWindowNotifyFn nfn, void *sys)
{
	Filesel *filesel = FILESEL(iwnd);
	Column *col = COLUMN(client);
	Workspace *ws = col->ws;
	char *filename;

	workspace_deselect_all(ws);
	column_select_symbols(col);

	if ((filename = filesel_get_filename(filesel))) {
		if (workspace_selected_save(ws, filename)) {
			workspace_deselect_all(ws);
			nfn(sys, IWINDOW_YES);
		}
		else
			nfn(sys, IWINDOW_ERROR);

		g_free(filename);
	}
	else
		nfn(sys, IWINDOW_ERROR);
}
 */

/* Save a column ... can't just do view_save_as_cb(), since we need to save
 * the enclosing workspace too. Hence we have to save_selected on the ws, but
 * only after we have the filename.
 */
static void
columnview_save_as_cb(GtkWidget *wid, GtkWidget *host, Columnview *cview)
{
	/*
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	GtkWindow *window = view_get_window(VIEW(cview));
	GtkWidget *filesel = filesel_new();

	iwindow_set_title(IWINDOW(filesel),
		_("Save Column \"%s\""), IOBJECT(col)->name);
	filesel_set_flags(FILESEL(filesel), FALSE, TRUE);
	filesel_set_filetype(FILESEL(filesel), filesel_type_workspace, 0);
	iwindow_set_parent(IWINDOW(filesel), GTK_WIDGET(iwnd));
	idialog_set_iobject(IDIALOG(filesel), IOBJECT(col));
	filesel_set_done(FILESEL(filesel), columnview_save_as_sub, col);
	iwindow_build(IWINDOW(filesel));

	gtk_widget_show(GTK_WIDGET(filesel));
	 */

	printf("columnview_save_as_cb:\n");
}

/* Make a name for a column menu file, based on what we're going to call the
 * menu item.
 */
static void
columnview_filename(char *file, const char *caption)
{
	int i;
	char name[FILENAME_MAX];

	vips_strncpy(name, caption, 10);
	for (i = 0; i < strlen(name); i++)
		if (name[i] == ' ')
			name[i] = '_';

	for (i = 0;; i++) {
		vips_snprintf(file, FILENAME_MAX, "$SAVEDIR/data/%s-%d.ws", name, i);
		if (!existsf("%s", file))
			break;
	}
}

/* Remember the name of the last toolkit the user asked to add to.
 */
static char *columnview_to_menu_last_toolkit = NULL;

/* Done button hit.
static void
columnview_to_menu_done_cb(iWindow *iwnd, void *client,
	iWindowNotifyFn nfn, void *sys)
{
	Column *col = COLUMN(client);
	Workspace *ws = col->ws;
	Stringset *ss = STRINGSET(iwnd);
	StringsetChild *name = stringset_child_get(ss, _("Name"));
	StringsetChild *toolkit = stringset_child_get(ss, _("Toolkit"));
	StringsetChild *file = stringset_child_get(ss, _("Filename"));

	char name_text[1024];
	char toolkit_text[1024];
	char file_text[1024];

	if (!get_geditable_string(name->entry, name_text, 1024) ||
		!get_geditable_name(toolkit->entry, toolkit_text, 1024) ||
		!get_geditable_filename(file->entry, file_text, 1024)) {
		nfn(sys, IWINDOW_ERROR);
		return;
	}

	workspace_deselect_all(ws);
	column_select_symbols(col);

	if (!workspace_selected_save(ws, file_text)) {
		nfn(sys, IWINDOW_ERROR);
		return;
	}

	workspace_deselect_all(ws);

	if (!tool_new_dia(toolkit_by_name(ws->kitg, toolkit_text),
			-1, name_text, file_text)) {
		unlinkf("%s", file_text);
		nfn(sys, IWINDOW_ERROR);
		return;
	}

	VIPS_SETSTR(columnview_to_menu_last_toolkit, toolkit_text);

	nfn(sys, IWINDOW_YES);
}
 */

/* Make a column into a menu item.
 */
static void
columnview_to_menu_cb(GtkWidget *wid, GtkWidget *host, Columnview *cview)
{
	/*
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	GtkWindow *window = view_get_window(VIEW(cview));
	GtkWidget *ss = stringset_new();
	char *name;
	char *kit_name;
	char filename[FILENAME_MAX];

	if (!(name = IOBJECT(col)->caption))
		name = "untitled";
	columnview_filename(filename, name);

	if (columnview_to_menu_last_toolkit)
		kit_name = columnview_to_menu_last_toolkit;
	else
		kit_name = "untitled";

	stringset_child_new(STRINGSET(ss),
		_("Name"), name, _("Set menu item text here"));
	stringset_child_new(STRINGSET(ss),
		_("Toolkit"), kit_name, _("Add to this toolkit"));
	stringset_child_new(STRINGSET(ss),
		_("Filename"), filename, _("Store column in this file"));

	iwindow_set_title(IWINDOW(ss),
		_("New Menu Item from Column \"%s\""), IOBJECT(col)->name);
	idialog_set_callbacks(IDIALOG(ss),
		iwindow_true_cb, NULL, NULL, col);
	idialog_set_help_tag(IDIALOG(ss), "sec:diaref");
	idialog_add_ok(IDIALOG(ss), columnview_to_menu_done_cb,
		_("Menuize"));
	iwindow_set_parent(IWINDOW(ss), GTK_WIDGET(iwnd));
	iwindow_build(IWINDOW(ss));

	gtk_widget_show(ss);
	 */

	printf("columnview_to_menu_cb: FIXME\n");
}

/* Find the position and size of a columnview.
 */
void
columnview_get_position(Columnview *cview, int *x, int *y, int *w, int *h)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(cview));

	graphene_rect_t bounds;

	if (gtk_widget_compute_bounds(GTK_WIDGET(cview), parent, &bounds)) {
		*x = bounds.origin.x;
		*y = bounds.origin.y;
		*w = bounds.size.width;
		*h = bounds.size.height;

#ifdef DEBUG
		printf("columnview_get_position: %s, x = %d, y = %d, w = %d, h = %d\n",
			IOBJECT(col)->name, *x, *y, *w, *h);
#endif /*DEBUG*/
	}
	else {
		/* Nothing there yet ... guess.
		 */
		*x = col->x;
		*y = col->y;
		*w = 200;
		*h = 50;
	}
}

void
columnview_add_shadow(Columnview *cview)
{
	if (!cview->shadow) {
		Column *col = COLUMN(VOBJECT(cview)->iobject);
		Workspaceview *wview = cview->wview;

		Columnview *shadow;

		printf("columnview_add_shadow:\n");

		/* We can't use model_view_new() etc as we don't want the shadow to be
		 * part of the viewchild system, or to auto update when col updates.
		 */
		shadow = COLUMNVIEW(columnview_new());
		shadow->wview = wview;
		VIEW(shadow)->parent = VIEW(wview);
		VOBJECT(shadow)->iobject = IOBJECT(col);
		cview->shadow = shadow;
		shadow->master = cview;

		/* Shadow will have one ref held by fixed.
		 */
		gtk_fixed_put(GTK_FIXED(wview->fixed),
			GTK_WIDGET(shadow), col->x, col->y);

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
		Workspaceview *wview = cview->wview;

		printf("columnview_remove_shadow:\n");

		cview->shadow->master = NULL;
		view_child_remove(cview->shadow);
		cview->shadow = NULL;
	}
}

static void
columnview_dispose(GObject *object)
{
	Columnview *cview;
	Column *col;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_COLUMNVIEW(object));

	cview = COLUMNVIEW(object);
	col = COLUMN(VOBJECT(cview)->iobject);

#ifdef DEBUG
	printf("columnview_dispose:\n");
#endif /*DEBUG*/

	if (cview->shadow)
		view_child_remove(VIEW(cview->shadow));

	gtk_widget_dispose_template(GTK_WIDGET(cview), COLUMNVIEW_TYPE);

	G_OBJECT_CLASS(columnview_parent_class)->dispose(object);
}

/* Arrow button on title bar.
 */
static void
columnview_updown_cb(GtkWidget *wid, Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	column_set_open(col, !col->open);
}

/* Delete this column from the popup menu.
 */
static void
columnview_destroy_cb(GtkWidget *wid, GtkWidget *host, Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	model_check_destroy(GTK_WIDGET(cview), MODEL(col));
}

/* Delete this column with a click on the 'x' button.
 */
static void
columnview_destroy2_cb(GtkWidget *wid, Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	model_check_destroy(GTK_WIDGET(cview), MODEL(col));
}

/* Add a caption entry to a columnview if not there.
 */
static void
columnview_add_caption(Columnview *cview)
{
	printf("columnview_add_caption: FIXME\n");
}

static const char *
columnview_css(Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	if (cview->master)
		return "shadow_widget";
	else if (col->selected)
		return "selected_widget";
	else
		return "widget";
}

static void
columnview_refresh(vObject *vobject)
{
	Columnview *cview = COLUMNVIEW(vobject);
	Columnview *shadow = cview->shadow;
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	if (!col)
		return;

	gboolean editable = col->ws->mode != WORKSPACE_MODE_NOEDIT;

#ifdef DEBUG
	printf("columnview_refresh: %s\n", IOBJECT(col)->name);
#endif /*DEBUG*/

	/* If this column has a shadow, workspaceview will have put a layout
	 * position into it. See workspaceview_layout_set_pos().
	 */
	if (shadow)
		view_child_position(VIEW(shadow));

	if (col->x != cview->lx ||
		col->y != cview->ly) {
#ifdef DEBUG
		printf("columnview_refresh: move column %s to %d x %d\n",
			IOBJECT(col)->name, col->x, col->y);
#endif /*DEBUG*/

		cview->lx = col->x;
		cview->ly = col->y;
		view_child_position(VIEW(cview));

		/* Update the save offset hints too.
		 */
		filemodel_set_offset(FILEMODEL(col), cview->lx, cview->ly);
	}

	/* Titlebar off in no-edit mode.
	 */
	gtk_widget_set_visible(cview->title, editable);

	/* Update name and caption.
	 */
	set_glabel(cview->name, "%s", IOBJECT(col)->name);
	if (IOBJECT(col)->caption)
		set_glabel(cview->caption, "%s", IOBJECT(col)->caption);
	else {
		char buf[256];

		set_glabel(cview->caption, "double-click to set caption");
	}
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
			!cview->master);

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
	Columnview *cview = COLUMNVIEW(view);
	Workspaceview *wview = WORKSPACEVIEW(parent);

	VIEW_CLASS(columnview_parent_class)->link(view, model, parent);

	cview->wview = wview;
}

static void
columnview_child_add(View *parent, View *child)
{
	Columnview *cview = COLUMNVIEW(parent);
	Subcolumnview *sview = SUBCOLUMNVIEW(child);

	VIEW_CLASS(columnview_parent_class)->child_add(parent, child);

	gtk_box_prepend(GTK_BOX(cview->body), GTK_WIDGET(sview));
}

static void
columnview_child_remove(View *parent, View *child)
{
	Columnview *cview = COLUMNVIEW(parent);
	Subcolumnview *sview = SUBCOLUMNVIEW(child);

#ifdef DEBUG
	printf("columnview_child_remove:\n");
#endif /*DEBUG*/

	VIEW_CLASS(columnview_parent_class)->child_remove(parent, child);

	// must be at the end since this will unref the child
	gtk_box_remove(GTK_BOX(cview->body), GTK_WIDGET(sview));
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
	Workspaceview *wview = cview->wview;

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
columnview_menu(GtkGestureClick *gesture,
	guint n_press, double x, double y, Columnview *cview)
{
	// menu will act on this widget
	mainwindow_set_action_view(VIEW(cview));

	gtk_popover_set_pointing_to(GTK_POPOVER(cview->right_click_menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	gtk_popover_popup(GTK_POPOVER(cview->right_click_menu));
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
		mainwindow_error(MAINWINDOW(view_get_window(VIEW(cview))));
		symbol_recalculate_all();
		return;
	}

	set_gentry(GTK_WIDGET(self), NULL);
}

static void
columnview_action(GSimpleAction *action, GVariant *parameter, View *view)
{
	Columnview *cview = COLUMNVIEW(view);
	const char *name = g_action_get_name(G_ACTION(action));
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	printf("columnview_action: %s\n", name);

	if (g_str_equal(name, "column-delete")) {
		Column *col = COLUMN(VOBJECT(cview)->iobject);

		iobject_destroy(IOBJECT(col));
	}
	else if (g_str_equal(name, "column-duplicate")) {
		char new_name[MAX_STRSIZE];
		Column *new_col;

		workspace_column_name_new(ws, new_name);
		new_col = workspace_column_get(ws, new_name);
		iobject_set(IOBJECT(new_col), NULL, IOBJECT(col)->caption);
		new_col->x = col->x + 100;
		new_col->y = col->y;

		workspace_deselect_all(ws);
		column_select_symbols(col);
		workspace_column_select(ws, new_col);
		if (!workspace_selected_duplicate(ws))
			mainwindow_error(MAINWINDOW(view_get_window(view)));
		workspace_deselect_all(ws);
		workspace_queue_layout(ws);

		symbol_recalculate_all();
	}
	else if (g_str_equal(name, "column-edit-caption"))
		columnview_edit(cview);
	else if (g_str_equal(name, "column-select-all")) {
		workspace_deselect_all(ws);
		column_select_symbols(col);
	}
}

static void
columnview_close_clicked(GtkButton *button, void *user_data)
{
	Columnview *cview = COLUMNVIEW(user_data);
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	printf("columnview_close_clicked:\n");

	iobject_destroy(col);
}

static void
columnview_class_init(ColumnviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("columnview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_CALLBACK(columnview_pressed);
	BIND_CALLBACK(columnview_expand_clicked);
	BIND_CALLBACK(columnview_menu);
	BIND_CALLBACK(columnview_activate);
	BIND_CALLBACK(columnview_close_clicked);
	BIND_CALLBACK(columnview_caption_edit_activate);
	BIND_CALLBACK(columnview_caption_key_pressed);

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
	BIND_VARIABLE(Columnview, right_click_menu);

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
}

View *
columnview_new(void)
{
	Columnview *cview = g_object_new(COLUMNVIEW_TYPE, NULL);

	return VIEW(cview);
}
