/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "kplot.h"

static void
bbox_extents(struct kplotctx *ctx, const char *v,
	double *h, double *w, double rot)
{
	cairo_text_extents_t e;

	cairo_text_extents(ctx->cr, v, &e);
	*h = fabs(e.width * sin(rot)) + fabs(e.height * cos(rot));
	*w = fabs(e.width * cos(rot)) + fabs(e.height * sin(rot));
}

typedef struct _Bounds {
	double width;
	double height;
} Bounds;

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

static void
kplotctx_label_init_x(struct kplotctx *ctx, double offs, void *user_data)
{
	Bounds *bounds = (Bounds *) user_data;

	cairo_text_extents_t e;
	char buf[128];

	/* Call out to xformat function. */
	if (NULL == ctx->cfg.xticlabelfmt)
		snprintf(buf, sizeof(buf), "%g",
			ctx->minv.x + offs * (ctx->maxv.x - ctx->minv.x));
	else
		(*ctx->cfg.xticlabelfmt)
			(ctx->minv.x + offs * (ctx->maxv.x - ctx->minv.x),
			buf, sizeof(buf));

	cairo_text_extents(ctx->cr, buf, &e);

	// rotate the label size, if necessary
	double width = ctx->cfg.xticlabelrot > 0.0 ?
		e.width * cos(M_PI * 2.0 - (M_PI_2 - ctx->cfg.xticlabelrot)) +
			e.height * sin((ctx->cfg.xticlabelrot)) :
		e.width;

	double height = ctx->cfg.xticlabelrot > 0.0 ?
		e.width * sin(ctx->cfg.xticlabelrot) +
			e.height * cos(M_PI * 2.0 - (M_PI_2 - ctx->cfg.xticlabelrot)) :
		e.height;

	bounds->width = MAX(bounds->width, width);
	bounds->height = MAX(bounds->height, height);
}

static void
kplotctx_label_init_y(struct kplotctx *ctx, double offs, void *user_data)
{
	Bounds *bounds = (Bounds *) user_data;

	cairo_text_extents_t e;
	char buf[128];

	if (NULL == ctx->cfg.yticlabelfmt)
		snprintf(buf, sizeof(buf), "%g",
			ctx->minv.y + offs * (ctx->maxv.y - ctx->minv.y));
	else
		(*ctx->cfg.yticlabelfmt)
			(ctx->minv.y + offs * (ctx->maxv.y - ctx->minv.y),
			buf, sizeof(buf));

	cairo_text_extents(ctx->cr, buf, &e);

	bounds->width = MAX(bounds->width, e.width);
	bounds->height = MAX(bounds->height, e.height);
}

static void
kplotctx_label_init_draw_x(struct kplotctx *ctx, double offs, void *user_data)
{
	Bounds *bounds = (Bounds *) user_data;

	cairo_text_extents_t e;
	char buf[128];

	if (NULL == ctx->cfg.xticlabelfmt)
		snprintf(buf, sizeof(buf), "%g",
			ctx->minv.x + offs * (ctx->maxv.x - ctx->minv.x));
	else
		(*ctx->cfg.xticlabelfmt)
			(ctx->minv.x + offs * (ctx->maxv.x - ctx->minv.x),
			buf, sizeof(buf));

	cairo_text_extents(ctx->cr, buf, &e);

	if (TICLABEL_BOTTOM & ctx->cfg.ticlabel) {
		if (ctx->cfg.xticlabelrot > 0.0) {
			cairo_move_to(ctx->cr,
				ctx->offs.x + offs * ctx->dims.x,
				ctx->offs.y + ctx->dims.y +
				 e.height * cos(M_PI * 2.0 - (M_PI_2 - ctx->cfg.xticlabelrot)) +
				 ctx->cfg.xticlabelpad);
			cairo_save(ctx->cr);
			cairo_rotate(ctx->cr, ctx->cfg.xticlabelrot);
		} else
			cairo_move_to(ctx->cr,
				ctx->offs.x + offs * ctx->dims.x - (e.width / 2.0),
				ctx->offs.y + ctx->dims.y +
				 bounds->height + ctx->cfg.xticlabelpad);

		cairo_show_text(ctx->cr, buf);
		if (ctx->cfg.xticlabelrot > 0.0)
			cairo_restore(ctx->cr);
	}

	if (TICLABEL_TOP & ctx->cfg.ticlabel) {
		cairo_move_to(ctx->cr,
			ctx->offs.x + offs * ctx->dims.x - (e.width / 2.0),
			ctx->offs.y - bounds->height);
		cairo_show_text(ctx->cr, buf);
	}
}

