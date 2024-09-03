/* Decls for imageinfo.c
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

/* Meta we attach for the filename we loaded from.
 */
#define ORIGINAL_FILENAME "original-filename"

/* Group imageinfo with this.
 */

#define IMAGEINFOGROUP_TYPE (imageinfogroup_get_type())
#define IMAGEINFOGROUP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), \
		IMAGEINFOGROUP_TYPE, Imageinfogroup))
#define IMAGEINFOGROUP_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), \
		IMAGEINFOGROUP_TYPE, ImageinfogroupClass))
#define IS_IMAGEINFOGROUP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGEINFOGROUP_TYPE))
#define IS_IMAGEINFOGROUP_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IMAGEINFOGROUP_TYPE))
#define IMAGEINFOGROUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), \
		IMAGEINFOGROUP_TYPE, ImageinfogroupClass))

typedef struct _Imageinfogroup {
	iContainer parent_object;

	/* Hash from filename to list of imageinfo. We can't use the
	 * icontainer hash, since our filenames are not unique (we can have
	 * the same file loaded several times, if some other application is
	 * changing our files behind our back).
	 */
	GHashTable *filename_hash;
} Imageinfogroup;

typedef struct _ImageinfogroupClass {
	iContainerClass parent_class;

} ImageinfogroupClass;

GType imageinfogroup_get_type(void);
Imageinfogroup *imageinfogroup_new(void);

/* An image.
 */

#define IMAGEINFO_TYPE (imageinfo_get_type())
#define IMAGEINFO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGEINFO_TYPE, Imageinfo))
#define IMAGEINFO_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IMAGEINFO_TYPE, ImageinfoClass))
#define IS_IMAGEINFO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGEINFO_TYPE))
#define IS_IMAGEINFO_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IMAGEINFO_TYPE))
#define IMAGEINFO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IMAGEINFO_TYPE, ImageinfoClass))

/* Attach one of these to any VipsImage we monitor. It has the same lifetime as
 * the VipsImage and gets zapped by the imageinfo on dispose. This lets us spot
 * VipsImage events after the holding Imageinfo has gone.
 */
typedef struct _Imageinfoproxy {
	VipsImage *image;
	Imageinfo *imageinfo;
} Imageinfoproxy;

/* A VIPS image wrapped up nicely.
 */
struct _Imageinfo {
	Managed parent_object;

	VipsImage *image;	   /* Image we manage */
	Imageinfoproxy *proxy; /* Proxy for VipsImage callbacks */

	gboolean from_file; /* Set if ->name is a user file */
	time_t mtime;		/* mtime when we loaded this file */

	/* Exprs which are thought to have this image as their value. See
	 * expr_value_new().
	 */
	GSList *exprs;

	/* Have we attached progress stuff to this ii?
	 */
	gboolean monitored;

	/* If we're from a file, the timestamp on the file we loaded from ...
	 * used to spot changes.
	 */
	time_t check_mtime;
	guint check_tid;
};

typedef struct _ImageinfoClass {
	ManagedClass parent_class;

	/* An area of the screen needs repainting. This can happen for regions
	 * being dragged, for example, and doesn't always mean pixels have
	 * changed.
	 */
	void (*area_changed)(Imageinfo *, VipsRect *);

	/* Our VipsImage* has signaled "invalidate".
	 */
	void (*invalidate)(Imageinfo *);

	/* The underlying file has changed ... higher levels should try to
	 * reload.
	 */
	void (*file_changed)(Imageinfo *);
} ImageinfoClass;

void *imageinfo_area_changed(Imageinfo *imageinfo, VipsRect *dirty);
void *imageinfo_expr_remove(Expr *expr, Imageinfo *imageinfo);
void imageinfo_expr_add(Imageinfo *imageinfo, Expr *expr);
GSList *imageinfo_expr_which(Imageinfo *imageinfo);

GType imageinfo_get_type(void);
Imageinfo *imageinfo_new(Imageinfogroup *imageinfogroup,
	Heap *heap, VipsImage *im, const char *name);
Imageinfo *imageinfo_new_temp(Imageinfogroup *imageinfogroup,
	Heap *heap, const char *name);
Imageinfo *imageinfo_new_from_pixbuf(Imageinfogroup *imageinfogroup,
	Heap *heap, GdkPixbuf *pixbuf);
gboolean imageinfo_is_from_file(Imageinfo *imageinfo);
Imageinfo *imageinfo_new_input(Imageinfogroup *imageinfogroup,
	GtkWidget *parent, Heap *heap, const char *name);
