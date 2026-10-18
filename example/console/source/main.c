/*-------------------------------------------------------------
wiiosk console example: a text-mode app that pops up the
keyboard with the blocking OSK_Prompt().
-------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wiiosk.h>

static void *xfb;
static GXRModeObj *rmode;

static void print_menu(const char *last)
{
	printf("\x1b[2J\x1b[2;0H");
	printf("  wiiosk " WIIOSK_VERSION_STRING " console example\n\n");
	printf("  A     enter a name (QWERTY)\n");
	printf("  1     enter a number (numpad)\n");
	printf("  HOME  exit\n\n");
	if (last)
		printf("  You entered: \"%s\"\n", last);
}

int main(int argc, char **argv)
{
	char buf[128];
	osk_config cfg;
	osk_status st;

	VIDEO_Init();
	WPAD_Init();
	PAD_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	OSK_InitUSBKeyboard();
	print_menu(NULL);

	for (;;) {
		u32 down;

		WPAD_ScanPads();
		PAD_ScanPads();
		down = WPAD_ButtonsDown(0);

		if (down & WPAD_BUTTON_HOME)
			break;
		if (down & (WPAD_BUTTON_A | WPAD_BUTTON_1)) {
			OSK_DefaultConfig(&cfg);
			if (down & WPAD_BUTTON_A) {
				cfg.title = "What is your name?";
				cfg.max_chars = 24;
			} else {
				cfg.title = "Enter a number";
				cfg.layout = OSK_LAYOUT_NUMPAD;
				cfg.max_chars = 10;
			}
			if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
				cfg.flags |= OSK_FLAG_WIDESCREEN;

			st = OSK_Prompt(&cfg, rmode, xfb, buf, sizeof(buf));
			/* the prompt changed the Wii Remote's data format; restore ours */
			WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC);
			print_menu(st == OSK_STATUS_DONE ? buf : "(cancelled)");
		}
		VIDEO_WaitVSync();
	}

	OSK_ShutdownUSBKeyboard();
	return 0;
}
