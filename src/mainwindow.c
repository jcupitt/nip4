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
	GtkWidget *error_bar;
	GtkWidget *error_top;
	GtkWidget *error_sub;
	GtkWidget *wsgview;

	/* Batch refresh with this, it's slow.
	 */
	guint refresh_timeout;

	/* Set for progress cancel.
	 */
	gboolean cancel;
};

// current autocalc state
gboolean mainwindow_auto_recalc = TRUE;

// every alive mainwindow
static GSList *mainwindow_all = NULL;

G_DEFINE_TYPE(Mainwindow, mainwindow, GTK_TYPE_APPLICATION_WINDOW);

void
mainwindow_set_action_view(View *action_view)
{
	Mainwindow *main = MAINWINDOW(view_get_window(action_view));

	if (main)
		main->action_view = action_view;
}

Workspacegroupview *
mainwindow_get_workspacegroupview(Mainwindow *main)
{
	return WORKSPACEGROUPVIEW(main->wsgview);
}

GFile *
mainwindow_get_save_folder(Mainwindow *main)
{
	return main->save_folder;
}

static GFile *
get_parent(GFile *file)
{
	GFile *parent = g_file_get_parent(file);

	return parent ? parent : g_file_new_for_path("/");
}

void
mainwindow_set_save_folder(Mainwindow *main, GFile *file)
{
	VIPS_UNREF(main->save_folder);
	main->save_folder = get_parent(file);
}

GFile *
mainwindow_get_load_folder(Mainwindow *main)
{
	return main->load_folder;
}

void
mainwindow_set_load_folder(Mainwindow *main, GFile *file)
{
	VIPS_UNREF(main->load_folder);
	main->load_folder = get_parent(file);
}

void
mainwindow_error(Mainwindow *main)
{
	set_glabel(main->error_top, error_get_top());
	set_glabel(main->error_sub, error_get_sub());
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->error_bar), TRUE);
}

void
mainwindow_error_hide(Mainwindow *main)
{
#ifdef DEBUG
	printf("mainwindow_error_hide:\n");
#endif /*DEBUG*/

	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->error_bar), FALSE);
}

static void
mainwindow_dispose(GObject *object)
{
	Mainwindow *main = MAINWINDOW(object);

#ifdef DEBUG
	printf("mainwindow_dispose:\n");
#endif /*DEBUG*/

	IDESTROY(main->wsg);
	VIPS_FREEF(g_source_remove, main->refresh_timeout);

	mainwindow_all = g_slist_remove(mainwindow_all, main);

	G_OBJECT_CLASS(mainwindow_parent_class)->dispose(object);
}

static gboolean
mainwindow_open_workspace(Mainwindow *main, const char *filename)
{
	GtkApplication *app = gtk_window_get_application(GTK_WINDOW(main));

	Workspacegroup *wsg;
	if (!(wsg = workspacegroup_new_from_file(main_workspaceroot,
			  filename, filename))) {
		return FALSE;
	}

	Mainwindow *new_main = g_object_new(MAINWINDOW_TYPE,
		"application", app,
		NULL);

	mainwindow_set_wsg(new_main, wsg);
	mainwindow_set_gfile(new_main, NULL);
	gtk_window_present(GTK_WINDOW(new_main));

	symbol_recalculate_all();

	return TRUE;
}

static Workspace *
mainwindow_get_workspace(Mainwindow *main)
{
	if (!main->wsg) {
		Workspacegroup *wsg =
			workspacegroup_new_blank(main_workspaceroot, NULL);
		mainwindow_set_wsg(main, wsg);
	}

	return WORKSPACE(ICONTAINER(main->wsg)->current);
}

