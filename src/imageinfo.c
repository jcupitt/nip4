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

- reference counting layer ... in Managed base class, plus links to heap
  garbage collection

- filesystem tracking: we stat open files and signal file_changed if we see a
  change

- cache: several open( "fred.v" )s share a single Imageinfo, provided their
  mtimes are all the same

- lookup table management ... if an operation can work with pixel lookup
  tables (found by examining a flag in the VIPS function descriptor), then
  instead of operating on the image, the operation runs on the LUT associated
  with that image ... Imageinfo tracks the LUTs representing delayed eval

- dependency tracking ... an imageinfo can require several other imageinfos
  to be open for it to work properly; we follow these dependencies, and
  delay destroying an imageinfo until it's not required by any others

- temp file management ... we can make temp images on disc; we unlink() these
  temps when they're no longer needed

- imageinfo/expr association tracking ... we track when an expr
  receives an imageinfo as its value; the info is used to get region views
  to display in the right image ... see expr_real_new_value()

- paint stuff: also undo/redo buffers, each with a "*_changed" signal

 */

/*

more stuff:

while we transition to vips8, also use imageinfo to wrap VipsImage

most of the jobs above are pushed down into vips8 now ... except for

- reference counting layer ... in Managed base class

- filesystem tracking: we stat open files and signal file_changed if we see a
  change

- cache: several open( "fred.v" )s share a single Imageinfo, provided their
  mtimes are all the same

 */

#include "nip4.h"

/*
#define DEBUG_RGB
#define DEBUG_OPEN
#define DEBUG_CHECK
 */
#define DEBUG_MAKE
#define DEBUG

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
	GSList *hits;

#ifdef DEBUG_MAKE
	printf("imageinfogroup_child_add: %s\n", name);
#endif /*DEBUG_MAKE*/

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
	GSList *hits;

#ifdef DEBUG_MAKE
	printf("imageinfogroup_child_remove: %s\n", name);
#endif /*DEBUG_MAKE*/

	hits = (GSList *) g_hash_table_lookup(imageinfogroup->filename_hash,
		name);
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

	imageinfogroup->filename_hash =
		g_hash_table_new(g_str_hash, g_str_equal);
}

Imageinfogroup *
imageinfogroup_new(void)
{
	Imageinfogroup *imageinfogroup = IMAGEINFOGROUP(
		g_object_new(IMAGEINFOGROUP_TYPE, NULL));

	return imageinfogroup;
}

static void *
imageinfogroup_lookup_test(Imageinfo *imageinfo, struct stat *buf)
{
	const char *name = IOBJECT(imageinfo)->name;

	if (name && buf->st_mtime == imageinfo->mtime)
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
	SIG_AREA_PAINTED, /* Area of image has been painted */
	SIG_UNDO_CHANGED, /* Undo/redo state has changed */
	SIG_FILE_CHANGED, /* Underlying file seems to have changed */
	SIG_INVALIDATE,	  /* VipsImage* has been invalidated */
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
		IOBJECT(imageinfo)->name,
		(int) imageinfo->mtime,
		imageinfo);
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

void *
imageinfo_area_painted(Imageinfo *imageinfo, VipsRect *dirty)
{
	g_assert(IS_IMAGEINFO(imageinfo));

#ifdef DEBUG
	printf("imageinfo_area_painted: left = %d, top = %d, "
		   "width = %d, height = %d\n",
		dirty->left, dirty->top, dirty->width, dirty->height);
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(imageinfo),
		imageinfo_signals[SIG_AREA_PAINTED], 0, dirty);

	return NULL;
}

static void *
imageinfo_undo_changed(Imageinfo *imageinfo)
{
	g_assert(IS_IMAGEINFO(imageinfo));

	g_signal_emit(G_OBJECT(imageinfo), imageinfo_signals[SIG_UNDO_CHANGED], 0);

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
	printf("has imageinfo \"%s\" as value\n", imageinfo->im->filename);
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
		imageinfo->im->filename);
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

/* Find the underlying image in an imageinfo.
 */
VipsImage *
imageinfo_get_underlying(Imageinfo *imageinfo)
{
	if (imageinfo->underlying)
		return imageinfo_get_underlying(imageinfo->underlying);
	else
		return imageinfo->im;
}

/* Free up an undo fragment.
 */
static void
imageinfo_undofragment_free(Undofragment *frag)
{
	VIPS_UNREF(frag->im);
	VIPS_FREE(frag);
}

/* Free an undo buffer.
 */
static void
imageinfo_undobuffer_free(Undobuffer *undo)
{
	slist_map(undo->frags,
		(SListMapFn) imageinfo_undofragment_free, NULL);
	VIPS_FREEF(g_slist_free, undo->frags);
	VIPS_FREE(undo);
}

/* Free all undo information attached to an imageinfo.
 */
static void
imageinfo_undo_free(Imageinfo *imageinfo)
{
	slist_map(imageinfo->redo,
		(SListMapFn) imageinfo_undobuffer_free, NULL);
	VIPS_FREEF(g_slist_free, imageinfo->redo);
	slist_map(imageinfo->undo,
		(SListMapFn) imageinfo_undobuffer_free, NULL);
	VIPS_FREEF(g_slist_free, imageinfo->undo);
	VIPS_FREEF(imageinfo_undobuffer_free, imageinfo->cundo);
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

	G_OBJECT_CLASS(imageinfo_parent_class)->dispose(gobject);
}

