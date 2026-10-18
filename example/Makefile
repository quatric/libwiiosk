# Builds the gx and console examples. input_wii needs libinput-wii and is
# built on its own: make -C example/input_wii LIBINPUT_WII=/path/to/libinput-wii
.PHONY: all clean
all:
	@$(MAKE) --no-print-directory -C gx
	@$(MAKE) --no-print-directory -C console
clean:
	@$(MAKE) --no-print-directory -C gx clean
	@$(MAKE) --no-print-directory -C console clean
	@$(MAKE) --no-print-directory -C input_wii clean
