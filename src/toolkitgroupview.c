/* Toolkitgroupview dialog.
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

static GQuark node_quark = 0;

enum {
	// text we search for the find expression
	PROP_SEARCH_TEXT = 1,
};

static void
node_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Node *node = NODE(object);

	switch (prop_id) {
	case PROP_SEARCH_TEXT:
		if (node->kit &&
			MODEL(node->kit)->display &&
			IOBJECT(node->kit)->name)
			g_value_set_string(value, IOBJECT(node->kit)->name);
		else if (node->toolitem &&
			MODEL(node->toolitem->tool)->display &&
			!node->toolitem->is_separator &&
			node->toolitem->name)
			g_value_set_string(value, node->toolitem->help);
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

	g_object_class_install_property(gobject_class, PROP_SEARCH_TEXT,
		g_param_spec_string("search-text",
			_("Text to search for matches"),
			_("Display name, or NULL"),
			NULL,
			G_PARAM_READABLE));
}

static void
node_init(Node *node)
{
}

static Node *
node_new(Toolkit *kit, Toolitem *toolitem)
{
	Node *node = g_object_new(NODE_TYPE, NULL);

	g_assert(kit || toolitem);
	g_assert(!(kit && toolitem));

	node->kit = kit;
	node->toolitem = toolitem;

	return node;
}

static const char *
node_get_name(Node *node)
{
	return node->toolitem ?
		node->toolitem->name : IOBJECT(node->kit)->name;
}

G_DEFINE_TYPE(Toolkitgroupview, toolkitgroupview, VIEW_TYPE);

enum {
	SIG_ACTIVATE = 1,

	SIG_LAST
};

static GQuark toolkitgroupview_quark = 0;

static guint toolkitgroupview_signals[SIG_LAST] = { 0 };

static void
toolkitgroupview_activate(Toolkitgroupview *kitgview, Toolitem *toolitem)
{
#ifdef DEBUG
	printf("toolkitgroupview_activate:\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(kitgview),
		toolkitgroupview_signals[SIG_ACTIVATE], 0, toolitem);
}

static void
toolkitgroupview_dispose(GObject *object)
{
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(object);

	gtk_widget_dispose_template(GTK_WIDGET(kitgview), TOOLKITGROUPVIEW_TYPE);
	VIPS_FREEF(g_slist_free, kitgview->page_names);

	G_OBJECT_CLASS(toolkitgroupview_parent_class)->dispose(object);
}

/* Build a page in the tree view.
 */

static void *
toolkitgroupview_build_kit(Toolkit *kit, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(b);

	if (kit &&
		MODEL(kit)->display &&
		IOBJECT(kit)->name)
		g_list_store_append(store, G_OBJECT(node_new(kit, NULL)));

	return NULL;
}

static void *
toolkitgroupview_build_tool(Tool *tool, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(b);

	if (MODEL(tool)->display &&
		tool->toolitem)
		g_list_store_append(store, G_OBJECT(node_new(NULL, tool->toolitem)));

	return NULL;
}

static void *
toolkitgroupview_build_toolitem(Toolitem *toolitem, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(b);

	if (toolitem->tool &&
		MODEL(toolitem->tool)->display &&
		!toolitem->is_separator)
		g_list_store_append(store, G_OBJECT(node_new(NULL, toolitem)));

	return NULL;
}

static GListModel *
toolkitgroupview_build_node(Toolkitgroupview *kitgview, Node *parent)
{
	Toolkitgroup *kitg = TOOLKITGROUP(VOBJECT(kitgview)->iobject);
	GListStore *store = g_list_store_new(NODE_TYPE);

	// make "go to parent" the first item
	if (parent)
		g_list_store_append(store, G_OBJECT(parent));

	if (!parent &&
		kitg)
		// make the root store
		toolkitgroup_map(kitg, toolkitgroupview_build_kit, kitgview, store);
	else if (parent &&
		parent->kit)
		// expand the kit to a new store
		toolkit_map(parent->kit,
			toolkitgroupview_build_tool, kitgview, store);
	else if (parent &&
		parent->toolitem &&
		parent->toolitem->is_pullright)
		// expand the pullright to a new store
		slist_map2(parent->toolitem->children,
			(SListMap2Fn) toolkitgroupview_build_toolitem,
			kitgview, store);

	return G_LIST_MODEL(store);
}

