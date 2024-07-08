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
 */
#define DEBUG

#include "nip4.h"

G_DEFINE_TYPE(Subcolumnview, subcolumnview, VIEW_TYPE)

// how long shadows take to animate open and closed
static const double subcolumnview_animation_duration = 0.5;
//static const double subcolumnview_animation_duration = 3.0;

void
subcolumnview_remove_shadow(Subcolumnview *sview)
{
	if (sview->shadow) {
		gtk_grid_remove(GTK_GRID(sview->grid), sview->shadow);
		sview->shadow = NULL;
		sview->shadow_row = -1;
	}

	if (sview->previous_shadow) {
		gtk_grid_remove(GTK_GRID(sview->grid), sview->previous_shadow);
		sview->previous_shadow = NULL;
		sview->previous_shadow_row = -1;
	}
}

static void
subcolumnview_set_shadow_height(Subcolumnview *sview, int shadow_height)
{
	if (sview->shadow)
		gtk_widget_set_margin_top(sview->shadow, shadow_height);
	if (sview->previous_shadow)
		gtk_widget_set_margin_top(sview->previous_shadow,
			sview->max_shadow_height - shadow_height);

	// animation done? we can remove the previous shadow
	if (shadow_height == sview->max_shadow_height &&
		sview->previous_shadow) {
		gtk_grid_remove(GTK_GRID(sview->grid), sview->previous_shadow);
		sview->previous_shadow = NULL;
		sview->previous_shadow_row = -1;
	}
}

static void
subcolumnview_stop_animation(Subcolumnview *sview)
{
	if (sview->tick_handler) {
		gtk_widget_remove_tick_callback(GTK_WIDGET(sview), sview->tick_handler);
		sview->tick_handler = 0;
	}
}

/* From clutter-easing.c, based on Robert Penner's infamous easing equations,
 * MIT license.
 */
static double
ease_out_cubic(double t)
{
	double p = t - 1;

	return p * p * p + 1;
}

static gboolean
subcolumnview_tick(GtkWidget *widget, GdkFrameClock *frame_clock,
	gpointer user_data)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(user_data);

	gint64 frame_time = gdk_frame_clock_get_frame_time(frame_clock);
	double dt = sview->last_frame_time > 0 ?
		(double) (frame_time - sview->last_frame_time) / G_TIME_SPAN_SECOND :
		1.0 / G_TIME_SPAN_SECOND;

#ifdef DEBUG
	//printf("subcolumnview_tick: dt = %g\n", dt);
#endif /*DEBUG*/

	sview->elapsed += dt;
	sview->last_frame_time = frame_time;

	// 0-1 progress in animation
	double duration = sview->should_animate ?
		subcolumnview_animation_duration : sview->elapsed;
	double t = VIPS_CLIP(0, ease_out_cubic(sview->elapsed / duration), 1);

	subcolumnview_set_shadow_height(sview, t * sview->max_shadow_height);

	if (t == 1.0)
		subcolumnview_stop_animation(sview);

	return G_SOURCE_CONTINUE;
}

static void
subcolumnview_start_animation(Subcolumnview *sview)
{
	if (!sview->tick_handler) {
		sview->last_frame_time = -1;
		sview->tick_handler = gtk_widget_add_tick_callback(GTK_WIDGET(sview),
			subcolumnview_tick, sview, NULL);
	}

	sview->elapsed = 0.0;
}

// make a new full-size shadow
void
subcolumnview_add_shadow(Subcolumnview *sview,
	int max_shadow_height, int shadow_row)
{
	printf("subcolumnview_add_shadow: %p %d\n", sview, shadow_row);

	subcolumnview_remove_shadow(sview);

	if (shadow_row != -1) {
		sview->shadow = gtk_frame_new(NULL);
		sview->max_shadow_height = max_shadow_height;
		sview->shadow_row = shadow_row;

		gtk_widget_set_margin_top(sview->shadow, sview->max_shadow_height);
		gtk_grid_attach(GTK_GRID(sview->grid), sview->shadow,
			0, shadow_row, 3, 1);
	}
}

// animate the shadow to a new position
void
subcolumnview_move_shadow(Subcolumnview *sview,
	int max_shadow_height, int shadow_row)
{
	printf("subcolumnview_move_shadow: %p %d\n", sview, shadow_row);

	if (shadow_row != sview->shadow_row) {
		// any previous shadow vanishes
		if (sview->previous_shadow) {
			gtk_grid_remove(GTK_GRID(sview->grid), sview->previous_shadow);
			sview->previous_shadow = NULL;
			sview->previous_shadow_row = -1;
		}

		// any current shadow becomes the previous shadow
		if (sview->shadow) {
			sview->previous_shadow = sview->shadow;
			sview->shadow = NULL;
			sview->previous_shadow_row = sview->shadow_row;
		}

		// start the shadow growing from the new row
		if (shadow_row != -1) {
			sview->shadow = gtk_frame_new(NULL);
			sview->max_shadow_height = max_shadow_height;
			sview->shadow_row = shadow_row;

			gtk_widget_set_margin_top(sview->shadow, 0);
			gtk_grid_attach(GTK_GRID(sview->grid), sview->shadow,
				0, shadow_row, 3, 1);
		}

		subcolumnview_start_animation(sview);

		if (sview->shadow)
			printf("\tshadow_row = %d\n", sview->shadow_row);
		if (sview->previous_shadow)
			printf("\tprevious_shadow_row = %d\n", sview->previous_shadow_row);
	}
}

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

	subcolumnview_stop_animation(sview);
	subcolumnview_remove_shadow(sview);

	gtk_widget_dispose_template(GTK_WIDGET(sview), SUBCOLUMNVIEW_TYPE);

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

#ifdef DEBUG
	printf("subcolumnview_child_add:\n");
#endif /*DEBUG*/

	VIEW_CLASS(subcolumnview_parent_class)->child_add(parent, child);
}

static void
subcolumnview_child_remove(View *parent, View *child)
{
	Subcolumnview *sview = SUBCOLUMNVIEW(parent);
	Rowview *rview = ROWVIEW(child);

#ifdef DEBUG
	printf("subcolumnview_child_remove:\n");
#endif /*DEBUG*/

	/* rowview_refresh() attaches spin, frame and rhsview to the grid, we need
	 * to remove them here.
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

	printf("subcolumnview_refresh: FIXME ... nested subcolumns?\n");
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

	if (sview->n_vis != old_n_vis)
		iobject_changed(IOBJECT(scol->top_col));

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

	sview->shadow_row = -1;
	sview->previous_shadow_row = -1;
	sview->should_animate = TRUE;
}

View *
subcolumnview_new(void)
{
	Subcolumnview *sview = g_object_new(SUBCOLUMNVIEW_TYPE, NULL);

	return VIEW(sview);
}
