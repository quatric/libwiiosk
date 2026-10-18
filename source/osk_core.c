/*-------------------------------------------------------------
osk_core.c -- keyboard state, text editing and input handling
-------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wiiuse/wpad.h>

#include "osk_internal.h"

/* Set by OSK_InitUSBKeyboard() (osk_usbkbd.c) so that apps which do not
   use a USB keyboard need not link libwiikeyboard. */
void (*osk_usb_poll)(osk_input *in) = NULL;

static const osk_theme default_theme = {
	.overlay		= 0x000000A0,
	.panel			= 0xE8ECF0F4,
	.key_top		= 0xFFFFFFFF,
	.key_bottom		= 0xDCE3EAFF,
	.special_top		= 0xD2DAE2FF,
	.special_bottom		= 0xB0BCC8FF,
	.focus_top		= 0xA8E6FFFF,
	.focus_bottom		= 0x38B4E8FF,
	.active			= 0x38B4E8FF,
	.key_text		= 0x303840FF,
	.key_text_disabled	= 0xB0B8C0FF,
	.textbox		= 0xFFFFFFFF,
	.textbox_border		= 0x38B4E8FF,
	.text			= 0x202428FF,
	.caret			= 0x38B4E8FF,
	.title			= 0x303840FF,
	.hint			= 0x707A86FF,
};

const osk_theme *OSK_DefaultTheme(void)
{
	return &default_theme;
}

void OSK_DefaultConfig(osk_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->max_chars = 64;
	cfg->layout = OSK_LAYOUT_QWERTY;
	cfg->screen_width = 640;
	cfg->screen_height = 480;
}

/*-------------------------------------------------------------
  UTF-8
-------------------------------------------------------------*/
size_t OSK_UTF8Encode(uint32_t cp, char out[4])
{
	if (cp < 0x80) {
		out[0] = cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = 0xC0 | (cp >> 6);
		out[1] = 0x80 | (cp & 0x3F);
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = 0xE0 | (cp >> 12);
		out[1] = 0x80 | ((cp >> 6) & 0x3F);
		out[2] = 0x80 | (cp & 0x3F);
		return 3;
	}
	out[0] = 0xF0 | ((cp >> 18) & 0x07);
	out[1] = 0x80 | ((cp >> 12) & 0x3F);
	out[2] = 0x80 | ((cp >> 6) & 0x3F);
	out[3] = 0x80 | (cp & 0x3F);
	return 4;
}

size_t OSK_UTF8Decode(const char *str, uint32_t *cp)
{
	const u8 *s = (const u8 *)str;
	size_t n, i;
	uint32_t c;

	if (!s || !s[0])
		return 0;
	if (s[0] < 0x80) {
		*cp = s[0];
		return 1;
	}
	if ((s[0] & 0xE0) == 0xC0) {
		n = 2; c = s[0] & 0x1F;
	} else if ((s[0] & 0xF0) == 0xE0) {
		n = 3; c = s[0] & 0x0F;
	} else if ((s[0] & 0xF8) == 0xF0) {
		n = 4; c = s[0] & 0x07;
	} else {
		/* stray byte: treat as Latin-1 */
		*cp = s[0];
		return 1;
	}
	for (i = 1; i < n; i++) {
		if ((s[i] & 0xC0) != 0x80) {
			*cp = s[0];
			return 1;
		}
		c = (c << 6) | (s[i] & 0x3F);
	}
	*cp = c;
	return n;
}

/*-------------------------------------------------------------
  helpers
-------------------------------------------------------------*/
static void emit(osk_keyboard *osk, osk_event ev)
{
	if (osk->cfg.on_event)
		osk->cfg.on_event(osk, ev, osk->cfg.user);
}

static void reject(osk_keyboard *osk)
{
	osk->reject_time = 12;
	emit(osk, OSK_EVENT_REJECT);
}

bool osk_accepts(const osk_keyboard *osk, uint32_t cp)
{
	if (!cp || !OSK_CanDisplay(cp))
		return false;
	if (osk->cfg.filter && !osk->cfg.filter(cp, osk->cfg.user))
		return false;
	return true;
}

