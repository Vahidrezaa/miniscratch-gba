#include "block_engine.h"
#include <string.h>
#include <stdio.h>

static Block blocks[MAX_BLOCKS];
static int active_drag_idx = -1;
static int drag_offset_x = 0;
static int drag_offset_y = 0;

// Palette definitions (For Phase 1)
static const char* pal_text[] = {"Move 10", "Turn 15", "Say Hello", "Wait 1s"};
static const BlockCategory pal_cat[] = {CAT_MOTION, CAT_MOTION, CAT_LOOKS, CAT_CONTROL};

void block_engine_init(void) {
    memset(blocks, 0, sizeof(blocks));
    
    // Inject Scratch colors into the NanoUI palette at runtime
    ui_palette_set_rgb(CAT_MOTION,     9, 18, 31);  // Blue
    ui_palette_set_rgb(CAT_LOOKS,     19, 12, 31);  // Purple
    ui_palette_set_rgb(CAT_SOUND,     25, 12, 25);  // Pink
    ui_palette_set_rgb(CAT_EVENTS,    31, 23,  0);  // Yellow
    ui_palette_set_rgb(CAT_CONTROL,   31, 21,  3);  // Orange
    ui_palette_set_rgb(CAT_SENSING,   11, 22, 26);  // Light Blue
    ui_palette_set_rgb(CAT_OPERATORS, 11, 24, 11);  // Green
    ui_palette_set_rgb(CAT_VARIABLES, 31, 17,  3);  // Dark Orange
}

// Draws a scratch-style notched block
static void draw_block_shape(int x, int y, int w, int h, BlockCategory cat, const char* text, int arg, bool is_shadow) {
    u16 color = cat;
    u16 shadow = UI_COL_BLACK;
    u16 base_color = is_shadow ? shadow : color;
    
    // Draw top notch (indentation)
    // We simulate the notch by drawing a smaller rect on top and side rects
    ui_fill_rect(x + NOTCH_W, y, w - (NOTCH_W * 2), NOTCH_H, base_color);
    ui_fill_rect(x, y + NOTCH_H, w, h - NOTCH_H * 2, base_color);
    
    // Draw bottom bump (protrusion)
    ui_fill_rect(x, y + h - NOTCH_H, NOTCH_W, NOTCH_H, base_color);
    ui_fill_rect(x + NOTCH_W, y + h - NOTCH_H, w - (NOTCH_W * 2), NOTCH_H, base_color);
    ui_fill_rect(x + w - NOTCH_W, y + h - NOTCH_H, NOTCH_W, NOTCH_H, base_color);

    if (!is_shadow) {
        // Draw text
        char buf[32];
        if (arg > 0) snprintf(buf, sizeof(buf), "%s %d", text, arg);
        else snprintf(buf, sizeof(buf), "%s", text);
        
        // Add a slight dark border for depth (optional but helps readability)
        ui_draw_frame(x, y, w, h, UI_COL_BLACK);
        
        ui_draw_text(x + 4, y + 3, buf, UI_COL_TEXT);
    }
}

void block_engine_draw_palette(void) {
    // Draw the 4 blocks in the palette (X: 12, W: 58)
    for (int i = 0; i < 4; i++) {
        int px = 14;
        int py = 20 + i * (BLOCK_H + 4);
        draw_block_shape(px, py, 54, BLOCK_H, pal_cat[i], pal_text[i], 0, false);
    }
}

void block_engine_draw_sandbox(void) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (!blocks[i].active) continue;

        // Draw shadow first
        draw_block_shape(blocks[i].x + 1, blocks[i].y + 1, blocks[i].w, blocks[i].h, blocks[i].cat, blocks[i].text, blocks[i].arg, true);
        // Draw block
        draw_block_shape(blocks[i].x, blocks[i].y, blocks[i].w, blocks[i].h, blocks[i].cat, blocks[i].text, blocks[i].arg, false);
    }
}

void block_engine_update(void) {
    int cx = ui_cursor_x();
    int cy = ui_cursor_y();
    
    // 1. Start Dragging
    if (ui_btn_pressed(KEY_A) && active_drag_idx == -1) {
        // Check Palette
        if (cx >= 12 && cx <= 70) {
            for (int i = 0; i < 4; i++) {
                int py = 20 + i * (BLOCK_H + 4);
                if (cy >= py && cy <= py + BLOCK_H) {
                    // Create new block in sandbox
                    for (int j = 0; j < MAX_BLOCKS; j++) {
                        if (!blocks[j].active) {
                            blocks[j].active = true;
                            blocks[j].x = cx; 
                            blocks[j].y = cy;
                            blocks[j].w = 60; 
                            blocks[j].h = BLOCK_H;
                            blocks[j].cat = pal_cat[i];
                            blocks[j].text = pal_text[i];
                            // Give it a default argument if it's Move or Turn
                            blocks[j].arg = (i == 0 || i == 1) ? 10 : 0;
                            blocks[j].next_block = -1;
                            blocks[j].is_dragging = true;
                            
                            active_drag_idx = j;
                            drag_offset_x = 0;
                            drag_offset_y = 0;
                            break;
                        }
                    }
                }
            }
        }
        // Check Sandbox (Pick up existing block)
        else if (cx >= 70 && cx <= 170) {
            for (int i = MAX_BLOCKS - 1; i >= 0; i--) {
                if (blocks[i].active && cx >= blocks[i].x && cx <= blocks[i].x + blocks[i].w &&
                    cy >= blocks[i].y && cy <= blocks[i].y + blocks[i].h) {
                    active_drag_idx = i;
                    drag_offset_x = cx - blocks[i].x;
                    drag_offset_y = cy - blocks[i].y;
                    blocks[i].is_dragging = true;
                    break;
                }
            }
        }
    }

    // 2. Dragging Movement
    if (active_drag_idx != -1) {
        Block *b = &blocks[active_drag_idx];
        b->x = cx - drag_offset_x;
        b->y = cy - drag_offset_y;

        // Delete if dragged back to palette or pressing B
        if (b->x < 70 || ui_btn_pressed(KEY_B)) {
            b->active = false;
            b->is_dragging = false;
            active_drag_idx = -1;
        }
    }

    // 3. Release and Snap
    if (ui_btn_released(KEY_A) && active_drag_idx != -1) {
        Block *b = &blocks[active_drag_idx];
        b->is_dragging = false;
        
        // Snapping Logic
        for (int i = 0; i < MAX_BLOCKS; i++) {
            if (i == active_drag_idx || !blocks[i].active) continue;
            Block *target = &blocks[i];
            
            // Check if dropped near the bottom of another block
            int snap_x_dist = abs(b->x - target->x);
            int snap_y_dist = abs(b->y - (target->y + target->h - NOTCH_H));
            
            if (snap_x_dist < 6 && snap_y_dist < 6) {
                b->x = target->x;
                b->y = target->y + target->h - NOTCH_H;
                target->next_block = active_drag_idx;
                break;
            }
        }
        active_drag_idx = -1;
    }
}
