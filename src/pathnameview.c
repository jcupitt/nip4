/* run the display for an arrow in a workspace
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

G_DEFINE_TYPE(Pathnameview, pathnameview, GRAPHICVIEW_TYPE)

static void
pathnameview_dispose(GObject *object)
{
	Pathnameview *pathnameview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_PATHNAMEVIEW(object));

	pathnameview = PATHNAMEVIEW(object);

	VIPS_FREEF(gtk_widget_unparent, pathnameview->top);

	G_OBJECT_CLASS(pathnameview_parent_class)->dispose(object);
}

static void
pathnameview_link(View *view, Model *model, View *parent)
{
	Pathnameview *pathnameview = PATHNAMEVIEW(view);

	VIEW_CLASS(pathnameview_parent_class)->link(view, model, parent);

	if (GRAPHICVIEW(view)->sview)
		gtk_size_group_add_widget(GRAPHICVIEW(view)->sview->group,
			pathnameview->label);
}

static void
pathnameview_refresh(vObject *vobject)
{
	Pathnameview *pathnameview = PATHNAMEVIEW(vobject);
	Pathname *pathname = PATHNAME(VOBJECT(vobject)->iobject);

#ifdef DEBUG
	printf("pathnameview_refresh: ");
	row_name_print(HEAPMODEL(pathname)->row);
	printf("\n");
#endif /*DEBUG*/

	if (vobject->iobject->caption)
		set_glabel(pathnameview->label, _("%s:"), vobject->iobject->caption);

	if (pathname->value) {
		g_autofree char *basename = g_path_get_basename(pathname->value);
		gtk_button_set_label(GTK_BUTTON(pathnameview->button), basename);
	}

	VOBJECT_CLASS(pathnameview_parent_class)->refresh(vobject);
}

static void
pathnameview_select_result(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Pathnameview *pathnameview = PATHNAMEVIEW(user_data);
	Pathname *pathname = PATHNAME(VOBJECT(pathnameview)->iobject);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);

	g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		g_autofree char *path = g_file_get_path(file);

		VIPS_SETSTR(pathname->value, path);
		classmodel_update_view(CLASSMODEL(pathname));
		symbol_recalculate_all();
	}
}

static void
pathnameview_clicked(GtkWidget *widget, Pathnameview *pathnameview)
{
	Mainwindow *main =
		MAINWINDOW(gtk_widget_get_root(GTK_WIDGET(pathnameview)));
	Pathname *pathname = PATHNAME(VOBJECT(pathnameview)->iobject);

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Select");
	gtk_file_dialog_set_modal(dialog, TRUE);

	if (pathname->value) {
		g_autoptr(GFile) file = g_file_new_for_path(pathname->value);
		gtk_file_dialog_set_initial_file(dialog, file);
	}
	else {
		GFile *load_folder = mainwindow_get_load_folder(main);
		if (load_folder)
			gtk_file_dialog_set_initial_folder(dialog, load_folder);
	}

	gtk_file_dialog_open(dialog, GTK_WINDOW(main), NULL,
		pathnameview_select_result, pathnameview);
}

static void
pathnameview_class_init(PathnameviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("pathnameview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Pathnameview, top);
	BIND_VARIABLE(Pathnameview, label);
	BIND_VARIABLE(Pathnameview, button);

	BIND_CALLBACK(pathnameview_clicked);

	object_class->dispose = pathnameview_dispose;

	vobject_class->refresh = pathnameview_refresh;

	view_class->link = pathnameview_link;
}

static void
pathnameview_init(Pathnameview *pathnameview)
{
	gtk_widget_init_template(GTK_WIDGET(pathnameview));
}

View *
pathnameview_new(void)
{
	Pathnameview *pathnameview = g_object_new(PATHNAMEVIEW_TYPE, NULL);

	return VIEW(pathnameview);
}
