/* abstract base class for things which form the filemodel half of a
 * filemodel/view pair
 */

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
#define DEBUG_VERBOSE
#define DEBUG
 */

/* Don't compress save files.

	FIXME ... some prebuilt libxml2s on win32 don't support libz
	compression, so don't turn this off

 */
#define DEBUG_SAVEFILE

#include "nip4.h"

G_DEFINE_TYPE(Filemodel, filemodel, MODEL_TYPE)

static GSList *filemodel_registered = NULL;

/* Register a file model. Registered models are part of the "xxx has been
 * modified, save before quit?" check.
 */
void
filemodel_register(Filemodel *filemodel)
{
	if (!filemodel->registered) {
		filemodel->registered = TRUE;
		filemodel_registered = g_slist_prepend(filemodel_registered,
			filemodel);

#ifdef DEBUG
		printf("filemodel_register: %s \"%s\" (%p)\n",
			G_OBJECT_TYPE_NAME(filemodel),
			IOBJECT(filemodel)->name,
			filemodel);
#endif /*DEBUG*/
	}
}

void
filemodel_unregister(Filemodel *filemodel)
{
	if (filemodel->registered) {
		filemodel->registered = FALSE;
		filemodel_registered = g_slist_remove(filemodel_registered, filemodel);

#ifdef DEBUG
		printf("filemodel_unregister: %s \"%s\" (%p)\n",
			G_OBJECT_TYPE_NAME(filemodel),
			IOBJECT(filemodel)->name,
			filemodel);
#endif /*DEBUG*/
	}
}

/* Trigger the top_load method for a filemodel.
 */
void *
filemodel_top_load(Filemodel *filemodel,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	FilemodelClass *filemodel_class = FILEMODEL_GET_CLASS(filemodel);

	if (filemodel_class->top_load) {
		if (!filemodel_class->top_load(filemodel, state, parent, xnode))
			return filemodel;
	}
	else {
		error_top(_("Not implemented"));
		error_sub(_("_%s() not implemented for class \"%s\""),
			"top_load",
			G_OBJECT_CLASS_NAME(filemodel_class));

		return filemodel;
	}

	return NULL;
}

/* Trigger the set_modified method for a filemodel.
 */
void
filemodel_set_modified(Filemodel *filemodel, gboolean modified)
{
	FilemodelClass *filemodel_class = FILEMODEL_GET_CLASS(filemodel);

	/*
	printf("filemodel_set_modified: %s, %d\n",
		IOBJECT(filemodel)->name, modified);
	 */

	if (filemodel_class->set_modified)
		filemodel_class->set_modified(filemodel, modified);
}

gboolean
filemodel_top_save(Filemodel *filemodel, const char *filename)
{
	FilemodelClass *filemodel_class = FILEMODEL_GET_CLASS(filemodel);

	if (filemodel_class->top_save) {
		char *old_filename;
		int result;

		/* We must always have the new filename in the save file or
		 * auto path rewriting will get confused on reload.
		 *
		 * Equally, we must not change the filename on the model, in
		 * case this save is not something initiated by the user, for
		 * example, an auto-backup of the workspace.
		 *
		 * Save and restore the filename. Our caller must set the
		 * final filename, if required (after save-as, for example).
		 */
		old_filename = g_strdup(filemodel->filename);
		filemodel_set_filename(filemodel, filename);

		result = filemodel_class->top_save(filemodel, filename);

		filemodel_set_filename(filemodel, old_filename);
		g_free(old_filename);

		return result;
	}
	else {
		error_top(_("Not implemented"));
		error_sub(_("_%s() not implemented for class \"%s\""),
			"top_save",
			G_OBJECT_CLASS_NAME(filemodel_class));

		return FALSE;
	}
}

static void
filemodel_info(iObject *iobject, VipsBuf *buf)
{
	Filemodel *filemodel = FILEMODEL(iobject);

	IOBJECT_CLASS(filemodel_parent_class)->info(iobject, buf);

	vips_buf_appendf(buf, "filename = \"%s\"\n",
		filemodel->filename);
	vips_buf_appendf(buf, "modified = \"%s\"\n",
		bool_to_char(filemodel->modified));
	vips_buf_appendf(buf, "registered = \"%s\"\n",
		bool_to_char(filemodel->registered));
	vips_buf_appendf(buf, "auto_load = \"%s\"\n",
		bool_to_char(filemodel->auto_load));
}

