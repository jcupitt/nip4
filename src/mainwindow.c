/*
 */
#define DEBUG

#include "nip4.h"

/* How much to scale view by each frame.
 */
#define SCALE_STEP (1.05)

/* Duration of discrete zoom in secs.
 */
#define SCALE_DURATION (0.5)

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
	GtkWidget *status_bar;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;

	GSettings *settings;

	GtkCssProvider *css_provider;
};

G_DEFINE_TYPE(MainWindow, main_window, GTK_TYPE_APPLICATION_WINDOW);

static void
main_window_set_error(MainWindow *win, const char *message)
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
	gtk_label_set_text(GTK_LABEL(win->error_label), err);
	g_free(err);

	gtk_info_bar_set_revealed(GTK_INFO_BAR(win->error_bar), TRUE);
}

static void
main_window_error(MainWindow *win)
{
	main_window_set_error(win, vips_error_buffer());
	vips_error_clear();
}

static void
main_window_gerror(MainWindow *win, GError **error)
{
	if (error && *error) {
		main_window_set_error(win, (*error)->message);
		g_error_free(*error);
	}
}

static void
main_window_error_hide(MainWindow *win)
{
#ifdef DEBUG
	printf("main_window_error_hide:\n");
#endif /*DEBUG*/

	gtk_info_bar_set_revealed(GTK_INFO_BAR(win->error_bar), FALSE);
}

static void
main_window_dispose(GObject *object)
{
	MainWindow *win = MAIN_WINDOW(object);

#ifdef DEBUG
	printf("main_window_dispose:\n");
#endif /*DEBUG*/

	gtk_style_context_remove_provider_for_display(
		gdk_display_get_default(),
		GTK_STYLE_PROVIDER(win->css_provider));

	//VIPS_FREEF(gtk_widget_unparent, win->right_click_menu);
	VIPS_FREEF(g_timer_destroy, win->progress_timer);

	G_OBJECT_CLASS(main_window_parent_class)->dispose(object);
}

static void
main_window_preeval(VipsImage *image,
	VipsProgress *progress, MainWindow *win)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(win->progress_bar),
		TRUE);
}

typedef struct _EvalUpdate {
	MainWindow *win;
	int eta;
	int percent;
} EvalUpdate;

static gboolean
main_window_eval_idle(void *user_data)
{
	EvalUpdate *update = (EvalUpdate *) user_data;
	MainWindow *win = update->win;

	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC(str);

	vips_buf_appendf(&buf, "%d%% complete, %d seconds to go",
		update->percent, update->eta);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(win->progress),
		vips_buf_all(&buf));

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(win->progress),
		update->percent / 100.0);

	g_object_unref(win);

	g_free(update);

	return FALSE;
}

static void
main_window_eval(VipsImage *image,
	VipsProgress *progress, MainWindow *win)
{
	double time_now;
	EvalUpdate *update;

	/* We can be ^Q'd during load. This is NULLed in _dispose.
	 */
	if (!win->progress_timer)
		return;

	time_now = g_timer_elapsed(win->progress_timer, NULL);

	/* Throttle to 10Hz.
	 */
	if (time_now - win->last_progress_time < 0.1)
		return;
	win->last_progress_time = time_now;

#ifdef DEBUG_VERBOSE
	printf("main_window_eval: %d%%\n", progress->percent);
#endif /*DEBUG_VERBOSE*/

	/* This can come from the background load thread, so we can't update
	 * the UI directly.
	 */

	update = g_new(EvalUpdate, 1);

	update->win = win;
	update->percent = progress->percent;
	update->eta = progress->eta;

	/* We don't want win to vanish before we process this update. The
	 * matching unref is in the handler above.
	 */
	g_object_ref(win);

	g_idle_add(main_window_eval_idle, update);
}

static void
main_window_posteval(VipsImage *image,
	VipsProgress *progress, MainWindow *win)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(win->progress_bar),
		FALSE);
}

static void
main_window_cancel_clicked(GtkWidget *button, MainWindow *win)
{
	VipsImage *image;

	// cancel
}

static void
main_window_error_response(GtkWidget *button, int response,
	MainWindow *win)
{
	main_window_error_hide(win);
}

static GFile *
get_parent(GFile *file)
{
	GFile *parent = g_file_get_parent(file);

	return parent ? parent : g_file_new_for_path("/");
}

static void
main_window_replace_result(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	MainWindow *win = MAIN_WINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	GFile *file;

	file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		// load!

		VIPS_UNREF(file);
	}
}