static void ensure_caret_visible(osk_keyboard *osk)
{
	unsigned visible = osk_textbox_visible_chars(osk);

	if (osk->caret < osk->scroll)
		osk->scroll = osk->caret;
	if (osk->caret >= osk->scroll + visible)
		osk->scroll = osk->caret - visible + 1;
	/* pull back when text got shorter */
	if (osk->len + 1 < osk->scroll + visible)
		osk->scroll = osk->len + 1 > visible ? osk->len + 1 - visible : 0;
}

static bool insert_char(osk_keyboard *osk, uint32_t cp)
{
	if (cp == 0xA0)
		cp = ' ';
	if (osk->len >= osk->max_chars || !osk_accepts(osk, cp)) {
		reject(osk);
		return false;
	}
	memmove(&osk->text[osk->caret + 1], &osk->text[osk->caret],
		(osk->len - osk->caret) * sizeof(uint32_t));
	osk->text[osk->caret++] = cp;
	osk->len++;
	osk->utf8_dirty = true;
	return true;
}

static void backspace(osk_keyboard *osk)
{
	if (!osk->caret) {
		reject(osk);
		return;
	}
	memmove(&osk->text[osk->caret - 1], &osk->text[osk->caret],
		(osk->len - osk->caret) * sizeof(uint32_t));
	osk->caret--;
	osk->len--;
	osk->utf8_dirty = true;
}

static void delete_char(osk_keyboard *osk)
{
	if (osk->caret >= osk->len) {
		reject(osk);
		return;
	}
	memmove(&osk->text[osk->caret], &osk->text[osk->caret + 1],
		(osk->len - osk->caret - 1) * sizeof(uint32_t));
	osk->len--;
	osk->utf8_dirty = true;
}

static void confirm(osk_keyboard *osk)
{
	if ((osk->cfg.flags & OSK_FLAG_REQUIRE_TEXT) && !osk->len) {
		reject(osk);
		return;
	}
	osk->status = OSK_STATUS_DONE;
	emit(osk, OSK_EVENT_DONE);
}

static void cancel(osk_keyboard *osk)
{
	if (osk->cfg.flags & OSK_FLAG_NO_CANCEL)
		return;
	osk->status = OSK_STATUS_CANCELLED;
	emit(osk, OSK_EVENT_CANCEL);
}

static void set_focus(osk_keyboard *osk, int key)
{
	if (key == osk->focus)
		return;
	osk->focus = key;
	if (key >= 0)
		emit(osk, OSK_EVENT_FOCUS);
}

static int key_at(const osk_keyboard *osk, float x, float y)
{
	int i;

	for (i = 0; i < osk->keys.nkeys; i++) {
		const osk_key *k = &osk->keys.keys[i];
		if (x >= k->x && x < k->x + k->w && y >= k->y && y < k->y + k->h)
			return i;
	}
	return -1;
}

static int default_key(const osk_keyboard *osk)
{
	int i;

	/* first character key of the second row, else the first key */
	for (i = 0; i < osk->keys.nkeys; i++)
		if (osk->keys.keys[i].y > osk->keys.keys[0].y)
			return i;
	return 0;
}

