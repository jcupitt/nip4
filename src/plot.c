/* an input plot
 */

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define DEBUG
 */

#include "nip4.h"

G_DEFINE_TYPE(Plot, plot, CLASSMODEL_TYPE)

static void
plot_free_columns(Plot *plot)
{
	for (int i = 0; i < plot->columns; i++) {
		VIPS_FREE(plot->xcolumn[i]);
		VIPS_FREE(plot->ycolumn[i]);
	}
	VIPS_FREE(plot->xcolumn);
	VIPS_FREE(plot->ycolumn);

	plot->columns = 0;
	plot->rows = 0;
}

static gboolean
plot_alloc_columns(Plot *plot, int columns, int rows)
{
	plot_free_columns(plot);

	plot->xcolumn = VIPS_ARRAY(NULL, columns, double *);
	plot->ycolumn = VIPS_ARRAY(NULL, columns, double *);
	if (!plot->xcolumn ||
		!plot->ycolumn) {
		plot_free_columns(plot);
		return FALSE;
	}

	plot->columns = columns;
	plot->rows = rows;

	for (int c = 0; c < columns; c++) {
		plot->xcolumn[c] = VIPS_ARRAY(NULL, rows, double);
		plot->ycolumn[c] = VIPS_ARRAY(NULL, rows, double);
		if (!plot->xcolumn[c] ||
			!plot->ycolumn[c]) {
			plot_free_columns(plot);
			return FALSE;
		}
	}

	return TRUE;
}

static void
plot_finalize(GObject *gobject)
{
	Plot *plot;

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_PLOT(gobject));

	plot = PLOT(gobject);

#ifdef DEBUG
	printf("plot_finalize\n");
#endif /*DEBUG*/

	/* My instance finalize stuff.
	 */
	image_value_destroy(&plot->value);
	plot_free_columns(plot);
	vips_buf_destroy(&plot->caption_buffer);

	G_OBJECT_CLASS(plot_parent_class)->finalize(gobject);
}

char *
plot_f2c(PlotFormat format)
{
	switch (format) {
	case PLOT_FORMAT_YYYY:
		return _("YYYY");
	case PLOT_FORMAT_XYYY:
		return _("XYYY");
	case PLOT_FORMAT_XYXY:
		return _("XYXY");

	default:
		g_assert(0);

		/* Keep gcc happy.
		 */
		return 0;
	}
}

char *
plot_s2c(PlotStyle style)
{
	switch (style) {
	case PLOT_STYLE_POINT:
		return _("Point");
	case PLOT_STYLE_LINE:
		return _("Line");
	case PLOT_STYLE_SPLINE:
		return _("Spline");
	case PLOT_STYLE_BAR:
		return _("Bar");

	default:
		g_assert(0);

		/* Keep gcc happy.
		 */
		return 0;
	}
}

static const char *
plot_generate_caption(iObject *iobject)
{
	Plot *plot = PLOT(iobject);
	VipsBuf *buf = &plot->caption_buffer;

	vips_buf_rewind(buf);
	image_value_caption(&plot->value, buf);
	vips_buf_appendf(buf, ", %d series, %d points",
		plot->columns, plot->rows);
	vips_buf_appendf(buf, ", xrange [%g, %g]", plot->xmin, plot->xmax);
	vips_buf_appendf(buf, ", yrange [%g, %g]", plot->ymin, plot->ymax);

	return vips_buf_all(buf);
}

/* Unpack all data formats to XYXYXY.
 *
 * image is 1xn, double, in memory
 *
 * 	FIXME ... could save mem by reusing columns of Xes in YYYY and XYYY
 * 	cases
 */
