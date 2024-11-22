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

#define TOOLKITBROWSER_TYPE (toolkitbrowser_get_type())
#define TOOLKITBROWSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TOOLKITBROWSER_TYPE, Toolkitbrowser))
#define TOOLKITBROWSER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TOOLKITBROWSER_TYPE, ToolkitbrowserClass))
#define IS_TOOLKITBROWSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TOOLKITBROWSER_TYPE))
#define IS_TOOLKITBROWSER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TOOLKITBROWSER_TYPE))
#define TOOLKITBROWSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TOOLKITBROWSER_TYPE, ToolkitbrowserClass))

struct _Toolkitbrowser {
	vObject parent_object;

	Toolkitgroup *kitg;
	Workspace *ws;

	/* The model for this toolkit view. This implements GListModel, which we
	 * display in the GtkListView.
	 */
	GtkTreeListModel *treemodel;
	GtkStringFilter *filter;

	GtkWidget *top;
	GtkWidget *search_entry;	/* Search widget */
	GtkWidget *list_view;		/* Display of items */
};

typedef struct _ToolkitbrowserClass {
	vObjectClass parent_class;

} ToolkitbrowserClass;

GType toolkitbrowser_get_type(void);
Toolkitbrowser *toolkitbrowser_new(void);
void toolkitbrowser_set_workspace(Toolkitbrowser *toolkitbrowser,
	Workspace *ws);
