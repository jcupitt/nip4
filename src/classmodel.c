/* like a heapmodel, but we represent a class in the heap
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

/*
#define DEBUG
 */

G_DEFINE_TYPE(Classmodel, classmodel, HEAPMODEL_TYPE);

/* All the classmodel that have been updated ... recomp needs to mark these
 * dirty before pickng the next row.
 */
static GSList *classmodel_updated = NULL;

void
image_value_init(ImageValue *image, Classmodel *classmodel)
{
	image->ii = NULL;
	image->file_changed_sid = 0;
	image->classmodel = classmodel;
}

void
image_value_destroy(ImageValue *image)
{
	FREESID(image->file_changed_sid, image->ii);
	MANAGED_UNREF(image->ii);
}

static void
image_value_file_changed_cb(Imageinfo *ii, ImageValue *image)
{
#ifdef DEBUG
	printf("image_value_file_changed_cb: ");
	iobject_print(IOBJECT(image->classmodel));
#endif /*DEBUG*/

	if (CALC_RELOAD) {
		Row *row = HEAPMODEL(image->classmodel)->row;

		(void) expr_dirty(row->expr, link_serial_new());
		symbol_recalculate_all();
	}
}

void
image_value_set(ImageValue *image, Imageinfo *ii)
{
	image_value_destroy(image);

	image->ii = ii;

	if (ii) {
		MANAGED_REF(image->ii);
		image->file_changed_sid = g_signal_connect(
			G_OBJECT(image->ii), "file_changed",
			G_CALLBACK(image_value_file_changed_cb), image);
	}

#ifdef DEBUG
	printf("iimage_instance_update: ii = %p\n", ii);
#endif /*DEBUG*/
}

/* Generate a descriptive name for an imagevalue. Used by plot.c etc. as well.
 */
void
image_value_caption(ImageValue *value, VipsBuf *buf)
{
	Imageinfo *ii = value->ii;
	Classmodel *classmodel = value->classmodel;

	/* Show the filename if this ii came from a file, otherwise show
	 * the class.
	 */
	if (ii && imageinfo_is_from_file(ii) && classmodel->filename) {
		g_autofree char *basename = g_path_get_basename(classmodel->filename);

		vips_buf_appends(buf, basename);
	}
	else if (!heapmodel_name(HEAPMODEL(classmodel), buf))
		/* Only if there's no value, I think.
		 */
		vips_buf_appends(buf, CLASS_IMAGE);
}

void *
classmodel_get_instance(Classmodel *classmodel)
{
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);

	return class && class->get_instance ? class->get_instance(classmodel) : NULL;
}

static void
classmodel_graphic_save_cb(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Classmodel *classmodel = CLASSMODEL(user_data);
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
	GtkWindow *window = g_object_get_data(G_OBJECT(dialog), "nip4-window");

	g_autoptr(GFile) file = gtk_file_dialog_save_finish(dialog, res, NULL);
	if (file) {
		g_autofree char *filename = g_file_get_path(file);

		if (class->graphic_save(classmodel, window, filename)) {
			VIPS_SETSTR(classmodel->filename, filename);
			iobject_changed(IOBJECT(classmodel));
		}
		else
			error_alert(window);
	}
}

void
classmodel_graphic_save(Classmodel *classmodel, GtkWindow *window)
{
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);

	if (!class->graphic_save) {
		error_top(_("Not implemented"));
		error_sub(_("_%s() method not implemented for %s"),
			"graphic_save", IOBJECT_GET_CLASS_NAME(classmodel));
		error_alert(window);
		return;
	}

	GtkFileDialog *dialog = gtk_file_dialog_new();

	char txt[100];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	row_qualified_name(HEAPMODEL(classmodel)->row, &buf);
	char txt2[100];
	VipsBuf buf2 = VIPS_BUF_STATIC(txt2);
	vips_buf_appendf(&buf2, _("Save %s \"%s\""),
		IOBJECT_GET_CLASS_NAME(classmodel), vips_buf_all(&buf));
	gtk_file_dialog_set_title(dialog, vips_buf_all(&buf2));

	gtk_file_dialog_set_modal(dialog, TRUE);

	if (classmodel->filename) {
		g_autoptr(GFile) file = g_file_new_for_path(classmodel->filename);

		if (file)
			gtk_file_dialog_set_initial_file(dialog, file);
	}

	g_object_set_data(G_OBJECT(dialog), "nip4-window", window);

	if (class->graphic_filter_save) {
		GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);

		GtkFileFilter *filter;

		filter = class->graphic_filter_save();
		g_list_store_append(filters, G_OBJECT(filter));
		g_object_unref(filter);

		filter = mainwindow_filter_all_new();
		g_list_store_append(filters, G_OBJECT(filter));
		g_object_unref(filter);

		gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
		g_object_unref(filters);
	}

	gtk_file_dialog_save(dialog, window, NULL,
		classmodel_graphic_save_cb, classmodel);
}

gboolean
classmodel_graphic_replace_filename(Classmodel *classmodel,
	GtkWindow *window, const char *filename)
{
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);

	if (!class->graphic_replace)
		return TRUE;

	if (!class->graphic_replace(classmodel, window, filename))
		return FALSE;

	g_object_ref(G_OBJECT(classmodel));

	symbol_recalculate_all();
	VIPS_SETSTR(classmodel->filename, filename);
	iobject_changed(IOBJECT(classmodel));

	g_object_unref(G_OBJECT(classmodel));

	return TRUE;
}

static void
classmodel_graphic_replace_cb(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	Classmodel *classmodel = CLASSMODEL(user_data);
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
	GtkWindow *window = g_object_get_data(G_OBJECT(dialog), "nip4-window");

	g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file) {
		g_autofree char *filename = g_file_get_path(file);

		if (!classmodel_graphic_replace_filename(classmodel, window, filename))
			error_alert(window);
	}
}

void
classmodel_graphic_replace(Classmodel *classmodel, GtkWindow *window)
{
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);

	if (!class->graphic_replace) {
		error_top(_("Not implemented"));
		error_sub(_("_%s() method not implemented for %s"),
			"graphic_replace",
			IOBJECT_GET_CLASS_NAME(classmodel));
		error_alert(window);
		return;
	}

	GtkFileDialog *dialog = gtk_file_dialog_new();

	char txt[100];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	row_qualified_name(HEAPMODEL(classmodel)->row, &buf);
	char txt2[100];
	VipsBuf buf2 = VIPS_BUF_STATIC(txt2);
	vips_buf_appendf(&buf2, _("Replace %s \"%s\""),
		IOBJECT_GET_CLASS_NAME(classmodel), vips_buf_all(&buf));
	gtk_file_dialog_set_title(dialog, vips_buf_all(&buf2));

	gtk_file_dialog_set_modal(dialog, TRUE);

	if (classmodel->filename) {
		g_autoptr(GFile) file = g_file_new_for_path(classmodel->filename);

		if (file)
			gtk_file_dialog_set_initial_file(dialog, file);
	}

	g_object_set_data(G_OBJECT(dialog), "nip4-window", window);

	if (class->graphic_filter_replace) {
		GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);

		GtkFileFilter *filter;

		filter = class->graphic_filter_replace();
		g_list_store_append(filters, G_OBJECT(filter));
		g_object_unref(filter);

		filter = mainwindow_filter_all_new();
		g_list_store_append(filters, G_OBJECT(filter));
		g_object_unref(filter);

		gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
		g_object_unref(filters);
	}

	gtk_file_dialog_open(dialog, window, NULL,
		classmodel_graphic_replace_cb, classmodel);
}

/* Make and break links between classmodels and the iimages displaying them.
 */
static void
classmodel_iimage_link(Classmodel *classmodel, iImage *iimage)
{
	if (!g_slist_find(classmodel->iimages, iimage)) {
#ifdef DEBUG
		printf("classmodel_iimage_link: linking ");
		row_name_print(HEAPMODEL(classmodel)->row);
		printf(" to ");
		row_name_print(HEAPMODEL(iimage)->row);
		printf("\n");
#endif /*DEBUG*/

		iimage->classmodels = g_slist_prepend(iimage->classmodels, classmodel);
		classmodel->iimages = g_slist_prepend(classmodel->iimages, iimage);
	}
}

void *
classmodel_iimage_unlink(Classmodel *classmodel, iImage *iimage)
{
	if (g_slist_find(classmodel->iimages, iimage)) {
#ifdef DEBUG
		printf("classmodel_iimage_unlink: unlinking ");
		row_name_print(HEAPMODEL(classmodel)->row);
		printf(" from ");
		row_name_print(HEAPMODEL(iimage)->row);
		printf("\n");
#endif /*DEBUG*/

		iimage->classmodels = g_slist_remove(iimage->classmodels, classmodel);
		classmodel->iimages = g_slist_remove(classmodel->iimages, iimage);
	}

	return NULL;
}

static void *
classmodel_iimage_unlink_rev(iImage *iimage, Classmodel *classmodel)
{
	return classmodel_iimage_unlink(classmodel, iimage);
}

