// main application window

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

/*
#define DEBUG
 */

#include "nip4.h"

struct _Mainwindow {
	GtkApplicationWindow parent;

	/* The current save and load directories.
	 */
	GFile *save_folder;
	GFile *load_folder;

	// the model we display
	Workspacegroup *wsg;

	// The view we trigger actions on
	View *action_view;

	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *progress_bar;
	GtkWidget *progress;
	GtkWidget *progress_cancel;
	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *wsgview;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;

	/* Batch refresh with this, it's slow.
	 */
	guint refresh_timeout;
};

// current autocalc state
gboolean mainwindow_auto_recalc = TRUE;

// relayout timer
static gint mainwindow_layout_timeout = 0;

G_DEFINE_TYPE(Mainwindow, mainwindow, GTK_TYPE_APPLICATION_WINDOW);

static void
mainwindow_error(Mainwindow *main)
{
	char txt[256];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	vips_buf_appendf(&buf, "<span weight=\"regular\"><span size=\"large\">%s</span>\n%s</span>",
		error_get_top(), error_get_sub());
	gtk_label_set_markup(GTK_LABEL(main->error_label), vips_buf_all(&buf));
#ifdef DEBUG
	printf("mainwindow_set_error: %s\n", vips_buf_all(&buf));
#endif /*DEBUG*/

	gtk_info_bar_set_revealed(GTK_INFO_BAR(main->error_bar), TRUE);
}

static void
mainwindow_gerror(Mainwindow *main, GError **error)
{
	if (error && *error) {
		error_top("Error");
		error_sub("%s", (*error)->message);
		g_error_free(*error);
		mainwindow_error(main);
	}
}

static void
mainwindow_verror(Mainwindow *main)
{
	error_vips();
	mainwindow_error(main);
}

static void
mainwindow_error_hide(Mainwindow *main)
{
#ifdef DEBUG
	printf("mainwindow_error_hide:\n");
#endif /*DEBUG*/

	gtk_info_bar_set_revealed(GTK_INFO_BAR(main->error_bar), FALSE);
}

static void
mainwindow_dispose(GObject *object)
{
	Mainwindow *main = MAINWINDOW(object);

#ifdef DEBUG
	printf("mainwindow_dispose:\n");
#endif /*DEBUG*/

	IDESTROY(main->wsg);

	VIPS_FREEF(g_timer_destroy, main->progress_timer);
	VIPS_FREEF(g_source_remove, main->refresh_timeout);

	G_OBJECT_CLASS(mainwindow_parent_class)->dispose(object);
}

static void
mainwindow_preeval(VipsImage *image, VipsProgress *progress, Mainwindow *main)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->progress_bar), TRUE);
}

typedef struct _EvalUpdate {
	Mainwindow *main;
	int eta;
	int percent;
} EvalUpdate;

static gboolean
mainwindow_eval_idle(void *user_data)
{
	EvalUpdate *update = (EvalUpdate *) user_data;
	Mainwindow *main = update->main;

	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC(str);

	vips_buf_appendf(&buf, "%d%% complete, %d seconds to go",
		update->percent, update->eta);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(main->progress),
		vips_buf_all(&buf));

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(main->progress),
		update->percent / 100.0);

	g_object_unref(main);
	g_free(update);

	return FALSE;
}

static void
mainwindow_eval(VipsImage *image,
	VipsProgress *progress, Mainwindow *main)
{
	double time_now;
	EvalUpdate *update;

	/* We can be ^Q'd during load. This is NULLed in _dispose.
	 */
	if (!main->progress_timer)
		return;

	time_now = g_timer_elapsed(main->progress_timer, NULL);

	/* Throttle to 10Hz.
	 */
	if (time_now - main->last_progress_time < 0.1)
		return;
	main->last_progress_time = time_now;

#ifdef DEBUG_VERBOSE
	printf("mainwindow_eval: %d%%\n", progress->percent);
#endif /*DEBUG_VERBOSE*/

	/* This can come from the background load thread, so we can't update
	 * the UI directly.
	 */

	update = g_new(EvalUpdate, 1);
	update->main = main;
	update->percent = progress->percent;
	update->eta = progress->eta;

	/* We don't want main to vanish before we process this update. The
	 * matching unref is in the handler above.
	 */
	g_object_ref(main);

	g_idle_add(mainwindow_eval_idle, update);
}

