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
#ifndef KPLOT_H
#define KPLOT_H

#include <assert.h>
#include <cairo.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <string.h>

// not in c11, so we have our own
void *reallocarray(void *ptr, size_t nmemb, size_t size);
#define M_PI   3.14159265358979323846   /* pi */
#define M_PI_2   1.57079632679489661923 /* pi/2 */

struct 	kpair {
	double	 x;
	double	 y;
};

enum	kplottype {
	KPLOT_POINTS,
	KPLOT_MARKS,
	KPLOT_LINES,
	KPLOT_LINESPOINTS,
	KPLOT_LINESMARKS
};

enum	ksmthtype {
	KSMOOTH_NONE,
	KSMOOTH_MOVAVG,
	KSMOOTH_CDF,
	KSMOOTH_PMF
};

enum	kplotstype {
	KPLOTS_SINGLE,
	KPLOTS_YERRORLINE,
	KPLOTS_YERRORBAR
};

enum	kplotctype {
	KPLOTCTYPE_DEFAULT = 0,
	KPLOTCTYPE_PALETTE,
	KPLOTCTYPE_PATTERN,
	KPLOTCTYPE_RGBA
};

struct	kplotccfg {
	enum kplotctype	 type;
	size_t		 palette;
	cairo_pattern_t	*pattern;
	double		 rgba[4];
};

struct 	kplotfont {
	cairo_font_slant_t   slant;
	cairo_font_weight_t  weight;
	const char	    *family;
	double		     sz;
	struct kplotccfg     clr;
};

struct	kplotticln {
	double		  sz;
	double		  len;
#define	KPLOT_DASH_MAX	  8
	double	  	  dashes[KPLOT_DASH_MAX];
	size_t		  dashesz;
	double	 	  dashoff;
	struct kplotccfg  clr;
};

struct	kplotpoint {
	double		  sz;
	double		  radius;
	double	  	  dashes[KPLOT_DASH_MAX];
	size_t		  dashesz;
	double	 	  dashoff;
	struct kplotccfg  clr;
};

struct	kplotline {
	double		  sz;
	double	  	  dashes[KPLOT_DASH_MAX];
	size_t		  dashesz;
	double	 	  dashoff;
	cairo_line_join_t join;
	struct kplotccfg  clr;
};

struct	ksmthcfg {
	size_t		  movsamples;
};

struct	kdatacfg {
	struct kplotline  line;
	struct kplotpoint point;
};

struct	kplotcfg {
	struct kplotccfg *clrs;
	size_t		  clrsz;
	double		  marginsz;
#define	MARGIN_LEFT	  0x01
#define	MARGIN_RIGHT	  0x02
#define	MARGIN_TOP	  0x04
#define	MARGIN_BOTTOM	  0x08
#define	MARGIN_ALL	  0xf
	unsigned int	  margin;
	struct kplotline  borderline;
	double		  bordersz;
#define	BORDER_LEFT	  0x01
#define	BORDER_RIGHT	  0x02
#define	BORDER_TOP	  0x04
#define	BORDER_BOTTOM	  0x08
#define	BORDER_ALL	  0xf
	unsigned int	  border;
	double		  xinterval;
	double		  yinterval;
	struct kplotticln ticline;
#define	TIC_LEFT_IN	  0x01
#define	TIC_LEFT_OUT	  0x02
#define	TIC_RIGHT_IN	  0x04
#define	TIC_RIGHT_OUT	  0x08
#define	TIC_TOP_IN	  0x10
#define	TIC_TOP_OUT	  0x20
#define	TIC_BOTTOM_IN	  0x40
#define	TIC_BOTTOM_OUT	  0x80
	unsigned int	  tic;
	double		  xticlabelrot;
	void		(*xticlabelfmt)(double, char *, size_t);
	void		(*yticlabelfmt)(double, char *, size_t);
	double		  yticlabelpad;
	double		  xticlabelpad;
	struct kplotfont  ticlabelfont;
#define	TICLABEL_LEFT	  0x01
#define	TICLABEL_RIGHT	  0x02
#define	TICLABEL_TOP	  0x04
#define	TICLABEL_BOTTOM	  0x08
	unsigned int	  ticlabel;
#define	GRID_X 		  0x01
#define GRID_Y 		  0x02
#define GRID_ALL 	  0x03
	unsigned int 	  grid;
	struct kplotline  gridline;
	double		  xaxislabelpad;
	double		  yaxislabelpad;
	const char	 *xaxislabel;
	const char	 *x2axislabel;
	const char	 *yaxislabel;
	const char	 *y2axislabel;
	struct kplotfont  axislabelfont;
	double		  xaxislabelrot;
	double		  yaxislabelrot;
#define	EXTREMA_XMIN	  0x01
#define	EXTREMA_XMAX	  0x02
#define	EXTREMA_YMIN	  0x04
#define	EXTREMA_YMAX	  0x08
	unsigned int	  extrema;
	double		  extrema_xmin;
	double		  extrema_xmax;
	double		  extrema_ymin;
	double		  extrema_ymax;
};