static void move_focus(osk_keyboard *osk, int dx, int dy)
{
	const osk_key *cur, *k;
	float cx, cy, kx, ky, best = 1e9f, score;
	int i, found = -1;

	if (osk->focus < 0) {
		set_focus(osk, default_key(osk));
		return;
	}
	cur = &osk->keys.keys[osk->focus];
	cx = cur->x + cur->w / 2;
	cy = cur->y + cur->h / 2;

	for (i = 0; i < osk->keys.nkeys; i++) {
		if (i == osk->focus)
			continue;
		k = &osk->keys.keys[i];
		kx = k->x + k->w / 2;
		ky = k->y + k->h / 2;
		if (dx) {
			if (fabsf(ky - cy) > 1.0f || (kx - cx) * dx <= 0)
				continue;
			score = fabsf(kx - cx);
		} else {
			if ((ky - cy) * dy <= 1.0f)
				continue;
			/* nearest row first, then the key closest to the current column */
			score = fabsf(ky - cy) * 1000.0f;
			if (cx < k->x || cx >= k->x + k->w)
				score += fabsf(kx - cx);
		}
		if (score < best) {
			best = score;
			found = i;
		}
	}

	if (found < 0 && dx) {
		/* wrap within the row */
		for (i = 0; i < osk->keys.nkeys; i++) {
			k = &osk->keys.keys[i];
			kx = k->x + k->w / 2;
			ky = k->y + k->h / 2;
			if (fabsf(ky - cy) > 1.0f)
				continue;
			score = kx * dx;	/* LEFT wraps to the rightmost key and vice versa */
			if (found < 0 || score < best) {
				best = score;
				found = i;
			}
		}
	}
	if (found >= 0)
		set_focus(osk, found);
}

static void press_key(osk_keyboard *osk, int idx, bool repeat)
{
	const osk_key *k = &osk->keys.keys[idx];

	if (repeat && k->kind != KEY_BACK && k->kind != KEY_LEFT && k->kind != KEY_RIGHT)
		return;

	osk->flash_key = idx;
	osk->flash_time = 8;
	emit(osk, OSK_EVENT_PRESS);

	switch (k->kind) {
	case KEY_CHAR:
		if (insert_char(osk, osk_key_char(osk, k)))
			osk->shift = false;
		break;
	case KEY_SPACE:
		insert_char(osk, ' ');
		break;
	case KEY_BACK:
		backspace(osk);
		break;
	case KEY_SHIFT:
		osk->shift = !osk->shift;
		break;
	case KEY_CAPS:
		osk->caps = !osk->caps;
		break;
	case KEY_ALT:
		osk->alt = !osk->alt;
		break;
	case KEY_LEFT:
		if (osk->caret)
			osk->caret--;
		break;
	case KEY_RIGHT:
		if (osk->caret < osk->len)
			osk->caret++;
		break;
	case KEY_OK:
		confirm(osk);
		break;
	case KEY_CANCEL:
		cancel(osk);
		break;
	}
}

static bool pointer_in_textbox(const osk_keyboard *osk, unsigned *index)
{
	float x, y, w, h;
	int col;

	osk_textbox_rect(osk, &x, &y, &w, &h);
	if (osk->px < x || osk->px >= x + w || osk->py < y || osk->py >= y + h)
		return false;
	col = (int)((osk->px - x - 12.0f) / (OSK_GLYPH_W * OSK_TEXT_SCALE) + 0.5f);
	if (col < 0)
		col = 0;
	*index = osk->scroll + col;
	if (*index > osk->len)
		*index = osk->len;
	return true;
}

/*-------------------------------------------------------------
  public API
-------------------------------------------------------------*/
osk_keyboard *OSK_Create(const osk_config *cfg)
{
	osk_keyboard *osk;
	osk_config defaults;

	if (!cfg) {
		OSK_DefaultConfig(&defaults);
		cfg = &defaults;
	}
	if (!osk_font_init())
		return NULL;

	osk = calloc(1, sizeof(*osk));
	if (!osk)
		return NULL;

	osk->cfg = *cfg;
	osk->theme = cfg->theme ? *cfg->theme : default_theme;
	osk->max_chars = cfg->max_chars && cfg->max_chars <= OSK_MAX_CHARS ? cfg->max_chars : OSK_MAX_CHARS;
	if (!osk->cfg.screen_width)
		osk->cfg.screen_width = 640;
	if (!osk->cfg.screen_height)
		osk->cfg.screen_height = 480;

	/* The virtual canvas is always 480 lines tall and as wide as needed for
	   square pixels on the display; the design area is centered in it. */
	osk->vh = OSK_DESIGN_H;
	osk->vw = (cfg->flags & OSK_FLAG_WIDESCREEN) ? OSK_DESIGN_W * 4.0f / 3.0f : OSK_DESIGN_W;
	osk->ox = (osk->vw - OSK_DESIGN_W) / 2;
	osk->oy = 0;

	osk->focus = -1;
	osk->flash_key = -1;
	osk_build_keys(osk);
	osk->focus = default_key(osk);

	OSK_SetText(osk, cfg->initial);
	return osk;
}

