#include "blocks.h"

const BlockDef g_palette[PALETTE_COUNT] = {
    {"Move",      OP_MOVE,         1, 50, 0},
    {"Turn",      OP_TURN,        -90, 90, 0},
    {"Goto 0 0",  OP_GOTO,         0, 240, 0},
    {"Point",     OP_POINT,        0, 359, 0},
    {"Costume+",  OP_NEXT_COSTUME, 0, 0, 1},
    {"Say",       OP_SAY,          0, 7, 1},
    {"Wait",      OP_WAIT,         1, 255, 2},
    {"Repeat",    OP_REPEAT,       1, 20, 2},
    {"Forever",   OP_FOREVER,      0, 0, 2},
    {"If Var=0",  OP_IF,           0, 7, 2},
    {"Set Var",   OP_SET_VAR,      0, 99, 3},
    {"Chg Var",   OP_CHANGE_VAR,  -99, 99, 3},
    {"Rand Var",  OP_RANDOM_VAR,   0, 99, 3},
    {"Stop",      OP_STOP,         0, 0, 2},
    {"Next",      OP_NEXT_COSTUME, 0, 0, 1},
    {"Set Cos",   OP_SET_COSTUME,  0, 3, 1}
};

static const char* op_names[] = {
    "END","MOVE","TURN","GOTO","POINT","NEXT COS","SET COS","SAY",
    "WAIT","REPEAT","FOREVER","IF","IF/ELSE","SET VAR","CHG VAR",
    "RAND VAR","STOP"
};
const char* block_label(u8 op) { return op < 17 ? op_names[op] : "?"; }

u8 block_color(u8 op) {
    switch (op) {
        case OP_MOVE: case OP_TURN: case OP_GOTO: case OP_POINT: return 32;
        case OP_NEXT_COSTUME: case OP_SET_COSTUME: case OP_SAY: return 33;
        case OP_WAIT: case OP_REPEAT: case OP_FOREVER: case OP_IF:
        case OP_IF_ELSE: case OP_STOP: return 34;
        case OP_SET_VAR: case OP_CHANGE_VAR: case OP_RANDOM_VAR: return 35;
    }
    return 32;
}
