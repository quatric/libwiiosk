# wiiosk

On-screen software keyboard library for Wii homebrew, built on libogc / GX.

![QWERTY, the More page, AZERTY and the numpad](docs/layouts.png)

- The look and feel of the Wii system keyboard: glass keys that grow under the pointer, a two-line
  text box with a pulsing caret, Quit / OK buttons and letters / numpad tabs in the toolbar
- QWERTY (US), QWERTZ (German), AZERTY (French) and numeric keypad layouts
- A "More" page on every letter layout with accented Latin-1 letters, `€` and extra symbols
- Shift (one-shot), Caps Lock, caret movement, auto-repeat, max length, password mode, optional
  line breaks
- Input from the Wii Remote pointer and buttons, the Classic Controller, the GameCube controller and USB keyboards
- 4:3 and 16:9 (anamorphic) output, NTSC and PAL
- UTF-8 in and out
- No asset files: every texture is generated at startup, and the anti-aliased proportional font
  (Zen Maru Gothic, SIL Open Font License) is compiled in
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
cfg.title         = "Enter your name";   /* placeholder while the box is empty */
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
    OSK_Draw(osk);          /* covers the whole screen */
    /* GX_DrawDone / GX_CopyDisp / VIDEO_SetNextFramebuffer as usual */
}
OSK_Destroy(osk);
```

`OSK_Draw()` changes GX state: projection, position matrix 0, vertex format 0, TEV stages 0-1,
TEV registers 0-1, texmap 0, channels, blending, alpha compare and Z mode. Set them again before
your own drawing. The textures are built by the first `OSK_Create()` (about 0.4 MB, kept until exit).

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
| More page     |            | Y       | Z             |               |
| Space         |            | X       | X             | Space         |
| OK            | +          | +       | Start         | Enter (line break with `OSK_FLAG_MULTILINE`) |
| Cancel        | HOME       | HOME    | (Quit button) | Esc           |

To move the caret with the pointer, point into the text box and press A. The arrows that appear
at the right of the text box move the caret a line up or down.

## Options

- `filter`: a callback that refuses code points, for example digits only. Keys whose character is
  refused are drawn greyed out.
- `on_event`: a callback for focus changes, key presses, rejects, done and cancel. Use it for sounds
  or rumble; `example/gx` uses it for Wii-Menu-style rumble.
- `theme`: copy `*OSK_DefaultTheme()` and change the colours you want.
- `theme`: copy `*OSK_DefaultTheme()` and change the colours you want. Since 2.0 only some fields
  have an effect; `wiiosk.h` lists them.
- Flags:
  - `OSK_FLAG_PASSWORD`: show `*` for every character
  - `OSK_FLAG_WIDESCREEN`
  - `OSK_FLAG_NO_CANCEL`: hide Quit
  - `OSK_FLAG_REQUIRE_TEXT`: OK is refused while the text is empty
  - `OSK_FLAG_NO_POINTER`
  - `OSK_FLAG_GX_READY`
  - `OSK_FLAG_MULTILINE`: Enter inserts `\n` instead of confirming
  - `OSK_FLAG_NO_ENTER`: hide the Enter key
  - `OSK_FLAG_LOCK_LAYOUT`: hide the letters / numpad tabs
  - `OSK_FLAG_NO_HINTS`: no effect since 2.0

The keyboard accepts any character `OSK_CanDisplay()` reports. That covers printable ASCII,
Latin-1, `Œ œ` and `€`. It does not include CJK.

## Upgrading from 1.x

- The keyboard now fills the screen and uses the Wii system keyboard layout. Pointer coordinates
  of keys changed, and the default focus is `q`.
- `title` is a placeholder inside the empty text box instead of a heading.
- The Alt key is the "More" button; the Cancel key is the Quit button in the toolbar. The ← / →
  caret keys are gone; use the 1 / 2 buttons or the pointer.
- The help line and the character counter are gone, so `OSK_FLAG_NO_HINTS` does nothing.
- On the German and French layouts, Caps and Shift follow the tables of the Wii keyboard (French
  Caps gives digits on the top row).

## Tests

`tests/selftest` drives `OSK_Update()` with scripted input and needs no controller. It covers
editing, max length, UTF-8, navigation and wrap, Shift and Caps, the More page, auto-repeat, 4:3 and
16:9 pointer mapping, the caret pointer, Enter and line breaks, the toolbar buttons and tabs, the
German and French tables, filters and flags. On the Wii the verdict appears in the text box
(`SELFTEST PASS n/n`), then it cycles through every layout for a visual check. It runs in Dolphin.

The same cases also build for the desktop, together with a preview tool that draws the keyboard
with a software rasterizer that follows the GX path:

```sh
make -C tools/preview
tools/preview/wiiosk_selftest
tools/preview/wiiosk_preview out_dir 2     # one PNG per scene, at 2x
```

## Layout of this folder

```
include/wiiosk.h        public API
source/                 library sources
example/gx              GX integration example
example/console         OSK_Prompt() example
tests/selftest          scripted self-test
tools/mkfont.py         generates source/osk_font_data.c from the font
tools/fonts/OFL.txt     the font's license
tools/preview           desktop build: previews and self-test
nintendo_sdk_original/  original SDK package (local reference only, git-ignored)
```

## Provenance

This folder used to hold Nintendo's 2009 *Software Keyboard* SDK sample, which is NintendoWare/RVL
SDK code. That material is kept in `nintendo_sdk_original/` only for local reference and must not be
redistributed. wiiosk is a separate, original implementation that uses only libogc. The look is
modelled on the Wii system keyboard and recreated with procedurally generated graphics; no Nintendo
code, layouts, textures, fonts or sounds are included.

The font is Zen Maru Gothic Medium, (c) 2021 The Zen Maru Gothic Project Authors, under the SIL Open Font License 1.1
(`tools/fonts/OFL.txt`). `source/osk_font_data.c` holds glyph bitmaps rendered from it by
`tools/mkfont.py`.
