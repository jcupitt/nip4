/*	$Id$ */
/*
 * Copyright (c) 2014, 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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

/*
 * Simple function to check that the double-precision values in the
 * kpair are valid: normal (or 0.0) values.
 */
static inline int
kpair_vrfy(const struct kpair *data)
{

	if (0.0 != data->x && ! isnormal(data->x))
		return(0);
	if (0.0 != data->y && ! isnormal(data->y))
		return(0);
	return(1);
}

/*
 * Set the pair "kp" to the value at position "pos", which depends upon
 * the smoothing type (if stipulated).
 * The value "kp" SHOULD NOT be cleared between invocations, as some
 * data streams (e.g., KSMOOTH_CDF) accumulate it.
 */
static void
kpair_set(const struct kplotdat *d, size_t pos, struct kpair *kp)
{
	size_t		 j, sz, samps;
	ssize_t		 start;

	switch (d->smthtype) {
	case (KSMOOTH_CDF):
		kp->x = d->datas[0]->pairs[pos].x;
		kp->y += d->datas[0]->pairs[pos].y / d->sum;
		break;
	case (KSMOOTH_PMF):
		kp->x = d->datas[0]->pairs[pos].x;
		kp->y = d->datas[0]->pairs[pos].y / d->sum;
		break;
	case (KSMOOTH_MOVAVG):
		*kp = d->datas[0]->pairs[pos];
		samps = d->smth.movsamples / 2;
		start = pos - samps;
		sz = pos + samps;
		if (start < 0 || sz >= d->datas[0]->pairsz)
			break;
		for (kp->y = 0.0, j = start; j <= sz; j++) {
			if ( ! kpair_vrfy(&d->datas[0]->pairs[j]))
				break;
			kp->y += d->datas[0]->pairs[j].y;
		}
		kp->y /= (double)d->smth.movsamples;
		if (j <= sz)
			*kp = d->datas[0]->pairs[pos];
		break;
	default:
		*kp = d->datas[0]->pairs[pos];
		break;
	}
}

/*
 * Accumulate extrema where we add and subtract the second data source
 * from the first, e.g., in a graph with mean and standard deviation.
 */
static void
kdata_extrema_yerr(struct kplotdat *d, struct kplotctx *ctx)
{
	size_t	 	 i, sz;
	struct kpair	*p, *err;

	assert(d->datasz > 1);
	p = d->datas[0]->pairs;
	err = d->datas[1]->pairs;

	/* Truncate to the smaller of the two pair lengths. */
	sz = d->datas[0]->pairsz < d->datas[1]->pairsz ?
		d->datas[0]->pairsz : d->datas[1]->pairsz;

	for (i = 0; i < sz; i++) {
		/* Both must be valid. */
		if ( ! (kpair_vrfy(&p[i]) && kpair_vrfy(&err[i])))
			continue;

		/*
		 * Since the error can be negative, check in both
		 * directions from the basis point.
		 */
		if (p[i].x < ctx->minv.x)
			ctx->minv.x = p[i].x;
		if (p[i].x > ctx->maxv.x)
			ctx->maxv.x = p[i].x;
		if (p[i].y - err[i].y < ctx->minv.y)
			ctx->minv.y = p[i].y - err[i].y;
		if (p[i].y + err[i].y < ctx->minv.y)
			ctx->minv.y = p[i].y + err[i].y;
		if (p[i].y - err[i].y > ctx->maxv.y)
			ctx->maxv.y = p[i].y - err[i].y;
		if (p[i].y + err[i].y > ctx->maxv.y)
			ctx->maxv.y = p[i].y + err[i].y;
	}
}

/*
 * Accumulate extrema of a single data source.
 */
