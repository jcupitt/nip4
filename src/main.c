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
#define DEBUG_FATAL
 */
#define DEBUG

#include "nip4.h"

static char prefix_buffer[FILENAME_MAX];
static gboolean prefix_valid = FALSE;

/* General stuff.
 */
Workspaceroot *main_workspaceroot = NULL;	/* All the workspaces */
Toolkitgroup *main_toolkitgroup = NULL;		/* All the toolkits */
Symbol *main_symbol_root = NULL;			/* Root of symtable */
Watchgroup *main_watchgroup = NULL;			/* All of the watches */
Imageinfogroup *main_imageinfogroup = NULL; /* All of the images */

void *main_c_stack_base = NULL; /* Base of C stack */

gboolean main_starting = TRUE; /* In startup */

static const char *main_argv0 = NULL; /* argv[0] */
static iOpenFile *main_stdin = NULL;  /* stdin as an iOpenFile */

static char *main_option_script = NULL;
static char *main_option_expression = NULL;
gboolean main_option_batch = FALSE;
static gboolean main_option_no_load_menus = FALSE;
static gboolean main_option_no_load_args = FALSE;
static gboolean main_option_stdin_ws = FALSE;
static gboolean main_option_stdin_def = FALSE;
static char *main_option_output = NULL;
static char **main_option_set = NULL;
static gboolean main_option_benchmark = FALSE;
gboolean main_option_time_save = FALSE;
gboolean main_option_profile = FALSE;
gboolean main_option_i18n = FALSE;
gboolean main_option_verbose = FALSE;
static gboolean main_option_print_main = FALSE;
static gboolean main_option_version = FALSE;
static gboolean main_option_test = FALSE;
static char *main_option_prefix = NULL;

