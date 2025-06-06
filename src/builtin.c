/* Run builtin functions ... sin/error etc.
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

#include "nip4.h"

#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_errno.h>

/* Trace builtin calls.
#define DEBUG
 */

/* Spot something that might be an arg to sin/cos/tan etc.
 */
static gboolean
ismatharg(Reduce *rc, PElement *base)
{
	return PEISIMAGE(base) || PEISREAL(base) || PEISCOMPLEX(base);
}

/* Spot something that might be an arg to re/im etc.
 */
static gboolean
iscomplexarg(Reduce *rc, PElement *base)
{
	return PEISIMAGE(base) || PEISCOMPLEX(base);
}

/* Spot anything.
 */
static gboolean
isany(Reduce *rc, PElement *base)
{
	return TRUE;
}

/* Other PEIS as functions.
 */
static gboolean
pe_is_image(Reduce *rc, PElement *base)
{
	return PEISIMAGE(base);
}

static gboolean
pe_is_real(Reduce *rc, PElement *base)
{
	return PEISREAL(base);
}

static gboolean
pe_is_complex(Reduce *rc, PElement *base)
{
	return PEISCOMPLEX(base);
}

static gboolean
pe_is_bool(Reduce *rc, PElement *base)
{
	return PEISBOOL(base);
}

static gboolean
pe_is_char(Reduce *rc, PElement *base)
{
	return PEISCHAR(base);
}

static gboolean
pe_is_list(Reduce *rc, PElement *base)
{
	return PEISLIST(base);
}

static gboolean
pe_is_flist(Reduce *rc, PElement *base)
{
	return PEISFLIST(base);
}

static gboolean
pe_is_class(Reduce *rc, PElement *base)
{
	return PEISCLASS(base);
}

/* The types we might want to spot for builtins.
 *
 * Others, eg.:
 *
static BuiltinTypeSpot bool_spot = { "bool", pe_is_bool };
static BuiltinTypeSpot realvec_spot = { "[real]", reduce_is_realvec };
static BuiltinTypeSpot matrix_spot = { "[[real]]", reduce_is_matrix };
static gboolean pe_is_gobject( Reduce *rc, PElement *base )
		{ return( PEISMANAGEDGOBJECT( base ) ); }
static BuiltinTypeSpot gobject_spot = { "GObject", pe_is_gobject };
 *
 */

static BuiltinTypeSpot vimage_spot = { "vips_image", pe_is_image };
static BuiltinTypeSpot real_spot = { "real", pe_is_real };
static BuiltinTypeSpot complex_spot = { "complex|image", iscomplexarg };
static BuiltinTypeSpot flist_spot = { "non-empty list", pe_is_flist };
static BuiltinTypeSpot string_spot = { "[char]", reduce_is_finitestring };
static BuiltinTypeSpot list_spot = { "[*]", reduce_is_list };
static BuiltinTypeSpot math_spot = { "image|real|complex", ismatharg };
static BuiltinTypeSpot instance_spot = { "class", pe_is_class };
static BuiltinTypeSpot any_spot = { "any", isany };

static gboolean
matrix_new(Heap *heap,
	VipsImage *image, double *values, int width, int height, PElement *out)
{
	Symbol *sym;
	if (!(sym = compile_lookup(symbol_root->expr->compile, CLASS_MATRIX)) ||
		!sym->expr ||
		!sym->expr->compile ||
		!heap_copy(heap, sym->expr->compile, out))
		return FALSE;

	PElement rhs;
	double scale = vips_image_get_scale(image);
	double offset = vips_image_get_offset(image);
	// suffix makes guess_display default to numeric
	const char *filename = image->filename ? image->filename : "untitled.mat";
	if (!heap_appl_add(heap, out, &rhs) ||
		!heap_matrix_new(heap, width, height, values, &rhs) ||
		!heap_appl_add(heap, out, &rhs) ||
		!heap_real_new(heap, scale, &rhs) ||
		!heap_appl_add(heap, out, &rhs) ||
		!heap_real_new(heap, offset, &rhs) ||
		!heap_appl_add(heap, out, &rhs) ||
		!heap_managedstring_new(heap, filename, &rhs) ||
		!heap_appl_add(heap, out, &rhs) ||
		!heap_real_new(heap, matrix_guess_display(filename), &rhs))
		return FALSE;

	return TRUE;
}

/* Args for "vips_matrix".
 */
static BuiltinTypeSpot *image2matrix_args[] = {
	&vimage_spot
};

static void
apply_image2matrix_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;

	PEPOINTRIGHT(arg[0], &rhs);
	Imageinfo *ii = reduce_get_image(rc, &rhs);

	double *values;
	int width;
	int height;
	if (image2matrix(ii->image, &values, &width, &height))
		reduce_throw(rc);

	if (!matrix_new(rc->heap, ii->image, values, width, height, out)) {
		VIPS_FREE(values);
		reduce_throw(rc);
	}
	VIPS_FREE(values);
}

/* Args for "vips_image_matrix".
 */
static BuiltinTypeSpot *matrix2image_args[] = {
	&instance_spot
};

