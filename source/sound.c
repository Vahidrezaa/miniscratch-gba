#include "sound.h"

void snd_init(void) {
    REG_SNDSTAT = SSTAT_ENABLE;
    REG_SNDDMGCNT = SDMG_BUILD_LR(SDMG_SQR1|SDMG_SQR2|SDMG_NOISE, 7);
    REG_SNDDSCNT = 0;
}

static void play(int ch, int freq, int ms, int duty) {
    (void)ms; // Unused for now
    u16 f = 2048 - (131072 / freq);
    if (ch == 1) {
        REG_SND1SWEEP = 0;
        REG_SND1CNT = SSQR_ENV_BUILD(15, 0, 1) | ((duty & 3) << 6);
        REG_SND1FREQ = SFREQ_RESET | SFREQ_RATE(f);
    } else {
        REG_SND2CNT = SSQR_ENV_BUILD(15, 0, 1) | ((duty & 3) << 6);
        REG_SND2FREQ = SFREQ_RESET | SFREQ_RATE(f);
    }
}

void snd_beep(int freq, int ms, int ch) { play(ch, freq, ms, 2); }
void snd_click(void) { play(2, 1200, 20, 1); }
void snd_move(void)  { play(2, 800, 15, 1); }
void snd_ok(void)    { play(1, 880, 60, 2); play(2, 1320, 60, 2); }
void snd_err(void)   { play(1, 220, 120, 3); }
