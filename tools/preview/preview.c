/*-------------------------------------------------------------
preview.c -- renders keyboard states to PNG on the desktop

    make -C tools/preview
    tools/preview/wiiosk_preview out_dir [scale]

One PNG per scene, at scale x the 640x480 (or 854x480) virtual screen.
-------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osk_internal.h"
#include "png.h"
#include "soft.h"

static int scale = 2;
static const char *out_dir;

static void frames(osk_keyboard *osk, int n, u32 down)
{
	osk_input in;

	while (n-- > 0) {
		memset(&in, 0, sizeof(in));
		in.down = down;
		down = 0;
		OSK_Update(osk, &in);
	}
}

static void point(osk_keyboard *osk, float x, float y, int n)
{
	osk_input in;

	while (n-- > 0) {
		memset(&in, 0, sizeof(in));
		in.pointer_valid = true;
		in.pointer_x = x;
		in.pointer_y = y;
		OSK_Update(osk, &in);
	}
}

/* point at (x, y) and press A there */
static void press(osk_keyboard *osk, float x, float y)
{
	osk_input in;

	point(osk, x, y, 1);
	memset(&in, 0, sizeof(in));
	in.pointer_valid = true;
	in.pointer_x = x;
	in.pointer_y = y;
	in.down = in.held = OSK_BTN_SELECT;
	OSK_Update(osk, &in);
}

static void shoot(osk_keyboard *osk, const char *name)
{
	int w = (int)(osk->vw * scale), h = (int)(osk->vh * scale), i;
	float *rgb = calloc((size_t)w * h * 3, sizeof(float));
	u8 *px = malloc((size_t)w * h * 3);
	char path[512];

	soft_target(rgb, w, h, scale);
	OSK_Draw(osk);
	for (i = 0; i < w * h * 3; i++) {
		float v = rgb[i] * 255 + 0.5f;
		px[i] = v < 0 ? 0 : v > 255 ? 255 : (u8)v;
	}
	snprintf(path, sizeof(path), "%s/%s.png", out_dir, name);
	png_write(path, px, w, h, 3);
	free(rgb);
	free(px);
}

static const osk_dictionary *dictionary;

static osk_keyboard *make(osk_layout layout, unsigned flags, const char *title, const char *text)
{
	osk_config cfg;

	OSK_DefaultConfig(&cfg);
	cfg.layout = layout;
	cfg.flags = flags;
	cfg.title = title;
	cfg.initial = text;
	cfg.max_chars = OSK_MAX_CHARS;
	cfg.dictionary = dictionary;
	if (flags & OSK_FLAG_WIDESCREEN)
		cfg.screen_width = 854;
	return OSK_Create(&cfg);
}

/* drop the focus so the scene matches the resting layout */
static void unfocus(osk_keyboard *osk)
{
	int i;

	osk->focus = -1;
	for (i = 0; i < osk->nkeys; i++)
		osk->keys[i].focus = 0;
}

