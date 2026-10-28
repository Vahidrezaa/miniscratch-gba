#ifndef VM_H
#define VM_H
#include <tonc.h>

typedef enum {
    OP_END = 0,
    OP_MOVE,         // arg1 = steps
    OP_TURN,         // arg1 = degrees (+left/-right)
    OP_GOTO,         // arg1 = x, arg2 = y
    OP_POINT,        // arg1 = direction
    OP_NEXT_COSTUME,
    OP_SET_COSTUME,  // arg1 = index
    OP_SAY,          // arg1 = text id (0..7)
    OP_WAIT,         // arg1 = frames
    OP_REPEAT,       // arg1 = count, body_start/body_end
    OP_FOREVER,
    OP_IF,           // arg1 = var idx, arg2 = compare value, body
    OP_IF_ELSE,
    OP_SET_VAR,      // arg1 = var idx, arg2 = value
    OP_CHANGE_VAR,   // arg1 = var idx, arg2 = delta
    OP_RANDOM_VAR,   // arg1 = var idx, arg2 = max
    OP_STOP
} OpCode;

typedef struct {
    u8 opcode;
    s16 arg1;
    s16 arg2;
    u8 body_start;
    u8 body_end;
} Block;

typedef struct {
    s16 x, y;       // cat position (pixels)
    s16 dir;        // 0..359
    u8  costume;
    u8  visible;
    const char* say;
    u16 say_timer;
} CatState;

extern CatState g_cat;
extern s16 g_vars[8];

void vm_reset(void);
void vm_run(const Block* script, u8 count);
void vm_step(const Block* script, u8 count);
int  vm_is_running(void);
void vm_draw_cat(void);
#endif
