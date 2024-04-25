// app startup and lifetime management

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

struct _App {
	GtkApplication parent;

	// all application settings go in here
	GSettings *settings;
};

G_DEFINE_TYPE(App, app, GTK_TYPE_APPLICATION);

static void
app_init(App *app)
{
}

static void
app_activate(GApplication *gapp)
{
	MainWindow *main;

	printf("app_activate:\n");

	main = main_window_new(APP(gapp));

	/* Make a start workspace and workspacegroup.
	 * stuff into.
	 */
	Workspacegroup *wsg = workspacegroup_new_blank(main_workspaceroot, NULL);
	Workspace *ws = WORKSPACE(icontainer_get_nth_child(ICONTAINER(wsg), 0));
	main_window_set_wsg(main, wsg);

	gtk_window_present(GTK_WINDOW(main));
}

static void
app_quit_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	g_application_quit(G_APPLICATION(user_data));
}

static void
app_new_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	app_activate(G_APPLICATION(user_data));
}

static MainWindow *
app_win(App *app)
{
	GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));

	if (windows)
		return MAIN_WINDOW(windows->data);
	else
		return NULL;
}

static void
app_about_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	App *app = APP(user_data);
	MainWindow *win = app_win(app);

	static const char *authors[] = {
		"jcupitt",
		NULL
	};

#ifdef DEBUG
	printf("app_about_activated:\n");
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
	{ "quit", app_quit_activated },
	{ "new", app_new_activated },
	{ "about", app_about_activated },
};

static void
app_startup(GApplication *app)
{
	struct {
		const gchar *action_and_target;
		const gchar *accelerators[2];
	} accels[] = {
		// application wide accels
		{ "app.quit", { "<Primary>q", NULL } },

		// main window accels ... the "win." prefix is wired into gtk
		{ "win.new-column", { "<Primary>n", NULL } },
		{ "win.open", { "<Primary>o", NULL } },
		{ "win.copy", { "<Primary>c", NULL } },
		{ "win.paste", { "<Primary>v", NULL } },
		{ "win.duplicate", { "<Primary>d", NULL } },
		{ "win.close", { "<Primary>w", NULL } },
		{ "win.replace", { "<Primary>o", NULL } },
		{ "win.save", { "<Primary>s", NULL } },
		{ "win.reload", { "F5", NULL } },
		{ "win.prev", { "<Primary>comma", NULL } },
		{ "win.next", { "<Primary>period", NULL } },
		{ "win.prev_image", { "<Alt>Left", NULL } },
		{ "win.next_image", { "<Alt>Right", NULL } },
		{ "win.fullscreen", { "F11", NULL } },
		{ "win.properties", { "<Alt>Return", NULL } },
	};

	// all our private application settings
	APP(app)->settings = g_settings_new(APPLICATION_ID);

	G_APPLICATION_CLASS(app_parent_class)->startup(app);

	/* Image display programs are supposed to default to a dark theme,
	 * according to the HIG.
	 */
	GtkSettings *settings = gtk_settings_get_default();
	g_object_set(settings,
		"gtk-application-prefer-dark-theme", TRUE,
		NULL);

	/* Build our GUI widgets.
	 */
	MAIN_WINDOW_TYPE;
	VIEW_TYPE;
	WORKSPACEGROUPVIEW_TYPE;
	WORKSPACEVIEW_TYPE;
	WORKSPACEVIEWLABEL_TYPE;
	COLUMNVIEW_TYPE;
	SUBCOLUMNVIEW_TYPE;
	SPIN_TYPE;
	ROWVIEW_TYPE;
	RHSVIEW_TYPE;
	FORMULA_TYPE;
	ITEXTVIEW_TYPE;

	/* We have to init some of our other classes to get them registered
	 * with the XML loader.
	 */
	printf("app_startup: FIXME ... register more types\n");
	/*
	(void) g_type_class_ref(TYPE_CLOCK);
	(void) g_type_class_ref(TYPE_COLOUR);
	(void) g_type_class_ref(TYPE_EXPRESSION);
	(void) g_type_class_ref(TYPE_FONTNAME);
	(void) g_type_class_ref(TYPE_GROUP);
	(void) g_type_class_ref(TYPE_IARROW);
	(void) g_type_class_ref(TYPE_IREGION);
	(void) g_type_class_ref(TYPE_MATRIX);
	(void) g_type_class_ref(TYPE_NUMBER);
	(void) g_type_class_ref(TYPE_OPTION);
	(void) g_type_class_ref(TYPE_PATHNAME);
	(void) g_type_class_ref(TYPE_PLOT);
	(void) g_type_class_ref(TYPE_REAL);
	(void) g_type_class_ref(TYPE_SLIDER);
	(void) g_type_class_ref(TYPE_STRING);
	(void) g_type_class_ref(TYPE_TOGGLE);
	(void) g_type_class_ref(TYPE_VECTOR);
	 */

	(void) g_type_class_ref(IMAGEDISPLAY_TYPE);
	(void) g_type_class_ref(IIMAGE_TYPE);
	(void) g_type_class_ref(WORKSPACE_TYPE);
	(void) g_type_class_ref(COLUMN_TYPE);
	(void) g_type_class_ref(SUBCOLUMN_TYPE);
	(void) g_type_class_ref(ROW_TYPE);
	(void) g_type_class_ref(RHS_TYPE);
	(void) g_type_class_ref(ITEXT_TYPE);

	/* We have custom CSS for our dynamic widgets.
	 */
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(provider, APP_PATH "/mainwindow.css");
	gtk_css_provider_load_from_resource(provider, APP_PATH "/columnview.css");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

	g_action_map_add_action_entries(G_ACTION_MAP(app),
		app_entries, G_N_ELEMENTS(app_entries), app);

	for (int i = 0; i < G_N_ELEMENTS(accels); i++)
		gtk_application_set_accels_for_action(GTK_APPLICATION(app),
			accels[i].action_and_target, accels[i].accelerators);
}

static void
app_open(GApplication *app, GFile **files, int n_files, const char *hint)
{
	for (int i = 0; i < n_files; i++) {
		MainWindow *win = main_window_new(APP(app));
		main_window_set_gfile(win, files[i]);
		gtk_window_present(GTK_WINDOW(win));
	}
}

static void
app_shutdown(GApplication *app)
{
	MainWindow *win;

#ifdef DEBUG
	printf("app_shutdown:\n");
#endif /*DEBUG*/

	/* Force down all our windows ... this will not happen automatically
	 * on _quit().
	 */
	while ((win = app_win(APP(app))))
		gtk_window_destroy(GTK_WINDOW(win));

	G_APPLICATION_CLASS(app_parent_class)->shutdown(app);
}

static void
app_class_init(AppClass *class)
{
	G_APPLICATION_CLASS(class)->startup = app_startup;
	G_APPLICATION_CLASS(class)->activate = app_activate;
	G_APPLICATION_CLASS(class)->open = app_open;
	G_APPLICATION_CLASS(class)->shutdown = app_shutdown;
}

App *
app_new(void)
{
	return g_object_new(APP_TYPE,
		"application-id", APPLICATION_ID,
		"flags", G_APPLICATION_HANDLES_OPEN,
		"inactivity-timeout", 3000,
		"register-session", TRUE,
		NULL);
}

GSettings *
app_settings(App *app)
{
	return app->settings;
}
