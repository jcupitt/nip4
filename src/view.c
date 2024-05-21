// base class for all view widgets

/*

	Copyright (C) 1991-2003 The National Gallery
	Copyright (C) 2004-2023 libvips.org

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
#define DEBUG_VIEWCHILD
#define DEBUG
 */
#define DEBUG_LEAK

/* Time each refresh
#define DEBUG_TIME
 */

#include "nip4.h"

G_DEFINE_TYPE(View, view, VOBJECT_TYPE)

static GSList *view_scannable = NULL;

static GSList *view_resettable = NULL;

#ifdef DEBUG_LEAK
#warning "DEBUG_LEAK is on in view.c"
static GSList *all_view_widgets = NULL;
#endif /*DEBUG_LEAK*/

#ifdef DEBUG_LEAK
void
view_dump(void)
{
	if (all_view_widgets) {
		printf("view_dump(): %d views\n", g_slist_length(all_view_widgets));

		for (GSList *p = all_view_widgets; p; p = p->next) {
			View *view = VIEW(p->data);
			iObject *model = VOBJECT(view)->iobject;

			printf("%p %s count=%d ",
				view, G_OBJECT_TYPE_NAME(view), G_OBJECT(view)->ref_count);
			if (model)
				printf("%s %s ",
					G_OBJECT_TYPE_NAME(model), IOBJECT(model)->name);
			printf("\n");
		}
	}
}
#endif /*DEBUG_LEAK*/

void
view_scannable_register(View *view)
{
	/* Must have a scan method.
	 */
	g_assert(VIEW_GET_CLASS(view)->scan);

	if (!view->scannable) {
		view_scannable = g_slist_prepend(view_scannable, view);
		view->scannable = TRUE;
	}
}

void
view_scannable_unregister(View *view)
{
	if (view->scannable) {
		view_scannable = g_slist_remove(view_scannable, view);
		view->scannable = FALSE;
	}
}

gboolean
view_scan_all(void)
{
	if (slist_map(view_scannable, (SListMapFn) view_scan, NULL))
		return FALSE;

	view_reset_all();

	return TRUE;
}

void
view_resettable_register(View *view)
{
	/* Must have a reset method.
	 */
	g_assert(VIEW_GET_CLASS(view)->reset);

	if (!view->resettable) {
		view_resettable = g_slist_prepend(view_resettable, view);
		view->resettable = TRUE;
	}
}

void
view_resettable_unregister(View *view)
{
	if (view->resettable) {
		view_resettable = g_slist_remove(view_resettable, view);
		view->resettable = FALSE;
	}
}

void
view_reset_all(void)
{
	(void) slist_map(view_resettable, (SListMapFn) view_reset, NULL);
}

static void *
view_viewchild_destroy(ViewChild *viewchild)
{
	View *view = viewchild->view;
	View *child_view = viewchild->child_view;

#ifdef DEBUG_VIEWCHILD
	printf("view_viewchild_destroy: %s\n", G_OBJECT_TYPE_NAME(view));
#endif /*DEBUG_VIEWCHILD*/

	FREESID(viewchild->child_model_changed_sid, viewchild->child_model);
	view->managed = g_slist_remove(view->managed, viewchild);

	g_free(viewchild);

	return NULL;
}

/* Should a viewchild be displayed? If model->display is true, also give the
 * enclosing view a chance to filter.
 */
static gboolean
view_viewchild_display(ViewChild *viewchild)
{
	View *view = viewchild->view;
	Model *child_model = viewchild->child_model;
	ViewClass *view_class = VIEW_GET_CLASS(view);

	return (child_model->display && view_class->display) ? view_class->display(view, child_model) : child_model->display;
}

/* One of the children of the model we watch has changed ... create or destroy
 * the child view as required.
 */
