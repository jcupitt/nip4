/* an image class object in a workspace
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
#define DEBUG
 */

#include "nip4.h"

/* Set of allowed colour_space strings. Do a case-insensitive match.
 */
static const char *colour_colour_space[] = {
	"xyz",	  /* index 0 */
	"yxy",	  /* index 1 */
	"lab",	  /* index 2 */
	"lch",	  /* index 3 */
	"ucs",	  /* index 4 */
	"rgb",	  /* index 5 */
	"srgb",	  /* index 6 */
	"rgb16",  /* index 7 */
	"grey16", /* index 8 */
	"scrgb",  /* index 9 */
};

/* For each allowed colourspace, the corresponding VIPS interpretation.
 */
static const int colour_type[] = {
	VIPS_INTERPRETATION_XYZ,
	VIPS_INTERPRETATION_YXY,
	VIPS_INTERPRETATION_LAB,
	VIPS_INTERPRETATION_LCH,
	VIPS_INTERPRETATION_CMC,
	VIPS_INTERPRETATION_RGB,
	VIPS_INTERPRETATION_sRGB,
	VIPS_INTERPRETATION_RGB16,
	VIPS_INTERPRETATION_GREY16,
	VIPS_INTERPRETATION_scRGB,
};

G_DEFINE_TYPE(Colour, colour, CLASSMODEL_TYPE)

static void
colour_finalize(GObject *gobject)
{
	Colour *colour = COLOUR(gobject);

#ifdef DEBUG
	printf("colour_finalize:\n");
#endif /*DEBUG*/

	VIPS_FREE(colour->colour_space);
	vips_buf_destroy(&colour->caption);

	G_OBJECT_CLASS(colour_parent_class)->finalize(gobject);
}

/* Find the VIPS type for a colour space string.
 */
static int
colour_get_vips_interpretation(Colour *colour)
{
	/* Default to something harmless.
	 */
	int type = VIPS_INTERPRETATION_MULTIBAND;

	if (colour->colour_space)
		for (int i = 0; i < VIPS_NUMBER(colour_colour_space); i++)
			if (g_ascii_strcasecmp(colour->colour_space,
					colour_colour_space[i]) == 0) {
				type = colour_type[i];
				break;
			}

	return type;
}

/* Are two doubles more or less equal. We need this when we check
 * for update to stop loops. The 0.0001 is a bit of a fudge :-(
 */
#define DEQ(A, B) (ABS((A) - (B)) < 0.0001)

/* Update non-model stuff in object from the model params.
 */
static void
colour_refresh(Colour *colour)
{
#ifdef DEBUG
	printf("colour_refresh:\n");
#endif /*DEBUG*/

	vips_buf_rewind(&colour->caption);
	vips_buf_appendf(&colour->caption, CLASS_COLOUR " %s [%g, %g, %g]",
		colour->colour_space,
		colour->value[0], colour->value[1], colour->value[2]);
}

void
colour_set_colour(Colour *colour,
	const char *colour_space, double value[3])
{
#ifdef DEBUG
	printf("colour_set_colour:\n");
	printf("\tspace = %s\n", colour_space);
	printf("\tv0 = %g, v1 = %g, v2 = %g\n", value[0], value[1], value[2]);
#endif /*DEBUG*/

	/* No change?
	 */
	int i;
	for (i = 0; i < 3; i++)
		if (!DEQ(value[i], colour->value[i]))
			break;
	if (i < 3 ||
		!colour_space ||
		g_ascii_strcasecmp(colour_space, colour->colour_space) != 0) {
		for (i = 0; i < 3; i++)
			colour->value[i] = value[i];
		VIPS_SETSTR(colour->colour_space, colour_space);

		colour_refresh(colour);
		classmodel_update(CLASSMODEL(colour));
		symbol_recalculate_all();
	}
}

/* Code up a colour as an ii. Refcount zero! Will go on next GC.
 */
Imageinfo *
colour_ii_new(Colour *colour)
{
	Imageinfo *ii;

#ifdef DEBUG
	printf("colour_ii_new:\n");
	printf("\tspace = %s\n", colour->colour_space);
	printf("\tv0 = %g, v1 = %g, v2 = %g\n",
		colour->value[0], colour->value[1], colour->value[2]);
#endif /*DEBUG*/

	/* Make a 3 band 32-bit FLOAT memory image. libvips colour funcs like
	 * float (not double).
	 */
	if (!(ii = imageinfo_new_memory(main_imageinfogroup,
			  reduce_context->heap, NULL)))
		return NULL;
	vips_image_init_fields(ii->image, 1, 1, 3,
		VIPS_FORMAT_FLOAT, VIPS_CODING_NONE,
		colour_get_vips_interpretation(colour), 1.0, 1.0);

	float valuef[3];
	for (int i = 0; i < 3; i++)
		// make sure we don't write -nan etc.
		valuef[i] = isnormal(colour->value[i]) ? colour->value[i] : 0.0;
	if (vips_image_write_line(ii->image, 0, (VipsPel *) valuef))
		return NULL;

#ifdef DEBUG
	char txt[256];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	vips_buf_appendi(&buf, ii->image);
	printf("\tmade %s\n", vips_buf_all(&buf));
	vips_buf_rewind(&buf);
	imageinfo_to_text(ii, &buf);
	printf("\tpixels %s\n", vips_buf_all(&buf));
#endif /*DEBUG*/

	return ii;
}

