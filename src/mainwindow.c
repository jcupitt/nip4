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
 */
#define DEBUG

#include "nip4.h"

struct _MainWindow {
	GtkApplicationWindow parent;

	/* The current save and load directories.
	 */
	GFile *save_folder;
	GFile *load_folder;

	GtkWidget *right_click_menu;
	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *progress_bar;
	GtkWidget *progress;
	GtkWidget *progress_cancel;
	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *scrolled_window;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;
};

G_DEFINE_TYPE(MainWindow, main_window, GTK_TYPE_APPLICATION_WINDOW);

static void
main_window_set_error(MainWindow *main, const char *message)
{
	char *err;
	int i;

#ifdef DEBUG
	printf("main_window_set_error: %s\n", message);
#endif /*DEBUG*/

	/* Remove any trailing \n.
	 */
	err = g_strdup(message);
	for (i = strlen(err); i > 0 && err[i - 1] == '\n'; i--)
		err[i - 1] = '\0';
	gtk_label_set_text(GTK_LABEL(main->error_label), err);
	g_free(err);

	gtk_info_bar_set_revealed(GTK_INFO_BAR(main->error_bar), TRUE);
}

static void
main_window_error(MainWindow *main)
{
	main_window_set_error(main, vips_error_buffer());
	vips_error_clear();
}

static void
main_window_gerror(MainWindow *main, GError **error)
{
	if (error && *error) {
		main_window_set_error(main, (*error)->message);
		g_error_free(*error);
	}
}

static void
main_window_error_hide(MainWindow *main)
{
#ifdef DEBUG
	printf("main_window_error_hide:\n");
#endif /*DEBUG*/

	gtk_info_bar_set_revealed(GTK_INFO_BAR(main->error_bar), FALSE);
}

static void
main_window_dispose(GObject *object)
{
	MainWindow *main = MAIN_WINDOW(object);

#ifdef DEBUG
	printf("main_window_dispose:\n");
#endif /*DEBUG*/

	//VIPS_FREEF(gtk_widget_unparent, main->right_click_menu);
	VIPS_FREEF(g_timer_destroy, main->progress_timer);

	G_OBJECT_CLASS(main_window_parent_class)->dispose(object);
}

static void
main_window_preeval(VipsImage *image,
	VipsProgress *progress, MainWindow *main)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->progress_bar),
		TRUE);
}

typedef struct _EvalUpdate {
	MainWindow *main;
	int eta;
	int percent;
} EvalUpdate;

static gboolean
main_window_eval_idle(void *user_data)
{
	EvalUpdate *update = (EvalUpdate *) user_data;
	MainWindow *main = update->main;

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
main_window_eval(VipsImage *image,
	VipsProgress *progress, MainWindow *main)
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
	printf("main_window_eval: %d%%\n", progress->percent);
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

	g_idle_add(main_window_eval_idle, update);
}

static void
main_window_posteval(VipsImage *image,
	VipsProgress *progress, MainWindow *main)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(main->progress_bar),
		FALSE);
}

static void
main_window_cancel_clicked(GtkWidget *button, MainWindow *main)
{
	VipsImage *image;

	// cancel
}

static void
main_window_error_response(GtkWidget *button, int response,
	MainWindow *main)
{
	main_window_error_hide(main);
}

static GFile *
get_parent(GFile *file)
{
	GFile *parent = g_file_get_parent(file);

	return parent ? parent : g_file_new_for_path("/");
}

static void
main_window_open_result(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	MainWindow *main = MAIN_WINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	GFile *file;

	file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		// load!

		VIPS_UNREF(file);
	}
}

static void
main_window_open_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	MainWindow *main = MAIN_WINDOW(user_data);

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
		main_window_open_result, main);
}

static void
main_window_saveas_options_response(GtkDialog *dialog,
	gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT ||
		response == GTK_RESPONSE_CANCEL)
		gtk_window_destroy(GTK_WINDOW(dialog));

	// other return codes are intermediate stages of processing and we
	// should do nothing
}

static void
main_window_on_file_save_cb(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	MainWindow *main = MAIN_WINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
	GFile *file;

	file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		char *filename;

		// note the save directory for next time
		VIPS_UNREF(main->save_folder);
		main->save_folder = get_parent(file);

		filename = g_file_get_path(file);
		g_object_unref(file);

		// save it!

		g_free(filename);
	}
}

static void
main_window_saveas_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	MainWindow *main = MAIN_WINDOW(user_data);

	GtkFileDialog *dialog;
	GFile *file;

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save file");
	gtk_file_dialog_set_modal(dialog, TRUE);

	// if we loaded from a file, use that dir, else
	if (main->save_folder)
		gtk_file_dialog_set_initial_folder(dialog, main->save_folder);

	gtk_file_dialog_save(dialog, GTK_WINDOW(main), NULL,
		&main_window_on_file_save_cb, main);
}

