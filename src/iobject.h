// abstract base class for all nip objects

/*

	Copyright (C) 1991-2003 The National Gallery
	Copyright (C) 2004-2023 libvips.org

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

#define TYPE_IOBJECT (iobject_get_type())
#define IOBJECT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_IOBJECT, iObject))
#define IOBJECT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_IOBJECT, iObjectClass))
#define IS_IOBJECT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_IOBJECT))
#define IS_IOBJECT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_IOBJECT))
#define IOBJECT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_IOBJECT, iObjectClass))

typedef struct _iObject {
	// a gobject with floating references
	GInitiallyUnowned parent_object;

	/* My instance vars.
	 */
	char *name;	   /* iObject name */
	char *caption; /* Comment of some sort */

} iObject;

typedef struct _iObjectClass {
	GInitiallyUnownedClass parent_class;

	/* Something about the object has changed.
	 */
	void (*changed)(iObject *);

	/* Try and say something useful about us.
	 */
	void (*info)(iObject *, VipsBuf *);

	/* Called on _changed() to update the caption. Define this if you want
	 * the caption to be an explanatory note about the object.
	 */
	const char *(*generate_caption)(iObject *);

	/* The i18n name for this class we show the user. For example,
	 * Workspace is referred to as "tab" by the user.
	 */
	const char *user_name;
} iObjectClass;

#define IOBJECT_GET_CLASS_NAME(obj) \
	((G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_IOBJECT, iObjectClass))->user_name)

void *iobject_changed(iObject *iobject);
void *iobject_info(iObject *iobject, VipsBuf *);

GType iobject_get_type(void);

void *iobject_test_name(iObject *iobject, const char *name);
void *iobject_print(iObject *iobject);
void iobject_set(iObject *iobject, const char *name, const char *caption);
void iobject_dump(iObject *iobject);