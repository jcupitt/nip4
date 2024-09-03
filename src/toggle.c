/* a toggle button ... put/get methods
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

#include "nip4.h"

/*
#define DEBUG
 */

G_DEFINE_TYPE(Toggle, toggle, CLASSMODEL_TYPE)

static View *
toggle_view_new(Model *model, View *parent)
{
	return toggleview_new();
}

/* Members of toggle we automate.
 */
static ClassmodelMember toggle_members[] = {
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_CAPTION, "caption", N_("Caption"),
		G_STRUCT_OFFSET(iObject, caption) },
	{ CLASSMODEL_MEMBER_BOOLEAN, NULL, 0,
		MEMBER_VALUE, "value", N_("Value"),
		G_STRUCT_OFFSET(Toggle, value) }
};

static void
toggle_class_init(ToggleClass *class)
{
	ModelClass *model_class = (ModelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	model_class->view_new = toggle_view_new;

	classmodel_class->members = toggle_members;
	classmodel_class->n_members = VIPS_NUMBER(toggle_members);

	model_register_loadable(MODEL_CLASS(class));
}

static void
toggle_init(Toggle *toggle)
{
	iobject_set(IOBJECT(toggle), CLASS_TOGGLE, NULL);
}

