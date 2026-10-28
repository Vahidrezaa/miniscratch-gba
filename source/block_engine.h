#ifndef BLOCK_ENGINE_H
#define BLOCK_ENGINE_H

#include "nano_ui.h"

#define MAX_BLOCKS 32
#define BLOCK_H 14
#define NOTCH_W 8
#define NOTCH_H 3

/* Palette indices reserved for Scratch colors (17-29 are free for app use) */
typedef enum {
    CAT_MOTION    = 20,
    CAT_LOOKS     = 21,
    CAT_SOUND     = 22,
    CAT_EVENTS    = 23,
    CAT_CONTROL   = 24,
    CAT_SENSING   = 25,
    CAT_OPERATORS = 26,
    CAT_VARIABLES = 27
} BlockCategory;

typedef struct {
    bool active;
    int x, y;
    int w, h;
    BlockCategory cat;
    const char* text;
    int arg;          // For "Move [10]"
    bool is_dragging;
    int next_block;   // Index of the block snapped below this one (-1 if none)
} Block;

void block_engine_init(void);
void block_engine_draw_palette(void);
void block_engine_draw_sandbox(void);
void block_engine_update(void);

#endif
