// watch an iobject and call _refresh on idle if it changes

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

#define VOBJECT_TYPE (vobject_get_type())
#define VOBJECT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), VOBJECT_TYPE, vObject))
#define VOBJECT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), VOBJECT_TYPE, vObjectClass))
#define IS_VOBJECT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), VOBJECT_TYPE))
#define IS_VOBJECT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), VOBJECT_TYPE))
#define VOBJECT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), VOBJECT_TYPE, vObjectClass))

typedef struct _vObject {
	GtkWidget parent_instance;

	iObject *iobject; /* weakref to iObject we are watching */
	gboolean dirty;	  /* In need of refreshment */
} vObject;

typedef struct _vObjectClass {
	GtkWidgetClass parent_class;

	/* State change

		refresh				refresh widgets (don't look at heap value,
							look at model)

		link				this vobject has been linked to an iobject

							we also have View::link() -- vObject::link is
							a lower-level link which is handy for views
							which are not Views, eg. toolkitbrowser

	 */
	void (*refresh)(vObject *);
	void (*link)(vObject *, iObject *);
} vObjectClass;

void *vobject_refresh_queue(vObject *vobject);

GType vobject_get_type(void);
void vobject_base_init(void);

void vobject_link(vObject *vobject, iObject *iobject);

void *vobject_refresh(vObject *vobject);
