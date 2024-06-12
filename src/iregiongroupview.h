/* coordinate the display of regionviews on imageviews
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

#define IREGIONGROUPVIEW_TYPE (iregiongroupview_get_type())
#define IREGIONGROUPVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IREGIONGROUPVIEW_TYPE, iRegiongroupview))
#define IREGIONGROUPVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IREGIONGROUPVIEW_TYPE, \
		 iRegiongroupviewClass))
#define IS_IREGIONGROUPVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IREGIONGROUPVIEW_TYPE))
#define IS_IREGIONGROUPVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IREGIONGROUPVIEW_TYPE))
#define IREGIONGROUPVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IREGIONGROUPVIEW_TYPE, \
		iRegiongroupviewClass))

typedef struct _iRegiongroupview {
	View parent_class;

	/* Keep our classmodel here, we need it during destroy.
	 */
	Classmodel *classmodel;

} iRegiongroupview;

typedef struct _iRegiongroupviewClass {
	ViewClass parent_class;

	/* My methods.
	 */
} iRegiongroupviewClass;

GType iregiongroupview_get_type(void);
View *iregiongroupview_new(void);