static void
view_viewchild_changed(Model *model, ViewChild *viewchild)
{
	gboolean display = view_viewchild_display(viewchild);
	View *child = viewchild->child_view;

#ifdef DEBUG_VIEWCHILD
	printf("view_viewchild_changed: %s \"%s\" display=%d, child=%p\n",
		G_OBJECT_TYPE_NAME(model),
		IOBJECT(model)->name,
		display,
		child);
#endif /*DEBUG_VIEWCHILD*/

	if (!display && child) {
#ifdef DEBUG_VIEWCHILD
		printf("\tremoving view %s\n", G_OBJECT_TYPE_NAME(child));
#endif /*DEBUG_VIEWCHILD*/

		view_child_remove(child);
	}
	else if (display && !child) {
#ifdef DEBUG_VIEWCHILD
		printf("\tadding view\n");
#endif /*DEBUG_VIEWCHILD*/

		// this will set the viewchild->child_view pointer, see
		// view_real_child_add()
		model_view_new(model, viewchild->view);
	}
}

static ViewChild *
view_viewchild_new(View *view, Model *child_model)
{
	ViewChild *viewchild;

#ifdef DEBUG_VIEWCHILD
	printf("view_viewchild_new: view \"%s\" watching %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(view),
		G_OBJECT_TYPE_NAME(child_model), IOBJECT(child_model)->name);
#endif /*DEBUG_VIEWCHILD*/

	if (!(viewchild = INEW(NULL, ViewChild)))
		return NULL;

	viewchild->view = view;
	viewchild->child_model = child_model;
	viewchild->child_model_changed_sid = g_signal_connect(child_model,
		"changed", G_CALLBACK(view_viewchild_changed), viewchild);
	viewchild->child_view = NULL;

	view->managed = g_slist_append(view->managed, viewchild);

	return viewchild;
}

static void *
view_viewchild_child_model_equal(ViewChild *viewchild, Model *child_model)
{
	if (viewchild->child_model == child_model)
		return viewchild;

	return NULL;
}

/* Do we have a model?
 */
gboolean
view_hasmodel(View *view)
{
	return VOBJECT(view)->iobject != NULL;
}

void *
view_model_equal(View *view, Model *model)
{
	if (VOBJECT(view)->iobject == IOBJECT(model))
		return view;

	return NULL;
}

/* Link to enclosing model and view.
 */
void
view_link(View *view, Model *model, View *parent)
{
	VIEW_GET_CLASS(view)->link(view, model, parent);
}

/* Add a child.
 */
void
view_child_add(View *parent, View *child)
{
	VIEW_GET_CLASS(parent)->child_add(parent, child);
}

/* Remove a child.
 */
void
view_child_remove(View *child)
{
	View *parent = child->parent;

#ifdef DEBUG
	printf("view_child_remove: child %s %p\n",
		G_OBJECT_TYPE_NAME(child), child);
	if (parent)
		printf("\tparent %s %p\n", G_OBJECT_TYPE_NAME(parent), parent);
#endif /*DEBUG*/

	if (parent)
		VIEW_GET_CLASS(parent)->child_remove(parent, child);
}

/* Child needs repositioning.
 */
void
view_child_position(View *child)
{
	View *parent = child->parent;

	VIEW_GET_CLASS(parent)->child_position(parent, child);
}

/* Pop child to front of stacking order.
 */
void
view_child_front(View *child)
{
	View *parent = child->parent;

	if (parent)
		VIEW_GET_CLASS(parent)->child_front(parent, child);
}

static void
view_dispose(GObject *object)
{
	View *view;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_VIEW(object));

	view = VIEW(object);

#ifdef DEBUG
	printf("view_dispose: %p \"%s\"\n", object, G_OBJECT_TYPE_NAME(object));
#endif /*DEBUG*/

	if (view->scannable)
		view_scannable_unregister(view);
	if (view->resettable)
		view_resettable_unregister(view);

	// unlink from parent
	if (view->parent)
		view_child_remove(view);
	g_assert(!view->parent);

