/* abstract base class for graphic views, ie. things we can display as part of
 * the graphic component of a rhsview
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

#define GRAPHICVIEW_TYPE (graphicview_get_type())
#define GRAPHICVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), GRAPHICVIEW_TYPE, Graphicview))
#define GRAPHICVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), GRAPHICVIEW_TYPE, GraphicviewClass))
#define IS_GRAPHICVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GRAPHICVIEW_TYPE))
#define IS_GRAPHICVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GRAPHICVIEW_TYPE))
#define GRAPHICVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), GRAPHICVIEW_TYPE, GraphicviewClass))

typedef struct _Graphicview {
	View view;

	/* My instance vars.
	 */
	Subcolumnview *sview; /* Enclosing subc. */
} Graphicview;

typedef struct _GraphicviewClass {
	ViewClass parent_class;

	/* My methods.
	 */
} GraphicviewClass;

GType graphicview_get_type(void);
