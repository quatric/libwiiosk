/*-------------------------------------------------------------
osk_layouts.c -- key arrangements and geometry
-------------------------------------------------------------*/
#include <string.h>

#include "osk_internal.h"

/* A letter layout fills up to 50 key slots: rows of 11, 10, 10 and 10
   keys, then six (or nine) keys around the space bar. Each table is
   four UTF-8 strings of 50 characters, one per state: default, Caps,
   Shift, Shift+Caps. A space marks a slot without a key. */
#define NSLOTS		50

typedef struct {
	const char *state[4];
} letter_table;

static const letter_table table_qwerty = { {
	"1234567890-" "qwertyuiop" "asdfghjkl:" "zxcvbnm,.=" "   " "[]'`/@",
	"1234567890-" "QWERTYUIOP" "ASDFGHJKL:" "ZXCVBNM,.=" "   " "[]'`/@",
	"!\\#$%^&*()_" "QWERTYUIOP" "ASDFGHJKL;" "ZXCVBNM<>+" "   " "{}\"?~|",
	"!\\#$%^&*()_" "qwertyuiop" "asdfghjkl;" "zxcvbnm<>+" "   " "{}\"?~|",
} };

static const letter_table table_qwertz = { {
	"1234567890\xC3\x9F" "qwertzuiop" "asdfghjkl\xC3\xBC" "yxcvbnm\xC3\xB6\xC3\xA4+" "[]\\" "#@\xC2\xB4,.-",
	"1234567890\xC3\x9F" "QWERTZUIOP" "ASDFGHJKL\xC3\x9C" "YXCVBNM\xC3\x96\xC3\x84*" "{}~" "'\xE2\x82\xAC`;:_",
	"!\"\xC2\xA7$%&/()=?" "QWERTZUIOP" "ASDFGHJKL\xC3\x9C" "YXCVBNM\xC3\x96\xC3\x84*" "<>~" "'\xE2\x82\xAC`;:_",
	"!\"\xC2\xA7$%&/()=?" "qwertzuiop" "asdfghjkl\xC3\xBC" "yxcvbnm\xC3\xB6\xC3\xA4+" "<>~" "#@\xC2\xB4,.-",
} };

static const letter_table table_azerty = { {
	"&\xC3\xA9\"'(-\xC3\xA8_\xC3\xA7\xC3\xA0)" "azertyuiop" "qsdfghjklm" "wxcvbn,.;:"
		"\xC5\x93^\xC2\xA8" "[]\xC3\xB9@!?",
	"1234567890\xC2\xB0" "AZERTYUIOP" "QSDFGHJKLM" "WXCVBN+-*/"
		"\xC5\x92\xC3\x87\\" "[]=%<>",
	"\xC3\x80\xC3\x82\xC3\x88\xC3\x89\xC3\x8A\xC3\x8B\xC3\x8E\xC3\x8F\xC3\x94\xC3\x99\xC3\x9B"
		"AZERTYUIOP" "QSDFGHJKLM" "WXCVBN~#|\xC2\xB5"
		"\xC5\x92\xC3\x87\\" "{}\xC2\xA7\xE2\x82\xAC\xC2\xA3$",
	"\xC3\xA0\xC3\xA5\xC3\xA8\xC3\xA9\xC3\xAA\xC3\xAB\xC3\xAE\xC3\xAF\xC3\xB4\xC3\xB9\xC3\xBB"
		"azertyuiop" "qsdfghjklm" "wxcvbn~#|\xC2\xB5"
		"\xC5\x93^\xC2\xA8" "{}\xC2\xA7\xE2\x82\xAC\xC2\xA3$",
} };

static const letter_table table_uk = { {
	"1234567890-" "qwertyuiop" "asdfghjkl;" "zxcvbnm,.=" " \xC2\xA5\xE2\x82\xAC" "[]\\/'#",
	"1234567890-" "QWERTYUIOP" "ASDFGHJKL;" "ZXCVBNM,.=" " \xC2\xA5\xE2\x82\xAC" "[]\\/'#",
	"!\"\xC2\xA3$%^&*()_" "QWERTYUIOP" "ASDFGHJKL:" "ZXCVBNM<>+" " \xC2\xA6`" "{}|?@~",
	"!\"\xC2\xA3$%^&*()_" "qwertyuiop" "asdfghjkl:" "zxcvbnm<>+" " \xC2\xA6`" "{}|?@~",
} };