static void
main_window_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	MainWindow *main = MAIN_WINDOW(user_data);

	gtk_window_destroy(GTK_WINDOW(main));
}

static void
main_window_fullscreen(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	MainWindow *main = MAIN_WINDOW(user_data);

	g_object_set(main,
		"fullscreened", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

static GActionEntry main_window_entries[] = {
	{ "open", main_window_open_action },
	{ "saveas", main_window_saveas_action },
	{ "close", main_window_close_action },

	{ "fullscreen", action_toggle, NULL, "false", main_window_fullscreen },
};

static void
main_window_init(MainWindow *main)
{
	GtkEventController *controller;

#ifdef DEBUG
	puts("main_window_init");
#endif /*DEBUG*/

	main->progress_timer = g_timer_new();
	main->last_progress_time = -1;
	char *cwd = g_get_current_dir();
	main->save_folder = g_file_new_for_path(cwd);
	main->load_folder = g_file_new_for_path(cwd);
	g_free(cwd);

	gtk_widget_init_template(GTK_WIDGET(main));

	g_signal_connect_object(main->progress_cancel, "clicked",
		G_CALLBACK(main_window_cancel_clicked), main, 0);

	g_signal_connect_object(main->error_bar, "response",
		G_CALLBACK(main_window_error_response), main, 0);

	g_action_map_add_action_entries(G_ACTION_MAP(main),
		main_window_entries, G_N_ELEMENTS(main_window_entries),
		main);

	/* We are a drop target for filenames and images.
	 *
	 * FIXME ... implement this
	 *
	controller = GTK_EVENT_CONTROLLER(
		gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY));
	gtk_drop_target_set_gtypes(GTK_DROP_TARGET(controller),
		main_window_supported_types,
		main_window_n_supported_types);
	g_signal_connect(controller, "drop",
		G_CALLBACK(main_window_dnd_drop), main);
	gtk_widget_add_controller(main->imagedisplay, controller);
	 */
}

static void
main_window_pressed_cb(GtkGestureClick *gesture,
	guint n_press, double x, double y, MainWindow *main)
{
	gtk_popover_set_pointing_to(GTK_POPOVER(main->right_click_menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	/* This produces a lot of warnings :( not sure why. I tried calling
	 * gtk_popover_present() in realize to force allocation, but it didn't
	 * help.
	 */
	gtk_popover_popup(GTK_POPOVER(main->right_click_menu));
}

#define BIND(field) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		MainWindow, field);

static void
main_window_class_init(MainWindowClass *class)
{
	G_OBJECT_CLASS(class)->dispose = main_window_dispose;

	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
		APP_PATH "/mainwindow.ui");

	BIND(right_click_menu);
	BIND(title);
	BIND(subtitle);
	BIND(gears);
	BIND(progress_bar);
	BIND(progress);
	BIND(progress_cancel);
	BIND(error_bar);
	BIND(error_label);
	BIND(scrolled_window);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		main_window_pressed_cb);
}

void
main_window_set_gfile(MainWindow *main, GFile *gfile)
{
#ifdef DEBUG
	printf("main_window_set_gfile:\n");
#endif /*DEBUG*/

	gtk_label_set_text(GTK_LABEL(main->title), "Untitled");
	if (gfile) {
		char *file = g_file_get_path(gfile);
		gtk_label_set_text(GTK_LABEL(main->subtitle), file);
		g_free(file);
	}

	vips_error("MainWindow", "Workspace load not implemented.");
	main_window_error(main);
}

static GSettings *
main_window_settings(MainWindow *main)
{
	App *app;

	// FIXME ... or have an "app" member in main?
	g_object_get(main, "application", &app, NULL );

	return app ? app_settings(app) : NULL;
}

static void
main_window_init_settings(MainWindow *main)
{
	GSettings *settings = main_window_settings(main);

	if (settings) {
		// FIXME ... set window state from settings
	}
}

MainWindow *
main_window_new(App *app)
{
	MainWindow *main = g_object_new(MAIN_WINDOW_TYPE, "application", app, NULL);

	main_window_set_gfile(main, NULL);

	// we can't do this until the window has been linked to the app where the
	// settings live
	main_window_init_settings(main);

	return main;
}

static void
copy_value(GObject *to, GObject *from, const char *name)
{
	GValue value = { 0 };

	g_object_get_property(from, name, &value);
	g_object_set_property(to, name, &value);
	g_value_unset(&value);
}
