// shim until matrix lands

struct _Matrix {
	VipsRect range;
	gboolean selected;
};

#define IS_MATRIX(X) (FALSE)
#define MATRIX(X) ((Matrix *) (X))

gboolean matrix_value_resize(MatrixValue *value, int width, int height);

int image2matrix(VipsImage *in, double **values, int *width, int *height);
VipsImage *matrix2image(PElement *root);
