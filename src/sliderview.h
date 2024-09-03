/* a sliderview in a workspace
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

#define SLIDERVIEW_TYPE (sliderview_get_type())
#define SLIDERVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), SLIDERVIEW_TYPE, Sliderview))
#define SLIDERVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), SLIDERVIEW_TYPE, SliderviewClass))
#define IS_SLIDERVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), SLIDERVIEW_TYPE))
#define IS_SLIDERVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), SLIDERVIEW_TYPE))
#define SLIDERVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), SLIDERVIEW_TYPE, SliderviewClass))

typedef struct _Sliderview {
	Graphicview parent_object;

	/* My instance vars.
	 */
	GtkWidget *top;
	GtkWidget *label;
	Tslider *tslider;
} Sliderview;

typedef struct _SliderviewClass {
	GraphicviewClass parent_class;

	/* My methods.
	 */
} SliderviewClass;

GType sliderview_get_type(void);
View *sliderview_new(void);