/* Final death!
 */
static void
imageinfo_finalize(GObject *gobject)
{
	Imageinfo *imageinfo = IMAGEINFO(gobject);
	gboolean isfile = imageinfo->im && vips_image_isfile(imageinfo->im);

#ifdef DEBUG_MAKE
	printf("imageinfo_finalize:");
	imageinfo_print(imageinfo);
#endif /*DEBUG_MAKE*/

	VIPS_UNREF(imageinfo->im);
	VIPS_UNREF(imageinfo->mapped_im);
	VIPS_UNREF(imageinfo->identity_lut);

	MANAGED_UNREF(imageinfo->underlying);

	imageinfo_undo_free(imageinfo);

	G_OBJECT_CLASS(imageinfo_parent_class)->finalize(gobject);
}

/* Make an info string about an imageinfo.
 */
static void
imageinfo_info(iObject *iobject, VipsBuf *buf)
{
	Imageinfo *imageinfo = IMAGEINFO(iobject);

	vips_buf_appendi(buf, imageinfo_get(FALSE, imageinfo));

	/* Don't chain up to parent->info(), we don't want all the other
	 * stuff, this is going to be used for a caption.
	 */
}

static void
imageinfo_real_area_changed(Imageinfo *imageinfo, VipsRect *dirty)
{
}

static void
imageinfo_real_area_painted(Imageinfo *imageinfo, VipsRect *dirty)
{
	/* Cache attaches to this signal and invalidates on paint. Trigger a
	 * repaint in turn.
	 */
	imageinfo_area_changed(imageinfo, dirty);
}

static void
imageinfo_real_undo_changed(Imageinfo *imageinfo)
{
}

static void
imageinfo_real_file_changed(Imageinfo *imageinfo)
{
}

static void
imageinfo_real_invalidate(Imageinfo *imageinfo)
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
	class->area_painted = imageinfo_real_area_painted;
	class->undo_changed = imageinfo_real_undo_changed;
	class->file_changed = imageinfo_real_file_changed;
	class->invalidate = imageinfo_real_invalidate;

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
	imageinfo_signals[SIG_AREA_PAINTED] = g_signal_new("area_painted",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ImageinfoClass, area_painted),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	imageinfo_signals[SIG_UNDO_CHANGED] = g_signal_new("undo_changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ImageinfoClass, undo_changed),
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
	imageinfo_signals[SIG_INVALIDATE] = g_signal_new("invalidate",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ImageinfoClass, invalidate),
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

	imageinfo->im = NULL;
	imageinfo->mapped_im = NULL;
	imageinfo->identity_lut = NULL;
	imageinfo->underlying = NULL;
	imageinfo->proxy = NULL;

	imageinfo->dfile = FALSE;
	imageinfo->from_file = FALSE;
	imageinfo->mtime = 0;
	imageinfo->exprs = NULL;
	imageinfo->ok_to_paint = FALSE;
	imageinfo->undo = NULL;
	imageinfo->redo = NULL;
	imageinfo->cundo = NULL;

	imageinfo->monitored = FALSE;

	imageinfo->check_mtime = 0;
	imageinfo->check_tid = 0;
}

static int
imageinfo_proxy_eval(Imageinfoproxy *proxy)
{
	Imageinfo *imageinfo = proxy->imageinfo;

	printf("imageinfo_proxy_eval: FIXME\n");
	return;

	if (imageinfo && imageinfo->im->time)
		if (progress_update_percent(imageinfo->im->time->percent,
				imageinfo->im->time->eta))
			return -1;

	return 0;
}

static int
imageinfo_proxy_invalidate(Imageinfoproxy *proxy)
{
	Imageinfo *imageinfo = proxy->imageinfo;

	if (imageinfo)
		imageinfo_invalidate(imageinfo);

	return 0;
}

static int
imageinfo_proxy_preclose(Imageinfoproxy *proxy)
{
	Imageinfo *imageinfo = proxy->imageinfo;

	/* Remove everything related to progress.
	 */
	if (imageinfo)
		imageinfo_dispose_eval(imageinfo);

	return 0;
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
	if (!(imageinfo->proxy = VIPS_NEW(imageinfo->im, Imageinfoproxy)))
		return;
	imageinfo->proxy->im = imageinfo->im;
	imageinfo->proxy->imageinfo = imageinfo;

	g_signal_connect(imageinfo->im, "eval",
		G_CALLBACK(imageinfo_proxy_eval), imageinfo->proxy);
	g_signal_connect(imageinfo->im, "invalidate",
		G_CALLBACK(imageinfo_proxy_invalidate), imageinfo->proxy);
	g_signal_connect(imageinfo->im, "preclose",
		G_CALLBACK(imageinfo_proxy_preclose), imageinfo->proxy);
}

/* Make a basic imageinfo. No refs, will be destroyed on next GC. If name is
 * NULL, make a temp name up; otherwise name needs to be unique.
 */
