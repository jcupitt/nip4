// startup stuff

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

extern Workspaceroot *main_workspaceroot;
extern Toolkitgroup *main_toolkitgroup;
extern Symbol *main_symbol_root;
extern Watchgroup *main_watchgroup;
extern Imageinfogroup *main_imageinfogroup;

extern void *main_c_stack_base;
extern const char *main_argv0;

extern gboolean main_option_time_save;
extern gboolean main_option_profile;
extern gboolean main_option_i18n;
extern gboolean main_option_batch;
extern gboolean main_option_no_load_menus;

void set_prefix(const char *prefix);
const char *get_prefix(void);

void main_startup(int argc, char **argv);
void main_shutdown(void);
