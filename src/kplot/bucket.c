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
kdata_bucket_alloc(size_t rmin, size_t rmax)
{
	struct kdata	*d;
	size_t		 i;

	if (NULL == (d = calloc(1, sizeof(struct kdata))))
		return(NULL);

	d->refs = 1;
	d->pairsz = rmax - rmin;
	d->pairs = calloc(d->pairsz, sizeof(struct kpair));
	if (NULL == d->pairs) {
		free(d);
		return(NULL);
	}

	for (i = 0; i < d->pairsz; i++)
		d->pairs[i].x = rmin + i;

	d->type = KDATA_BUCKET;
	d->d.bucket.rmin = rmin;
	d->d.bucket.rmax = rmax;
	return(d);
}

static int
kdata_bucket_checkrange(const struct kdata *d, size_t v)
{

	return(KDATA_BUCKET == d->type &&
		v >= d->d.bucket.rmin && v < d->d.bucket.rmax);
}

int
kdata_bucket_set(struct kdata *d, size_t v, double x, double y)
{

	if ( ! kdata_bucket_checkrange(d, v))
		return(0);
	return(kdata_set(d, v - d->d.bucket.rmin, x, y));
}

int
kdata_bucket_add(struct kdata *d, size_t v, double val)
{
	double 	 x, y;

	if ( ! kdata_bucket_checkrange(d, v))
		return(0);
	x = d->pairs[v - d->d.bucket.rmin].x;
	y = d->pairs[v - d->d.bucket.rmin].y + val;
	return(kdata_set(d, v - d->d.bucket.rmin, x, y));
}
