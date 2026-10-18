# Desktop build of the portable wiiosk sources: scene previews and the
# self-test, drawn by a software rasterizer instead of GX.
CC	?= cc
CFLAGS	?= -O2 -g
CFLAGS	+= -Wall -Wextra -Wno-unused-parameter -Iinclude -I../../include -I../../source
LIBSRC	:= ../../source/osk_core.c ../../source/osk_layouts.c ../../source/osk_text.c \
	   ../../source/osk_render.c ../../source/osk_gfx.c ../../source/osk_font_data.c \
	   ../../source/osk_predict.c $(wildcard ../../source/dict/*.c)
HOSTSRC	:= soft.c png.c

all: wiiosk_preview wiiosk_selftest wiiosk_texdump

wiiosk_preview: preview.c $(HOSTSRC) $(LIBSRC)
	$(CC) $(CFLAGS) -o $@ $^ -lm

wiiosk_selftest: ../../tests/selftest/source/tests.c host_selftest.c $(HOSTSRC) $(LIBSRC)
	$(CC) $(CFLAGS) -I../../tests/selftest/source -o $@ $^ -lm

wiiosk_texdump: texdump.c png.c ../../source/osk_gfx.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -rf wiiosk_preview wiiosk_selftest wiiosk_texdump *.dSYM

.PHONY: all clean
