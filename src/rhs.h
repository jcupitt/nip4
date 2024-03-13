// shim until rhs.h lands

#define RHS(X) ((Rhs *) X)

struct _Rhs {
	Model *graphic; /* Graphic display ... toggle/slider/etc */
	Model *scol;	/* Class display */
	Model *itext;	/* Text display */
};
