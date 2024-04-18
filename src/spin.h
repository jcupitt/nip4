/* a pair of spin buttons
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

#define SPIN_TYPE (spin_get_type())
#define SPIN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), SPIN_TYPE, Spin))
#define SPIN_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), SPIN_TYPE, SpinClass))
#define IS_SPIN(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), SPIN_TYPE))
#define IS_SPIN_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), SPIN_TYPE))
#define SPIN_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), SPIN_TYPE, SpinClass))

typedef struct _Spin {
	GtkWidget parent_object;

} Spin;

typedef struct _SpinClass {
	GtkWidgetClass parent_class;

	void (*up_click)(Spin *);
	void (*down_click)(Spin *);
} SpinClass;

GType spin_get_type(void);
GtkWidget *spin_new(void);
