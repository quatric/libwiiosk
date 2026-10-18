/*-------------------------------------------------------------
soft.c -- software implementation of the wiiosk drawing backend

Follows the GX path in source/osk_gx.c: bilinear filtering with
clamped texture coordinates, texel centers at (i + 0.5) / size,
pixels covered when their center is inside the quad, and
straight alpha blending.
-------------------------------------------------------------*/
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "osk_internal.h"
#include "soft.h"

static osk_image images[OSK_TEX_IMAGES + 1];
static bool ready;

static float *canvas;		/* RGB, 0..1 */
static int cw, ch;
static float cscale;

bool osk_backend_init(void)
{
	osk_image *font = &images[OSK_TEX_FONT];
	unsigned x, y;

	if (ready)
		return true;
	if (!osk_gfx_generate(images))
		return false;
	font->w = osk_font_atlas_w;
	font->h = osk_font_atlas_h;
	font->rgba = malloc(font->w * font->h * 4);
	for (y = 0; y < font->h; y++) {
		for (x = 0; x < font->w; x++) {
			u8 b = osk_font_atlas[(y * font->w + x) / 2];
			u8 v = ((x & 1) ? b & 15 : b >> 4) * 17;
			u8 *p = &font->rgba[(y * font->w + x) * 4];
			p[0] = p[1] = p[2] = p[3] = v;
		}
	}
	ready = true;
	return true;
}

void soft_target(float *rgb, int w, int h, float scale)
{
	canvas = rgb;
	cw = w;
	ch = h;
	cscale = scale;
}

void osk_backend_begin(const osk_keyboard *osk)
{
	(void)osk;
}

void osk_backend_end(void)
{
}

static void sample(const osk_image *im, float s, float t, float out[4])
{
	float fx = s * im->w - 0.5f, fy = t * im->h - 0.5f;
	int x0, y0, x1, y1, c;
	float ax, ay;

	fx = fx < 0 ? 0 : fx > im->w - 1 ? im->w - 1 : fx;
	fy = fy < 0 ? 0 : fy > im->h - 1 ? im->h - 1 : fy;
	x0 = (int)fx;
	y0 = (int)fy;
	x1 = x0 + 1 < im->w ? x0 + 1 : x0;
	y1 = y0 + 1 < im->h ? y0 + 1 : y0;
	ax = fx - x0;
	ay = fy - y0;
	for (c = 0; c < 4; c++) {
		float a = im->rgba[(y0 * im->w + x0) * 4 + c], b = im->rgba[(y0 * im->w + x1) * 4 + c];
		float d = im->rgba[(y1 * im->w + x0) * 4 + c], e = im->rgba[(y1 * im->w + x1) * 4 + c];
		out[c] = ((a + (b - a) * ax) * (1 - ay) + (d + (e - d) * ax) * ay) / 255.0f;
	}
}

void osk_backend_quad(osk_tex tex, float x0, float y0, float x1, float y1,
		      float s0, float t0, float s1, float t1,
		      osk_color black, osk_color white, const osk_color vtx[4])
{
	const osk_image *im = &images[tex];
	const float blk[4] = { black.r / 255.0f, black.g / 255.0f, black.b / 255.0f, black.a / 255.0f };
	const float wht[4] = { white.r / 255.0f, white.g / 255.0f, white.b / 255.0f, white.a / 255.0f };
	int px0 = (int)ceilf(x0 * cscale - 0.5f), px1 = (int)ceilf(x1 * cscale - 0.5f);
	int py0 = (int)ceilf(y0 * cscale - 0.5f), py1 = (int)ceilf(y1 * cscale - 0.5f);
	int x, y, c;

	if (px0 < 0) px0 = 0;
	if (py0 < 0) py0 = 0;
	if (px1 > cw) px1 = cw;
	if (py1 > ch) py1 = ch;

	for (y = py0; y < py1; y++) {
		float v = ((y + 0.5f) / cscale - y0) / (y1 - y0);
		for (x = px0; x < px1; x++) {
			float u = ((x + 0.5f) / cscale - x0) / (x1 - x0);
			float tx[4], col[4], *dst = &canvas[(y * cw + x) * 3];

			sample(im, s0 + (s1 - s0) * u, t0 + (t1 - t0) * v, tx);
			for (c = 0; c < 4; c++) {
				const u8 *q0 = &vtx[0].r, *q1 = &vtx[1].r, *q2 = &vtx[2].r, *q3 = &vtx[3].r;
				float top = q0[c] + (q1[c] - q0[c]) * u, bot = q3[c] + (q2[c] - q3[c]) * u;
				float vc = (top + (bot - top) * v) / 255.0f;
				col[c] = (blk[c] + (wht[c] - blk[c]) * tx[c]) * vc;
			}
			for (c = 0; c < 3; c++)
				dst[c] = col[c] * col[3] + dst[c] * (1 - col[3]);
		}
	}
}