static gboolean
mainwindow_open_definition(Mainwindow *main, const char *filename)
{
	// turn it into eg. (Image_file "filename")
	char txt[MAX_STRSIZE];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	if (!workspace_load_file_buf(&buf, filename)) {
		mainwindow_error(main);
		return FALSE;
	}

	Workspace *ws = mainwindow_get_workspace(main);
	if (!workspace_add_def_recalc(ws, vips_buf_all(&buf))) {
		error_top(_("Load failed."));
		error_sub(_("unable to execute:\n   %s"), vips_buf_all(&buf));
		return FALSE;
	}

	symbol_recalculate_all();

	gtk_window_present(GTK_WINDOW(main));

	return TRUE;
}

typedef gboolean (*FileTypeHandler)(Mainwindow *main, const char *filename);

typedef struct _FileType {
	char *suffix;
	FileTypeHandler handler;
} FileType;

static FileType mainwindow_file_types[] = {
	{ ".ws", mainwindow_open_workspace },
	{ "", mainwindow_open_definition }
};

void
mainwindow_open(Mainwindow *main, GFile *file)
{
	g_autofree char *filename = g_file_get_path(file);

	mainwindow_set_load_folder(main, file);

	for (int i = 0; i < VIPS_NUMBER(mainwindow_file_types); i++)
		if (vips_iscasepostfix(filename, mainwindow_file_types[i].suffix)) {
			if (!mainwindow_file_types[i].handler(main, filename))
				mainwindow_error(main);
			return;
		}

	// the last item in mainwindow_file_types should catch everything
	g_assert_not_reached();
}

static void
mainwindow_open_result(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file)
		mainwindow_open(main, file);
}

static void
mainwindow_open_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Open workspace");
	gtk_file_dialog_set_modal(dialog, TRUE);

	// if we have a loaded file, use that folder, else
	GFile *load_folder = mainwindow_get_load_folder(main);
	if (load_folder)
		gtk_file_dialog_set_initial_folder(dialog, load_folder);

	gtk_file_dialog_open(dialog, GTK_WINDOW(main), NULL,
		mainwindow_open_result, main);
}

static void
mainwindow_merge_result(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		g_autofree char *filename = g_file_get_path(file);

		if (!workspacegroup_merge_workspaces(main->wsg, filename))
			mainwindow_error(main);

		symbol_recalculate_all();
	}
}

static void
mainwindow_merge_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Merge workspace");
	gtk_file_dialog_set_accept_label(dialog, "Merge");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GFile *load_folder = mainwindow_get_load_folder(main);
	if (load_folder)
		gtk_file_dialog_set_initial_folder(dialog, load_folder);

	gtk_file_dialog_open(dialog, GTK_WINDOW(main), NULL,
		mainwindow_merge_result, main);
}

static void
mainwindow_duplicate_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	Workspacegroup *new_wsg;

	if (!(new_wsg = workspacegroup_duplicate(main->wsg)))
		mainwindow_error(main);
	else {
		GtkApplication *app = gtk_window_get_application(GTK_WINDOW(main));
		Mainwindow *new_main = g_object_new(MAINWINDOW_TYPE,
			"application", app,
			NULL);

		mainwindow_set_wsg(new_main, new_wsg);
		gtk_window_present(GTK_WINDOW(new_main));
		symbol_recalculate_all();
	}
}

static void
mainwindow_saveas_sub(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		mainwindow_set_save_folder(main, file);

		g_autofree char *filename = g_file_get_path(file);

		if (workspacegroup_save_all(main->wsg, filename)) {
			filemodel_set_modified(FILEMODEL(main->wsg), FALSE);
			filemodel_set_filename(FILEMODEL(main->wsg), filename);
		}
		else
			mainwindow_error(main);
	}
}

static void
mainwindow_saveas(Mainwindow *main)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save workspace as");
	gtk_file_dialog_set_modal(dialog, TRUE);

	GFile *save_folder = mainwindow_get_save_folder(main);
	if (save_folder)
		gtk_file_dialog_set_initial_folder(dialog, save_folder);

	gtk_file_dialog_save(dialog, GTK_WINDOW(main), NULL,
		&mainwindow_saveas_sub, main);
}

