/* image management ... a layer over VipsImage
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

/*

jobs:

	- integration with GC in Managed base class

	- filesystem event tracking: we stat open files and signal file_changed
	  if we see a change

	- cache: several open("fred.v")s share a single Imageinfo,
	  provided their mtimes are all the same

	- imageinfo/expr association tracking, see expr_real_new_value()

 */

#include "nip4.h"

/*
#define DEBUG_RGB
#define DEBUG_CHECK
#define DEBUG_OPEN
#define DEBUG_MAKE
#define DEBUG
 */

G_DEFINE_TYPE(Imageinfogroup, imageinfogroup, ICONTAINER_TYPE)

static void
imageinfogroup_finalize(GObject *gobject)
{
	Imageinfogroup *imageinfogroup = IMAGEINFOGROUP(gobject);

#ifdef DEBUG_MAKE
	printf("imageinfogroup_finalize: %p\n", imageinfogroup);
#endif /*DEBUG_MAKE*/

	VIPS_FREEF(g_hash_table_destroy, imageinfogroup->filename_hash);

	G_OBJECT_CLASS(imageinfogroup_parent_class)->finalize(gobject);
}

static void
imageinfogroup_child_add(iContainer *parent, iContainer *child, int pos)
{
	Imageinfogroup *imageinfogroup = IMAGEINFOGROUP(parent);
	Imageinfo *imageinfo = IMAGEINFO(child);
	const char *name = IOBJECT(imageinfo)->name;

#ifdef DEBUG_MAKE
	printf("imageinfogroup_child_add: %s\n", name);
#endif /*DEBUG_MAKE*/

	GSList *hits;
	hits = (GSList *) g_hash_table_lookup(imageinfogroup->filename_hash, name);
	hits = g_slist_prepend(hits, imageinfo);
	g_hash_table_insert(imageinfogroup->filename_hash,
		(gpointer) name, (gpointer) hits);

	ICONTAINER_CLASS(imageinfogroup_parent_class)->child_add(parent, child, pos);
}

static void
imageinfogroup_child_remove(iContainer *parent, iContainer *child)
{
	Imageinfogroup *imageinfogroup = IMAGEINFOGROUP(parent);
	Imageinfo *imageinfo = IMAGEINFO(child);
	const char *name = IOBJECT(imageinfo)->name;

#ifdef DEBUG_MAKE
	printf("imageinfogroup_child_remove: %s\n", name);
#endif /*DEBUG_MAKE*/

	GSList *hits;
	hits = (GSList *) g_hash_table_lookup(imageinfogroup->filename_hash, name);
	g_assert(hits);
	hits = g_slist_remove(hits, imageinfo);

	/* child is going away (probably), so we don't want to link hits back
	 * on again with child->name as the key ... if possible, look down
	 * hits for another name we can use instead.
	 */
	if (hits) {
		const char *new_name = IOBJECT(hits->data)->name;

		g_hash_table_replace(imageinfogroup->filename_hash,
			(gpointer) new_name, (gpointer) hits);
	}
	else
		g_hash_table_remove(imageinfogroup->filename_hash,
			(gpointer) name);

	ICONTAINER_CLASS(imageinfogroup_parent_class)->child_remove(parent, child);
}

static void
imageinfogroup_class_init(ImageinfogroupClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	iContainerClass *icontainer_class = ICONTAINER_CLASS(class);

	gobject_class->finalize = imageinfogroup_finalize;

	icontainer_class->child_add = imageinfogroup_child_add;
	icontainer_class->child_remove = imageinfogroup_child_remove;
}

static void
imageinfogroup_init(Imageinfogroup *imageinfogroup)
{
#ifdef DEBUG_MAKE
	printf("imageinfogroup_init\n");
#endif /*DEBUG_MAKE*/

	imageinfogroup->filename_hash = g_hash_table_new(g_str_hash, g_str_equal);
}

Imageinfogroup *
imageinfogroup_new(void)
{
	Imageinfogroup *imageinfogroup =
		IMAGEINFOGROUP(g_object_new(IMAGEINFOGROUP_TYPE, NULL));

	return imageinfogroup;
}