typedef struct {
	Classmodel *classmodel;
	Imageinfo *ii;
} ClassmodelSearch;

static void *
classmodel_iimage_expr_model(Model *model, ClassmodelSearch *parms)
{
	/* Look for iimages which aren't super ... ie. if this is a class
	 * derived from Image, display on the derived class, not on the
	 * superclass.
	 */
	if (IS_IIMAGE(model) &&
		HEAPMODEL(model)->row->sym &&
		!is_super(HEAPMODEL(model)->row->sym) &&
		!is_this(HEAPMODEL(model)->row->sym)) {
		iImage *iimage = IIMAGE(model);

		if (iimage->value.ii == parms->ii)
			classmodel_iimage_link(parms->classmodel, iimage);
	}

	return NULL;
}

/* This classmodel is defined on an Imageinfo recorded as having been the value
 * of expr ... find an associated iImage, and link to that.
 */
static void *
classmodel_iimage_expr(Expr *expr, ClassmodelSearch *parms)
{
	if (expr->row) {
#ifdef DEBUG
		printf("classmodel_iimage_expr: starting for ");
		row_name_print(expr->row);
		printf("\n");
#endif /*DEBUG*/

		/* Search this part of the tally for an iImage with ii as its
		 * derived value, and link to us.
		 */
		(void) icontainer_map_all(ICONTAINER(expr->row->top_row),
			(icontainer_map_fn) classmodel_iimage_expr_model, parms);
	}

	return NULL;
}

/* classmodel is defined on ii ... update all the classmodel->iimage links.
 */
void
classmodel_iimage_update(Classmodel *classmodel, Imageinfo *ii)
{
	ClassmodelSearch parms;

	parms.classmodel = classmodel;
	parms.ii = ii;
	slist_map(classmodel->iimages,
		(SListMapFn) classmodel_iimage_unlink_rev, classmodel);

	/* Don't make links for supers/this.
	 */
	if (HEAPMODEL(classmodel)->row->sym &&
		!is_super(HEAPMODEL(classmodel)->row->sym) &&
		!is_this(HEAPMODEL(classmodel)->row->sym)) {
#ifdef DEBUG
		printf("classmodel_iimage_update: ");
		row_name_print(HEAPMODEL(classmodel)->row);
		printf(" is defined on ii \"%s\" ... searching for client "
			   "displays\n",
			ii->im->filename);
#endif /*DEBUG*/
		slist_map(imageinfo_expr_which(ii),
			(SListMapFn) classmodel_iimage_expr, &parms);
	}
}

static gboolean
classmodel_class_member_new(Classmodel *classmodel,
	ClassmodelMember *m, Heap *heap, PElement *out);

static gboolean
classmodel_dict_new(Classmodel *classmodel,
	ClassmodelMember *options, int noptions, Heap *heap, PElement *out)
{
	PElement list = *out;

	/* Make first RHS ... the end of the list.
	 */
	heap_list_init(&list);

	for (int i = 0; i < noptions; i++) {
		PElement pair, key, value;

		if (!heap_list_add(heap, &list, &pair) ||
			!heap_list_add(heap, &pair, &key) ||
			!heap_list_add(heap, &pair, &value) ||
			!heap_managedstring_new(heap, options[i].member_name, &key) ||
			!classmodel_class_member_new(classmodel, &options[i], heap, &value))
			return FALSE;

		(void) heap_list_next(&list);
	}

	return TRUE;
}

static gboolean
classmodel_class_member_new(Classmodel *classmodel,
	ClassmodelMember *m, Heap *heap, PElement *out)
{
	switch (m->type) {
	case CLASSMODEL_MEMBER_INT:
	case CLASSMODEL_MEMBER_ENUM:
		if (!heap_real_new(heap,
				G_STRUCT_MEMBER(int, classmodel, m->offset),
				out))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_BOOLEAN:
		if (!heap_bool_new(heap,
				G_STRUCT_MEMBER(gboolean, classmodel, m->offset),
				out))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_DOUBLE:
		if (!heap_real_new(heap,
				G_STRUCT_MEMBER(double, classmodel, m->offset),
				out))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_STRING:
		if (!heap_managedstring_new(heap,
				G_STRUCT_MEMBER(char *, classmodel, m->offset),
				out))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_STRING_LIST:
		if (!heap_lstring_new(heap,
				G_STRUCT_MEMBER(GSList *, classmodel, m->offset),
				out))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_REALVEC_FIXED:
		if (!heap_realvec_new(heap, m->extent,
				&G_STRUCT_MEMBER(double, classmodel, m->offset),
				out))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_MATRIX: {
		MatrixValue *value =
			&G_STRUCT_MEMBER(MatrixValue, classmodel, m->offset);

		if (!heap_matrix_new(heap,
				value->width, value->height, value->coeff, out))
			return FALSE;
		break;
	}

	case CLASSMODEL_MEMBER_OPTIONS:
		if (!classmodel_dict_new(classmodel,
				(ClassmodelMember *) m->details, m->extent,
				heap, out))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_IMAGE: {
		ImageValue *value =
			&G_STRUCT_MEMBER(ImageValue, classmodel, m->offset);

		PEPUTP(out, ELEMENT_MANAGED, value->ii);
		break;
	}

	default:
		g_assert(0);
	}

	return TRUE;
}

