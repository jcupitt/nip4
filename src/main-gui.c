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

static gboolean main_option_version = FALSE;

static GOptionEntry main_gui_options[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &main_option_version,
		N_("print version number"), NULL },
	{ NULL }
};

int
main(int argc, char **argv)
{
	if (VIPS_INIT(argv[0]))
		vips_error_exit("unable to start libvips");

	/* First time we've been run? Show a welcome message. We must do this
	 * before main_startup().
	 */
	gboolean welcome = !existsf("%s", get_savedir());

	/* Set localised application name.
	 */
	g_set_application_name(_(PACKAGE));

    GOptionContext *context =
        g_option_context_new(_("- image processing spreadsheet"));
    GOptionGroup *main_group = g_option_group_new(NULL, NULL, NULL, NULL, NULL);
    g_option_group_add_entries(main_group, main_gui_options);
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

	/* On Windows, argv is ascii-only ... use this to get a utf-8 version of
	 * the args.
	 */
#ifdef G_OS_WIN32
	argv = g_win32_get_command_line();
#endif /*G_OS_WIN32*/

	main_startup(argc, argv);

	App *app = app_new(welcome);
	int status = g_application_run(G_APPLICATION(app), argc, argv);

	main_shutdown();

	return status;
}
