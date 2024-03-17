/* a managed gobject
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

#define MANAGEDGOBJECT_TYPE (managedgobject_get_type())
#define MANAGEDGOBJECT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), MANAGEDGOBJECT_TYPE, Managedgobject))
#define MANAGEDGOBJECT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), \
		MANAGEDGOBJECT_TYPE, ManagedgobjectClass))
#define IS_MANAGEDGOBJECT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), MANAGEDGOBJECT_TYPE))
#define IS_MANAGEDGOBJECT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), MANAGEDGOBJECT_TYPE))
#define MANAGEDGOBJECT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), \
		MANAGEDGOBJECT_TYPE, ManagedgobjectClass))

struct _Managedgobject {
	Managed parent_object;

	GObject *object;
};

typedef struct _ManagedgobjectClass {
	ManagedClass parent_class;

} ManagedgobjectClass;

GType managedgobject_get_type(void);

Managedgobject *managedgobject_new(Heap *heap, GObject *value);
