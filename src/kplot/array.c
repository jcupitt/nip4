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
kdata_array_alloc(const struct kpair *np, size_t npsz)
{
	struct kdata	*d;
	size_t		 i;

	if (NULL == (d = calloc(1, sizeof(struct kdata))))
		return(NULL);

	d->pairsz = npsz;
	d->pairs = calloc(d->pairsz, sizeof(struct kpair));
	if (NULL == d->pairs) {
		free(d);
		return(NULL);
	}

	if (NULL == np)
		for (i = 0; i < d->pairsz; i++)
			d->pairs[i].x = i;
	else
		memcpy(d->pairs, np, d->pairsz * sizeof(struct kpair));

	d->refs = 1;
	d->type = KDATA_ARRAY;
	return(d);
}

int
kdata_array_fill_ysizes(struct kdata *d, const size_t *v)
{
	size_t		i;
	int		rc = 1;

	if (KDATA_ARRAY != d->type)
		return(0);

	if (d->depsz)
		for (i = 0; 0 != rc && i < d->pairsz; i++)
			rc = kdata_set(d, i, d->pairs[i].x, v[i]);
	else
		for (i = 0; i < d->pairsz; i++)
			d->pairs[i].y = v[i];

	return(rc);
}

int
kdata_array_fill_ydoubles(struct kdata *d, const double *v)
{
	size_t		i;
	int		rc = 1;

	if (KDATA_ARRAY != d->type)
		return(0);

	if (d->depsz)
		for (i = 0; 0 != rc && i < d->pairsz; i++)
			rc = kdata_set(d, i, d->pairs[i].x, v[i]);
	else
		for (i = 0; i < d->pairsz; i++)
			d->pairs[i].y = v[i];

	return(rc);
}

int
kdata_array_fill(struct kdata *d, void *arg,
	void (*fp)(size_t, struct kpair *, void *))
{
	size_t		i;
	int		rc = 1;
	struct kpair	kp;

	if (KDATA_ARRAY != d->type)
		return(0);

	/* Act directly on the data if not having deps. */
	if (d->depsz)
		for (i = 0; 0 != rc && i < d->pairsz; i++) {
			(*fp)(i, &kp, arg);
			rc = kdata_set(d, i, kp.x, kp.y);
		}
	else
		for (i = 0; i < d->pairsz; i++)
			(*fp)(i, &d->pairs[i], arg);

	return(rc);
}

static int
kdata_array_checkrange(const struct kdata *d, size_t v)
{

	return(KDATA_ARRAY == d->type && v < d->pairsz);
}

int
kdata_array_add(struct kdata *d, size_t v, double val)
{
	double	 x, y;

	if ( ! kdata_array_checkrange(d, v))
		return(0);
	x = d->pairs[v].x;
	y = d->pairs[v].y + val;
	return(kdata_set(d, v, x, y));
}

int
kdata_array_set(struct kdata *d, size_t v, double x, double y)
{

	if ( ! kdata_array_checkrange(d, v))
		return(0);
	return(kdata_set(d, v, x, y));
}