/* filename can be NULL for unset.
 */
void
filemodel_set_filename(Filemodel *filemodel, const char *filename)
{
	if (filemodel->filename != filename) {
		char buf[VIPS_PATH_MAX];

		/* We want to keep the absolute, compact form of the filename
		 * inside the object so we don't get a dependency on CWD.
		 */
		if (filename) {
			g_strlcpy(buf, filename, VIPS_PATH_MAX);
			path_compact(buf);
			filename = buf;
		}

		VIPS_SETSTR(filemodel->filename, filename);
		iobject_changed(IOBJECT(filemodel));
	}
}

static void
filemodel_finalize(GObject *gobject)
{
	Filemodel *filemodel;

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_FILEMODEL(gobject));

	filemodel = FILEMODEL(gobject);

#ifdef DEBUG_VERBOSE
	printf("filemodel_finalize: %s \"%s\" (%s)\n",
		G_OBJECT_TYPE_NAME(filemodel),
		IOBJECT(filemodel)->name,
		filemodel->filename);
#endif /*DEBUG_VERBOSE*/

	VIPS_FREE(filemodel->filename);

	G_OBJECT_CLASS(filemodel_parent_class)->finalize(gobject);
}

static void
filemodel_dispose(GObject *gobject)
{
	Filemodel *filemodel;

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_FILEMODEL(gobject));

	filemodel = FILEMODEL(gobject);

#ifdef DEBUG_VERBOSE
	printf("filemodel_dispose: %s \"%s\" (%s)\n",
		G_OBJECT_TYPE_NAME(filemodel),
		IOBJECT(filemodel)->name,
		filemodel->filename);
#endif /*DEBUG_VERBOSE*/

	filemodel_unregister(filemodel);

	G_OBJECT_CLASS(filemodel_parent_class)->dispose(gobject);
}

static xmlNode *
filemodel_save(Model *model, xmlNode *xnode)
{
	Filemodel *filemodel = FILEMODEL(model);
	xmlNode *xthis;

	if (!(xthis = MODEL_CLASS(filemodel_parent_class)->save(model, xnode)))
		return NULL;

	if (!set_sprop(xthis, "filename", filemodel->filename))
		return NULL;

	return xthis;
}

static gboolean
filemodel_load(Model *model,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	Filemodel *filemodel = FILEMODEL(model);

	char buf[MAX_STRSIZE];

	if (get_sprop(xnode, "filename", buf, MAX_STRSIZE))
		filemodel_set_filename(filemodel, buf);

	if (!MODEL_CLASS(filemodel_parent_class)->load(model, state, parent, xnode))
		return FALSE;

	return TRUE;
}

static gboolean
filemodel_real_top_load(Filemodel *filemodel,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	return TRUE;
}

static void
filemodel_real_set_modified(Filemodel *filemodel, gboolean modified)
{
	if (filemodel->modified != modified) {
#ifdef DEBUG
		printf("filemodel_real_set_modified: %s \"%s\" (%s) %s\n",
			G_OBJECT_TYPE_NAME(filemodel),
			IOBJECT(filemodel)->name,
			filemodel->filename,
			bool_to_char(modified));
#endif /*DEBUG*/

		filemodel->modified = modified;

		iobject_changed(IOBJECT(filemodel));
	}
}

static int
filemodel_xml_save_format_file(const char *filename, xmlDoc *doc)
{
	return xmlSaveFormatFile(filename, doc, 1) == -1;
}

/* Save to filemodel->filename.
 */
