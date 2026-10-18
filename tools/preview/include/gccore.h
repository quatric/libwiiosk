/* Minimal stand-in for libogc's <gccore.h> so the portable parts of
   wiiosk build on a desktop for previews and tests. */
#ifndef __PREVIEW_GCCORE_H__
#define __PREVIEW_GCCORE_H__

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef float f32;

typedef struct _gx_rmodeobj GXRModeObj;

#endif
