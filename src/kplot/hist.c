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

struct kdata *
kdata_hist_alloc(double rmin, double rmax, size_t bins)
{
	struct kdata	*d;
	size_t		 i;

	assert(rmax > rmin);

	if (NULL == (d = calloc(1, sizeof(struct kdata))))
		return(NULL);

	d->refs = 1;
	d->pairsz = bins;
	d->pairs = calloc(d->pairsz, sizeof(struct kpair));
	if (NULL == d->pairs) {
		free(d);
		return(NULL);
	}

	for (i = 0; i < bins; i++)
		d->pairs[i].x = rmin +
			i / (double)bins * (rmax - rmin);

	d->type = KDATA_HIST;
	d->d.hist.rmin = rmin;
	d->d.hist.rmax = rmax;
	return(d);
}

static ssize_t
kdata_hist_checkrange(const struct kdata *d, double v)
{
	double	 frac;
	ssize_t	 bucket;

	if (KDATA_HIST != d->type)
		return(-1);
	else if (v < d->d.hist.rmin)
		return(-1);
	else if (v >= d->d.hist.rmax)
		return(-1);

	frac = (v - d->d.hist.rmin) /
		(d->d.hist.rmax - d->d.hist.rmin);
	assert(frac >= 0.0 && frac < 1.0);
	bucket = floor((double)d->pairsz * frac);

	if ((size_t)bucket == d->pairsz - 1) {
		assert(d->pairs[bucket].x <= v);
	} else {
		assert(d->pairs[bucket].x <= v);
		assert(d->pairs[bucket + 1].x >= v);
	}

	return(bucket);
}

int
kdata_hist_add(struct kdata *d, double v, double val)
{
	ssize_t	 bucket;
	double	 x, y;

	if ((bucket = kdata_hist_checkrange(d, v)) < 0)
		return(0);
	x = d->pairs[bucket].x;
	y = d->pairs[bucket].y + val;
	return(kdata_set(d, bucket, x, y));
}

int
kdata_hist_set(struct kdata *d, double v, double y)
{
	ssize_t	 bucket;
	double 	 x;

	if ((bucket = kdata_hist_checkrange(d, v)) < 0)
		return(0);
	x = d->pairs[bucket].x;
	return(kdata_set(d, bucket, x, y));
}