static void
kplotctx_label_init_draw_y(struct kplotctx *ctx, double offs, void *user_data)
{
	cairo_text_extents_t e;
	char buf[128];

	if (NULL == ctx->cfg.yticlabelfmt)
		snprintf(buf, sizeof(buf), "%g",
			ctx->minv.y + offs * (ctx->maxv.y - ctx->minv.y));
	else
		(*ctx->cfg.yticlabelfmt)
			(ctx->minv.y + offs * (ctx->maxv.y - ctx->minv.y),
			buf, sizeof(buf));

	cairo_text_extents(ctx->cr, buf, &e);

	if (TICLABEL_LEFT & ctx->cfg.ticlabel) {
		cairo_move_to(ctx->cr,
			ctx->offs.x - e.width - ctx->cfg.yticlabelpad,
			(ctx->offs.y + ctx->dims.y) - (offs * ctx->dims.y) +
			 (e.height / 2.0));
		cairo_show_text(ctx->cr, buf);
	}
	if (TICLABEL_RIGHT & ctx->cfg.ticlabel) {
		cairo_move_to(ctx->cr,
			ctx->offs.x + ctx->dims.x + ctx->cfg.yticlabelpad,
			(ctx->offs.y + ctx->dims.y) - (offs * ctx->dims.y) +
			 (e.height / 2.0));
		cairo_show_text(ctx->cr, buf);
	}
}

