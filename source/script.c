#include "script.h"
#include "sound.h"

Block g_script[MAX_BLOCKS];
u8    g_script_count = 0;
u8    g_cursor = 0;

static const char* texts[8] = {
    "Hello!", "Meow!", "Done.", "Nice!", "Oops!", "Go!", "Wow!", ""
};

const char* script_text(u8 id) { return texts[id & 7]; }
void script_text_set(u8 id, const char* s) { texts[id & 7] = s; }

// fixed-point sin/cos, radius 256 units (returns Q8.8)
static const s16 sin_tab[91] = {
0,4,9,13,18,22,27,31,36,40,44,49,53,57,62,66,70,74,78,83,87,91,95,99,103,107,111,115,119,123,127,131,135,139,143,147,150,154,158,162,166,169,173,177,180,184,187,191,194,198,201,205,208,211,215,218,221,224,227,230,233,236,239,242,245,247,250,253,255,258,260,263,265,268,270,272,274,277,279,281,283,285,287,289,291,292,294,296,298,299,301,302,304,305,306,308,309,310,311,312
};

s16 sin_fx(int rad) {
    int sign = 1;
    rad %= 360; if (rad < 0) rad += 360;
    if (rad >= 180) { rad -= 180; sign = -1; }
    if (rad > 90) rad = 180 - rad;
    return sign * sin_tab[rad];
}

s16 cos_fx(int rad) {
    return sin_fx(rad + 90);
}

void script_clear(void) { g_script_count = 0; g_cursor = 0; }

void script_add(u8 opcode, s16 a1, s16 a2) {
    if (g_script_count >= MAX_BLOCKS) { snd_err(); return; }
    Block b = { opcode, a1, a2, 0, 0 };
    if (opcode == OP_REPEAT || opcode == OP_FOREVER ||
        opcode == OP_IF || opcode == OP_IF_ELSE) {
        b.body_start = g_script_count + 1;
        b.body_end = g_script_count + 1;  // empty body
    }
    g_script[g_script_count++] = b;
    snd_click();
}

void script_remove(u8 idx) {
    if (idx >= g_script_count) return;
    for (int i = idx; i < g_script_count - 1; i++) g_script[i] = g_script[i+1];
    g_script_count--;
    if (g_cursor >= g_script_count && g_cursor > 0) g_cursor--;
}

int script_move_up(u8 idx) {
    if (idx == 0 || idx >= g_script_count) return 0;
    Block t = g_script[idx-1]; g_script[idx-1] = g_script[idx]; g_script[idx] = t;
    return 1;
}
int script_move_down(u8 idx) {
    if (idx + 1 >= g_script_count) return 0;
    Block t = g_script[idx+1]; g_script[idx+1] = g_script[idx]; g_script[idx] = t;
    return 1;
}