static void *
imageinfogroup_lookup_test(Imageinfo *imageinfo, struct stat *buf)
{
	const char *name = IOBJECT(imageinfo)->name;

	if (name &&
		buf->st_mtime == imageinfo->mtime)
		return imageinfo;

	return NULL;
}

/* Look up by filename ... mtimes have to match too.
 */
static Imageinfo *
imageinfogroup_lookup(Imageinfogroup *imageinfogroup, const char *filename)
{
	GSList *hits;
	Imageinfo *imageinfo;
	struct stat buf;

	if (stat(filename, &buf) == 0 &&
		(hits = (GSList *) g_hash_table_lookup(
			 imageinfogroup->filename_hash, filename)) &&
		(imageinfo = IMAGEINFO(slist_map(hits,
			 (SListMapFn) imageinfogroup_lookup_test, &buf))))
		return imageinfo;

	return NULL;
}

/* Our signals.
 */
enum {
	SIG_AREA_CHANGED, /* Area of image has changed: update screen */
	SIG_INVALIDATE,	  /* VipsImage* has been invalidated */
	SIG_FILE_CHANGED, /* Underlying file seems to have changed */
	SIG_LAST
};

G_DEFINE_TYPE(Imageinfo, imageinfo, MANAGED_TYPE)

static guint imageinfo_signals[SIG_LAST] = { 0 };

#if defined(DEBUG) || defined(DEBUG_OPEN) || defined(DEBUG_RGB) || \
	defined(DEBUG_CHECK) || defined(DEBUG_MAKE)
static void
imageinfo_print(Imageinfo *imageinfo)
{
	printf(" \"%s\" mtime = %d (%p)\n",
		IOBJECT(imageinfo)->name, (int) imageinfo->mtime, imageinfo);
}
#endif

void *
imageinfo_area_changed(Imageinfo *imageinfo, VipsRect *dirty)
{
	g_assert(IS_IMAGEINFO(imageinfo));

#ifdef DEBUG
	printf("imageinfo_area_changed: "
		   "left = %d, top = %d, width = %d, height = %d\n",
		dirty->left, dirty->top, dirty->width, dirty->height);
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(imageinfo),
		imageinfo_signals[SIG_AREA_CHANGED], 0, dirty);

	return NULL;
}

static void *
imageinfo_file_changed(Imageinfo *imageinfo)
{
	g_assert(IS_IMAGEINFO(imageinfo));

#ifdef DEBUG_CHECK
	printf("imageinfo_file_changed:");
	imageinfo_print(imageinfo);
#endif /*DEBUG_CHECK*/

	g_signal_emit(G_OBJECT(imageinfo), imageinfo_signals[SIG_FILE_CHANGED], 0);

	return NULL;
}

static void *
imageinfo_invalidate(Imageinfo *imageinfo)
{
	g_assert(IS_IMAGEINFO(imageinfo));

#ifdef DEBUG_CHECK
	printf("imageinfo_invalidate:");
	imageinfo_print(imageinfo);
#endif /*DEBUG_CHECK*/

	g_signal_emit(G_OBJECT(imageinfo), imageinfo_signals[SIG_INVALIDATE], 0);

	return NULL;
}

void
imageinfo_expr_add(Imageinfo *imageinfo, Expr *expr)
{
#ifdef DEBUG
	printf("imageinfo_expr_add: ");
	expr_name_print(expr);
	printf("has imageinfo \"%s\" as value\n", imageinfo->image->filename);
#endif /*DEBUG*/

	g_assert(!g_slist_find(imageinfo->exprs, expr));
	g_assert(!expr->imageinfo);

	expr->imageinfo = imageinfo;
	imageinfo->exprs = g_slist_prepend(imageinfo->exprs, expr);
}

void *
imageinfo_expr_remove(Expr *expr, Imageinfo *imageinfo)
{
#ifdef DEBUG
	printf("imageinfo_expr_remove: ");
	expr_name_print(expr);
	printf("has lost imageinfo \"%s\" as value\n",
		imageinfo->image->filename);
#endif /*DEBUG*/

	g_assert(expr->imageinfo);
	g_assert(g_slist_find(imageinfo->exprs, expr));
	g_assert(expr->imageinfo == imageinfo);

	expr->imageinfo = NULL;
	imageinfo->exprs = g_slist_remove(imageinfo->exprs, expr);

	return NULL;
}