static void
toolkitgroupview_setup_browse_item(GtkListItemFactory *factory,
	GtkListItem *item)
{
	GtkWidget *button = gtk_button_new();
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_button_set_child(GTK_BUTTON(button), box);

	GtkWidget *left = gtk_image_new();
	gtk_image_set_from_icon_name(GTK_IMAGE(left), "go-previous-symbolic");
	gtk_widget_set_visible(left, FALSE);
	gtk_box_append(GTK_BOX(box), left);

	GtkWidget *label = gtk_label_new("");
	gtk_widget_set_hexpand(label, TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_append(GTK_BOX(box), label);

	GtkWidget *right = gtk_image_new();
	gtk_image_set_from_icon_name(GTK_IMAGE(right), "go-next-symbolic");
	gtk_box_append(GTK_BOX(box), right);

	Toolkitgroupview *kitgview =
		g_object_get_qdata(G_OBJECT(factory), toolkitgroupview_quark);
	g_object_set_qdata(G_OBJECT(button), toolkitgroupview_quark, kitgview);

	gtk_list_item_set_child(item, button);
}

static void
toolkitgroupview_build_browse_page(Toolkitgroupview *kitgview, Node *parent);

static void
toolkitgroupview_browse_clicked(GtkWidget *button,
	Toolkitgroupview *kitgview)
{
	Node *node = g_object_get_qdata(G_OBJECT(button), node_quark);

	toolkitgroupview_activate(kitgview, node->toolitem);

	if (node->kit ||
		(node->toolitem && node->toolitem->is_pullright))
		toolkitgroupview_build_browse_page(kitgview, node);
}

static void
toolkitgroupview_browse_back_clicked(GtkWidget *button,
	Toolkitgroupview *kitgview)
{
	const char *last_name =
		(const char *) g_slist_last(kitgview->page_names)->data;
	kitgview->page_names = g_slist_remove(kitgview->page_names, last_name);
	const char *name = (const char *) g_slist_last(kitgview->page_names)->data;
	GtkWidget *last_page =
		gtk_stack_get_visible_child(GTK_STACK(kitgview->stack));

	gtk_stack_set_visible_child_name(GTK_STACK(kitgview->stack), name);
	gtk_stack_remove(GTK_STACK(kitgview->stack), last_page);
}

static void
toolkitgroupview_bind_browse_item(GtkListItemFactory *factory,
	GtkListItem *item)
{
	Toolkitgroupview *kitgview =
		g_object_get_qdata(G_OBJECT(factory), toolkitgroupview_quark);

	GtkWidget *button = gtk_list_item_get_child(item);
	Node *node = gtk_list_item_get_item(item);
	Node *parent = g_object_get_qdata(G_OBJECT(factory), node_quark);

	GtkWidget *box = gtk_button_get_child(GTK_BUTTON(button));
	GtkWidget *left = gtk_widget_get_first_child(box);
	GtkWidget *label = gtk_widget_get_next_sibling(left);
	GtkWidget *right = gtk_widget_get_next_sibling(label);

	// bind can be triggered many times, we don't want to eg. attach
	// callbacks more than once
	if (g_object_get_qdata(G_OBJECT(button), node_quark))
		return;

	if (kitgview->search_mode)
		gtk_label_set_label(GTK_LABEL(label), node->toolitem->user_path);
	else if (node->kit ||
		(node->toolitem && !node->toolitem->is_separator))
		gtk_label_set_label(GTK_LABEL(label), node_get_name(node));

	if (parent &&
		gtk_list_item_get_position(item) == 0) {
		gtk_widget_set_visible(left, TRUE);
		gtk_widget_set_visible(right, FALSE);
		gtk_label_set_xalign(GTK_LABEL(label), 0.5);
		g_object_set_qdata(G_OBJECT(button), node_quark, parent);

		g_signal_connect(button, "clicked",
			G_CALLBACK(toolkitgroupview_browse_back_clicked), kitgview);
	}
	else {
		gtk_widget_set_visible(right,
			node->kit ||
			(node->toolitem && node->toolitem->is_pullright));

		if (node->toolitem)
			set_tooltip(button, node->toolitem->help);

		g_object_set_qdata(G_OBJECT(button), node_quark, node);

		g_signal_connect(button, "clicked",
			G_CALLBACK(toolkitgroupview_browse_clicked), kitgview);
	}

	gtk_widget_add_css_class(button, "toolkitgroupview-item");
	if (node->toolitem &&
		node->toolitem->is_separator)
		gtk_widget_set_sensitive(button, FALSE);

	gtk_widget_remove_css_class(gtk_widget_get_parent(button), "activatable");
}

static void
toolkitgroupview_fill_browse_page(Toolkitgroupview *kitgview,
	Node *this, GtkWidget *list_view)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_object_set_qdata(G_OBJECT(factory), toolkitgroupview_quark, kitgview);
	g_object_set_qdata(G_OBJECT(factory), node_quark, this);
	g_signal_connect(factory, "setup",
		G_CALLBACK(toolkitgroupview_setup_browse_item), NULL);
	g_signal_connect(factory, "bind",
		G_CALLBACK(toolkitgroupview_bind_browse_item), NULL);
	gtk_list_view_set_factory(GTK_LIST_VIEW(list_view), factory);

	GListModel *model = toolkitgroupview_build_node(kitgview, this);
	GtkNoSelection *selection = gtk_no_selection_new(G_LIST_MODEL(model));
	gtk_list_view_set_model(GTK_LIST_VIEW(list_view),
			GTK_SELECTION_MODEL(selection));
}

