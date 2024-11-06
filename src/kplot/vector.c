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
kdata_vector_alloc(size_t step)
{
	struct kdata	*d;

	if (NULL == (d = calloc(1, sizeof(struct kdata))))
		return(NULL);

	d->refs = 1;
	d->type = KDATA_VECTOR;
	d->d.vector.stepsz = step;
	return(d);
}

int
kdata_vector_append(struct kdata *d, double x, double y)
{
	void	*p;

	if (KDATA_VECTOR != d->type)
		return(0);

	if (d->pairsz + 1 >= d->d.vector.pairbufsz) {
		while (d->pairsz + 1 >= d->d.vector.pairbufsz)
			d->d.vector.pairbufsz += d->d.vector.stepsz;
		assert(d->d.vector.pairbufsz > d->pairsz + 1);
		p = reallocarray(d->pairs,
			d->d.vector.pairbufsz, sizeof(struct kpair));
		if (NULL == p)
			return(0);
		d->pairs = p;
	}

	d->pairsz++;
	return(kdata_set(d, d->pairsz - 1, x, y));
}

int
kdata_vector_set(struct kdata *d, size_t v, double x, double y)
{

	if (KDATA_VECTOR != d->type || v >= d->pairsz)
		return(0);
	return(kdata_set(d, v, x, y));
}
