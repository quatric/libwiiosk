#ifndef __PREVIEW_PNG_H__
#define __PREVIEW_PNG_H__

#include <stdbool.h>
#include <stdint.h>

/* Writes 8-bit RGBA (channels = 4) or RGB (channels = 3) without compression. */
bool png_write(const char *path, const uint8_t *px, int w, int h, int channels);

#endif
