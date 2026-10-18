/* Writes every generated texture to <dir>/tex_<n>.png (n = osk_tex). */
#include <stdio.h>

#include "osk_internal.h"
#include "png.h"

int main(int argc, char **argv)
{
	osk_image img[OSK_TEX_IMAGES];
	char path[512];
	int i;

	if (argc < 2 || !osk_gfx_generate(img))
		return 1;
	for (i = 0; i < OSK_TEX_IMAGES; i++) {
		snprintf(path, sizeof(path), "%s/tex_%02d.png", argv[1], i);
		png_write(path, img[i].rgba, img[i].w, img[i].h, 4);
	}
	osk_gfx_free(img);
	return 0;
}
