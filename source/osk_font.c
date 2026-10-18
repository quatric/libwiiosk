/*-------------------------------------------------------------
osk_font.c -- glyph atlas built at runtime from libogc's 8x16
              console font (code page 437)

Atlas slots are indexed by Latin-1 code point. Glyphs missing from
code page 437 are composed from a base letter plus a diacritic.
-------------------------------------------------------------*/
#include <malloc.h>
#include <string.h>

#include "osk_internal.h"

extern const u8 console_font_8x16[];

#define ATLAS_COLS	16
#define ATLAS_W		(ATLAS_COLS * OSK_GLYPH_W)	/* 128 */
#define ATLAS_H		(16 * OSK_GLYPH_H)		/* 256 */

#define SLOT_SOLID	0x00
#define SLOT_EURO	0x80
#define SLOT_INVALID	'?'

static u8 *atlas;
static GXTexObj atlas_obj;
static bool avail[256];

/* Latin-1 -> code page 437 for characters CP437 has natively. */
static const struct { u8 latin1, cp437; } cp437_map[] = {
	{0xA0,0x20},{0xA1,0xAD},{0xA2,0x9B},{0xA3,0x9C},{0xA5,0x9D},{0xA7,0x15},
	{0xAA,0xA6},{0xAB,0xAE},{0xAC,0xAA},{0xB0,0xF8},{0xB1,0xF1},{0xB2,0xFD},
	{0xB5,0xE6},{0xB6,0x14},{0xB7,0xFA},{0xBA,0xA7},{0xBB,0xAF},{0xBC,0xAC},
	{0xBD,0xAB},{0xBF,0xA8},{0xC4,0x8E},{0xC5,0x8F},{0xC6,0x92},{0xC7,0x80},
	{0xC9,0x90},{0xD1,0xA5},{0xD6,0x99},{0xDC,0x9A},{0xDF,0xE1},{0xE0,0x85},
	{0xE1,0xA0},{0xE2,0x83},{0xE4,0x84},{0xE5,0x86},{0xE6,0x91},{0xE7,0x87},
	{0xE8,0x8A},{0xE9,0x82},{0xEA,0x88},{0xEB,0x89},{0xEC,0x8D},{0xED,0xA1},
	{0xEE,0x8C},{0xEF,0x8B},{0xF1,0xA4},{0xF2,0x95},{0xF3,0xA2},{0xF4,0x93},
	{0xF6,0x94},{0xF7,0xF6},{0xF9,0x97},{0xFA,0xA3},{0xFB,0x96},{0xFC,0x81},
	{0xFF,0x98},
};

enum { ACC_GRAVE, ACC_ACUTE, ACC_CIRC, ACC_TILDE, ACC_DIAER, ACC_SLASH };

/* Diacritics: 2 rows above capitals (rows 0-1), 3 rows above lowercase (rows 1-3). */
static const u8 acc_upper[5][2] = {
	{0x30,0x18}, {0x0C,0x18}, {0x38,0x6C}, {0x76,0xDC}, {0x6C,0x00},
};
static const u8 acc_lower[5][3] = {
	{0x30,0x18,0x0C}, {0x0C,0x18,0x30}, {0x10,0x38,0x6C}, {0x00,0x76,0xDC}, {0x00,0x6C,0x00},
};

static const struct { u8 latin1; char base; u8 accent; } composed[] = {
	{0xC0,'A',ACC_GRAVE},{0xC1,'A',ACC_ACUTE},{0xC2,'A',ACC_CIRC},{0xC3,'A',ACC_TILDE},
	{0xC8,'E',ACC_GRAVE},{0xCA,'E',ACC_CIRC},{0xCB,'E',ACC_DIAER},
	{0xCC,'I',ACC_GRAVE},{0xCD,'I',ACC_ACUTE},{0xCE,'I',ACC_CIRC},{0xCF,'I',ACC_DIAER},
	{0xD2,'O',ACC_GRAVE},{0xD3,'O',ACC_ACUTE},{0xD4,'O',ACC_CIRC},{0xD5,'O',ACC_TILDE},
	{0xD8,'O',ACC_SLASH},
	{0xD9,'U',ACC_GRAVE},{0xDA,'U',ACC_ACUTE},{0xDB,'U',ACC_CIRC},{0xDD,'Y',ACC_ACUTE},
	{0xE3,'a',ACC_TILDE},{0xF5,'o',ACC_TILDE},{0xF8,'o',ACC_SLASH},{0xFD,'y',ACC_ACUTE},
};

static const u8 glyph_euro[16] = {
	0,0,0x1E,0x33,0x60,0xFC,0x60,0xFC,0x60,0x60,0x33,0x1E,0,0,0,0
};

