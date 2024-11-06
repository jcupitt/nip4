/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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

static int
kdata_stddev_set(struct kdata *d, size_t pos, double x, double y)
{
	double	 delta, delta_n, term1, newy;
	void	*p;
	size_t	 n1;

	assert(KDATA_STDDEV == d->type);

	if (pos >= d->pairsz) {
		/*
		 * A note on this.
		 * Our only growable data source is the vector, which
		 * can only grow one at a time.
		 * Thus, if we attach to a vector, we'll never exceed
		 * this.
		 * If we have non-monotonically increasing data source
		 * sizes, this will need to be addressed.
		 * FIXME: this is very inefficient!
		 */
		assert(pos == d->pairsz);
		d->pairsz = pos + 1;
		p = reallocarray(d->pairs,
			d->pairsz, sizeof(struct kpair));
		if (NULL == p)
			return(0);
		d->pairs = p;
		p = reallocarray(d->d.stddev.ns,
			d->pairsz, sizeof(size_t));
		if (NULL == p)
			return(0);
		d->d.stddev.ns = p;
		p = reallocarray(d->d.stddev.m2s,
			d->pairsz, sizeof(double));
		if (NULL == p)
			return(0);
		d->d.stddev.m2s = p;
		p = reallocarray(d->d.stddev.m1s,
			d->pairsz, sizeof(double));
		if (NULL == p)
			return(0);
		d->d.stddev.m1s = p;
	}

	n1 = d->d.stddev.ns[pos]++;
        delta = y - d->d.stddev.m1s[pos];
        delta_n = delta / (double)d->d.stddev.ns[pos];
	term1 = delta * delta_n * (double)n1;

	d->d.stddev.m1s[pos] += delta_n;
	d->d.stddev.m2s[pos] += term1;
	if (d->d.stddev.ns[pos] < 2)
		newy = 0.0;
	else
		newy = sqrt(d->d.stddev.m2s[pos] /
			 ((double)d->d.stddev.ns[pos] - 1.0));

	return(kdata_set(d, pos, x, newy));
}

struct kdata *
kdata_stddev_alloc(struct kdata *dep)
{
	struct kdata	*d;
	size_t		 i;

	if (NULL == (d = calloc(1, sizeof(struct kdata))))
		return(NULL);

	d->refs = 1;
	d->type = KDATA_STDDEV;
	if (NULL == dep)
		return(d);

	d->pairsz = dep->pairsz;
	d->pairs = calloc(d->pairsz, sizeof(struct kpair));
	d->d.stddev.ns = calloc(d->pairsz, sizeof(size_t));
	d->d.stddev.m1s = calloc(d->pairsz, sizeof(double));
	d->d.stddev.m2s = calloc(d->pairsz, sizeof(double));
	if (NULL == d->pairs ||
		 NULL == d->d.stddev.ns ||
		 NULL == d->d.stddev.m1s ||
		 NULL == d->d.stddev.m2s) {
		free(d->pairs);
		free(d->d.stddev.ns);
		free(d->d.stddev.m1s);
		free(d->d.stddev.m2s);
		free(d);
		return(NULL);
	}
	kdata_dep_add(d, dep, kdata_stddev_set);

	for (i = 0; i < dep->pairsz; i++)
		d->pairs[i].x = dep->pairs[i].x;

	return(d);
}

int
kdata_stddev_attach(struct kdata *d, struct kdata *dep)
{
	void	*p;
	size_t	 i;

	if (KDATA_STDDEV != d->type)
		return(0);
	if (NULL == dep)
		return(1);

	if (d->pairsz < dep->pairsz) {
		p = reallocarray(d->pairs,
			dep->pairsz, sizeof(struct kpair));
		if (NULL == p)
			return(0);
		d->pairs = p;
		/* FIXME: don't loop, just do the math. */
		for (i = d->pairsz; i < dep->pairsz; i++)
			memset(&d->pairs[i], 0, sizeof(struct kpair));

		p = reallocarray(d->d.stddev.ns,
			dep->pairsz, sizeof(size_t));
		if (NULL == p)
			return(0);
		d->d.stddev.ns = p;
		for (i = d->pairsz; i < dep->pairsz; i++)
			d->d.stddev.ns[i] = 0;

		p = reallocarray(d->d.stddev.m1s,
			dep->pairsz, sizeof(double));
		if (NULL == p)
			return(0);
		d->d.stddev.m1s = p;
		for (i = d->pairsz; i < dep->pairsz; i++)
			d->d.stddev.m1s[i] = 0.0;

		p = reallocarray(d->d.stddev.m2s,
			dep->pairsz, sizeof(double));
		if (NULL == p)
			return(0);
		d->d.stddev.m2s = p;
		for (i = d->pairsz; i < dep->pairsz; i++)
			d->d.stddev.m2s[i] = 0.0;

		d->pairsz = dep->pairsz;
		for (i = 0; i < dep->pairsz; i++)
			d->pairs[i].x = dep->pairs[i].x;
	}

	kdata_dep_add(d, dep, kdata_stddev_set);
	return(1);
}
