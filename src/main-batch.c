// C startup

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
#define DEBUG
#define DEBUG_FATAL
 */

#include "nip4.h"

static char *main_option_output = NULL;
static gboolean main_option_test = FALSE;
static char *main_option_prefix = NULL;
static gboolean main_option_version = FALSE;
static gboolean main_option_workspace = FALSE;
static char *main_option_expression = NULL;
static char **main_option_set = NULL;
static gboolean main_option_verbose = FALSE;
static gboolean main_option_print = FALSE;

static GOptionEntry main_batch_options[] = {
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &main_option_output,
        N_("write value of 'main' to FILE"), "FILE" },
    { "test", 'T', 0, G_OPTION_ARG_NONE, &main_option_test,
        N_("test for errors and quit"), NULL },
    { "prefix", 'x', 0, G_OPTION_ARG_FILENAME, &main_option_prefix,
        N_("start as if installed to PREFIX"), "PREFIX" },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &main_option_version,
        N_("print version number"), NULL },
    { "workspace", 'w', 0, G_OPTION_ARG_NONE, &main_option_workspace,
        N_("load args as workspaces"), NULL },
    { "i18n", 'i', 0, G_OPTION_ARG_NONE, &main_option_i18n,
        N_("output strings for internationalisation"), NULL },
    { "expression", 'e', 0, G_OPTION_ARG_STRING, &main_option_expression,
        N_("evaluate and print EXPRESSION"), "EXPRESSION" },
    { "verbose", 'V', 0, G_OPTION_ARG_NONE, &main_option_verbose,
        N_("verbose error output"), NULL },
    { "set", '=', 0, G_OPTION_ARG_STRING_ARRAY, &main_option_set,
        N_("set values"), NULL },
    { "print", 'p', 0, G_OPTION_ARG_NONE, &main_option_print,
        N_("print \"main\""), NULL },

    { NULL }
};

/* Accumulate startup errors here.
 */
static char main_start_error_txt[MAX_STRSIZE];
static VipsBuf main_start_error = VIPS_BUF_STATIC(main_start_error_txt);

static void
main_log_add(const char *fmt, ...)
{
    va_list ap;

	va_start(ap, fmt);
    vips_buf_vappendf(&main_start_error, fmt, ap);
	va_end(ap);
}

static const char *
main_log_get(void)
{
    return vips_buf_all(&main_start_error);
}

static gboolean
main_log_is_empty(void)
{
    return vips_buf_is_empty(&main_start_error);
}

/* Print all errors and quit.
 */
static void
main_error_exit(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    (void) vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    if (!g_str_equal(error_get_top(), "")) {
        fprintf(stderr, "%s\n", error_get_top());
        if (!g_str_equal(error_get_sub(), ""))
            fprintf(stderr, "%s\n", error_get_sub());
    }

    if (main_option_verbose) {
        char txt[MAX_STRSIZE];
        VipsBuf buf = VIPS_BUF_STATIC(txt);

        slist_map(expr_error_all, (SListMapFn) expr_error_print, &buf);
        fprintf(stderr, "%s", vips_buf_all(&buf));

		if (!main_log_is_empty())
			fprintf(stderr, "%s", main_log_get());
    }

    exit(1);
}

static gboolean main_save_item(PElement *item, char *filename);

static gboolean
main_save_list(PElement *list, char *filename)
{
    int length;

    if ((length = heap_list_length(list)) < 0)
        return FALSE;

    for (int i = 0; i < length; i++) {
        PElement item;

        if (!heap_list_index(list, i, &item) ||
            !main_save_item(&item, filename))
            return FALSE;
    }

    return TRUE;
}

/* Can increment filename.
 */
static gboolean
main_save_item(PElement *item, char *filename)
{
    gboolean result;
    Imageinfo *ii;
    char buf[VIPS_PATH_MAX];

    /* We don't want $VAR etc. in the filename we pass down to the file
     * ops.
     */
    g_strlcpy(buf, filename, VIPS_PATH_MAX);
    path_expand(buf);

    if (!heap_is_instanceof(CLASS_GROUP, item, &result))
        return FALSE;
    if (result) {
        PElement value;

        if (!class_get_member(item, MEMBER_VALUE, NULL, &value) ||
            !main_save_list(&value, filename))
            return FALSE;
    }

    if (PEISLIST(item)) {
        if (!main_save_list(item, filename))
            return FALSE;
    }

    if (PEISIMAGE(item)) {
        if (!heap_get_image(item, &ii) ||
            vips_image_write_to_file(ii->image, buf, NULL))
            return FALSE;

        increment_filename(filename);
    }

    if (!heap_is_instanceof(CLASS_IMAGE, item, &result))
        return FALSE;
    if (result) {
        PElement value;

        if (!class_get_member(item, MEMBER_VALUE, NULL, &value) ||
            !heap_get_image(&value, &ii) ||
            vips_image_write_to_file(ii->image, buf, NULL))
            return FALSE;

        increment_filename(filename);
    }

    return TRUE;
}

