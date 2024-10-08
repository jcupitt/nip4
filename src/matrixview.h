/* a matrixview in a workspace
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

#define MATRIXVIEW_TYPE (matrixview_get_type())
#define MATRIXVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), MATRIXVIEW_TYPE, Matrixview))
#define MATRIXVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), MATRIXVIEW_TYPE, MatrixviewClass))
#define IS_MATRIXVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), MATRIXVIEW_TYPE))
#define IS_MATRIXVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), MATRIXVIEW_TYPE))
#define MATRIXVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), MATRIXVIEW_TYPE, MatrixviewClass))

typedef struct _Matrixview {
	Graphicview parent_object;

	// widgets
	GtkWidget *top;
	GtkWidget *swin;
	GtkWidget *grid;
	GtkWidget *scaleoffset;
	GtkWidget *scale;
	GtkWidget *offset;

	// currently displaying
	MatrixDisplayType display;
	GSList *items;
	int width;
	int height;
} Matrixview;

typedef struct _MatrixviewClass {
	GraphicviewClass parent_class;

	/* My methods.
	 */
} MatrixviewClass;

GType matrixview_get_type(void);
View *matrixview_new(void);
