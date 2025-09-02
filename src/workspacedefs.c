/* Workspace-local defs.
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

G_DEFINE_TYPE(Workspacedefs, workspacedefs, VIEW_TYPE);

static void
workspacedefs_dispose(GObject *object)
{
	Workspacedefs *workspacedefs = WORKSPACEDEFS(object);

	gtk_widget_dispose_template(GTK_WIDGET(workspacedefs), WORKSPACEDEFS_TYPE);

	G_OBJECT_CLASS(workspacedefs_parent_class)->dispose(object);
}

static void
workspacedefs_text_changed(GtkTextBuffer *buffer,
	Workspacedefs *workspacedefs)
{
#ifdef DEBUG
	printf("workspacedefs_text_changed\n");
#endif /*DEBUG*/

	if (!workspacedefs->changed) {
		workspacedefs->changed = TRUE;

#ifdef DEBUG
		printf("\t(changed = TRUE)\n");
#endif /*DEBUG*/

		/* The workspace hasn't changed, but this will queue a refresh
		 * on us.
		 */
		iobject_changed(IOBJECT(workspacedefs->ws));
	}
}

static void
workspacedefs_refresh(vObject *vobject)
{
	Workspacedefs *workspacedefs = WORKSPACEDEFS(vobject);
	Workspace *ws = workspacedefs->ws;
	char txt[256];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

#ifdef DEBUG
	printf("workspacedefs_refresh:\n");
#endif /*DEBUG*/

	if (!workspacedefs->changed) {
		guint text_hash = g_str_hash(ws->local_defs);

		if (text_hash != workspacedefs->text_hash) {
			GtkTextView *text = GTK_TEXT_VIEW(workspacedefs->text);
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);

			g_signal_handlers_block_by_func(buffer,
				workspacedefs_text_changed, workspacedefs);

			text_view_set_text(text, ws->local_defs, TRUE);

			g_signal_handlers_unblock_by_func(buffer,
				workspacedefs_text_changed, workspacedefs);

			workspacedefs->text_hash = text_hash;
		}
	}

	if (ws->local_kit) {
		int n = icontainer_get_n_children(ICONTAINER(ws->local_kit));

		vips_buf_appendf(&buf,
			ngettext("%d definition", "%d definitions", n), n);
	}
	else
		vips_buf_appendf(&buf, _("no definitions"));
	if (workspacedefs->errors) {
		if (!vips_buf_is_empty(&buf))
			vips_buf_appendf(&buf, ", ");
		vips_buf_appendf(&buf, _("errors"));
	}
	if (workspacedefs->changed) {
		if (!vips_buf_is_empty(&buf))
			vips_buf_appendf(&buf, ", ");
		vips_buf_appendf(&buf, _("modified"));
	}

	set_glabel(workspacedefs->status, "%s", vips_buf_all(&buf));

	VOBJECT_CLASS(workspacedefs_parent_class)->refresh(vobject);
}

static void
workspacedefs_link(vObject *vobject, iObject *iobject)
{
	Workspacedefs *workspacedefs = WORKSPACEDEFS(vobject);
	Workspace *ws = WORKSPACE(iobject);

	g_assert(!workspacedefs->ws);

	workspacedefs->ws = ws;

	VOBJECT_CLASS(workspacedefs_parent_class)->link(vobject, iobject);
}

static gboolean
workspacedefs_set_text(Workspacedefs *workspacedefs, const char *txt)
{
	Workspace *ws = workspacedefs->ws;

	workspacedefs->changed = FALSE;
	workspacedefs->errors = FALSE;
	workspacedefs->text_hash = g_str_hash(txt);
	if (!workspace_local_set(ws, txt)) {
		text_view_select_text(GTK_TEXT_VIEW(workspacedefs->text),
			input_state.charpos - yyleng, input_state.charpos);
		workspacedefs->errors = TRUE;

		return FALSE;
	}

	symbol_recalculate_all();

	return TRUE;
}

/* "Process" in defs area.
 */