static void
mainwindow_posteval(VipsImage *image,
	VipsProgress *progress, Mainwindow *main)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->progress_bar), FALSE);
}

static void
mainwindow_cancel_clicked(GtkWidget *button, Mainwindow *main)
{
	VipsImage *image;

	// cancel
}

static void
mainwindow_error_response(GtkWidget *button, int response,
	Mainwindow *main)
{
	mainwindow_error_hide(main);
}

static GFile *
get_parent(GFile *file)
{
	GFile *parent = g_file_get_parent(file);

	return parent ? parent : g_file_new_for_path("/");
}

static void
mainwindow_open_result(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		Workspacegroup *wsg;

		g_autofree char *filename = g_file_get_path(file);

		GtkApplication *app = gtk_window_get_application(GTK_WINDOW(main));

		// quick hack ... yuk!

		if (!(wsg = workspacegroup_new_from_file(main_workspaceroot,
				  filename, filename))) {
			mainwindow_error(main);
			return;
		}

		Mainwindow *new_main = g_object_new(MAINWINDOW_TYPE,
			"application", app,
			NULL);

		mainwindow_set_wsg(new_main, wsg);

		mainwindow_set_gfile(new_main, NULL);

		gtk_window_present(GTK_WINDOW(new_main));
	}
}

static void
mainwindow_open_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	GtkFileDialog *dialog;
	GFile *file;

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Open workspace");
	gtk_file_dialog_set_accept_label(dialog, "Open");
	gtk_file_dialog_set_modal(dialog, TRUE);

	// if we have a loaded file, use that folder, else
	if (main->load_folder)
		gtk_file_dialog_set_initial_folder(dialog, main->load_folder);

	gtk_file_dialog_open(dialog, GTK_WINDOW(main), NULL,
		mainwindow_open_result, main);
}

static void
mainwindow_on_file_save(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		// note the save directory for next time
		VIPS_UNREF(main->save_folder);
		main->save_folder = get_parent(file);

		g_autofree char *filename = g_file_get_path(file);

		if (!workspacegroup_save_current(main->wsg, filename))
			mainwindow_error(main);
	}
}

static void
mainwindow_saveas_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	GtkFileDialog *dialog;
	GFile *file;

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save file");
	gtk_file_dialog_set_modal(dialog, TRUE);

	// if we loaded from a file, use that dir, else
	if (main->save_folder)
		gtk_file_dialog_set_initial_folder(dialog, main->save_folder);

	gtk_file_dialog_save(dialog, GTK_WINDOW(main), NULL,
		&mainwindow_on_file_save, main);
}

static void
mainwindow_save_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	// there needs to be a model to save
	if (!main->wsg)
		return;

	// unmodified? no save to do
	if (!FILEMODEL(main->wsg)->modified)
		return;

	const char *filename;
	if (!(filename = FILEMODEL(main->wsg)->filename)) {
		// no filename, we need to go via save-as
		mainwindow_saveas_action(action, parameter, user_data);
	}
	else {
		// we have a filename associated with this workspacegroup ... we can
		// just save directly
		if (!workspacegroup_save_current(main->wsg, filename))
			mainwindow_error(main);
	}
}

static void
mainwindow_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	gtk_window_destroy(GTK_WINDOW(main));
}

