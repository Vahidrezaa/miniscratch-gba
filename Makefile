ifeq ($(strip $(DEVKITARM)),)
$(error "set DEVKITARM")
endif
include $(DEVKITARM)/gba_rules

TARGET   := miniscratch
BUILD    := build
SOURCES  := source
INCLUDES := source
LIBS     := -ltonc

ARCH    := -mthumb-interwork -mthumb
CFLAGS  := $(ARCH) -I$(INCLUDES) -Wall -O2 -std=gnu99
LDFLAGS := $(ARCH) -specs=gba.specs

export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(SOURCES)
export DEPSDIR  := $(CURDIR)/$(BUILD)
CFILES := $(notdir $(wildcard $(SOURCES)/*.c))
OFILES := $(CFILES:.c=.o)
export OFILES

.PHONY: all clean
all: $(BUILD)

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba $(TARGET).sav

ifeq ($(strip $(DEVKITARM)),)
else
ifeq ($(notdir $(CURDIR)),$(BUILD))
$(OUTPUT).gba: $(OUTPUT).elf
	$(OBJCOPY) -O binary $< $@
	gbafix $@
$(OUTPUT).elf: $(OFILES)
	$(CC) $(LDFLAGS) -o $@ $(OFILES) $(LIBPATHS) $(LIBS)
endif
endif