#ifdef DEBUG_LEAK
	// we should have no living child widgets
	gboolean stray_children = FALSE;
	for (GSList *p = view->managed; p; p = p->next) {
		ViewChild *viewchild = (ViewChild *) p->data;

		if (viewchild->child_view) {
			iObject *child_model = VOBJECT(viewchild->child_view)->iobject;

			printf("%p %s count=%d ",
				viewchild->child_view,
				G_OBJECT_TYPE_NAME(viewchild->child_view),
				G_OBJECT(viewchild->child_view)->ref_count);
			if (child_model)
				printf("%s %s ",
					G_OBJECT_TYPE_NAME(child_model),
					IOBJECT(child_model)->name);
			printf("\n");
			stray_children = TRUE;
		}
	}
	//g_assert(!stray_children);
#endif /*DEBUG_LEAK*/

	// just a viewchild skeleton we can free
	slist_map(view->managed, (SListMapFn) view_viewchild_destroy, NULL);

#ifdef DEBUG_LEAK
	all_view_widgets = g_slist_remove(all_view_widgets, view);
#endif /*DEBUG_LEAK*/

	G_OBJECT_CLASS(view_parent_class)->dispose(object);
}

static void
view_finalize(GObject *gobject)
{
#ifdef DEBUG
	printf("view_finalize: \"%s\"\n", G_OBJECT_TYPE_NAME(gobject));
#endif /*DEBUG*/

	G_OBJECT_CLASS(view_parent_class)->finalize(gobject);
}

/* Called for model pos_changed signal ... queue a refresh.
 */
static void
view_model_pos_changed(Model *model, View *view)
{
	g_assert(IS_MODEL(model));
	g_assert(IS_VIEW(view));

#ifdef DEBUG
	printf("view_model_pos_changed: %s %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(view),
		G_OBJECT_TYPE_NAME(model),
		IOBJECT(model)->name);
#endif /*DEBUG*/

	vobject_refresh_queue(VOBJECT(view));
}

/* Called for model scrollto signal ... try scrolling.
 */
static void
view_model_scrollto(Model *model, ModelScrollPosition position, View *view)
{
	g_assert(IS_MODEL(model));
	g_assert(IS_VIEW(view));

#ifdef DEBUG
	printf("view_model_scrollto: %s\n", IOBJECT(model)->name);
#endif /*DEBUG*/

	view_scrollto(view, position);
}

/* Called for model layout signal ... try to lay out children.
 */
static void
view_model_layout(Model *model, View *view)
{
	g_assert(IS_MODEL(model));
	g_assert(IS_VIEW(view));

#ifdef DEBUG
	printf("view_model_layout: %s\n", IOBJECT(model)->name);
#endif /*DEBUG*/

	view_layout(view);
}

/* Called for model reset signal ... try resetting.
 */
static void
view_model_reset(Model *model, View *view)
{
	g_assert(IS_MODEL(model));
	g_assert(IS_VIEW(view));

#ifdef DEBUG
	printf("view_model_reset: %s\n", IOBJECT(model)->name);
#endif /*DEBUG*/

	view_reset(view);
}

/* Called for model front signal ... bring view to front.
 */
