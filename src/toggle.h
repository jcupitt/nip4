/* a toggle button in a workspace
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

#define TOGGLE_TYPE (toggle_get_type())
#define TOGGLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TOGGLE_TYPE, Toggle))
#define TOGGLE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TOGGLE_TYPE, ToggleClass))
#define IS_TOGGLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TOGGLE_TYPE))
#define IS_TOGGLE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TOGGLE_TYPE))
#define TOGGLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TOGGLE_TYPE, ToggleClass))

typedef struct _Toggle {
	Classmodel parent_class;

	/* My instance vars.
	 */
	gboolean value;
} Toggle;

typedef struct _ToggleClass {
	ClassmodelClass parent_class;

	/* My methods.
	 */
} ToggleClass;

GType toggle_get_type(void);