/* Output a single main.
 */
static void
main_print_main(Symbol *sym)
{
    PElement *root;

    /* Strict reduction of this object, then print.
     */
    root = &sym->expr->root;
    if (!symbol_recalculate_check(sym) ||
        !reduce_pelement(reduce_context, reduce_spine_strict, root))
        main_error_exit(_( "error calculating \"%s\""), symbol_name_scope(sym));

    if (main_option_output) {
        char filename[VIPS_PATH_MAX];

        g_strlcpy(filename, main_option_output, VIPS_PATH_MAX);
        if (!main_save_item(root, filename))
            main_error_exit(_( "error saving \"%s\""), symbol_name_scope(sym));
    }

    graph_value(root);
}

static void
main_load_file(Workspace *ws, const char *filename)
{
	if (vips_iscasepostfix(filename, ".def")) {
		Toolkit *kit;

        if (!(kit = toolkit_new_from_file(main_toolkitgroup, filename)))
			main_error_exit("unable to load toolkit \"%s\"", filename);
	}
	else if (vips_iscasepostfix(filename, ".ws")) {
		Workspacegroup *wsg;

		if (!(wsg = workspacegroup_new_from_file(main_workspaceroot,
			filename, filename)))
			main_error_exit("unable to load workspace \"%s\"", filename);
	}
	else {
		Symbol *sym;

		if (!(sym = workspace_load_file(ws, filename)))
			main_error_exit("unable to load object \"%s\"", filename);
	}
}

static gboolean
main_set(const char *str)
{
    Symbol *sym;

    attach_input_string(str);
    if (!(sym = parse_set_symbol()))
        return FALSE;

    /* Put the input just after the '=', ready to parse a RHS into the
     * symbol.
     */
    attach_input_string(str +
		VIPS_CLIP(0, input_state.charpos - 1, strlen(str)));

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

        return FALSE;
    }

    symbol_made(sym);

    /* Is there a row? Make sure any modified text there can't zap our new
     * text.
     */
    if (sym->expr->row) {
        Row *row = sym->expr->row;

        heapmodel_set_modified(HEAPMODEL(row->child_rhs->itext), FALSE);
    }

    return TRUE;
}

static void *
main_print_ws(Workspace *ws, gboolean *found)
{
    Symbol *sym;

    if ((sym = compile_lookup(ws->sym->expr->compile, "main"))) {
        main_print_main(sym);
        *found = TRUE;
    }

    return NULL;
}

/* Make nip's argc/argv[].
 */
static void
main_build_argv(int argc, char **argv)
{
	Toolkit *kit = toolkit_new(main_toolkitgroup, "_args");

	char txt[MAX_STRSIZE];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	vips_buf_rewind(&buf);
	vips_buf_appendf(&buf, "argc = %d;", argc);
	attach_input_string(vips_buf_all(&buf));
	(void) parse_onedef(kit, -1);

	vips_buf_rewind(&buf);
	vips_buf_appendf(&buf, "argv = [");
	for (int i = 0; i < argc; i++) {
		/* Ignore "--" args. Consider eg.
		 *
		 * 	./try201.nip4 -o x.v -- -12 ~/pics/shark.jpg
		 *
		 * if we didn't remove --, all scripts would need to.
		 */
		if (g_str_equal(argv[i], "--"))
			continue;

		if(i > 0)
			vips_buf_appendf(&buf, ", ");
		vips_buf_appendf(&buf, "\"%s\"", argv[i]);
	}
	vips_buf_appendf(&buf, "];");

	attach_input_string(vips_buf_all(&buf));
	if (!parse_onedef(kit, -1))
		main_log_add("%s\n", error_get_sub());

	filemodel_set_modified(FILEMODEL(kit), FALSE);
}

static void
main_expression(const char *expression)
{
	Toolkit *kit = toolkit_new(main_toolkitgroup, "_expression");

	char txt[MAX_STRSIZE];
	VipsBuf buf = VIPS_BUF_STATIC(txt);

	vips_buf_appendf(&buf, "main = %s;", main_option_expression);
	attach_input_string(vips_buf_all(&buf));
	(void) parse_onedef(kit, -1);

	filemodel_set_modified(FILEMODEL(kit), FALSE);
}