/* Trigger the class_new method for a classmodel ... look for a constructor:
 * try CLASS_edit, then if that's not defined, try CLASS. Eg.
 *	A1.Scale_edit from to value
 * if Scale_edit is not defined, try
 *	A1.Scale from to value
 */
static gboolean
classmodel_class_instance_new(Classmodel *classmodel)
{
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);
	Row *row = HEAPMODEL(classmodel)->row;
	PElement *root = &row->expr->root;
	const char *cname = IOBJECT(classmodel)->name;
	Reduce *rc = reduce_context;
	Heap *heap = rc->heap;

	char cname_new[256];
	PElement fn;

#ifdef DEBUG
	printf("classmodel_class_instance_new: ");
	row_name_print(HEAPMODEL(classmodel)->row);
	printf("\n");
#endif /*DEBUG*/

	/* Find and build.
	 */
	g_snprintf(cname_new, 256, "%s_edit", cname);
	if (!class_get_member(root, cname_new, NULL, &fn)) {
		if (!class_get_member(root, cname, NULL, &fn))
			return FALSE;
	}

	if (class->class_new) {
		if (!class->class_new(classmodel, &fn, root))
			return FALSE;
	}
	else {
		heap_appl_init(root, &fn);

		for (int i = 0; i < class->n_members; i++) {
			PElement rhs;

			if (!heap_appl_add(heap, root, &rhs))
				return FALSE;

			if (!classmodel_class_member_new(classmodel,
					&class->members[i], heap, &rhs))
				return FALSE;
		}
	}

	/* Reduce to base type.
	 */
	if (!reduce_pelement(rc, reduce_spine, root))
		return FALSE;

	/* We have a new heap struct ... tell everyone to get new pointers.
	 */
	if (heapmodel_new_heap(HEAPMODEL(row), root))
		return FALSE;

	return TRUE;
}

static void
classmodel_dispose(GObject *gobject)
{
	Classmodel *classmodel;

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_CLASSMODEL(gobject));

	classmodel = CLASSMODEL(gobject);

	/* My instance destroy stuff.
	 */
	slist_map(classmodel->iimages,
		(SListMapFn) classmodel_iimage_unlink_rev, classmodel);
	VIPS_FREE(classmodel->filename);

	classmodel_updated = g_slist_remove(classmodel_updated, classmodel);

	G_OBJECT_CLASS(classmodel_parent_class)->dispose(gobject);
}

/* We don't want subclases like Group to have an _info() method, since it
 * will appear in tooltips and the Container _info() is rather annoying.
 *
 * Things like iImage define an _info() with useful stuff in.
 */
static void
classmodel_info(iObject *iobject, VipsBuf *buf)
{
}

static void
classmodel_parent_add(iContainer *child)
{
	g_assert(IS_CLASSMODEL(child));

	ICONTAINER_CLASS(classmodel_parent_class)->parent_add(child);
}

/* How many widgets we allow for member automation edit.
 */
#define MAX_WIDGETS (10)

