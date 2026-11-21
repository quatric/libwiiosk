/*-------------------------------------------------------------
osk_gfx.c -- the textures, made at start-up

They are the system keyboard's own textures, unpacked from
osk_tex_data.c (tools/mktex.py); only the Wii Remote pointer is
drawn here. Grey images are later tinted by the material colors of
the quad that uses them (see osk_backend_quad()).
-------------------------------------------------------------*/
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "osk_internal.h"

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static float mixf(float a, float b, float t)
{
	return a + (b - a) * t;
}

/* even-odd point in polygon test; xy holds n x,y pairs */
static bool in_poly(const float *xy, int n, float x, float y)
{
	bool in = false;
	int i, j;

	for (i = 0, j = n - 1; i < n; j = i++) {
		float xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
		if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
			in = !in;
	}
	return in;
}

/* polygon coverage of pixel (x, y), 4x4 samples */
static float poly(const float *xy, int n, int x, int y)
{
	int i, hits = 0;

	for (i = 0; i < 16; i++)
		hits += in_poly(xy, n, x + ((i & 3) + 0.5f) / 4, y + ((i >> 2) + 0.5f) / 4);
	return hits / 16.0f;
}

static bool alloc_image(osk_image *im, int w, int h)
{
	im->w = w;
	im->h = h;
	im->rgba = calloc(w * h, 4);
	return im->rgba != NULL;
}

static void put(osk_image *im, int x, int y, float r, float g, float b, float a)
{
	u8 *p = &im->rgba[(y * im->w + x) * 4];

	p[0] = (u8)(clampf(r, 0, 255) + 0.5f);
	p[1] = (u8)(clampf(g, 0, 255) + 0.5f);
	p[2] = (u8)(clampf(b, 0, 255) + 0.5f);
	p[3] = (u8)(clampf(a, 0, 1) * 255 + 0.5f);
}

/*-------------------------------------------------------------
  Wii Remote pointer: white arrow with a dark outline and a soft
  shadow
-------------------------------------------------------------*/
static void gen_pointer(osk_image *im)
{
	static const float arrow[] = {
		2, 2, 2, 29, 9, 23, 14, 34, 19, 32, 14, 21, 23, 21,
	};
	static const float outline[] = {
		0, -1, 0, 33.5f, 8.5f, 26.5f, 13, 37, 22, 33, 17.5f, 23.5f, 27.5f, 23.5f,
	};
	int x, y;

	for (y = 0; y < im->h; y++) {
		for (x = 0; x < im->w; x++) {
			float shadow = poly(outline, 7, x - 2, y - 2) * 0.35f;
			float out = poly(outline, 7, x, y);
			float fill = poly(arrow, 7, x, y);
			float a = out + shadow * (1 - out);
			float v = a > 0 ? mixf(0x20, 255, fill) * out / a : 0;

			put(im, x, y, v, v, v, a);
		}
	}
}

/* rebuild an image from its 4-bit planes, see tools/mktex.py */
static void from_planes(osk_image *im, const osk_tex_source *src, const u8 *planes)
{
	unsigned x, y, i;

	for (y = 0; y < src->h; y++) {
		for (x = 0; x < src->w; x++) {
			u8 v[8], *p = &im->rgba[(y * im->w + x) * 4];
			unsigned n = src->fmt == OSK_TEXFMT_I4 ? 1 : src->fmt == OSK_TEXFMT_IA4 ? 2 :
				     src->fmt == OSK_TEXFMT_IA8 ? 4 : 8;

			for (i = 0; i < n; i++) {
				unsigned at = (src->row + i * src->h + y) * osk_tex_plane_w + x;
				u8 b = planes[at / 2];
				v[i] = (at & 1) ? b & 15 : b >> 4;
			}
			switch (src->fmt) {
			case OSK_TEXFMT_I4:
				p[0] = p[1] = p[2] = p[3] = v[0] * 17;
				break;
			case OSK_TEXFMT_IA4:
				p[0] = p[1] = p[2] = v[0] * 17;
				p[3] = v[1] * 17;
				break;
			case OSK_TEXFMT_IA8:
				p[0] = p[1] = p[2] = v[0] << 4 | v[1];
				p[3] = v[2] << 4 | v[3];
				break;
			default:
				for (i = 0; i < 4; i++)
					p[i] = v[2 * i] << 4 | v[2 * i + 1];
				break;
			}
		}
	}
}

bool osk_gfx_generate(osk_image images[OSK_TEX_IMAGES])
{
	u8 *planes = osk_unpack(osk_tex_packed, osk_tex_packed_size, NULL);
	int i;

	memset(images, 0, sizeof(osk_image) * OSK_TEX_IMAGES);
	if (!planes)
		return false;
	for (i = 0; i < OSK_TEX_IMAGES; i++) {
		const osk_tex_source *src = &osk_tex_sources[i];
		bool pointer = src->fmt == OSK_TEXFMT_NONE;	/* the one that is not a stored texture */

		if (!alloc_image(&images[i], pointer ? 32 : src->w, pointer ? 40 : src->h)) {
			free(planes);
			osk_gfx_free(images);
			return false;
		}
		if (pointer)
			gen_pointer(&images[i]);
		else
			from_planes(&images[i], src, planes);
	}
	free(planes);
	return true;
}

void osk_gfx_free(osk_image images[OSK_TEX_IMAGES])
{
	int i;

	for (i = 0; i < OSK_TEX_IMAGES; i++) {
		free(images[i].rgba);
		images[i].rgba = NULL;
	}
}