void OSK_Destroy(osk_keyboard *osk)
{
	free(osk);
}

void OSK_SetText(osk_keyboard *osk, const char *utf8)
{
	uint32_t cp;
	size_t n;

	osk->len = 0;
	while (utf8 && (n = OSK_UTF8Decode(utf8, &cp)) != 0) {
		utf8 += n;
		if (cp == 0xA0)
			cp = ' ';
		if (osk->len >= osk->max_chars)
			break;
		if (!OSK_CanDisplay(cp))
			cp = '?';
		osk->text[osk->len++] = cp;
	}
	osk->caret = osk->len;
	osk->scroll = 0;
	osk->utf8_dirty = true;
	ensure_caret_visible(osk);
}

const char *OSK_GetText(osk_keyboard *osk)
{
	unsigned i;
	size_t pos = 0;

	if (osk->utf8_dirty) {
		for (i = 0; i < osk->len; i++)
			pos += OSK_UTF8Encode(osk->text[i], &osk->utf8[pos]);
		osk->utf8[pos] = 0;
		osk->utf8_dirty = false;
	}
	return osk->utf8;
}

size_t OSK_CopyText(osk_keyboard *osk, char *buf, size_t size)
{
	unsigned i;
	size_t pos = 0, n;
	char tmp[4];

	if (!buf || !size)
		return 0;
	for (i = 0; i < osk->len; i++) {
		n = OSK_UTF8Encode(osk->text[i], tmp);
		if (pos + n >= size)
			break;
		memcpy(&buf[pos], tmp, n);
		pos += n;
	}
	buf[pos] = 0;
	return pos;
}

unsigned OSK_GetLength(const osk_keyboard *osk)
{
	return osk->len;
}

osk_status OSK_GetStatus(const osk_keyboard *osk)
{
	return osk->status;
}

void OSK_Resume(osk_keyboard *osk)
{
	osk->status = OSK_STATUS_EDITING;
}

#define REPEATABLE	(OSK_BTN_SELECT | OSK_BTN_BACKSPACE | OSK_BTN_DELETE | \
			 OSK_BTN_UP | OSK_BTN_DOWN | OSK_BTN_LEFT | OSK_BTN_RIGHT | \
			 OSK_BTN_CARET_LEFT | OSK_BTN_CARET_RIGHT)