GSList *
imageinfo_expr_which(Imageinfo *imageinfo)
{
	return imageinfo->exprs;
}

void
imageinfo_set_delete_on_close(Imageinfo *imageinfo)
{
	// this must be a temp file of some kind
	imageinfo->from_file = FALSE;

	imageinfo->dfile = TRUE;
	VIPS_SETSTR(imageinfo->delete_filename, imageinfo->image->filename);
}

static void
imageinfo_dispose_eval(Imageinfo *imageinfo)
{
	imageinfo->monitored = FALSE;

	/* Make sure any callbacks from the VipsImage stop working.
	 */
	if (imageinfo->proxy) {
		imageinfo->proxy->imageinfo = NULL;
		imageinfo->proxy = NULL;
	}
}

static void
imageinfo_dispose(GObject *gobject)
{
	Imageinfo *imageinfo = IMAGEINFO(gobject);

#ifdef DEBUG_OPEN
	printf("imageinfo_dispose:");
	imageinfo_print(imageinfo);
#endif /*DEBUG_OPEN*/

	slist_map(imageinfo->exprs, (SListMapFn) imageinfo_expr_remove, imageinfo);
	g_assert(!imageinfo->exprs);

	imageinfo_dispose_eval(imageinfo);

	VIPS_FREEF(g_source_remove, imageinfo->check_tid);

	if (imageinfo->dfile &&
		imageinfo->delete_filename)
		unlinkf("%s", imageinfo->delete_filename);

	VIPS_UNREF(imageinfo->image);

	G_OBJECT_CLASS(imageinfo_parent_class)->dispose(gobject);
}

/* Final death!
 */
static void
imageinfo_finalize(GObject *gobject)
{
	Imageinfo *imageinfo = IMAGEINFO(gobject);

#ifdef DEBUG_MAKE
	printf("imageinfo_finalize:");
	imageinfo_print(imageinfo);
#endif /*DEBUG_MAKE*/

	VIPS_FREE(imageinfo->delete_filename);

	G_OBJECT_CLASS(imageinfo_parent_class)->finalize(gobject);
}

/* Make an info string about an imageinfo.
 */
static void
imageinfo_info(iObject *iobject, VipsBuf *buf)
{
	Imageinfo *imageinfo = IMAGEINFO(iobject);

	vips_buf_appendi(buf, imageinfo->image);

	/* Don't chain up to parent->info(), we don't want all the other
	 * stuff, this is going to be used for a caption.
	 */
}

static void
imageinfo_real_area_changed(Imageinfo *imageinfo, VipsRect *dirty)
{
}

static void
imageinfo_real_invalidate(Imageinfo *imageinfo)
{
}

static void
imageinfo_real_file_changed(Imageinfo *imageinfo)
{
}

