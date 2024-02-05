#include "nip4.h"

struct _Nip4App {
	GtkApplication parent;
};

G_DEFINE_TYPE(Nip4App, nip4_app, GTK_TYPE_APPLICATION);

static void
nip4_app_init(Nip4App *app)
{
}

static void
nip4_app_activate(GApplication *app)
{
	MainWindow *win;

	win = main_window_new(APP(app));
	gtk_window_present(GTK_WINDOW(win));
}

static void
nip4_app_quit_activated(GSimpleAction *action,
	GVariant *parameter, gpointer app)
{
	g_application_quit(G_APPLICATION(app));
}

static void
nip4_app_new_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	nip4_app_activate(G_APPLICATION(user_data));
}

static MainWindow *
nip4_app_win(Nip4App *app)
{
	GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));

	if (windows)
		return MAIN_WINDOW(windows->data);
	else
		return NULL;
}

static void
nip4_app_about_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Nip4App *app = APP(user_data);
	MainWindow *win = nip4_app_win(app);

	static const char *authors[] = {
		"jcupitt",
		NULL
	};

#ifdef DEBUG
	printf("nip4_app_about_activated:\n");
#endif /*DEBUG*/

	gtk_show_about_dialog(win ? GTK_WINDOW(win) : NULL,
		"program-name", PACKAGE,
		"logo-icon-name", APPLICATION_ID,
		"title", _("About nip4"),
		"authors", authors,
		"version", VERSION,
		"comments", _("An image processing spreadsheet"),
		"license-type", GTK_LICENSE_GPL_2_0,
		"website-label", "Visit nip4 on github",
		"website", "https://github.com/jcupitt/nip4",
		NULL);
}

static GActionEntry app_entries[] = {
	{ "quit", nip4_app_quit_activated },
	{ "new", nip4_app_new_activated },
	{ "about", nip4_app_about_activated },
};

static void
nip4_app_startup(GApplication *app)
{
	int i;
	GtkSettings *settings;

	struct {
		const gchar *action_and_target;
		const gchar *accelerators[2];
	} accels[] = {
		{ "app.quit", { "<Primary>q", NULL } },
		{ "app.new", { "<Primary>n", NULL } },

		{ "win.copy", { "<Primary>c", NULL } },
		{ "win.paste", { "<Primary>v", NULL } },
		{ "win.duplicate", { "<Primary>d", NULL } },
		{ "win.close", { "<Primary>w", NULL } },
		{ "win.replace", { "<Primary>o", NULL } },
		{ "win.saveas", { "<Primary>s", NULL } },
		{ "win.reload", { "F5", NULL } },
		{ "win.prev", { "<Primary>comma", NULL } },
		{ "win.next", { "<Primary>period", NULL } },
		{ "win.prev_image", { "<Alt>Left", NULL } },
		{ "win.next_image", { "<Alt>Right", NULL } },
		{ "win.fullscreen", { "F11", NULL } },
		{ "win.properties", { "<Alt>Return", NULL } },
	};

	G_APPLICATION_CLASS(nip4_app_parent_class)->startup(app);

	/* Image display programs are supposed to default to a dark theme,
	 * according to the HIG.
	 */
	settings = gtk_settings_get_default();
	g_object_set(settings,
		"gtk-application-prefer-dark-theme", TRUE,
		NULL);

	/* We have custom CSS for our dynamic widgets.
	 */
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(provider,
		APP_PATH "/mainwindow.css");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

	/* Build our classes.
	 */
	MAIN_WINDOW_TYPE;

	g_action_map_add_action_entries(G_ACTION_MAP(app),
		app_entries, G_N_ELEMENTS(app_entries),
		app);

	for (i = 0; i < G_N_ELEMENTS(accels); i++)
		gtk_application_set_accels_for_action(GTK_APPLICATION(app),
			accels[i].action_and_target, accels[i].accelerators);
}

static void
nip4_app_open(GApplication *app,
	GFile **files, int n_files, const char *hint)
{
	for(int i = 0; i < n_files; i++) {
		MainWindow *win = main_window_new(APP(app));

		main_window_set_gfile(win, files[i]);
		gtk_window_present(GTK_WINDOW(win));
	}
}

static void
nip4_app_shutdown(GApplication *app)
{
	MainWindow *win;

#ifdef DEBUG
	printf("nip4_app_shutdown:\n");
#endif /*DEBUG*/

	/* Force down all our windows ... this will not happen automatically
	 * on _quit().
	 */
	while ((win = nip4_app_win(APP(app))))
		gtk_window_destroy(GTK_WINDOW(win));

	G_APPLICATION_CLASS(nip4_app_parent_class)->shutdown(app);
}

static void
nip4_app_class_init(Nip4AppClass *class)
{
	G_APPLICATION_CLASS(class)->startup = nip4_app_startup;
	G_APPLICATION_CLASS(class)->activate = nip4_app_activate;
	G_APPLICATION_CLASS(class)->open = nip4_app_open;
	G_APPLICATION_CLASS(class)->shutdown = nip4_app_shutdown;
}

Nip4App *
nip4_app_new(void)
{
	return g_object_new(APP_TYPE,
		"application-id", APPLICATION_ID,
		"flags", G_APPLICATION_HANDLES_OPEN,
		"inactivity-timeout", 3000,
		"register-session", TRUE,
		NULL);
}
