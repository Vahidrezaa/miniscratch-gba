#ifndef SOUND_H
#define SOUND_H
#include <tonc.h>

void snd_init(void);
void snd_beep(int freq, int ms, int ch);
void snd_click(void);
void snd_ok(void);
void snd_err(void);
void snd_move(void);
#endif