static void compose(u8 rows[16], char base, int accent)
{
	bool upper = base >= 'A' && base <= 'Z';
	int r;

	memcpy(rows, &console_font_8x16[(u8)base * 16], 16);

	if (accent == ACC_SLASH) {
		int top = upper ? 2 : 5, bottom = 11;
		for (r = top; r <= bottom; r++)
			rows[r] |= 0x80 >> (7 - (r - top) * 7 / (bottom - top));
		return;
	}
	if (upper) {
		rows[0] |= acc_upper[accent][0];
		rows[1] |= acc_upper[accent][1];
	} else {
		/* drop the dot of 'i' and anything else above the x-height */
		for (r = 0; r < 5; r++)
			rows[r] = 0;
		for (r = 0; r < 3; r++)
			rows[1 + r] |= acc_lower[accent][r];
	}
}

static bool glyph_rows(unsigned slot, u8 rows[16])
{
	unsigned i;

	memset(rows, 0, 16);

	if (slot == SLOT_SOLID) {
		memset(rows, 0xFF, 16);
		return false;
	}
	if (slot < 0x20) {
		/* CP437 pictographs (arrows) used for key labels */
		memcpy(rows, &console_font_8x16[slot * 16], 16);
		return false;
	}
	if (slot < 0x7F) {
		memcpy(rows, &console_font_8x16[slot * 16], 16);
		return true;
	}
	if (slot == SLOT_EURO) {
		memcpy(rows, glyph_euro, 16);
		return false;	/* availability of U+20AC is handled separately */
	}
	if (slot < 0xA0)
		return false;

	for (i = 0; i < sizeof(cp437_map) / sizeof(cp437_map[0]); i++) {
		if (cp437_map[i].latin1 == slot) {
			memcpy(rows, &console_font_8x16[cp437_map[i].cp437 * 16], 16);
			return true;
		}
	}
	for (i = 0; i < sizeof(composed) / sizeof(composed[0]); i++) {
		if (composed[i].latin1 == slot) {
			compose(rows, composed[i].base, composed[i].accent);
			return true;
		}
	}

	switch (slot) {
	case 0xA6:	/* broken bar */
		rows[2] = rows[3] = rows[4] = rows[5] = 0x18;
		rows[8] = rows[9] = rows[10] = rows[11] = 0x18;
		return true;
	case 0xA8:	/* diaeresis */
		rows[3] = 0x6C;
		return true;
	case 0xAF:	/* macron */
		rows[2] = 0x7E;
		return true;
	case 0xB4:	/* acute accent */
		rows[2] = 0x0C; rows[3] = 0x18; rows[4] = 0x30;
		return true;
	case 0xD0:	/* Eth: D with a bar */
		memcpy(rows, &console_font_8x16['D' * 16], 16);
		rows[6] |= 0xF0;
		return true;
	case 0xD7:	/* multiplication sign */
		rows[6] = 0xC6; rows[7] = 0x6C; rows[8] = 0x38; rows[9] = 0x6C; rows[10] = 0xC6;
		return true;
	}
	return false;
}

/* GX I8 textures are stored as 8x4 texel tiles. */
static inline u32 i8_offset(u32 x, u32 y)
{
	return ((y >> 2) * (ATLAS_W >> 3) + (x >> 3)) * 32 + ((y & 3) << 3) + (x & 7);
}

bool osk_font_init(void)
{
	unsigned slot, r, c;
	u8 rows[16];

	if (atlas)
		return true;

	atlas = memalign(32, ATLAS_W * ATLAS_H);
	if (!atlas)
		return false;
	memset(atlas, 0, ATLAS_W * ATLAS_H);

	for (slot = 0; slot < 256; slot++) {
		u32 gx = (slot % ATLAS_COLS) * OSK_GLYPH_W;
		u32 gy = (slot / ATLAS_COLS) * OSK_GLYPH_H;

		avail[slot] = glyph_rows(slot, rows);
		for (r = 0; r < OSK_GLYPH_H; r++)
			for (c = 0; c < OSK_GLYPH_W; c++)
				if (rows[r] & (0x80 >> c))
					atlas[i8_offset(gx + c, gy + r)] = 0xFF;
	}
	/* NBSP is accepted but not offered on any key */
	avail[0xA0] = true;

	DCFlushRange(atlas, ATLAS_W * ATLAS_H);
	GX_InitTexObj(&atlas_obj, atlas, ATLAS_W, ATLAS_H, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&atlas_obj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
	return true;
}

GXTexObj *osk_font_texture(void)
{
	return &atlas_obj;
}

unsigned osk_font_slot(uint32_t cp)
{
	if (cp == 0x20AC)
		return SLOT_EURO;
	if (cp < 0x20)
		return cp ? cp : SLOT_INVALID;	/* pictographs; slot 0 is the solid block */
	if (cp < 256 && (avail[cp] || !atlas))
		return cp == 0xA0 ? ' ' : cp;
	return SLOT_INVALID;
}

bool OSK_CanDisplay(uint32_t cp)
{
	if (cp == 0x20AC)
		return true;
	if (cp < 0x20 || cp >= 256)
		return false;
	if (!atlas)
		osk_font_init();
	return avail[cp];
}
