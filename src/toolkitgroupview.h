/* Toolkit browser
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

#define TOOLKITGROUPVIEW_TYPE (toolkitgroupview_get_type())
#define TOOLKITGROUPVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TOOLKITGROUPVIEW_TYPE, Toolkitgroupview))
#define TOOLKITGROUPVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), \
		TOOLKITGROUPVIEW_TYPE, ToolkitgroupviewClass))
#define IS_TOOLKITGROUPVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TOOLKITGROUPVIEW_TYPE))
#define IS_TOOLKITGROUPVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TOOLKITGROUPVIEW_TYPE))
#define TOOLKITGROUPVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), \
		TOOLKITGROUPVIEW_TYPE, ToolkitgroupviewClass))

/* The max depth of the tree.
 */
#define MAX_PAGE_DEPTH (256)

struct _Toolkitgroupview {
	View parent_object;

	// show all tools and toolkits, even hidden ones
	gboolean show_all;

	/* Filter list mode with this.
	 */
	GtkStringFilter *filter;

	/* In search mode.
	 */
	gboolean search_mode;

	/* The number of pages deep we are, so 1 for on the root page.
	 */
	int n_pages;

	/* Page names for our stack, root first. Use this for go-back, and to
	 * rebuild the view on refresh. These strings must be g_free()d. The root
	 * page is called "root".
	 */
	char *page_names[MAX_PAGE_DEPTH];

	/* For each page in the stack, TRUE if the user has pushed the "pin"
	 * button.
	 */
	gboolean pinned[MAX_PAGE_DEPTH];

	GtkWidget *stack;
	GtkWidget *search_toggle;
	GtkWidget *search_entry;
	GtkWidget *scrolled_window;
	GtkWidget *list_view;

};

typedef struct _ToolkitgroupviewClass {
	ViewClass parent_class;

} ToolkitgroupviewClass;

GType toolkitgroupview_get_type(void);
View *toolkitgroupview_new(void);
void toolkitgroupview_home(Toolkitgroupview *kitgview);