/* Convert our colour to rgb. Slow!
 */
static void
colour_get_rgb(Colour *colour, double rgb[3])
{
	Imageinfo *imageinfo;

#ifdef DEBUG
	printf("colour_get_rgb:\n");
#endif /*DEBUG*/

	for (int i = 0; i < 3; i++)
		rgb[i] = 0.0;
	if ((imageinfo = colour_ii_new(colour)))
		imageinfo_get_rgb(imageinfo, rgb);
}

void
colour_set_rgb(Colour *colour, double rgb[3])
{
	Imageinfo *imageinfo;

#ifdef DEBUG
	printf("colour_set_rgb:\n");
	printf("\tr = %g, g = %g, b = %g\n", rgb[0], rgb[1], rgb[2]);
#endif /*DEBUG*/

	double current_rgb[3];

	/* Setting as RGB can't express small differences since we're
	 * going via 8 bit RGB. So only accept the new value if it's
	 * sufficiently different from what we have now.
	 */
	colour_get_rgb(colour, current_rgb);
	if (fabs(rgb[0] - current_rgb[0]) > 0.5 ||
		fabs(rgb[1] - current_rgb[1]) > 0.5 ||
		fabs(rgb[2] - current_rgb[2]) > 0.5) {
		if ((imageinfo = colour_ii_new(colour))) {
			imageinfo_set_rgb(imageinfo, rgb);
			double new_rgb[3];
			for (int i = 0; i < 3; i++)
				new_rgb[i] = ((float *) imageinfo->image->data)[i];
			colour_set_colour(colour, colour->colour_space, new_rgb);
		}
	}
}

static void
colour_edit_choose_rgba(GObject *source_object,
	GAsyncResult *res, gpointer data)
{
	GtkColorDialog *dialog = GTK_COLOR_DIALOG(source_object);
	Colour *colour = COLOUR(data);

	g_autoptr(GdkRGBA) selected_colour =
		gtk_color_dialog_choose_rgba_finish(dialog, res, NULL);
	if (selected_colour) {
		double rgb[3];

		rgb[0] = selected_colour->red * 255.0;
		rgb[1] = selected_colour->green * 255.0;
		rgb[2] = selected_colour->blue * 255.0;
		colour_set_rgb(colour, rgb);
	}
}

static void
colour_edit(Model *model, GtkWindow *window)
{
	Colour *colour = COLOUR(model);

	GtkColorDialog *dialog = gtk_color_dialog_new();
	gtk_color_dialog_set_with_alpha(GTK_COLOR_DIALOG(dialog), FALSE);

	double rgb[3];
	colour_get_rgb(colour, rgb);
	GdkRGBA initial_color = {
		.red = rint(rgb[0] / 255.0),
		.green = rint(rgb[1] / 255.0),
		.blue = rint(rgb[2] / 255.0),
		.alpha = 1.0
	};

	gtk_color_dialog_choose_rgba(dialog, window,
		&initial_color, NULL, colour_edit_choose_rgba, colour);
}

static View *
colour_view_new(Model *model, View *parent)
{
	return colourview_new();
}

static void *
colour_update_model(Heapmodel *heapmodel)
{
	Colour *colour = COLOUR(heapmodel);

#ifdef DEBUG
	printf("colour_update_model:\n");
#endif /*DEBUG*/

	if (HEAPMODEL_CLASS(colour_parent_class)->update_model(heapmodel))
		return heapmodel;

	colour_refresh(colour);

	return NULL;
}

/* Members of colour we automate.
 */
static ClassmodelMember colour_members[] = {
	{ CLASSMODEL_MEMBER_STRING, NULL, 0,
		MEMBER_COLOUR_SPACE, "colour_space", N_("Colour Space"),
		G_STRUCT_OFFSET(Colour, colour_space) },
	{ CLASSMODEL_MEMBER_REALVEC_FIXED, NULL, 3,
		MEMBER_VALUE, "value", N_("Value"),
		G_STRUCT_OFFSET(Colour, value) }
};

static void
colour_class_init(ColourClass *class)
{
	GObjectClass *gobject_class = (GObjectClass *) class;
	ModelClass *model_class = (ModelClass *) class;
	HeapmodelClass *heapmodel_class = (HeapmodelClass *) class;
	ClassmodelClass *classmodel_class = (ClassmodelClass *) class;

	/* Create signals.
	 */

	/* Init methods.
	 */
	gobject_class->finalize = colour_finalize;

	model_class->view_new = colour_view_new;
	model_class->edit = colour_edit;

	heapmodel_class->update_model = colour_update_model;

	/* Static init.
	 */
	model_register_loadable(MODEL_CLASS(class));

	classmodel_class->members = colour_members;
	classmodel_class->n_members = VIPS_NUMBER(colour_members);
}

static void
colour_init(Colour *colour)
{
#ifdef DEBUG
	printf("colour_init:\n");
#endif /*DEBUG*/

	colour->value[0] = 0.0;
	colour->value[1] = 0.0;
	colour->value[2] = 0.0;
	colour->colour_space = NULL;
	vips_buf_init_dynamic(&colour->caption, MAX_LINELENGTH);

	iobject_set(IOBJECT(colour), CLASS_COLOUR, NULL);
}