static void
apply_matrix2image_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;

	PEPOINTRIGHT(arg[0], &rhs);
	if (!reduce_is_instanceof(rc, CLASS_MATRIX, &rhs)) {
		char txt[100];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		itext_value_ev(rc, &buf, &rhs);
		error_top(_("Bad argument"));
		error_sub(_("argument to \"%s\" should "
					"be instance of \"%s\", you passed:\n  %s"),
			name, CLASS_MATRIX, vips_buf_all(&buf));
		reduce_throw(rc);
	}

	VipsImage *matrix;
	if (!(matrix = matrix2image(&rhs)))
		reduce_throw(rc);

	Imageinfo *ii;
	if (!(ii = imageinfo_new(main_imageinfogroup,
			  rc->heap, matrix, matrix->filename))) {
		VIPS_UNREF(matrix);
		reduce_throw(rc);
	}

	PEPUTP(out, ELEMENT_MANAGED, ii);
}

/* Args for "_".
 */
static BuiltinTypeSpot *underscore_args[] = {
	&string_spot
};

/* Do a _ call. Args already spotted.
 */
static void
apply_underscore_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char text[MAX_STRSIZE];

	PEPOINTRIGHT(arg[0], &rhs);
	(void) reduce_get_string(rc, &rhs, text, MAX_STRSIZE);

	/* Pump though gettext.
	 */
	if (!heap_managedstring_new(rc->heap, _(text), out))
		reduce_throw(rc);
}

/* Args for "has_member".
 */
static BuiltinTypeSpot *has_member_args[] = {
	&string_spot,
	&any_spot
};

/* Do a has_member call. Args already spotted.
 */
static void
apply_has_member_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char mname[MAX_STRSIZE];
	PElement member;

	PEPOINTRIGHT(arg[1], &rhs);
	(void) reduce_get_string(rc, &rhs, mname, MAX_STRSIZE);
	PEPOINTRIGHT(arg[0], &rhs);
	PEPUTP(out, ELEMENT_BOOL,
		class_get_member(&rhs, mname, NULL, &member));
}

/* Args for "is_instanceof".
 */
static BuiltinTypeSpot *is_instanceof_args[] = {
	&string_spot,
	&any_spot
};

/* Do an is_instance call. Args already spotted.
 */
static void
apply_is_instanceof_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char kname[MAX_STRSIZE];

	PEPOINTRIGHT(arg[1], &rhs);
	(void) reduce_get_string(rc, &rhs, kname, MAX_STRSIZE);
	PEPOINTRIGHT(arg[0], &rhs);
	PEPUTP(out, ELEMENT_BOOL, reduce_is_instanceof(rc, kname, &rhs));
}

/* Args for builtin on complex.
 */
static BuiltinTypeSpot *complex_args[] = {
	&complex_spot
};

/* Do a complex op. Args already spotted.
 */
static void
apply_complex_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;

	PEPOINTRIGHT(arg[0], &rhs);

	if (PEISIMAGE(&rhs)) {
		if (g_str_equal(name, "re"))
			vo_callva(rc, out, "complexget",
				PEGETIMAGE(&rhs), VIPS_OPERATION_COMPLEXGET_REAL);
		else if (g_str_equal(name, "im"))
			vo_callva(rc, out, "complexget",
				PEGETIMAGE(&rhs), VIPS_OPERATION_COMPLEXGET_IMAG);
		else
			PEPUTP(out, ELEMENT_ELIST, NULL);
	}
	else if (PEISCOMPLEX(&rhs)) {
		if (g_str_equal(name, "re"))
			PEPUTP(out, ELEMENT_NODE, GETLEFT(PEGETVAL(&rhs)));
		else if (g_str_equal(name, "im"))
			PEPUTP(out, ELEMENT_NODE, GETRIGHT(PEGETVAL(&rhs)));
		else
			PEPUTP(out, ELEMENT_ELIST, NULL);
	}
	else
		error("internal error #98743698437639487");
}

/* Args for builtin on list.
 */
static BuiltinTypeSpot *flist_args[] = {
	&flist_spot
};

/* Do a list op. Args already spotted.
 */
static void
apply_list_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	PElement a;

	PEPOINTRIGHT(arg[0], &rhs);
	g_assert(PEISFLIST(&rhs));

	reduce_get_list(rc, &rhs);

	if (strcmp(name, "hd") == 0) {
		PEGETHD(&a, &rhs);
		PEPUTPE(out, &a);
	}
	else if (strcmp(name, "tl") == 0) {
		PEGETTL(&a, &rhs);
		PEPUTPE(out, &a);
	}
	else
		error("internal error #098734953");
}

/* "gammq"
 */
static BuiltinTypeSpot *gammq_args[] = {
	&real_spot,
	&real_spot
};

static void
apply_gammq_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	double a, x, Q;

	PEPOINTRIGHT(arg[1], &rhs);
	a = PEGETREAL(&rhs);
	PEPOINTRIGHT(arg[0], &rhs);
	x = PEGETREAL(&rhs);

	if (a <= 0 || x < 0) {
		error_top(_("Out of range"));
		error_sub(_("gammq arguments must be a > 0, x >= 0."));
		reduce_throw(rc);
	}

	Q = gsl_sf_gamma_inc_Q(a, x);

	if (!heap_real_new(rc->heap, Q, out))
		reduce_throw(rc);
}

/* Args for "vips_image".
 */
static BuiltinTypeSpot *image_args[] = {
	&string_spot
};

/* Do a image call.
 */
