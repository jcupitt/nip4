/* an image display window
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
 */
#define DEBUG

#include "nip4.h"

/* We have a number of active imageui (open images) and look them up via the
 * tilesource filename or image pointer.
 *
 * Use these to flip back to an already open image.
 */
typedef struct _Active {
	Imagewindow *win;

	/* This is a ref ... we can't just use the ref in the widget hierarchy or
	 * it'll be freed during gtk_stack_remove() and cause chaos.
	 *
	 * Fetch active
	 */
	Imageui *imageui;

	/* A timestamp we update on every action -- we throw away the oldest views
	 * first.
	 */
	int timestamp;
} Active;

typedef struct _ViewSettings {
	gboolean valid;

	// we don't save "mode", that's an image property, not a view property
	double scale;
	double offset;
	int page;
	gboolean falsecolour;
	gboolean log;
	gboolean icc;
	gboolean active;

	TilecacheBackground background;
	double zoom;
	double x;
	double y;

} ViewSettings;

struct _Imagewindow {
	GtkApplicationWindow parent;

	/* A ref to the iimage we came from ... this is what we update on open
	 * etc.
	 */
	iImage *iimage;
	guint iimage_changed_sid;
	guint iimage_destroy_sid;

	/* The imageui we are currently displaying, or NULL. This is not
	 * a reference --- the real refs are in @stack.
	 */
	Imageui *imageui;

	/* The set of filenames we cycle through.
	 */
	char **files;
	int n_files;
	int current_file;
	gboolean preserve;

	/* Widgets.
	 */
	GtkWidget *right_click_menu;
	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *progress_bar;
	GtkWidget *progress;
	GtkWidget *progress_cancel;
	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *main_box;
	GtkWidget *stack;
	GtkWidget *properties;
	GtkWidget *display_bar;
	GtkWidget *info_bar;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;

	/* The set of active images in the stack right now. These are not
	 * references.
	 */
	GSList *active;

	/* Keep recent view setting here on image change.
	 */
	ViewSettings view_settings;

	GSettings *settings;
};

G_DEFINE_TYPE(Imagewindow, imagewindow, GTK_TYPE_APPLICATION_WINDOW);

/* Our signals.
 */
enum {
	SIG_CHANGED,		/* A new imageui and tilesource */
	SIG_STATUS_CHANGED, /* New mouse position */
	SIG_LAST
};

static guint imagewindow_signals[SIG_LAST] = { 0 };

/* GTypes we handle in copy/paste and drag/drop paste ... these are in the
 * order we try, so a GFile is preferred to a simple string.
 *
 * gnome file manager pastes as GdkFileList, GFile, gchararray
 * print-screen button pastes as GdkTexture, GdkPixbuf
 *
 * Created in _class_init(), since some of these types are only defined at
 * runtime.
 */
static GType *imagewindow_supported_types;
static int imagewindow_n_supported_types;

static void
imagewindow_set_error(Imagewindow *win, const char *message)
{
	int i;

#ifdef DEBUG
	printf("imagewindow_set_error: %s\n", message);
#endif /*DEBUG*/

	/* Remove any trailing \n.
	 */
	g_autofree char *err = g_strdup(message);
	for (i = strlen(err); i > 0 && err[i - 1] == '\n'; i--)
		err[i - 1] = '\0';
	gtk_label_set_text(GTK_LABEL(win->error_label), err);

	gtk_info_bar_set_revealed(GTK_INFO_BAR(win->error_bar), TRUE);
}

static void
imagewindow_error(Imagewindow *win)
{
	imagewindow_set_error(win, vips_error_buffer());
	vips_error_clear();
}

static void
imagewindow_gerror(Imagewindow *win, GError **error)
{
	if (error && *error) {
		imagewindow_set_error(win, (*error)->message);
		g_error_free(*error);
	}
}

static void
imagewindow_error_hide(Imagewindow *win)
{
#ifdef DEBUG
	printf("imagewindow_error_hide:\n");
#endif /*DEBUG*/

	gtk_info_bar_set_revealed(GTK_INFO_BAR(win->error_bar), FALSE);
}

/* Manage the set of active views.
 */

static Active *
imagewindow_active_lookup_by_filename(Imagewindow *win, const char *filename)
{
	for (GSList *p = win->active; p; p = p->next) {
		Active *active = (Active *) p->data;
		Tilesource *tilesource = imageui_get_tilesource(active->imageui);

		if (tilesource->filename &&
			g_str_equal(tilesource->filename, filename))
			return active;
	}

	return NULL;
}

static void
imagewindow_active_touch(Imagewindow *win, Active *active)
{
	static int serial = 0;

	active->timestamp = serial++;
}

static void
imagewindow_active_remove(Imagewindow *win, Active *active)
{
	gtk_stack_remove(GTK_STACK(win->stack), GTK_WIDGET(active->imageui));

	if (active->imageui == win->imageui)
		win->imageui = NULL;

	VIPS_UNREF(active->imageui);

	win->active = g_slist_remove(win->active, active);

	g_free(active);
}

static void
imagewindow_active_remove_all(Imagewindow *win)
{
	while (win->active) {
		Active *active = (Active *) win->active->data;

		imagewindow_active_remove(win, active);
	}
}

