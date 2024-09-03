/* actions on the graph
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

/* Index with binop or uop.
 */
const char *operator_table[] = {
	"none",				   /* BI_NONE */
	"add",				   /* BI_ADD */
	"subtract",			   /* BI_SUB */
	"remainder",		   /* BI_REM */
	"power",			   /* BI_POW */
	"subscript",		   /* BI_SELECT */
	"left_shift",		   /* BI_LSHIFT */
	"right_shift",		   /* BI_RSHIFT */
	"divide",			   /* BI_DIV */
	"join",				   /* BI_JOIN */
	"dot",				   /* BI_DOT */
	"comma",			   /* BI_COMMA */
	"multiply",			   /* BI_MUL */
	"logical_and",		   /* BI_LAND */
	"logical_or",		   /* BI_LOR */
	"bitwise_and",		   /* BI_BAND */
	"bitwise_or",		   /* BI_BOR */
	"eor",				   /* BI_EOR */
	"equal",			   /* BI_EQ */
	"not_equal",		   /* BI_NOTEQ */
	"pointer_equal",	   /* BI_PEQ */
	"pointer_not_equal",   /* BI_PNOTEQ */
	"less",				   /* BI_LESS */
	"less_equal",		   /* BI_LESSEQ */
	"none",				   /* BI_MORE */
	"none",				   /* BI_MOREEQ */
	"if_then_else",		   /* BI_IF */
	"cons",				   /* BI_CONS */
	"none",				   /* UN_NONE */
	"cast_signed_char",	   /* UN_CSCHAR */
	"cast_unsigned_char",  /* UN_CUCHAR */
	"cast_signed_short",   /* UN_CSSHORT */
	"cast_unsigned_short", /* UN_CUSHORT */
	"cast_signed_int",	   /* UN_CSINT */
	"cast_unsigned_int",   /* UN_CUINT */
	"cast_float",		   /* UN_CFLOAT */
	"cast_double",		   /* UN_CDOUBLE */
	"cast_complex",		   /* UN_CCOMPLEX */
	"cast_double_complex", /* UN_CDCOMPLEX */
	"unary_minus",		   /* UN_MINUS */
	"negate",			   /* UN_NEG */
	"complement",		   /* UN_COMPLEMENT */
	"unary_plus"		   /* UN_PLUS */
};
const int noperator_table = VIPS_NUMBER(operator_table);

/* Bad bop error.
 */
static void
action_boperror(Reduce *rc, Compile *compile, const char *str,
	int op, const char *name, PElement *a, PElement *b)
{
	const char *top_str = str ? str : _("Bad arguments");
	const char *op_name = op >= 0 ? decode_BinOp(op) : name;

	char txt[MAX_ERROR_FRAG];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	char txt2[MAX_ERROR_FRAG];
	VipsBuf buf2 = VIPS_BUF_STATIC(txt2);
	char txt3[MAX_ERROR_FRAG];
	VipsBuf buf3 = VIPS_BUF_STATIC(txt3);

	itext_value_ev(rc, &buf, a);
	itext_value_ev(rc, &buf2, b);
	if (compile) {
		/* Expands to eg. 'bad args to "+", called from "fred"'
		 */
		vips_buf_appends(&buf3, _("called from"));
		vips_buf_appends(&buf3, " ");
		compile_name(compile, &buf3);
	}

	error_top("%s", top_str);
	error_sub(_("error in binary %s\n"
				"left = %s\n"
				"right = %s\n%s"),
		op_name,
		vips_buf_all(&buf),
		vips_buf_all(&buf2),
		vips_buf_all(&buf3));

	reduce_throw(rc);
}

/* Member not found in class instance error.
 */
static void
action_nomerror(Reduce *rc, Compile *compile, PElement *a, PElement *b)
{
	char txt[500];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	char txt2[MAX_ERROR_FRAG];
	VipsBuf buf2 = VIPS_BUF_STATIC(txt2);
	char txt3[MAX_ERROR_FRAG];
	VipsBuf buf3 = VIPS_BUF_STATIC(txt3);

	if (PEISCLASS(a))
		symbol_qualified_name(PEGETCLASSCOMPILE(a)->sym, &buf3);
	else if (PEISSYMREF(a))
		symbol_qualified_name(PEGETSYMREF(a), &buf3);
	else if (PEISSYMBOL(a))
		symbol_qualified_name(PEGETSYMBOL(a), &buf3);
	else if (PEISCOMPILEREF(a))
		symbol_qualified_name(PEGETCOMPILE(a)->sym, &buf3);
	else
		vips_buf_appends(&buf3, "<thing>");
	itext_value_ev(rc, &buf2, b);
	vips_buf_appendf(&buf, _("member \"%s\" not found in class \"%s\""),
		vips_buf_all(&buf2), vips_buf_all(&buf3));
	vips_buf_appendf(&buf, "\n");

	vips_buf_rewind(&buf3);
	itext_value_ev(rc, &buf3, a);
	vips_buf_appendf(&buf, "  ");
	vips_buf_appendf(&buf, _("object = %s"), vips_buf_all(&buf3));
	vips_buf_appendf(&buf, "\n");

	vips_buf_appendf(&buf, "  ");
	vips_buf_appendf(&buf, _("tag = %s"), vips_buf_all(&buf2));
	vips_buf_appendf(&buf, "\n");

	vips_buf_rewind(&buf3);
	symbol_qualified_name(compile->sym, &buf3);
	vips_buf_appendf(&buf, _("Reference attempted in \"%s\"."),
		vips_buf_all(&buf3));
	vips_buf_appendf(&buf, "\n");

	error_top(_("Member not found"));
	error_sub("%s", vips_buf_all(&buf));
	reduce_throw(rc);
}

/* Bad uop error.
 */
static void
action_uoperror(Reduce *rc, Compile *compile,
	const char *str, int op, const char *name, PElement *a)
{
	const char *top_str = str ? str : _("Bad argument");
	const char *op_name = op >= 0 ? decode_UnOp(op) : name;

	char txt[MAX_ERROR_FRAG];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	char txt2[MAX_ERROR_FRAG];
	VipsBuf buf2 = VIPS_BUF_STATIC(txt2);

	itext_value_ev(rc, &buf, a);

	if (compile) {
		/* Expands to eg. 'bad args to "+", called from "fred"'
		 */
		vips_buf_appends(&buf2, _("called from"));
		vips_buf_appends(&buf2, " ");
		compile_name(compile, &buf2);
	}

	error_top("%s", top_str);
	error_sub(_("error in unary %s\n"
				"argument = %s\n%s"),
		op_name, vips_buf_all(&buf), vips_buf_all(&buf2));

	reduce_throw(rc);
}

/* Clip real part of number a to a range.
 */
static void
action_set_range(Reduce *rc, double mn, double mx, PElement *a, PElement *out)
{
	Heap *heap = rc->heap;
	double d;

	/* Get real part.
	 */
	if (PEISREAL(a))
		d = PEGETREAL(a);
	else
		d = PEGETREALPART(a);

	if (d < mn)
		d = mn;
	else if (d > mx)
		d = mx;
	else
		d = (int) d;

	if (!heap_real_new(heap, d, out))
		reduce_throw(rc);
}

/* bitwise EOR two things.
 */
