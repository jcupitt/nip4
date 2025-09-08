// win application window

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

struct _Window {
	GtkApplicationWindow parent;

	/* The current save and load directories.
	 */
	GFile *save_folder;
	GFile *load_folder;

	// the model we display
	Model *model;

	// the view for that model
	View *view;

	// The view we trigger actions on
	View *action_view;

	/* Store things like toolkit menu visibility in gsettings.
	 */
	GSettings *settings;

	/* We need to detect ctrl-click and shift-click for range selecting.
	 *
	 * Windows doesn't seem to support device polling, so we record ctrl and
	 * shift state here in the keyboard handler.
	 */
	guint modifiers;

};

// every alive top-level window
static GSList *window_all = NULL;

G_DEFINE_TYPE(Window, window, GTK_TYPE_APPLICATION_WINDOW);

void
window_set_action_view(View *action_view)
{
	Window *win = WINDOW(view_get_window(action_view));

	if (win)
		win->action_view = action_view;
}

View *
window_get_view(Window *win)
{
	return win->view;
}

GFile *
window_get_save_folder(Window *win)
{
	return win->save_folder;
}

static GFile *
get_parent(GFile *file)
{
	GFile *parent = g_file_get_parent(file);

	return parent ? parent : g_file_new_for_path("/");
}

void
window_set_save_folder(Window *win, GFile *file)
{
	VIPS_UNREF(win->save_folder);
	win->save_folder = get_parent(file);
}

GFile *
window_get_load_folder(Window *win)
{
	return win->load_folder;
}

void
window_set_load_folder(Window *win, GFile *file)
{
	VIPS_UNREF(win->load_folder);
	win->load_folder = get_parent(file);
}

void
window_error(Window *win)
{
	workspace_show_error(window_get_workspace(win));
}

static void
window_dispose(GObject *object)
{
	Window *win = WINDOW(object);

#ifdef DEBUG
	printf("window_dispose:\n");
#endif /*DEBUG*/

	IDESTROY(win->wsg);
	VIPS_UNREF(win->settings);

	window_all = g_slist_remove(window_all, win);

	G_OBJECT_CLASS(window_parent_class)->dispose(object);
}

static void
window_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Window *win = WINDOW(user_data);

	gtk_window_close(GTK_WINDOW(win));
}

static void *
window_quit_action_next(void *a, void *b)
{
	Window *win = WINDOW(a);

	App *app;
	g_object_get(win, "application", &app, NULL);

	g_application_quit(G_APPLICATION(app));

	return NULL;
}

static void
window_quit_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	// from ^Q ... close all modified filemodels, then quit the app
	filemodel_close_registered(window_quit_action_next, user_data);
}

