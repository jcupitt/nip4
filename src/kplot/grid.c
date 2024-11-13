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
kplotctx_grid_init_x(struct kplotctx *ctx, double offs, void *user_data)
{
	double v = kplotctx_line_fix(ctx,
		ctx->cfg.gridline.sz,
		ctx->offs.x + offs * ctx->dims.x);

	cairo_move_to(ctx->cr, v, ctx->offs.y);
	cairo_rel_line_to(ctx->cr, 0.0, ctx->dims.y);
}

static void
kplotctx_grid_init_y(struct kplotctx *ctx, double offs, void *user_data)
{
	double v = kplotctx_line_fix(ctx,
		ctx->cfg.gridline.sz,
		ctx->offs.y + ctx->dims.y - offs * ctx->dims.y);

	cairo_move_to(ctx->cr, ctx->offs.x, v);
	cairo_rel_line_to(ctx->cr, ctx->dims.x, 0.0);
}

void
kplotctx_grid_init(struct kplotctx *ctx)
{
	kplotctx_line_init(ctx, &ctx->cfg.gridline);

	if (GRID_X & ctx->cfg.grid)
		kplot_loop_xtics(ctx, kplotctx_grid_init_x, NULL);

	if (GRID_Y & ctx->cfg.grid)
		kplot_loop_ytics(ctx, kplotctx_grid_init_y, NULL);

	cairo_stroke(ctx->cr);
}
