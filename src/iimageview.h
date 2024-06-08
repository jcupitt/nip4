/* a iimageview in a workspace
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

#define IIMAGEVIEW_TYPE (iimageview_get_type())
#define IIMAGEVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IIMAGEVIEW_TYPE, iImageview))
#define IIMAGEVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IIMAGEVIEW_TYPE, iImageviewClass))
#define IS_IIMAGEVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IIMAGEVIEW_TYPE))
#define IS_IIMAGEVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IIMAGEVIEW_TYPE))
#define IIMAGEVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IIMAGEVIEW_TYPE, iImageviewClass))

typedef struct _iImageview {
	Graphicview parent_object;

	GtkWidget *top;
	Imagedisplay *imagedisplay;
	GtkWidget *label;



} iImageview;

typedef struct _iImageviewClass {
	GraphicviewClass parent_class;

	/* My methods.
	 */
} iImageviewClass;

GtkWidget *iimageview_drag_window_new(int width, int height);
GType iimageview_get_type(void);
View *iimageview_new(void);
