/* Manage workspace objects.
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
#define DEBUG_VERBOSE
#define DEBUG
 */

#include "nip4.h"

G_DEFINE_TYPE(Workspace, workspace, MODEL_TYPE)

static GSList *workspace_all = NULL;

static GSList *workspace_needs_layout = NULL;

static gint workspace_layout_timeout = 0;

/* Ask views to display or hide their error message.
 */
void
workspace_set_error(Workspace *ws, gboolean show_error)
{
	if (show_error) {
		VIPS_SETSTR(ws->error_top, error_get_top());
		VIPS_SETSTR(ws->error_sub, error_get_sub());
	}

	if (ws->show_error != show_error) {
		ws->show_error = show_error;
		iobject_changed(IOBJECT(ws));
	}
}

/* Display an error message.
 */
void
workspace_show_error(Workspace *ws)
{
	workspace_set_error(ws, TRUE);
}

static void *
workspace_clear_error(Workspace *ws)
{
	workspace_set_error(ws, FALSE);

	return NULL;
}

void
workspace_clear_error_all(void)
{
	slist_map(workspace_all, (SListMapFn) workspace_clear_error, NULL);
}

/* Ask views to display or hide their alert message.
 */
void
workspace_set_alert(Workspace *ws, gboolean show_alert)
{
	if (show_alert) {
		VIPS_SETSTR(ws->alert_top, error_get_top());
		VIPS_SETSTR(ws->alert_sub, error_get_sub());
	}

	if (ws->show_alert != show_alert) {
		ws->show_alert = show_alert;
		iobject_changed(IOBJECT(ws));
	}
}

/* Display an alert message.
 */
void
workspace_show_alert(Workspace *ws)
{
	workspace_set_alert(ws, TRUE);
}

static void
workspace_set_needs_layout(Workspace *ws, gboolean needs_layout)
{
#ifdef DEBUG_VERBOSE
	printf("workspace_set_needs_layout: %p %s %d\n",
		ws, IOBJECT(ws)->name, needs_layout);
#endif /*DEBUG_VERBOSE*/

	if (!ws->in_dispose &&
		needs_layout != ws->needs_layout) {
		g_assert((!g_slist_find(workspace_needs_layout, ws)) == needs_layout);

		if (needs_layout)
			workspace_needs_layout =
				g_slist_prepend(workspace_needs_layout, ws);
		else
			workspace_needs_layout =
				g_slist_remove(workspace_needs_layout, ws);

		ws->needs_layout = needs_layout;
	}
}

static void *
workspace_layout_sub(Workspace *ws)
{
	model_layout(MODEL(ws));
	workspace_set_needs_layout(ws, FALSE);

	return NULL;
}

static gboolean
workspace_layout_timeout_cb(gpointer user_data)
{
	workspace_layout_timeout = 0;

	slist_map(workspace_needs_layout,
		(SListMapFn) workspace_layout_sub, NULL);

	return FALSE;
}

void
workspace_queue_layout(Workspace *ws)
{
	workspace_set_needs_layout(ws, TRUE);

	VIPS_FREEF(g_source_remove, workspace_layout_timeout);
	workspace_layout_timeout = g_timeout_add(300,
		(GSourceFunc) workspace_layout_timeout_cb, NULL);
}

Workspacegroup *
workspace_get_workspacegroup(Workspace *ws)
{
	iContainer *parent;

	if ((parent = ICONTAINER(ws)->parent))
		return WORKSPACEGROUP(parent);

	return NULL;
}

Workspaceroot *
workspace_get_workspaceroot(Workspace *ws)
{
	return workspace_get_workspacegroup(ws)->wsr;
}

void
workspace_set_modified(Workspace *ws, gboolean modified)
{
	Workspacegroup *wsg;

	if ((wsg = workspace_get_workspacegroup(ws)))
		filemodel_set_modified(FILEMODEL(wsg), modified);
}

static void *
workspace_map_sub(Workspacegroup *wsg, workspace_map_fn fn, void *a, void *b)
{
	g_assert(IS_WORKSPACEGROUP(wsg));

	return icontainer_map(ICONTAINER(wsg), (icontainer_map_fn) fn, a, b);
}

/* Over all workspaces.
 */
void *
workspace_map(workspace_map_fn fn, void *a, void *b)
{
	return icontainer_map3(ICONTAINER(main_workspaceroot),
		(icontainer_map3_fn) workspace_map_sub,
		fn, a, b);
}

/* Map across the columns in a workspace.
 */
void *
workspace_map_column(Workspace *ws, column_map_fn fn, void *a)
{
	return icontainer_map(ICONTAINER(ws), (icontainer_map_fn) fn, a, NULL);
}

/* Map across a Workspace, applying to the symbols of the top-level rows.
 */
void *
workspace_map_symbol(Workspace *ws, symbol_map_fn fn, void *a)
{
	return icontainer_map(ICONTAINER(ws),
		(icontainer_map_fn) column_map_symbol, (void *) fn, a);
}

static void *
workspace_is_empty_sub(Symbol *sym)
{
	return sym;
}

/* Does a workspace contain no rows?
 */
gboolean
workspace_is_empty(Workspace *ws)
{
	return workspace_map_symbol(ws,
			   (symbol_map_fn) workspace_is_empty_sub, NULL) == NULL;
}

/* Map a function over all selected rows in a workspace.
 */
void *
workspace_selected_map(Workspace *ws, row_map_fn fn, void *a, void *b)
{
	return slist_map2(ws->selected, (SListMap2Fn) fn, a, b);
}

static void *
workspace_selected_map_sym_sub(Row *row, symbol_map_fn fn, void *a)
{
	return fn(row->sym, a, NULL, NULL);
}

