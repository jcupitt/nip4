/* the rhs of a tallyrow ... group together everything to the right of the
 * button
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

G_DEFINE_TYPE(Rhsview, rhsview, VIEW_TYPE)

static void
rhsview_dispose(GObject *object)
{
	Rhsview *rhsview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_ROWVIEW(object));

	rhsview = RHSVIEW(object);

#ifdef DEBUG
	printf("rhsview_dispose:\n");
#endif /*DEBUG*/

	UNPARENT(rhsview->grid);

	G_OBJECT_CLASS(rhsview_parent_class)->dispose(object);
}

/* Get this if ws->mode changes.
 */
static void
rhsview_reset(View *view)
{
	Rhsview *rhsview = RHSVIEW(view);
	Rhs *rhs = RHS(VOBJECT(rhsview)->iobject);
	Row *row = HEAPMODEL(rhs)->row;

	model_display(rhs->itext,
		row->ws->mode == WORKSPACE_MODE_FORMULA || rhs->flags & RHS_ITEXT);

	VIEW_CLASS(rhsview_parent_class)->reset(view);
}

static void
rhsview_refresh(vObject *vobject)
{
	Rhsview *rhsview = RHSVIEW(vobject);
	Rhs *rhs = RHS(VOBJECT(rhsview)->iobject);
	Row *row = HEAPMODEL(rhs)->row;

#ifdef DEBUG
	printf("rhsview_refresh: ");
	row_name_print(HEAPMODEL(rhs)->row);
	printf(" ");
	if (rhs->flags & RHS_GRAPHIC)
		printf("RHS_GRAPHIC ");
	if (rhs->flags & RHS_SCOL)
		printf("RHS_SCOL ");
	if (rhs->flags & RHS_ITEXT)
		printf("RHS_ITEXT ");
	printf("\n");
#endif /*DEBUG*/

	/* Add/remove children according to rhs->flags.
	 */
	model_display(rhs->graphic, rhs->flags & RHS_GRAPHIC);
	model_display(rhs->scol, rhs->flags & RHS_SCOL);

	switch (row->ws->mode) {
	case WORKSPACE_MODE_REGULAR:
		model_display(rhs->itext, rhs->flags & RHS_ITEXT);
		break;

	case WORKSPACE_MODE_FORMULA:
		model_display(rhs->itext, TRUE);
		break;

	case WORKSPACE_MODE_NOEDIT:
		/* Only show the text if it's the only thing we have for this
		 * row.
		 */
		if (rhs->graphic &&
			rhs->flags & RHS_GRAPHIC)
			model_display(rhs->itext, FALSE);
		else if (rhs->scol &&
			rhs->flags & RHS_SCOL)
			model_display(rhs->itext, FALSE);
		else
			model_display(rhs->itext,
				rhs->flags & RHS_ITEXT);
		break;

	default:
		g_assert(0);
	}

	VOBJECT_CLASS(rhsview_parent_class)->refresh(vobject);
}

static void
rhsview_link(View *view, Model *model, View *parent)
{
	Rhsview *rhsview = RHSVIEW(view);
	Rowview *rview = ROWVIEW(parent);

#ifdef DEBUG
	printf("rhsview_link: ");
	row_name_print(ROW(VOBJECT(rview)->iobject));
	printf("\n");
#endif /*DEBUG*/

	VIEW_CLASS(rhsview_parent_class)->link(view, model, parent);

	rhsview->rview = rview;
}

static void
rhsview_child_add(View *parent, View *child)
{
	Rhsview *rhsview = RHSVIEW(parent);

	if (IS_SUBCOLUMNVIEW(child)) {
		gtk_grid_attach(GTK_GRID(rhsview->grid), GTK_WIDGET(child), 0, 1, 1, 1);
		rhsview->scol = child;
	}
	else if (IS_ITEXTVIEW(child)) {
		gtk_grid_attach(GTK_GRID(rhsview->grid), GTK_WIDGET(child), 0, 2, 1, 1);
		rhsview->itext = child;
	}
	else {
		g_assert(IS_GRAPHICVIEW(child));

		gtk_grid_attach(GTK_GRID(rhsview->grid), GTK_WIDGET(child), 0, 0, 1, 1);
		rhsview->graphic = child;
	}

	VIEW_CLASS(rhsview_parent_class)->child_add(parent, child);
}

static void
rhsview_child_remove(View *parent, View *child)
{
	Rhsview *rhsview = RHSVIEW(parent);

	if (IS_SUBCOLUMNVIEW(child))
		rhsview->scol = NULL;
	else if (IS_ITEXTVIEW(child))
		rhsview->itext = NULL;
	else
		rhsview->graphic = NULL;

	VIEW_CLASS(rhsview_parent_class)->child_remove(parent, child);
}

static void
rhsview_class_init(RhsviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("rhsview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(Rhsview, grid);

	/* Create signals.
	 */

	/* Init methods.
	 */
	object_class->dispose = rhsview_dispose;

	vobject_class->refresh = rhsview_refresh;

	view_class->link = rhsview_link;
	view_class->child_add = rhsview_child_add;
	view_class->child_remove = rhsview_child_remove;
	view_class->reset = rhsview_reset;
}

static void
rhsview_init(Rhsview *rhsview)
{
	gtk_widget_init_template(GTK_WIDGET(rhsview));
}

View *
rhsview_new(void)
{
	Rhsview *rhsview = g_object_new(RHSVIEW_TYPE, NULL);

	return VIEW(rhsview);
}
