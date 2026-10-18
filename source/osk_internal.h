/*-------------------------------------------------------------
osk_internal.h -- private declarations shared by the wiiosk sources
-------------------------------------------------------------*/
#ifndef __OSK_INTERNAL_H__
#define __OSK_INTERNAL_H__

#include "wiiosk.h"

/* Design space: everything is laid out for a 640x480 canvas which is
   centered in the virtual screen. */
#define OSK_DESIGN_W		640
#define OSK_DESIGN_H		480

#define OSK_MAX_KEYS		80

#define OSK_REPEAT_DELAY	24	/* frames before auto-repeat starts */
#define OSK_REPEAT_RATE		4	/* frames between repeats */

typedef enum {
	KEY_CHAR = 0,
	KEY_BACK,
	KEY_SHIFT,
	KEY_CAPS,
	KEY_ALT,
	KEY_SPACE,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_OK,
	KEY_CANCEL
} key_kind;

/* Page index: [alt][shift]. */
typedef struct {
	float x, y, w, h;		/* design-space rectangle */
	key_kind kind;
	uint32_t cp[2][2];
} osk_key;

/* One layout definition: character rows plus which special keys exist. */
typedef struct {
	int nkeys;
	osk_key keys[OSK_MAX_KEYS];
	bool has_alt;
} osk_keyset;

struct osk_keyboard {
	osk_config cfg;
	osk_theme theme;

	uint32_t text[OSK_MAX_CHARS];
	unsigned len;
	unsigned caret;
	unsigned max_chars;
	char utf8[OSK_MAX_CHARS * 4 + 1];
	bool utf8_dirty;

	osk_keyset keys;
	int focus;			/* index into keys, -1 = none */
	bool shift, caps, alt;

	osk_status status;

	/* virtual screen */
	float vw, vh, ox, oy;

	/* pointer */
	bool pointer_valid;
	float px, py;			/* design space */
	float last_px, last_py;

	/* animation */
	unsigned frame;
	int flash_key;
	int flash_time;
	int reject_time;
	unsigned scroll;		/* first visible char in text box */

	/* auto repeat counters, one per OSK_BTN_* bit */
	u16 repeat[16];
};

/* osk_core.c */
bool osk_accepts(const osk_keyboard *osk, uint32_t cp);

/* osk_layouts.c */
void osk_build_keys(osk_keyboard *osk);
uint32_t osk_key_char(const osk_keyboard *osk, const osk_key *k);
const char *osk_key_label(const osk_keyboard *osk, const osk_key *k);

/* osk_font.c */
#define OSK_GLYPH_W	8
#define OSK_GLYPH_H	16
bool osk_font_init(void);
GXTexObj *osk_font_texture(void);
/* atlas slot for a code point, falls back to '?' */
unsigned osk_font_slot(uint32_t cp);

/* osk_render.c */
void osk_render(osk_keyboard *osk);
/* text box geometry, shared with input hit testing */
void osk_textbox_rect(const osk_keyboard *osk, float *x, float *y, float *w, float *h);
unsigned osk_textbox_visible_chars(const osk_keyboard *osk);
#define OSK_TEXT_SCALE	2.0f

#endif /* __OSK_INTERNAL_H__ */
