/*-------------------------------------------------------------
osk_sdl.c -- SDL2 drawing backend (Wii U)

Every quad is colored like a layout picture:
	color = lerp(black, white, texel) * vertex color
GX does the lerp in a TEV stage. SDL can only multiply a texture by
the vertex color, so the lerp is baked into a copy of the texture for
each (texture, black, white) the keyboard uses. The keyboard uses
about 55 such pairs in all, so they are made once and kept.
-------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>

#include "osk_internal.h"
#include "wiiosk_sdl.h"

#define CACHE_SIZE 128

typedef struct {
	SDL_Texture *tex;
	osk_tex id;
	osk_color black, white;
	unsigned used;
} cached;

static osk_image images[OSK_TEX_IMAGES + 1];
static bool ready;
static SDL_Renderer *rend;
static cached cache[CACHE_SIZE];
static unsigned frame;

static float sx = 1, sy = 1, offx, offy;

void OSK_SDL_SetRenderer(SDL_Renderer *renderer)
{
	if (renderer != rend)
		OSK_SDL_Shutdown();
	rend = renderer;
}

void OSK_SDL_Shutdown(void)
{
	int i;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (cache[i].tex)
			SDL_DestroyTexture(cache[i].tex);
		memset(&cache[i], 0, sizeof(cache[i]));
	}
}

bool osk_backend_init(void)
{
	osk_image *font = &images[OSK_TEX_FONT];
	unsigned x, y;
	u8 *atlas;

	if (ready)
		return true;
	if (!osk_gfx_generate(images))
		return false;
	atlas = osk_font_unpack();
	if (!atlas)
		return false;
	font->w = osk_font_atlas_w;
	font->h = osk_font_atlas_h;
	font->rgba = malloc(font->w * font->h * 4);
	if (!font->rgba) {
		free(atlas);
		return false;
	}
	for (y = 0; y < font->h; y++) {
		for (x = 0; x < font->w; x++) {
			u8 b = atlas[(y * font->w + x) / 2];
			u8 v = ((x & 1) ? b & 15 : b >> 4) * 17;
			u8 *p = &font->rgba[(y * font->w + x) * 4];

			p[0] = p[1] = p[2] = p[3] = v;
		}
	}
	free(atlas);
	ready = true;
	return true;
}

static bool same(osk_color a, osk_color b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static SDL_Texture *bake(osk_tex id, osk_color black, osk_color white)
{
	const osk_image *im = &images[id];
	const u8 lo[4] = { black.r, black.g, black.b, black.a };
	const u8 hi[4] = { white.r, white.g, white.b, white.a };
	SDL_Texture *t;
	u8 *buf, *dst;
	const u8 *src;
	unsigned i, n = im->w * im->h;
	int c;

	if (!im->rgba || !im->w || !im->h)
		return NULL;
	buf = malloc(n * 4);
	if (!buf)
		return NULL;
	src = im->rgba;
	dst = buf;
	for (i = 0; i < n; i++, src += 4, dst += 4)
		for (c = 0; c < 4; c++)
			dst[c] = (u8)(lo[c] + ((hi[c] - lo[c]) * src[c] + 127) / 255);

	t = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, im->w, im->h);
	if (t) {
		SDL_UpdateTexture(t, NULL, buf, im->w * 4);
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
	}
	free(buf);
	return t;
}

static SDL_Texture *texture(osk_tex id, osk_color black, osk_color white)
{
	cached *slot = NULL;
	int i;

	for (i = 0; i < CACHE_SIZE; i++) {
		cached *e = &cache[i];

		if (e->tex && e->id == id && same(e->black, black) && same(e->white, white)) {
			e->used = frame;
			return e->tex;
		}
	}
	/* a free slot, else the least recently used one */
	for (i = 0; i < CACHE_SIZE; i++) {
		cached *e = &cache[i];

		if (!e->tex) {
			slot = e;
			break;
		}
		if (!slot || e->used < slot->used)
			slot = e;
	}
	if (slot->tex)
		SDL_DestroyTexture(slot->tex);
	slot->tex = bake(id, black, white);
	slot->id = id;
	slot->black = black;
	slot->white = white;
	slot->used = frame;
	return slot->tex;
}

void osk_backend_begin(const osk_keyboard *osk)
{
	int lw = 0, lh = 0;

	frame++;
	if (!rend)
		return;
	SDL_RenderGetLogicalSize(rend, &lw, &lh);
	if (lw <= 0 || lh <= 0)
		SDL_GetRendererOutputSize(rend, &lw, &lh);
	sx = (float)lw / (osk->cfg.screen_width ? osk->cfg.screen_width : 640);
	sy = (float)lh / (osk->cfg.screen_height ? osk->cfg.screen_height : 480);
	offx = offy = 0;
}

void osk_backend_end(void)
{
}

void osk_backend_quad(osk_tex tex, float x0, float y0, float x1, float y1,
		      float s0, float t0, float s1, float t1,
		      osk_color black, osk_color white, const osk_color vtx[4])
{
	static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
	SDL_Vertex v[4];
	SDL_Texture *t;
	int i;

	if (!rend)
		return;
	t = texture(tex, black, white);
	if (!t)
		return;

	v[0].position.x = v[3].position.x = offx + x0 * sx;
	v[1].position.x = v[2].position.x = offx + x1 * sx;
	v[0].position.y = v[1].position.y = offy + y0 * sy;
	v[2].position.y = v[3].position.y = offy + y1 * sy;
	v[0].tex_coord.x = v[3].tex_coord.x = s0;
	v[1].tex_coord.x = v[2].tex_coord.x = s1;
	v[0].tex_coord.y = v[1].tex_coord.y = t0;
	v[2].tex_coord.y = v[3].tex_coord.y = t1;
	for (i = 0; i < 4; i++) {
		v[i].color.r = vtx[i].r;
		v[i].color.g = vtx[i].g;
		v[i].color.b = vtx[i].b;
		v[i].color.a = vtx[i].a;
	}
	SDL_RenderGeometry(rend, t, v, 4, idx, 6);
}
