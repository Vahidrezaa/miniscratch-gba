TARGET   := miniscratch
BUILD    := build
SOURCES  := source
INCLUDES := source
LIBPATHS :=
LIBS     := -ltonc

ARCH     := -mthumb-interwork -mthumb
CFLAGS   := $(ARCH) -I$(INCLUDES) -Wall -O2 -std=gnu99
LDFLAGS  := $(ARCH) -specs=gba.specs

# پیدا کردن خودکار همه فایل‌های C در پوشه source
CFILES   := $(wildcard $(SOURCES)/*.c)
# تبدیل نام فایل‌های C به فایل‌های آبجکت (.o) در پوشه build
OFILES   := $(patsubst $(SOURCES)/%.c, $(BUILD)/%.o, $(CFILES))

.PHONY: all clean

all: $(TARGET).gba

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	gbafix $@

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBPATHS) $(LIBS)

$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba
