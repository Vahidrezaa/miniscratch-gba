#include "input.h"

static u16 prev = 0, now = 0, edge = 0;
static u16 rep_cnt = 0;

void input_update(void) {
    prev = now;
    now = ~REG_KEYINPUT & 0x03FF;
    edge = now & ~prev;
}

u16 input_down(void) { return now; }
u16 input_pressed(void) { return edge; }

u16 input_repeat(u16 mask) {
    u16 hit = edge & mask;
    if (now & mask) {
        if (rep_cnt == 0) { hit |= (now & mask); rep_cnt = 18; }
        else if (--rep_cnt == 0) { hit |= (now & mask); rep_cnt = 4; }
    } else if ((now & mask) == 0) rep_cnt = 0;
    return hit;
}