static gboolean
filemodel_top_save_xml(Filemodel *filemodel, const char *filename)
{
	xmlDoc *xdoc;
	char namespace[256];

	if (!(xdoc = xmlNewDoc((xmlChar *) "1.0"))) {
		error_top(_("XML library error"));
		error_sub(_("model_save_filename: xmlNewDoc() failed"));
		return FALSE;
	}

#ifndef DEBUG_SAVEFILE
	xmlSetDocCompressMode(xdoc, 1);
#endif /*!DEBUG_SAVEFILE*/

	g_snprintf(namespace, 256, "%s/%d.%d.%d",
		NAMESPACE,
		filemodel->major, filemodel->minor, filemodel->micro);
	if (!(xdoc->children = xmlNewDocNode(xdoc,
			  NULL, (xmlChar *) "root", NULL)) ||
		!set_sprop(xdoc->children, "xmlns", namespace)) {
		error_top(_("XML library error"));
		error_sub(_("model_save_filename: xmlNewDocNode() failed"));
		xmlFreeDoc(xdoc);
		return FALSE;
	}

	if (model_save(MODEL(filemodel), xdoc->children)) {
		xmlFreeDoc(xdoc);
		return FALSE;
	}

	if (calli_string_filename(
			(calli_string_fn) filemodel_xml_save_format_file,
			filename, xdoc, NULL, NULL)) {
		error_top(_("Save failed"));
		error_sub(_("save of %s \"%s\" to file \"%s\" failed\n%s"),
			IOBJECT_GET_CLASS_NAME(filemodel),
			IOBJECT(filemodel)->name,
			filename,
			g_strerror(errno));
		xmlFreeDoc(xdoc);

		return FALSE;
	}

	xmlFreeDoc(xdoc);

	return TRUE;
}

static gboolean
filemodel_top_save_text(Filemodel *filemodel, const char *filename)
{
	iOpenFile *of;

	if (!(of = ifile_open_write("%s", filename)))
		return FALSE;

	if (model_save_text(MODEL(filemodel), of)) {
		ifile_close(of);
		return FALSE;
	}
	ifile_close(of);

	return TRUE;
}

static gboolean
filemodel_real_top_save(Filemodel *filemodel, const char *filename)
{
	ModelClass *model_class = MODEL_GET_CLASS(filemodel);

#ifdef DEBUG
	printf("filemodel_real_top_save: save %s \"%s\" to file \"%s\"\n",
		G_OBJECT_TYPE_NAME(filemodel),
		IOBJECT(filemodel)->name,
		filename);
#endif /*DEBUG*/

	if (model_class->save_text) {
		if (!filemodel_top_save_text(filemodel, filename))
			return FALSE;
	}
	else if (model_class->save) {
		if (!filemodel_top_save_xml(filemodel, filename))
			return FALSE;
	}
	else {
		error_top(_("Not implemented"));
		error_sub(_("filemodel_real_top_save: no save method"));
		return FALSE;
	}

	return TRUE;
}

static void
filemodel_class_init(FilemodelClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	iObjectClass *iobject_class = IOBJECT_CLASS(class);
	ModelClass *model_class = (ModelClass *) class;

	gobject_class->finalize = filemodel_finalize;
	gobject_class->dispose = filemodel_dispose;

	iobject_class->info = filemodel_info;

	model_class->save = filemodel_save;
	model_class->load = filemodel_load;

	class->top_load = filemodel_real_top_load;
	class->set_modified = filemodel_real_set_modified;
	class->top_save = filemodel_real_top_save;
}

static void
filemodel_init(Filemodel *filemodel)
{
	/* Init our instance fields.
	 */
	filemodel->filename = NULL;
	filemodel->modified = FALSE;
	filemodel->registered = FALSE;
	filemodel->auto_load = FALSE;

	/* Default version.
	 */
	filemodel->versioned = FALSE;
	filemodel->major = MAJOR_VERSION;
	filemodel->minor = MINOR_VERSION;
	filemodel->micro = MICRO_VERSION;
}