static const letter_table table_it = { {
	"1234567890'" "qwertyuiop" "asdfghjkl+" "zxcvbnm,.-"
		"\xC3\xA0\xC3\xA8\xC3\xA9" "\xC3\xAC@#\xE2\x82\xAC\xC3\xB2\xC3\xB9",
	"1234567890'" "QWERTYUIOP" "ASDFGHJKL+" "ZXCVBNM,.-"
		"\xC3\x80\xC3\x88\xC3\x89" "\xC3\x8C@#\xE2\x82\xAC\xC3\x92\xC3\x99",
	"!\"\xC2\xA3$%&/()=?" "QWERTYUIOP" "ASDFGHJKL*" "ZXCVBNM;:_" "^<>" "\xC2\xB0[]~\\|",
	"!\"\xC2\xA3$%&/()=?" "qwertyuiop" "asdfghjkl*" "zxcvbnm;:_" "^<>" "\xC2\xB0[]~\\|",
} };

static const letter_table table_es = { {
	"1234567890\xC2\xA1" "qwertyuiop" "asdfghjkl\xC3\xB1" "zxcvbnm,.-" " \xC2\xB7\xC3\xA7" "()@\xC2\xB4`+",
	"1234567890\xC2\xA1" "QWERTYUIOP" "ASDFGHJKL\xC3\x91" "ZXCVBNM\xC2\xBA\xC2\xAA_"
		" \xC2\xAB\xC2\xBB" "<>\xE2\x82\xAC#\\'",
	"!\"$%&/{}=?\xC2\xBF" "QWERTYUIOP" "ASDFGHJKL\xC3\x91" "ZXCVBNM;:_" " |\xC3\x87" "[]~\xC2\xA8^*",
	"!\"$%&/{}=?\xC2\xBF" "qwertyuiop" "asdfghjkl\xC3\xB1" "zxcvbnm;:_" " |\xC3\x87" "[]~\xC2\xA8^*",
} };

static const letter_table table_nl = { {
	"1234567890-" "qwertyuiop" "asdfghjkl:" "zxcvbnm,./" "  `" "[]\\'=\xE2\x82\xAC",
	"1234567890-" "QWERTYUIOP" "ASDFGHJKL:" "ZXCVBNM,./" "  `" "[]\\'=\xE2\x82\xAC",
	"!@#$%^&*()_" "QWERTYUIOP" "ASDFGHJKL;" "ZXCVBNM<>?" "  ~" "{}|\"+\xE2\x82\xAC",
	"!@#$%^&*()_" "qwertyuiop" "asdfghjkl;" "zxcvbnm<>?" "  ~" "{}|\"+\xE2\x82\xAC",
} };

/* "More" page, shared by all letter layouts and filled into the visible
   slots in reading order: accented letters and extra symbols. The last
   three characters are only used by layouts with 50 slots. */
static const char alt_normal[] =
	"\xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8\xC3\xA9\xC3\xAA\xC3\xAB\xE2\x82\xAC"
	"\xC3\xAC\xC3\xAD\xC3\xAE\xC3\xAF\xC3\xB1\xC3\xB2\xC3\xB3\xC3\xB4\xC3\xB5\xC3\xB6\xC3\xB8\xC3\xB9\xC3\xBA"
	"\xC3\xBB\xC3\xBC\xC3\xBD\xC3\xBF\xC3\x9F\xC2\xA1\xC2\xBF\xC2\xAB\xC2\xBB\xC2\xB0\xC2\xA3"
	"@{}[]<>|\\~"
	"\xC2\xA9\xC2\xAE\xC2\xB6";
static const char alt_shifted[] =
	"\xC3\x80\xC3\x81\xC3\x82\xC3\x83\xC3\x84\xC3\x85\xC3\x86\xC3\x87\xC3\x88\xC3\x89\xC3\x8A\xC3\x8B\xE2\x82\xAC"
	"\xC3\x8C\xC3\x8D\xC3\x8E\xC3\x8F\xC3\x91\xC3\x92\xC3\x93\xC3\x94\xC3\x95\xC3\x96\xC3\x98\xC3\x99\xC3\x9A"
	"\xC3\x9B\xC3\x9C\xC3\x9D\xC3\xBF\xC3\x9F\xC2\xA1\xC2\xBF\xC2\xAB\xC2\xBB\xC2\xB0\xC2\xA3"
	"\xC2\xA2\xC2\xA5\xC2\xA7\xC2\xB5\xC2\xB1\xC3\xB7\xC3\x97\xC2\xB2\xC2\xBA\xC2\xAA"
	"\xC2\xB9\xC2\xB3\xC2\xBD";

