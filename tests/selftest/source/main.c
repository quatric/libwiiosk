/*-------------------------------------------------------------
wiiosk self-test: runs the cases in tests.c and shows the verdict
in the keyboard's text box. Run it in Dolphin or on hardware; no
controller needed.
-------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wiiosk.h>

#include "tests.h"

#define FIFO_SIZE	(256 * 1024)

static GXRModeObj *rmode;
static void *xfb[2];
static int fb;

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
	char text[2][320];
	osk_config cfg;
	osk_keyboard *osk;

	WPAD_Init();
	run_tests();
	init_video();

	/* the verdict goes on the first line of the text box */
	if (failed)
		snprintf(text[0], sizeof(text[0]), "FAIL %d/%d: %s\n", passed, passed + failed, first_failure);
	else
		snprintf(text[0], sizeof(text[0]), "SELFTEST PASS %d/%d\n", passed, passed);
	strcpy(text[1], text[0]);
	strcat(text[0], "\xC3\x80 \xC3\x89 \xC3\x91 \xC3\x9C \xC3\x98 \xC3\xB8 \xC3\xA7 \xC3\xBD \xE2\x82\xAC \xC5\x93 \xC2\xA9");
	strcat(text[1], "3.14159");

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
			cfg.initial = text[pages[page].layout == OSK_LAYOUT_NUMPAD];
			cfg.max_chars = OSK_MAX_CHARS;
			cfg.layout = pages[page].layout;
			cfg.screen_width = rmode->fbWidth;
			cfg.screen_height = rmode->efbHeight;
			cfg.flags = OSK_FLAG_MULTILINE;
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