void
kplotctx_label_init(struct kplotctx *ctx)
{
	// max size of horizontal axis labels
	Bounds xbounds = { 0 };
	kplot_loop_xtics(ctx, kplotctx_label_init_x, &xbounds);
	// max size of vertical axis labels
	Bounds ybounds = { 0 };
	kplot_loop_ytics(ctx, kplotctx_label_init_y, &ybounds);

	/*
	 * Take into account the axis labels.
	 * These sit to the bottom and left of the plot and its tic
	 * labels.
	 */
	kplotctx_font_init(ctx, &ctx->cfg.axislabelfont);

	if (NULL != ctx->cfg.xaxislabel) {
		double w, h;

		bbox_extents(ctx, ctx->cfg.xaxislabel, &h, &w, ctx->cfg.xaxislabelrot);
		ctx->dims.y -= h + ctx->cfg.xaxislabelpad;
	}

	if (NULL != ctx->cfg.x2axislabel) {
		double w, h;

		bbox_extents(ctx, ctx->cfg.x2axislabel, &h, &w, ctx->cfg.xaxislabelrot);
		ctx->offs.y += h + ctx->cfg.xaxislabelpad;
		ctx->dims.y -= h + ctx->cfg.xaxislabelpad;
	}

	if (NULL != ctx->cfg.yaxislabel) {
		double w, h;

		bbox_extents(ctx, ctx->cfg.yaxislabel, &h, &w, ctx->cfg.yaxislabelrot);
		ctx->offs.x += w + ctx->cfg.yaxislabelpad;
		ctx->dims.x -= w + ctx->cfg.yaxislabelpad;
	}

	if (NULL != ctx->cfg.y2axislabel) {
		double w, h;

		bbox_extents(ctx, ctx->cfg.y2axislabel, &h, &w, ctx->cfg.yaxislabelrot);
		ctx->dims.x -= w + ctx->cfg.yaxislabelpad;
	}

	if (TICLABEL_LEFT & ctx->cfg.ticlabel) {
		ctx->offs.x += ybounds.width + ctx->cfg.yticlabelpad;
		ctx->dims.x -= ybounds.width + ctx->cfg.yticlabelpad;
	}

	/*
	 * Now look at the tic labels.
	 * Start with the right label.
	 * Also check if our overflow for the horizontal axes into the
	 * right buffer zone exists.
	 */
	if (TICLABEL_RIGHT & ctx->cfg.ticlabel) {
		if (ybounds.width + ctx->cfg.yticlabelpad > ybounds.height / 2)
			ctx->dims.x -= ybounds.width + ctx->cfg.yticlabelpad;
		else
			ctx->dims.x -= xbounds.width / 2;
	} else if (xbounds.width / 2 > 0.0)
		ctx->dims.x -= xbounds.width / 2;

	/*
	 * Like with TICLABEL_RIGHT, we accomodate for the topmost vertical
	 * axes bleeding into the horizontal axis area above.
	 */
	if (TICLABEL_TOP & ctx->cfg.ticlabel) {
		if (xbounds.height + ctx->cfg.xticlabelpad > ybounds.height / 2) {
			ctx->offs.y += xbounds.height + ctx->cfg.xticlabelpad;
			ctx->dims.y -= xbounds.height + ctx->cfg.xticlabelpad;
		} else {
			ctx->offs.y += ybounds.height / 2;
			ctx->dims.y -= ybounds.height / 2;
		}
	} else if (ybounds.height / 2 > 0.0) {
		ctx->offs.y += ybounds.height / 2;
		ctx->dims.y -= ybounds.height / 2;
	}

	if (TICLABEL_BOTTOM & ctx->cfg.ticlabel) {
		if (xbounds.height + ctx->cfg.xticlabelpad > ybounds.height / 2)
			ctx->dims.y -= xbounds.height + ctx->cfg.xticlabelpad;
		else
			ctx->dims.y -= ybounds.height / 2;
	} else if (ybounds.height / 2 > 0.0)
		ctx->dims.y -= ybounds.height / 2;

	/*
	 * Now we actually want to draw the tic labels below the plot,
	 * now that we know what the plot dimensions are going to be.
	 * Start with the x-axis.
	 */
	kplotctx_font_init(ctx, &ctx->cfg.ticlabelfont);

	kplot_loop_xtics(ctx, kplotctx_label_init_draw_x, &xbounds);
	kplot_loop_ytics(ctx, kplotctx_label_init_draw_y, NULL);

	/*
	 * Now show the axis labels.
	 * These go after everything else has been computed, as we can
	 * just set them given the margin offset.
	 */
	kplotctx_font_init(ctx, &ctx->cfg.axislabelfont);

	if (NULL != ctx->cfg.xaxislabel) {
		double w, h;
		cairo_text_extents_t e;

		bbox_extents(ctx, ctx->cfg.xaxislabel, &h, &w, ctx->cfg.xaxislabelrot);
		cairo_save(ctx->cr);
		cairo_translate(ctx->cr,
			ctx->offs.x + ctx->dims.x / 2.0,
			(MARGIN_BOTTOM & ctx->cfg.margin ?
			ctx->h - ctx->cfg.marginsz : ctx->h) - h / 2.0);
		cairo_rotate(ctx->cr, ctx->cfg.xaxislabelrot);
		cairo_text_extents(ctx->cr, ctx->cfg.xaxislabel, &e);
		w = -e.width / 2.0;
		h = e.height / 2.0;
		cairo_translate(ctx->cr, w, h);
		cairo_move_to(ctx->cr, 0.0, 0.0);
		cairo_show_text(ctx->cr, ctx->cfg.xaxislabel);
		cairo_restore(ctx->cr);
	}

	if (NULL != ctx->cfg.x2axislabel) {
		double w, h;
		cairo_text_extents_t e;

		bbox_extents(ctx, ctx->cfg.x2axislabel, &h, &w, ctx->cfg.xaxislabelrot);
		cairo_save(ctx->cr);
		cairo_translate(ctx->cr,
			ctx->offs.x + ctx->dims.x / 2.0,
			(MARGIN_TOP & ctx->cfg.margin ? ctx->cfg.marginsz : 0.0) + h / 2.0);
		cairo_rotate(ctx->cr, ctx->cfg.xaxislabelrot);
		cairo_text_extents(ctx->cr, ctx->cfg.x2axislabel, &e);
		w = -e.width / 2.0;
		h = e.height / 2.0;
		cairo_translate(ctx->cr, w, h);
		cairo_move_to(ctx->cr, 0.0, 0.0);
		cairo_show_text(ctx->cr, ctx->cfg.x2axislabel);
		cairo_restore(ctx->cr);
	}

	if (NULL != ctx->cfg.yaxislabel) {
		double w, h;
		cairo_text_extents_t e;

		bbox_extents(ctx, ctx->cfg.yaxislabel, &h, &w, ctx->cfg.yaxislabelrot);
		cairo_save(ctx->cr);
		cairo_translate(ctx->cr,
			(MARGIN_LEFT & ctx->cfg.margin ? ctx->cfg.marginsz : 0.0) + w / 2.0,
			ctx->offs.y + ctx->dims.y / 2.0);
		cairo_rotate(ctx->cr, ctx->cfg.yaxislabelrot);
		cairo_text_extents(ctx->cr, ctx->cfg.yaxislabel, &e);
		w = -e.width / 2.0;
		h = e.height / 2.0;
		cairo_translate(ctx->cr, w, h);
		cairo_move_to(ctx->cr, 0.0, 0.0);
		cairo_show_text(ctx->cr, ctx->cfg.yaxislabel);
		cairo_restore(ctx->cr);
	}

	if (NULL != ctx->cfg.y2axislabel) {
		double w, h;
		cairo_text_extents_t e;

		bbox_extents(ctx, ctx->cfg.y2axislabel, &h, &w, ctx->cfg.yaxislabelrot);
		cairo_save(ctx->cr);
		cairo_translate(ctx->cr,
			(MARGIN_RIGHT & ctx->cfg.margin ?
			 ctx->w - ctx->cfg.marginsz : ctx->w) - w / 2.0,
			ctx->offs.y + ctx->dims.y / 2.0);
		cairo_rotate(ctx->cr, ctx->cfg.yaxislabelrot);
		cairo_text_extents(ctx->cr, ctx->cfg.y2axislabel, &e);
		w = -e.width / 2.0;
		h = e.height / 2.0;
		cairo_translate(ctx->cr, w, h);
		cairo_move_to(ctx->cr, 0.0, 0.0);
		cairo_show_text(ctx->cr, ctx->cfg.y2axislabel);
		cairo_restore(ctx->cr);
	}
}
