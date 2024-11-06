/* a view of a text thingy
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

G_DEFINE_ABSTRACT_TYPE(Editview, editview, GRAPHICVIEW_TYPE)

static void
editview_dispose(GObject *object)
{
	Editview *editview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_EDITVIEW(object));

	editview = EDITVIEW(object);

	gtk_widget_dispose_template(GTK_WIDGET(editview), EDITVIEW_TYPE);
	VIPS_FREEF(gtk_widget_unparent, editview->top);

	G_OBJECT_CLASS(editview_parent_class)->dispose(object);
}

static void
editview_link(View *view, Model *model, View *parent)
{
	Editview *editview = EDITVIEW(view);

	VIEW_CLASS(editview_parent_class)->link(view, model, parent);

	if (GRAPHICVIEW(view)->sview)
		gtk_size_group_add_widget(GRAPHICVIEW(view)->sview->group,
			editview->label);
}

static void
editview_refresh(vObject *vobject)
{
	Editview *editview = EDITVIEW(vobject);

#ifdef DEBUG
	printf("editview_refresh:\n");
#endif /*DEBUG*/

	if (vobject->iobject->caption)
		set_glabel(editview->label, _("%s:"), vobject->iobject->caption);

	VOBJECT_CLASS(editview_parent_class)->refresh(vobject);
}

static void
editview_class_init(EditviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("editview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Editview, top);
	BIND_VARIABLE(Editview, label);
	BIND_VARIABLE(Editview, ientry);

	BIND_CALLBACK(view_changed);
	BIND_CALLBACK(view_cancel);
	BIND_CALLBACK(view_activate);

	object_class->dispose = editview_dispose;

	vobject_class->refresh = editview_refresh;

	view_class->link = editview_link;
}

static void
editview_init(Editview *editview)
{
	gtk_widget_init_template(GTK_WIDGET(editview));
}

void
editview_set_entry(Editview *editview, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	g_autofree char *text = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	/* Make sure we don't trigger "changed" when we set the
	 * text.
	 */
	g_signal_handlers_block_matched(G_OBJECT(editview->ientry),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editview);
	g_object_set(editview->ientry, "text", text, NULL);
	g_signal_handlers_unblock_matched(G_OBJECT(editview->ientry),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editview);
}
