/* a pair of spin buttons, with no entry ... don't actually use buttons,
 * since we may have lots and lots of these, and we don't want to make an X
 * window for each one
 *
 * we do the event handling ourselves ... our enclosing view passes the ev
 * to spin_event(), this triggers signals as required
 */

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

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

*/

/*
 */
#define DEBUG

#include "nip4.h"

G_DEFINE_TYPE(Spin, spin, GTK_TYPE_WIDGET)

/* Our signals. Up and down click.
 */
enum {
	UP_CLICK,
	DOWN_CLICK,
	LAST_SIGNAL
};

static guint spin_signals[LAST_SIGNAL] = { 0 };

static void
spin_dispose(GObject *gobject)
{
	Spin *spin = SPIN(gobject);

#ifdef DEBUG
	printf("spin_dispose:\n");
#endif /*DEBUG*/

	UNPARENT(spin->top);
	gtk_widget_dispose_template(GTK_WIDGET(spin), SPIN_TYPE);

	G_OBJECT_CLASS(spin_parent_class)->dispose(gobject);
}

/* Default up and down signal handlers.
 */
static void
spin_real_up_click(Spin *spin)
{
#ifdef DEBUG
	printf("spin_real_up_click\n");
#endif /*DEBUG*/
}

static void
spin_real_down_click(Spin *spin)
{
#ifdef DEBUG
	printf("spin_real_down_click\n");
#endif /*DEBUG*/
}

static void
spin_up(GtkGestureClick *gesture,
	guint n_press, double x, double y, Spin *spin)
{
	g_signal_emit(G_OBJECT(spin), spin_signals[UP_CLICK], 0);
}

static void
spin_down(GtkGestureClick *gesture,
	guint n_press, double x, double y, Spin *spin)
{
	g_signal_emit(G_OBJECT(spin), spin_signals[DOWN_CLICK], 0);
}

static void
spin_class_init(SpinClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

	BIND_RESOURCE("spin.ui");

	gtk_widget_class_set_layout_manager_type(widget_class,
		GTK_TYPE_BIN_LAYOUT);

	BIND_CALLBACK(spin_up);
	BIND_CALLBACK(spin_down);

	BIND_VARIABLE(Spin, top);

	/* Create signals.
	 */
	spin_signals[UP_CLICK] = g_signal_new("up_click",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(SpinClass, up_click),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	spin_signals[DOWN_CLICK] = g_signal_new("down_click",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(SpinClass, down_click),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	gobject_class->dispose = spin_dispose;

	class->up_click = spin_real_up_click;
	class->down_click = spin_real_down_click;
}

static void
spin_init(Spin *spin)
{
	gtk_widget_init_template(GTK_WIDGET(spin));
}

GtkWidget *
spin_new(void)
{
	Spin *spin = g_object_new(SPIN_TYPE, NULL);

	return GTK_WIDGET(spin);
}