/* Map a function over all selected symbols in a workspace.
 */
void *
workspace_selected_map_sym(Workspace *ws,
	symbol_map_fn fn, void *a, void *b)
{
	return workspace_selected_map(ws,
		(row_map_fn) workspace_selected_map_sym_sub, (void *) fn, a);
}

/* Are there any selected rows?
 */
gboolean
workspace_selected_any(Workspace *ws)
{
	return ws->selected != NULL;
}

/* Number of selected rows.
 */
int
workspace_selected_num(Workspace *ws)
{
	return g_slist_length(ws->selected);
}

static void *
workspace_selected_sym_sub(Row *row, Symbol *sym)
{
	return row->sym == sym ? sym : NULL;
}

/* Is sym selected?
 */
gboolean
workspace_selected_sym(Workspace *ws, Symbol *sym)
{
	return workspace_selected_map(ws,
			   (row_map_fn) workspace_selected_sym_sub, sym, NULL) != NULL;
}

/* Is just one row selected? If yes, return it.
 */
Row *
workspace_selected_one(Workspace *ws)
{
	int len = g_slist_length(ws->selected);

	if (len == 1)
		return (Row *) (ws->selected->data);
	else if (len == 0) {
		error_top(_("No objects selected"));
		error_sub(_("select exactly one object and try again"));
		return NULL;
	}
	else {
		error_top(_("More than one object selected"));
		error_sub(_("select exactly one object and try again"));
		return NULL;
	}
}

static void *
workspace_deselect_all_sub(Column *col)
{
	col->last_select = NULL;

	return NULL;
}

/* Deselect all rows.
 */
void
workspace_deselect_all(Workspace *ws)
{
	(void) workspace_selected_map(ws,
		(row_map_fn) row_deselect, NULL, NULL);
	(void) workspace_map_column(ws,
		(column_map_fn) workspace_deselect_all_sub, NULL);
}

/* Track this while we build a names list.
 */
typedef struct {
	VipsBuf *buf;
	const char *separator;
	gboolean first;
} NamesInfo;

/* Add a name to a string for a symbol.
 */
static void *
workspace_selected_names_sub(Row *row, NamesInfo *names)
{
	if (!names->first)
		vips_buf_appends(names->buf, names->separator);
	row_qualified_name(row, names->buf);

	names->first = FALSE;

	return NULL;
}

/* Add a list of selected symbol names to a string.
 */
void
workspace_selected_names(Workspace *ws, VipsBuf *buf, const char *separator)
{
	NamesInfo names;

	names.buf = buf;
	names.separator = separator;
	names.first = TRUE;

	(void) workspace_selected_map(ws,
		(row_map_fn) workspace_selected_names_sub, &names, NULL);
}

void
workspace_column_names(Column *col, VipsBuf *buf, const char *separator)
{
	NamesInfo names;

	names.buf = buf;
	names.separator = separator;
	names.first = TRUE;

	(void) column_map(col,
		(row_map_fn) workspace_selected_names_sub, &names, NULL);
}

/* Select all objects in all columns.
 */
void
workspace_select_all(Workspace *ws)
{
	(void) icontainer_map(ICONTAINER(ws),
		(icontainer_map_fn) column_select_symbols, NULL, NULL);
}

/* Is there just one column, and is it empty?
 */
Column *
workspace_is_one_empty(Workspace *ws)
{
	GSList *children = ICONTAINER(ws)->children;
	Column *col;

	if (g_slist_length(children) != 1)
		return NULL;

	col = COLUMN(children->data);
	if (!column_is_empty(col))
		return NULL;

	return col;
}

/* Search for a column by name.
 */
Column *
workspace_column_find(Workspace *ws, const char *name)
{
	Model *model;

	if (!(model = icontainer_map(ICONTAINER(ws),
			  (icontainer_map_fn) iobject_test_name, (void *) name, NULL)))
		return NULL;

	return COLUMN(model);
}

/* Make up a new column name. Check for not already in workspace.
 */
void
workspace_column_name_new(Workspace *ws, char *name)
{
	do {
		number_to_string(ws->next++, name);
	} while (workspace_column_find(ws, name));
}

Column *
workspace_get_column(Workspace *ws)
{
	if (ICONTAINER(ws)->current)
		return COLUMN(ICONTAINER(ws)->current);

	return NULL;
}

/* Select a column. Can select NULL for no current col in this ws.
 */
void
workspace_column_select(Workspace *ws, Column *col)
{
	icontainer_current(ICONTAINER(ws), ICONTAINER(col));

	// since columns change height on select
	workspace_queue_layout(ws);
}

/* Make sure we have a column selected ... pick one of the existing columns; if
 * there are none, make a column.
 */
Column *
workspace_column_pick(Workspace *ws)
{
	Column *col;

	if ((col = workspace_get_column(ws)))
		return col;
	if ((col = COLUMN(icontainer_get_nth_child(ICONTAINER(ws), 0)))) {
		workspace_column_select(ws, col);
		return col;
	}

	col = column_new(ws, "A", -1, -1);
	workspace_column_select(ws, col);
	workspace_queue_layout(ws);

	return col;
}

Column *
workspace_column_new(Workspace *ws)
{
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	char new_name[MAX_STRSIZE];
	workspace_column_name_new(ws, new_name);

	// position just to right of currently selected column
	Column *old_col = workspace_get_column(ws);
	int x = old_col ? old_col->x + 110 : 110;
	int y = old_col ? old_col->y : 0;
	Column *col;
	if (!(col = column_new(ws, new_name, x, y)))
		return NULL;

	workspace_column_select(ws, col);
	workspace_queue_layout(ws);
	column_scrollto(col, MODEL_SCROLL_TOP);
	filemodel_set_modified(FILEMODEL(wsg), TRUE);

	return col;
}