static void
imagewindow_active_add(Imagewindow *win, Imageui *imageui)
{
	Active *active = VIPS_NEW(NULL, Active);
	active->win = win;
	active->imageui = imageui;

	// we hold a true reference
	g_object_ref(imageui);

	win->active = g_slist_prepend(win->active, active);

	imagewindow_active_touch(win, active);

	while (g_slist_length(win->active) > 3) {
		Active *oldest;

		oldest = NULL;
		for (GSList *p = win->active; p; p = p->next) {
			Active *active = (Active *) p->data;

			if (!oldest ||
				active->timestamp < oldest->timestamp)
				oldest = active;
		}

		imagewindow_active_remove(win, oldest);
	}
}

/* Manage the set of filenames we have lined up to view.
 */

static void
imagewindow_files_free(Imagewindow *win)
{
	imagewindow_active_remove_all(win);

	VIPS_FREEF(g_strfreev, win->files);
	win->n_files = 0;
	win->current_file = 0;
}

static void
imagewindow_files_set_list_gfiles(Imagewindow *win, GSList *files)
{
	GSList *p;
	int i;

	imagewindow_files_free(win);

	win->n_files = g_slist_length(files);
	win->files = VIPS_ARRAY(NULL, win->n_files + 1, char *);
	for (i = 0, p = files; i < win->n_files; i++, p = p->next) {
		GFile *file = (GFile *) p->data;

		win->files[i] = g_file_get_path(file);
	}
}

static int
sort_filenames(const void *a, const void *b)
{
	const char *f1 = (const char *) a;
	const char *f2 = (const char *) b;

	return g_ascii_strcasecmp(f1, f2);
}

static void
imagewindow_files_set_list(Imagewindow *win, GSList *files)
{
	GSList *p;
	int i;

	imagewindow_files_free(win);

	win->n_files = g_slist_length(files);
	win->files = VIPS_ARRAY(NULL, win->n_files + 1, char *);
	for (i = 0, p = files; i < win->n_files; i++, p = p->next)
		win->files[i] = g_strdup((char *) p->data);

#ifdef DEBUG
	printf("imagewindow_files_set_list: %d files\n", win->n_files);
#endif /*DEBUG*/
}

static void
imagewindow_files_set_path(Imagewindow *win, char *path)
{
	g_autofree char *dirname = g_path_get_dirname(path);
	g_autoptr(GFile) file = g_file_new_for_path(path);

	GError *error = NULL;

	const char *filename;

#ifdef DEBUG
	printf("imagewindow_files_set_path:\n");
#endif /*DEBUG*/

	g_autoptr(GDir) dir = g_dir_open(dirname, 0, &error);
	if (!dir) {
		imagewindow_gerror(win, &error);
		return;
	}

	GSList *files = NULL;

	// always add the passed-in file, even if it doesn't exist
	files = g_slist_prepend(files, g_strdup(path));

	while ((filename = g_dir_read_name(dir))) {
		g_autofree char *path = g_build_path("/", dirname, filename, NULL);
		g_autoptr(GFile) this_file = g_file_new_for_path(path);

		// - never add the the passed-in filename (we add it above)
		// - avoid directories and dotfiles
		if (!g_file_equal(file, this_file) &&
			!vips_isprefix(".", filename) &&
			!g_file_test(path, G_FILE_TEST_IS_DIR))
			files = g_slist_prepend(files, g_steal_pointer(&path));
	}

	files = g_slist_sort(files, (GCompareFunc) sort_filenames);

	imagewindow_files_set_list(win, files);

	// it's be great to use g_autoslist(), but I don't see how :(
	g_slist_free_full(g_steal_pointer(&files), g_free);

	for (int i = 0; i < win->n_files; i++) {
		g_autoptr(GFile) file2 = g_file_new_for_path(win->files[i]);

		if (g_file_equal(file, file2)) {
			win->current_file = i;
			break;
		}
	}
}

static void
imagewindow_files_set(Imagewindow *win, char **files, int n_files)
{
	imagewindow_files_free(win);

	if (n_files == 1)
		imagewindow_files_set_path(win, files[0]);
	else if (n_files > 1) {
		win->n_files = n_files;
		win->files = VIPS_ARRAY(NULL, n_files + 1, char *);
		for (int i = 0; i < win->n_files; i++)
			win->files[i] = g_strdup(files[i]);

		win->current_file = 0;
	}
}

double
imagewindow_get_zoom(Imagewindow *win)
{
	double zoom;

	if (win->imageui)
		g_object_get(win->imageui,
			"zoom", &zoom,
			NULL);
	else
		zoom = 1.0;

	return zoom;
}

void
imagewindow_get_mouse_position(Imagewindow *win,
	double *image_x, double *image_y)
{
	*image_x = 0.0;
	*image_y = 0.0;
	if (win->imageui)
		imageui_get_mouse_position(win->imageui, image_x, image_y);
}

Tilesource *
imagewindow_get_tilesource(Imagewindow *win)
{
	return win->imageui ? imageui_get_tilesource(win->imageui) : NULL;
}

static void
imagewindow_reset_view(Imagewindow *win)
{
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	printf("imagewindow_reset_view:\n");

	if (tilesource) {
		g_object_set(tilesource,
			"falsecolour", FALSE,
			"log", FALSE,
			"icc", FALSE,
			"scale", 1.0,
			"offset", 0.0,
			NULL);
	}
}

