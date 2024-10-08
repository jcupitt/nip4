/* an input option ... put/get methods
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

G_DEFINE_TYPE(Option, option, CLASSMODEL_TYPE)

static void
option_finalize(GObject *gobject)
{
	Option *option;

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_OPTION(gobject));

	option = OPTION(gobject);

	/* My instance finalize stuff.
	 */
	g_slist_free_full(g_steal_pointer(&option->labels), g_free);

	G_OBJECT_CLASS(option_parent_class)->finalize(gobject);
}

static View *
option_view_new(Model *model, View *parent)
{
	return optionview_new();
}

/* Members of option we automate.
 */
static ClassmodelMember option_members[] = {
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_CAPTION, "caption", N_("Caption"),
		G_STRUCT_OFFSET(iObject, caption) },
	{ CLASSMODEL_MEMBER_STRING_LIST, NULL, 0,
		MEMBER_LABELS, "labels", N_("Labels"),
		G_STRUCT_OFFSET(Option, labels) },
	{ CLASSMODEL_MEMBER_INT, NULL, 0,
		MEMBER_VALUE, "value", N_("Value"),
		G_STRUCT_OFFSET(Option, value) }
};

static void
option_class_init(OptionClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	ModelClass *model_class = (ModelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	gobject_class->finalize = option_finalize;

	model_class->view_new = option_view_new;

	classmodel_class->members = option_members;
	classmodel_class->n_members = VIPS_NUMBER(option_members);

	model_register_loadable(MODEL_CLASS(class));
}

static void
option_init(Option *option)
{
	option->labels = NULL;
	option->value = 0;

	iobject_set(IOBJECT(option), CLASS_OPTION, NULL);
}