int main(int argc, char **argv)
{
	osk_keyboard *osk;

	if (argc < 2) {
		fprintf(stderr, "usage: %s out_dir [scale]\n", argv[0]);
		return 1;
	}
	out_dir = argv[1];
	if (argc > 2)
		scale = atoi(argv[2]);

	/* the resting US keyboard, as in the reference picture */
	osk = make(OSK_LAYOUT_QWERTY, 0, NULL, "Player");
	unfocus(osk);
	osk->frame = 11;	/* caret fully visible */
	shoot(osk, "us");
	OSK_Destroy(osk);

	/* default focus on 'q', placeholder text */
	osk = make(OSK_LAYOUT_QWERTY, 0, "Enter your name.", NULL);
	frames(osk, 11, 0);
	shoot(osk, "us_focus");
	/* shift latched, focus moved onto it */
	frames(osk, 1, OSK_BTN_SHIFT);
	frames(osk, 1, OSK_BTN_DOWN);
	frames(osk, 1, OSK_BTN_DOWN);
	frames(osk, 1, OSK_BTN_LEFT);
	frames(osk, 10, 0);
	shoot(osk, "us_shift");
	/* key push, 3 frames in */
	frames(osk, 1, OSK_BTN_RIGHT);
	frames(osk, 10, 0);
	frames(osk, 1, OSK_BTN_SELECT);
	frames(osk, 2, 0);
	shoot(osk, "us_push");
	OSK_Destroy(osk);

	/* More page */
	osk = make(OSK_LAYOUT_QWERTY, 0, NULL, "Caf\xC3\xA9 cr\xC3\xA8me");
	frames(osk, 1, OSK_BTN_ALT);
	point(osk, 506, 369, 10);
	shoot(osk, "us_more");
	OSK_Destroy(osk);

	/* long text, scrolled */
	osk = make(OSK_LAYOUT_QWERTY, OSK_FLAG_MULTILINE, NULL,
		   "The quick brown fox jumps over the lazy dog.\nSecond line\nThird line, which is long enough to wrap around.");
	point(osk, 125, 425, 10);
	shoot(osk, "us_scroll");
	OSK_Destroy(osk);

	osk = make(OSK_LAYOUT_QWERTZ, 0, NULL, "Gr\xC3\xBC\xC3\x9F" "e");
	unfocus(osk);
	shoot(osk, "de");
	OSK_Destroy(osk);

	osk = make(OSK_LAYOUT_AZERTY, 0, NULL, "\xC5\x93uvre");
	unfocus(osk);
	shoot(osk, "fr");
	OSK_Destroy(osk);

	/* cellphone keypad: "Hello " then the 2 key tapped twice, its "B"
	   still pending */
	osk = make(OSK_LAYOUT_QWERTY, 0, NULL, "Hello ");
	press(osk, 352, 424);
	press(osk, 327, 199);
	press(osk, 327, 199);
	point(osk, 327, 199, 10);
	shoot(osk, "phone");
	/* small letters, focus on the tag */
	press(osk, 40, 252);
	point(osk, 40, 252, 10);
	shoot(osk, "phone_lower");
	/* 123 */
	press(osk, 40, 358);
	point(osk, 40, 358, 10);
	shoot(osk, "phone_digits");
	/* the symbol window, opened, then sliding to the next page */
	press(osk, 40, 305);
	press(osk, 443, 358);
	point(osk, 212, 250, 20);
	shoot(osk, "symbols");
	press(osk, 213, 408);
	point(osk, 213, 408, 6);
	shoot(osk, "symbols_slide");
	point(osk, 213, 408, 20);
	press(osk, 213, 408);
	press(osk, 213, 408);
	press(osk, 213, 408);
	press(osk, 213, 408);
	press(osk, 213, 408);
	point(osk, 320, 302, 24);
	shoot(osk, "symbols_greek");
	OSK_Destroy(osk);

	/* T9: "Hello " then 4 6 6 3 being typed, the candidates above */
	dictionary = &osk_dict_en_us;
	osk = make(OSK_LAYOUT_QWERTY, 0, NULL, "Hello ");
	press(osk, 352, 424);
	press(osk, 211, 252);
	press(osk, 443, 252);
	press(osk, 443, 252);
	press(osk, 443, 199);
	point(osk, 443, 199, 10);
	osk->frame = 11;
	shoot(osk, "phone_t9");
	OSK_Destroy(osk);
	dictionary = NULL;

	osk = make(OSK_LAYOUT_NUMPAD, 0, NULL, "3.14159");
	frames(osk, 11, 0);
	shoot(osk, "numpad");
	OSK_Destroy(osk);

	osk = make(OSK_LAYOUT_NUMPAD, OSK_FLAG_LOCK_LAYOUT, NULL, "192.168.0.1");
	frames(osk, 11, 0);
	shoot(osk, "numeric");
	OSK_Destroy(osk);

	osk = make(OSK_LAYOUT_QWERTY, OSK_FLAG_WIDESCREEN | OSK_FLAG_PASSWORD | OSK_FLAG_NO_CANCEL, NULL, "secret");
	point(osk, 400, 300, 10);
	shoot(osk, "wide");
	OSK_Destroy(osk);

	/* word prediction: candidates for "th", the second one focused */
	dictionary = &osk_dict_en_us;
	osk = make(OSK_LAYOUT_QWERTY, 0, NULL, "Hello th");
	frames(osk, 1, OSK_BTN_UP);
	frames(osk, 1, OSK_BTN_UP);
	frames(osk, 1, OSK_BTN_RIGHT);
	frames(osk, 10, 0);
	shoot(osk, "us_predict");
	OSK_Destroy(osk);

	/* prediction switched off, pointer on the prediction key */
	osk = make(OSK_LAYOUT_QWERTY, OSK_FLAG_PREDICT_OFF, NULL, "Hello th");
	point(osk, 133, 370, 10);
	shoot(osk, "us_predict_off");
	OSK_Destroy(osk);

	{
		/* every language's layout with its dictionary, as picked by
		   OSK_UseSystemLanguage() */
		static const struct {
			osk_language lang;
			const char *text, *name;
		} langs[] = {
			{ OSK_LANG_EN_GB, "Favourite col", "uk" },
			{ OSK_LANG_IT, "Buona sera, per", "it" },
			{ OSK_LANG_ES, "Ma\xC3\xB1" "ana que", "es" },
			{ OSK_LANG_NL, "Goedemorgen gro", "nl" },
		};
		unsigned i;

		for (i = 0; i < sizeof(langs) / sizeof(langs[0]); i++) {
			dictionary = OSK_LanguageDictionary(langs[i].lang);
			osk = make(OSK_LanguageLayout(langs[i].lang), 0, NULL, langs[i].text);
			unfocus(osk);
			osk->frame = 11;
			shoot(osk, langs[i].name);
			OSK_Destroy(osk);
		}
	}

	dictionary = &osk_dict_fr;
	osk = make(OSK_LAYOUT_AZERTY, 0, NULL, "Il a \xC3\xA9t\xC3\xA9 fran");
	unfocus(osk);
	osk->frame = 11;
	shoot(osk, "fr_predict");
	OSK_Destroy(osk);
	return 0;
}
