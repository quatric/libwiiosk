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

#define OSK_MAX_KEYS		96

#define OSK_REPEAT_DELAY	24	/* frames before auto-repeat starts */
#define OSK_REPEAT_RATE		4	/* frames between repeats */

#define OSK_TAP_FRAMES		90	/* a multi-tap letter is confirmed this long after the last tap */

/* key animation lengths in frames */
#define OSK_FOCUS_IN_FRAMES	5
#define OSK_FOCUS_OUT_FRAMES	8
#define OSK_PUSH_FRAMES		20

/* symbol window: zooms in, out, and slides between pages */
#define OSK_SYMBOL_OPEN_FRAMES	18
#define OSK_SYMBOL_CLOSE_FRAMES	13
#define OSK_SYMBOL_SLIDE_FRAMES	20

typedef enum {
	KEY_CHAR = 0,
	KEY_BACK,
	KEY_ENTER,
	KEY_SHIFT,
	KEY_CAPS,
	KEY_SPACE,
	KEY_ALT,		/* "More": toggles the accent / symbol page */
	KEY_OK,
	KEY_CANCEL,
	KEY_TAB_LETTERS,	/* layout tabs in the toolbar */
	KEY_TAB_PHONE,
	KEY_PREDICT,		/* toggles word prediction */
	KEY_SCROLL_L,		/* candidate bar */
	KEY_SCROLL_R,
	KEY_CANDIDATE,
	KEY_PHONE,		/* cellphone keypad key, arg = slot 0-11 */
	KEY_PHONE_MODE,		/* Abc / abc / ABC / 123 tag, arg = phone_mode */
	KEY_SYMBOLS,		/* opens the symbol window */
	KEY_SYMBOL,		/* symbol window key, arg = slot 0-19 */
	KEY_SYMBOL_PREV,
	KEY_SYMBOL_NEXT,
	KEY_SYMBOL_CLOSE
} key_kind;

/* input modes of the cellphone keypad, in the order of its tags */
typedef enum {
	PHONE_ABC = 0,		/* capital at the start of a sentence */
	PHONE_LOWER,
	PHONE_UPPER,
	PHONE_DIGITS,
	PHONE_MODES
} phone_mode;

#define OSK_PHONE_SLOTS		12	/* 3 x 4 grid, in reading order */
#define OSK_SYMBOL_SLOTS	20	/* 5 x 4 grid of the symbol window */

/* How a key is drawn. */
typedef enum {
	LOOK_CHAR = 0,		/* small glass key */
	LOOK_SPECIAL,		/* blue glass key (Delete, Enter, Shift, Caps) */
	LOOK_SPACE,
	LOOK_MORE,
	LOOK_PILL,		/* Quit / OK */
	LOOK_TAB,
	LOOK_PAD,		/* cellphone keypad key */
	LOOK_PAD_BLUE,		/* its prediction and symbol keys */
	LOOK_MODE,		/* its input mode tags */
	LOOK_SCROLL,		/* candidate bar arrows */
	LOOK_CANDIDATE,		/* a suggested word */
	LOOK_SYMBOL,		/* symbol window key */
	LOOK_SYMBOL_BUTTON	/* symbol window prev / next / close */
} key_look;

/* Code point index: [alt page][state], state = caps | shift << 1. */
typedef struct {
	float cx, cy, w, h;		/* picture: center and size, design space */
	float hx, hy, hw, hh;		/* hit box: top-left corner and size */
	key_kind kind;
	key_look look;
	uint32_t cp[2][4];
	unsigned arg;			/* KEY_CANDIDATE: index into the candidates;
					   KEY_PHONE, KEY_PHONE_MODE, KEY_SYMBOL: see key_kind */

	/* animation */
	float focus;			/* 0..1 */
	int push;			/* frames since pressed, -1 = idle */
} osk_key;

/* osk_predict.c: a parsed dictionary and the current suggestions */
#define OSK_MAX_CANDIDATES	20
#define OSK_CAND_LEN		32	/* longest word, in code points */