static GOptionEntry main_option[] = {
	{ "expression", 'e', 0, G_OPTION_ARG_STRING, &main_option_expression,
		N_("evaluate and print EXPRESSION"),
		"EXPRESSION" },
	{ "script", 's', 0, G_OPTION_ARG_FILENAME, &main_option_script,
		N_("load FILE as a set of definitions"),
		"FILE" },
	{ "output", 'o', 0, G_OPTION_ARG_FILENAME, &main_option_output,
		N_("write value of 'main' to FILE"), "FILE" },
	{ "batch", 'b', 0, G_OPTION_ARG_NONE, &main_option_batch,
		N_("run in batch mode"), NULL },
	{ "set", '=', 0, G_OPTION_ARG_STRING_ARRAY, &main_option_set,
		N_("set values"), NULL },
	{ "verbose", 'V', 0, G_OPTION_ARG_NONE, &main_option_verbose,
		N_("verbose error output"), NULL },
	{ "no-load-menus", 'm', 0, G_OPTION_ARG_NONE,
		&main_option_no_load_menus,
		N_("don't load menu definitions"), NULL },
	{ "no-load-args", 'a', 0, G_OPTION_ARG_NONE, &main_option_no_load_args,
		N_("don't try to load command-line arguments"), NULL },
	{ "stdin-ws", 'w', 0, G_OPTION_ARG_NONE, &main_option_stdin_ws,
		N_("load stdin as a workspace"), NULL },
	{ "stdin-def", 'd', 0, G_OPTION_ARG_NONE, &main_option_stdin_def,
		N_("load stdin as a set of definitions"), NULL },
	{ "print-main", 'p', 0, G_OPTION_ARG_NONE, &main_option_print_main,
		N_("print value of 'main' to stdout"),
		NULL },
	{ "benchmark", 'c', 0, G_OPTION_ARG_NONE, &main_option_benchmark,
		N_("start up and shut down"),
		NULL },
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

/* Override the install guess from vips. Handy for testing.
 */
static void
set_prefix(const char *prefix)
{
	vips_strncpy(prefix_buffer, prefix, FILENAME_MAX);
	nativeize_path(prefix_buffer);
	absoluteize_path(prefix_buffer);
	setenvf("VIPSHOME", "%s", prefix_buffer);
	prefix_valid = TRUE;
}

/* Guess VIPSHOME, if we can.
 */
const char *
get_prefix(void)
{
	if (!prefix_valid) {
		const char *prefix;

		if (!(prefix = vips_guess_prefix(main_argv0, "VIPSHOME"))) {
			error_top(_("Unable to find install area"));
			error_vips();

			return NULL;
		}

		set_prefix(prefix);
	}

	return prefix_buffer;
}

/* Make sure a savedir exists. Used to build the "~/.nip2-xx/tmp" etc.
 * directory tree.
 */
static void
main_mkdir(const char *dir)
{
	if (!existsf("%s/%s", get_savedir(), dir))
		if (!mkdirf("%s/%s", get_savedir(), dir))
			vips_error_exit(_("unable to make %s %s: %s"),
				get_savedir(), dir, g_strerror(errno));
}

static void *
main_load_def(const char *filename)
{
	g_autofree char *dirname = g_path_get_dirname(filename);

	Toolkit *kit;

	if (!main_option_no_load_menus ||
		dirname[0] == '_') {
		progress_update_loading(0, dirname);

		if (!(kit = toolkit_new_from_file(main_toolkitgroup, filename)))
			error_alert(NULL);
		else
			filemodel_set_auto_load(FILEMODEL(kit));
	}

	return NULL;
}

static void *
main_load_wsg(const char *filename)
{
	g_autofree char *dirname = g_path_get_dirname(filename);

	Workspacegroup *wsg;

	progress_update_loading(0, dirname);

	if (!(wsg = workspacegroup_new_from_file(main_workspaceroot,
			  filename, filename)))
		error_alert(NULL);
	else
		filemodel_set_auto_load(FILEMODEL(wsg));

	return NULL;
}

int
main(int argc, char **argv)
{
	App *app;
	int status;

	main_argv0 = argv[0];
	if (VIPS_INIT(argv[0]))
		vips_error_exit("unable to start libvips");

#ifdef DEBUG
	vips_leak_set(TRUE);
#endif /*DEBUG*/

	/* Set localised application name.
	 */
	g_set_application_name(_(PACKAGE));

	/* On Windows, argv is ascii-only .. use this to get a utf-8 version of
	 * the args.
	 */
#ifdef G_OS_WIN32
	argv = g_win32_get_command_line();
#endif /*G_OS_WIN32*/

	GOptionContext *context =
		g_option_context_new(_("- image processing spreadsheet"));
	GOptionGroup *main_group = g_option_group_new(NULL, NULL, NULL, NULL, NULL);
	g_option_group_add_entries(main_group, main_option);
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

	/* Override the install guess from vips. This won't pick up msg
	 * cats sadly :( since we have to init i18n before arg parsing. Handy
	 * for testing without installing.
	 */
	if (main_option_prefix)
		set_prefix(main_option_prefix);

	if (main_option_version) {
		printf("%s-%s", PACKAGE, VERSION);
		printf("\n");

		printf(_("linked to vips-%s"), vips_version_string());
		printf("\n");

		exit(0);
	}

#ifdef DEBUG_FATAL
	/* Set masks for debugging ... stop on any problem.
	 */
	g_log_set_always_fatal(
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING);

	g_setenv("G_DEBUG", "fatal-warnings", TRUE);
	vips_leak_set(TRUE);
	printf("DEBUG on in main.c\n");
#else /*!DEBUG_FATAL*/
#ifdef OS_WIN32
	/* No logging output ... on win32, log output pops up a very annoying
	 * console text box.
	 */
	g_log_set_handler("GLib",
		G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		main_log_null, NULL);
	g_log_set_handler("Gtk",
		G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		main_log_null, NULL);
	g_log_set_handler(NULL,
		G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		main_log_null, NULL);
#endif /*OS_WIN32*/
#endif /*DEBUG_FATAL*/

#ifdef DEBUG
	printf("main: sizeof(HeapNode) == %zd\n", sizeof(HeapNode));

	/* Should be 3 pointers, hopefully.
	 */
	if (sizeof(HeapNode) != 3 * sizeof(void *))
		printf("*** struct packing problem!\n");
#endif /*DEBUG*/

	/* Want numeric locale to be "C", so we have C rules for doing
	 * double <-> string (ie. no "," for decimal point).
	 */
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	/* Make sure our LC_NUMERIC setting is not trashed.
	 */
	gtk_disable_setlocale();

	/* Pass config.h stuff down to .ws files.
	 */
	setenvf("PACKAGE", "%s", PACKAGE);
	setenvf("VERSION", "%s", VERSION);

	/* Name of the dir we store our config stuff in. This can get used by
	 * Preferences.ws.
	 */
	setenvf("SAVEDIR", "%s", get_savedir());

	/* Path separator on this platform.
	 */
	setenvf("SEP", "%s", G_DIR_SEPARATOR_S);

	/* Executable file extension (eg. ".exe" on Windows). Used by some defs.
	 */
	setenvf("EXEEXT", "%s", EXEEXT);

#ifdef OS_WIN32
	{
		/* No HOME on windows ... make one from HOMEDRIVE and HOMEDIR (via
		 * glib).
		 */
		const char *home;
		char buf[FILENAME_MAX];

		if (!(home = g_getenv("HOME")))
			home = g_get_home_dir();

		/* We need native paths.
		 */
		strncpy(buf, home, FILENAME_MAX);
		nativeize_path(buf);
		setenvf("HOME", "%s", buf);
	}
#endif /*OS_WIN32*/

#ifdef HAVE_GETRLIMIT
	/* Make sure we have lots of file descriptors. Some platforms have cur
	 * as 256 and max at 1024 to keep stdio happy.
	 */
	struct rlimit rlp;
	if (getrlimit(RLIMIT_NOFILE, &rlp) == 0) {
		rlim_t old_limit = rlp.rlim_cur;

		rlp.rlim_cur = rlp.rlim_max;
		if (setrlimit(RLIMIT_NOFILE, &rlp) == 0) {
#ifdef DEBUG
			printf("set max file descriptors to %d\n",
				(int) rlp.rlim_max);
#endif /*DEBUG*/
		}
		else if ((int) rlp.rlim_max != -1)
			/* -1 means can't-be-set, at least on os x, so don't
			 * warn.
			 */
			g_warning(_("unable to change max file descriptors\n"
						"max file descriptors still set to %d"),
				(int) old_limit);
	}
	else
		g_warning(_("unable to read max file descriptors"));
#endif /*HAVE_GETRLIMIT*/

	main_stdin = ifile_open_read_stdin();

	path_init();

	/* First time we've been run? Welcome message.
	 */
	gboolean welcome = FALSE;
	if (!existsf("%s", get_savedir()))
		welcome = TRUE;

	/* Always make these in case some get deleted.
	 */
	main_mkdir("");
	main_mkdir("tmp");
	main_mkdir("start");
	main_mkdir("data");

	reduce_context = reduce_new();

	main_symbol_root = symbol_root_init();
	iobject_ref_sink(IOBJECT(main_symbol_root));

	main_workspaceroot = workspaceroot_new("Workspaces");
	iobject_ref_sink(IOBJECT(main_workspaceroot));

	main_toolkitgroup = toolkitgroup_new(symbol_root);
	iobject_ref_sink(IOBJECT(main_toolkitgroup));

	main_imageinfogroup = imageinfogroup_new();
	iobject_ref_sink(IOBJECT(main_imageinfogroup));

	/* Add builtin toolkit.
	 */
	builtin_init();

	/* Load up all defs and wses.
	 */
#ifdef DEBUG
	printf("definitions init\n");
#endif /*DEBUG*/
	(void) path_map(PATH_START, "*.def", (path_map_fn) main_load_def, NULL);

#ifdef DEBUG
	printf("ws init\n");
#endif /*DEBUG*/
	(void) path_map(PATH_START, "*.ws", (path_map_fn) main_load_wsg, NULL);

	/* Recalc to build all classes and gets prefs working.
	 *
	 * We have to do this in batch mode since we can find dirties through
	 * dynamic lookups. Even though you might think we could just follow
	 * recomps.
	 */
	symbol_recalculate_all_force(TRUE);

	app = app_new(welcome);

	status = g_application_run(G_APPLICATION(app), argc, argv);

	/* Remove any ws retain files.
	 */
	workspacegroup_autosave_clean();

	/* Junk all symbols. This may remove a bunch of intermediate images
	 * too.
	 */
	// UNREF(main_watchgroup);
	VIPS_UNREF(main_symbol_root);
	VIPS_UNREF(main_toolkitgroup);
	VIPS_UNREF(main_workspaceroot);

	/* Junk reduction machine ... this should remove all image temps.
	 */
	reduce_destroy(reduce_context);

#ifdef HAVE_LIBGOFFICE
	/* Not quite sure what this does, but don't do it in batch mode.
	 */
	if (!main_option_batch)
		libgoffice_shutdown();
#endif /*HAVE_LIBGOFFICE*/

	path_rewrite_free_all();

	/* Should have freed everything now.
	 */

	/* Make sure!
	 */
	VIPS_UNREF(main_imageinfogroup);
	heap_check_all_destroyed();
	vips_shutdown();
	managed_check_all_destroyed();
	util_check_all_destroyed();
	view_dump();

	return status;
}
