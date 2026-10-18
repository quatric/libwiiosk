/*-------------------------------------------------------------
wiiosk self-test: drives OSK_Update() with scripted input and
shows the verdict in the keyboard title. Run it in Dolphin or
on hardware; no controller needed.
-------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wiiosk.h>

#define FIFO_SIZE	(256 * 1024)

static GXRModeObj *rmode;
static void *xfb[2];
static int fb;

static int passed, failed;
static char first_failure[96];
static int rejects;

static void check(bool ok, const char *what)
{
	if (ok) {
		passed++;
		return;
	}
	failed++;
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

static void point(osk_keyboard *osk, float x, float y, u32 down)
{
	osk_input in;
	memset(&in, 0, sizeof(in));
	in.pointer_valid = true;
	in.pointer_x = x;
	in.pointer_y = y;
	in.down = down;
	in.held = down;
	OSK_Update(osk, &in);
}

static bool digits_only(uint32_t cp, void *user)
{
	return cp >= '0' && cp <= '9';
}

static void run_tests(void)
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

	/* --- key navigation, shift, alt page --- */
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
	check(!strcmp(OSK_GetText(osk), "qwWw\xC3\xA1"), "alt page");
	step(osk, OSK_BTN_LEFT, 0);
	step(osk, OSK_BTN_LEFT, 0);			/* wraps to the last key of row 0 */
	step(osk, OSK_BTN_SELECT, 0);
	check(!strcmp(OSK_GetText(osk), "qwWw\xC3\xA1\xE2\x82\xAC"), "wrap + euro");

	/* --- auto repeat: 1 press + repeats after the delay --- */
	OSK_SetText(osk, "abcdefgh");
	step(osk, OSK_BTN_BACKSPACE, 0);
	for (i = 0; i < 24 + 4 * 2; i++)
		step(osk, 0, OSK_BTN_BACKSPACE);
	check(OSK_GetLength(osk) == 4, "backspace repeat");

	/* --- pointer: centre of the 'a' key is design (54, 289) --- */
	step(osk, OSK_BTN_ALT, 0);			/* back to the letter page */
	OSK_SetText(osk, "");
	point(osk, 54, 289, 0);
	point(osk, 54, 289, OSK_BTN_SELECT);
	check(!strcmp(OSK_GetText(osk), "a"), "pointer 4:3");

	/* pointer in the text box moves the caret */
	OSK_SetText(osk, "abc");
	point(osk, 32 + 12 + 16 * 1, 110, 0);
	point(osk, 32 + 12 + 16 * 1, 110, OSK_BTN_SELECT);
	type(osk, "Z");
	check(!strcmp(OSK_GetText(osk), "aZbc"), "pointer caret");

	/* --- confirm --- */
	check(step(osk, OSK_BTN_CONFIRM, 0) == OSK_STATUS_DONE, "confirm");
	OSK_Destroy(osk);

	/* --- widescreen pointer mapping --- */
	cfg.flags = OSK_FLAG_WIDESCREEN;
	osk = OSK_Create(&cfg);
	{
		float sx = (54 + (640 * 4.0f / 3.0f - 640) / 2) * 640 / (640 * 4.0f / 3.0f);
		point(osk, sx, 289, 0);
		point(osk, sx, 289, OSK_BTN_SELECT);
	}
	check(!strcmp(OSK_GetText(osk), "a"), "pointer 16:9");
	check(step(osk, OSK_BTN_CANCEL, 0) == OSK_STATUS_CANCELLED, "cancel");
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
	step(osk, OSK_BTN_SELECT, 0);			/* default focus '4' */
	check(!strcmp(OSK_GetText(osk), "1234"), "numpad key");
	check(step(osk, OSK_BTN_CONFIRM, 0) == OSK_STATUS_DONE, "numpad confirm");
	OSK_Resume(osk);
	check(OSK_GetStatus(osk) == OSK_STATUS_EDITING, "resume");
	OSK_Destroy(osk);

	/* --- font coverage --- */
	check(OSK_CanDisplay('A') && OSK_CanDisplay(0xE9) && OSK_CanDisplay(0xC0) &&
	      OSK_CanDisplay(0x20AC) && !OSK_CanDisplay(0x3042) && !OSK_CanDisplay(0xA9), "font coverage");
}

static void init_video(void)
{
	void *fifo;
	u32 xfb_height;

	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb[0]);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	fifo = memalign(32, FIFO_SIZE);
	memset(fifo, 0, FIFO_SIZE);
	GX_Init(fifo, FIFO_SIZE);
	GX_SetCopyClear((GXColor){ failed ? 0x80 : 0x10, failed ? 0x10 : 0x60, 0x20, 0xFF }, GX_MAX_Z24);
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	xfb_height = GX_SetDispCopyYScale(GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight));
	GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfb_height);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,
			(rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE);
	GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
}

int main(int argc, char **argv)
{
	char title[160];
	osk_config cfg;
	osk_keyboard *osk;

	WPAD_Init();
	run_tests();
	init_video();

	if (failed)
		snprintf(title, sizeof(title), "FAIL %d/%d: %s", passed, passed + failed, first_failure);
	else
		snprintf(title, sizeof(title), "SELFTEST PASS %d/%d", passed, passed);

	/* show every layout in turn: 90 frames each */
	static const struct { osk_layout layout; bool alt; } pages[] = {
		{ OSK_LAYOUT_QWERTY, false }, { OSK_LAYOUT_QWERTY, true },
		{ OSK_LAYOUT_QWERTZ, false }, { OSK_LAYOUT_AZERTY, false },
		{ OSK_LAYOUT_NUMPAD, false },
	};
	unsigned frame = 0, page = 0;

	osk = NULL;
	for (;;) {
		if (frame % 90 == 0) {
			OSK_Destroy(osk);
			OSK_DefaultConfig(&cfg);
			cfg.title = title;
			cfg.initial = pages[page].layout == OSK_LAYOUT_NUMPAD ? "3.14159" :
				"\xC3\x80 \xC3\x89 \xC3\x91 \xC3\x9C \xC3\x98 \xC3\xB8 \xC3\xA7 \xC3\xBD \xE2\x82\xAC";
			cfg.layout = pages[page].layout;
			cfg.screen_width = rmode->fbWidth;
			cfg.screen_height = rmode->efbHeight;
			cfg.flags = OSK_FLAG_NO_HINTS;
			osk = OSK_Create(&cfg);
			if (pages[page].alt) {
				osk_input in = { .down = OSK_BTN_ALT };
				OSK_Update(osk, &in);
			}
			page = (page + 1) % (sizeof(pages) / sizeof(pages[0]));
		}
		frame++;

		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
			break;
		OSK_Update(osk, NULL);
		GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
		OSK_Draw(osk);
		GX_DrawDone();
		GX_CopyDisp(xfb[fb], GX_TRUE);
		GX_Flush();
		VIDEO_SetNextFramebuffer(xfb[fb]);
		VIDEO_Flush();
		VIDEO_WaitVSync();
		fb ^= 1;
	}
	return 0;
}
