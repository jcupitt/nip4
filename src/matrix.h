/* a matrix in a workspace
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

#define MATRIX_TYPE (matrix_get_type())
#define MATRIX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), MATRIX_TYPE, Matrix))
#define MATRIX_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), MATRIX_TYPE, MatrixClass))
#define IS_MATRIX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), MATRIX_TYPE))
#define IS_MATRIX_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), MATRIX_TYPE))
#define MATRIX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), MATRIX_TYPE, MatrixClass))

/* What kind of ui bits have we asked for for this matrix? Keep in sync with
 * matrix_guess_display().
 */
typedef enum {
	MATRIX_DISPLAY_TEXT = 0,		  /* Set of text widgets */
	MATRIX_DISPLAY_SLIDER,			  /* Set of sliders */
	MATRIX_DISPLAY_TOGGLE,			  /* Set of 3 value toggles */
	MATRIX_DISPLAY_TEXT_SCALE_OFFSET, /* Text, with scale/offset widgets */
	MATRIX_DISPLAY_LAST
} MatrixDisplayType;

typedef struct _Matrix {
	Classmodel model;

	/* Base class fields.
	 */
	MatrixValue value;

	/* Other class fields.
	 */
	MatrixDisplayType display; /* Display as */
	double scale;
	double offset;
} Matrix;

typedef struct _MatrixClass {
	ClassmodelClass parent_class;

	/* My methods.
	 */
} MatrixClass;

gboolean matrix_value_resize(MatrixValue *value, int width, int height);

GType matrix_get_type(void);

/* Select rectangular areas of matricies.
 */
void matrix_select(Matrix *matrix, int left, int top, int width, int height);
void matrix_deselect(Matrix *matrix);

int matrix_guess_display(const char *filename);
int image2matrix(VipsImage *in, double **values, int *width, int *height);
VipsImage *matrix2image(PElement *root);