static gboolean
plot_unpack(Plot *plot, VipsImage *image)
{
	g_assert (image->Xsize == 1 &&
		image->data &&
		image->BandFmt == VIPS_FORMAT_DOUBLE);
	double *data = (double *) image->data;

	int rows = image->Ysize;
	int columns;
	switch (plot->format) {
	case PLOT_FORMAT_YYYY:
		columns = image->Bands;
		break;

	case PLOT_FORMAT_XYYY:
		if (image->Bands < 2) {
			error_top(_("Bad value"));
			error_sub(_("More than one column needed for XY plots"));
			return FALSE;
		}
		columns = image->Bands - 1;
		break;

	case PLOT_FORMAT_XYXY:
		if ((image->Bands & 1) != 0) {
			error_top(_("Bad value"));
			error_sub(_("Even number of columns only for XY format plots"));
			return FALSE;
		}
		columns = image->Bands / 2;
		break;

	default:
		columns = 1;
		g_assert(0);
	}

	if (plot->columns != columns ||
		plot->rows != rows)
		plot_alloc_columns(plot, columns, rows);

	switch (plot->format) {
	case PLOT_FORMAT_YYYY:
		for (int c = 0; c < columns; c++)
			for (int r = 0; r < rows; r++) {
				plot->xcolumn[c][r] = r;
				plot->ycolumn[c][r] = data[c + r * image->Bands];
			}
		break;

	case PLOT_FORMAT_XYYY:
		for (int c = 0; c < columns; c++)
			for (int r = 0; r < rows; r++) {
				plot->xcolumn[c][r] = data[r * image->Bands];
				plot->ycolumn[c][r] = data[c + 1 + r * image->Bands];
			}
		break;

	case PLOT_FORMAT_XYXY:
		for (int c = 0; c < columns; c++)
			for (int r = 0; r < rows; r++) {
				plot->xcolumn[c][r] = data[c * 2 + r * image->Bands];
				plot->ycolumn[c][r] = data[c * 2 + 1 + r * image->Bands];
			}
		break;

	default:
		g_assert(0);
	}

	double xmin, xmax;
	double ymin, ymax;
	xmin = plot->xcolumn[0][0];
	xmax = plot->xcolumn[0][0];
	ymin = plot->ycolumn[0][0];
	ymax = plot->ycolumn[0][0];

	for (int c = 0; c < columns; c++)
		for (int r = 0; r < rows; r++) {
			xmax = VIPS_MAX(xmax, plot->xcolumn[c][r]);
			xmin = VIPS_MIN(xmin, plot->xcolumn[c][r]);
			ymax = VIPS_MAX(ymax, plot->ycolumn[c][r]);
			ymin = VIPS_MIN(ymin, plot->ycolumn[c][r]);
		}

	if (plot->xmin == PLOT_RANGE_UNSET)
		plot->xmin = xmin;
	if (plot->xmax == PLOT_RANGE_UNSET)
		plot->xmax = xmax;
	if (plot->ymin == PLOT_RANGE_UNSET)
		plot->ymin = ymin;
	if (plot->ymax == PLOT_RANGE_UNSET)
		plot->ymax = ymax;

	return TRUE;
}

static View *
plot_view_new(Model *model, View *parent)
{
	//return plotview_new();
	return NULL;
}

static void
plot_edit(GtkWidget *parent, Model *model)
{
	Plot *plot = PLOT(model);
	//Plotwindow *plotwindow;
	//plotwindow = plotwindow_new(plot, parent);
}

static xmlNode *
plot_save(Model *model, xmlNode *xnode)
{
	Plot *plot = PLOT(model);
	xmlNode *xthis;

	if (!(xthis = MODEL_CLASS(plot_parent_class)->save(model, xnode)))
		return NULL;

	if (!set_iprop(xthis, "plot_left", plot->left) ||
		!set_iprop(xthis, "plot_top", plot->top) ||
		!set_iprop(xthis, "plot_mag", plot->mag) ||
		!set_sprop(xthis, "show_status",
			bool_to_char(plot->show_status)))
		return NULL;

	return xthis;
}

static gboolean
plot_load(Model *model,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	Plot *plot = PLOT(model);

	g_assert(IS_RHS(parent));

	(void) get_iprop(xnode, "plot_left", &plot->left);
	(void) get_iprop(xnode, "plot_top", &plot->top);
	(void) get_iprop(xnode, "plot_mag", &plot->mag);
	(void) get_bprop(xnode, "show_status", &plot->show_status);

	return MODEL_CLASS(plot_parent_class)->load(model, state, parent, xnode);
}

/* Members of plot we automate.
 */
static ClassmodelMember plot_options[] = {
	{ CLASSMODEL_MEMBER_ENUM, NULL, PLOT_FORMAT_LAST - 1,
		"format", "format", N_("Format"),
		G_STRUCT_OFFSET(Plot, format) },
	{ CLASSMODEL_MEMBER_ENUM, NULL, PLOT_STYLE_LAST - 1,
		"style", "style", N_("Style"),
		G_STRUCT_OFFSET(Plot, style) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		"xmin", "xmin", N_("Xmin"),
		G_STRUCT_OFFSET(Plot, xmin) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		"xmax", "xmax", N_("Xmax"),
		G_STRUCT_OFFSET(Plot, xmax) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		"ymin", "ymin", N_("Ymin"),
		G_STRUCT_OFFSET(Plot, ymin) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		"ymax", "ymax", N_("Ymax"),
		G_STRUCT_OFFSET(Plot, ymax) },
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_CAPTION, "caption", N_("Caption"),
		G_STRUCT_OFFSET(Plot, caption) },
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_XCAPTION, "xcaption", N_("X Axis Caption"),
		G_STRUCT_OFFSET(Plot, xcaption) },
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_YCAPTION, "ycaption", N_("Y Axis Caption"),
		G_STRUCT_OFFSET(Plot, ycaption) },
	{ CLASSMODEL_MEMBER_STRING_LIST, NULL, 0,
		MEMBER_SERIES_CAPTIONS, "series_captions",
		N_("Series Captions"),
		G_STRUCT_OFFSET(Plot, series_captions) }

};

