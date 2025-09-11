/* abstract base class for things which are loaded or saved from files
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

#define FILEMODEL_LOAD_STATE(obj) ((FilemodelLoadState *) obj)

#define FILEMODEL_TYPE (filemodel_get_type())
#define FILEMODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), FILEMODEL_TYPE, Filemodel))
#define FILEMODEL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), FILEMODEL_TYPE, FilemodelClass))
#define IS_FILEMODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), FILEMODEL_TYPE))
#define IS_FILEMODEL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), FILEMODEL_TYPE))
#define FILEMODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), FILEMODEL_TYPE, FilemodelClass))

struct _Filemodel {
	Model model;

	char *filename;		 /* File we read this thing from */
	gboolean modified;	 /* Set if modified (and should be saved) */
	gboolean registered; /* Set if on list of things to save on quit */
	gboolean auto_load;	 /* TRUE if loaded from path_start */

	/* When we loaded this filemodel, the version numbers we saw in the
	 * XML file.
	 */
	gboolean versioned; /* Set means from a versioned file */
	int major;
	int minor;
	int micro;

	/* Where this filemodel would like dialogs displayed.
	 */
	GtkWindow *window_hint;
};

typedef struct _FilemodelClass {
	ModelClass parent_class;

	/*

		top_load			top level load function ... controls how the
							rest of the load happens ... eg. merge,
							rename, etc.

		set_modified		set/clear the modified state

		top_save			top level save ... intercept this to override

		filter_new			make a new filemodel filter for this file type

		suffix				if non-NULL, the required suffix for files of
							this type

	 */

	gboolean (*top_load)(Filemodel *filemodel,
		ModelLoadState *state, Model *parent, xmlNode *xnode);
	void (*set_modified)(Filemodel *filemodel, gboolean modified);
	gboolean (*top_save)(Filemodel *filemodel, const char *filename);
	GtkFileFilter *(*filter_new)(Filemodel *filemodel);
	const char *suffix;

	/* The current save and load directories for objects of this type.
	 */
	GFile *save_folder;
	GFile *load_folder;

} FilemodelClass;

void filemodel_register(Filemodel *filemodel);
void filemodel_unregister(Filemodel *filemodel);

void *filemodel_top_load(Filemodel *filemodel,
	ModelLoadState *state, Model *parent, xmlNode *xnode);

void filemodel_set_filename(Filemodel *filemodel, const char *filename);
void filemodel_set_modified(Filemodel *filemodel, gboolean state);

GType filemodel_get_type(void);

gboolean filemodel_top_save(Filemodel *filemodel, const char *filename);
gboolean filemodel_load_all(Filemodel *filemodel, Model *parent,
	const char *filename, const char *filename_user);
gboolean filemodel_load_all_openfile(Filemodel *filemodel,
	Model *parent, iOpenFile *of);

void filemodel_set_auto_load(Filemodel *filemodel);

void filemodel_set_window_hint(Filemodel *filemodel, GtkWindow *window);
GtkWindow *filemodel_get_window_hint(Filemodel *filemodel);

typedef void (*FilemodelSaveasResult)(GtkWindow *window,
	Filemodel *filemodel, void *a, void *b);
void filemodel_saveas(GtkWindow *window, Filemodel *filemodel,
	FilemodelSaveasResult next,
	FilemodelSaveasResult error, void *a, void *b);
void filemodel_save_before_close(Filemodel *filemodel,
	FilemodelSaveasResult next,
	FilemodelSaveasResult error, void *a, void *b);
void filemodel_save(GtkWindow *window, Filemodel *filemodel,
	FilemodelSaveasResult next,
	FilemodelSaveasResult error, void *a, void *b);

void filemodel_open(GtkWindow *window, Filemodel *filemodel,
	const char *verb,
	FilemodelSaveasResult next,
	FilemodelSaveasResult error, void *a, void *b);
void filemodel_replace(GtkWindow *window, Filemodel *filemodel,
	const char *verb,
	FilemodelSaveasResult next,
	FilemodelSaveasResult error, void *a, void *b);

void filemodel_close_registered(SListMapFn callback, void *user_data);