static void
kdata_extrema_single(struct kplotdat *d, struct kplotctx *ctx)
{
	size_t	 	 i;
	double		 max;
	struct kpair	 kp;

	max = -DBL_MAX;
	d->sum = 0.0;
	memset(&kp, 0, sizeof(struct kpair));
	for (i = 0; i < d->datas[0]->pairsz; i++) {
		if ( ! kpair_vrfy(&d->datas[0]->pairs[i]))
			continue;
		kpair_set(d, i, &kp);
		if (KSMOOTH_CDF == d->smthtype)
			d->sum += d->datas[0]->pairs[i].y;
		if (KSMOOTH_PMF == d->smthtype) {
			d->sum += d->datas[0]->pairs[i].y;
			if (d->datas[0]->pairs[i].y > max)
				max = d->datas[0]->pairs[i].y;
		}
		if (kp.x < ctx->minv.x)
			ctx->minv.x = kp.x;
		if (kp.x > ctx->maxv.x)
			ctx->maxv.x = kp.x;
		switch (d->smthtype) {
		case (KSMOOTH_CDF):
		case (KSMOOTH_PMF):
			break;
		default:
			if (kp.y < ctx->minv.y)
				ctx->minv.y = kp.y;
			if (kp.y > ctx->maxv.y)
				ctx->maxv.y = kp.y;
			break;
		}
	}
	if (KSMOOTH_CDF == d->smthtype) {
		if (0.0 < ctx->minv.y)
			ctx->minv.y = 0.0;
		if (1.0 > ctx->maxv.y)
			ctx->maxv.y = 1.0;
	} else if (KSMOOTH_PMF == d->smthtype) {
		if (0.0 < ctx->minv.y)
			ctx->minv.y = 0.0;
		if (max / d->sum > ctx->maxv.y)
			ctx->maxv.y = max / d->sum;
	}
}


/*
 * Adjust a plot point to be within the graphing space.
 * The graphing space is the same for all data sources in the plot, so
 * we simply take the point and adjust it.
 * NOTE: this might fall outside of the drawable area.
 * That's ok: we'll discard it (if points) or clip it (lines).
 */
static inline void
kpoint_to_real(const struct kpair *data, struct kpair *real,
	const struct kpair *minv, const struct kpair *maxv,
	double w, double h)
{

	real->x = maxv->x == minv->x ? 0.0 :
		w * (data->x - minv->x) / (maxv->x - minv->x);
	real->y = maxv->y == minv->y ? h :
		h - h * (data->y - minv->y) / (maxv->y - minv->y);
}

/*
 * Verify that a given point is real (in terms of floating-point) and if
 * so, convert it to the plot space.
 */
static int
kplotctx_point_to_real(const struct kpair *data,
	struct kpair *real, const struct kplotctx *ctx)
{

	if ( ! kpair_vrfy(data))
		return(0);
	kpoint_to_real(data, real,
		&ctx->minv, &ctx->maxv, ctx->w, ctx->h);
	return(1);
}

/*
 * Draw a circle (arc) to the plot IFF it happens to fall within the
 * boundaries we set with the plot, otherwise do nothing.
 */
static void
kplot_arc(const struct kpair *kp,
	const struct kplotpoint *p, struct kplotctx *ctx)
{
	struct kpair	 pair;

	if (kp->x < ctx->minv.x || kp->x > ctx->maxv.x)
		return;
	if (kp->y < ctx->minv.y || kp->y > ctx->maxv.y)
		return;
	if (0 == kplotctx_point_to_real(kp, &pair, ctx))
		return;
	cairo_arc(ctx->cr, pair.x, pair.y, p->radius, 0, 2 * M_PI);
	cairo_stroke(ctx->cr);
}

static void
kplot_mark(const struct kpair *kp,
	const struct kplotpoint *p, struct kplotctx *ctx)
{
	struct kpair	 pair;

	if (kp->x < ctx->minv.x || kp->x > ctx->maxv.x)
		return;
	if (kp->y < ctx->minv.y || kp->y > ctx->maxv.y)
		return;
	if (0 == kplotctx_point_to_real(kp, &pair, ctx))
		return;

	cairo_move_to(ctx->cr, pair.x - p->radius, pair.y - p->radius);
	cairo_line_to(ctx->cr, pair.x + p->radius, pair.y + p->radius);
	cairo_move_to(ctx->cr, pair.x - p->radius, pair.y + p->radius);
	cairo_line_to(ctx->cr, pair.x + p->radius, pair.y - p->radius);
	cairo_stroke(ctx->cr);
}

