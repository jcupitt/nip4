/* Declarations for workspace.c.
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

#define WORKSPACE_TYPE (workspace_get_type())
#define WORKSPACE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), WORKSPACE_TYPE, Workspace))
#define WORKSPACE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), WORKSPACE_TYPE, WorkspaceClass))
#define IS_WORKSPACE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), WORKSPACE_TYPE))
#define IS_WORKSPACE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), WORKSPACE_TYPE))
#define WORKSPACE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), WORKSPACE_TYPE, WorkspaceClass))

/* Three sorts of workspace file load.
 */
typedef enum {
	WORKSPACE_MODE_REGULAR, /* Vanilla! */
	WORKSPACE_MODE_FORMULA, /* Show formula, not values */
	WORKSPACE_MODE_NOEDIT	/* Shut down all edits */
} WorkspaceMode;

/* A workspace.
 */
struct _Workspace {
	Model parent_object;

	/* Our rows are part of this symbol.
	 */
	Symbol *sym;

	/* We are using this set of toolkits.
	 */
	Toolkitgroup *kitg;

	/* State.
	 */
	int next;			/* Index for next column name */
	GSList *selected;	/* Rows selected in this workspace */
	GSList *errors;		/* Rows with errors */
	WorkspaceMode mode; /* Display mode */
	gboolean locked;	/* WS has been locked */

	/* The nip2 version that made this workspace.
	 */
	int major;
	int minor;

	/* The last row we scrolled to on next-error.
	 */
	Row *last_error;

	/* Set if the view should display error_top and error_sub.
	 */
	gboolean show_error;
	char *error_top;
	char *error_sub;

	/* Set if the view should display alert_top and alert_sub.
	 */
	gboolean show_alert;
	char *alert_top;
	char *alert_sub;

	VipsRect area;		 /* Rect enclosing the set of columns */
	VipsRect vp;		 /* Viewport hint ... set by views */

	/* Visualisation defaults for this ws.
	 */
	double scale;
	double offset;

	/* Workspace-local defs, displayed in the left pane. All in a private
	 * kitg and kit.
	 */
	char *local_defs;
	Toolkitgroup *local_kitg;
	Toolkit *local_kit;

	/* Some view inside this ws has changed and this ws needs a relayout.
	 * Use in_dispose to stop relayout during ws shutdown.
	 */
	gboolean needs_layout;
	gboolean in_dispose;
};

typedef struct _WorkspaceClass {
	ModelClass parent_class;

	/* Methods.
	 */
} WorkspaceClass;

void workspace_set_error(Workspace *ws, gboolean show_error);
void workspace_show_error(Workspace *ws);
void workspace_clear_error_all(void);

void workspace_set_alert(Workspace *ws, gboolean show_alert);
void workspace_show_alert(Workspace *ws);

void workspace_queue_layout(Workspace *ws);

Workspacegroup *workspace_get_workspacegroup(Workspace *ws);
Workspaceroot *workspace_get_workspaceroot(Workspace *ws);
void workspace_set_modified(Workspace *ws, gboolean modified);

void *workspace_map(workspace_map_fn fn, void *a, void *b);
void *workspace_map_column(Workspace *ws, column_map_fn fn, void *a);
void *workspace_map_symbol(Workspace *ws, symbol_map_fn fn, void *a);
void *workspace_map_view(Workspace *ws, view_map_fn fn, void *a);

gboolean workspace_is_empty(Workspace *ws);

void *workspace_selected_map(Workspace *ws, row_map_fn fn, void *a, void *b);
void *workspace_selected_map_sym(Workspace *ws,
	symbol_map_fn fn, void *a, void *b);
gboolean workspace_selected_any(Workspace *ws);
int workspace_selected_num(Workspace *ws);
gboolean workspace_selected_sym(Workspace *ws, Symbol *sym);
Row *workspace_selected_one(Workspace *ws);
void workspace_deselect_all(Workspace *ws);
void workspace_selected_names(Workspace *ws,
	VipsBuf *buf, const char *separator);
void workspace_column_names(Column *col,
	VipsBuf *buf, const char *separator);
void workspace_select_all(Workspace *ws);
Column *workspace_is_one_empty(Workspace *ws);

Column *workspace_column_find(Workspace *ws, const char *name);
void workspace_column_name_new(Workspace *ws, char *name);
Column *workspace_column_pick(Workspace *ws);
Column *workspace_column_new(Workspace *ws);
void workspace_column_select(Workspace *ws, Column *col);

Symbol *workspace_add_def(Workspace *ws, const char *str);
Symbol *workspace_add_def_recalc(Workspace *ws, const char *str);
gboolean workspace_load_file_buf(VipsBuf *buf, const char *filename);
Symbol *workspace_load_file(Workspace *ws, const char *filename);

void workspace_get_version(Workspace *ws, int *major, int *minor);
int workspace_have_compat(int major, int minor,
	int *best_major, int *best_minor);
gboolean workspace_load_compat(Workspace *ws, int major, int minor);

GType workspace_get_type(void);
Workspace *workspace_new(Workspacegroup *wsg, const char *name);
Workspace *workspace_new_blank(Workspacegroup *wsg);

gboolean workspace_add_action(Workspace *ws,
	const char *name, const char *action, int nparam);

int workspace_number(void);

gboolean workspace_selected_recalc(Workspace *ws);
void workspace_selected_remove_yesno(Workspace *ws, GtkWindow *window);
gboolean workspace_selected_ungroup(Workspace *ws);
gboolean workspace_selected_group(Workspace *ws);

void workspace_error_sanity(Workspace *ws);

gboolean workspace_next_error(Workspace *ws);
gboolean workspace_prev_error(Workspace *ws);

void workspace_set_mode(Workspace *ws, WorkspaceMode mode);

gboolean workspace_local_set(Workspace *ws, const char *txt);
gboolean workspace_local_set_from_file(Workspace *ws, const char *fname);

gboolean workspace_merge_file(Workspace *ws, const char *filename);
gboolean workspace_selected_duplicate(Workspace *ws);
gboolean workspace_selected_save(Workspace *ws, const char *filename);

gboolean workspace_rename(Workspace *ws, const char *name);
void workspace_set_locked(Workspace *ws, gboolean locked);
gboolean workspace_duplicate(Workspace *ws);
