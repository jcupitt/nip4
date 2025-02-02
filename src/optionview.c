/* run the display for a option in a workspace
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

G_DEFINE_TYPE(Optionview, optionview, GRAPHICVIEW_TYPE)

/* Copy a gslist of strings.
 */
static GSList *
lstring_copy(GSList *lstring)
{
	GSList *new;

	new = NULL;
	for (GSList *p = lstring; p; p = p->next)
		new = g_slist_prepend(new, g_strdup((const char *) p->data));

	new = g_slist_reverse(new);

	return new;
}

/* Are two lstrings equal?
 */
static gboolean
lstring_equal(GSList *a, GSList *b)
{
	for (; a && b; a = a->next, b = b->next)
		if (!g_str_equal((const char *) a->data, (const char *) b->data))
			return FALSE;

	if (a || b)
		return FALSE;

	return TRUE;
}

/* slist -> null-terminated array
 */
static const char *const *
lstring_to_array(GSList *a)
{
	int n = g_slist_length(a);

	char **array = VIPS_ARRAY(NULL, n + 1, char *);
	GSList *p;
	int i;
	for (p = a, i = 0; p; p = p->next, i++)
		array[i] = (char *) p->data;
	array[n] = NULL;

	return (const char *const *) array;
}

static void
optionview_dispose(GObject *object)
{
	Optionview *optionview;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_OPTIONVIEW(object));

	optionview = OPTIONVIEW(object);

	VIPS_FREEF(gtk_widget_unparent, optionview->top);
	g_slist_free_full(g_steal_pointer(&optionview->labels), g_free);

	G_OBJECT_CLASS(optionview_parent_class)->dispose(object);
}

static void
optionview_link(View *view, Model *model, View *parent)
{
	Optionview *optionview = OPTIONVIEW(view);

	VIEW_CLASS(optionview_parent_class)->link(view, model, parent);

	if (GRAPHICVIEW(view)->sview)
		gtk_size_group_add_widget(GRAPHICVIEW(view)->sview->group,
			optionview->label);
}

static void
optionview_notify_selected(GObject *self, GParamSpec *pspec, void *user_data)
{
	Optionview *optionview = OPTIONVIEW(user_data);
	Option *option = OPTION(VOBJECT(optionview)->iobject);
	Classmodel *classmodel = CLASSMODEL(option);
	const int new_value =
		gtk_drop_down_get_selected(GTK_DROP_DOWN(optionview->options));

	if (option->value != new_value) {
		option->value = new_value;

		classmodel_update_view(classmodel);
		symbol_recalculate_all();
	}
}

static void
optionview_refresh(vObject *vobject)
{
	Optionview *optionview = OPTIONVIEW(vobject);
	Option *option = OPTION(VOBJECT(optionview)->iobject);

#ifdef DEBUG
	printf("optionview_refresh: ");
	row_name_print(HEAPMODEL(option)->row);
	printf("\n");
#endif /*DEBUG*/

	/* Only rebuild the menu if there's been a change.
	 */
	if (!lstring_equal(optionview->labels, option->labels)) {
		g_slist_free_full(g_steal_pointer(&optionview->labels), g_free);
		optionview->labels = lstring_copy(option->labels);

		g_autofree const char *const *label_array =
			lstring_to_array(optionview->labels);
		g_autoptr(GtkStringList) slist = gtk_string_list_new(label_array);

		g_signal_handlers_block_by_func(optionview->options,
			optionview_notify_selected, optionview);
		gtk_drop_down_set_model(GTK_DROP_DOWN(optionview->options),
			G_LIST_MODEL(slist));
		g_signal_handlers_unblock_by_func(optionview->options,
			optionview_notify_selected, optionview);
	}

	const int set_value =
		gtk_drop_down_get_selected(GTK_DROP_DOWN(optionview->options));
	if (set_value != option->value)
		gtk_drop_down_set_selected(GTK_DROP_DOWN(optionview->options),
			option->value);

	set_glabel(optionview->label, _("%s:"), IOBJECT(option)->caption);

	VOBJECT_CLASS(optionview_parent_class)->refresh(vobject);
}

static void
optionview_class_init(OptionviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;
	ViewClass *view_class = (ViewClass *) class;

	BIND_RESOURCE("optionview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Optionview, top);
	BIND_VARIABLE(Optionview, label);
	BIND_VARIABLE(Optionview, options);

	object_class->dispose = optionview_dispose;

	vobject_class->refresh = optionview_refresh;

	view_class->link = optionview_link;
}

static void
optionview_init(Optionview *optionview)
{
	gtk_widget_init_template(GTK_WIDGET(optionview));

	g_signal_connect(optionview->options, "notify::selected",
		G_CALLBACK(optionview_notify_selected), optionview);
}

View *
optionview_new(void)
{
	Optionview *optionview = g_object_new(OPTIONVIEW_TYPE, NULL);

	return VIEW(optionview);
}
