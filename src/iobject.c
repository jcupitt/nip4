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

/*
#define DEBUG
 */

#include "nip4.h"

/* Our signals.
 */
enum {
	SIG_CHANGED, /* iObject has changed somehow */
	SIG_LAST
};

static GObjectClass *parent_class = NULL;

static guint iobject_signals[SIG_LAST] = { 0 };

void *
iobject_changed(iObject *iobject)
{
	g_return_val_if_fail(iobject != NULL, NULL);
	g_return_val_if_fail(IS_IOBJECT(iobject), NULL);

#ifdef DEBUG
	printf("iobject_changed: ");
	iobject_print(iobject);
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(iobject), iobject_signals[SIG_CHANGED], 0);

	return NULL;
}

void *
iobject_info(iObject *iobject, VipsBuf *buf)
{
	iObjectClass *iobject_class = IOBJECT_GET_CLASS(iobject);

	g_return_val_if_fail(iobject != NULL, NULL);
	g_return_val_if_fail(IS_IOBJECT(iobject), NULL);

	if (iobject_class->info)
		iobject_class->info(iobject, buf);

	return NULL;
}

static void
iobject_dispose(GObject *gobject)
{
#ifdef DEBUG
	iObject *iobject = IOBJECT(gobject);

	printf("iobject_dispose: ");
	iobject_print(iobject);
#endif /*DEBUG*/

	G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static void
iobject_finalize(GObject *gobject)
{
	iObject *iobject = IOBJECT(gobject);

#ifdef DEBUG
	printf("iobject_finalize: ");
	iobject_print(iobject);
#endif /*DEBUG*/

	VIPS_FREE(iobject->name);
	VIPS_FREE(iobject->caption);

	G_OBJECT_CLASS(parent_class)->finalize(gobject);
}

static void
iobject_real_changed(iObject *iobject)
{
	iObjectClass *iobject_class = IOBJECT_GET_CLASS(iobject);

	if (iobject_class->generate_caption)
		VIPS_SETSTR(iobject->caption, iobject_class->generate_caption(iobject));
}

static void
iobject_real_info(iObject *iobject, VipsBuf *buf)
{
	if (iobject->name)
		vips_buf_appendf(buf, "name = \"%s\"\n", iobject->name);
	if (iobject->caption)
		vips_buf_appendf(buf, "caption = \"%s\"\n", iobject->caption);
	vips_buf_appendf(buf, "iObject :: \"%s\"\n", G_OBJECT_TYPE_NAME(iobject));
}

static void
iobject_class_init(iObjectClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	parent_class = g_type_class_peek_parent(class);

	gobject_class->dispose = iobject_dispose;
	gobject_class->finalize = iobject_finalize;

	class->changed = iobject_real_changed;
	class->info = iobject_real_info;
	class->generate_caption = NULL;

	class->user_name = _("Object");

	/* Create signals.
	 */
	iobject_signals[SIG_CHANGED] = g_signal_new("changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(iObjectClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
iobject_init(iObject *iobject)
{
#ifdef DEBUG
	printf("iobject_init: ");
	iobject_print(iobject);
#endif /*DEBUG*/
}

GType
iobject_get_type(void)
{
	static GType iobject_type = 0;

	if (!iobject_type) {
		static const GTypeInfo info = {
			sizeof(iObjectClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) iobject_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(iObject),
			32, /* n_preallocs */
			(GInstanceInitFunc) iobject_init,
		};

		iobject_type = g_type_register_static(G_TYPE_OBJECT,
			"iObject", &info, 0);
	}

	return iobject_type;
}

/* Test the name field ... handy with map.
 */
void *
iobject_test_name(iObject *iobject, const char *name)
{
	g_return_val_if_fail(iobject != NULL, NULL);
	g_return_val_if_fail(IS_IOBJECT(iobject), NULL);

	if (iobject->name &&
		strcmp(iobject->name, name) == 0)
		return iobject;

	return NULL;
}

void *
iobject_print(iObject *iobject)
{
	g_print("%s \"%s\" (%p)\n",
		G_OBJECT_TYPE_NAME(iobject), iobject->name, iobject);

	return NULL;
}

void
iobject_set(iObject *iobject, const char *name, const char *caption)
{
	gboolean changed = FALSE;

	g_return_if_fail(iobject != NULL);
	g_return_if_fail(IS_IOBJECT(iobject));

	if (name &&
		name != iobject->name) {
		VIPS_SETSTR(iobject->name, name);
		changed = TRUE;
	}
	if (caption &&
		caption != iobject->caption) {
		VIPS_SETSTR(iobject->caption, caption);
		changed = TRUE;
	}

	if (changed)
		iobject_changed(iobject);

#ifdef DEBUG
	printf("iobject_set: ");
	iobject_print(iobject);
#endif /*DEBUG*/
}

void
iobject_dump(iObject *iobject)
{
	char txt[1000];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	iobject_info(iobject, &buf);
	printf("%s", vips_buf_all(&buf));
}
