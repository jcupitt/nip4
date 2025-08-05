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

/*
#define DEBUG
 */

#include "nip4.h"

struct _App {
	GtkApplication parent;

	gboolean welcome;

	// all application settings go in here
	GSettings *settings;
};

G_DEFINE_TYPE(App, app, GTK_TYPE_APPLICATION);

enum {
	PROP_WELCOME = 1,
};

static void
app_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	App *app = (App *) object;

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("app_set_property: %d %s\n", prop_id, str);
	}
#endif /*DEBUG*/

	switch (prop_id) {
	case PROP_WELCOME:
		app->welcome = g_value_get_boolean(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
app_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	App *app = (App *) object;

	switch (prop_id) {
	case PROP_WELCOME:
		g_value_set_boolean(value, app->welcome);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("app_get_property: %d %s\n", prop_id, str);
	}
#endif /*DEBUG*/
}

static void
app_init(App *app)
{
}

static void
app_activate(GApplication *gapp)
{
	Mainwindow *main;

	main = mainwindow_new(APP(gapp), NULL);
	gtk_window_present(GTK_WINDOW(main));
}

static void *
app_quit_activated_next(void *a, void *b)
{
	GApplication *gapp = G_APPLICATION(a);

	g_application_quit(gapp);

	return NULL;
}

static void
app_quit_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	// do save-before-quit, then quit
	filemodel_close_registered(app_quit_activated_next, user_data);
}

static void
app_new_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	app_activate(G_APPLICATION(user_data));
}

static Mainwindow *
app_main(App *app)
{
	GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));

	// can have non-mainwindow windows
	for (GList *p = windows; p; p = p->next)
		if (IS_MAINWINDOW(p->data))
			return MAINWINDOW(p->data);

	return NULL;
}

static void
app_about_activated(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	App *app = APP(user_data);
	Mainwindow *main = app_main(app);

	char txt[MAX_STRSIZE];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	vips_buf_appendf(&buf, "%s", _("An image processing spreadsheet\n\n"));
	mainwindow_about(main, &buf);

	static const char *authors[] = {
		"jcupitt",
		"kleisauke",
		"jpadfield",
		"angstyloop",
		"TingPing",
		"earboxer",
		NULL
	};

#ifdef DEBUG
	printf("app_about_activated:\n");
#endif /*DEBUG*/

	gtk_show_about_dialog(main ? GTK_WINDOW(main) : NULL,
		"program-name", PACKAGE,
		"logo-icon-name", APPLICATION_ID,
		"title", _("About nip4"),
		"authors", authors,
		"version", VERSION,
		"comments", vips_buf_all(&buf),
		"license-type", GTK_LICENSE_GPL_2_0,
		"website-label", "Visit nip4 on github",
		"website", "https://github.com/jcupitt/nip4",
		NULL);
}

