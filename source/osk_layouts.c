/*-------------------------------------------------------------
osk_layouts.c -- key arrangements
-------------------------------------------------------------*/
#include <string.h>

#include "osk_internal.h"

/* Character rows of a letter layout: normal and shifted strings (UTF-8).
   Row lengths are fixed: 13, 13, 11, 10 keys. */
typedef struct {
	const char *normal[4];
	const char *shifted[4];
} letter_rows;

static const letter_rows rows_qwerty = {
	{ "`1234567890-=", "qwertyuiop[]\\", "asdfghjkl;'", "zxcvbnm,./" },
	{ "~!@#$%^&*()_+", "QWERTYUIOP{}|", "ASDFGHJKL:\"", "ZXCVBNM<>?" },
};

static const letter_rows rows_qwertz = {
	{ "^1234567890\xC3\x9F'", "qwertzuiop\xC3\xBC+#", "asdfghjkl\xC3\xB6\xC3\xA4", "yxcvbnm,.-" },
	{ "\xC2\xB0!\"\xC2\xA7$%&/()=?`", "QWERTZUIOP\xC3\x9C*'", "ASDFGHJKL\xC3\x96\xC3\x84", "YXCVBNM;:_" },
};

static const letter_rows rows_azerty = {
	{ "\xC2\xB2&\xC3\xA9\"'(-\xC3\xA8_\xC3\xA7\xC3\xA0)=", "azertyuiop^$*", "qsdfghjklm\xC3\xB9", "wxcvbn,;:!" },
	{ "~1234567890\xC2\xB0+", "AZERTYUIOP\xC2\xA8\xC2\xA3\xC2\xB5", "QSDFGHJKLM%", "WXCVBN?./\xC2\xA7" },
};

/* Alt page, shared by all letter layouts: accented letters and extra symbols. */
static const letter_rows rows_alt = {
	{
		"\xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8\xC3\xA9\xC3\xAA\xC3\xAB\xE2\x82\xAC",
		"\xC3\xAC\xC3\xAD\xC3\xAE\xC3\xAF\xC3\xB1\xC3\xB2\xC3\xB3\xC3\xB4\xC3\xB5\xC3\xB6\xC3\xB8\xC3\xB9\xC3\xBA",
		"\xC3\xBB\xC3\xBC\xC3\xBD\xC3\xBF\xC3\x9F\xC2\xA1\xC2\xBF\xC2\xAB\xC2\xBB\xC2\xB0\xC2\xA3",
		"@{}[]<>|\\~",
	},
	{
		"\xC3\x80\xC3\x81\xC3\x82\xC3\x83\xC3\x84\xC3\x85\xC3\x86\xC3\x87\xC3\x88\xC3\x89\xC3\x8A\xC3\x8B\xE2\x82\xAC",
		"\xC3\x8C\xC3\x8D\xC3\x8E\xC3\x8F\xC3\x91\xC3\x92\xC3\x93\xC3\x94\xC3\x95\xC3\x96\xC3\x98\xC3\x99\xC3\x9A",
		"\xC3\x9B\xC3\x9C\xC3\x9D\xC3\xBF\xC3\x9F\xC2\xA1\xC2\xBF\xC2\xAB\xC2\xBB\xC2\xB0\xC2\xA3",
		"\xC2\xA2\xC2\xA5\xC2\xA7\xC2\xB5\xC2\xB1\xC3\xB7\xC3\x97\xC2\xB2\xC2\xBA\xC2\xAA",
	},
};

#define KB_X		32.0f
#define KB_Y		164.0f
#define KB_ROW		50.0f
#define LETTER_UNIT	(576.0f / 13.0f)
#define NUMPAD_UNIT	80.0f

static osk_key *add_key(osk_keyset *set, float x, float y, float w, float unit, key_kind kind)
{
	osk_key *k;

	if (set->nkeys >= OSK_MAX_KEYS)
		return NULL;
	k = &set->keys[set->nkeys++];
	memset(k, 0, sizeof(*k));
	k->x = x;
	k->y = y;
	k->w = w * unit;
	k->h = KB_ROW;
	k->kind = kind;
	return k;
}

/* Adds one row of character keys; returns the x after the last key. */
static float add_char_row(osk_keyset *set, float x, float y, float unit,
			  const char *n0, const char *s0, const char *n1, const char *s1)
{
	uint32_t a, b, c, d;
	size_t la, lb, lc, ld;

	for (;;) {
		la = OSK_UTF8Decode(n0, &a);
		if (!la)
			break;
		lb = OSK_UTF8Decode(s0, &b);
		lc = n1 ? OSK_UTF8Decode(n1, &c) : 0;
		ld = s1 ? OSK_UTF8Decode(s1, &d) : 0;

		osk_key *k = add_key(set, x, y, 1.0f, unit, KEY_CHAR);
		if (!k)
			break;
		k->cp[0][0] = a;
		k->cp[0][1] = lb ? b : a;
		k->cp[1][0] = lc ? c : a;
		k->cp[1][1] = ld ? d : (lc ? c : k->cp[0][1]);

		n0 += la;
		s0 += lb;
		if (n1) n1 += lc;
		if (s1) s1 += ld;
		x += unit;
	}
	return x;
}

