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
 */
#define DEBUG

#include "nip4.h"

// a .ws file we found in the recover area
typedef struct _Recoverfile {
	GObject parent_instance;

	char *filename;

	// read from the file stats
	char *date;
	char *time;
	guint64 size;

	// parsed from the filename, see workspacegroup_checkmark_timeout()
	char *name;
	char *pid;

} Recoverfile;

typedef struct _RecoverfileClass {
	GObjectClass parent_class;

} RecoverfileClass;

G_DEFINE_TYPE(Recoverfile, recoverfile, G_TYPE_OBJECT);

#define RECOVERFILE_TYPE (recoverfile_get_type())
#define RECOVERFILE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), RECOVERFILE_TYPE, Recoverfile))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Recoverfile, g_object_unref)

enum {
	PROP_NAME = 1,
	PROP_DATE,
	PROP_TIME,
	PROP_SIZE,
	PROP_PID,
};

static void
recoverfile_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Recoverfile *file = RECOVERFILE(object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string(value, file->name);
		break;

	case PROP_DATE:
		g_value_set_string(value, file->date);
		break;

	case PROP_TIME:
		g_value_set_string(value, file->time);
		break;

	case PROP_SIZE:
		g_value_set_uint64(value, file->size);
		break;

	case PROP_PID:
		g_value_set_string(value, file->pid);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
recoverfile_class_init(RecoverfileClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->get_property = recoverfile_get_property;

	g_object_class_install_property(gobject_class, PROP_NAME,
		g_param_spec_string("name",
			_("Name"),
			_("Workspace name"),
			NULL,
			G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, PROP_DATE,
		g_param_spec_string("date",
			_("Date"),
			_("Last modification date"),
			NULL,
			G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, PROP_TIME,
		g_param_spec_string("time",
			_("Time"),
			_("Last modification time"),
			NULL,
			G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, PROP_SIZE,
		g_param_spec_uint64("size",
			_("Size"),
			_("Size in bytes"),
			0, 1024 * 1024 * 1024, 0,
			G_PARAM_READABLE));

	// as taken from the filename
	g_object_class_install_property(gobject_class, PROP_PID,
		g_param_spec_string("pid",
			_("PID"),
			_("Process ID"),
			NULL,
			G_PARAM_READABLE));

}

static void
recoverfile_init(Recoverfile *file)
{
}

#ifdef DEBUG
static void
recoverfile_print(Recoverfile *file)
{
	printf("Recoverfile: %s\n", file->filename);
	printf("\tname = %s\n", file->name);
	printf("\tdate = %s\n", file->date);
	printf("\ttime = %s\n", file->time);
	printf("\tsize = %" G_GUINT64_FORMAT "\n", file->size);
	printf("\tpid = %s\n", file->pid);
}
#endif /*DEBUG*/

static Recoverfile *
recoverfile_new(const char *filename)
{
#ifdef DEBUG
	printf("recoverfile_new: %s\n", filename);
#endif /*DEBUG*/

	g_autoptr(Recoverfile) file = g_object_new(RECOVERFILE_TYPE, NULL);

	char buf[FILENAME_MAX];
	expand_variables(filename, buf);
	file->filename = g_strdup(buf);

	struct stat st;
	if (stat(file->filename, &st) == -1)
		return NULL;
	file->size = st.st_size;

	// use last modified
	g_autoptr(GDateTime) datetime =
		g_date_time_new_from_unix_local(st.st_mtime);

	file->date = g_strdup_printf("%04d/%02d/%02d",
		g_date_time_get_year(datetime),
		g_date_time_get_month(datetime),
		g_date_time_get_day_of_month(datetime));

	file->time = g_strdup_printf("%02d:%02d.%02d",
		g_date_time_get_hour(datetime),
		g_date_time_get_minute(datetime),
		g_date_time_get_second(datetime));

	// tmp ws names are eg.
	//		/tmp/nip4-untitled-234324-2-XXXXXXX.ws
	// see temp_name()
	g_autofree char *basename = g_path_get_basename(file->filename);

	// back past "xxxxxx.ws"
	char *p;
	if (!(p = strrchr(basename, '-')))
		return NULL;
	*p = '\0';

	// back past the serial number
	if (!(p = strrchr(basename, '-')))
		return NULL;
	*p = '\0';

	// PID
	if (!(p = strrchr(basename, '-')))
		return NULL;
	file->pid = g_strdup(p + 1);
	*p = '\0';

	// ignore retain files created by this process
	GPid gpid;
	if (sscanf(file->pid, "%" G_PID_FORMAT, &gpid) != 1 ||
		get_gpid() == gpid)
		return NULL;

	// name
	if (!(p = strrchr(basename, '-')))
		return NULL;
	file->name = g_strdup(p + 1);

#ifdef DEBUG
	printf("\tbuilt:\n");
	recoverfile_print(file);
#endif /*DEBUG*/

	return g_steal_pointer(&file);
}

struct _Recover {
	GtkWindow parent_instance;

	GtkWidget *table;

	GtkSingleSelection *selection;
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

static void
recover_setup_item(GtkListItemFactory *factory,
	GtkListItem *item, void *user_data)
{
	const char *field = (const char *) user_data;
	GtkWidget *label = gtk_label_new("");

	gtk_widget_set_hexpand(label, TRUE);
	if (g_str_equal(field, "size"))
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	else
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	gtk_list_item_set_child(item, label);
}

static void
recover_bind_item(GtkListItemFactory *factory,
	GtkListItem *item, void *user_data)
{
	const char *field = (const char *) user_data;
	GtkWidget *label = gtk_list_item_get_child(item);
	Recoverfile *file = gtk_list_item_get_item(item);

	GValue value = { 0 };
	g_object_get_property(G_OBJECT(file), field, &value);

	switch (G_VALUE_TYPE(&value)) {
	case G_TYPE_STRING:
		gtk_label_set_label(GTK_LABEL(label), g_value_get_string(&value));
		break;

	case G_TYPE_UINT64:
		g_autofree char *str =
			g_strdup_printf("%" G_GUINT64_FORMAT, g_value_get_uint64(&value));
		gtk_label_set_label(GTK_LABEL(label), str);
		break;

	default:
		g_autofree char *txt = g_strdup_value_contents(&value);
		gtk_label_set_label(GTK_LABEL(label), txt);
		break;
	}

	g_value_unset(&value);
}

static void *
store_add_file(const char *filename, GListStore *store)
{
	Recoverfile *file = recoverfile_new(filename);
	if (file)
		g_list_store_append(store, G_OBJECT(file));

	return NULL;
}

static void
recover_init(Recover *recover)
{
	GtkListItemFactory *factory;
	GtkColumnViewColumn *column;

	gtk_widget_init_template(GTK_WIDGET(recover));

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "name");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "name");
	column = gtk_column_view_column_new("Name", factory);
	gtk_column_view_column_set_expand(GTK_COLUMN_VIEW_COLUMN(column), TRUE);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "date");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "date");
	column = gtk_column_view_column_new("Date", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "time");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "time");
	column = gtk_column_view_column_new("Time", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "size");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "size");
	column = gtk_column_view_column_new("Size", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "pid");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "pid");
	column = gtk_column_view_column_new("Process ID", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	GListModel *model = G_LIST_MODEL(g_list_store_new(RECOVERFILE_TYPE));
	(void) path_map_dir(PATH_TMP, "*.ws",
		(path_map_fn) store_add_file, model);

	GtkExpression *expression =
		gtk_property_expression_new(RECOVERFILE_TYPE, NULL, "size");
	GtkSorter *sorter = GTK_SORTER(gtk_numeric_sorter_new(expression));
	model = G_LIST_MODEL(gtk_sort_list_model_new(model, sorter));

	recover->selection = gtk_single_selection_new(model);

	gtk_column_view_set_model(GTK_COLUMN_VIEW(recover->table),
		GTK_SELECTION_MODEL(recover->selection));
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
