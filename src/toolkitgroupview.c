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
	PROP_SORT_TEXT,
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
			g_value_set_string(value, "");
		break;

	case PROP_SORT_TEXT:
		if (node->toolitem &&
			node->toolitem->name)
			g_value_set_string(value, node->toolitem->user_path);
		else if (node->tool &&
			IOBJECT(node->tool)->name)
			g_value_set_string(value, IOBJECT(node->tool)->name);
		else
			g_value_set_string(value, "");
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
			_("Display name"),
			NULL,
			G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, PROP_SORT_TEXT,
		g_param_spec_string("sort-text",
			_("Sort by this text"),
			_("Search key"),
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
	PROP_SEARCH,

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
	for (int i = 0; i < MAX_PAGE_DEPTH; i++)
		VIPS_FREE(kitgview->page_names[i]);

	G_OBJECT_CLASS(toolkitgroupview_parent_class)->dispose(object);
}

static void
toolkitgroupview_set_path(Toolkitgroupview *kitgview, char *path)
{
	for (int i = 0; i < MAX_PAGE_DEPTH; i++) {
		VIPS_FREE(kitgview->page_names[i]);
		kitgview->pinned[i] = FALSE;
	}
	kitgview->n_pages = 1;
	kitgview->page_names[0] = g_strdup("root");

	if (path) {
		char *p;
		char *q;

		kitgview->n_pages = 0;
		for (p = path; (q = vips_break_token(p, ">")); p = q) {
			VIPS_SETSTR(kitgview->page_names[kitgview->n_pages], g_strdup(p));

			if (kitgview->n_pages >= MAX_PAGE_DEPTH - 1)
				return;
			kitgview->n_pages += 1;
		}
	}
}