static void
mainwindow_saveas_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	mainwindow_saveas(main);
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
	if (!(filename = FILEMODEL(main->wsg)->filename))
		// no filename, we need to go via save-as
		mainwindow_saveas(main);
	else {
		// we have a filename associated with this workspacegroup ... we can
		// just save directly
		if (workspacegroup_save_all(main->wsg, filename))
			filemodel_set_modified(FILEMODEL(main->wsg), FALSE);
		else
			mainwindow_error(main);
	}
}

static void
mainwindow_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	gtk_window_close(GTK_WINDOW(main));
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

static void
mainwindow_tab_new(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	if (!workspace_new_blank(main->wsg))
		mainwindow_error(main);
}

static void
mainwindow_tab_close_current(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);
	Workspace *ws = WORKSPACE(ICONTAINER(main->wsg)->current);

	if (ws &&
		!ws->locked)
		model_check_destroy(GTK_WINDOW(main), MODEL(ws));
}

// call ->action on the linked view
static void
mainwindow_view_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	printf("mainwindow_view_action: %s\n", g_action_get_name(G_ACTION(action)));

	Mainwindow *main = MAINWINDOW(user_data);

	if (main->action_view &&
		IS_VIEW(main->action_view)) {
		View *view = VIEW(main->action_view);
		ViewClass *view_class = VIEW_GET_CLASS(view);

		if (view_class->action)
			view_class->action(action, parameter, view);

		main->action_view = NULL;
	}
}

// ^D in the main window ... duplicate selected rows, or last row
static void
mainwindow_keyboard_duplicate_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	Workspace *ws;
	if ((ws = WORKSPACE(ICONTAINER(main->wsg)->current))) {
		if (!workspace_selected_duplicate(ws))
			mainwindow_error(main);

		workspace_deselect_all(ws);
		symbol_recalculate_all();
	}
}

static GActionEntry mainwindow_entries[] = {
	// main window actions
	{ "open", mainwindow_open_action },
	{ "merge", mainwindow_merge_action },
	{ "duplicate", mainwindow_duplicate_action },
	{ "save", mainwindow_save_action },
	{ "saveas", mainwindow_saveas_action },
	{ "close", mainwindow_close_action },
	{ "keyboard-duplicate", mainwindow_keyboard_duplicate_action },
	{ "fullscreen", action_toggle, NULL, "false", mainwindow_fullscreen },

	// workspace tab menu
	{ "tab-new", mainwindow_tab_new },
	{ "tab-close-current", mainwindow_tab_close_current },
	{ "tab-rename", mainwindow_view_action },
	{ "tab-select-all", mainwindow_view_action },
	{ "tab-duplicate", mainwindow_view_action },
	{ "tab-merge", mainwindow_view_action },
	{ "tab-saveas", mainwindow_view_action },
	{ "tab-lock", action_toggle, NULL, "false", mainwindow_view_action },
	{ "tab-delete", mainwindow_view_action },

	// workspaceview rightclick menu
	{ "column-new", mainwindow_view_action },
	{ "next-error", mainwindow_view_action },

	// column menu
	{ "column-edit-caption", mainwindow_view_action },
	{ "column-select-all", mainwindow_view_action },
	{ "column-duplicate", mainwindow_view_action },
	{ "column-merge", mainwindow_view_action },
	{ "column-saveas", mainwindow_view_action },
	{ "column-delete", mainwindow_view_action },

	// row menu
	{ "row-edit", mainwindow_view_action },
	{ "row-saveas", mainwindow_view_action },
	{ "row-group", mainwindow_view_action },
	{ "row-ungroup", mainwindow_view_action },
	{ "row-duplicate", mainwindow_view_action },
	{ "row-replace", mainwindow_view_action },
	{ "row-recalculate", mainwindow_view_action },
	{ "row-reset", mainwindow_view_action },
	{ "row-delete", mainwindow_view_action },

};

static void
mainwindow_progress_begin(Progress *progress, Mainwindow *main)
{
	printf("mainwindow_progress_begin:\n");

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(main->progress), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(main->progress), "");
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->progress_bar), TRUE);
}