static void
apply_image_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	Heap *heap = rc->heap;

	PElement rhs;
	char buf[VIPS_PATH_MAX];
	char filename[VIPS_PATH_MAX];
	char mode[VIPS_PATH_MAX];

	char *fn;
	Imageinfo *ii;

	/* Get string.
	 */
	PEPOINTRIGHT(arg[0], &rhs);
	(void) reduce_get_string(rc, &rhs, buf, VIPS_PATH_MAX);

	/* The buf might be something like n3862.pyr.tif:1, ie. contain some
	 * load options. Split and search just for the filename component.
	 */
	vips__filename_split8(buf, filename, mode);

	/* Try to load image from given string.
	 */
	if (!(fn = path_find_file(filename)))
		reduce_throw(rc);

	/* Reattach the mode and load.
	 */
	g_snprintf(buf, VIPS_PATH_MAX, "%s%s", fn, mode);
	if (!(ii = imageinfo_new_input(main_imageinfogroup, NULL, heap, buf))) {
		VIPS_FREE(fn);
		reduce_throw(rc);
	}
	VIPS_FREE(fn);

	PEPUTP(out, ELEMENT_MANAGED, ii);
	MANAGED_UNREF(ii);
}

/* Args for "read".
 */
static BuiltinTypeSpot *read_args[] = {
	&string_spot
};

/* Do a read call.
 */
static void
apply_read_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char buf[VIPS_PATH_MAX];

	/* Get string.
	 */
	PEPOINTRIGHT(arg[0], &rhs);
	(void) reduce_get_string(rc, &rhs, buf, VIPS_PATH_MAX);

	if (!heap_file_new(rc->heap, buf, out))
		reduce_throw(rc);
}

/* Args for "graph_export_image".
 */
static BuiltinTypeSpot *graph_export_image_args[] = {
	&real_spot,
	&any_spot
};

/* Do a graph_export_image call.
 */
static void
apply_graph_export_image_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	double dpi;
	Plot *plot;
	Imageinfo *ii;

	PEPOINTRIGHT(arg[1], &rhs);
	dpi = PEGETREAL(&rhs);

	PEPOINTRIGHT(arg[0], &rhs);
	if (!reduce_is_instanceof(rc, CLASS_PLOT, &rhs)) {
		char txt[100];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		itext_value_ev(rc, &buf, &rhs);
		error_top(_("Bad argument"));
		error_sub(_("argument 2 to \"%s\" should "
					"be instance of \"%s\", you passed:\n  %s"),
			name, CLASS_PLOT,
			vips_buf_all(&buf));
		reduce_throw(rc);
	}

	plot = g_object_new(PLOT_TYPE, NULL);

	if (!classmodel_update_members(CLASSMODEL(plot), &rhs)) {
		VIPS_UNREF(plot);
		reduce_throw(rc);
	}

	if (!(ii = plotdisplay_to_image(plot,
			rc, dpi * 340.0 / 72.0, dpi * 226.0 / 72.0))) {
		VIPS_UNREF(plot);
		reduce_throw(rc);
	}

	VIPS_UNREF(plot);

	PEPUTP(out, ELEMENT_MANAGED, ii);
}

/* Args for "header_get_typeof_args".
 */
static BuiltinTypeSpot *header_get_typeof_args[] = {
	&string_spot,
	&vimage_spot
};

/* Get type of header field.
 */
static void
apply_header_get_type_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	Heap *heap = rc->heap;

	PElement rhs;

	/* Get string.
	 */
	PEPOINTRIGHT(arg[1], &rhs);
	char buf[VIPS_PATH_MAX];
	(void) reduce_get_string(rc, &rhs, buf, VIPS_PATH_MAX);

	PEPOINTRIGHT(arg[0], &rhs);
	Imageinfo *ii = reduce_get_image(rc, &rhs);

	GType type = vips_image_get_typeof(ii->image, buf);
	if (!heap_real_new(heap, type, out))
		reduce_throw(rc);
}

/* Get an int.
 */
static void
apply_header_int_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	Heap *heap = rc->heap;

	PElement rhs;

	/* Get string.
	 */
	PEPOINTRIGHT(arg[1], &rhs);
	char buf[VIPS_PATH_MAX];
	(void) reduce_get_string(rc, &rhs, buf, VIPS_PATH_MAX);

	PEPOINTRIGHT(arg[0], &rhs);
	Imageinfo *ii = reduce_get_image(rc, &rhs);

	int value;
	if (vips_image_get_int(ii->image, buf, &value) ||
		!heap_real_new(heap, value, out))
		reduce_throw(rc);
}

/* Get a double.
 */
static void
apply_header_double_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	Heap *heap = rc->heap;

	PElement rhs;

	/* Get string.
	 */
	PEPOINTRIGHT(arg[1], &rhs);
	char buf[VIPS_PATH_MAX];
	(void) reduce_get_string(rc, &rhs, buf, VIPS_PATH_MAX);

	PEPOINTRIGHT(arg[0], &rhs);
	Imageinfo *ii = reduce_get_image(rc, &rhs);

	double value;
	if (vips_image_get_double(ii->image, buf, &value) ||
		!heap_real_new(heap, value, out))
		reduce_throw(rc);
}

/* Get a string.
 */