static gboolean
filemodel_load_all_xml(Filemodel *filemodel,
	Model *parent, ModelLoadState *state)
{
	xmlNode *xnode;

	/* Check the root element for type/version compatibility.
	 */
	if (!(xnode = xmlDocGetRootElement(state->xdoc)) ||
		!xnode->nsDef ||
		!is_prefix(NAMESPACE, (char *) xnode->nsDef->href)) {
		error_top(_("Load failed"));
		error_sub(_("can't load XML file \"%s\", it's not a %s save file"),
			state->filename, PACKAGE);
		return FALSE;
	}
	if (sscanf((char *) xnode->nsDef->href + strlen(NAMESPACE) + 1, "%d.%d.%d",
			&state->major, &state->minor, &state->micro) != 3) {
		error_top(_("Load failed"));
		error_sub(_("can't load XML file \"%s\", "
					"unable to extract version information from namespace"),
			state->filename);
		return FALSE;
	}

#ifdef DEBUG
	printf("filemodel_load_all_xml: major = %d, minor = %d, micro = %d\n",
		state->major, state->minor, state->micro);
#endif /*DEBUG*/

	if (filemodel_top_load(filemodel, state, parent, xnode))
		return FALSE;

	return TRUE;
}

static gboolean
filemodel_load_all_xml_file(Filemodel *filemodel, Model *parent,
	const char *filename, const char *filename_user)
{
	ModelLoadState *state;

	if (!(state = model_loadstate_new(filename, filename_user)))
		return FALSE;
	if (!filemodel_load_all_xml(filemodel, parent, state)) {
		model_loadstate_destroy(state);
		return FALSE;
	}
	model_loadstate_destroy(state);

	return TRUE;
}

static gboolean
filemodel_load_all_xml_openfile(Filemodel *filemodel,
	Model *parent, iOpenFile *of)
{
	ModelLoadState *state;

	if (!(state = model_loadstate_new_openfile(of)))
		return FALSE;
	if (!filemodel_load_all_xml(filemodel, parent, state)) {
		model_loadstate_destroy(state);
		return FALSE;
	}
	model_loadstate_destroy(state);

	return TRUE;
}

static gboolean
filemodel_load_all_text(Filemodel *filemodel, Model *parent,
	const char *filename, const char *filename_user)
{
	iOpenFile *of;

	if (!(of = ifile_open_read("%s", filename)))
		return FALSE;

	if (model_load_text(MODEL(filemodel), parent, of)) {
		ifile_close(of);
		return FALSE;
	}
	ifile_close(of);

	return TRUE;
}

/* Load filename into filemodel ... can mean merge as well as init.
 *
 * We load from @filename. If @filename_user is non-NULL, that's the filename
 * we should record in the model.
 */
gboolean
filemodel_load_all(Filemodel *filemodel, Model *parent,
	const char *filename, const char *filename_user)
{
	ModelClass *model_class = MODEL_GET_CLASS(filemodel);
	const char *tname = G_OBJECT_CLASS_NAME(model_class);

#ifdef DEBUG
	printf("filemodel_load_all: load file \"%s\" into parent %s \"%s\"\n",
		filename,
		G_OBJECT_TYPE_NAME(parent),
		IOBJECT(parent)->name);
#endif /*DEBUG*/

	if (model_class->load_text) {
		if (!filemodel_load_all_text(filemodel, parent,
				filename, filename_user))
			return FALSE;
	}
	else if (model_class->load) {
		if (!filemodel_load_all_xml_file(filemodel, parent,
				filename, filename_user))
			return FALSE;
	}
	else {
		error_top(_("Not implemented"));
		error_sub(_("_%s() not implemented for class \"%s\""), "load", tname);
		return FALSE;
	}

	/* Don't recomp here, we may be loading a bunch of interdependent
	 * files.
	 */

	return TRUE;
}

/* Load iOpenFile into filemodel ... can mean merge as well as init.
 */
gboolean
filemodel_load_all_openfile(Filemodel *filemodel, Model *parent, iOpenFile *of)
{
	ModelClass *model_class = MODEL_GET_CLASS(filemodel);
	const char *tname = G_OBJECT_CLASS_NAME(model_class);

#ifdef DEBUG
	printf("filemodel_load_all_openfile: load \"%s\" "
		   "into parent %s \"%s\"\n",
		of->fname,
		G_OBJECT_TYPE_NAME(parent),
		IOBJECT(parent)->name);
#endif /*DEBUG*/

	if (model_class->load_text) {
		if (model_load_text(MODEL(filemodel), parent, of))
			return FALSE;
	}
	else if (model_class->load) {
		if (!filemodel_load_all_xml_openfile(filemodel, parent, of))
			return FALSE;
	}
	else {
		error_top(_("Not implemented"));
		error_sub(_("_%s() not implemented for class \"%s\""), "load", tname);
		return FALSE;
	}

	/* Don't recomp here, we may be loading a bunch of interdependent
	 * files.
	 */

	return TRUE;
}