/*
 * When drawing points, arrange the drawing space.
 * It's the responsibility of kplot_arc() to avoid points that would be
 * drawn outside of this range.
 * You must call cairo_restore(ctx->cr) to symmetrise.
 */
static void
ksubwin_points(struct kplotctx *ctx)
{

	cairo_save(ctx->cr);
	cairo_translate(ctx->cr, ctx->offs.x, ctx->offs.y);
}

/*
 * When drawing lines (or bars), create a subwindow large enough for
 * lines within the context dimensions.
 * Lines drawn outside of the subwindow will be clipped.
 * You must call cairo_restore(ctx->cr) to symmetrise.
 */
static void
ksubwin_lines(struct kplotctx *ctx, const struct kdatacfg *dat)
{
	double		 width;

	width = dat->line.sz / 2.0;
	cairo_save(ctx->cr);
	cairo_translate(ctx->cr,
		ctx->offs.x - width,
		ctx->offs.y - width);
	cairo_rectangle(ctx->cr, 0, 0,
		ctx->dims.x + width * 2,
		ctx->dims.y + width * 2);
	cairo_clip(ctx->cr);
	cairo_translate(ctx->cr, width, width);
}

static size_t
kplotctx_draw_yerrline_start(struct kplotctx *ctx,
	const struct kplotdat *d, size_t *end)
{
	size_t	 start;

	/* Overlap between both point sets. */
	*end = d->datas[0]->pairsz < d->datas[1]->pairsz ?
		d->datas[0]->pairsz : d->datas[1]->pairsz;

	/* Skip past bad points to get to initial. */
	for (start = 0; start < *end; start++)
		if (kpair_vrfy(&d->datas[0]->pairs[start]) &&
			kpair_vrfy(&d->datas[1]->pairs[start]))
			return(start);

	return(*end);
}

