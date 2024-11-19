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

G_DEFINE_TYPE(Matrixview, matrixview, GRAPHICVIEW_TYPE)

static void
matrixview_dispose(GObject *object)
{
	Matrixview *matrixview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_MATRIXVIEW(object));

	matrixview = MATRIXVIEW(object);

	VIPS_FREEF(gtk_widget_unparent, matrixview->top);
	VIPS_FREEF(g_slist_free, matrixview->items);

	G_OBJECT_CLASS(matrixview_parent_class)->dispose(object);
}

static void
matrixview_toggle_clicked(GtkWidget *widget, Matrixview *matrixview)
{
	Matrix *matrix = MATRIX(VOBJECT(matrixview)->iobject);
	int width = matrix->value.width;
	int height = matrix->value.height;
	int pos = g_slist_index(matrixview->items, widget);

	if (pos >= 0 && pos < width * height) {
		switch ((int) matrix->value.coeff[pos]) {
		case 0:
			matrix->value.coeff[pos] = 128.0;
			break;

		case 255:
			matrix->value.coeff[pos] = 0.0;
			break;

		default:
			matrix->value.coeff[pos] = 255.0;
			break;
		}

		classmodel_update(CLASSMODEL(matrix));
		symbol_recalculate_all();
	}
}

static void
matrixview_set_callbacks(Matrixview *matrixview, GtkWidget *widget)
{
	g_signal_connect(widget, "changed", G_CALLBACK(view_changed), matrixview);
	g_signal_connect(widget, "cancel", G_CALLBACK(view_cancel), matrixview);
	g_signal_connect(widget, "activate", G_CALLBACK(view_activate), matrixview);
}

/* Drag on a scale in a Tslider.
 */
static void
matrixview_slider_changed(Tslider *tslider, Matrixview *matrixview)
{
	Matrix *matrix = MATRIX(VOBJECT(matrixview)->iobject);
	int pos = g_slist_index(matrixview->items, tslider);
	int x = pos % matrixview->width;
	int y = pos / matrixview->width;
	int i = x + y * matrix->value.width;

	g_assert(pos >= 0);

	/* Install value.
	 */
	if (matrix->value.coeff[i] != tslider->svalue) {
		matrix->value.coeff[i] = tslider->svalue;

		classmodel_update(CLASSMODEL(matrix));
		symbol_recalculate_all();
	}
}

static void
matrixview_grid_build(Matrixview *matrixview)
{
	Matrix *matrix = MATRIX(VOBJECT(matrixview)->iobject);
	int width = matrix->value.width;
	int height = matrix->value.height;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			GtkWidget *item;

			switch (matrix->display) {
			case MATRIX_DISPLAY_TEXT_SCALE_OFFSET:
			case MATRIX_DISPLAY_TEXT:
				item = GTK_WIDGET(ientry_new());
				g_object_set(item, "width-chars", 5, NULL);
				matrixview_set_callbacks(matrixview, item);
				break;

			case MATRIX_DISPLAY_SLIDER:
				item = tslider_new(-2.0, 2.0, 3);

    			g_signal_connect(item, "text_changed",
					G_CALLBACK(view_changed), matrixview);
    			g_signal_connect(item, "activate",
    				G_CALLBACK(view_activate), matrixview);
    			g_signal_connect(item, "changed",
    				G_CALLBACK(matrixview_slider_changed), matrixview);

				block_scroll(item);
				break;

			case MATRIX_DISPLAY_TOGGLE:
				item = gtk_button_new_with_label("0");
				g_signal_connect(item, "clicked",
					G_CALLBACK(matrixview_toggle_clicked), matrixview);

				if (x == width / 2 &&
					y == height / 2)
					gtk_widget_add_css_class(item, "centre_widget");
				break;

			default:
				g_assert_not_reached();
			}

			gtk_grid_attach(GTK_GRID(matrixview->grid), item, x, y, 1, 1);
			matrixview->items = g_slist_append(matrixview->items, item);
		}
	}

	gtk_widget_set_visible(matrixview->scaleoffset,
		matrix->display == MATRIX_DISPLAY_TEXT_SCALE_OFFSET);

	int max_width;
	int max_height;
	switch (matrix->display) {
	case MATRIX_DISPLAY_TEXT:
		// 11 lets vips_stats() output display without horizontal scrolling
		max_width = 11;
		max_height = 10;
		break;

	case MATRIX_DISPLAY_SLIDER:
		max_width = 3;
		max_height = 10;
		break;

	case MATRIX_DISPLAY_TOGGLE:
		max_width = 20;
		max_height = 10;
		break;

	case MATRIX_DISPLAY_TEXT_SCALE_OFFSET:
		max_width = 8;
		max_height = 10;
		break;

	default:
		g_assert_not_reached();
	}

	GtkPolicyType hpolicy = width > max_width ?
		GTK_POLICY_ALWAYS : GTK_POLICY_NEVER;
	GtkPolicyType vpolicy = height > max_height ?
		GTK_POLICY_ALWAYS : GTK_POLICY_NEVER;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(matrixview->swin),
		hpolicy, vpolicy);
}

