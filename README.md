# wiiosk

On-screen software keyboard library for Wii homebrew, built on libogc / GX.

![Layouts running in Dolphin](docs/layouts.png)

- QWERTY (US), QWERTZ (German), AZERTY (French) and numeric keypad layouts
- An Alt page on every letter layout with accented Latin-1 letters, `€` and extra symbols
- Shift (one-shot), Caps Lock, caret movement, auto-repeat, max length, password mode
- Input from the Wii Remote pointer and buttons, the Classic Controller, the GameCube controller and USB keyboards
- 4:3 and 16:9 (anamorphic) output, NTSC and PAL
- UTF-8 in and out
- No asset files: glyphs are generated at startup from libogc's built-in console font, with accented capitals composed in code
- Two ways to use it:
  - Inside your own GX render loop with `OSK_Update()` and `OSK_Draw()`.
  - As a blocking `OSK_Prompt()` for console/framebuffer apps.

## Building

Needs devkitPPC and libogc.

```sh
make                 # lib/libwiiosk.a
make install         # copies header + lib to $DEVKITPRO/portlibs/wii
make example         # example/gx/wiiosk_gx.dol, example/console/wiiosk_console.dol
make -C tests/selftest
```

Link with `-lwiiosk -lwiiuse -lbte -logc -lm`. Add `-lwiikeyboard` only if you call
`OSK_InitUSBKeyboard()`.

## Usage in a GX app

```c
#include <wiiosk.h>

osk_config cfg;
OSK_DefaultConfig(&cfg);
cfg.title         = "Enter your name";
cfg.initial       = "Player";
cfg.max_chars     = 24;
cfg.layout        = OSK_LAYOUT_QWERTY;
cfg.screen_width  = rmode->fbWidth;
cfg.screen_height = rmode->efbHeight;
if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
    cfg.flags |= OSK_FLAG_WIDESCREEN;

/* pointer support */
WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
WPAD_SetVRes(WPAD_CHAN_0, rmode->fbWidth, rmode->efbHeight);

osk_keyboard *osk = OSK_Create(&cfg);

for (;;) {
    osk_input in;
    WPAD_ScanPads();
    PAD_ScanPads();
    OSK_ReadInput(&in, WPAD_CHAN_0, PAD_CHAN0);   /* -1 skips a controller type */

    osk_status st = OSK_Update(osk, &in);
    if (st == OSK_STATUS_DONE) { use(OSK_GetText(osk)); break; }
    if (st == OSK_STATUS_CANCELLED) break;

    draw_my_scene();
    OSK_Draw(osk);          /* overlays a dimmed backdrop + keyboard */
    /* GX_DrawDone / GX_CopyDisp / VIDEO_SetNextFramebuffer as usual */
}
OSK_Destroy(osk);
```

`OSK_Draw()` changes GX state: projection, position matrix 0, vertex format 0, TEV stage 0, texmap 0,
channels, blending and Z mode. Set them again before your own drawing.

## Usage from a console app

```c
char name[64];
osk_config cfg;
OSK_DefaultConfig(&cfg);
cfg.title = "What is your name?";
if (OSK_Prompt(&cfg, rmode, xfb, name, sizeof(name)) == OSK_STATUS_DONE)
    printf("Hello %s\n", name);
```

`OSK_Prompt()` initialises GX on first use. Pass `OSK_FLAG_GX_READY` if your app has already
initialised GX. It takes over the display and restores your framebuffer contents when it returns.
It switches the Wii Remote on channel 0 to IR mode, so set your own data format again afterwards.

## Controls

| Action        | Wii Remote | Classic | GameCube      | USB keyboard  |
|---------------|------------|---------|---------------|---------------|
| Press key     | A (point or D-pad) | A | A          | (type)        |
| Move focus    | D-pad      | D-pad   | D-pad / stick |               |
| Erase         | B          | B       | B             | Backspace / Delete |
| Move caret    | 1 / 2      | L / R   | L / R         | ← → Home End  |
| Shift         | −          | −       | Y             | (keyboard's own) |
| Alt page      |            | Y       | Z             |               |
| Space         |            | X       | X             | Space         |
| OK            | +          | +       | Start         | Enter         |
| Cancel        | HOME       | HOME    | (Cancel key)  | Esc           |

To move the caret with the pointer, point into the text box and press A.

## Options

- `filter`: a callback that refuses code points, for example digits only. Keys whose character is
  refused are drawn greyed out.
- `on_event`: a callback for focus changes, key presses, rejects, done and cancel. Use it for sounds
  or rumble; `example/gx` uses it for Wii-Menu-style rumble.
- `theme`: copy `*OSK_DefaultTheme()` and change the colours you want.
- Flags:
  - `OSK_FLAG_PASSWORD`
  - `OSK_FLAG_WIDESCREEN`
  - `OSK_FLAG_NO_CANCEL`
  - `OSK_FLAG_REQUIRE_TEXT`
  - `OSK_FLAG_NO_HINTS`
  - `OSK_FLAG_NO_POINTER`
  - `OSK_FLAG_GX_READY`

The keyboard accepts any character `OSK_CanDisplay()` reports. That covers printable ASCII,
nearly all of Latin-1 and `€`. It does not include CJK.

## Tests

`tests/selftest` drives `OSK_Update()` with scripted input and needs no controller. It covers
editing, max length, UTF-8, navigation and wrap, shift, the Alt page, auto-repeat, 4:3 and 16:9
pointer mapping, the caret pointer, filters and flags. The verdict appears in the keyboard title
(`SELFTEST PASS n/n`). It then cycles through every layout for a visual check. It runs in Dolphin.

## Layout of this folder

```
include/wiiosk.h        public API
source/                 library sources
example/gx              GX integration example
example/console         OSK_Prompt() example
tests/selftest          scripted self-test
nintendo_sdk_original/  original SDK package (local reference only, git-ignored)
```

## Provenance

This folder used to hold Nintendo's 2009 *Software Keyboard* SDK sample, which is NintendoWare/RVL
SDK code. That material is kept in `nintendo_sdk_original/` only for local reference and must not be
redistributed. wiiosk is a separate, original implementation that uses only libogc. It does not
include or derive from any of Nintendo's code, layouts, textures, fonts or sounds. The glyphs come
from libogc's console font, which is under libogc's own license.
