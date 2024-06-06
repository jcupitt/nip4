/* abstract base class for things which form the model half of a model/view
 * pair
 */

/*

	Copyright (C) 1991-2003 The National Gallery
	Copyright (C) 2004-2023 libvips.org

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

/* Stuff from bison ... needed as we call the lexer directly to rewrite
 * expressions.
 */
#include "parse.h"

/* Our signals.
 */
enum {
	SIG_SCROLLTO, /* Views should try to make themselves visible */
	SIG_LAYOUT,	  /* Views should lay out their children */
	SIG_RESET,	  /* Reset edit mode in views */
	SIG_FRONT,	  /* Bring views to front */
	SIG_DISPLAY,  /* Display on/off */
	SIG_LAST
};

G_DEFINE_TYPE(Model, model, ICONTAINER_TYPE)

static guint model_signals[SIG_LAST] = { 0 };

/* All the model classes which can be built from XML.
 */
static GSList *model_registered_loadable = NULL;

/* The loadstate the lexer gets its rename stuff from.
 */
ModelLoadState *model_loadstate = NULL;

/* Rename list functions.
 */
static void *
model_rename_destroy(ModelRename *rename)
{
	VIPS_FREE(rename->old_name);
	VIPS_FREE(rename->new_name);
	VIPS_FREE(rename);

	return NULL;
}

static ModelRename *
model_rename_new(const char *old_name, const char *new_name)
{
	ModelRename *rename;

	if (!(rename = INEW(NULL, ModelRename)))
		return NULL;
	rename->old_name = g_strdup(old_name);
	rename->new_name = g_strdup(new_name);
	if (!rename->old_name || !rename->new_name) {
		model_rename_destroy(rename);
		return NULL;
	}

	return rename;
}

gboolean
model_loadstate_rename_new(ModelLoadState *state,
	const char *old_name, const char *new_name)
{
	/* Make a rename, even if old_name == new_name, since we want to have
	 * new_name on the taken list.
	 */
	ModelRename *rename;

	if (!(rename = model_rename_new(old_name, new_name)))
		return FALSE;
	state->renames = g_slist_prepend(state->renames, rename);

	return TRUE;
}

static void *
model_loadstate_taken_sub(ModelRename *rename, const char *name)
{
	if (strcmp(rename->new_name, name) == 0)
		return rename;

	return NULL;
}

/* Is something already being renamed to @name.
 */
gboolean
model_loadstate_taken(ModelLoadState *state, const char *name)
{
	return slist_map(state->renames,
			   (SListMapFn) model_loadstate_taken_sub, (char *) name) != NULL;
}

gboolean
model_loadstate_column_rename_new(ModelLoadState *state,
	const char *old_name, const char *new_name)
{
	if (strcmp(old_name, new_name) != 0) {
		ModelRename *rename;

		if (!(rename = model_rename_new(old_name, new_name)))
			return FALSE;
		state->column_renames =
			g_slist_prepend(state->column_renames, rename);
	}

	return TRUE;
}

/* Is something already being renamed to @name.
 */
gboolean
model_loadstate_column_taken(ModelLoadState *state, const char *name)
{
	return !!slist_map(state->column_renames,
		(SListMapFn) model_loadstate_taken_sub, (char *) name);
}

void
model_loadstate_destroy(ModelLoadState *state)
{
	/* We are probably registered as the xml error handler ... unregister!
	 */
	xmlSetGenericErrorFunc(NULL, NULL);

	VIPS_FREE(state->filename);
	VIPS_FREE(state->filename_user);
	VIPS_FREEF(xmlFreeDoc, state->xdoc);
	slist_map(state->renames,
		(SListMapFn) model_rename_destroy, NULL);
	slist_map(state->column_renames,
		(SListMapFn) model_rename_destroy, NULL);
	g_slist_free(state->renames);

	if (state->old_dir) {
		path_rewrite_add(state->old_dir, NULL, FALSE);
		VIPS_FREE(state->old_dir);
	}

	VIPS_FREE(state);
}

static void
model_loadstate_error(ModelLoadState *state, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) vips_buf_vappendf(&state->error_log, fmt, ap);
	va_end(ap);
}

static void
model_loadstate_error_get(ModelLoadState *state)
{
	char *utf8;

	utf8 = f2utf8(vips_buf_all(&state->error_log));
	error_top(_("Load failed"));
	error_sub(_("Unable to load from file \"%s\". Error log is:\n%s"),
		state->filename, utf8);
	g_free(utf8);
}