Imageinfo *
imageinfo_new(Imageinfogroup *imageinfogroup,
	Heap *heap, VipsImage *im, const char *name)
{
	Imageinfo *imageinfo = IMAGEINFO(g_object_new(IMAGEINFO_TYPE, NULL));
	char buf[FILENAME_MAX];

#ifdef DEBUG_OPEN
	printf("imageinfo_new: %p \"%s\"\n", imageinfo, im->filename);
#endif /*DEBUG_OPEN*/

	managed_link_heap(MANAGED(imageinfo), heap);

	if (!name) {
		if (!temp_name(buf, "v"))
			/* Will be freed on next GC.
			 */
			return NULL;

		name = buf;
	}
	iobject_set(IOBJECT(imageinfo), name, NULL);

	/* Only record the pointer when we know we will make the imageinfo
	 * successfully.
	 */
	imageinfo->im = im;

	icontainer_child_add(ICONTAINER(imageinfogroup), ICONTAINER(imageinfo), -1);
	imageinfo_proxy_add(imageinfo);

	return imageinfo;
}

/* An image is a result of a LUT operation on an earlier imageinfo.
 */
void
imageinfo_set_underlying(Imageinfo *top_imageinfo, Imageinfo *imageinfo)
{
	g_assert(!top_imageinfo->underlying);

	top_imageinfo->underlying = imageinfo;
	MANAGED_REF(top_imageinfo->underlying);
}

/* Make a temp image. Deleted on close. No refs: closed on next GC. If you
 * want it to stick around, ref it!
 */
Imageinfo *
imageinfo_new_temp(Imageinfogroup *imageinfogroup,
	Heap *heap, const char *name)
{
	VipsImage *im;
	Imageinfo *imageinfo;

	if (!(im = vips_image_new_temp_file("%s.v")))
		return NULL;

	if (!(imageinfo = imageinfo_new(imageinfogroup, heap, im, name))) {
		VIPS_UNREF(im);
		return NULL;
	}
	imageinfo->dfile = TRUE;

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
	VipsImage *im;

	if (!(im = vips_image_new_from_file(filename, NULL)))
		return NULL;

	if (!(imageinfo = imageinfo_new(open->imageinfogroup,
			  open->heap, im, open->filename))) {
		VIPS_UNREF(im);
		return NULL;
	}
	MANAGED_REF(imageinfo);

#ifdef DEBUG_OPEN
	printf("imageinfo_open_image_input: opened VIPS \"%s\"\n", filename);
#endif /*DEBUG_OPEN*/

	/* The rewind will have removed everything from the VipsImage. Reattach
	 * progress.
	 */
	imageinfo_proxy_add(imageinfo);

	/* Get ready for input.
	 */
	if (vips_image_pio_input(imageinfo->im))
		return NULL;

	/* Attach the original filename ... pick this up again later as a
	 * save default.
	 */
	vips_image_set_string(imageinfo->im, ORIGINAL_FILENAME, filename);

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
	vips_image_init_fields(ii->im, width, height, bands,
		VIPS_FORMAT_UCHAR, VIPS_CODING_NONE,
		VIPS_INTERPRETATION_sRGB, 1.0, 1.0);

	guint length;
	guchar *bytes = gdk_pixbuf_get_pixels_with_length(pixbuf, &length);
	size_t vips_length = VIPS_IMAGE_SIZEOF_LINE(ii->im) * height;
	if (vips_length != length) {
		error_top(_("Unable to create image."));
		error_sub(_("vips expected %zd bytes, gdkpixbuf made %d"),
			vips_length, length);
		return NULL;
	}

	if (vips_image_write_prepare(ii->im))
		return NULL;
	memcpy(ii->im->data, bytes, length);

	return ii;
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

/* Open a filename for input. The filenmae can have an embedded mode.
 */
Imageinfo *
imageinfo_new_input(Imageinfogroup *imageinfogroup, GtkWidget *parent,
	Heap *heap, const char *name)
{
	Imageinfo *imageinfo;
	ImageinfoOpen open;

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
		error_top(_("Unable to open image."));
		error_sub(_("Unable to open file \"%s\" as image."),
			name);
		error_vips();
		return NULL;
	}

	imageinfo->from_file = TRUE;
	imageinfo_attach_check(imageinfo);

	return imageinfo;
}

/* Add an identity lut, if this is a LUTtable image.
 */
static VipsImage *
imageinfo_get_identity_lut(Imageinfo *imageinfo)
{
	if (imageinfo->im->Coding == VIPS_CODING_NONE &&
		imageinfo->im->BandFmt == VIPS_FORMAT_UCHAR) {
		if (!imageinfo->identity_lut) {
			if (vips_identity(&imageinfo->identity_lut,
					"bands",
					imageinfo->im->Bands,
					NULL))
				return NULL;
		}

		return imageinfo->identity_lut;
	}
	else
		return NULL;
}

static VipsImage *
imageinfo_get_mapped(Imageinfo *imageinfo)
{
	if (!imageinfo->mapped_im) {
		if (vips_maplut(imageinfo_get_underlying(imageinfo),
				&imageinfo->mapped_im, imageinfo->im, NULL)) {
			error_vips_all();
			return NULL;
		}
	}

	return imageinfo->mapped_im;
}

/* Get a lut ... or not!
 */
VipsImage *
imageinfo_get(gboolean use_lut, Imageinfo *imageinfo)
{
	if (!imageinfo)
		return NULL;

	if (use_lut && imageinfo->underlying)
		return imageinfo->im;
	if (use_lut && !imageinfo->underlying) {
		VipsImage *lut;

		if ((lut = imageinfo_get_identity_lut(imageinfo)))
			return lut;
		else
			return imageinfo->im;
	}
	else if (!use_lut && imageinfo->underlying)
		return imageinfo_get_mapped(imageinfo);
	else
		return imageinfo->im;
}