struct 	kdata;
struct	kplot;

__BEGIN_DECLS

void		 kdata_destroy(struct kdata *);
int		 kdata_get(const struct kdata *, size_t, struct kpair *);

int		 kdata_array_add(struct kdata *, size_t, double);
struct kdata	*kdata_array_alloc(const struct kpair *, size_t);
int		 kdata_array_fill(struct kdata *, void *,
			void (*)(size_t, struct kpair *, void *));
int		 kdata_array_fill_ydoubles(struct kdata *, const double *);
int		 kdata_array_fill_ysizes(struct kdata *, const size_t *);
int		 kdata_array_set(struct kdata *, size_t, double, double);

int		 kdata_bucket_add(struct kdata *, size_t, double);
struct kdata	*kdata_bucket_alloc(size_t, size_t);
int		 kdata_bucket_set(struct kdata *, size_t, double, double);

struct kdata	*kdata_buffer_alloc(size_t);
int		 kdata_buffer_copy(struct kdata *, const struct kdata *);

int		 kdata_hist_add(struct kdata *, double, double);
struct kdata	*kdata_hist_alloc(double, double, size_t);
int		 kdata_hist_set(struct kdata *, double, double);

struct kdata	*kdata_mean_alloc(struct kdata *);
int		 kdata_mean_attach(struct kdata *, struct kdata *);

struct kdata	*kdata_stddev_alloc(struct kdata *);
int		 kdata_stddev_attach(struct kdata *, struct kdata *);

struct kdata	*kdata_vector_alloc(size_t);
int		 kdata_vector_append(struct kdata *, double, double);
int		 kdata_vector_set(struct kdata *, size_t, double, double);

double		 kdata_pmfmean(const struct kdata *);
double		 kdata_pmfvar(const struct kdata *);
double		 kdata_pmfstddev(const struct kdata *);

ssize_t		 kdata_xmax(const struct kdata *, struct kpair *);
double		 kdata_xmean(const struct kdata *);
ssize_t		 kdata_xmin(const struct kdata *, struct kpair *);

double		 kdata_xstddev(const struct kdata *);
ssize_t		 kdata_ymax(const struct kdata *, struct kpair *);
double		 kdata_ymean(const struct kdata *);
double		 kdata_ystddev(const struct kdata *);
ssize_t		 kdata_ymin(const struct kdata *, struct kpair *);

void		 kdatacfg_defaults(struct kdatacfg *);
void		 kplotcfg_defaults(struct kplotcfg *);
int		 kplotcfg_default_palette(struct kplotccfg **, size_t *);
void		 ksmthcfg_defaults(struct ksmthcfg *);

struct kplot	*kplot_alloc(const struct kplotcfg *);
int		 kplot_detach(struct kplot *, const struct kdata *);
int		 kplot_attach_data(struct kplot *, struct kdata *,
			enum kplottype, const struct kdatacfg *);
int		 kplot_attach_smooth(struct kplot *, struct kdata *,
			enum kplottype, const struct kdatacfg *,
			enum ksmthtype, const struct ksmthcfg *);
int		 kplot_attach_datas(struct kplot *, size_t,
			struct kdata **, const enum kplottype *,
			const struct kdatacfg *const *, enum kplotstype);
void		 kplot_draw(struct kplot *, double, double, cairo_t *);
void		 kplot_free(struct kplot *);
int		 kplot_get_datacfg(struct kplot *, size_t,
			struct kdatacfg **, size_t *);
struct kplotcfg	*kplot_get_plotcfg(struct kplot *);

typedef void (*tic_loop_fn)(struct kplotctx *ctx, double offs, void *user_data);
void kplot_loop_xtics(struct kplotctx *ctx, tic_loop_fn fn, void *user_data);
void kplot_loop_ytics(struct kplotctx *ctx, tic_loop_fn fn, void *user_data);

#include "extern.h"

__END_DECLS

#endif
