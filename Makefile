#---------------------------------------------------------------------------------
# MiniScratch (NanoUI) - GBA Makefile
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment: export DEVKITARM=<path to>devkitARM")
endif
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment: export DEVKITPRO=<path to>devkitPro")
endif

TARGET   := miniscratch
TITLE    := MINISCRATCH
SRCS     := main.c nano_ui.c font_data.c
BUILD    := build
OBJS     := $(SRCS:%.c=$(BUILD)/%.o)

PREFIX   := $(DEVKITARM)/bin/arm-none-eabi-
CC       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
GBAFIX   := $(DEVKITPRO)/tools/bin/gbafix

TONC     := $(DEVKITPRO)/libtonc
ARCH     := -mthumb -mthumb-interwork
CFLAGS   := -std=gnu11 -O2 -Wall -Wextra -mcpu=arm7tdmi -mtune=arm7tdmi \
            -fomit-frame-pointer -ffunction-sections -fdata-sections \
            -MMD -MP -I. -I$(TONC)/include $(ARCH)
LDFLAGS  := -specs=gba.specs $(ARCH) -Wl,--gc-sections \
            -Wl,-Map,$(BUILD)/$(TARGET).map -L$(TONC)/lib
LIBS     := -ltonc

.PHONY: all clean

all: $(TARGET).gba

$(TARGET).gba: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@ -t$(TITLE)
	@echo "built $@"

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(TARGET).gba

-include $(OBJS:.o=.d)
