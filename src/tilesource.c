/* generate tiles for the pyramid
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
#define DEBUG_VERBOSE
#define DEBUG
#define DEBUG_MAKE
 */

#include "nip4.h"

/* Use this threadpool to do background loads of images.
 */
static GThreadPool *tilesource_background_load_pool = NULL;

G_DEFINE_TYPE(Tilesource, tilesource, G_TYPE_OBJECT);

enum {
	/* Properties.
	 */
	PROP_MODE = 1,
	PROP_SCALE,
	PROP_OFFSET,
	PROP_PAGE,
	PROP_FALSECOLOUR,
	PROP_LOG,
	PROP_ICC,
	PROP_ACTIVE,
	PROP_LOADED,
	PROP_VISIBLE,
	PROP_PRIORITY,

	/* Signals.
	 */
	SIG_PREEVAL,
	SIG_EVAL,
	SIG_POSTEVAL,
	SIG_CHANGED,
	SIG_TILES_CHANGED,
	SIG_COLLECT,
	SIG_PAGE_CHANGED,
	SIG_LOADED,

	SIG_LAST
};

static guint tilesource_signals[SIG_LAST] = { 0 };

static void
tilesource_dispose(GObject *object)
{
	Tilesource *tilesource = TILESOURCE(object);

#ifdef DEBUG_MAKE
	printf("tilesource_dispose: %s\n", tilesource->filename);
#endif /*DEBUG_MAKE*/

	VIPS_FREEF(g_source_remove, tilesource->page_flip_id);

	VIPS_FREE(tilesource->filename);

	VIPS_UNREF(tilesource->base);
	VIPS_UNREF(tilesource->image);
	VIPS_UNREF(tilesource->image_region);
	VIPS_UNREF(tilesource->display);
	VIPS_UNREF(tilesource->mask);
	VIPS_UNREF(tilesource->rgb);
	VIPS_UNREF(tilesource->rgb_region);
	VIPS_UNREF(tilesource->mask_region);

	VIPS_FREE(tilesource->delay);
	VIPS_FREE(tilesource->load_message);

	G_OBJECT_CLASS(tilesource_parent_class)->dispose(object);
}

void
tilesource_changed(Tilesource *tilesource)
{
	g_signal_emit(tilesource, tilesource_signals[SIG_CHANGED], 0);
}

static void
tilesource_tiles_changed(Tilesource *tilesource)
{
	g_signal_emit(tilesource, tilesource_signals[SIG_TILES_CHANGED], 0);
}

static void
tilesource_collect(Tilesource *tilesource, VipsRect *dirty, int z)
{
	g_signal_emit(tilesource, tilesource_signals[SIG_COLLECT], 0, dirty, z);
}

static void
tilesource_page_changed(Tilesource *tilesource)
{
	g_signal_emit(tilesource, tilesource_signals[SIG_PAGE_CHANGED], 0);
}

static void
tilesource_loaded(Tilesource *tilesource)
{
	g_signal_emit(tilesource, tilesource_signals[SIG_LOADED], 0);
}

typedef struct _TilesourceUpdate {
	Tilesource *tilesource;
	VipsImage *image;
	VipsRect rect;
	int z;
} TilesourceUpdate;

/* Open a specified level. Take page (if relevant) from the tilesource.
 */
static VipsImage *
tilesource_open(Tilesource *tilesource, int level)
{
	/* In toilet-roll and pages-as-bands modes, we open all pages
	 * together.
	 */
	gboolean all_pages = tilesource->type == TILESOURCE_TYPE_TOILET_ROLL;
	int n = all_pages ? -1 : 1;
	int page = all_pages ? 0 : tilesource->page;

	VipsImage *image;

	/* Only for tiles_source which have something you can reopen.
	 */
	g_assert(tilesource->filename);

	if (vips_isprefix("openslide", tilesource->loader)) {
		/* These only have a "level" dimension.
		 */
		image = vips_image_new_from_file(tilesource->filename,
			"level", level,
			NULL);
	}
	else if (vips_isprefix("tiff", tilesource->loader)) {
		/* We support three modes: subifd pyramids, page-based
		 * pyramids, and simple multi-page TIFFs (no pyramid).
		 */
		if (tilesource->subifd_pyramid)
			/* subifd == -1 means the main image. subifd 0 picks
			 * the first subifd.
			 */
			image = vips_image_new_from_file(tilesource->filename,
				"page", page,
				"subifd", level - 1,
				"n", n,
				NULL);
		else if (tilesource->page_pyramid)
			/* No "n" here since pages are mag levels.
			 */
			image = vips_image_new_from_file(tilesource->filename,
				"page", level,
				NULL);
		else
			/* Pages are regular pages.
			 */
			image = vips_image_new_from_file(tilesource->filename,
				"page", page,
				"n", n,
				NULL);
	}
	else if (vips_isprefix("jp2k", tilesource->loader) ||
		vips_isprefix("pdf", tilesource->loader)) {
		/* These formats only have "page", or have pages that can vary in
		 * size, making "n" not always useful.
		 */
		if (all_pages)
			image = NULL;
		else
			image = vips_image_new_from_file(tilesource->filename,
				"page", level,
				NULL);
	}
	else if (vips_isprefix("webp", tilesource->loader) ||
		vips_isprefix("jxl", tilesource->loader) ||
		vips_isprefix("gif", tilesource->loader)) {
		/* These formats have pages all the same size and support page and n.
		 */
		image = vips_image_new_from_file(tilesource->filename,
			"page", level,
			"n", n,
			NULL);
	}
	else if (vips_isprefix("svg", tilesource->loader)) {
		image = vips_image_new_from_file(tilesource->filename,
			"scale", tilesource->zoom / (1 << level),
			NULL);
	}
	else
		/* No page spec support.
		 */
		image = vips_image_new_from_file(tilesource->filename, NULL);

	return image;
}

/* Run by the main GUI thread when a notify comes in from libvips that a tile
 * we requested is now available.
 */
static gboolean
tilesource_render_notify_idle(void *user_data)
{
	TilesourceUpdate *update = (TilesourceUpdate *) user_data;
	Tilesource *tilesource = update->tilesource;

	/* Only bother fetching the updated tile if it's from our current
	 * pipeline.
	 */
	if (update->image == tilesource->display)
		tilesource_collect(tilesource, &update->rect, update->z);

	/* The update that's just for this one event needs freeing.
	 */
	g_free(update);

	return FALSE;
}

/* Come here from the vips_sink_screen() background thread when a tile has been
 * calculated. This is a bbackground thread, so we add an idle callback
 * which will be run by the main thread when it next hits the mainloop.
 */
