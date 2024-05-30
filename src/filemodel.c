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
		error_sub(_("_%s() not implemented for class \"%s\"."),
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

void
filemodel_set_window_hint(Filemodel *filemodel, GtkWindow *win)
{
	/* This can be called repeatedly if objects are moved between windows.
	 */
	filemodel->window_hint = win;
}

GtkWindow *
filemodel_get_window_hint(Filemodel *filemodel)
{
	return filemodel->window_hint;
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
		error_sub(_("_%s() not implemented for class \"%s\"."),
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
		char buf[FILENAME_MAX];

		/* We want to keep the absolute, compact form of the filename
		 * inside the object so we don't get a dependency on CWD.
		 */
		if (filename) {
			vips_strncpy(buf, filename, FILENAME_MAX);
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

#ifdef DEBUG
	printf("filemodel_finalize: %s \"%s\" (%s)\n",
		G_OBJECT_TYPE_NAME(filemodel),
		IOBJECT(filemodel)->name,
		filemodel->filename);
#endif /*DEBUG*/

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

#ifdef DEBUG
	printf("filemodel_dispose: %s \"%s\" (%s)\n",
		G_OBJECT_TYPE_NAME(filemodel),
		IOBJECT(filemodel)->name,
		filemodel->filename);
#endif /*DEBUG*/

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

	vips_snprintf(namespace, 256, "%s/%d.%d.%d",
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

	column_set_offset(filemodel->x_off, filemodel->y_off);
	if (model_save(MODEL(filemodel), xdoc->children)) {
		xmlFreeDoc(xdoc);
		return FALSE;
	}

	if (calli_string_filename(
			(calli_string_fn) filemodel_xml_save_format_file,
			filename, xdoc, NULL, NULL)) {
		error_top(_("Save failed"));
		error_sub(_("Save of %s \"%s\" to file \"%s\" failed.\n%s"),
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

	column_set_offset(filemodel->x_off, filemodel->y_off);
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
	filemodel->x_off = 0;
	filemodel->y_off = 0;

	/* Default version.
	 */
	filemodel->versioned = FALSE;
	filemodel->major = MAJOR_VERSION;
	filemodel->minor = MINOR_VERSION;
	filemodel->micro = MICRO_VERSION;

	filemodel->window_hint = NULL;
}

void
filemodel_set_offset(Filemodel *filemodel, int x_off, int y_off)
{
#ifdef DEBUG
	printf("filemodel_set_offset: %s \"%s\" %d x %d\n",
		G_OBJECT_TYPE_NAME(filemodel),
		IOBJECT(filemodel)->name,
		x_off, y_off);
#endif /*DEBUG*/

	filemodel->x_off = x_off;
	filemodel->y_off = y_off;
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
		error_sub(_("Can't load XML file \"%s\", it's not a %s save file."),
			state->filename, PACKAGE);
		return FALSE;
	}
	if (sscanf((char *) xnode->nsDef->href + strlen(NAMESPACE) + 1,
			"%d.%d.%d",
			&state->major, &state->minor, &state->micro) != 3) {
		error_top(_("Load failed"));
		error_sub(_("Can't load XML file \"%s\", "
					"unable to extract version information from namespace."),
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
		error_sub(_("_%s() not implemented for class \"%s\"."), "load", tname);
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
		error_sub(_("_%s() not implemented for class \"%s\"."), "load", tname);
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
		char buf[FILENAME_MAX];

		g_assert(p);

		vips_snprintf(buf, FILENAME_MAX, "$SAVEDIR/start/%s", p + 1);
		filemodel_set_filename(filemodel, buf);
	}
}
