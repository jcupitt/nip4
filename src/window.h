// base class for top level windows

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

#ifndef __WINDOW_H
#define __WINDOW_H

#define WINDOW_TYPE (window_get_type())

#define WINDOW NIP4_WINDOW
#define IS_WINDOW NIP4_IS_WINDOW

G_DECLARE_FINAL_TYPE(Window, window, NIP4, WINDOW, GtkApplicationWindow)

void window_set_action_view(View *action_view);

void window_set_save_folder(Window *win, GFile *save_folder);
GFile *window_get_save_folder(Window *win);
void window_set_load_folder(Window *win, GFile *save_folder);
GFile *window_get_load_folder(Window *win);

void window_error(Window *win);

guint window_get_modifiers(Window *win);
GSettings *window_get_settings(Window *win);

#endif /* __WINDOW_H */