static void
mainwindow_progress_update(Progress *progress,
	gboolean *cancel, Mainwindow *main)
{
	printf("mainwindow_progress_update: %d%% %s\n",
		progress->percent, vips_buf_all(&progress->feedback));

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(main->progress),
		progress->percent / 100.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(main->progress),
		vips_buf_all(&progress->feedback));

	if (main->cancel)
		*cancel = TRUE;
}

static void
mainwindow_progress_end(Progress *progress, Mainwindow *main)
{
	printf("mainwindow_progress_end:\n");

	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->progress_bar), FALSE);
}

static void
mainwindow_init(Mainwindow *main)
{
#ifdef DEBUG
	printf("mainwindow_init:\n");
#endif /*DEBUG*/

	g_autofree char *cwd = g_get_current_dir();
	main->save_folder = g_file_new_for_path(cwd);
	main->load_folder = g_file_new_for_path(cwd);

	gtk_widget_init_template(GTK_WIDGET(main));

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
	) */

	Progress *progress = progress_get();
	g_signal_connect_object(progress, "begin",
		G_CALLBACK(mainwindow_progress_begin), main, 0);
	g_signal_connect_object(progress, "update",
		G_CALLBACK(mainwindow_progress_update), main, 0);
	g_signal_connect_object(progress, "end",
		G_CALLBACK(mainwindow_progress_end), main, 0);

	mainwindow_all = g_slist_prepend(mainwindow_all, main);
}

static void
mainwindow_progress_cancel_clicked(GtkButton *button, void *user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	// picked up by eval, see below
	main->cancel = TRUE;
}

static void
mainwindow_error_close_clicked(GtkButton *button, void *user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	mainwindow_error_hide(main);
}

static gboolean
mainwindow_close_request_idle(void *user_data)
{
	Mainwindow *main = MAINWINDOW(user_data);

	gtk_window_close(GTK_WINDOW(main));

	return FALSE;
}

static void
mainwindow_close_request_next(GtkWindow *parent,
	Filemodel *filemodel, void *a, void *b)
{
	Mainwindow *main = MAINWINDOW(a);

	/* We can't go back to close immediately, the alert is still being shown
	 * and must detach. Close the window when we are back in idle.
	 */
	g_idle_add(mainwindow_close_request_idle, main);
}

static gboolean
mainwindow_close_request(GtkWindow *self, gpointer user_data)
{
	Mainwindow *main = MAINWINDOW(self);

	if (FILEMODEL(main->wsg)->modified) {
		filemodel_save_before_close(FILEMODEL(main->wsg),
			mainwindow_close_request_next, main, NULL);
		// block close, then retrigger after save, see
		// mainwindow_close_request_idle()
		return TRUE;
	}
	else
		return FALSE;
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
	BIND_VARIABLE(Mainwindow, error_bar);
	BIND_VARIABLE(Mainwindow, error_top);
	BIND_VARIABLE(Mainwindow, error_sub);
	BIND_VARIABLE(Mainwindow, wsgview);

	BIND_CALLBACK(mainwindow_progress_cancel_clicked);
	BIND_CALLBACK(mainwindow_error_close_clicked);
	BIND_CALLBACK(mainwindow_close_request);
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
	main->wsg = NULL;

	gtk_window_destroy(GTK_WINDOW(main));
}