osk_status OSK_Update(osk_keyboard *osk, const osk_input *in)
{
	u32 fire = 0, repeated = 0;
	unsigned i, idx;
	osk_input none;

	if (osk->status != OSK_STATUS_EDITING)
		return osk->status;
	if (!in) {
		memset(&none, 0, sizeof(none));
		in = &none;
	}

	osk->frame++;
	if (osk->flash_time > 0)
		osk->flash_time--;
	if (osk->reject_time > 0)
		osk->reject_time--;

	/* pointer */
	if (in->pointer_valid) {
		float px = in->pointer_x * osk->vw / osk->cfg.screen_width - osk->ox;
		float py = in->pointer_y * osk->vh / osk->cfg.screen_height - osk->oy;
		bool moved = !osk->pointer_valid ||
			     fabsf(px - osk->last_px) > 1.5f || fabsf(py - osk->last_py) > 1.5f;

		osk->px = px;
		osk->py = py;
		if (moved) {
			int hit = key_at(osk, px, py);
			if (hit != osk->focus)
				set_focus(osk, hit);
			osk->last_px = px;
			osk->last_py = py;
		}
	}
	osk->pointer_valid = in->pointer_valid;

	/* auto repeat */
	for (i = 0; i < 16; i++) {
		u32 bit = 1u << i;
		if (in->down & bit) {
			osk->repeat[i] = 0;
			fire |= bit;
		} else if (in->held & bit) {
			if (osk->repeat[i] < 0xFFFF)
				osk->repeat[i]++;
			if ((REPEATABLE & bit) && osk->repeat[i] >= OSK_REPEAT_DELAY &&
			    (osk->repeat[i] - OSK_REPEAT_DELAY) % OSK_REPEAT_RATE == 0) {
				fire |= bit;
				repeated |= bit;
			}
		} else {
			osk->repeat[i] = 0;
		}
	}

	for (i = 0; i < in->text_count && i < OSK_INPUT_MAX_TEXT; i++)
		insert_char(osk, in->text[i]);

	if (fire & OSK_BTN_SELECT) {
		if (!(repeated & OSK_BTN_SELECT) && in->pointer_valid && pointer_in_textbox(osk, &idx))
			osk->caret = idx;
		else if (osk->focus >= 0)
			press_key(osk, osk->focus, (repeated & OSK_BTN_SELECT) != 0);
	}
	if (fire & OSK_BTN_BACKSPACE)	backspace(osk);
	if (fire & OSK_BTN_DELETE)	delete_char(osk);
	if ((fire & OSK_BTN_CARET_LEFT) && osk->caret)
		osk->caret--;
	if ((fire & OSK_BTN_CARET_RIGHT) && osk->caret < osk->len)
		osk->caret++;
	if (fire & OSK_BTN_CARET_HOME)	osk->caret = 0;
	if (fire & OSK_BTN_CARET_END)	osk->caret = osk->len;
	if (fire & OSK_BTN_SHIFT)	osk->shift = !osk->shift;
	if ((fire & OSK_BTN_ALT) && osk->keys.has_alt)
		osk->alt = !osk->alt;
	if (fire & OSK_BTN_SPACE)	insert_char(osk, ' ');
	if (fire & OSK_BTN_UP)		move_focus(osk, 0, -1);
	if (fire & OSK_BTN_DOWN)	move_focus(osk, 0, 1);
	if (fire & OSK_BTN_LEFT)	move_focus(osk, -1, 0);
	if (fire & OSK_BTN_RIGHT)	move_focus(osk, 1, 0);

	if (osk->status == OSK_STATUS_EDITING) {
		if (fire & OSK_BTN_CONFIRM)
			confirm(osk);
		else if (fire & OSK_BTN_CANCEL)
			cancel(osk);
	}

	ensure_caret_visible(osk);
	return osk->status;
}

/*-------------------------------------------------------------
  controller input
-------------------------------------------------------------*/
static const struct { u32 from, to; } wpad_map[] = {
	{ WPAD_BUTTON_A,		OSK_BTN_SELECT },
	{ WPAD_BUTTON_B,		OSK_BTN_BACKSPACE },
	{ WPAD_BUTTON_UP,		OSK_BTN_UP },
	{ WPAD_BUTTON_DOWN,		OSK_BTN_DOWN },
	{ WPAD_BUTTON_LEFT,		OSK_BTN_LEFT },
	{ WPAD_BUTTON_RIGHT,		OSK_BTN_RIGHT },
	{ WPAD_BUTTON_PLUS,		OSK_BTN_CONFIRM },
	{ WPAD_BUTTON_MINUS,		OSK_BTN_SHIFT },
	{ WPAD_BUTTON_1,		OSK_BTN_CARET_LEFT },
	{ WPAD_BUTTON_2,		OSK_BTN_CARET_RIGHT },
	{ WPAD_BUTTON_HOME,		OSK_BTN_CANCEL },
	{ WPAD_CLASSIC_BUTTON_A,	OSK_BTN_SELECT },
	{ WPAD_CLASSIC_BUTTON_B,	OSK_BTN_BACKSPACE },
	{ WPAD_CLASSIC_BUTTON_UP,	OSK_BTN_UP },
	{ WPAD_CLASSIC_BUTTON_DOWN,	OSK_BTN_DOWN },
	{ WPAD_CLASSIC_BUTTON_LEFT,	OSK_BTN_LEFT },
	{ WPAD_CLASSIC_BUTTON_RIGHT,	OSK_BTN_RIGHT },
	{ WPAD_CLASSIC_BUTTON_PLUS,	OSK_BTN_CONFIRM },
	{ WPAD_CLASSIC_BUTTON_MINUS,	OSK_BTN_SHIFT },
	{ WPAD_CLASSIC_BUTTON_FULL_L,	OSK_BTN_CARET_LEFT },
	{ WPAD_CLASSIC_BUTTON_FULL_R,	OSK_BTN_CARET_RIGHT },
	{ WPAD_CLASSIC_BUTTON_X,	OSK_BTN_SPACE },
	{ WPAD_CLASSIC_BUTTON_Y,	OSK_BTN_ALT },
	{ WPAD_CLASSIC_BUTTON_HOME,	OSK_BTN_CANCEL },
};

