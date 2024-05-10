// gtk utility funcs

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

#define WHITESPACE " \t\r\b\n"

#define UNPARENT(W) \
	G_STMT_START \
	{ \
		if (W) { \
			gtk_widget_unparent(GTK_WIDGET(W)); \
			(W) = NULL; \
		} \
	} \
	G_STMT_END

#define BIND_RESOURCE(resource) \
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), \
		APP_PATH "/" resource);

#define BIND_VARIABLE(class_name, name) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		class_name, name);

#define BIND_CALLBACK(name) \
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), \
		name);

/* Like G_CHECK_TYPE, but insist on an exact match.
 */
#define TYPE_EXACT(OBJECT, TYPE) (G_TYPE_FROM_INSTANCE(OBJECT) == (TYPE))

void set_symbol_drag_type(GtkWidget *widget);

void set_glabel(GtkWidget *label, const char *fmt, ...);
void set_glabel1(GtkWidget *label, const char *fmt, ...);
void set_gentryv(GtkWidget *edit, const char *fmt, va_list ap);
void set_gentry(GtkWidget *edit, const char *fmt, ...);
gboolean get_geditable_double(GtkWidget *text, double *out);
GtkWidget *build_entry(int nchars);
void set_tooltip(GtkWidget *wid, const char *fmt, ...);
void copy_adj(GtkAdjustment *to, GtkAdjustment *from);

void change_state(GtkWidget *widget, const char *name, GVariant *state);
void set_state(GtkWidget *to, GSettings *settings, const char *name);
GVariant *get_state(GtkWidget *widget, const char *name);
void copy_state(GtkWidget *to, GtkWidget *from, const char *name);

void process_events(void);

void block_scroll(GtkWidget *widget);

gboolean widget_should_animate(GtkWidget *widget);

void action_toggle(GSimpleAction *action,
	GVariant *parameter, gpointer user_data);
void action_radio(GSimpleAction *action,
	GVariant *parameter, gpointer user_data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(cairo_t, cairo_destroy)