static void
toolkitgroupview_build_browse_page(Toolkitgroupview *kitgview, Node *this)
{
	const char *name = node_get_name(this);

	GtkWidget *scrolled_window = gtk_scrolled_window_new();
	gtk_stack_add_named(GTK_STACK(kitgview->stack), scrolled_window, name);
	kitgview->page_names = g_slist_append(kitgview->page_names, (void *) name);

	GtkWidget *list_view = gtk_list_view_new(NULL, NULL);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window),
		list_view);

	toolkitgroupview_fill_browse_page(kitgview, this, list_view);

	gtk_stack_set_visible_child(GTK_STACK(kitgview->stack),
		scrolled_window);
}

/* Build the flat search list.
 */

static void *
toolkitgroupview_build_toolitem_list(Toolitem *toolitem, void *a)
{
	GListStore *store = G_LIST_STORE(a);

	if (toolitem->tool &&
		MODEL(toolitem->tool)->display &&
		!toolitem->is_pullright &&
		!toolitem->is_separator)
		g_list_store_append(store, G_OBJECT(node_new(NULL, toolitem)));

	return NULL;
}

static void *
toolkitgroupview_build_tool_list(Tool *tool, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);

	if (MODEL(tool)->display &&
		tool->toolitem)
		slist_map(tool->toolitem->children,
			(SListMapFn) toolkitgroupview_build_toolitem_list, store);

	return NULL;
}

static void *
toolkitgroupview_build_kit_list(Toolkit *kit, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);

	if (kit &&
		MODEL(kit)->display &&
		IOBJECT(kit)->name)
		toolkit_map(kit, toolkitgroupview_build_tool_list, store, NULL);

	return NULL;
}

static GListModel *
toolkitgroupview_create_list(Toolkitgroupview *kitgview)
{
	Toolkitgroup *kitg = TOOLKITGROUP(VOBJECT(kitgview)->iobject);

	GListStore *store;

	store = g_list_store_new(NODE_TYPE);
	toolkitgroup_map(kitg, toolkitgroupview_build_kit_list, store, NULL);

	return G_LIST_MODEL(store);
}

static void
toolkitgroupview_refresh(vObject *vobject)
{
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(vobject);

#ifdef DEBUG
	printf("toolkitgroupview_refresh:\n");
#endif /*DEBUG*/

	// remove all stack pages except the first
	GtkWidget *stack = kitgview->stack;
	GtkWidget *root_page = gtk_widget_get_first_child(stack);
	if (root_page) {
		GtkWidget *child;

		while ((child = gtk_widget_get_next_sibling(root_page)))
			gtk_stack_remove(GTK_STACK(stack), child);
	}
	VIPS_FREEF(g_slist_free, kitgview->page_names);
	kitgview->page_names =
		g_slist_append(kitgview->page_names, (void *) "root");

	// remove any model the base list_view
	gtk_list_view_set_model(GTK_LIST_VIEW(kitgview->list_view), NULL);
	kitgview->filter = NULL;

	if (kitgview->search_mode) {
		// build the flat search list
		GListModel *list_model = toolkitgroupview_create_list(kitgview);

		GtkExpression *expression =
			gtk_property_expression_new(NODE_TYPE, NULL, "search-text");
		kitgview->filter = gtk_string_filter_new(expression);
		GtkFilterListModel *filter_model =
			gtk_filter_list_model_new(G_LIST_MODEL(list_model),
					GTK_FILTER(kitgview->filter));

		GtkNoSelection *selection =
			gtk_no_selection_new(G_LIST_MODEL(filter_model));
		gtk_list_view_set_model(GTK_LIST_VIEW(kitgview->list_view),
			GTK_SELECTION_MODEL(selection));
	}
	else {
		// build the nested menu
		toolkitgroupview_fill_browse_page(kitgview, NULL, kitgview->list_view);
		kitgview->filter = NULL;
	}

	VOBJECT_CLASS(toolkitgroupview_parent_class)->refresh(vobject);
}

