#ifndef BLOCKS_H
#define BLOCKS_H
#include "vm.h"

typedef struct {
    const char* name;
    u8 opcode;
    u8 arg_min, arg_max;
    u8 category;
} BlockDef;

#define PALETTE_COUNT 16
extern const BlockDef g_palette[PALETTE_COUNT];
const char* block_label(u8 opcode);
u8 block_color(u8 opcode);
#endif
