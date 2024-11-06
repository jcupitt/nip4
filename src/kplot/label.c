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

void
kplotctx_label_init(struct kplotctx *ctx)
{
	char		buf[128];
	size_t		i;
	cairo_text_extents_t e;
	double		maxh, maxw, offs, lastx,
			lasty, firsty, w, h;

	maxh = maxw = lastx = lasty = firsty = 0.0;

	/*
	 * First, acquire the maximum space that will be required for
	 * the vertical (left or right) or horizontal (top or bottom)
	 * tic labels.
	 */
	kplotctx_font_init(ctx, &ctx->cfg.ticlabelfont);

	for (i = 0; i < ctx->cfg.xtics; i++) {
		offs = 1 == ctx->cfg.xtics ? 0.5 :
			i / (double)(ctx->cfg.xtics - 1);

		/* Call out to xformat function. */
		if (NULL == ctx->cfg.xticlabelfmt)
			snprintf(buf, sizeof(buf), "%g",
				ctx->minv.x + offs *
				(ctx->maxv.x - ctx->minv.x));
		else
			(*ctx->cfg.xticlabelfmt)
				(ctx->minv.x + offs *
				 (ctx->maxv.x - ctx->minv.x),
				 buf, sizeof(buf));

		cairo_text_extents(ctx->cr, buf, &e);

		/*
		 * Important: if we're on the last x-axis value, then
		 * save the width, because we'll check that the
		 * right-hand buffer zone accomodates for it.
		 * FIXME: only do this is TICLABEL_TOP, etc...
		 */
		if (i == ctx->cfg.xtics - 1 && ctx->cfg.xticlabelrot > 0.0)
			lastx = e.width * cos
				(M_PI * 2.0 -
				 (M_PI_2 - ctx->cfg.xticlabelrot)) +
				e.height * sin((ctx->cfg.xticlabelrot));
		else if (i == ctx->cfg.xtics - 1)
			lastx = e.width / 2.0;

		/*
		 * If we're rotating, get our height by computing the
		 * sum of the vertical segments.
		 */
		if (ctx->cfg.xticlabelrot > 0.0)
			e.height = e.width * sin(ctx->cfg.xticlabelrot) +
				e.height * cos(M_PI * 2.0 -
					(M_PI_2 - ctx->cfg.xticlabelrot));

		if (e.height > maxh)
			maxh = e.height;
	}

	/* Now for the y-axis... */
	for (i = 0; i < ctx->cfg.ytics; i++) {
		offs = 1 == ctx->cfg.ytics ? 0.5 :
			i / (double)(ctx->cfg.ytics - 1);

		if (NULL == ctx->cfg.yticlabelfmt)
			snprintf(buf, sizeof(buf), "%g",
				ctx->minv.y + offs *
				(ctx->maxv.y - ctx->minv.y));
		else
			(*ctx->cfg.yticlabelfmt)
				(ctx->minv.y + offs *
				 (ctx->maxv.y - ctx->minv.y),
				 buf, sizeof(buf));

		cairo_text_extents(ctx->cr, buf, &e);

		/*
		 * If we're the first or last tic label, record our
		 * height so that the plot is buffered and our label
		 * isn't cut off if there are no margins.
		 */
		if (i == 0)
			firsty = e.height / 2.0;
		if (i == ctx->cfg.ytics - 1)
			lasty = e.height / 2.0;

		if (e.width > maxw)
			maxw = e.width;
	}

	/*
	 * Take into account the axis labels.
	 * These sit to the bottom and left of the plot and its tic
	 * labels.
	 */
	kplotctx_font_init(ctx, &ctx->cfg.axislabelfont);

	if (NULL != ctx->cfg.xaxislabel) {
		bbox_extents(ctx, ctx->cfg.xaxislabel,
			&h, &w, ctx->cfg.xaxislabelrot);
		ctx->dims.y -= h + ctx->cfg.xaxislabelpad;
	}

	if (NULL != ctx->cfg.x2axislabel) {
		bbox_extents(ctx, ctx->cfg.x2axislabel,
			&h, &w, ctx->cfg.xaxislabelrot);
		ctx->offs.y += h + ctx->cfg.xaxislabelpad;
		ctx->dims.y -= h + ctx->cfg.xaxislabelpad;
	}

	if (NULL != ctx->cfg.yaxislabel) {
		bbox_extents(ctx, ctx->cfg.yaxislabel,
			&h, &w, ctx->cfg.yaxislabelrot);
		ctx->offs.x += w + ctx->cfg.yaxislabelpad;
		ctx->dims.x -= w + ctx->cfg.yaxislabelpad;
	}

	if (NULL != ctx->cfg.y2axislabel) {
		bbox_extents(ctx, ctx->cfg.y2axislabel,
			&h, &w, ctx->cfg.yaxislabelrot);
		ctx->dims.x -= w + ctx->cfg.yaxislabelpad;
	}

	if (TICLABEL_LEFT & ctx->cfg.ticlabel) {
		ctx->offs.x += maxw + ctx->cfg.yticlabelpad;
		ctx->dims.x -= maxw + ctx->cfg.yticlabelpad;
	}

	/*
	 * Now look at the tic labels.
	 * Start with the right label.
	 * Also check if our overflow for the horizontal axes into the
	 * right buffer zone exists.
	 */
	if (TICLABEL_RIGHT & ctx->cfg.ticlabel) {
		if (maxw + ctx->cfg.yticlabelpad > lastx)
			ctx->dims.x -= maxw + ctx->cfg.yticlabelpad;
		else
			ctx->dims.x -= lastx;
	} else if (lastx > 0.0)
		ctx->dims.x -= lastx;

	/*
	 * Like with TICLABEL_RIGHT, we accomodate for the topmost vertical
	 * axes bleeding into the horizontal axis area above.
	 */
	if (TICLABEL_TOP & ctx->cfg.ticlabel) {
		if (maxh + ctx->cfg.xticlabelpad > lasty) {
			ctx->offs.y += maxh + ctx->cfg.xticlabelpad;
			ctx->dims.y -= maxh + ctx->cfg.xticlabelpad;
		} else {
			ctx->offs.y += lasty;
			ctx->dims.y -= lasty;
		}
	} else if (lasty > 0.0) {
		ctx->offs.y += lasty;
		ctx->dims.y -= lasty;
	}

	if (TICLABEL_BOTTOM & ctx->cfg.ticlabel) {
		if (maxh + ctx->cfg.xticlabelpad > firsty)
			ctx->dims.y -= maxh + ctx->cfg.xticlabelpad;
		else
			ctx->dims.y -= firsty;
	} else if (firsty > 0.0)
		ctx->dims.y -= firsty;

	/*
	 * Now we actually want to draw the tic labels below the plot,
	 * now that we know what the plot dimensions are going to be.
	 * Start with the x-axis.
	 */
	kplotctx_font_init(ctx, &ctx->cfg.ticlabelfont);

	for (i = 0; i < ctx->cfg.xtics; i++) {
		offs = 1 == ctx->cfg.xtics ? 0.5 :
			i / (double)(ctx->cfg.xtics - 1);

		if (NULL == ctx->cfg.xticlabelfmt)
			snprintf(buf, sizeof(buf), "%g",
				ctx->minv.x + offs *
				(ctx->maxv.x - ctx->minv.x));
		else
			(*ctx->cfg.xticlabelfmt)
				(ctx->minv.x + offs *
				 (ctx->maxv.x - ctx->minv.x),
				 buf, sizeof(buf));

		cairo_text_extents(ctx->cr, buf, &e);

		if (TICLABEL_BOTTOM & ctx->cfg.ticlabel) {
			if (ctx->cfg.xticlabelrot > 0.0) {
				cairo_move_to(ctx->cr,
					ctx->offs.x +
					offs * ctx->dims.x,
					ctx->offs.y + ctx->dims.y +
					e.height * cos
					 (M_PI * 2.0 -
					  (M_PI_2 - ctx->cfg.xticlabelrot)) +
					ctx->cfg.xticlabelpad);
				cairo_save(ctx->cr);
				cairo_rotate(ctx->cr, ctx->cfg.xticlabelrot);
			} else
				cairo_move_to(ctx->cr,
					ctx->offs.x + offs * ctx->dims.x -
					(e.width / 2.0),
					ctx->offs.y + ctx->dims.y +
					maxh + ctx->cfg.xticlabelpad);

			cairo_show_text(ctx->cr, buf);
			if (ctx->cfg.xticlabelrot > 0.0)
				cairo_restore(ctx->cr);
		}

		if (TICLABEL_TOP & ctx->cfg.ticlabel) {
			cairo_move_to(ctx->cr,
				ctx->offs.x + offs * ctx->dims.x -
				(e.width / 2.0),
				ctx->offs.y - maxh);
			cairo_show_text(ctx->cr, buf);
		}
	}

	/* Now move on to the y-axis... */
	for (i = 0; i < ctx->cfg.ytics; i++) {
		offs = 1 == ctx->cfg.ytics ? 0.5 :
			i / (double)(ctx->cfg.ytics - 1);

		if (NULL == ctx->cfg.yticlabelfmt)
			snprintf(buf, sizeof(buf), "%g",
				ctx->minv.y + offs *
				(ctx->maxv.y - ctx->minv.y));
		else
			(*ctx->cfg.yticlabelfmt)
				(ctx->minv.y + offs *
				 (ctx->maxv.y - ctx->minv.y),
				 buf, sizeof(buf));

		cairo_text_extents(ctx->cr, buf, &e);

		if (TICLABEL_LEFT & ctx->cfg.ticlabel) {
			cairo_move_to(ctx->cr,
				ctx->offs.x - e.width -
				ctx->cfg.yticlabelpad,
				(ctx->offs.y + ctx->dims.y) -
				(offs * ctx->dims.y) +
				(e.height / 2.0));
			cairo_show_text(ctx->cr, buf);
		}
		if (TICLABEL_RIGHT & ctx->cfg.ticlabel) {
			cairo_move_to(ctx->cr,
				ctx->offs.x + ctx->dims.x +
				ctx->cfg.yticlabelpad,
				(ctx->offs.y + ctx->dims.y) -
				(offs * ctx->dims.y) +
				(e.height / 2.0));
			cairo_show_text(ctx->cr, buf);
		}
	}

	/*
	 * Now show the axis labels.
	 * These go after everything else has been computed, as we can
	 * just set them given the margin offset.
	 */
	kplotctx_font_init(ctx, &ctx->cfg.axislabelfont);

	if (NULL != ctx->cfg.xaxislabel) {
		bbox_extents(ctx, ctx->cfg.xaxislabel,
			&h, &w, ctx->cfg.xaxislabelrot);
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
		bbox_extents(ctx, ctx->cfg.x2axislabel,
			&h, &w, ctx->cfg.xaxislabelrot);
		cairo_save(ctx->cr);
		cairo_translate(ctx->cr,
			ctx->offs.x + ctx->dims.x / 2.0,
			(MARGIN_TOP & ctx->cfg.margin ?
			ctx->cfg.marginsz : 0.0) + h / 2.0);
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
		bbox_extents(ctx, ctx->cfg.yaxislabel,
			&h, &w, ctx->cfg.yaxislabelrot);
		cairo_save(ctx->cr);
		cairo_translate(ctx->cr,
			(MARGIN_LEFT & ctx->cfg.margin ?
			 ctx->cfg.marginsz : 0.0) + w / 2.0,
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
		bbox_extents(ctx, ctx->cfg.y2axislabel,
			&h, &w, ctx->cfg.yaxislabelrot);
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