/* key centers of the letter slots */
static void slot_center(int i, float *x, float *y)
{
	static const float last[4] = { 443.33f, 486.33f, 528.83f, 572.33f };

	if (i < 11) {
		*x = 67 + 43 * i;
		*y = 190;
	} else if (i < 21) {
		*x = 88 + 43 * (i - 11);
		*y = 225;
	} else if (i < 31) {
		*x = 153 + 43 * (i - 21);
		*y = 260;
	} else if (i < 41) {
		*x = 166 + 43 * (i - 31);
		*y = 295;
	} else {
		*x = i < 46 ? 67 + 43 * (i - 41) : last[i - 46];
		*y = 330;
	}
}

static osk_key *add_key(osk_keyboard *osk, key_kind kind, key_look look,
			float cx, float cy, float w, float h, float hw, float hh)
{
	osk_key *k;

	if (osk->nkeys >= OSK_MAX_KEYS)
		return NULL;
	k = &osk->keys[osk->nkeys++];
	memset(k, 0, sizeof(*k));
	k->kind = kind;
	k->look = look;
	k->cx = cx;
	k->cy = cy;
	k->w = w;
	k->h = h;
	k->hx = cx - hw / 2;
	k->hy = cy - hh / 2;
	k->hw = hw;
	k->hh = hh;
	k->push = -1;
	return k;
}

static bool is_letter(uint32_t cp)
{
	if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z'))
		return true;
	return cp >= 0xC0 && cp <= 0xFF && cp != 0xD7 && cp != 0xF7 && cp != 0xDF && cp != 0xFF;
}

static void build_letters(osk_keyboard *osk, const letter_table *t)
{
	const char *s[4], *an = alt_normal, *as = alt_shifted;
	uint32_t cp[4], a, b;
	int i, j;
	bool enter = !(osk->cfg.flags & OSK_FLAG_NO_ENTER);

	for (j = 0; j < 4; j++)
		s[j] = t->state[j];

	for (i = 0; i < NSLOTS; i++) {
		float x, y;
		osk_key *k;

		for (j = 0; j < 4; j++)
			s[j] += OSK_UTF8Decode(s[j], &cp[j]);
		if (cp[0] == ' ')
			continue;

		slot_center(i, &x, &y);
		k = add_key(osk, KEY_CHAR, LOOK_CHAR, x, y, 43, 35, 43, 35);
		if (!k)
			break;
		memcpy(k->cp[0], cp, sizeof(cp));

		an += OSK_UTF8Decode(an, &a);
		as += OSK_UTF8Decode(as, &b);
		k->cp[1][0] = a;
		k->cp[1][1] = is_letter(a) ? b : a;
		k->cp[1][2] = b;
		k->cp[1][3] = is_letter(a) ? a : b;
	}

	add_key(osk, KEY_BACK, LOOK_SPECIAL, 556, 190, 70, 32, 75, 35);
	if (enter)
		add_key(osk, KEY_ENTER, LOOK_SPECIAL, 545, 225, 92, 32, 93, 35);
	add_key(osk, KEY_CAPS, LOOK_SPECIAL, 88, 260, 84, 32, 85, 35);
	add_key(osk, KEY_SHIFT, LOOK_SPECIAL, 93, 295, 94, 32, 96, 35);
	add_key(osk, KEY_SPACE, LOOK_SPACE, 341, 330, 156, 32, 161, 35);
	add_key(osk, KEY_ALT, LOOK_MORE, 506, 369, 174, 40, 174, 40);
	if (osk->dict.ok)
		add_key(osk, KEY_PREDICT, LOOK_MORE, 133, 370, 174, 40, 174, 40);
}

/*-------------------------------------------------------------
  cellphone keypad
-------------------------------------------------------------*/
/* Slots 0-8 and 10 of the 3 x 4 grid: lower case key tops and the
   characters a key cycles through, ending with its digit. Capitals drop
   the sharp s. Slot 9 holds the prediction key, slot 11 the symbol key. */
typedef struct {
	const char *label0;		/* punctuation key top */
	const char *chars[9];
	const char *label5;		/* "mno" key top */
} phone_table;

