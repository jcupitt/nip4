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
kplotctx_ccfg_init(struct kplotctx *ctx, struct kplotccfg *cfg)
{

	switch (cfg->type) {
	case (KPLOTCTYPE_PALETTE):
		cairo_set_source_rgba(ctx->cr,
			ctx->cfg.clrs[cfg->palette % ctx->cfg.clrsz].rgba[0],
			ctx->cfg.clrs[cfg->palette % ctx->cfg.clrsz].rgba[1],
			ctx->cfg.clrs[cfg->palette % ctx->cfg.clrsz].rgba[2],
			ctx->cfg.clrs[cfg->palette % ctx->cfg.clrsz].rgba[3]);
		break;
	case (KPLOTCTYPE_PATTERN):
		cairo_set_source(ctx->cr, cfg->pattern);
		break;
	case (KPLOTCTYPE_RGBA):
		cairo_set_source_rgba(ctx->cr, cfg->rgba[0],
			cfg->rgba[1], cfg->rgba[2], cfg->rgba[3]);
		break;
	default:
		abort();
	}
}

void
kplotctx_ticln_init(struct kplotctx *ctx, struct kplotticln *line)
{

	kplotctx_ccfg_init(ctx, &line->clr);
	cairo_set_line_width(ctx->cr, line->sz);
	cairo_set_dash(ctx->cr, line->dashes,
		line->dashesz, line->dashoff);
}

void
kplotctx_font_init(struct kplotctx *ctx, struct kplotfont *font)
{

	kplotctx_ccfg_init(ctx, &font->clr);
	cairo_select_font_face
		(ctx->cr, font->family,
		 font->slant,
		 font->weight);
	cairo_set_font_size(ctx->cr, font->sz);
}

void
kplotctx_point_init(struct kplotctx *ctx, struct kplotpoint *pnt)
{

	kplotctx_ccfg_init(ctx, &pnt->clr);
	cairo_set_line_width(ctx->cr, pnt->sz);
	cairo_set_dash(ctx->cr, pnt->dashes,
		pnt->dashesz, pnt->dashoff);
}

void
kplotctx_line_init(struct kplotctx *ctx, struct kplotline *line)
{

	kplotctx_ccfg_init(ctx, &line->clr);
	cairo_set_line_width(ctx->cr, line->sz);
	cairo_set_dash(ctx->cr, line->dashes,
		line->dashesz, line->dashoff);
	cairo_set_line_join(ctx->cr, line->join);
}

/*
 * Given a plotting context and a position for drawing a line, determine
 * whether we want to "fix" the line so that it's fine.
 * This is a foible of Cairo and drawing with doubles.
 */
double
kplotctx_line_fix(const struct kplotctx *ctx, double sz, double pos)
{
	double	 v;

	if (0 == (int)sz % 2)
		return(pos);
	v = pos - floor(pos);
	return(v < DBL_EPSILON ? pos + 0.5 : pos - v + 0.5);
}
