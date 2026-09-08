/* Draw a mask on an image.
 *
 * Copied from libvips to get a direct draw path,
 */

#include <stdio.h>
#include <stdlib.h>

#include <vips/vips.h>

static int
draw_image_clipped(VipsImage *from, VipsImage *to,
	VipsRect *area, int x, int y)
{
	int ps = VIPS_IMAGE_SIZEOF_PEL(from);
	int line_size = ps * area->width;

	for (int i = 0; i < area->height; i++) {
		VipsPel *p = VIPS_IMAGE_ADDR(from, area->left, area->top + i);
		VipsPel *q = VIPS_IMAGE_ADDR(to, x, y + i);

		memcpy(q, p, line_size);
	}

	return 0;
}

/* Direct path for undo/redo image copying.
 */
int
draw_image(VipsImage *from, VipsImage *to, VipsRect *area, int x, int y)
{
	VipsRect image;

	if (vips_check_coding_noneorlabq("draw_image", from) ||
		vips_check_coding_noneorlabq("draw_image", to) ||
		vips_check_bands_same("draw_image", from, to) ||
		vips_check_format_same("draw_image", from, to) ||
		vips_check_coding_same("draw_image", from, to))
		return -1;

	/* Clip against the source image.
	 */
	image.left = 0;
	image.top = 0;
	image.width = from->Xsize;
	image.height = from->Ysize;
	VipsRect from_rect;
	vips_rect_intersectrect(area, &image, &from_rect);

	/* Move to the dest image and clip again.
	 */
	VipsRect to_rect;
	to_rect.left = x + (from_rect.left - area->left);
	to_rect.top = y + (from_rect.top - area->top);
	to_rect.width = from_rect.width;
	to_rect.height = from_rect.height;
	image.left = 0;
	image.top = 0;
	image.width = to->Xsize;
	image.height = to->Ysize;
	VipsRect clipped;
	vips_rect_intersectrect(&image, &to_rect, &clipped);

	/* Back to the source image again.
	 */
	VipsRect final;
	final.left = area->left + (clipped.left - to_rect.left);
	final.top = area->top + (clipped.top - to_rect.top);
    final.width = clipped.width;
    final.height = clipped.height;

	if (!vips_rect_isempty(&from_rect) &&
		draw_image_clipped(from, to, &final, clipped.left, clipped.top))
		return -1;

	return 0;
}