/* Make a new symbol, part of the current column.
 */
static Symbol *
workspace_add_symbol(Workspace *ws)
{
	Column *col = workspace_column_pick(ws);

	Symbol *sym;
	char *name;

	name = column_name_new(col);
	sym = symbol_new(ws->sym->expr->compile, name);
	VIPS_FREE(name);

	return sym;
}

/* Make up a new definition.
 */
Symbol *
workspace_add_def(Workspace *ws, const char *str)
{
	Column *col = workspace_column_pick(ws);
	Symbol *sym;
	char *name;

#ifdef DEBUG
	printf("workspace_add_def: %s\n", str);
#endif /*DEBUG*/

	if (!str || strspn(str, WHITESPACE) == strlen(str))
		return NULL;

	/* Try parsing as a "fred = 12" style def.
	 */
	attach_input_string(str);
	if ((name = parse_test_define())) {
		sym = symbol_new(ws->sym->expr->compile, name);
		VIPS_FREE(name);
		attach_input_string(str +
			VIPS_CLIP(0, input_state.charpos - 1, strlen(str)));
	}
	else {
		/* That didn't work. Make a sym from the col name.
		 */
		sym = workspace_add_symbol(ws);
		attach_input_string(str);
	}

	if (!symbol_user_init(sym) ||
		!parse_rhs(sym->expr, PARSE_RHS)) {
		/* Another parse error.
		 */
		expr_error_get(sym->expr);

		/* Block changes to error_string ... symbol_destroy()
		 * can set this for compound objects.
		 */
		error_block();
		IDESTROY(sym);
		error_unblock();

		return NULL;
	}

	/* If we're redefining a sym, it might have a row already.
	 *
	 * Don't set modified on the ws, we might be here from parse.
	 */
	if (!sym->expr->row)
		(void) row_new(col->scol, sym, &sym->expr->root);
	symbol_made(sym);

	return sym;
}

/* Make up a new definition, recalc and scroll to make it visible.
 */
Symbol *
workspace_add_def_recalc(Workspace *ws, const char *str)
{
	Column *col = workspace_column_pick(ws);

	Symbol *sym;

#ifdef DEBUG
	printf("workspace_add_def_recalc: %s\n", str);
#endif /*DEBUG*/

	if (!(sym = workspace_add_def(ws, str)))
		return NULL;

	if (!symbol_recalculate_check(sym)) {
		/* Eval error.
		 */
		expr_error_get(sym->expr);
		error_block();
		IDESTROY(sym);
		error_unblock();

		return NULL;
	}

	/* Jump to column containing object.
	 */
	column_set_open(col, TRUE);
	column_scrollto(col, MODEL_SCROLL_BOTTOM);

	return sym;
}

gboolean
workspace_load_file_buf(VipsBuf *buf, const char *filename)
{
	if (callv_string_filenamef(
			(callv_string_fn) vips_foreign_find_load, "%s", filename))
		vips_buf_appends(buf, "Image_file");
	else
		vips_buf_appends(buf, "Matrix_file");

	vips_buf_appends(buf, " \"");
	vips_buf_appendsc(buf, TRUE, filename);
	vips_buf_appends(buf, "\"");

	return TRUE;
}

/* Load a matrix or image. Don't recalc: you need to recalc later to test for
 * success/fail. See eg. workspace_add_def_recalc()
 */
Symbol *
workspace_load_file(Workspace *ws, const char *filename)
{
	char txt[MAX_STRSIZE];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	Symbol *sym;

	if (!workspace_load_file_buf(&buf, filename))
		return NULL;
	if (!(sym = workspace_add_def(ws, vips_buf_all(&buf))))
		return NULL;

	return sym;
}

static void
workspace_dispose(GObject *gobject)
{
	Workspace *ws;

#ifdef DEBUG
	printf("workspace_dispose: %p %s\n", gobject, IOBJECT(gobject)->name);
#endif /*DEBUG*/

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_WORKSPACE(gobject));

	ws = WORKSPACE(gobject);

	workspace_set_needs_layout(ws, FALSE);
	ws->in_dispose = TRUE;

	VIPS_UNREF(ws->kitg);
	VIPS_UNREF(ws->local_kitg);
	IDESTROY(ws->sym);

	G_OBJECT_CLASS(workspace_parent_class)->dispose(gobject);
}

static void
workspace_finalize(GObject *gobject)
{
	Workspace *ws;

#ifdef DEBUG
	printf("workspace_finalize: %p %s\n", gobject, IOBJECT(gobject)->name);
#endif /*DEBUG*/

	g_return_if_fail(gobject != NULL);
	g_return_if_fail(IS_WORKSPACE(gobject));

	ws = WORKSPACE(gobject);

	VIPS_FREE(ws->local_defs);

	workspace_all = g_slist_remove(workspace_all, ws);

	G_OBJECT_CLASS(workspace_parent_class)->finalize(gobject);
}

static void
workspace_changed(iObject *iobject)
{
	Workspace *ws;
	Workspacegroup *wsg;

#ifdef DEBUG_VERBOSE
	printf("workspace_changed: %s\n", iobject->name);
#endif /*DEBUG_VERBOSE*/

	g_return_if_fail(iobject != NULL);
	g_return_if_fail(IS_WORKSPACE(iobject));

	ws = WORKSPACE(iobject);
	wsg = workspace_get_workspacegroup(ws);

	/* Signal changed on our workspacegroup, if we're the current object.
	 */
	if (wsg &&
		ICONTAINER(wsg)->current == ICONTAINER(iobject))
		iobject_changed(IOBJECT(wsg));

	IOBJECT_CLASS(workspace_parent_class)->changed(iobject);
}

