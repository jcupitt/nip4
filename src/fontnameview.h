/* a fontname view in a workspace
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

#define FONTNAMEVIEW_TYPE (fontnameview_get_type())
#define FONTNAMEVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), FONTNAMEVIEW_TYPE, Fontnameview))
#define FONTNAMEVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), FONTNAMEVIEW_TYPE, FontnameviewClass))
#define IS_FONTNAMEVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), FONTNAMEVIEW_TYPE))
#define IS_FONTNAMEVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), FONTNAMEVIEW_TYPE))
#define FONTNAMEVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), FONTNAMEVIEW_TYPE, FontnameviewClass))

typedef struct _Fontnameview {
	Graphicview parent_object;

	GtkWidget *top;
	GtkWidget *label;
	GtkWidget *button;
} Fontnameview;

typedef struct _FontnameviewClass {
	GraphicviewClass parent_class;

	/* My methods.
	 */
} FontnameviewClass;

GType fontnameview_get_type(void);
View *fontnameview_new(void);