static const phone_table phone_us = {
	".,?@", { ".,?!-':@/$#&1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9" }, "mno"
};
static const phone_table phone_uk = {
	".,?@", { ".,?!-':@/\xC2\xA3" "1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9" },
	"mno"
};
static const phone_table phone_de = {
	"'@_", { ".,!?:;-()'@_/&1", "abc\xC3\xA4" "2", "def3", "ghi4", "jkl5", "mno\xC3\xB6" "6",
		 "pqrs\xC3\x9F" "7", "tuv\xC3\xBC" "8", "wxyz9" }, "mno"
};
static const phone_table phone_it = {
	".,?@", { ".,?!:;'\"-()@/_1", "abc\xC3\xA0\xC3\xA1\xC3\xA4\xC3\xA2\xC3\xA7" "2",
		  "def\xC3\xA8\xC3\xA9\xC3\xAB\xC3\xAA" "3", "ghi\xC3\xAC\xC3\xAD\xC3\xAF\xC3\xAE" "4", "jkl5",
		  "mno\xC3\xB2\xC3\xB3\xC3\xB6\xC3\xB4\xC3\xB1\xC5\x93" "6", "pqrs\xC3\x9F" "7",
		  "tuv\xC3\xB9\xC3\xBA\xC3\xBC\xC3\xBB" "8", "wxyz9" }, "mno"
};
static const phone_table phone_nl = {
	".,:", { ".,-?!'@:;/()1", "abc\xC3\xA4\xC3\xA1\xC3\xA0\xC3\xA2\xC3\xA7" "2",
		 "def\xC3\xAB\xC3\xA9\xC3\xA8\xC3\xAA" "3", "ghi\xC3\xAF\xC3\xAD\xC3\xAC\xC3\xAE" "4", "jkl5",
		 "mno\xC3\xB6\xC3\xB3\xC3\xB2\xC3\xB4" "6", "pqrs7", "tuv\xC3\xBC\xC3\xBA\xC3\xB9\xC3\xBB" "8",
		 "wxyz9" }, "mno"
};
static const phone_table phone_es = {
	".,?@", { ".,:;'\"\xC2\xA1!\xC2\xBF?@/-_1", "abc\xC3\xA7\xC3\xA1\xC3\xA0\xC3\xA4\xC3\xA2" "2",
		  "def\xC3\xA9\xC3\xA8\xC3\xAB\xC3\xAA" "3", "ghi\xC3\xAD\xC3\xAC\xC3\xAF\xC3\xAE" "4", "jkl5",
		  "mn\xC3\xB1o\xC3\xB3\xC3\xB2\xC3\xB6\xC3\xB4\xC5\x93" "6", "pqrs\xC3\x9F" "7",
		  "tuv\xC3\xBA\xC3\xB9\xC3\xBC\xC3\xBB" "8", "wxyz9" }, "mn\xC3\xB1o"
};
static const phone_table phone_fr = {
	".!@", { ".,'!?@-_:/\"1", "abc\xC3\xA0\xC3\xA2\xC3\xA4\xC3\xA1\xC3\xA7" "2",
		 "def\xC3\xA9\xC3\xA8\xC3\xAA\xC3\xAB" "3", "ghi\xC3\xAE\xC3\xAF\xC3\xAD\xC3\xAC" "4", "jkl5",
		 "mno\xC3\xB1\xC3\xB4\xC5\x93\xC3\xB6\xC3\xB3\xC3\xB2" "6", "pqrs\xC3\x9F" "7",
		 "tuv\xC3\xB9\xC3\xBB\xC3\xBC\xC3\xBA" "8", "wxyz9" }, "mno"
};

static const phone_table *phone_table_of(const osk_keyboard *osk)
{
	switch (osk->letters) {
	case OSK_LAYOUT_QWERTY_UK:	return &phone_uk;
	case OSK_LAYOUT_QWERTZ:		return &phone_de;
	case OSK_LAYOUT_QWERTY_IT:	return &phone_it;
	case OSK_LAYOUT_QWERTY_NL:	return &phone_nl;
	case OSK_LAYOUT_QWERTY_ES:	return &phone_es;
	case OSK_LAYOUT_AZERTY:		return &phone_fr;
	default:			return &phone_us;
	}
}

/* letter keys show capitals in ABC mode and, in Abc mode, for the
   letter that starts a sentence */
static bool phone_capitals(const osk_keyboard *osk)
{
	return osk->mode == PHONE_UPPER || (osk->mode == PHONE_ABC && osk->phone_upper);
}

