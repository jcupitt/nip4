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

/* The columnview popup menu.
 */
static GtkWidget *columnview_menu = NULL;

/* Edit caption ... right button menu on title bar.
 */
static void
columnview_caption_cb(GtkWidget *wid, GtkWidget *host, Columnview *cview)
{
	/* Edit caption!
	 */
	if (cview->state == COL_EDIT)
		return;

	cview->state = COL_EDIT;
	vobject_refresh_queue(VOBJECT(cview));
}

/* Select all objects in menu's column.
 */
static void
columnview_select_cb(GtkWidget *wid, GtkWidget *host, Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	workspace_deselect_all(ws);
	column_select_symbols(col);
}

/* Clone a column.
 */
static void
columnview_clone_cb(GtkWidget *wid, GtkWidget *host, Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	Workspace *ws = col->ws;

	char new_name[MAX_STRSIZE];
	Column *newcol;

	workspace_column_name_new(ws, new_name);
	newcol = workspace_column_get(ws, new_name);
	iobject_set(IOBJECT(newcol), NULL, IOBJECT(col)->caption);
	newcol->x = col->x + 100;
	newcol->y = col->y;

	workspace_deselect_all(ws);
	column_select_symbols(col);
	workspace_column_select(ws, newcol);
	if (!workspace_selected_duplicate(ws))
		error_alert(GTK_WIDGET(cview));
	workspace_deselect_all(ws);

	symbol_recalculate_all();
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
	iWindow *iwnd = IWINDOW(view_get_toplevel(VIEW(cview)));
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
	iWindow *iwnd = IWINDOW(view_get_toplevel(VIEW(cview)));
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
		vips_snprintf(file, FILENAME_MAX,
			"$SAVEDIR" G_DIR_SEPARATOR_S "data" G_DIR_SEPARATOR_S
			"%s-%d.ws",
			name, i);
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
	iWindow *iwnd = IWINDOW(view_get_toplevel(VIEW(cview)));
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

static void
columnview_add_shadow(Columnview *old_cview)
{
	Column *col = COLUMN(VOBJECT(old_cview)->iobject);
	Workspaceview *wview = old_cview->wview;

	if (!old_cview->shadow) {
		Columnview *new_cview;

		new_cview = COLUMNVIEW(columnview_new());
		new_cview->wview = wview;
		VIEW(new_cview)->parent = VIEW(wview);
		VOBJECT(new_cview)->iobject = IOBJECT(col);

		gtk_fixed_put(GTK_FIXED(wview->fixed),
			GTK_WIDGET(new_cview), col->x, col->y);

		gtk_widget_show(GTK_WIDGET(new_cview));

		old_cview->shadow = new_cview;
		new_cview->master = old_cview;

		/* The shadow will be on top of the real column and hide it.
		 * Put the real column to the front.
		 */
		model_front(MODEL(col));
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

	VIPS_UNREF(cview->shadow);

	/* The column has gone .. relayout.
	 */
	if (col &&
		col->ws) {
		workspace_set_needs_layout(col->ws, TRUE);
		main_window_layout();
	}

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

	model_check_destroy(view_get_toplevel(VIEW(cview)), MODEL(col));
}

/* Delete this column with a click on the 'x' button.
 */
static void
columnview_destroy2_cb(GtkWidget *wid, Columnview *cview)
{
	Column *col = COLUMN(VOBJECT(cview)->iobject);

	model_check_destroy(view_get_toplevel(VIEW(cview)), MODEL(col));
}

/* Add a caption entry to a columnview if not there.
 */
static void
columnview_add_caption(Columnview *cview)
{
	printf("columnview_add_caption: FIXME\n");
}

/* Add bottom entry widget.
 */
static void
columnview_add_text(Columnview *cview)
{
	printf("columnview_add_text: FIXME\n");
}

static void
columnview_refresh(vObject *vobject)
{
	Columnview *cview = COLUMNVIEW(vobject);
	Columnview *shadow = cview->shadow;
	Column *col = COLUMN(VOBJECT(cview)->iobject);
	gboolean editable = col->ws->mode != WORKSPACE_MODE_NOEDIT;

#ifdef DEBUG
	printf("columnview_refresh: %s\n", IOBJECT(col)->name);
#endif /*DEBUG*/

	/* If this column has a shadow, workspaceview will have put a layout
	 * position into it. See workspaceview_layout_set_pos().
	 */
	if (shadow)
		view_child_position(VIEW(shadow));

	if (shadow) {
		printf("columnview_refresh: FIXME ... set shadow\n");
		/*
		gtk_widget_set_size_request(GTK_WIDGET(shadow->frame),
			GTK_WIDGET(cview->frame)->allocation.width,
			GTK_WIDGET(cview->frame)->allocation.height);
		gtk_frame_set_shadow_type(GTK_FRAME(shadow->frame),
			GTK_SHADOW_IN);
		 */
	}

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

	/* Turn titlebar on/off.
	 */
	widget_visible(cview->title, editable);
	if (editable)
		gtk_frame_set_label(GTK_FRAME(cview->frame), NULL);
	else if (IOBJECT(col)->caption) {
		GtkWidget *label;
		char buf[256];

		gtk_frame_set_label(GTK_FRAME(cview->frame), "x");
		label = gtk_frame_get_label_widget(GTK_FRAME(cview->frame));
		escape_markup(IOBJECT(col)->caption, buf, 256);
		set_glabel(label, "<b>%s</b>", buf);
	}

	/* Update names.
	 */
	set_glabel(cview->lab, "%s - ", IOBJECT(col)->name);
	if (IOBJECT(col)->caption)
		set_glabel(cview->head, "%s", IOBJECT(col)->caption);
	else {
		char buf[256];

		vips_snprintf(buf, 256, "<i>%s</i>",
			_("doubleclick to set title"));
		gtk_label_set_markup(GTK_LABEL(cview->head), buf);
	}

	/* Set open/closed.
	 */
	if (col->open) {
		set_tooltip(cview->updownb, _("Fold the column away"));
	}
	else {
		set_tooltip(cview->updownb, _("Open the column"));
	}
	model_display(MODEL(col->scol), col->open);

	/* Closed columns are hidden in NOEDIT mode.
	 */
	widget_visible(GTK_WIDGET(cview), editable || col->open);

	/* Set caption edit.
	 */
	if (cview->state == COL_EDIT) {
		columnview_add_caption(cview);

		gtk_widget_show(cview->capedit);
		gtk_widget_hide(cview->headfr);

		if (IOBJECT(col)->caption) {
			set_gentry(cview->capedit, "%s", IOBJECT(col)->caption);
			gtk_editable_select_region(GTK_EDITABLE(cview->capedit), 0, -1);
		}
		gtk_widget_grab_focus(cview->capedit);
	}
	else {
		gtk_widget_show(cview->headfr);
		VIPS_FREEF(gtk_widget_unparent, cview->capedit);
	}

	/* Set bottom entry.
	 */
	if (col->selected &&
		col->open &&
		editable &&
		!cview->master) {
		columnview_add_text(cview);
		gtk_widget_show(cview->textfr);
	}
	else
		VIPS_FREEF(gtk_widget_unparent, cview->textfr);

	/* Set select state.
	 */
	if (cview->master)
		gtk_widget_set_name(cview->title, "shadow_widget");
	else if (col->selected && !cview->selected) {
		gtk_widget_set_name(cview->title, "selected_widget");
		cview->selected = TRUE;
		if (cview->textfr)
			gtk_widget_grab_focus(cview->text);
	}
	else if (!col->selected) {
		/* Always do this, even if cview->selected, so we set on the
		 * first _refresh().
		 */
		gtk_widget_set_name(cview->title, "column_widget");
		cview->selected = FALSE;
	}

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

	printf("columnview_child_add: FIXME ... add to container?\n");
	// gtk_container_add(GTK_CONTAINER(cview->frame), GTK_WIDGET(sview));
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

	if (position == MODEL_SCROLL_BOTTOM)
		/* 35 is supposed to be enough to ensure the whole of the edit
		 * box gets on the screen.
		 */
		workspaceview_scroll(wview, x, y + h, w, 35);
	else
		// workspaceview_scroll(wview, x, y, w, cview->title->allocation.height);
		workspaceview_scroll(wview, x, y, w, 50);
}

#define BIND(field) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		Columnview, field);

static void
columnview_class_init(ColumnviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	/* Create signals.
	 */

	/* Init methods.
	 */
	object_class->dispose = columnview_dispose;

	gtk_widget_class_set_layout_manager_type(widget_class,
		GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
		APP_PATH "/columbview.ui");

	BIND(title);

	vobject_class->refresh = columnview_refresh;

	view_class->link = columnview_link;
	view_class->child_add = columnview_child_add;
	view_class->scrollto = columnview_scrollto;

	printf("columnview_class_init: columnview menu FIXME\n");
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