static void
imagewindow_preeval(VipsImage *image,
	VipsProgress *progress, Imagewindow *win)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(win->progress_bar), TRUE);
}

typedef struct _EvalUpdate {
	Imagewindow *win;
	int eta;
	int percent;
} EvalUpdate;

static gboolean
imagewindow_eval_idle(void *user_data)
{
	EvalUpdate *update = (EvalUpdate *) user_data;
	Imagewindow *win = update->win;

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
imagewindow_eval(VipsImage *image,
	VipsProgress *progress, Imagewindow *win)
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
	printf("imagewindow_eval: %d%%\n", progress->percent);
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

	g_idle_add(imagewindow_eval_idle, update);
}

static void
imagewindow_posteval(VipsImage *image,
	VipsProgress *progress, Imagewindow *win)
{
	gtk_action_bar_set_revealed(GTK_ACTION_BAR(win->progress_bar), FALSE);
}

/* Save and restore view setttings.
 */

static void
imagewindow_save_view_settings(Imagewindow *win, ViewSettings *view_settings)
{
	Tilesource *tilesource = imagewindow_get_tilesource(win);
	Imageui *imageui = win->imageui;

	if (!tilesource ||
		!imageui) {
		view_settings->valid = FALSE;
		return;
	}

	g_object_get(tilesource,
		"scale", &view_settings->scale,
		"offset", &view_settings->offset,
		"page", &view_settings->page,
		"falsecolour", &view_settings->falsecolour,
		"log", &view_settings->log,
		"icc", &view_settings->icc,
		"active", &view_settings->active,
		NULL);

	g_object_get(imageui,
		"background", &view_settings->background,
		"zoom", &view_settings->zoom,
		"x", &view_settings->x,
		"y", &view_settings->y,
		NULL);

	view_settings->valid = TRUE;
}

static void
imagewindow_restore_view_settings(Imagewindow *win,
	ViewSettings *view_settings)
{
	Tilesource *tilesource = imagewindow_get_tilesource(win);
	Imageui *imageui = win->imageui;

	if (!view_settings->valid)
		return;

	if (tilesource)
		g_object_set(tilesource,
			"scale", view_settings->scale,
			"offset", view_settings->offset,
			"page", view_settings->page,
			"falsecolour", view_settings->falsecolour,
			"log", view_settings->log,
			"icc", view_settings->icc,
			"active", view_settings->active,
			NULL);

	if (imageui) {
		g_object_set(imageui,
			"background", view_settings->background,
			"zoom", view_settings->zoom,
			NULL);

		// must set zoom before xy
		g_object_set(imageui,
			"x", view_settings->x,
			"y", view_settings->y,
			NULL);
	}
}

static void
imagewindow_tilesource_changed(Tilesource *tilesource, Imagewindow *win)
{
#ifdef DEBUG
	printf("imagewindow_tilesource_changed:\n");
#endif /*DEBUG*/

	if (tilesource->load_error)
		imagewindow_set_error(win, tilesource->load_message);

	if (win->preserve)
		// change the display to match our window settings
		imagewindow_restore_view_settings(win, &win->view_settings);
	else {
		// update win settings from new tile source
		GVariant *state;

		state = g_variant_new_boolean(tilesource->falsecolour);
		change_state(GTK_WIDGET(win), "falsecolour", state);

		state = g_variant_new_boolean(tilesource->log);
		change_state(GTK_WIDGET(win), "log", state);

		state = g_variant_new_boolean(tilesource->icc);
		change_state(GTK_WIDGET(win), "icc", state);

		const char *name =
			vips_enum_nick(TILESOURCE_MODE_TYPE, tilesource->mode);
		state = g_variant_new_string(name);
		change_state(GTK_WIDGET(win), "mode", state);
	}
}

static void
imagewindow_status_changed(Imagewindow *win)
{
	g_signal_emit(win, imagewindow_signals[SIG_STATUS_CHANGED], 0);
}

static void
imagewindow_changed(Imagewindow *win)
{
	g_signal_emit(win, imagewindow_signals[SIG_CHANGED], 0);
}

static void
imagewindow_imageui_changed(Imageui *imageui, Imagewindow *win)
{
	TilecacheBackground background;
	g_object_get(imageui, "background", &background, NULL);

	const char *str = vips_enum_string(TILECACHE_BACKGROUND_TYPE, background);
	if (str) {
		GVariant *state = g_variant_new_string(str);
		change_state(GTK_WIDGET(win), "background", state);

		imagewindow_status_changed(win);
	}
}

/* Add an imageui to the stack.
 */
static void
imagewindow_imageui_add(Imagewindow *win, Imageui *imageui)
{
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	g_signal_connect_object(tilesource, "preeval",
		G_CALLBACK(imagewindow_preeval), win, 0);
	g_signal_connect_object(tilesource, "eval",
		G_CALLBACK(imagewindow_eval), win, 0);
	g_signal_connect_object(tilesource, "posteval",
		G_CALLBACK(imagewindow_posteval), win, 0);

	g_signal_connect_object(tilesource, "changed",
		G_CALLBACK(imagewindow_tilesource_changed), win, 0);

	g_signal_connect_object(imageui, "changed",
		G_CALLBACK(imagewindow_imageui_changed), win, 0);

	gtk_stack_add_child(GTK_STACK(win->stack), GTK_WIDGET(imageui));

	imagewindow_active_add(win, imageui);

	// now it's in the UI, trigger a load
	tilesource_background_load(tilesource);
}