static void
action_proc_eor(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISBOOL(a) && PEISBOOL(b))
		PEPUTP(out, ELEMENT_BOOL, PEGETBOOL(a) ^ PEGETBOOL(b));
	else if (PEISINT(a) && PEISINT(b)) {
		// a and b any combination of bool, real, char
		int v1 = PEGETINT(a);
		int v2 = PEGETINT(b);

		if (!heap_real_new(heap, v1 ^ v2, out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "boolean",
			PEGETIMAGE(a), PEGETIMAGE(b), VIPS_OPERATION_BOOLEAN_EOR);
	else if (PEISIMAGE(a) && PEISINT(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETINT(b));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(a), VIPS_OPERATION_BOOLEAN_EOR, c);
	}
	else if (PEISINT(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETINT(a));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(b), VIPS_OPERATION_BOOLEAN_EOR, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* bitwise OR two things.
 */
static void
action_proc_bor(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISBOOL(a) && PEISBOOL(b))
		PEPUTP(out, ELEMENT_BOOL, PEGETBOOL(a) | PEGETBOOL(b));
	else if (PEISINT(a) && PEISINT(b)) {
		int v1 = PEGETINT(a);
		int v2 = PEGETINT(b);

		if (!heap_real_new(heap, v1 | v2, out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "boolean",
			PEGETIMAGE(a), PEGETIMAGE(b), VIPS_OPERATION_BOOLEAN_OR);
	else if (PEISIMAGE(a) && PEISINT(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETINT(b));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(a), VIPS_OPERATION_BOOLEAN_OR, c);
	}
	else if (PEISINT(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETINT(a));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(b), VIPS_OPERATION_BOOLEAN_OR, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* bitwise AND two things.
 */
static void
action_proc_band(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISBOOL(a) && PEISBOOL(b))
		PEPUTP(out, ELEMENT_BOOL, PEGETBOOL(a) & PEGETBOOL(b));
	if (PEISINT(a) && PEISINT(b)) {
		int v1 = PEGETINT(a);
		int v2 = PEGETINT(b);

		if (!heap_real_new(heap, v1 & v2, out))
			action_boperror(rc, compile, error_get_sub(),
				op, name, a, b);
	}
	if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "boolean",
			PEGETIMAGE(a), PEGETIMAGE(b), VIPS_OPERATION_BOOLEAN_AND);
	else if (PEISIMAGE(a) && PEISINT(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETINT(b));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(a), VIPS_OPERATION_BOOLEAN_AND, c);
	}
	else if (PEISINT(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETINT(a));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(b), VIPS_OPERATION_BOOLEAN_AND, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

static void *
action_proc_dot_add_link(Expr *expr, Symbol *child)
{
	return link_add(child, expr, TRUE);
}

static char *
action_proc_dot_tag(Reduce *rc, PElement *b, char *tag, int n)
{
	if (PEISTAG(b))
		return PEGETTAG(b);
	else if (reduce_is_string(rc, b)) {
		(void) reduce_get_string(rc, b, tag, n);
		return tag;
	}

	return NULL;
}

/* Extract field from object. Be careful, a can be equal to out.
 */
static void
action_proc_dot(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	char tag[256];
	char *p;

	if (PEISCLASS(a)) {
		PElement c;

		if ((p = action_proc_dot_tag(rc, b, tag, 256))) {
			if (!class_get_member(a, p, NULL, &c))
				action_nomerror(rc, compile, a, b);

			PEPUTPE(out, &c);
		}
		else if (PEISSYMREF(b)) {
			if (!class_get_symbol(a, PEGETSYMREF(b), &c))
				action_nomerror(rc, compile, a, b);

			PEPUTPE(out, &c);
		}
		else
			action_boperror(rc, compile,
				_("bad right hand side of '.'"), op, name, a, b);
	}

	else if (PEISSYMREF(a)) {
		Symbol *sym = PEGETSYMREF(a);
		Symbol *child;

		if (!is_scope(sym))
			action_boperror(rc, compile,
				_("symbol on left hand side of '.' is not scope"),
				op, name, a, b);

		g_assert(sym->expr);

		if (!(p = action_proc_dot_tag(rc, b, tag, 256)))
			action_boperror(rc, compile,
				_("bad right hand side of '.'"), op, name, a, b);
		if (!(child = compile_lookup(sym->expr->compile, p)))
			action_nomerror(rc, compile, a, b);

		/* Add all exprs which use compile to dynamic link graph.
		 */
		if (slist_map(compile->exprs,
				(SListMapFn) action_proc_dot_add_link, child))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);

		/* Don't check for dirty here ... wait for
		 * link.
		 */
		if (child->type == SYM_VALUE)
			PEPUTP(out, ELEMENT_SYMBOL, child);
		else
			PEPUTP(out, ELEMENT_SYMREF, child);
	}
	else if (PEISMANAGEDGOBJECT(a)) {
		GObject *gobject = PEGETMANAGEDGOBJECT(a);
		GObjectClass *gclass = G_OBJECT_GET_CLASS(gobject);
		GValue value = { 0 };
		GParamSpec *pspec;

		if (!(p = action_proc_dot_tag(rc, b, tag, 256)))
			action_boperror(rc, compile,
				_("bad right hand side of '.'"), op, name, a, b);
		if (!(pspec = g_object_class_find_property(gclass, p)))
			action_boperror(rc, compile,
				_("property not found"), op, name, a, b);

		g_value_init(&value, G_PARAM_SPEC_VALUE_TYPE(pspec));
		g_object_get_property(gobject, p, &value);

		if (!heap_gvalue_to_ip(&value, out)) {
			g_value_unset(&value);
			reduce_throw(rc);
		}

		g_value_unset(&value);
	}
	else
		action_boperror(rc, compile,
			_("bad left hand side of '.'"), op, name, a, b);
}

/* Less than or equal to.
 */
static void
action_proc_lesseq(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	if (PEISREAL(a) && PEISREAL(b)) {
		PEPUTP(out, ELEMENT_BOOL, PEGETREAL(a) <= PEGETREAL(b));
	}
	else if (PEISINT(a) && PEISINT(b)) {
		PEPUTP(out, ELEMENT_BOOL, PEGETINT(a) <= PEGETINT(b));
	}
	else if (PEISLIST(a) && PEISLIST(b) &&
		reduce_is_string(rc, a) && reduce_is_string(rc, b)) {
		char a_string[MAX_STRSIZE];
		char b_string[MAX_STRSIZE];

		reduce_get_string(rc, a, a_string, MAX_STRSIZE);
		reduce_get_string(rc, b, b_string, MAX_STRSIZE);

		PEPUTP(out, ELEMENT_BOOL, strcmp(a_string, b_string) <= 0);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "relational",
			PEGETIMAGE(a), PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_LESSEQ);
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "relational_const",
			PEGETIMAGE(a), VIPS_OPERATION_RELATIONAL_LESSEQ, c);
	}
	else if (PEISREAL(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(a));

		vo_callva(rc, out, "relational_const",
			PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_MOREEQ, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Strict less than.
 */
static void
action_proc_less(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	if (PEISREAL(a) && PEISREAL(b)) {
		PEPUTP(out, ELEMENT_BOOL, PEGETREAL(a) < PEGETREAL(b));
	}
	else if (PEISINT(a) && PEISINT(b)) {
		PEPUTP(out, ELEMENT_BOOL, PEGETINT(a) < PEGETINT(b));
	}
	else if (PEISLIST(a) && PEISLIST(b) &&
		reduce_is_string(rc, a) && reduce_is_string(rc, b)) {
		char a_string[MAX_STRSIZE];
		char b_string[MAX_STRSIZE];

		reduce_get_string(rc, a, a_string, MAX_STRSIZE);
		reduce_get_string(rc, b, b_string, MAX_STRSIZE);

		PEPUTP(out, ELEMENT_BOOL, strcmp(a_string, b_string) < 0);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "relational",
			PEGETIMAGE(a), PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_LESS);
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "relational_const",
			PEGETIMAGE(a), VIPS_OPERATION_RELATIONAL_LESS, c);
	}
	else if (PEISREAL(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(a));

		vo_callva(rc, out, "relational_const",
			PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_MORE, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Forward ref.
 */
static gboolean action_element_equal(Reduce *rc, PElement *a, PElement *b);

/* Test two nodes for equality.
 */
static gboolean
action_node_equal(Reduce *rc, HeapNode *a, HeapNode *b)
{
	PElement la, ra;
	PElement lb, rb;

	/* Easy!
	 */
	if (a->type != b->type)
		return FALSE;

	switch (a->type) {
	case TAG_APPL:
	case TAG_CLASS:
		/* Function compare ... don't allow it.
		 */
		return FALSE;

	case TAG_CONS:
		/* Compare the elements.
		 */
		PEPOINTLEFT(a, &la);
		PEPOINTLEFT(b, &lb);
		PEPOINTRIGHT(a, &ra);
		PEPOINTRIGHT(b, &rb);
		return action_element_equal(rc, &la, &lb) &&
			action_element_equal(rc, &ra, &rb);

	case TAG_DOUBLE:
		return a->body.num == b->body.num;

	case TAG_COMPLEX:
		return GETLEFT(a)->body.num == GETLEFT(b)->body.num &&
			GETRIGHT(a)->body.num == GETRIGHT(b)->body.num;

	case TAG_GEN:
	case TAG_FILE:
	case TAG_FREE:
	case TAG_SHARED:
	case TAG_REFERENCE:
	default:
		g_assert(FALSE);

		/* Keep gcc happy.
		 */
		return FALSE;
	}
}

static gboolean
action_image_equal(Reduce *rc, Imageinfo *a, Imageinfo *b)
{
	double mn;

	/* Easy tests first.
	 */
	if (!a || !b)
		return FALSE;
	if (a == b)
		return TRUE;

	/* Size and bands must be the same.
	 */
	if (a->image->Xsize != b->image->Xsize ||
		a->image->Ysize != b->image->Ysize ||
		a->image->Bands != b->image->Bands ||
		a->image->Coding != b->image->Coding)
		return FALSE;

	/* Exhaustive test.
	 */
	g_autoptr(VipsImage) t1 = NULL;
	if (vips_equal(a->image, b->image, &t1, NULL) ||
		vips_min(t1, &mn, NULL)) {
		error_vips_all();
		reduce_throw(rc);
	}

	return mn == 255;
}

/* One of p1/p2 is a managedstring.
 *
 * This is pretty dumb. We could have a special loop down the list side which
 * compared to the managedstring directly, but I doubt if this will ever be a
 * performance issue.
 */
static gboolean
action_string_equal(Reduce *rc, PElement *p1, PElement *p2)
{
	char a_string[MAX_STRSIZE];
	char b_string[MAX_STRSIZE];

	reduce_get_string(rc, p1, a_string, MAX_STRSIZE);
	reduce_get_string(rc, p2, b_string, MAX_STRSIZE);

	return g_str_equal(a_string, b_string);
}

/* Test two elements for equality. Force computation as required.
 */
static gboolean
action_element_equal(Reduce *rc, PElement *p1, PElement *p2)
{
	/* Reduce a bit.
	 */
	reduce_spine(rc, p1);
	reduce_spine(rc, p2);

	/* We can often test for eg. "fred" == "fred" by just checking
	 * pointers.
	 */
	if (PEGETTYPE(p1) == PEGETTYPE(p2)) {
		switch (PEGETTYPE(p1)) {
		case ELEMENT_CHAR:
		case ELEMENT_NODE:
		case ELEMENT_BOOL:
		case ELEMENT_MANAGED:
			if (PEGETVAL(p1) == PEGETVAL(p2))
				return TRUE;
			break;

		case ELEMENT_ELIST:
			return TRUE;
		}
	}

	/* Special case if either is a managedstring.
	 */
	if (PEISMANAGEDSTRING(p1) ||
		PEISMANAGEDSTRING(p2))
		return action_string_equal(rc, p1, p2);

	/* No other implicit conversions, so types must match.
	 */
	if (PEGETTYPE(p1) != PEGETTYPE(p2))
		return FALSE;

	switch (PEGETTYPE(p1)) {
	case ELEMENT_TAG:
	case ELEMENT_BINOP:
	case ELEMENT_UNOP:
	case ELEMENT_SYMBOL:
	case ELEMENT_SYMREF:
	case ELEMENT_COMPILEREF:
	case ELEMENT_CONSTRUCTOR:
	case ELEMENT_COMB:
		/* Don't allow function compare.
		 */
		return FALSE;

	case ELEMENT_NODE:
		/* Compare the HeapNodes.
		 */
		return action_node_equal(rc, PEGETVAL(p1), PEGETVAL(p2));

	case ELEMENT_CHAR:
		return PEGETCHAR(p1) == PEGETCHAR(p2);

	case ELEMENT_BOOL:
		return PEGETBOOL(p1) == PEGETBOOL(p2);

	case ELEMENT_MANAGED:
		if (PEISIMAGE(p1) && PEISIMAGE(p2))
			return action_image_equal(rc, PEGETII(p1), PEGETII(p2));
		else
			return FALSE;

	case ELEMENT_ELIST:
		return TRUE;

	case ELEMENT_NOVAL:
	default:
		g_assert(FALSE);

		/* Keep gcc happy.
		 */
		return FALSE;
	}
}

/* Top-level == ... special form for image args.
 */
static void
action_proc_equal(Reduce *rc, Compile *compile,
	int op, const char *name, HeapNode **arg, PElement *out)
{
	gboolean res;
	PElement left, right;
	PElement *a = &left;
	PElement *b = &right;

	PEPOINTRIGHT(arg[1], &left);
	PEPOINTRIGHT(arg[0], &right);

	if (PEISIMAGE(a) || PEISIMAGE(b)) {
		/* Special image equals form. Hyperstrict.
		 */
		reduce_spine_strict(rc, a);
		reduce_spine_strict(rc, b);

		if (PEISIMAGE(a) && PEISIMAGE(b))
			vo_callva(rc, out, "relational",
				PEGETIMAGE(a), PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_EQUAL);
		else if (PEISIMAGE(a) && PEISREAL(b)) {
			g_autoptr(VipsArrayDouble) c =
				vips_array_double_newv(1, PEGETREAL(b));

			vo_callva(rc, out, "relational_const",
				PEGETIMAGE(a), VIPS_OPERATION_RELATIONAL_EQUAL, c);
		}
		else if (PEISREAL(a) && PEISIMAGE(b)) {
			g_autoptr(VipsArrayDouble) c =
				vips_array_double_newv(1, PEGETREAL(a));

			vo_callva(rc, out, "relational_const",
				PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_EQUAL, c);
		}
		else
			PEPUTP(out, ELEMENT_BOOL, FALSE);
	}
	else {
		/* Lazy form.
		 */
		res = action_element_equal(rc, a, b);
		PEPUTP(out, ELEMENT_BOOL, res);
	}
}

/* Top-level != ... special form for image args.
 */
static void
action_proc_notequal(Reduce *rc, Compile *compile,
	int op, const char *name, HeapNode **arg, PElement *out)
{
	gboolean res;
	PElement left, right;
	PElement *a = &left;
	PElement *b = &right;

	PEPOINTRIGHT(arg[1], &left);
	PEPOINTRIGHT(arg[0], &right);

	if (PEISIMAGE(a) || PEISIMAGE(b)) {
		/* Special image equals form. Hyperstrict.
		 */
		reduce_spine_strict(rc, a);
		reduce_spine_strict(rc, b);

		if (PEISIMAGE(a) && PEISIMAGE(b))
			vo_callva(rc, out, "relational",
				PEGETIMAGE(a), PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_NOTEQ);
		else if (PEISIMAGE(a) && PEISREAL(b)) {
			g_autoptr(VipsArrayDouble) c =
				vips_array_double_newv(1, PEGETREAL(b));

			vo_callva(rc, out, "relational_const",
				PEGETIMAGE(a), VIPS_OPERATION_RELATIONAL_NOTEQ, c);
		}
		else if (PEISREAL(a) && PEISIMAGE(b)) {
			g_autoptr(VipsArrayDouble) c =
				vips_array_double_newv(1, PEGETREAL(a));

			vo_callva(rc, out, "relational_const",
				PEGETIMAGE(b), VIPS_OPERATION_RELATIONAL_NOTEQ, c);
		}
		else
			PEPUTP(out, ELEMENT_BOOL, TRUE);
	}
	else {
		res = action_element_equal(rc, a, b);
		PEPUTP(out, ELEMENT_BOOL, !res);
	}
}

static void *
action_proc_join_sub(Reduce *rc, PElement *pe,
	PElement *a, PElement *b, PElement *out)
{
	if (!heap_list_cat(rc, a, b, pe))
		return a;

	PEPUTPE(out, pe);

	return NULL;
}

static void
action_proc_join(Reduce *rc, Compile *compile,
	int op, const char *name, HeapNode **arg, PElement *out)
{
	PElement left, right;
	PElement *a = &left;
	PElement *b = &right;

	PEPOINTRIGHT(arg[1], &left);
	PEPOINTRIGHT(arg[0], &right);

	if (PEISIMAGE(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayImage) c =
			vips_array_image_newv(2, PEGETIMAGE(a), PEGETIMAGE(b));

		vo_callva(rc, out, "bandjoin", c);
	}
	else if (PEISLIST(a) && PEISLIST(b)) {
		if (reduce_safe_pointer(rc,
				(reduce_safe_pointer_fn) action_proc_join_sub,
				a, b, out, NULL))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISELIST(b)) {
		PEPUTPE(out, a);
	}
	else if (PEISIMAGE(b) && PEISELIST(a)) {
		PEPUTPE(out, b);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

static void
action_proc_index(Reduce *rc, Compile *compile,
	int op, const char *name, HeapNode **arg, PElement *out)
{
	PElement left, right;
	PElement *a = &left;
	PElement *b = &right;

	PEPOINTRIGHT(arg[1], &left);
	PEPOINTRIGHT(arg[0], &right);

	if (PEISLIST(a) && PEISREAL(b)) {
		PElement result;

		reduce_list_index(rc, a, PEGETREAL(b), &result);
		PEPUTPE(out, &result);
	}
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		vo_callva(rc, out, "extract_band", PEGETIMAGE(a), (int) PEGETREAL(b));
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Raise to power.
 */
static void
action_proc_exp(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		if (!heap_real_new(heap,
				pow(PEGETREAL(a), PEGETREAL(b)), out))
			action_boperror(rc, compile, error_get_sub(),
				op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "math2_const",
			PEGETIMAGE(a), VIPS_OPERATION_MATH2_POW, c);
	}
	else if (PEISREAL(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(a));

		vo_callva(rc, out, "math2_const",
			PEGETIMAGE(b), VIPS_OPERATION_MATH2_WOP, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Left shift.
 */
static void
action_proc_lshift(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		int v1 = PEGETREAL(a);
		int v2 = PEGETREAL(b);

		if (!heap_real_new(heap, v1 << v2, out))
			action_boperror(rc, compile, error_get_sub(),
				op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(a), VIPS_OPERATION_BOOLEAN_LSHIFT, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Right shift.
 */
static void
action_proc_rshift(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		int v1 = PEGETREAL(a);
		int v2 = PEGETREAL(b);

		if (!heap_real_new(heap, v1 >> v2, out))
			action_boperror(rc, compile, error_get_sub(),
				op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "boolean_const",
			PEGETIMAGE(a), VIPS_OPERATION_BOOLEAN_RSHIFT, c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Remainder.
 */
static void
action_proc_rem(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		int v1 = PEGETREAL(a);
		int v2 = PEGETREAL(b);

		if (v2 == 0)
			action_boperror(rc, compile, _("division by zero"), op, name, a, b);

		if (!heap_real_new(heap, v1 % v2, out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "remainder", PEGETIMAGE(a), PEGETIMAGE(b));
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "remainder_const", PEGETIMAGE(a), c);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Divide two objects.
 */
static void
action_proc_div(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		if (!heap_real_new(heap, PEGETREAL(a) / PEGETREAL(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISCOMPLEX(b)) {
		double x1 = PEGETREALPART(a);
		double y1 = PEGETIMAGPART(a);
		double x2 = PEGETREALPART(b);
		double y2 = PEGETIMAGPART(b);

		if (!heap_complex_new(heap,
				(x1 * x2 + y1 * y2) / (x2 * x2 + y2 * y2),
				(y1 * x2 - x1 * y2) / (x2 * x2 + y2 * y2), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISREAL(b)) {
		double x1 = PEGETREALPART(a);
		double y1 = PEGETIMAGPART(a);
		double x2 = PEGETREAL(b);

		if (!heap_complex_new(heap,
				(x1 * x2) / (x2 * x2),
				(y1 * x2) / (x2 * x2), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISREAL(a) && PEISCOMPLEX(b)) {
		double x1 = PEGETREAL(a);
		double x2 = PEGETREALPART(b);
		double y2 = PEGETIMAGPART(b);

		if (!heap_complex_new(heap,
				(x1 * x2) / (x2 * x2 + y2 * y2),
				(-x1 * y2) / (x2 * x2 + y2 * y2), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "divide", PEGETIMAGE(a), PEGETIMAGE(b));
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) aa =
			vips_array_double_newv(1, 1.0 / PEGETREAL(b));
		g_autoptr(VipsArrayDouble) ab =
			vips_array_double_newv(1, 0.0);

		vo_callva(rc, out, "linear", PEGETIMAGE(a), aa, ab);
	}
	else if (PEISREAL(a) && PEISIMAGE(b)) {
		HeapNode hn;
		PElement rhs;

		/* Use this for intermediates.
		 */
		PEPOINTRIGHT(&hn, &rhs);

		/* Take recip.
		 */
		g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, -1.0);
		vo_callva(rc, &rhs, "math2_const",
			PEGETIMAGE(b), VIPS_OPERATION_MATH2_POW, c);

		/* Now multiply by const.
		 */
		g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, PEGETREAL(a));
		g_autoptr(VipsArrayDouble) ab = vips_array_double_newv(1, 0.0);
		vo_callva(rc, out, "linear", PEGETIMAGE(&rhs), aa, ab);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Multiply two objects.
 */
static void
action_proc_mul(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		if (!heap_real_new(heap, PEGETREAL(a) * PEGETREAL(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISCOMPLEX(b)) {
		double x1 = PEGETREALPART(a);
		double y1 = PEGETIMAGPART(a);
		double x2 = PEGETREALPART(b);
		double y2 = PEGETIMAGPART(b);

		if (!heap_complex_new(heap, x1 * x2 - y1 * y2, x1 * y2 + x2 * y1, out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISREAL(b)) {
		if (!heap_complex_new(heap,
				PEGETREALPART(a) * PEGETREAL(b),
				PEGETIMAGPART(a) * PEGETREAL(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISREAL(a) && PEISCOMPLEX(b)) {
		if (!heap_complex_new(heap,
				PEGETREAL(a) * PEGETREALPART(b),
				PEGETREAL(a) * PEGETIMAGPART(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "multiply", PEGETIMAGE(a), PEGETIMAGE(b));
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, PEGETREAL(b));
		g_autoptr(VipsArrayDouble) ab = vips_array_double_newv(1, 0.0);

		vo_callva(rc, out, "linear", PEGETIMAGE(a), aa, ab);
	}
	else if (PEISREAL(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, PEGETREAL(a));
		g_autoptr(VipsArrayDouble) ab = vips_array_double_newv(1, 0.0);

		vo_callva(rc, out, "linear", PEGETIMAGE(b), aa, ab);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Subtract two objects.
 */
static void
action_proc_sub(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		if (!heap_real_new(heap, PEGETREAL(a) - PEGETREAL(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISCOMPLEX(b)) {
		if (!heap_complex_new(heap,
				PEGETREALPART(a) - PEGETREALPART(b),
				PEGETIMAGPART(a) - PEGETIMAGPART(b),
				out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISREAL(b)) {
		if (!heap_complex_new(heap,
				PEGETREALPART(a) - PEGETREAL(b),
				PEGETIMAGPART(a), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISREAL(a) && PEISCOMPLEX(b)) {
		if (!heap_complex_new(heap,
				PEGETREAL(a) - PEGETREALPART(b),
				PEGETIMAGPART(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "subtract", PEGETIMAGE(a), PEGETIMAGE(b));
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, 1.0);
		g_autoptr(VipsArrayDouble) ab =
			vips_array_double_newv(1, -PEGETREAL(b));

		vo_callva(rc, out, "linear", PEGETIMAGE(a), aa, ab);
	}
	else if (PEISREAL(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, -1.0);
		g_autoptr(VipsArrayDouble) ab = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "linear", PEGETIMAGE(a), aa, ab);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Add two objects.
 */
static void
action_proc_add(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	Heap *heap = rc->heap;

	if (PEISREAL(a) && PEISREAL(b)) {
		if (!heap_real_new(heap, PEGETREAL(a) + PEGETREAL(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISCOMPLEX(b)) {
		if (!heap_complex_new(heap,
				PEGETREALPART(a) + PEGETREALPART(b),
				PEGETIMAGPART(a) + PEGETIMAGPART(b),
				out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISCOMPLEX(a) && PEISREAL(b)) {
		if (!heap_complex_new(heap,
				PEGETREALPART(a) + PEGETREAL(b),
				PEGETIMAGPART(a), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISREAL(a) && PEISCOMPLEX(b)) {
		if (!heap_complex_new(heap,
				PEGETREAL(a) + PEGETREALPART(b),
				PEGETIMAGPART(b), out))
			action_boperror(rc, compile, error_get_sub(), op, name, a, b);
	}
	else if (PEISIMAGE(a) && PEISIMAGE(b))
		vo_callva(rc, out, "add", PEGETIMAGE(a), PEGETIMAGE(b));
	else if (PEISIMAGE(a) && PEISREAL(b)) {
		g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, 1.0);
		g_autoptr(VipsArrayDouble) ab = vips_array_double_newv(1, PEGETREAL(b));

		vo_callva(rc, out, "linear", PEGETIMAGE(a), aa, ab);
	}
	else if (PEISREAL(a) && PEISIMAGE(b)) {
		g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, 1.0);
		g_autoptr(VipsArrayDouble) ab = vips_array_double_newv(1, PEGETREAL(a));

		vo_callva(rc, out, "linear", PEGETIMAGE(b), aa, ab);
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);
}

/* Evaluate a binary operator on args a and b, write the result to out. a and
 * b already reduced.
 *
 * Call one of the things above. Not all combinations implemented, got bored :/
 * Implement simple things in the switch, break to the functions above for
 * the rest.
 *
 * out can be rhs of argv[0], careful.
 */
static void
action_proc_bop_strict(Reduce *rc, Compile *compile,
	int op, const char *name, HeapNode **arg, PElement *out)
{
	Heap *heap = rc->heap;
	HeapNode *hn;
	PElement left, right;
	PElement *a = &left;
	PElement *b = &right;

	PEPOINTRIGHT(arg[1], &left);
	PEPOINTRIGHT(arg[0], &right);

	switch (op) {
	case BI_SELECT:
		action_proc_index(rc, compile, op, name, arg, out);
		break;

	case BI_JOIN:
		action_proc_join(rc, compile, op, name, arg, out);
		break;

	case BI_EQ:
		action_proc_equal(rc, compile, op, name, arg, out);
		break;

	case BI_NOTEQ:
		action_proc_notequal(rc, compile, op, name, arg, out);
		break;

	case BI_PEQ:
		PEPUTP(out, ELEMENT_BOOL,
			PEGETTYPE(a) == PEGETTYPE(b) && PEGETVAL(a) == PEGETVAL(b));
		break;

	case BI_PNOTEQ:
		PEPUTP(out, ELEMENT_BOOL,
			PEGETTYPE(a) != PEGETTYPE(b) || PEGETVAL(a) != PEGETVAL(b));
		break;

	case BI_ADD:
		action_proc_add(rc, compile, op, name, a, b, out);
		break;

	case BI_SUB:
		action_proc_sub(rc, compile, op, name, a, b, out);
		break;

	case BI_MUL:
		action_proc_mul(rc, compile, op, name, a, b, out);
		break;

	case BI_DIV:
		action_proc_div(rc, compile, op, name, a, b, out);
		break;

	case BI_DOT:
		action_proc_dot(rc, compile, op, name, a, b, out);
		break;

	case BI_POW:
		action_proc_exp(rc, compile, op, name, a, b, out);
		break;

	case BI_LSHIFT:
		action_proc_lshift(rc, compile, op, name, a, b, out);
		break;

	case BI_RSHIFT:
		action_proc_rshift(rc, compile, op, name, a, b, out);
		break;

	case BI_REM:
		action_proc_rem(rc, compile, op, name, a, b, out);
		break;

	case BI_LESS:
		action_proc_less(rc, compile, op, name, a, b, out);
		break;

	case BI_LESSEQ:
		action_proc_lesseq(rc, compile, op, name, a, b, out);
		break;

	case BI_COMMA:
		if (PEISREAL(a) && PEISREAL(b)) {
			if (NEWNODE(heap, hn))
				action_boperror(rc, compile, error_get_sub(), op, name, a, b);

			/* Form complex node.
			 */
			hn->type = TAG_COMPLEX;
			PPUT(hn,
				ELEMENT_NODE, PEGETVAL(a),
				ELEMENT_NODE, PEGETVAL(b));

			PEPUTP(out, ELEMENT_NODE, hn);
		}
		else if (PEISIMAGE(a) && PEISIMAGE(b))
			vo_callva(rc, out, "complexform", PEGETIMAGE(a), PEGETIMAGE(b));
		else
			action_boperror(rc, compile, NULL, op, name, a, b);

		break;

	case BI_BAND:
		action_proc_band(rc, compile, op, name, a, b, out);
		break;

	case BI_BOR:
		action_proc_bor(rc, compile, op, name, a, b, out);
		break;

	case BI_EOR:
		action_proc_eor(rc, compile, op, name, a, b, out);
		break;

	case BI_NONE:
	default:
		action_boperror(rc, compile, _("unimplemented"), op, name, a, b);
		break;
	}
}

/* Evaluate a unary operator on arg a, write the result to out. a
 * already reduced.
 */
void
action_proc_uop(Reduce *rc, Compile *compile,
	int op, const char *name, HeapNode **arg, PElement *out)
{
	Heap *heap = rc->heap;
	PElement pe, *a = &pe;
	PElement rhs;

	PEPOINTRIGHT(arg[0], &pe);

	switch (op) {
	case UN_NEG:
		if (PEISREAL(a)) {
			if (!heap_real_new(heap, !PEGETREAL(a), out))
				reduce_throw(rc);
		}
		else if (PEISCOMPLEX(a)) {
			if (!heap_complex_new(heap, !PEGETREALPART(a),
					!PEGETIMAGPART(a), out))
				reduce_throw(rc);
		}
		else if (PEISBOOL(a))
			PEPUTP(out, ELEMENT_BOOL, !PEGETBOOL(a));
		else if (PEISIMAGE(a)) {
			g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, 0.0);

			vo_callva(rc, out, "relational_const",
				PEGETIMAGE(a), VIPS_OPERATION_RELATIONAL_NOTEQ, c);
		}
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_MINUS:
		if (PEISREAL(a)) {
			if (!heap_real_new(heap, -PEGETREAL(a), out))
				reduce_throw(rc);
		}
		else if (PEISCOMPLEX(a)) {
			if (!heap_complex_new(heap, -PEGETREALPART(a),
					-PEGETIMAGPART(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a)) {
			g_autoptr(VipsArrayDouble) aa = vips_array_double_newv(1, -1.0);
			g_autoptr(VipsArrayDouble) ab = vips_array_double_newv(1, 0.0);

			vo_callva(rc, out, "linear", PEGETIMAGE(a), aa, ab);
		}
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_COMPLEMENT:
		if (PEISREAL(a)) {
			int v = PEGETREAL(a);

			if (!heap_real_new(heap, ~v, out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a)) {
			g_autoptr(VipsArrayDouble) c = vips_array_double_newv(1, -1.0);

			vo_callva(rc, out, "boolean_const",
				PEGETIMAGE(a), VIPS_OPERATION_BOOLEAN_EOR, c);
		}
		else
			action_uoperror(rc, compile, NULL, op, name, a);
		break;

	case UN_PLUS:
		PEPUTPE(out, a);
		break;

	case UN_CSCHAR:
		/* Convert to signed char.
		 */
		if (PEISNUM(a))
			action_set_range(rc, SCHAR_MIN, SCHAR_MAX, a, out);
		else if (PEISCHAR(a)) {
			if (!heap_real_new(heap, PEGETCHAR(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_UCHAR);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CUCHAR:
		/* Convert to unsigned char, eg. 65 => 'A'.
		 */
		if (PEISREAL(a)) {
			double v = PEGETREAL(a);

			v = VIPS_CLIP(0, v, UCHAR_MAX);

			PEPUTP(out, ELEMENT_CHAR, (int) v);
		}
		else if (PEISCOMPLEX(a)) {
			double v = PEGETREALPART(a);

			v = VIPS_CLIP(0, v, UCHAR_MAX);

			PEPUTP(out, ELEMENT_CHAR, (int) v);
		}
		else if (PEISCHAR(a))
			PEPUTPE(out, a);
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_UCHAR);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CSINT:
		/* Convert to signed int.
		 */
		if (PEISNUM(a))
			action_set_range(rc, INT_MIN, INT_MAX, a, out);
		else if (PEISCHAR(a)) {
			if (!heap_real_new(heap, PEGETCHAR(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_INT);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CUINT:
		/* Convert to unsigned int.
		 */
		if (PEISREAL(a) || PEISCOMPLEX(a))
			action_set_range(rc, 0, UINT_MAX, a, out);
		else if (PEISCHAR(a)) {
			if (!heap_real_new(heap, PEGETCHAR(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_UINT);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CSSHORT:
		/* Convert to signed short.
		 */
		if (PEISREAL(a) || PEISCOMPLEX(a))
			action_set_range(rc, SHRT_MIN, SHRT_MAX, a, out);
		else if (PEISCHAR(a)) {
			if (!heap_real_new(heap, PEGETCHAR(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_SHORT);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CUSHORT:
		/* Convert to unsigned short.
		 */
		if (PEISREAL(a) || PEISCOMPLEX(a))
			action_set_range(rc, 0, USHRT_MAX, a, out);
		else if (PEISCHAR(a)) {
			if (!heap_real_new(heap, PEGETCHAR(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_USHORT);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CFLOAT:
		/* Convert to float ... just drop imag part.
		 */
		if (PEISCOMPLEX(a)) {
			if (!heap_real_new(heap, PEGETREALPART(a), out))
				reduce_throw(rc);
		}
		else if (PEISREAL(a))
			PEPUTPE(out, a);
		else if (PEISCHAR(a)) {
			if (!heap_real_new(heap, PEGETCHAR(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_FLOAT);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CDOUBLE:
		/* Convert to double ... just drop imag part.
		 */
		if (PEISCOMPLEX(a)) {
			if (!heap_real_new(heap, PEGETREALPART(a), out))
				reduce_throw(rc);
		}
		else if (PEISREAL(a))
			PEPUTPE(out, a);
		else if (PEISCHAR(a)) {
			if (!heap_real_new(heap, PEGETCHAR(a), out))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a), VIPS_FORMAT_DOUBLE);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_CDCOMPLEX:
	case UN_CCOMPLEX:
		/* Convert to complex ... set imag = 0.
		 */
		if (PEISREAL(a)) {
			/* Make base node.
			 */
			if (!heap_complex_element_new(heap, a, a, out))
				reduce_throw(rc);

			/* Install new imag part.
			 */
			PEPOINTRIGHT(PEGETVAL(out), &rhs);
			if (!heap_real_new(heap, 0, &rhs))
				reduce_throw(rc);
		}
		else if (PEISCOMPLEX(a))
			PEPUTPE(out, a);
		else if (PEISCHAR(a)) {
			/* Make base node.
			 */
			if (!heap_complex_element_new(heap, a, a, out))
				reduce_throw(rc);

			/* Install new real and imag parts.
			 */
			PEPOINTLEFT(PEGETVAL(out), &rhs);
			if (!heap_real_new(heap, PEGETCHAR(a), &rhs))
				reduce_throw(rc);
			PEPOINTRIGHT(PEGETVAL(out), &rhs);
			if (!heap_real_new(heap, 0, &rhs))
				reduce_throw(rc);
		}
		else if (PEISIMAGE(a))
			vo_callva(rc, out, "cast", PEGETIMAGE(a),
				op == UN_CCOMPLEX ? VIPS_FORMAT_COMPLEX : VIPS_FORMAT_DPCOMPLEX);
		else
			action_uoperror(rc, compile, NULL, op, name, a);

		break;

	case UN_NONE:
	default:
		action_uoperror(rc, compile,
			_("Unimplemented."), op, name, a);
		break;
	}
}

static void *
action_proc_construct_sub(Reduce *rc, PElement *pe,
	Compile *compile, HeapNode **arg, PElement *out)
{
	if (!class_new(rc->heap, compile, arg, pe))
		reduce_throw(rc);

	PEPUTPE(out, pe);

	return NULL;
}

/* Eval a constructor. Nasty: out can be RHS of arg[0], so we have to build
 * instance in a safe spot, then write back to out afterwards.
 */
void
action_proc_construct(Reduce *rc,
	Compile *compile, HeapNode **arg, PElement *out)
{
	if (trace_flags & TRACE_CLASS_NEW) {
		VipsBuf *buf = trace_push();

		vips_buf_appendf(buf, "constructor \"%s\" ",
			IOBJECT(compile->sym)->name);
		trace_args(arg, compile->nparam + compile->nsecret);
	}

	if (reduce_safe_pointer(rc,
			(reduce_safe_pointer_fn) action_proc_construct_sub,
			compile, arg, out, NULL))
		reduce_throw(rc);

	/* Is it a class with a typecheck member? Return that instead.
	 */
	if (compile_lookup(compile, MEMBER_CHECK) &&
		class_get_member(out, MEMBER_CHECK, NULL, out)) {
#ifdef DEBUG
		printf("reduce: invoking arg checker\n");
#endif
	}

	if (trace_flags & TRACE_CLASS_NEW) {
		trace_result(TRACE_CLASS_NEW, out);
		trace_pop();
	}
}

static void *
action_proc_class_binary_sub(Reduce *rc, PElement *pe,
	PElement *fn, const char *name, PElement *b, PElement *out)
{
	PElement rhs;
	PElement base;

	base = *pe;
	heap_appl_init(&base, fn);
	if (!heap_appl_add(rc->heap, &base, &rhs) ||
		!heap_managedstring_new(rc->heap, name, &rhs) ||
		!heap_appl_add(rc->heap, &base, &rhs))
		return out;

	PEPUTPE(&rhs, b);
	PEPUTPE(out, pe);

	return NULL;
}

/* Something like "class + 12" ... call (class.add 12)
 */
static void
action_proc_class_binary(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	TraceFlags flags = op >= 0 ? TRACE_OPERATOR : TRACE_BUILTIN;
	PElement fn;

	if (trace_flags & flags) {
		VipsBuf *buf = trace_push();

		vips_buf_appendf(buf, "%s\n", _("invoking method:"));
		vips_buf_appends(buf, "     ");
		trace_pelement(a);
		vips_buf_appendf(buf, ".%s \"%s\" ", MEMBER_OO_BINARY, name);
		trace_pelement(b);
		vips_buf_appends(buf, "\n");

		trace_text(flags, "%s", vips_buf_all(buf));

		trace_pop();
	}

	/* Look up a.oo_binary and build (a.dispatch_binary "add" b)
	 * application.
	 */
	if (!class_get_member(a, MEMBER_OO_BINARY, NULL, &fn))
		action_boperror(rc, compile, error_get_sub(), op, name, a, b);

	if (reduce_safe_pointer(rc,
			(reduce_safe_pointer_fn) action_proc_class_binary_sub,
			&fn, (void *) name, b, out))
		action_boperror(rc, compile, error_get_sub(), op, name, a, b);
}

/* Something like "12 + class" ... call (class.add' 12)
 */
static void
action_proc_class_binary2(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	TraceFlags flags = op >= 0 ? TRACE_OPERATOR : TRACE_BUILTIN;
	PElement fn;

	if (trace_flags & flags) {
		VipsBuf *buf = trace_push();

		vips_buf_appendf(buf, "%s\n", _("invoking method:"));
		vips_buf_appends(buf, "     ");
		trace_pelement(b);
		vips_buf_appendf(buf, ".%s \"%s\" ", MEMBER_OO_BINARY2, name);
		trace_pelement(a);
		vips_buf_appends(buf, "\n");

		trace_text(flags, "%s", vips_buf_all(buf));

		trace_pop();
	}

	/* Look up b.dispatch_binary2 and build
	 * (b.dispatch_binary2 "add" a) application.
	 */
	if (!class_get_member(b, MEMBER_OO_BINARY2, NULL, &fn))
		action_boperror(rc, compile, error_get_sub(), op, name, a, b);

	if (reduce_safe_pointer(rc,
			(reduce_safe_pointer_fn) action_proc_class_binary_sub,
			&fn, (void *) name, a, out))
		action_boperror(rc, compile, error_get_sub(), op, name, a, b);
}

static void
action_landlor(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	reduce_spine(rc, a);

	if (PEISCOMB(a) && PEGETCOMB(a) == COMB_I)
		/* The reduce_spine() did us recursively ... bounce back.
		 */
		return;

	if (trace_flags & TRACE_OPERATOR)
		trace_push();

	/* Examine the LHS and see if we can avoid RHS eval.
	 */
	if (PEISCLASS(a))
		action_proc_class_binary(rc, compile, op, name, a, b, out);
	else if (PEISBOOL(a)) {
		if (op == BI_LOR && PEGETBOOL(a)) {
			if (trace_flags & TRACE_OPERATOR)
				trace_binop(compile, a, op, b);

			PEPUTP(out, ELEMENT_BOOL, TRUE);

			if (trace_flags & TRACE_OPERATOR)
				trace_result(TRACE_OPERATOR, out);
		}
		else if (op == BI_LAND && !PEGETBOOL(a)) {
			if (trace_flags & TRACE_OPERATOR)
				trace_binop(compile, a, op, b);

			PEPUTP(out, ELEMENT_BOOL, FALSE);

			if (trace_flags & TRACE_OPERATOR)
				trace_result(TRACE_OPERATOR, out);
		}
		else {
			/* Need to look at RHS too.
			 */
			reduce_spine(rc, b);

			if (PEISCOMB(b) && PEGETCOMB(b) != COMB_I) {
				if (trace_flags & TRACE_OPERATOR)
					trace_pop();
				return;
			}

			if (PEISCLASS(b))
				action_proc_class_binary2(rc, compile, op, name, a, b, out);
			else if (PEISBOOL(b)) {
				if (trace_flags & TRACE_OPERATOR)
					trace_binop(compile, a, op, b);

				PEPUTP(out, ELEMENT_BOOL, PEGETBOOL(b));

				if (trace_flags & TRACE_OPERATOR)
					trace_result(TRACE_OPERATOR, out);
			}
			else
				action_boperror(rc, compile, NULL, op, name, a, b);
		}
	}
	else
		action_boperror(rc, compile, NULL, op, name, a, b);

	if (trace_flags & TRACE_OPERATOR)
		trace_pop();
}

static void
action_if(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *b, PElement *out)
{
	reduce_spine(rc, a);

	if (PEISCOMB(a) && PEGETCOMB(a) == COMB_I)
		/* The reduce_spine() did us recursively ... bounce back.
		 */
		return;

	if (PEISCLASS(a))
		action_proc_class_binary(rc, compile, op, name, a, b, out);
	else {
		PElement t, e;

		/* a is condition, b should be [then-part, else-part] ...
		 * look down b and find them. Block trace for this, not very
		 * interesting.
		 */
		trace_block();
		reduce_list_index(rc, b, 0, &t);
		reduce_list_index(rc, b, 1, &e);
		trace_unblock();

		/* Can be BOOL or image.
		 */
		if (PEISBOOL(a)) {
			if (trace_flags & TRACE_OPERATOR) {
				VipsBuf *buf = trace_push();

				vips_buf_appendf(buf, "if ");
				trace_pelement(a);
				vips_buf_appendf(buf, " then ");
				trace_pelement(&t);
				vips_buf_appendf(buf, " else ");
				trace_pelement(&e);
				vips_buf_appendf(buf, " ->\n");
			}

			if (PEGETBOOL(a)) {
				PEPUTPE(out, &t);
			}
			else {
				PEPUTPE(out, &e);
			}

			if (trace_flags & TRACE_OPERATOR) {
				trace_result(TRACE_OPERATOR, out);
				trace_pop();
			}
		}
		else if (PEISIMAGE(a)) {
			reduce_spine_strict(rc, &t);
			reduce_spine_strict(rc, &e);

			/* then/else parts must both be image.
			 */
			if (!PEISIMAGE(&t) || !PEISIMAGE(&e))
				action_boperror(rc, compile, NULL, op, name, a, b);

			vo_callva(rc, out, "ifthenelse",
				PEGETIMAGE(a), PEGETIMAGE(&t), PEGETIMAGE(&e));
		}
		else
			action_boperror(rc, compile, NULL, op, name, a, b);
	}
}

/* Do a binary operator. Result in arg[0].
 */
void
action_proc_bop(Reduce *rc, Compile *compile, BinOp bop, HeapNode **arg)
{
	PElement a, b, out;

	switch (bop) {
	case BI_LAND:
	case BI_LOR:
		/* Special ninja magic :-( we need to handle reduce carefully
		 * here.
		 */
		PEPOINTRIGHT(arg[0], &b);
		PEPOINTRIGHT(arg[1], &a);
		PEPOINTRIGHT(arg[0], &out);

		action_landlor(rc, compile, bop, OPERATOR_NAME(bop), &a, &b, &out);

		/* Overwrite arg[0] with I node, in case this is a
		 * shared node.
		 */
		PPUTLEFT(arg[0], ELEMENT_COMB, COMB_I);

		break;

	case BI_IF:
		/* If is lazy-ish in it's 2nd argument too.
		 */
		PEPOINTRIGHT(arg[0], &b);
		PEPOINTRIGHT(arg[1], &a);
		PEPOINTRIGHT(arg[0], &out);

		action_if(rc, compile, bop, OPERATOR_NAME(bop), &a, &b, &out);

		/* Overwrite arg[0] with I node, in case this is a
		 * shared node.
		 */
		PPUTLEFT(arg[0], ELEMENT_COMB, COMB_I);

		break;

	case BI_DOT:
	case BI_PEQ:
	case BI_PNOTEQ:
		/* Strict, not overrideable.
		 */
		action_dispatch(rc, compile, reduce_spine,
			bop, OPERATOR_NAME(bop), FALSE,
			(ActionFn) action_proc_bop_strict, 2, arg, NULL);

		break;

	default:
		/* Strict, overrideable.
		 */
		action_dispatch(rc, compile, reduce_spine,
			bop, OPERATOR_NAME(bop), TRUE,
			(ActionFn) action_proc_bop_strict, 2, arg, NULL);
	}
}

static void *
action_proc_class_unary_sub(Reduce *rc, PElement *pe,
	PElement *fn, const char *name, PElement *out)
{
	PElement rhs;
	PElement base;

	base = *pe;
	heap_appl_init(&base, fn);
	if (!heap_appl_add(rc->heap, &base, &rhs) ||
		!heap_managedstring_new(rc->heap, name, &rhs))
		return out;

	PEPUTPE(out, pe);

	return NULL;
}

static void
action_proc_class_unary(Reduce *rc, Compile *compile,
	int op, const char *name, PElement *a, PElement *out)
{
	TraceFlags flags = op >= 0 ? TRACE_OPERATOR : TRACE_BUILTIN;
	PElement fn;

	if (trace_flags & flags) {
		VipsBuf *buf = trace_push();

		vips_buf_appendf(buf, "%s\n", _("invoking method:"));
		vips_buf_appends(buf, "     ");
		trace_pelement(a);
		vips_buf_appendf(buf, ".%s \"%s\"\n", MEMBER_OO_UNARY, name);

		trace_text(flags, "%s", vips_buf_all(buf));

		trace_pop();
	}

	/* Look up a.dispatch_unary and build
	 * (a.oo_unary "minus") application.
	 */
	if (!class_get_member(a, MEMBER_OO_UNARY, NULL, &fn))
		action_uoperror(rc, compile, error_get_sub(), op, name, a);

	if (reduce_safe_pointer(rc,
			(reduce_safe_pointer_fn) action_proc_class_unary_sub,
			&fn, (void *) name, out, NULL))
		action_uoperror(rc, compile, error_get_sub(), op, name, a);
}

/* Run a function on the graph ... eval all the args, avoid eval if we reduce
 * ourselves as a side effect (happens on recursive calls). Result in RHS of
 * arg[0].
 */
void
action_dispatch(Reduce *rc, Compile *compile, ReduceFunction rfn,
	int op, const char *name, gboolean override,
	ActionFn afn, int nargs, HeapNode **arg, void *user)
{
	TraceFlags flags = op >= 0 ? TRACE_OPERATOR : TRACE_BUILTIN;
	PElement a, b;
	int i;

	/* Don't allow nargs == 0. We rely on having a bit of graph we can
	 * replace with (I result) for caching.
	 */
	g_assert(nargs > 0);

	/* We need to have the
	 */
	g_assert(noperator_table == UN_LAST);

	/* Reduce all the args.
	 */
	for (i = 0; i < nargs; i++) {
		PElement rhs;

		PEPOINTRIGHT(arg[i], &rhs);
		rfn(rc, &rhs);
	}

	/* We may have evaled ourselves already.
	 */
	PEPOINTLEFT(arg[0], &b);
	if (PEISCOMB(&b) && PEGETCOMB(&b) == COMB_I)
		return;

	PEPOINTRIGHT(arg[0], &b);
	PEPOINTRIGHT(arg[1], &a);

	if (trace_flags & flags) {
		VipsBuf *buf = trace_push();

		vips_buf_appendf(buf, "\"%s\" ", name);
		trace_args(arg, nargs);
	}

	if (override && nargs == 2 && PEISCLASS(&a))
		action_proc_class_binary(rc, compile, op, name, &a, &b, &b);
	else if (override && nargs == 2 && PEISCLASS(&b))
		action_proc_class_binary2(rc, compile, op, name, &a, &b, &b);
	else if (override && nargs == 1 && PEISCLASS(&b))
		action_proc_class_unary(rc, compile, op, name, &b, &b);
	else
		afn(rc, compile, op, name, arg, &b, user);

	PPUTLEFT(arg[0], ELEMENT_COMB, COMB_I);

	if (trace_flags & flags) {
		trace_result(flags, &b);
		trace_pop();
	}
}
