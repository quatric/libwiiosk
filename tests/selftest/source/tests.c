/*-------------------------------------------------------------
tests.c -- wiiosk self-test cases: drive OSK_Update() with scripted
input and check the resulting text. Built for the Wii (main.c) and
for the desktop (tools/preview).
-------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <wiiosk.h>

#include "tests.h"

int passed, failed;
char first_failure[96];
static int rejects;

static void check(bool ok, const char *what)
{
	if (ok) {
		passed++;
		return;
	}
	failed++;
	printf("FAIL: %s\n", what);
	if (!first_failure[0])
		snprintf(first_failure, sizeof(first_failure), "%s", what);
}

static void on_event(osk_keyboard *osk, osk_event ev, void *user)
{
	if (ev == OSK_EVENT_REJECT)
		rejects++;
}

static osk_status step(osk_keyboard *osk, u32 down, u32 held)
{
	osk_input in;
	memset(&in, 0, sizeof(in));
	in.down = down;
	in.held = held | down;
	return OSK_Update(osk, &in);
}

static void type(osk_keyboard *osk, const char *utf8)
{
	osk_input in;
	uint32_t cp;
	size_t n;

	memset(&in, 0, sizeof(in));
	while ((n = OSK_UTF8Decode(utf8, &cp)) != 0 && in.text_count < OSK_INPUT_MAX_TEXT) {
		in.text[in.text_count++] = cp;
		utf8 += n;
	}
	OSK_Update(osk, &in);
}

static osk_status point(osk_keyboard *osk, float x, float y, u32 down)
{
	osk_input in;
	memset(&in, 0, sizeof(in));
	in.pointer_valid = true;
	in.pointer_x = x;
	in.pointer_y = y;
	in.down = down;
	in.held = down;
	return OSK_Update(osk, &in);
}

/* move the pointer onto (x, y) and press A there */
static osk_status click(osk_keyboard *osk, float x, float y)
{
	point(osk, x, y, 0);
	return point(osk, x, y, OSK_BTN_SELECT);
}

static bool digits_only(uint32_t cp, void *user)
{
	return cp >= '0' && cp <= '9';
}

static bool candidate_is(osk_keyboard *osk, unsigned i, const char *want)
{
	char buf[64];
	OSK_CopyCandidate(osk, i, buf, sizeof(buf));
	return !strcmp(buf, want);
}

static bool has_candidate(osk_keyboard *osk, const char *want)
{
	unsigned i;
	for (i = 0; i < OSK_GetCandidateCount(osk); i++)
		if (candidate_is(osk, i, want))
			return true;
	return false;
}

static osk_sound sounds[16];
static unsigned nsounds;

static void on_sound(osk_keyboard *osk, osk_sound snd, void *user)
{
	if (nsounds < 16)
		sounds[nsounds] = snd;
	nsounds++;
}

/* the sounds since the last call, as a string of their numbers + 'a' */
static bool heard(const char *want)
{
	char got[17];
	unsigned i;

	for (i = 0; i < nsounds && i < 16; i++)
		got[i] = 'a' + sounds[i];
	got[i] = 0;
	nsounds = 0;
	return !strcmp(got, want);
}

#define S(x)	((char)('a' + OSK_SOUND_##x))

static void sound_tests(void)
{
	osk_config cfg;
	osk_keyboard *osk;
	osk_sound_data d;
	char want[8];
	unsigned i, total = 0;
	bool ok = true;

	OSK_DefaultConfig(&cfg);
	cfg.on_sound = on_sound;
	cfg.max_chars = 2;
	nsounds = 0;
	osk = OSK_Create(&cfg);
	check(heard((char[]){ S(OPEN), 0 }), "sound open");
	type(osk, "abc");
	check(heard((char[]){ S(INPUT), S(INPUT), S(ERROR), 0 }), "sound input");
	step(osk, OSK_BTN_BACKSPACE, 0);
	step(osk, OSK_BTN_CARET_LEFT, 0);
	check(heard((char[]){ S(DELETE), S(CARET), 0 }), "sound delete, caret");
	step(osk, OSK_BTN_BACKSPACE, 0);
	check(heard((char[]){ S(ERROR), 0 }), "sound backspace refused");
	step(osk, OSK_BTN_SHIFT, 0);
	step(osk, OSK_BTN_SHIFT, 0);
	step(osk, OSK_BTN_ALT, 0);
	step(osk, OSK_BTN_ALT, 0);
	want[0] = S(SHIFT_ON); want[1] = S(SHIFT_OFF); want[2] = S(MORE); want[3] = S(BACK); want[4] = 0;
	check(heard(want), "sound shift, more");
	step(osk, OSK_BTN_RIGHT, 0);
	click(osk, 352, 424);				/* numpad tab */
	want[0] = S(FOCUS); want[1] = S(FOCUS); want[2] = S(NUMPAD); want[3] = 0;
	check(heard(want), "sound focus, tab");
	step(osk, OSK_BTN_CONFIRM, 0);
	check(heard((char[]){ S(OK), 0 }), "sound ok");
	OSK_Destroy(osk);

	for (i = 0; i < OSK_SOUND_COUNT; i++) {
		if (!OSK_GetSound(i, &d) || !d.samples || d.rate < 8000)
			ok = false;
		else
			total += d.samples * 1000 / d.rate;
	}
	check(ok && total > 4000 && !OSK_GetSound(OSK_SOUND_COUNT, &d), "sound data");
}

