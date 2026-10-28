#ifndef SCRIPT_H
#define SCRIPT_H
#include "vm.h"

#define MAX_BLOCKS 64

extern Block g_script[MAX_BLOCKS];
extern u8    g_script_count;
extern u8    g_cursor;

void  script_clear(void);
void  script_add(u8 opcode, s16 a1, s16 a2);
void  script_remove(u8 idx);
int   script_move_up(u8 idx);
int   script_move_down(u8 idx);

s16   cos_fx(int rad);
s16   sin_fx(int rad);
const char* script_text(u8 id);
void  script_text_set(u8 id, const char* s);
#endif
