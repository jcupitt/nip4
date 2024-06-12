/* display a region in a workspace
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

#define IREGIONVIEW_TYPE (iregionview_get_type())
#define IREGIONVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IREGIONVIEW_TYPE, iRegionview))
#define IREGIONVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IREGIONVIEW_TYPE, iRegionviewClass))
#define IS_IREGIONVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IREGIONVIEW_TYPE))
#define IS_IREGIONVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IREGIONVIEW_TYPE))
#define IREGIONVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IREGIONVIEW_TYPE, iRegionviewClass))

typedef struct _iRegionview {
	iImageview parent_class;

} iRegionview;

typedef struct _iRegionviewClass {
	iImageviewClass parent_class;

	/* My methods.
	 */
} iRegionviewClass;

GType iregionview_get_type(void);
View *iregionview_new(void);
