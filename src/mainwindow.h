// main application window

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

#ifndef __MAINWINDOW_H
#define __MAINWINDOW_H

#define MAINWINDOW_TYPE (mainwindow_get_type())

#define NIP4_MAINWINDOW MAINWINDOW

G_DECLARE_FINAL_TYPE(Mainwindow, mainwindow,
	NIP4, MAINWINDOW, GtkApplicationWindow)

extern gboolean mainwindow_auto_recalc;

void mainwindow_error(Mainwindow *main);
Mainwindow *mainwindow_new(App *app);
void mainwindow_set_wsg(Mainwindow *main, Workspacegroup *wsg);
void mainwindow_set_gfile(Mainwindow *main, GFile *gfile);
void mainwindow_cull(void);
void mainwindow_layout(void);
void mainwindow_set_action_view(View *action_view);

#endif /* __MAINWINDOW_H */