static void
workspace_child_add(iContainer *parent, iContainer *child, int pos)
{
	Workspace *ws = WORKSPACE(parent);
	Column *col = COLUMN(child);

	ICONTAINER_CLASS(workspace_parent_class)->child_add(parent, child, pos);

	if (col->selected)
		workspace_column_select(ws, col);
}

static void
workspace_child_remove(iContainer *parent, iContainer *child)
{
	Workspace *ws = WORKSPACE(parent);

	workspace_set_modified(ws, TRUE);

	ICONTAINER_CLASS(workspace_parent_class)->child_remove(parent, child);
}

static void
workspace_current(iContainer *parent, iContainer *child)
{
	Workspace *ws = WORKSPACE(parent);
	Column *col = COLUMN(child);
	Column *current = workspace_get_column(ws);

	if (current)
		current->selected = FALSE;
	if (col)
		col->selected = TRUE;

	ICONTAINER_CLASS(workspace_parent_class)->current(parent, child);
}

static void
workspace_link(Workspace *ws, Workspacegroup *wsg, const char *name)
{
	Workspaceroot *wsr = wsg->wsr;

	Symbol *sym;

#ifdef DEBUG
	printf("workspace_link: naming ws %p as %s\n", ws, name);
#endif /*DEBUG*/

	sym = symbol_new_defining(wsr->sym->expr->compile, name);

	ws->sym = sym;
	sym->type = SYM_WORKSPACE;
	sym->ws = ws;
	sym->expr = expr_new(sym);
	(void) compile_new(sym->expr);
	symbol_made(sym);
	iobject_set(IOBJECT(ws), name, NULL);

	ws->local_kitg = toolkitgroup_new(ws->sym);
	iobject_ref_sink(IOBJECT(ws->local_kitg));
}

static const char *
workspacemode_to_char(WorkspaceMode mode)
{
	switch (mode) {
	case WORKSPACE_MODE_REGULAR:
		return "WORKSPACE_MODE_REGULAR";

	case WORKSPACE_MODE_FORMULA:
		return "WORKSPACE_MODE_FORMULA";

	case WORKSPACE_MODE_NOEDIT:
		return "WORKSPACE_MODE_NOEDIT";

	default:
		return NULL;
	}
}

static WorkspaceMode
char_to_workspacemode(const char *mode)
{
	if (g_ascii_strcasecmp(mode, "WORKSPACE_MODE_REGULAR") == 0)
		return WORKSPACE_MODE_REGULAR;
	else if (g_ascii_strcasecmp(mode, "WORKSPACE_MODE_FORMULA") == 0)
		return WORKSPACE_MODE_FORMULA;
	else if (g_ascii_strcasecmp(mode, "WORKSPACE_MODE_NOEDIT") == 0)
		return WORKSPACE_MODE_NOEDIT;
	else
		return (WorkspaceMode) -1;
}

static View *
workspace_view_new(Model *model, View *parent)
{
	return workspaceview_new();
}

static gboolean
workspace_load(Model *model,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	Workspace *ws = WORKSPACE(model);

	char buf[VIPS_PATH_MAX];
	char *txt;

	g_assert(IS_WORKSPACEGROUP(parent));

	/* "view" is optional, for backwards compatibility.
	 */
	if (get_sprop(xnode, "view", buf, VIPS_PATH_MAX)) {
		WorkspaceMode mode = char_to_workspacemode(buf);

		if ((int) mode >= 0)
			/* Could call workspace_set_mode(), but this is only a
			 * load, so so what.
			 */
			ws->mode = mode;
	}

	/* Also optional.
	 */
	(void) get_dprop(xnode, "scale", &ws->scale);
	(void) get_dprop(xnode, "offset", &ws->offset);

	(void) get_bprop(xnode, "locked", &ws->locked);

	if (get_sprop(xnode, "name", buf, VIPS_PATH_MAX))
		VIPS_SETSTR(IOBJECT(ws)->name, buf);

	/* Don't use get_sprop() and avoid a limit on def size.
	 */
	if ((txt = (char *) xmlGetProp(xnode, (xmlChar *) "local_defs"))) {
		(void) workspace_local_set(ws, txt);
		VIPS_FREEF(xmlFree, txt);
	}

	(void) get_iprop(xnode, "major", &ws->major);
	(void) get_iprop(xnode, "minor", &ws->minor);

	if (!MODEL_CLASS(workspace_parent_class)->load(model, state, parent, xnode))
		return FALSE;

	return TRUE;
}

static xmlNode *
workspace_save(Model *model, xmlNode *xnode)
{
	Workspace *ws = WORKSPACE(model);
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);
	xmlNode *xthis;

	if (!(xthis = MODEL_CLASS(workspace_parent_class)->save(model, xnode)))
		return NULL;

	if (!set_sprop(xthis, "view", workspacemode_to_char(ws->mode)) ||
		!set_dprop(xthis, "scale", ws->scale) ||
		!set_dprop(xthis, "offset", ws->offset) ||
		!set_sprop(xthis, "locked", bool_to_char(ws->locked)) ||
		!set_sprop(xthis, "local_defs", ws->local_defs) ||
		!set_sprop(xthis, "name", IOBJECT(ws)->name))
		return NULL;

	/* We have to save our workspacegroup's filename here for compt with
	 * older nip2.
	 */
	if (!set_sprop(xthis, "filename", FILEMODEL(wsg)->filename))
		return NULL;

	if (!set_iprop(xthis, "major", ws->major) ||
		!set_iprop(xthis, "minor", ws->minor))
		return NULL;

	return xthis;
}

