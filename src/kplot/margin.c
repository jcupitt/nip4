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
kplotctx_margin_init(struct kplotctx *ctx)
{

	ctx->dims.x = ctx->w;
	ctx->dims.y = ctx->h;

	if (MARGIN_LEFT & ctx->cfg.margin) {
		ctx->dims.x -= ctx->cfg.marginsz;
		ctx->offs.x = ctx->cfg.marginsz;
	}
	if (MARGIN_RIGHT & ctx->cfg.margin)
		ctx->dims.x -= ctx->cfg.marginsz;

	if (MARGIN_TOP & ctx->cfg.margin) {
		ctx->dims.y -= ctx->cfg.marginsz;
		ctx->offs.y = ctx->cfg.marginsz;
	}
	if (MARGIN_BOTTOM & ctx->cfg.margin)
		ctx->dims.y -= ctx->cfg.marginsz;
}

