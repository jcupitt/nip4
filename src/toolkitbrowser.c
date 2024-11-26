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

static GQuark toolkitbrowser_quark = 0;
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

G_DEFINE_TYPE(Toolkitbrowser, toolkitbrowser, VOBJECT_TYPE);

static void
toolkitbrowser_dispose(GObject *object)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(object);

	gtk_widget_dispose_template(GTK_WIDGET(toolkitbrowser),
		TOOLKITBROWSER_TYPE);

	VIPS_FREEF(g_slist_free, toolkitbrowser->page_names);

	G_OBJECT_CLASS(toolkitbrowser_parent_class)->dispose(object);
}

/* Build a page in the tree view.
 */

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

static GListModel *
toolkitbrowser_build_node(Toolkitbrowser *toolkitbrowser, Node *parent)
{
	GListStore *store = g_list_store_new(NODE_TYPE);

	// make "go to parent" the first item
	if (parent)
		g_list_store_append(store, G_OBJECT(parent));

	if (!parent &&
		toolkitbrowser->kitg)
		// make the root store
		toolkitgroup_map(toolkitbrowser->kitg,
			toolkitbrowser_build_kit, toolkitbrowser, store);
	else if (parent &&
		parent->kit)
		// expand the kit to a new store
		toolkit_map(parent->kit,
			toolkitbrowser_build_tool, toolkitbrowser, store);
	else if (parent &&
		parent->toolitem &&
		parent->toolitem->is_pullright)
		// expand the pullright to a new store
		slist_map2(parent->toolitem->children,
			(SListMap2Fn) toolkitbrowser_build_toolitem,
			toolkitbrowser, store);

	return G_LIST_MODEL(store);
}

static void
toolkitbrowser_setup_browse_item(GtkListItemFactory *factory, GtkListItem *item)
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

	Toolkitbrowser *toolkitbrowser =
		g_object_get_qdata(G_OBJECT(factory), toolkitbrowser_quark);
	g_object_set_qdata(G_OBJECT(button), toolkitbrowser_quark, toolkitbrowser);

	gtk_list_item_set_child(item, button);
}

