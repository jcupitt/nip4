/* run the display for a slider in a workspace
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

G_DEFINE_TYPE(Sliderview, sliderview, GRAPHICVIEW_TYPE)

static void
sliderview_dispose(GObject *object)
{
	Sliderview *sliderview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_SLIDERVIEW(object));

	sliderview = SLIDERVIEW(object);

	VIPS_FREEF(gtk_widget_unparent, sliderview->top);

	G_OBJECT_CLASS(sliderview_parent_class)->dispose(object);
}

static void
sliderview_refresh(vObject *vobject)
{
	Sliderview *sliderview = SLIDERVIEW(vobject);
	Slider *slider = SLIDER(VOBJECT(sliderview)->iobject);
	Tslider *tslider = sliderview->tslider;

	const double range = slider->to - slider->from;
	const double lrange = log10(range);
	const char *caption = IOBJECT(slider)->caption;

#ifdef DEBUG
	printf("sliderview_refresh:\n");
	printf("\tslider->from = %g\n", slider->from);
	printf("\tslider->to = %g\n", slider->to);
	printf("\tslider->value = %g\n", slider->value);
#endif /*DEBUG*/

	/* Compatibility ... we used to not have a caption. Don't display
	 * anything if there's no caption.
	 */
	if (caption && !g_str_equal(caption, ""))
		set_glabel(sliderview->label, "%s:", caption);
	else
		set_glabel(sliderview->label, "%s", "");

	tslider->from = slider->from;
	tslider->to = slider->to;
	tslider->svalue = slider->value;
	tslider->value = slider->value;
	tslider->digits = VIPS_MAX(0, ceil(2 - lrange));
	tslider_changed(tslider);

	VOBJECT_CLASS(sliderview_parent_class)->refresh(vobject);
}

static void *
sliderview_scan(View *view)
{
	Sliderview *sliderview = SLIDERVIEW(view);
	Slider *slider = SLIDER(VOBJECT(sliderview)->iobject);
	Classmodel *classmodel = CLASSMODEL(slider);
	Expr *expr = HEAPMODEL(classmodel)->row->expr;

#ifdef DEBUG
	printf("sliderview_scan:\n");
#endif /*DEBUG*/

	double value;

	if (!get_geditable_double(sliderview->tslider->entry, &value)) {
		expr_error_set(expr);
		return (view);
	}

	if (slider->value != value) {
		slider->value = value;
		classmodel_update(classmodel);
	}

	return (VIEW_CLASS(sliderview_parent_class)->scan(view));
}

static void
sliderview_link(View *view, Model *model, View *parent)
{
	Sliderview *sliderview = SLIDERVIEW(view);

	VIEW_CLASS(sliderview_parent_class)->link(view, model, parent);

	if (GRAPHICVIEW(view)->sview)
		gtk_size_group_add_widget(GRAPHICVIEW(view)->sview->group,
			sliderview->label);
}

static void
sliderview_changed(Tslider *tslider, Sliderview *sliderview)
{
#ifdef DEBUG
	printf("sliderview_changed:\n");
#endif /*DEBUG*/

	Slider *slider = SLIDER(VOBJECT(sliderview)->iobject);

	if (slider->value != tslider->svalue) {
		slider->value = tslider->svalue;

		classmodel_update(CLASSMODEL(slider));
		symbol_recalculate_all();
	}
}

static void
sliderview_text_changed(Tslider *tslider, Sliderview *sliderview)
{
#ifdef DEBUG
	printf("sliderview_text_changed:\n");
#endif /*DEBUG*/

	// text must be scanned on next recomp
	view_scannable_register(VIEW(sliderview));
}

static void
sliderview_class_init(SliderviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("sliderview.ui");

	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class),
		GTK_TYPE_BIN_LAYOUT);

	BIND_VARIABLE(Sliderview, top);
	BIND_VARIABLE(Sliderview, label);
	BIND_VARIABLE(Sliderview, tslider);

	BIND_CALLBACK(sliderview_changed);
	BIND_CALLBACK(sliderview_text_changed);

	object_class->dispose = sliderview_dispose;

	vobject_class->refresh = sliderview_refresh;

	view_class->scan = sliderview_scan;
	view_class->link = sliderview_link;
}

static void
sliderview_init(Sliderview *sliderview)
{
	gtk_widget_init_template(GTK_WIDGET(sliderview));
	block_scroll(GTK_WIDGET(sliderview->tslider));
}

View *
sliderview_new(void)
{
	Sliderview *sliderview = g_object_new(SLIDERVIEW_TYPE, NULL);

	return (VIEW(sliderview));
}
