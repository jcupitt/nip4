/* base model for a client regions on an imageview
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

#define IREGIONGROUP_TYPE (iregiongroup_get_type())
#define IREGIONGROUP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IREGIONGROUP_TYPE, iRegiongroup))
#define IREGIONGROUP_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IREGIONGROUP_TYPE, iRegiongroupClass))
#define IS_IREGIONGROUP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IREGIONGROUP_TYPE))
#define IS_IREGIONGROUP_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IREGIONGROUP_TYPE))
#define IREGIONGROUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IREGIONGROUP_TYPE, iRegiongroupClass))

struct _iRegiongroup {
	Classmodel parent_class;
};

typedef struct _iRegiongroupClass {
	ClassmodelClass parent_class;

	/* My methods.
	 */
} iRegiongroupClass;

GType iregiongroup_get_type(void);
iRegiongroup *iregiongroup_new(Classmodel *classmodel);