/* Change the image we are manipulating. The imageui is in the stack already.
 */
static void
imagewindow_imageui_set_visible(Imagewindow *win,
	Imageui *imageui, GtkStackTransitionType transition)
{
	Tilesource *old_tilesource = imagewindow_get_tilesource(win);
	Tilesource *new_tilesource =
		imageui ? imageui_get_tilesource(imageui) : NULL;

	VipsImage *image;
	char *title;

	printf("imagewindow_imageui_set_visible\n");

	/* Save the current view settings in case we need to restore them.
	 */
	imagewindow_save_view_settings(win, &win->view_settings);

	if (old_tilesource)
		g_object_set(old_tilesource,
			"visible", FALSE,
			NULL);

	g_object_set(win->properties,
		"tilesource", new_tilesource,
		NULL);

	/* Update title and subtitle.
	 */
	title = new_tilesource ? (char *) tilesource_get_path(new_tilesource) : NULL;
	title = (char *) tilesource_get_path(new_tilesource);
	gtk_label_set_text(GTK_LABEL(win->title), title ? title : "Untitled");

	if (new_tilesource &&
		(image = tilesource_get_base_image(new_tilesource))) {
		char str[256];
		VipsBuf buf = VIPS_BUF_STATIC(str);

		vips_buf_appendf(&buf, "%dx%d, ", image->Xsize, image->Ysize);
		if (new_tilesource->n_pages > 1)
			vips_buf_appendf(&buf, "%d pages, ", new_tilesource->n_pages);
		if (vips_image_get_coding(image) == VIPS_CODING_NONE)
			vips_buf_appendf(&buf,
				g_dngettext(GETTEXT_PACKAGE,
					" %s, %d band, %s",
					" %s, %d bands, %s",
					image->Bands),
				vips_enum_nick(VIPS_TYPE_BAND_FORMAT, image->BandFmt),
				vips_image_get_bands(image),
				vips_enum_nick(VIPS_TYPE_INTERPRETATION, image->Type));
		else
			vips_buf_appendf(&buf, "%s",
				vips_enum_nick(VIPS_TYPE_CODING,
					vips_image_get_coding(image)));
		vips_buf_appends(&buf, ", ");

		vips_buf_append_size(&buf,
			(double) image->Ysize * VIPS_IMAGE_SIZEOF_LINE(image));
		vips_buf_appends(&buf, ", ");

		vips_buf_appendf(&buf, "%g x %g p/mm", image->Xres, image->Yres);
		gtk_label_set_text(GTK_LABEL(win->subtitle), vips_buf_all(&buf));
	}
	else
		gtk_label_set_text(GTK_LABEL(win->subtitle), "");

	if (imageui) {
		printf("imagewindow_imageui_set_visible: FIXME ... rotate transitions cause a stack overflow? how odd\n");
		// gtk_stack_set_transition_type(GTK_STACK(win->stack), transition);
		gtk_stack_set_visible_child(GTK_STACK(win->stack), GTK_WIDGET(imageui));

		/* Enable the control settings, if the displaycontrolbar is on.
		 */
		g_autoptr(GVariant) control =
			g_settings_get_value(win->settings, "control");

		g_object_set(new_tilesource,
			"active", g_variant_get_boolean(control),
			"visible", TRUE,
			NULL);
	}

	// not a ref, so we can just overwrite it
	win->imageui = imageui;

	// tell everyone there's a new imageui
	imagewindow_changed(win);

	// update the menus
	imagewindow_tilesource_changed(new_tilesource, win);
}

static void
imagewindow_open_current_file(Imagewindow *win,
	GtkStackTransitionType transition)
{
	imagewindow_error_hide(win);

	if (!win->files)
		imagewindow_imageui_set_visible(win, NULL, transition);
	else {
		char *filename = win->files[win->current_file];

		Imageui *imageui;
		Active *active;

#ifdef DEBUG
		printf("imagewindow_open_current_file: %s:\n", filename);
#endif /*DEBUG*/

		imageui = NULL;
		if ((active = imagewindow_active_lookup_by_filename(win, filename)))
			// reselect old image
			imageui = active->imageui;
		else {
			// new image
			g_autoptr(Tilesource) tilesource =
				tilesource_new_from_file(filename);
			if (!tilesource) {
				imagewindow_error(win);
				return;
			}

			imageui = imageui_new(tilesource);
			if (!imageui) {
				imagewindow_error(win);
				return;
			}

			imagewindow_imageui_add(win, imageui);
		}

		printf("imagewindow_open_current_file: FIXME ... update the iimage we came from?\n");
		/* do need something to prevent update loops?
		 * we emit "changed" for a new imageui, perhaps iimage should just
		 * listen for that?
		 */
		if (win->iimage) {
			iimage_replace(win->iimage, filename);
			symbol_recalculate_all_force(FALSE);
		}

		imagewindow_imageui_set_visible(win, imageui, transition);
	}
}

static void
imagewindow_dispose(GObject *object)
{
	Imagewindow *win = IMAGEWINDOW(object);

#ifdef DEBUG
	printf("imagewindow_dispose:\n");
#endif /*DEBUG*/

	imagewindow_files_free(win);

	FREESID(win->iimage_changed_sid, win->iimage);
	FREESID(win->iimage_destroy_sid, win->iimage);
	VIPS_FREEF(gtk_widget_unparent, win->right_click_menu);
	VIPS_FREEF(g_timer_destroy, win->progress_timer);

	G_OBJECT_CLASS(imagewindow_parent_class)->dispose(object);
}