static unsigned decode_all(const char *s, uint32_t out[16], bool capitals)
{
	unsigned n = 0;
	uint32_t cp;
	size_t k;

	while (n < 16 && (k = OSK_UTF8Decode(s, &cp)) != 0) {
		s += k;
		if (capitals) {
			if (cp == 0xDF)
				continue;
			cp = osk_upper(cp);
		}
		out[n++] = cp;
	}
	return n;
}

unsigned osk_phone_chars(const osk_keyboard *osk, unsigned slot, uint32_t out[16])
{
	static const char numeric[] = "123456789-0.,";

	if (osk->numeric) {
		if (slot >= sizeof(numeric) - 1)
			return 0;
		out[0] = numeric[slot];
		return 1;
	}
	if (slot == 10) {
		out[0] = '0';
		if (osk->mode == PHONE_DIGITS)
			return 1;
		out[0] = ' ';
		out[1] = '0';
		return 2;
	}
	if (slot > 8)
		return 0;
	if (osk->mode == PHONE_DIGITS) {
		out[0] = '1' + slot;
		return 1;
	}
	return decode_all(phone_table_of(osk)->chars[slot], out, slot && phone_capitals(osk));
}

void osk_phone_label(const osk_keyboard *osk, unsigned slot, char out[32])
{
	static const char *const letters[9] = { NULL, "abc", "def", "ghi", "jkl", NULL, "pqrs", "tuv", "wxyz" };
	const phone_table *t = phone_table_of(osk);
	uint32_t cp[16];
	unsigned n, i;
	size_t pos = 0;

	out[0] = 0;
	if (osk->numeric || osk->mode == PHONE_DIGITS || slot > 8) {
		uint32_t c[16];
		if (slot == 10 && !osk->numeric && osk->mode != PHONE_DIGITS) {
			/* open box, and 0 unless T9 makes the key a space */
			strcpy(out, osk_t9_active(osk) ? "\xE2\x90\xA3" : "\xE2\x90\xA3" "0");
			return;
		}
		if (osk_phone_chars(osk, slot, c) == 1)
			out[OSK_UTF8Encode(c[0], out)] = 0;
		return;
	}
	if (!slot) {
		strcpy(out, t->label0);
		return;
	}
	n = decode_all(slot == 5 ? t->label5 : letters[slot], cp, phone_capitals(osk));
	for (i = 0; i < n; i++)
		pos += OSK_UTF8Encode(cp[i], &out[pos]);
	out[pos] = 0;
}

/* grid slot centers */
static float phone_x(unsigned slot)
{
	return 211 + 116 * (slot % 3);
}

static float phone_y(unsigned slot)
{
	return 199 + 53 * (slot / 3);
}

static void build_phone(osk_keyboard *osk)
{
	unsigned i;
	osk_key *k;

	for (i = 0; i < OSK_PHONE_SLOTS; i++) {
		if (!osk->numeric && (i == 9 || i == 11))
			continue;
		k = add_key(osk, KEY_PHONE, LOOK_PAD, phone_x(i), phone_y(i), 116, 54, 115, 53);
		if (k)
			k->arg = i;
	}
	add_key(osk, KEY_BACK, LOOK_PAD, 584, 199, 116, 54, 115, 53);
	if (!(osk->cfg.flags & OSK_FLAG_NO_ENTER))
		add_key(osk, KEY_ENTER, LOOK_PAD, 583, 252, 116, 54, 115, 53);

	if (osk->numeric) {
		/* ',' below Enter */
		k = add_key(osk, KEY_PHONE, LOOK_PAD, 584, 305, 116, 54, 115, 53);
		if (k)
			k->arg = 12;
		return;
	}
	if (osk->dict.ok)
		add_key(osk, KEY_PREDICT, LOOK_PAD_BLUE, phone_x(9), phone_y(9), 116, 54, 115, 53);
	add_key(osk, KEY_SYMBOLS, LOOK_PAD_BLUE, phone_x(11), phone_y(11), 116, 54, 115, 53);
	/* the tags stick out from the left edge; the hit box reaches the screen edge */
	for (i = 0; i < PHONE_MODES; i++) {
		k = add_key(osk, KEY_PHONE_MODE, LOOK_MODE, 61, phone_y(3 * i), 142, 50, 160, 46);
		if (k) {
			k->arg = i;
			k->hx = -32;
		}
	}
}

/*-------------------------------------------------------------
  symbol window
-------------------------------------------------------------*/
/* Ten pages of twenty symbols; the first page depends on the language,
   the others are shared. */
