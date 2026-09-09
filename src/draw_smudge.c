/* Smudge a region on an image.
 *
 * Copied from libvips to get a direct draw path,
 */

#include <stdio.h>
#include <stdlib.h>

#include <vips/vips.h>

#define SMUDGEI(TYPE) \
	for (int y = 0; y < clip.height; y++) { \
		TYPE *q = (TYPE *) VIPS_IMAGE_ADDR(image, clip.left, clip.top + y); \
		TYPE *p0 = (TYPE *) (pixels + y * lsize); \
		TYPE *p1 = (TYPE *) (pixels + (y + 1) * lsize); \
		TYPE *p2 = (TYPE *) (pixels + (y + 2) * lsize); \
\
		for (int x = 0; x < clip.width * ne; x++) { \
			q[x] = (p0[0] +     p0[ne] + p0[2 * ne] + \
					p1[0] + 8 * p1[ne] + p1[2 * ne] + \
					p2[0] +     p2[ne] + p2[2 * ne] + 8) / 16; \
\
			p0 += 1; \
			p1 += 1; \
			p2 += 1; \
		} \
	}

#define SMUDGEF(TYPE) \
	for (int y = 0; y < clip.height; y++) { \
		TYPE *q = (TYPE *) VIPS_IMAGE_ADDR(image, clip.left, clip.top + y); \
		TYPE *p0 = (TYPE *) (pixels + y * lsize); \
		TYPE *p1 = (TYPE *) (pixels + (y + 1) * lsize); \
		TYPE *p2 = (TYPE *) (pixels + (y + 2) * lsize); \
\
		const double f = 1.0 / 16.0; \
		const double F = 8.0 / 16.0; \
		for (int x = 0; x < clip.width * ne; x++) { \
			q[x] = f * p0[0] + f * p0[ne] + f * p0[2 * ne] + \
				   f * p1[0] + F * p1[ne] + f * p1[2 * ne] + \
				   f * p2[0] + f * p2[ne] + f * p2[2 * ne]; \
\
			p0 += 1; \
			p1 += 1; \
			p2 += 1; \
		} \
	}

int
draw_smudge(VipsImage *image, VipsRect *area)
{
	VipsRect clip = {0, 0, image->Xsize, image->Ysize};
	vips_rect_intersectrect(area, &clip, &clip);
	if (vips_rect_isempty(&clip))
		return 0;

	/* Take a copy of the pixels to blur.
	 */
	int psize = VIPS_IMAGE_SIZEOF_PEL(image);
	int lsize = psize * clip.width;
	g_autofree VipsPel *pixels =
		VIPS_ARRAY(NULL, lsize * clip.height, VipsPel);
	for (int y = 0; y < clip.height; y++)
		memcpy(pixels + lsize * y,
			   VIPS_IMAGE_ADDR(image, clip.left, y + clip.top),
			   lsize);

	/* Don't do the margins.
	 */
	vips_rect_marginadjust(&clip, -1);
	if (vips_rect_isempty(&clip))
		return 0;

	/* Number of numeric values per pixel. Notionally double for
	 * complex types.
	 */
	int ne = image->Bands *
		(vips_band_format_iscomplex(image->BandFmt) ? 2 : 1);

	switch (vips_image_get_format(image)) {
	case VIPS_FORMAT_UCHAR:
		SMUDGEI(unsigned char);
		break;
	case VIPS_FORMAT_CHAR:
		SMUDGEI(char);
		break;
	case VIPS_FORMAT_USHORT:
		SMUDGEI(unsigned short);
		break;
	case VIPS_FORMAT_SHORT:
		SMUDGEI(short);
		break;
	case VIPS_FORMAT_UINT:
		SMUDGEI(unsigned int);
		break;
	case VIPS_FORMAT_INT:
		SMUDGEI(int);
		break;
	case VIPS_FORMAT_FLOAT:
		SMUDGEF(float);
		break;
	case VIPS_FORMAT_DOUBLE:
		SMUDGEF(double);
		break;
	case VIPS_FORMAT_COMPLEX:
		SMUDGEF(float);
		break;
	case VIPS_FORMAT_DPCOMPLEX:
		SMUDGEF(double);
		break;

	default:
		g_assert_not_reached();
	}

	return 0;
}
