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

struct kdata *
kdata_buffer_alloc(size_t hint)
{
	struct kdata	*d;

	if (NULL == (d = calloc(1, sizeof(struct kdata))))
		return(NULL);

	d->pairsz = hint;
	d->pairs = calloc(d->pairsz, sizeof(struct kpair));
	if (NULL == d->pairs) {
		free(d);
		return(NULL);
	}

	d->refs = 1;
	d->type = KDATA_BUFFER;
	return(d);
}

int
kdata_buffer_copy(struct kdata *dst, const struct kdata *src)
{
	void	*p;
	size_t	 i;
	int	 rc = 1;

	if (KDATA_BUFFER != dst->type)
		return(0);

	/*
	 * FIXME: use a pairbufsz-type of construct.
	 * We're not tied to any particular buffer size, so this should
	 * grow and shrink efficiently.
	 * Obviously, the current method is not efficient.
	 */
	if (src->pairsz > dst->pairsz) {
		dst->pairsz = src->pairsz;
		p = reallocarray(dst->pairs,
			dst->pairsz, sizeof(struct kpair));
		if (NULL == p)
			return(0);
		dst->pairs = p;
	}
	dst->pairsz = src->pairsz;

	if (dst->depsz)
		for (i = 0; 0 != rc && i < dst->pairsz; i++)
			rc = kdata_set(dst, i,
				src->pairs[i].x,
				src->pairs[i].y);
	else
		memcpy(dst->pairs, src->pairs,
			dst->pairsz * sizeof(struct kpair));

	return(rc);
}
