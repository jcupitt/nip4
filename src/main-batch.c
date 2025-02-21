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

static iOpenFile *main_stdin = NULL;
static char *main_option_script = NULL;
static char *main_option_expression = NULL;
static gboolean main_option_stdin_ws = FALSE;
static gboolean main_option_stdin_def = FALSE;
static char *main_option_output = NULL;
static char **main_option_set = NULL;
static gboolean main_option_version = FALSE;
static gboolean main_option_verbose = FALSE;
static gboolean main_option_test = FALSE;
static char *main_option_prefix = NULL;

static GOptionEntry main_batch_options[] = {
    { "expression", 'e', 0, G_OPTION_ARG_STRING, &main_option_expression,
        N_("evaluate and print EXPRESSION"),
        "EXPRESSION" },
    { "script", 's', 0, G_OPTION_ARG_FILENAME, &main_option_script,
        N_("load FILE as a set of definitions"),
        "FILE" },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &main_option_output,
        N_("write value of 'main' to FILE"), "FILE" },
    { "set", '=', 0, G_OPTION_ARG_STRING_ARRAY, &main_option_set,
        N_("set values"), NULL },
    { "verbose", 'V', 0, G_OPTION_ARG_NONE, &main_option_verbose,
        N_("verbose error output"), NULL },
    { "no-load-menus", 'm', 0, G_OPTION_ARG_NONE,
        &main_option_no_load_menus,
        N_("don't load menu definitions"), NULL },
    { "stdin-ws", 'w', 0, G_OPTION_ARG_NONE, &main_option_stdin_ws,
        N_("load stdin as a workspace"), NULL },
    { "stdin-def", 'd', 0, G_OPTION_ARG_NONE, &main_option_stdin_def,
        N_("load stdin as a set of definitions"), NULL },
    { "time-save", 't', 0, G_OPTION_ARG_NONE, &main_option_time_save,
        N_("time image save operations"),
        NULL },
    { "profile", 'r', 0, G_OPTION_ARG_NONE, &main_option_profile,
        N_("profile workspace calculation"),
        NULL },
    { "prefix", 'x', 0, G_OPTION_ARG_FILENAME, &main_option_prefix,
        N_("start as if installed to PREFIX"), "PREFIX" },
    { "i18n", 'i', 0, G_OPTION_ARG_NONE, &main_option_i18n,
        N_("output strings for internationalisation"),
        NULL },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &main_option_version,
        N_("print version number"),
        NULL },
    { "test", 'T', 0, G_OPTION_ARG_NONE, &main_option_test,
        N_("test for errors and quit"), NULL },
    { NULL }
};

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

        slist_map(expr_error_all,
            (SListMapFn) expr_error_print, &buf);
        fprintf(stderr, "%s", vips_buf_all(&buf));
    }

    exit(1);
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

    /* FIXME ... need Group for this
    if (main_option_output) {
        char filename[VIPS_PATH_MAX];

        g_strlcpy(filename, main_option_output, VIPS_PATH_MAX);
        if (!group_save_item(root, filename))
            main_error_exit(_( "error saving \"%s\""),
                symbol_name_scope(sym));
    }
     */

    graph_value(root);
}

static void
main_load_file(char *filename)
{
	if (vips_iscasepostfix(filename, ".def")) {
		Toolkit *kit;

        if (!(kit = toolkit_new_from_file(main_toolkitgroup, filename)))
			main_error_exit("unable to load toolkit \"%s\"", filename);

		Symbol *sym =
			compile_lookup(main_toolkitgroup->root->expr->compile, "main");
		if (sym)
			main_print_main(sym);
	}
	else if (vips_iscasepostfix(filename, ".ws")) {
		Workspacegroup *wsg;

		if (!(wsg = workspacegroup_new_from_file(main_workspaceroot,
			filename, filename)))
			main_error_exit("unable to load workspace \"%s\"", filename);

		Workspace *ws = workspacegroup_get_workspace(wsg);
		Symbol *sym = compile_lookup(ws->sym->expr->compile, "main");
		if (sym)
			main_print_main(sym);
	}
	else {
		printf("FIXME ... batch image load\n");
	}
}

int
main(int argc, char **argv)
{
    main_option_batch = TRUE;
	main_option_no_load_menus = TRUE;

    /* Set localised application name.
     */
    g_set_application_name(_(PACKAGE));

    GOptionContext *context =
        g_option_context_new(_("- image processing spreadsheet"));
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

    if (main_option_i18n)
        /* We'll need to load all menus.
         */
        main_option_no_load_menus = FALSE;

	/* On Windows, argv is ascii-only ... use this to get a utf-8 version of
	 * the args.
	 */
#ifdef G_OS_WIN32
	argv = g_win32_get_command_line();
#endif /*G_OS_WIN32*/

    main_startup(argc, argv);

	for (int i = 1; i < argc; i++)
		main_load_file(argv[i]);

    if (main_option_test) {
        /* Make sure we've had at least one recomp on everything.
         */
        symbol_recalculate_all_force(TRUE);
        if (expr_error_all)
            main_error_exit("--test: errors found");
    }

    main_shutdown();

    return 0;
}