/* Mark something as having been loaded (or made) during startup. If we loaded
 * from one of the system areas, zap the filename so that we will save to the
 * user's area on changes.
 */
void
filemodel_set_auto_load(Filemodel *filemodel)
{
	filemodel->auto_load = TRUE;

	/*

		FIXME ... not very futureproof

	 */
	if (filemodel->filename &&
		strstr(filemodel->filename, "share/" PACKAGE)) {
		char *p = strrchr(filemodel->filename, '/');
		char buf[VIPS_PATH_MAX];

		g_assert(p);

		g_snprintf(buf, VIPS_PATH_MAX, "$SAVEDIR/start/%s", p + 1);
		filemodel_set_filename(filemodel, buf);
	}
}

void
filemodel_set_window_hint(Filemodel *filemodel, GtkWindow *window)
{
	/* This can be called repeatedly if objects are moved between windows.
	 */
	filemodel->window_hint = window;
}

GtkWindow *
filemodel_get_window_hint(Filemodel *filemodel)
{
	if (filemodel->window_hint)
		return filemodel->window_hint;
	else
		return GTK_WINDOW(mainwindow_pick_one());
}

static void
filemodel_saveas_sub(GObject *source_object,
	GAsyncResult *res, void *data)
{
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
	GtkWindow *window = g_object_get_data(G_OBJECT(dialog), "nip4-window");
	Filemodel *filemodel =
		g_object_get_data(G_OBJECT(dialog), "nip4-filemodel");
	FilemodelSaveasResult next =
		g_object_get_data(G_OBJECT(dialog), "nip4-next");
	FilemodelSaveasResult error =
		g_object_get_data(G_OBJECT(dialog), "nip4-error");
	void *a = g_object_get_data(G_OBJECT(dialog), "nip4-a");
	void *b = g_object_get_data(G_OBJECT(dialog), "nip4-b");

	g_autoptr(GFile) file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		g_autofree char *filename = g_file_get_path(file);

		if (!filemodel_top_save(filemodel, filename))
			error(window, filemodel, a, b);
		else {
			filemodel_set_filename(filemodel, filename);
			filemodel_set_modified(filemodel, FALSE);

			if (next)
				next(window, filemodel, a, b);
		}
	}
}

void
filemodel_saveas(GtkWindow *window, Filemodel *filemodel,
	FilemodelSaveasResult next,
	FilemodelSaveasResult error, void *a, void *b)
{
	const char *tname = IOBJECT_GET_CLASS_NAME(filemodel);
	g_autofree char *title = g_strdup_printf("Save %s as", tname);

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, title);
	gtk_file_dialog_set_modal(dialog, TRUE);
	g_object_set_data(G_OBJECT(dialog), "nip4-window", window);
	g_object_set_data(G_OBJECT(dialog), "nip4-filemodel", filemodel);
	g_object_set_data(G_OBJECT(dialog), "nip4-next", next);
	g_object_set_data(G_OBJECT(dialog), "nip4-error", error);
	g_object_set_data(G_OBJECT(dialog), "nip4-a", a);
	g_object_set_data(G_OBJECT(dialog), "nip4-b", b);

	if (filemodel->filename) {
		char filename[VIPS_PATH_MAX];

		expand_variables(filemodel->filename, filename);
		g_autoptr(GFile) file = g_file_new_for_path(filename);
		if (file)
			gtk_file_dialog_set_initial_file(dialog, file);
	}

	gtk_file_dialog_save(dialog, window, NULL,
		filemodel_saveas_sub, NULL);
}

