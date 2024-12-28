/* a vertical tally of name / value pairs
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

static void
subcolumnview_dispose(GObject *object)
{
	Subcolumnview *sview;

#ifdef DEBUG
	printf("subcolumnview_dispose: %p\n", object);
#endif /*DEBUG*/

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_SUBCOLUMNVIEW(object));

	sview = SUBCOLUMNVIEW(object);

	gtk_widget_dispose_template(GTK_WIDGET(sview), SUBCOLUMNVIEW_TYPE);
	VIPS_UNREF(sview->group);

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
subcolumnview_child_remove(View *parent, View *child)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(parent);
	Rowview *rview = ROWVIEW(child);

#ifdef DEBUG
	printf("subcolumnview_child_remove: ");
	row_name_print(ROW(VOBJECT(rview)->iobject));
	printf("\n");
#endif /*DEBUG*/

	/* rowview_update_widgets() attaches spin, frame and rhsview to the grid,
	 * we need to remove them here.
	 *
	 * If refresh hasn't been called, these widgets will still be attached to
	 * the rowview and we mustn't take them off the grid.
	 */
	if (sview->grid &&
		gtk_widget_get_parent(rview->spin) == sview->grid) {
		gtk_grid_remove(GTK_GRID(sview->grid), rview->spin);
		gtk_grid_remove(GTK_GRID(sview->grid), rview->frame);
		if (rview->rhsview)
			gtk_grid_remove(GTK_GRID(sview->grid), GTK_WIDGET(rview->rhsview));

		// all pointers now invalid
		rview->spin = NULL;
		rview->frame = NULL;
		rview->rhsview = NULL;
	}

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

typedef struct _SubcolumnviewEnds {
	Rowview *first;
	Rowview *last;
} SubcolumnviewEnds;

int
rowview_get_pos(Rowview *rview)
{
	Row *row = ROW(VOBJECT(rview)->iobject);

	return ICONTAINER(row)->pos;
}

static void *
subcolumnview_find_ends_sub(Rowview *rview,
	Subcolumnview *sview, SubcolumnviewEnds *ends)
{
	if (rview->visible) {
		gtk_widget_remove_css_class(rview->frame, "top");
		gtk_widget_remove_css_class(rview->frame, "bottom");

		if (!ends->first ||
			rowview_get_pos(rview) < rowview_get_pos(ends->first))
			ends->first = rview;
		if (!ends->last ||
			rowview_get_pos(rview) > rowview_get_pos(ends->last))
			ends->last = rview;
	}

	return NULL;
}

static void
subcolumnview_find_ends(Subcolumnview *sview)
{
	SubcolumnviewEnds ends = { 0 };
	(void) view_map(VIEW(sview),
		(view_map_fn) subcolumnview_find_ends_sub, sview, &ends);

	if (ends.first)
		gtk_widget_add_css_class(ends.first->frame, "top");
	if (ends.last)
		gtk_widget_add_css_class(ends.last->frame, "bottom");
}

static void
subcolumnview_refresh(vObject *vobject)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(vobject);
	Subcolumn *scol = SUBCOLUMN(VOBJECT(sview)->iobject);
	Column *col = subcolumn_get_column(scol);
	int old_n_vis = sview->n_vis;

#ifdef DEBUG
	printf("subcolumnview_refresh: scol = %p\n", scol);
	printf("\told n_rows = %d\n", sview->n_rows);
	printf("\tnew n_rows = %d\n", icontainer_get_n_children(ICONTAINER(scol)));
#endif /*DEBUG*/

	sview->n_rows = icontainer_get_n_children(ICONTAINER(scol));
	sview->n_vis = 0;
	(void) view_map(VIEW(sview),
		(view_map_fn) subcolumnview_refresh_sub, sview, NULL);

	if (sview->n_vis != old_n_vis)
		iobject_changed(IOBJECT(col));

	subcolumnview_find_ends(sview);

	VOBJECT_CLASS(subcolumnview_parent_class)->refresh(vobject);
}

static void
subcolumnview_class_init(SubcolumnviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("subcolumnview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Subcolumnview, grid);

	object_class->dispose = subcolumnview_dispose;

	vobject_class->refresh = subcolumnview_refresh;

	view_class->link = subcolumnview_link;
	view_class->child_remove = subcolumnview_child_remove;
}

static void
subcolumnview_init(Subcolumnview *sview)
{
	gtk_widget_init_template(GTK_WIDGET(sview));

	sview->group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
}

View *
subcolumnview_new(void)
{
	Subcolumnview *sview = g_object_new(SUBCOLUMNVIEW_TYPE, NULL);

	return VIEW(sview);
}