static void
workspace_empty(Model *model)
{
	Workspace *ws = WORKSPACE(model);

	/* Make sure this gets reset.
	 */
	ws->area.left = 0;
	ws->area.top = 0;
	ws->area.width = 0;
	ws->area.height = 0;

	MODEL_CLASS(workspace_parent_class)->empty(model);
}

static void *
workspace_load_toolkit(const char *filename, Toolkitgroup *toolkitgroup)
{
	if (!toolkit_new_from_file(toolkitgroup, filename))
		error_alert(NULL);

	return NULL;
}

/* The compat modes this version of nip2 has. Search the compat dir and make a
 * list of these things.
 */
#define MAX_COMPAT (100)
static int compat_major[MAX_COMPAT];
static int compat_minor[MAX_COMPAT];
static int n_compat = 0;

static void *
workspace_build_compat_fn(const char *filename)
{
	g_autofree char *basename = g_path_get_basename(filename);
	int major;
	int minor;
	if (sscanf(basename, "%d.%d", &major, &minor) == 2) {
		compat_major[n_compat] = major;
		compat_minor[n_compat] = minor;
		n_compat += 1;

#ifdef DEBUG
		printf("\tfound major = %d, minor = %d\n", major, minor);
#endif /*DEBUG*/
	}

	return NULL;
}

/* Build the list of ws compatibility defs we have.
 */
static void
workspace_build_compat(void)
{
	if (n_compat > 0)
		return;

	path_map_dir("$VIPSHOME/share/" PACKAGE "/compat", "*.*",
		(path_map_fn) workspace_build_compat_fn, NULL);
}

/* Given a major/minor (eg. read from a ws header), return non-zero if we have
 * a set of compat defs.
 */
int
workspace_have_compat(int major, int minor, int *best_major, int *best_minor)
{
	int i;
	int best;

#ifdef DEBUG
	printf("workspace_have_compat: searching for %d.%d\n", major, minor);
#endif /*DEBUG*/

	/* Sets of ws compatibility defs cover themselves and any earlier
	 * releases, as far back as the next set of compat defs. We need to
	 * search for the smallest compat version that's greater than the
	 * version number in the file.
	 */
	workspace_build_compat();
	best = -1;
	for (i = 0; i < n_compat; i++)
		if (major <= compat_major[i] && minor <= compat_minor[i])
			/* Found a possible compat set, is it better than the
			 * best we've seen so far?
			 */
			if (best == -1 ||
				compat_major[i] < compat_major[best] ||
				compat_minor[i] < compat_minor[best])
				best = i;
	if (best == -1)
		return 0;

#ifdef DEBUG
	printf("\tfound %d.%d\n", compat_major[best], compat_minor[best]);
#endif /*DEBUG*/

	if (best_major)
		*best_major = compat_major[best];
	if (best_minor)
		*best_minor = compat_minor[best];

	return 1;
}

void
workspace_get_version(Workspace *ws, int *major, int *minor)
{
	*major = ws->major;
	*minor = ws->minor;
}

gboolean
workspace_load_compat(Workspace *ws, int major, int minor)
{
	char pathname[VIPS_PATH_MAX];
	GSList *path;
	int best_major;
	int best_minor;

	if (workspace_have_compat(major, minor, &best_major, &best_minor)) {
		/* Make a private toolkitgroup local to this workspace to
		 * hold the compatibility defs we are planning to load.
		 */
		VIPS_UNREF(ws->kitg);
		ws->kitg = toolkitgroup_new(ws->sym);
		iobject_ref_sink(IOBJECT(ws->kitg));

		ws->kitg->compat_major = best_major;
		ws->kitg->compat_minor = best_minor;

		g_snprintf(pathname, VIPS_PATH_MAX,
			"$VIPSHOME/share/" PACKAGE "/compat/%d.%d", best_major, best_minor);
		path = path_parse(pathname);
		if (path_map(path, "*.def",
				(path_map_fn) workspace_load_toolkit, ws->kitg)) {
			path_free2(path);
			return FALSE;
		}
		path_free2(path);

#ifdef DEBUG
		printf("workspace_load_compat: loaded %d.%d\n",
			best_major, best_minor);
#endif /*DEBUG*/
	}

	return TRUE;
}

static void
workspace_class_init(WorkspaceClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	iObjectClass *iobject_class = IOBJECT_CLASS(class);
	iContainerClass *icontainer_class = (iContainerClass *) class;
	ModelClass *model_class = (ModelClass *) class;

	/* Create signals.
	 */

	/* Init methods.
	 */
	gobject_class->dispose = workspace_dispose;
	gobject_class->finalize = workspace_finalize;

	iobject_class->changed = workspace_changed;
	iobject_class->user_name = _("Tab");

	icontainer_class->child_add = workspace_child_add;
	icontainer_class->child_remove = workspace_child_remove;
	icontainer_class->current = workspace_current;

	model_class->view_new = workspace_view_new;
	model_class->load = workspace_load;
	model_class->save = workspace_save;
	model_class->empty = workspace_empty;

	/* Static init.
	 */
	model_register_loadable(MODEL_CLASS(class));
}