ModelLoadState *
model_loadstate_new(const char *filename, const char *filename_user)
{
	ModelLoadState *state;

	if (!(state = INEW(NULL, ModelLoadState)))
		return NULL;
	state->xdoc = NULL;
	state->renames = NULL;
	state->column_renames = NULL;
	state->major = MAJOR_VERSION;
	state->minor = MINOR_VERSION;
	state->micro = MICRO_VERSION;
	state->rewrite_path = FALSE;
	state->old_dir = FALSE;

	state->filename = g_strdup(filename);
	if (filename_user)
		state->filename_user = g_strdup(filename_user);
	else
		state->filename_user = g_strdup(filename);
	if (!state->filename ||
		!state->filename_user) {
		model_loadstate_destroy(state);
		return NULL;
	}

	vips_buf_init_static(&state->error_log,
		state->error_log_buffer, MAX_STRSIZE);

	xmlSetGenericErrorFunc(state,
		(xmlGenericErrorFunc) model_loadstate_error);
	if (!(state->xdoc = (xmlDoc *) callv_string_filename(
			  (callv_string_fn) xmlParseFile,
			  state->filename, NULL, NULL, NULL))) {
		model_loadstate_error_get(state);
		model_loadstate_destroy(state);
		return NULL;
	}

	return state;
}

ModelLoadState *
model_loadstate_new_openfile(iOpenFile *of)
{
	ModelLoadState *state;
	char load_buffer[MAX_STRSIZE];

	if (!(state = INEW(NULL, ModelLoadState)))
		return NULL;
	state->renames = NULL;
	state->xdoc = NULL;
	if (!(state->filename = g_strdup(of->fname))) {
		model_loadstate_destroy(state);
		return NULL;
	}
	vips_buf_init_static(&state->error_log,
		state->error_log_buffer, MAX_STRSIZE);

	xmlSetGenericErrorFunc(state,
		(xmlGenericErrorFunc) model_loadstate_error);
	if (!ifile_read_buffer(of, load_buffer, MAX_STRSIZE)) {
		model_loadstate_destroy(state);
		return NULL;
	}
	if (!(state->xdoc = xmlParseMemory(load_buffer, MAX_STRSIZE))) {
		model_loadstate_error_get(state);
		model_loadstate_destroy(state);
		return NULL;
	}

	return state;
}

/* If old_name is on the global rewrite list, rewrite it! Called from the
 * lexer.
 */
char *
model_loadstate_rewrite_name(char *name)
{
	ModelLoadState *state = model_loadstate;
	GSList *i;

	if (!state || !state->renames)
		return NULL;

	for (i = state->renames; i; i = i->next) {
		ModelRename *rename = (ModelRename *) (i->data);

		if (strcmp(name, rename->old_name) == 0)
			return rename->new_name;
	}

	return NULL;
}

/* Use the lexer to rewrite an expression, swapping all symbols on the rewrite
 * list.
 */
void
model_loadstate_rewrite(ModelLoadState *state, char *old_rhs, char *new_rhs)
{
	int yychar;
	extern int yylex(void);

	model_loadstate = state;
	attach_input_string(old_rhs);
	if (setjmp(parse_error_point)) {
		/* Here for yyerror in lex. Just ignore errors --- the parser
		 * will spot them later anyway.
		 */
		model_loadstate = NULL;
		return;
	}

	/* Lex and rewrite.
	 */
	state->rewrite_path = FALSE;
	while ((yychar = yylex()) > 0) {
		/* If we see an Image_file or Matrix_file token, rewrite the
		 * following token if it's a string constant.
		 */
		state->rewrite_path = FALSE;
		if (yychar == TK_IDENT &&
			strcmp(yylval.yy_name, "Image_file") == 0)
			state->rewrite_path = TRUE;
		if (yychar == TK_IDENT &&
			strcmp(yylval.yy_name, "Matrix_file") == 0)
			state->rewrite_path = TRUE;

		free_lex(yychar);
	}

	model_loadstate = NULL;

	/* Take copy of lexed and rewritten stuff.
	 */
	vips_strncpy(new_rhs, vips_buf_all(&lex_text), MAX_STRSIZE);
}

View *
model_view_new(Model *model, View *parent)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);
	View *view;

	if (!model_class->view_new)
		return NULL;

	view = model_class->view_new(model, parent);
	if (view)
		view_link(view, model, parent);

	return view;
}

