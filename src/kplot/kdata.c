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

void
kdata_destroy(struct kdata *d)
{
	size_t	 i;

	if (NULL == d)
		return;

	assert(d->refs > 0);
	if (--d->refs > 0)
		return;

	switch (d->type) {
	case (KDATA_MEAN):
		free(d->d.mean.ns);
		break;
	case (KDATA_STDDEV):
		free(d->d.stddev.ns);
		free(d->d.stddev.m1s);
		free(d->d.stddev.m2s);
		break;
	default:
		break;
	}

	/* Destroy dependeants along with ourselves. */
	for (i = 0; i < d->depsz; i++)
		kdata_destroy(d->deps[i].dep);

	free(d->deps);
	free(d->pairs);
	free(d);
}

void
kdatacfg_defaults(struct kdatacfg *cfg)
{

	memset(cfg, 0, sizeof(struct kdatacfg));
	cfg->point.radius = 3.0;
	cfg->point.sz = 2.0;
	cfg->point.clr.type = KPLOTCTYPE_DEFAULT;
	cfg->line.sz = 2.0;
	cfg->line.join = CAIRO_LINE_JOIN_ROUND;
	cfg->line.clr.type = KPLOTCTYPE_DEFAULT;
}

/*
 * We've modified a value at (pair) position "pos".
 * Pass this through to the underlying functional sources, if any.
 */
int
kdata_dep_run(struct kdata *data, size_t pos)
{
	size_t	 i;
	int	 rc;
	double	 x, y;

	x = data->pairs[pos].x;
	y = data->pairs[pos].y;

	for (rc = 1, i = 0; 0 != rc && i < data->depsz; i++)
		rc = data->deps[i].func
			(data->deps[i].dep, pos, x, y);

	return(rc);
}

/*
 * Add a functional kdata source "data" (e.g., stddev) to another kdata
 * source "dep" as a dependent.
 * All a source's dependents are updated with each modification of the
 * source's internal pair values.
 */
int
kdata_dep_add(struct kdata *data, struct kdata *dep, ksetfunc fp)
{
	void	*p;

	p = reallocarray(dep->deps,
		dep->depsz + 1, sizeof(struct kdep));
	if (NULL == p)
		return(0);
	dep->deps = p;
	dep->deps[dep->depsz].dep = data;
	dep->deps[dep->depsz].func = fp;
	dep->depsz++;

	/* While the parent exists, we must exist. */
	data->refs++;
	return(1);
}

double
kdata_pmfvar(const struct kdata *data)
{
	double	 ysum, mean, var;
	size_t	 i;

	if (0 == data->pairsz)
		return(0.0);

	for (ysum = 0.0, i = 0; i < data->pairsz; i++)
		ysum += data->pairs[i].y;

	if (ysum == 0.0)
		return(0.0);

	for (mean = 0.0, i = 0; i < data->pairsz; i++)
		mean += data->pairs[i].y / ysum * data->pairs[i].x;

	for (var = 0.0, i = 0; i < data->pairsz; i++)
		var += data->pairs[i].y / ysum *
			(data->pairs[i].x - mean) *
			(data->pairs[i].x - mean);

	return(var);
}

double
kdata_pmfstddev(const struct kdata *data)
{

	return(sqrt(kdata_pmfvar(data)));
}

double
kdata_pmfmean(const struct kdata *data)
{
	double	 ysum, sum;
	size_t	 i;

	if (0 == data->pairsz)
		return(0.0);

	for (ysum = 0.0, i = 0; i < data->pairsz; i++)
		ysum += data->pairs[i].y;

	if (ysum == 0.0)
		return(0.0);

	for (sum = 0.0, i = 0; i < data->pairsz; i++)
		sum += data->pairs[i].y / ysum * data->pairs[i].x;

	return(sum);
}

double
kdata_xmean(const struct kdata *data)
{
	double	 sum;
	size_t	 i;

	if (0 == data->pairsz)
		return(0.0);
	for (sum = 0.0, i = 0; i < data->pairsz; i++)
		sum += data->pairs[i].x;
	return(sum / (double)data->pairsz);
}

double
kdata_ymean(const struct kdata *data)
{
	double	 sum;
	size_t	 i;

	if (0 == data->pairsz)
		return(0.0);
	for (sum = 0.0, i = 0; i < data->pairsz; i++)
		sum += data->pairs[i].y;
	return(sum / (double)data->pairsz);
}

double
kdata_xstddev(const struct kdata *data)
{
	double	 sum, mean;
	size_t	 i;

	if (0 == data->pairsz)
		return(0.0);
	mean = kdata_xmean(data);
	for (sum = 0.0, i = 0; i < data->pairsz; i++)
		sum += (data->pairs[i].x - mean) *
		       (data->pairs[i].x - mean);
	return(sqrt(sum / (double)data->pairsz));
}

double
kdata_ystddev(const struct kdata *data)
{
	double	 sum, mean;
	size_t	 i;

	if (0 == data->pairsz)
		return(0.0);
	mean = kdata_xmean(data);
	for (sum = 0.0, i = 0; i < data->pairsz; i++)
		sum += (data->pairs[i].y - mean) *
		       (data->pairs[i].y - mean);
	return(sqrt(sum / (double)data->pairsz));
}

ssize_t
kdata_xmax(const struct kdata *d, struct kpair *kp)
{
	size_t	 	i, max;
	struct kpair	pair;

	if (0 == d->pairsz)
		return(-1);

	max = 0;
	pair = d->pairs[max];
	for (i = 1; i < d->pairsz; i++)
		if (d->pairs[i].x > pair.x) {
			pair = d->pairs[i];
			max = i;
		}
	if (NULL != kp)
		*kp = pair;
	return(max);
}

ssize_t
kdata_xmin(const struct kdata *d, struct kpair *kp)
{
	size_t	 	i, min;
	struct kpair	pair;

	if (0 == d->pairsz)
		return(-1);

	min = 0;
	pair = d->pairs[min];
	for (i = 1; i < d->pairsz; i++)
		if (d->pairs[i].x < pair.x) {
			pair = d->pairs[i];
			min = i;
		}
	if (NULL != kp)
		*kp = pair;
	return(min);
}

ssize_t
kdata_ymax(const struct kdata *d, struct kpair *kp)
{
	size_t	 	i, max;
	struct kpair	pair;

	if (0 == d->pairsz)
		return(-1);

	max = 0;
	pair = d->pairs[max];
	for (i = 1; i < d->pairsz; i++)
		if (d->pairs[i].y > pair.y) {
			pair = d->pairs[i];
			max = i;
		}
	if (NULL != kp)
		*kp = pair;
	return(max);
}

ssize_t
kdata_ymin(const struct kdata *d, struct kpair *kp)
{
	size_t	 	i, min;
	struct kpair	pair;

	if (0 == d->pairsz)
		return(-1);

	min = 0;
	pair = d->pairs[min];
	for (i = 1; i < d->pairsz; i++)
		if (d->pairs[i].y < pair.y) {
			pair = d->pairs[i];
			min = i;
		}
	if (NULL != kp)
		*kp = pair;
	return(min);
}

int
kdata_get(const struct kdata *d, size_t pos, struct kpair *kp)
{

	if (pos >= d->pairsz)
		return(0);
	*kp = d->pairs[pos];
	return(1);
}

int
kdata_set(struct kdata *d, size_t pos, double x, double y)
{

	if (pos >= d->pairsz)
		return(0);
	d->pairs[pos].x = x;
	d->pairs[pos].y = y;
	return(d->depsz ? kdata_dep_run(d, pos) : 1);
}