static void
apply_header_string_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	Heap *heap = rc->heap;

	PElement rhs;

	/* Get string.
	 */
	PEPOINTRIGHT(arg[1], &rhs);
	char buf[VIPS_PATH_MAX];
	(void) reduce_get_string(rc, &rhs, buf, VIPS_PATH_MAX);

	PEPOINTRIGHT(arg[0], &rhs);
	Imageinfo *ii = reduce_get_image(rc, &rhs);

	// a managedstring, since value might go away ... take a copy and control
	// it with our GC
	const char *value;
	if (vips_image_get_string(ii->image, buf, &value))
		reduce_throw(rc);
	if (!value) {
		error_top(_("Null value"));
		error_sub(_("field %s has a NULL value"), buf);
		reduce_throw(rc);
	}
	if (!heap_managedstring_new(heap, value, out))
		reduce_throw(rc);
}

/* Args for "math".
 */
static BuiltinTypeSpot *math_args[] = {
	&math_spot
};

/* A math function ... name, number implementation, image implementation.
 */
typedef struct {
	const char *name;	   /* ip name */
	double (*rfn)(double); /* Number implementation */
	const char *operation; /* libvips operation name */
	int type;			   /* and operation enum */
} MathFn;

static double
ip_sin(double a)
{
	return sin(VIPS_RAD(a));
}

static double
ip_cos(double a)
{
	return cos(VIPS_RAD(a));
}

static double
ip_tan(double a)
{
	return tan(VIPS_RAD(a));
}

static double
ip_asin(double a)
{
	return VIPS_DEG(asin(a));
}

static double
ip_acos(double a)
{
	return VIPS_DEG(acos(a));
}

static double
ip_atan(double a)
{
	return VIPS_DEG(atan(a));
}

static double
ip_exp10(double a)
{
	return pow(10.0, a);
}

static double
ip_ceil(double a)
{
	return ceil(a);
}

static double
ip_floor(double a)
{
	return floor(a);
}

/* Table of math functions ... number implementations, image implementations.
 */
static MathFn math_fn[] = {
	{ "sin", &ip_sin, "math", VIPS_OPERATION_MATH_SIN },
	{ "cos", &ip_cos, "math", VIPS_OPERATION_MATH_COS },
	{ "tan", &ip_tan, "math", VIPS_OPERATION_MATH_TAN },
	{ "asin", &ip_asin, "math", VIPS_OPERATION_MATH_ASIN },
	{ "acos", &ip_acos, "math", VIPS_OPERATION_MATH_ACOS },
	{ "atan", &ip_atan, "math", VIPS_OPERATION_MATH_ATAN },
	{ "log", &log, "math", VIPS_OPERATION_MATH_LOG },
	{ "log10", &log10, "math", VIPS_OPERATION_MATH_LOG10 },
	{ "exp", &exp, "math", VIPS_OPERATION_MATH_EXP },
	{ "exp10", &ip_exp10, "math", VIPS_OPERATION_MATH_EXP10 },
	{ "ceil", &ip_ceil, "round", VIPS_OPERATION_ROUND_CEIL },
	{ "floor", &ip_floor, "round", VIPS_OPERATION_ROUND_FLOOR }
};

/* Do a math function (eg. sin, cos, tan).
 */
static void
apply_math_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	int i;

	/* Find implementation.
	 */
	for (i = 0; i < VIPS_NUMBER(math_fn); i++)
		if (g_str_equal(name, math_fn[i].name))
			break;
	if (i == VIPS_NUMBER(math_fn))
		error("internal error #928456936");

	/* Get arg type ... real/complex/image
	 */
	PEPOINTRIGHT(arg[0], &rhs);
	if (PEISIMAGE(&rhs))
		/* Easy ... pass to VIPS.
		 */
		vo_callva(rc, out, math_fn[i].operation,
			PEGETIMAGE(&rhs), math_fn[i].type);
	else if (PEISREAL(&rhs)) {
		double a = PEGETREAL(&rhs);
		double b = math_fn[i].rfn(a);

		if (!heap_real_new(rc->heap, b, out))
			reduce_throw(rc);
	}
	else if (PEISCOMPLEX(&rhs)) {
		error_top(_("Not implemented"));
		error_sub(_("complex math ops not implemented"));
		reduce_throw(rc);
	}
	else
		error("internal error #92870653");
}

/* Args for "predicate".
 */
static BuiltinTypeSpot *pred_args[] = {
	&any_spot
};

/* A predicate function ... name, implementation.
 */
typedef struct {
	const char *name;					  /* ip name */
	gboolean (*fn)(Reduce *, PElement *); /* Implementation */
} PredicateFn;

/* Table of predicate functions ... name and implementation.
 */
static PredicateFn predicate_fn[] = {
	{ "is_image", &pe_is_image },
	{ "is_bool", &pe_is_bool },
	{ "is_real", &pe_is_real },
	{ "is_char", &pe_is_char },
	{ "is_class", &pe_is_class },
	{ "is_list", &pe_is_list },
	{ "is_complex", &pe_is_complex }
};

/* Do a predicate function (eg. is_bool)
 */
