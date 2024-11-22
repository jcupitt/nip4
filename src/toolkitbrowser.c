/* Toolkitbrowser dialog.
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
 */
#define DEBUG

#include "nip4.h"

/* A node in our tree ... a kit, or a pointer to a toolitem linked to a tool.
 * Toolitems can be computred, pullrights, separators, etc.
 *
 * Node objects are made and unreffed as you expand and collapse listview
 * expanders, and completely rebuilt on any toolkitchange.
 */

typedef struct _Node {
	GObject parent_instance;

	// not refs ... we rebuild all these things on any toolkit change
	Toolkit *kit;
	Toolitem *toolitem;
} Node;

typedef struct _NodeClass {
	GObjectClass parent_class;

} NodeClass;

G_DEFINE_TYPE(Node, node, G_TYPE_OBJECT);

#define NODE_TYPE (node_get_type())
#define NODE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NODE_TYPE, Node))

enum {
	PROP_NAME = 1,
};

static void
node_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Node *node = NODE(object);

	switch (prop_id) {
	case PROP_NAME:
		// Name to be displayed, or NULL for no display
		if (node->kit &&
			MODEL(node->kit)->display &&
			IOBJECT(node->kit)->name)
			g_value_set_string(value, IOBJECT(node->kit)->name);
		else if (node->toolitem &&
			MODEL(node->toolitem->tool)->display &&
			!node->toolitem->is_separator &&
			node->toolitem->name)
			g_value_set_string(value, node->toolitem->name);
		else
			g_value_set_string(value, NULL);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
node_class_init(NodeClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->get_property = node_get_property;

	g_object_class_install_property(gobject_class, PROP_NAME,
		g_param_spec_string("name",
			_("Name"),
			_("Display name, or NULL"),
			NULL,
			G_PARAM_READABLE));
}

static void
node_init(Node *node)
{
}

Node *
node_new(Toolkit *kit, Toolitem *toolitem)
{
	Node *node = g_object_new(NODE_TYPE, NULL);

	g_assert(kit || toolitem);
	g_assert(!(kit && toolitem));

	node->kit = kit;
	node->toolitem = toolitem;

	return node;
}

G_DEFINE_TYPE(Toolkitbrowser, toolkitbrowser, VOBJECT_TYPE);

static void
toolkitbrowser_dispose(GObject *object)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(object);

	gtk_widget_dispose_template(GTK_WIDGET(toolkitbrowser),
		TOOLKITBROWSER_TYPE);

	VIPS_UNREF(toolkitbrowser->treemodel);
	VIPS_UNREF(toolkitbrowser->filter);

	G_OBJECT_CLASS(toolkitbrowser_parent_class)->dispose(object);
}

static void *
toolkitbrowser_build_kit(Toolkit *kit, void *a, void *b)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(a);
	GListStore *store = G_LIST_STORE(b);

	printf("toolkitbrowser_build_kit: %s\n", IOBJECT(kit)->name);

	if (kit &&
		MODEL(kit)->display &&
		IOBJECT(kit)->name)
		g_list_store_append(store, G_OBJECT(node_new(kit, NULL)));

	return NULL;
}

static void *
toolkitbrowser_build_tool(Tool *tool, void *a, void *b)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(a);
	GListStore *store = G_LIST_STORE(b);

	printf("toolkitbrowser_build_tool: %s\n", IOBJECT(tool)->name);

	if (MODEL(tool)->display &&
		tool->toolitem)
		g_list_store_append(store, G_OBJECT(node_new(NULL, tool->toolitem)));

	return NULL;
}

static void *
toolkitbrowser_build_toolitem(Toolitem *toolitem, void *a, void *b)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(a);
	GListStore *store = G_LIST_STORE(b);

	printf("toolkitbrowser_build_toolitem: %s\n", toolitem->name);

	if (toolitem->tool &&
		MODEL(toolitem->tool)->display &&
		!toolitem->is_separator)
		g_list_store_append(store, G_OBJECT(node_new(NULL, toolitem)));

	return NULL;
}

/* Make a layer in the tree of nodes.
 */
static GListModel *
toolkitbrowser_create_model(void *a, void *b)
{
	Node *node = NODE(a);
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(b);

	GListStore *store;

	store = NULL;

	if (!node &&
		toolkitbrowser->kitg) {
		// make the root store
		store = g_list_store_new(NODE_TYPE);
		toolkitgroup_map(toolkitbrowser->kitg,
			toolkitbrowser_build_kit, toolkitbrowser, store);
	}
	else if (node &&
		node->kit) {
		// expand the kit to a new store
		store = g_list_store_new(NODE_TYPE);
		toolkit_map(node->kit,
			toolkitbrowser_build_tool, toolkitbrowser, store);
	}
	else if (node &&
		node->toolitem &&
		node->toolitem->is_pullright) {
		// expand the pullright to a new store
		store = g_list_store_new(NODE_TYPE);
		slist_map2(node->toolitem->children,
			(SListMap2Fn) toolkitbrowser_build_toolitem,
			toolkitbrowser, store);
	}

	return G_LIST_MODEL(store);
}