/* internal (osk_syslang.c): CONF language, area, region -> language */
osk_language osk_conf_language(int lang, int area, int region);

static void language_tests(void)
{
	static const struct { int lang, area, region; osk_language want; } cases[] = {
		{ 1, 1, 1, OSK_LANG_EN_US },	/* English, USA */
		{ 1, 2, 2, OSK_LANG_EN_GB },	/* English, Europe */
		{ 1, 3, 2, OSK_LANG_EN_GB },	/* English, Australia */
		{ 2, 2, 2, OSK_LANG_DE },
		{ 3, 1, 1, OSK_LANG_FR_CA },	/* French, USA */
		{ 3, 2, 2, OSK_LANG_FR },
		{ 4, 10, 1, OSK_LANG_ES_LA },	/* Spanish, Latin America */
		{ 4, 4, 1, OSK_LANG_ES_LA },	/* Spanish, Brazil */
		{ 4, 2, 2, OSK_LANG_ES },
		{ 5, 2, 2, OSK_LANG_IT },
		{ 6, 2, 2, OSK_LANG_NL },
		{ 1, -1, 1, OSK_LANG_EN_US },	/* no area: the region decides */
		{ 1, -1, 2, OSK_LANG_EN_GB },
		{ 0, 0, 0, OSK_LANG_NONE },	/* Japanese */
		{ 9, 7, 4, OSK_LANG_NONE },	/* Korean */
	};
	unsigned i;
	bool ok = true;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		if (osk_conf_language(cases[i].lang, cases[i].area, cases[i].region) != cases[i].want)
			ok = false;
	check(ok, "system language");
	check(OSK_LanguageLayout(OSK_LANG_EN_US) == OSK_LAYOUT_QWERTY &&
	      OSK_LanguageLayout(OSK_LANG_EN_GB) == OSK_LAYOUT_QWERTY_UK &&
	      OSK_LanguageLayout(OSK_LANG_DE) == OSK_LAYOUT_QWERTZ &&
	      OSK_LanguageLayout(OSK_LANG_FR_CA) == OSK_LAYOUT_AZERTY &&
	      OSK_LanguageLayout(OSK_LANG_ES_LA) == OSK_LAYOUT_QWERTY_ES &&
	      OSK_LanguageLayout(OSK_LANG_IT) == OSK_LAYOUT_QWERTY_IT &&
	      OSK_LanguageLayout(OSK_LANG_NL) == OSK_LAYOUT_QWERTY_NL &&
	      OSK_LanguageLayout(OSK_LANG_NONE) == OSK_LAYOUT_QWERTY, "language layout");
	check(OSK_LanguageDictionary(OSK_LANG_EN_GB) == &osk_dict_en_gb &&
	      OSK_LanguageDictionary(OSK_LANG_NL) == &osk_dict_nl &&
	      !OSK_LanguageDictionary(OSK_LANG_NONE), "language dictionary");
#ifdef GEKKO
	check(OSK_SystemDictionary() == OSK_LanguageDictionary(OSK_SystemLanguage()), "system dictionary");
	{
		osk_config c;
		OSK_DefaultConfig(&c);
		OSK_UseSystemLanguage(&c);
		check(c.layout == OSK_SystemLayout() && c.dictionary == OSK_SystemDictionary(), "use system language");
		c.layout = OSK_LAYOUT_NUMPAD;
		OSK_UseSystemLanguage(&c);
		check(c.layout == OSK_LAYOUT_NUMPAD, "system language keeps the numpad");
	}
#endif
}