static void
workspace_init(Workspace *ws)
{
	/* We default to using the main toolkitgroup for our definitions.
	 * Unref and load private defs if we need compatibility.
	 */
	ws->kitg = main_toolkitgroup;
	g_object_ref(G_OBJECT(ws->kitg));

	ws->mode = WORKSPACE_MODE_REGULAR;

	ws->major = MAJOR_VERSION;
	ws->minor = MINOR_VERSION;

	ws->vp = ws->area;

	ws->scale = 1.0;
	ws->offset = 0.0;

	ws->local_defs = g_strdup(_("// local definitions for this tab\n"));

	workspace_all = g_slist_prepend(workspace_all, ws);
}

Workspace *
workspace_new(Workspacegroup *wsg, const char *name)
{
	Workspaceroot *wsr = wsg->wsr;

	Workspace *ws;

#ifdef DEBUG
	printf("workspace_new: %s\n", name);
#endif /*DEBUG*/

	if (compile_lookup(wsr->sym->expr->compile, name)) {
		error_top(_("Name clash"));
		error_sub(_("can't create workspace \"%s\", "
					"a symbol with that name already exists"),
			name);
		return NULL;
	}

	ws = WORKSPACE(g_object_new(WORKSPACE_TYPE, NULL));
	workspace_link(ws, wsg, name);
	icontainer_child_add(ICONTAINER(wsg), ICONTAINER(ws), -1);

	return ws;
}

/* Make the blank workspace we present the user with (in the absence of
 * anything else).
 */
Workspace *
workspace_new_blank(Workspacegroup *wsg)
{
	char name[256];
	Workspace *ws;

	workspaceroot_name_new(wsg->wsr, name);
	if (!(ws = workspace_new(wsg, name)))
		return NULL;

	/* Make an empty column.
	 */
	(void) workspace_column_pick(ws);

	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));

	iobject_set(IOBJECT(ws), NULL, _("Default empty tab"));

	return ws;
}

/* Get the bottom row from the current column.
 */
static Row *
workspace_get_bottom(Workspace *ws)
{
	return column_get_bottom(workspace_column_pick(ws));
}

gboolean
workspace_add_action(Workspace *ws,
	const char *name, const char *action, int nparam)
{
	Column *col = workspace_column_pick(ws);
	char txt[1024];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	/* Are there any selected symbols?
	 */
	vips_buf_appends(&buf, action);
	if (nparam > 0 && workspace_selected_any(ws)) {
		if (nparam != workspace_selected_num(ws)) {
			error_top(_("Wrong number of arguments"));
			error_sub(_("%s needs %d arguments, there are %d selected"),
				name, nparam, workspace_selected_num(ws));
			return FALSE;
		}

		vips_buf_appends(&buf, " ");
		workspace_selected_names(ws, &buf, " ");
		if (vips_buf_is_full(&buf)) {
			error_top(_("Overflow error"));
			error_sub(_("too many names selected"));
			return FALSE;
		}

		if (!workspace_add_def_recalc(ws, vips_buf_all(&buf)))
			return FALSE;
		workspace_deselect_all(ws);
	}
	else {
		/* Try to use the previous n items in this column as the
		 * arguments.
		 */
		if (!column_add_n_names(col, name, &buf, nparam) ||
			!workspace_add_def_recalc(ws, vips_buf_all(&buf)))
			return FALSE;
	}

	return TRUE;
}

int
workspace_number(void)
{
	return g_slist_length(workspace_all);
}

static void *
workspace_row_dirty(Row *row, int serial)
{
	return expr_dirty(row->expr, serial);
}

/* Recalculate selected items.
 */
gboolean
workspace_selected_recalc(Workspace *ws)
{
	if (workspace_selected_map(ws, (row_map_fn) workspace_row_dirty,
			GINT_TO_POINTER(link_serial_new()), NULL))
		return FALSE;

	/* Recalc even if autorecomp is off.
	 */
	symbol_recalculate_all_force(TRUE);

	workspace_deselect_all(ws);

	return TRUE;
}

static void *
workspace_selected_remove2(Row *row)
{
	if (row != row->top_row)
		return row;

	return NULL;
}

static void *
workspace_selected_remove3(Row *row, int *nsel)
{
	if (row->selected)
		*nsel += 1;

	return NULL;
}

static void *
workspace_selected_remove4(Column *col, GSList **cs)
{
	int nsel = 0;

	(void) column_map(col,
		(row_map_fn) workspace_selected_remove3, &nsel, NULL);
	if (nsel > 0)
		*cs = g_slist_prepend(*cs, col);

	return NULL;
}

static void *
workspace_selected_remove5(Column *col)
{
	Subcolumn *scol = col->scol;
	int nmembers = g_slist_length(ICONTAINER(scol)->children);

	if (nmembers > 0)
		icontainer_pos_renumber(ICONTAINER(scol));
	else
		IDESTROY(col);

	return NULL;
}

/* Remove selected items.
 *
 * 0. check all objects to be destroyed are top level rows
 * 1. look for and note all columns containing items we are going to delete
 * 2. loop over selected items, and delete them one-by-one.
 * 3. loop over the columns we noted in 1 and delete empty ones
 * 4. renumber affected columns
 */
static gboolean
workspace_selected_remove(Workspace *ws)
{
	g_autoptr(GSList) cs = NULL;
	Row *row;

	if ((row = (Row *) workspace_selected_map(ws,
			 (row_map_fn) workspace_selected_remove2, NULL, NULL))) {
		error_top(_("You can only remove top level rows"));
		error_sub(_("not all selected objects are top level rows"));
		return FALSE;
	}

	(void) workspace_map_column(ws,
		(column_map_fn) workspace_selected_remove4, &cs);
	(void) workspace_selected_map_sym(ws,
		(symbol_map_fn) iobject_destroy, NULL, NULL);
	(void) slist_map(cs,
		(SListMapFn) workspace_selected_remove5, NULL);

	symbol_recalculate_all();
	workspace_set_modified(ws, TRUE);

	return TRUE;
}