static void
apply_pred_call(Reduce *rc, const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	gboolean res;
	int i;

	/* Find implementation.
	 */
	for (i = 0; i < VIPS_NUMBER(predicate_fn); i++)
		if (strcmp(name, predicate_fn[i].name) == 0)
			break;
	if (i == VIPS_NUMBER(predicate_fn))
		error("internal error #928456936");

	/* Call!
	 */
	PEPOINTRIGHT(arg[0], &rhs);
	res = predicate_fn[i].fn(rc, &rhs);
	PEPUTP(out, ELEMENT_BOOL, res);
}

/* Args for "error".
 */
static BuiltinTypeSpot *error_args[] = {
	&string_spot
};

/* Do "error".
 */
static void
apply_error_call(Reduce *rc, const char *name, HeapNode **arg, PElement *out)
{
	char buf[MAX_STRSIZE];
	PElement rhs;

	/* Get string.
	 */
	PEPOINTRIGHT(arg[0], &rhs);
	(void) reduce_get_string(rc, &rhs, buf, MAX_STRSIZE);

	error_top(_("Macro error"));
	error_sub("%s", buf);
	reduce_throw(rc);
}

/* Args for "search".
 */
static BuiltinTypeSpot *search_args[] = {
	&string_spot
};

/* Do "search".
 */
static void
apply_search_call(Reduce *rc, const char *name, HeapNode **arg, PElement *out)
{
	char buf[MAX_STRSIZE];
	PElement rhs;
	char *fn;

	/* Get string.
	 */
	PEPOINTRIGHT(arg[0], &rhs);
	(void) reduce_get_string(rc, &rhs, buf, MAX_STRSIZE);

	if (!(fn = path_find_file(buf)))
		/* If not found, return [].
		 */
		fn = g_strdup("");

	if (!heap_managedstring_new(rc->heap, fn, out)) {
		VIPS_FREE(fn);
		reduce_throw(rc);
	}
	VIPS_FREE(fn);
}

/* Args for "print".
 */
static BuiltinTypeSpot *print_args[] = {
	&any_spot
};

/* Do "print".
 */
static void
apply_print_call(Reduce *rc, const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char txt[MAX_STRSIZE];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	PEPOINTRIGHT(arg[0], &rhs);
	itext_value_ev(rc, &buf, &rhs);

	if (!heap_managedstring_new(rc->heap, vips_buf_all(&buf), out))
		reduce_throw(rc);
}

/* Args for "dir".
 */
static BuiltinTypeSpot *dir_args[] = {
	&any_spot
};

static void *
dir_object_member(Symbol *sym, PElement *value,
	Reduce *rc, PElement *list)
{
	PElement t;

	if (!heap_list_add(rc->heap, list, &t) ||
		!heap_managedstring_new(rc->heap, IOBJECT(sym)->name, &t))
		reduce_throw(rc);
	(void) heap_list_next(list);

	return NULL;
}

static void *
dir_object(Reduce *rc, PElement *list, PElement *instance, PElement *out)
{
	PElement p;

	/* p walks down the list as we build it, list stays pointing at the
	 * head ready to be written to out.
	 */
	p = *list;
	heap_list_init(&p);
	class_map(instance, (class_map_fn) dir_object_member, rc, &p);
	PEPUTPE(out, list);

	return NULL;
}

static void *
dir_scope(Symbol *sym, Reduce *rc, PElement *list)
{
	PElement t;

	if (!heap_list_add(rc->heap, list, &t) ||
		!heap_managedstring_new(rc->heap, IOBJECT(sym)->name, &t))
		reduce_throw(rc);
	(void) heap_list_next(list);

	return NULL;
}

static void *
dir_gtype(GType type, void *a, void *b)
{
	Reduce *rc = (Reduce *) a;
	PElement *list = (PElement *) b;
	PElement t;

	if (!heap_list_add(rc->heap, list, &t) ||
		!heap_real_new(rc->heap, type, &t))
		return rc;
	(void) heap_list_next(list);

	return NULL;
}

static void
dir_gobject(Reduce *rc,
	GParamSpec **properties, guint n_properties, PElement *out)
{
	int i;
	PElement list;

	list = *out;
	heap_list_init(&list);

	for (i = 0; i < n_properties; i++) {
		PElement t;

		if (!heap_list_add(rc->heap, &list, &t) ||
			!heap_managedstring_new(rc->heap,
				properties[i]->name, &t))
			reduce_throw(rc);
		(void) heap_list_next(&list);
	}
}

/* Do "dir".
 */
static void
apply_dir_call(Reduce *rc, const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;

	PEPOINTRIGHT(arg[0], &rhs);

	if (PEISCLASS(&rhs))
		/* This is more complex than it looks. We have to walk a class
		 * instance generating a list of member names, while not
		 * destroying the instance as we go, in the case that out will
		 * overwrite (rhs) arg[0].
		 */
		reduce_safe_pointer(rc, (reduce_safe_pointer_fn) dir_object,
			&rhs, out, NULL, NULL);
	else if (PEISSYMREF(&rhs)) {
		Symbol *sym = PEGETSYMREF(&rhs);

		if (is_scope(sym) && sym->expr && sym->expr->compile) {
			PElement list;

			list = *out;
			heap_list_init(&list);

			icontainer_map(ICONTAINER(sym->expr->compile),
				(icontainer_map_fn) dir_scope, rc, &list);
		}
	}
	else if (PEISREAL(&rhs)) {
		/* Assume this is a gtype and try to get the children of that
		 * type.
		 */
		GType type = PEGETREAL(&rhs);
		PElement list;

		list = *out;
		heap_list_init(&list);

		if (!g_type_name(type)) {
			error_top(_("No such type"));
			error_sub(_("GType %u not found"), (unsigned int) type);
			reduce_throw(rc);
		}

		if (vips_type_map(type, dir_gtype, rc, &list))
			reduce_throw(rc);
	}
	else if (PEISMANAGEDGOBJECT(&rhs)) {
		guint n_properties;
		ManagedgobjectClass *class =
			MANAGEDGOBJECT_GET_CLASS(PEGETMANAGEDGOBJECT(&rhs));
		GParamSpec **properties;

		properties = g_object_class_list_properties(
			G_OBJECT_CLASS(class), &n_properties);
		dir_gobject(rc, properties, n_properties, out);
		g_free(properties);
	}
	else
		/* Just [], ie. no names possible.
		 */
		heap_list_init(out);
}

