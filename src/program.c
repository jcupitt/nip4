// program application window

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
 */
#define DEBUG

#include "nip4.h"

struct _Program {
	GtkApplicationWindow parent;

	/* The current save and load directories.
	 */
	GFile *save_folder;
	GFile *load_folder;

	// the model we display
	Toolkitgroup *kitg;
	Tool *tool;

	// our text_view has been modified
	gboolean changed;

	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *left;
	GtkWidget *kitgview;
	GtkWidget *text_view;

	GSettings *settings;
};

// every alive program
static GSList *program_all = NULL;

enum {
	PROP_TOOLKITGROUP = 1,
};

G_DEFINE_TYPE(Program, program, GTK_TYPE_APPLICATION_WINDOW);

GFile *
program_get_save_folder(Program *program)
{
	return program->save_folder;
}

static GFile *
get_parent(GFile *file)
{
	GFile *parent = g_file_get_parent(file);

	return parent ? parent : g_file_new_for_path("/");
}

void
program_set_save_folder(Program *program, GFile *file)
{
	VIPS_UNREF(program->save_folder);
	program->save_folder = get_parent(file);
}

GFile *
program_get_load_folder(Program *program)
{
	return program->load_folder;
}

void
program_set_load_folder(Program *program, GFile *file)
{
	VIPS_UNREF(program->load_folder);
	program->load_folder = get_parent(file);
}

static void
program_dispose(GObject *object)
{
	Program *program = PROGRAM(object);

#ifdef DEBUG
	printf("program_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(program->kitg);
	VIPS_UNREF(program->settings);

	program_all = g_slist_remove(program_all, program);

	G_OBJECT_CLASS(program_parent_class)->dispose(object);
}

static void
program_refresh(Program *program)
{
#ifdef DEBUG
	printf("program_refresh:\n");
#endif /*DEBUG*/

	char txt1[512];
	char txt2[512];
	VipsBuf title = VIPS_BUF_STATIC(txt1);
	VipsBuf subtitle = VIPS_BUF_STATIC(txt2);

	if (program->kitg) {
		vips_buf_appends(&title, _("Toolkits for "));
		symbol_qualified_name(program->kitg->root, &title);
	}

	if (program->tool) {
		if (FILEMODEL(program->tool->kit)->modified)
			vips_buf_appends(&subtitle, "*");

		// filename for the toolkit containing this tool
		char *filename;
		if ((filename = FILEMODEL(program->tool->kit)->filename))
			vips_buf_appends(&subtitle, filename);
		else
			vips_buf_appends(&subtitle, _("unsaved toolkit"));

		vips_buf_appends(&subtitle, " - ");
		symbol_qualified_name(program->tool->sym, &subtitle);

		if (program->changed)
			vips_buf_appends(&subtitle, " [modified]");
	}

	gtk_label_set_text(GTK_LABEL(program->title), vips_buf_all(&title));
	gtk_label_set_text(GTK_LABEL(program->subtitle), vips_buf_all(&subtitle));
}

static void
program_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Program *program = PROGRAM(object);

	switch (prop_id) {
	case PROP_TOOLKITGROUP:
		g_value_set_object(value, program->kitg);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
program_kitg_changed(Toolkitgroup *kitg, Program *program)
{
	program_refresh(program);
}

static void
program_text_changed(GtkTextBuffer *buffer, Program *program)
{
#ifdef DEBUG
	printf("program_text_changed\n");
#endif /*DEBUG*/

	if (!program->changed) {
		program->changed = TRUE;
		program_refresh(program);

#ifdef DEBUG
		printf("\t(changed = TRUE)\n");
#endif /*DEBUG*/
	}
}

static void
program_set_text(Program *program, const char *text)
{
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(program->text_view));

	g_signal_handlers_block_by_func(buffer, program_text_changed, program);
	text_view_set_text(GTK_TEXT_VIEW(program->text_view), text, TRUE );
	g_signal_handlers_unblock_by_func(buffer, program_text_changed, program);
}

static void
program_kitgview_activate(Toolkitgroupview *kitgview,
	Toolitem *toolitem, Tool *tool, Program *program)
{
	Tool *selected = tool ? tool : (toolitem ? toolitem->tool : NULL);

	if (selected &&
		program->tool != selected) {
		program->tool = selected;

		program_set_text(program, program->tool->sym->expr->compile->text);
		iobject_changed(IOBJECT(program->kitg));
	}
}

static void
program_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Program *program = PROGRAM(object);

	switch (prop_id) {
	case PROP_TOOLKITGROUP:
		VIPS_UNREF(program->kitg);
		Toolkitgroup *kitg = g_value_get_object(value);
		program->kitg = kitg;

		g_signal_connect_object(G_OBJECT(program->kitg), "changed",
			G_CALLBACK(program_kitg_changed), program, 0);

		vobject_link(VOBJECT(program->kitgview), IOBJECT(program->kitg));
		g_signal_connect_object(G_OBJECT(program->kitgview), "activate",
			G_CALLBACK(program_kitgview_activate), program, 0);

		iobject_changed(IOBJECT(program->kitg));

		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
program_init(Program *program)
{
#ifdef DEBUG
	printf("program_init:\n");
#endif /*DEBUG*/

	program->settings = g_settings_new(APPLICATION_ID);

	g_autofree char *cwd = g_get_current_dir();
	program->save_folder = g_file_new_for_path(cwd);
	program->load_folder = g_file_new_for_path(cwd);

	gtk_widget_init_template(GTK_WIDGET(program));

	program_all = g_slist_prepend(program_all, program);
}

static void
program_process_clicked(GtkButton *button, Program *program)
{
#ifdef DEBUG
	printf("program_process_clicked\n");
#endif /*DEBUG*/
}

static void
program_class_init(ProgramClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	BIND_RESOURCE("program.ui");

	BIND_VARIABLE(Program, title);
	BIND_VARIABLE(Program, subtitle);
	BIND_VARIABLE(Program, gears);
	BIND_VARIABLE(Program, left);
	BIND_VARIABLE(Program, kitgview);
	BIND_VARIABLE(Program, text_view);

	BIND_CALLBACK(program_text_changed);
	BIND_CALLBACK(program_process_clicked);

	gobject_class->dispose = program_dispose;
	gobject_class->get_property = program_get_property;
	gobject_class->set_property = program_set_property;

	g_object_class_install_property(gobject_class, PROP_TOOLKITGROUP,
		g_param_spec_object("toolkitgroup",
			_("Toolkitgroup"),
			_("Toolkitgroup to edit"),
			TOOLKITGROUP_TYPE,
			G_PARAM_READWRITE));

}

Program *
program_new(App *app, Toolkitgroup *kitg)
{
	Program *program = g_object_new(PROGRAM_TYPE,
		"application", app,
		"toolkitgroup", kitg,
		NULL);

	gtk_window_present(GTK_WINDOW(program));

	return program;
}