static void
matrixview_grid_refresh(Matrixview *matrixview)
{
	Matrix *matrix = MATRIX(VOBJECT(matrixview)->iobject);

	int i;
	i = 0;
	for (GSList *p = matrixview->items; p; p = p->next, i++) {
		GtkWidget *item = GTK_WIDGET(p->data);

		switch (matrix->display) {
		case MATRIX_DISPLAY_TEXT:
		case MATRIX_DISPLAY_TEXT_SCALE_OFFSET:
			ientry_set_double(IENTRY(item), 8, matrix->value.coeff[i]);
			break;

		case MATRIX_DISPLAY_SLIDER:
			Tslider *tslider = TSLIDER(item);
			tslider->value = matrix->value.coeff[i];
			tslider->svalue = matrix->value.coeff[i];
			tslider_changed(TSLIDER(item));
			break;

		case MATRIX_DISPLAY_TOGGLE:
			const char *label;
			switch ((int) matrix->value.coeff[i]) {
			case 0:
				label = "0";
				break;

			case 255:
				label = "1";
				break;

			default:
				label = " ";
				break;
			}
			gtk_button_set_label(GTK_BUTTON(item), label);
			break;

		default:
			g_assert_not_reached();
		}
	}

	if (matrix->display == MATRIX_DISPLAY_TEXT_SCALE_OFFSET) {
		ientry_set_double(IENTRY(matrixview->scale), 8, matrix->scale);
		ientry_set_double(IENTRY(matrixview->offset), 8, matrix->offset);
	}
}

static void
matrixview_refresh(vObject *vobject)
{
	Matrixview *matrixview = MATRIXVIEW(vobject);
	Matrix *matrix = MATRIX(VOBJECT(vobject)->iobject);

#ifdef DEBUG
	printf("matrixview_refresh: ");
	row_name_print(HEAPMODEL(matrix)->row);
	printf("\n");
#endif /*DEBUG*/

	// do we need to rebuild the display?
	if (matrixview->width != matrix->value.width ||
		matrixview->height != matrix->value.height ||
		matrixview->display != matrix->display) {
		for (GSList *p = matrixview->items; p; p = p->next) {
			GtkWidget *item = GTK_WIDGET(p->data);

			gtk_grid_remove(GTK_GRID(matrixview->grid), item);
		}

		VIPS_FREEF(g_slist_free, matrixview->items);

		matrixview->display = matrix->display;
		matrixview->width = matrix->value.width;
		matrixview->height = matrix->value.height;

		matrixview_grid_build(matrixview);
	}

	matrixview_grid_refresh(matrixview);

	VOBJECT_CLASS(matrixview_parent_class)->refresh(vobject);
}

static gboolean
matrixview_scan_ientry(GtkWidget *ientry, double *out, gboolean *changed)
{
	double value;

	if (!ientry_get_double(IENTRY(ientry), &value)) {
		error_top(_("Bad value"));
		error_sub("%s", _("not a number"));
		return FALSE;
	}

	// look for some largish difference, or we'll scan accidentally because of
	// float->str->float conversion errors
	if (fabs(value - *out) > 0.0001) {
		*out = value;
		*changed = TRUE;
	}

	return TRUE;
}

static void *
matrixview_scan(View *view)
{
	Matrixview *matrixview = MATRIXVIEW(view);
	Matrix *matrix = MATRIX(VOBJECT(matrixview)->iobject);
	int width = matrix->value.width;
	int height = matrix->value.height;
	Expr *expr = HEAPMODEL(matrix)->row->expr;

#ifdef DEBUG
#endif /*DEBUG*/
	printf("matrixview_scan: %s\n", row_name(expr->row));

	gboolean changed;

	expr_error_clear(expr);
	changed = FALSE;

	if (matrix->display == MATRIX_DISPLAY_TEXT_SCALE_OFFSET)
		if (!matrixview_scan_ientry(matrixview->scale,
				&matrix->scale, &changed) ||
		   !matrixview_scan_ientry(matrixview->offset,
				&matrix->offset, &changed)) {
			expr_error_set(expr);
			return view;
		}

	GSList *p;
	int x, y;
	for (p = matrixview->items, y = 0; y < height; y++)
		for (x = 0; x < width; x++, p = p->next) {
			int i = x + y * width;
			GtkWidget *item = matrix->display == MATRIX_DISPLAY_SLIDER ?
				TSLIDER(p->data)->entry : GTK_WIDGET(p->data);

			if (!matrixview_scan_ientry(item,
				&matrix->value.coeff[i], &changed)) {
				error_top(_("Bad value"));
				error_sub(_("cell (%d, %d) is not a number"), x, y);
				expr_error_set(expr);

				return view;
			}
		}

	if (changed)
		classmodel_update(CLASSMODEL(matrix));

	return VIEW_CLASS(matrixview_parent_class)->scan(view);
}

static void
matrixview_class_init(MatrixviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("matrixview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Matrixview, top);
	BIND_VARIABLE(Matrixview, swin);
	BIND_VARIABLE(Matrixview, grid);
	BIND_VARIABLE(Matrixview, scaleoffset);
	BIND_VARIABLE(Matrixview, scale);
	BIND_VARIABLE(Matrixview, offset);

	BIND_CALLBACK(view_changed);
	BIND_CALLBACK(view_cancel);
	BIND_CALLBACK(view_activate);

	object_class->dispose = matrixview_dispose;

	vobject_class->refresh = matrixview_refresh;

	view_class->scan = matrixview_scan;
}

static void
matrixview_init(Matrixview *matrixview)
{
	gtk_widget_init_template(GTK_WIDGET(matrixview));
}

View *
matrixview_new(void)
{
	Matrixview *matrixview = g_object_new(MATRIXVIEW_TYPE, NULL);

	return VIEW(matrixview);
}
