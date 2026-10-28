#include "save.h"

static u32 checksum(const void* p, int n) {
    const u8* b = p; u32 s = 0x811C9DC5u;
    for (int i = 0; i < n; i++) { s ^= b[i]; s *= 0x01000193u; }
    return s;
}

static void sram_write(u32 off, const u8* src, int n) {
    for (int i = 0; i < n; i++) SRAM_BASE[off + i] = src[i];
}
static void sram_read(u32 off, u8* dst, int n) {
    for (int i = 0; i < n; i++) dst[i] = SRAM_BASE[off + i];
}

int save_write(const SaveData* d) {
    SaveData tmp = *d;
    tmp.checksum = checksum(&tmp, sizeof(SaveData) - 4);
    sram_write(0, (const u8*)&tmp, sizeof(SaveData));
    return 1;
}

int save_read(SaveData* d) {
    sram_read(0, (u8*)d, sizeof(SaveData));
    if (d->magic != SRAM_MAGIC) return 0;
    u32 c = checksum(d, sizeof(SaveData) - 4);
    if (c != d->checksum) return 0;
    return 1;
}

void save_wipe(void) {
    SaveData z = {0};
    sram_write(0, (const u8*)&z, sizeof(SaveData));
}
