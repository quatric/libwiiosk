/*-------------------------------------------------------------
osk_render.c -- GX drawing
-------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "osk_internal.h"

#define PANEL_X		16.0f
#define PANEL_Y		34.0f
#define PANEL_W		608.0f
#define PANEL_H		414.0f
#define TEXTBOX_X	32.0f
#define TEXTBOX_Y	88.0f
#define TEXTBOX_W	576.0f
#define TEXTBOX_H	60.0f
#define TEXTBOX_PAD	12.0f
#define HINT_Y		424.0f

#define ATLAS_TEX_W	128.0f
#define ATLAS_TEX_H	256.0f

/* texture coordinate inside the solid slot (slot 0) */
#define SOLID_S		(4.0f / ATLAS_TEX_W)
#define SOLID_T		(8.0f / ATLAS_TEX_H)

static float ox, oy;	/* design -> virtual offset for the current draw */

static inline void vtx(float x, float y, u32 c, float s, float t)
{
	GX_Position2f32(x + ox, y + oy);
	GX_Color4u8(c >> 24, c >> 16, c >> 8, c);
	GX_TexCoord2f32(s, t);
}

static u32 lerp_color(u32 a, u32 b, float t)
{
	u32 out = 0;
	int shift;

	for (shift = 24; shift >= 0; shift -= 8) {
		float ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
		out |= ((u32)(ca + (cb - ca) * t) & 0xFF) << shift;
	}
	return out;
}

static u32 with_alpha(u32 c, u32 a)
{
	return (c & 0xFFFFFF00) | ((c & 0xFF) * a / 255);
}

static void rect(float x, float y, float w, float h, u32 top, u32 bottom)
{
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	vtx(x,     y,     top,    SOLID_S, SOLID_T);
	vtx(x + w, y,     top,    SOLID_S, SOLID_T);
	vtx(x + w, y + h, bottom, SOLID_S, SOLID_T);
	vtx(x,     y + h, bottom, SOLID_S, SOLID_T);
	GX_End();
}

/* Rectangle with chamfered corners and a vertical gradient. */
static void box(float x, float y, float w, float h, float r, u32 top, u32 bottom)
{
	const float px[8] = { x + r, x + w - r, x + w, x + w, x + w - r, x + r, x, x };
	const float py[8] = { y, y, y + r, y + h - r, y + h, y + h, y + h - r, y + r };
	int i;

	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, 10);
	vtx(x + w / 2, y + h / 2, lerp_color(top, bottom, 0.5f), SOLID_S, SOLID_T);
	for (i = 0; i <= 8; i++) {
		int j = i & 7;
		vtx(px[j], py[j], lerp_color(top, bottom, (py[j] - y) / h), SOLID_S, SOLID_T);
	}
	GX_End();
}

static void glyph(float x, float y, uint32_t cp, float scale, u32 color)
{
	unsigned slot = osk_font_slot(cp);
	float s0 = (slot % 16) * OSK_GLYPH_W / ATLAS_TEX_W;
	float t0 = (slot / 16) * OSK_GLYPH_H / ATLAS_TEX_H;
	float s1 = s0 + OSK_GLYPH_W / ATLAS_TEX_W;
	float t1 = t0 + OSK_GLYPH_H / ATLAS_TEX_H;
	float w = OSK_GLYPH_W * scale, h = OSK_GLYPH_H * scale;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	vtx(x,     y,     color, s0, t0);
	vtx(x + w, y,     color, s1, t0);
	vtx(x + w, y + h, color, s1, t1);
	vtx(x,     y + h, color, s0, t1);
	GX_End();
}

static unsigned text_len(const char *s)
{
	unsigned n = 0;
	uint32_t cp;
	size_t k;

	while ((k = OSK_UTF8Decode(s, &cp)) != 0) {
		s += k;
		n++;
	}
	return n;
}

static void text(float x, float y, const char *s, float scale, u32 color)
{
	uint32_t cp;
	size_t k;

	while ((k = OSK_UTF8Decode(s, &cp)) != 0) {
		glyph(x, y, cp, scale, color);
		x += OSK_GLYPH_W * scale;
		s += k;
	}
}

static void text_centered(float cx, float cy, const char *s, float scale, u32 color)
{
	float w = text_len(s) * OSK_GLYPH_W * scale;
	text(cx - w / 2, cy - OSK_GLYPH_H * scale / 2, s, scale, color);
}

