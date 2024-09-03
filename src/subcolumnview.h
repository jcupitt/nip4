/* a column of tallyrows in a workspace
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

#define SUBCOLUMNVIEW_TYPE (subcolumnview_get_type())
#define SUBCOLUMNVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), SUBCOLUMNVIEW_TYPE, Subcolumnview))
#define SUBCOLUMNVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), SUBCOLUMNVIEW_TYPE, SubcolumnviewClass))
#define IS_SUBCOLUMNVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), SUBCOLUMNVIEW_TYPE))
#define IS_SUBCOLUMNVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), SUBCOLUMNVIEW_TYPE))
#define SUBCOLUMNVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), SUBCOLUMNVIEW_TYPE, SubcolumnviewClass))

struct _Subcolumnview {
	View view;

	/* Enclosing rhsview, if any.
	 */
	Rhsview *rhsview;

	/* My instance vars.
	 */
	GtkWidget *grid;		/* Central grid area for column */
	int n_rows;				/* Number of rows atm */
	int n_vis;				/* Number of children currently visible */

	GtkSizeGroup *group;	/* Alignment widget for children */
};

typedef struct _SubcolumnviewClass {
	ViewClass parent_class;

	/* My methods.
	 */
} SubcolumnviewClass;

GType subcolumnview_get_type(void);

View *subcolumnview_new(void);