typedef struct {
	bool ok;
	const u8 *zsd;
	u32 zsd_size;
	u32 charset;			/* u16BE code points */
	unsigned nchars;
	u32 graph;			/* root node */
	u32 block;			/* blocklist: length prefixed Latin-1 */
	unsigned nblock;
	const u8 *znd;
	u32 znd_size;
	unsigned nznd;
	const char *name;
} osk_predictor;

typedef struct {
	uint32_t cp[OSK_CAND_LEN];
	u8 len;
} osk_candidate;

struct osk_keyboard {
	osk_config cfg;
	osk_theme theme;

	uint32_t text[OSK_MAX_CHARS];
	unsigned len;
	unsigned caret;
	unsigned max_chars;
	char utf8[OSK_MAX_CHARS * 4 + 1];
	bool utf8_dirty;

	osk_layout letters;		/* letter layout behind the letters tab */
	bool phone;			/* cellphone keypad shown */
	bool numeric;			/* OSK_LAYOUT_NUMPAD, locked: the keypad holds only numbers */
	phone_mode mode;
	bool phone_upper;		/* PHONE_ABC: the next letter is a capital */
	int tap_slot;			/* key of the pending multi-tap letter (at
					   caret - 1), -1 if none */
	unsigned tap_index;		/* its place in the key's sequence */
	unsigned tap_frames;		/* frames since the last tap */
	bool symbols;			/* symbol window open */
	unsigned symbol_page;
	unsigned symbol_anim;		/* frames since it opened or closed */
	unsigned symbol_slide;		/* frames since the page changed */
	unsigned symbol_old_page;
	int symbol_dir;			/* +1: next page, -1: previous */
	int nunder;
	osk_key under[OSK_MAX_KEYS];	/* the page behind the window, drawn still */
	int nkeys;
	osk_key keys[OSK_MAX_KEYS];
	int focus;			/* index into keys, -1 = none */
	bool shift, caps, alt;

	osk_status status;

	/* virtual screen */
	float vw, vh, ox, oy;

	/* pointer */
	bool pointer_valid;
	float px, py;			/* design space */
	float last_px, last_py;

	unsigned frame;
	unsigned scroll;		/* first visible text line */

	/* auto repeat counters, one per OSK_BTN_* bit */
	u16 repeat[16];

	/* word prediction */
	osk_predictor dict;
	bool predict;			/* switched on by the prediction key */
	int nbase;			/* keys before the candidate bar's keys */
	unsigned word_start;		/* the word the candidates were made for */
	uint32_t word[OSK_CAND_LEN];
	unsigned word_len;
	osk_candidate cand[OSK_MAX_CANDIDATES];
	unsigned ncand;
	unsigned cand_first, cand_shown;	/* visible candidates */
};

/* osk_core.c */
bool osk_accepts(const osk_keyboard *osk, uint32_t cp);
/* set by OSK_InitSound() (osk_asnd.c) */
extern void (*osk_sound_hook)(osk_sound sound);

/* osk_sound_data.c (generated by tools/mksound.py) */
typedef struct {
	u32 offset;			/* into the unpacked ADPCM */
	u16 samples, rate;
	u16 jitter_cents;
} osk_sound_clip;

extern const u8 osk_sound_packed[];		/* IMA ADPCM, packed */
extern const unsigned osk_sound_packed_size;
extern const osk_sound_clip osk_sound_clips[OSK_SOUND_COUNT];
/* osk_sound.c: like OSK_GetSound(), but out->pcm is a new buffer that the
   caller frees (or the cached one if OSK_GetSound() already made it;
   *owned tells which) */
bool osk_sound_load(osk_sound sound, osk_sound_data *out, bool *owned);

/* osk_predict.c */
bool osk_dict_parse(osk_predictor *p, const osk_dictionary *d);
/* true while the candidate bar is shown */
bool osk_predict_active(const osk_keyboard *osk);
/* recompute the candidates if the word before the caret changed; true
   if they did */