static void
toolkitbrowser_activate(Toolkitbrowser *toolkitbrowser, Toolitem *toolitem)
{
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
toolkitbrowser_build_browse_page(Toolkitbrowser *toolkitbrowser, Node *parent);

static void
toolkitbrowser_browse_clicked(GtkWidget *button,
	Toolkitbrowser *toolkitbrowser)
{
	Node *node = g_object_get_qdata(G_OBJECT(button), node_quark);

	printf("toolkitbrowser_browse_clicked:\n");

	if (node->toolitem &&
		!node->toolitem->is_separator &&
		!node->toolitem->is_pullright &&
		node->toolitem->action)
		toolkitbrowser_activate(toolkitbrowser, node->toolitem);

	if (node->kit ||
		(node->toolitem && node->toolitem->is_pullright))
		toolkitbrowser_build_browse_page(toolkitbrowser, node);
}

static void
toolkitbrowser_browse_back_clicked(GtkWidget *button,
	Toolkitbrowser *toolkitbrowser)
{
	printf("toolkitbrowser_browse_back_clicked:\n");

	Node *parent = g_object_get_qdata(G_OBJECT(button), node_quark);

	GtkWidget *box = gtk_button_get_child(GTK_BUTTON(button));
	GtkWidget *left = gtk_widget_get_first_child(box);
	GtkWidget *label = gtk_widget_get_next_sibling(left);
	GtkWidget *right = gtk_widget_get_next_sibling(label);

	const char *last_name =
		(const char *) g_slist_last(toolkitbrowser->page_names)->data;
	toolkitbrowser->page_names =
		g_slist_remove(toolkitbrowser->page_names, last_name);
	const char *name =
		(const char *) g_slist_last(toolkitbrowser->page_names)->data;

	GtkWidget *last_page =
		gtk_stack_get_visible_child(GTK_STACK(toolkitbrowser->stack));

	gtk_stack_set_visible_child_name(GTK_STACK(toolkitbrowser->stack),
		name);

	gtk_stack_remove(GTK_STACK(toolkitbrowser->stack), last_page);
}

static void
toolkitbrowser_bind_browse_item(GtkListItemFactory *factory, GtkListItem *item)
{
	Toolkitbrowser *toolkitbrowser =
		g_object_get_qdata(G_OBJECT(factory), toolkitbrowser_quark);

	GtkWidget *button = gtk_list_item_get_child(item);
	Node *node = gtk_list_item_get_item(item);
	Node *parent = g_object_get_qdata(G_OBJECT(factory), node_quark);

	GtkWidget *box = gtk_button_get_child(GTK_BUTTON(button));
	GtkWidget *left = gtk_widget_get_first_child(box);
	GtkWidget *label = gtk_widget_get_next_sibling(left);
	GtkWidget *right = gtk_widget_get_next_sibling(label);

	gtk_label_set_label(GTK_LABEL(label), node_get_name(node));

	if (parent &&
		gtk_list_item_get_position(item) == 0) {
		gtk_widget_set_visible(left, TRUE);
		gtk_widget_set_visible(right, FALSE);
		gtk_label_set_xalign(GTK_LABEL(label), 0.5);
		g_object_set_qdata(G_OBJECT(button), node_quark, parent);

		g_signal_connect(button, "clicked",
			G_CALLBACK(toolkitbrowser_browse_back_clicked), toolkitbrowser);
	}
	else {
		gtk_widget_set_visible(right,
			node->kit ||
			(node->toolitem && node->toolitem->is_pullright));

		if (node->toolitem)
			set_tooltip(button, node->toolitem->help);

		g_object_set_qdata(G_OBJECT(button), node_quark, node);

		g_signal_connect(button, "clicked",
			G_CALLBACK(toolkitbrowser_browse_clicked), toolkitbrowser);
	}

	if (node->toolitem &&
		node->toolitem->is_separator)
		gtk_widget_add_css_class(button, "toolkitbrowser-separator");
	else
		gtk_widget_add_css_class(button, "toolkitbrowser-button");

	gtk_widget_remove_css_class(gtk_widget_get_parent(button), "activatable");
}

static void
toolkitbrowser_fill_browse_page(Toolkitbrowser *toolkitbrowser,
	Node *this, GtkWidget *list_view)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_object_set_qdata(G_OBJECT(factory), toolkitbrowser_quark, toolkitbrowser);
	g_object_set_qdata(G_OBJECT(factory), node_quark, this);
	g_signal_connect(factory, "setup",
		G_CALLBACK(toolkitbrowser_setup_browse_item), NULL);
	g_signal_connect(factory, "bind",
		G_CALLBACK(toolkitbrowser_bind_browse_item), NULL);
	gtk_list_view_set_factory(GTK_LIST_VIEW(list_view), factory);

	GListModel *model = toolkitbrowser_build_node(toolkitbrowser, this);
	GtkNoSelection *selection = gtk_no_selection_new(G_LIST_MODEL(model));
	gtk_list_view_set_model(GTK_LIST_VIEW(list_view),
			GTK_SELECTION_MODEL(selection));

}

static void
toolkitbrowser_build_browse_page(Toolkitbrowser *toolkitbrowser,
	Node *this)
{
	const char *name = node_get_name(this);

	GtkWidget *scrolled_window = gtk_scrolled_window_new();
	gtk_stack_add_named(GTK_STACK(toolkitbrowser->stack),
		scrolled_window, name);
	toolkitbrowser->page_names =
		g_slist_append(toolkitbrowser->page_names, (void *) name);

	GtkWidget *list_view = gtk_list_view_new(NULL, NULL);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window),
		list_view);

	toolkitbrowser_fill_browse_page(toolkitbrowser, this, list_view);

	gtk_stack_set_visible_child(GTK_STACK(toolkitbrowser->stack),
		scrolled_window);
}

/* Build the flat search list.
 */

static void *
toolkitbrowser_build_toolitem_list(Toolitem *toolitem, void *a)
{
	GListStore *store = G_LIST_STORE(a);

	if (toolitem->tool &&
		MODEL(toolitem->tool)->display &&
		!toolitem->is_separator)
		g_list_store_append(store, G_OBJECT(node_new(NULL, toolitem)));

	return NULL;
}

static void *
toolkitbrowser_build_tool_list(Tool *tool, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);

	if (MODEL(tool)->display &&
		tool->toolitem)
		slist_map(tool->toolitem->children,
			(SListMapFn) toolkitbrowser_build_toolitem_list, store);

	return NULL;
}

static void *
toolkitbrowser_build_kit_list(Toolkit *kit, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);

	if (kit &&
		MODEL(kit)->display &&
		IOBJECT(kit)->name)
		toolkit_map(kit,
			toolkitbrowser_build_tool_list, store, NULL);

	return NULL;
}

