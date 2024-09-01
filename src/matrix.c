// shim

#include "nip4.h"

gboolean
matrix_value_resize(MatrixValue *value, int width, int height)
{
	return TRUE;
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
		!temp_name(buf, "mat")) {
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
	printf("image2matrix: in = %p\n", in);

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

	// big enough for a 16-bit LUT
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

