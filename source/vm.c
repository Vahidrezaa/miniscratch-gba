#include "vm.h"
#include "gfx.h"
#include "font.h"
#include "sound.h"
#include "script.h"

CatState g_cat;
s16 g_vars[8];

static const Block* s_script;
static u8  s_count;
static u8  s_pc;
static u8  s_stack[8];
static u8  s_stack_top;
static u8  s_forever[8];
static u8  s_forever_top;
static u8  s_running;
static u16 s_wait;

void vm_reset(void) {
    g_cat.x = 120; g_cat.y = 80; g_cat.dir = 90; g_cat.costume = 0;
    g_cat.visible = 1; g_cat.say = 0; g_cat.say_timer = 0;
    for (int i = 0; i < 8; i++) g_vars[i] = 0;
    s_running = 0; s_wait = 0;
    s_stack_top = 0; s_forever_top = 0;
}

void vm_run(const Block* script, u8 count) {
    s_script = script; s_count = count;
    s_pc = 0; s_stack_top = 0; s_forever_top = 0;
    s_running = 1; s_wait = 0;
}

int vm_is_running(void) { return s_running; }

void vm_step(const Block* script, u8 count) {
    (void)script; (void)count;
    if (!s_running) return;
    if (s_wait) { s_wait--; return; }
    if (s_pc >= s_count) { s_running = 0; return; }

    const Block* b = &s_script[s_pc];
    switch (b->opcode) {
    case OP_END: s_running = 0; break;
    case OP_MOVE: {
        int rad = ((g_cat.dir - 90) * 355) / 18000;
        s16 dx = (cos_fx(rad) * b->arg1) >> 8;
        s16 dy = (sin_fx(rad) * b->arg1) >> 8;
        g_cat.x += dx; g_cat.y += dy;
        if (g_cat.x < 16) g_cat.x = 16;
        if (g_cat.x > 224) g_cat.x = 224;
        if (g_cat.y < 16) g_cat.y = 16;
        if (g_cat.y > 144) g_cat.y = 144;
        s_pc++; break;
    }
    case OP_TURN: {
        g_cat.dir += b->arg1;
        while (g_cat.dir < 0) g_cat.dir += 360;
        g_cat.dir %= 360;
        s_pc++; break;
    }
    case OP_GOTO: g_cat.x = b->arg1; g_cat.y = b->arg2; s_pc++; break;
    case OP_POINT: g_cat.dir = b->arg1; s_pc++; break;
    case OP_NEXT_COSTUME: g_cat.costume = (g_cat.costume + 1) % 4; s_pc++; break;
    case OP_SET_COSTUME: g_cat.costume = b->arg1 % 4; s_pc++; break;
    case OP_SAY: g_cat.say = script_text(b->arg1); g_cat.say_timer = 120; s_pc++; break;
    case OP_WAIT: s_wait = b->arg1; s_pc++; break;
    case OP_REPEAT:
        if (b->arg1 > 0) {
            if (s_stack_top < 8) s_stack[s_stack_top] = s_pc;
            s_stack_top++;
            s_pc = b->body_start;
        } else s_pc = b->body_end;
        break;
    case OP_FOREVER:
        if (s_forever_top < 8) s_forever[s_forever_top] = s_pc;
        s_forever_top++;
        s_pc = b->body_start;
        break;
    case OP_IF:
        if (g_vars[b->arg1] == b->arg2) s_pc = b->body_start;
        else s_pc = b->body_end;
        break;
    case OP_SET_VAR: g_vars[b->arg1] = b->arg2; s_pc++; break;
    case OP_CHANGE_VAR: g_vars[b->arg1] += b->arg2; s_pc++; break;
    case OP_RANDOM_VAR: {
        u16 seed = REG_VCOUNT ^ (s_pc * 2654435761u);
        g_vars[b->arg1] = (seed % (b->arg2 + 1));
        s_pc++; break;
    }
    case OP_STOP: s_running = 0; break;
    default: s_pc++; break;
    }
    
    if (s_running && s_stack_top > 0 && s_pc == s_script[s_stack[s_stack_top-1]].body_end) {
        s_stack_top--;
        const Block* rep = &s_script[s_stack[s_stack_top]];
        if (rep->arg1 > 1) {
            ((Block*)rep)->arg1--;
            s_pc = rep->body_start;
            s_stack_top++;
        }
    }
    if (s_running && s_forever_top > 0 && s_pc == s_script[s_forever[s_forever_top-1]].body_end) {
        s_pc = s_script[s_forever[s_forever_top-1]].body_start;
    }
}

void vm_draw_cat(void) {
    // اسپرایت گربه در ایندکس 1 از obj_buffer قرار دارد
    OBJ_ATTR* cat = &obj_buffer[1];
    
    if (!g_cat.visible) {
        obj_set_pos(cat, 0, 160); // مخفی کردن در بیرون صفحه
        return;
    }

    // تنظیمات اسپرایت (مربع 32x32، 4bpp، بانک پالت 0، تایل 0)
    obj_set_attr(cat,
        ATTR0_SQUARE | ATTR0_4BPP,
        ATTR1_SIZE_32,
        ATTR2_PALBANK(0) | 0);
        
    // قرار دادن در مختصات مرکز گربه
    obj_set_pos(cat, g_cat.x - 16, g_cat.y - 16);

    // رسم حباب گفتار (این یکی روی پس‌زمینه رسم می‌شود)
    if (g_cat.say) {
        int w = font_width(g_cat.say) + 6;
        int bx = g_cat.x - w/2;
        int by = g_cat.y - 40;
        if (bx < 2) bx = 2;
        if (by < 2) by = 2;
        gfx_fill_rect(bx, by, w, 12, 1);
        gfx_rect(bx, by, w, 12, 2);
        font_draw(bx+3, by+2, g_cat.say, 2);
        if (g_cat.say_timer) g_cat.say_timer--;
        else g_cat.say = 0;
    }
}