int
main(int argc, char **argv)
{
	if (VIPS_INIT(argv[0]))
		vips_error_exit("unable to start libvips");

    main_option_batch = TRUE;
	main_option_no_load_menus = TRUE;

    /* Set localised application name.
     */
    g_set_application_name(_(PACKAGE));

    GOptionContext *context =
		g_option_context_new(_("- batch interface to nip4"));
    GOptionGroup *main_group = g_option_group_new(NULL, NULL, NULL, NULL, NULL);
    g_option_group_add_entries(main_group, main_batch_options);
    vips_add_option_entries(main_group);
    g_option_group_set_translation_domain(main_group, GETTEXT_PACKAGE);
    g_option_context_set_main_group(context, main_group);

    GError *error = NULL;
#ifdef G_OS_WIN32
    if (!g_option_context_parse_strv(context, &argv, &error))
#else  /*!G_OS_WIN32*/
    if (!g_option_context_parse(context, &argc, &argv, &error))
#endif /*G_OS_WIN32*/
    {
        if (error) {
            fprintf(stderr, "%s\n", error->message);
            g_error_free(error);
        }

        vips_error_exit("try \"%s --help\"", g_get_prgname());
    }

    g_option_context_free(context);

    if (main_option_version) {
        printf("%s-%s", PACKAGE, VERSION);
        printf("\n");

        printf(_("linked to vips-%s"), vips_version_string());
        printf("\n");

        exit(0);
    }

    /* Override the install guess from vips. This won't pick up msg
     * cats sadly :( since we have to init i18n before arg parsing. Handy
     * for testing without installing.
     */
    if (main_option_prefix)
        set_prefix(main_option_prefix);

    if (main_option_test)
        main_option_verbose = TRUE;

	/* We'll need to load all menus for workspace mode, or to gen i18n.
	 */
    if (main_option_i18n ||
		main_option_workspace)
        main_option_no_load_menus = FALSE;

	/* On Windows, argv is ascii-only ... use this to get a utf-8 version of
	 * the args.
	 */
#ifdef G_OS_WIN32
	argv = g_win32_get_command_line();
#endif /*G_OS_WIN32*/

    main_startup(argc, argv);

	/* In the default script mode (used for #! scripts), we load argv[1] as a
	 * set of defs and pass the main func any remaining arguments.
	 *
	 * In --workspace mode we load all the args as workspaces / images / defs
	 * etc.
	 */

	Workspacegroup *wsg;
	if (!main_option_workspace &&
		argc > 1) {
		// load argv[1] as a set of defs
		if (!toolkit_new_from_file(main_toolkitgroup, argv[1]))
			main_log_add("%s\n", error_get_sub());

		// the rest of argc/argv become nip4 defs
		main_build_argv(argc - 1, argv + 1);

		// always print main in script mode
		main_option_print = TRUE;
	}
	else {
		// load args as workspaces ... the empty wsg is something for images
		// etc to load into
		wsg = workspacegroup_new_blank(main_workspaceroot, "untitled");
		Workspace *ws = workspacegroup_get_workspace(wsg);

		for (int i = 1; i < argc; i++)
			main_load_file(ws, argv[i]);
	}

	if (main_option_set)
        for (int i = 0; main_option_set[i]; i++) {
            if (main_option_verbose)
                printf("main_set: %s\n", main_option_set[i]);

            if (!main_set(main_option_set[i]))
                main_log_add("%s\n%s", error_get_top(), error_get_sub());
        }

    if (main_option_test) {
        /* Make sure we've had at least one recomp on everything.
         */
		symbol_recalculate_all_force(TRUE);
        if (expr_error_all)
            main_error_exit("--test: errors found");
    }

    if (main_option_expression) {
		main_expression(main_option_expression);
		main_option_print = TRUE;
	}

	/* Print or save all the mains we can find: one at the top level,
	 * one in each workspace.
	 */
	if (main_option_print) {
		Symbol *sym;
		gboolean found;

		symbol_recalculate_all_force(TRUE);

		found = FALSE;
		if ((sym = compile_lookup(symbol_root->expr->compile, "main"))) {
			main_print_main(sym);
			found = TRUE;
		}
		workspace_map((workspace_map_fn) main_print_ws, &found, NULL);

		if (!found)
			main_error_exit("%s", _("no \"main\" found"));
	}

    main_shutdown();

    return 0;
}