/* Do a set of II all refer to the same underlying image? Used to spot
 * LUTable optimisations.
 */
gboolean
imageinfo_same_underlying(Imageinfo *imageinfo[], int n)
{
	int i;

	if (n < 2)
		return TRUE;
	else {
		VipsImage *first = imageinfo_get_underlying(imageinfo[0]);

		for (i = 1; i < n; i++)
			if (imageinfo_get_underlying(imageinfo[i]) != first)
				return FALSE;

		return TRUE;
	}
}

/* Write to a filename.
 */
gboolean
imageinfo_write(Imageinfo *imageinfo, const char *name)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);

	if (vips_image_write_to_file(im, name, NULL)) {
		char filename[FILENAME_MAX];
		char mode[FILENAME_MAX];

		vips__filename_split8(name, filename, mode);
		error_top(_("Unable to write to file."));
		error_sub(_("Error writing image to file \"%s\"."), filename);
		error_vips();

		return FALSE;
	}

	return TRUE;
}

static gboolean
imageinfo_make_paintable(Imageinfo *imageinfo)
{
	progress_begin();
	if (vips_image_inplace(imageinfo->im)) {
		progress_end();
		error_top(_("Unable to paint on image."));
		error_sub(_("Unable to get write permission for "
					"file \"%s\".\nCheck permission settings."),
			imageinfo->im->filename);
		error_vips();
		return FALSE;
	}
	progress_end();

	imageinfo->ok_to_paint = TRUE;

	return TRUE;
}

/* Check painting is OK. nfn() called on "ok!". Returns FALSE if it's
 * not immediately obvious that we can paint.
 */
gboolean
imageinfo_check_paintable(Imageinfo *imageinfo, GtkWidget *parent)
{
	printf("imageinfo_check_paintable: FIXME ... needs a yesno dialog\n");

	return imageinfo_make_paintable(imageinfo);
}

/* Try to get an Imageinfo from a symbol.
 */
Imageinfo *
imageinfo_sym_image(Symbol *sym)
{
	PElement *root = &sym->expr->root;

	if (sym->type == SYM_VALUE && PEISIMAGE(root))
		return PEGETII(root);
	else
		return NULL;
}

static Undofragment *
imageinfo_undofragment_new(Undobuffer *undo)
{
	Undofragment *frag = INEW(NULL, Undofragment);

	frag->undo = undo;
	frag->im = NULL;

	return frag;
}

static Undobuffer *
imageinfo_undobuffer_new(Imageinfo *imageinfo)
{
	Undobuffer *undo = INEW(NULL, Undobuffer);

	undo->imageinfo = imageinfo;
	undo->frags = NULL;

	/* No pixels in bounding box at the moment.
	 */
	undo->bbox.left = 0;
	undo->bbox.top = 0;
	undo->bbox.width = 0;
	undo->bbox.height = 0;

	return undo;
}

/* Grab from the image into an VipsImage buffer. Always grab to memory.
 */
static VipsImage *
imageinfo_undo_grab_area(VipsImage *im, VipsRect *dirty)
{
	VipsImage *save;

	if (vips_crop(im, &save,
			dirty->left, dirty->top, dirty->width, dirty->height, NULL)) {
		error_vips_all();
		return NULL;
	}

	if (vips_image_wio_input(save)) {
		VIPS_UNREF(save);
		error_vips_all();
		return NULL;
	}

	return save;
}

/* Grab into an undo fragment. Add frag to frag list on undo buffer, expand
 * bounding box.
 */
static Undofragment *
imageinfo_undo_grab(Undobuffer *undo, VipsRect *dirty)
{
	Imageinfo *imageinfo = undo->imageinfo;
	Undofragment *frag = imageinfo_undofragment_new(undo);
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	VipsRect bbox;

	/* Try to extract from im. Memory allocation happens at this
	 * point, so we must be careful!
	 */
	if (!(frag->im = imageinfo_undo_grab_area(im, dirty))) {
		imageinfo_undofragment_free(frag);
		error_vips_all();
		return NULL;
	}

	/* Note position of this frag.
	 */
	frag->pos = *dirty;

	/* Add frag to frag list on undo buffer.
	 */
	undo->frags = g_slist_prepend(undo->frags, frag);

	/* Find bounding box for saved pixels.
	 */
	vips_rect_unionrect(dirty, &undo->bbox, &bbox);
	undo->bbox = bbox;

	/* Return new frag.
	 */
	return frag;
}

/* Trim the undo buffer if we have more than x items on it.
 */
static void
imageinfo_undo_trim(Imageinfo *imageinfo)
{
	int max = PAINTBOX_MAX_UNDO;
	int len = g_slist_length(imageinfo->undo);

	if (max >= 0 && len > max) {
		GSList *l;
		int i;

		l = g_slist_reverse(imageinfo->undo);

		for (i = 0; i < len - max; i++) {
			Undobuffer *undo = (Undobuffer *) l->data;

			imageinfo_undobuffer_free(undo);
			l = g_slist_remove(l, undo);
		}

		imageinfo->undo = g_slist_reverse(l);
	}

#ifdef DEBUG
	printf("imageinfo_undo_trim: %d items in undo buffer\n",
		g_slist_length(imageinfo->undo));
#endif /*DEBUG*/
}