void osk_textbox_rect(const osk_keyboard *osk, float *x, float *y, float *w, float *h)
{
	(void)osk;
	*x = TEXTBOX_X;
	*y = TEXTBOX_Y;
	*w = TEXTBOX_W;
	*h = TEXTBOX_H;
}

unsigned osk_textbox_visible_chars(const osk_keyboard *osk)
{
	(void)osk;
	return (unsigned)((TEXTBOX_W - 2 * TEXTBOX_PAD) / (OSK_GLYPH_W * OSK_TEXT_SCALE));
}

static void setup_gx(osk_keyboard *osk)
{
	Mtx44 proj;
	Mtx view;

	guOrtho(proj, 0, osk->vh, 0, osk->vw, 0, 1000.0f);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
	guMtxIdentity(view);
	guMtxTransApply(view, view, 0.0f, 0.0f, -100.0f);
	GX_LoadPosMtxImm(view, GX_PNMTX0);
	GX_SetCurrentMtx(GX_PNMTX0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_InvVtxCache();

	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	GX_InvalidateTexAll();
	GX_LoadTexObj(osk_font_texture(), GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetColorUpdate(GX_TRUE);
	GX_SetAlphaUpdate(GX_TRUE);
}

static bool key_latched(const osk_keyboard *osk, const osk_key *k)
{
	switch (k->kind) {
	case KEY_SHIFT:	return osk->shift;
	case KEY_CAPS:	return osk->caps;
	case KEY_ALT:	return osk->alt;
	default:	return false;
	}
}

static void draw_key(osk_keyboard *osk, int idx)
{
	const osk_theme *th = &osk->theme;
	const osk_key *k = &osk->keys.keys[idx];
	bool focused = idx == osk->focus;
	bool pressed = idx == osk->flash_key && osk->flash_time > 0;
	bool latched = key_latched(osk, k);
	bool special = k->kind != KEY_CHAR && k->kind != KEY_SPACE;
	float x = k->x + 2, y = k->y + 2, w = k->w - 4, h = k->h - 6;
	u32 top, bottom, label_color = th->key_text;
	const char *label;

	if (focused) {
		top = th->focus_top;
		bottom = th->focus_bottom;
	} else if (latched) {
		top = lerp_color(th->active, 0xFFFFFFFF, 0.35f);
		bottom = th->active;
	} else if (special) {
		top = th->special_top;
		bottom = th->special_bottom;
	} else {
		top = th->key_top;
		bottom = th->key_bottom;
	}

	if (pressed) {
		y += 2;
		top = lerp_color(top, bottom, 0.6f);
	} else {
		box(x, y + 3, w, h, 6, 0x00000038, 0x00000038);	/* drop shadow */
	}
	box(x, y, w, h, 6, top, bottom);

	if (k->kind == KEY_CHAR) {
		uint32_t cp = osk_key_char(osk, k);
		if (!osk_accepts(osk, cp))
			label_color = th->key_text_disabled;
		glyph(x + w / 2 - OSK_GLYPH_W, y + h / 2 - OSK_GLYPH_H, cp, 2.0f, label_color);
		return;
	}

	label = osk_key_label(osk, k);
	if (k->kind == KEY_SPACE) {
		if (!osk_accepts(osk, ' '))
			label_color = th->key_text_disabled;
		label_color = with_alpha(label_color, 0x80);
	}
	if (latched && !focused)
		label_color = 0xFFFFFFFF;
	if (label)
		text_centered(x + w / 2, y + h / 2, label,
			      (k->kind == KEY_LEFT || k->kind == KEY_RIGHT) ? 2.0f : 1.0f, label_color);
}

static void draw_textbox(osk_keyboard *osk)
{
	const osk_theme *th = &osk->theme;
	float x = TEXTBOX_X, y = TEXTBOX_Y;
	float cw = OSK_GLYPH_W * OSK_TEXT_SCALE;
	unsigned visible = osk_textbox_visible_chars(osk);
	u32 border = th->textbox_border;
	unsigned i;

	if (osk->reject_time > 0) {
		x += (osk->reject_time & 2) ? 3.0f : -3.0f;
		border = 0xE04040FF;
	}

	box(x, y, TEXTBOX_W, TEXTBOX_H, 8, border, border);
	box(x + 3, y + 3, TEXTBOX_W - 6, TEXTBOX_H - 6, 6, th->textbox, lerp_color(th->textbox, 0xE0E4E8FF, 0.5f));

	for (i = 0; i < visible && osk->scroll + i < osk->len; i++) {
		uint32_t cp = osk->text[osk->scroll + i];
		if (osk->cfg.flags & OSK_FLAG_PASSWORD)
			cp = '*';
		glyph(x + TEXTBOX_PAD + i * cw, y + (TEXTBOX_H - OSK_GLYPH_H * OSK_TEXT_SCALE) / 2,
		      cp, OSK_TEXT_SCALE, th->text);
	}

	/* scroll markers */
	if (osk->scroll > 0)
		glyph(x + 2, y + TEXTBOX_H / 2 - 8, 0x11, 1.0f, th->caret);
	if (osk->scroll + visible < osk->len)
		glyph(x + TEXTBOX_W - 10, y + TEXTBOX_H / 2 - 8, 0x10, 1.0f, th->caret);

	if ((osk->frame / 30) % 2 == 0 || osk->repeat[0] || osk->flash_time > 0) {
		float cx = x + TEXTBOX_PAD + (osk->caret - osk->scroll) * cw - 1;
		rect(cx, y + 12, 3, TEXTBOX_H - 24, th->caret, th->caret);
	}
}

static void draw_header(osk_keyboard *osk)
{
	const osk_theme *th = &osk->theme;
	char count[16];
	const char *title = osk->cfg.title;
	unsigned n;

	if (title && *title) {
		n = text_len(title);
		if (n * OSK_GLYPH_W * 2 <= TEXTBOX_W - 96)
			text(TEXTBOX_X + 4, 48, title, 2.0f, th->title);
		else
			text(TEXTBOX_X + 4, 64, title, 1.0f, th->title);
	}

	snprintf(count, sizeof(count), "%u/%u", osk->len, osk->max_chars);
	n = strlen(count);
	text(TEXTBOX_X + TEXTBOX_W - 4 - n * OSK_GLYPH_W, 64, count, 1.0f,
	     osk->len >= osk->max_chars ? 0xE04040FF : th->hint);
}

static void draw_hints(osk_keyboard *osk)
{
	const char *hint = (osk->cfg.flags & OSK_FLAG_NO_CANCEL)
		? "A: Select   B: Erase   -: Shift   1/2: Move   +: OK"
		: "A: Select   B: Erase   -: Shift   1/2: Move   +: OK   HOME: Cancel";

	text_centered(OSK_DESIGN_W / 2, HINT_Y + 8, hint, 1.0f, osk->theme.hint);
}

static void draw_pointer(osk_keyboard *osk)
{
	static const float shape[7][2] = {
		{0, 0}, {0, 22}, {5, 17}, {9, 26}, {13, 24}, {9, 15}, {16, 15},
	};
	float x = osk->px, y = osk->py;
	int pass, i;

	for (pass = 0; pass < 2; pass++) {
		/* pass 0: dark outline drawn enlarged, pass 1: fill */
		float s = pass ? 1.0f : 1.25f;
		float dx = pass ? 0.0f : -1.5f, dy = pass ? 0.0f : -2.0f;
		u32 c = pass ? 0xFFFFFFFF : 0x202830E0;

		GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, 7);
		for (i = 0; i < 7; i++)
			vtx(x + dx + shape[i][0] * s, y + dy + shape[i][1] * s, c, SOLID_S, SOLID_T);
		GX_End();
	}
}

void osk_render(osk_keyboard *osk)
{
	const osk_theme *th = &osk->theme;
	int i;

	setup_gx(osk);

	ox = 0;
	oy = 0;
	rect(0, 0, osk->vw, osk->vh, th->overlay, th->overlay);

	ox = osk->ox;
	oy = osk->oy;
	box(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 14, th->panel, lerp_color(th->panel, 0xC8D0D8FF, 0.5f));

	draw_header(osk);
	draw_textbox(osk);
	for (i = 0; i < osk->keys.nkeys; i++)
		draw_key(osk, i);
	if (!(osk->cfg.flags & OSK_FLAG_NO_HINTS))
		draw_hints(osk);
	if (osk->pointer_valid && !(osk->cfg.flags & OSK_FLAG_NO_POINTER))
		draw_pointer(osk);
}

void OSK_Draw(osk_keyboard *osk)
{
	if (osk)
		osk_render(osk);
}
