/*-------------------------------------------------------------
wiiosk.h -- on-screen software keyboard for Wii homebrew (libogc)

This software is provided 'as-is', without any express or implied
warranty. See LICENSE for the full zlib license text.
-------------------------------------------------------------*/
#ifndef __WIIOSK_H__
#define __WIIOSK_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <gccore.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIIOSK_VERSION_MAJOR	1
#define WIIOSK_VERSION_MINOR	0
#define WIIOSK_VERSION_PATCH	0
#define WIIOSK_VERSION_STRING	"1.0.0"

/* Hard upper bound for the number of characters (code points) in the buffer. */
#define OSK_MAX_CHARS		256

typedef struct osk_keyboard osk_keyboard;

/* Key arrangement. Every letter layout also has an "Alt" page with accented
   Latin-1 letters and extra symbols. */
typedef enum {
	OSK_LAYOUT_QWERTY = 0,	/* US English */
	OSK_LAYOUT_QWERTZ,	/* German */
	OSK_LAYOUT_AZERTY,	/* French */
	OSK_LAYOUT_NUMPAD	/* digits, '.', '-' */
} osk_layout;

enum {
	OSK_FLAG_PASSWORD	= 1 << 0,	/* show '*' instead of the text */
	OSK_FLAG_WIDESCREEN	= 1 << 1,	/* 16:9 output, compensate horizontal stretch */
	OSK_FLAG_NO_CANCEL	= 1 << 2,	/* hide Cancel key, ignore cancel buttons */
	OSK_FLAG_REQUIRE_TEXT	= 1 << 3,	/* OK is rejected while the text is empty */
	OSK_FLAG_NO_HINTS	= 1 << 4,	/* hide the button help line */
	OSK_FLAG_NO_POINTER	= 1 << 5,	/* do not draw the Wii Remote pointer */
	OSK_FLAG_GX_READY	= 1 << 6	/* OSK_Prompt(): GX was already initialised by the app */
};

typedef enum {
	OSK_STATUS_EDITING = 0,
	OSK_STATUS_DONE,	/* OK pressed, text accepted */
	OSK_STATUS_CANCELLED
} osk_status;

/* Delivered through osk_config.on_event, e.g. to play sounds or rumble. */
typedef enum {
	OSK_EVENT_FOCUS = 0,	/* the highlighted key changed */
	OSK_EVENT_PRESS,	/* a key was activated */
	OSK_EVENT_REJECT,	/* input refused (full buffer, filter, empty OK) */
	OSK_EVENT_DONE,
	OSK_EVENT_CANCEL
} osk_event;

/* RGBA8 colors, 0xRRGGBBAA. */
typedef struct {
	u32 overlay;		/* full-screen dim drawn behind the keyboard */
	u32 panel;
	u32 key_top, key_bottom;
	u32 special_top, special_bottom;
	u32 focus_top, focus_bottom;
	u32 active;		/* latched Shift / Caps / Alt */
	u32 key_text;
	u32 key_text_disabled;
	u32 textbox;
	u32 textbox_border;
	u32 text;
	u32 caret;
	u32 title;
	u32 hint;
} osk_theme;

typedef struct {
	const char *title;		/* UTF-8 prompt above the text box, may be NULL */
	const char *initial;		/* UTF-8 initial text, may be NULL */
	unsigned max_chars;		/* 1..OSK_MAX_CHARS, 0 means OSK_MAX_CHARS */
	osk_layout layout;
	unsigned flags;			/* OSK_FLAG_* */
	unsigned screen_width;		/* render target, usually rmode->fbWidth (0 = 640) */
	unsigned screen_height;		/* usually rmode->efbHeight (0 = 480) */
	const osk_theme *theme;		/* NULL selects the built-in theme */

	/* Optional: return false to refuse a code point. Keys whose character is
	   refused are drawn disabled. */
	bool (*filter)(uint32_t codepoint, void *user);
	void (*on_event)(osk_keyboard *osk, osk_event event, void *user);
	void *user;
} osk_config;