static void
imagewindow_cancel_clicked(GtkWidget *button, Imagewindow *win)
{
	Tilesource *tilesource;
	VipsImage *image;

	if ((tilesource = imagewindow_get_tilesource(win)) &&
		(image = tilesource_get_image(tilesource)))
		vips_image_set_kill(image, TRUE);
}

static void
imagewindow_error_response(GtkWidget *button, int response, Imagewindow *win)
{
	imagewindow_error_hide(win);
}

static GdkTexture *
texture_new_from_image(VipsImage *image)
{
	GdkTexture *texture;

	// don't convert huge images to textures, we'll kill the machine
	if ((gint64) image->Xsize * image->Ysize > 5000 * 5000) {
		vips_error("Convert to texture",
			_("Image too large to convert to texture"));
		return NULL;
	}

	void *buf;
	size_t len;
	if (vips_tiffsave_buffer(image, &buf, &len, NULL))
		return NULL;

	g_autoptr(GBytes) bytes = g_bytes_new_take(buf, len);
	GError *error = NULL;
	texture = gdk_texture_new_from_bytes(bytes, &error);
	if (!texture) {
		vips_error("Convert to texture", "%s", error->message);
		g_error_free(error);
	}

	return texture;
}

static void
imagewindow_copy_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(win));
		g_autoptr(GFile) file = tilesource_get_file(tilesource);

		VipsImage *image;

		if (file)
			gdk_clipboard_set(clipboard, G_TYPE_FILE, file);
		else if ((image = tilesource_get_base_image(tilesource))) {
			g_autoptr(GdkTexture) texture = texture_new_from_image(image);

			if (texture)
				gdk_clipboard_set(clipboard, GDK_TYPE_TEXTURE, texture);
			else
				imagewindow_error(win);
		}
	}
}

static void
image_new_from_texture_free(VipsImage *image, GBytes *bytes)
{
	g_bytes_unref(bytes);
}

static VipsImage *
image_new_from_texture(GdkTexture *texture)
{
	g_autoptr(GBytes) bytes = gdk_texture_save_to_tiff_bytes(texture);

	if (bytes) {
		gsize len;
		gconstpointer data = g_bytes_get_data(bytes, &len);

		VipsImage *image;
		if ((image = vips_image_new_from_buffer(data, len, "", NULL))) {
			g_signal_connect(image, "close",
				G_CALLBACK(image_new_from_texture_free), bytes);
			g_bytes_ref(bytes);

			return image;
		}
	}
	else
		vips_error("Convert to TIFF", _("unable to convert texture to TIFF"));

	return NULL;
}

static void
imagewindow_set_from_value(Imagewindow *win, const GValue *value)
{
	if (G_VALUE_TYPE(value) == GDK_TYPE_FILE_LIST) {
		GdkFileList *file_list = g_value_get_boxed(value);
		g_autoptr(GSList) files = gdk_file_list_get_files(file_list);

		imagewindow_open_list_gfiles(win, files);
	}
	else if (G_VALUE_TYPE(value) == G_TYPE_FILE) {
		GFile *file = g_value_get_object(value);

		imagewindow_open_gfiles(win, &file, 1);
	}
	else if (G_VALUE_TYPE(value) == G_TYPE_STRING) {
		// remove leading and trailing whitespace
		// modifies the string in place, so we must dup
		g_autofree char *text = g_strstrip(g_strdup(g_value_get_string(value)));

		imagewindow_open_files(win, &text, 1);
	}
	else if (G_VALUE_TYPE(value) == GDK_TYPE_TEXTURE) {
		GdkTexture *texture = g_value_get_object(value);

		g_autoptr(VipsImage) image = image_new_from_texture(texture);
		if (image)
			imagewindow_open_image(win, image);
		else
			imagewindow_error(win);
	}
}

static void
imagewindow_paste_action_ready(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	GdkClipboard *clipboard = GDK_CLIPBOARD(source_object);
	Imagewindow *win = IMAGEWINDOW(user_data);

	const GValue *value;
	GError *error = NULL;
	value = gdk_clipboard_read_value_finish(clipboard, res, &error);
	if (error)
		imagewindow_gerror(win, &error);
	else if (value)
		imagewindow_set_from_value(win, value);
}

static void
imagewindow_paste_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(win));

	GdkContentFormats *formats = gdk_clipboard_get_formats(clipboard);
	gsize n_types;
	const GType *types = gdk_content_formats_get_gtypes(formats, &n_types);

#ifdef DEBUG
	printf("clipboard in %lu formats\n", n_types);
	for (gsize i = 0; i < n_types; i++)
		printf("%lu - %s\n", i, g_type_name(types[i]));
#endif /*DEBUG*/

	gboolean handled = FALSE;
	for (gsize i = 0; i < n_types; i++) {
		for (int j = 0; j < imagewindow_n_supported_types; j++)
			if (types[i] == imagewindow_supported_types[j]) {
				gdk_clipboard_read_value_async(clipboard,
					imagewindow_supported_types[j],
					G_PRIORITY_DEFAULT,
					NULL,
					imagewindow_paste_action_ready,
					win);
				handled = TRUE;
				break;
			}

		if (handled)
			break;
	}
}

static void
imagewindow_magin_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_magin(win->imageui);
}

static void
imagewindow_magout_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_magout(win->imageui);
}

static void
imagewindow_bestfit_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_bestfit(win->imageui);
}

static void
imagewindow_oneone_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_oneone(win->imageui);
}

static void
imagewindow_reload_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		g_autoptr(GFile) file = tilesource_get_file(tilesource);

		if (file)
			imagewindow_open_gfiles(win, &file, 1);
	}
}

static void
imagewindow_duplicate_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	GtkStackTransitionType transition = GTK_STACK_TRANSITION_TYPE_NONE;

	App *app;
	Imagewindow *new_win;
	int width, height;

	g_object_get(win, "application", &app, NULL);
	new_win = imagewindow_new(app);

	new_win->n_files = win->n_files;
	new_win->files = g_strdupv(win->files);
	new_win->current_file = win->current_file;

	gtk_window_get_default_size(GTK_WINDOW(win), &width, &height);
	gtk_window_set_default_size(GTK_WINDOW(new_win), width, height);

	copy_state(GTK_WIDGET(new_win), GTK_WIDGET(win), "control");
	copy_state(GTK_WIDGET(new_win), GTK_WIDGET(win), "info");
	copy_state(GTK_WIDGET(new_win), GTK_WIDGET(win), "properties");
	copy_state(GTK_WIDGET(new_win), GTK_WIDGET(win), "background");

	if (win->imageui) {
		Tilesource *tilesource = imageui_get_tilesource(win->imageui);
		g_autoptr(Tilesource) new_tilesource =
			tilesource_duplicate(tilesource);
		Imageui *new_imageui = imageui_duplicate(new_tilesource, win->imageui);

		imagewindow_imageui_add(new_win, new_imageui);
		imagewindow_imageui_set_visible(new_win, new_imageui, transition);
	}

	gtk_window_present(GTK_WINDOW(new_win));
}

static void
imagewindow_replace_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->iimage)
		classmodel_graphic_replace(CLASSMODEL(win->iimage), GTK_WIDGET(win));
}

static void
imagewindow_saveas_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->iimage)
		classmodel_graphic_save(CLASSMODEL(win->iimage), GTK_WIDGET(win));
}

static GFile *
get_parent(GFile *file)
{
	GFile *parent = g_file_get_parent(file);

	return parent ? parent : g_file_new_for_path("/");
}

static void
imagewindow_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	gtk_window_destroy(GTK_WINDOW(win));
}