static const struct { u32 from, to; } pad_map[] = {
	{ PAD_BUTTON_A,		OSK_BTN_SELECT },
	{ PAD_BUTTON_B,		OSK_BTN_BACKSPACE },
	{ PAD_BUTTON_UP,	OSK_BTN_UP },
	{ PAD_BUTTON_DOWN,	OSK_BTN_DOWN },
	{ PAD_BUTTON_LEFT,	OSK_BTN_LEFT },
	{ PAD_BUTTON_RIGHT,	OSK_BTN_RIGHT },
	{ PAD_BUTTON_START,	OSK_BTN_CONFIRM },
	{ PAD_BUTTON_Y,		OSK_BTN_SHIFT },
	{ PAD_BUTTON_X,		OSK_BTN_SPACE },
	{ PAD_TRIGGER_Z,	OSK_BTN_ALT },
	{ PAD_TRIGGER_L,	OSK_BTN_CARET_LEFT },
	{ PAD_TRIGGER_R,	OSK_BTN_CARET_RIGHT },
};

#define MAP_BUTTONS(table, bits) ({ \
	u32 _r = 0; unsigned _i; \
	for (_i = 0; _i < sizeof(table) / sizeof(table[0]); _i++) \
		if ((bits) & table[_i].from) _r |= table[_i].to; \
	_r; })

#define STICK_THRESHOLD	48

static u32 stick_prev[4];

void OSK_ReadInput(osk_input *in, int wpad_chan, int pad_chan)
{
	memset(in, 0, sizeof(*in));

	if (wpad_chan >= 0) {
		WPADData *wd = WPAD_Data(wpad_chan);

		in->down |= MAP_BUTTONS(wpad_map, WPAD_ButtonsDown(wpad_chan));
		in->held |= MAP_BUTTONS(wpad_map, WPAD_ButtonsHeld(wpad_chan));
		if (wd && wd->ir.valid) {
			in->pointer_valid = true;
			in->pointer_x = wd->ir.x;
			in->pointer_y = wd->ir.y;
		}
	}

	if (pad_chan >= 0 && pad_chan < 4) {
		s8 sx = PAD_StickX(pad_chan), sy = PAD_StickY(pad_chan);
		u32 stick = 0;

		in->down |= MAP_BUTTONS(pad_map, PAD_ButtonsDown(pad_chan));
		in->held |= MAP_BUTTONS(pad_map, PAD_ButtonsHeld(pad_chan));

		if (sx < -STICK_THRESHOLD) stick |= OSK_BTN_LEFT;
		if (sx > STICK_THRESHOLD)  stick |= OSK_BTN_RIGHT;
		if (sy > STICK_THRESHOLD)  stick |= OSK_BTN_UP;
		if (sy < -STICK_THRESHOLD) stick |= OSK_BTN_DOWN;
		in->held |= stick;
		in->down |= stick & ~stick_prev[pad_chan];
		stick_prev[pad_chan] = stick;
	}

	if (osk_usb_poll)
		osk_usb_poll(in);
}