static void
mainwindow_wsg_child_remove(iContainer parent, iContainer *child,
	Mainwindow *main)
{
	mainwindow_cull();
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
	g_signal_connect_object(main->wsg, "child_remove",
		G_CALLBACK(mainwindow_wsg_child_remove), main, 0);

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

	/* Make a start workspace and workspacegroup to load
	 * stuff into.
	 */
	Workspacegroup *wsg = workspacegroup_new_blank(main_workspaceroot, NULL);
	mainwindow_set_wsg(main, wsg);
	mainwindow_set_gfile(main, NULL);

	// we can't do this in _init() since we need app to be set
	mainwindow_init_settings(main);

	gboolean welcome;
	g_object_get(app, "welcome", &welcome, NULL);
	if (welcome) {
		char save_dir[FILENAME_MAX];
		char buf[256];

		g_snprintf(buf, 256, _("Welcome to %s-%s!"), PACKAGE, VERSION);
		g_strlcpy(save_dir, get_savedir(), FILENAME_MAX);
		path_expand(save_dir);
		error_top("%s", buf);
		error_sub(
			_("a new directory has been created to hold startup, "
			  "data and temporary files: %s\n"
			  "if you've used previous versions of %s, you might want "
			  "to copy files over from your old work area"),
			save_dir, PACKAGE);
		mainwindow_error(main);
	}

	return main;
}

static void *
mainwindow_cull_sub(Mainwindow *main)
{
	if (workspacegroup_is_empty(main->wsg)) {
		filemodel_set_modified(FILEMODEL(main->wsg), FALSE);
		gtk_window_close(GTK_WINDOW(main));
	}

	return NULL;
}

static guint mainwindow_cull_timeout_id = 0;

static gboolean
mainwindow_cull_timeout(gpointer user_data)
{
	mainwindow_cull_timeout_id = 0;

	slist_map(mainwindow_all,
		(SListMapFn) mainwindow_cull_sub, NULL);

	return FALSE;
}

void
mainwindow_cull(void)
{
	// do this on a timeout so we can cull from anywhere
	VIPS_FREEF(g_source_remove, mainwindow_cull_timeout_id);
	mainwindow_cull_timeout_id = g_timeout_add(100,
		(GSourceFunc) mainwindow_cull_timeout, NULL);
}

static void *
mainwindow_count_images(VipsObject *object, int *n)
{
	if (VIPS_IS_IMAGE(object))
		*n += 1;

	return NULL;
}

static void *
mainwindow_size_images(VipsObject *object, size_t *size)
{
	if (VIPS_IS_IMAGE(object))
		*size += VIPS_IMAGE_SIZEOF_IMAGE(VIPS_IMAGE(object));

	return NULL;
}

void
mainwindow_about(Mainwindow *main, VipsBuf *buf)
{
	double sz = find_space(PATH_TMP);

	if (sz < 0)
		vips_buf_appendf(buf, _("no temp area\n"));
	else {
		char txt[MAX_STRSIZE];
		VipsBuf buf2 = VIPS_BUF_STATIC(txt);

		vips_buf_append_size(&buf2, sz);
		vips_buf_appendf(buf, _("%s free"), vips_buf_all(&buf2));

		vips_buf_appendf(buf, _(" in \"%s\"\n"), PATH_TMP);
	}

	Heap *heap = reduce_context->heap;
	vips_buf_appendf(buf,
		_("%d cells in heap, %d cells free, %d cells maximum\n"),
		heap->ncells, heap->nfree, heap->max_fn(heap));

	if (main->wsg)
		vips_buf_appendf(buf, _("%d objects in workspace\n"),
			workspacegroup_get_n_objects(main->wsg));

	vips_buf_appendf(buf, _("using %d threads\n"), vips_concurrency_get());

	size_t size;
	int n;

	size = 0;
	n = 0;
	vips_object_map((VipsSListMap2Fn) mainwindow_size_images, &size, NULL);
	vips_object_map((VipsSListMap2Fn) mainwindow_count_images, &n, NULL);

	vips_buf_append_size(buf, size);
	vips_buf_appendf(buf, _(" in %d images"), n);
}

gboolean
mainwindow_is_empty(Mainwindow *main)
{
	// if the error bar is open, the user needs to be able to see it
	return workspacegroup_is_empty(main->wsg) &&
		!gtk_action_bar_get_revealed(GTK_ACTION_BAR(main->error_bar));
}

GtkWindow *
mainwindow_pick_one(void)
{
	return mainwindow_all ? GTK_WINDOW(mainwindow_all->data) : NULL;
}