static void
add_css(const char *filename)
{
	g_autoptr(GtkCssProvider) provider = gtk_css_provider_new();

	gtk_css_provider_load_from_resource(provider, filename);
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static GActionEntry app_entries[] = {
	{ "quit", app_quit_activated },
	{ "new", app_new_activated },
	{ "about", app_about_activated },
};

static void
app_startup(GApplication *app)
{
#ifdef DEBUG
	printf("app_startup:\n");
#endif /*DEBUG*/

	struct {
		const gchar *action_and_target;
		const gchar *accelerators[2];
	} accels[] = {
		// main window accels ... the "win." prefix is wired into gtk
		{ "win.cancel", { "Escape", NULL } },
		{ "win.ok", { "Return", NULL } },
		{ "win.close", { "<Primary>w", NULL } },
		{ "win.quit", { "<Primary>q", NULL } },
		{ "win.open", { "<Primary>o", NULL } },
		{ "win.copy", { "<Primary>c", NULL } },
		{ "win.paste", { "<Primary>v", NULL } },
		{ "win.tab-new", { "<Primary>t", NULL } },
		{ "win.tab-close-current", { "<Primary>w", NULL } },
		{ "win.column-new-current", { "<Primary>n", NULL } },
		{ "win.replace", { "<Primary>o", NULL } },
		{ "win.save", { "<Primary>s", NULL } },
		{ "win.reload", { "F5", NULL } },
		{ "win.prev", { "<Primary>comma", NULL } },
		{ "win.next", { "<Primary>period", NULL } },
		{ "win.prev_image", { "<Alt>Left", NULL } },
		{ "win.next_image", { "<Alt>Right", NULL } },
		{ "win.fullscreen", { "F11", NULL } },
		{ "win.properties", { "<Alt>Return", NULL } },
		{ "win.keyboard-duplicate", { "<Primary>d", NULL } },
		{ "win.keyboard-group-selected", { "<Primary>g", NULL } },
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
	MAINWINDOW_TYPE;
	VIEW_TYPE;
	WORKSPACEGROUPVIEW_TYPE;
	WORKSPACEVIEW_TYPE;
	WORKSPACEVIEWLABEL_TYPE;
	WORKSPACEDEFS_TYPE;
	TOOLKITGROUPVIEW_TYPE;
	COLUMNVIEW_TYPE;
	SUBCOLUMNVIEW_TYPE;
	SPIN_TYPE;
	ROWVIEW_TYPE;
	RHSVIEW_TYPE;
	FORMULA_TYPE;
	ITEXTVIEW_TYPE;
	PROPERTIES_TYPE;
	DISPLAYBAR_TYPE;
	INFOBAR_TYPE;
	TSLIDER_TYPE;

	/* Some custom CSS.
	 */
	add_css(APP_PATH "/nip4.css");
	add_css(APP_PATH "/saveoptions.css");
	add_css(APP_PATH "/properties.css");
	add_css(APP_PATH "/iimageview.css");

	g_action_map_add_action_entries(G_ACTION_MAP(app),
		app_entries, G_N_ELEMENTS(app_entries), app);

	for (int i = 0; i < G_N_ELEMENTS(accels); i++)
		gtk_application_set_accels_for_action(GTK_APPLICATION(app),
			accels[i].action_and_target, accels[i].accelerators);
}

static void
app_open(GApplication *app, GFile **files, int n_files, const char *hint)
{
#ifdef DEBUG
	printf("app_open:\n");
#endif /*DEBUG*/

	Mainwindow *main = mainwindow_new(APP(app), NULL);

	gtk_window_present(GTK_WINDOW(main));

	for (int i = 0; i < n_files; i++)
		mainwindow_open(main, files[i]);

	if (n_files > 0 &&
		mainwindow_is_empty(main))
		gtk_window_destroy(GTK_WINDOW(main));
}

static void
app_shutdown(GApplication *app)
{
	Mainwindow *main;

#ifdef DEBUG
	printf("app_shutdown:\n");
#endif /*DEBUG*/

	/* Force down all our windows ... this will not happen automatically
	 * on _quit().
	 */
	while ((main = app_main(APP(app))))
		gtk_window_destroy(GTK_WINDOW(main));

	G_APPLICATION_CLASS(app_parent_class)->shutdown(app);
}

static void
app_class_init(AppClass *class)
{
	G_OBJECT_CLASS(class)->set_property = app_set_property;
	G_OBJECT_CLASS(class)->get_property = app_get_property;

	G_APPLICATION_CLASS(class)->startup = app_startup;
	G_APPLICATION_CLASS(class)->activate = app_activate;
	G_APPLICATION_CLASS(class)->open = app_open;
	G_APPLICATION_CLASS(class)->shutdown = app_shutdown;

	g_object_class_install_property(G_OBJECT_CLASS(class), PROP_WELCOME,
		g_param_spec_boolean("welcome",
			_("welcome"),
			_("Welcome"),
			FALSE,
			G_PARAM_READWRITE));
}

App *
app_new(gboolean welcome)
{
#ifdef DEBUG
	printf("app_new:\n");
#endif /*DEBUG*/

	return g_object_new(APP_TYPE,
		"welcome", welcome,
		"flags", G_APPLICATION_HANDLES_OPEN,
		// we usually want each invocation to be a separate instance so they
		// can't take each opther down
		// "register-session", TRUE,
		// "inactivity-timeout", 3000,
		// "application-id", APPLICATION_ID,
		NULL);
}

GSettings *
app_settings(App *app)
{
	return app->settings;
}