static void
toolkitgroupview_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(object);
	g_autofree char *path = NULL;
	g_autofree char *search_text = NULL;

	switch (prop_id) {
	case PROP_SHOW_ALL:
		kitgview->show_all = g_value_get_boolean(value);
		vobject_refresh_queue(VOBJECT(kitgview));
		break;

	case PROP_PATH:
		path = g_strdup(g_value_get_string(value));
		if (strlen(path) > 0) {
			toolkitgroupview_set_path(kitgview, path);
			kitgview->search_mode = FALSE;
			vobject_refresh_queue(VOBJECT(kitgview));
		}
		break;

	case PROP_SEARCH:
		search_text = g_strdup(g_value_get_string(value));
		if (strlen(search_text) > 0) {
			kitgview->search_mode = TRUE;
			vobject_refresh_queue(VOBJECT(kitgview));
			gtk_editable_set_text(GTK_EDITABLE(kitgview->search_entry),
				search_text);
			gtk_string_filter_set_search(kitgview->filter, search_text);
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static char *
toolkitgroupview_print_path(Toolkitgroupview *kitgview)
{
	GString *path = g_string_new("");

	for (int i = 0; i < kitgview->n_pages; i++) {
		if (i > 0)
			g_string_append(path, ">");
		g_string_append(path, kitgview->page_names[i]);
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
		if (kitgview->search_mode)
			g_value_set_string(value, "");
		else
			g_value_take_string(value, toolkitgroupview_print_path(kitgview));
		break;

	case PROP_SEARCH:
		if (kitgview->search_mode)
			g_value_set_string(value,
				gtk_editable_get_text(GTK_EDITABLE(kitgview->search_entry)));
		else
			g_value_set_string(value, "");
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

/* Build a page in the tree view.
 */

static void
toolkitgroupview_add_kit(Toolkitgroupview *kitgview,
	GListStore *store, Toolkit *kit)
{
	if (kit &&
		(kitgview->show_all || MODEL(kit)->display) &&
		IOBJECT(kit)->name)
		g_list_store_append(store, G_OBJECT(node_new(kit, NULL, NULL)));
}

static void
toolkitgroupview_add_tool(Toolkitgroupview *kitgview,
	GListStore *store, Tool *tool)
{
	if (tool &&
		(kitgview->show_all || MODEL(tool)->display) &&
		IOBJECT(tool)->name)
		g_list_store_append(store, G_OBJECT(node_new(NULL, tool, NULL)));
}

static void
toolkitgroupview_add_toolitem(Toolkitgroupview *kitgview,
	GListStore *store, Toolitem *toolitem)
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

// go back one, if we can
static void
toolkitgroupview_browse_back(Toolkitgroupview *kitgview)
{
	if (kitgview->n_pages > 1) {
		GtkWidget *last_page =
			gtk_stack_get_visible_child(GTK_STACK(kitgview->stack));

		// display the second-last page
		gtk_stack_set_visible_child_name(GTK_STACK(kitgview->stack),
			kitgview->page_names[kitgview->n_pages - 2]);

		// delete the last page
		gtk_stack_remove(GTK_STACK(kitgview->stack), last_page);

		kitgview->n_pages -= 1;
	}
}

static GtkWidget *
toolkitgroupview_add_browse_page(Toolkitgroupview *kitgview, Node *parent);

static void
toolkitgroupview_browse_clicked(GtkWidget *button,
	Toolkitgroupview *kitgview)
{
	Node *node = g_object_get_qdata(G_OBJECT(button), node_quark);

	GtkWidget *box = gtk_button_get_child(GTK_BUTTON(button));
	GtkWidget *left = gtk_widget_get_first_child(box);

	// click on "go back?"
	if (gtk_widget_is_visible(left))
		toolkitgroupview_browse_back(kitgview);
	else {
		if (node->kit ||
			(node->toolitem && node->toolitem->is_pullright))
			toolkitgroupview_add_browse_page(kitgview, node);

		// activate after moving to the new page so listeners can get the new
		// page name
		toolkitgroupview_activate(kitgview, node->toolitem, node->tool);
	}
}

static void
toolkitgroupview_pin_toggled(GtkCheckButton *self, gpointer user_data)
{
	Toolkitgroupview *kitgview = TOOLKITGROUPVIEW(user_data);

	kitgview->pinned[kitgview->n_pages - 1] ^= 1;
}

static void
toolkitgroupview_setup_browse_item(GtkListItemFactory *factory,
	GtkListItem *item, Toolkitgroupview *kitgview)
{
	// enclosing box, so we can have the pin me up tick on the right
	GtkWidget *enclosing = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	GtkWidget *button = gtk_button_new();
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
	gtk_widget_add_css_class(button, "toolkitgroupview-item");
	g_object_set_qdata(G_OBJECT(button), toolkitgroupview_quark, kitgview);
	g_signal_connect(button, "clicked",
		G_CALLBACK(toolkitgroupview_browse_clicked), kitgview);
	gtk_box_append(GTK_BOX(enclosing), button);

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

	GtkWidget *pin = gtk_check_button_new();
	set_tooltip(pin, "Pin menu in place");
	g_signal_connect(pin, "toggled",
		G_CALLBACK(toolkitgroupview_pin_toggled), kitgview);
	gtk_box_append(GTK_BOX(enclosing), pin);

	gtk_list_item_set_child(item, enclosing);
}

static void
toolkitgroupview_bind_browse_item(GtkListItemFactory *factory,
	GtkListItem *item, Toolkitgroupview *kitgview)
{
	GtkWidget *enclosing = gtk_list_item_get_child(item);
	Node *node = gtk_list_item_get_item(item);
	Node *parent = g_object_get_qdata(G_OBJECT(factory), node_quark);

	GtkWidget *button = gtk_widget_get_first_child(enclosing);
	GtkWidget *pin = gtk_widget_get_next_sibling(button);

	GtkWidget *box = gtk_button_get_child(GTK_BUTTON(button));
	GtkWidget *left = gtk_widget_get_first_child(box);
	GtkWidget *label = gtk_widget_get_next_sibling(left);
	GtkWidget *right = gtk_widget_get_next_sibling(label);

	if (node->kit ||
		(node->toolitem && !node->toolitem->is_separator) ||
		node->tool)
		gtk_label_set_label(GTK_LABEL(label), node_get_name(node));

	if (parent &&
		gtk_list_item_get_position(item) == 0) {
		gtk_widget_set_visible(left, TRUE);
		gtk_widget_set_visible(right, FALSE);
		gtk_widget_set_visible(pin, TRUE);
		gtk_label_set_xalign(GTK_LABEL(label), 0.5);
		g_object_set_qdata(G_OBJECT(button), node_quark, parent);

		// note the pin widget on the page
	}
	else {
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_widget_set_visible(right,
			node->kit ||
			(node->toolitem && node->toolitem->is_pullright));
		gtk_widget_set_visible(pin, FALSE);

		if (node->toolitem)
			set_tooltip(button, node->toolitem->help);

		g_object_set_qdata(G_OBJECT(button), node_quark, node);
	}

	if (node->toolitem &&
		node->toolitem->is_separator)
		gtk_widget_set_sensitive(button, FALSE);

	gtk_widget_remove_css_class(gtk_widget_get_parent(button), "activatable");
}

static GtkWidget *
toolkitgroupview_build_browse_list_view(Toolkitgroupview *kitgview, Node *this)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_object_set_qdata(G_OBJECT(factory), toolkitgroupview_quark, kitgview);
	g_object_set_qdata(G_OBJECT(factory), node_quark, this);
	g_signal_connect(factory, "setup",
		G_CALLBACK(toolkitgroupview_setup_browse_item), kitgview);
	g_signal_connect(factory, "bind",
		G_CALLBACK(toolkitgroupview_bind_browse_item), kitgview);

	GListModel *model = toolkitgroupview_build_node(kitgview, this);
	GtkSelectionModel *selection =
		GTK_SELECTION_MODEL(gtk_no_selection_new(G_LIST_MODEL(model)));

	return gtk_list_view_new(selection, factory);
}

static GtkWidget *
toolkitgroupview_add_browse_page(Toolkitgroupview *kitgview, Node *this)
{
	const char *name = node_get_name(this);

#ifdef DEBUG
	printf("toolkitgroupview_add_browse_page: adding page %s\n", name);
#endif /*DEBUG*/

	GtkWidget *scrolled_window = gtk_scrolled_window_new();
	// no scrollbars ... they obstruct useful widgets
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
		GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL);
	gtk_stack_add_named(GTK_STACK(kitgview->stack), scrolled_window, name);
	kitgview->page_names[kitgview->n_pages] = g_strdup(name);
	kitgview->pinned[kitgview->n_pages] = FALSE;
	kitgview->n_pages += 1;

	GtkWidget *list_view =
		toolkitgroupview_build_browse_list_view(kitgview, this);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window),
		list_view);

	gtk_stack_set_visible_child(GTK_STACK(kitgview->stack), scrolled_window);

	return list_view;
}