/* Mark the start or end of an undo session. Copy current undo information
 * to the undo buffers and NULL out the current undo pointer. Junk all redo
 * information: this new undo action makes all that out of date.
 */
void
imageinfo_undo_mark(Imageinfo *imageinfo)
{
	/* Is there an existing undo save area?
	 */
	if (imageinfo->cundo) {
		/* Left over from the last undo save. Copy to undo save list
		 * and get ready for new undo buffer.
		 */
		imageinfo->undo =
			g_slist_prepend(imageinfo->undo, imageinfo->cundo);
		imageinfo->cundo = NULL;
	}

	/* Junk all redo information.
	 */
	slist_map(imageinfo->redo,
		(SListMapFn) imageinfo_undobuffer_free, NULL);
	VIPS_FREEF(g_slist_free, imageinfo->redo);

	/* Trim undo buffer.
	 */
	imageinfo_undo_trim(imageinfo);

	/* Update menus.
	 */
	imageinfo_undo_changed(imageinfo);
}

/* Add to the undo buffer. If there is no undo buffer currently under
 * construction, make a new one. If there is an existing undo buffer, try to
 * grow it left/right/up/down so as to just enclose the new bounding box. We
 * assume that our dirty areas are not going to be disconnected. Is this
 * always true? No - if you move smudge or smear quickly, you can get
 * non-overlapping areas. However: if you do lots of little operations in more
 * or less the same place (surely the usual case), then this technique will be
 * far better.
 */
static gboolean
imageinfo_undo_add(Imageinfo *imageinfo, VipsRect *dirty)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	Undobuffer *undo = imageinfo->cundo;
	VipsRect over, image, clipped;

	/* Undo disabled? Do nothing.
	 */
	if (PAINTBOX_MAX_UNDO == 0)
		return TRUE;

	/* Clip dirty against image size.
	 */
	image.left = 0;
	image.top = 0;
	image.width = im->Xsize;
	image.height = im->Ysize;
	vips_rect_intersectrect(&image, dirty, &clipped);

	/* Is there anything left? If not, can return immediately.
	 */
	if (vips_rect_isempty(&clipped))
		return TRUE;

	if (!undo) {
		/* No current undo buffer ... start a new one for this action.
		 */
		if (!(imageinfo->cundo = undo =
					imageinfo_undobuffer_new(imageinfo)))
			return FALSE;

		return imageinfo_undo_grab(undo, &clipped) != NULL;
	}

	/* Existing stuff we are to add to. Try to expand our undo
	 * area to just enclose the new bounding box. We assume that
	 * there is an overlap between the new and old stuff.
	 */

	/* Do we need to expand our saved area to the right?
	 */
	if (VIPS_RECT_RIGHT(&clipped) > VIPS_RECT_RIGHT(&undo->bbox)) {
		/* Expand to the right. Calculate the section we need
		 * to add to our bounding box.
		 */
		over.left = VIPS_RECT_RIGHT(&undo->bbox);
		over.top = undo->bbox.top;
		over.width = VIPS_RECT_RIGHT(&clipped) -
			VIPS_RECT_RIGHT(&undo->bbox);
		over.height = undo->bbox.height;

		/* Grab new fragment.
		 */
		if (!imageinfo_undo_grab(undo, &over))
			return FALSE;
	}

	/* Do we need to expand our saved area to the left?
	 */
	if (undo->bbox.left > clipped.left) {
		over.left = clipped.left;
		over.top = undo->bbox.top;
		over.width = undo->bbox.left - clipped.left;
		over.height = undo->bbox.height;

		if (!imageinfo_undo_grab(undo, &over))
			return FALSE;
	}

	/* Do we need to expand our saved area upwards?
	 */
	if (undo->bbox.top > clipped.top) {
		over.left = undo->bbox.left;
		over.top = clipped.top;
		over.width = undo->bbox.width;
		over.height = undo->bbox.top - clipped.top;

		if (!imageinfo_undo_grab(undo, &over))
			return FALSE;
	}

	/* Do we need to expand our saved area downwards?
	 */
	if (VIPS_RECT_BOTTOM(&clipped) > VIPS_RECT_BOTTOM(&undo->bbox)) {
		over.left = undo->bbox.left;
		over.top = VIPS_RECT_BOTTOM(&undo->bbox);
		over.width = undo->bbox.width;
		over.height = VIPS_RECT_BOTTOM(&clipped) -
			VIPS_RECT_BOTTOM(&undo->bbox);

		if (!imageinfo_undo_grab(undo, &over))
			return FALSE;
	}

	return TRUE;
}

/* Paste an undo fragment back into the image.
 */
static void *
imageinfo_undofragment_paste(Undofragment *frag)
{
	Undobuffer *undo = frag->undo;
	Imageinfo *imageinfo = undo->imageinfo;
	VipsImage *im = imageinfo_get(FALSE, imageinfo);

	vips_draw_image(im, frag->im, frag->pos.left, frag->pos.top, NULL);
	imageinfo_area_painted(imageinfo, &frag->pos);

	return NULL;
}

/* Paste a whole undo buffer back into the image.
 */
static void
imageinfo_undobuffer_paste(Undobuffer *undo)
{
	slist_map(undo->frags,
		(SListMapFn) imageinfo_undofragment_paste, NULL);
}

/* Undo a paint action.
 */