static void
kplotctx_draw_yerrline_basepoints(struct kplotctx *ctx,
	size_t start, size_t end, const struct kplotdat *d)
{
	size_t		 i;

	ksubwin_points(ctx);
	kplotctx_point_init(ctx, &d->cfgs[0].point);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		kplot_arc(&d->datas[0]->pairs[i],
			&d->cfgs[0].point, ctx);
	}
	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_yerrline_basemarks(struct kplotctx *ctx,
	size_t start, size_t end, const struct kplotdat *d)
{
	size_t		 i;

	ksubwin_points(ctx);
	kplotctx_point_init(ctx, &d->cfgs[0].point);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		kplot_mark(&d->datas[0]->pairs[i],
			&d->cfgs[0].point, ctx);
	}
	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_yerrline_pairbars(struct kplotctx *ctx,
	size_t start, size_t end, const struct kplotdat *d)
{
	size_t	 	 i;
	struct kpair	 bot, top, pair;
	int		 rc;

	ksubwin_lines(ctx, &d->cfgs[1]);
	kplotctx_line_init(ctx, &d->cfgs[1].line);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;

		bot.x = top.x = d->datas[0]->pairs[i].x;
		bot.y = d->datas[0]->pairs[i].y -
			 d->datas[1]->pairs[i].y;
		top.y = d->datas[0]->pairs[i].y +
			 d->datas[1]->pairs[i].y;

		rc = kplotctx_point_to_real(&bot, &pair, ctx);
		assert(0 != rc);
		cairo_move_to(ctx->cr, pair.x, pair.y);

		rc = kplotctx_point_to_real(&top, &pair, ctx);
		assert(0 != rc);
		cairo_line_to(ctx->cr, pair.x, pair.y);
	}
	cairo_stroke(ctx->cr);
	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_yerrline_pairpoints(struct kplotctx *ctx,
	size_t start, size_t end, const struct kplotdat *d)
{
	size_t	 	 i;
	struct kpair	 orig;

	ksubwin_points(ctx);
	kplotctx_point_init(ctx, &d->cfgs[1].point);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		orig.x = d->datas[0]->pairs[i].x;
		orig.y = d->datas[0]->pairs[i].y +
			 d->datas[1]->pairs[i].y;
		kplot_arc(&orig, &d->cfgs[1].point, ctx);
	}

	kplotctx_point_init(ctx, &d->cfgs[1].point);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		orig.x = d->datas[0]->pairs[i].x;
		orig.y = d->datas[0]->pairs[i].y -
			 d->datas[1]->pairs[i].y;
		kplot_arc(&orig, &d->cfgs[1].point, ctx);
	}

	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_yerrline_pairmarks(struct kplotctx *ctx,
	size_t start, size_t end, const struct kplotdat *d)
{
	size_t	 	 i;
	struct kpair	 orig;

	ksubwin_points(ctx);
	kplotctx_point_init(ctx, &d->cfgs[1].point);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		orig.x = d->datas[0]->pairs[i].x;
		orig.y = d->datas[0]->pairs[i].y +
			 d->datas[1]->pairs[i].y;
		kplot_mark(&orig, &d->cfgs[1].point, ctx);
	}

	kplotctx_point_init(ctx, &d->cfgs[1].point);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		orig.x = d->datas[0]->pairs[i].x;
		orig.y = d->datas[0]->pairs[i].y -
			 d->datas[1]->pairs[i].y;
		kplot_mark(&orig, &d->cfgs[1].point, ctx);
	}

	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_yerrline_baselines(struct kplotctx *ctx,
	size_t start, size_t end, const struct kplotdat *d)
{
	size_t		 i;
	struct kpair	 pair;
	int		 rc;

	assert(d->datasz > 1);
	ksubwin_lines(ctx, &d->cfgs[0]);
	kplotctx_line_init(ctx, &d->cfgs[0].line);
	rc = kplotctx_point_to_real
		(&d->datas[0]->pairs[start], &pair, ctx);
	assert(0 != rc);
	cairo_move_to(ctx->cr, pair.x, pair.y);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		rc = kplotctx_point_to_real
			(&d->datas[0]->pairs[i], &pair, ctx);
		assert(0 != rc);
		cairo_line_to(ctx->cr, pair.x, pair.y);
	}
	cairo_stroke(ctx->cr);
	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_yerrline_pairlines(struct kplotctx *ctx,
	size_t start, size_t end, const struct kplotdat *d)
{
	struct kpair	 orig, pair;
	size_t		 i;
	int		 rc;

	ksubwin_lines(ctx, &d->cfgs[1]);
	kplotctx_line_init(ctx, &d->cfgs[1].line);
	orig.x = d->datas[0]->pairs[start].x;
	orig.y = d->datas[0]->pairs[start].y +
		 d->datas[1]->pairs[start].y;
	rc = kplotctx_point_to_real(&orig, &pair, ctx);
	assert(0 != rc);
	cairo_move_to(ctx->cr, pair.x, pair.y);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		orig.x = d->datas[0]->pairs[i].x;
		orig.y = d->datas[0]->pairs[i].y +
			 d->datas[1]->pairs[i].y;
		rc = kplotctx_point_to_real(&orig, &pair, ctx);
		assert(0 != rc);
		cairo_line_to(ctx->cr, pair.x, pair.y);
	}
	cairo_stroke(ctx->cr);

	kplotctx_line_init(ctx, &d->cfgs[1].line);
	orig.x = d->datas[0]->pairs[start].x;
	orig.y = d->datas[0]->pairs[start].y -
		 d->datas[1]->pairs[start].y;
	kplotctx_point_to_real(&orig, &pair, ctx);
	cairo_move_to(ctx->cr, pair.x, pair.y);
	for (i = start; i < end; i++) {
		if ( ! (kpair_vrfy(&d->datas[0]->pairs[i]) &&
			kpair_vrfy(&d->datas[1]->pairs[i])))
			continue;
		orig.x = d->datas[0]->pairs[i].x;
		orig.y = d->datas[0]->pairs[i].y -
			 d->datas[1]->pairs[i].y;
		rc = kplotctx_point_to_real(&orig, &pair, ctx);
		assert(0 != rc);
		cairo_line_to(ctx->cr, pair.x, pair.y);
	}
	cairo_stroke(ctx->cr);
	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_lines(struct kplotctx *ctx, const struct kplotdat *d)
{
	size_t		 i;
	struct kpair	 kp, pair;
	int		 rc;

	ksubwin_lines(ctx, &d->cfgs[0]);
	memset(&kp, 0, sizeof(struct kpair));
	for (i = 0; i < d->datas[0]->pairsz; i++) {
		kpair_set(d, i, &kp);
		if (kplotctx_point_to_real(&kp, &pair, ctx))
			break;
	}

	if (i == d->datas[0]->pairsz)
		goto out;

	kplotctx_line_init(ctx, &d->cfgs[0].line);
	cairo_move_to(ctx->cr, pair.x, pair.y);
	memset(&kp, 0, sizeof(struct kpair));
	for ( ; i < d->datas[0]->pairsz; i++) {
		if ( ! kpair_vrfy(&d->datas[0]->pairs[i]))
			continue;
		kpair_set(d, i, &kp);
		rc = kplotctx_point_to_real(&kp, &pair, ctx);
		if ( ! rc)
			continue;
		cairo_line_to(ctx->cr, pair.x, pair.y);
	}
	cairo_stroke(ctx->cr);
out:
	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_points(struct kplotctx *ctx, const struct kplotdat *d)
{
	size_t		 i;
	struct kpair	 kp;

	ksubwin_points(ctx);
	memset(&kp, 0, sizeof(struct kpair));
	kplotctx_point_init(ctx, &d->cfgs[0].point);
	for (i = 0; i < d->datas[0]->pairsz; i++) {
		if ( ! kpair_vrfy(&d->datas[0]->pairs[i]))
			continue;
		kpair_set(d, i, &kp);
		kplot_arc(&kp, &d->cfgs[0].point, ctx);
	}
	cairo_restore(ctx->cr);
}

static void
kplotctx_draw_marks(struct kplotctx *ctx, const struct kplotdat *d)
{
	size_t		 i;
	struct kpair	 kp;

	ksubwin_points(ctx);
	memset(&kp, 0, sizeof(struct kpair));
	kplotctx_point_init(ctx, &d->cfgs[0].point);
	for (i = 0; i < d->datas[0]->pairsz; i++) {
		if ( ! kpair_vrfy(&d->datas[0]->pairs[i]))
			continue;
		kpair_set(d, i, &kp);
		kplot_mark(&kp, &d->cfgs[0].point, ctx);
	}
	cairo_restore(ctx->cr);
}

void
kplotfont_defaults(struct kplotfont *font)
{

	memset(font, 0, sizeof(struct kplotfont));

	/* Point 12 size serif font. */
	font->family = "serif";
	font->sz = 12.0;
	font->slant = CAIRO_FONT_SLANT_NORMAL;
	font->weight = CAIRO_FONT_WEIGHT_NORMAL;
}

void
kplotcfg_defaults(struct kplotcfg *cfg)
{

	memset(cfg, 0, sizeof(struct kplotcfg));

	/* Five left and bottom grey tic labels. */
	kplotfont_defaults(&cfg->ticlabelfont);
	cfg->ticlabel = TICLABEL_LEFT | TICLABEL_BOTTOM;
	cfg->xticlabelpad = cfg->yticlabelpad = 15.0;
	cfg->xinterval = cfg->yinterval = 1.0;

	/* A bit of margin. */
	cfg->margin = MARGIN_ALL;
	cfg->marginsz = 15.0;

	/* Innie tics, grey. */
	cfg->tic = TIC_LEFT_IN | TIC_BOTTOM_IN;
	cfg->ticline.len = 5.0;
	cfg->ticline.sz = 1.0;

	/* Grid line: dotted, grey. */
	cfg->grid = GRID_ALL;
	cfg->gridline.sz = 1.0;
	cfg->gridline.dashes[0] = 1.0;
	cfg->gridline.dashes[1] = 4.0;
	cfg->gridline.dashesz = 2;

	/* Border line: solid, grey. */
	cfg->border = BORDER_LEFT | BORDER_BOTTOM;
	cfg->borderline.sz = 1.0;

	/* Black axis labels. */
	kplotfont_defaults(&cfg->axislabelfont);
	cfg->xaxislabelpad = cfg->yaxislabelpad = 15.0;
}

void
kplot_draw(struct kplot *p, double w, double h, cairo_t *cr)
{
	size_t	 	 i, start, end;
	struct kplotctx	 ctx;
	struct kplotdat	*d;
	struct kplotccfg defs[7];

	memset(&ctx, 0, sizeof(struct kplotctx));

	ctx.w = w;
	ctx.h = h;
	ctx.cr = cr;
	ctx.minv.x = ctx.minv.y = DBL_MAX;
	ctx.maxv.x = ctx.maxv.y = -DBL_MAX;
	ctx.cfg = p->cfg;

	if (KPLOTCTYPE_DEFAULT == ctx.cfg.borderline.clr.type) {
		ctx.cfg.borderline.clr.type = KPLOTCTYPE_RGBA;
		ctx.cfg.borderline.clr.rgba[0] = 0.0;
		ctx.cfg.borderline.clr.rgba[1] = 0.0;
		ctx.cfg.borderline.clr.rgba[2] = 0.0;
		ctx.cfg.borderline.clr.rgba[3] = 1.0;
	}

	if (KPLOTCTYPE_DEFAULT == ctx.cfg.axislabelfont.clr.type) {
		ctx.cfg.axislabelfont.clr.type = KPLOTCTYPE_RGBA;
		ctx.cfg.axislabelfont.clr.rgba[0] = 0.0;
		ctx.cfg.axislabelfont.clr.rgba[1] = 0.0;
		ctx.cfg.axislabelfont.clr.rgba[2] = 0.0;
		ctx.cfg.axislabelfont.clr.rgba[3] = 1.0;
	}

	if (KPLOTCTYPE_DEFAULT == ctx.cfg.ticline.clr.type) {
		ctx.cfg.ticline.clr.type = KPLOTCTYPE_RGBA;
		ctx.cfg.ticline.clr.rgba[0] = 0.0;
		ctx.cfg.ticline.clr.rgba[1] = 0.0;
		ctx.cfg.ticline.clr.rgba[2] = 0.0;
		ctx.cfg.ticline.clr.rgba[3] = 1.0;
	}

	if (KPLOTCTYPE_DEFAULT == ctx.cfg.gridline.clr.type) {
		ctx.cfg.gridline.clr.type = KPLOTCTYPE_RGBA;
		ctx.cfg.gridline.clr.rgba[0] = 0.5;
		ctx.cfg.gridline.clr.rgba[1] = 0.5;
		ctx.cfg.gridline.clr.rgba[2] = 0.5;
		ctx.cfg.gridline.clr.rgba[3] = 1.0;
	}

	if (KPLOTCTYPE_DEFAULT == ctx.cfg.ticlabelfont.clr.type) {
		ctx.cfg.ticlabelfont.clr.type = KPLOTCTYPE_RGBA;
		ctx.cfg.ticlabelfont.clr.rgba[0] = 0.5;
		ctx.cfg.ticlabelfont.clr.rgba[1] = 0.5;
		ctx.cfg.ticlabelfont.clr.rgba[2] = 0.5;
		ctx.cfg.ticlabelfont.clr.rgba[3] = 1.0;
	}

	if (0 == ctx.cfg.clrsz) {
		ctx.cfg.clrs = defs;
		ctx.cfg.clrsz = 7;
		for (i = 0; i < ctx.cfg.clrsz; i++) {
			ctx.cfg.clrs[i].type = KPLOTCTYPE_RGBA;
			ctx.cfg.clrs[i].rgba[3] = 1.0;
		}
		ctx.cfg.clrs[0].rgba[0] = 0x94 / 255.0;
		ctx.cfg.clrs[0].rgba[1] = 0x04 / 255.0;
		ctx.cfg.clrs[0].rgba[2] = 0xd3 / 255.0;
		ctx.cfg.clrs[1].rgba[0] = 0x00 / 255.0;
		ctx.cfg.clrs[1].rgba[1] = 0x9e / 255.0;
		ctx.cfg.clrs[1].rgba[2] = 0x73 / 255.0;
		ctx.cfg.clrs[2].rgba[0] = 0x56 / 255.0;
		ctx.cfg.clrs[2].rgba[1] = 0xb4 / 255.0;
		ctx.cfg.clrs[2].rgba[2] = 0xe9 / 255.0;
		ctx.cfg.clrs[3].rgba[0] = 0xe6 / 255.0;
		ctx.cfg.clrs[3].rgba[1] = 0x9f / 255.0;
		ctx.cfg.clrs[3].rgba[2] = 0x00 / 255.0;
		ctx.cfg.clrs[4].rgba[0] = 0xf0 / 255.0;
		ctx.cfg.clrs[4].rgba[1] = 0xe4 / 255.0;
		ctx.cfg.clrs[4].rgba[2] = 0x42 / 255.0;
		ctx.cfg.clrs[5].rgba[0] = 0x00 / 255.0;
		ctx.cfg.clrs[5].rgba[1] = 0x72 / 255.0;
		ctx.cfg.clrs[5].rgba[2] = 0xb2 / 255.0;
		ctx.cfg.clrs[6].rgba[0] = 0xe5 / 255.0;
		ctx.cfg.clrs[6].rgba[1] = 0x1e / 255.0;
		ctx.cfg.clrs[6].rgba[2] = 0x10 / 255.0;
	}

	for (i = 0; i < p->datasz; i++) {
		d = &p->datas[i];
		switch (d->stype) {
		case (KPLOTS_YERRORBAR):
		case (KPLOTS_YERRORLINE):
			kdata_extrema_yerr(d, &ctx);
			break;
		case (KPLOTS_SINGLE):
			kdata_extrema_single(d, &ctx);
			break;
		}
	}

	if (EXTREMA_XMIN & ctx.cfg.extrema)
		ctx.minv.x = ctx.cfg.extrema_xmin;
	if (EXTREMA_YMIN & ctx.cfg.extrema)
		ctx.minv.y = ctx.cfg.extrema_ymin;
	if (EXTREMA_XMAX & ctx.cfg.extrema)
		ctx.maxv.x = ctx.cfg.extrema_xmax;
	if (EXTREMA_YMAX & ctx.cfg.extrema)
		ctx.maxv.y = ctx.cfg.extrema_ymax;

	if (ctx.minv.x > ctx.maxv.x)
		ctx.minv.x = ctx.maxv.x = 0.0;
	if (ctx.minv.y > ctx.maxv.y)
		ctx.minv.y = ctx.maxv.y = 0.0;

	kplotctx_margin_init(&ctx);
	kplotctx_label_init(&ctx);
	kplotctx_grid_init(&ctx);
	kplotctx_border_init(&ctx);
	kplotctx_tic_init(&ctx);

	ctx.h = ctx.dims.y;
	ctx.w = ctx.dims.x;

	for (i = 0; i < p->datasz; i++) {
		d = &p->datas[i];
		switch (d->stype) {
		case (KPLOTS_SINGLE):
			switch (d->types[0]) {
			case (KPLOT_POINTS):
				kplotctx_draw_points(&ctx, d);
				break;
			case (KPLOT_MARKS):
				kplotctx_draw_marks(&ctx, d);
				break;
			case (KPLOT_LINES):
				kplotctx_draw_lines(&ctx, d);
				break;
			case (KPLOT_LINESPOINTS):
				kplotctx_draw_points(&ctx, d);
				kplotctx_draw_lines(&ctx, d);
				break;
			case (KPLOT_LINESMARKS):
				kplotctx_draw_marks(&ctx, d);
				kplotctx_draw_lines(&ctx, d);
				break;
			default:
				abort();
				break;
			}
			break;
		case (KPLOTS_YERRORBAR):
		case (KPLOTS_YERRORLINE):
			start = kplotctx_draw_yerrline_start
				(&ctx, d, &end);
			if (start == end)
				break;
			assert(d->datasz > 1);
			switch (d->types[0]) {
			case (KPLOT_POINTS):
				kplotctx_draw_yerrline_basepoints
					(&ctx, start, end, d);
				break;
			case (KPLOT_MARKS):
				kplotctx_draw_yerrline_basemarks
					(&ctx, start, end, d);
				break;
			case (KPLOT_LINES):
				kplotctx_draw_yerrline_baselines
					(&ctx, start, end, d);
				break;
			case (KPLOT_LINESPOINTS):
				kplotctx_draw_yerrline_basepoints
					(&ctx, start, end, d);
				kplotctx_draw_yerrline_baselines
					(&ctx, start, end, d);
				break;
			case (KPLOT_LINESMARKS):
				kplotctx_draw_yerrline_basemarks
					(&ctx, start, end, d);
				kplotctx_draw_yerrline_baselines
					(&ctx, start, end, d);
				break;
			default:
				abort();
				break;
			}
			switch (p->datas[i].types[1]) {
			case (KPLOT_POINTS):
				kplotctx_draw_yerrline_pairpoints
					(&ctx, start, end, d);
				break;
			case (KPLOT_MARKS):
				kplotctx_draw_yerrline_pairmarks
					(&ctx, start, end, d);
				break;
			case (KPLOT_LINES):
				kplotctx_draw_yerrline_pairlines
					(&ctx, start, end, d);
				break;
			case (KPLOT_LINESPOINTS):
				kplotctx_draw_yerrline_pairpoints
					(&ctx, start, end, d);
				kplotctx_draw_yerrline_pairlines
					(&ctx, start, end, d);
				break;
			case (KPLOT_LINESMARKS):
				kplotctx_draw_yerrline_pairmarks
					(&ctx, start, end, d);
				kplotctx_draw_yerrline_pairlines
					(&ctx, start, end, d);
				break;
			default:
				abort();
				break;
			}
			if (KPLOTS_YERRORBAR == d->stype)
				kplotctx_draw_yerrline_pairbars
					(&ctx, start, end, d);
			break;
		default:
			break;
		}
	}
}

int
kplotcfg_default_palette(struct kplotccfg **pp, size_t *szp)
{
	size_t		 i;

	*szp = 7;
	if (NULL == (*pp = calloc(*szp, sizeof(struct kplotccfg))))
		return(0);

	for (i = 0; i < *szp; i++) {
		(*pp)[i].type = KPLOTCTYPE_RGBA;
		(*pp)[i].rgba[3] = 1.0;
	}

	(*pp)[0].rgba[0] = 0x94 / 255.0;
	(*pp)[0].rgba[1] = 0x04 / 255.0;
	(*pp)[0].rgba[2] = 0xd3 / 255.0;
	(*pp)[1].rgba[0] = 0x00 / 255.0;
	(*pp)[1].rgba[1] = 0x9e / 255.0;
	(*pp)[1].rgba[2] = 0x73 / 255.0;
	(*pp)[2].rgba[0] = 0x56 / 255.0;
	(*pp)[2].rgba[1] = 0xb4 / 255.0;
	(*pp)[2].rgba[2] = 0xe9 / 255.0;
	(*pp)[3].rgba[0] = 0xe6 / 255.0;
	(*pp)[3].rgba[1] = 0x9f / 255.0;
	(*pp)[3].rgba[2] = 0x00 / 255.0;
	(*pp)[4].rgba[0] = 0xf0 / 255.0;
	(*pp)[4].rgba[1] = 0xe4 / 255.0;
	(*pp)[4].rgba[2] = 0x42 / 255.0;
	(*pp)[5].rgba[0] = 0x00 / 255.0;
	(*pp)[5].rgba[1] = 0x72 / 255.0;
	(*pp)[5].rgba[2] = 0xb2 / 255.0;
	(*pp)[6].rgba[0] = 0xe5 / 255.0;
	(*pp)[6].rgba[1] = 0x1e / 255.0;
	(*pp)[6].rgba[2] = 0x10 / 255.0;

	return(1);
}
