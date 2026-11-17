/*-------------------------------------------------------------
wiiosk_sdl.h -- draw wiiosk with an SDL2 renderer (Wii U)

On the Wii U libwiiuosk.a draws through SDL2 instead of GX. Hand it
the renderer once, then use OSK_Create / OSK_Update / OSK_Draw as on
the Wii:

	OSK_SDL_SetRenderer(renderer);
	osk_keyboard *osk = OSK_Create(&cfg);
	...
	OSK_Update(osk, &in);
	draw_my_scene();
	OSK_Draw(osk);		// covers the whole screen
	SDL_RenderPresent(renderer);

The keyboard's screen_width x screen_height is mapped onto the
renderer's logical size if one is set, otherwise onto its output.
OSK_Draw() leaves the renderer's draw colour and blend mode as they
were. Call OSK_SDL_Shutdown() before destroying the renderer; it
frees the keyboard's textures.

Not on the Wii U: OSK_Prompt(), OSK_ReadInput(),
OSK_InitUSBKeyboard() and OSK_InitSound() (ASND). Read the
controllers with wiiosk_input_wiiu.h (libwiiuinput) or fill
osk_input yourself; play sounds from cfg.on_sound with
OSK_GetSound().

This software is provided 'as-is', without any express or implied
warranty. See LICENSE for the full zlib license text.
-------------------------------------------------------------*/
#ifndef __WIIOSK_SDL_H__
#define __WIIOSK_SDL_H__

#include <SDL2/SDL.h>

#include "wiiosk.h"

#ifdef __cplusplus
extern "C" {
#endif

void OSK_SDL_SetRenderer(SDL_Renderer *renderer);
void OSK_SDL_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIIOSK_SDL_H__ */
