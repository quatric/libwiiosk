/*-------------------------------------------------------------
wiiosk GX example: the keyboard drawn inside the app's own
render loop with OSK_Update() / OSK_Draw().
-------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wiiosk.h>

#define FIFO_SIZE	(256 * 1024)

static GXRModeObj *rmode;
static void *xfb[2];
static int fb;
static int rumble_frames;

static void init_video(void)
{
	void *fifo;
	f32 yscale;
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
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	fifo = memalign(32, FIFO_SIZE);
	memset(fifo, 0, FIFO_SIZE);
	GX_Init(fifo, FIFO_SIZE);
	GX_SetCopyClear((GXColor){ 0, 0, 0, 0xFF }, GX_MAX_Z24);
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
	xfb_height = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfb_height);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,
			(rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE);
	GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_CopyDisp(xfb[1], GX_TRUE);
}

/* Stand-in for the app's own scene: a slowly shifting clear color. */
static void begin_frame(unsigned frame)
{
	float t = frame / 120.0f;
	GX_SetCopyClear((GXColor){
		(u8)(40 + 30 * sinf(t)), (u8)(70 + 30 * sinf(t + 2.0f)), (u8)(110 + 40 * sinf(t + 4.0f)), 0xFF
	}, GX_MAX_Z24);
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
}

static void end_frame(void)
{
	GX_DrawDone();
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(xfb[fb], GX_TRUE);
	GX_Flush();
	VIDEO_SetNextFramebuffer(xfb[fb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	fb ^= 1;
}

static void on_event(osk_keyboard *osk, osk_event ev, void *user)
{
	/* short buzz like the Wii Menu when the pointer moves onto a key */
	if (ev == OSK_EVENT_FOCUS || ev == OSK_EVENT_REJECT) {
		WPAD_Rumble(WPAD_CHAN_0, 1);
		rumble_frames = ev == OSK_EVENT_REJECT ? 6 : 2;
	}
}

static bool digits_only(uint32_t cp, void *user)
{
	return cp >= '0' && cp <= '9';
}

int main(int argc, char **argv)
{
	static const struct {
		osk_layout layout;
		unsigned flags, max;
		const char *title;
		bool (*filter)(uint32_t, void *);
		const osk_dictionary *dictionary;
	} demos[] = {
		{ OSK_LAYOUT_QWERTY, 0, 64, "Write a message", NULL, NULL },	/* system language */
		{ OSK_LAYOUT_QWERTZ, OSK_FLAG_PASSWORD | OSK_FLAG_REQUIRE_TEXT, 16, "Passwort", NULL, NULL },
		{ OSK_LAYOUT_AZERTY, 0, 64, "Message", NULL, &osk_dict_fr },
		{ OSK_LAYOUT_NUMPAD, OSK_FLAG_REQUIRE_TEXT, 4, "PIN (4 digits)", digits_only, NULL },
	};
	char title[160], result[OSK_MAX_CHARS * 4 + 1] = "";
	osk_config cfg;
	osk_keyboard *osk = NULL;
	osk_input in;
	unsigned frame = 0, demo = 0;
	bool widescreen;

	init_video();
	WPAD_Init();
	PAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_0, rmode->fbWidth, rmode->efbHeight);
	OSK_InitUSBKeyboard();	/* optional */
	OSK_InitSound(true);	/* optional, -lasnd */
	widescreen = CONF_GetAspectRatio() == CONF_ASPECT_16_9;

	for (;;) {
		if (!osk) {
			OSK_DefaultConfig(&cfg);
			cfg.layout = demos[demo].layout;
			cfg.flags = demos[demo].flags | (widescreen ? OSK_FLAG_WIDESCREEN : 0);
			cfg.max_chars = demos[demo].max;
			cfg.filter = demos[demo].filter;
			cfg.dictionary = demos[demo].dictionary;
			if (demo == 0)
				OSK_UseSystemLanguage(&cfg);	/* layout + dictionary */
			cfg.on_event = on_event;
			cfg.screen_width = rmode->fbWidth;
			cfg.screen_height = rmode->efbHeight;
			if (result[0])
				snprintf(title, sizeof(title), "%s (last: %.40s)", demos[demo].title, result);
			else
				snprintf(title, sizeof(title), "%s", demos[demo].title);
			cfg.title = title;
			osk = OSK_Create(&cfg);
			if (!osk)
				break;
		}

		WPAD_ScanPads();
		PAD_ScanPads();
		OSK_ReadInput(&in, WPAD_CHAN_0, PAD_CHAN0);

		switch (OSK_Update(osk, &in)) {
		case OSK_STATUS_DONE:
			OSK_CopyText(osk, result, sizeof(result));
			OSK_Destroy(osk);
			osk = NULL;
			demo = (demo + 1) % (sizeof(demos) / sizeof(demos[0]));
			continue;
		case OSK_STATUS_CANCELLED:
			OSK_Destroy(osk);
			OSK_ShutdownUSBKeyboard();
			WPAD_Rumble(WPAD_CHAN_0, 0);
			exit(0);
		default:
			break;
		}

		if (rumble_frames && --rumble_frames == 0)
			WPAD_Rumble(WPAD_CHAN_0, 0);

		begin_frame(frame++);
		/* ...the app would draw its own scene here... */
		OSK_Draw(osk);
		end_frame();
	}
	return 0;
}
