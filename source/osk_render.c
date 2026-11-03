/*-------------------------------------------------------------
osk_render.c -- the keyboard scene, drawn through the backend

Positions are design-space pixels (640x480, see osk_internal.h);
every element is a textured quad tinted by a black and a white
material color, like the pictures of a 2D layout.
-------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "osk_internal.h"

/* design -> virtual screen offset, the scale applied around a key
   center while it is drawn, and around that the symbol window's zoom
   and fade */
static float ox, oy;
static float xf_s = 1, xf_cx, xf_cy;
static float win_s = 1, win_cx, win_cy, win_a = 1;

static const osk_color c_white = { 255, 255, 255, 255 };
static const osk_color c_clear = { 0, 0, 0, 0 };
static const osk_color c_none[4] = {
	{ 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
};

static osk_color rgb(u8 r, u8 g, u8 b)
{
	osk_color c = { r, g, b, 255 };
	return c;
}

static osk_color grey(u8 v)
{
	return rgb(v, v, v);
}

static osk_color from_u32(u32 c)
{
	osk_color o = { c >> 24, c >> 16, c >> 8, c };
	return o;
}

static osk_color with_alpha(osk_color c, u8 a)
{
	c.a = a;
	return c;
}

static u8 clamp8(float v)
{
	return v < 0 ? 0 : v > 255 ? 255 : (u8)(v + 0.5f);
}

static osk_color mix(osk_color a, osk_color b, float t)
{
	osk_color o = {
		clamp8(a.r + (b.r - a.r) * t), clamp8(a.g + (b.g - a.g) * t),
		clamp8(a.b + (b.b - a.b) * t), clamp8(a.a + (b.a - a.a) * t),
	};
	return o;
}

/* layout animations use hermite keys with zero slopes */
static float smooth(float t)
{
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	return t * t * (3 - 2 * t);
}

static void quad(osk_tex tex, float x0, float y0, float x1, float y1,
		 float s0, float t0, float s1, float t1,
		 osk_color black, osk_color white, const osk_color vtx[4])
{
	osk_color v[4];
	int i;

	x0 = win_cx + (xf_cx + (x0 - xf_cx) * xf_s - win_cx) * win_s + ox;
	x1 = win_cx + (xf_cx + (x1 - xf_cx) * xf_s - win_cx) * win_s + ox;
	y0 = win_cy + (xf_cy + (y0 - xf_cy) * xf_s - win_cy) * win_s + oy;
	y1 = win_cy + (xf_cy + (y1 - xf_cy) * xf_s - win_cy) * win_s + oy;
	if (!vtx)
		vtx = c_none;
	if (win_a < 1) {
		for (i = 0; i < 4; i++) {
			v[i] = vtx[i];
			v[i].a = clamp8(v[i].a * win_a);
		}
		vtx = v;
	}
	osk_backend_quad(tex, x0, y0, x1, y1, s0, t0, s1, t1, black, white, vtx);
}

static void pic(osk_tex tex, float x, float y, float w, float h, osk_color black, osk_color white)
{
	quad(tex, x, y, x + w, y + h, 0, 0, 1, 1, black, white, NULL);
}

static void pic_c(osk_tex tex, float cx, float cy, float w, float h, osk_color black, osk_color white)
{
	pic(tex, cx - w / 2, cy - h / 2, w, h, black, white);
}

static void vgrad(osk_color top, osk_color bottom, osk_color v[4])
{
	v[0] = v[1] = top;
	v[2] = v[3] = bottom;
}

/*-------------------------------------------------------------
  text
-------------------------------------------------------------*/
typedef struct {
	osk_font_size fs;
	osk_color top, bottom;		/* gradient over the line height */
} text_style;

/* one glyph with its pen position on the baseline of a line starting at
   line_top; returns the advance */
static float glyph(const text_style *st, uint32_t cp, float x, float line_top)
{
	const osk_glyph *g = osk_find_glyph(cp);
	float sy = osk_glyph_scale_y(&st->fs), sx = sy * (st->fs.sx / 30.0f) / (st->fs.sy / 36.0f);
	float base = line_top + 30.0f * st->fs.sy / 36.0f;
	float x0, y0, x1, y1;
	osk_color v[4];

	if (!g)
		g = osk_find_glyph('?');
	if (g->w && g->h) {
		x0 = x + g->left * sx;
		y0 = base - g->top * sy;
		x1 = x0 + g->w * sx;
		y1 = y0 + g->h * sy;
		vgrad(mix(st->top, st->bottom, (y0 - line_top) / st->fs.sy),
		      mix(st->top, st->bottom, (y1 - line_top) / st->fs.sy), v);
		quad(OSK_TEX_FONT, x0, y0, x1, y1,
		     (float)g->x / osk_font_atlas_w, (float)g->y / osk_font_atlas_h,
		     (float)(g->x + g->w) / osk_font_atlas_w, (float)(g->y + g->h) / osk_font_atlas_h,
		     with_alpha(c_white, 0), c_white, v);
	}
	return g->adv16 / 16.0f * sx + st->fs.char_space;
}

static void text_at(const text_style *st, const char *s, float x, float line_top)
{
	uint32_t cp;
	size_t k;

	while ((k = OSK_UTF8Decode(s, &cp)) != 0) {
		x += glyph(st, cp, x, line_top);
		s += k;
	}
}

static void text_centered(const text_style *st, const char *s, float cx, float cy)
{
	text_at(st, s, cx - osk_utf8_width(s, &st->fs) / 2, cy - st->fs.sy / 2);
}

static void char_centered(const text_style *st, uint32_t cp, float cx, float cy)
{
	char buf[5] = { 0 };

	OSK_UTF8Encode(cp, buf);
	text_centered(st, buf, cx, cy);
}

/*-------------------------------------------------------------
  background and toolbar
-------------------------------------------------------------*/
/* vertical gradient quad spanning [y0, y1] whose colors are given at
   ya and yb, extrapolated outside */
static void band(float x0, float x1, float y0, float y1, float ya, osk_color ca,
		 float yb, osk_color cb, osk_color tint)
{
	osk_color v[4];

	vgrad(mix(ca, cb, (y0 - ya) / (yb - ya)), mix(ca, cb, (y1 - ya) / (yb - ya)), v);
	quad(OSK_TEX_WHITE, x0, y0, x1, y1, 0, 0, 1, 1, c_clear, tint, v);
}

static void draw_background(osk_keyboard *osk)
{
	osk_color panel = from_u32(osk->theme.panel);
	osk_color black = mix(with_alpha(c_clear, 255), panel, 246 / 255.0f);
	float x0 = -ox, x1 = osk->vw - ox, y;
	osk_color bar0 = { 255, 255, 255, 32 }, bar1 = { 255, 255, 255, 160 };

	black.a = 255;
	/* fine stripes: the 8 texel stripe pattern repeated 24 times over
	   (14,6) 612x464, one quad per repeat */
	for (y = 6 - 464.0f / 24; y < OSK_DESIGN_H; y += 464.0f / 24)
		quad(OSK_TEX_STRIPES, x0, y, x1, y + 464.0f / 24, 0, 0, 1, 1, black, panel, NULL);

	/* toolbar: light blue wash from y=130, darker blue band from y=394 */
	band(x0, x1, 130, OSK_DESIGN_H, 130, bar0, 472, bar1, rgb(0, 182, 255));
	band(x0, x1, 394, OSK_DESIGN_H, 394, (osk_color){ 128, 168, 203, 96 },
	     472, (osk_color){ 10, 135, 177, 255 }, c_white);
}

/*-------------------------------------------------------------
  text box
-------------------------------------------------------------*/
/* window with one corner texture mirrored into all four corners; the
   edges stretch the corner's last texel row or column */
static void window(osk_tex tex, float c, float x, float y, float w, float h,
		   osk_color black, osk_color white)
{
	float e = (c - 0.5f) / c;

	quad(OSK_TEX_WHITE, x + c, y + c, x + w - c, y + h - c, 0, 0, 1, 1, c_clear, c_white, NULL);
	quad(tex, x, y, x + c, y + c, 0, 0, 1, 1, black, white, NULL);
	quad(tex, x + w - c, y, x + w, y + c, 1, 0, 0, 1, black, white, NULL);
	quad(tex, x, y + h - c, x + c, y + h, 0, 1, 1, 0, black, white, NULL);
	quad(tex, x + w - c, y + h - c, x + w, y + h, 1, 1, 0, 0, black, white, NULL);
	quad(tex, x + c, y, x + w - c, y + c, e, 0, e, 1, black, white, NULL);
	quad(tex, x + c, y + h - c, x + w - c, y + h, e, 1, e, 0, black, white, NULL);
	quad(tex, x, y + c, x + c, y + h - c, 0, e, 1, e, black, white, NULL);
	quad(tex, x + w - c, y + c, x + w, y + h - c, 1, e, 0, e, black, white, NULL);
}

/* rounded box stretched horizontally: the ends keep their shape */
static void round_box(float x0, float y0, float x1, float y1, osk_color c)
{
	float r = (y1 - y0) / 2;
	osk_color rim = with_alpha(c, 0);	/* the texture's dark rim takes the fill color */

	quad(OSK_TEX_ROUND, x0, y0, x0 + r, y1, 0, 0, 0.5f, 1, rim, c, NULL);
	quad(OSK_TEX_ROUND, x0 + r, y0, x1 - r, y1, 0.5f, 0, 0.5f, 1, rim, c, NULL);
	quad(OSK_TEX_ROUND, x1 - r, y0, x1, y1, 0.5f, 0, 1, 1, rim, c, NULL);
}

/* white window between the text box and the keys, with a plate
   behind the scroll arrows */
static void draw_bar(osk_keyboard *osk)
{
	window(OSK_TEX_FRAME_THIN, 8, 38, 134, 484, 36, with_alpha(grey(150), 0), c_white);
	round_box(436, 137, 520, 167, grey(226));
}

static void draw_textbox(osk_keyboard *osk)
{
	const osk_theme *th = &osk->theme;
	osk_color frame = from_u32(th->textbox_border);
	unsigned start[OSK_MAX_CHARS + 2], n, line, i, caret_line;
	text_style st = { osk_text_font, from_u32(th->text), from_u32(th->text) };
	osk_color grey_arrow = grey(128);

	window(OSK_TEX_FRAME_THICK, 16, 39, 30.33f, 556, 100, with_alpha(frame, 0), c_white);
	window(OSK_TEX_FRAME_THIN, 8, 46, 36.83f, 542, 87, with_alpha(frame, 0), c_white);

	if (!osk->len && osk->cfg.title && osk->cfg.title[0]) {
		osk_color t = from_u32(th->title);
		text_style ph = { { 25, 30, -1 }, t, mix(t, c_white, 6 / 255.0f) };
		text_at(&ph, osk->cfg.title, 52, 46.33f);
	}

	n = osk_wrap_text(osk, start);
	for (line = osk->scroll; line < n && line < osk->scroll + OSK_TEXT_LINES; line++) {
		float x = OSK_TEXT_X, top = OSK_TEXT_Y + (line - osk->scroll) * OSK_TEXT_LINE_H;
		for (i = start[line]; i < start[line + 1]; i++) {
			uint32_t cp = osk_display_char(osk, i);
			text_style pending = st;

			if (cp == '\n')
				continue;
			/* the multi-tap letter still being chosen */
			if (osk->tap_slot >= 0 && i + 1 == osk->caret) {
				pending.top = pending.bottom = cp == 0x2423 ? rgb(255, 20, 20) : rgb(200, 50, 50);
				x += glyph(&pending, cp, x, top);
			} else {
				x += glyph(&st, cp, x, top);
			}
		}
	}

	/* caret: red bar pulsing at 8 degrees per frame */
	caret_line = osk_line_of(start, n, osk->caret);
	if (caret_line >= osk->scroll && caret_line < osk->scroll + OSK_TEXT_LINES) {
		float x = OSK_TEXT_X + osk_caret_x(osk, start, caret_line, osk->caret);
		float top = OSK_TEXT_Y + (caret_line - osk->scroll) * OSK_TEXT_LINE_H;
		float a = sinf(osk->frame * 8.0f * (float)M_PI / 180.0f) * 127 + 127;

		pic(OSK_TEX_WHITE, x - 2, top + 2, 4, OSK_TEXT_LINE_H - 4, c_clear,
		    with_alpha(from_u32(th->caret), clamp8(a * (th->caret & 0xFF) / 255.0f)));
	}

	/* scroll arrows while the text is longer than the box */
	if (osk->scroll > 0)
		pic(OSK_TEX_ARROW, 546, 46, 38, 30, with_alpha(grey_arrow, 0), c_white);
	if (osk->scroll + OSK_TEXT_LINES < n)
		quad(OSK_TEX_ARROW, 546.17f, 84.92f, 584.17f, 114.92f, 0, 1, 1, 0,
		     with_alpha(grey_arrow, 0), c_white, NULL);
}

/*-------------------------------------------------------------
  keys
-------------------------------------------------------------*/
typedef struct {
	float focus_scale, push_scale;
	int color_peak;			/* frame of the strongest push tint */
} look_anim;

static const look_anim look_anims[] = {
	[LOOK_CHAR]	= { 1.5f,    1.3f,    3 },
	[LOOK_SPECIAL]	= { 1.3f,    1.15f,   3 },
	[LOOK_SPACE]	= { 1.3f,    1.15f,   3 },
	[LOOK_MORE]	= { 1.15f,   1.05f,   3 },
	[LOOK_PILL]	= { 1.1229f, 1.0453f, 3 },
	[LOOK_TAB]	= { 1.1f,    1.0f,    3 },
	[LOOK_PAD]	= { 1.4f,    1.3f,    7 },
	[LOOK_PAD_BLUE]	= { 1.4f,    1.3f,    7 },
	[LOOK_MODE]	= { 1.0f,    1.0f,    3 },
	[LOOK_SCROLL]	= { 1.15f,   1.05f,   3 },
	[LOOK_CANDIDATE] = { 1.0f,   1.0f,    3 },
	[LOOK_SYMBOL]	= { 1.1f,    1.07f,   3 },
	[LOOK_SYMBOL_BUTTON] = { 1.08f, 1.05f, 3 },
};

/* scale dips to push_scale 3 frames after the press and is back after 7 */
static float push_scale(const osk_key *k, float scale)
{
	const look_anim *la = &look_anims[k->look];
	float lo = scale * la->push_scale / la->focus_scale;

	if (k->push < 0 || k->look == LOOK_TAB)
		return scale;
	if (k->push <= 3)
		return scale + (lo - scale) * smooth(k->push / 3.0f);
	if (k->push <= 7)
		return lo + (scale - lo) * smooth((k->push - 3) / 4.0f);
	return scale;
}

/* 0 -> 1 -> 0 over the push animation */
static float push_tint(const osk_key *k)
{
	int peak = look_anims[k->look].color_peak;

	if (k->push < 0)
		return 0;
	if (k->push <= peak)
		return smooth((float)k->push / peak);
	return 1 - smooth((float)(k->push - peak) / (OSK_PUSH_FRAMES - peak));
}

/* a blue caption with the font of the cellphone keypad's side keys */
static const text_style pad_blue = { { 28, 30, -1 }, { 0, 72, 255, 255 }, { 0, 80, 255, 255 } };

static void draw_key(osk_keyboard *osk, const osk_key *k)
{
	const osk_theme *th = &osk->theme;
	const look_anim *la = &look_anims[k->look];
	float f = smooth(k->focus), p = push_tint(k);
	float scale = push_scale(k, 1 + (la->focus_scale - 1) * f);
	osk_color focus_white = from_u32(th->focus_top), white = c_white, black;
	text_style st;
	const char *label = osk_key_label(osk, k);
	bool latched = osk_key_latched(osk, k);
	float dx = 0;			/* mode tags slide out */

	xf_s = scale;
	xf_cx = k->cx;
	xf_cy = k->cy;

	switch (k->look) {
	case LOOK_CHAR:
	case LOOK_SPECIAL:
	case LOOK_SPACE:
		if (latched) {
			osk_color on = from_u32(th->active);
			white = mix(on, rgb(on.r * 220 / 255, on.g * 220 / 255, on.b), f);
		} else {
			white = mix(c_white, k->kind == KEY_SHIFT || k->kind == KEY_CAPS ?
				    rgb(focus_white.r * 220 / 224, focus_white.g * 220 / 224, focus_white.b) :
				    focus_white, f);
		}
		if (k->kind != KEY_SHIFT && k->kind != KEY_CAPS)
			white.b = clamp8(white.b + ((k->look == LOOK_CHAR ? 38 : 64) - white.b) * p);
		black = k->look == LOOK_CHAR ? rgb(138, 148, 153) : rgb(76, 146, 181);
		pic_c(k->look == LOOK_CHAR ? OSK_TEX_GLASS_KEY :
		      k->look == LOOK_SPACE ? OSK_TEX_GLASS_SPACE : OSK_TEX_GLASS_WIDE,
		      k->cx, k->cy, k->w, k->h, with_alpha(black, 0), white);
		break;
	case LOOK_MORE:
		black = mix(rgb(16, 72, 114), rgb(128, 220, 255), p);
		white = mix(rgb(240, 240, 255), grey(232), p);
		pic_c(OSK_TEX_PLATE, k->cx, k->cy, k->w, k->h, with_alpha(black, 0), white);
		break;
	case LOOK_PILL:
		white.b = clamp8(255 - 95 * p);
		pic_c(OSK_TEX_PILL, k->cx, k->cy, k->w, k->h, c_clear, white);
		break;
	case LOOK_TAB:
		white = latched ? rgb(52, 190, 237) : mix(grey(160), c_white, f);
		pic_c(OSK_TEX_TOOL, k->cx, k->cy, k->w, k->h, with_alpha(grey(40), 0), white);
		pic_c(k->kind == KEY_TAB_LETTERS ? OSK_TEX_ICON_QWERTY : OSK_TEX_ICON_NUMPAD,
		      k->cx, k->cy, k->w - 4, 32, with_alpha(grey(80), 0), c_white);
		break;
	case LOOK_PAD:
	case LOOK_PAD_BLUE:
		if (k->look == LOOK_PAD_BLUE)
			white.r = 216;
		white.b = clamp8(255 - 127 * p);
		pic_c(OSK_TEX_TOOL, k->cx, k->cy, k->w, k->h, c_clear, white);
		break;
	case LOOK_MODE:
		/* the chosen tag is white and stands out from the edge, like a
		   focused one */
		dx = 12 * (latched ? 1 : f);
		black = latched ? grey(0) : rgb(0, 16, 160);
		white = latched ? c_white : rgb(144, 220, 232);
		pic_c(OSK_TEX_TOOL, k->cx + dx, k->cy, k->w, k->h, with_alpha(black, 0), white);
		break;
	case LOOK_SYMBOL:
		black = mix(grey(140), grey(32), f);
		white.b = clamp8(255 - 159 * p);
		pic_c(OSK_TEX_TOOL, k->cx, k->cy, k->w, k->h, with_alpha(black, 0), white);
		break;
	case LOOK_SYMBOL_BUTTON:
		/* only Close shows its plate */
		if (k->kind == KEY_SYMBOL_CLOSE) {
			white = rgb(217, 255, clamp8(255 - 127 * p));
			pic_c(OSK_TEX_WINDOW_PLATE, k->cx, k->cy, k->w, k->h, with_alpha(rgb(76, 138, 138), 0), white);
		}
		break;
	case LOOK_SCROLL: {
		bool can = k->kind == KEY_SCROLL_L ? osk->cand_first > 0 :
			   osk->cand_first + osk->cand_shown < osk->ncand;
		osk_color dark = can ? grey(64) : grey(190);
		float s0 = k->kind == KEY_SCROLL_L ? 0 : 1;

		white = mix(c_white, focus_white, f);
		white.b = clamp8(white.b + (64 - white.b) * p);
		quad(OSK_TEX_SCROLL, k->cx - k->w / 2, k->cy - k->h / 2, k->cx + k->w / 2, k->cy + k->h / 2,
		     s0, 0, 1 - s0, 1, with_alpha(dark, 0), white, NULL);
		break;
	}
	case LOOK_CANDIDATE:
		if (f > 0 || p > 0) {
			osk_color hl = mix(from_u32(th->focus_top), rgb(255, 255, 160), p);
			round_box(k->cx - k->w / 2, k->cy - k->h / 2, k->cx + k->w / 2, k->cy + k->h / 2,
				  with_alpha(hl, clamp8(255 * fmaxf(f, p))));
		}
		break;
	}

	/* contents */
	switch (k->kind) {
	case KEY_CHAR: {
		uint32_t cp = osk_key_char(osk, k);
		osk_color c = from_u32(osk_accepts(osk, cp) ? th->key_text : th->key_text_disabled);
		text_style ch = { { 26, 27, 0 }, c, c };

		char_centered(&ch, cp, k->cx, k->cy);
		break;
	}
	case KEY_PHONE: {
		uint32_t seq[16];
		unsigned n = osk_phone_chars(osk, k->arg, seq), i;
		bool any = false;
		char top[32];
		osk_color c;

		for (i = 0; i < n; i++)
			any |= osk_accepts(osk, seq[i]);
		c = any ? grey(72) : from_u32(th->key_text_disabled);
		st = (text_style){ { 28.8f, 34.2f, 0 }, c, c };
		osk_phone_label(osk, k->arg, top);
		if (osk_utf8_width(top, &st.fs) > 108)
			st.fs.sx *= 108 / osk_utf8_width(top, &st.fs);
		text_centered(&st, top, k->cx, k->cy + 1);
		break;
	}
	case KEY_PHONE_MODE:
		st = (text_style){ { 27.2f, 32.3f, 0 }, rgb(0, 64, 255), rgb(0, 24, 98) };
		text_centered(&st, label, k->cx - 4.5f + dx, k->cy + 0.3f);
		break;
	case KEY_SYMBOLS:
		text_centered(&pad_blue, label, k->cx, k->cy + 1);
		break;
	case KEY_SYMBOL: {
		uint32_t cp = osk_symbol(osk, osk->symbol_page, k->arg);
		osk_color c = osk_accepts(osk, cp) ? grey(64) : from_u32(th->key_text_disabled);

		st = (text_style){ { 35.2f, 41.8f, 0 }, c, c };
		char_centered(&st, cp, k->cx, k->cy);
		break;
	}
	case KEY_SYMBOL_PREV:
	case KEY_SYMBOL_NEXT:
	case KEY_SYMBOL_CLOSE:
		st = (text_style){ { 28.8f, 34.2f, 0 }, rgb(64, 108, 108), rgb(64, 108, 108) };
		text_centered(&st, label, k->cx - (k->kind == KEY_SYMBOL_CLOSE ? 4.1f : 0.8f), k->cy - 0.6f);
		break;
	case KEY_BACK:
		if (k->look == LOOK_PAD)
			pic_c(OSK_TEX_ICON_BACK, k->cx - 8, k->cy, 52, 26, c_clear, grey(128));
		else
			pic_c(OSK_TEX_ICON_BACK, k->cx, k->cy, 44, 20, c_clear, grey(128));
		break;
	case KEY_ENTER:
		if (k->look == LOOK_PAD)
			pic_c(OSK_TEX_ICON_ENTER, k->cx - 8, k->cy, 54, 40, c_clear, grey(128));
		else
			pic_c(OSK_TEX_ICON_ENTER, k->cx - 1, k->cy, 44, 26, c_clear, grey(128));
		break;
	case KEY_SHIFT:
	case KEY_CAPS:
		/* the French, German, Italian and Spanish keyboards show an
		   arrow instead of the words */
		if (osk->letters == OSK_LAYOUT_AZERTY || osk->letters == OSK_LAYOUT_QWERTZ ||
		    osk->letters == OSK_LAYOUT_QWERTY_IT || osk->letters == OSK_LAYOUT_QWERTY_ES) {
			pic_c(OSK_TEX_ICON_SHIFT, k->cx, k->cy, k->kind == KEY_SHIFT ? 52 : 48, 28,
			      with_alpha(grey(128), 0), grey(128));
			break;
		}
		st = (text_style){ { 24, 22, 1 }, grey(64), grey(64) };
		text_centered(&st, label, k->cx, k->cy);
		break;
	case KEY_SPACE:
		st = (text_style){ { 22, 24, 0 }, grey(64), grey(64) };
		if (!osk_accepts(osk, ' '))
			st.top = st.bottom = from_u32(th->key_text_disabled);
		text_centered(&st, label, k->cx + 1, k->cy);
		break;
	case KEY_ALT:
		st = (text_style){ { 26, 30, 0 }, rgb(46, 46, 174), rgb(42, 42, 154) };
		text_centered(&st, label, k->cx + 2, k->cy);
		break;
	case KEY_PREDICT: {
		/* book icon on the left, the language name in the rest */
		float w;

		if (k->look == LOOK_PAD_BLUE) {
			/* keypad: short name with a fading line under it */
			osk_color line[4] = { rgb(0, 68, 255), rgb(0, 68, 255), rgb(0, 68, 255), rgb(0, 68, 255) };

			if (latched)
				pic_c(OSK_TEX_BOOK_OPEN, k->cx - 32.5f, k->cy + 0.3f, 41.6f, 28.8f,
				      with_alpha(grey(40), 0), c_white);
			else
				pic_c(OSK_TEX_BOOK_CLOSED, k->cx - 32.3f, k->cy + 0.3f, 32, 32,
				      with_alpha(grey(40), 0), c_white);
			quad(OSK_TEX_FADE_BAR, k->cx - 42.2f, k->cy + 15.3f, k->cx + 45.8f, k->cy + 17.3f,
			     0, 0, 1, 1, c_clear, c_white, line);
			st = pad_blue;
			st.fs = (osk_font_size){ 32, 30, -1 };
			w = osk_utf8_width(label, &st.fs);
			if (w > 64)
				st.fs.sx *= 64 / w;
			text_centered(&st, label, k->cx + 18.6f, k->cy - 3.7f);
			break;
		}
		if (latched)
			pic_c(OSK_TEX_BOOK_OPEN, k->cx - 56, k->cy, 44, 28, with_alpha(grey(40), 0), c_white);
		else
			pic_c(OSK_TEX_BOOK_CLOSED, k->cx - 56, k->cy, 32, 28, with_alpha(grey(40), 0), c_white);
		st = (text_style){ { 22, 26, 0 }, rgb(46, 46, 174), rgb(42, 42, 154) };
		w = osk_utf8_width(label, &st.fs);
		if (w > 104)
			st.fs.sx *= 104 / w;
		text_centered(&st, label, k->cx + 27, k->cy);
		break;
	}
	case KEY_CANDIDATE: {
		const osk_candidate *c = &osk->cand[k->arg];
		float w = osk_text_width(c->cp, c->len, &osk_cand_font), x;
		unsigned i;

		st = (text_style){ osk_cand_font, grey(96), grey(96) };
		if (w > OSK_BAR_X1 - OSK_BAR_X0) {
			st.fs.sx *= (OSK_BAR_X1 - OSK_BAR_X0) / w;
			w = OSK_BAR_X1 - OSK_BAR_X0;
		}
		x = k->cx - w / 2;
		for (i = 0; i < c->len; i++)
			x += glyph(&st, c->cp[i], x, k->cy - st.fs.sy / 2);
		break;
	}
	case KEY_OK:
	case KEY_CANCEL:
		st = (text_style){ { 27, 29, -1 }, grey(0), grey(0) };
		text_centered(&st, label, k->cx, k->cy);
		break;
	default:
		break;
	}

	xf_s = 1;
}

static bool toolbar_key(const osk_key *k)
{
	return k->look == LOOK_PILL || k->look == LOOK_TAB;
}

static bool animating(const osk_key *k)
{
	return k->focus > 0 || k->push >= 0;
}

/* resting keys first, then the raised ones from the least to the most
   focused so that the highlighted key ends up on top */
static void draw_keys(osk_keyboard *osk, const osk_key *keys, int nkeys, bool toolbar)
{
	int order[OSK_MAX_KEYS], n = 0, i, j;

	for (i = 0; i < nkeys; i++)
		if (toolbar_key(&keys[i]) == toolbar && !animating(&keys[i]))
			draw_key(osk, &keys[i]);
	for (i = 0; i < nkeys; i++) {
		if (toolbar_key(&keys[i]) != toolbar || !animating(&keys[i]))
			continue;
		for (j = n; j > 0 && keys[order[j - 1]].focus > keys[i].focus; j--)
			order[j] = order[j - 1];
		order[j] = i;
		n++;
	}
	for (i = 0; i < n; i++)
		draw_key(osk, &keys[order[i]]);
}

/*-------------------------------------------------------------
  symbol window
-------------------------------------------------------------*/
/* 0 -> 1.01 -> 1 while opening, back to 0.75 while closing */
static void symbol_zoom(const osk_keyboard *osk)
{
	float t;

	win_cx = 320;
	win_cy = 240;
	if (osk->symbols) {
		t = (float)osk->symbol_anim / OSK_SYMBOL_OPEN_FRAMES;
		win_a = t;
		win_s = t < 14.0f / 18 ? 0.75f + 0.26f * smooth(t * 18 / 14) :
			1.01f - 0.01f * smooth((t * 18 - 14) / 4);
	} else {
		t = (float)osk->symbol_anim / OSK_SYMBOL_CLOSE_FRAMES;
		win_a = 1 - t;
		win_s = 1 - 0.25f * smooth(t);
	}
}

/* the symbol keys showing another page, shifted and faded */
static void draw_symbol_page(osk_keyboard *osk, const osk_key *keys, int n, unsigned page,
			     float dx, float alpha)
{
	unsigned shown = osk->symbol_page;
	float a = win_a;
	int i;

	osk->symbol_page = page;
	win_a *= alpha;
	for (i = 0; i < n; i++) {
		osk_key k = keys[i];

		if (k.kind != KEY_SYMBOL)
			continue;
		k.cx += dx;
		draw_key(osk, &k);
	}
	osk->symbol_page = shown;
	win_a = a;
}

static void draw_symbol_window(osk_keyboard *osk, const osk_key *keys, int n)
{
	text_style st = { { 25.6f, 30.4f, 0 }, grey(64), grey(64) };
	char num[8];
	int i;

	symbol_zoom(osk);
	window(OSK_TEX_FRAME_THICK, 16, 44, 163.5f, 552, 280, with_alpha(grey(0), 0), c_white);
	/* dark strip along the bottom, a pale plate behind the page number */
	pic_c(OSK_TEX_WHITE, 320, 408.8f, 536, 52, c_clear, (osk_color){ 0, 0, 0, 32 });
	pic_c(OSK_TEX_TOOL, 319.7f, 406.1f, 102, 48, with_alpha(c_white, 0), (osk_color){ 255, 255, 255, 80 });
	snprintf(num, sizeof(num), "%u/%u", osk->symbol_page + 1, osk_symbol_pages);
	text_centered(&st, num, 316.9f, 408.4f);

	if (osk->symbol_slide >= OSK_SYMBOL_SLIDE_FRAMES) {
		draw_keys(osk, keys, n, false);
	} else {
		/* the old page slides out and fades while the new one comes in
		   from the other side */
		float t = smooth((float)osk->symbol_slide / OSK_SYMBOL_SLIDE_FRAMES);
		float fade = fminf(osk->symbol_slide / 15.0f, 1);
		float d = 300.0f * osk->symbol_dir;

		draw_symbol_page(osk, keys, n, osk->symbol_old_page, d * t, 1 - fade);
		draw_symbol_page(osk, keys, n, osk->symbol_page, -d * (1 - t), fade);
		for (i = 0; i < n; i++)
			if (keys[i].kind != KEY_SYMBOL)
				draw_key(osk, &keys[i]);
	}
	win_s = win_a = 1;
}

static void draw_pointer(osk_keyboard *osk)
{
	pic(OSK_TEX_POINTER, osk->px - 2, osk->py - 2, 32, 40, c_clear, c_white);
}

void osk_render(osk_keyboard *osk)
{
	osk_backend_begin(osk);

	ox = osk->ox;
	oy = osk->oy;
	xf_s = 1;

	/* while the symbol window is open the page behind it is drawn at
	   rest; while it closes, the window is */
	const osk_key *page = osk->symbols ? osk->under : osk->keys;
	int npage = osk->symbols ? osk->nunder : osk->nkeys;
	bool closing = !osk->symbols && osk->symbol_anim < OSK_SYMBOL_CLOSE_FRAMES;

	draw_background(osk);
	if (!(osk->cfg.flags & OSK_FLAG_LOCK_LAYOUT)) {
		osk_color v[4];
		vgrad(c_white, grey(224), v);
		quad(OSK_TEX_TOOL, 250, 399, 390, 449, 0, 0, 1, 1, with_alpha(grey(126), 0), grey(231), v);
	}
	draw_keys(osk, page, npage, true);
	draw_textbox(osk);
	if (osk_predict_active(osk))
		draw_bar(osk);
	draw_keys(osk, page, npage, false);
	if (osk->symbols)
		draw_symbol_window(osk, osk->keys, osk->nkeys);
	else if (closing)
		draw_symbol_window(osk, osk->under, osk->nunder);

	if (osk->pointer_valid && !(osk->cfg.flags & OSK_FLAG_NO_POINTER))
		draw_pointer(osk);

	osk_backend_end();
}

void OSK_Draw(osk_keyboard *osk)
{
	if (osk)
		osk_render(osk);
}
