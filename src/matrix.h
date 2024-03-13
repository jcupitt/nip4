// shim until matrix lands

struct _Matrix {
	VipsRect range;
	gboolean selected;
};

#define IS_MATRIX(X) (FALSE)
#define MATRIX(X) ((Matrix *) (X))