static GListModel *
toolkitbrowser_create_list(Toolkitbrowser *toolkitbrowser)
{
	GListStore *store;

	store = g_list_store_new(NODE_TYPE);

	toolkitgroup_map(toolkitbrowser->kitg,
		toolkitbrowser_build_kit_list, store, NULL);

	return G_LIST_MODEL(store);
}

static void
toolkitbrowser_refresh(vObject *vobject)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(vobject);

#ifdef DEBUG
	printf("toolkitbrowser_refresh:\n");
#endif /*DEBUG*/

	// remove all stack pages except the first
	GtkWidget *stack = toolkitbrowser->stack;
	GtkWidget *root_page = gtk_widget_get_first_child(stack);
	if (root_page) {
		GtkWidget *child;

		while ((child = gtk_widget_get_next_sibling(root_page)))
			gtk_stack_remove(GTK_STACK(stack), child);
	}
	VIPS_FREEF(g_slist_free, toolkitbrowser->page_names);
	toolkitbrowser->page_names =
		g_slist_append(toolkitbrowser->page_names, (void *) "root");

	if (toolkitbrowser->search_mode) {
		// build the flat search list
		GListModel *list_model = toolkitbrowser_create_list(toolkitbrowser);

		GtkExpression *expression =
			gtk_property_expression_new(NODE_TYPE, NULL, "search-text");
		toolkitbrowser->filter = gtk_string_filter_new(expression);
		GtkFilterListModel *filter_model =
			gtk_filter_list_model_new(G_LIST_MODEL(list_model),
					GTK_FILTER(toolkitbrowser->filter));

		GtkNoSelection *selection =
			gtk_no_selection_new(G_LIST_MODEL(filter_model));
		gtk_list_view_set_model(GTK_LIST_VIEW(toolkitbrowser->list_view),
			GTK_SELECTION_MODEL(selection));
	}
	else {
		// build the nested menu
		toolkitbrowser_fill_browse_page(toolkitbrowser,
			NULL, toolkitbrowser->list_view);
		toolkitbrowser->filter = NULL;
	}

	VOBJECT_CLASS(toolkitbrowser_parent_class)->refresh(vobject);
}

static void
toolkitbrowser_link(vObject *vobject, iObject *iobject)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(vobject);
	Toolkitgroup *kitg = TOOLKITGROUP(iobject);

#ifdef DEBUG
	printf("toolkitbrowser_link:\n");
#endif /*DEBUG*/

	g_assert(!toolkitbrowser->kitg);

	toolkitbrowser->kitg = kitg;

	// do we need this?
	vobject_refresh_queue(VOBJECT(toolkitbrowser));

	VOBJECT_CLASS(toolkitbrowser_parent_class)->link(vobject, iobject);
}

static void
toolkitbrowser_list_view_activate(GtkListView *self,
	guint position, gpointer user_data)
{
	Toolkitbrowser *toolkitbrowser = TOOLKITBROWSER(user_data);

	/*
	Node *node =
		NODE(gtk_single_selection_get_selected_item(toolkitbrowser->selection));
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
	*/
}

static void
toolkitbrowser_set_search_mode(Toolkitbrowser *toolkitbrowser,
	gboolean search_mode)
{
	if (toolkitbrowser->search_mode != search_mode) {
		toolkitbrowser->search_mode = search_mode;

		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(toolkitbrowser->search_toggle), search_mode);

		if (search_mode)
			gtk_widget_grab_focus(toolkitbrowser->search_entry);
		else {
			GtkEntryBuffer *buffer =
				gtk_entry_get_buffer(GTK_ENTRY(toolkitbrowser->search_entry));

			g_signal_handlers_block_matched(
				G_OBJECT(toolkitbrowser->search_entry),
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, toolkitbrowser);
			gtk_entry_buffer_set_text(buffer, "", 0);
			g_signal_handlers_unblock_matched(
				G_OBJECT(toolkitbrowser->search_entry),
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, toolkitbrowser);

			gtk_widget_grab_focus(toolkitbrowser->search_toggle);
		}

		toolkitbrowser_refresh(VOBJECT(toolkitbrowser));
	}
}

static void
toolkitbrowser_search_toggled(GtkToggleButton *button,
	Toolkitbrowser *toolkitbrowser)
{
#ifdef DEBUG
	printf("toolkitbrowser_search_toggled:\n");
#endif /*DEBUG*/

	toolkitbrowser_set_search_mode(toolkitbrowser,
		gtk_toggle_button_get_active(button));
}