static void
mainwindow_fullscreen(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	g_object_set(main,
		"fullscreened", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

// call ->action on the linked view
static void
mainwindow_view_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	printf("mainwindow_view_action: %s\n",
			g_action_get_name(G_ACTION(action)));

	Mainwindow *main = MAINWINDOW(user_data);

	if (main->action_view &&
		IS_VIEW(main->action_view)) {
		View *view = VIEW(main->action_view);
		ViewClass *view_class = VIEW_GET_CLASS(view);

		if (view_class->action)
			view_class->action(action, parameter, view);
	}
}

static GActionEntry mainwindow_entries[] = {
	// main window actions
	{ "open", mainwindow_open_action },
	{ "saveas", mainwindow_saveas_action },
	{ "save", mainwindow_save_action },
	{ "close", mainwindow_close_action },
	{ "fullscreen", action_toggle, NULL, "false", mainwindow_fullscreen },

	// workspaceview rightclick menu
	{ "column-new", mainwindow_view_action },

	// column menu
	{ "column-edit-caption", mainwindow_view_action },
	{ "column-select-all", mainwindow_view_action },
	{ "column-duplicate", mainwindow_view_action },
	{ "column-merge", mainwindow_view_action },
	{ "column-saveas", mainwindow_view_action },
	{ "column-delete", mainwindow_view_action },

};

static void
mainwindow_init(Mainwindow *main)
{
	GtkEventController *controller;

#ifdef DEBUG
	printf("mainwindow_init:\n");
#endif /*DEBUG*/

	main->progress_timer = g_timer_new();
	main->last_progress_time = -1;
	char *cwd = g_get_current_dir();
	main->save_folder = g_file_new_for_path(cwd);
	main->load_folder = g_file_new_for_path(cwd);
	g_free(cwd);

	gtk_widget_init_template(GTK_WIDGET(main));

	g_signal_connect_object(main->progress_cancel, "clicked",
		G_CALLBACK(mainwindow_cancel_clicked), main, 0);

	g_signal_connect_object(main->error_bar, "response",
		G_CALLBACK(mainwindow_error_response), main, 0);

	g_action_map_add_action_entries(G_ACTION_MAP(main),
		mainwindow_entries, G_N_ELEMENTS(mainwindow_entries),
		main);

	/* We are a drop target for filenames and images.
	 *
	 * FIXME ... implement this
	 *
	controller = GTK_EVENT_CONTROLLER(
		gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY));
	gtk_drop_target_set_gtypes(GTK_DROP_TARGET(controller),
		mainwindow_supported_types,
		mainwindow_n_supported_types);
	g_signal_connect(controller, "drop",
		G_CALLBACK(mainwindow_dnd_drop), main);
	gtk_widget_add_controller(main->imagedisplay, controller);
	 */
}

static void
mainwindow_class_init(MainwindowClass *class)
{
	G_OBJECT_CLASS(class)->dispose = mainwindow_dispose;

	BIND_RESOURCE("mainwindow.ui");

	BIND_VARIABLE(Mainwindow, title);
	BIND_VARIABLE(Mainwindow, subtitle);
	BIND_VARIABLE(Mainwindow, gears);
	BIND_VARIABLE(Mainwindow, progress_bar);
	BIND_VARIABLE(Mainwindow, progress);
	BIND_VARIABLE(Mainwindow, progress_cancel);
	BIND_VARIABLE(Mainwindow, error_bar);
	BIND_VARIABLE(Mainwindow, error_label);
	BIND_VARIABLE(Mainwindow, wsgview);
}

static Workspace *
mainwindow_get_workspace(Mainwindow *main)
{
	Workspace *ws;

	if (main->wsg &&
		(ws = WORKSPACE(ICONTAINER(main->wsg)->current)))
		return ws;

	return NULL;
}

static void
mainwindow_title_update(Mainwindow *main)
{
	Workspace *ws;
	char *filename;

	char txt1[512];
	char txt2[512];
	VipsBuf title = VIPS_BUF_STATIC(txt1);
	VipsBuf subtitle = VIPS_BUF_STATIC(txt2);

	if (main->wsg &&
		FILEMODEL(main->wsg)->modified)
		vips_buf_appendf(&title, "*");

	if (main->wsg &&
		(filename = FILEMODEL(main->wsg)->filename)) {
		g_autofree char *base = g_path_get_basename(filename);
		g_autofree char *dir = g_path_get_dirname(filename);

		vips_buf_appendf(&title, "%s", base);
		vips_buf_appendf(&subtitle, "%s", dir);
	}
	else
		vips_buf_appends(&title, _("unsaved workspace"));

	if ((ws = mainwindow_get_workspace(main))) {
		vips_buf_appends(&title, " - ");
		vips_buf_appendf(&title, "%s", IOBJECT(ws->sym)->name);
		if (ws->compat_major) {
			vips_buf_appends(&title, " - ");
			vips_buf_appends(&title, _("compatibility mode"));
			vips_buf_appendf(&title, " %d.%d",
				ws->compat_major,
				ws->compat_minor);
		}
	}

	gtk_label_set_text(GTK_LABEL(main->title), vips_buf_all(&title));
	gtk_label_set_text(GTK_LABEL(main->subtitle), vips_buf_all(&subtitle));
}

static gboolean
mainwindow_refresh_timeout(gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	main->refresh_timeout = 0;

	mainwindow_title_update(main);

	// quite a lot of stuff to go in here

	return FALSE;
}

static void
mainwindow_refresh(Mainwindow *main)
{
	VIPS_FREEF(g_source_remove, main->refresh_timeout);

	main->refresh_timeout = g_timeout_add(100,
		(GSourceFunc) mainwindow_refresh_timeout, main);
}

static void
mainwindow_wsg_changed(Workspacegroup *wsg, Mainwindow *main)
{
	mainwindow_refresh(main);
}

static void
mainwindow_wsg_destroy(Workspacegroup *wsg, Mainwindow *main)
{
	printf("mainwindow_wsg_destroy:\n");
	main->wsg = NULL;
}

void
mainwindow_set_wsg(Mainwindow *main, Workspacegroup *wsg)
{
	g_assert(!main->wsg);

	main->wsg = wsg;
	iobject_ref_sink(IOBJECT(main->wsg));

	// our wsgview is the view for this model
	view_link(VIEW(main->wsgview), MODEL(main->wsg), NULL);

	g_signal_connect_object(main->wsg, "changed",
		G_CALLBACK(mainwindow_wsg_changed), main, 0);
	g_signal_connect_object(main->wsg, "destroy",
		G_CALLBACK(mainwindow_wsg_destroy), main, 0);

	/* Set start state.
	 */
	(void) mainwindow_refresh(main);
}

void
mainwindow_set_gfile(Mainwindow *main, GFile *gfile)
{
#ifdef DEBUG
	printf("mainwindow_set_gfile:\n");
#endif /*DEBUG*/

	if (gfile) {
		g_autofree char *file = g_file_get_path(gfile);
		Workspaceroot *root = main_workspaceroot;

		Workspacegroup *wsg;

		if (!(wsg = workspacegroup_new_from_file(root, file, file))) {
			mainwindow_error(main);
			return;
		}

		mainwindow_set_wsg(main, wsg);

		symbol_recalculate_all();
	}
}

static GSettings *
mainwindow_settings(Mainwindow *main)
{
	App *app;

	// FIXME ... or have an "app" member in main? or use
	// gtk_window_get_application()?
	g_object_get(main, "application", &app, NULL);

	return app ? app_settings(app) : NULL;
}

static void
mainwindow_init_settings(Mainwindow *main)
{
	GSettings *settings = mainwindow_settings(main);

	if (settings) {
		// FIXME ... set window state from settings
	}
}

Mainwindow *
mainwindow_new(App *app)
{
	Mainwindow *main = g_object_new(MAINWINDOW_TYPE,
		"application", app,
		NULL);

	printf("mainwindow_new: FIXME .. make col A?\n");

	/* Make a start workspace and workspacegroup to load
	 * stuff into.
	Workspacegroup *wsg = workspacegroup_new_blank(main_workspaceroot, NULL);
	Workspace *ws = WORKSPACE(icontainer_get_nth_child(ICONTAINER(wsg), 0));
	mainwindow_set_wsg(main, wsg);

	mainwindow_set_gfile(main, NULL);
	 */

	// we can't do this in _init() since we need app to be set
	mainwindow_init_settings(main);

	return main;
}

void
mainwindow_cull(void)
{
	// destroy any empty Mainwindow (may have had all tabs dragged out)
	printf("mainwindow_cull: FIXME ... implement this\n");
}

static void *
mainwindow_layout_sub(Workspace *ws)
{
	model_layout(MODEL(ws));
	workspace_set_needs_layout(ws, FALSE);

	return NULL;
}

static gboolean
mainwindow_layout_timeout_cb(gpointer user_data)
{
	printf("mainwindow_layout_timeout_cb:\n");

	mainwindow_layout_timeout = 0;

	slist_map(workspace_get_needs_layout(),
		(SListMapFn) mainwindow_layout_sub, NULL);

	return FALSE;
}

void
mainwindow_layout(void)
{
	VIPS_FREEF(g_source_remove, mainwindow_layout_timeout);
	mainwindow_layout_timeout = g_timeout_add(300,
		(GSourceFunc) mainwindow_layout_timeout_cb, NULL);
}

void
mainwindow_set_action_view(View *action_view)
{
	GtkWidget *widget = GTK_WIDGET(action_view);
	Mainwindow *main = MAINWINDOW(gtk_widget_get_root(widget));

	if (main)
		main->action_view = action_view;
}
