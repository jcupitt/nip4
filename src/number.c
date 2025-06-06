/* an editable number
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

G_DEFINE_TYPE(Number, number, CLASSMODEL_TYPE)

static View *
number_view_new(Model *model, View *parent)
{
	return numberview_new();
}

/* Members of number we automate.
 */
static ClassmodelMember number_members[] = {
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_CAPTION, "caption", N_("Caption"),
		G_STRUCT_OFFSET(iObject, caption) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		MEMBER_VALUE, "value", N_("Value"),
		G_STRUCT_OFFSET(Number, value) }
};

static void
number_class_init(NumberClass *class)
{
	iObjectClass *iobject_class = (iObjectClass *) class;
	ModelClass *model_class = (ModelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	/* Init methods.
	 */
	iobject_class->user_name = _("Number");

	model_class->view_new = number_view_new;

	/* Static init.
	 */
	model_register_loadable(MODEL_CLASS(class));

	classmodel_class->members = number_members;
	classmodel_class->n_members = VIPS_NUMBER(number_members);
}

static void
number_init(Number *number)
{
	number->value = 0.0;

	iobject_set(IOBJECT(number), CLASS_NUMBER, NULL);
}