static const char *const symbol_page0[] = {
	/* English, German */
	".,\xE2\x80\x98:;\xE2\x80\x9E\xE2\x80\x9C\xE2\x80\x9D'\"?!()_\xC2\xBF\xC2\xA1\xC2\xAB\xC2\xBB&",
	/* French, Italian */
	".,':;\xE2\x80\x98\xE2\x80\x99\xE2\x80\x9C\xE2\x80\x9D\"?!()_\xC2\xBF\xC2\xA1\xC2\xAB\xC2\xBB&",
	/* Spanish */
	".,:;'\xC2\xA1!\xC2\xBF?\"\xC2\xAB\xC2\xBB\xE2\x80\x9C\xE2\x80\x9D_\xE2\x80\x98\xE2\x80\x99()&",
	/* Dutch */
	".,\xE2\x80\x98:;&\xE2\x80\x9C\xE2\x80\x9D'\"?!()_\xC2\xBF\xC2\xA1\xC2\xAB\xC2\xBB\xE2\x80\x9E",
};

static const char *const symbol_pages[] = {
	"[]{}\xC2\xB7<>+-\xC3\x97\xC3\xB7=\xC2\xB1\xE2\x88\x9E%\\/|\xC2\xA7@",
	"^~\xE2\x84\xA2\xC2\xA9\xC2\xAE\xC2\xBA\xC2\xAA\xE2\x99\xAD\xE2\x99\xAA*"
		"\xE2\x86\x90\xE2\x86\x92\xE2\x86\x91\xE2\x86\x93#$\xC2\xA2\xE2\x82\xAC\xC2\xA3\xC2\xA5",
	"\xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA3\xC3\xA7\xC3\xA8\xC3\xA9"
		"\xC3\xAA\xC3\xAB\xC3\xAC\xC3\xAD\xC3\xAE\xC3\xAF\xC3\xB1\xC3\xB2\xC3\xB3\xC3\xB4",
	"\xC3\xB6\xC5\x93\xC3\xB8\xC3\xB5\xC3\x9F\xC3\xB9\xC3\xBA\xC3\xBB\xC3\xBC\xC3\xBD"
		"\xC3\xBF\xC3\x80\xC3\x81\xC3\x82\xC3\x84\xC3\x85\xC3\x86\xC3\x83\xC3\x87\xC3\x88",
	"\xC3\x89\xC3\x8A\xC3\x8B\xC3\x8C\xC3\x8D\xC3\x8E\xC3\x8F\xC3\x91\xC3\x92\xC3\x93"
		"\xC3\x94\xC3\x96\xC5\x92\xC3\x98\xC3\x95\xC3\x99\xC3\x9A\xC3\x9B\xC3\x9C\xC3\x9D",
	"\xC5\xB8\xCE\xB1\xCE\xB2\xCE\xB3\xCE\xB4\xCE\xB5\xCE\xB6\xCE\xB7\xCE\xB8\xCE\xB9"
		"\xCE\xBA\xCE\xBB\xCE\xBC\xCE\xBD\xCE\xBE\xCE\xBF\xCF\x80\xCF\x81\xCF\x83\xCF\x84",
	"\xCF\x85\xCF\x86\xCF\x87\xCF\x88\xCF\x89\xCE\x91\xCE\x92\xCE\x93\xCE\x94\xCE\x95"
		"\xCE\x96\xCE\x97\xCE\x98\xCE\x99\xCE\x9A\xCE\x9B\xCE\x9C\xCE\x9D\xCE\x9E\xCE\x9F",
	"\xCE\xA0\xCE\xA1\xCE\xA3\xCE\xA4\xCE\xA5\xCE\xA6\xCE\xA7\xCE\xA8\xCE\xA9\xCD\xBE"
		"\xCE\x84\xCE\x85\xCE\x86\xCE\x87\xCE\x88\xCE\x89\xCE\x8A\xCE\x8C\xCE\x8E\xCE\x8F",
	"\xCE\x90\xCE\xAA\xCE\xAB\xCE\xAC\xCE\xAD\xCE\xAE\xCE\xAF\xCE\xB0\xCF\x82\xCF\x8A"
		"\xCF\x8B\xCF\x8C\xCF\x8D\xCF\x8E\xE2\x97\x8E\xE2\x98\x86\xE2\x97\x8B\xE2\x97\x87"
		"\xE2\x96\xA1\xE2\x96\xB3",
};

const unsigned osk_symbol_pages = 1 + sizeof(symbol_pages) / sizeof(symbol_pages[0]);

