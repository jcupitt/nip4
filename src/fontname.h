/* a fontname in a workspace
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

#define FONTNAME_TYPE (fontname_get_type())
#define FONTNAME(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), FONTNAME_TYPE, Fontname))
#define FONTNAME_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), FONTNAME_TYPE, FontnameClass))
#define IS_FONTNAME(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), FONTNAME_TYPE))
#define IS_FONTNAME_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), FONTNAME_TYPE))
#define FONTNAME_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), FONTNAME_TYPE, FontnameClass))

typedef struct _Fontname {
	Classmodel model;

	char *value;
} Fontname;

typedef struct _FontnameClass {
	ClassmodelClass parent_class;

	/* My methods.
	 */
} FontnameClass;

GType fontname_get_type(void);