/* Register a model subclass as loadable ... what we allow when we load an
 * XML node's children.
 */
void
model_register_loadable(ModelClass *model_class)
{
	model_registered_loadable = g_slist_prepend(model_registered_loadable,
		model_class);
}

void
model_edit(GtkWidget *parent, Model *model)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);

	if (model_class->edit)
		model_class->edit(parent, model);
}

void
model_scrollto(Model *model, ModelScrollPosition position)
{
	g_assert(IS_MODEL(model));

	g_signal_emit(G_OBJECT(model),
		model_signals[SIG_SCROLLTO], 0, position);
}

void
model_layout(Model *model)
{
	g_assert(IS_MODEL(model));

	g_signal_emit(G_OBJECT(model), model_signals[SIG_LAYOUT], 0);
}

void
model_front(Model *model)
{
	g_assert(IS_MODEL(model));

	g_signal_emit(G_OBJECT(model), model_signals[SIG_FRONT], 0);
}

void
model_display(Model *model, gboolean display)
{
	if (model) {
		g_assert(IS_MODEL(model));

		g_signal_emit(G_OBJECT(model), model_signals[SIG_DISPLAY], 0, display);
	}
}

void *
model_reset(Model *model)
{
	g_assert(IS_MODEL(model));

	g_signal_emit(G_OBJECT(model), model_signals[SIG_RESET], 0);

	return NULL;
}

void *
model_save(Model *model, xmlNode *xnode)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);

	if (model_save_test(model)) {
		if (model_class->save && !model_class->save(model, xnode))
			return model;
	}

	return NULL;
}

gboolean
model_save_test(Model *model)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);

	if (model_class->save_test)
		return model_class->save_test(model);

	return TRUE;
}

void *
model_save_text(Model *model, iOpenFile *of)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);

	if (model_class->save_text && !model_class->save_text(model, of))
		return model;

	return NULL;
}

void *
model_load(Model *model,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);

	if (model_class->load) {
		if (!model_class->load(model, state, parent, xnode))
			return model;
	}
	else {
		error_top(_("Not implemented"));
		error_sub(_("_%s() not implemented for class \"%s\"."),
			"load",
			G_OBJECT_CLASS_NAME(model_class));
	}

	return NULL;
}

void *
model_load_text(Model *model, Model *parent, iOpenFile *of)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);

	if (model_class->load_text) {
		if (!model_class->load_text(model, parent, of))
			return model;
	}
	else {
		error_top("Not implemented");
		error_sub(_("_%s() not implemented for class \"%s\"."),
			"load_text",
			G_OBJECT_CLASS_NAME(model_class));
	}

	return NULL;
}

void *
model_empty(Model *model)
{
	ModelClass *model_class = MODEL_GET_CLASS(model);

	if (model_class->empty)
		model_class->empty(model);

	return NULL;
}

static void
model_real_scrollto(Model *model, ModelScrollPosition position)
{
}

static void
model_real_front(Model *model)
{
}

static void
model_real_display(Model *model, gboolean display)
{
	if (display != model->display) {
		model->display = display;
		iobject_changed(IOBJECT(model));
	}
}

static xmlNode *
model_real_save(Model *model, xmlNode *xnode)
{
	const char *tname = G_OBJECT_TYPE_NAME(model);
	xmlNode *xthis;

	if (!(xthis = xmlNewChild(xnode, NULL, (xmlChar *) tname, NULL))) {
		error_top(_("XML library error"));
		error_sub(_("model_save: xmlNewChild() failed"));
		return NULL;
	}

	if (icontainer_map(ICONTAINER(model),
			(icontainer_map_fn) model_save, xthis, NULL))
		return NULL;

	if (model->window_width != -1) {
		if (!set_iprop(xthis, "window_x", model->window_x) ||
			!set_iprop(xthis, "window_y", model->window_y) ||
			!set_iprop(xthis, "window_width",
				model->window_width) ||
			!set_iprop(xthis, "window_height",
				model->window_height))
			return NULL;
	}

	return xthis;
}

static void *
model_new_xml_sub(ModelClass *model_class,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	GType type = G_TYPE_FROM_CLASS(model_class);
	const char *tname = g_type_name(type);

	if (g_ascii_strcasecmp((char *) xnode->name, tname) == 0) {
		Model *model = MODEL(g_object_new(type, NULL));

		if (model_load(model, state, parent, xnode)) {
			g_object_unref(model);
			return model_class;
		}

		return NULL;
	}

	return NULL;
}