/* The menu is on the empty root page. Rebuild everything, matching the page
 * structure in page_names, if possible.
 */
static void
toolkitgroupview_rebuild_browse(Toolkitgroupview *kitgview)
{
	g_assert(!kitgview->list_view);
	g_assert(!kitgview->filter);

	// don't animate the browser rebuild process
	gboolean should_animate =
		widget_should_animate(GTK_WIDGET(kitgview->stack));
	g_object_set(gtk_widget_get_settings(GTK_WIDGET(kitgview->stack)),
		"gtk-enable-animations", FALSE,
		NULL);

	// steal the old pages_names list and reset to the root
	char *old_page_names[MAX_PAGE_DEPTH];
	int old_n_pages = kitgview->n_pages;
	for (int i = 0; i < old_n_pages; i++) {
		old_page_names[i] = kitgview->page_names[i];
		kitgview->page_names[i] = NULL;
	}

	toolkitgroupview_set_path(kitgview, NULL);

	// build and fill the root page in the root scrolled window
	kitgview->list_view =
		toolkitgroupview_build_browse_list_view(kitgview, NULL);
	gtk_scrolled_window_set_child(
		GTK_SCROLLED_WINDOW(kitgview->scrolled_window), kitgview->list_view);

	GListModel *list_model =
		G_LIST_MODEL(gtk_list_view_get_model(
			GTK_LIST_VIEW(kitgview->list_view)));

	for (int i = 0; i < old_n_pages; i++) {
		/* Is there an item on the page we've just made with the next name on
		 * the page list?
		 */
		Node *this = NULL;
		for (int j = 0; j < g_list_model_get_n_items(list_model); j++) {
			Node *item = g_list_model_get_item(list_model, j);
			const char *item_name = node_get_name(item);

			// name can be NULL for eg separators
			if (item_name &&
				g_str_equal(item_name, old_page_names[i])) {
				this = item;
				break;
			}
		}
		if (!this)
			break;

		GtkWidget *list_view =
			toolkitgroupview_add_browse_page(kitgview, this);
		list_model = G_LIST_MODEL(gtk_list_view_get_model(
			GTK_LIST_VIEW(list_view)));
	}

	for (int i = 0; i < old_n_pages; i++)
		VIPS_FREE(old_page_names[i]);

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
toolkitgroupview_list_clicked(GtkWidget *button,
	Toolkitgroupview *kitgview)
{
	Node *node = g_object_get_qdata(G_OBJECT(button), node_quark);

	toolkitgroupview_activate(kitgview, node->toolitem, node->tool);
}

static void
toolkitgroupview_setup_list_item(GtkListItemFactory *factory,
	GtkListItem *item, Toolkitgroupview *kitgview)
{
	GtkWidget *label = gtk_label_new("");
	gtk_widget_set_hexpand(label, TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_START);

	GtkWidget *button = gtk_button_new();
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
	gtk_button_set_child(GTK_BUTTON(button), label);
	g_signal_connect(button, "clicked",
		G_CALLBACK(toolkitgroupview_list_clicked), kitgview);
	gtk_widget_add_css_class(button, "toolkitgroupview-item");

	g_object_set_qdata(G_OBJECT(button), toolkitgroupview_quark, kitgview);

	gtk_list_item_set_child(item, button);
}

static void
toolkitgroupview_bind_list_item(GtkListItemFactory *factory,
	GtkListItem *item, Toolkitgroupview *kitgview)
{
	GtkWidget *button = gtk_list_item_get_child(item);
	Node *node = gtk_list_item_get_item(item);

#ifdef DEBUG
	printf("toolkitgroupview_bind_list_item: ");
	node_print(node);
#endif /*DEBUG*/

	GtkWidget *label = gtk_button_get_child(GTK_BUTTON(button));

	if (node->toolitem)
		gtk_label_set_label(GTK_LABEL(label), node->toolitem->user_path);
	else
		gtk_label_set_label(GTK_LABEL(label), IOBJECT(node->tool)->name);

	if (node->toolitem)
		set_tooltip(button, node->toolitem->help);

	g_object_set_qdata(G_OBJECT(button), node_quark, node);

	gtk_widget_remove_css_class(gtk_widget_get_parent(button), "activatable");
}

static void
toolkitgroupview_rebuild_list(Toolkitgroupview *kitgview)
{
	g_assert(!kitgview->list_view);
	g_assert(!kitgview->filter);

	GtkExpression *expression;
	GListModel *list_model;

	list_model = toolkitgroupview_create_list(kitgview);

	expression = gtk_property_expression_new(NODE_TYPE, NULL, "sort-text");
	GtkSorter *sorter = GTK_SORTER(gtk_string_sorter_new(expression));
	list_model = G_LIST_MODEL(gtk_sort_list_model_new(list_model, sorter));

	expression = gtk_property_expression_new(NODE_TYPE, NULL, "search-text");
	kitgview->filter = gtk_string_filter_new(expression);
	gtk_string_filter_set_search(kitgview->filter, "");
	list_model = G_LIST_MODEL(gtk_filter_list_model_new(list_model,
			GTK_FILTER(kitgview->filter)));

	GtkSelectionModel *selection =
		GTK_SELECTION_MODEL(gtk_no_selection_new(list_model));

	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup",
		G_CALLBACK(toolkitgroupview_setup_list_item), kitgview);
	g_signal_connect(factory, "bind",
		G_CALLBACK(toolkitgroupview_bind_list_item), kitgview);

	kitgview->list_view = gtk_list_view_new(selection, factory);

	gtk_scrolled_window_set_child(
		GTK_SCROLLED_WINDOW(kitgview->scrolled_window), kitgview->list_view);

	kitgview->n_pages = 1;
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

	// remove the base list_view
	gtk_scrolled_window_set_child(
		GTK_SCROLLED_WINDOW(kitgview->scrolled_window), NULL);
	kitgview->list_view = NULL;
	kitgview->filter = NULL;

	if (kitgview->search_mode)
		toolkitgroupview_rebuild_list(kitgview);
	else
		toolkitgroupview_rebuild_browse(kitgview);

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
	BIND_VARIABLE(Toolkitgroupview, scrolled_window);

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

	g_object_class_install_property(gobject_class, PROP_SEARCH,
		g_param_spec_string("search",
			_("Search"),
			_("Text to search for"),
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

	toolkitgroupview_set_path(kitgview, NULL);

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

void
toolkitgroupview_home(Toolkitgroupview *kitgview)
{
	while(kitgview->n_pages > 1 &&
		!kitgview->pinned[kitgview->n_pages - 1])
		toolkitgroupview_browse_back(kitgview);
}
