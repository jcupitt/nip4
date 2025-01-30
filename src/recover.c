/* show image save options
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
#define DEBUG
 */

#include "nip4.h"

// have a GArray of these things for the .ws files we discover in the tmp area
typedef struct {
	char filename[FILENAME_MAX];

	// parsed from the filename, see workspacegroup_checkmark_timeout()
	char name[FILENAME_MAX];
	GPid pid;

	// read from the file stats
	time_t time;
	uint64_t bytes;

} RecoverFile;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(RecoverFile, g_free)

static void
recover_file_add(const char *filename, GPtrArray *recover_files)
{
	g_autoptr(RecoverFile) file = g_new0(RecoverFile, 1);

	g_strlcpy(file->filename, filename, FILENAME_MAX);

	for (int i = 0; i < WS_RETAIN; i++)
		if (retain_files[i] &&
			g_str_equal(file->filename, retain_files[i]))
			return;

    struct stat st;
    if (stat(file->filename, &st) == -1)
        return;
	file->bytes = st.st_size;
	file->time = st.st_mtime;

	// tmp ws names are eg.
	//		/tmp/nip4-untitled-234324-2-XXXXXXX.ws
	g_autofree char *basename = g_path_get_basename(file->filename);
	if (sscanf(basename,

}



/* This file any better than the previous best candidate? Subfn of below.
 */
static void *
workspacegroup_test_file(const char *name, void *a, void *b, void *c)
{
	AutoRecover *recover = (AutoRecover *) a;

	char buf[FILENAME_MAX];
	time_t time;
	int i;

	g_strlcpy(buf, name, FILENAME_MAX);
	path_expand(buf);
	for (i = 0; i < WS_RETAIN; i++)
		if (retain_files[i] &&
			strcmp(buf, retain_files[i]) == 0)
			return NULL;
	if (!(time = mtime("%s", buf)))
		return NULL;
	if (recover->time > 0 && time < recover->time)
		return NULL;

	strcpy(recover->filename, buf);
	recover->time = time;

	return NULL;
}

/* Search for the most recent "*.ws" file
 * in the tmp area owned by us, with a size > 0, that's not in our
 * retain_files[] set.
 */
char *
workspacegroup_autosave_recover(void)
{
	AutoRecover recover;

	strcpy(recover.filename, "");
	recover.time = 0;
	(void) path_map_dir(PATH_TMP, "*.ws",
		(path_map_fn) workspacegroup_test_file, &recover);

	if (!recover.time)
		return NULL;

	return g_strdup(recover.filename);
}

struct _Recover {
	GtkWindow parent_instance;

	GtkWidget *table;

	GtkColumnViewColumn *date_column;
	GtkColumnViewColumn *time_column;
	GtkColumnViewColumn *size_column;
	GtkColumnViewColumn *id_column;

	GListModel *model;
};

struct _RecoverClass {
	GtkWindowClass parent_class;

};

G_DEFINE_TYPE(Recover, recover, GTK_TYPE_WINDOW);

static void
recover_dispose(GObject *object)
{
	Recover *recover = RECOVER(object);

	gtk_widget_dispose_template(GTK_WIDGET(recover), RECOVER_TYPE);

	G_OBJECT_CLASS(recover_parent_class)->dispose(object);
}

/*
static void
recover_setup_header_item(GtkListItemFactory *factory,
	GtkListItem *item, void *user_data)
{
	GtkWidget *label =

	gtk_list_item_set_child(item, label);
}

static void
recover_bind_row_item(GtkListItemFactory *factory,
	GtkListItem *item, void *user_data)
{
}

static GListModel *
recover_build_model(Recover *recover)
{
	GListStore *store = g_list_store_new(NODE_TYPE);

	// make "go to parent" the first item
		g_list_store_append(store, g_object);

	return G_LIST_MODEL(store);
}
 */

static void
recover_init(Recover *recover)
{
	GtkListItemFactory *factory;

	gtk_widget_init_template(GTK_WIDGET(recover));

	/*
	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_header_item), NULL);
	gtk_column_view_set_header_factory(GTK_COLUMN_VIEW(recover->table),
		factory);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_header_item), NULL);
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_row_item), NULL);
	gtk_column_view_set_row_factory(GTK_COLUMN_VIEW(recover->table),
		factory);

    recover->model = recover_build_model(recover);
    GtkSelectionModel *selection =
        GTK_SELECTION_MODEL(gtk_single_selection_new(recover->model));
	gtk_column_view_set_model(GTK_COLUMN_VIEW(recover->table),
		model);
		 */


	  GListModel *model = create_settings_model (NULL, NULL);
      GtkTreeListModel *treemodel = gtk_tree_list_model_new (model,
                                           FALSE,
                                           TRUE,
                                           create_settings_model,
                                           NULL,
                                           NULL);

      GtkSingleSelection *selection = gtk_single_selection_new (G_LIST_MODEL (treemodel));
      g_object_bind_property_full (selection, "selected-item",
                                   recover->table, "model",
                                   G_BINDING_SYNC_CREATE,
                                   transform_settings_to_keys,
                                   NULL,
                                   recover->table, NULL);


}

static void
recover_delete_clicked(GtkWidget *button, Recover *recover)
{
	printf("recover_delete_clicked:\n");
}

static void
recover_ok_clicked(GtkWidget *button, Recover *recover)
{
	printf("recover_ok_clicked:\n");
}

static void
recover_cancel_clicked(GtkWidget *button, Recover *recover)
{
	printf("recover_cancel_clicked:\n");
}

static void
recover_class_init(RecoverClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	gobject_class->dispose = recover_dispose;

	BIND_RESOURCE("recover.ui");

	BIND_VARIABLE(Recover, table);
	BIND_VARIABLE(Recover, date_column);
	BIND_VARIABLE(Recover, time_column);
	BIND_VARIABLE(Recover, size_column);
	BIND_VARIABLE(Recover, id_column);

	BIND_CALLBACK(recover_delete_clicked);
	BIND_CALLBACK(recover_ok_clicked);
	BIND_CALLBACK(recover_cancel_clicked);

}

Recover *
recover_new(GtkWindow *parent_window)
{
	Recover *recover;

	recover = g_object_new(RECOVER_TYPE,
		"transient-for", parent_window,
		NULL);

	return recover;
}
