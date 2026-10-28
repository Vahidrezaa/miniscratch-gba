#---------------------------------------------------------------------------------
# MiniScratch (NanoUI) - GBA Makefile
#---------------------------------------------------------------------------------
TARGET   := miniscratch
BUILD    := build
SOURCES  := source
INCLUDES := source
LIBS     := -ltonc

# Absolute paths proven to work in GitHub Actions
DEVKITARM := /opt/devkitpro/devkitARM
LIBTONC   := /opt/devkitpro/libtonc

CC       := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY  := $(DEVKITARM)/bin/arm-none-eabi-objcopy
GBAFIX   := /opt/devkitpro/tools/bin/gbafix

ARCH     := -mthumb-interwork -mthumb
CFLAGS   := $(ARCH) -I$(INCLUDES) -I$(LIBTONC)/include -Wall -O2 -std=gnu11
LDFLAGS  := $(ARCH) -specs=gba.specs -L$(LIBTONC)/lib -L$(DEVKITARM)/lib

# Find all .c files in the source directory
CFILES   := $(wildcard $(SOURCES)/*.c)
# Map them to the build directory
OFILES   := $(patsubst $(SOURCES)/%.c, $(BUILD)/%.o, $(CFILES))

.PHONY: all clean

all: $(TARGET).gba

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@ -tMINISCRATCH
	@echo "Built $@ successfully!"

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba
