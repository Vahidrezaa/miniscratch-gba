#ifndef SAVE_H
#define SAVE_H
#include <tonc.h>

#define SRAM_BASE ((vu8*)0x0E000000)
#define SRAM_MAGIC 0x4D534352  // "MSCR"

typedef struct {
    u32 magic;
    u8  blocks[64*5];
    u8  block_count;
    s16 variables[8];
    u8  costume_count;
    u8  costumes[4*32*32];
    u32 checksum;
} SaveData;

int save_write(const SaveData* d);
int save_read(SaveData* d);
void save_wipe(void);
#endif
