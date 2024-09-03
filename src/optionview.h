/* a optionview in a workspace
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

#define OPTIONVIEW_TYPE (optionview_get_type())
#define OPTIONVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), OPTIONVIEW_TYPE, Optionview))
#define OPTIONVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), OPTIONVIEW_TYPE, OptionviewClass))
#define IS_OPTIONVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), OPTIONVIEW_TYPE))
#define IS_OPTIONVIEW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), OPTIONVIEW_TYPE))
#define OPTIONVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), OPTIONVIEW_TYPE, OptionviewClass))

typedef struct _Optionview {
	Graphicview parent_object;

	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *options;

	/* The [[char]] we set on the previous refresh. Use this to avoid
	 * rebuilding the optionmenu unless we have to.
	 */
	GSList *labels;
} Optionview;

typedef struct _OptionviewClass {
	GraphicviewClass parent_class;

	/* My methods.
	 */
} OptionviewClass;

GType optionview_get_type(void);
View *optionview_new(void);