static void
workspace_selected_remove_yesno_cb(GtkWindow *window, gpointer user_data)
{
	Workspace *ws = WORKSPACE(user_data);

	if (!workspace_selected_remove(ws))
		workspace_show_error(ws);
}

/* Ask before removing selected. If there's only one row selected, don't
 * bother.
 */
void
workspace_selected_remove_yesno(Workspace *ws, GtkWindow *window)
{
	if (workspace_selected_num(ws) == 1) {
		if (!workspace_selected_remove(ws))
			workspace_show_error(ws);
	}
	else if (workspace_selected_num(ws) > 1) {
		char txt[30];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		workspace_selected_names(ws, &buf, ", ");
		alert_yesno(window,
			workspace_selected_remove_yesno_cb, ws,
			_("Are you sure?"),
			_("Are you sure you want to delete rows %s?"),
			vips_buf_all(&buf));
	}
}

/* Sub fn of below ... add a new index expression.
 */
static gboolean
workspace_ungroup_add_index(Row *row, const char *fmt, int i)
{
	static char txt[200];
	static VipsBuf buf = VIPS_BUF_STATIC(txt);

	vips_buf_rewind(&buf);
	row_qualified_name(row, &buf);
	vips_buf_appendf(&buf, fmt, i);
	if (!workspace_add_def_recalc(row->ws, vips_buf_all(&buf)))
		return FALSE;

	return TRUE;
}

static void *
workspace_ungroup_row(Row *row)
{
	PElement *root = &row->expr->root;
	gboolean result;
	PElement value;
	int length;
	int i;

	if (!heap_is_instanceof(CLASS_GROUP, root, &result))
		return row;
	if (result) {
		if (!class_get_member(root, MEMBER_VALUE, NULL, &value) ||
			(length = heap_list_length_max(&value, 100)) < 0)
			return row;

		for (i = 0; i < length; i++)
			if (!workspace_ungroup_add_index(row,
					".value?%d", i))
				return row;
	}
	else {
		if (!heap_is_list(root, &result))
			return row;
		if (result) {
			if ((length = heap_list_length_max(root, 100)) < 0)
				return row;

			for (i = 0; i < length; i++)
				if (!workspace_ungroup_add_index(row,
						"?%d", i))
					return row;
		}
		else {
			char txt[100];
			VipsBuf buf = VIPS_BUF_STATIC(txt);

			row_qualified_name(row, &buf);
			error_top(_("Unable to ungroup"));
			error_sub(_("row \"%s\" is not a Group or a list"),
				vips_buf_all(&buf));

			return row;
		}
	}

	return NULL;
}

/* Ungroup the selected object(s), or the bottom object.
 */
gboolean
workspace_selected_ungroup(Workspace *ws)
{
	if (!workspace_selected_any(ws)) {
		Row *row;

		if ((row = workspace_get_bottom(ws))) {
			if (workspace_ungroup_row(row))
				return FALSE;

			symbol_recalculate_all();
		}
	}
	else {
		/* Ungroup selected symbols.
		 */
		if (workspace_selected_map(ws,
				(row_map_fn) workspace_ungroup_row, NULL, NULL)) {
			symbol_recalculate_all();
			return FALSE;
		}
		symbol_recalculate_all();
		workspace_deselect_all(ws);
	}

	return TRUE;
}

/* Group the selected object(s).
 */
gboolean
workspace_selected_group(Workspace *ws)
{
	char txt[MAX_STRSIZE];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	if (!workspace_selected_any(ws)) {
		Row *row;

		if ((row = workspace_get_bottom(ws)))
			row_select(row);
	}

	vips_buf_appends(&buf, "Group [");
	workspace_selected_names(ws, &buf, ", ");
	vips_buf_appends(&buf, "]");
	if (!workspace_add_def_recalc(ws, vips_buf_all(&buf)))
		return FALSE;
	workspace_deselect_all(ws);

	return TRUE;
}

void
workspace_error_sanity(Workspace *ws)
{
#ifdef DEBUG
	if (ws->errors) {
		g_assert(!ws->last_error || ws->last_error->err);
		g_assert(!ws->last_error || g_slist_find(ws->errors, ws->last_error));

		GSList *p;
		for (p = ws->errors; p; p = p->next) {
			Row *row = ROW(p->data);

			g_assert(row->top_row);
			g_assert(row->top_row->expr);

			g_assert(row->err);
			g_assert(row->expr);
			g_assert(strlen(row->expr->error_top) > 0);
			g_assert(strlen(row->expr->error_sub) > 0);
		}
	}
	else
		g_assert(ws->last_error == NULL);
#endif /*DEBUG*/
}

static Row *
workspace_test_error(Row *row, Workspace *ws, int *found)
{
	g_assert(row->err);

	/* Found next?
	 */
	if (*found)
		return row;

	if (row == ws->last_error) {
		/* Found the last one ... return the next one.
		 */
		*found = 1;
		return NULL;
	}

	return NULL;
}

/* FALSE for no errors.
 */