/* Abstract buttons understood by OSK_Update(). */
enum {
	OSK_BTN_SELECT		= 1 << 0,	/* press focused key */
	OSK_BTN_BACKSPACE	= 1 << 1,
	OSK_BTN_DELETE		= 1 << 2,
	OSK_BTN_UP		= 1 << 3,	/* move key focus */
	OSK_BTN_DOWN		= 1 << 4,
	OSK_BTN_LEFT		= 1 << 5,
	OSK_BTN_RIGHT		= 1 << 6,
	OSK_BTN_CARET_LEFT	= 1 << 7,	/* move text cursor */
	OSK_BTN_CARET_RIGHT	= 1 << 8,
	OSK_BTN_CARET_HOME	= 1 << 9,
	OSK_BTN_CARET_END	= 1 << 10,
	OSK_BTN_SHIFT		= 1 << 11,
	OSK_BTN_ALT		= 1 << 12,	/* toggle accent / symbol page */
	OSK_BTN_SPACE		= 1 << 13,
	OSK_BTN_CONFIRM		= 1 << 14,
	OSK_BTN_CANCEL		= 1 << 15
};

#define OSK_INPUT_MAX_TEXT	32

/* One frame of input. Fill it with OSK_ReadInput() or build it yourself. */
typedef struct {
	u32 down;			/* OSK_BTN_* newly pressed this frame */
	u32 held;			/* OSK_BTN_* currently held (enables auto-repeat) */
	bool pointer_valid;
	float pointer_x, pointer_y;	/* in screen_width x screen_height pixels */
	uint32_t text[OSK_INPUT_MAX_TEXT];	/* typed code points (USB keyboard) */
	unsigned text_count;
} osk_input;

/* Fill cfg with defaults: QWERTY, 640x480, 64 chars, built-in theme. */
void OSK_DefaultConfig(osk_config *cfg);
const osk_theme *OSK_DefaultTheme(void);

osk_keyboard *OSK_Create(const osk_config *cfg);
void OSK_Destroy(osk_keyboard *osk);

/* Gather input after WPAD_ScanPads() / PAD_ScanPads(). Pass -1 to skip a
   controller type. The Wii Remote channel needs WPAD_FMT_BTNS_ACC_IR and
   WPAD_SetVRes(chan, screen_width, screen_height) for pointing. USB keyboard
   events are included once OSK_InitUSBKeyboard() succeeded. */
void OSK_ReadInput(osk_input *in, int wpad_chan, int pad_chan);

/* Optional USB keyboard support through libwiikeyboard (link -lwiikeyboard). */
bool OSK_InitUSBKeyboard(void);
void OSK_ShutdownUSBKeyboard(void);

/* Advance one frame. Call once per frame with that frame's input. */
osk_status OSK_Update(osk_keyboard *osk, const osk_input *in);

/* Render with GX inside the caller's frame (between clearing and
   GX_CopyDisp). Assumes a full-screen viewport. Clobbers projection, position
   matrix 0, vertex descriptors/format 0, TEV stage 0, texmap 0, channels,
   blending and Z mode; re-set them before your own drawing. */
void OSK_Draw(osk_keyboard *osk);

osk_status OSK_GetStatus(const osk_keyboard *osk);

/* Current text as UTF-8. The pointer stays valid until the next call that
   modifies the keyboard. */
const char *OSK_GetText(osk_keyboard *osk);
/* Copy UTF-8 text into buf (always NUL terminated, never splits a character).
   Returns the number of bytes written excluding the terminator. */
size_t OSK_CopyText(osk_keyboard *osk, char *buf, size_t size);
void OSK_SetText(osk_keyboard *osk, const char *utf8);
unsigned OSK_GetLength(const osk_keyboard *osk);

/* Return to OSK_STATUS_EDITING, keeping the text (e.g. after validation failed). */
void OSK_Resume(osk_keyboard *osk);

/* Self-contained blocking prompt for apps that render with the console or
   their own framebuffer. Takes over the display until OK or Cancel, then
   restores the framebuffer contents. WPAD_Init()/PAD_Init() must have been
   called. GX is initialised on first use unless OSK_FLAG_GX_READY is set.
   Returns OSK_STATUS_DONE or OSK_STATUS_CANCELLED. */
osk_status OSK_Prompt(const osk_config *cfg, GXRModeObj *rmode, void *xfb,
		      char *out, size_t out_size);

/* UTF-8 helpers used by the library, exported for convenience. */
size_t OSK_UTF8Encode(uint32_t cp, char out[4]);
/* Decodes one code point; returns bytes consumed (0 at end of string). */
size_t OSK_UTF8Decode(const char *s, uint32_t *cp);

/* True if the built-in font can display (and therefore accept) cp. */
bool OSK_CanDisplay(uint32_t cp);

#ifdef __cplusplus
}
#endif

#endif /* __WIIOSK_H__ */
