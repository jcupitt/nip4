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

G_DEFINE_TYPE(Stringview, stringview, EDITVIEW_TYPE)

/* Re-read the text in a tally entry.
 */
static void *
stringview_scan(View *view)
{
	Stringview *stringview = STRINGVIEW(view);
	String *string = STRING(VOBJECT(stringview)->iobject);

#ifdef DEBUG
	Row *row = HEAPMODEL(string)->row;

	printf("stringview_scan: ");
	row_name_print(row);
	printf("\n");
#endif /*DEBUG*/

	g_autofree char *text = NULL;
	g_object_get(EDITVIEW(stringview)->ientry, "text", &text, NULL);
	if (text) {
		char value2[MAX_STRSIZE];

		my_strccpy(value2, text);
		if (!g_str_equal(string->value, value2)) {
			VIPS_SETSTR(string->value, value2);
			classmodel_update_view(CLASSMODEL(string));
		}
	}

	return VIEW_CLASS(stringview_parent_class)->scan(view);
}

static void
stringview_refresh(vObject *vobject)
{
	Stringview *stringview = STRINGVIEW(vobject);
	String *string = STRING(VOBJECT(stringview)->iobject);

#ifdef DEBUG
	Row *row = HEAPMODEL(string)->row;

	printf("stringview_refresh: ");
	row_name_print(row);
	printf(" (%p)\n", vobject);
#endif /*DEBUG*/

	if (string->value) {
		char txt[MAX_STRSIZE];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		vips_buf_appendsc(&buf, FALSE, string->value);
		editview_set_entry(EDITVIEW(stringview), "%s", vips_buf_all(&buf));
	}

	VOBJECT_CLASS(stringview_parent_class)->refresh(vobject);
}

static void
stringview_class_init(StringviewClass *class)
{
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	/* Create signals.
	 */

	/* Init methods.
	 */
	vobject_class->refresh = stringview_refresh;

	view_class->scan = stringview_scan;
}

static void
stringview_init(Stringview *stringview)
{
}

View *
stringview_new(void)
{
	Stringview *stringview = g_object_new(STRINGVIEW_TYPE, NULL);

	return VIEW(stringview);
}
