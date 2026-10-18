/*-------------------------------------------------------------
wiiosk with libinput-wii: every controller, the USB keyboard and
the USB mouse reach the keyboard through OSK_ReadInputWii().
-------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <input_wii.h>
#include <wiiosk.h>
#include <wiiosk_input_wii.h>

#define FIFO_SIZE	(256 * 1024)

static GXRModeObj *rmode;
static void *xfb[2];
static int fb;

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

int main(int argc, char **argv)
{
	osk_config cfg;
	osk_keyboard *osk;
	osk_input in;
	unsigned frame = 0;

	init_video();
	INPUT_Init();			/* pads, Wii Remotes, USB keyboard and mouse */
	INPUT_SetScreenSize(rmode->fbWidth, rmode->efbHeight);

	OSK_DefaultConfig(&cfg);
	cfg.title = "Type with anything";
	cfg.screen_width = rmode->fbWidth;
	cfg.screen_height = rmode->efbHeight;
	cfg.dictionary = &osk_dict_en_us;
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		cfg.flags |= OSK_FLAG_WIDESCREEN;
	osk = OSK_Create(&cfg);
	if (!osk)
		return 1;

	for (;;) {
		INPUT_Update();
		OSK_ReadInputWii(&in);
		if (OSK_Update(osk, &in) != OSK_STATUS_EDITING) {
			/* show the result as the next placeholder */
			static char last[OSK_MAX_CHARS * 4 + 1];
			if (OSK_GetStatus(osk) == OSK_STATUS_CANCELLED)
				break;
			OSK_CopyText(osk, last, sizeof(last));
			OSK_Destroy(osk);
			cfg.title = last;
			osk = OSK_Create(&cfg);
			if (!osk)
				break;
		}
		begin_frame(frame++);
		OSK_Draw(osk);
		end_frame();
	}
	OSK_Destroy(osk);
	INPUT_Shutdown();
	return 0;
}