static ClassmodelMember plot_members[] = {
	{ CLASSMODEL_MEMBER_OPTIONS, &plot_options, VIPS_NUMBER(plot_options),
		MEMBER_OPTIONS, NULL, N_("Options"),
		0 },
	{ CLASSMODEL_MEMBER_IMAGE, NULL, 0,
		MEMBER_VALUE, "value", N_("Value"),
		G_STRUCT_OFFSET(Plot, value) }
};

/* Come here after we've read in new values from the heap.
 */
static gboolean
plot_class_get(Classmodel *classmodel, PElement *root)
{
	Plot *plot = PLOT(classmodel);
	ImageValue *value = &plot->value;

	// will be removed on next GC
	Imageinfo *context = imageinfo_new_temp(main_imageinfogroup,
			  reduce_context->heap, NULL);
	VipsImage **t = (VipsImage **)
		vips_object_local_array(VIPS_OBJECT(context->image), 2);

	VipsImage *image;

	image = value->ii->image;

	/* nx1 or 1xm images only ... use Bands for columns.
	 */
	if (image->Xsize != 1 &&
		image->Ysize != 1) {
		error_top(_("Bad value"));
		error_sub(_("1xn or nx1 images only for Plot"));
		return FALSE;
	}

	/* Always vertical.
	 */
	if (image->Ysize == 1) {
		if (vips_rot(image, &t[0], VIPS_ANGLE_D90, NULL)) {
			error_top(_("Bad value"));
			error_vips();
			return FALSE;
		}
		image = t[0];
	}

	/* And always double, and in memory.
	 */
	if (vips_cast(image, &t[1], VIPS_FORMAT_DOUBLE, NULL) ||
		vips_image_wio_input(t[1])) {
		error_top(_("Bad value"));
		error_vips();
		return FALSE;
	}
	image = t[1];

	if (!plot_unpack(plot, image))
		return FALSE;

	return TRUE;
}

static void
plot_reset(Classmodel *classmodel)
{
	Plot *plot = PLOT(classmodel);

	image_value_destroy(&plot->value);
	plot->format = PLOT_FORMAT_YYYY;
	plot->style = PLOT_STYLE_LINE;
	plot->xmin = PLOT_RANGE_UNSET;
	plot->xmax = PLOT_RANGE_UNSET;
	plot->ymin = PLOT_RANGE_UNSET;
	plot->ymax = PLOT_RANGE_UNSET;
	VIPS_SETSTR(plot->caption, NULL);
	VIPS_SETSTR(plot->xcaption, NULL);
	VIPS_SETSTR(plot->ycaption, NULL);
	g_slist_free_full(g_steal_pointer(&plot->series_captions), g_free);
}

static gboolean
plot_graphic_save(Classmodel *classmodel,
	GtkWidget *parent, const char *filename)
{
	Plot *plot = PLOT(classmodel);
	ImageValue *value = &plot->value;

	if (value->ii) {
		char buf[FILENAME_MAX];
		expand_variables(filename, buf);

		if (vips_image_write_to_file(value->ii->image, buf, NULL)) {
			error_top(_("Unable to write"));
			error_vips();
			return FALSE;
		}
	}

	return TRUE;
}

static void
plot_class_init(PlotClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	iObjectClass *iobject_class = (iObjectClass *) class;
	ModelClass *model_class = (ModelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	/* Create signals.
	 */

	/* Init methods.
	 */
	gobject_class->finalize = plot_finalize;

	iobject_class->generate_caption = plot_generate_caption;

#ifdef HAVE_LIBGOFFICE
	model_class->view_new = plot_view_new;
#endif /*HAVE_LIBGOFFICE*/
	model_class->edit = plot_edit;
	model_class->save = plot_save;
	model_class->load = plot_load;

	classmodel_class->class_get = plot_class_get;
	classmodel_class->members = plot_members;
	classmodel_class->n_members = VIPS_NUMBER(plot_members);
	classmodel_class->reset = plot_reset;

	classmodel_class->graphic_save = plot_graphic_save;

	/* Static init.
	 */
	model_register_loadable(MODEL_CLASS(class));
}

static void
plot_init(Plot *plot)
{
#ifdef DEBUG
	printf("plot_init\n");
#endif /*DEBUG*/

	image_value_init(&plot->value, CLASSMODEL(plot));

	plot->xcolumn = NULL;
	plot->ycolumn = NULL;
	plot->rows = 0;
	plot->columns = 0;

	plot->show_status = FALSE;
	plot->mag = 100;
	plot->left = 0;
	plot->top = 0;

	vips_buf_init_dynamic(&plot->caption_buffer, MAX_LINELENGTH);

	iobject_set(IOBJECT(plot), CLASS_PLOT, NULL);

	plot_reset(CLASSMODEL(plot));
}
