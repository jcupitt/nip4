/* an input group ... put/get methods
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

G_DEFINE_TYPE(Group, group, VALUE_TYPE)

static gboolean group_save_list(PElement *list, char *filename);

/* Exported, since main.c uses this to save 'main' to a file. @filename is
 * incremented.
 */
gboolean
group_save_item(PElement *item, char *filename)
{
	gboolean result;
	Imageinfo *ii;
	char buf[FILENAME_MAX];

	/* We don't want $VAR etc. in the filename we pass down to the file
	 * ops.
	 */
	g_strlcpy(buf, filename, FILENAME_MAX);
	path_expand(buf);

	if (!heap_is_instanceof(CLASS_GROUP, item, &result))
		return FALSE;
	if (result) {
		PElement value;

		if (!class_get_member(item, MEMBER_VALUE, NULL, &value) ||
			!group_save_list(&value, filename))
			return FALSE;
	}

	if (!heap_is_instanceof(CLASS_IMAGE, item, &result))
		return FALSE;
	if (result) {
		PElement value;

		if (!class_get_member(item, MEMBER_VALUE, NULL, &value) ||
			!heap_get_image(&value, &ii))
			return FALSE;
		if (vips_image_write_to_file(ii->image, buf, NULL)) {
			error_vips_all();
			return FALSE;
		}

		increment_filename(filename);
	}

	if (PEISIMAGE(item)) {
		if (!heap_get_image(item, &ii))
			return FALSE;
		if (vips_image_write_to_file(ii->image, buf, NULL)) {
			error_vips_all();
			return FALSE;
		}

		increment_filename(filename);
	}

	if (PEISLIST(item)) {
		if (!group_save_list(item, filename))
			return FALSE;
	}

	return TRUE;
}

static gboolean
group_save_list(PElement *list, char *filename)
{
	int i;
	int length;

	if ((length = heap_list_length(list)) < 0)
		return FALSE;

	for(i = 0; i < length; i++) {
		PElement item;

		if (!heap_list_index(list, i, &item) ||
			!group_save_item(&item, filename))
			return FALSE;
	}

	return TRUE;
}

static gboolean
group_graphic_save(Classmodel *classmodel,
	GtkWindow *parent, const char *filename)
{
	Group *group = GROUP(classmodel);
	Row *row = HEAPMODEL(group)->row;
	PElement *root = &row->expr->root;
	char buf[FILENAME_MAX];

	/* We are going to increment the filename ... make sure there's some
	 * space at the end of the string.
	 */
	g_strlcpy(buf, filename, FILENAME_MAX - 5);

	if (!group_save_item(root, buf))
		return FALSE;

	return TRUE;
}

static void
group_class_init(GroupClass *class)
{
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	/* Create signals.
	 */

	classmodel_class->graphic_save = group_graphic_save;

	model_register_loadable(MODEL_CLASS(class));
}

static void
group_init(Group *group)
{
	iobject_set(IOBJECT(group), CLASS_GROUP, NULL);
}