bool osk_predict_update(osk_keyboard *osk, bool force);
bool osk_is_word_char(uint32_t cp);

/* osk_syslang.c: CONF_GetLanguage(), CONF_GetArea() (negative if
   unknown) and CONF_GetRegion() -> dictionary language */
osk_language osk_conf_language(int lang, int area, int region);

/* osk_text.c: case mapping of the Latin-1 letters and the OE ligature */
uint32_t osk_lower(uint32_t c);
uint32_t osk_upper(uint32_t c);

/* osk_layouts.c */
void osk_build_keys(osk_keyboard *osk);
/* re-add the candidate bar's keys after the base keys */
void osk_build_bar(osk_keyboard *osk);
uint32_t osk_key_char(const osk_keyboard *osk, const osk_key *k);
const char *osk_key_label(const osk_keyboard *osk, const osk_key *k);
bool osk_key_latched(const osk_keyboard *osk, const osk_key *k);
/* The characters a cellphone key cycles through in the current mode and
   case, before the filter; returns their number (at most 16). */
unsigned osk_phone_chars(const osk_keyboard *osk, unsigned slot, uint32_t out[16]);
/* its key top, UTF-8 */
void osk_phone_label(const osk_keyboard *osk, unsigned slot, char out[32]);
/* symbol window pages */
extern const unsigned osk_symbol_pages;
uint32_t osk_symbol(const osk_keyboard *osk, unsigned page, unsigned slot);
/* short dictionary name for the keypad's prediction key ("Eng", "DE") */
const char *osk_dict_short_name(const osk_keyboard *osk);

/* osk_unpack.c: data packed by tools/rcpack.py; malloc()ed, NULL on
   failure */
u8 *osk_unpack(const u8 *in, size_t in_size, u32 *out_size);

/* osk_font_data.c (generated by tools/mkfont.py) */
typedef struct {
	u16 cp;
	u16 x, y;			/* atlas position */
	u8 w, h;
	s8 left, top;			/* bitmap offset from the pen, top is above the baseline */
	u16 adv16;			/* advance in 1/16 pixel */
} osk_glyph;

extern const unsigned osk_font_atlas_w, osk_font_atlas_h;
extern const float osk_font_em, osk_font_cap, osk_font_ascent, osk_font_descent;
extern const u8 osk_font_atlas_packed[];
extern const unsigned osk_font_atlas_packed_size;
extern const osk_glyph osk_font_glyphs[];
extern const unsigned osk_font_nglyphs;

/* osk_text.c: proportional text metrics. Sizes follow the system font: a
   size of (sx, sy) means a 30x36 font cell scaled to sx by sy pixels. */
typedef struct {
	float sx, sy;			/* font size */
	float char_space;
} osk_font_size;

u8 *osk_font_unpack(void);
const osk_glyph *osk_find_glyph(uint32_t cp);
float osk_glyph_scale_y(const osk_font_size *fs);
float osk_char_advance(uint32_t cp, const osk_font_size *fs);
float osk_text_width(const uint32_t *cps, unsigned n, const osk_font_size *fs);
float osk_utf8_width(const char *s, const osk_font_size *fs);

/* candidate bar layout (osk_layouts.c) */
#define OSK_BAR_X0		45.0f	/* candidate text area */
#define OSK_BAR_X1		435.0f
#define OSK_BAR_CY		152.0f
#define OSK_BAR_GAP		20.0f
extern const osk_font_size osk_cand_font;
float osk_cand_width(const osk_candidate *c);

/* text box layout */
#define OSK_TEXT_X		51.0f
#define OSK_TEXT_Y		40.33f
#define OSK_TEXT_W		488.0f
#define OSK_TEXT_LINE_H		37.0f
#define OSK_TEXT_LINES		2
extern const osk_font_size osk_text_font;

/* Breaks the text into lines; line i covers [start[i], start[i + 1]). The
   '\n' ending a line belongs to that line. Returns the number of lines. */
