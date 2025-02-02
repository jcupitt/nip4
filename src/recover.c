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
	GtkApplicationWindow parent_instance;

	Workspaceroot *wsr;

	GtkWidget *location;
	GtkWidget *table;
	GtkWidget *used;

	GtkSingleSelection *selection;
};

struct _RecoverClass {
	GtkApplicationWindowClass parent_class;

};

G_DEFINE_TYPE(Recover, recover, GTK_TYPE_APPLICATION_WINDOW);

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
	if (g_str_equal(field, "name"))
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	else
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);

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

	gtk_widget_set_tooltip_text(label, file->filename);
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
recover_refresh(Recover *recover)
{
	set_glabel(recover->location, "Save files in %s", PATH_TMP);

	GListModel *model = G_LIST_MODEL(g_list_store_new(RECOVERFILE_TYPE));
	(void) path_map_dir(PATH_TMP, "*.ws",
		(path_map_fn) store_add_file, model);

	// sort by PID, then date, then time
	GtkMultiSorter *multi = gtk_multi_sorter_new();
	GtkExpression *expression;
	GtkSorter *sorter;
	expression = gtk_property_expression_new(RECOVERFILE_TYPE, NULL, "pid");
	sorter = GTK_SORTER(gtk_string_sorter_new(expression));
	gtk_multi_sorter_append(multi, sorter);
	expression = gtk_property_expression_new(RECOVERFILE_TYPE, NULL, "date");
	sorter = GTK_SORTER(gtk_string_sorter_new(expression));
	gtk_multi_sorter_append(multi, sorter);
	expression = gtk_property_expression_new(RECOVERFILE_TYPE, NULL, "time");
	sorter = GTK_SORTER(gtk_string_sorter_new(expression));
	gtk_multi_sorter_append(multi, sorter);
	model = G_LIST_MODEL(gtk_sort_list_model_new(model, GTK_SORTER(multi)));

	recover->selection = gtk_single_selection_new(model);

	gtk_column_view_set_model(GTK_COLUMN_VIEW(recover->table),
		GTK_SELECTION_MODEL(recover->selection));

	char txt[256];
	VipsBuf msg = VIPS_BUF_STATIC(txt);
	vips_buf_appendf(&msg, "Total of ");
	vips_buf_append_size(&msg, directory_size(PATH_TMP));
	set_glabel(recover->used, "%s", vips_buf_all(&msg));
}

static void *
recover_delete_temp(const char *filename)
{
    unlinkf("%s", filename);

    return NULL;
}

static void
recover_delete_temps(void)
{
    /* Don't "rm *", too dangerous.
     */
    path_map_dir(PATH_TMP, "*.v",
        (path_map_fn) recover_delete_temp, NULL);
    path_map_dir(PATH_TMP, "*.ws",
        (path_map_fn) recover_delete_temp, NULL);

    /* _stdenv.def:magick can generate .tif files.
     */
    path_map_dir(PATH_TMP, "*.tif",
        (path_map_fn) recover_delete_temp, NULL);

    /* autotrace can make some others.
     */
    path_map_dir(PATH_TMP, "*.ppm",
        (path_map_fn) recover_delete_temp, NULL);
    path_map_dir(PATH_TMP, "*.svg",
        (path_map_fn) recover_delete_temp, NULL);
}

static void
recover_delete_temps_yesno(GtkWindow *parent, void *user_data)
{
	Recover *recover = RECOVER(user_data);

	recover_delete_temps();
	recover_refresh(recover);
}

static void
recover_delete_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Recover *recover = RECOVER(user_data);

	alert_yesno(GTK_WINDOW(recover), recover_delete_temps_yesno, recover,
		_("Wipe the nip4 temp area?"),
		"%s", _("This will remove all temporary files and backups, and "
		"cannot be undone."));
}

static void
recover_cancel_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Recover *recover = RECOVER(user_data);

	gtk_window_destroy(GTK_WINDOW(recover));
}

static void
recover_ok_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Recover *recover = RECOVER(user_data);
	GObject *item = gtk_single_selection_get_selected_item(recover->selection);
	Recoverfile *file = RECOVERFILE(item);

	Workspacegroup *wsg = workspacegroup_new_from_file(recover->wsr,
		file->filename, file->filename);
	if (wsg) {
		GtkApplication *app = gtk_window_get_application(GTK_WINDOW(recover));

		// try to restore the filename
		iobject_set(IOBJECT(wsg), file->name, "recovered workspace");
		g_autofree char *filename = g_strdup_printf("%s.ws", file->name);
		filemodel_set_filename(FILEMODEL(wsg), filename);

		Mainwindow *main = mainwindow_new(APP(app), wsg);
		gtk_window_present(GTK_WINDOW(main));
		mainwindow_cull();
		symbol_recalculate_all();

		gtk_window_destroy(GTK_WINDOW(recover));
	}
}

static GActionEntry recover_entries[] = {
	// FIXME ... ooof

	// { "open", program_open_action },

	// { "new-toolkit", program_new_toolkit_action },
	// { "new-tool", program_new_tool_action },

	// { "save", program_saveas_action },
	// { "saveas", program_saveas_action },

	{ "delete", recover_delete_action },
	{ "cancel", recover_cancel_action },
	{ "ok", recover_ok_action },
};

static void
recover_init(Recover *recover)
{
	GtkListItemFactory *factory;
	GtkColumnViewColumn *column;

	g_action_map_add_action_entries(G_ACTION_MAP(recover),
		recover_entries, G_N_ELEMENTS(recover_entries),
		recover);

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
		G_CALLBACK(recover_setup_item), "pid");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "pid");
	column = gtk_column_view_column_new("Process ID", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "date");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "date");
	column = gtk_column_view_column_new("YYYY/MM/DD", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "time");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "time");
	column = gtk_column_view_column_new("HH:MM.ss", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(recover_setup_item), "size");
	g_signal_connect(factory, "bind",
		G_CALLBACK(recover_bind_item), "size");
	column = gtk_column_view_column_new("Bytes", factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(recover->table), column);

	recover_refresh(recover);
}

static void
recover_class_init(RecoverClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	gobject_class->dispose = recover_dispose;

	BIND_RESOURCE("recover.ui");

	BIND_VARIABLE(Recover, location);
	BIND_VARIABLE(Recover, table);
	BIND_VARIABLE(Recover, used);
}

Recover *
recover_new(GtkWindow *parent_window, Workspaceroot *wsr)
{
	Recover *recover;

	recover = g_object_new(RECOVER_TYPE,
		"transient-for", parent_window,
		"application", gtk_window_get_application(parent_window),
		NULL);

	recover->wsr = wsr;

	return recover;
}