static void
window_fullscreen(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Window *win = WINDOW(user_data);

	g_object_set(win,
		"fullscreened", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

// call ->action on the linked view
static void
window_view_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Window *win = WINDOW(user_data);

	if (win->action_view &&
		IS_VIEW(win->action_view)) {
		View *view = VIEW(win->action_view);
		ViewClass *view_class = VIEW_GET_CLASS(view);

		if (view_class->action)
			view_class->action(action, parameter, view);

		win->action_view = NULL;
	}
}

static GActionEntry window_entries[] = {
	// win window actions

	{ "close", window_close_action },
	{ "quit", window_quit_action },
	{ "fullscreen", action_toggle, NULL, "false", window_fullscreen },

};

static void
window_init(Window *win)
{
#ifdef DEBUG
	printf("window_init:\n");
#endif /*DEBUG*/

	win->settings = g_settings_new(APPLICATION_ID);

	g_autofree char *cwd = g_get_current_dir();
	win->save_folder = g_file_new_for_path(cwd);
	win->load_folder = g_file_new_for_path(cwd);

	gtk_widget_init_template(GTK_WIDGET(win));

	g_action_map_add_action_entries(G_ACTION_MAP(win),
		window_entries, G_N_ELEMENTS(window_entries),
		win);

	window_all = g_slist_prepend(window_all, win);
}

static gboolean
window_close_request_idle(void *user_data)
{
	Window *win = WINDOW(user_data);

	gtk_window_close(GTK_WINDOW(win));

	return FALSE;
}

static void
window_close_request_next(GtkWindow *window,
	Filemodel *filemodel, void *a, void *b)
{
	Window *win = WINDOW(a);

	/* We can't go back to close immediately, the alert is still being shown
	 * and must detach. Close the window when we are back in idle.
	 */
	g_idle_add(window_close_request_idle, win);
}

static gboolean
window_close_request(GtkWindow *self, gpointer user_data)
{
	Window *win = WINDOW(self);

	if (FILEMODEL(win->wsg)->modified) {
		filemodel_save_before_close(FILEMODEL(win->wsg),
			window_close_request_next, win, NULL);
		// block close, then retrigger after save, see
		// window_close_request_idle()
		return TRUE;
	}
	else
		return FALSE;
}

static gboolean
window_key_pressed(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	Window *win = WINDOW(user_data);

	switch (keyval) {
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
		win->modifiers |= GDK_CONTROL_MASK;
		break;

	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
		win->modifiers |= GDK_SHIFT_MASK;
		break;

	default:
		break;
	}

	return FALSE;
}

static gboolean
window_key_released(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	Window *win = WINDOW(user_data);

	switch (keyval) {
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
		win->modifiers &= ~GDK_CONTROL_MASK;
		break;

	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
		win->modifiers &= ~GDK_SHIFT_MASK;
		break;

	default:
		break;
	}

	return FALSE;
}

guint
window_get_modifiers(Window *win)
{
	return win->modifiers;
}

static void
window_class_init(WindowClass *class)
{
	G_OBJECT_CLASS(class)->dispose = window_dispose;

	BIND_RESOURCE("window.ui");

	BIND_CALLBACK(window_close_request);
	BIND_CALLBACK(window_key_pressed);
	BIND_CALLBACK(window_key_released);
}

static void
window_wsg_changed(Workspacegroup *wsg, Window *win)
{
	window_refresh(win);
}

static void
window_wsg_destroy(Workspacegroup *wsg, Window *win)
{
	win->wsg = NULL;

	gtk_window_destroy(GTK_WINDOW(win));
}

static void
window_wsg_child_remove(iContainer parent, iContainer *child,
	Window *win)
{
	window_cull();
}

void
window_set_wsg(Window *win, Workspacegroup *wsg)
{
	g_assert(!win->wsg);

	win->wsg = wsg;
	iobject_ref_sink(IOBJECT(win->wsg));

	// our wsgview is the view for this model
	view_link(VIEW(win->wsgview), MODEL(win->wsg), NULL);

	g_signal_connect_object(win->wsg, "changed",
		G_CALLBACK(window_wsg_changed), win, 0);
	g_signal_connect_object(win->wsg, "destroy",
		G_CALLBACK(window_wsg_destroy), win, 0);
	g_signal_connect_object(win->wsg, "child_remove",
		G_CALLBACK(window_wsg_child_remove), win, 0);

	/* Set start state.
	 */
	(void) window_refresh(win);
}

void
window_set_gfile(Window *win, GFile *gfile)
{
#ifdef DEBUG
	printf("window_set_gfile:\n");
#endif /*DEBUG*/

	if (gfile) {
		g_autofree char *file = g_file_get_path(gfile);
		Workspaceroot *root = win_workspaceroot;

		Workspacegroup *wsg;

		if (!(wsg = workspacegroup_new_from_file(root, file, file))) {
			window_error(win);
			return;
		}

		window_set_wsg(win, wsg);

		symbol_recalculate_all();
	}
}

static GSettings *
window_settings(Window *win)
{
	App *app;

	// FIXME ... or have an "app" member in win? or use
	// gtk_window_get_application()?
	g_object_get(win, "application", &app, NULL);

	return app ? app_settings(app) : NULL;
}

// init stateful actions from settings
static void
window_init_settings(Window *win)
{
	GSettings *settings = window_settings(win);

	if (settings) {
		set_state(GTK_WIDGET(win), window_settings(win), "definitions");
		set_state(GTK_WIDGET(win), window_settings(win), "toolkits");
	}
}

Window *
window_new(App *app, Workspacegroup *wsg)
{
#ifdef DEBUG
	printf("window_new:\n");
#endif /*DEBUG*/

	Window *win = g_object_new(WINDOW_TYPE,
		"application", app,
		NULL);

	window_set_wsg(win,
		wsg ? wsg : workspacegroup_new_blank(win_workspaceroot, NULL));

    // we can't do this in _init() since we need app to be set
    window_init_settings(win);

	gboolean welcome;
	g_object_get(app, "welcome", &welcome, NULL);
	if (welcome) {
		char save_dir[VIPS_PATH_MAX];
		char buf[256];

		g_snprintf(buf, 256, _("Welcome to %s-%s!"), PACKAGE, VERSION);
		g_strlcpy(save_dir, get_savedir(), VIPS_PATH_MAX);
		path_expand(save_dir);
		error_top("%s", buf);
		error_sub(
			_("a new directory has been created to hold startup, "
			  "data and temporary files: %s\n"
			  "if you've used previous versions of %s, you might want "
			  "to copy files over from your old work area"),
			save_dir, PACKAGE);

		workspace_show_alert(window_get_workspace(win));
	}

	double size = directory_size(PATH_TMP);
	if (size > 10 * 1024 * 1024) {
		error_top("%s", _("Many files in temporary area"));

		char save_dir[VIPS_PATH_MAX];
		g_strlcpy(save_dir, PATH_TMP, VIPS_PATH_MAX);
		path_expand(save_dir);

		char txt[256];
		VipsBuf buf = VIPS_BUF_STATIC(txt);
		vips_buf_append_size(&buf, size);
		error_sub(_("The temporary area \"%s\" contains %s of files. "
            "Use \"Recover after crash\" to clean up."),
            save_dir, vips_buf_all(&buf));

		workspace_show_alert(window_get_workspace(win));
	}

	return win;
}

static void *
window_cull_sub(Window *win)
{
	if (workspacegroup_is_empty(win->wsg)) {
		filemodel_set_modified(FILEMODEL(win->wsg), FALSE);
		gtk_window_close(GTK_WINDOW(win));
	}

	return NULL;
}

static guint window_cull_timeout_id = 0;

static gboolean
window_cull_timeout(gpointer user_data)
{
	window_cull_timeout_id = 0;

	slist_map(window_all, (SListMapFn) window_cull_sub, NULL);

	return FALSE;
}

void
window_cull(void)
{
	// do this on a timeout so we can cull from anywhere
	VIPS_FREEF(g_source_remove, window_cull_timeout_id);
	window_cull_timeout_id = g_timeout_add(100,
		(GSourceFunc) window_cull_timeout, NULL);
}

static void *
window_count_images(VipsObject *object, int *n)
{
	if (VIPS_IS_IMAGE(object))
		*n += 1;

	return NULL;
}

static void *
window_size_images(VipsObject *object, size_t *size)
{
	if (VIPS_IS_IMAGE(object))
		*size += VIPS_IMAGE_SIZEOF_IMAGE(VIPS_IMAGE(object));

	return NULL;
}

void
window_about(Window *win, VipsBuf *buf)
{
	vips_buf_appendf(buf, "%s %d.%d.%d\n",
		_("linked to libvips"),
		vips_version(0), vips_version(1), vips_version(2));

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

	if (win->wsg)
		vips_buf_appendf(buf, _("%d objects in workspace\n"),
			workspacegroup_get_n_objects(win->wsg));

	vips_buf_appendf(buf, _("using %d threads\n"), vips_concurrency_get());

	size_t size;
	int n;

	size = 0;
	n = 0;
	vips_object_map((VipsSListMap2Fn) window_size_images, &size, NULL);
	vips_object_map((VipsSListMap2Fn) window_count_images, &n, NULL);

	vips_buf_append_size(buf, size);
	vips_buf_appendf(buf, _(" in %d images\n"), n);

	vips_buf_appendf(buf, _("%d operations cached\n"), vips_cache_get_size());

	vips_buf_append_size(buf, vips_tracked_get_mem());
	vips_buf_appendf(buf, _(" bytes in %d pixel buffers\n"),
		vips_tracked_get_allocs());
	vips_buf_append_size(buf, vips_tracked_get_mem_highwater());
	vips_buf_appendf(buf, _(" pixel buffer highwater mark\n"));

	vips_buf_appendf(buf, _("%d open files\n"), vips_tracked_get_files());
}

gboolean
window_is_empty(Window *win)
{
	Workspace *ws = window_get_workspace(win);

	// if the error bar is open, the user needs to be able to see it
	return workspacegroup_is_empty(win->wsg) && !ws->show_error;
}

GtkWindow *
window_pick_one(void)
{
	return window_all ? GTK_WINDOW(window_all->data) : NULL;
}

GSettings *
window_get_settings(GtkWidget *widget)
{
	Window *win = WINDOW(gtk_widget_get_root(GTK_WIDGET(widget)));

	return win->settings;
}