/* Args for "expand".
 */
static BuiltinTypeSpot *expand_args[] = {
	&string_spot
};

/* Do "expand".
 */
static void
apply_expand_call(Reduce *rc, const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char txt[VIPS_PATH_MAX];
	char txt2[VIPS_PATH_MAX];

	PEPOINTRIGHT(arg[0], &rhs);
	(void) reduce_get_string(rc, &rhs, txt, VIPS_PATH_MAX);
	expand_variables(txt, txt2);

	if (!heap_managedstring_new(rc->heap, txt2, out))
		reduce_throw(rc);
}

/* Args for "name2gtype".
 */
static BuiltinTypeSpot *name2gtype_args[] = {
	&string_spot
};

/* Do "name2gtype".
 */
static void
apply_name2gtype_call(Reduce *rc, const char *name,
	HeapNode **arg, PElement *out)
{
	PElement rhs;
	char txt[VIPS_PATH_MAX];
	int gtype;

	PEPOINTRIGHT(arg[0], &rhs);
	(void) reduce_get_string(rc, &rhs, txt, VIPS_PATH_MAX);

	gtype = g_type_from_name(txt);

	if (!heap_real_new(rc->heap, gtype, out))
		reduce_throw(rc);
}

/* Args for "gtype2name".
 */
static BuiltinTypeSpot *gtype2name_args[] = {
	&real_spot
};

/* Do "gtype2name".
 */
static void
apply_gtype2name_call(Reduce *rc, const char *name,
	HeapNode **arg, PElement *out)
{
	PElement rhs;
	int gtype;

	PEPOINTRIGHT(arg[0], &rhs);
	gtype = PEGETREAL(&rhs);

	if (!heap_managedstring_new(rc->heap, g_type_name(gtype), out))
		reduce_throw(rc);
}

/* Args for "vips_object_new".
 */
static BuiltinTypeSpot *vo_new_args[] = {
	&string_spot,
	&list_spot,
	&list_spot
};

/* Do a vips_object_new call.
 */
static void
apply_vo_new_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char buf[256];
	PElement required;
	PElement optional;

	PEPOINTRIGHT(arg[2], &rhs);
	reduce_get_string(rc, &rhs, buf, 256);
	PEPOINTRIGHT(arg[1], &required);
	PEPOINTRIGHT(arg[0], &optional);

	vo_object_new(rc, out, buf, &required, &optional);
}

/* Args for "vips_call".
 */
static BuiltinTypeSpot *vo_call_args[] = {
	&string_spot,
	&list_spot,
	&list_spot
};

/* Do a vips_call call.
 */
static void
apply_vo_call_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char buf[256];
	PElement required;
	PElement optional;

	PEPOINTRIGHT(arg[2], &rhs);
	reduce_get_string(rc, &rhs, buf, 256);
	PEPOINTRIGHT(arg[1], &required);
	PEPOINTRIGHT(arg[0], &optional);

	vo_call(rc, out, buf, &required, &optional);
}

/* Do a vips_call call, nip9 style.
 */
static void
apply_vo_call9_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	PElement rhs;
	char buf[256];
	PElement required;
	PElement optional;

	PEPOINTRIGHT(arg[2], &rhs);
	reduce_get_string(rc, &rhs, buf, 256);
	PEPOINTRIGHT(arg[1], &required);
	PEPOINTRIGHT(arg[0], &optional);

	vo_call9(rc, out, buf, &required, &optional);
}

/* Args for "copy_set_meta".
 */
static BuiltinTypeSpot *vo_copy_set_meta_args[] = {
	&vimage_spot,
	&string_spot,
	&any_spot
};

static void
apply_vo_copy_set_meta_call(Reduce *rc,
	const char *name, HeapNode **arg, PElement *out)
{
	// im_copy_set_meta in field value
	PElement image;
	PElement field;
	PElement value;

	PEPOINTRIGHT(arg[2], &image);
	PEPOINTRIGHT(arg[1], &field);
	PEPOINTRIGHT(arg[0], &value);

	Imageinfo *ii = reduce_get_image(rc, &image);

	char buf[256];
	reduce_get_string(rc, &field, buf, 256);

	GValue gvalue = { 0 };
	if (!heap_ip_to_gvalue(&value, &gvalue))
		reduce_throw(rc);

	VipsImage *x;
	if (vips_copy(ii->image, &x, NULL)) {
		g_value_unset(&gvalue);
		error_vips_all();
		reduce_throw(rc);
	}

	vips_image_set(x, buf, &gvalue);

	g_value_unset(&gvalue);

	if (!(ii = imageinfo_new(main_imageinfogroup, rc->heap, x, NULL))) {
		VIPS_UNREF(x);
		reduce_throw(rc);
	}

	PEPUTP(out, ELEMENT_MANAGED, ii);
}