gboolean
workspace_next_error(Workspace *ws)
{
	workspace_error_sanity(ws);

	int found;

	if (!ws->errors) {
		error_top(_("No errors in tab"));
		error_sub("%s",
			_("there are no errors (that I can see) in this tab"));
		return FALSE;
	}

	/* Search for the one after the last one.
	 */
	found = 0;
	ws->last_error = (Row *) slist_map2(ws->errors,
		(SListMap2Fn) workspace_test_error, ws, &found);

	/* NULL? We've hit end of table, start again.
	 */
	if (!ws->last_error) {
		found = 1;
		ws->last_error = (Row *) slist_map2(ws->errors,
			(SListMap2Fn) workspace_test_error, ws, &found);
	}

	/* *must* have one now.
	 */
	g_assert(ws->last_error &&
		ws->last_error->err);

	model_scrollto(MODEL(ws->last_error), MODEL_SCROLL_TOP);

	error_top("%s", ws->last_error->expr->error_top);
	error_sub("%s", ws->last_error->expr->error_sub);

	workspace_error_sanity(ws);

	return TRUE;
}

void
workspace_set_mode(Workspace *ws, WorkspaceMode mode)
{
	if (ws->mode != mode) {
		ws->mode = mode;

		/* Rebuild all the views. Yuk! It would be better to get the
		 * views that change with workspace mode to watch the
		 * enclosing workspace and update on that. But we'd have
		 * connections from almost every object in the ws. We don't
		 * change mode very often, so just loop over them all.
		 */
		icontainer_map_all(ICONTAINER(ws),
			(icontainer_map_fn) iobject_changed, NULL);
	}
}

/* New ws private defs.
 */
gboolean
workspace_local_set(Workspace *ws, const char *txt)
{
	/* New kit for defs ... will destroy any old defs, since we can't have
	 * two kits with the same name. Don't register it, we don't want it
	 * to be autosaved on quit.
	 */
	ws->local_kit = toolkit_new(ws->local_kitg, "Workspace Locals");
	filemodel_unregister(FILEMODEL(ws->local_kit));
	VIPS_SETSTR(ws->local_defs, txt);
	iobject_changed(IOBJECT(ws));

	attach_input_string(txt);
	if (!parse_toplevel(ws->local_kit, 0))
		return FALSE;

	return TRUE;
}

gboolean
workspace_local_set_from_file(Workspace *ws, const char *fname)
{
	iOpenFile *of;
	char *txt;

	if (!(of = ifile_open_read("%s", fname)))
		return FALSE;
	if (!(txt = ifile_read(of))) {
		ifile_close(of);
		return FALSE;
	}
	if (!workspace_local_set(ws, txt)) {
		g_free(txt);
		ifile_close(of);
		return FALSE;
	}

	filemodel_set_filename(FILEMODEL(ws->local_kit), fname);

	g_free(txt);
	ifile_close(of);

	return TRUE;
}

/* Merge file into this workspace.
 */
gboolean
workspace_merge_file(Workspace *ws, const char *filename)
{
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));

	return workspacegroup_merge_columns(wsg, filename);
}

/* Duplicate selected rows in this workspace.
 */
gboolean
workspace_selected_duplicate(Workspace *ws)
{
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	char filename[VIPS_PATH_MAX];

	if (!workspace_selected_any(ws)) {
		Row *row;

		if ((row = workspace_get_bottom(ws)))
			row_select(row);
	}

	if (!temp_name(filename, IOBJECT(ws)->name, "ws"))
		return FALSE;
	if (!workspace_selected_save(ws, filename))
		return FALSE;

	progress_begin();

	if (!workspacegroup_merge_rows(wsg, filename)) {
		progress_end();
		unlinkf("%s", filename);

		return FALSE;
	}
	unlinkf("%s", filename);

	symbol_recalculate_all();
	workspace_deselect_all(ws);
	column_scrollto(workspace_get_column(ws), MODEL_SCROLL_BOTTOM);

	progress_end();

	return TRUE;
}

/* Bounding box of columns to be saved. Though we only really set top/left.
 */
static void *
workspace_selected_save_box(Column *col, VipsRect *box)
{
	if (model_save_test(MODEL(col))) {
		if (vips_rect_isempty(box)) {
			box->left = col->x;
			box->top = col->y;
			box->width = 100;
			box->height = 100;
		}
		else {
			box->left = VIPS_MIN(box->left, col->x);
			box->top = VIPS_MIN(box->top, col->y);
		}
	}

	return NULL;
}

/* Save just the selected objects.
 */
gboolean
workspace_selected_save(Workspace *ws, const char *filename)
{
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	VipsRect box = { 0 };

	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));

	workspace_map_column(ws,
		(column_map_fn) workspace_selected_save_box,
		&box);

	if (!workspacegroup_save_selected(wsg, filename))
		return FALSE;

	return TRUE;
}

gboolean
workspace_rename(Workspace *ws, const char *name)
{
	if (!symbol_rename(ws->sym, name))
		return FALSE;
	iobject_set(IOBJECT(ws), IOBJECT(ws->sym)->name, NULL);
	workspace_set_modified(ws, TRUE);

	symbol_recalculate_all();

	return TRUE;
}

void
workspace_set_locked(Workspace *ws, gboolean locked)
{
	if (ws->locked != locked) {
		ws->locked = locked;
		iobject_changed(IOBJECT(ws));
		workspace_set_modified(ws, TRUE);
	}
}

gboolean
workspace_duplicate(Workspace *ws)
{
	Workspacegroup *wsg = workspace_get_workspacegroup(ws);

	char filename[VIPS_PATH_MAX];

	if (!temp_name(filename, IOBJECT(ws)->name, "ws"))
		return FALSE;
	icontainer_current(ICONTAINER(wsg), ICONTAINER(ws));
	if (!workspacegroup_save_current(wsg, filename))
		return FALSE;

	progress_begin();

	if (!workspacegroup_merge_workspaces(wsg, filename)) {
		progress_end();
		unlinkf("%s", filename);

		return FALSE;
	}
	unlinkf("%s", filename);

	symbol_recalculate_all();

	progress_end();

	return TRUE;
}
