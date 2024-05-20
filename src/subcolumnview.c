/* a subcolumnview button in a workspace
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

G_DEFINE_TYPE(Subcolumnview, subcolumnview, VIEW_TYPE)

static void *
subcolumnview_dispose_sub(void *item, void *a)
{
	View *child = VIEW(item);

	view_child_remove(child);

	return NULL;
}

static void
subcolumnview_dispose(GObject *object)
{
	Subcolumnview *sview;

#ifdef DEBUG
#endif /*DEBUG*/
	printf("subcolumnview_dispose\n");

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_SUBCOLUMNVIEW(object));

	sview = SUBCOLUMNVIEW(object);

	gtk_widget_dispose_template(GTK_WIDGET(sview), SUBCOLUMNVIEW_TYPE);

	printf("subcolumnview_dispose: removing %d children\n",
		g_slist_length(sview->rows));
	slist_map(sview->rows, subcolumnview_dispose_sub, NULL);
	VIPS_FREEF(g_slist_free, sview->rows);

	G_OBJECT_CLASS(subcolumnview_parent_class)->dispose(object);
}

static void
subcolumnview_link(View *view, Model *model, View *parent)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(view);
	Subcolumn *scol = SUBCOLUMN(model);

#ifdef DEBUG
	printf("subcolumnview_link: ");
	if (HEAPMODEL(scol)->row)
		row_name_print(HEAPMODEL(scol)->row);
	else
		printf("(null)");
	printf("\n");
#endif /*DEBUG*/

	VIEW_CLASS(subcolumnview_parent_class)->link(view, model, parent);

	/* Add to enclosing column, if there is one. Attached to enclosing row
	 * by rowview_refresh() if we're a subcolumn.
	 */
	if (!scol->is_top)
		sview->rhsview = RHSVIEW(parent);
}

static void
subcolumnview_child_add(View *parent, View *child)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(parent);
	Rowview *rview = ROWVIEW(child);

	g_assert(!g_slist_find(sview->rows, child));
	sview->rows = g_slist_prepend(sview->rows, child);

	VIEW_CLASS(subcolumnview_parent_class)->child_add(parent, child);
}

static void
subcolumnview_child_remove(View *parent, View *child)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(parent);
	Rowview *rview = ROWVIEW(child);

	g_assert(g_slist_find(sview->rows, child));
	sview->rows = g_slist_remove(sview->rows, child);

	VIEW_CLASS(subcolumnview_parent_class)->child_remove(parent, child);
}

static void *
subcolumnview_refresh_sub(Rowview *rview, Subcolumnview *sview)
{
	Subcolumn *scol = SUBCOLUMN(VOBJECT(sview)->iobject);
	Row *row = ROW(VOBJECT(rview)->iobject);
	int i;

	/* Most predicates need a sym.
	 */
	if (!row->sym)
		return NULL;

	for (i = 0; i <= scol->vislevel; i++)
		if (subcolumn_visibility[i].pred(row)) {
			rowview_set_visible(rview, TRUE);
			sview->n_vis++;
			break;
		}
	if (i > scol->vislevel)
		rowview_set_visible(rview, FALSE);

	return NULL;
}

static void
subcolumnview_refresh(vObject *vobject)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(vobject);
	Subcolumn *scol = SUBCOLUMN(VOBJECT(sview)->iobject);
	int old_n_vis = sview->n_vis;
	gboolean editable = scol->top_col->ws->mode != WORKSPACE_MODE_NOEDIT;

#ifdef DEBUG
	printf("subcolumnview_refresh\n");
#endif /*DEBUG*/

	sview->n_rows = icontainer_get_n_children(ICONTAINER(scol));

	/* Nested subcols: we just change the left indent.
	if (!scol->is_top && editable) {
		gtk_alignment_set_padding(GTK_ALIGNMENT(sview->align),
			0, 0, 0, 0);
	}
	else if (!scol->is_top && !editable) {
		gtk_alignment_set_padding(GTK_ALIGNMENT(sview->align),
			0, 0, 15, 0);
	}
	 */

	sview->n_vis = 0;
	(void) view_map(VIEW(sview),
		(view_map_fn) subcolumnview_refresh_sub, sview, NULL);

	if (sview->n_vis != old_n_vis) {
		view_resize(VIEW(sview));
		iobject_changed(IOBJECT(scol->top_col));
	}

	VOBJECT_CLASS(subcolumnview_parent_class)->refresh(vobject);
}

static void
subcolumnview_class_init(SubcolumnviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("subcolumnview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(Subcolumnview, grid);

	object_class->dispose = subcolumnview_dispose;

	vobject_class->refresh = subcolumnview_refresh;

	view_class->link = subcolumnview_link;
	view_class->child_add = subcolumnview_child_add;
	view_class->child_remove = subcolumnview_child_remove;
}

static void
subcolumnview_init(Subcolumnview *sview)
{
	gtk_widget_init_template(GTK_WIDGET(sview));
}

View *
subcolumnview_new(void)
{
	Subcolumnview *sview = g_object_new(SUBCOLUMNVIEW_TYPE, NULL);

	return VIEW(sview);
}