static void
toolkitgroupview_set_search_mode(Toolkitgroupview *kitgview,
	gboolean search_mode)
{
	if (kitgview->search_mode != search_mode) {
		kitgview->search_mode = search_mode;

		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(kitgview->search_toggle), search_mode);

		if (search_mode)
			gtk_widget_grab_focus(kitgview->search_entry);
		else {
			GtkEntryBuffer *buffer =
				gtk_entry_get_buffer(GTK_ENTRY(kitgview->search_entry));

			g_signal_handlers_block_matched(
				G_OBJECT(kitgview->search_entry),
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, kitgview);
			gtk_entry_buffer_set_text(buffer, "", 0);
			g_signal_handlers_unblock_matched(
				G_OBJECT(kitgview->search_entry),
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, kitgview);

			gtk_widget_grab_focus(kitgview->search_toggle);
		}

		toolkitgroupview_refresh(VOBJECT(kitgview));
	}
}

static void
toolkitgroupview_search_toggled(GtkToggleButton *button,
	Toolkitgroupview *kitgview)
{
#ifdef DEBUG
	printf("toolkitgroupview_search_toggled:\n");
#endif /*DEBUG*/

	toolkitgroupview_set_search_mode(kitgview,
		gtk_toggle_button_get_active(button));
}

static gboolean
toolkitgroupview_key_pressed(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state,
	Toolkitgroupview *kitgview)
{
	gboolean handled;

#ifdef DEBUG
	printf("toolkitgroupview_key_pressed:\n");
#endif /*DEBUG*/

	handled = FALSE;

	if (keyval == GDK_KEY_Escape) {
		toolkitgroupview_set_search_mode(kitgview, FALSE);
		handled = TRUE;
	}

	return handled;
}

static void
toolkitgroupview_focus_enter(GtkEventControllerFocus *self,
	Toolkitgroupview *kitgview)
{
	toolkitgroupview_set_search_mode(kitgview, TRUE);
}

static void
toolkitgroupview_class_init(ToolkitgroupviewClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	gobject_class->dispose = toolkitgroupview_dispose;

	vobject_class->refresh = toolkitgroupview_refresh;

	BIND_RESOURCE("toolkitgroupview.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Toolkitgroupview, stack);
	BIND_VARIABLE(Toolkitgroupview, search_toggle);
	BIND_VARIABLE(Toolkitgroupview, search_entry);
	BIND_VARIABLE(Toolkitgroupview, list_view);

	BIND_CALLBACK(toolkitgroupview_search_toggled);
	BIND_CALLBACK(toolkitgroupview_key_pressed);
	BIND_CALLBACK(toolkitgroupview_focus_enter);

	toolkitgroupview_signals[SIG_ACTIVATE] = g_signal_new("activate",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	toolkitgroupview_quark =
		g_quark_from_static_string("toolkitgroupview-quark");
	node_quark = g_quark_from_static_string("node-quark");
}

static void
toolkitgroupview_entry_length_notify(GtkWidget *widget,
	GParamSpec *pspec, Toolkitgroupview *kitgview)
{
#ifdef DEBUG
	printf("toolkitgroupview_length_notify:\n");
#endif /*DEBUG*/

	if (kitgview->filter)
		gtk_string_filter_set_search(kitgview->filter,
			gtk_editable_get_text(GTK_EDITABLE(kitgview->search_entry)));
}

static void
toolkitgroupview_init(Toolkitgroupview *kitgview)
{
	gtk_widget_init_template(GTK_WIDGET(kitgview));

	GtkEntryBuffer *buffer =
		gtk_entry_get_buffer(GTK_ENTRY(kitgview->search_entry));
	g_signal_connect(buffer, "notify::length",
		G_CALLBACK(toolkitgroupview_entry_length_notify), kitgview);
}

View *
toolkitgroupview_new(void)
{
	Toolkitgroupview *kitgview = g_object_new(TOOLKITGROUPVIEW_TYPE, NULL);

	return VIEW(kitgview);
}

