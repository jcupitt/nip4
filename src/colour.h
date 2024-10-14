/* a colour colour in a workspace
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

#define COLOUR_TYPE (colour_get_type())
#define COLOUR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), COLOUR_TYPE, Colour))
#define COLOUR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), COLOUR_TYPE, ColourClass))
#define IS_COLOUR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), COLOUR_TYPE))
#define IS_COLOUR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), COLOUR_TYPE))
#define COLOUR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), COLOUR_TYPE, ColourClass))

struct _Colour {
	Classmodel parent_class;

	/* Class fields.
	 */
	double value[3];
	char *colour_space;

	/* Build view caption here.
	 */
	VipsBuf caption;
};

typedef struct _ColourClass {
	ClassmodelClass parent_class;

	/* My methods.
	 */
} ColourClass;

Imageinfo *colour_ii_new(Colour *colour);
void colour_set_rgb(Colour *colour, double rgb[3]);

GType colour_get_type(void);