static void
tilesource_render_notify(VipsImage *image, VipsRect *rect, void *client)
{
	TilesourceUpdate *update = (TilesourceUpdate *) client;

	/* We're passed an update made by tilesource_display_image() to track
	 * just this image. We need one dedicated to this single event.
	 */
	TilesourceUpdate *new_update = g_new(TilesourceUpdate, 1);

	/* From image cods to level0 cods.
	 */
	*new_update = *update;
	new_update->rect.left = rect->left << update->z;
	new_update->rect.top = rect->top << update->z;
	new_update->rect.width = rect->width << update->z;
	new_update->rect.height = rect->height << update->z;

	g_idle_add(tilesource_render_notify_idle, new_update);
}

/* Build the first half of the render pipeline. This ends in the sink_screen
 * which will issue any repaints.
 */
static VipsImage *
tilesource_display_image(Tilesource *tilesource, VipsImage **mask_out)
{
	VipsImage *x;
	VipsImage *mask;

	g_assert(mask_out);

	g_autoptr(VipsImage) image = NULL;
	if (tilesource->level_count) {
		/* There's a pyramid ... compute the size of image we need,
		 * then find the layer which is one larger.
		 */
		int required_width = tilesource->width >> tilesource->current_z;

		int i;
		int level;

		for (i = 0; i < tilesource->level_count; i++)
			if (tilesource->level_width[i] < required_width)
				break;
		level = VIPS_CLIP(0, i - 1, tilesource->level_count - 1);

#ifdef DEBUG
		printf("tilesource_display_image: loading level %d\n", level);
#endif /*DEBUG*/

		if (!(image = tilesource_open(tilesource, level)))
			return NULL;
	}
	else if (tilesource->type == TILESOURCE_TYPE_MULTIPAGE) {
#ifdef DEBUG
		printf("tilesource_display_image: loading page %d\n",
			tilesource->page);
#endif /*DEBUG*/

		if (!(image = tilesource_open(tilesource, tilesource->page)))
			return NULL;
	}
	else {
		image = tilesource->image;
		g_object_ref(image);
	}

	/* In multipage display mode, crop out the page we want.
	 *
	 * We need to crop using the page size on image, since it might have
	 * been shrunk by shrink-on-load above ^^
	 */
	if (tilesource->type == TILESOURCE_TYPE_TOILET_ROLL &&
		(tilesource->mode == TILESOURCE_MODE_MULTIPAGE ||
			tilesource->mode == TILESOURCE_MODE_ANIMATED)) {
		int page_width = image->Xsize;
		int page_height = vips_image_get_page_height(image);

		VipsImage *x;

		if (vips_crop(image, &x,
				0, tilesource->page * page_height,
				page_width, page_height, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	/* In pages-as-bands mode, crop out all pages and join band-wise.
	 *
	 * We need to crop using the page size on image, since it might
	 * have been shrunk by shrink-on-load above ^^
	 */
	if (tilesource->type == TILESOURCE_TYPE_TOILET_ROLL &&
		tilesource->mode == TILESOURCE_MODE_PAGES_AS_BANDS) {
		int page_width = image->Xsize;
		int page_height = vips_image_get_page_height(image);
		// not too many pages or we can have terrible performance problems
		// more than 3 for the info bar
		int n_pages = VIPS_MIN(10, tilesource->n_pages);

		g_autoptr(VipsObject) context = VIPS_OBJECT(vips_image_new());
		VipsImage **t = (VipsImage **)
			vips_object_local_array(context, n_pages);

		int page;
		VipsImage *x;

		for (page = 0; page < n_pages; page++)
			if (vips_crop(image, &t[page],
					0, page * page_height, page_width, page_height,
					NULL))
				return NULL;
		if (vips_bandjoin(t, &x, n_pages, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;

		/* Pick an interpretation ... one of the RGB types, if we
		 * can.
		 */
		if (image->Bands < 3)
			image->Type = image->BandFmt == VIPS_FORMAT_USHORT ? VIPS_INTERPRETATION_GREY16 : VIPS_INTERPRETATION_B_W;
		else
			image->Type = image->BandFmt == VIPS_FORMAT_USHORT ? VIPS_INTERPRETATION_RGB16 : VIPS_INTERPRETATION_sRGB;
	}

	/* Histogram type ... plot the histogram.
	 */
	if (image->Type == VIPS_INTERPRETATION_HISTOGRAM &&
		(image->Xsize == 1 || image->Ysize == 1)) {
		g_autoptr(VipsObject) context = VIPS_OBJECT(vips_image_new());
		VipsImage **t = (VipsImage **) vips_object_local_array(context, 7);

		// so it's unreffed when we unref context
		t[0] = image;

		if (vips_image_decode(t[0], &t[1]) ||
			vips_hist_norm(t[1], &t[2], NULL) ||
			vips_hist_plot(t[2], &t[3], NULL))
			return NULL;

		image = t[3];
		g_object_ref(image);

		// we've changed the size of the image we need to display
		tilesource->width = image->Xsize;
		tilesource->height = image->Ysize;
		tilesource->display_width = tilesource->width;
		tilesource->display_height = tilesource->height;
	}

	/* This has to be before the sink_screen since we need to find the image
	 * maximum .... this sounds bad, but FFTs are image at a time anyway, so
	 * the pipeline should be short.
	 */
	if (image->Type == VIPS_INTERPRETATION_FOURIER) {
		if (vips_abs(image, &x, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;

		if (vips_scale(image, &x, "log", TRUE, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	if (tilesource->current_z > 0) {
        /* We may have already zoomed out a bit because we've loaded
         * some layer other than the base one. Calculate the
         * subsample as (current_width / required_width).
         */
        int width = VIPS_MAX(1, tilesource->width >> tilesource->current_z);
        int height = VIPS_MAX(1, tilesource->height >> tilesource->current_z);
        int xfac = image->Xsize / width;
        int yfac = image->Ysize / height;

        if (vips_subsample(image, &x, xfac, yfac, NULL))
            return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	if (tilesource->synchronous) {
		if (vips_copy(image, &x, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;

		*mask_out = NULL;
	}
	else {
		/* Need something to track the z at which we made this sink_screen.
		 */
		TilesourceUpdate *update = VIPS_NEW(image, TilesourceUpdate);
		update->tilesource = tilesource;
		update->z = tilesource->current_z;

		x = vips_image_new();
		mask = vips_image_new();
		if (vips_sink_screen(image, x, mask,
				TILE_SIZE, TILE_SIZE, MAX_TILES, tilesource->priority,
				tilesource_render_notify, update)) {
			VIPS_UNREF(x);
			VIPS_UNREF(mask);
			return NULL;
		}
		VIPS_UNREF(image);
		image = x;

		update->image = image;

		*mask_out = mask;
	}

	return g_steal_pointer(&image);
}

static VipsImage *
tilesource_image_log(VipsImage *image)
{
	static const double power = 0.25;
	const double scale = 255.0 / log10(1.0 + pow(255.0, power));

	g_autoptr(VipsObject) context = VIPS_OBJECT(vips_image_new());
	VipsImage **t = (VipsImage **) vips_object_local_array(context, 7);

	VipsImage *x;

	if (vips_pow_const1(image, &t[0], power, NULL) ||
		vips_linear1(t[0], &t[1], 1.0, 1.0, NULL) ||
		vips_log10(t[1], &t[2], NULL) ||
		// add 0.5 to get round to nearest
		vips_linear1(t[2], &x, scale, 0.5, NULL)) {
		return NULL;
	}
	image = x;

	return image;
}

static int
tilesource_n_colour(VipsImage *image)
{
	switch (image->Type) {
	case VIPS_INTERPRETATION_B_W:
	case VIPS_INTERPRETATION_GREY16:
		return 1;

	case VIPS_INTERPRETATION_RGB:
	case VIPS_INTERPRETATION_CMC:
	case VIPS_INTERPRETATION_LCH:
	case VIPS_INTERPRETATION_LABS:
	case VIPS_INTERPRETATION_sRGB:
	case VIPS_INTERPRETATION_YXY:
	case VIPS_INTERPRETATION_XYZ:
	case VIPS_INTERPRETATION_LAB:
	case VIPS_INTERPRETATION_RGB16:
	case VIPS_INTERPRETATION_scRGB:
	case VIPS_INTERPRETATION_HSV:
		return 3;

	case VIPS_INTERPRETATION_CMYK:
		return 4;

	default:
		/* We can't really infer anything about alpha from things like
		 * HISTOGRAM or FOURIER.
		 */
		return image->Bands;
	}
}

/* Build the second half of the image pipeline. This ends with an RGB or RGBA
 * image we can make textures from.
 */
static VipsImage *
tilesource_rgb_image(Tilesource *tilesource, VipsImage *in)
{
	VipsImage *x;
	int n_bands;

	g_autoptr(VipsImage) image = in;
	g_object_ref(image);

	/* Coded images won't unalpha correctly.
	 */
	if (vips_image_decode(image, &x))
		return NULL;
	VIPS_UNREF(image);
	image = x;

	/* The image interpretation might be crazy (eg. a mono image tagged as
	 * srgb) and that'll mess up our rules for display.
	 */
	image->Type = vips_image_guess_interpretation(image);

	/* We don't want vis controls to touch alpha ... remove and reattach at
	 * the end.
	 */
	g_autoptr(VipsImage) alpha = NULL;
	n_bands = tilesource_n_colour(image);
	if (image->Bands > n_bands) {
		if (vips_extract_band(image, &x, 0, "n", n_bands, NULL))
			return NULL;

		// just use the first alpha
		if (vips_extract_band(image, &alpha, n_bands, "n", 1, NULL))
			return NULL;

		VIPS_UNREF(image);
		image = x;
	}

	/* Visualisation controls ... the scale and offset values must be applied
	 * in terms of the original image values, so we can't go to RGB first.
	 */
	if (tilesource->active &&
		(tilesource->scale != 1.0 ||
			tilesource->offset != 0.0 ||
			tilesource->falsecolour ||
			tilesource->log)) {
		if (tilesource->log) {
			if (!(x = tilesource_image_log(image)))
				return NULL;
			VIPS_UNREF(image);
			image = x;
		}

		if (tilesource->scale != 1.0 ||
			tilesource->offset != 0.0) {
			if (vips_linear1(image, &x,
					tilesource->scale, tilesource->offset, NULL))
				return NULL;
			VIPS_UNREF(image);
			image = x;
		}
	}

	/* Colour management to srgb.
	 */
	if (tilesource->active &&
		tilesource->icc) {
		if (vips_icc_transform(image, &x, "srgb", NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	/* Go to uint8 sRGB in a nice way.
	 */
	if (image->Type != VIPS_INTERPRETATION_sRGB &&
		vips_colourspace_issupported(image)) {
		if (vips_colourspace(image, &x, VIPS_INTERPRETATION_sRGB, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	if (image->BandFmt != VIPS_FORMAT_UCHAR) {
		if (vips_cast(image, &x, VIPS_FORMAT_UCHAR, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	/* This must be after conversion to sRGB.
	 */
	if (tilesource->active &&
		tilesource->falsecolour) {
		if (vips_falsecolour(image, &x, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	/* The number of bands could still be wrong for spaces like
	 * MATRIX or FOURIER.
	 */
	if (image->Bands > 3) {
		if (vips_extract_band(image, &x, 0, "n", 3, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}
	else if (image->Bands == 2) {
		if (vips_bandjoin_const1(image, &x, 0, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}
	else if (image->Bands == 1) {
		VipsImage *in[3] = { image, image, image };

		if (vips_bandjoin(in, &x, 3, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	// reattach alpha
	if (alpha) {
		double max_alpha_before = vips_interpretation_max_alpha(alpha->Type);
		double max_alpha_after = vips_interpretation_max_alpha(image->Type);

		if (max_alpha_before != max_alpha_after) {
			if (vips_linear1(alpha, &x,
				max_alpha_after / max_alpha_before, 0.0, NULL))
				return NULL;
			VIPS_UNREF(alpha);
			alpha = x;
		}

		if (vips_cast(alpha, &x, image->BandFmt, NULL))
			return NULL;
		VIPS_UNREF(alpha);
		alpha = x;

		if (vips_bandjoin2(image, alpha, &x, NULL))
			return NULL;
		VIPS_UNREF(image);
		image = x;
	}

	return g_steal_pointer(&image);
}

/* Rebuild just the second half of the image pipeline, eg. after a change to
 * falsecolour, scale, or if current_z changes.
 */
static int
tilesource_update_rgb(Tilesource *tilesource)
{
#ifdef DEBUG
	printf("tilesource_update_rgb:\n");
#endif /*DEBUG*/

	if (tilesource->display) {
		VipsImage *rgb;

		if (!(rgb = tilesource_rgb_image(tilesource, tilesource->display))) {
			printf("tilesource_rgb_image failed!\n");
			return -1;
		}
		VIPS_UNREF(tilesource->rgb);
		tilesource->rgb = rgb;

		VIPS_UNREF(tilesource->rgb_region);
		tilesource->rgb_region = vips_region_new(tilesource->rgb);
	}

	return 0;
}

/* Rebuild the entire display pipeline eg. after a page flip, or if current_z
 * changes.
 */
static int
tilesource_update_display(Tilesource *tilesource)
{
	VipsImage *display;
	VipsImage *mask;

#ifdef DEBUG
	printf("tilesource_update_display:\n");
#endif /*DEBUG*/

	/* Don't update if we're still loading.
	 */
	if (!tilesource->loaded ||
		!tilesource->image)
		return 0;

	if (!(display = tilesource_display_image(tilesource, &mask))) {
#ifdef DEBUG
		printf("tilesource_update_display: build failed\n");
#endif /*DEBUG*/
		return -1;
	}

	VIPS_UNREF(tilesource->display);
	VIPS_UNREF(tilesource->mask);
	tilesource->display = display;
	tilesource->mask = mask;

	VIPS_UNREF(tilesource->mask_region);
	tilesource->mask_region = vips_region_new(tilesource->mask);

	if (tilesource_update_rgb(tilesource))
		return -1;

	return 0;
}

#ifdef DEBUG
static const char *
tilesource_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_MODE:
		return "MODE";
		break;

	case PROP_SCALE:
		return "SCALE";
		break;

	case PROP_OFFSET:
		return "OFFSET";
		break;

	case PROP_PAGE:
		return "PAGE";
		break;

	case PROP_FALSECOLOUR:
		return "FALSECOLOUR";
		break;

	case PROP_LOG:
		return "LOG";
		break;

	case PROP_ICC:
		return "ICC";
		break;

	case PROP_ACTIVE:
		return "ACTIVE";
		break;

	case PROP_LOADED:
		return "LOADED";
		break;

	case PROP_VISIBLE:
		return "VISIBLE";
		break;

	case PROP_PRIORITY:
		return "PRIORITY";
		break;

	default:
		return "<unknown>";
	}
}
#endif /*DEBUG*/

/* Each timeout fires once, sets the next timeout, and flips the page.
 */
static gboolean
tilesource_page_flip(void *user_data)
{
	Tilesource *tilesource = (Tilesource *) user_data;
	int page = VIPS_CLIP(0, tilesource->page, tilesource->n_pages - 1);

	int timeout;

	/* By convention, GIFs default to 10fps.
	 */
	timeout = 100;

	if (tilesource->delay) {
		int i = VIPS_MIN(page, tilesource->n_delay - 1);

		/* By GIF convention, timeout 0 means unset.
		 */
		if (tilesource->delay[i])
			timeout = tilesource->delay[i];
	}

	/* gtk can struggle at more than 30fps.
	 */
	timeout = VIPS_CLIP(33, timeout, 100000);

	tilesource->page_flip_id =
		g_timeout_add(timeout, tilesource_page_flip, tilesource);

#ifdef DEBUG
	printf("tilesource_page_flip: timeout %d ms\n", timeout);
#endif /*DEBUG*/

	/* Only flip the page if everything has loaded and the image is visible.
	 */
	if (tilesource->rgb &&
		tilesource->visible)
		g_object_set(tilesource,
			"page", (page + 1) % tilesource->n_pages,
			NULL);

	return FALSE;
}

static void
tilesource_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Tilesource *tilesource = (Tilesource *) object;

	TilesourceMode mode;
	int i;
	double d;
	gboolean b;

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("tilesource_set_property: %s = %s\n",
			tilesource_property_name(prop_id), str);
	}
#endif /*DEBUG*/

	switch (prop_id) {
	case PROP_MODE:
		mode = g_value_get_enum(value);
		if (mode >= 0 &&
			mode < TILESOURCE_MODE_LAST &&
			tilesource->mode != mode) {
			tilesource->mode = mode;
			tilesource->display_width = tilesource->width;
			tilesource->display_height = tilesource->height;
			if (tilesource->mode == TILESOURCE_MODE_TOILET_ROLL)
				tilesource->display_height *= tilesource->n_pages;

			tilesource_update_display(tilesource);

			tilesource_changed(tilesource);

			/* In animation mode, create the page flip timeout.
			 */
			if (tilesource->page_flip_id)
				VIPS_FREEF(g_source_remove, tilesource->page_flip_id);
			if (tilesource->mode == TILESOURCE_MODE_ANIMATED &&
				tilesource->n_pages > 1)
				tilesource->page_flip_id = g_timeout_add(100,
					tilesource_page_flip, tilesource);
		}
		break;

	case PROP_SCALE:
		d = g_value_get_double(value);
		if (d > 0 &&
			d <= 1000000 &&
			tilesource->scale != d) {
			tilesource->scale = d;
			tilesource_update_rgb(tilesource);
			tilesource_tiles_changed(tilesource);
		}
		break;

	case PROP_OFFSET:
		d = g_value_get_double(value);
		if (d >= -100000 &&
			d <= 1000000 &&
			tilesource->offset != d) {
			tilesource->offset = d;
			tilesource_update_rgb(tilesource);
			tilesource_tiles_changed(tilesource);
		}
		break;

	case PROP_PAGE:
		i = g_value_get_int(value);
		if (i >= 0 &&
			i <= 1000000 &&
			tilesource->page != i) {
			tilesource->page = i;
			tilesource_update_display(tilesource);

			/* If all pages have the same size, we can flip pages
			 * without rebuilding the pyramid.
			 */
			if (tilesource->pages_same_size)
				tilesource_tiles_changed(tilesource);
			else
				tilesource_changed(tilesource);

			tilesource_page_changed(tilesource);
		}
		break;

	case PROP_FALSECOLOUR:
		b = g_value_get_boolean(value);
		if (tilesource->falsecolour != b) {
			tilesource->falsecolour = b;
			tilesource_update_rgb(tilesource);

			tilesource_tiles_changed(tilesource);
		}
		break;

	case PROP_LOG:
		b = g_value_get_boolean(value);
		if (tilesource->log != b) {
			tilesource->log = b;
			tilesource_update_rgb(tilesource);
			tilesource_tiles_changed(tilesource);
		}
		break;

	case PROP_ICC:
		b = g_value_get_boolean(value);
		if (tilesource->icc != b) {
			tilesource->icc = b;
			tilesource_update_rgb(tilesource);
			tilesource_tiles_changed(tilesource);
		}
		break;

	case PROP_ACTIVE:
		b = g_value_get_boolean(value);
		if (tilesource->active != b) {
			tilesource->active = b;
			tilesource_update_rgb(tilesource);
			tilesource_tiles_changed(tilesource);
		}
		break;

	case PROP_LOADED:
		b = g_value_get_boolean(value);
		if (tilesource->loaded != b) {
			tilesource->loaded = b;
			tilesource_update_display(tilesource);
			tilesource_changed(tilesource);
		}
		break;

	case PROP_VISIBLE:
		b = g_value_get_boolean(value);
		if (tilesource->visible != b)
			tilesource->visible = b;
		break;

	case PROP_PRIORITY:
		i = g_value_get_int(value);
		if (tilesource->priority != i)
			tilesource->priority = i;
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
tilesource_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Tilesource *tilesource = (Tilesource *) object;

	switch (prop_id) {
	case PROP_MODE:
		g_value_set_enum(value, tilesource->mode);
		break;

	case PROP_SCALE:
		g_value_set_double(value, tilesource->scale);
		break;

	case PROP_OFFSET:
		g_value_set_double(value, tilesource->offset);
		break;

	case PROP_PAGE:
		g_value_set_int(value, tilesource->page);
		break;

	case PROP_FALSECOLOUR:
		g_value_set_boolean(value, tilesource->falsecolour);
		break;

	case PROP_LOG:
		g_value_set_boolean(value, tilesource->log);
		break;

	case PROP_ICC:
		g_value_set_boolean(value, tilesource->icc);
		break;

	case PROP_ACTIVE:
		g_value_set_boolean(value, tilesource->active);
		break;

	case PROP_LOADED:
		g_value_set_boolean(value, tilesource->loaded);
		break;

	case PROP_VISIBLE:
		g_value_set_boolean(value, tilesource->visible);
		break;

	case PROP_PRIORITY:
		g_value_set_int(value, tilesource->priority);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("tilesource_get_property: %s %s\n",
			tilesource_property_name(prop_id), str);
	}
#endif /*DEBUG*/
}

static void
tilesource_init(Tilesource *tilesource)
{
#ifdef DEBUG_MAKE
	printf("tilesource_init:\n");
#endif /*DEBUG_MAKE*/

	tilesource->scale = 1.0;
	tilesource->zoom = 1.0;
}

static int
tilesource_force_load(Tilesource *tilesource)
{
	if (tilesource->image &&
		!tilesource->loaded) {
		/* We can't just call prepare on image_region -- this will get region
		 * ownership tangled up. Crop and average a pixel.
		 */
		g_autoptr(VipsImage) pixel = NULL;
		double d;
		if (vips_crop(tilesource->image, &pixel, 0, 0, 1, 1, NULL) ||
			vips_avg(pixel, &d, NULL))
			return -1;
	}

	return 0;
}

/* This runs in the main thread when the bg load is done. We can't use
 * postload since that will only fire if we are actually loading, and not if
 * the image is coming from cache.
 */
static gboolean
tilesource_background_load_done_idle(void *user_data)
{
	Tilesource *tilesource = (Tilesource *) user_data;

#ifdef DEBUG
	printf("tilesource_background_load_done_cb: ... unreffing\n");
#endif /*DEBUG*/

	/* You can now fetch pixels.
	 */
	g_object_set(tilesource,
		"loaded", TRUE,
		"visible", TRUE,
		NULL);
	tilesource_update_display(tilesource);
	tilesource_loaded(tilesource);

	/* Drop the ref that kept this tilesource alive during load, see
	 * tilesource_background_load().
	 */
	g_object_unref(tilesource);

	return FALSE;
}

/* This runs for the background load threadpool.
 */
static void
tilesource_background_load_worker(void *data, void *user_data)
{
	Tilesource *tilesource = (Tilesource *) data;

#ifdef DEBUG
	printf("tilesource_background_load_worker: starting ...\n");
#endif /*DEBUG*/

	g_assert(tilesource->image_region);

	if (tilesource_force_load(tilesource)) {
		tilesource->load_error = TRUE;
		tilesource->load_message = vips_error_buffer_copy();
	}

	g_idle_add(tilesource_background_load_done_idle, tilesource);

#ifdef DEBUG
	printf("tilesource_background_load_worker: ... done\n");
#endif /*DEBUG*/
}

static void
tilesource_class_init(TilesourceClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	gobject_class->dispose = tilesource_dispose;
	gobject_class->set_property = tilesource_set_property;
	gobject_class->get_property = tilesource_get_property;

	g_object_class_install_property(gobject_class, PROP_MODE,
		g_param_spec_enum("mode",
			_("Mode"),
			_("Display mode"),
			TILESOURCE_MODE_TYPE,
			TILESOURCE_MODE_MULTIPAGE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_SCALE,
		g_param_spec_double("scale",
			_("scale"),
			_("Scale"),
			-1000000, 1000000, 1,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_OFFSET,
		g_param_spec_double("offset",
			_("offset"),
			_("Offset"),
			-1000000, 1000000, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PAGE,
		g_param_spec_int("page",
			_("Page"),
			_("Page number"),
			0, 1000000, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_FALSECOLOUR,
		g_param_spec_boolean("falsecolour",
			_("falsecolour"),
			_("False colour"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_LOG,
		g_param_spec_boolean("log",
			_("log"),
			_("Log"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ICC,
		g_param_spec_boolean("icc",
			_("icc"),
			_("ICC"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ACTIVE,
		g_param_spec_boolean("active",
			_("Active"),
			_("Visualisation controls are active"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_LOADED,
		g_param_spec_boolean("loaded",
			_("loaded"),
			_("Image has finished loading"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_VISIBLE,
		g_param_spec_boolean("visible",
			_("visible"),
			_("Image is currently visible"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PRIORITY,
		g_param_spec_int("priority",
			_("priority"),
			_("Render priority"),
			-1000, 1000, 0,
			G_PARAM_READWRITE));

	tilesource_signals[SIG_PREEVAL] = g_signal_new("preeval",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, preeval),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	tilesource_signals[SIG_EVAL] = g_signal_new("eval",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, eval),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	tilesource_signals[SIG_POSTEVAL] = g_signal_new("posteval",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, posteval),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	tilesource_signals[SIG_CHANGED] = g_signal_new("changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	tilesource_signals[SIG_TILES_CHANGED] = g_signal_new("tiles-changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, tiles_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	tilesource_signals[SIG_COLLECT] = g_signal_new("collect",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, collect),
		NULL, NULL,
		nip4_VOID__POINTER_INT,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_INT);

	tilesource_signals[SIG_PAGE_CHANGED] = g_signal_new("page-changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, page_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	tilesource_signals[SIG_LOADED] = g_signal_new("loaded",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(TilesourceClass, loaded),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	g_assert(!tilesource_background_load_pool);
	tilesource_background_load_pool = g_thread_pool_new(
		tilesource_background_load_worker,
		NULL, -1, FALSE, NULL);
}

#ifdef DEBUG
static void
tilesource_print(Tilesource *tilesource)
{
	int i;

	printf("tilesource: %p\n", tilesource);
	printf("\tloader = %s\n", tilesource->loader);
	printf("\twidth = %d\n", tilesource->width);
	printf("\theight = %d\n", tilesource->height);
	printf("\tn_pages = %d\n", tilesource->n_pages);
	printf("\tn_subifds = %d\n", tilesource->n_subifds);
	printf("\tsubifd_pyramid = %d\n", tilesource->subifd_pyramid);
	printf("\tpage_pyramid = %d\n", tilesource->page_pyramid);
	printf("\tlevel_count = %d\n", tilesource->level_count);

	for (i = 0; i < tilesource->level_count; i++)
		printf("\t%2d) %d x %d\n",
			i,
			tilesource->level_width[i],
			tilesource->level_height[i]);

	printf("\tpages_same_size = %d\n", tilesource->pages_same_size);
	printf("\tall_mono = %d\n", tilesource->all_mono);
	printf("\ttype = %s\n",
		vips_enum_nick(TILESOURCE_TYPE_TYPE, tilesource->type));
	printf("\tmode = %s\n",
		vips_enum_nick(TILESOURCE_MODE_TYPE, tilesource->mode));
	printf("\tdelay = %p\n", tilesource->delay);
	printf("\tn_delay = %d\n", tilesource->n_delay);
	printf("\tdisplay_width = %d\n", tilesource->display_width);
	printf("\tdisplay_height = %d\n", tilesource->display_height);
}
#endif /*DEBUG*/

/* Sniff basic properties from an image.
 */
static int
tilesource_set_image(Tilesource *tilesource, VipsImage *image)
{
#ifdef DEBUG
	printf("tilesource_set_image:\n");
#endif /*DEBUG*/

	/* Note the base open ... this gets used for eg. the titlebar.
	 */
	g_assert(!tilesource->base);
	tilesource->base = image;
	g_object_ref(tilesource->base);

	tilesource->width = image->Xsize;
	tilesource->height = vips_image_get_page_height(image);
	tilesource->bands = image->Bands;
	tilesource->n_subifds = vips_image_get_n_subifds(image);
	tilesource->type = TILESOURCE_TYPE_TOILET_ROLL;

	if (vips_image_get_typeof(image, "delay")) {
		int *delay;
		int n_delay;

		if (vips_image_get_array_int(image, "delay",
				&delay, &n_delay))
			return -1;

		tilesource->delay = g_new(int, n_delay);
		memcpy(tilesource->delay, delay, n_delay * sizeof(int));
		tilesource->n_delay = n_delay;
	}

#ifdef DEBUG
	tilesource_print(tilesource);
#endif /*DEBUG*/

	return 0;
}

/* Pick a default display mode.
 */
static void
tilesource_default_mode(Tilesource *tilesource)
{
	TilesourceMode mode;

	if (tilesource->type == TILESOURCE_TYPE_TOILET_ROLL &&
		tilesource->n_pages > 1) {
		if (tilesource->delay)
			mode = TILESOURCE_MODE_ANIMATED;
		else if (tilesource->all_mono)
			mode = TILESOURCE_MODE_PAGES_AS_BANDS;
		else
			mode = TILESOURCE_MODE_MULTIPAGE;
	}
	else
		mode = TILESOURCE_MODE_MULTIPAGE;

	g_object_set(tilesource,
		"mode", mode,
		NULL);
}

/* From a VipsImage, so no reopen is possible.
 */
Tilesource *
tilesource_new_from_image(VipsImage *image)
{
	g_autoptr(Tilesource) tilesource = g_object_new(TILESOURCE_TYPE, NULL);

	if (tilesource_set_image(tilesource, image))
		return NULL;

	/* No reopen, so we get n_pages from the image geometry.
	 */
	tilesource->n_pages = image->Ysize % tilesource->height == 0 ?
		image->Ysize / tilesource->height : 1;

	tilesource->image = image;
	g_object_ref(image);
	tilesource->image_region = vips_region_new(tilesource->image);

	/* image_region is used by the bg load thread.
	 */
	vips__region_no_ownership(tilesource->image_region);

	tilesource_default_mode(tilesource);

	return g_steal_pointer(&tilesource);
}

// use the file interface if we can, so we get fast zooming
Tilesource *
tilesource_new_from_imageinfo(Imageinfo *ii)
{
	if (imageinfo_is_from_file(ii))
		return callv_string_filename(
			(callv_string_fn) tilesource_new_from_file, IOBJECT(ii)->name,
			NULL, NULL, NULL);
	else
		return tilesource_new_from_image(ii->image);
}

Tilesource *
tilesource_new_from_iimage(iImage *iimage, int priority)
{
	Tilesource *tilesource = tilesource_new_from_imageinfo(iimage->value.ii);

	g_object_set(tilesource,
		"active", TRUE,
		"scale", iimage->scale,
		"offset", iimage->offset,
		"falsecolour", iimage->falsecolour,
		"log", iimage->log,
		"icc", iimage->icc,
		"priority", priority,
		NULL);

	return tilesource;
}

gboolean
tilesource_has_imageinfo(Tilesource *tilesource, Imageinfo *ii)
{
	if (!ii)
		return !tilesource->image;
	else if (imageinfo_is_from_file(ii) &&
		tilesource->filename)
		return filenames_equal(IOBJECT(ii)->name, tilesource->filename);
	else
		return ii->image == tilesource->image;
}

/* Detect a TIFF pyramid made of subifds following a roughly /2 shrink.
 */
static void
tilesource_get_pyramid_subifd(Tilesource *tilesource)
{
	int i;

#ifdef DEBUG
	printf("tilesource_get_pyramid_subifd:\n");
#endif /*DEBUG*/

	for (i = 0; i < tilesource->n_subifds; i++) {
		int level_width;
		int level_height;
		int expected_level_width;
		int expected_level_height;

		/* Just bail out if there are too many levels.
		 */
		if (i >= MAX_LEVELS)
			break;

		g_autoptr(VipsImage) level = tilesource_open(tilesource, i);
		if (!level)
			return;
		level_width = level->Xsize;
		level_height = level->Ysize;

		expected_level_width = tilesource->width / (1 << i);
		expected_level_height = tilesource->height / (1 << i);

		/* This won't be exact due to rounding etc.
		 */
		if (abs(level_width - expected_level_width) > 5 ||
			level_width < 2 ||
			abs(level_height - expected_level_height) > 5 ||
			level_height < 2) {
#ifdef DEBUG
			printf("  bad subifd level %d\n", i);
#endif /*DEBUG*/
			return;
		}

		tilesource->level_width[i] = level_width;
		tilesource->level_height[i] = level_height;
	}

	/* Tag as a subifd pyramid.
	 */
	tilesource->subifd_pyramid = TRUE;
	tilesource->level_count = tilesource->n_subifds;
}

/* Detect a pyramid made of pages following a roughly /2 shrink. Can be eg.
 * jp2k or TIFF.
 */
static void
tilesource_get_pyramid_page(Tilesource *tilesource)
{
	int i;

#ifdef DEBUG
	printf("tilesource_get_pyramid_page:\n");
#endif /*DEBUG*/

	/* Single-page docs can't be pyramids.
	 */
	if (tilesource->n_pages < 2)
		return;

	for (i = 0; i < tilesource->n_pages; i++) {
		int level_width;
		int level_height;
		int expected_level_width;
		int expected_level_height;

		/* Stop checking if there are too many levels.
		 */
		if (i >= MAX_LEVELS)
			break;

		g_autoptr(VipsImage) level = tilesource_open(tilesource, i);
		if (!level)
			return;
		level_width = level->Xsize;
		level_height = level->Ysize;

		expected_level_width = tilesource->width / (1 << i);
		expected_level_height = tilesource->height / (1 << i);

		/* We've found enough levels, and the levels have become very
		 * small.
		 */
		if (i > 2 &&
			(expected_level_width < 32 ||
				expected_level_height < 32))
			break;

		/* This won't be exact due to rounding etc.
		 */
		if (abs(level_width - expected_level_width) > 5 ||
			level_width < 2)
			return;
		if (abs(level_height - expected_level_height) > 5 ||
			level_height < 2)
			return;

		tilesource->level_width[i] = level_width;
		tilesource->level_height[i] = level_height;
	}

	/* Tag as a page pyramid.
	 */
	tilesource->page_pyramid = TRUE;
	tilesource->level_count = i;
}

static void
tilesource_preeval(VipsImage *image,
	VipsProgress *progress, Tilesource *tilesource)
{
	g_signal_emit(tilesource,
		tilesource_signals[SIG_PREEVAL], 0, progress);
}

static void
tilesource_eval(VipsImage *image,
	VipsProgress *progress, Tilesource *tilesource)
{
	g_signal_emit(tilesource,
		tilesource_signals[SIG_EVAL], 0, progress);
}

static void
tilesource_posteval(VipsImage *image,
	VipsProgress *progress, Tilesource *tilesource)
{
	g_signal_emit(tilesource,
		tilesource_signals[SIG_POSTEVAL], 0, progress);
}

static void
tilesource_attach_progress(Tilesource *tilesource)
{
#ifdef DEBUG
	printf("tilesource_attach_progress:\n");
#endif /*DEBUG*/

	vips_image_set_progress(tilesource->image, TRUE);
	g_signal_connect_object(tilesource->image, "preeval",
		G_CALLBACK(tilesource_preeval), tilesource, 0);
	g_signal_connect_object(tilesource->image, "eval",
		G_CALLBACK(tilesource_eval), tilesource, 0);
	g_signal_connect_object(tilesource->image, "posteval",
		G_CALLBACK(tilesource_posteval), tilesource, 0);
}

/* Fetch a string-encoded int image header field, eg. from openslide. These
 * are all represented as strings. Return the default value if there's any
 * problem.
 */
static int
get_int(VipsImage *image, const char *field, int default_value)
{
	const char *str;

	if (vips_image_get_typeof(image, field) &&
		!vips_image_get_string(image, field, &str))
		return atoi(str);

	return default_value;
}

Tilesource *
tilesource_new_from_file(const char *filename)
{
	g_autoptr(Tilesource) tilesource = g_object_new(TILESOURCE_TYPE, NULL);

	const char *loader;

#ifdef DEBUG
	printf("tilesource_new_from_file: %s\n", filename);
#endif /*DEBUG*/

	tilesource->filename = g_strdup(filename);

	if (!(loader = vips_foreign_find_load(filename)))
		return NULL;

	/* vips_foreign_find_load() gives us eg.
	 * "VipsForeignLoadNsgifFile", but we need "gifload", the
	 * generic name.
	 */
	tilesource->loader = vips_nickname_find(g_type_from_name(loader));

	/* A very basic open to fetch image properties.
	 */
	g_autoptr(VipsImage) base = vips_image_new_from_file(filename, NULL);
	if (!base)
		return NULL;
	if (tilesource_set_image(tilesource, base))
		return NULL;

	/* We can reopen this image, so we can use get_n_pages (number of pages in
	 * the underlying image file).
	 */
	tilesource->n_pages = vips_image_get_n_pages(base);

	/* For openslide, we can read out the level structure directly.
	 */
	if (vips_image_get_typeof(base, "openslide.level-count")) {
		int level_count;
		int level;

		level_count = get_int(base, "openslide.level-count", 1);
		level_count = VIPS_CLIP(1, level_count, MAX_LEVELS);
		tilesource->level_count = level_count;

		for (level = 0; level < level_count; level++) {
			char name[256];

			g_snprintf(name, 256, "openslide.level[%d].width", level);
			tilesource->level_width[level] = get_int(base, name, 0);
			g_snprintf(name, 256, "openslide.level[%d].height", level);
			tilesource->level_height[level] = get_int(base, name, 0);
		}

		/* Some openslide images don't have levels on x2 boundaries. SVS and
		 * images derived from SVS (eg. converted to DICOM) for example are
		 * in x4, and they also have a final thumbnail or overview image
		 * which is not on any boundary at all.
		 *
		 * Check the levels and only use the ones which are on power of two
		 * boundaries.
		 */
		for (level = 1; level < level_count; level++) {
			double downsample = (double) tilesource->level_width[0] /
				tilesource->level_width[level];
			double power = log(downsample) / log(2);

			if (fabs(power - floor(power)) > 0.01) {
				// too far away from a power of two, ignore any remaining
				// frames
				tilesource->level_count = level;
				break;
			}
		}
	}

	if (vips_isprefix("svg", tilesource->loader)) {
		int size;
		int n_levels;
		int tile_levels;
		int level;

		/* Compute a zoom (scale) factor which will keep us under 32k
		 * pixels per axis.
		 *
		 * Very large scales produce very large performance problems in
		 * librsvg.
		 */
		tilesource->zoom = VIPS_CLIP(1,
			32767.0 / VIPS_MAX(base->Xsize, base->Ysize),
			200);

		/* Apply the zoom and build the pyramid.
		 */
		g_autoptr(VipsImage) x = vips_image_new_from_file(filename,
			"scale", tilesource->zoom,
			NULL);
		tilesource->width = x->Xsize;
		tilesource->height = x->Ysize;

		/* Fake the pyramid geometry. No sense going smaller than
		 * a tile.
		 */
		size = VIPS_MAX(tilesource->width, tilesource->height);
		n_levels = ceil(log2(size));
		tile_levels = ceil(log2(TILE_SIZE));
		tilesource->level_count =
			VIPS_CLIP(1, n_levels - tile_levels, MAX_LEVELS);

		for (level = 0; level < tilesource->level_count; level++) {
			tilesource->level_width[level] = tilesource->width / (1 << level);
			tilesource->level_height[level] = tilesource->height / (1 << level);
		}
	}

	/* Can we open in toilet-roll mode? We need to test that n_pages and
	 * page_size are sane too.
	 */
#ifdef DEBUG
	printf("tilesource_new_from_source: test toilet-roll mode\n");
#endif /*DEBUG*/

	/* Block error messages from eg. page-pyramidal TIFFs where pages
	 * are not all the same size.
	 */
	tilesource->type = TILESOURCE_TYPE_TOILET_ROLL;
	vips_error_freeze();
	g_autoptr(VipsImage) x = tilesource_open(tilesource, 0);
	vips_error_thaw();
	if (x) {
		/* Toilet-roll mode worked. We can update n_pages.
		 */
		if (x->Ysize % tilesource->height == 0) {
			tilesource->n_pages = x->Ysize / tilesource->height;
			tilesource->pages_same_size = TRUE;
		}
		else {
#ifdef DEBUG
			printf("tilesource_new_from_source: bad page layout\n");
#endif /*DEBUG*/

			tilesource->n_pages = 1;
			tilesource->height = x->Ysize;
			VIPS_FREE(tilesource->delay);
			tilesource->n_delay = 0;
		}
	}

	/* Back to plain multipage for the rest of the sniff period. For
	 * example, subifd pyramid needs single page opening.
	 *
	 * We reset this at the end.
	 */
	tilesource->type = TILESOURCE_TYPE_MULTIPAGE;

	/* Are all pages the same size and format, and also all mono (one
	 * band)? We can display pages-as-bands.
	 */
	tilesource->all_mono =
		tilesource->pages_same_size &&
		tilesource->bands == 1;

	/* Test for a subifd pyr first, since we can do that from just
	 * one page.
	 */
	if (!tilesource->level_count) {
		tilesource->subifd_pyramid = TRUE;
		tilesource_get_pyramid_subifd(tilesource);
		if (!tilesource->level_count)
			tilesource->subifd_pyramid = FALSE;
	}

	/* If that failed, try to read as a page pyramid.
	 */
	if (!tilesource->level_count) {
		tilesource->page_pyramid = TRUE;
		tilesource_get_pyramid_page(tilesource);
		if (!tilesource->level_count)
			tilesource->page_pyramid = FALSE;
	}

	/* Sniffing is done ... set the image type.
	 */
	if (tilesource->pages_same_size)
		tilesource->type = TILESOURCE_TYPE_TOILET_ROLL;
	else {
		if (tilesource->page_pyramid)
			tilesource->type = TILESOURCE_TYPE_PAGE_PYRAMID;
		else
			tilesource->type = TILESOURCE_TYPE_MULTIPAGE;
	}

#ifdef DEBUG
	printf("tilesource_new_from_source: after sniff\n");
	tilesource_print(tilesource);
#endif /*DEBUG*/

	/* And now we can reopen in the correct mode.
	 */
	VipsImage *image = tilesource_open(tilesource, 0);
	if (!image)
		return NULL;
	g_assert(!tilesource->image);
	g_assert(!tilesource->image_region);
	tilesource->image = image;
	tilesource->image_region = vips_region_new(tilesource->image);

	/* image_region is used by the bg load thread.
	 */
	vips__region_no_ownership(tilesource->image_region);

	tilesource_default_mode(tilesource);

	tilesource_attach_progress(tilesource);

	return g_steal_pointer(&tilesource);
}

/* Call this some time after tilesource_new_from_file() or
 * tilesource_new_from_source(), and once all callbacks have been
 * attached, to trigger a bg load.
 */
void
tilesource_background_load(Tilesource *tilesource)
{
	/* We ref this tilesource so it won't die before the
	 * background load is done. The matching unref is at the end
	 * of bg load in tilesource_background_load_done_idle().
	 */
	g_object_ref(tilesource);

	g_thread_pool_push(tilesource_background_load_pool,
		tilesource, NULL);
}

/* Request a tile from the pipeline. The tile might be already there (in
 * cache), and we are all done, or it might need to be computed and collected
 * later.
 */
int
tilesource_request_tile(Tilesource *tilesource, Tile *tile)
{
#ifdef DEBUG_VERBOSE
	printf("tilesource_request_tile: %d x %d\n",
		tile->region->valid.left, tile->region->valid.top);
#endif /*DEBUG_VERBOSE*/

	/* Change z if necessary.
	 */
	if (tilesource->current_z != tile->z ||
		!tilesource->display) {
		tilesource->current_z = tile->z;
		tilesource_update_display(tilesource);
	}

	if (vips_region_prepare(tilesource->mask_region, &tile->region->valid))
		return -1;

	/* tile is within a single tile, so we only need to test the first byte
	 * of the mask.
	 */
	tile->valid = VIPS_REGION_ADDR(tilesource->mask_region,
		tile->region->valid.left, tile->region->valid.top)[0];

#ifdef DEBUG_VERBOSE
	printf("  valid = %d\n", tile->valid);
#endif /*DEBUG_VERBOSE*/

	/* If the tile is not in cache, we must fetch to trigger a bg recomp.
	 *
	 * If it is in cache, we also fetch the pixels and set a texture ready
	 * for the cache.
	 */
	if (vips_region_prepare_to(tilesource->rgb_region, tile->region,
			&tile->region->valid,
			tile->region->valid.left,
			tile->region->valid.top))
		return -1;

	/* Do we have new, valid pixels? Update the texture. We need to do
	 * this now since the data in the region may change later.
	 */
	if (tile->valid) {
		tile_free_texture(tile);
		tile_get_texture(tile);
	}

	return 0;
}

/* Try to collect a computed tile.
 */
int
tilesource_collect_tile(Tilesource *tilesource, Tile *tile)
{
#ifdef DEBUG_VERBOSE
	printf("tilesource_collect_tile: %d x %d\n",
		tile->region->valid.left, tile->region->valid.top);
#endif /*DEBUG_VERBOSE*/

	if (vips_region_prepare(tilesource->mask_region, &tile->region->valid))
		return -1;

	/* tile is within a single tile, so we only need to test the first byte
	 * of the mask.
	 */
	tile->valid = VIPS_REGION_ADDR(tilesource->mask_region,
		tile->region->valid.left, tile->region->valid.top)[0];

#ifdef DEBUG_VERBOSE
	printf("  valid = %d\n", tile->valid);
#endif /*DEBUG_VERBOSE*/

	/* Only read out the tile if it's valid. We don't want to trigger another
	 * compute.
	 */
	if (tile->valid) {
		if (vips_region_prepare_to(tilesource->rgb_region, tile->region,
				&tile->region->valid,
				tile->region->valid.left,
				tile->region->valid.top))
			return -1;

		tile_free_texture(tile);
		tile_get_texture(tile);
	}

	return 0;
}

const char *
tilesource_get_path(Tilesource *tilesource)
{
	return tilesource->filename;
}

GFile *
tilesource_get_file(Tilesource *tilesource)
{
	const char *path = tilesource_get_path(tilesource);

	return path ? g_file_new_for_path(path) : NULL;
}

/* The image as used to generate the display, so including page extraction and
 * composition.
 */
VipsImage *
tilesource_get_image(Tilesource *tilesource)
{
	return tilesource->image;
}

/* The base image is the plain open, so no scaling or page extraction.
 */
VipsImage *
tilesource_get_base_image(Tilesource *tilesource)
{
	return tilesource->base;
}

gboolean
tilesource_get_pixel(Tilesource *tilesource, int image_x, int image_y,
	double **vector, int *n)
{
	if (!tilesource->loaded ||
		!tilesource->image)
		return FALSE;

	/* Block outside the image.
	 */
	if (image_x < 0 ||
		image_y < 0 ||
		image_x >= tilesource->image->Xsize ||
		image_y >= tilesource->image->Ysize)
		return FALSE;

	/* Fetch from image (not ->display), even though this can be very slow.
	 * This is run in a bg thread, so speed should not matter too much.
	 */
	if (vips_getpoint(tilesource->image, vector, n, image_x, image_y,
			"unpack_complex", TRUE,
			NULL))
		return FALSE;

	return TRUE;
}

void
tilesource_set_synchronous(Tilesource *tilesource, gboolean synchronous)
{
	if (tilesource->synchronous != synchronous) {
#ifdef DEBUG
		printf("tilesource_set_synchronous: %d", synchronous);
		iobject_print(IOBJECT(tilesource));
#endif /*DEBUG*/

		tilesource->synchronous = synchronous;
		tilesource_update_rgb(tilesource);
	}
}