gboolean
imageinfo_undo(Imageinfo *imageinfo)
{
	Undobuffer *undo;

	/* Find the undo action we are to perform.
	 */
	if (!imageinfo->undo)
		return TRUE;
	undo = (Undobuffer *) imageinfo->undo->data;

	/* We are going to undo the first action on the undo list. We must
	 * save the area under the first undo action to the redo list. Do
	 * the save, even if undo is disabled.
	 */
	if (!imageinfo_undo_add(imageinfo, &undo->bbox))
		return FALSE;

	/* Add new undo area.
	 */
	imageinfo->redo = g_slist_prepend(imageinfo->redo, imageinfo->cundo);
	imageinfo->cundo = NULL;

	/* Paint undo back.
	 */
	imageinfo_undobuffer_paste(undo);

	/* Junk the undo action we have performed.
	 */
	imageinfo->undo = g_slist_remove(imageinfo->undo, undo);
	imageinfo_undobuffer_free(undo);

	/* Trim undo buffer.
	 */
	imageinfo_undo_trim(imageinfo);

	/* Update menus.
	 */
	imageinfo_undo_changed(imageinfo);

	return TRUE;
}

/* Redo a paint action, if possible.
 */
gboolean
imageinfo_redo(Imageinfo *imageinfo)
{
	Undobuffer *undo;

	/* Find the redo action we are to perform.
	 */
	if (!imageinfo->redo)
		return TRUE;
	undo = (Undobuffer *) imageinfo->redo->data;

	/* We are going to redo the first action on the redo list. We must
	 * save the area under the first redo action to the undo list. Save
	 * even if undo is disabled.
	 */
	if (!imageinfo_undo_add(imageinfo, &undo->bbox))
		return FALSE;

	/* Add this new buffer to the undo list.
	 */
	imageinfo->undo = g_slist_prepend(imageinfo->undo, imageinfo->cundo);
	imageinfo->cundo = NULL;

	/* Paint redo back.
	 */
	imageinfo_undobuffer_paste(undo);

	/* We can junk the head of the undo list now.
	 */
	imageinfo->redo = g_slist_remove(imageinfo->redo, undo);
	imageinfo_undobuffer_free(undo);

	/* Trim undo buffer.
	 */
	imageinfo_undo_trim(imageinfo);

	/* Update menus.
	 */
	imageinfo_undo_changed(imageinfo);

	return TRUE;
}

void
imageinfo_undo_clear(Imageinfo *imageinfo)
{
	imageinfo_undo_free(imageinfo);
	imageinfo_undo_changed(imageinfo);
}

typedef struct _ImageinfoDraw {
	void *a;
	void *b;
	void *c;

} ImageinfoDraw;

static void
imageinfo_draw_point_cb(VipsImage *im, int x, int y, void *client)
{
	ImageinfoDraw *draw = (ImageinfoDraw *) client;
	VipsImage *mask = (VipsImage *) draw->a;
	VipsPel *ink = (VipsPel *) draw->b;

	double *vec;
	int n;
	if ((vec = ink_to_vector("point", im, ink, &n)))
		(void) vips_draw_mask(im, vec, n, mask,
			x - mask->Xsize / 2, y - mask->Ysize / 2, NULL);
}

/* Draw a line.
 */
gboolean
imageinfo_paint_line(Imageinfo *imageinfo,
	Imageinfo *ink, Imageinfo *mask,
	int x1, int y1, int x2, int y2)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	VipsImage *ink_im = imageinfo_get(FALSE, ink);
	VipsImage *mask_im = imageinfo_get(FALSE, mask);
	VipsPel *data = (VipsPel *) ink_im->data;
	ImageinfoDraw draw = { mask_im, data };

	VipsRect dirty, p1, p2, image, clipped;

	p1.width = mask_im->Xsize;
	p1.height = mask_im->Ysize;
	p1.left = x1 - mask_im->Xsize / 2;
	p1.top = y1 - mask_im->Ysize / 2;
	p2.width = mask_im->Xsize;
	p2.height = mask_im->Ysize;
	p2.left = x2 - mask_im->Xsize / 2;
	p2.top = y2 - mask_im->Ysize / 2;
	vips_rect_unionrect(&p1, &p2, &dirty);

	image.left = 0;
	image.top = 0;
	image.width = im->Xsize;
	image.height = im->Ysize;
	vips_rect_intersectrect(&dirty, &image, &clipped);

	if (vips_rect_isempty(&clipped))
		return TRUE;

	if (!imageinfo_undo_add(imageinfo, &clipped))
		return FALSE;

	draw_line(im, x1, y1, x2, y2, imageinfo_draw_point_cb, &draw);

	imageinfo_area_painted(imageinfo, &dirty);

	return TRUE;
}

static void
imageinfo_draw_smudge_cb(VipsImage *im, int x, int y, void *client)
{
	ImageinfoDraw *draw = (ImageinfoDraw *) client;
	VipsRect *oper = (VipsRect *) draw->a;

	(void) vips_draw_smudge(im,
		oper->left, oper->top, oper->width, oper->height, NULL);
}

/* Smudge a line.
 */