static gboolean
classmodel_save_member(Classmodel *classmodel,
	ClassmodelMember *m, xmlNode *xthis)
{
	int i;

	switch (m->type) {
	case CLASSMODEL_MEMBER_INT:
	case CLASSMODEL_MEMBER_ENUM:
		if (!set_iprop(xthis, m->save_name,
				G_STRUCT_MEMBER(int, classmodel, m->offset)))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_BOOLEAN:
		if (!set_sprop(xthis, m->save_name,
				bool_to_char(G_STRUCT_MEMBER(gboolean, classmodel, m->offset))))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_DOUBLE:
		if (!set_dprop(xthis, m->save_name,
				G_STRUCT_MEMBER(double, classmodel, m->offset)))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_STRING:
		if (!set_sprop(xthis, m->save_name,
				G_STRUCT_MEMBER(char *, classmodel, m->offset)))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_STRING_LIST:
		if (!set_slprop(xthis, m->save_name,
				G_STRUCT_MEMBER(GSList *, classmodel, m->offset)))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_REALVEC_FIXED:
		for (i = 0; i < m->extent; i++) {
			double value = (&G_STRUCT_MEMBER(double, classmodel, m->offset))[i];

			char buf[256];

			g_snprintf(buf, 256, "%s%d", m->save_name, i);
			if (!set_dprop(xthis, buf, value))
				return FALSE;
		}
		break;

	case CLASSMODEL_MEMBER_MATRIX: {
		MatrixValue *value =
			&G_STRUCT_MEMBER(MatrixValue, classmodel, m->offset);
		const int n = value->width * value->height;

		if (!set_dlprop(xthis, "value", value->coeff, n) ||
			!set_iprop(xthis, "width", value->width) ||
			!set_iprop(xthis, "height", value->height))
			return FALSE;

		break;
	}

	case CLASSMODEL_MEMBER_OPTIONS:
		for (i = 0; i < m->extent; i++) {
			ClassmodelMember *options = (ClassmodelMember *) m->details;

			if (!classmodel_save_member(classmodel, &options[i], xthis))
				return FALSE;
		}

		break;

	case CLASSMODEL_MEMBER_IMAGE:
		break;

	default:
		g_assert(0);
	}

	return TRUE;
}

static xmlNode *
classmodel_save(Model *model, xmlNode *xnode)
{
	Classmodel *classmodel = CLASSMODEL(model);
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);
	xmlNode *xthis;
	int i;

#ifdef DEBUG
	printf("classmodel_save: ");
	row_name_print(HEAPMODEL(classmodel)->row);
	printf("\n");
#endif /*DEBUG*/

	if (!(xthis = MODEL_CLASS(classmodel_parent_class)->save(model, xnode)))
		return NULL;

	if (classmodel->edited)
		for (i = 0; i < class->n_members; i++)
			if (!classmodel_save_member(classmodel, &class->members[i], xthis))
				return NULL;

	return xthis;
}

static gboolean
classmodel_load_member(Classmodel *classmodel,
	ClassmodelMember *m, xmlNode *xthis)
{
	char buf[MAX_STRSIZE];
	gboolean found;
	int i;

	found = FALSE;

	switch (m->type) {
	case CLASSMODEL_MEMBER_INT:
		if (get_iprop(xthis, m->save_name,
				&G_STRUCT_MEMBER(int, classmodel, m->offset)))
			found = TRUE;
		break;

	case CLASSMODEL_MEMBER_ENUM: {
		int v;

		if (get_iprop(xthis, m->save_name, &v)) {
			v = VIPS_CLIP(0, v, m->extent);
			G_STRUCT_MEMBER(int, classmodel, m->offset) = v;
			found = TRUE;
		}
		break;
	}

	case CLASSMODEL_MEMBER_BOOLEAN:
		if (get_bprop(xthis, m->save_name,
				&G_STRUCT_MEMBER(gboolean, classmodel, m->offset)))
			found = TRUE;
		break;

	case CLASSMODEL_MEMBER_DOUBLE:
		if (get_dprop(xthis, m->save_name,
				&G_STRUCT_MEMBER(double, classmodel, m->offset)))
			found = TRUE;
		break;

	case CLASSMODEL_MEMBER_STRING:
		if (get_sprop(xthis, m->save_name, buf, MAX_STRSIZE)) {
			VIPS_SETSTR(G_STRUCT_MEMBER(char *, classmodel, m->offset), buf);
			found = TRUE;
		}

		/* Nasty: before member automation, we used to always
		 * save/load caption, as a member of model. Now caption is
		 * only present if the class has it as a automated member.
		 * Plus some classes used to not support captions (eg. Scale).
		 * So: caption can be missing, even if it should be there. Set
		 * a fall-back value.
		 */
		if (!found && strcmp(m->save_name, "caption") == 0) {
			VIPS_SETSTR(G_STRUCT_MEMBER(char *, classmodel, m->offset), "");
			found = TRUE;
		}
		break;

	case CLASSMODEL_MEMBER_STRING_LIST: {
		GSList **member = &G_STRUCT_MEMBER(GSList *, classmodel, m->offset);

		GSList *slist;

		if (get_slprop(xthis, m->member_name, &slist)) {
			g_slist_free_full(g_steal_pointer(member), g_free);
			*member = slist;
			found = TRUE;
		}

		break;
	}

	case CLASSMODEL_MEMBER_REALVEC_FIXED:
		for (i = 0; i < m->extent; i++) {
			g_snprintf(buf, MAX_STRSIZE, "%s%d", m->save_name, i);
			if (get_dprop(xthis, buf,
					&((&G_STRUCT_MEMBER(double, classmodel, m->offset))[i])))
				found = TRUE;
		}
		break;

	case CLASSMODEL_MEMBER_MATRIX: {
		MatrixValue *value =
			&G_STRUCT_MEMBER(MatrixValue, classmodel, m->offset);

		if (get_dlprop(xthis, "value", &value->coeff) &&
			get_iprop(xthis, "width", &value->width) &&
			get_iprop(xthis, "height", &value->height))
			found = TRUE;

		break;
	}

	case CLASSMODEL_MEMBER_OPTIONS:
		for (i = 0; i < m->extent; i++) {
			ClassmodelMember *options = (ClassmodelMember *) m->details;

			if (!classmodel_load_member(classmodel, &options[i], xthis))
				return FALSE;
		}

		break;

	case CLASSMODEL_MEMBER_IMAGE:
		break;

	default:
		g_assert(0);
	}

	return found;
}