static void
workspacedefs_clicked(GtkWidget *wid, Workspacedefs *workspacedefs)
{
#ifdef DEBUG
	printf("workspacedefs_clicked:\n");
#endif /*DEBUG*/

	g_autofree char *txt =
		text_view_get_text(GTK_TEXT_VIEW(workspacedefs->text));
	if (!workspacedefs_set_text(workspacedefs, txt))
		workspace_show_error(workspacedefs->ws);
	else
		workspace_set_modified(workspacedefs->ws, TRUE);
}

static void
workspacedefs_class_init(WorkspacedefsClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	gobject_class->dispose = workspacedefs_dispose;

	vobject_class->refresh = workspacedefs_refresh;
	vobject_class->link = workspacedefs_link;

	BIND_RESOURCE("workspacedefs.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Workspacedefs, top);
	BIND_VARIABLE(Workspacedefs, status);
	BIND_VARIABLE(Workspacedefs, text);

	BIND_CALLBACK(workspacedefs_clicked);
	BIND_CALLBACK(workspacedefs_text_changed);

}

/*
static gboolean
workspacedefs_set_text_from_file(Workspacedefs *workspacedefs,
	const char *fname)
{
	Workspace *ws = workspacedefs->ws;

	workspacedefs->changed = FALSE;
	workspacedefs->errors = FALSE;
	if (!workspace_local_set_from_file(ws, fname)) {
		text_view_select_text(GTK_TEXT_VIEW(workspacedefs->text),
			input_state.charpos - yyleng, input_state.charpos);
		workspacedefs->errors = TRUE;

		return FALSE;
	}

	symbol_recalculate_all();

	return TRUE;
}
 */

/* Callback from load browser.
static void
workspacedefs_load_file_cb(iWindow *iwnd,
	void *client, iWindowNotifyFn nfn, void *sys)
{
	Filesel *filesel = FILESEL(iwnd);
	Workspacedefs *workspacedefs = WORKSPACEDEFS(client);
	char *fname;

	if (!(fname = filesel_get_filename(filesel))) {
		nfn(sys, IWINDOW_ERROR);
		return;
	}

	if (!workspacedefs_set_text_from_file(workspacedefs, fname)) {
		g_free(fname);
		nfn(sys, IWINDOW_ERROR);
		return;
	}

	g_free(fname);

	nfn(sys, IWINDOW_YES);
}

static void
workspacedefs_replace_cb(GtkWidget *wid, Workspacedefs *workspacedefs)
{
	GtkWidget *filesel;

	filesel = filesel_new();
	iwindow_set_title(IWINDOW(filesel),
		_("Replace Definition From File"));
	filesel_set_flags(FILESEL(filesel), FALSE, FALSE);
	filesel_set_filetype(FILESEL(filesel), filesel_type_definition, 0);
	iwindow_set_parent(IWINDOW(filesel), GTK_WIDGET(wid));
	filesel_set_done(FILESEL(filesel),
		workspacedefs_load_file_cb, workspacedefs);
	iwindow_build(IWINDOW(filesel));

	gtk_widget_show(GTK_WIDGET(filesel));
}

static void
workspacedefs_save_as_cb(GtkWidget *wid, Workspacedefs *workspacedefs)
{
	Workspace *ws = workspacedefs->ws;

	if (ws->local_kit)
		filemodel_inter_saveas(IWINDOW(wid),
			FILEMODEL(ws->local_kit));
}
 */

static void
workspacedefs_init(Workspacedefs *workspacedefs)
{
#ifdef DEBUG
	printf("workspacedefs_init:\n");
#endif /*DEBUG*/

	workspacedefs->changed = FALSE;
	workspacedefs->errors = FALSE;
	workspacedefs->text_hash = 0;

	gtk_widget_init_template(GTK_WIDGET(workspacedefs));

	gtk_label_set_xalign(GTK_LABEL(workspacedefs->status), 1.0);
}

Workspacedefs *
workspacedefs_new(void)
{
	Workspacedefs *workspacedefs = g_object_new(WORKSPACEDEFS_TYPE, NULL);

	return workspacedefs;
}
