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

void
kplot_loop_xtics(struct kplotctx *ctx, tic_loop_fn fn, void *user_data)
{
	double offs;

	double max = ctx->cfg.extrema_xmax;
	double min = ctx->cfg.extrema_xmin;
	double interval = ctx->cfg.xinterval;

	double range = max - min;
	double start = (ciel(min / interval) * interval - min) / range;
	double step = interval / range;

	for (double offs = start; offs <= 1.0; offs += step)
		fn(ctx, offs, user_data);
}

void
kplot_loop_ytics(struct kplotctx *ctx, tic_loop_fn fn, void *user_data)
{
	double offs;

	double max = ctx->cfg.extrema_ymax;
	double min = ctx->cfg.extrema_ymin;
	double interval = ctx->cfg.yinterval;

	double range = max - min;
	double start = (ciel(min / interval) * interval - min) / range;
	double step = interval / range;

	for (double offs = start; offs <= 1.0; offs += step)
		fn(ctx, offs, user_data);
}

static void
kplotctx_tic_init_x(struct kplotctx *ctx, double offs, void *user_data)
{
	double v = kplotctx_line_fix(ctx,
		ctx->cfg.ticline.sz,
		ctx->offs.x + offs * ctx->dims.x);

	if (TIC_BOTTOM_IN & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, v, ctx->offs.y + ctx->dims.y);
		cairo_rel_line_to(ctx->cr, 0.0, -ctx->cfg.ticline.len);
	}
	if (TIC_BOTTOM_OUT & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, v, ctx->offs.y + ctx->dims.y);
		cairo_rel_line_to(ctx->cr, 0.0, ctx->cfg.ticline.len);
	}
	if (TIC_TOP_IN & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, v, ctx->offs.y);
		cairo_rel_line_to(ctx->cr, 0.0, ctx->cfg.ticline.len);
	}
	if (TIC_TOP_OUT & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, v, ctx->offs.y);
		cairo_rel_line_to(ctx->cr, 0.0, -ctx->cfg.ticline.len);
	}
}

static void
kplotctx_tic_init_y(struct kplotctx *ctx, double offs, void *user_data)
{
	double v = kplotctx_line_fix(ctx,
		ctx->cfg.ticline.sz,
		ctx->offs.y + offs * ctx->dims.y);

	if (TIC_LEFT_IN & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, ctx->offs.x, v);
		cairo_rel_line_to(ctx->cr, ctx->cfg.ticline.len, 0.0);
	}
	if (TIC_LEFT_OUT & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, ctx->offs.x, v);
		cairo_rel_line_to(ctx->cr, -ctx->cfg.ticline.len, 0.0);
	}
	if (TIC_RIGHT_IN & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, ctx->offs.x + ctx->dims.x, v);
		cairo_rel_line_to(ctx->cr, -ctx->cfg.ticline.len, 0.0);
	}
	if (TIC_RIGHT_OUT & ctx->cfg.tic) {
		cairo_move_to(ctx->cr, ctx->offs.x + ctx->dims.x, v);
		cairo_rel_line_to(ctx->cr, ctx->cfg.ticline.len, 0.0);
	}
}

void
kplotctx_tic_init(struct kplotctx *ctx)
{
	kplotctx_ticln_init(ctx, &ctx->cfg.ticline);

	kplot_loop_xtics(ctx, kplotctx_tic_init_x, NULL);
	kplot_loop_ytics(ctx, kplotctx_tic_init_y, NULL);

	cairo_stroke(ctx->cr);
}