static gboolean
classmodel_load(Model *model,
	ModelLoadState *state, Model *parent, xmlNode *xthis)
{
	Classmodel *classmodel = CLASSMODEL(model);
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);

#ifdef DEBUG
	printf("classmodel_load: ");
	row_name_print(HEAPMODEL(classmodel)->row);
	printf("\n");
#endif /*DEBUG*/

	/* Only for classes with member automation.
	 */
	if (class->n_members) {
		gboolean all_found;
		int i;

		/* Before we mark the graphic as edited, insist all
		 * members have values set. This can be important in
		 * compatibility mode, where the old nip might not have
		 * supported all the members we have.
		 */
		all_found = TRUE;
		for (i = 0; i < class->n_members; i++)
			all_found &= classmodel_load_member(classmodel,
				&class->members[i], xthis);
		if (all_found)
			classmodel_set_edited(CLASSMODEL(model), TRUE);
	}

	return MODEL_CLASS(classmodel_parent_class)->
		load(model, state, parent, xthis);
}

static gboolean
classmodel_get_item(Classmodel *classmodel,
	ClassmodelMember *m, PElement *value);

static void *
classmodel_parse_option(const char *key, PElement *value,
	Classmodel *classmodel, ClassmodelMember *m)
{
	ClassmodelMember *options = (ClassmodelMember *) m->details;
	int noptions = m->extent;
	int i;

	for (i = 0; i < noptions; i++)
		if (strcmp(key, options[i].member_name) == 0)
			break;
	if (i == noptions) {
		error_top(_("Unknown option"));
		error_sub(_("option \"%s\" not known"), key);

		return value;
	}

	if (!classmodel_get_item(classmodel, &options[i], value))
		return value;

	return NULL;
}