unsigned osk_wrap_text(const osk_keyboard *osk, unsigned start[OSK_MAX_CHARS + 2]);
uint32_t osk_display_char(const osk_keyboard *osk, unsigned index);
/* x offset of the caret position index within its line */
float osk_caret_x(const osk_keyboard *osk, const unsigned *start, unsigned line, unsigned index);
unsigned osk_line_of(const unsigned *start, unsigned nlines, unsigned index);

/* osk_gfx.c: the textures, unpacked from osk_tex_data.c or (built with
   OSK_GENERATED_TEXTURES) drawn procedurally */
typedef enum {
	OSK_TEX_WHITE = 0,
	OSK_TEX_STRIPES,
	OSK_TEX_GLASS_KEY,
	OSK_TEX_GLASS_WIDE,
	OSK_TEX_GLASS_SPACE,
	OSK_TEX_PLATE,
	OSK_TEX_PILL,
	OSK_TEX_TOOL,
	OSK_TEX_ICON_QWERTY,
	OSK_TEX_ICON_NUMPAD,
	OSK_TEX_ICON_BACK,
	OSK_TEX_ICON_ENTER,
	OSK_TEX_ICON_SHIFT,		/* Shift / Caps arrow on European layouts */
	OSK_TEX_ARROW,
	OSK_TEX_FRAME_THICK,
	OSK_TEX_FRAME_THIN,
	OSK_TEX_POINTER,
	OSK_TEX_BOOK_OPEN,
	OSK_TEX_BOOK_CLOSED,
	OSK_TEX_SCROLL,
	OSK_TEX_ROUND,
	OSK_TEX_WINDOW_PLATE,		/* symbol window buttons */
	OSK_TEX_FADE_BAR,		/* line under the keypad's dictionary name */
	OSK_TEX_IMAGES,			/* number of generated images */
	OSK_TEX_FONT = OSK_TEX_IMAGES	/* the glyph atlas, handled by the backend */
} osk_tex;

/* straight-alpha RGBA8, row major */
typedef struct {
	u16 w, h;
	u8 *rgba;
} osk_image;

/* osk_tex_data.c (generated by tools/mktex.py): every texture is a run
   of 4-bit planes, osk_tex_plane_w wide, starting at plane row `row` */
typedef enum {
	OSK_TEXFMT_NONE = 0,
	OSK_TEXFMT_I4,			/* I */
	OSK_TEXFMT_IA4,			/* I, A */
	OSK_TEXFMT_IA8,			/* I high, I low, A high, A low */
	OSK_TEXFMT_RGBA			/* R, G, B, A, each high then low */
} osk_texfmt;

typedef struct {
	u16 w, h;
	u8 fmt;				/* osk_texfmt */
	u16 row;
} osk_tex_source;

extern const unsigned osk_tex_plane_w;
extern const osk_tex_source osk_tex_sources[OSK_TEX_IMAGES];
extern const u8 osk_tex_packed[];
extern const unsigned osk_tex_packed_size;

bool osk_gfx_generate(osk_image images[OSK_TEX_IMAGES]);
void osk_gfx_free(osk_image images[OSK_TEX_IMAGES]);

/* Drawing backend (osk_gx.c on the Wii). Every quad is axis aligned in
   virtual screen coordinates and colored like a layout picture:
   color = lerp(black, white, texel) * vertex color, per channel. */
typedef struct {
	u8 r, g, b, a;
} osk_color;

bool osk_backend_init(void);
void osk_backend_begin(const osk_keyboard *osk);
/* vtx: top-left, top-right, bottom-right, bottom-left */
void osk_backend_quad(osk_tex tex, float x0, float y0, float x1, float y1,
		      float s0, float t0, float s1, float t1,
		      osk_color black, osk_color white, const osk_color vtx[4]);
void osk_backend_end(void);

/* osk_render.c */
void osk_render(osk_keyboard *osk);

#endif /* __OSK_INTERNAL_H__ */