static void
main_window_replace_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	MainWindow *win = MAIN_WINDOW(user_data);

	GtkFileDialog *dialog;
	GFile *file;

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Replace from file");
	gtk_file_dialog_set_accept_label(dialog, "Replace");
	gtk_file_dialog_set_modal(dialog, TRUE);

	// if we have a loaded file, use that folder, else
	if (win->load_folder)
		gtk_file_dialog_set_initial_folder(dialog, win->load_folder);

	gtk_file_dialog_open(dialog, GTK_WINDOW(win), NULL,
		main_window_replace_result, win);
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
	MainWindow *win = MAIN_WINDOW(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
	GFile *file;

	file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		char *filename;

		// note the save directory for next time
		VIPS_UNREF(win->save_folder);
		win->save_folder = get_parent(file);

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
	MainWindow *win = MAIN_WINDOW(user_data);

	GtkFileDialog *dialog;
	GFile *file;

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Save file");
	gtk_file_dialog_set_modal(dialog, TRUE);

	// if we loaded from a file, use that dir, else
	if (win->save_folder)
		gtk_file_dialog_set_initial_folder(dialog, win->save_folder);

	gtk_file_dialog_save(dialog, GTK_WINDOW(win), NULL,
		&main_window_on_file_save_cb, win);
}

static void
main_window_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	MainWindow *win = MAIN_WINDOW(user_data);

	gtk_window_destroy(GTK_WINDOW(win));
}

static void
main_window_fullscreen(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	MainWindow *win = MAIN_WINDOW(user_data);

	g_object_set(win,
		"fullscreened", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

static void
main_window_radio(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	g_action_change_state(G_ACTION(action), parameter);
}

static void
main_window_toggle(GSimpleAction *action,
    GVariant *parameter, gpointer user_data)
{
    GVariant *state;

    state = g_action_get_state(G_ACTION(action));
    g_action_change_state(G_ACTION(action),
        g_variant_new_boolean(!g_variant_get_boolean(state)));
    g_variant_unref(state);
}

static GActionEntry main_window_entries[] = {
	{ "replace", main_window_replace_action },
	{ "saveas", main_window_saveas_action },
	{ "close", main_window_close_action },

	{ "fullscreen", action_toggle, NULL, "false", main_window_fullscreen },
};

static void
main_window_init(MainWindow *win)
{
	GtkEventController *controller;

#ifdef DEBUG
	puts("main_window_init");
#endif /*DEBUG*/

	win->progress_timer = g_timer_new();
	win->last_progress_time = -1;
	win->settings = g_settings_new(APPLICATION_ID);
	char *cwd = g_get_current_dir();
	win->save_folder = g_file_new_for_path(cwd);
	win->load_folder = g_file_new_for_path(cwd);
	g_free(cwd);

	gtk_widget_init_template(GTK_WIDGET(win));

	win->css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(win->css_provider,
		APP_PATH "/mainwindow.css");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(win->css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_signal_connect_object(win->progress_cancel, "clicked",
		G_CALLBACK(main_window_cancel_clicked), win, 0);

	g_signal_connect_object(win->error_bar, "response",
		G_CALLBACK(main_window_error_response), win, 0);

	g_action_map_add_action_entries(G_ACTION_MAP(win),
		main_window_entries, G_N_ELEMENTS(main_window_entries),
		win);

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
		G_CALLBACK(main_window_dnd_drop), win);
	gtk_widget_add_controller(win->imagedisplay, controller);
	 */
}

static void
main_window_pressed_cb(GtkGestureClick *gesture,
	guint n_press, double x, double y, MainWindow *win)
{
	gtk_popover_set_pointing_to(GTK_POPOVER(win->right_click_menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	/* This produces a lot of warnings :( not sure why. I tried calling
	 * gtk_popover_present() in realize to force allocation, but it didn't
	 * help.
	 */
	gtk_popover_popup(GTK_POPOVER(win->right_click_menu));
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
	BIND(status_bar);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		main_window_pressed_cb);
}

void
main_window_set_gfile(MainWindow *win, GFile *gfile)
{
#ifdef DEBUG
	printf("main_window_set_gfile:\n");
#endif /*DEBUG*/

	gtk_label_set_text(GTK_LABEL(win->title), "Untitled");
	if (gfile) {
		char *file = g_file_get_path(gfile);
		gtk_label_set_text(GTK_LABEL(win->subtitle), file);
		g_free(file);
	}
}

MainWindow *
main_window_new(Nip4App *app)
{
	MainWindow *win = g_object_new(MAIN_WINDOW_TYPE, "application", app, NULL);

	main_window_set_gfile(win, NULL);

	return win;
}

static void
copy_value(GObject *to, GObject *from, const char *name)
{
	GValue value = { 0 };

	g_object_get_property(from, name, &value);
	g_object_set_property(to, name, &value);
	g_value_unset(&value);
}

GSettings *
main_window_get_settings(MainWindow *win)
{
	return win->settings;
}
