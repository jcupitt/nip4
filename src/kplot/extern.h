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
#ifndef EXTERN_H
#define EXTERN_H

struct	kdatahist {
	double		 rmin; /* minimum inclusive */
	double		 rmax; /* maximum non-inclusive */
};

struct	kdatabucket {
	size_t		 rmin; /* minimum inclusive */
	size_t		 rmax; /* maximum non-inclusive */
};

struct	kdatamean {
	size_t		*ns; /* number of bucket modifications */
};

struct	kdatastddev {
	size_t		*ns; /* number of bucket modifications */
	double		*m1s; /* incremental mean */
	double		*m2s; /* incremental variance parameter */
};

struct	kdatavector {
	size_t		 stepsz; /* vector increase slush size */
	size_t		 pairbufsz; /* allocated buffer size */
};

enum	kdatatype {
	KDATA_ARRAY,
	KDATA_BUCKET,
	KDATA_BUFFER,
	KDATA_HIST,
	KDATA_MEAN,
	KDATA_STDDEV,
	KDATA_VECTOR
};

struct	kdata;

typedef	int (*ksetfunc)(struct kdata *, size_t, double, double);

/*
 * A dependant tacks on to a data source and is notified (by way of
 * "func") whenever the value of a bucket has changed.
 */
struct	kdep {
	struct kdata	 *dep;
	ksetfunc	  func;
};

/*
 * A data source can either be "real" (in the sense of being modified by
 * the calling code) or a "dependant" (in the sense of being updated
 * from another data source).
 */
struct	kdata {
	struct kpair	*pairs; /* data pairs */
	size_t		 pairsz; /* number of pairs */
	size_t		 refs; /* >0 references to data */
	struct kdep	*deps; /* dependants */
	size_t		 depsz; /* number of dependants */
	enum kdatatype	 type;
	union {
		struct kdatahist	hist;
		struct kdatavector	vector;
		struct kdatabucket	bucket;
		struct kdatamean	mean;
		struct kdatastddev	stddev;
	} d;
};

struct	kplotdat {
	struct kdata	**datas; /* referenced data */
	size_t		  datasz; /* number of data sets */
	struct kdatacfg	 *cfgs; /* plot configurations */
	enum kplottype	 *types; /* plot types */
	enum kplotstype	  stype; /* multiplot type */
	enum ksmthtype	  smthtype; /* smoothing type */
	struct ksmthcfg	  smth; /* smooth configuration */
	double		  sum; /* used for KSMOOTH_CDF */
};

struct	kplot {
	struct kplotdat	*datas; /* data sets per plot */
	size_t		 datasz; /* number of data sets */
	struct kplotcfg	 cfg; /* configuration */
};

struct	kplotctx {
	cairo_t		*cr; /* cairo context */
	double		 h; /* height of context */
	double		 w; /* width of context */
	struct kpair	 minv; /* minimum data point values */
	struct kpair	 maxv; /* maximum data point values */
	struct kplotcfg	 cfg;

	/*
	 * When computing the plot context, we need to account for a
	 * margin, labels, and boundary.
	 * To do this, we use these "soft" offset and dimensions.
	 * Once we've accounted for the above, we'll use this to
	 * translate and resize the Cairo context for graphing.
	 */
	struct kpair	 offs;
	struct kpair	 dims;
};

__BEGIN_DECLS

int	 kdata_dep_add(struct kdata *, struct kdata *, ksetfunc);
int	 kdata_dep_run(struct kdata *, size_t);
int	 kdata_set(struct kdata *, size_t, double, double);

void	 kplotctx_border_init(struct kplotctx *);
void	 kplotctx_grid_init(struct kplotctx *);
void	 kplotctx_margin_init(struct kplotctx *);
void	 kplotctx_tic_init(struct kplotctx *);
void	 kplotctx_label_init(struct kplotctx *);

double	 kplotctx_line_fix(const struct kplotctx *, double, double);

void	 kplotctx_font_init(struct kplotctx *, struct kplotfont *);
void	 kplotctx_line_init(struct kplotctx *, struct kplotline *);
void	 kplotctx_point_init(struct kplotctx *, struct kplotpoint *);
void	 kplotctx_ticln_init(struct kplotctx *, struct kplotticln *);

void	 kplotccfg_init_palette(struct kplotccfg *, size_t);

__END_DECLS

#endif