static gboolean
classmodel_get_item(Classmodel *classmodel,
	ClassmodelMember *m, PElement *value)
{
	char buf[MAX_STRSIZE];
	double vec[3];
	int l;
	int i;
	double d;

	switch (m->type) {
	case CLASSMODEL_MEMBER_INT:
		if (!heap_get_real(value, &d))
			return FALSE;
		G_STRUCT_MEMBER(int, classmodel, m->offset) = d;
		break;

	case CLASSMODEL_MEMBER_ENUM:
		if (!heap_get_real(value, &d))
			return FALSE;
		d = VIPS_CLIP(0, d, m->extent);
		G_STRUCT_MEMBER(int, classmodel, m->offset) = d;
		break;

	case CLASSMODEL_MEMBER_BOOLEAN:
		if (!heap_get_bool(value,
				&G_STRUCT_MEMBER(gboolean, classmodel, m->offset)))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_DOUBLE:
		if (!heap_get_real(value,
				&G_STRUCT_MEMBER(double, classmodel, m->offset)))
			return FALSE;
		break;

	case CLASSMODEL_MEMBER_STRING:
		if (!heap_get_string(value, buf, MAX_STRSIZE))
			return FALSE;
		VIPS_SETSTR(G_STRUCT_MEMBER(char *, classmodel, m->offset), buf);
		break;

	case CLASSMODEL_MEMBER_STRING_LIST: {
		GSList *slist;
		GSList **member = &G_STRUCT_MEMBER(GSList *, classmodel, m->offset);

		if (!heap_get_lstring(value, &slist))
			return FALSE;

		g_slist_free_full(g_steal_pointer(member), g_free);
		*member = slist;

		break;
	}

	case CLASSMODEL_MEMBER_REALVEC_FIXED:
		g_assert(m->extent < 4);

		if ((l = heap_get_realvec(value, vec, m->extent)) < 0)
			return FALSE;
		if (l != m->extent) {
			error_top(_("Bad value"));
			error_sub(_("%d band value only"), m->extent);
			return FALSE;
		}
		for (i = 0; i < m->extent; i++)
			(&G_STRUCT_MEMBER(double, classmodel, m->offset))[i] =
				vec[i];
		break;

	case CLASSMODEL_MEMBER_MATRIX: {
		MatrixValue *matrix =
			&G_STRUCT_MEMBER(MatrixValue, classmodel, m->offset);
		int w, h;

		if (!heap_get_matrix_size(value, &w, &h) ||
			!matrix_value_resize(matrix, w, h) ||
			!heap_get_matrix(value,
				matrix->coeff, matrix->width * matrix->height,
				&w, &h))
			return FALSE;

		break;
	}

	case CLASSMODEL_MEMBER_OPTIONS:
		/* If there are optional fields, we have to have a reset
		 * method for clearing the ones we don't use.
		 */
		g_assert(CLASSMODEL_GET_CLASS(classmodel)->reset);

		if (heap_map_dict(value,
				(heap_map_dict_fn) classmodel_parse_option,
				classmodel, m))
			return FALSE;

		break;

	case CLASSMODEL_MEMBER_IMAGE: {
		ImageValue *image =
			&G_STRUCT_MEMBER(ImageValue, classmodel, m->offset);
		Imageinfo *ii;

		g_assert(image->classmodel == classmodel);

		if (!heap_get_image(value, &ii))
			return FALSE;
		image_value_set(image, ii);

		break;
	}

	default:
		g_assert(0);
	}

	return TRUE;
}

static gboolean
classmodel_update_model_member(Classmodel *classmodel,
	ClassmodelMember *m, PElement *root)
{
	PElement value;

	if (!class_get_member(root, m->member_name, NULL, &value))
		return FALSE;

#ifdef DEBUG
	printf("classmodel_update_model_member: setting %s = ",
		m->member_name);
	pgraph(&value);
#endif /*DEBUG*/

	if (!classmodel_get_item(classmodel, m, &value))
		return FALSE;

	return TRUE;
}

/* Update all members from the heap. Also used from graph_export_image.
 */
gboolean
classmodel_update_members(Classmodel *classmodel, PElement *root)
{
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);

	int i;

	for (i = 0; i < class->n_members; i++)
		if (!classmodel_update_model_member(classmodel,
				&class->members[i], root))
			return FALSE;

	if (class->class_get &&
		!class->class_get(classmodel, root))
		return FALSE;

	return TRUE;
}

static void *
classmodel_update_model(Heapmodel *heapmodel)
{
	Classmodel *classmodel = CLASSMODEL(heapmodel);
	ClassmodelClass *class = CLASSMODEL_GET_CLASS(classmodel);

#ifdef DEBUG
	printf("classmodel_update_model: ");
	row_name_print(heapmodel->row);
	printf("\n");
#endif /*DEBUG*/

	/* If necessary, reset model to default.
	 */
	if (class->reset)
		class->reset(classmodel);

	if (heapmodel->row &&
		heapmodel->row->expr) {
		Expr *expr = heapmodel->row->expr;

		if (!heapmodel->modified)
			if (!classmodel_update_members(classmodel,
					&expr->root))
				return classmodel;
	}

	return HEAPMODEL_CLASS(classmodel_parent_class)->update_model(heapmodel);
}

static void *
classmodel_update_heap(Heapmodel *heapmodel)
{
	Classmodel *classmodel = CLASSMODEL(heapmodel);

#ifdef DEBUG
	printf("classmodel_update_heap: ");
	row_name_print(HEAPMODEL(classmodel)->row);
	printf("\n");
#endif /*DEBUG*/

	/* Nasty: classmodel_class_instance_new() can (indirectly) destroy us.
	 * Wrap a _ref()/_unref() pair around it to make sure we stay alive.
	 */
	g_object_ref(G_OBJECT(heapmodel));

	/* Build a new instance from the model.
	 */
	if (!classmodel_class_instance_new(classmodel)) {
		g_object_unref(G_OBJECT(heapmodel));
		return heapmodel;
	}

	if (HEAPMODEL_CLASS(classmodel_parent_class)->update_heap(heapmodel)) {
		g_object_unref(G_OBJECT(heapmodel));
		return heapmodel;
	}

	g_object_unref(G_OBJECT(heapmodel));

	return NULL;
}