gboolean
model_new_xml(ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	/*

		FIXME ... slow! some sort of hash? time this at some point

	 */
	if (slist_map3(model_registered_loadable,
			(SListMap3Fn) model_new_xml_sub, state, parent, xnode))
		return FALSE;

	return TRUE;
}

static gboolean
model_real_load(Model *model,
	ModelLoadState *state, Model *parent, xmlNode *xnode)
{
	const char *tname = G_OBJECT_TYPE_NAME(model);
	xmlNode *i;

	/* Should just be a sanity check.
	 */
	if (g_ascii_strcasecmp((char *) xnode->name, tname) != 0) {
		error_top(_("XML load error"));
		error_sub(_("Can't load node of type \"%s\" into "
					"object of type \"%s\""),
			xnode->name, tname);
		return FALSE;
	}

	(void) get_iprop(xnode, "window_x", &model->window_x);
	(void) get_iprop(xnode, "window_y", &model->window_y);
	(void) get_iprop(xnode, "window_width", &model->window_width);
	(void) get_iprop(xnode, "window_height", &model->window_height);

	if (!ICONTAINER(model)->parent)
		icontainer_child_add(ICONTAINER(parent),
			ICONTAINER(model), -1);

	for (i = xnode->children; i; i = i->next)
		if (!model_new_xml(state, MODEL(model), i))
			return FALSE;

#ifdef DEBUG
	printf("model_real_load: finished loading %s (name = %s)\n",
		tname, IOBJECT(model)->name);
#endif /*DEBUG*/

	return TRUE;
}

static void
model_real_empty(Model *model)
{
	icontainer_map(ICONTAINER(model),
		(icontainer_map_fn) icontainer_child_remove, NULL, NULL);
}

static void
model_class_init(ModelClass *class)
{
	iObjectClass *object_class = IOBJECT_CLASS(class);

	class->view_new = NULL;
	class->scrollto = model_real_scrollto;
	class->layout = NULL;
	class->front = model_real_front;
	class->display = model_real_display;
	class->reset = NULL;
	class->save = model_real_save;
	class->save_test = NULL;
	class->save_text = NULL;
	class->load = model_real_load;
	class->load_text = NULL;
	class->empty = model_real_empty;

	/* Create signals.
	 */
	model_signals[SIG_SCROLLTO] = g_signal_new("scrollto",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ModelClass, scrollto),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);
	model_signals[SIG_LAYOUT] = g_signal_new("layout",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ModelClass, layout),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	model_signals[SIG_FRONT] = g_signal_new("front",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ModelClass, front),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	model_signals[SIG_RESET] = g_signal_new("reset",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ModelClass, reset),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	model_signals[SIG_DISPLAY] = g_signal_new("display",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ModelClass, display),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1,
		G_TYPE_BOOLEAN);
}

static void
model_init(Model *model)
{
	model->display = TRUE;

	/* Magic: -1 means none of these saved settings are valid. It'd be
	 * nice to do something better, but we'd break old workspaces.
	 */
	model->window_x = 0;
	model->window_y = 0;
	model->window_width = -1;
	model->window_height = 0;
}

static void
model_check_destroy_yesno(GtkWidget *parent, void *user_data)
{
	Model *model = MODEL(user_data);

    IDESTROY(model);
    symbol_recalculate_all();
}

void
model_check_destroy(GtkWidget *parent, Model *model)
{
	char txt[30];
    VipsBuf buf = VIPS_BUF_STATIC( txt );
    const char *name;

    if (IS_SYMBOL(model)) {
        symbol_qualified_name(SYMBOL(model), &buf);
        name = vips_buf_all(&buf);
    }
    else
        name = IOBJECT(model)->name;

	alert_yesno(view_get_window(parent), model_check_destroy_yesno, model,
        _("Are you sure you want to delete %s \"%s\"?"),
        IOBJECT_GET_CLASS_NAME(model), name);
}

/* Useful for icontainer_map_all() ... trigger all heapmodel_clear_edited()
 * methods.
 */
void *
model_clear_edited(Model *model)
{
	void *result;

	if (IS_HEAPMODEL(model) &&
		(result = heapmodel_clear_edited(HEAPMODEL(model))))
		return result;

	return NULL;
}
