/* an input fontname ... put/get methods
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

G_DEFINE_TYPE(Fontname, fontname, CLASSMODEL_TYPE)

static void
fontname_dispose(GObject *gobject)
{
	Fontname *fontname = FONTNAME(gobject);

#ifdef DEBUG
	printf("fontname_dispose\n");
#endif /*DEBUG*/

	VIPS_FREE(fontname->value);

	G_OBJECT_CLASS(fontname_parent_class)->dispose(gobject);
}

static View *
fontname_view_new(Model *model, View *parent)
{
	return (fontnameview_new());
}

static void *
fontname_update_model(Heapmodel *heapmodel)
{
#ifdef DEBUG
	printf("fontname_update_model\n");
#endif /*DEBUG*/

	if (HEAPMODEL_CLASS(fontname_parent_class)->update_model(heapmodel))
		return (heapmodel);

	return (NULL);
}

/* Members of fontname we automate.
 */
static ClassmodelMember fontname_members[] = {
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_CAPTION, "caption", N_("Caption"),
		G_STRUCT_OFFSET(iObject, caption) },
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_VALUE, "value", N_("Value"),
		G_STRUCT_OFFSET(Fontname, value) }
};

static void
fontname_class_init(FontnameClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	ModelClass *model_class = (ModelClass *) class;
	HeapmodelClass *heapmodel_class = (HeapmodelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	gobject_class->dispose = fontname_dispose;

	model_class->view_new = fontname_view_new;

	heapmodel_class->update_model = fontname_update_model;

	classmodel_class->members = fontname_members;
	classmodel_class->n_members = VIPS_NUMBER(fontname_members);

	model_register_loadable(MODEL_CLASS(class));
}

static void
fontname_init(Fontname *fontname)
{
	/* Overridden later. Just something sensible.
	 */
	fontname->value = NULL;
	VIPS_SETSTR(fontname->value, "Sans");

	iobject_set(IOBJECT(fontname), CLASS_FONTNAME, NULL);
}