uint32_t osk_symbol(const osk_keyboard *osk, unsigned page, unsigned slot)
{
	const char *s;
	uint32_t cp = 0;
	size_t k;

	if (page >= osk_symbol_pages || slot >= OSK_SYMBOL_SLOTS)
		return 0;
	if (page) {
		s = symbol_pages[page - 1];
	} else {
		switch (osk->letters) {
		case OSK_LAYOUT_AZERTY:
		case OSK_LAYOUT_QWERTY_IT:	s = symbol_page0[1]; break;
		case OSK_LAYOUT_QWERTY_ES:	s = symbol_page0[2]; break;
		case OSK_LAYOUT_QWERTY_NL:	s = symbol_page0[3]; break;
		default:			s = symbol_page0[0]; break;
		}
	}
	do {
		k = OSK_UTF8Decode(s, &cp);
		s += k;
	} while (k && slot--);
	return k ? cp : 0;
}

static void build_symbols(osk_keyboard *osk)
{
	static const float sx[5] = { 105, 212.3f, 319.6f, 426.9f, 533.7f };
	static const float sy[4] = { 197.5f, 249.5f, 302.4f, 355.5f };
	unsigned i;

	for (i = 0; i < OSK_SYMBOL_SLOTS; i++) {
		osk_key *k = add_key(osk, KEY_SYMBOL, LOOK_SYMBOL, sx[i % 5], sy[i / 5], 102, 48, 102, 48);
		if (k)
			k->arg = i;
	}
	add_key(osk, KEY_SYMBOL_PREV, LOOK_SYMBOL_BUTTON, 105, 408.5f, 102, 48, 102, 48);
	add_key(osk, KEY_SYMBOL_NEXT, LOOK_SYMBOL_BUTTON, 213, 408.5f, 102, 48, 102, 48);
	add_key(osk, KEY_SYMBOL_CLOSE, LOOK_SYMBOL_BUTTON, 480, 408.5f, 210, 52, 208, 48);
}

/* "Eng", "Fra", "Esp" for the American dictionaries, the two letter
   codes for the European ones, like the system keyboard */
const char *osk_dict_short_name(const osk_keyboard *osk)
{
	static const struct { const char *name, *code; } codes[] = {
		{ "English", "Eng" }, { "English (UK)", "EN" }, { "Deutsch", "DE" },
		{ "Fran\xC3\xA7" "ais", "FR" }, { "Fran\xC3\xA7" "ais (CA)", "Fra" },
		{ "Espa\xC3\xB1" "ol", "ES" }, { "Espa\xC3\xB1" "ol (LA)", "Esp" },
		{ "Italiano", "IT" }, { "Nederlands", "NL" },
	};
	unsigned i;

	for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
		if (!strcmp(osk->dict.name, codes[i].name))
			return codes[i].code;
	return osk->dict.name;
}

void osk_build_keys(osk_keyboard *osk)
{
	osk->nkeys = 0;
	if (osk->symbols) {
		build_symbols(osk);
		osk->nbase = osk->nkeys;
		return;
	}
	if (osk->phone) {
		build_phone(osk);
	} else {
		switch (osk->letters) {
		case OSK_LAYOUT_QWERTZ:		build_letters(osk, &table_qwertz); break;
		case OSK_LAYOUT_AZERTY:		build_letters(osk, &table_azerty); break;
		case OSK_LAYOUT_QWERTY_UK:	build_letters(osk, &table_uk); break;
		case OSK_LAYOUT_QWERTY_IT:	build_letters(osk, &table_it); break;
		case OSK_LAYOUT_QWERTY_ES:	build_letters(osk, &table_es); break;
		case OSK_LAYOUT_QWERTY_NL:	build_letters(osk, &table_nl); break;
		default:			build_letters(osk, &table_qwerty); break;
		}
	}

	/* toolbar */
	if (!(osk->cfg.flags & OSK_FLAG_NO_CANCEL))
		add_key(osk, KEY_CANCEL, LOOK_PILL, 125, 425, 172, 52, 172, 48);
	if (!(osk->cfg.flags & OSK_FLAG_LOCK_LAYOUT)) {
		add_key(osk, KEY_TAB_LETTERS, LOOK_TAB, 287, 424, 60, 38, 60, 40);
		add_key(osk, KEY_TAB_PHONE, LOOK_TAB, 352, 424, 58, 38, 60, 40);
	}
	add_key(osk, KEY_OK, LOOK_PILL, 518, 425, 172, 52, 172, 48);

	osk->nbase = osk->nkeys;
	osk_build_bar(osk);
}