static void
toolkitbrowser_refresh(vObject *vobject)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(vobject);

#ifdef DEBUG
	printf("toolkitbrowser_refresh:\n");
#endif /*DEBUG*/

	VIPS_UNREF(toolkitbrowser->treemodel);

	GListModel *model = toolkitbrowser_create_model(NULL, toolkitbrowser);
	toolkitbrowser->treemodel = gtk_tree_list_model_new(model,
		FALSE, FALSE, toolkitbrowser_create_model,
		toolkitbrowser, NULL);

	GtkExpression *row_expression =
		gtk_property_expression_new(GTK_TYPE_TREE_LIST_ROW, NULL, "item");
	GtkExpression *expression =
		gtk_property_expression_new(NODE_TYPE, row_expression, "name");
	toolkitbrowser->filter = gtk_string_filter_new(expression);
	GtkFilterListModel *filter_model =
		gtk_filter_list_model_new(G_LIST_MODEL(toolkitbrowser->treemodel),
				GTK_FILTER(toolkitbrowser->filter));

	toolkitbrowser->selection =
		gtk_single_selection_new(G_LIST_MODEL(filter_model));

	gtk_list_view_set_model(GTK_LIST_VIEW(toolkitbrowser->list_view),
		GTK_SELECTION_MODEL(toolkitbrowser->selection));

	VOBJECT_CLASS(toolkitbrowser_parent_class)->refresh(vobject);
}

static void
toolkitbrowser_link(vObject *vobject, iObject *iobject)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(vobject);
	Toolkitgroup *kitg = TOOLKITGROUP(iobject);

	g_assert(!toolkitbrowser->kitg);

	toolkitbrowser->kitg = kitg;

	// do we need this?
	toolkitbrowser_refresh(VOBJECT(toolkitbrowser));

	VOBJECT_CLASS(toolkitbrowser_parent_class)->link(vobject, iobject);
}

static void
toolkitbrowser_activate(GtkListView *self, guint position, gpointer user_data)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(user_data);
	GObject *row =
		gtk_single_selection_get_selected_item(toolkitbrowser->selection);
	Node *node = NODE(gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(row)));
	Toolitem *toolitem = node->toolitem;

	if (toolitem &&
		!toolitem->is_separator &&
		!toolitem->is_pullright &&
		toolitem->action) {
		if (!workspace_add_action(toolkitbrowser->ws,
			toolitem->name, toolitem->action,
			toolitem->action_sym->expr->compile->nparam))
			workspace_set_show_error(toolkitbrowser->ws, TRUE);
	}
}

static void
toolkitbrowser_set_expand(Toolkitbrowser *toolkitbrowser, gboolean expand)
{
}

static void
toolkitbrowser_search_changed(GtkSearchEntry *entry, void *a)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(a);

#ifdef DEBUG
	printf("toolkitbrowser_search_changed:\n");
#endif /*DEBUG*/

	const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

	toolkitbrowser_set_expand(toolkitbrowser, strlen(text) > 0);

	gtk_string_filter_set_search(toolkitbrowser->filter, text);
}

static void
toolkitbrowser_class_init(ToolkitbrowserClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	gobject_class->dispose = toolkitbrowser_dispose;

	vobject_class->refresh = toolkitbrowser_refresh;
	vobject_class->link = toolkitbrowser_link;

	BIND_RESOURCE("toolkitbrowser.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Toolkitbrowser, top);
	BIND_VARIABLE(Toolkitbrowser, search_entry);
	BIND_VARIABLE(Toolkitbrowser, list_view);

	BIND_CALLBACK(toolkitbrowser_activate);
	BIND_CALLBACK(toolkitbrowser_search_changed);

}

static void
toolkitbrowser_init(Toolkitbrowser *toolkitbrowser)
{
	gtk_widget_init_template(GTK_WIDGET(toolkitbrowser));

}

Toolkitbrowser *
toolkitbrowser_new(void)
{
	Toolkitbrowser *toolkitbrowser = g_object_new(TOOLKITBROWSER_TYPE, NULL);

	return toolkitbrowser;
}

void
toolkitbrowser_set_workspace(Toolkitbrowser *toolkitbrowser, Workspace *ws)
{
	g_assert(!toolkitbrowser->ws);

	toolkitbrowser->ws = ws;
}
