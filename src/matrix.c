/* an input matrix
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

G_DEFINE_TYPE(Matrix, matrix, CLASSMODEL_TYPE)

static void
matrix_finalize(GObject *gobject)
{
	Matrix *matrix;

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_MATRIX(gobject));

	matrix = MATRIX(gobject);

#ifdef DEBUG
	printf("matrix_finalize\n");
#endif /*DEBUG*/

	/* My instance finalize stuff.
	 */
	VIPS_FREE(matrix->value.coeff);

	G_OBJECT_CLASS(matrix_parent_class)->finalize(gobject);
}

/* Rearrange our model for a new width/height.
 */
gboolean
matrix_value_resize(MatrixValue *value, int width, int height)
{
	double *coeff;
	int x, y, i;

	if (width == value->width && height == value->height)
		return TRUE;

	if (!(coeff = IARRAY(NULL, width * height, double)))
		return FALSE;

	/* Set what we can with values from the old matrix.
	 */
	for (i = 0, y = 0; y < height; y++)
		for (x = 0; x < width; x++, i++)
			if (y < value->height && x < value->width)
				coeff[i] = value->coeff[x + y * value->width];
			else
				coeff[i] = 0.0;

	/* Install new values.
	 */
	VIPS_FREE(value->coeff);
	value->coeff = coeff;
	value->width = width;
	value->height = height;

	return TRUE;
}

static View *
matrix_view_new(Model *model, View *parent)
{
	return matrixview_new();
}

/* Members of matrix we automate.
 */
static ClassmodelMember matrix_members[] = {
	{ CLASSMODEL_MEMBER_MATRIX, NULL, 0,
		MEMBER_VALUE, NULL, N_("Value"),
		G_STRUCT_OFFSET(Matrix, value) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		MEMBER_SCALE, "scale", N_("Scale"),
		G_STRUCT_OFFSET(Matrix, scale) },
	{ CLASSMODEL_MEMBER_DOUBLE, NULL, 0,
		MEMBER_OFFSET, "offset", N_("Offset"),
		G_STRUCT_OFFSET(Matrix, offset) },
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_FILENAME, "filename", N_("Filename"),
		G_STRUCT_OFFSET(Classmodel, filename) },
	{ CLASSMODEL_MEMBER_ENUM, NULL, MATRIX_DISPLAY_LAST - 1,
		MEMBER_DISPLAY, "display", N_("Display"),
		G_STRUCT_OFFSET(Matrix, display) }
};

static void
matrix_class_init(MatrixClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	iObjectClass *iobject_class = (iObjectClass *) class;
	ModelClass *model_class = (ModelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	gobject_class->finalize = matrix_finalize;

	iobject_class->user_name = _("Matrix");

	model_class->view_new = matrix_view_new;

	/* FIXME ... need this?
	classmodel_class->graphic_save = matrix_graphic_save;
	classmodel_class->graphic_replace = matrix_graphic_replace;

	classmodel_class->filetype = filesel_type_matrix;
	classmodel_class->filetype_pref = "MATRIX_FILE_TYPE";
	 */

	classmodel_class->members = matrix_members;
	classmodel_class->n_members = VIPS_NUMBER(matrix_members);

	model_register_loadable(MODEL_CLASS(class));
}

static void
matrix_init(Matrix *matrix)
{
#ifdef DEBUG
	printf("matrix_init\n");
#endif /*DEBUG*/

	matrix->value.coeff = NULL;
	matrix->value.width = 0;
	matrix->value.height = 0;
	matrix->display = MATRIX_DISPLAY_TEXT;
	matrix->scale = 1.0;
	matrix->offset = 0.0;

	iobject_set(IOBJECT(matrix), CLASS_MATRIX, NULL);
}

int
matrix_guess_display(const char *filename)
{
	/* Choose display type based on filename suffix ... rec
	 * displays as 1, mor displays as 2, .con displays as 3, all others
	 * display as 0. Keep in sync with MatrixDisplayType.
	 */
	static const char *suffixes[] = {
		".mat",
		".mor",
		".con",
	};

	if (!filename)
		return 0;

	for (int i = 0; i < VIPS_NUMBER(suffixes); i++)
		if (vips_iscasepostfix(filename, suffixes[i]))
			return i + 1;

	return 0;
}

/* Make an image from a nip class instance.
 */
VipsImage *
matrix2image(PElement *root)
{
	int width, height;
	if (!class_get_member_matrix_size(root, MEMBER_VALUE, &width, &height))
		return NULL;
	VipsImage *matrix;
	if (!(matrix = vips_image_new_matrix(width, height)))
		return NULL;

	char buf[MAX_STRSIZE];
	if (!class_get_member_string(root, MEMBER_FILENAME, buf, MAX_STRSIZE) &&
		!temp_name(buf, NULL, "mat")) {
		VIPS_UNREF(matrix);
		return NULL;
	}
	VIPS_SETSTR(matrix->filename, buf);

	if (!class_get_member_matrix(root, MEMBER_VALUE,
			(double *) matrix->data, width * height, &width, &height)) {
		VIPS_UNREF(matrix);
		return FALSE;
	}

	double scale, offset;
	if (!class_get_member_real(root, MEMBER_SCALE, &scale))
		scale = 1.0;
	if (!class_get_member_real(root, MEMBER_OFFSET, &offset))
		offset = 0.0;
	vips_image_set_double(matrix, "scale", scale);
	vips_image_set_double(matrix, "offset", offset);

	return matrix;
}

// turn an image into a 2D matrix ... result must be g_free()d
int
image2matrix(VipsImage *in, double **values, int *width, int *height)
{
	if (in->Bands == 1) {
		*width = in->Xsize;
		*height = in->Ysize;
	}
	else if (in->Xsize == 1) {
		*width = in->Bands;
		*height = in->Ysize;
	}
	else if (in->Ysize == 1) {
		*width = in->Xsize;
		*height = in->Bands;
	}
	else {
		error_top(_("Out of range"));
		error_sub(_("one band, nx1, or 1xn images only"));
		return -1;
	}

	// limit to 16-bit LUT size
	if (*width > 65536 ||
		*height > 65536 ||
		*width * *height > 65536) {
		error_top(_("Out of range"));
		error_sub(_("image too large"));
		return -1;
	}

	VipsImage *t;
	if (vips_cast(in, &t, VIPS_FORMAT_DOUBLE, NULL))
		return -1;

	void *mem;
	size_t size;
	if (!(mem = vips_image_write_to_memory(t, &size))) {
		VIPS_UNREF(t);
		return -1;
	}
	VIPS_UNREF(t);

	*values = (double *) mem;

	return 0;
}