gboolean
imageinfo_paint_smudge(Imageinfo *imageinfo,
	VipsRect *oper, int x1, int y1, int x2, int y2)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	ImageinfoDraw draw = { oper };

	VipsRect p1, p2, dirty;

	/* Calculate bounding box for smudge.
	 */
	p1 = *oper;
	p1.left += x1;
	p1.top += y1;
	p2 = *oper;
	p2.left += x2;
	p2.top += y2;
	vips_rect_unionrect(&p1, &p2, &dirty);
	if (!imageinfo_undo_add(imageinfo, &dirty))
		return FALSE;

	draw_line(im, x1, y1, x2, y2, imageinfo_draw_smudge_cb, &draw);

	imageinfo_area_painted(imageinfo, &dirty);

	return TRUE;
}

/* Flood an area.
 */
gboolean
imageinfo_paint_flood(Imageinfo *imageinfo, Imageinfo *ink,
	int x, int y, gboolean blob)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	VipsImage *ink_im = imageinfo_get(FALSE, ink);
	VipsPel *data = (VipsPel *) ink_im->data;

	/* Save undo area. We have to save the entire image since we don't know
	 * how much the flood will change :(
	 */
	if (!imageinfo_undo_add(imageinfo,
			&((VipsRect){ 0, 0, im->Xsize, im->Ysize })))
		return FALSE;

	double *vec;
	int n;
	VipsRect dirty;
	if (!(vec = ink_to_vector("flood", im, data, &n)) ||
		vips_draw_flood(im, vec, n, x, y,
			"equal", blob,
			"left", &dirty.left,
			"top", &dirty.top,
			"width", &dirty.width,
			"height", &dirty.height,
			NULL)) {
		error_vips_all();
		return FALSE;
	}

	imageinfo_area_painted(imageinfo, &dirty);

	return TRUE;
}

gboolean
imageinfo_paint_dropper(Imageinfo *imageinfo, Imageinfo *ink, int x, int y)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	VipsImage *ink_im = imageinfo_get(FALSE, ink);

	g_autoptr(VipsImage) t1 = NULL;

	if (vips_crop(im, &t1, x, y, 1, 1, NULL) ||
		vips_image_write(t1, ink_im)) {
		error_vips_all();
		return FALSE;
	}

	vips_image_invalidate_all(ink_im);
	imageinfo_area_painted(ink, &((VipsRect){ 0, 0, 1, 1 }));

	return TRUE;
}

/* Fill a rect.
 */
gboolean
imageinfo_paint_rect(Imageinfo *imageinfo, Imageinfo *ink, VipsRect *area)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	VipsImage *ink_im = imageinfo_get(FALSE, ink);
	VipsPel *data = (VipsPel *) ink_im->data;

	if (!imageinfo_undo_add(imageinfo, area))
		return FALSE;

	double *vec;
	int n;
	if (!(vec = ink_to_vector("rect", im, data, &n)) ||
		vips_draw_rect(im, vec, n,
			area->left, area->top, area->width, area->height,
			"fill", TRUE,
			NULL)) {
		error_vips_all();
		return FALSE;
	}

	imageinfo_area_painted(imageinfo, area);

	return TRUE;
}

/* Paint text into imageinfo, return width/height in tarea.
 */
gboolean
imageinfo_paint_text(Imageinfo *imageinfo,
	const char *font_name, const char *text, VipsRect *tarea)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);

	g_autoptr(VipsImage) t1 = NULL;

	if (vips_text(&t1, text,
			"font", font_name,
			"dpi", get_dpi(),
			NULL) ||
		vips_image_write(t1, im)) {
		error_top(_("Unable to paint text."));
		error_sub(_("Unable to paint text \"%s\" in font \"%s\"."),
			text, font_name);
		error_vips();

		return FALSE;
	}

	tarea->left = 0;
	tarea->top = 0;
	tarea->width = im->Xsize;
	tarea->height = im->Ysize;

	return TRUE;
}

/* Draw a nib mask. Radius 0 means a single-pixel mask.
 */
gboolean
imageinfo_paint_nib(Imageinfo *imageinfo, int radius)
{
	static VipsPel ink[1] = { 255 };

	VipsImage *im = imageinfo_get(FALSE, imageinfo);

	printf("imageinfo_paint_nib: FIXME\n");
	/*
	if (radius) {
		int r2 = radius * 2;
		VipsImage *t;

		if (!(t = im_open("imageinfo_paint_nib", "p"))) {
			error_vips();
			return FALSE;
		}
		if (im_black(t, 2 * (r2 + 1), 2 * (r2 + 1), 1) ||
			im_draw_circle(t, r2, r2, r2, 1, ink) ||
			im_shrink(t, im, 2, 2)) {
			im_close(t);
			error_vips();
			return FALSE;
		}
		im_close(t);
	}
	else {
		if (im_black(im, 1, 1, 1) ||
			im_draw_circle(im, 0, 0, 0, 1, ink))
			return FALSE;
	}
	 */

	return TRUE;
}

/* Paint a mask.
 */