static void build_letters(osk_keyboard *osk, const letter_rows *rows)
{
	osk_keyset *set = &osk->keys;
	const float u = LETTER_UNIT;
	bool cancel = !(osk->cfg.flags & OSK_FLAG_NO_CANCEL);
	float x, y;
	int r;

	set->has_alt = true;
	for (r = 0; r < 4; r++) {
		x = KB_X;
		y = KB_Y + r * KB_ROW;
		if (r == 3) {
			add_key(set, x, y, 1.5f, u, KEY_SHIFT);
			x += 1.5f * u;
		}
		x = add_char_row(set, x, y, u, rows->normal[r], rows->shifted[r],
				 rows_alt.normal[r], rows_alt.shifted[r]);
		if (r == 2)
			add_key(set, x, y, 2.0f, u, KEY_BACK);
		if (r == 3)
			add_key(set, x, y, 1.5f, u, KEY_SHIFT);
	}

	y = KB_Y + 4 * KB_ROW;
	x = KB_X;
	add_key(set, x, y, 1.5f, u, KEY_CAPS);	x += 1.5f * u;
	add_key(set, x, y, 1.5f, u, KEY_ALT);	x += 1.5f * u;
	add_key(set, x, y, 1.0f, u, KEY_LEFT);	x += 1.0f * u;
	add_key(set, x, y, 5.0f, u, KEY_SPACE);	x += 5.0f * u;
	add_key(set, x, y, 1.0f, u, KEY_RIGHT);	x += 1.0f * u;
	if (cancel) {
		add_key(set, x, y, 1.5f, u, KEY_CANCEL);
		x += 1.5f * u;
		add_key(set, x, y, 1.5f, u, KEY_OK);
	} else {
		add_key(set, x, y, 3.0f, u, KEY_OK);
	}
}

static void build_numpad(osk_keyboard *osk)
{
	static const char *const digits[4] = { "789", "456", "123", "-0.," };
	static const key_kind side[3] = { KEY_BACK, KEY_LEFT, KEY_RIGHT };
	osk_keyset *set = &osk->keys;
	const float u = NUMPAD_UNIT;
	const float x0 = (OSK_DESIGN_W - 4 * u) / 2;
	float x, y;
	int r;

	set->has_alt = false;
	for (r = 0; r < 4; r++) {
		y = KB_Y + r * KB_ROW;
		x = add_char_row(set, x0, y, u, digits[r], digits[r], NULL, NULL);
		if (r < 3)
			add_key(set, x, y, 1.0f, u, side[r]);
	}
	y = KB_Y + 4 * KB_ROW;
	if (!(osk->cfg.flags & OSK_FLAG_NO_CANCEL)) {
		add_key(set, x0, y, 2.0f, u, KEY_CANCEL);
		add_key(set, x0 + 2 * u, y, 2.0f, u, KEY_OK);
	} else {
		add_key(set, x0, y, 4.0f, u, KEY_OK);
	}
}

void osk_build_keys(osk_keyboard *osk)
{
	osk->keys.nkeys = 0;
	switch (osk->cfg.layout) {
	case OSK_LAYOUT_QWERTZ:	build_letters(osk, &rows_qwertz); break;
	case OSK_LAYOUT_AZERTY:	build_letters(osk, &rows_azerty); break;
	case OSK_LAYOUT_NUMPAD:	build_numpad(osk); break;
	case OSK_LAYOUT_QWERTY:
	default:		build_letters(osk, &rows_qwerty); break;
	}
}

static bool is_letter(uint32_t cp)
{
	if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z'))
		return true;
	return cp >= 0xC0 && cp <= 0xFF && cp != 0xD7 && cp != 0xF7;
}

uint32_t osk_key_char(const osk_keyboard *osk, const osk_key *k)
{
	int page = osk->alt ? 1 : 0;
	bool shift = osk->shift;

	if (k->kind == KEY_SPACE)
		return ' ';
	if (k->kind != KEY_CHAR)
		return 0;
	if (osk->caps && is_letter(k->cp[page][0]))
		shift = !shift;
	return k->cp[page][shift ? 1 : 0];
}

const char *osk_key_label(const osk_keyboard *osk, const osk_key *k)
{
	switch (k->kind) {
	case KEY_BACK:		return "Back";
	case KEY_SHIFT:		return "Shift";
	case KEY_CAPS:		return "Caps";
	case KEY_ALT:		return osk->alt ? "abc" : "\xC3\xA0\xC3\xA9\xC3\xB1";
	case KEY_SPACE:		return "Space";
	case KEY_LEFT:		return "\x11";
	case KEY_RIGHT:		return "\x10";
	case KEY_OK:		return "OK";
	case KEY_CANCEL:	return "Cancel";
	default:		return NULL;
	}
}
