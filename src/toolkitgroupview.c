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
 */
#define DEBUG

#include "nip4.h"

/* A node in our tree ... a kit, a tool, or a pointer to a toolitem linked
 * to a tool (ie. a computed tool or pullright or separator).
 *
 * Node objects are made and unreffed as you expand and collapse listview
 * expanders, and completely rebuilt on any toolkitchange.
 */

typedef struct _Node {
	GObject parent_instance;

	// not refs ... we rebuild all these things on any toolkit change
	Toolkit *kit;
	Tool *tool;
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

#ifdef DEBUG
static void
node_print(Node *node)
{
	if (node->kit)
		printf("node kit %s\n", IOBJECT(node->kit)->name);
	if (node->tool)
		printf("node tool %s\n", IOBJECT(node->tool)->name);
	if (node->toolitem)
		printf("node toolitem %s\n", IOBJECT(node->toolitem->tool)->name);
}
#endif /*DEBUG*/

static void
node_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Node *node = NODE(object);

	switch (prop_id) {
	case PROP_SEARCH_TEXT:
		if (node->kit &&
			IOBJECT(node->kit)->name)
			g_value_set_string(value, IOBJECT(node->kit)->name);
		else if (node->tool &&
			IOBJECT(node->tool)->name &&
			node->tool->help)
			g_value_set_string(value, node->tool->help);
		else if (node->tool &&
			IOBJECT(node->tool)->name)
			g_value_set_string(value, IOBJECT(node->tool)->name);
		else if (node->toolitem &&
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
node_new(Toolkit *kit, Tool *tool, Toolitem *toolitem)
{
	Node *node = g_object_new(NODE_TYPE, NULL);

	g_assert(kit || tool || toolitem);

	node->kit = kit;
	node->tool = tool;
	node->toolitem = toolitem;

#ifdef DEBUG
	node_print(node);
#endif /*DEBUG*/

	return node;
}

static const char *
node_get_name(Node *node)
{
	if (node->toolitem)
		return node->toolitem->name;
	else if (node->kit)
		return IOBJECT(node->kit)->name;
	else if (node->tool)
		return IOBJECT(node->tool)->name;
	else
		return NULL;
}

G_DEFINE_TYPE(Toolkitgroupview, toolkitgroupview, VIEW_TYPE);

enum {
	PROP_SHOW_ALL = 1,
	PROP_PATH,

	SIG_ACTIVATE,

	SIG_LAST
};

static GQuark toolkitgroupview_quark = 0;

static guint toolkitgroupview_signals[SIG_LAST] = { 0 };

static void
toolkitgroupview_activate(Toolkitgroupview *kitgview,
	Toolitem *toolitem, Tool *tool)
{
#ifdef DEBUG
	printf("toolkitgroupview_activate:\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(kitgview),
		toolkitgroupview_signals[SIG_ACTIVATE], 0, toolitem, tool);
}

static void
toolkitgroupview_dispose(GObject *object)
{
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(object);

	gtk_widget_dispose_template(GTK_WIDGET(kitgview), TOOLKITGROUPVIEW_TYPE);
	g_slist_free_full(g_steal_pointer(&kitgview->page_names), g_free);

	G_OBJECT_CLASS(toolkitgroupview_parent_class)->dispose(object);
}

static GSList *
toolkitgroupview_parse_path(char *path)
{
	GSList *page_names = NULL;

	char *p;
	char *q;
	for (p = path; (q = vips_break_token(p, ">")); p = q)
		page_names = g_slist_append(page_names, g_strdup(p));

	return page_names;
}

static void
toolkitgroupview_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(object);

	switch (prop_id) {
	case PROP_SHOW_ALL:
		kitgview->show_all = g_value_get_boolean(value);
		vobject_refresh_queue(VOBJECT(kitgview));
		break;

	case PROP_PATH:
		g_slist_free_full(g_steal_pointer(&kitgview->page_names), g_free);
		g_autofree char *path = g_strdup(g_value_get_string(value));
		kitgview->page_names = toolkitgroupview_parse_path(path);
		vobject_refresh_queue(VOBJECT(kitgview));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static char *
toolkitgroupview_print_path(GSList *page_names)
{
	GString *path = g_string_new("");

	for (GSList *p = page_names; p; p = p->next) {
		const char *name = (const char *) p->data;

		g_string_append(path, name);
		if (p->next)
			g_string_append(path, ">");
	}

	return g_string_free_and_steal(path);
}

static void
toolkitgroupview_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(object);

	switch (prop_id) {
	case PROP_SHOW_ALL:
		g_value_set_boolean(value, kitgview->show_all);
		break;

	case PROP_PATH:
		g_value_take_string(value,
			toolkitgroupview_print_path(kitgview->page_names));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

/* Build a page in the tree view.
 */

static void
toolkitgroupview_add_kit(Toolkitgroupview *kitgview, GListStore *store, Toolkit *kit)
{
	if (kit &&
		(kitgview->show_all || MODEL(kit)->display) &&
		IOBJECT(kit)->name)
		g_list_store_append(store, G_OBJECT(node_new(kit, NULL, NULL)));
}

static void
toolkitgroupview_add_tool(Toolkitgroupview *kitgview, GListStore *store, Tool *tool)
{
	if (tool &&
		(kitgview->show_all || MODEL(tool)->display) &&
		IOBJECT(tool)->name)
		g_list_store_append(store, G_OBJECT(node_new(NULL, tool, NULL)));
}

static void
toolkitgroupview_add_toolitem(Toolkitgroupview *kitgview, GListStore *store, Toolitem *toolitem)
{
	if (toolitem &&
		(kitgview->show_all || MODEL(toolitem->tool)->display) &&
		IOBJECT(toolitem->tool)->name)
		g_list_store_append(store, G_OBJECT(node_new(NULL, NULL, toolitem)));
}

static void *
toolkitgroupview_build_kit(Toolkit *kit, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(b);

	toolkitgroupview_add_kit(kitgview, store, kit);

	return NULL;
}

static void *
toolkitgroupview_build_tool(Tool *tool, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(b);

	if (tool->toolitem)
		toolkitgroupview_add_toolitem(kitgview, store, tool->toolitem);
	else
		toolkitgroupview_add_tool(kitgview, store, tool);

	return NULL;
}

static void *
toolkitgroupview_build_toolitem(void *a, void *b, void *c)
{
	Toolitem *toolitem = (Toolitem *) a;
	GListStore *store = G_LIST_STORE(b);
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(c);

	if (toolitem->tool &&
		(kitgview->show_all || MODEL(toolitem->tool)->display) &&
		(kitgview->show_all || !toolitem->is_separator))
		toolkitgroupview_add_toolitem(kitgview, store, toolitem);

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
		toolkitgroup_map(kitg, toolkitgroupview_build_kit, store, kitgview);
	else if (parent &&
		parent->kit)
		// expand the kit to a new store
		toolkit_map(parent->kit, toolkitgroupview_build_tool, store, kitgview);
	else if (parent &&
		parent->toolitem &&
		parent->toolitem->is_pullright)
		// expand the pullright to a new store
		slist_map2(parent->toolitem->children,
			toolkitgroupview_build_toolitem, store, kitgview);

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

static GListModel *
toolkitgroupview_build_browse_page(Toolkitgroupview *kitgview, Node *parent);

static void
toolkitgroupview_browse_clicked(GtkWidget *button,
	Toolkitgroupview *kitgview)
{
	Node *node = g_object_get_qdata(G_OBJECT(button), node_quark);

	if (node->kit ||
		(node->toolitem && node->toolitem->is_pullright))
		toolkitgroupview_build_browse_page(kitgview, node);

	// avtivate after moving to the new page so listeners can get the new page
	// name
	toolkitgroupview_activate(kitgview, node->toolitem, node->tool);
}

static void
toolkitgroupview_browse_back_clicked(GtkWidget *button,
	Toolkitgroupview *kitgview)
{
	// remove the last item from the page_names list
	// we know we are at least one deep in the menu
	g_assert(kitgview->page_names->next);
	char *last_name =
		(char *) g_slist_last(kitgview->page_names)->data;
	kitgview->page_names = g_slist_remove(kitgview->page_names, last_name);
	g_free(last_name);

	// get the (new) last name
	const char *name =
		(const char *) g_slist_last(kitgview->page_names)->data;
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

	if (kitgview->search_mode) {
		if (node->toolitem)
			gtk_label_set_label(GTK_LABEL(label), node->toolitem->user_path);
		else
			gtk_label_set_label(GTK_LABEL(label), IOBJECT(node->tool)->name);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_START);
	}
	else if (node->kit ||
		(node->toolitem && !node->toolitem->is_separator) ||
		node->tool)
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

static GListModel *
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

	return model;
}

static GListModel *
toolkitgroupview_build_browse_page(Toolkitgroupview *kitgview, Node *this)
{
	const char *name = node_get_name(this);

	GtkWidget *scrolled_window = gtk_scrolled_window_new();
	printf("toolkitgroupview_build_browse_page: adding %s\n", name);
	gtk_stack_add_named(GTK_STACK(kitgview->stack), scrolled_window, name);
	kitgview->page_names = g_slist_append(kitgview->page_names, g_strdup(name));

	GtkWidget *list_view = gtk_list_view_new(NULL, NULL);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window),
		list_view);

	gtk_stack_set_visible_child(GTK_STACK(kitgview->stack), scrolled_window);

	return toolkitgroupview_fill_browse_page(kitgview, this, list_view);
}

/* The menu is on the empty root page. Rebuild everything, matching the page
 * structure in page_names, if possible.
 */
static void
toolkitgroupview_rebuild_browse(Toolkitgroupview *kitgview)
{
	// take a copy of the old pages_names list and reset to the root
	GSList *old_page_names = kitgview->page_names;
	kitgview->page_names = g_slist_append(NULL, g_strdup("root"));

	// don't animate the browser rebuild
	gboolean should_animate =
		widget_should_animate(GTK_WIDGET(kitgview->stack));
	g_object_set(gtk_widget_get_settings(GTK_WIDGET(kitgview->stack)),
		"gtk-enable-animations", FALSE,
		NULL);

	// fill the root page
	GListModel *list_model = toolkitgroupview_fill_browse_page(kitgview,
		NULL, kitgview->list_view);

	for (GSList *p = old_page_names->next; p; p = p->next) {
		char *name = (char *) p->data;

		/* Is there an item on the page we've just made with the next name on
		 * the page list?
		 */
		Node *this = NULL;
		for (int i = 0; i < g_list_model_get_n_items(list_model); i++) {
			Node *item = g_list_model_get_item(list_model, i);
			const char *item_name = node_get_name(item);

			// name can be NULL for eg separators
			if (item_name &&
				g_str_equal(item_name, name)) {
				this = item;
				break;
			}
		}
		if (!this)
			break;

		list_model = toolkitgroupview_build_browse_page(kitgview, this);
	}

	g_slist_free_full(g_steal_pointer(&old_page_names), g_free);

	g_object_set(gtk_widget_get_settings(GTK_WIDGET(kitgview->stack)),
		"gtk-enable-animations", should_animate,
		NULL);
}

/* Build the flat search list.
 */

static void *
toolkitgroupview_build_toolitem_list(void *a, void *b, void *c)
{
	Toolitem *toolitem = (Toolitem *) a;
	GListStore *store = G_LIST_STORE(b);
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(c);

	if (toolitem->children)
		slist_map2(toolitem->children,
			toolkitgroupview_build_toolitem_list, store, kitgview);
	else if (!toolitem->is_pullright &&
		!toolitem->is_separator)
		toolkitgroupview_add_toolitem(kitgview, store, toolitem);

	return NULL;
}

static void *
toolkitgroupview_build_tool_list(Tool *tool, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(b);

	if (tool->toolitem)
		toolkitgroupview_build_toolitem_list(tool->toolitem, store, kitgview);
	else if (kitgview->show_all)
		toolkitgroupview_add_tool(kitgview, store, tool);

	return NULL;
}

static void *
toolkitgroupview_build_kit_list(Toolkit *kit, void *a, void *b)
{
	GListStore *store = G_LIST_STORE(a);
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(b);

	toolkit_map(kit, toolkitgroupview_build_tool_list, store, kitgview);

	return NULL;
}

static GListModel *
toolkitgroupview_create_list(Toolkitgroupview *kitgview)
{
	Toolkitgroup *kitg = TOOLKITGROUP(VOBJECT(kitgview)->iobject);

	GListStore *store;

	store = g_list_store_new(NODE_TYPE);

	if (kitg)
		toolkitgroup_map(kitg,
			toolkitgroupview_build_kit_list, store, kitgview);

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
	gtk_stack_set_visible_child(GTK_STACK(kitgview->stack), root_page);

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

		// truncate the page_names list, since we're a flat view
		g_slist_free_full(g_steal_pointer(&kitgview->page_names->next), g_free);
	}
	else {
		// rebuild the nested menu
		toolkitgroupview_rebuild_browse(kitgview);
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

		if (search_mode) {
			gtk_widget_set_sensitive(kitgview->search_entry, TRUE);
			gtk_widget_grab_focus(kitgview->search_entry);
		}
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

			gtk_widget_set_sensitive(kitgview->search_entry, FALSE);
		}

		vobject_refresh_queue(VOBJECT(kitgview));
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
	gobject_class->set_property = toolkitgroupview_set_property;
	gobject_class->get_property = toolkitgroupview_get_property;

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

	g_object_class_install_property(gobject_class, PROP_SHOW_ALL,
		g_param_spec_boolean("show-all",
			_("Show all"),
			_("Show all tools and toolkits"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PATH,
		g_param_spec_string("path",
			_("Path"),
			_("Path to display"),
			"",
			G_PARAM_READWRITE));

	toolkitgroupview_signals[SIG_ACTIVATE] = g_signal_new("activate",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		nip4_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
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

	kitgview->page_names =
		g_slist_append(kitgview->page_names, g_strdup("root"));

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

