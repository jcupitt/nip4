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

static void
kplotdat_free(struct kplotdat *p)
{
	size_t	 i;

	if (NULL == p)
		return;

	for (i = 0; i < p->datasz; i++) {
		kdata_destroy(p->datas[i]);
		if (KPLOTCTYPE_PATTERN == p->cfgs[i].line.clr.type)
			cairo_pattern_destroy(p->cfgs[i].line.clr.pattern);
		if (KPLOTCTYPE_PATTERN == p->cfgs[i].point.clr.type)
			cairo_pattern_destroy(p->cfgs[i].point.clr.pattern);
	}

	free(p->datas);
	free(p->cfgs);
	free(p->types);
}

struct kplot *
kplot_alloc(const struct kplotcfg *cfg)
{
	struct kplot	*p;
	size_t		 i;

	p = calloc(1, sizeof(struct kplot));

	if (NULL == p)
		return(NULL);

	if (NULL == cfg)
		kplotcfg_defaults(&p->cfg);
	else
		p->cfg = *cfg;

	/* Refernece all patterns. */

	if (KPLOTCTYPE_PATTERN == p->cfg.borderline.clr.type)
		cairo_pattern_reference
			(p->cfg.borderline.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.ticline.clr.type)
		cairo_pattern_reference
			(p->cfg.ticline.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.gridline.clr.type)
		cairo_pattern_reference
			(p->cfg.gridline.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.ticlabelfont.clr.type)
		cairo_pattern_reference
			(p->cfg.ticlabelfont.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.axislabelfont.clr.type)
		cairo_pattern_reference
			(p->cfg.axislabelfont.clr.pattern);

	if (0 == p->cfg.clrsz)
		return(p);

	/*
	 * If we pass an array of colour settings, then we want to
	 * duplicate the array instead of copying it wholesale, as the
	 * caller may free it in the meantime.
	 * In doing so, we need to reference the Cairo patterns.
	 */
	p->cfg.clrs = calloc(p->cfg.clrsz, sizeof(struct kplotccfg));

	if (NULL == p->cfg.clrs) {
		p->cfg.clrsz = 0;
		kplot_free(p);
		return(NULL);
	}

	memcpy(p->cfg.clrs, cfg->clrs,
		p->cfg.clrsz * sizeof(struct kplotccfg));

	for (i = 0; i < p->cfg.clrsz; i++)
		if (KPLOTCTYPE_PATTERN == p->cfg.clrs[i].type)
			cairo_pattern_reference(p->cfg.clrs[i].pattern);

	return(p);
}

struct kplotcfg *
kplot_get_plotcfg(struct kplot *p)
{

	return(&p->cfg);
}

static void
kplot_data_remove_all(struct kplot *p)
{
	size_t	 i;

	for (i = 0; i < p->datasz; i++)
		kplotdat_free(&p->datas[i]);

	free(p->datas);
	p->datas = NULL;
	p->datasz = 0;
}

void
kplot_free(struct kplot *p)
{
	size_t	 i;

	if (NULL == p)
		return;

	kplot_data_remove_all(p);

	if (KPLOTCTYPE_PATTERN == p->cfg.borderline.clr.type)
		cairo_pattern_destroy
			(p->cfg.borderline.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.ticline.clr.type)
		cairo_pattern_destroy
			(p->cfg.ticline.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.gridline.clr.type)
		cairo_pattern_destroy
			(p->cfg.gridline.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.ticlabelfont.clr.type)
		cairo_pattern_destroy
			(p->cfg.ticlabelfont.clr.pattern);
	if (KPLOTCTYPE_PATTERN == p->cfg.axislabelfont.clr.type)
		cairo_pattern_destroy
			(p->cfg.axislabelfont.clr.pattern);

	for (i = 0; i < p->cfg.clrsz; i++)
		if (KPLOTCTYPE_PATTERN == p->cfg.clrs[i].type)
			cairo_pattern_destroy(p->cfg.clrs[i].pattern);

	free(p->cfg.clrs);
	free(p->datas);
	free(p);
}

void
ksmthcfg_defaults(struct ksmthcfg *p)
{

	p->movsamples = 3;
}