static void
view_model_front(Model *model, View *view)
{
	g_assert(IS_MODEL(model));
	g_assert(IS_VIEW(view));

#ifdef DEBUG
	printf("view_model_front: model %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(model), IOBJECT(model)->name);
	printf("\tview %s\n", G_OBJECT_TYPE_NAME(view));
#endif /*DEBUG*/

	view_child_front(view);
}

/* Called for model child_add signal ... start watching that child.
 */
static void
view_model_child_add(Model *parent, Model *child, int pos, View *view)
{
	ViewChild *viewchild;

#ifdef DEBUG
	printf("view_model_child_add\n");
	printf("\tparent = %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(parent), IOBJECT(parent)->name);
#endif /*DEBUG*/

	g_assert(IS_MODEL(parent));
	g_assert(IS_MODEL(child));
	g_assert(IS_VIEW(view));

#ifdef DEBUG
	viewchild = slist_map(view->managed,
		(SListMapFn) view_viewchild_child_model_equal, child);
	g_assert(!viewchild);
#endif /*DEBUG*/

	viewchild = view_viewchild_new(view, child);
	view_viewchild_changed(child, viewchild);
}

/* Called for model child_remove signal ... stop watching that child. child
 * may have been finalized already.
 */
static void
view_model_child_remove(iContainer *parent, iContainer *child, View *view)
{
	ViewChild *viewchild;

#ifdef DEBUG
	printf("view_model_child_remove:\n");
	printf("\tchild = %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(child), IOBJECT(child)->name);
	printf("\tparent = %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(parent), IOBJECT(parent)->name);
	printf("\tview = %s\n",
		G_OBJECT_TYPE_NAME(view));
#endif /*DEBUG*/

	viewchild = slist_map(view->managed,
		(SListMapFn) view_viewchild_child_model_equal, child);

	g_assert(viewchild);

	(void) view_viewchild_destroy(viewchild);
}

/* Called for model parent_detach signal ... remove the viewchild for this
 * child. child_attach will build a new one.
 */
static void
view_model_child_detach(iContainer *old_parent, iContainer *child,
	View *old_view)
{
	ViewChild *viewchild;

#ifdef DEBUG
	printf("view_model_child_detach:\n");
	printf("\tchild = %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(child), IOBJECT(child)->name);
	printf("\told_parent = %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(old_parent), IOBJECT(old_parent)->name);

	printf("view_model_child_detach: old_view = view of %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(VOBJECT(old_view)->iobject),
		IOBJECT(VOBJECT(old_view)->iobject)->name);
#endif /*DEBUG*/

	viewchild = slist_map(old_view->managed,
		(SListMapFn) view_viewchild_child_model_equal, child);

	g_assert(viewchild);
	g_assert(!child->temp_view);

	child->temp_view = viewchild->child_view;

	(void) view_viewchild_destroy(viewchild);
}

/* Called for model child_attach signal ... make a new viewchild on the new
 * parent view.
 */
static void
view_model_child_attach(iContainer *new_parent, iContainer *child, int pos,
	View *new_view)
{
	ViewChild *viewchild;

#ifdef DEBUG
	printf("view_model_child_attach:\n");
	printf("\tnew_parent =  %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(new_parent), IOBJECT(new_parent)->name);
	printf("\tchild =  %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(child), IOBJECT(child)->name);
	printf("\tnew_view =  %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(new_view), IOBJECT(new_view)->name);
#endif /*DEBUG*/

	g_assert(!slist_map(new_view->managed,
		(SListMapFn) view_viewchild_child_model_equal, child));

	viewchild = view_viewchild_new(new_view, MODEL(child));

	g_assert(child->temp_view && IS_VIEW(child->temp_view));
	viewchild->child_view = child->temp_view;
	child->temp_view = NULL;

	viewchild->child_view->parent = new_view;
}

static void *
view_real_link_sub(Model *child_model, View *view)
{
	ViewChild *viewchild;

	viewchild = view_viewchild_new(view, child_model);
	view_viewchild_changed(child_model, viewchild);

	return NULL;
}

/* Link to model and to enclosing view.
 */
static void
view_real_link(View *view, Model *model, View *parent_view)
{
	g_assert(view != NULL);
	g_assert(IS_VIEW(view) && IS_MODEL(model));
	g_assert(!VOBJECT(view)->iobject);

#ifdef DEBUG
	printf("view_real_link: view %s to model %s \"%s\"\n",
		G_OBJECT_TYPE_NAME(view),
		G_OBJECT_TYPE_NAME(model), IOBJECT(model)->name);
#endif /*DEBUG*/

	vobject_link(VOBJECT(view), IOBJECT(model));
	if (parent_view)
		view_child_add(parent_view, view);

	g_signal_connect_object(model, "pos_changed",
		G_CALLBACK(view_model_pos_changed), view, 0);
	g_signal_connect_object(model, "scrollto",
		G_CALLBACK(view_model_scrollto), view, 0);
	g_signal_connect_object(model, "layout",
		G_CALLBACK(view_model_layout), view, 0);
	g_signal_connect_object(model, "reset",
		G_CALLBACK(view_model_reset), view, 0);
	g_signal_connect_object(model, "front",
		G_CALLBACK(view_model_front), view, 0);
	g_signal_connect_object(model, "child_add",
		G_CALLBACK(view_model_child_add), view, 0);
	g_signal_connect_object(model, "child_remove",
		G_CALLBACK(view_model_child_remove), view, 0);
	g_signal_connect_object(model, "child_detach",
		G_CALLBACK(view_model_child_detach), view, 0);
	g_signal_connect_object(model, "child_attach",
		G_CALLBACK(view_model_child_attach), view, 0);

	icontainer_map(ICONTAINER(model),
		(icontainer_map_fn) view_real_link_sub, view, NULL);
}

static void
view_real_child_add(View *parent, View *child)
{
	Model *child_model = VOBJECT(child)->iobject;

	ViewChild *viewchild;

	g_assert(IS_VIEW(parent) && IS_VIEW(child));
	g_assert(child->parent == NULL);

#ifdef DEBUG
	printf("view_real_child_add:\n");
	printf("\tparent = %p %s\n", parent, G_OBJECT_TYPE_NAME(parent));
	printf("\tchild = %p %s\n", child, G_OBJECT_TYPE_NAME(child));
#endif /*DEBUG*/

	viewchild = slist_map(parent->managed,
		(SListMapFn) view_viewchild_child_model_equal, child_model);

	g_assert(viewchild);
	g_assert(viewchild->child_view == NULL);

	viewchild->child_view = child;

	g_assert(!child->parent);
	child->parent = parent;

	/* We don't ref and unref ... our superclasses will add the view to the
	 * enclosing widget and that will hold a ref for us. Views without an
	 * enclosing widget (eg. rowview) need a ref/unref in the enclosing view
	 * instead, see subcolumnview.
	 */
}

static void
view_real_child_remove(View *parent, View *child)
{
	Model *child_model = VOBJECT(child)->iobject;

	ViewChild *viewchild;

#ifdef DEBUG
	printf("view_real_child_remove:\n");
	printf("\tparent = %p %s\n", parent, G_OBJECT_TYPE_NAME(parent));
	printf("\tchild = %p %s\n", child, G_OBJECT_TYPE_NAME(child));
#endif /*DEBUG*/

	viewchild = slist_map(parent->managed,
		(SListMapFn) view_viewchild_child_model_equal, child_model);

	g_assert(child->parent);
	g_assert(child->parent == parent);
	child->parent = NULL;

    if (viewchild &&
        viewchild->child_view == child)
        viewchild->child_view = NULL;
}

static void
view_real_child_position(View *parent, View *child)
{
#ifdef DEBUG
	printf("view_real_child_position: parent %s, child %s\n",
		G_OBJECT_TYPE_NAME(parent), G_OBJECT_TYPE_NAME(child));
#endif /*DEBUG*/
}

static void
view_real_child_front(View *parent, View *child)
{
#ifdef DEBUG
	printf("view_real_child_front: parent %s, child %s\n",
		G_OBJECT_TYPE_NAME(parent), G_OBJECT_TYPE_NAME(child));
#endif /*DEBUG*/
}

static void
view_real_reset(View *view)
{
	view_resettable_unregister(view);
}

static void *
view_real_scan(View *view)
{
	Model *model = MODEL(VOBJECT(view)->iobject);

	Heapmodel *heapmodel;

	view_scannable_unregister(view);

	/* If we've changed something in this model, mark it for recomp.
	 */
	if (model && IS_HEAPMODEL(model) &&
		(heapmodel = HEAPMODEL(model))->modified &&
		heapmodel->row) {
		Expr *expr = heapmodel->row->expr;

		if (expr)
			(void) expr_dirty(expr, link_serial_new());
	}

	return NULL;
}

static void
view_class_init(ViewClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	gobject_class->finalize = view_finalize;
	gobject_class->dispose = view_dispose;

	/* Create signals.
	 */

	/* Init default methods.
	 */
	class->link = view_real_link;
	class->child_add = view_real_child_add;
	class->child_remove = view_real_child_remove;
	class->child_position = view_real_child_position;
	class->child_front = view_real_child_front;
	class->display = NULL;

	class->reset = view_real_reset;
	class->scan = view_real_scan;
	class->scrollto = NULL;
	class->layout = NULL;
}

static void
view_init(View *view)
{
#ifdef DEBUG_LEAK
	all_view_widgets = g_slist_prepend(all_view_widgets, view);
#endif /*DEBUG_LEAK*/
}

/* Trigger the reset method for a view.
 */
void *
view_reset(View *view)
{
	ViewClass *view_class = VIEW_GET_CLASS(view);

	if (view_class->reset)
		view_class->reset(view);

	return NULL;
}

/* Trigger the scan method for a view.
 */
void *
view_scan(View *view)
{
	ViewClass *view_class = VIEW_GET_CLASS(view);

	if (view_class->scan)
		return view_class->scan(view);

	return NULL;
}

/* Trigger the scrollto method for a view.
 */
void *
view_scrollto(View *view, ModelScrollPosition position)
{
	ViewClass *view_class = VIEW_GET_CLASS(view);

	if (view_class->scrollto)
		view_class->scrollto(view, position);

	return NULL;
}

/* Trigger the layout method for a view.
 */
void *
view_layout(View *view)
{
	ViewClass *view_class = VIEW_GET_CLASS(view);

	if (view_class->layout)
		view_class->layout(view);

	return NULL;
}

static void *
view_map_sub(ViewChild *viewchild, view_map_fn fn, void *a, void *b)
{
	if (viewchild->child_view)
		return fn(viewchild->child_view, a, b);

	return NULL;
}

/* Map over a view's children.
 */
void *
view_map(View *view, view_map_fn fn, void *a, void *b)
{
	return slist_map3(view->managed,
		(SListMap3Fn) view_map_sub, (void *) fn, a, b);
}

/* Apply a function to view, and to all its children.
 */
void *
view_map_all(View *view, view_map_fn fn, void *a)
{
	View *result;

	if ((result = fn(view, a, NULL)))
		return result;

	return view_map(view, (view_map_fn) view_map_all, (void *) fn, a);
}

void
view_save_as_cb(GtkWidget *menu, GtkWidget *host, View *view)
{
	Model *model = MODEL(VOBJECT(view)->iobject);

	if (IS_FILEMODEL(model)) {
		GtkWindow *window = view_get_window(view);

		printf("view_save_as_cb: FIXME\n");
		// filemodel_inter_saveas(win, FILEMODEL(model));
	}
}

void
view_save_cb(GtkWidget *menu, GtkWidget *host, View *view)
{
	Model *model = MODEL(VOBJECT(view)->iobject);

	if (IS_FILEMODEL(model)) {
		GtkWindow *window = view_get_window(view);

		printf("view_save_cb: FIXME\n");
		// filemodel_inter_save(win, FILEMODEL(model));
	}
}

void
view_close_cb(GtkWidget *menu, GtkWidget *host, View *view)
{
	Model *model = MODEL(VOBJECT(view)->iobject);

	if (IS_FILEMODEL(model)) {
		GtkWindow *window = view_get_window(view);

		printf("view_close_cb: FIXME\n");
		// filemodel_inter_savenclose(win, FILEMODEL(model));
	}
}

/* Callback for "activate" on a view.
 */
void
view_activate_cb(View *view)
{
	view_scannable_register(view);
	symbol_recalculate_all();
}

/* Callback for "changed" on a view.
 */
void
view_changed_cb(View *view)
{
	/* Make sure it's on the scannable list.
	 */
	view_scannable_register(view);
}

void
view_not_implemented_cb(GtkWidget *menu, GtkWidget *host, View *view)
{
	error_top(_("Not implemented"));
	mainwindow_error(MAINWINDOW(view_get_window(view)));
}

GtkWindow *
view_get_window(View *view)
{
	while (view->parent)
		view = view->parent;

	return GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
}

Columnview *
view_get_columnview(View *child)
{
	View *view;

	for (view = child; view && !IS_COLUMNVIEW(view); view = view->parent)
		;

	if (!view)
		return NULL;

	return COLUMNVIEW(view);
}

