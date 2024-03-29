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

#ifndef __MAIN_WINDOW_H
#define __MAIN_WINDOW_H

#define MAIN_WINDOW_TYPE (main_window_get_type())

#define NIP4_MAIN_WINDOW MAIN_WINDOW

G_DECLARE_FINAL_TYPE(MainWindow, main_window,
	NIP4, MAIN_WINDOW, GtkApplicationWindow)

extern gboolean main_window_auto_recalc;

MainWindow *main_window_new(App *app);
void main_window_set_gfile(MainWindow *win, GFile *gfile);
void main_window_cull(void);
void main_window_layout(void);

#endif /* __MAIN_WINDOW_H */