/* All ip's builtin functions.
 */
static BuiltinInfo builtin_table[] = {
	/* Other.
	 */
	{ "dir", N_("return list of names of members"),
		FALSE, VIPS_NUMBER(dir_args),
		&dir_args[0], &apply_dir_call },
	{ "search", N_("search for file"),
		FALSE, VIPS_NUMBER(search_args),
		&search_args[0], &apply_search_call },
	{ "error", N_("raise error"),
		FALSE, VIPS_NUMBER(error_args),
		&error_args[0], &apply_error_call },
	{ "print", N_("convert to [char]"),
		FALSE, VIPS_NUMBER(print_args),
		&print_args[0], &apply_print_call },
	{ "expand", N_("expand environment variables"),
		FALSE, VIPS_NUMBER(expand_args),
		&expand_args[0], &apply_expand_call },
	{ "name2gtype", N_("convert [char] to GType"),
		FALSE, VIPS_NUMBER(name2gtype_args),
		&name2gtype_args[0], &apply_name2gtype_call },
	{ "gtype2name", N_("convert GType to [char]"),
		FALSE, VIPS_NUMBER(gtype2name_args),
		&gtype2name_args[0], &apply_gtype2name_call },
	{ "_", N_("look up localised string"),
		FALSE, VIPS_NUMBER(underscore_args),
		&underscore_args[0], &apply_underscore_call },

	/* vips8 wrapper.
	 */
	{ "vips_object_new", N_("create new vips8 object"),
		FALSE, VIPS_NUMBER(vo_new_args),
		&vo_new_args[0], apply_vo_new_call },
	{ "vips_call", N_("call vips8 operator"),
		FALSE, VIPS_NUMBER(vo_call_args),
		&vo_call_args[0], apply_vo_call_call },
	{ "vips_call9", N_("call vips8 operator, nip9 style"),
		FALSE, VIPS_NUMBER(vo_call_args),
		&vo_call_args[0], apply_vo_call9_call },

	/* compat
	 */
	{ "im_copy_set_meta", N_("copy, setting a metadata item"),
		FALSE, VIPS_NUMBER(vo_copy_set_meta_args),
		&vo_copy_set_meta_args[0], apply_vo_copy_set_meta_call },

	/* Predicates.
	 */
	{ "is_image", N_("true if argument is primitive image"),
		FALSE, VIPS_NUMBER(pred_args),
		&pred_args[0], apply_pred_call },
	{ "is_bool", N_("true if argument is primitive bool"),
		FALSE, VIPS_NUMBER(pred_args),
		&pred_args[0], apply_pred_call },
	{ "is_real", N_("true if argument is primitive real number"),
		FALSE, VIPS_NUMBER(pred_args),
		&pred_args[0], apply_pred_call },
	{ "is_class", N_("true if argument is class"),
		FALSE, VIPS_NUMBER(pred_args),
		&pred_args[0], apply_pred_call },
	{ "is_char", N_("true if argument is primitive char"),
		FALSE, VIPS_NUMBER(pred_args),
		&pred_args[0], apply_pred_call },
	{ "is_list", N_("true if argument is primitive list"),
		FALSE, VIPS_NUMBER(pred_args),
		&pred_args[0], apply_pred_call },
	{ "is_complex", N_("true if argument is primitive complex"),
		FALSE, VIPS_NUMBER(pred_args),
		&pred_args[0], apply_pred_call },
	{ "is_instanceof", N_("true if argument class instance of type"),
		FALSE, VIPS_NUMBER(is_instanceof_args),
		&is_instanceof_args[0], apply_is_instanceof_call },
	{ "has_member", N_("true if class has named member"),
		FALSE, VIPS_NUMBER(has_member_args),
		&has_member_args[0], apply_has_member_call },

	/* List and complex projections.
	 */
	{ "re", N_("real part of complex"),
		TRUE, VIPS_NUMBER(complex_args),
		&complex_args[0], apply_complex_call },
	{ "im", N_("imaginary part of complex"),
		TRUE, VIPS_NUMBER(complex_args),
		&complex_args[0], apply_complex_call },
	{ "hd", N_("head of list"),
		TRUE, VIPS_NUMBER(flist_args),
		&flist_args[0], apply_list_call },
	{ "tl", N_("tail of list"),
		TRUE, VIPS_NUMBER(flist_args),
		&flist_args[0], apply_list_call },

	/* Math.
	 */
	{ "sin", N_("sine of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "cos", N_("cosine of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "tan", N_("tangent of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "asin", N_("arc sine of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "acos", N_("arc cosine of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "atan", N_("arc tangent of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "log", N_("log base e of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "log10", N_("log base 10 of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "exp", N_("e to the power of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "exp10", N_("10 to the power of real number"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "ceil", N_("real to int, rounding up"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },
	{ "floor", N_("real to int, rounding down"),
		TRUE, VIPS_NUMBER(math_args),
		&math_args[0], apply_math_call },

	/* GSL funcs.
	 */
	{ "gammq", N_("gamma function"),
		TRUE, VIPS_NUMBER(gammq_args),
		&gammq_args[0], apply_gammq_call },

	/* Constructors.
	 */
	{ "vips_image", N_("load vips image"),
		FALSE, VIPS_NUMBER(image_args),
		&image_args[0], apply_image_call },
	{ "vips_image_from_matrix", N_("new image from a matrix"),
		FALSE, VIPS_NUMBER(matrix2image_args),
		&matrix2image_args[0], apply_matrix2image_call },

	{ "vips_matrix_from_image", N_("new matrix from an image"),
		FALSE, VIPS_NUMBER(image2matrix_args),
		&image2matrix_args[0], apply_image2matrix_call },

	{ "read", N_("load text file"),
		FALSE, VIPS_NUMBER(read_args),
		&read_args[0], apply_read_call },
	{ "graph_export_image", N_("generate image from Plot object"),
		FALSE, VIPS_NUMBER(graph_export_image_args),
		&graph_export_image_args[0], apply_graph_export_image_call },

	{ "vips_header_typeof", N_("get header field type"),
		FALSE, VIPS_NUMBER(header_get_typeof_args),
		&header_get_typeof_args[0], apply_header_get_type_call },
	{ "vips_header_int", N_("get int valued field"),
		FALSE, VIPS_NUMBER(header_get_typeof_args),
		&header_get_typeof_args[0], apply_header_int_call },
	{ "vips_header_double", N_("get double valued field"),
		FALSE, VIPS_NUMBER(header_get_typeof_args),
		&header_get_typeof_args[0], apply_header_double_call },
	{ "vips_header_string", N_("get string valued field"),
		FALSE, VIPS_NUMBER(header_get_typeof_args),
		&header_get_typeof_args[0], apply_header_string_call },

};

static void
builtin_gsl_error(const char *reason, const char *file,
	int line, int gsl_errno)
{
	error_top(_("GSL library error"));
	error_sub("%s - (%s:%d) - %s",
		reason, file, line, gsl_strerror(gsl_errno));

	reduce_throw(reduce_context);
}

void
builtin_init(void)
{
	Toolkit *kit;
	int i;

	/* Make the _builtin toolkit and populate.
	 */
	kit = toolkit_new(main_toolkitgroup, "_builtin");

	for (i = 0; i < VIPS_NUMBER(builtin_table); i++) {
		Symbol *sym;

		sym = symbol_new(symbol_root->expr->compile,
			builtin_table[i].name);
		g_assert(sym->type == SYM_ZOMBIE);
		sym->type = SYM_BUILTIN;
		sym->builtin = &builtin_table[i];
		(void) tool_new_sym(kit, -1, sym);
		symbol_made(sym);
	}

	filemodel_set_auto_load(FILEMODEL(kit));
	filemodel_set_modified(FILEMODEL(kit), FALSE);
	kit->pseudo = TRUE;

	gsl_set_error_handler(builtin_gsl_error);
}

/* Make a usage error.
 */
void
builtin_usage(VipsBuf *buf, BuiltinInfo *builtin)
{
	int i;

	vips_buf_appendf(buf,
		ngettext("Builtin \"%s\" takes %d argument.",
			"Builtin \"%s\" takes %d arguments.",
			builtin->nargs),
		builtin->name, builtin->nargs);
	vips_buf_appends(buf, "\n");

	for (i = 0; i < builtin->nargs; i++)
		vips_buf_appendf(buf, "    %d - %s\n",
			i + 1,
			builtin->args[i]->name);
}

#ifdef DEBUG
static void
builtin_trace_args(Heap *heap, const char *name, int n, HeapNode **arg)
{
	int i;
	char txt[100];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	for (i = 0; i < n; i++) {
		PElement t;

		PEPOINTRIGHT(arg[n - i - 1], &t);
		vips_buf_appends(&buf, "(");
		graph_pelement(heap, &buf, &t, FALSE);
		vips_buf_appends(&buf, ") ");
	}

	printf("builtin: %s %s\n", name, vips_buf_all(&buf));
}
#endif /*DEBUG*/

/* Execute the internal implementation of a builtin function.
 */
void
builtin_run(Reduce *rc, Compile *compile,
	int op, const char *name, HeapNode **arg, PElement *out,
	BuiltinInfo *builtin)
{
	int i;

	/* Typecheck args.
	 */
	for (i = 0; i < builtin->nargs; i++) {
		BuiltinTypeSpot *ts = builtin->args[i];
		PElement base;

		PEPOINTRIGHT(arg[builtin->nargs - i - 1], &base);
		if (!ts->pred(rc, &base)) {
			char txt[100];
			VipsBuf buf = VIPS_BUF_STATIC(txt);

			itext_value_ev(rc, &buf, &base);
			error_top(_("Bad argument"));
			error_sub(_("argument %d to builtin \"%s\" should "
						"be \"%s\", you passed:\n  %s"),
				i + 1, name, ts->name,
				vips_buf_all(&buf));
			reduce_throw(rc);
		}
	}

#ifdef DEBUG
	builtin_trace_args(rc->heap, name, builtin->nargs, arg);
#endif /*DEBUG*/

	builtin->fn(rc, name, arg, out);
}
