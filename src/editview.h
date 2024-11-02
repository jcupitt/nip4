/* abstract base class for text editable view widgets
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

#define EDITVIEW_TYPE (editview_get_type())
#define EDITVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), EDITVIEW_TYPE, Editview))
#define EDITVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), EDITVIEW_TYPE, EditviewClass))
#define IS_EDITVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), EDITVIEW_TYPE))
#define IS_EDITVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), EDITVIEW_TYPE))
#define EDITVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), EDITVIEW_TYPE, EditviewClass))

typedef struct _Editview {
	Graphicview parent_object;

	/* Widgets.
	 */
	GtkWidget *top;
	GtkWidget *label;	/* Display caption here */
	GtkWidget *text;	/* Edit value here */
} Editview;

typedef struct _EditviewClass {
	GraphicviewClass parent_class;

	/* My methods.
	 */
} EditviewClass;

GType editview_get_type(void);
void editview_set_entry(Editview *editview, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
