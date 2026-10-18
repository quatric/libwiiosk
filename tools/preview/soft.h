#ifndef __PREVIEW_SOFT_H__
#define __PREVIEW_SOFT_H__

/* Render target for the software backend: RGB floats, w x h pixels,
   scale pixels per virtual screen unit. */
void soft_target(float *rgb, int w, int h, float scale);

#endif
