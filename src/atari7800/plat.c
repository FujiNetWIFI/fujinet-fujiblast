/*
 * plat.c -- 7800 start-up, the 60 Hz clock and quitting.
 *
 * The clock is the frame's DLI (nmi.s), 60 jiffies a second on NTSC and PAL
 * alike: it counts on while a network call waits.
 *
 * The game's loop is one pass a jiffy, and a pass with four players and the
 * link can take two or three. When frame_wait finds the game behind the
 * clock it returns at once: that pass is logic only (walking, timers), so
 * our player walks at 60 steps a second as the server's bots do. Every
 * MAX_SKIP passes it draws all the same. (The Atari's plat.c.)
 */
#ifdef __ATARI7800__

#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
#include "a78hw.h"
#include "fujinet-atari7800.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes without drawing, at most, when behind */

static uint16_t seed = 0x1234;

void plat_init(void)
{
    snd_init();
    input_init();
    gfx_init();
}

/* Back to CONFIG (the cartridge's loader), saying goodbye to the server. */
void plat_quit(void)
{
    nst_close();
    fuji_a7800_config();
}

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
    d = *(volatile uint16_t *)&frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (a call, an appkey): let it go */
        seen = frame_count - 1;
        d = 1;
    }
    if (d >= 2 && skipped < MAX_SKIP) {
        ++skipped;
        ++seen;                 /* behind: a pass with no drawing */
        return;
    }
    skipped = 0;
    spr_flush();
    while (*(volatile uint16_t *)&frame_count == seen)
        ;
    ++seen;
    seed += frame_count;
}

uint8_t plat_rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed ^ RANDOM;
}

#endif
