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
}