/* charset c a t r d o g; graph c(->d) a t r, d o g */
static const u8 mini_zsd[] = {
	0, 0, 0, 0,
	0, 0, 34, 0, 0, 34,  0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0,  0, 0, 1, 0, 0, 68,
	0, 0, 0, 21,  0, 'c', 0, 'a', 0, 't', 0, 'r', 0, 'd', 0, 'o', 0, 'g',  0, 0, 0, 0, 0, 0, 0,
	0x70, 0, 6, 0x11, 0x32, 0x03, 0x14, 0x15, 0x06,
	3, 'c', 'a', 'r',
};

static void prediction_tests(void)
{
	static u8 junk[4096];
	osk_config cfg;
	osk_keyboard *osk;
	osk_dictionary dict;
	unsigned i, seed = 1;

	OSK_DefaultConfig(&cfg);
	cfg.dictionary = &osk_dict_en_us;
	cfg.on_event = on_event;
	osk = OSK_Create(&cfg);
	check(OSK_GetPrediction(osk) && OSK_GetCandidateCount(osk) == 0, "prediction idle");

	type(osk, "Hello wh");
	check(OSK_GetCandidateCount(osk) > 4 && candidate_is(osk, 0, "why"), "candidates");
	click(osk, 50, 152);				/* first candidate */
	check(!strcmp(OSK_GetText(osk), "Hello why "), "pick candidate");
	check(OSK_GetCandidateCount(osk) == 0, "no word after space");

	OSK_SetText(osk, "Th");
	check(candidate_is(osk, 0, "Th") && has_candidate(osk, "The"), "capitalised");
	OSK_SetText(osk, "HEL");
	check(has_candidate(osk, "HELLO"), "all caps");
	OSK_SetText(osk, "resume");
	check(candidate_is(osk, 0, "resume") && has_candidate(osk, "r\xC3\xA9sum\xC3\xA9"), "accent folding");
	OSK_SetText(osk, "wi");
	check(has_candidate(osk, "wii"), "extra words");
	OSK_SetText(osk, "thx");
	step(osk, OSK_BTN_CARET_LEFT, 0);
	check(OSK_GetCandidateCount(osk) == 0, "caret inside a word");

	/* D-pad: up twice from 'q' reaches the bar, right moves along it */
	OSK_SetText(osk, "wh");
	step(osk, OSK_BTN_UP, 0);
	step(osk, OSK_BTN_UP, 0);
	step(osk, OSK_BTN_SELECT, 0);
	check(!strncmp(OSK_GetText(osk), "wh", 2) && OSK_GetLength(osk) > 3 &&
	      OSK_GetText(osk)[OSK_GetLength(osk) - 1] == ' ', "d-pad candidate");

	/* the prediction key */
	OSK_SetText(osk, "wh");
	click(osk, 133, 370);
	check(!OSK_GetPrediction(osk) && OSK_GetCandidateCount(osk) == 0, "prediction off");
	click(osk, 133, 370);
	check(OSK_GetPrediction(osk) && OSK_GetCandidateCount(osk) > 0, "prediction on");

	/* scrolling */
	OSK_SetText(osk, "a");
	rejects = 0;
	click(osk, 458, 152);				/* left: already at the start */
	check(rejects == 1, "scroll left rejected");
	click(osk, 500, 152);
	click(osk, 50, 152);
	check(OSK_GetLength(osk) > 2 && !candidate_is(osk, 0, "a"), "scroll right");
	OSK_Destroy(osk);

	/* full buffer: only words that fit are offered */
	cfg.max_chars = 8;
	osk = OSK_Create(&cfg);
	type(osk, "abcde th");
	for (i = 0; i < OSK_GetCandidateCount(osk); i++) {
		char buf[64];
		if (OSK_CopyCandidate(osk, i, buf, sizeof(buf)) > 2)
			break;
	}
	check(OSK_GetCandidateCount(osk) > 0 && i == OSK_GetCandidateCount(osk), "candidates fit");
	OSK_ChooseCandidate(osk, 0);
	check(OSK_GetLength(osk) == 8, "no room for the space");
	OSK_Destroy(osk);

	/* no prediction for passwords or on the numpad */
	cfg.max_chars = 0;
	cfg.flags = OSK_FLAG_PASSWORD;
	cfg.initial = "wh";
	osk = OSK_Create(&cfg);
	check(!OSK_GetPrediction(osk) && OSK_GetCandidateCount(osk) == 0, "password");
	OSK_Destroy(osk);
	cfg.flags = 0;
	cfg.layout = OSK_LAYOUT_NUMPAD;
	osk = OSK_Create(&cfg);
	check(OSK_GetCandidateCount(osk) == 0, "numpad");
	OSK_Destroy(osk);
	cfg.layout = OSK_LAYOUT_QWERTY;

	/* German, unpacked again after OSK_FreeDictionaries() */
	cfg.dictionary = &osk_dict_de;
	cfg.initial = "uber";
	osk = OSK_Create(&cfg);
	check(candidate_is(osk, 0, "\xC3\xBC" "ber"), "german");
	OSK_Destroy(osk);
	OSK_FreeDictionaries();
	osk = OSK_Create(&cfg);
	check(candidate_is(osk, 0, "\xC3\xBC" "ber"), "dictionary unpacked again");
	OSK_Destroy(osk);

	/* a hand made .zsd: cat, car, dog, with "car" blocked */
	check(!OSK_DictionaryFromMemory(&dict, "x", junk, 10, NULL, 0), "reject tiny zsd");
	check(!OSK_DictionaryFromMemory(&dict, "x", mini_zsd, 50, NULL, 0), "reject truncated zsd");
	check(OSK_DictionaryFromMemory(&dict, "Mini", mini_zsd, sizeof(mini_zsd), NULL, 0), "zsd from memory");
	cfg.dictionary = &dict;
	cfg.initial = "ca";
	osk = OSK_Create(&cfg);
	check(OSK_GetCandidateCount(osk) == 1 && candidate_is(osk, 0, "cat"), "mini dictionary, blocklist");
	OSK_SetText(osk, "Do");
	check(OSK_GetCandidateCount(osk) == 1 && candidate_is(osk, 0, "Dog"), "mini dictionary");
	OSK_Destroy(osk);

	/* random graph behind a valid header */
	memcpy(junk, mini_zsd, 40);
	for (i = 40; i < sizeof(junk); i++) {
		seed = seed * 1103515245 + 12345;
		junk[i] = seed >> 16;
	}
	junk[4] = 0;
	junk[5] = (sizeof(junk) - 34) >> 8;
	junk[6] = (sizeof(junk) - 34) & 0xFF;
	check(OSK_DictionaryFromMemory(&dict, "x", junk, sizeof(junk), junk, sizeof(junk)), "junk zsd");
	cfg.initial = "a";
	osk = OSK_Create(&cfg);
	type(osk, "bcdefg");
	OSK_SetText(osk, "e");
	OSK_SetText(osk, "t");
	check(osk != NULL, "damaged dictionary survives");
	OSK_Destroy(osk);
}

void run_tests(void)
{
	osk_config cfg;
	osk_keyboard *osk;
	char buf[8];
	int i;

	/* --- text editing --- */
	OSK_DefaultConfig(&cfg);
	cfg.max_chars = 8;
	cfg.on_event = on_event;
	osk = OSK_Create(&cfg);
	check(osk != NULL, "create");

	type(osk, "Hello");
	check(!strcmp(OSK_GetText(osk), "Hello"), "type text");
	step(osk, OSK_BTN_BACKSPACE, 0);
	check(!strcmp(OSK_GetText(osk), "Hell"), "backspace");
	step(osk, OSK_BTN_CARET_HOME, 0);
	type(osk, "X");
	check(!strcmp(OSK_GetText(osk), "XHell"), "insert at caret");
	step(osk, OSK_BTN_DELETE, 0);
	check(!strcmp(OSK_GetText(osk), "Xell"), "delete");
	rejects = 0;
	type(osk, "ABCDEF");
	check(OSK_GetLength(osk) == 8 && rejects == 2, "max length");
	check(!strcmp(OSK_GetText(osk), "XABCDell"), "insert order");

	/* UTF-8 round trip and truncation */
	OSK_SetText(osk, "\xC3\xA9t\xC3\xA9");
	check(OSK_GetLength(osk) == 3, "utf8 decode");
	check(OSK_CopyText(osk, buf, 4) == 3 && !strcmp(buf, "\xC3\xA9t"), "copy truncates on char boundary");

	/* --- key navigation, shift, More page --- */
	OSK_SetText(osk, "");
	step(osk, OSK_BTN_SELECT, 0);			/* default focus: 'q' */
	step(osk, OSK_BTN_RIGHT, 0);
	step(osk, OSK_BTN_SELECT, 0);			/* 'w' */
	step(osk, OSK_BTN_SHIFT, 0);
	step(osk, OSK_BTN_SELECT, 0);			/* 'W', shift released */
	step(osk, OSK_BTN_SELECT, 0);			/* 'w' */
	check(!strcmp(OSK_GetText(osk), "qwWw"), "navigate + shift");
	step(osk, OSK_BTN_ALT, 0);
	step(osk, OSK_BTN_UP, 0);			/* row 0, column 1 */
	step(osk, OSK_BTN_SELECT, 0);
	check(!strcmp(OSK_GetText(osk), "qwWw\xC3\xA1"), "more page");
	step(osk, OSK_BTN_LEFT, 0);
	step(osk, OSK_BTN_LEFT, 0);			/* wraps to Delete at the end of row 0 */
	step(osk, OSK_BTN_SELECT, 0);
	check(!strcmp(OSK_GetText(osk), "qwWw"), "wrap onto delete");
	click(osk, 131, 225);				/* second key of row 1 */
	check(!strcmp(OSK_GetText(osk), "qwWw\xE2\x82\xAC"), "euro");

	/* --- auto repeat: 1 press + repeats after the delay --- */
	OSK_SetText(osk, "abcdefgh");
	step(osk, OSK_BTN_BACKSPACE, 0);
	for (i = 0; i < 24 + 4 * 2; i++)
		step(osk, 0, OSK_BTN_BACKSPACE);
	check(OSK_GetLength(osk) == 4, "backspace repeat");

	/* --- pointer: the 'a' key is centered at design (153, 260) --- */
	step(osk, OSK_BTN_ALT, 0);			/* back to the letter page */
	OSK_SetText(osk, "");
	click(osk, 153, 260);
	check(!strcmp(OSK_GetText(osk), "a"), "pointer 4:3");

	/* pointer in the text box moves the caret: just after the 'a' */
	OSK_SetText(osk, "abc");
	click(osk, 51 + 17, 58);
	type(osk, "Z");
	check(!strcmp(OSK_GetText(osk), "aZbc"), "pointer caret");

	/* Caps applies to letters only, Shift to everything */
	OSK_SetText(osk, "");
	click(osk, 88, 260);				/* Caps */
	click(osk, 153, 260);				/* 'A' */
	click(osk, 67, 190);				/* '1' */
	click(osk, 93, 295);				/* Shift */
	click(osk, 153, 260);				/* Shift+Caps: 'a' */
	check(!strcmp(OSK_GetText(osk), "A1a"), "caps");

	/* --- confirm --- */
	check(step(osk, OSK_BTN_CONFIRM, 0) == OSK_STATUS_DONE, "confirm");
	OSK_Destroy(osk);

	/* --- widescreen pointer mapping --- */
	cfg.flags = OSK_FLAG_WIDESCREEN;
	osk = OSK_Create(&cfg);
	{
		float sx = (153 + (640 * 4.0f / 3.0f - 640) / 2) * 640 / (640 * 4.0f / 3.0f);
		point(osk, sx, 260, 0);
		point(osk, sx, 260, OSK_BTN_SELECT);
	}
	check(!strcmp(OSK_GetText(osk), "a"), "pointer 16:9");
	check(step(osk, OSK_BTN_CANCEL, 0) == OSK_STATUS_CANCELLED, "cancel");
	OSK_Destroy(osk);

	/* --- Enter, line breaks, Quit --- */
	OSK_DefaultConfig(&cfg);
	osk = OSK_Create(&cfg);
	OSK_SetText(osk, "a\nb");
	check(!strcmp(OSK_GetText(osk), "a b"), "no line breaks by default");
	check(click(osk, 545, 225) == OSK_STATUS_DONE, "enter confirms");
	OSK_Destroy(osk);

	cfg.flags = OSK_FLAG_MULTILINE;
	osk = OSK_Create(&cfg);
	type(osk, "ab\ncd");
	click(osk, 545, 225);
	check(!strcmp(OSK_GetText(osk), "ab\ncd\n"), "multiline enter");
	check(click(osk, 125, 425) == OSK_STATUS_CANCELLED, "quit button");
	OSK_Destroy(osk);

	cfg.flags = OSK_FLAG_NO_ENTER;
	osk = OSK_Create(&cfg);
	check(click(osk, 545, 225) == OSK_STATUS_EDITING && OSK_GetLength(osk) == 0, "no enter key");
	check(click(osk, 518, 425) == OSK_STATUS_DONE, "ok button");
	OSK_Destroy(osk);

	/* --- layout tabs --- */
	OSK_DefaultConfig(&cfg);
	osk = OSK_Create(&cfg);
	click(osk, 352, 424);				/* numpad tab */
	click(osk, 262, 252);				/* '5' */
	click(osk, 287, 424);				/* letters tab */
	click(osk, 153, 260);				/* 'a' */
	check(!strcmp(OSK_GetText(osk), "5a"), "layout tabs");
	OSK_Destroy(osk);

	cfg.flags = OSK_FLAG_LOCK_LAYOUT;
	osk = OSK_Create(&cfg);
	click(osk, 352, 424);				/* no tab here */
	click(osk, 146, 358);				/* numpad '-', nothing on the letter page */
	check(OSK_GetLength(osk) == 0, "locked layout");
	OSK_Destroy(osk);

	/* --- German and French tables --- */
	cfg.flags = 0;
	cfg.layout = OSK_LAYOUT_QWERTZ;
	osk = OSK_Create(&cfg);
	click(osk, 67, 330);				/* extra key left of the space bar */
	click(osk, 303, 225);				/* 'z' */
	check(!strcmp(OSK_GetText(osk), "[z"), "qwertz");
	OSK_Destroy(osk);

	/* UK, Italian, Spanish, Dutch: the key left of Enter's row end and the
	   first extra key of the bottom row */
	{
		static const struct { osk_layout l; const char *want; } extra[] = {
			{ OSK_LAYOUT_QWERTY_UK, ";\xC2\xA3" },	/* ';', Shift+'3' */
			{ OSK_LAYOUT_QWERTY_IT, "+\xC2\xA3" },
			{ OSK_LAYOUT_QWERTY_ES, "\xC3\xB1$" },	/* n tilde, Shift+'3' */
			{ OSK_LAYOUT_QWERTY_NL, ":#" },
		};
		unsigned k;
		bool ok = true;

		for (k = 0; k < 4; k++) {
			cfg.layout = extra[k].l;
			osk = OSK_Create(&cfg);
			click(osk, 540, 260);			/* last key of row 2 */
			click(osk, 93, 295);			/* Shift */
			click(osk, 153, 190);			/* '3' */
			if (strcmp(OSK_GetText(osk), extra[k].want))
				ok = false;
			OSK_Destroy(osk);
		}
		check(ok, "uk, it, es, nl layouts");
	}

	cfg.layout = OSK_LAYOUT_AZERTY;
	osk = OSK_Create(&cfg);
	click(osk, 67, 190);				/* '&' */
	click(osk, 88, 260);				/* Caps */
	click(osk, 67, 190);				/* '1' */
	click(osk, 67, 330);				/* OE ligature */
	check(!strcmp(OSK_GetText(osk), "&1\xC5\x92"), "azerty");
	OSK_Destroy(osk);

	/* --- numpad, filter, REQUIRE_TEXT, NO_CANCEL --- */
	OSK_DefaultConfig(&cfg);
	cfg.layout = OSK_LAYOUT_NUMPAD;
	cfg.filter = digits_only;
	cfg.flags = OSK_FLAG_REQUIRE_TEXT | OSK_FLAG_NO_CANCEL;
	cfg.on_event = on_event;
	osk = OSK_Create(&cfg);
	rejects = 0;
	check(step(osk, OSK_BTN_CONFIRM, 0) == OSK_STATUS_EDITING && rejects == 1, "require text");
	check(step(osk, OSK_BTN_CANCEL, 0) == OSK_STATUS_EDITING, "no cancel");
	type(osk, "12a3");
	check(!strcmp(OSK_GetText(osk), "123"), "filter");
	step(osk, OSK_BTN_SELECT, 0);			/* default focus '1' */
	check(!strcmp(OSK_GetText(osk), "1231"), "numpad key");
	check(step(osk, OSK_BTN_CONFIRM, 0) == OSK_STATUS_DONE, "numpad confirm");
	OSK_Resume(osk);
	check(OSK_GetStatus(osk) == OSK_STATUS_EDITING, "resume");
	OSK_Destroy(osk);

	/* --- font coverage --- */
	check(OSK_CanDisplay('A') && OSK_CanDisplay(0xE9) && OSK_CanDisplay(0xC0) &&
	      OSK_CanDisplay(0x20AC) && OSK_CanDisplay(0xA9) && OSK_CanDisplay(0x153) &&
	      !OSK_CanDisplay(0x3042) && !OSK_CanDisplay('\n'), "font coverage");

	prediction_tests();
	language_tests();
	sound_tests();
}
