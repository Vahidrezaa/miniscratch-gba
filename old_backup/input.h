#ifndef INPUT_H
#define INPUT_H
#include <tonc.h>

#define BTN_A       KEY_A
#define BTN_B       KEY_B
#define BTN_L       KEY_L
#define BTN_R       KEY_R
#define BTN_START   KEY_START
#define BTN_SELECT  KEY_SELECT
#define BTN_UP      KEY_UP
#define BTN_DOWN    KEY_DOWN
#define BTN_LEFT    KEY_LEFT
#define BTN_RIGHT   KEY_RIGHT
#define BTN_DIRS    (KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT)

void input_update(void);
u16  input_down(void);       // currently held
u16  input_pressed(void);    // newly pressed this frame
u16  input_repeat(u16 mask); // pressed + auto-repeat for directions
#endif
