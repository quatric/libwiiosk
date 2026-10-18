#---------------------------------------------------------------------------------
# wiiosk example (libinput-wii)
# Needs a built libinput-wii (AGPL-3.0, so the resulting .dol is AGPL too):
#   make LIBINPUT_WII=/path/to/libinput-wii
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPro/devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

TARGET		:=	wiiosk_input_wii
OSKDIR		:=	$(CURDIR)/../..
LIBINPUT_WII	?=	$(OSKDIR)/../libinput-wii
BUILD		:=	build

CFLAGS		:=	-g -O2 -Wall $(MACHDEP) -I$(OSKDIR)/include -I$(LIBINPUT_WII)/include -I$(LIBOGC_INC)
LDFLAGS		:=	-g $(MACHDEP) -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS		:=	-L$(OSKDIR)/lib -L$(LIBINPUT_WII)/lib -L$(LIBOGC_LIB) -lwiiosk -linput-wii -lwiikeyboard -lwiiuse -lbte -logc -lm

CFILES		:=	$(wildcard source/*.c)
OFILES		:=	$(patsubst source/%.c,$(BUILD)/%.o,$(CFILES))

.PHONY: all clean

all: $(TARGET).dol

$(TARGET).dol: $(TARGET).elf
	@echo output ... $@
	@elf2dol $< $@

$(TARGET).elf: $(OFILES) $(OSKDIR)/lib/libwiiosk.a
	@echo linking ... $@
	@$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

$(BUILD)/%.o: source/%.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).dol