static void
imagewindow_fullscreen(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	g_object_set(win,
		"fullscreened", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_control(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	g_object_set(win->display_bar,
		"revealed", g_variant_get_boolean(state),
		NULL);

	/* Disable most visualisation settings if the controls are hidden. It's
	 * much too confusing.
	 */
	if (tilesource)
		g_object_set(tilesource,
			"active", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_info(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	g_object_set(win->info_bar,
		"revealed", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

// is an image being background-loaded?
static gboolean
imagewindow_loading(Imagewindow *win)
{
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		gboolean loaded;

		g_object_get(tilesource, "loaded", &loaded, NULL);
		return !loaded;
	}

	return FALSE;
}

static void
imagewindow_next_image(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	printf("imagewindow_next_image:\n");

	// if there's a background load active, do nothing
	// we want to prevent many bg loads queueing up
	if (imagewindow_loading(win))
		return;

	if (win->n_files > 0) {
		win->current_file = (win->current_file + 1) % win->n_files;
		imagewindow_open_current_file(win,
			GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT);
	}
}

static void
imagewindow_prev_image(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	// if there's a background load active, then do nothing
	// we want to prevent many bg loads queueing up
	if (imagewindow_loading(win))
		return;

	if (win->n_files > 0) {
		win->current_file = (win->current_file + win->n_files - 1) %
			win->n_files;
		imagewindow_open_current_file(win,
			GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT);
	}
}

static void
imagewindow_next(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		int n_pages = tilesource->n_pages;
		int page = VIPS_CLIP(0, tilesource->page, n_pages - 1);

		g_object_set(tilesource,
			"page", (page + 1) % n_pages,
			NULL);
	}
}

static void
imagewindow_prev(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		int n_pages = tilesource->n_pages;
		int page = VIPS_CLIP(0, tilesource->page, n_pages - 1);

		g_object_set(tilesource,
			"page", page == 0 ? n_pages - 1 : page - 1,
			NULL);
	}
}

static void
imagewindow_scale(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui) {
		if (!imageui_scale(win->imageui))
			imagewindow_error(win);
	}
}

static void
imagewindow_log(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource)
		g_object_set(tilesource,
			"log", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_icc(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource)
		g_object_set(tilesource,
			"icc", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_falsecolour(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource)
		g_object_set(tilesource,
			"falsecolour", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_preserve(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	win->preserve = g_variant_get_boolean(state);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_mode(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);
	const gchar *str = g_variant_get_string(state, NULL);
	TilesourceMode mode =
		vips_enum_from_nick("nip4", TILESOURCE_MODE_TYPE, str);

	if (tilesource)
		g_object_set(tilesource,
			"mode", mode,
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_background(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	const gchar *str = g_variant_get_string(state, NULL);
	TilecacheBackground background =
		vips_enum_from_nick("vipsdisp", TILECACHE_BACKGROUND_TYPE, str);

	if (win->imageui)
		g_object_set(win->imageui,
			"background", background,
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_reset(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	imagewindow_reset_view(win);
}

static void
imagewindow_properties(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	gboolean revealed = g_variant_get_boolean(state);

#ifdef DEBUG
	printf("imagewindow_properties:\n");
#endif /*DEBUG*/

	g_object_set(win->properties,
		"revealed", revealed,
		NULL);

	g_simple_action_set_state(action, state);
}

static GActionEntry imagewindow_entries[] = {
	{ "copy", imagewindow_copy_action },
	{ "paste", imagewindow_paste_action },

	{ "magin", imagewindow_magin_action },
	{ "magout", imagewindow_magout_action },
	{ "bestfit", imagewindow_bestfit_action },
	{ "oneone", imagewindow_oneone_action },

	{ "reload", imagewindow_reload_action },
	{ "duplicate", imagewindow_duplicate_action },
	{ "replace", imagewindow_replace_action },
	{ "saveas", imagewindow_saveas_action },
	{ "close", imagewindow_close_action },

	{ "fullscreen", action_toggle, NULL, "false", imagewindow_fullscreen },
	{ "control", action_toggle, NULL, "false", imagewindow_control },
	{ "info", action_toggle, NULL, "false", imagewindow_info },
	{ "properties", action_toggle, NULL, "false", imagewindow_properties },

	{ "next_image", imagewindow_next_image },
	{ "prev_image", imagewindow_prev_image },

	{ "next", imagewindow_next },
	{ "prev", imagewindow_prev },
	{ "scale", imagewindow_scale },
	{ "log", action_toggle, NULL, "false", imagewindow_log },
	{ "icc", action_toggle, NULL, "false", imagewindow_icc },
	{ "falsecolour", action_toggle, NULL, "false", imagewindow_falsecolour },
	{ "preserve", action_toggle, NULL, "false", imagewindow_preserve },
	{ "mode", action_radio, "s", "'multipage'", imagewindow_mode },
	{ "background", action_radio, "s", "'checkerboard'",
		imagewindow_background },

	{ "reset", imagewindow_reset },
};

static void
imagewindow_properties_leave(GtkEventControllerFocus *self,
	gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	gboolean revealed;

	g_object_get(win->properties,
		"revealed", &revealed,
		NULL);

	// if the props pane had the focus and it's being hidden, we must refocus
	if (!revealed)
		gtk_widget_grab_focus(GTK_WIDGET(win->imageui));
}

static gboolean
imagewindow_dnd_drop(GtkDropTarget *target,
	const GValue *value, double x, double y, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	imagewindow_set_from_value(win, value);

	return TRUE;
}

static void
imagewindow_init(Imagewindow *win)
{
	GtkEventController *controller;

#ifdef DEBUG
	puts("imagewindow_init");
#endif /*DEBUG*/

	win->progress_timer = g_timer_new();
	win->settings = g_settings_new(APPLICATION_ID);

	gtk_widget_init_template(GTK_WIDGET(win));

	g_object_set(win->display_bar,
		"image-window", win,
		NULL);
	g_object_set(win->info_bar,
		"image-window", win,
		NULL);

	g_signal_connect_object(win->progress_cancel, "clicked",
		G_CALLBACK(imagewindow_cancel_clicked), win, 0);
	g_signal_connect_object(win->error_bar, "response",
		G_CALLBACK(imagewindow_error_response), win, 0);

	g_action_map_add_action_entries(G_ACTION_MAP(win),
		imagewindow_entries, G_N_ELEMENTS(imagewindow_entries),
		win);

	/* We need to know if the props pane has the focus so we can refocus on
	 * hide.
	 */
	controller = GTK_EVENT_CONTROLLER(gtk_event_controller_focus_new());
	g_signal_connect(controller, "leave",
		G_CALLBACK(imagewindow_properties_leave), win);
	gtk_widget_add_controller(win->properties, controller);

	g_settings_bind(win->settings, "control",
		G_OBJECT(win->display_bar),
		"revealed",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind(win->settings, "info",
		G_OBJECT(win->info_bar),
		"revealed",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind(win->settings, "properties",
		G_OBJECT(win->properties),
		"revealed",
		G_SETTINGS_BIND_DEFAULT);

	/* We are a drop target for filenames and images.
	 */
	controller = GTK_EVENT_CONTROLLER(
		gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY));
	gtk_drop_target_set_gtypes(GTK_DROP_TARGET(controller),
		imagewindow_supported_types,
		imagewindow_n_supported_types);
	g_signal_connect(controller, "drop",
		G_CALLBACK(imagewindow_dnd_drop), win);
	gtk_widget_add_controller(win->main_box, controller);

	/* We can't be a drag source, we use drag for pan. Copy/paste images out
	 * instead.
	 */

	/* Menu state from settings.
	 */
	set_state(GTK_WIDGET(win), win->settings, "properties");
	set_state(GTK_WIDGET(win), win->settings, "control");
	set_state(GTK_WIDGET(win), win->settings, "info");
	set_state(GTK_WIDGET(win), win->settings, "background");

	// some kind of gtk bug? hexpand on properties can't be set from .ui or in
	// properties.c, but must be set after adding to a parent
	g_object_set(win->properties, "hexpand", FALSE, NULL);
}

static void
imagewindow_pressed_cb(GtkGestureClick *gesture,
	guint n_press, double x, double y, Imagewindow *win)
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
		Imagewindow, field);

static void
imagewindow_class_init(ImagewindowClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
		APP_PATH "/imagewindow.ui");

	BIND(right_click_menu);
	BIND(title);
	BIND(subtitle);
	BIND(gears);
	BIND(progress_bar);
	BIND(progress);
	BIND(progress_cancel);
	BIND(error_bar);
	BIND(error_label);
	BIND(main_box);
	BIND(stack);
	BIND(properties);
	BIND(display_bar);
	BIND(info_bar);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		imagewindow_pressed_cb);

	gobject_class->dispose = imagewindow_dispose;

	imagewindow_signals[SIG_STATUS_CHANGED] = g_signal_new(
		"status-changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	imagewindow_signals[SIG_CHANGED] = g_signal_new("changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	GType supported_types[] = {
		GDK_TYPE_FILE_LIST,
		G_TYPE_FILE,
		GDK_TYPE_TEXTURE,
		G_TYPE_STRING,
	};

	imagewindow_n_supported_types = VIPS_NUMBER(supported_types);
	imagewindow_supported_types =
		VIPS_ARRAY(NULL, imagewindow_n_supported_types + 1, GType);
	for (int i = 0; i < imagewindow_n_supported_types; i++)
		imagewindow_supported_types[i] = supported_types[i];
}

Imagewindow *
imagewindow_new(App *app)
{
	return g_object_new(IMAGEWINDOW_TYPE, "application", app, NULL);
}

GSettings *
imagewindow_get_settings(Imagewindow *win)
{
	return win->settings;
}

void
imagewindow_open_files(Imagewindow *win, char **files, int n_files)
{
#ifdef DEBUG
	printf("imagewindow_open_files:\n");
#endif /*DEBUG*/

	imagewindow_files_set(win, files, n_files);
	imagewindow_open_current_file(win, GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT);
}

void
imagewindow_open_list_gfiles(Imagewindow *win, GSList *gfiles)
{
#ifdef DEBUG
	printf("imagewindow_open_list_gfiles:\n");
#endif /*DEBUG*/

	imagewindow_files_set_list_gfiles(win, gfiles);
	imagewindow_open_current_file(win, GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT);
}

void
imagewindow_open_gfiles(Imagewindow *win, GFile **gfiles, int n_files)
{
#ifdef DEBUG
	printf("imagewindow_open_gfiles:\n");
#endif /*DEBUG*/

	g_auto(GStrv) files = VIPS_ARRAY(NULL, n_files + 1, char *);
	for (int i = 0; i < n_files; i++)
		files[i] = g_file_get_path(gfiles[i]);

	imagewindow_open_files(win, files, n_files);
}

void
imagewindow_open_image(Imagewindow *win, VipsImage *image)
{
#ifdef DEBUG
	printf("imagewindow_open_image:\n");
#endif /*DEBUG*/

	g_autoptr(Tilesource) tilesource = tilesource_new_from_image(image);
	if (!tilesource) {
		imagewindow_error(win);
		return;
	}

	Imageui *imageui = imageui_new(tilesource);
	if (!imageui) {
		imagewindow_error(win);
		return;
	}

	imagewindow_files_free(win);
	imagewindow_imageui_add(win, imageui);
	imagewindow_imageui_set_visible(win,
		imageui, GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN);
}

static void *
imagewindow_open_iimage_filename(const char *filename,
	void *a, void *b, void *c)
{
	Imagewindow *win = IMAGEWINDOW(a);

	imagewindow_open_files(win, &filename, 1);
}

static void
imagewindow_iimage_changed(iImage *iimage, Imagewindow *win)
{
#ifdef DEBUG
#endif /*DEBUG*/
	printf("imagewindow_iimage_changed:\n");

	imagewindow_open_iimage(win, iimage);
}

static void
imagewindow_iimage_destroy(iImage *iimage, Imagewindow *win)
{
#ifdef DEBUG
#endif /*DEBUG*/
	printf("imagewindow_iimage_destroy:\n");

	gtk_window_destroy(GTK_WINDOW(win));
}

void
imagewindow_open_iimage(Imagewindow *win, iImage *iimage)
{
	Imageinfo *ii = iimage->value.ii;

#ifdef DEBUG
	printf("imagewindow_open_iimage:\n");
#endif /*DEBUG*/

	if (ii) {
		if (imageinfo_is_from_file(ii))
			// we must expand $HOME etc.
			callv_string_filename(imagewindow_open_iimage_filename,
				IOBJECT(ii)->name, win, NULL, NULL);
		else
			imagewindow_open_image(win, imageinfo_get(FALSE, ii));

		// not a reference .. we watch for destroy
		win->iimage = iimage;

		FREESID(win->iimage_changed_sid, win->iimage);
		win->iimage_changed_sid = g_signal_connect(iimage, "changed",
			G_CALLBACK(imagewindow_iimage_changed), win);

		FREESID(win->iimage_destroy_sid, win->iimage);
		win->iimage_destroy_sid = g_signal_connect(iimage, "destroy",
			G_CALLBACK(imagewindow_iimage_destroy), win);
	}
}