int
kplot_detach(struct kplot *p, const struct kdata *d)
{
	size_t		 i, j;
	struct kplotdat	*dat;
	void		*pp;

	/*
	 * Search for the data plot.
	 * We look in all plot sources, so if this is just one of a
	 * multiplot, we'll still remove it.
	 */
	for (i = 0; i < p->datasz; i++) {
		dat = &p->datas[i];
		for (j = 0; j < dat->datasz; j++)
			if (dat->datas[j] == d)
				break;
		if (j < dat->datasz)
			break;
	}
	/* Not found... */
	if (i == p->datasz)
		return(0);

	/* Free the found data plot source. */
	kplotdat_free(&p->datas[i]);

	/*
	 * Move data above the copied region to replace the current
	 * region.
	 * This preserves the order of plot sets.
	 */
	memmove(&p->datas[i], &p->datas[i + 1],
		(p->datasz - i - 1) *
		sizeof(struct kplotdat));
	p->datasz--;
	pp = reallocarray(p->datas,
		p->datasz, sizeof(struct kplotdat));
	if (NULL == pp) {
		/* This really, really shouldn't happen. */
		return(0);
	}
	p->datas = pp;
	return(1);
}

static int
kplotdat_attach(struct kplot *p, size_t sz, struct kdata **d,
	const struct kdatacfg *const *cfg,
	const enum kplottype *types, enum kplotstype stype,
	enum ksmthtype smthtype, const struct ksmthcfg *smth)
{
	void		*pp;
	size_t		 i;
	struct kdatacfg	*dcfg;

	pp = reallocarray(p->datas,
		p->datasz + 1, sizeof(struct kplotdat));
	if (NULL == pp)
		return(0);
	p->datas = pp;

	p->datas[p->datasz].datas =
		calloc(sz, sizeof(struct kdata *));
	if (NULL == p->datas[p->datasz].datas)
		return(0);
	p->datas[p->datasz].cfgs =
		calloc(sz, sizeof(struct kdatacfg));
	if (NULL == p->datas[p->datasz].cfgs)
		return(0);
	p->datas[p->datasz].types =
		calloc(sz, sizeof(enum kplottype));
	if (NULL == p->datas[p->datasz].types)
		return(0);

	for (i = 0; i < sz; i++) {
		p->datas[p->datasz].datas[i] = d[i];
		p->datas[p->datasz].types[i] = types[i];
		dcfg = &p->datas[p->datasz].cfgs[i];
		if (NULL == cfg || NULL == cfg[i])
			kdatacfg_defaults(dcfg);
		else
			*dcfg = *cfg[i];
		kplotccfg_init_palette
			(&dcfg->point.clr, p->datasz);
		kplotccfg_init_palette
			(&dcfg->line.clr, p->datasz);
		d[i]->refs++;
	}

	p->datas[p->datasz].smthtype = smthtype;
	if (NULL != smth)  {
		p->datas[p->datasz].smth = *smth;
		/* Make sure we're odd around the sample. */
		if (0 == (2 % p->datas[p->datasz].smth.movsamples))
			p->datas[p->datasz].smth.movsamples++;
	} else
		ksmthcfg_defaults(&p->datas[p->datasz].smth);
	p->datas[p->datasz].datasz = sz;
	p->datas[p->datasz].stype = stype;
	p->datasz++;
	return(1);
}

int
kplot_get_datacfg(struct kplot *p, size_t pos,
	struct kdatacfg **datas, size_t *datasz)
{

	*datas = NULL;
	*datasz = 0;

	if (pos >= p->datasz)
		return(0);

	*datas = p->datas[pos].cfgs;
	*datasz = p->datas[pos].datasz;
	return(1);
}

int
kplot_attach_smooth(struct kplot *p, struct kdata *d,
	enum kplottype t, const struct kdatacfg *cfg,
	enum ksmthtype smthtype, const struct ksmthcfg *smth)
{

	return(kplotdat_attach(p, 1, &d, &cfg,
		&t, KPLOTS_SINGLE, smthtype, smth));
}

int
kplot_attach_data(struct kplot *p, struct kdata *d,
	enum kplottype t, const struct kdatacfg *cfg)
{

	return(kplotdat_attach(p, 1, &d, &cfg,
		&t, KPLOTS_SINGLE, KSMOOTH_NONE, NULL));
}

int
kplot_attach_datas(struct kplot *p, size_t sz,
	struct kdata **d, const enum kplottype *t,
	const struct kdatacfg *const *cfg, enum kplotstype st)
{

	if (sz < 2)
		return(0);
	return(kplotdat_attach(p, sz, d,
		cfg, t, st, KSMOOTH_NONE, NULL));
}