static gboolean
toolkitbrowser_key_pressed(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state,
	Toolkitbrowser *toolkitbrowser)
{
	gboolean handled;

#ifdef DEBUG
	printf("toolkitbrowser_key_pressed:\n");
#endif /*DEBUG*/

	handled = FALSE;

	if (keyval == GDK_KEY_Escape) {
		toolkitbrowser_set_search_mode(toolkitbrowser, FALSE);
		handled = TRUE;
	}

	return handled;
}

static void
toolkitbrowser_focus_enter(GtkEventControllerFocus *self,
	Toolkitbrowser *toolkitbrowser)
{
	toolkitbrowser_set_search_mode(toolkitbrowser, TRUE);
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

	BIND_VARIABLE(Toolkitbrowser, stack);
	BIND_VARIABLE(Toolkitbrowser, search_toggle);
	BIND_VARIABLE(Toolkitbrowser, search_entry);
	BIND_VARIABLE(Toolkitbrowser, list_view);

	BIND_CALLBACK(toolkitbrowser_list_view_activate);
	BIND_CALLBACK(toolkitbrowser_search_toggled);
	BIND_CALLBACK(toolkitbrowser_key_pressed);
	BIND_CALLBACK(toolkitbrowser_focus_enter);

	toolkitbrowser_quark = g_quark_from_static_string("toolkitbrowser-quark");
	node_quark = g_quark_from_static_string("node-quark");
}

static void
toolkitbrowser_setup_listitem(GtkListItemFactory *factory,
	GtkListItem *list_item)
{
	GtkWidget *button = gtk_button_new();
	gtk_widget_add_css_class(button, "toolkitbrowser-button");
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);

	GtkWidget *label = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_button_set_child(GTK_BUTTON(button), label);

	Toolkitbrowser *toolkitbrowser =
		g_object_get_qdata(G_OBJECT(factory), toolkitbrowser_quark);
	g_object_set_qdata(G_OBJECT(button), toolkitbrowser_quark, toolkitbrowser);

	gtk_list_item_set_child(list_item, button);
}

static void
toolkitbrowser_bind_listitem(GtkListItemFactory *factory,
	GtkListItem *list_item)
{
	Toolkitbrowser *toolkitbrowser =
		g_object_get_qdata(G_OBJECT(factory), toolkitbrowser_quark);

	GtkWidget *button = gtk_list_item_get_child(list_item);
	Node *node = gtk_list_item_get_item(list_item);

	g_object_set_qdata(G_OBJECT(button), node_quark, node);

	GtkWidget *label = gtk_button_get_child(GTK_BUTTON(button));
	gtk_label_set_label(GTK_LABEL(label), node->toolitem->user_path);
	set_tooltip(button, node->toolitem->help);

	g_signal_connect(button, "clicked",
		G_CALLBACK(toolkitbrowser_browse_clicked), toolkitbrowser);
}

static void
toolkitbrowser_entry_length_notify(GtkWidget *widget,
	GParamSpec *pspec, Toolkitbrowser *toolkitbrowser)
{
#ifdef DEBUG
	printf("ientry_length_notify:\n");
#endif /*DEBUG*/

	//toolkitbrowser_set_search_mode(toolkitbrowser, TRUE);

	if (toolkitbrowser->filter)
		gtk_string_filter_set_search(toolkitbrowser->filter,
			gtk_editable_get_text(GTK_EDITABLE(toolkitbrowser->search_entry)));
}

static void
toolkitbrowser_init(Toolkitbrowser *toolkitbrowser)
{
	gtk_widget_init_template(GTK_WIDGET(toolkitbrowser));

	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_object_set_qdata(G_OBJECT(factory), toolkitbrowser_quark, toolkitbrowser);
	g_signal_connect(factory, "setup",
		G_CALLBACK(toolkitbrowser_setup_listitem), NULL);
	g_signal_connect(factory, "bind",
		G_CALLBACK(toolkitbrowser_bind_listitem), NULL);
	gtk_list_view_set_factory(GTK_LIST_VIEW(toolkitbrowser->list_view),
		factory);

	GtkEntryBuffer *buffer =
		gtk_entry_get_buffer(GTK_ENTRY(toolkitbrowser->search_entry));
	g_signal_connect(buffer, "notify::length",
		G_CALLBACK(toolkitbrowser_entry_length_notify), toolkitbrowser);
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
