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
 */

#include "nip4.h"

int
main(int argc, char **argv)
{
	App *app;
	int status;

	if (VIPS_INIT(argv[0]))
		vips_error_exit("unable to start libvips");

#ifdef DEBUG
	printf("DEBUG on in main.c\n");
	vips_leak_set(TRUE);

	g_log_set_always_fatal(
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING |
		0);

	g_setenv("G_DEBUG", "fatal-warnings", FALSE);
#endif /*DEBUG*/

	/* Magickload will lock up on eg. AVI files.
	 */
	printf("nip4.main: blocking VipsForeignLoadMagick\n");
	vips_operation_block_set("VipsForeignLoadMagick", TRUE);

	app = app_new();

	status = g_application_run(G_APPLICATION(app), argc, argv);

	vips_shutdown();

	return status;
}