gboolean
imageinfo_paint_mask(Imageinfo *imageinfo,
	Imageinfo *ink, Imageinfo *mask, int x, int y)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	VipsImage *ink_im = imageinfo_get(FALSE, ink);
	VipsImage *mask_im = imageinfo_get(FALSE, mask);
	VipsRect dirty, image, clipped;

	dirty.left = x;
	dirty.top = y;
	dirty.width = mask_im->Xsize;
	dirty.height = mask_im->Ysize;
	image.left = 0;
	image.top = 0;
	image.width = im->Xsize;
	image.height = im->Ysize;
	vips_rect_intersectrect(&dirty, &image, &clipped);

	if (vips_rect_isempty(&clipped))
		return TRUE;

	if (!imageinfo_undo_add(imageinfo, &clipped))
		return FALSE;

	printf("imageinfo_paint_mask: FIXME\n");
	/*
	if (im_plotmask(im, 0, 0,
			(VipsPel *) ink_im->data, (VipsPel *) mask_im->data, &dirty)) {
		error_vips_all();
		return FALSE;
	}
	 */

	imageinfo_area_painted(imageinfo, &dirty);

	return TRUE;
}

/* Print a pixel. Output has to be parseable by imageinfo_from_text().
 */
void
imageinfo_to_text(Imageinfo *imageinfo, VipsBuf *buf)
{
	VipsImage *im = imageinfo_get(FALSE, imageinfo);
	VipsPel *p = (VipsPel *) im->data;
	int i;

#define PRINT_INT(T, I) vips_buf_appendf(buf, "%d", ((T *) p)[I]);
#define PRINT_FLOAT(T, I) vips_buf_appendg(buf, ((T *) p)[I]);

	for (i = 0; i < im->Bands; i++) {
		if (i)
			vips_buf_appends(buf, ", ");

		switch (im->BandFmt) {
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

/* Set band i to value.
 */
static void
imageinfo_from_text_band(Imageinfo *imageinfo, int i, double re, double im)
{
	VipsImage *image = imageinfo_get(FALSE, imageinfo);
	VipsPel *p = (VipsPel *) image->data;
	double mod = sqrt(re * re + im * im);

	if (i < 0 || i >= image->Bands)
		return;

#define SET_INT(T, I, X) (((T *) p)[I] = (T) VIPS_RINT(X))
#define SET_FLOAT(T, I, X) (((T *) p)[I] = (T) (X))

	switch (image->BandFmt) {
	case VIPS_FORMAT_UCHAR:
		SET_INT(unsigned char, i, mod);
		break;

	case VIPS_FORMAT_CHAR:
		SET_INT(char, i, mod);
		break;

	case VIPS_FORMAT_USHORT:
		SET_INT(unsigned short, i, mod);
		break;

	case VIPS_FORMAT_SHORT:
		SET_INT(short, i, mod);
		break;

	case VIPS_FORMAT_UINT:
		SET_INT(unsigned int, i, mod);
		break;

	case VIPS_FORMAT_INT:
		SET_INT(int, i, mod);
		break;

	case VIPS_FORMAT_FLOAT:
		SET_FLOAT(float, i, mod);
		break;

	case VIPS_FORMAT_COMPLEX:
		SET_FLOAT(float, (i << 1), re);
		SET_FLOAT(float, (i << 1) + 1, im);
		break;

	case VIPS_FORMAT_DOUBLE:
		SET_FLOAT(double, i, mod);
		break;

	case VIPS_FORMAT_DPCOMPLEX:
		SET_FLOAT(double, i << 1, re);
		SET_FLOAT(double, (i << 1) + 1, im);
		break;

	default:
		break;
	}
}

/* Parse a string to an imageinfo.
 * Strings are from imageinfo_to_text(), ie. of the form:
 *
 *	50, 0, 0
 *	(12,13), (14,15)
 *
 */
gboolean
imageinfo_from_text(Imageinfo *imageinfo, const char *text)
{
	char buf[MAX_LINELENGTH];
	char *p;
	int i;
	VipsRect dirty;

#ifdef DEBUG_RGB
	printf("imageinfo_from_text: in: \"\%s\"\n", text);
#endif /*DEBUG_RGB*/

	vips_strncpy(buf, text, MAX_LINELENGTH);

	for (i = 0, p = buf; p += strspn(p, WHITESPACE), *p; i++) {
		double re, im;

		if (p[0] == '(') {
			/* Complex constant.
			 */
			re = g_ascii_strtod(p + 1, NULL);
			p = break_token(p, ",");
			im = g_ascii_strtod(p, NULL);
			p = break_token(p, ")");
		}
		else {
			/* Real constant.
			 */
			re = g_ascii_strtod(p, NULL);
			im = 0;
		}

		p = break_token(p, ",");

		imageinfo_from_text_band(imageinfo, i, re, im);
	}

#ifdef DEBUG_RGB
	{
		char txt[256];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		printf("imageinfo_from_text: out: ");
		imageinfo_to_text(imageinfo, &buf);
		printf("%s\n", vips_buf_all(&buf));
	}
#endif /*DEBUG_RGB*/

	dirty.left = 0;
	dirty.top = 0;
	dirty.width = 1;
	dirty.height = 1;
	imageinfo_area_painted(imageinfo, &dirty);

	return TRUE;
}

/* Get the image as display RGB in rgb[0-2].
 */
void
imageinfo_to_rgb(Imageinfo *imageinfo, double *rgb)
{
	printf("imageinfo_to_rgb: FIXME\n");
}

/* Try to overwrite an imageinfo with a display RGB colour.
 */
void
imageinfo_from_rgb(Imageinfo *imageinfo, double *rgb)
{
	printf("imageinfo_from_rgb: FIXME\n");
}

void
imageinfo_colour_edit(GtkWidget *parent, Imageinfo *imageinfo)
{
	printf("imageinfo_colour_edit: FIXME\n");
}
