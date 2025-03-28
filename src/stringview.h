/* edit a string
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

#define STRINGVIEW_TYPE (stringview_get_type())
#define STRINGVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), STRINGVIEW_TYPE, Stringview))
#define STRINGVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), STRINGVIEW_TYPE, StringviewClass))
#define IS_STRINGVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), STRINGVIEW_TYPE))
#define IS_STRINGVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), STRINGVIEW_TYPE))
#define STRINGVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), STRINGVIEW_TYPE, StringviewClass))

typedef struct _Stringview {
	Editview parent_object;

} Stringview;

typedef struct _StringviewClass {
	EditviewClass parent_class;

	/* My methods.
	 */
} StringviewClass;

GType stringview_get_type(void);
View *stringview_new(void);