static void
filemodel_save_before_close_cb(GObject *source_object,
	GAsyncResult *result, void *data)
{
	GtkAlertDialog *alert = GTK_ALERT_DIALOG(source_object);
	GtkWindow *window = g_object_get_data(G_OBJECT(alert), "nip4-window");
	Filemodel *filemodel = g_object_get_data(G_OBJECT(alert), "nip4-filemodel");
	FilemodelSaveasResult next =
		g_object_get_data(G_OBJECT(alert), "nip4-next");
	FilemodelSaveasResult error =
		g_object_get_data(G_OBJECT(alert), "nip4-error");
	void *a = g_object_get_data(G_OBJECT(alert), "nip4-a");
	void *b = g_object_get_data(G_OBJECT(alert), "nip4-b");
	int choice = gtk_alert_dialog_choose_finish(alert, result, NULL);

	switch (choice) {
	case 0:
		// close without saving ... tag as unmodified, and move on
		// "next" is responsible for doing the gtk_window_close()
		filemodel_set_modified(filemodel, FALSE);
		next(window, filemodel, a, b);
		break;

	case 2:
		// save then move on
		filemodel_saveas(window, filemodel, next, error, a, b);
		break;

	default:
		// cancel whole process
		break;
	}
}

static void
filemodel_save_before_close_error(GtkWindow *window,
	Filemodel *filemodel, void *a, void *b)
{
	Mainwindow *main = MAINWINDOW(window);

	mainwindow_error(main);
}

void
filemodel_save_before_close(Filemodel *filemodel,
	FilemodelSaveasResult next, void *a, void *b)
{
	GtkWindow *window = filemodel_get_window_hint(filemodel);
	const char *tname = IOBJECT_GET_CLASS_NAME(filemodel);

	g_autofree char *message = g_strdup_printf("%s has been modified", tname);
	g_autofree char *detail = NULL;
	if (filemodel->filename)
		detail = g_strdup_printf("%s has been modified "
								 "since it was loaded from \"%s\".\n\n"
								 "Do you want to save your changes?",
			tname, filemodel->filename);
	else
		detail = g_strdup_printf("%s has been modified. "
								 "Do you want to save your changes?",
			tname);

	const char *labels[] = { "Close without saving", "Cancel", "Save", NULL };

	GtkAlertDialog *alert = gtk_alert_dialog_new("%s", message);
	gtk_alert_dialog_set_detail(alert, detail);
	gtk_alert_dialog_set_buttons(alert, labels);
	gtk_alert_dialog_set_modal(alert, TRUE);
	g_object_set_data(G_OBJECT(alert), "nip4-window", window);
	g_object_set_data(G_OBJECT(alert), "nip4-filemodel", filemodel);
	g_object_set_data(G_OBJECT(alert), "nip4-next", next);
	g_object_set_data(G_OBJECT(alert), "nip4-error",
		filemodel_save_before_close_error);
	g_object_set_data(G_OBJECT(alert), "nip4-a", a);
	g_object_set_data(G_OBJECT(alert), "nip4-b", b);

	gtk_alert_dialog_choose(alert, window, NULL,
		filemodel_save_before_close_cb, NULL);
}

static Filemodel *
filemodel_get_registered(void)
{
	for (GSList *p = filemodel_registered; p; p = p->next) {
		Filemodel *filemodel = FILEMODEL(p->data);

		if (filemodel->modified)
			return filemodel;
	}

	return NULL;
}

static gboolean
filemodel_close_registered_next_reply_idle(void *user_data)
{
	gtk_window_close(GTK_WINDOW(user_data));

	return FALSE;
}

static void
filemodel_close_registered_next(GtkWindow *window,
	Filemodel *filemodel, void *a, void *b);

static void
filemodel_close_registered_next_reply(GtkWindow *window,
	Filemodel *filemodel, void *a, void *b)
{
	/* We can't close immediately, the alert is still being shown
	 * and must detach. Close the window back in idle.
	 */
	g_idle_add(filemodel_close_registered_next_reply_idle, window);

	filemodel_close_registered_next(NULL, NULL, a, b);
}

static void
filemodel_close_registered_next(GtkWindow *window,
	Filemodel *filemodel, void *a, void *b)
{
	SListMapFn callback = (SListMapFn) a;

	if ((filemodel = filemodel_get_registered()))
		filemodel_save_before_close(filemodel,
			filemodel_close_registered_next_reply, callback, b);
	else if (callback)
		callback(b, NULL);
}

void
filemodel_close_registered(SListMapFn callback, void *user_data)
{
	filemodel_close_registered_next(NULL, NULL, callback, user_data);
}
