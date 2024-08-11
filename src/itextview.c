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

G_DEFINE_TYPE(iTextview, itextview, VIEW_TYPE)

static void
itextview_dispose(GObject *object)
{
	iTextview *itextview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_ITEXTVIEW(object));

	itextview = ITEXTVIEW(object);

#ifdef DEBUG
	printf("itextview_dispose:\n");
#endif /*DEBUG*/

	gtk_widget_dispose_template(GTK_WIDGET(itextview), ITEXTVIEW_TYPE);

	G_OBJECT_CLASS(itextview_parent_class)->dispose(object);
}

static void
itextview_refresh(vObject *vobject)
{
	iTextview *itextview = ITEXTVIEW(vobject);
	iText *itext = ITEXT(VOBJECT(itextview)->iobject);
	Row *row = HEAPMODEL(itext)->row;

	const char *display;

#ifdef DEBUG
	printf("itextview_refresh: ");
	row_name_print(row);
	printf(" (%p)\n", vobject);
#endif /*DEBUG*/

	/* Only reset edit mode if the text hasn't been changed. We
	 * don't want the user to lose work.
	 */
	if (!itextview->formula->changed)
		switch (row->ws->mode) {
		case WORKSPACE_MODE_REGULAR:
			formula_set_edit(itextview->formula, FALSE);
			formula_set_sensitive(itextview->formula, TRUE);
			break;

		case WORKSPACE_MODE_FORMULA:
			formula_set_edit(itextview->formula, TRUE);
			formula_set_sensitive(itextview->formula, TRUE);
			break;

		case WORKSPACE_MODE_NOEDIT:
			formula_set_edit(itextview->formula, FALSE);
			formula_set_sensitive(itextview->formula, FALSE);
			break;

		default:
			g_assert_not_reached();
		}

	/* We display the formula if this is a class ... we assume the members
	 * and/or the graphic will represent the value.
	 */
	if (row->is_class)
		display = itext->formula;
	else
		display = vips_buf_all(&itext->value);

	if (itextview->formula &&
		itext->value.base)
		formula_set_value_expr(itextview->formula,
			display, itext->formula);

	VOBJECT_CLASS(itextview_parent_class)->refresh(vobject);
}

static void
itextview_link(View *view, Model *model, View *parent)
{
	iTextview *itextview = ITEXTVIEW(view);
	iText *itext = ITEXT(model);
	Row *row = HEAPMODEL(itext)->row;

#ifdef DEBUG
	printf("itextview_link: ");
	row_name_print(row);
	printf("\n");
#endif /*DEBUG*/

	VIEW_CLASS(itextview_parent_class)->link(view, model, parent);

	/* Edit mode defaults to edit mode for workspace.
	 */
	formula_set_edit(itextview->formula,
		row->ws->mode == WORKSPACE_MODE_FORMULA);
}

/* Reset edit mode ... go back to whatever is set for this ws.
 */
static void
itextview_reset(View *view)
{
	iTextview *itextview = ITEXTVIEW(view);
	iText *itext = ITEXT(VOBJECT(itextview)->iobject);
	Row *row = HEAPMODEL(itext)->row;

#ifdef DEBUG
	printf("itextview_reset: ");
	row_name_print(row);
	printf("\n");
#endif /*DEBUG*/

	formula_set_edit(ITEXTVIEW(view)->formula,
		row->ws->mode == WORKSPACE_MODE_FORMULA);

	VIEW_CLASS(itextview_parent_class)->reset(view);
}

/* Re-read the text in a tally entry.
 */
static void *
itextview_scan(View *view)
{
	iTextview *itextview = ITEXTVIEW(view);
	iText *itext = ITEXT(VOBJECT(itextview)->iobject);

#ifdef DEBUG
	Row *row = HEAPMODEL(itext)->row;

	printf("itextview_scan: ");
	row_name_print(row);
	printf("\n");
#endif /*DEBUG*/

	if (formula_scan(itextview->formula) &&
		itext_set_formula(itext, itextview->formula->expr))
		itext_set_edited(itext, TRUE);

	return VIEW_CLASS(itextview_parent_class)->scan(view);
}

void
itextview_edit(Formula *formula, iTextview *itextview)
{
	view_resettable_register(VIEW(itextview));
}

void
itextview_changed(Formula *formula, iTextview *itextview)
{
	/* Make sure it's on the scannable list.
	 */
	view_scannable_register(VIEW(itextview));
}

void
itextview_activate(Formula *formula, iTextview *itextview)
{
	iText *itext = ITEXT(VOBJECT(itextview)->iobject);
	Row *row = HEAPMODEL(itext)->row;

	/* Reset edits on this row and all children. Our (potentially) next
	 * text will invlidate all of them.
	 */
	(void) icontainer_map_all(ICONTAINER(row),
		(icontainer_map_fn) heapmodel_clear_edited, NULL);

	/* Make sure we scan this text, even if it's not been edited.
	 */
	view_scannable_register(VIEW(itextview));

	workspace_set_modified(row->ws, TRUE);

	symbol_recalculate_all();

	if (row->sym &&
		!symbol_recalculate_check(row->sym)) {
		Mainwindow *main = MAINWINDOW(view_get_window(VIEW(itextview)));

		mainwindow_error(main);
	}
}

static void
itextview_class_init(iTextviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("itextview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(iTextview, formula);

	BIND_CALLBACK(itextview_edit);
	BIND_CALLBACK(itextview_changed);
	BIND_CALLBACK(itextview_activate);

	/* Create signals.
	 */

	/* Init methods.
	 */
	object_class->dispose = itextview_dispose;

	vobject_class->refresh = itextview_refresh;

	view_class->link = itextview_link;
	view_class->reset = itextview_reset;
	view_class->scan = itextview_scan;
}

static void
itextview_init(iTextview *itextview)
{
	gtk_widget_init_template(GTK_WIDGET(itextview));
}

View *
itextview_new(void)
{
	iTextview *itextview = g_object_new(ITEXTVIEW_TYPE, NULL);

	return (VIEW(itextview));
}
