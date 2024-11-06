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
kplotctx_border_init(struct kplotctx *ctx)
{
	double		 v;

	kplotctx_line_init(ctx, &ctx->cfg.borderline);

	if (BORDER_LEFT & ctx->cfg.border) {
		v = kplotctx_line_fix(ctx,
			ctx->cfg.borderline.sz, ctx->offs.x);
		cairo_move_to(ctx->cr, v, ctx->offs.y);
		cairo_rel_line_to(ctx->cr, 0.0, ctx->dims.y);
	}

	if (BORDER_RIGHT & ctx->cfg.border) {
		v = kplotctx_line_fix(ctx,
			ctx->cfg.borderline.sz,
			ctx->offs.x + ctx->dims.x);
		cairo_move_to(ctx->cr, v, ctx->offs.y);
		cairo_rel_line_to(ctx->cr, 0.0, ctx->dims.y);
	}

	if (BORDER_TOP & ctx->cfg.border) {
		v = kplotctx_line_fix(ctx,
			ctx->cfg.borderline.sz, ctx->offs.y);
		cairo_move_to(ctx->cr, ctx->offs.x, v);
		cairo_rel_line_to(ctx->cr, ctx->dims.x, 0.0);
	}

	if (BORDER_BOTTOM & ctx->cfg.border) {
		v = kplotctx_line_fix(ctx,
			ctx->cfg.borderline.sz,
			ctx->offs.y + ctx->dims.y);
		cairo_move_to(ctx->cr, ctx->offs.x, v);
		cairo_rel_line_to(ctx->cr, ctx->dims.x, 0.0);
	}

	cairo_stroke(ctx->cr);
}