static void *
classmodel_clear_edited(Heapmodel *heapmodel)
{
	Classmodel *classmodel = CLASSMODEL(heapmodel);

	classmodel_set_edited(classmodel, FALSE);

	return HEAPMODEL_CLASS(classmodel_parent_class)->clear_edited(heapmodel);
}

static gboolean
classmodel_real_class_get(Classmodel *classmodel, PElement *root)
{
	return TRUE;
}

static void
classmodel_class_init(ClassmodelClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	iObjectClass *iobject_class = (iObjectClass *) class;
	iContainerClass *icontainer_class = (iContainerClass *) class;
	ModelClass *model_class = (ModelClass *) class;
	HeapmodelClass *heapmodel_class = (HeapmodelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	/* Init methods.
	 */
	gobject_class->dispose = classmodel_dispose;

	iobject_class->info = classmodel_info;

	icontainer_class->parent_add = classmodel_parent_add;

	model_class->save = classmodel_save;
	model_class->load = classmodel_load;

	heapmodel_class->update_model = classmodel_update_model;
	heapmodel_class->update_heap = classmodel_update_heap;
	heapmodel_class->clear_edited = classmodel_clear_edited;

	classmodel_class->get_instance = NULL;

	classmodel_class->class_get = classmodel_real_class_get;
	classmodel_class->class_new = NULL;

	classmodel_class->graphic_save = NULL;
	classmodel_class->graphic_replace = NULL;

	classmodel_class->members = NULL;
	classmodel_class->n_members = 0;
}

static void
classmodel_init(Classmodel *classmodel)
{
	Model *model = MODEL(classmodel);

	model->display = FALSE;

	classmodel->edited = FALSE;

	classmodel->iimages = NULL;
	classmodel->views = NULL;

	classmodel->filename = NULL;
}

void
classmodel_set_edited(Classmodel *classmodel, gboolean edited)
{
	if (classmodel->edited != edited) {
#ifdef DEBUG
		printf("classmodel_set_edited: ");
		row_name_print(HEAPMODEL(classmodel)->row);
		printf(" %s\n", bool_to_char(edited));
#endif /*DEBUG*/

		classmodel->edited = edited;
		iobject_changed(IOBJECT(classmodel));
	}

	/* Mark eds for application.
	 */
	if (edited)
		heapmodel_set_modified(HEAPMODEL(classmodel), TRUE);
}

/* The model has changed: mark for recomp.
 */
void
classmodel_update(Classmodel *classmodel)
{
	Row *row = HEAPMODEL(classmodel)->row;

	/* Eg. for no symbol on load.
	 */
	if (!row->expr)
		return;

#ifdef DEBUG
	printf("classmodel_update: ");
	row_name_print(HEAPMODEL(classmodel)->row);
	printf("\n");
#endif /*DEBUG*/

	/* classmodel_update_heap() will rebuild us on recomp. Don't set ws
	 * modified, we might be in parse.
	 */
	classmodel_set_edited(classmodel, TRUE);

	classmodel_updated = g_slist_prepend(classmodel_updated, classmodel);
}

/* Model has been changed by a view. Mark for recomp, and since this was a
 * user action from a view, mark the ws as modified.
 */
void
classmodel_update_view(Classmodel *classmodel)
{
	classmodel_update(classmodel);

	Row *row = HEAPMODEL(classmodel)->row;
	if (row &&
		row->expr)
		workspace_set_modified(row->ws, TRUE);
}

static void *
classmodel_dirty_updated_sub(void *a, void *b)
{
	Classmodel *classmodel = CLASSMODEL(a);
	Row *row = HEAPMODEL(classmodel)->row;

	if (row &&
		row->expr)
		expr_dirty(row->expr, link_serial_new());

	return NULL;
}

void
classmodel_dirty_updated(void)
{
	slist_map(classmodel_updated, classmodel_dirty_updated_sub, NULL);
	VIPS_FREEF(g_slist_free, classmodel_updated);
}

/* Make a new classmodel subtype (eg. TYPE_PATHNAME) and link it on.
 */
Classmodel *
classmodel_new_classmodel(GType type, Rhs *rhs)
{
	Classmodel *classmodel;

	classmodel = g_object_new(type, NULL);
	icontainer_child_add(ICONTAINER(rhs), ICONTAINER(classmodel), -1);

	return classmodel;
}