/*-------------------------------------------------------------
  candidate bar
-------------------------------------------------------------*/
const osk_font_size osk_cand_font = { 23.4f, 24.3f, 0 };

/* text width, squeezed to fit the bar */
float osk_cand_width(const osk_candidate *c)
{
	float w = osk_text_width(c->cp, c->len, &osk_cand_font);
	return w < OSK_BAR_X1 - OSK_BAR_X0 ? w : OSK_BAR_X1 - OSK_BAR_X0;
}

/* the scroll arrows, then the candidates of the page starting at
   cand_first that fit */
void osk_build_bar(osk_keyboard *osk)
{
	float x = OSK_BAR_X0;
	unsigned i;

	osk->nkeys = osk->nbase;
	osk->cand_shown = 0;
	if (!osk_predict_active(osk))
		return;
	add_key(osk, KEY_SCROLL_L, LOOK_SCROLL, 458, OSK_BAR_CY, 36, 32, 42, 36);
	add_key(osk, KEY_SCROLL_R, LOOK_SCROLL, 500, OSK_BAR_CY, 36, 32, 42, 36);
	for (i = osk->cand_first; i < osk->ncand; i++) {
		float w = osk_cand_width(&osk->cand[i]);
		osk_key *k;

		if (x + w > OSK_BAR_X1 && i > osk->cand_first)
			break;
		k = add_key(osk, KEY_CANDIDATE, LOOK_CANDIDATE, x + w / 2, OSK_BAR_CY, w + 12, 30,
			    w + OSK_BAR_GAP, 36);
		if (!k)
			break;
		k->arg = i;
		x += w + OSK_BAR_GAP;
		osk->cand_shown++;
	}
}

uint32_t osk_key_char(const osk_keyboard *osk, const osk_key *k)
{
	int page = osk->alt && !osk->phone ? 1 : 0;

	if (k->kind == KEY_SPACE)
		return ' ';
	if (k->kind != KEY_CHAR)
		return 0;
	return k->cp[page][(osk->caps ? 1 : 0) | (osk->shift ? 2 : 0)];
}

const char *osk_key_label(const osk_keyboard *osk, const osk_key *k)
{
	switch (k->kind) {
	case KEY_SHIFT:		return "Shift";
	case KEY_CAPS:		return "Caps";
	case KEY_SPACE:		return "Space";
	case KEY_ALT:		return osk->alt ? "Back" : "More";
	case KEY_OK:		return "OK";
	case KEY_CANCEL:	return "Quit";
	case KEY_PREDICT:	return osk->phone ? osk_dict_short_name(osk) : osk->dict.name;
	case KEY_SYMBOLS:	return osk->letters == OSK_LAYOUT_QWERTY ? "More" :
				       osk->letters == OSK_LAYOUT_QWERTY_UK || osk->letters == OSK_LAYOUT_QWERTZ ||
				       osk->letters == OSK_LAYOUT_QWERTY_NL ? "\xE2\x82\xAC\xC3\xA9\xCE\xB1" :
				       "\xE2\x82\xAC\xC3\xA5\xCE\xB1";
	case KEY_PHONE_MODE: {
		static const char *const names[PHONE_MODES] = { "Abc", "abc", "ABC", "123" };
		return names[k->arg];
	}
	case KEY_SYMBOL_PREV:	return "\xE2\x86\x90";
	case KEY_SYMBOL_NEXT:	return "\xE2\x86\x92";
	case KEY_SYMBOL_CLOSE:
		switch (osk->letters) {
		case OSK_LAYOUT_AZERTY:		return "Fermer";
		case OSK_LAYOUT_QWERTZ:		return "Schlie\xC3\x9F" "en";
		case OSK_LAYOUT_QWERTY_IT:	return "Chiudi";
		case OSK_LAYOUT_QWERTY_ES:	return "Cerrar";
		case OSK_LAYOUT_QWERTY_NL:	return "Sluiten";
		default:			return "Close";
		}
	default:		return NULL;
	}
}

bool osk_key_latched(const osk_keyboard *osk, const osk_key *k)
{
	switch (k->kind) {
	case KEY_SHIFT:		return osk->shift;
	case KEY_CAPS:		return osk->caps;
	case KEY_TAB_LETTERS:	return !osk->phone;
	case KEY_TAB_PHONE:	return osk->phone;
	case KEY_PREDICT:	return osk->predict;
	case KEY_PHONE_MODE:	return osk->mode == k->arg;
	default:		return false;
	}
}
