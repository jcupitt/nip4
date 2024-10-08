/* an input slider ... put/get methods
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

G_DEFINE_TYPE(Slider, slider, CLASSMODEL_TYPE)

static View *
slider_view_new(Model *model, View *parent)
{
	return (sliderview_new());
}

/* Members of slider we automate.
 */
static ClassmodelMember slider_members[] = {
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_CAPTION, "caption", N_("Caption"),
		G_STRUCT_OFFSET(iObject, caption) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		MEMBER_FROM, "from", N_("From"),
		G_STRUCT_OFFSET(Slider, from) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		MEMBER_TO, "to", N_("To"),
		G_STRUCT_OFFSET(Slider, to) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		MEMBER_VALUE, "value", N_("Value"),
		G_STRUCT_OFFSET(Slider, value) }
};

static void
slider_class_init(SliderClass *class)
{
	ModelClass *model_class = (ModelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	model_class->view_new = slider_view_new;

	classmodel_class->members = slider_members;
	classmodel_class->n_members = VIPS_NUMBER(slider_members);

	model_register_loadable(MODEL_CLASS(class));
}

static void
slider_init(Slider *slider)
{
	/* Overridden later. Just something sensible.
	 */
	slider->from = 0;
	slider->to = 255;
	slider->value = 128;

	/* Need to set caption to something too, since it's an automated
	 * member.
	 */
	iobject_set(IOBJECT(slider), CLASS_SLIDER, "");
}