static void
imageinfo_class_init(ImageinfoClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	iObjectClass *iobject_class = IOBJECT_CLASS(class);
	ManagedClass *managed_class = MANAGED_CLASS(class);

	gobject_class->dispose = imageinfo_dispose;
	gobject_class->finalize = imageinfo_finalize;

	iobject_class->info = imageinfo_info;

	/* Timeout on unreffed images.
	 */
	managed_class->keepalive = 60.0;

	class->area_changed = imageinfo_real_area_changed;
	class->invalidate = imageinfo_real_invalidate;
	class->file_changed = imageinfo_real_file_changed;

	/* Create signals.
	 */
	imageinfo_signals[SIG_AREA_CHANGED] = g_signal_new("area_changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ImageinfoClass, area_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	imageinfo_signals[SIG_INVALIDATE] = g_signal_new("invalidate",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ImageinfoClass, invalidate),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	imageinfo_signals[SIG_FILE_CHANGED] = g_signal_new("file_changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ImageinfoClass, file_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
imageinfo_init(Imageinfo *imageinfo)
{
#ifdef DEBUG_MAKE
	printf("imageinfo_init: %p\n", imageinfo);
#endif /*DEBUG_MAKE*/

	imageinfo->image = NULL;
	imageinfo->proxy = NULL;

	imageinfo->from_file = FALSE;
	imageinfo->mtime = 0;
	imageinfo->exprs = NULL;

	imageinfo->monitored = FALSE;

	imageinfo->check_mtime = 0;
	imageinfo->check_tid = 0;
}

static void
imageinfo_proxy_preeval(VipsImage *image, VipsProgress *progress,
	Imageinfoproxy *proxy)
{
	progress_begin();
}

static void
imageinfo_proxy_eval(VipsImage *image, VipsProgress *progress,
	Imageinfoproxy *proxy)
{
	Imageinfo *imageinfo = proxy->imageinfo;

	if (imageinfo &&
		image->time) {
		gboolean cancel;

		if (imageinfo_is_from_file(imageinfo))
			cancel = progress_update_loading(image->time->percent,
				IOBJECT(imageinfo)->name);
		else
			cancel = progress_update_percent(image->time->percent,
				image->time->eta);

		if (cancel)
			vips_image_set_kill(image, TRUE);
	}
}

static void
imageinfo_proxy_posteval(VipsImage *image, VipsProgress *progress,
	Imageinfoproxy *proxy)
{
	progress_end();
}

static void
imageinfo_proxy_invalidate(VipsImage *image, Imageinfoproxy *proxy)
{
	Imageinfo *imageinfo = proxy->imageinfo;

	if (imageinfo)
		imageinfo_invalidate(imageinfo);
}

static void
imageinfo_proxy_preclose(VipsImage *image, Imageinfoproxy *proxy)
{
	Imageinfo *imageinfo = proxy->imageinfo;

	/* Remove everything related to progress.
	 */
	if (imageinfo)
		imageinfo_dispose_eval(imageinfo);
}

/* Add a proxy to track VipsImage events.
 */
static void
imageinfo_proxy_add(Imageinfo *imageinfo)
{
	/* Only if we're running interactively.
	 */
	if (main_option_batch)
		return;

	/* Already being monitored?
	 */
	if (imageinfo->monitored)
		return;
	imageinfo->monitored = TRUE;

	/* Need a proxy on VipsImage.
	 */
	g_assert(!imageinfo->proxy);
	if (!(imageinfo->proxy = VIPS_NEW(imageinfo->image, Imageinfoproxy)))
		return;
	imageinfo->proxy->image = imageinfo->image;
	imageinfo->proxy->imageinfo = imageinfo;

	g_signal_connect(imageinfo->image, "preeval",
		G_CALLBACK(imageinfo_proxy_preeval), imageinfo->proxy);
	g_signal_connect(imageinfo->image, "eval",
		G_CALLBACK(imageinfo_proxy_eval), imageinfo->proxy);
	g_signal_connect(imageinfo->image, "posteval",
		G_CALLBACK(imageinfo_proxy_posteval), imageinfo->proxy);
	g_signal_connect(imageinfo->image, "invalidate",
		G_CALLBACK(imageinfo_proxy_invalidate), imageinfo->proxy);
	g_signal_connect(imageinfo->image, "preclose",
		G_CALLBACK(imageinfo_proxy_preclose), imageinfo->proxy);
}

/* Make a basic imageinfo. No refs, will be destroyed on next GC. If name is
 * NULL, make a temp name up; otherwise name needs to be unique.
 */
Imageinfo *
imageinfo_new(Imageinfogroup *imageinfogroup,
	Heap *heap, VipsImage *image, const char *name)
{
	Imageinfo *imageinfo = IMAGEINFO(g_object_new(IMAGEINFO_TYPE, NULL));

#ifdef DEBUG_OPEN
	printf("imageinfo_new: %p \"%s\"\n", imageinfo, image->filename);
	printf("\tfor image %p\n", image);
#endif /*DEBUG_OPEN*/

	managed_link_heap(MANAGED(imageinfo), heap);

	char buf[VIPS_PATH_MAX];
	if (!name) {
		if (!temp_name(buf, NULL, "v"))
			/* Will be freed on next GC.
			 */
			return NULL;

		name = buf;
	}
	iobject_set(IOBJECT(imageinfo), name, NULL);

	/* Only record the pointer when we know we will make the imageinfo
	 * successfully.
	 */
	imageinfo->image = image;

	icontainer_child_add(ICONTAINER(imageinfogroup), ICONTAINER(imageinfo), -1);
	imageinfo_proxy_add(imageinfo);

	return imageinfo;
}

/* Make a temp image. Deleted on close. No refs: closed on next GC. If you
 * want it to stick around, ref it!
 */
Imageinfo *
imageinfo_new_temp(Imageinfogroup *imageinfogroup,
	Heap *heap, const char *name)
{
	VipsImage *image;
	Imageinfo *imageinfo;

	if (!(image = vips_image_new_temp_file("%s.v")))
		return NULL;

	if (!(imageinfo = imageinfo_new(imageinfogroup, heap, image, name))) {
		VIPS_UNREF(image);
		return NULL;
	}

	return imageinfo;
}

/* Make a memory image. No refs: closed on next GC. If you
 * want it to stick around, ref it!
 */
Imageinfo *
imageinfo_new_memory(Imageinfogroup *imageinfogroup,
	Heap *heap, const char *name)
{
	VipsImage *image;
	Imageinfo *imageinfo;

	if (!(image = vips_image_new_memory()))
		return NULL;

	if (!(imageinfo = imageinfo_new(imageinfogroup, heap, image, name))) {
		VIPS_UNREF(image);
		return NULL;
	}

	return imageinfo;
}

/* Need this context during imageinfo_open_image_input().
 */
typedef struct _ImageinfoOpen {
	Imageinfogroup *imageinfogroup;
	Heap *heap;
	const char *filename;
	GtkWidget *parent;
} ImageinfoOpen;

/* Open for read ... returns a non-heap pointer, destroy if it goes in the
 * heap.
 */
static Imageinfo *
imageinfo_open_image_input(const char *filename, ImageinfoOpen *open)
{
	Imageinfo *imageinfo;
	VipsImage *image;

	// always open all pages, if we can
	if (!(image = vips_image_new_from_file(filename, "n", -1, NULL))) {
		vips_error_clear();
		if (!(image = vips_image_new_from_file(filename, NULL)))
			return NULL;
	}

	if (!(imageinfo = imageinfo_new(open->imageinfogroup,
			  open->heap, image, open->filename))) {
		VIPS_UNREF(image);
		return NULL;
	}
	MANAGED_REF(imageinfo);
	imageinfo->from_file = TRUE;

#ifdef DEBUG_OPEN
	printf("imageinfo_open_image_input: opened VIPS \"%s\"\n", filename);
#endif /*DEBUG_OPEN*/

	/* The rewind will have removed everything from the VipsImage. Reattach
	 * progress.
	 */
	imageinfo_proxy_add(imageinfo);

	/* Get ready for input.
	 */
	if (vips_image_pio_input(imageinfo->image))
		return NULL;

	/* Attach the original filename ... pick this up again later as a
	 * save default.
	 */
	vips_image_set_string(imageinfo->image, ORIGINAL_FILENAME, filename);

	return imageinfo;
}

Imageinfo *
imageinfo_new_from_pixbuf(Imageinfogroup *imageinfogroup,
	Heap *heap, GdkPixbuf *pixbuf)
{
	int width;
	int height;
	int bands;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	bands = gdk_pixbuf_get_n_channels(pixbuf);

	Imageinfo *ii;
	if (!(ii = imageinfo_new_temp(imageinfogroup, heap, NULL)))
		return NULL;
	vips_image_init_fields(ii->image, width, height, bands,
		VIPS_FORMAT_UCHAR, VIPS_CODING_NONE,
		VIPS_INTERPRETATION_sRGB, 1.0, 1.0);

	guint length;
	guchar *bytes = gdk_pixbuf_get_pixels_with_length(pixbuf, &length);
	size_t vips_length = VIPS_IMAGE_SIZEOF_LINE(ii->image) * height;
	if (vips_length != length) {
		error_top(_("Unable to create image"));
		error_sub(_("vips expected %zd bytes, gdkpixbuf made %d"),
			vips_length, length);
		return NULL;
	}

	if (vips_image_write_prepare(ii->image))
		return NULL;
	memcpy(ii->image->data, bytes, length);

	return ii;
}

static void
imageinfo_new_from_texture_free(VipsImage *image, GBytes *bytes)
{
	g_bytes_unref(bytes);
}

Imageinfo *
imageinfo_new_from_texture(Imageinfogroup *imageinfogroup,
	Heap *heap, GdkTexture *texture)
{
	g_autoptr(GBytes) bytes = gdk_texture_save_to_tiff_bytes(texture);
	if (!bytes) {
		error_top(_("Unable to create image"));
		return NULL;
	}

	gsize len;
	gconstpointer data = g_bytes_get_data(bytes, &len);
	VipsImage *image;
	if (!(image = vips_image_new_from_buffer(data, len, "", NULL))) {
		error_top(_("Unable to create image"));
		error_vips();
		return NULL;
	}

	// we can't have NULL filenames
	char buf[VIPS_PATH_MAX];
	if (!temp_name(buf, NULL, "tif")) {
		VIPS_UNREF(image);
		return NULL;
	}
	VIPS_SETSTR(image->filename, buf);

	g_signal_connect(image, "close",
		G_CALLBACK(imageinfo_new_from_texture_free), bytes);
	g_bytes_ref(bytes);

	return imageinfo_new(imageinfogroup, heap, image, NULL);
}

/* Was this ii loaded from a file (ie. ->name contains a filename the user
 * might recognise).
 */
gboolean
imageinfo_is_from_file(Imageinfo *imageinfo)
{
	return IOBJECT(imageinfo)->name &&
		imageinfo->from_file;
}

static gint
imageinfo_attach_check_cb(Imageinfo *imageinfo)
{
	if (imageinfo_is_from_file(imageinfo) &&
		imageinfo->check_tid) {
		struct stat buf;

		if (!stat(IOBJECT(imageinfo)->name, &buf) &&
			buf.st_mtime != imageinfo->check_mtime) {
			imageinfo->check_mtime = buf.st_mtime;
			imageinfo_file_changed(imageinfo);
		}
	}

	return TRUE;
}

/* Start checking this file for updates, signal reload if there is one.
 */
static void
imageinfo_attach_check(Imageinfo *imageinfo)
{
	if (imageinfo_is_from_file(imageinfo) &&
		!imageinfo->check_tid) {
		struct stat buf;

		/* Need to be able to stat() to be able to track a file.
		 */
		if (stat(IOBJECT(imageinfo)->name, &buf))
			return;

		imageinfo->mtime = buf.st_mtime;
		imageinfo->check_mtime = imageinfo->mtime;
		imageinfo->check_tid = g_timeout_add(1000,
			(GSourceFunc) imageinfo_attach_check_cb, imageinfo);

#ifdef DEBUG_CHECK
		printf("imageinfo_attach_check: starting to check");
		imageinfo_print(imageinfo);
#endif /*DEBUG_CHECK*/
	}
	else
		VIPS_FREEF(g_source_remove, imageinfo->check_tid);
}

/* Open a filename for input. The filename can have an embedded mode.
 */
Imageinfo *
imageinfo_new_input(Imageinfogroup *imageinfogroup, GtkWidget *parent,
	Heap *heap, const char *name)
{
	Imageinfo *imageinfo;
	ImageinfoOpen open;

#ifdef DEBUG_OPEN
	printf("imageinfo_new_input: %s\n", name);
#endif /*DEBUG_OPEN*/

	if ((imageinfo = imageinfogroup_lookup(imageinfogroup, name))) {
		/* We always make a new non-heap pointer.
		 */
		MANAGED_REF(imageinfo);
		return imageinfo;
	}

	open.imageinfogroup = imageinfogroup;
	open.heap = heap;
	open.filename = name;
	open.parent = parent;

	if (!(imageinfo = (Imageinfo *) callv_string_filename(
			  (callv_string_fn) imageinfo_open_image_input,
			  name, &open, NULL, NULL))) {
		error_top(_("Unable to open image"));
		error_sub(_("unable to open file \"%s\" as image"), name);
		error_vips();
		return NULL;
	}

	imageinfo_attach_check(imageinfo);

	return imageinfo;
}

/* Get the image as display RGB in rgb[0-2].
 */
void
imageinfo_get_rgb(Imageinfo *imageinfo, double rgb[3])
{
#ifdef DEBUG_RGB
	{
		char txt[256];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		printf("imageinfo_get_rgb: in: ");
		imageinfo_to_text(imageinfo, &buf);
		printf("%s\n", vips_buf_all(&buf));
	}
#endif /*DEBUG_RGB*/

	/* Use a tilesource to convert the imageinfo to RGB.
	 */
	g_autoptr(Tilesource) tilesource = tilesource_new_from_imageinfo(imageinfo);
	tilesource_set_synchronous(tilesource, TRUE);

	VipsRect area = { 0, 0, 1, 1 };
	if (vips_region_prepare(tilesource->image_region, &area))
		return;
	VipsPel *p = (VipsPel *)
		VIPS_REGION_ADDR(tilesource->image_region, area.left, area.top);

	if (imageinfo->image->Bands < 3)
		for (int i = 0; i < 3; i++)
			rgb[i] = p[0];
	else
		for (int i = 0; i < 3; i++)
			rgb[i] = p[i];

#ifdef DEBUG_RGB
	printf("imageinfo_get_rgb: out: r = %g, g = %g, b = %g\n",
		rgb[0], rgb[1], rgb[2]);
#endif /*DEBUG_RGB*/
}

/* Try to overwrite an imageinfo with a display RGB colour.
 */
void
imageinfo_set_rgb(Imageinfo *imageinfo, double rgb[3])
{
	Imageinfogroup *imageinfogroup =
		IMAGEINFOGROUP(ICONTAINER(imageinfo)->parent);
	VipsImage *image = imageinfo->image;
	Heap *heap = reduce_context->heap;

#ifdef DEBUG_RGB
	printf("imageinfo_set_rgb: in: r = %g, g = %g, b = %g\n",
		rgb[0], rgb[1], rgb[2]);
#endif /*DEBUG_RGB*/

	/* Make 1 pixel images for conversion.
	 */
	Imageinfo *in, *out;
	if (!(in = imageinfo_new_memory(imageinfogroup, heap, NULL)) ||
		!(out = imageinfo_new_memory(imageinfogroup, heap, NULL)))
		return;

	/* Fill in with rgb.
	 */
	VipsPel value8[3];
	for (int i = 0; i < 3; i++)
		value8[i] = rint(rgb[i]);
	vips_image_init_fields(in->image, 1, 1, 3,
		VIPS_FORMAT_UCHAR, VIPS_CODING_NONE,
		VIPS_INTERPRETATION_sRGB, 1.0, 1.0);
	if (vips_image_write_line(in->image, 0, value8))
		return;

	/* To imageinfo->type. Make sure we get a float ... except for LABQ
	 * and RAD.
	 */
	VipsImage **t = (VipsImage **)
		vips_object_local_array(VIPS_OBJECT(image), 2);
	if (image->Coding == VIPS_CODING_LABQ) {
		if (vips_colourspace(in->image, &t[0],
				VIPS_INTERPRETATION_LABQ, NULL) ||
			vips_image_write(t[0], out->image))
			return;
	}
	else if (image->Coding == VIPS_CODING_RAD) {
		if (vips_colourspace(in->image, &t[0], VIPS_INTERPRETATION_XYZ, NULL) ||
			vips_float2rad(t[0], &t[1], NULL) ||
			vips_image_write(t[1], out->image))
			return;
	}
	else if (image->Coding == VIPS_CODING_NONE) {
		if (vips_colourspace(in->image, &t[0], image->Type, NULL) ||
			vips_cast_float(t[0], &t[1], NULL) ||
			vips_image_write(t[1], out->image))
			return;
	}

#define SET(TYPE, i) \
	((TYPE *) image->data)[i] = ((float *) out->image->data)[i];

	/* Now ... overwrite imageinfo.
	 */
	if (image->Coding == VIPS_CODING_LABQ ||
		image->Coding == VIPS_CODING_RAD) {
		for (int i = 0; i < image->Bands; i++)
			((VipsPel *) image->data)[i] = ((VipsPel *) out->image->data)[i];
	}
	else {
		for (int i = 0; i < image->Bands; i++)
			switch (image->BandFmt) {
			case VIPS_FORMAT_UCHAR:
				SET(unsigned char, i);
				break;

			case VIPS_FORMAT_CHAR:
				SET(signed char, i);
				break;

			case VIPS_FORMAT_USHORT:
				SET(unsigned short, i);
				break;

			case VIPS_FORMAT_SHORT:
				SET(signed short, i);
				break;

			case VIPS_FORMAT_UINT:
				SET(unsigned int, i);
				break;

			case VIPS_FORMAT_INT:
				SET(signed int, i);
				break;

			case VIPS_FORMAT_FLOAT:
				SET(float, i);
				break;

			case VIPS_FORMAT_DOUBLE:
				SET(double, i);
				break;

			case VIPS_FORMAT_COMPLEX:
				SET(float, i * 2);
				SET(float, i * 2 + 1);
				break;

			case VIPS_FORMAT_DPCOMPLEX:
				SET(double, i * 2);
				SET(double, i * 2 + 1);
				break;

			default:
				g_assert(FALSE);
			}
	}
	vips_image_invalidate_all(image);

#ifdef DEBUG_RGB
	{
		char txt[256];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		printf("imageinfo_set_rgb: out: ");
		imageinfo_to_text(imageinfo, &buf);
		printf("%s\n", vips_buf_all(&buf));
	}
#endif /*DEBUG_RGB*/
}

/* Print a pixel. Output has to be parseable by imageinfo_from_text().
 */
void
imageinfo_to_text(Imageinfo *imageinfo, VipsBuf *buf)
{
	VipsImage *image = imageinfo->image;
	VipsPel *p = (VipsPel *) image->data;

#define PRINT_INT(T, I) vips_buf_appendf(buf, "%d", ((T *) p)[I]);
#define PRINT_FLOAT(T, I) vips_buf_appendg(buf, ((T *) p)[I]);

	for (int i = 0; i < image->Bands; i++) {
		if (i)
			vips_buf_appends(buf, ", ");

		switch (image->BandFmt) {
		case VIPS_FORMAT_UCHAR:
			PRINT_INT(unsigned char, i);
			break;

		case VIPS_FORMAT_CHAR:
			PRINT_INT(char, i);
			break;

		case VIPS_FORMAT_USHORT:
			PRINT_INT(unsigned short, i);
			break;

		case VIPS_FORMAT_SHORT:
			PRINT_INT(short, i);
			break;

		case VIPS_FORMAT_UINT:
			PRINT_INT(unsigned int, i);
			break;

		case VIPS_FORMAT_INT:
			PRINT_INT(int, i);
			break;

		case VIPS_FORMAT_FLOAT:
			PRINT_FLOAT(float, i);
			break;

		case VIPS_FORMAT_COMPLEX:
			vips_buf_appends(buf, "(");
			PRINT_FLOAT(float, (i << 1));
			vips_buf_appends(buf, ", ");
			PRINT_FLOAT(float, (i << 1) + 1);
			vips_buf_appends(buf, ")");
			break;

		case VIPS_FORMAT_DOUBLE:
			PRINT_FLOAT(double, i);
			break;

		case VIPS_FORMAT_DPCOMPLEX:
			vips_buf_appends(buf, "(");
			PRINT_FLOAT(double, i << 1);
			vips_buf_appends(buf, ", ");
			PRINT_FLOAT(double, (i << 1) + 1);
			vips_buf_appends(buf, ")");
			break;

		default:
			vips_buf_appends(buf, "???");
			break;
		}
	}
}
